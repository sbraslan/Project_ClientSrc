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

## Terrain probe Windows evidence (user-reported, 2026-11-04)

- The user supplied a Windows PowerShell log at `c4aa81e0`: `DX11TerrainProbe.vcxproj` Release x64 built successfully, and the executable returned normally.
- The screenshot shows an angled **green synthetic 3D heightfield** with lighting/texture repetitions and a blue background, as designed.
- The user's log confirms `eterlib.vcxproj` Release x64 build succeeded at the same commit.
- These results validate the separate 3D probe, **not** Metin2's real `CTerrainPatch`, texture splatting, quadtree, character or game renderer. Wireframe toggle and resize/minimize weren't separately evidenced yet.
- `/v:minimal` output displayed no warnings; this is not equivalent to a clean-rebuild warning audit.

## Terrain proof checkpoint (source implementation recorded; standalone probe verified)

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
- **Confirmed by user's PowerShell log and screenshot:** Windows Release x64 3D terrain probe built and displayed expected synthetic terrain; the EterLib x64 build passed.
- Wireframe and resize/minimize remain unverified; no in-game terrain conversion is claimed.
- No changes to `GrpDevice.cpp`, `StateManager.cpp`, `GameLib`,
  DX9 path, server ABI, Granny, SpeedTree or locale.

## Legacy 17x17 patch-layout adapter (x64 build passed; P/W scene pending)

- `Src/GameLib/TerrainPatch.h` confirms `HardwareTransformPatch_SSourceVertex`
  consists of a D3DXVECTOR3 position plus D3DXVECTOR3 normal, with 17x17
  vertices for a 16x16 patch (`Src/PRTerrainLib/TerrainType.h`).
- Added `Src/EterLib/Renderer/DX11LegacyTerrainPatch.h/.cpp`: no D3D9
  dependency, packed 24-byte position/normal POD mirror, validation, provisional
  level-0 triangles/UVs, and preserving original positions/normals.
- Updated `Tools/DX11Smoke/TerrainProbe.cpp` to upload a second *synthetic*
  17x17 patch-style mesh and switch to it with **P**; **W** still toggles
  wireframe. Includes CPU tests for validity, counts, vertex preservation
  and rejection of degenerate normals without output mutation.
- `DX11TerrainProbe.vcxproj` and x64 `eterlib.vcxproj` now compile adapter.
- **User evidence at a9df3ba6:** Release x64 `DX11TerrainProbe.vcxproj`
  and `eterlib.vcxproj` both built successfully. Runtime exited without
  error, and the screenshot shows the original 49x49 terrain, not a
  clearly verified P-toggled 17x17 patch. The P/W branches and resize
  remain unverified visually.
- **New verification aid:** `TerrainProbe.cpp` displays the active
  "Full Synthetic Terrain" or "17x17 Patch Preview", and "Solid" or
  "Wireframe", in the window caption. Key-repeat is ignored.
- Real `GameLib` render path, coordinate conversion, LOD stitching,
  quadtree and splat textures are **not** integrated. Do not activate
  DX11 for real gameplay.

## 17x17 P/W mode confirmation (user screenshots)

- User supplied separate screenshot captions `17x17 Patch Preview | Solid`
  and `17x17 Patch Preview | Wireframe`. Both show the expected synthetic
  17x17 heightfield; the wireframe visibly contains triangle edges.
- This verifies the P/W **visual modes** and 289-vertex candidate patch
  preview. It does not establish that live map vertex sources or texture
  splatting are integrated. Resize/minimize and DX9 runtime parity are
  still awaiting dedicated tests.

## Real terrain index topology audit and prototype (new code, NOT yet built)

- `Src/GameLib/MapOutdoorRender.cpp::SelectIndexBuffer`: LOD 0 uses
  `D3DPT_TRIANGLESTRIP`; LOD 1/2 use `D3DPT_TRIANGLELIST`.
- `Src/GameLib/MapOutdoorIndexBuffer.cpp::SetIndexBuffer`: creates three
  16-bit GPU index buffers. LOD 0 zig-zags across rows with repeated
  degenerate connector indices. LOD 1/2 are assembled by special boundary
  helpers (`ADDLvl1*`, `ADDLvl2*`) rather than simple downsampling.
  Do not replace original index streams with our provisional regular
  512-triangle grid for game rendering.
- `Src/GameLib/MapOutdoorRenderHTP.cpp`: hardware terrain rendering uses
  distance-based LOD selection and a multipass texture/splat-alpha setup.
  A single grass texture is insufficient for game-compatible rendering.
- Added CPU-only `Src/EterLib/Renderer/DX11LegacyTerrainIndices.h/.cpp`
  to validate and convert original DX9 strip/list index streams to
  16-bit triangle lists, preserving strip winding parity even over
  degenerate row connectors, and leaving outputs intact on invalid input.
