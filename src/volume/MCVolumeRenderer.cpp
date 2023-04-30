#include "pch.h"
#include "MCVolumeRenderer.h"

MCVolumeRenderer::MCVolumeRenderer(std::shared_ptr<DX::DeviceResources> deviceRes): m_RandomGenerator(m_RandomDevice())
, m_RandomDistribution(-0.5f, +0.5f) {
	m_deviceResources = deviceRes;
	initialize();
}

void MCVolumeRenderer::initialize() {
	DX::ComPtr<ID3D11Device1> m_pDevice = m_deviceResources->GetD3DDevice();

	// initialize shaders
	m_shaders = std::make_unique<MCShaders>(m_pDevice);
	
	// parse transfer functions and generate textures
	std::string transferFunctionJSON = "data/config/transferFunction.json";
	m_transferFunctions = std::make_unique<MCTransferFunction>(transferFunctionJSON);
	generateTransferFunctionTextures(m_pDevice);

	// initialize samplers
	initializeSamplers(m_pDevice);

    // initialize render textures
    initializeRenderTextures(m_pDevice);

    // build volume and volume textures
    initializeVolume(m_deviceResources);

    initializeRenderTextures();

    initializeTileBuffers();

    initializeBuffers();

    initializeEnvironmentMap();
}

void MCVolumeRenderer::update(float deltaTime)
{
    m_DeltaTime = deltaTime;
    updateState();
}

void MCVolumeRenderer::blit(DX::ComPtr<ID3D11ShaderResourceView>  pSrc, DX::ComPtr<ID3D11RenderTargetView> targetView)
{
    m_deviceResources->PIXBeginEvent(L"Render Pass: Blit [Tone Map] -> [Back Buffer]");
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    auto m_pImmediateContext = m_deviceResources->GetD3DDeviceContext();
    auto destination = m_deviceResources->GetRenderTargetView();
    ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr, nullptr,  nullptr, nullptr, nullptr, nullptr };
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    D3D11_RECT scissor = { 0, 0,static_cast<int32_t>(width), static_cast<int32_t>(height) };

    m_pImmediateContext->OMSetRenderTargets(1, targetView.GetAddressOf(), nullptr);
    m_pImmediateContext->RSSetScissorRects(1, &scissor);
    m_pImmediateContext->RSSetViewports(1, &viewport);

    // Bind PSO and Resources
    m_shaders->m_PSOBlit.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetShaderResources(0, 1, pSrc.GetAddressOf());
    m_pImmediateContext->PSSetSamplers(0, 1, m_pSamplerPoint.GetAddressOf());

    // Execute
    m_pImmediateContext->Draw(6, 0);

    // Unbind RTV's
    m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pImmediateContext->RSSetScissorRects(0, nullptr);
    m_pImmediateContext->RSSetViewports(0, nullptr);

    // Unbind PSO and unbind Resources
    m_shaders->m_PSODefault.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
    m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
    m_deviceResources->PIXEndEvent();
}

