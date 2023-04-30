#include "pch.h"
#include "MCShaders.h"
#include <string>

MCShaders::MCShaders(DX::ComPtr<ID3D11Device1> m_pDevice) {
    auto compileShader = [](auto fileName, auto entrypoint, auto target, auto macros) -> DX::ComPtr<ID3DBlob> {
        DX::ComPtr<ID3DBlob> pCodeBlob;
        DX::ComPtr<ID3DBlob> pErrorBlob;

        uint32_t flags = 0;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        if (FAILED(D3DCompileFromFile(fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target, flags, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
            throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
        return pCodeBlob;
    };

    //TODO AMD 8x8x1 NV 8x4x1
    std::string threadSizeX = std::to_string(8);
    std::string threadSizeY = std::to_string(8);

    D3D_SHADER_MACRO macros[] = {
        {"THREAD_GROUP_SIZE_X", threadSizeX.c_str()},
        {"THREAD_GROUP_SIZE_Y", threadSizeY.c_str()},
        { nullptr, nullptr}
    };

    auto pBlobCSGeneratePrimaryRays = compileShader(L"data/shaders/GenerateRays.hlsl", "GenerateRays", "cs_5_0", macros);
    auto pBlobCSComputeDiffuseLight = compileShader(L"data/shaders/ComputeRadiance.hlsl", "ComputeRadiance", "cs_5_0", macros);
    auto pBlobCSAccumulate = compileShader(L"data/shaders/Accumulation.hlsl", "Accumulate", "cs_5_0", macros);
    auto pBlobCSComputeTiles = compileShader(L"data/shaders/ComputeTiles.hlsl", "ComputeTiles", "cs_5_0", macros);
    auto pBlobCSToneMap = compileShader(L"data/shaders/ToneMap.hlsl", "ToneMap", "cs_5_0", macros);
    auto pBlobCSComputeGradient = compileShader(L"data/shaders/Gradient.hlsl", "ComputeGradient", "cs_5_0", macros);
    auto pBlobCSGenerateMipLevel = compileShader(L"data/shaders/LevelOfDetail.hlsl", "GenerateMipLevel", "cs_5_0", macros);
    auto pBlobCSResetTiles = compileShader(L"data/shaders/ResetTiles.hlsl", "ResetTiles", "cs_5_0", macros);
    auto pBlobVSBlit = compileShader(L"data/shaders/Blitting.hlsl", "BlitVS", "vs_5_0", macros);
    auto pBlobPSBlit = compileShader(L"data/shaders/Blitting.hlsl", "BlitPS", "ps_5_0", macros);
    auto pBlobVSDebugTiles = compileShader(L"data/shaders/Debug.hlsl", "DebugTilesVS", "vs_5_0", macros);
    auto pBlobGSDebugTiles = compileShader(L"data/shaders/Debug.hlsl", "DebugTilesGS", "gs_5_0", macros);
    auto pBlobPSDegugTiles = compileShader(L"data/shaders/Debug.hlsl", "DebugTilesPS", "ps_5_0", macros);


    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGeneratePrimaryRays->GetBufferPointer(), pBlobCSGeneratePrimaryRays->GetBufferSize(), nullptr, m_PSOGeneratePrimaryRays.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeDiffuseLight->GetBufferPointer(), pBlobCSComputeDiffuseLight->GetBufferSize(), nullptr, m_PSOComputeDiffuseLight.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSAccumulate->GetBufferPointer(), pBlobCSAccumulate->GetBufferSize(), nullptr, m_PSOAccumulate.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeTiles->GetBufferPointer(), pBlobCSComputeTiles->GetBufferSize(), nullptr, m_PSOComputeTiles.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_PSOToneMap.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGenerateMipLevel->GetBufferPointer(), pBlobCSGenerateMipLevel->GetBufferSize(), nullptr, m_PSOGenerateMipLevel.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeGradient->GetBufferPointer(), pBlobCSComputeGradient->GetBufferSize(), nullptr, m_PSOComputeGradient.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSResetTiles->GetBufferPointer(), pBlobCSResetTiles->GetBufferSize(), nullptr, m_PSOResetTiles.pCS.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_PSOBlit.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_PSOBlit.pPS.ReleaseAndGetAddressOf()));
    m_PSOBlit.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSDebugTiles->GetBufferPointer(), pBlobVSDebugTiles->GetBufferSize(), nullptr, m_PSODebugTiles.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateGeometryShader(pBlobGSDebugTiles->GetBufferPointer(), pBlobGSDebugTiles->GetBufferSize(), nullptr, m_PSODebugTiles.pGS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSDegugTiles->GetBufferPointer(), pBlobPSDegugTiles->GetBufferSize(), nullptr, m_PSODebugTiles.pPS.ReleaseAndGetAddressOf()));
    m_PSODebugTiles.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
}