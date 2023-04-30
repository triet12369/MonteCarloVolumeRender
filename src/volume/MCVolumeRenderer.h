#pragma once

#include "directxtk/DDSTextureLoader.h"
#include "MCTransferFunction.h"
#include "../DeviceResources.h"
#include "MCShaders.h"
#include "MCVolumeDataLoader.h"
#include <Hawk/Components/Camera.hpp>
#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Transform.hpp>
#include <Hawk/Math/Converters.hpp>
#include <Hawk/Math/Transform.hpp>
#include <random>


struct FrameBuffer {
	Hawk::Math::Mat4x4 ViewProjectionMatrix;
	Hawk::Math::Mat4x4 NormalViewMatrix;
	Hawk::Math::Mat4x4 WorldViewProjectionMatrix;
	Hawk::Math::Mat4x4 ViewMatrix;
	Hawk::Math::Mat4x4 WorldMatrix;
	Hawk::Math::Mat4x4 NormalMatrix;

	Hawk::Math::Mat4x4 InvViewProjectionMatrix;
	Hawk::Math::Mat4x4 InvNormalViewMatrix;
	Hawk::Math::Mat4x4 InvWorldViewProjectionMatrix;
	Hawk::Math::Mat4x4 InvViewMatrix;
	Hawk::Math::Mat4x4 InvWorldMatrix;
	Hawk::Math::Mat4x4 InvNormalMatrix;

	uint32_t         FrameIndex;
	float            StepSize;
	Hawk::Math::Vec2 FrameOffset;

	Hawk::Math::Vec2 InvRenderTargetDim;
	Hawk::Math::Vec2 RenderTargetDim;

	float Density;
	Hawk::Math::Vec3 BoundingBoxMin;

	float Exposure;
	Hawk::Math::Vec3 BoundingBoxMax;
};

struct DispathIndirectBuffer {
	uint32_t ThreadGroupX;
	uint32_t ThreadGroupY;
	uint32_t ThreadGroupZ;
};

struct DrawInstancedIndirectBuffer {
	uint32_t VertexCount;
	uint32_t InstanceCount;
	uint32_t VertexOffset;
	uint32_t InstanceOffset;
};

/*
* Main class for handling Monte Carlo Volume Rendering data 
*/
class MCVolumeRenderer
{
	private:
		// D3D Device Resource reference
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		// collection of transfer functions
		std::unique_ptr<MCTransferFunction> m_transferFunctions;
		// collection of shaders
		std::unique_ptr<MCShaders> m_shaders;
		// volume information
		std::unique_ptr<MCVolumeDataLoader> m_volume;
		// camera
		Hawk::Components::Camera m_Camera = {};
		Hawk::Math::Vec3 m_BoundingBoxMin = Hawk::Math::Vec3(-0.5f, -0.5f, -0.5f);
		Hawk::Math::Vec3 m_BoundingBoxMax = Hawk::Math::Vec3(+0.5f, +0.5f, +0.5f);

		// samplers
		DX::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
		DX::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
		DX::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;

		// shader resources
		using D3D11ArrayUnorderedAccessView = std::vector< DX::ComPtr<ID3D11UnorderedAccessView>>;
		using D3D11ArrayShadeResourceView = std::vector< DX::ComPtr<ID3D11ShaderResourceView>>;

		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVDiffuseTF;
		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVSpecularTF;
		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVRoughnessTF;
		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVOpacityTF;
		DX::ComPtr<ID3D11ShaderResourceView> m_pSRVEnviroment;

		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVRadiance;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVRadiance;

		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVDiffuse;
		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVSpecular;
		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVNormal;
		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVDepth;
		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVColorSum;

		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVDiffuse;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVSpecular;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVNormal;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVDepth;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVColorSum;

		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVToneMap;
		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMapPrev;

		DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVDispersionTiles;
		DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVDispersionTiles;

		//buffers
		DX::ComPtr<ID3D11Buffer> m_pConstantBufferFrame;
		DX::ComPtr<ID3D11Buffer> m_pDispathIndirectBufferArgs;
		DX::ComPtr<ID3D11Buffer> m_pDrawInstancedIndirectBufferArgs;

		// scalars
		float    m_DeltaTime = 0.0f;
		float    m_RotateSensivity = 0.25f;
		float    m_ZoomSensivity = 1.5f;
		float    m_Density = 100.0f;
		float    m_Exposure = 20.0f;
		float    m_Zoom = 1.0f;
		uint32_t m_MipLevel = 0;
		uint32_t m_StepCount = 180;
		uint32_t m_FrameIndex = 0;
		uint32_t m_SampleDispersion = 8;
		uint32_t m_SamplingCount = 256;
		uint32_t m_MaximumSamples = 256;
		uint32_t m_MinRotateSamples = 16;

		std::random_device m_RandomDevice;
		std::mt19937       m_RandomGenerator;
		std::uniform_real_distribution<float> m_RandomDistribution;

	public:
		MCVolumeRenderer(const std::shared_ptr<DX::DeviceResources> deviceResource);
		void initialize();

		void update(float deltaTime);

		void blit(DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap, DX::ComPtr<ID3D11RenderTargetView> targetView);

		void renderFrame(DX::ComPtr<ID3D11RenderTargetView> targetView);

		void handleMouseMove(float x, float y);

	private:
		// bind transfer function data to shader resources
		void generateTransferFunctionTextures(DX::ComPtr<ID3D11Device> m_pDevice);

		void initializeSamplers(DX::ComPtr<ID3D11Device> m_pDevice);

		void initializeRenderTextures(DX::ComPtr<ID3D11Device> m_pDevice);

		void initializeVolume(std::shared_ptr<DX::DeviceResources> deviceResource);

		void initializeRenderTextures();

		void initializeTileBuffers();

		void initializeBuffers();

		void initializeEnvironmentMap();

		void updateState();
};