void MCVolumeRenderer::renderFrame(DX::ComPtr<ID3D11RenderTargetView> pRTV)
{
    if (m_FrameIndex > m_MaximumSamples) {
        blit(m_pSRVToneMap, pRTV);
        return;
    };
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    auto m_pImmediateContext = m_deviceResources->GetD3DDeviceContext();

    for (size_t i = 0; i < 8; i++) {
        ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr, nullptr, nullptr, nullptr };
        ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        uint32_t threadGroupsX = static_cast<uint32_t>(std::ceil(width / 8));
        uint32_t threadGroupsY = static_cast<uint32_t>(std::ceil(height / 8));

        m_pImmediateContext->VSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
        m_pImmediateContext->GSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
        m_pImmediateContext->PSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
        m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());

        if (m_FrameIndex < 1) {
            ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVDispersionTiles.Get() };
            uint32_t pCounters[] = { 0 };

            m_deviceResources->PIXBeginEvent(L"Render Pass: Reset computed tiles");
            m_shaders->m_PSOResetTiles.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
            m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_deviceResources->PIXEndEvent();
        }
        else {
            ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVToneMap.Get(), m_pSRVDepth.Get() };
            ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVDispersionTiles.Get() };
            uint32_t pCounters[] = { 0 };

            m_deviceResources->PIXBeginEvent(L"Render Pass: Generete computed tiles");
            m_shaders->m_PSOComputeTiles.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
            m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_deviceResources->PIXEndEvent();
        }

        float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_deviceResources->PIXBeginEvent(L"Render Pass: Clear buffers [Color, Normal, Depth]");
        m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVDiffuse.Get(), clearColor);
        m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVNormal.Get(), clearColor);
        m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVDepth.Get(), clearColor);
        m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVRadiance.Get(), clearColor);
        m_deviceResources->PIXEndEvent();

        m_deviceResources->PIXBeginEvent(L"Render Pass: Copy counters of tiles");
        m_pImmediateContext->CopyStructureCount(m_pDispathIndirectBufferArgs.Get(), 0, m_pUAVDispersionTiles.Get());
        m_pImmediateContext->CopyStructureCount(m_pDrawInstancedIndirectBufferArgs.Get(), 0, m_pUAVDispersionTiles.Get());
        m_deviceResources->PIXEndEvent();
        {
            ID3D11SamplerState* ppSamplers[] = {
                m_pSamplerPoint.Get(),
                m_pSamplerLinear.Get(),
                m_pSamplerAnisotropic.Get()
            };

            ID3D11ShaderResourceView* ppSRVResources[] = {
                m_volume->m_pSRVVolumeIntensity[m_MipLevel].Get(),
                m_volume->m_pSRVGradient.Get(),
                m_pSRVDiffuseTF.Get(),
                m_pSRVSpecularTF.Get(),
                m_pSRVRoughnessTF.Get(),
                m_pSRVOpacityTF.Get(),
                m_pSRVDispersionTiles.Get()
            };

            ID3D11UnorderedAccessView* ppUAVResources[] = {
                m_pUAVDiffuse.Get(),
                m_pUAVSpecular.Get(),
                m_pUAVNormal.Get(),
                m_pUAVDepth.Get()
            };

            m_deviceResources->PIXBeginEvent(L"Render pass: Generate Rays");
            m_shaders->m_PSOGeneratePrimaryRays.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
            m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_deviceResources->PIXEndEvent();
        }

        {
            ID3D11SamplerState* ppSamplers[] = {
                m_pSamplerPoint.Get(),
                m_pSamplerLinear.Get(),
                m_pSamplerAnisotropic.Get()
            };

            ID3D11ShaderResourceView* ppSRVResources[] = {
                m_volume->m_pSRVVolumeIntensity[m_MipLevel].Get(),
                m_pSRVOpacityTF.Get(),
                m_pSRVDiffuse.Get(),
                m_pSRVSpecular.Get(),
                m_pSRVNormal.Get(),
                m_pSRVDepth.Get(),
                m_pSRVEnviroment.Get(),
                m_pSRVDispersionTiles.Get()
            };

            ID3D11UnorderedAccessView* ppUAVResources[] = {
                m_pUAVRadiance.Get()
            };

            m_deviceResources->PIXBeginEvent(L"Render pass: Compute Radiance");
            m_shaders->m_PSOComputeDiffuseLight.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
            m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_deviceResources->PIXEndEvent();
        }

        {
            ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVRadiance.Get(),  m_pSRVDispersionTiles.Get() };
            ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVColorSum.Get() };

            m_deviceResources->PIXBeginEvent(L"Render Pass: Accumulate");
            m_shaders->m_PSOAccumulate.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
            m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_deviceResources->PIXEndEvent();
        }

        {
            ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVColorSum.Get(), m_pSRVDispersionTiles.Get() };
            ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVToneMap.Get() };

            m_deviceResources->PIXBeginEvent(L"Render Pass: Tone Map");
            m_shaders->m_PSOToneMap.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
            m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_deviceResources->PIXEndEvent();
        }
        m_FrameIndex++;
        // update
        updateState();
    }
    blit(m_pSRVToneMap, pRTV);
    /*   if (m_IsDrawDegugTiles) {
           ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVDispersionTiles.Get() };
           D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };

        m_deviceResources->PIXBeginEvent(L"Render Pass: Debug -> [Generated tiles]");
        m_pImmediateContext->OMSetRenderTargets(1, pRTV.GetAddressOf(), nullptr);
        m_pImmediateContext->RSSetViewports(1, &viewport);

        m_PSODegugTiles.Apply(m_pImmediateContext);
        m_pImmediateContext->VSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->DrawInstancedIndirect(m_pDrawInstancedIndirectBufferArgs.Get(), 0);

        m_pImmediateContext->VSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_PSODefault.Apply(m_pImmediateContext);
        m_pImmediateContext->RSSetViewports(0, nullptr);
        m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_deviceResources->PIXEndEvent();
    }*/
}

void MCVolumeRenderer::generateTransferFunctionTextures(DX::ComPtr<ID3D11Device> m_pDevice)
{
	m_pSRVOpacityTF = m_transferFunctions->opacityTF.GenerateTexture(m_pDevice, m_SamplingCount);
	m_pSRVDiffuseTF = m_transferFunctions->diffuseTF.GenerateTexture(m_pDevice, m_SamplingCount);
	m_pSRVSpecularTF = m_transferFunctions->specularTF.GenerateTexture(m_pDevice, m_SamplingCount);
	m_pSRVRoughnessTF = m_transferFunctions->roughnessTF.GenerateTexture(m_pDevice, m_SamplingCount);
}

