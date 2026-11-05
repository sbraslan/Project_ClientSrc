// Exclusive DX11-only FIRST FRAME in the actual Metin2 executable.
// Compile this source only when Release|x64 EnableDX11MainWindowBootstrap=true.
// The existing normal game WinMain path is unchanged and remains DX9.
#include <cstdint> // EterBase/Debug.h declares uint32_t; include it first.
#include "DX11MainWindowBootstrap.h"
#include "../EterBase/Debug.h"
#include "../EterLib/Renderer/DX11RenderDevice.h"
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
#include "DX11ExclusiveTerrainPreview.h"
#endif

namespace m2::render
{
    namespace
    {
        constexpr wchar_t kWindowClass[] = L"Metin2DX11ExclusiveBootstrapV1";
        constexpr wchar_t kWindowTitle[] =
            L"Metin2 | DX11 EXCLUSIVE MAIN WINDOW | NO DX9 | NO GAMEPLAY";
        constexpr unsigned kClientWidth = 1024u;
        constexpr unsigned kClientHeight = 640u;

        struct BootstrapState
        {
            DX11RenderDevice renderer;
            bool ready = false;
            bool resizeFailed = false;
            HRESULT resizeFailure = S_OK;
            std::uint64_t presentedFrames = 0;
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
            DX11ExclusiveTerrainPreview terrain;
            bool terrainEnabled = false;
#endif
        };

        LRESULT CALLBACK BootstrapWindowProc(HWND hwnd, UINT message,
                                             WPARAM wparam, LPARAM lparam)
        {
            auto* state = reinterpret_cast<BootstrapState*>(
                ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (message)
            {
            case WM_SIZE:
                if (state && state->ready && wparam != SIZE_MINIMIZED)
                {
                    const unsigned width = LOWORD(lparam);
                    const unsigned height = HIWORD(lparam);
                    if (width && height)
                    {
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
                        if (state->terrainEnabled)
                            state->terrain.BeforeResize(state->renderer);
#endif
                        HRESULT hr = state->renderer.Resize(width, height);
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
                        if (SUCCEEDED(hr) && state->terrainEnabled)
                            hr = state->terrain.AfterResize(state->renderer,
                                                             width, height);
#endif
                        if (FAILED(hr))
                        {
                            state->resizeFailed = true;
                            state->resizeFailure = hr;
                            ::DestroyWindow(hwnd);
                        }
                    }
                }
                return 0;
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
            case WM_KEYDOWN:
                if (state && state->terrainEnabled)
                {
                    if (wparam == 'W')
                        state->terrain.ToggleWireframe();
                    else if (wparam >= '1' && wparam <= '3')
                        state->terrain.SetSharedLOD(
                            static_cast<unsigned>(wparam - '1'));
                    else if (wparam == 'M')
                        state->terrain.CycleNeighborLOD();
                    else
                        return ::DefWindowProcW(hwnd, message, wparam, lparam);
                    TraceError("[DX11 EXCLUSIVE TERRAIN] active LOD A=%u B=%u",
                               state->terrain.MainLOD(),
                               state->terrain.NeighborLOD());
                    return 0;
                }
                return ::DefWindowProcW(hwnd, message, wparam, lparam);
#endif
            case WM_CLOSE:
                ::DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                // Dedicated DX11-only process loop, not the DX9 game loop.
                ::PostQuitMessage(0);
                return 0;
            default:
                return ::DefWindowProcW(hwnd, message, wparam, lparam);
            }
        }
    }

    bool IsDX11ExclusiveBootstrapRequested() noexcept
    {
        // A single character 1 is required. Other/missing values fail closed.
        char value[2]{};
        const DWORD length = ::GetEnvironmentVariableA(
            "METIN2_DX11_EXCLUSIVE_BOOT", value, sizeof(value));
        return length == 1u && value[0] == '1';
    }

