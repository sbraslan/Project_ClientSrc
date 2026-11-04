# Metin2 Client - DirectX 11 Migration Audit v1
Date: 2026-11-04
Status: EARLY DX11 PROBES VERIFIED BY USER / GAME INTEGRATION UNVERIFIED

## Progress recorded 2026-11-04

- User-provided Windows MSBuild log: `Tools/DX11Smoke/DX11Smoke.vcxproj` compiled in Release x64, **0 errors / 0 warnings**.
- User confirms a checker-textured triangle was displayed. This is a user-reported runtime result, not an independently reproduced run.
- EterLib now has an optional hardware-only DX11 render device under `Src/EterLib/Renderer`, registered for x64 compilation.
- User-provided Windows MSBuild log: `Tools/DX11Smoke/DX11BridgeProbe.vcxproj` compiled in Release x64 with **0 errors / 0 warnings**. The user-provided screenshot shows the expected dark teal DX11 bridge window; the PowerShell command completed with exit code 0.
- `Src/EterLib/eterlib.vcxproj` compiled in Release x64 with **0 errors / 1 warning** (existing `GrpSubImage.cpp:104` C4267, `size_t` -> `int`). The log confirms `DX11RenderDevice.cpp` included in `EterLib.lib`.
- The bridge class uses the hardware driver type and does not fall back to WARP. Reported runtime result verifies this probe path only; there is no proof of DX11 in-game rendering yet.
- The new DX11 device is not instantiated by the production game or selected as an active renderer. DX9 `GrpDevice` / `StateManager` are unchanged.

## GPU resource layer checkpoint (unverified build, 2026-11-04)

- New `Src/EterLib/Renderer/DX11RenderResources.h/.cpp` provides RAII-owned
  immutable vertex/index buffers (16-bit index compatibility), packed RGBA8
  shader resource views, SM5 vertex/pixel shader compilation with explicit input
  layouts and a point sampler.
- `Tools/DX11Smoke/BridgeProbe.cpp` now submits an indexed textured quad through
  this shared resource layer, rather than only clearing the framebuffer.
- Both `Tools/DX11Smoke/DX11BridgeProbe.vcxproj` and the x64
  `Src/EterLib/eterlib.vcxproj` explicitly compile the shared resource source.
- **Not yet validated on the user's Windows machine.** The previously working
  clear/present probe proves the old phase, not this new shader/mesh module.
- No changes to DX9 `GrpDevice.cpp`, `StateManager.cpp`, Granny, SpeedTree,
  client/server protocol or locale.

## Terrain proof checkpoint (code staged; Windows build NOT yet verified)

- Confirmed the legacy client stores terrain as **16x16 patches**
  (`Src/PRTerrainLib/TerrainType.h`), and
  `HardwareTransformPatch_SSourceVertex` contains position and normal
  (`Src/GameLib/TerrainPatch.h`). The production patch renderer, texture
  splatting and quadtree live in `GameLib`.
- Added an independent **CPU-only** `DX11TerrainMesh` heightfield-to-indexed
  triangles builder. 16-bit index limits, grid validation, normals and UV
  generation are explicit.
- Added `Tools/DX11Smoke/DX11TerrainProbe.vcxproj` and `TerrainProbe.cpp`:
  procedural 3D terrain, LH view/perspective camera (DirectXMath), depth
  buffer (D24S8), resize rebuilding, HLSL SM5 lighting/texture pass, wireframe
  toggle (W), and CPU correctness self-tests.
- Extended the DX11 device with a **renderer-only** RTV access method so the
  3D test can attach a DSV without changing DX9 or the public abstraction.
- Registered the pure CPU mesh generator in EterLib x64, not Win32.
- **Still pending:** Windows Release x64 build, actual 3D screenshot and
  runtime resize/wireframe tests; the code has not been compiled by this
  assistant. No in-game terrain conversion is claimed.
- No changes to `GrpDevice.cpp`, `StateManager.cpp`, `GameLib`,
  DX9 path, server ABI, Granny, SpeedTree or locale.

## Scope and source of truth

- Production/legacy client source: `sbraslan/Project_ClientSrc`, starting branch `migration/client-x64`.
- Isolated migration branch: `modernization/dx11-renderer` (created from `migration/client-x64`).
- Reference only: `sbraslan/Project_Nidavelir2` at `import/nidavelir2-reference`.
- Reference only: `sbraslan/Project_Grafik` at `import/jtx-v1.6-original`.
- No attempt is made to import the JTX/Nidavelir2 source trees wholesale.
- This audit inspected **selected files** through GitHub; it is **not** a complete project-wide dependency count.

## Confirmed DX9 coupling (client x64)

