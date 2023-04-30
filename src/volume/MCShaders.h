#include "pch.h"
#include "../DeviceResources.h"

class MCShaders
{
	public:
		MCShaders(DX::ComPtr<ID3D11Device1> m_pDevice);

        DX::GraphicsPSO m_PSODefault = {};
        DX::GraphicsPSO m_PSOBlit = {};
        DX::GraphicsPSO m_PSODebugTiles = {};

        DX::ComputePSO m_PSOGeneratePrimaryRays = {};
        DX::ComputePSO m_PSOComputeDiffuseLight = {};

        DX::ComputePSO  m_PSOAccumulate = {};
        DX::ComputePSO  m_PSOComputeTiles = {};
        DX::ComputePSO  m_PSOResetTiles = {};
        DX::ComputePSO  m_PSOToneMap = {};
        DX::ComputePSO  m_PSOGenerateMipLevel = {};
        DX::ComputePSO  m_PSOComputeGradient = {};
};
