#include "DX11RenderDevice.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace m2::render
{
    DX11RenderDevice::~DX11RenderDevice()
    {
        Shutdown();
    }

    HRESULT DX11RenderDevice::CreateRenderTarget() noexcept
    {
        if (!device_ || !swapChain_)
            return E_UNEXPECTED;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr))
            return hr;

        hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTarget_);
        return hr;
    }

    HRESULT DX11RenderDevice::Initialize(HWND window, unsigned width, unsigned height) noexcept
    {
        if (!window || !IsWindow(window) || width == 0 || height == 0)
            return E_INVALIDARG;

        Shutdown();

        constexpr D3D_FEATURE_LEVEL requestedLevels[] = {D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;

        // Hardware only: do not silently fall back to WARP in the game.
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            requestedLevels,
            ARRAYSIZE(requestedLevels),
            D3D11_SDK_VERSION,
            &device_,
            &featureLevel,
            &context_);

        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactory2> factory;

        hr = device_.As(&dxgiDevice);
        if (SUCCEEDED(hr))
            hr = dxgiDevice->GetAdapter(&adapter);
        if (SUCCEEDED(hr))
            hr = adapter->GetParent(IID_PPV_ARGS(&factory));

        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        hr = factory->CreateSwapChainForHwnd(
            device_.Get(), window, &desc, nullptr, nullptr, &swapChain_);
        if (SUCCEEDED(hr))
            factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

        if (SUCCEEDED(hr))
            hr = CreateRenderTarget();

        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        window_ = window;
        width_ = width;
        height_ = height;
        return S_OK;
    }

    HRESULT DX11RenderDevice::Resize(unsigned width, unsigned height) noexcept
    {
        if (!swapChain_ || !context_)
            return E_UNEXPECTED;

        // Minimized windows report zero-sized client areas.
        if (width == 0 || height == 0)
            return S_OK;

        if (width == width_ && height == height_)
            return S_OK;

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        context_->ClearState();
        context_->Flush();
        renderTarget_.Reset();

        const HRESULT hr = swapChain_->ResizeBuffers(
            0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
            return hr;

        const HRESULT targetHr = CreateRenderTarget();
        if (SUCCEEDED(targetHr))
        {
            width_ = width;
            height_ = height;
        }
        return targetHr;
    }

    HRESULT DX11RenderDevice::Clear(const float (&rgba)[4]) noexcept
    {
        if (!context_ || !renderTarget_ || width_ == 0 || height_ == 0)
            return E_UNEXPECTED;

        ID3D11RenderTargetView* view = renderTarget_.Get();
        context_->OMSetRenderTargets(1, &view, nullptr);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width_);
        viewport.Height = static_cast<float>(height_);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        context_->ClearRenderTargetView(view, rgba);
        return S_OK;
    }

    HRESULT DX11RenderDevice::Present(bool vsync) noexcept
    {
        if (!swapChain_ || !renderTarget_)
            return E_UNEXPECTED;

        // Surface/device loss is reported; automatic recovery is a future gate.
        return swapChain_->Present(vsync ? 1u : 0u, 0);
    }

    HRESULT DX11RenderDevice::GetDeviceRemovedReason() const noexcept
    {
        return device_ ? device_->GetDeviceRemovedReason() : E_UNEXPECTED;
    }

    void DX11RenderDevice::Shutdown() noexcept
    {
        if (context_)
        {
            context_->ClearState();
            context_->Flush();
        }

        renderTarget_.Reset();
        swapChain_.Reset();
        context_.Reset();
        device_.Reset();

        window_ = nullptr;
        width_ = 0;
        height_ = 0;
    }
}
