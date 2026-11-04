#pragma once

#include "IRenderDevice.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace m2::render
{
    // Hardware-only DX11 device/swapchain owner. This is not enabled by the
    // Metin2 game yet. Never initialize on the legacy DX9-owned window while
    // the DX9 game renderer is active.
    class DX11RenderDevice final : public IRenderDevice
    {
    public:
        DX11RenderDevice() noexcept = default;
        ~DX11RenderDevice() override;

        DX11RenderDevice(const DX11RenderDevice&) = delete;
        DX11RenderDevice& operator=(const DX11RenderDevice&) = delete;

        Backend GetBackend() const noexcept override { return Backend::DX11; }
        HRESULT Initialize(HWND window, unsigned width, unsigned height) noexcept override;
        HRESULT Resize(unsigned width, unsigned height) noexcept override;
        HRESULT Clear(const float (&rgba)[4]) noexcept override;
        HRESULT Present(bool vsync) noexcept override;
        void Shutdown() noexcept override;

        bool IsReady() const noexcept { return static_cast<bool>(renderTarget_); }
        HRESULT GetDeviceRemovedReason() const noexcept;

        // Native handles are for renderer modules only; gameplay should use
        // IRenderDevice, not Direct3D-specific types.
        ID3D11Device* NativeDevice() const noexcept { return device_.Get(); }
        ID3D11DeviceContext* NativeContext() const noexcept { return context_.Get(); }

    private:
        HRESULT CreateRenderTarget() noexcept;

        HWND window_ = nullptr;
        unsigned width_ = 0;
        unsigned height_ = 0;
        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget_;
    };
}
