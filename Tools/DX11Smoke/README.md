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
The EterLib bridge and indexed checkerboard resource test were subsequently
built and visually verified by the user (Release x64, zero errors and warnings).

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
- EterLib renderer bridge has its own regression test (below), verified by the user.

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

Expected after GPU resources update: **solid dark teal background with a
centered gold-and-blue checkerboard square**, drawn using an immutable indexed
quad, 16-bit index buffer, RGBA8 texture, SM5 shaders and explicit DX11 states.
Resize, minimize/restore and close should still work. The user verified
both the solid-teal and indexed checkerboard BridgeProbe scenes (Release x64). The hardware DX11 renderer in EterLib
does **not** silently fall back to WARP. If it fails, collect the HRESULT from
the Visual Studio Debug Output and do not enable DX11 in the main client.

Optional: independently compile the x64 EterLib static library in VS2022:

```powershell
& $msbuild ".\Src\EterLib\eterlib.vcxproj" /m /p:Configuration=Release /p:Platform=x64
```

The existing `build-smoke.ps1` is optional. If Windows blocks unsigned .ps1
scripts, use the `MSBuild.exe` command directly; do not change system-wide
PowerShell execution policy just to run this test.

## Resource layer phase (user-verified)

Source of truth:
- `Src/EterLib/Renderer/DX11RenderResources.h`
- `Src/EterLib/Renderer/DX11RenderResources.cpp`

This module compiles GPU vertex/index buffers, RGBA8 shader resource views,
Shader Model 5.0 vertex/pixel shaders and a point sampler. Both
`DX11BridgeProbe.vcxproj` and the x64 `eterlib.vcxproj` compile the shared
resource implementation (no need for an additional folder or repository).

From the same project directory, run the two build checks again:

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild ".\Tools\DX11Smoke\DX11BridgeProbe.vcxproj" /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "DX11BridgeProbe failed" }
& ".\Tools\DX11Smoke\bin\x64\Release\DX11BridgeProbe.exe"
if ($LASTEXITCODE -ne 0) { throw "DX11BridgeProbe runtime failed" }
& $msbuild ".\Src\EterLib\eterlib.vcxproj" /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "EterLib build failed" }
```

The bridge test uses the hardware backend, **not** WARP. Success here
still does not imply that Metin2 gameplay, Granny, SpeedTree, locale,
networking or UI have been ported to DX11.

## Terrain proof of concept (NEW: not yet built or run)

- `Tools/DX11Smoke/DX11TerrainProbe.vcxproj` is a separate Visual Studio 2022
  x64 executable; it does not overwrite the passing BridgeProbe.
- `Src/EterLib/Renderer/DX11TerrainMesh.h/.cpp` converts a row-major
  `HeightField` into 3D vertices (position/normal/UV) and 16-bit triangle indices.
- `TerrainProbe.cpp` generates a **procedural** 49x49 heightfield, uploads
  an indexed mesh, samples a green grass texture, computes directional lighting
  with SM5 shaders, and uses a real depth-stencil texture/view with a
  left-handed camera and perspective projection.
- Resize recreates the depth buffer. Press **W** to toggle wireframe; it should
  reveal the grid's triangles. A small CPU mesh self-test runs before creating
  the device.
- This is **not yet** connected to the real Metin2 `GameLib` terrain. It does
  not load game heightmaps or JTX/Nidavelir2 map packages.
- The reference Metin2 terrain currently uses 16x16 patches (`TERRAIN_PATCHSIZE`),
  so the actual terrain bridge will need a patch adapter and texture splatting
  strategy before rendering any game maps.

Build from the EXISTING `Project_ClientSrc` folder (no new folder needed):

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild ".\Tools\DX11Smoke\DX11TerrainProbe.vcxproj" /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "DX11TerrainProbe build failed" }
& ".\Tools\DX11Smoke\bin\x64\Release\DX11TerrainProbe.exe"
if ($LASTEXITCODE -ne 0) { throw "DX11TerrainProbe runtime failed" }
& $msbuild ".\Src\EterLib\eterlib.vcxproj" /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "x64 EterLib build failed" }
```

**Expected output:** a 3D rolling green landscape viewed at an angle against
a dark blue background. **W** switches to wireframe. Test resizing,
minimizing/restoring and closing the window. Capture compiler output and a
screenshot before accepting this gate. If shader compilation fails, use
Visual Studio Output/DebugView to read the compilation error.

The original DX11BridgeProbe and old DX9 game source remain intact. A green
terrain preview does not prove that the game's actual terrain, UI, Granny or
SpeedTree renderers have been migrated.