| Module/file | Observed DX9 API dependency | Migration consequence |
| --- | --- | --- |
| `Src/EterLib/GrpDevice.cpp` | `Direct3DCreate9`, `IDirect3DDevice9::Reset`, `TestCooperativeLevel`, backbuffer handling | Separate device/swapchain lifetime and resize/error recovery before DX11 activation |
| `Src/EterLib/GrpBase.cpp` | Static `LPDIRECT3DDEVICE9`, `D3DPRESENT_PARAMETERS`, `ID3DXMatrixStack`, DX9 buffers/meshes | Introduce renderer-owned resource handles and a matrix compatibility strategy |
| `Src/EterLib/StateManager.cpp` | `D3DLIGHT9`, `SetLight`, `SetRenderState`, `SetTextureStageState`, `SetSamplerState` | Fixed-function state does not map 1:1 to DX11; define explicit blend/depth/rasterizer/sampler/shader state |
| `Src/GameLib/MapOutdoorCharacterShadow.cpp` | `CreateTexture(... D3DUSAGE_RENDERTARGET ...)`, `CreateDepthStencilSurface`, `SetRenderTarget` | Implement DX11 DSV/RTV/SRV resources and shadow pass |
| `Src/EterGrnLib/ModelInstanceRender.cpp` | `LPDIRECT3DVERTEXBUFFER9`, `LPDIRECT3DINDEXBUFFER9`, `SetFVF`, `SetStreamSource` | Granny file/animation logic may stay, but GPU resource/render path must change |
| `Src/SpeedTreeLib/CSpeedTreeDirectX.cpp` | `d3d9.h`, `d3dx9.h`, `SetVertexShaderConstant`, `SetFVF`, texture-stage states | Legacy SpeedTree rendering path needs its own DX11 adapter/shaders |
| `Src/UserInterface/UserInterface.vcxproj` | VS v143 with Win32 and x64 project configurations | Keep x64 migration baseline; do not remove existing Win32 or x64 configurations |

## Reference assessment

- Nidavelir2 `ClientSRC/Client/EterLib/GrpDevice.cpp` uses a DX9Ex-oriented device member, **not a DX11 renderer**.
- Nidavelir2 `ClientSRC/Client/GameShaders/ShaderReader.h` loads precompiled `.psc` / `.vsc` assets via a DX9 device; the logic cannot simply be reused as DX11 shader bytecode.
- Nidavelir2 `ClientSRC/Client/GameLib/MapOutdoorCharacterShadow.cpp` uses DX9 shader and render-target APIs. Treat lighting/shadow equations as algorithmic references, not drop-in code.
- JTX uses `2/FULL_106/Before`, `2/FULL_106/After` and `1/jettyx_106.diff`; the patch includes DX8/DX9 compatibility conditionals. Treat it as a **difference/reference source**, not a DX11 implementation.
- JTX `2/FULL_106/After/GameLib/terrainShadowsPS.h` contains HLSL-style lighting/terrain data that can inform a *new* SM5 port. Validate coordinate systems, matrices, register bindings and texture slots during translation.

## Chosen architecture

1. **Preserve DX9 renderer** as a runnable fallback and baseline for screenshot/FPS comparisons.
2. First implement a **separate DX11 smoke-test target**, not wired into the game's renderer.
3. Define a small renderer interface at the device/command/resource boundary after validating actual callers (do not blindly mirror each D3D9 function).
4. Introduce `D3D11Renderer` backed by `ID3D11Device`, `ID3D11DeviceContext`, `IDXGISwapChain` and COM RAII; target feature level 11_0.
5. Add VS/PS shader compilation pipeline and a minimal textured primitive. Use SM5 and explicit input layouts, constant buffers, samplers, blend/depth/rasterizer states.
6. Port scene systems incrementally: terrain -> Granny models -> effects/2D UI -> trees/SpeedTree -> water/shadows/post-processing. Sequence can be adjusted after full dependency inventory.
7. Switch backend only behind an opt-in config/feature flag and only when test criteria pass.

## High-risk areas and mitigations

- **Granny mesh buffers:** Separate animation/deformation data from DX9 buffer creation; compare skinned vs rigid render in DX9 and DX11.
- **SpeedTree runtime:** Verify supplied SDK/license and the actual API version before attempting GPU modernization. Retain existing tree data compatibility.
- **Fixed-function pipeline:** Replace DX9 lighting, FVF, texture combiners, alpha test and fog with explicit shader logic; do not implement an arbitrary 1:1 state emulation without measuring scope.
- **Transforms and conventions:** Confirm handedness, row/column major convention, winding and depth range against known captured scenes.
- **Window/CEF/UI:** Keep window ownership, input, resize/minimize paths and browser surfaces working; avoid two APIs presenting to one window at the same time.
- **Shader assets:** Do not assume Nidavelir2 `.psc`/`.vsc` can run in DX11. Source HLSL must be assessed/ported/recompiled.
- **x64 protocol:** No changes to network packet layouts, source encoding/locale, server/client ABI or game logic as part of graphics work.
- **Performance:** DX11 and shader effects do *not* guarantee FPS increase; measure CPU/GPU frame time, VRAM, crashes and visual differences.

