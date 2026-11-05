#pragma once
// Special x64 Release diagnostic: read ONLY original EterPack map data.
// The caller must initialize CLZO/CEterPackManager using the existing
// PackInitialize("pack") BEFORE entering this function.
// No GameLib, Python, D3D9 or D3D11 devices in this diagnostic.
namespace m2::render
{
    bool DX11MapPackInspectRequested() noexcept;
    int RunDX11MapPackInspect() noexcept;
}
