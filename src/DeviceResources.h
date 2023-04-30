//
// DeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

namespace DX
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    interface IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        ~IDeviceNotify() = default;
    };

    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static constexpr unsigned int c_FlipPresent  = 0x1;
        static constexpr unsigned int c_AllowTearing = 0x2;
        static constexpr unsigned int c_EnableHDR    = 0x4;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_10_0,
                        unsigned int flags = c_FlipPresent) noexcept;
        ~DeviceResources() = default;

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Present();
        void UpdateColorSpace();

        // Device Accessors.
        RECT GetOutputSize() const noexcept { return m_outputSize; }

        // Direct3D Accessors.
        auto                    GetD3DDevice() const noexcept           { return m_d3dDevice.Get(); }
        auto                    GetD3DDeviceContext() const noexcept    { return m_d3dContext.Get(); }
        auto                    GetSwapChain() const noexcept           { return m_swapChain.Get(); }
        auto                    GetDXGIFactory() const noexcept         { return m_dxgiFactory.Get(); }
        HWND                    GetWindow() const noexcept              { return m_window; }
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const noexcept  { return m_d3dFeatureLevel; }
        ID3D11Texture2D*        GetRenderTarget() const noexcept        { return m_renderTarget.Get(); }
        ID3D11Texture2D*        GetDepthStencil() const noexcept        { return m_depthStencil.Get(); }
        ID3D11RenderTargetView*	GetRenderTargetView() const noexcept    { return m_d3dRenderTargetView.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView() const noexcept    { return m_d3dDepthStencilView.Get(); }
        DXGI_FORMAT             GetBackBufferFormat() const noexcept    { return m_backBufferFormat; }
        DXGI_FORMAT             GetDepthBufferFormat() const noexcept   { return m_depthBufferFormat; }
        D3D11_VIEWPORT          GetScreenViewport() const noexcept      { return m_screenViewport; }
        UINT                    GetBackBufferCount() const noexcept     { return m_backBufferCount; }
        DXGI_COLOR_SPACE_TYPE   GetColorSpace() const noexcept          { return m_colorSpace; }
        unsigned int            GetDeviceOptions() const noexcept       { return m_options; }

        // Performance events
        void PIXBeginEvent(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->BeginEvent(name);
        }

        void PIXEndEvent()
        {
            m_d3dAnnotation->EndEvent();
        }

        void PIXSetMarker(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->SetMarker(name);
        }

    private:
        void CreateFactory();
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);

        // Direct3D objects.
        ComPtr<IDXGIFactory2>               m_dxgiFactory;
        ComPtr<ID3D11Device1>               m_d3dDevice;
        ComPtr<ID3D11DeviceContext1>        m_d3dContext;
        ComPtr<IDXGISwapChain1>             m_swapChain;
        ComPtr<ID3DUserDefinedAnnotation>   m_d3dAnnotation;

        // Direct3D rendering objects. Required for 3D.
        ComPtr<ID3D11Texture2D>         m_renderTarget;
        ComPtr<ID3D11Texture2D>         m_depthStencil;
        ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView;
        ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView;
        D3D11_VIEWPORT                                  m_screenViewport;

        // Direct3D properties.
        DXGI_FORMAT                                     m_backBufferFormat;
        DXGI_FORMAT                                     m_depthBufferFormat;
        UINT                                            m_backBufferCount;
        D3D_FEATURE_LEVEL                               m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                            m_window;
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel;
        RECT                                            m_outputSize;

        // HDR Support
        DXGI_COLOR_SPACE_TYPE                           m_colorSpace;

        // DeviceResources options (see flags above)
        unsigned int                                    m_options;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify*                                  m_deviceNotify;
    };

    template<typename T>
    auto CreateConstantBuffer(ComPtr<ID3D11Device> pDevice) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {
        ComPtr<ID3D11Buffer> pBuffer;
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(T);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ThrowIfFailed(pDevice->CreateBuffer(&desc, nullptr, pBuffer.GetAddressOf()));
        return pBuffer;
    }

    template<typename T>
    auto CreateIndirectBuffer(ComPtr<ID3D11Device> pDevice, const T& pInitialData) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {
        ComPtr<ID3D11Buffer> pBuffer;
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(T);
        desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        desc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = &pInitialData;

        ThrowIfFailed(pDevice->CreateBuffer(&desc, &data, pBuffer.GetAddressOf()));
        return pBuffer;
    }

    template<typename T>
    auto CreateStructuredBuffer(ComPtr<ID3D11Device> pDevice, uint32_t numElements, bool isCPUWritable, bool isGPUWritable, const T* pInitialData = nullptr) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {
        ComPtr<ID3D11Buffer> pBuffer;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(T) * numElements;
        if ((!isCPUWritable) && (!isGPUWritable)) {
            desc.CPUAccessFlags = 0;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
        }
        else if (isCPUWritable && (!isGPUWritable)) {
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.Usage = D3D11_USAGE_DYNAMIC;
        }
        else if ((!isCPUWritable) && isGPUWritable) {
            desc.CPUAccessFlags = 0;
            desc.BindFlags = (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            desc.Usage = D3D11_USAGE_DEFAULT;
        }
        else {
            assert((!(isCPUWritable && isGPUWritable)));
        }

        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(T);

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = pInitialData;
        ThrowIfFailed(pDevice->CreateBuffer((&desc), (pInitialData) ? (&data) : nullptr, pBuffer.GetAddressOf()));
        return pBuffer;
    }

    template<typename DataType>
    class MapHelper {
    public:
        MapHelper() {}

        MapHelper(ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Buffer> pBuffer, D3D11_MAP mapType, uint32_t mapFlags)
            : MapHelper() {
            Map(pContext, pBuffer, mapType, mapFlags);
        }

        MapHelper(const MapHelper&) = delete;

        MapHelper& operator=(const MapHelper&) = delete;

        MapHelper(MapHelper&& rhs)
            : m_pBuffer(std::move(rhs.m_pBuffer))
            , m_pMappedData(std::move(rhs.m_pMappedData))
            , m_pContext(std::move(rhs.m_pContext))
            , m_MapType(std::move(rhs.m_MapType))
            , m_MapFlags(std::move(rhs.m_MapFlags)) {
            rhs.m_pBuffer = nullptr;
            rhs.m_pContext = nullptr;
            rhs.m_pMappedData = nullptr;
            rhs.m_MapType = static_cast<D3D11_MAP>(-1);
            rhs.m_MapFlags = static_cast<uint32_t>(-1);
        }

        MapHelper& operator = (MapHelper&& rhs) {
            m_pBuffer = std::move(rhs.m_pBuffer);
            m_pMappedData = std::move(rhs.m_pMappedData);
            m_pContext = std::move(rhs.m_pContext);
            m_MapType = std::move(rhs.m_MapType);
            m_MapFlags = std::move(rhs.m_MapFlags);
            rhs.m_pBuffer = nullptr;
            rhs.m_pContext = nullptr;
            rhs.m_pMappedData = nullptr;
            rhs.m_MapType = static_cast<D3D11_MAP>(-1);
            rhs.m_MapFlags = static_cast<uint32_t>(-1);
            return *this;
        }

        ~MapHelper() { Unmap(); }

        void Map(ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Buffer> pBuffer, D3D11_MAP mapType, uint32_t mapFlags) {
            //	assert(!m_pBuffer && !m_pMappedData && !m_pContext, "Object already mapped");
            Unmap();
#ifdef _DEBUG
            D3D11_BUFFER_DESC desc;
            pBuffer->GetDesc(&desc);
            //	assert(sizeof(DataType) <= BuffDesc.uiSizeInBytes, "Data type size exceeds buffer size");
#endif
            D3D11_MAPPED_SUBRESOURCE resource = {};
            ThrowIfFailed(pContext->Map(pBuffer.Get(), 0, mapType, mapFlags, &resource));
            m_pMappedData = static_cast<DataType*>(resource.pData);
            if (m_pMappedData != nullptr) {
                m_pContext = pContext;
                m_pBuffer = pBuffer;
                m_MapType = mapType;
                m_MapFlags = mapFlags;
            }
        }

        auto Unmap() -> void {
            if (m_pBuffer) {
                m_pContext->Unmap(m_pBuffer.Get(), 0);
                m_pBuffer = nullptr;
                m_MapType = static_cast<D3D11_MAP>(-1);
                m_MapFlags = static_cast<uint32_t>(-1);
            }
            m_pContext = nullptr;
            m_pMappedData = nullptr;
        }

        operator DataType* () { return m_pMappedData; }

        operator const DataType* () const { return m_pMappedData; }

        auto operator->() -> DataType* { return m_pMappedData; }

        auto operator->() const -> const DataType* { return m_pMappedData; }

    private:
        ComPtr<ID3D11Buffer>        m_pBuffer = nullptr;
        ComPtr<ID3D11DeviceContext> m_pContext = nullptr;
        DataType* m_pMappedData = nullptr;
        D3D11_MAP m_MapType = static_cast<D3D11_MAP>(-1);
        uint32_t  m_MapFlags = static_cast<uint32_t>(-1);
    };

    class GraphicsPSO {
    public:
        auto Apply(ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
            pDeviceContext->IASetPrimitiveTopology(PrimitiveTopology);
            pDeviceContext->IASetInputLayout(pInputLayout.Get());
            pDeviceContext->VSSetShader(pVS.Get(), nullptr, 0);
            pDeviceContext->GSSetShader(pGS.Get(), nullptr, 0);
            pDeviceContext->PSSetShader(pPS.Get(), nullptr, 0);
            pDeviceContext->RSSetState(pRasterState.Get());
            pDeviceContext->OMSetDepthStencilState(pDepthStencilState.Get(), 0);
            pDeviceContext->OMSetBlendState(pBlendState.Get(), nullptr, BlendMask);
        }

    public:
        ComPtr<ID3D11InputLayout>       pInputLayout = nullptr;
        ComPtr<ID3D11VertexShader>      pVS = nullptr;
        ComPtr<ID3D11GeometryShader>    pGS = nullptr;
        ComPtr<ID3D11PixelShader>       pPS = nullptr;
        ComPtr<ID3D11RasterizerState>   pRasterState = nullptr;
        ComPtr<ID3D11DepthStencilState> pDepthStencilState = nullptr;
        ComPtr<ID3D11BlendState>        pBlendState = nullptr;
        uint32_t                        BlendMask = 0xFFFFFFFF;
        D3D11_PRIMITIVE_TOPOLOGY        PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    };

    class ComputePSO {
    public:
        auto Apply(ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
            pDeviceContext->CSSetShader(pCS.Get(), nullptr, 0);
        }

    public:
        ComPtr<ID3D11ComputeShader> pCS = nullptr;
    };
}