- The *existing* `DX11TerrainProbe` runs isolated CPU self-tests for
  strip parity, stitched rows, triangle-list conversion, invalid indices
  and output immutability before creating the test window.
- `DX11TerrainProbe.vcxproj` and x64-only `eterlib.vcxproj` compile this
  module. **No GameLib hook and no game renderer switch exists yet.**
- **Windows x64 build/runtime for this new module: PENDING.** The visual
  screenshots only verified the previous `a73da1fe` P/W preview.

## Gate 1e Windows evidence and Gate 1f snapshot codec (2026-11-05)

- The user supplied a successful Release x64 build log at `c9db5b2c`
  for `DX11TerrainProbe.vcxproj` and `eterlib.vcxproj`; the executable
  closed with exit code 0 and did not report CPU LOD self-test failure.
  This verifies the small strip/list validation fixtures, **not** a
  captured complete GameLib index stream.
- Source audit confirms `GameLib/MapOutdoorIndexBuffer.cpp::SetIndexBuffer`
  temporarily owns original CPU `m_pwaIndices[0..2]` and `count[0..2]`
  before uploading to DX9 and freeing them. `EterLib/GrpIndexBuffer.cpp`
  creates the GPU index buffer with `D3DUSAGE_WRITEONLY`: do **not**
  attempt late GPU readback as a general capture strategy.
- Added isolated, versioned, little-endian `DX11LegacyIndexSnapshot`
  codec in `EterLib/Renderer` with strict 289-vertex/3-LOD bounds,
  index topology validation, FNV-1a accidental corruption checksum,
  fail-without-output-mutation decode, and explicit opt-in file I/O.
- Added `Tools/DX11Smoke/DX11LODSnapshotProbe.vcxproj` and
  `LODSnapshotProbe.cpp`: a CPU-only console target that checks
  in-memory roundtrip, output equality, checksum corruption rejection,
  invalid source rejection and optionally verifies a supplied capture.
- No GameLib hook is installed and no snapshot is automatically written.
  Real map capture (at the source CPU buffers before upload) will require
  a separate **explicitly disabled-by-default** diagnostic hook **after**
  these standalone Windows build/tests pass.
- This snapshot codec and new console target are **NOT YET WINDOWS BUILT**.
  Nothing here proves actual game terrain or DX11 in-game rendering.
- No DX9 drawing files, server ABI, locales, Granny or SpeedTree changed.

## Gate 1g — real GameLib LOD capture hook staged, **default-off** (2026-11-05)

- Source hook: `Src/GameLib/MapOutdoorIndexBuffer.cpp`, after LOD 0/1/2
  CPU index arrays/counts have been built, before the existing DX9 index
  buffer lock/memcpy/unlock/delete loop.
- Only `Release|x64` `gamelib.vcxproj` with
  `/p:EnableDX11LODCapture=true` defines the per-file macro
  `M2_DX11_LOD_CAPTURE`. The opt-in source compiles without the shared
  PCH to avoid a per-file /D inconsistency. Default builds compile no
  capture code; Win32 and WORLD_EDITOR paths remain unchanged.
- Even in an opt-in build, an exact process environment value
  `METIN2_DX11_LOD_CAPTURE_ONCE=1` is necessary. An atomic guard limits
  recording to one attempt per process; no DX9 indices or render state are
  rewritten.
- The validated three-LOD snapshot is created as
  `dx11_terrain_lod_capture.bin` in the game's **working directory**
  via Win32 `CREATE_NEW`, so an existing file cannot be overwritten.
  Failure is logged and does not intentionally abort legacy rendering.
- `.gitignore` excludes this diagnostic file. Detailed gated test
  procedure: `docs/dx11/REAL_LOD_CAPTURE_GUIDE.md`.
- **NOT YET Windows built or runtime tested.** Real game capture is
  explicitly deferred until default and opt-in GameLib build checks,
  SnapshotProbe verification and baseline DX9 client regression.
- The user's last visible PowerShell snippet at `5c23e053` confirms
  `DX11LegacyIndexSnapshot.cpp` built into EterLib and a final success
  marker, but does not show the independent console test's own output.
  Re-run and collect its actual output before treating its codec tests
  as directly evidenced.

## Gate 1g / DX9 full-client build verification (user PowerShell evidence, 2026-11-05)

- At `5277089e`, user supplied complete `DX11LODSnapshotProbe.exe` output:
  Encode, Decode, preserve original LOD bytes, LOD 0/1/2 conversion,
  checksum corruption rejection and invalid-source preservation all passed.
