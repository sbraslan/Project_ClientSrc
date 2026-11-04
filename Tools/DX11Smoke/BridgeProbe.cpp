// Builds the actual EterLib DX11RenderDevice in a standalone test executable.
// The existing DX11Smoke.exe and Metin2 DX9 client are unchanged.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "../../Src/EterLib/Renderer/DX11RenderDevice.h"
#include "../../Src/EterLib/Renderer/DX11RenderResources.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    m2::render::DX11RenderDevice g_renderer;
    m2::render::DX11MeshBuffers g_quad;
    m2::render::DX11Texture g_texture;
    m2::render::DX11ShaderPipeline g_shaders;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> g_sampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> g_rasterizer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> g_noDepth;
    bool g_ready = false;

    struct Vertex
    {
        float position[3];
        float color[4];
        float uv[2];
    };

    constexpr Vertex kVertices[] =
    {
        {{-0.68f,  0.68f, 0.0f}, {1, 1, 1, 1}, {0, 0}},
        {{ 0.68f,  0.68f, 0.0f}, {1, 1, 1, 1}, {1, 0}},
        {{-0.68f, -0.68f, 0.0f}, {1, 1, 1, 1}, {0, 1}},
        {{ 0.68f, -0.68f, 0.0f}, {1, 1, 1, 1}, {1, 1}},
    };
    constexpr std::uint16_t kIndices[] = {0, 1, 2, 2, 1, 3};

    constexpr char kHLSL[] = R"HLSL(
        Texture2D image : register(t0);
        SamplerState imageSampler : register(s0);
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR0;
            float2 uv : TEXCOORD0;
        };
        struct PSInput {
            float4 position : SV_POSITION;
            float4 tint : COLOR0;
            float2 uv : TEXCOORD0;
        };
        PSInput VSMain(VSInput input) {
            PSInput output;
            output.position = float4(input.position, 1.0f);
            output.tint = input.color;
            output.uv = input.uv;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET {
            return image.Sample(imageSampler, input.uv) * input.tint;
        }
    )HLSL";

    HRESULT InitializeDemoResources()
    {
        using m2::render::DX11RenderResources;
        ID3D11Device* device = g_renderer.NativeDevice();
        if (!device)
            return E_UNEXPECTED;

        HRESULT hr = DX11RenderResources::CreateImmutableMesh(
            device, kVertices, static_cast<UINT>(sizeof(Vertex)),
            ARRAYSIZE(kVertices), kIndices, ARRAYSIZE(kIndices), g_quad);
        if (FAILED(hr))
            return hr;

        constexpr D3D11_INPUT_ELEMENT_DESC elements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = DX11RenderResources::CreateShaderPipeline(
            device, kHLSL, "VSMain", "PSMain",
            elements, ARRAYSIZE(elements), g_shaders);
        if (FAILED(hr))
            return hr;

        constexpr UINT side = 64;
        std::array<std::uint8_t, side * side * 4> pixels{};
        for (UINT y = 0; y < side; ++y)
        {
            for (UINT x = 0; x < side; ++x)
            {
                const bool bright = ((x / 8u) + (y / 8u)) % 2u == 0u;
                const std::size_t offset = (static_cast<std::size_t>(y) * side + x) * 4u;
                pixels[offset + 0] = bright ? 250 : 35;
                pixels[offset + 1] = bright ? 190 : 110;
                pixels[offset + 2] = bright ? 55 : 190;
                pixels[offset + 3] = 255;
            }
        }
        hr = DX11RenderResources::CreateRGBA8Texture(
            device, side, side, pixels.data(), g_texture);
        if (FAILED(hr))
            return hr;

        hr = DX11RenderResources::CreatePointSampler(device, g_sampler);
        if (FAILED(hr))
            return hr;

        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&rasterizer, &g_rasterizer);
        if (FAILED(hr))
            return hr;

        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = FALSE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        return device->CreateDepthStencilState(&depth, &g_noDepth);
    }

    void DrawDemo()
    {
        ID3D11DeviceContext* ctx = g_renderer.NativeContext();
        if (!ctx || !g_quad.vertices || !g_quad.indices)
            return;

        const UINT offset = 0;
        ID3D11Buffer* vertex = g_quad.vertices.Get();
        ID3D11ShaderResourceView* texture = g_texture.view.Get();
        ID3D11SamplerState* sampler = g_sampler.Get();

        ctx->RSSetState(g_rasterizer.Get());
        ctx->OMSetDepthStencilState(g_noDepth.Get(), 0);
        ctx->IASetInputLayout(g_shaders.inputLayout.Get());
        ctx->IASetVertexBuffers(0, 1, &vertex, &g_quad.stride, &offset);
        ctx->IASetIndexBuffer(g_quad.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(g_shaders.vertex.Get(), nullptr, 0);
        ctx->PSSetShader(g_shaders.pixel.Get(), nullptr, 0);
        ctx->PSSetShaderResources(0, 1, &texture);
        ctx->PSSetSamplers(0, 1, &sampler);
        ctx->DrawIndexed(g_quad.indexCount, 0, 0);
    }

    void ReleaseDemo()
    {
        if (auto* ctx = g_renderer.NativeContext())
        {
            ctx->ClearState();
            ctx->Flush();
        }
        g_noDepth.Reset();
        g_rasterizer.Reset();
        g_sampler.Reset();
        g_texture.Reset();
        g_shaders.Reset();
        g_quad.Reset();
    }

    void LogHR(const wchar_t* operation, HRESULT hr)
    {
        wchar_t message[240]{};
        wsprintfW(message, L"%s failed: 0x%08X\n", operation, static_cast<unsigned>(hr));
        OutputDebugStringW(message);
    }

    LRESULT CALLBACK OnWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message)
        {
        case WM_SIZE:
            if (g_ready && wparam != SIZE_MINIMIZED)
            {
                const HRESULT hr = g_renderer.Resize(
                    static_cast<unsigned>(LOWORD(lparam)),
                    static_cast<unsigned>(HIWORD(lparam)));
                if (FAILED(hr))
                {
                    LogHR(L"DX11 bridge resize", hr);
                    PostQuitMessage(4);
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    constexpr wchar_t className[] = L"Metin2DX11BridgeProbe";
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = OnWindowMessage;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassW(&wc))
        return 1;

    HWND window = CreateWindowExW(
        0, className, L"EterLib - DX11 Renderer Bridge Probe",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 600,
        nullptr, nullptr, instance, nullptr);
    if (!window)
        return 2;

    ShowWindow(window, showCommand);

    RECT client{};
    GetClientRect(window, &client);
    HRESULT hr = g_renderer.Initialize(
        window,
        static_cast<unsigned>(client.right - client.left),
        static_cast<unsigned>(client.bottom - client.top));
    if (FAILED(hr))
    {
        LogHR(L"EterLib DX11 Initialize", hr);
        MessageBoxW(window, L"EterLib DX11 renderer initialization failed. Check debug output.",
                    L"DX11 Bridge Probe", MB_ICONERROR | MB_OK);
        DestroyWindow(window);
        return 3;
    }

    hr = InitializeDemoResources();
    if (FAILED(hr))
    {
        LogHR(L"EterLib DX11 resource initialization", hr);
        ReleaseDemo();
        g_renderer.Shutdown();
        DestroyWindow(window);
        return 6;
    }

    g_ready = true;
    MSG message{};
    int exitCode = 0;

    while (true)
    {
        bool quitting = false;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                exitCode = static_cast<int>(message.wParam);
                quitting = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (quitting)
            break;
        if (IsIconic(window))
        {
            WaitMessage();
            continue;
        }

        // Expected: teal background with a centered checker-textured quad.
        constexpr float clear[4] = {0.03f, 0.20f, 0.24f, 1.0f};
        hr = g_renderer.Clear(clear);
        if (SUCCEEDED(hr))
        {
            DrawDemo();
            hr = g_renderer.Present(true);
        }

        if (FAILED(hr))
        {
            LogHR(L"EterLib DX11 Clear/Present", hr);
            if (hr == DXGI_ERROR_DEVICE_REMOVED)
                LogHR(L"DX11 device removed reason", g_renderer.GetDeviceRemovedReason());
            exitCode = 5;
            break;
        }
    }

    g_ready = false;
    ReleaseDemo();
    g_renderer.Shutdown();
    if (IsWindow(window))
        DestroyWindow(window);
    return exitCode;
}
