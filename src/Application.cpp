//
// Application.cpp
//

#include "pch.h"
#include "Application.h"
#include <iostream>

extern void ExitApplication() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Application::Application() noexcept(false)
{
    m_deviceResources = std::make_shared<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Application::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // initialize mouse
    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);

    // initialize volume renderer
    m_volumeRenderer = std::make_unique<MCVolumeRenderer>(m_deviceResources);

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

#pragma region Frame Update
// Executes the basic game loop.
void Application::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Application::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    m_volumeRenderer->update(elapsedTime);
    auto mouse = m_mouse->GetState();


    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        //auto debug = fmt::format("[{}, {}] \n", mouse.x, mouse.y);
        //OutputDebugStringA(debug.c_str());
        m_volumeRenderer->handleMouseMove((float)mouse.x, (float)mouse.y);
    }

    m_mouse->SetMode(mouse.leftButton
        ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Application::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto swapChain = m_deviceResources->GetSwapChain();
    auto targetView = m_deviceResources->GetRenderTargetView();

    // TODO: Add your rendering code here.
    /*uint32_t frameIndex = swapChain->GetCurrentBackBufferIndex();
    m_pD3D11On12Device->AcquireWrappedResources(m_pD3D11BackBuffersDummy[frameIndex].GetAddressOf(), 1);
    m_pD3D11On12Device->ReleaseWrappedResources(m_pD3D11BackBuffersDummy[frameIndex].GetAddressOf(), 1);

    m_pD3D11On12Device->AcquireWrappedResources(m_pD3D11BackBuffers[frameIndex].GetAddressOf(), 1);*/

    m_volumeRenderer->renderFrame(targetView);

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Application::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto const viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Application::OnActivated()
{
    // TODO: Application is becoming active window.
}

void Application::OnDeactivated()
{
    // TODO: Application is becoming background window.
}

void Application::OnSuspending()
{
    // TODO: Application is being power-suspended (or minimized).
}

void Application::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Application is being power-resumed (or returning from minimize).
}

void Application::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Application::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Application::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Application window is being resized.
}

// Properties
void Application::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1920;
    height = 1080;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Application::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // TODO: Initialize device dependent objects here (independent of window size).
    device;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Application::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.
}

void Application::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
}

void Application::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
