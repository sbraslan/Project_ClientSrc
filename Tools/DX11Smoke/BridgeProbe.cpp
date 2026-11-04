// Builds the actual EterLib DX11RenderDevice in a standalone test executable.
// The existing DX11Smoke.exe and Metin2 DX9 client are unchanged.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "../../Src/EterLib/Renderer/DX11RenderDevice.h"

namespace
{
    m2::render::DX11RenderDevice g_renderer;
    bool g_ready = false;

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

        // Expected output: a solid deep teal window, not the earlier triangle.
        constexpr float clear[4] = {0.03f, 0.20f, 0.24f, 1.0f};
        hr = g_renderer.Clear(clear);
        if (SUCCEEDED(hr))
            hr = g_renderer.Present(true);

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
    g_renderer.Shutdown();
    if (IsWindow(window))
        DestroyWindow(window);
    return exitCode;
}
