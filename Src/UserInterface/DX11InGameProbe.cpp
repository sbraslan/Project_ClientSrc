// In-process DX11 transition gate: independent diagnostic HWND, never
// attach a second swapchain to the existing DX9-owned Metin2 window.
// This file is excluded from ALL normal configurations and is enabled
// ONLY for the opt-in UserInterface Release|x64 build property.
#include <cstdint>
#include <Windows.h>
#include "../EterBase/Debug.h"
#include "../EterLib/Renderer/DX11RenderDevice.h"
#include "DX11InGameProbe.h"

namespace m2::render
{
    namespace
    {
        constexpr wchar_t kProbeClass[] = L"Metin2DX11InGameProbeV1";
        constexpr unsigned kProbeWidth = 400;
        constexpr unsigned kProbeHeight = 230;

        struct ProbeState
        {
            DX11RenderDevice renderer;
            HWND window = nullptr;
            ATOM windowClass = 0;
            bool attempted = false;
            bool ready = false;
            bool disabled = false;
            std::uint64_t presents = 0;
            ULONGLONG lastPresentTick = 0;
        };

        ProbeState& State() noexcept
        {
            static ProbeState s;
            return s;
        }

        LRESULT CALLBACK ProbeWindowProc(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam)
        {
            // NEVER PostQuitMessage: that would end the actual DX9 game loop.
            if (message == WM_CLOSE)
            {
                ::DestroyWindow(window);
                return 0;
            }
            return ::DefWindowProcW(window, message, wparam, lparam);
        }

        void ReleaseProbe(ProbeState& state) noexcept
        {
            state.renderer.Shutdown();
            if (state.window && ::IsWindow(state.window))
                ::DestroyWindow(state.window);
            state.window = nullptr;
            state.ready = false;
            if (state.windowClass)
            {
                ::UnregisterClassW(kProbeClass, ::GetModuleHandleW(nullptr));
                state.windowClass = 0;
            }
        }

        bool IsExplicitlyEnabled() noexcept
        {
            char value[2]{};
            const DWORD length = ::GetEnvironmentVariableA(
                "METIN2_DX11_INGAME_PROBE", value,
                static_cast<DWORD>(sizeof(value)));
            return length == 1 && value[0] == '1';
        }

        void TryInitialize(ProbeState& state) noexcept
        {
            state.attempted = true;
            if (!IsExplicitlyEnabled())
                return;

            TraceError("[DX11 INGAME] opt-in environment detected; "
                       "DX9 main render path remains unchanged");

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.hInstance = ::GetModuleHandleW(nullptr);
            wc.lpfnWndProc = &ProbeWindowProc;
            wc.lpszClassName = kProbeClass;
            wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
            state.windowClass = ::RegisterClassExW(&wc);
            if (!state.windowClass)
            {
                TraceError("[DX11 INGAME] class registration failed; Win32=%lu; DX9 unaffected",
                           ::GetLastError());
                state.disabled = true;
                return;
            }

            state.window = ::CreateWindowExW(
                WS_EX_TOOLWINDOW, kProbeClass,
                L"Metin2 | D3D11 IN-PROCESS DIAGNOSTIC | GAME IS STILL DX9",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT,
                static_cast<int>(kProbeWidth + 16u),
                static_cast<int>(kProbeHeight + 39u),
                nullptr, nullptr, wc.hInstance, nullptr);
            if (!state.window)
            {
                TraceError("[DX11 INGAME] separate window failed; Win32=%lu; DX9 unaffected",
                           ::GetLastError());
                state.disabled = true;
                ReleaseProbe(state);
                return;
            }

            RECT rect{};
            if (!::GetClientRect(state.window, &rect) ||
                rect.right <= rect.left || rect.bottom <= rect.top)
            {
                TraceError("[DX11 INGAME] invalid probe client rectangle; DX9 unaffected");
                state.disabled = true;
                ReleaseProbe(state);
                return;
            }

            const HRESULT hr = state.renderer.Initialize(
                state.window,
                static_cast<unsigned>(rect.right - rect.left),
                static_cast<unsigned>(rect.bottom - rect.top));
            if (FAILED(hr))
            {
                TraceError("[DX11 INGAME] DX11 Initialize failed hr=0x%08lX; DX9 unaffected",
                           static_cast<unsigned long>(hr));
                state.disabled = true;
                ReleaseProbe(state);
                return;
            }

            state.ready = true;
            ::ShowWindow(state.window, SW_SHOWNOACTIVATE);
            ::UpdateWindow(state.window);
            TraceError("[DX11 INGAME] separate DX11 device + swapchain READY; "
                       "original game window and DX9 device unchanged");
        }
    }

    void DX11InGameProbeTick() noexcept
    {
        ProbeState& state = State();
        if (state.disabled)
            return;
        if (!state.attempted)
            TryInitialize(state);
        if (!state.ready)
            return;

        // The existing game's message pump also dispatches this HWND's events.
        // Closing only this window must not end the Metin2 game process.
        if (!state.window || !::IsWindow(state.window))
        {
            TraceError("[DX11 INGAME] diagnostic window closed; DX9 game continues");
            state.disabled = true;
            ReleaseProbe(state);
            return;
        }

        const ULONGLONG now = ::GetTickCount64();
        if (now - state.lastPresentTick < 100u)
            return; // throttle independent diagnostic to <= 10 fps
        state.lastPresentTick = now;

        const float phase = static_cast<float>(state.presents % 120u) / 120.0f;
        const float color[4] = {0.05f + phase * 0.15f,
                                0.13f, 0.31f + phase * 0.17f, 1.0f};
        HRESULT hr = state.renderer.Clear(color);
        if (SUCCEEDED(hr))
            hr = state.renderer.Present(false);
        if (FAILED(hr))
        {
            TraceError("[DX11 INGAME] Clear/Present failed hr=0x%08lX; "
                       "DX11 probe disabled; DX9 game continues",
                       static_cast<unsigned long>(hr));
            state.disabled = true;
            ReleaseProbe(state);
            return;
        }

        ++state.presents;
        if (state.presents == 30u)
            TraceError("[DX11 INGAME] 30 independent D3D11 Present calls passed "
                       "inside the actual game process; DX9 still active");
    }

    void DX11InGameProbeShutdown() noexcept
    {
        ProbeState& state = State();
        if (state.ready)
            TraceError("[DX11 INGAME] shutdown: successful D3D11 frames=%llu; "
                       "original DX9 device teardown remains independent",
                       static_cast<unsigned long long>(state.presents));
        ReleaseProbe(state);
        state.disabled = true;
    }
}
