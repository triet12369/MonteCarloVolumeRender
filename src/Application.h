//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "volume/MCVolumeRenderer.h"
#include "directxtk/Mouse.h"


// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Application final : public DX::IDeviceNotify
{
public:

    Application() noexcept(false);
    ~Application() = default;

    Application(Application&&) = default;
    Application& operator= (Application&&) = default;

    Application(Application const&) = delete;
    Application& operator= (Application const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::shared_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    std::unique_ptr<MCVolumeRenderer> m_volumeRenderer;

    // Mouse
    std::unique_ptr<DirectX::Mouse> m_mouse;
};