## Phase gate checklist

### Gate 0 - baseline and dependency inventory (ongoing)
- [x] Separate `modernization/dx11-renderer` branch created.
- [x] Identify representative DX9 choke points and reference source folders.
- [ ] Complete dependency/callsite inventory over all render modules.
- [ ] Record reproducible baseline x64 build log and ingame frame/capture samples.
- [ ] Confirm external Granny/SpeedTree binaries and redistributable compatibility.
- [ ] Confirm current game starts, character login and map rendering on baseline.

### Gate 1 - standalone DX11 smoke
- [x] Isolated x64 VS v143 smoke-test project and source committed.
- [x] User-reported Release x64 smoke build success (0 errors / 0 warnings); DX9 classes not replaced.
- [x] User-reported checker-textured triangle displayed.
- [x] User-provided x64 BridgeProbe build: 0 errors / 0 warnings; screenshot and exit status confirm clear/present path.
- [x] User-provided x64 EterLib build: 0 errors / 1 existing warning C4267 in `GrpSubImage.cpp:104`; new DX11 object included.
- [ ] Confirm legacy DX9 game build and gameplay regressions are absent (not tested in this checkpoint).
- [ ] Triage old C4267 warning separately; avoid risk to image loader in renderer migration.
- [ ] Clear screen + textured triangle + resize/minimize + device errors handled.
- [ ] Compile shaders and report errors in logs.
- [ ] Builds with zero errors; outputs a test screenshot.
- [ ] DX9 client remains unchanged and buildable.

### Gate 1b - reusable DX11 GPU resource module
- [x] Add standalone shared immutable mesh, RGBA8 texture, shader and sampler resource creation.
- [x] Register new resources only on x64 EterLib; preserve Win32 and DX9.
- [x] Update BridgeProbe to draw indexed checkerboard quad using shared resource module.
- [ ] User Windows Release x64 DX11BridgeProbe build and visual test (pending).
- [ ] User Windows Release x64 EterLib rebuild (pending).
- [ ] Characterize resize, fullscreen/minimize and device removal after new resource layer.
- [ ] Confirm DX9 production client build and runtime unaffected.

### Gate 1c - standalone 3D heightfield preview
- [x] Inspect source terrain vertex format and 16x16 legacy patch size.
- [x] Write validated CPU heightfield-to-16-bit-indexed mesh builder.
- [x] Add isolated DX11TerrainProbe build target, keeping BridgeProbe unchanged.
- [x] Implement depth testing, camera matrix and texture-lit 3D preview.
- [ ] Windows x64 Release build and 3D/wireframe runtime visual test.
- [ ] EterLib x64 build and legacy DX9 game regression test.
- [ ] Validate resize/minimize and hardware/device loss handling.
- [ ] After tests: design real CTerrainPatch adapter and texture splat pipeline.

### Gate 2 - integration boundaries
- [ ] Renderer abstraction agreed using actual call-site inventory.
- [ ] GPU resource ownership and lifetimes defined.
- [ ] Terrain + one rigid mesh rendering verified before more features are enabled.
- [ ] Backend selection supports rollback to DX9.

### Gate 3 - visual features
- [ ] Character rendering, animation and transparency correctness.
- [ ] UI text, images, render targets and CEF verified.
- [ ] Trees/wind, water, shadow passes and post-processing added one at a time.
- [ ] Side-by-side rendering, error logs and performance comparisons completed.

## Reviewed source links

- [Client DX9 device](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/EterLib/GrpDevice.cpp)
- [Client DX9 state manager](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/EterLib/StateManager.cpp)
- [Client graphics base](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/EterLib/GrpBase.cpp)
- [Client shadows](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/GameLib/MapOutdoorCharacterShadow.cpp)
- [Client Granny render](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/EterGrnLib/ModelInstanceRender.cpp)
- [Client SpeedTree DX](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/SpeedTreeLib/CSpeedTreeDirectX.cpp)
- [Client UserInterface project](https://github.com/sbraslan/Project_ClientSrc/blob/migration/client-x64/Src/UserInterface/UserInterface.vcxproj)
- [Nidavelir2 ShaderReader](https://github.com/sbraslan/Project_Nidavelir2/blob/import/nidavelir2-reference/ClientSRC/Client/GameShaders/ShaderReader.h)
- [Nidavelir2 shadow shaders](https://github.com/sbraslan/Project_Nidavelir2/blob/import/nidavelir2-reference/ClientSRC/Client/GameLib/MapOutdoorCharacterShadow.cpp)
- [JTX patch diff](https://github.com/sbraslan/Project_Grafik/blob/import/jtx-v1.6-original/1/jettyx_106.diff)
- [JTX updated terrain shader](https://github.com/sbraslan/Project_Grafik/blob/import/jtx-v1.6-original/2/FULL_106/After/GameLib/terrainShadowsPS.h)

**No compile, runtime test, branch merge, game-code edit or DX11 renderer is claimed at this point.**