- User supplied successful `EterLib` Release x64 build; `GameLib`
  Release x64 default-off build; opt-in
  `/p:EnableDX11LODCapture=true` GameLib build; and full
  `/t:Rebuild /p:EnableDX11LODCapture=false` restoration.
  This proves compile/link of static library objects, **not** an opt-in
  `metin2release.exe` end-to-end link or game capture.
- A subsequent user PowerShell log shows a clean `git status --short`
  and successful **full UserInterface Release x64** build with
  `/p:EnableDX11LODCapture=false`. All dependent projects compiled and
  the output `Src/bin/Release/x64/metin2release.exe` was produced.
- **Runtime DX9 login, character/world loading, map rendering, exit and
  syserr checks are still pending.** The client executable was built but
  user has not yet supplied evidence of running it in its game runtime
  directory with pack/locale assets.
- The real LOD capture file has **not** been generated or validated yet.
  The diagnostic remains default-off. Do not run capture before the DX9
  runtime baseline and a successfully linked opt-in test EXE are verified.

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
- [x] User-reported Windows x64 Release terrain build and 3D visual output.
- [ ] Wireframe toggle, resize/minimize behavior and clean shutdown regressions.
- [x] User-reported Release x64 EterLib build succeeded at c4aa81e0.
- [ ] Legacy DX9 game build and runtime regression test.
- [ ] Validate resize/minimize and hardware/device loss handling.
- [ ] After tests: design real CTerrainPatch adapter and texture splat pipeline.

### Gate 1d - legacy patch-format CPU adapter and preview
- [x] Establish 16-cell patch / 17x17 vertices, source position+normal layout.
- [x] Add D3D9-independent CPU adapter with fail-without-mutation validation.
- [x] Add synthetic 17x17 patch preview to existing terrain probe (P key).
- [x] Register adapter only on x64 EterLib and standalone terrain probe.
- [x] User Windows Release x64 build passed at a9df3ba6; original terrain screenshot received.
- [x] User screenshots confirm 17x17 Solid and 17x17 Wireframe P/W views.
- [ ] Resize/minimize and recovery behavior verified after P/W changes.
- [ ] Verify actual patch source layout, axis/handedness, normal orientation and LOD index topology in GameLib.
- [ ] Prepare real CTerrainPatch ingestion and splat layering **after** verification; no activation before DX9 regressions.

### Gate 1e - actual topology preservation (CPU-only)
- [x] Audit all 3 real GameLib LOD index formats and special boundary helpers.
- [x] Add legacy strip/list to triangle-list conversion with validation.
- [x] Add strip parity/connector and invalid-data regression tests.
- [x] User Windows Release x64 terrain/LOD CPU probe and EterLib build passed at c9db5b2c.
- [ ] Verify actual map vertex capture and texture splat coordinates without mutating DX9 rendering.
- [ ] Cross-check LOD edge stitching/orientation against captured legacy streams.

### Gate 1f - verified index snapshot transport (no GameLib hook)
- [x] Locate actual CPU LOD index streams before WRITEONLY DX9 upload.
- [x] Define bounded versioned 3-LOD snapshot codec with checksum.
- [x] Add isolated x64 console in-memory roundtrip and malformed-data tests.
- [x] Compile codec into x64 EterLib with no use from game renderer.
- [ ] User Windows Release x64 DX11LODSnapshotProbe build and self-test.
- [ ] User Windows Release x64 EterLib rebuild with codec.
- [ ] After verification: plan opt-in GameLib pre-upload capture (OFF by default).
- [ ] Validate captured actual LOD0/1/2 streams and edge stitching.

### Gate 1g - GameLib real LOD diagnostic capture (no gameplay activation)
- [x] Hook pre-upload CPU index arrays without changing DX9 upload logic.
- [x] Require opt-in Release x64 compile property and exact runtime environment flag.
- [x] One-shot non-overwriting binary snapshot + Git ignore rule.
- [x] Isolate opt-in macro from PCH, document full compile/test workflow.
- [x] User Windows x64 SnapshotProbe console passed (all encode/decode/corruption checks).
- [x] User default-off GameLib Release x64 build passed.
- [x] User explicitly opt-in GameLib Release x64 build passed.
- [x] User rebuilt default-off GameLib after opt-in compilation.
- [x] Full UserInterface Release x64 with Capture OFF builds and links metin2release.exe.
- [ ] Run DX9 baseline: login, enter world/map, exit and inspect syserr.
- [ ] Build/link separate opt-in test client before runtime capture.
- [ ] Actual 3-LOD capture generated and validated, only after above gates.

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

**Checkpoint:** Synthetic DX11 probes and their x64 builds are confirmed by user logs/screenshots. The real Metin2 DX9 renderer remains active; actual in-game DX11 rendering and the new snapshot codec's Windows build are not yet verified.
