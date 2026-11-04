// Isolated DirectX 11 smoke test: not linked into the Metin2 client.
// Build with Visual Studio 2022, Windows 10 SDK, x64 (v143).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr wchar_t kWindowClass[] = L"Metin2DX11SmokeWindow";
    constexpr wchar_t kWindowTitle[] = L"Metin2 - Standalone DX11 Smoke Test";

    ComPtr<ID3D11Device> g_device;
    ComPtr<ID3D11DeviceContext> g_context;
    ComPtr<IDXGISwapChain1> g_swapChain;
    ComPtr<ID3D11RenderTargetView> g_backBufferRTV;
    ComPtr<ID3D11VertexShader> g_vertexShader;
    ComPtr<ID3D11PixelShader> g_pixelShader;
    ComPtr<ID3D11InputLayout> g_inputLayout;
    ComPtr<ID3D11Buffer> g_vertexBuffer;

    struct Vertex
    {
        float position[3];
        float color[4];
    };

    constexpr Vertex kTriangle[] =
    {
        {{ 0.0f,  0.65f, 0.0f}, {1.0f, 0.25f, 0.15f, 1.0f}},
        {{ 0.65f, -0.65f, 0.0f}, {0.2f, 1.0f, 0.3f, 1.0f}},
        {{-0.65f, -0.65f, 0.0f}, {0.2f, 0.45f, 1.0f, 1.0f}},
    };

    constexpr char kShaderSource[] = R"HLSL(
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
        };
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };
        VSOutput VSMain(VSInput input) {
            VSOutput result;
            result.position = float4(input.position, 1.0f);
            result.color = input.color;
            return result;
        }
        float4 PSMain(VSOutput input) : SV_TARGET {
            return input.color;
        }
    )HLSL";

    void LogFailure(const wchar_t* operation, HRESULT hr)
    {
        wchar_t buffer[256]{};
        wsprintfW(buffer, L"%s failed (HRESULT 0x%08X)\n", operation,
                 static_cast<unsigned>(hr));
        OutputDebugStringW(buffer);
    }

    HRESULT CreateBackBuffer()
    {
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(&texture));
        if (FAILED(hr))
            return hr;

        return g_device->CreateRenderTargetView(texture.Get(), nullptr,
                                               &g_backBufferRTV);
    }

    HRESULT Resize(UINT width, UINT height)
    {
        if (!g_swapChain || !width || !height)
            return S_OK;

        g_context->OMSetRenderTargets(0, nullptr, nullptr);
        g_backBufferRTV.Reset();

        HRESULT hr = g_swapChain->ResizeBuffers(
            0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
            return hr;

        return CreateBackBuffer();
    }

    HRESULT CompileShader(const char* entry, const char* target,
                          ComPtr<ID3DBlob>& output)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompile(
            kShaderSource, std::strlen(kShaderSource), "SmokeDX11.hlsl",
            nullptr, nullptr, entry, target, flags, 0, &output, &errors);

        if (errors)
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));

        return hr;
    }

    HRESULT CreatePipeline()
    {
        ComPtr<ID3DBlob> vs;
        ComPtr<ID3DBlob> ps;
        HRESULT hr = CompileShader("VSMain", "vs_5_0", vs);
        if (FAILED(hr))
            return hr;
        hr = CompileShader("PSMain", "ps_5_0", ps);
        if (FAILED(hr))
            return hr;

        hr = g_device->CreateVertexShader(
            vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vertexShader);
        if (FAILED(hr))
            return hr;

        hr = g_device->CreatePixelShader(
            ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_pixelShader);
        if (FAILED(hr))
            return hr;

        constexpr D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        hr = g_device->CreateInputLayout(
            layout, ARRAYSIZE(layout), vs->GetBufferPointer(), vs->GetBufferSize(),
            &g_inputLayout);
        if (FAILED(hr))
            return hr;

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(kTriangle);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = kTriangle;
        return g_device->CreateBuffer(&desc, &data, &g_vertexBuffer);
    }

    HRESULT InitializeDX11(HWND hwnd)
    {
        constexpr D3D_FEATURE_LEVEL requestedLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0
        };
        D3D_FEATURE_LEVEL obtainedLevel = D3D_FEATURE_LEVEL_9_1;

        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            requestedLevels, ARRAYSIZE(requestedLevels),
            D3D11_SDK_VERSION, &g_device, &obtainedLevel, &g_context);