    int RunDX11ExclusiveBootstrap(HINSTANCE instance, int) noexcept
    {
        // The caller only enters here after both a special CLI argument and
        // the exact opt-in environment flag have been validated.
        // This path never calls Main(), CPythonApplication, or CGraphicDevice.
        OpenLogFile(true);
        TraceError("[DX11 EXCLUSIVE] entering WinMain DX11-only path; "
                   "game bootstrap, Python and D3D9 CreateDevice bypassed");

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = BootstrapWindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = kWindowClass;
        wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW, ANSI-neutral.
        wc.style = CS_HREDRAW | CS_VREDRAW;
        if (!::RegisterClassExW(&wc))
        {
            TraceError("[DX11 EXCLUSIVE] window class registration failed Win32=%lu",
                       ::GetLastError());
            return 2;
        }

        RECT outer{0, 0, static_cast<LONG>(kClientWidth),
                   static_cast<LONG>(kClientHeight)};
        constexpr DWORD style = WS_OVERLAPPEDWINDOW;
        if (!::AdjustWindowRectEx(&outer, style, FALSE, 0))
        {
            TraceError("[DX11 EXCLUSIVE] window bounds failed Win32=%lu",
                       ::GetLastError());
            ::UnregisterClassW(kWindowClass, instance);
            return 3;
        }

        HWND hwnd = ::CreateWindowExW(
            0, kWindowClass, kWindowTitle, style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            outer.right - outer.left, outer.bottom - outer.top,
            nullptr, nullptr, instance, nullptr);
        if (!hwnd)
        {
            TraceError("[DX11 EXCLUSIVE] main window creation failed Win32=%lu",
                       ::GetLastError());
            ::UnregisterClassW(kWindowClass, instance);
            return 4;
        }

        BootstrapState state;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(&state));
        RECT client{};
        const bool validSize = ::GetClientRect(hwnd, &client) != FALSE &&
                               client.right > client.left &&
                               client.bottom > client.top;
        HRESULT hr = validSize
            ? state.renderer.Initialize(
                  hwnd,
                  static_cast<unsigned>(client.right - client.left),
                  static_cast<unsigned>(client.bottom - client.top))
            : E_INVALIDARG;
        int exitCode = 0;
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
        // Third gate: even the special DX11 bootstrap defaults to a
        // clear-only frame. Source snapshots are required to opt into
        // terrain drawing; invalid input fails closed, never synthetic.
        char terrainFlag[2]{};
        const DWORD terrainFlagLength = ::GetEnvironmentVariableA(
            "METIN2_DX11_EXCLUSIVE_TERRAIN", terrainFlag,
            static_cast<DWORD>(sizeof(terrainFlag)));
        state.terrainEnabled = terrainFlagLength == 1u &&
                               terrainFlag[0] == '1';
        if (SUCCEEDED(hr) && state.terrainEnabled)
        {
            hr = state.terrain.Initialize(
                state.renderer,
                static_cast<unsigned>(client.right - client.left),
                static_cast<unsigned>(client.bottom - client.top));
            if (FAILED(hr))
            {
                exitCode = 8;
                TraceError("[DX11 EXCLUSIVE TERRAIN] capture/shader/mesh "
                           "init FAILED hr=0x%08lX; no synthetic fallback",
                           static_cast<unsigned long>(hr));
            }
            else
            {
                TraceError("[DX11 EXCLUSIVE TERRAIN] real adjacent A/B READY; "
                           "triangles LOD0=%u, LOD1=%u, LOD2=%u per patch",
                           state.terrain.TriangleCount(0u, 0u),
                           state.terrain.TriangleCount(0u, 1u),
                           state.terrain.TriangleCount(0u, 2u));
            }
        }
#endif
        if (FAILED(hr))
        {
            if (exitCode == 0)
            {
                TraceError("[DX11 EXCLUSIVE] D3D11 main-window init failed hr=0x%08lX; "
                           "no DX9 fallback or game resources started",
                           static_cast<unsigned long>(hr));
                exitCode = 5;
            }
        }
        else
        {
            state.ready = true;
            TraceError("[DX11 EXCLUSIVE] MAIN HWND D3D11 device and swapchain READY; "
                       "DX9 CreateDevice was never called");
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
            ::UpdateWindow(hwnd);

            // Only the D3D11 swapchain owns this process's visible main
            // window. This is a FIRST-FRAME gate, not working gameplay.
            bool done = false;
            while (!done)
            {
                MSG msg{};
                while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        done = true;
                        break;
                    }
                    ::TranslateMessage(&msg);
                    ::DispatchMessageW(&msg);
                }
                if (done)
                    break;
                if (::IsIconic(hwnd))
                {
                    ::WaitMessage();
                    continue;
                }

                const float cycle =
                    static_cast<float>(state.presentedFrames % 180u) / 180.0f;
                const float color[4] = {
                    0.09f + 0.27f * cycle,
                    0.16f,
                    0.36f + 0.21f * cycle,
                    1.0f
                };
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
                if (state.terrainEnabled)
                    hr = state.terrain.Draw(state.renderer, hwnd);
                else
#endif
                {
                    hr = state.renderer.Clear(color);
                    if (SUCCEEDED(hr))
                        hr = state.renderer.Present(true);
                }
                if (FAILED(hr))
                {
                    TraceError("[DX11 EXCLUSIVE] Clear/Present FAILED hr=0x%08lX",
                               static_cast<unsigned long>(hr));
                    exitCode = 6;
                    break;
                }
                ++state.presentedFrames;
                if (state.presentedFrames == 30u)
                {
                    TraceError("[DX11 EXCLUSIVE] 30 D3D11 frames on exclusive "
                               "main HWND PASSED; zero DX9 game frames");
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
                    if (state.terrainEnabled)
                        TraceError("[DX11 EXCLUSIVE TERRAIN] 30 real A/B patch "
                                   "GPU frames passed on exclusive DX11 main HWND");
#endif
                }
            }

            if (state.resizeFailed)
            {
                TraceError("[DX11 EXCLUSIVE] D3D11 resize FAILED hr=0x%08lX",
                           static_cast<unsigned long>(state.resizeFailure));
                exitCode = 7;
            }
        }

        state.ready = false;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
#if defined(M2_DX11_EXCLUSIVE_TERRAIN) && defined(_WIN64)
        state.terrain.Shutdown();
#endif
        state.renderer.Shutdown();
        if (::IsWindow(hwnd))
            ::DestroyWindow(hwnd);
        ::UnregisterClassW(kWindowClass, instance);
        TraceError("[DX11 EXCLUSIVE] shutdown: frames=%llu exit=%d; "
                   "DX9 game never initialized in this mode",
                   static_cast<unsigned long long>(state.presentedFrames),
                   exitCode);
        return exitCode;
    }
}
