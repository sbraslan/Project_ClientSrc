#pragma once

// In-process Direct3D11 diagnostic. The original DX9 renderer remains the
// only owner of the actual Metin2 game HWND. Compiled and linked solely when
// EnableDX11InGameProbe=true for UserInterface Release|x64.
namespace m2::render
{
    void DX11InGameProbeTick() noexcept;
    void DX11InGameProbeShutdown() noexcept;
}