void MCVolumeRenderer::initializeSamplers(DX::ComPtr<ID3D11Device> m_pDevice)
{
	auto createSamplerState = [this, m_pDevice](auto filter, auto addressMode) -> DX::ComPtr<ID3D11SamplerState> {
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = filter;
		desc.AddressU = addressMode;
		desc.AddressV = addressMode;
		desc.AddressW = addressMode;
		desc.MaxAnisotropy = D3D11_MAX_MAXANISOTROPY;
		desc.MaxLOD = FLT_MAX;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		Microsoft::WRL::ComPtr<ID3D11SamplerState> pSamplerState;
		DX::ThrowIfFailed(m_pDevice->CreateSamplerState(&desc, pSamplerState.GetAddressOf()));
		return pSamplerState;
	};

	m_pSamplerPoint = createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
	m_pSamplerLinear = createSamplerState(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
	m_pSamplerAnisotropic = createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
}

void MCVolumeRenderer::initializeRenderTextures(DX::ComPtr<ID3D11Device> m_pDevice)
{
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDiffuse;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuse.Get(), nullptr, m_pSRVDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuse.Get(), nullptr, m_pUAVDiffuse.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureSpecular;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureSpecular.Get(), nullptr, m_pSRVSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureSpecular.Get(), nullptr, m_pUAVSpecular.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDiffuseLight;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuseLight.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuseLight.Get(), nullptr, m_pSRVRadiance.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuseLight.Get(), nullptr, m_pUAVRadiance.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureNormal;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureNormal.Get(), nullptr, m_pSRVNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureNormal.Get(), nullptr, m_pUAVNormal.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDepth;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDepth.Get(), nullptr, m_pSRVDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDepth.Get(), nullptr, m_pUAVDepth.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColorSum;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColorSum.Get(), nullptr, m_pSRVColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColorSum.Get(), nullptr, m_pUAVColorSum.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
    }
}

void MCVolumeRenderer::initializeVolume(std::shared_ptr<DX::DeviceResources> deviceResource)
{
    MCVolumeDataLoaderInitializeSamplers samplers = {
        m_pSamplerPoint,
        m_pSamplerLinear,
        m_pSamplerAnisotropic
    };

    MCVolumeDataLoaderInitializeShaders shaders = {};
    shaders.m_PSOGenerateMipLevel = m_shaders->m_PSOGenerateMipLevel;
    shaders.m_PSOComputeGradient = m_shaders->m_PSOComputeGradient;

    m_volume = std::make_unique<MCVolumeDataLoader>(
        m_deviceResources, samplers, shaders, m_pSRVOpacityTF);
}

void MCVolumeRenderer::initializeRenderTextures()
{
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    auto m_pDevice = m_deviceResources->GetD3DDevice();
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDiffuse;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuse.Get(), nullptr, m_pSRVDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuse.Get(), nullptr, m_pUAVDiffuse.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureSpecular;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureSpecular.Get(), nullptr, m_pSRVSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureSpecular.Get(), nullptr, m_pUAVSpecular.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDiffuseLight;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuseLight.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuseLight.Get(), nullptr, m_pSRVRadiance.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuseLight.Get(), nullptr, m_pUAVRadiance.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureNormal;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureNormal.Get(), nullptr, m_pSRVNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureNormal.Get(), nullptr, m_pUAVNormal.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDepth;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDepth.Get(), nullptr, m_pSRVDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDepth.Get(), nullptr, m_pUAVDepth.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColorSum;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColorSum.Get(), nullptr, m_pSRVColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColorSum.Get(), nullptr, m_pUAVColorSum.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Width = width;
        desc.Height = height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
    }
}

void MCVolumeRenderer::initializeTileBuffers()
{
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    auto m_pDevice = m_deviceResources->GetD3DDevice();
    uint32_t threadGroupsX = static_cast<uint32_t>(std::ceil(width / 8));
    uint32_t threadGroupsY = static_cast<uint32_t>(std::ceil(width / 8));

    DX::ComPtr<ID3D11Buffer> pBuffer = DX::CreateStructuredBuffer<uint32_t>(m_pDevice, threadGroupsX * threadGroupsY, false, true, nullptr);
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        desc.BufferEx.FirstElement = 0;
        desc.BufferEx.NumElements = threadGroupsX * threadGroupsY;
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pBuffer.Get(), &desc, m_pSRVDispersionTiles.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = threadGroupsX * threadGroupsY;
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pBuffer.Get(), &desc, m_pUAVDispersionTiles.ReleaseAndGetAddressOf()));
    }
}

