#include "pch.h"
#include "MCVolumeDataLoader.h"

MCVolumeDataLoader::MCVolumeDataLoader(std::shared_ptr<DX::DeviceResources> deviceResource, MCVolumeDataLoaderInitializeSamplers samplers,
    MCVolumeDataLoaderInitializeShaders shaders,
    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVOpacityTF)
{
    std::unique_ptr<FILE, decltype(&fclose)> pFile(fopen(fileName.c_str(), "rb"), fclose);

    auto m_pImmediateContext = deviceResource->GetD3DDeviceContext();
    auto m_pDevice = deviceResource->GetD3DDevice();
    if (!pFile)
        throw std::runtime_error("Failed to open file: " + fileName);

    fread(reinterpret_cast<char*>(&m_DimensionX), sizeof(uint16_t), 1, pFile.get());
    fread(reinterpret_cast<char*>(&m_DimensionY), sizeof(uint16_t), 1, pFile.get());
    fread(reinterpret_cast<char*>(&m_DimensionZ), sizeof(uint16_t), 1, pFile.get());

    std::vector<uint16_t> intensity(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));
    fread(reinterpret_cast<char*>(intensity.data()), sizeof(uint16_t), m_DimensionX * m_DimensionY * m_DimensionZ, pFile.get());
    m_DimensionMipLevels = static_cast<uint16_t>(std::ceil(std::log2(std::max(std::max(m_DimensionX, m_DimensionY), m_DimensionZ)))) + 1;

    auto NormalizeIntensity = [](uint16_t intensity, uint16_t min, uint16_t max) -> uint16_t {
        return static_cast<uint16_t>(std::round(std::numeric_limits<uint16_t>::max() * ((intensity - min) / static_cast<F32>(max - min))));
    };

    uint16_t tmin = 0 << 12; // Min HU [0, 4096]
    uint16_t tmax = 1 << 12; // Max HU [0, 4096]
    for (size_t index = 0u; index < std::size(intensity); index++)
        intensity[index] = NormalizeIntensity(intensity[index], tmin, tmax);

    {
        DX::ComPtr<ID3D11Texture3D> pTextureIntensity;
        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = m_DimensionX;
        desc.Height = m_DimensionY;
        desc.Depth = m_DimensionZ;
        desc.Format = DXGI_FORMAT_R16_UNORM;
        desc.MipLevels = m_DimensionMipLevels;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;;
        DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, nullptr, pTextureIntensity.GetAddressOf()));

        for (uint32_t mipLevelID = 0; mipLevelID < desc.MipLevels; mipLevelID++) {
            D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {};
            descSRV.Format = DXGI_FORMAT_R16_UNORM;
            descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            descSRV.Texture3D.MipLevels = 1;
            descSRV.Texture3D.MostDetailedMip = mipLevelID;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRVVolumeIntensity;
            DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureIntensity.Get(), &descSRV, pSRVVolumeIntensity.GetAddressOf()));
            m_pSRVVolumeIntensity.push_back(pSRVVolumeIntensity);
        }

        for (uint32_t mipLevelID = 0; mipLevelID < desc.MipLevels; mipLevelID++) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV = {};
            descUAV.Format = DXGI_FORMAT_R16_UNORM;
            descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            descUAV.Texture3D.MipSlice = mipLevelID;
            descUAV.Texture3D.FirstWSlice = 0;
            descUAV.Texture3D.WSize = std::max(m_DimensionZ >> mipLevelID, 1);

            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVVolumeIntensity;
            DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureIntensity.Get(), &descUAV, pUAVVolumeIntensity.GetAddressOf()));
            m_pUAVVolumeIntensity.push_back(pUAVVolumeIntensity);
        }

        D3D11_BOX box = { 0, 0, 0,  desc.Width, desc.Height,  desc.Depth };
        m_pImmediateContext->UpdateSubresource(pTextureIntensity.Get(), 0, &box, std::data(intensity), sizeof(uint16_t) * desc.Width, sizeof(uint16_t) * desc.Height * desc.Width);

        for (uint32_t mipLevelID = 1; mipLevelID < desc.MipLevels - 1; mipLevelID++) {
            uint32_t threadGroupX = std::max(static_cast<uint32_t>(std::ceil((m_DimensionX >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupY = std::max(static_cast<uint32_t>(std::ceil((m_DimensionY >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupZ = std::max(static_cast<uint32_t>(std::ceil((m_DimensionZ >> mipLevelID) / 4.0f)), 1u);

            ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVVolumeIntensity[mipLevelID - 1].Get() };
            ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVVolumeIntensity[mipLevelID + 0].Get() };
            ID3D11SamplerState* ppSamplers[] = { samplers.m_pSamplerLinear.Get() };

            ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr };
            ID3D11ShaderResourceView* ppSRVClear[] = { nullptr };
            ID3D11SamplerState* ppSamplerClear[] = { nullptr };

            auto renderPassName = fmt::format("Render Pass: Compute Mip Map [{}] ", mipLevelID);
            auto renderPassNameWide = std::wstring(renderPassName.begin(), renderPassName.end());
            deviceResource->PIXBeginEvent(renderPassNameWide.c_str());
            shaders.m_PSOGenerateMipLevel.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
            deviceResource->PIXEndEvent();
        }
        m_pImmediateContext->Flush();
    }

    {
        DX::ComPtr<ID3D11Texture3D> pTextureGradient;
        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = m_DimensionX;
        desc.Height = m_DimensionY;
        desc.Depth = m_DimensionZ;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.MipLevels = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, nullptr, pTextureGradient.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureGradient.Get(), nullptr, m_pSRVGradient.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureGradient.Get(), nullptr, m_pUAVGradient.ReleaseAndGetAddressOf()));
        {
            uint32_t threadGroupX = static_cast<uint32_t>(std::ceil(m_DimensionX / 4.0f));
            uint32_t threadGroupY = static_cast<uint32_t>(std::ceil(m_DimensionY / 4.0f));
            uint32_t threadGroupZ = static_cast<uint32_t>(std::ceil(m_DimensionZ / 4.0f));

            ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVVolumeIntensity[0].Get(), m_pSRVOpacityTF.Get() };
            ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVGradient.Get() };
            ID3D11SamplerState* ppSamplers[] = { samplers.m_pSamplerPoint.Get(), samplers.m_pSamplerLinear.Get() };

            ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr };
            ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr };
            ID3D11SamplerState* ppSamplerClear[] = { nullptr, nullptr };

            deviceResource->PIXBeginEvent(L"Render Pass: Compute Gradient");
            shaders.m_PSOComputeGradient.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
            deviceResource->PIXEndEvent();
        }
        m_pImmediateContext->Flush();
    }
}
