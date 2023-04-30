#include "pch.h"
#include "../DeviceResources.h"
#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Transform.hpp>
#include <Hawk/Math/Converters.hpp>
#include "MCTransferFunction.h"
#include "fmt/format.h"
#include <vector>
#include <algorithm>
#include <utility>

#pragma once

struct MCVolumeDataLoaderInitializeSamplers {
	DX::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
	DX::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
	DX::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;
};

struct MCVolumeDataLoaderInitializeShaders {
	DX::ComputePSO  m_PSOGenerateMipLevel;
	DX::ComputePSO m_PSOComputeGradient;
};

class MCVolumeDataLoader
{
	std::string fileName = "data/volume/manix.dat";
public:
	MCVolumeDataLoader(std::shared_ptr<DX::DeviceResources> deviceResource, MCVolumeDataLoaderInitializeSamplers samplers,
		MCVolumeDataLoaderInitializeShaders shaders,
		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVOpacityTF);

	using D3D11ArrayUnorderedAccessView = std::vector< DX::ComPtr<ID3D11UnorderedAccessView>>;
	using D3D11ArrayShadeResourceView = std::vector< DX::ComPtr<ID3D11ShaderResourceView>>;
	// volume texture
	D3D11ArrayShadeResourceView   m_pSRVVolumeIntensity;
	D3D11ArrayUnorderedAccessView m_pUAVVolumeIntensity;
	// volume gradient texture
	DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVGradient;
	DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVGradient;

	uint16_t m_DimensionX = 0;
	uint16_t m_DimensionY = 0;
	uint16_t m_DimensionZ = 0;
	uint16_t m_DimensionMipLevels = 0;
};