#ifdef _DEBUG
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            OutputDebugStringW(L"DX11 debug layer unavailable; retrying without it.\n");
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                requestedLevels, ARRAYSIZE(requestedLevels),
                D3D11_SDK_VERSION, &g_device, &obtainedLevel, &g_context);
        }
#endif
        // WARP fallback is for smoke-test portability only. It does NOT
        // prove the real graphics card supports FL 11_0 or meets FPS targets.
        if (FAILED(hr))
        {
            OutputDebugStringW(L"Hardware DX11 unavailable; trying WARP.\n");
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                requestedLevels, ARRAYSIZE(requestedLevels),
                D3D11_SDK_VERSION, &g_device, &obtainedLevel, &g_context);
        }
        if (FAILED(hr))
            return hr;

        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        hr = g_device.As(&dxgiDevice);
        if (FAILED(hr))
            return hr;
        hr = dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr))
            return hr;
        hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        if (FAILED(hr))
            return hr;

        DXGI_SWAP_CHAIN_DESC1 swapDesc{};
        swapDesc.Width = 0;
        swapDesc.Height = 0;
        swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 2;
        swapDesc.Scaling = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        hr = factory->CreateSwapChainForHwnd(
            g_device.Get(), hwnd, &swapDesc, nullptr, nullptr, &g_swapChain);
        if (FAILED(hr))
            return hr;

        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        hr = CreateBackBuffer();
        if (FAILED(hr))
            return hr;
        return CreatePipeline();
    }

    HRESULT Render(HWND hwnd)
    {
        if (!g_backBufferRTV)
            return S_OK;

        RECT rect{};
        GetClientRect(hwnd, &rect);
        UINT width = static_cast<UINT>(rect.right - rect.left);
        UINT height = static_cast<UINT>(rect.bottom - rect.top);
        if (!width || !height)
            return S_OK;

        constexpr float clear[4] = {0.05f, 0.08f, 0.12f, 1.0f};
        ID3D11RenderTargetView* rtv = g_backBufferRTV.Get();
        g_context->OMSetRenderTargets(1, &rtv, nullptr);
        g_context->ClearRenderTargetView(rtv, clear);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MaxDepth = 1.0f;
        g_context->RSSetViewports(1, &viewport);

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vb = g_vertexBuffer.Get();
        g_context->IASetInputLayout(g_inputLayout.Get());
        g_context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
        g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
        g_context->Draw(ARRAYSIZE(kTriangle), 0);

        return g_swapChain->Present(1, 0); // vsync; not a benchmark
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
    {
        switch (message)
        {
        case WM_SIZE:
            if (g_swapChain && wp != SIZE_MINIMIZED)
            {
                const HRESULT hr = Resize(LOWORD(lp), HIWORD(lp));
                if (FAILED(hr))
                {
                    LogFailure(L"Resize", hr);
                    PostQuitMessage(1);
                }
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wp, lp);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    wc.lpfnWndProc = WindowProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc))
        return 1;

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 600,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd)
        return 2;

    ShowWindow(hwnd, showCommand);

    const HRESULT init = InitializeDX11(hwnd);
    if (FAILED(init))
    {
        LogFailure(L"InitializeDX11", init);
        MessageBoxW(hwnd, L"DX11 smoke test initialization failed. See debugger output.",
                    kWindowTitle, MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 3;
    }

    MSG message{};
    bool running = true;
    int exitCode = 0;
    while (running)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;
                exitCode = static_cast<int>(message.wParam);
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running)
            break;

        if (IsIconic(hwnd))
        {
            WaitMessage();
            continue;
        }
        const HRESULT hr = Render(hwnd);
        if (FAILED(hr))
        {
            LogFailure(L"Render/Present", hr);
            if (hr == DXGI_ERROR_DEVICE_REMOVED && g_device)
                LogFailure(L"DeviceRemovedReason", g_device->GetDeviceRemovedReason());
            exitCode = 4;
            break;
        }
    }

    g_context->ClearState();
    g_context->Flush();
    g_backBufferRTV.Reset();
    g_swapChain.Reset();
    g_vertexBuffer.Reset();
    g_inputLayout.Reset();
    g_vertexShader.Reset();
    g_pixelShader.Reset();
    g_context.Reset();
    g_device.Reset();
    DestroyWindow(hwnd);
    return exitCode;
}
