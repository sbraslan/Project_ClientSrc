# DX11 Smoke Test (experimental; NOT the game renderer)

This directory contains a **standalone Windows x64** DirectX 11 test target.
It does not link against or modify Metin2's DX9 `GrpDevice`, `StateManager`,
client DLLs, Granny or SpeedTree code.

## Purpose

Gate 1 bootstrap: validate a minimum DX11 device (feature level 11_0),
DXGI flip swapchain, HLSL shader model 5.0 compilation, and a checker-textured triangle.
The smoke test tries the hardware GPU first and falls back to WARP for
development diagnosis. A WARP success is **not** proof of hardware support.

**Validation note (2026-11-04):** The user reported a successful Release x64
MSBuild (0 errors / 0 warnings) and confirmed that the texture triangle was
visible. This was not independently compiled or run in this environment.
The separate EterLib DX11 bridge has NOT yet passed a user build/run test.

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

Expected: a dark window with a checker-textured triangle. Resize/maximize,
minimize/restore and close the window to exercise swapchain and lifetime paths.

If it fails, capture the **full build output**, the Windows error code and
Visual Studio Output/DebugView output (HRESULT). When a machine lacks the DX11
debug layer, Debug builds retry without it.

## Do not merge into the client yet

- No Metin2 solution/project references.
- No changes to DX9 classes or client/server packet structures.
- No assumptions that JTX/Nidavelir2 `.psc`/`.vsc` files are compatible.
- No DX9-vs-DX11 FPS conclusion without in-game measurements.
- Remaining Gate 1 milestones: independently recorded screenshot/build evidence,
  device-recovery handling and hardware-vs-WARP confirmation.
- EterLib renderer bridge has its own smoke test (below), not yet verified.

See `../../docs/dx11/MIGRATION_AUDIT_V1.md` for design decisions and gates.

## EterLib DX11 renderer bridge (separate regression test)

The new `Src/EterLib/Renderer/DX11RenderDevice.cpp` is listed in the x64
`Src/EterLib/eterlib.vcxproj` project. It is **not yet called** from the
Metin2 game. Existing DX9 classes remain untouched.

The `DX11BridgeProbe.vcxproj` compiles that **same EterLib source file** into
a standalone test program. This isolates renderer lifecycle checks and avoids
starting DX9 and DX11 swapchains on the same Metin2 window.

From the existing `Project_ClientSrc` repository directory in PowerShell:

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild ".\Tools\DX11Smoke\DX11BridgeProbe.vcxproj" /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "DX11BridgeProbe build failed" }
& ".\Tools\DX11Smoke\bin\x64\Release\DX11BridgeProbe.exe"
```

Expected: **solid dark teal window** (not checker triangle), with working
resize, minimize/restore and clean close. The hardware DX11 renderer in EterLib
does **not** silently fall back to WARP. If it fails, collect the HRESULT from
the Visual Studio Debug Output and do not enable DX11 in the main client.

Optional: independently compile the x64 EterLib static library in VS2022:

```powershell
& $msbuild ".\Src\EterLib\eterlib.vcxproj" /m /p:Configuration=Release /p:Platform=x64
```

The existing `build-smoke.ps1` is optional. If Windows blocks unsigned .ps1
scripts, use the `MSBuild.exe` command directly; do not change system-wide
PowerShell execution policy just to run this test.