void MCVolumeRenderer::initializeBuffers()
{
    auto m_pDevice = m_deviceResources->GetD3DDevice();
    m_pConstantBufferFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
    m_pDispathIndirectBufferArgs = DX::CreateIndirectBuffer<DispathIndirectBuffer>(m_pDevice, DispathIndirectBuffer{ 1, 1, 1 });
    m_pDrawInstancedIndirectBufferArgs = DX::CreateIndirectBuffer<DrawInstancedIndirectBuffer>(m_pDevice, DrawInstancedIndirectBuffer{ 0, 1, 0, 0 });
}

void MCVolumeRenderer::initializeEnvironmentMap()
{
    auto m_pDevice = m_deviceResources->GetD3DDevice();
    const wchar_t* filename = L"data/textures/clear_puresky_2k.dds";
    //const wchar_t* filename2 = L"data/textures/thatch_chapel_2k.dds";
    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice, filename, nullptr, m_pSRVEnviroment.GetAddressOf()));
}

void MCVolumeRenderer::updateState()
{
    auto width = m_deviceResources->GetOutputSize().right;
    auto height = m_deviceResources->GetOutputSize().bottom;
    auto m_pImmediateContext = m_deviceResources->GetD3DDeviceContext();
    Hawk::Math::Vec3 scaleVector = { 0.488f * m_volume->m_DimensionX, 0.488f * m_volume->m_DimensionY, 0.7f * m_volume->m_DimensionZ };
    scaleVector /= (std::max)({ scaleVector.x, scaleVector.y, scaleVector.z });

    auto const& matrixView = m_Camera.ToMatrix();
    Hawk::Math::Mat4x4 matrixProjection = Hawk::Math::Orthographic(m_Zoom * (width / static_cast<F32>(height)), m_Zoom, -1.0f, 1.0f);
    Hawk::Math::Mat4x4 matrixWorld = Hawk::Math::RotateX(Hawk::Math::Radians(-90.0f));
    Hawk::Math::Mat4x4 matrixNormal = Hawk::Math::Inverse(Hawk::Math::Transpose(matrixWorld));

    {
        DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pConstantBufferFrame, D3D11_MAP_WRITE_DISCARD, 0);
        map->BoundingBoxMin = scaleVector * m_BoundingBoxMin;
        map->BoundingBoxMax = scaleVector * m_BoundingBoxMax;

        map->ViewProjectionMatrix = matrixProjection * matrixView;
        map->NormalViewMatrix = matrixView * matrixNormal;
        map->WorldViewProjectionMatrix = matrixProjection * matrixView * matrixWorld;
        map->ViewMatrix = matrixView;
        map->WorldMatrix = matrixWorld;
        map->NormalMatrix = matrixNormal;

        map->InvViewProjectionMatrix = Hawk::Math::Inverse(map->ViewProjectionMatrix);
        map->InvNormalViewMatrix = Hawk::Math::Inverse(map->NormalViewMatrix);
        map->InvWorldViewProjectionMatrix = Hawk::Math::Inverse(map->WorldViewProjectionMatrix);
        map->InvViewMatrix = Hawk::Math::Inverse(map->InvViewMatrix);
        map->InvWorldMatrix = Hawk::Math::Inverse(map->WorldMatrix);
        map->InvNormalMatrix = Hawk::Math::Inverse(map->NormalMatrix);
        map->StepSize = Hawk::Math::Distance(map->BoundingBoxMin, map->BoundingBoxMax) / m_StepCount;

        map->Density = m_Density;
        map->FrameIndex = m_FrameIndex;
        map->Exposure = m_Exposure;

        map->FrameOffset = Hawk::Math::Vec2(m_RandomDistribution(m_RandomGenerator), m_RandomDistribution(m_RandomGenerator));
        map->RenderTargetDim = Hawk::Math::Vec2(static_cast<F32>(width), static_cast<F32>(height));
        map->InvRenderTargetDim = Hawk::Math::Vec2(1.0f, 1.0f) / map->RenderTargetDim;
    }
}

auto MCVolumeRenderer::handleMouseMove(float x, float y) -> void {
    if (x != 0.0f || y != 0.0f) {
        m_Camera.Rotate(Hawk::Components::Camera::LocalUp, m_DeltaTime * -m_RotateSensivity * x);
        m_Camera.Rotate(m_Camera.Right(), m_DeltaTime * -m_RotateSensivity * y);
        m_FrameIndex = 0;
    }
}