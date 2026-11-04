# DX11 Smoke Test (experimental; NOT the game renderer)

This directory contains a **standalone Windows x64** DirectX 11 test target.
It does not link against or modify Metin2's DX9 `GrpDevice`, `StateManager`,
client DLLs, Granny or SpeedTree code.

## Purpose

Gate 1 bootstrap: validate a minimum DX11 device (feature level 11_0),
DXGI flip swapchain, HLSL shader model 5.0 compilation, and a colored triangle.
The smoke test tries the hardware GPU first and falls back to WARP for
development diagnosis. A WARP success is **not** proof of hardware support.

**This source has not been compiled or executed in the author's environment.**
Do not mistake existence in GitHub for a passing build.

## Build (Visual Studio 2022)

Requirements: Windows 10/11, VS2022 **Desktop development with C++**, MSVC v143,
Windows 10/11 SDK, Direct3D 11 runtime. The Windows SDK supplies the headers
and import libraries for D3D11, DXGI and D3DCompiler.

From a Developer PowerShell for VS2022 (at the repository root):

```powershell
msbuild ".\Tools\DX11Smoke\DX11Smoke.vcxproj" /m /p:Configuration=Release /p:Platform=x64
```

Run:

```powershell
& ".\Tools\DX11Smoke\bin\x64\Release\DX11Smoke.exe"
```

Expected: blue/dark window and a red/green/blue triangle. Resize/maximize,
minimize/restore and close the window to exercise swapchain and lifetime paths.
The colored triangle is **not yet a textured triangle**; texture loading will
be a separate milestone.

If it fails, capture the **full build output**, the Windows error code and
Visual Studio Output/DebugView output (HRESULT). When a machine lacks the DX11
debug layer, Debug builds retry without it.

## Do not merge into the client yet

- No Metin2 solution/project references.
- No changes to DX9 classes or client/server packet structures.
- No assumptions that JTX/Nidavelir2 `.psc`/`.vsc` files are compatible.
- No DX9-vs-DX11 FPS conclusion without in-game measurements.
- Remaining Gate 1 milestones: actual Windows build, screenshots, textured draw,
  device-recovery handling and hardware-vs-WARP confirmation.

See `../../docs/dx11/MIGRATION_AUDIT_V1.md` for design decisions and gates.
