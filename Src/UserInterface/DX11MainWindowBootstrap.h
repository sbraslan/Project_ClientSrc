#pragma once
// Experimental top-level DX11-only WinMain mode. Does NOT create a D3D9
// device and does NOT attempt to start Python/Granny/the game.
#include <Windows.h>

namespace m2::render
{
    bool IsDX11ExclusiveBootstrapRequested() noexcept;
    int RunDX11ExclusiveBootstrap(HINSTANCE instance, int showCommand) noexcept;
}
