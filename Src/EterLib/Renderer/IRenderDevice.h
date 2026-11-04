#pragma once

// Renderer boundary shared by the legacy DX9 and future DX11 paths.
// Do not leak IDirect3DDevice9 / ID3D11Device into gameplay code.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstdint>

namespace m2::render
{
    enum class Backend : std::uint8_t
    {
        DX9 = 9,
        DX11 = 11
    };

    class IRenderDevice
    {
    public:
        virtual ~IRenderDevice() = default;

        virtual Backend GetBackend() const noexcept = 0;
        virtual HRESULT Initialize(HWND window, unsigned width, unsigned height) noexcept = 0;
        virtual HRESULT Resize(unsigned width, unsigned height) noexcept = 0;
        virtual HRESULT Clear(const float (&rgba)[4]) noexcept = 0;
        virtual HRESULT Present(bool vsync) noexcept = 0;
        virtual void Shutdown() noexcept = 0;
    };
}
