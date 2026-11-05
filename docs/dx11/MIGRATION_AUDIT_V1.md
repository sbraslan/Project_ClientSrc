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

## DX9 in-game baseline screenshots (2026-11-05, user evidence)

- The user provided **seven** gameplay screenshots with an OSD explicitly
  showing **D3D9**, not D3D11. Rendered scenes include: an extremely
  crowded mob/item-label cluster, open terrain, character/world UI,
  enemy models and labels, minimap, and combat particle/skill effects.
- Approximate *instantaneous* observed OSD values in screenshot order:
  3 FPS / 461.8 ms, 24 FPS / 37.5 ms, 61 FPS / 16.2 ms,
  61 FPS / 17.1 ms, 60 FPS / 17.2 ms, 57 FPS / 14.5 ms,
  59 FPS / 19.1 ms. These are screenshot readings, not a timed
  benchmark or statistically valid comparison. OSD FPS/frametime
  snapshots can use different averaging periods.
- **Observed working:** character/world scene, map terrain and textures,
  multiple enemy models/labels, UI and visible combat effects.
  **Not verified:** whether the screenshots were captured using the exact
  just-built `metin2release.exe` file, prolonged stability, normal exit,
  current `syserr`, FPS cap or controlled performance methodology.
- The first crowded screenshots show very low FPS. GPU OSD utilization
  appears relatively modest, but no CPU-core-level or draw-call data was
  provided; do not diagnose a CPU bottleneck as fact.
- **Next gate:** capture a safe baseline report and build/link a
  `Release|x64` opt-in diagnostic client with
  `EnableDX11LODCapture=true` but environment flag **unset**. Do not
  run real LOD capture before this opt-in full-link check.
- The normal DX9 path remains the reference; no in-game DX11 rendering
  is claimed.

## Opt-in x64 link and central syserr diagnostics (user evidence, 2026-11-05)

- The user-provided PowerShell transcript at `64a438ac` shows successful
  `GameLib` Release|x64 **Rebuild** with
  `EnableDX11LODCapture=true`, followed by successful
  `UserInterface` Release|x64 **link**, producing
  `Src/bin/Release/x64/metin2release.exe`.
- **Backup gate failed before build:** that output EXE did not exist at
  the backup check; therefore
  `metin2release_dx9_baseline_backup.exe` was **not** created.
  A later `Get-Item` reported the missing backup. The successful
  diagnostic build must NOT be presented as a backed-up baseline.
  PowerShell was run interactively line-by-line so it continued despite
  a preceding thrown error.
- Central `client_syserr.txt` confirms an x64 Python 3.15 client
  launched from the **game runtime folder** (`Project_Client`), which
  differs from the MSBuild output folder (`Project_ClientSrc/Src/bin/Release/x64`).
  Thus the log is NOT evidence that the newly linked opt-in EXE ran.
- The syserr has **nine** `UnicodeDecodeError` tracebacks from
  `game.py:1613` `SaveScreen`: byte `0xFC` decoded as UTF-8
  (likely Windows narrow-path handling of `ü`). Inspect the exact
  Python call and C++ extension before editing; preserve Unicode paths.
- Also contains **146** `Damage is equal or below 0.` messages and
  about **2,376** each of receive/decrypt DEBUG messages. Diagnostic
  events are logged with `[ERROR]` level and should not all be
  counted as crashes. The log ends with
  `PYTHON STARTUP: RunMainScript returned true`; no native
  crash stack is present. It does not independently prove normal exit.
- Before real capture, back up the **actually running** game client EXE,
  separately preserve the opt-in link artifact, verify build/runtime
  hashes, and fix the screenshot path/encoding mismatch in a separate
  locale-safe patch. Capture is **not yet evidenced**.
- No claims of in-game DX11 terrain rendering or sustained performance
  benchmarks follow from these logs.

## Gate B readiness: alternate-name one-shot live LOD capture

- Previous Windows log at `64a438ac` confirmed the opt-in
  `GameLib` x64 rebuild and full `UserInterface` x64 link.
  It did NOT produce a verified preserved DX9 baseline EXE:
  the originally attempted build-folder backup failed.
- Runtime `client_syserr.txt` identifies the actual launched client
  in `Project_Client/metin2release.exe`; the compiled opt-in
  executable is in `Project_ClientSrc/Src/bin/Release/x64/`.
- Safe next test: copy/hash-verify the **runtime** DX9 EXE first;
  install the opt-in diagnostic EXE **under a different name** in the
  same runtime folder; run it with the runtime folder as CWD and a
  single-process-only environment flag; never overwrite any EXE.
- Since alternate names may be rejected by launcher/pack assumptions,
  do not claim a successful in-game run based only on link success.
- **Not yet verified:** capture file generation, three real LOD
  counts, `DX11LODSnapshotProbe.exe --verify`, live-client clean
  exit, and no regression. All remain open gates.
- A separate `game.py:1613 SaveScreen` narrow-path/UTF-8 traceback
  was detected; handle it independently from terrain capture.

## Gate 1h — missing real capture root-cause probe (2026-11-05)

- User-provided read-only SHA256 comparison: active normal runtime EXE
  (`Project_Client/metin2release.exe`) contains **no** capture marker,
  while `metin2release_dx11_lod_probe.exe` in the game runtime
  directory and the opt-in build output have **identical SHA256** and
  both contain the literal capture environment/log marker strings.
  Original runtime DX9 EXE remains a distinct protected binary.
- No real `dx11_terrain_lod_capture.bin` exists; the most recent
  checked syserr had no `[DX11 LOD CAPTURE]` entries. Opt-in link
  success alone cannot establish that the map initialization path
  ran or that the environment flag reached the process.
- Source call chain checked:
  `CMapOutdoor::CreateTerrainPatchProxyList()` calls
  `SetIndexBuffer()`, which generates all LOD streams then calls
  `TryCaptureOriginalTerrainLOD()` before DX9 upload.
- Added **opt-in x64 only**, once-per-process
  `[DX11 LOD DIAG] hook reached | envLength=... | enabled=...`
  trace at the hook, even if runtime flag is absent. This separates
  no map initialization from missing environment flag and reports
  LOD 0/1/2 source counts. No diagnostic log in normal DX9 builds.
- **User Windows rebuild/link PASSED** for Release x64 opt-in,
  confirmed the new diagnostic marker inside the linked EXE, SHA256
  `78B024AE1483228B...`.
- **First instrumented in-game probe** (2026-11-05 01:11:57,
  PID 9944) reached the live `SetIndexBuffer` capture hook:
  `envLength=0 | enabled=0 | LOD0=574 LOD1=648 LOD2=552`.
  These are genuine runtime terrain index **counts**,
  not captured data or the probe's synthetic fixture values.
  `BOOTTRACE: AFTER SendEnterGamePacket` followed at 01:12:04;
  client exit code was 0, original DX9 EXE hash unchanged.
- No snapshot file: the opt-in runtime environment variable was missing
  or empty **when the hook ran**, causing an intentional early return.
  The underlying reason it was absent is not yet proven. Next test
  directly injects process environment via
  `.NET ProcessStartInfo(UseShellExecute=false).EnvironmentVariables`,
  without modifying the original game EXE or C++ source.
- Do not mark real LOD capture validated until a real file passes
  `DX11LODSnapshotProbe.exe --verify`.

## Gate 1i — real in-game 3-LOD snapshot verified (2026-11-05)

- User's new x64 opt-in EXE reached the original CPU index hook in game
  at **2026-11-05 18:32:51**, PID 17708:
  `envLength=1 | enabled=1 | LOD0=574 LOD1=648 LOD2=552`.
- Log confirmed `[DX11 LOD CAPTURE] created dx11_terrain_lod_capture.bin
  (3576 bytes, LOD0=574 LOD1=648 LOD2=552)`.
- First attempt to run verifier returned the generic invalid/inaccessible
  error. The user confirmed the file exists at
  `Project_Client/dx11_terrain_lod_capture.bin`, length **3576 bytes**.
  Re-running the verifier from `Project_Client` with an explicit
  working directory **PASSED**:
  `LOD0: 574 source indices, 512 converted triangles (STRIP)`;
  `LOD1: 648 source indices, 216 converted triangles (LIST)`;
  `LOD2: 552 source indices, 184 converted triangles (LIST)`.
  The verifier printed `[OK] Capture file validated` and
  `VERIFIER EXIT CODE: 0`.
- Combined converted triangle count: **912**. This is an index/topology
  validation only; no live DX11 GameLib renderer exists yet. The
  isolated DX11 terrain probe continues to use its own preview data;
  it does not yet load this snapshot into a GPU mesh.
- Production runtime `Project_Client/metin2release.exe` was not
  overwritten; previous hash checks confirmed it was unchanged. Preserve
  the capture file and avoid sharing/committing it without inspection.
- **Next**: optional standalone DX11 TerrainProbe path loading the real
  snapshot, preserving its current synthetic/default mode, verifying
  matching 17x17 vertex grid, strip parity, winding, LOD edge seams and
  wireframe before in-game renderer integration. No JTX patches yet.

## Gate 1k — visual real-index topology passed, real patch vertex capture staged (2026-11-05)

User supplied six screenshots after the Gate 1j Windows build/test:
- Standalone Direct3D11 TerrainProbe title shows **CAPTURED INDEX LOD0**,
  **512 triangles**, wireframe, over **SYNTHETIC HEIGHTS**.
- LOD1 title shows **216 triangles**, wireframe; LOD2 shows
  **184 triangles**, wireframe. Each level visibly changes triangular
  density/topology, with no obvious missing large mesh regions.
- Original full-synthetic wireframe mode and 17x17 synthetic patch
  wireframe/solid modes still render. Screenshots show a
  ~143-144 FPS D3D11 overlay in this standalone test, **not a
  benchmark of actual Metin2 DX9/DX11 game frame rates**.
- **Not yet verified**: exact patch-border seam correctness between
  neighbors, actual in-game normals/textures/material splats, real
  vertex position data, or an ordinary DX9 regression run after
  this screenshot set. No user-supplied exit code in this turn.

**Verified source location for the next gate:**
- `Src/GameLib/AreaTerrain.cpp::CTerrain::_CalculateTerrainPatch`
  generates original 17x17 `HardwareTransformPatch_SSourceVertex`
  values from the real height/normal maps; positions are
  `(fX, -fY, hgt * heightScale)` (**Z-up**, not the probe's Y-up)
  with normals read from source map.
- `Src/GameLib/TerrainPatch.cpp::CTerrainPatch::BuildTerrainVertexBuffer`
  receives that exact CPU array just before the existing DX9
  software/hardware vertex buffer path, and its source file contains
  no known garbled/unrepresentable legacy comment bytes.
- Stage only an **independent, opt-in patch vertex capture** there:
  `EnableDX11PatchCapture=true` for Release x64 only, plus
  `METIN2_DX11_PATCH_CAPTURE_ONCE=1`, one-shot non-overwriting
  `dx11_terrain_patch_capture.bin`. Snapshot format `M2PVERT1`,
  version 1, 289 vertices x 6 IEEE754 float32, 6956-byte fixed
  record with FNV-1a corruption checksum.
- New codec `DX11LegacyPatchSnapshot.h/.cpp` is platform-neutral,
  validates finite coordinates/nonzero normals, and preserves outputs
  on rejected data; new `--verify-patch` mode and in-memory corruption
  tests were added to the existing standalone
  `DX11LODSnapshotProbe`.
- **Status: code committed but NOT Windows compiled or runtime
  validated.** Treat it as a test-only gate. Normal DX9 renderer
  stays unchanged; the original verified `dx11_terrain_lod_capture.bin`
  remains read-only and must not be deleted or overwritten.
- After Windows builds/tests pass, use both verified vertex and LOD
  index snapshots in a separate standalone DX11 viewer. Apply
  explicit original DX9 Z-up to preview Y-up axis conversion and
  camera recentering, rather than swapping raw axes silently.

## Gate 1k real patch live run — 2026-11-05 22:26

- User reported successful Release x64 opt-in GameLib rebuild + full UserInterface link, separate hash-checked diagnostic EXE. The normal DX9 runtime EXE was not replaced.
- An elevated PowerShell process created the client with `ProcessStartInfo.UseShellExecute=false`, `WorkingDirectory=Project_Client`, and `EnvironmentVariables['METIN2_DX11_PATCH_CAPTURE_ONCE']='1'`.
- Syserr `PID=11484` at `22:26:15`: `[DX11 PATCH DIAG] hook reached | envLength=1 | enabled=1`; next line `[DX11 PATCH CAPTURE] created dx11_terrain_patch_capture.bin (6956 bytes, vertices=289)`. Process exit code 0.
- **Next gate pending:** standalone `DX11LODSnapshotProbe.exe --verify-patch dx11_terrain_patch_capture.bin` must return exit code 0. File exists according to game log, but actual snapshot decode/checksum/finite normals and min/max height have NOT yet been tested.
- Caution: the captured patch vertices come from the first `CTerrainPatch::BuildTerrainVertexBuffer` call, while the existing real LOD index snapshot is from `CMapOutdoor::SetIndexBuffer`. They share 17x17 indexing, but **this does not establish they were captured at the same spatial map patch**. The GPU preview must be labeled index topology + one sampled real patch; do not call it a full faithful game terrain render without provenance.

## Gate 1L — verified original positions + converted captured index GPU viewer (2026-11-05)

- **User-verified real vertex snapshot:** `dx11_terrain_patch_capture.bin`
  exists and is **6956 bytes**. Standalone
  `DX11LODSnapshotProbe.exe --verify-patch` reported `vertices=289`,
  `minZ=24249`, `maxZ=28585`, `height range=4336`,
  `[OK] Real terrain patch file validated`, and exit code **0**.
  The user also ran SHA256 for both captures. The hashes were
  visibly truncated in the provided console paste; never record
  unsupported full hash digests.
- Standalone `Tools/DX11Smoke/TerrainProbe.cpp` now accepts
  `--lod-snapshot <file> --patch-snapshot <file>` (either option order).
  Both paths use **Unicode read-only** CreateFileW. Files are
  individually validated by their existing magic, fixed/versioned
  snapshot decoders and checksums BEFORE DX11 resources are created.
  A patch without LOD is rejected; malformed input fails closed.
- The viewer retains the previous no-argument synthetic mode and
  LOD-only synthetic heights mode, and creates **three NEW immutable
  D3D11 meshes** for actual 289 real vertex positions/normals combined
  with previously captured/converted LOD0/1/2 index sequences.
- The original DX9 axis basis is **Z-up** (source `AreaTerrain.cpp`),
  unlike the test preview's **Y-up**. Preview maps **(X,Y,Z) ->
  (X,Z,-Y)** for positions/normals, centers the patch and applies
  a **uniform scale** on all axes (no exaggerated height).
  A camera adapted to the actual height extent prevents clipping.
  A CPU fixture checks equal XY spacing, vertical normal rotation
  and fail-without-mutation behavior.
- `V` toggles actual/synthetic vertices with the *same* captured
  index topology. `1/2/3` choose actual captured LOD, `W`
  solid/wireframe, `P` previous synthetic 17x17, `R` previous
  real-index mode. Window title explicitly distinguishes
  `REAL PATCH VERTICES` from `SYNTHETIC HEIGHTS`.
- The captured vertex file is from one sample patch, but
  the LOD snapshot is emitted during *global* index buffer creation.
  Combining these is a **structural 17x17 LOD preview**, NOT yet
  proof of patch adjacency, true texture coordinates or faithful
  in-game terrain rendering. No production DX9 renderer changes.
- **Windows build/visual test is pending** for this new viewer; do not
  mark integration verified until user reports Release x64 compile
  exit 0 and screenshots of actual vertex+LOD mode.
- Do NOT run GameLib or UserInterface builds for this step. Only
  rebuild the isolated `DX11TerrainProbe.vcxproj`. Existing LOD and
  patch capture files remain untouched.

## Gate 1m — real patch DX11 visuals and geometric QA (2026-11-05)

**User screenshot evidence (7 views):**
- Three screenshots show `REAL PATCH VERTICES | CAPTURED LOD0/1/2`
  and the exact expected triangle counts (512 / 216 / 184),
  alongside a live `D3D11` overlay in the independent TerrainProbe.
- Another screenshot shows `CAPTURED INDEX LOD2 | SYNTHETIC HEIGHTS`,
  verifying that the V comparison path remains selectable.
- The previous full synthetic terrain and 17x17 synthetic patch both
  remain visibly functional in wireframe. The synthetic 17x17 patch
  also renders in solid. Standalone overlay reports roughly 144 FPS;
  not comparable with actual full Metin2 gameplay.
- The **real patch has a very tall, narrow cliff-like silhouette**
  across all three LODs. This is not automatically an error: original
  captured Z ranges from 24249 to 28585 (range 4336), which may be
  comparable to or much greater than horizontal patch size.
  Screenshots alone cannot determine whether the slope and winding
  are authentic; full game DX11 terrain and adjacent seams are NOT
  confirmed. The preview's (X,Y,Z)->(X,Z,-Y) rotation is intentional.
- An opt-in **read-only CPU geometry audit** was added to
  `Tools/DX11Smoke/LODSnapshotProbe.cpp`:
  `--audit-geometry lod.bin patch.bin`. It validates both independently
  checksummed sources and reports X/Y/Z spans, height/horizontal ratio,
  mean Z-up normal, grid-neighbor maximum/steep-edge slopes, and
  projected positive/negative/degenerate winding counts per LOD.
  **Windows Release x64 build and the user audit both PASSED**
  on 2026-11-05: `AUDIT EXIT CODE: 0`.
- User should compile only the isolated
  `DX11LODSnapshotProbe.vcxproj`, preserve both snapshot files,
  and run the geometry audit from the game working directory.
  The game EXE and DX9 runtime must remain unchanged.

## Gate 1n — actual patch slope/winding QA PASSED (2026-11-05)

User ran the new `DX11LODSnapshotProbe.exe --audit-geometry` on
**both original captured snapshot files**, producing `AUDIT EXIT CODE: 0`.

Exact source-derived metrics:
- X = 3200, Y = 3200, Z/height range = 4336 original game units;
  ratio `Z/max(X,Y) = 1.355`. A tall cliff-like first sample patch
  is plausible; the viewer is using **uniform scale**, not deliberately
  exaggerating height.
- Mean normalized source `Z-up` normal component = **0.750077**.
- 544 sampled grid-neighbor edges, maximum slope **82.5454 degrees**;
  **30** edges >=60 degrees and **18** edges >=75 degrees.
- LOD0: **512** converted triangles, all **512** projected positive,
  **0** negative, **0** projected-degenerate.
- LOD1: **216** triangles, all **216** projected positive,
  **0** negative, **0** projected-degenerate.
- LOD2: **184** triangles, all **184** projected positive,
  **0** negative, **0** projected-degenerate.
- User log: `[OK] Read-only captured geometry audit complete.
  No runtime files were written.`

**Important limitations:**
- The positive winding sign describes **projected original DX9 XY
  coordinates**, not an already verified DX11 front-face policy.
- The standalone DX11 viewer's wireframe and solid rasterizers
  currently set `D3D11_CULL_NONE`; visibility there is NOT evidence
  that production backface culling / `FrontCounterClockwise`
  are correct. This must be verified before enabling culling.
- One original 17x17 patch is captured and the LOD index topology is
  reused for all patches. The audit does **not** verify seams between
  adjacent patches, shading, true terrain splatting, or production
  Metin2 rendering / performance.
- No snapshot files or normal DX9 game executable were modified by
  the CPU geometry audit.

**Next safe gate:** independently test actual captured patch in
`W`=solid mode and audit `CullMode` / triangle-facing configuration
against the game source. Then capture a coordinated pair of
adjacent real patches (stable IDs/provenance) for edge stitching
comparisons before integrating terrain materials or full DX11
rendering.

## Gate 1o — real patch solid preview PASSED; controlled backface test staged (2026-11-05)

- User supplied two more screenshots of the REAL captured-vertex
  LOD0 patch (512 tris): independent D3D11 viewer in **wireframe**
  and **solid** modes. Both show consistent cliff-shaped terrain,
  with expected texture on the solid surface, and the D3D11 overlay.
  The texture is the probe's procedural synthetic grass, **not**
  original Metin2 terrain splatting or textures.
- Static source trace: `EterLib/StateManager.cpp::CStateManager::SetDefaultState`
  sets DX9 `D3DRS_CULLMODE = D3DCULL_CW`. The inspected
  `MapOutdoorRenderHTP.cpp` and `MapOutdoorRenderSTP.cpp`
  do not contain a local explicit `D3DRS_CULLMODE` override.
  This is evidence of the global default, NOT yet proof of every
  possible runtime render-state modification.
- The standalone probe's default `D3D11_CULL_NONE` visual path
  remains unchanged. Its source now provides **optional key C**
  state cycling: `C:OFF`, `C:BACK/FRONT_CW` and
  `C:BACK/FRONT_CCW`, with distinct solid/wireframe D3D11
  rasterizer states. All modes display their name in window title;
  no mesh/index winding or original snapshot files are mutated.
- This lets the user visually test both actual DX11 front-facing
  rasterization conventions under the same camera rather than
  assume DX9's `D3DCULL_CW` maps mechanically after projection
  and Z-up to Y-up rotation.
- **Source committed; Windows Release x64 rebuild and actual C-key
  comparison screenshots PENDING.** Do not declare production
  backface policy verified until test results are available.
- No GameLib / UserInterface / DX9 runtime file was modified by
  this culling test.

## Gate 1p — LOD0 controlled CW/CCW culling screenshots (2026-11-05)

**Four user screenshots from the isolated D3D11 TerrainProbe:**
- `REAL PATCH VERTICES | CAPTURED LOD0: 512 tris | Wireframe | C:OFF`:
  all triangles visible, unchanged topology.
- `REAL PATCH VERTICES | CAPTURED LOD0: 512 tris | Solid | C:OFF`:
  synthetic grass material covers the full steep source patch.
- `REAL PATCH VERTICES | CAPTURED LOD0: 512 tris | Solid |
  C:BACK/FRONT_CW`: the intended cliff face remains visibly
  filled, close to culling-off appearance.
- `REAL PATCH VERTICES | CAPTURED LOD0: 512 tris | Solid |
  C:BACK/FRONT_CCW`: nearly the entire original face
  disappears, leaving only thin boundary fragments.

**Conclusion scoped to this camera/basis/LOD0 sample:**
`D3D11_CULL_BACK` with `FrontCounterClockwise = FALSE`
(front clockwise) matches the visible real patch orientation
in the current independent DX11 viewer. This was explicitly
tested, not inferred solely from the original game's
`D3DCULL_CW` (the latter is its *culled* winding, whereas
DX11's front-face flag determines *retained* facing).
Do NOT mechanically copy `D3DCULL_CW` into DX11,
or assume the result generalizes to reflected camera matrices,
other objects, neighboring patches, or production game raster
state. The current standalone probe still defaults to no culling
until further gates are satisfied.

**Remaining tests:** check `C:BACK/FRONT_CW` on REAL LOD1 and
REAL LOD2 solid modes and inspect patch seams across actual
adjacent source terrain pieces. No GameLib, UserInterface,
or original DX9 renderer changes were made here.

## Gate 1q — LOD0/1/2 CW culling passed; guarded adjacent-pair capture staged (2026-11-05)

**User screenshot evidence:** Four additional isolated D3D11 TerrainProbe
views show `REAL PATCH VERTICES`, captured LOD0/1/2, solid mode.
C:OFF on LOD0 leaves its 512 triangles fully visible. With
`C:BACK/FRONT_CW`, LOD0 (512 tris), LOD1 (216 tris), and LOD2
(184 tris) each retain their intended visible cliff face. For this
standalone viewer's current camera and original Z-up to viewer Y-up
rotation, `D3D11_CULL_BACK` with `FrontCounterClockwise=FALSE`
is now visually supported for all three LODs. This is NOT evidence
of a complete production DX11 game renderer or of adjacent patch
seam integrity. DX9 uses its separate `D3DCULL_CW` default state.

**Next stage implemented, Windows build/runtime PENDING:**
- New standalone read-only CLI `DX11LODSnapshotProbe.exe
  --audit-adjacent patchA.bin patchB.bin` loads two existing
  `M2PVERT1` snapshots, rejects non-neighboring origins, and
  compares all 17 boundary position samples (including height).
  Reports maximum border gap, mismatch count, and maximum normal
  deviation; warns on discontinuous normals. A mismatch returns a
  nonzero exit code. Synthetic fixtures cover right, left, down(-Y),
  up(+Y), bad seam height and non-neighboring origins.
- Separate opt-in diagnostic only:
  `GameLib/TerrainPatch.cpp` under
  `M2_DX11_ADJACENT_CAPTURE && _WIN64 && !WORLD_EDITOR`.
  The flag is enabled **only** via MSBuild
  `/p:EnableDX11AdjacentCapture=true` for
  `Release|x64` and must also receive exactly
  `METIN2_DX11_ADJACENT_CAPTURE_ONCE=1` in the elevated
  client's environment. Default DX9 builds do not include the hook.
- The adjacent sampler uses actual DX9 world-space original vertex
  origins (17x17 source, X increases, Y decreases). It maintains a
  bounded cache of at most 256 unique source patches, chooses the
  first pair separated by exactly one 3200-unit patch width on a
  single horizontal axis, and saves `dx11_terrain_adjacent_A.bin`
  and `dx11_terrain_adjacent_B.bin` using non-overwriting
  `CREATE_NEW`. No changes to DX9 VB uploads, index snapshots,
  camera, renderer, or texture state.
- The two saved patch files each use the separately validated
  6956-byte original vertex snapshot codec, with original world
  positions/normals preserved and existing single-patch and
  LOD capture files left untouched. A prior pair is never overwritten.
  The log records each source origin and delta. If no neighbor is
  encountered in the bounded search, no file is created.
- **Windows preflight order:** CPU-only snapshot probe build /
  self-test FIRST; only after that passes, opt-in GameLib and
  UserInterface compile/link with original production DX9
  EXE and snapshot backups verified. Never launch a regular
  `Project_Client/metin2release.exe` replacement for diagnostics.
  Do not claim real adjacent capture/edge continuity until actual
  Windows audit reports exit code 0.
- Technical caveat: source-space adjacency alone establishes
  geometric neighborhood; game map identity, runtime environment,
  normals, and texture border splats are separate concerns.
  The audit reports discontinuities rather than silently stitching
  or modifying the terrain.

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
- [x] Verified real in-game 3-LOD topology (574/648/552 source indices, 512/216/184 converted triangles; decoder exit 0 on 2026-11-05).
- [ ] Verify actual map vertex capture and texture splat coordinates without mutating DX9 rendering.
- [ ] Cross-check LOD edge stitching/orientation against captured legacy streams.

### Gate 1f - verified index snapshot transport (no GameLib hook)
- [x] Locate actual CPU LOD index streams before WRITEONLY DX9 upload.
- [x] Define bounded versioned 3-LOD snapshot codec with checksum.
- [x] Add isolated x64 console in-memory roundtrip and malformed-data tests.
- [x] Compile codec into x64 EterLib with no use from game renderer.
- [x] User Windows Release x64 DX11LODSnapshotProbe build and self-test (all fixture/corruption tests).
- [x] User Windows Release x64 EterLib build passed with codec.
- [x] Opt-in GameLib pre-upload capture implemented, built, linked and run in a real game map.
- [x] Validate captured actual LOD0/1/2 streams using the versioned snapshot and topology converter.
- [ ] Visually cross-check winding, border seams and edge stitching in the isolated DX11 renderer.

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
- [x] User screenshots show D3D9 rendering world terrain, actors, UI and combat effects.
- [ ] Verify runtime EXE identity, clean exit and syserr; collect timed FPS metrics.
- [x] User opt-in x64 GameLib rebuild + UserInterface link passed.
- [x] Preserve the original working DX9 runtime EXE; version-specific SHA256-matched backup confirmed.
- [ ] Verify runtime EXE identity/hash and a clean normal DX9 exit.
- [x] Run isolated opt-in capture after backup and exact environment-variable verification.
- [x] Run alternate-name copied diagnostic EXE; verify original runtime EXE SHA256 unchanged.
- [x] Actual 3-LOD capture generated and verified (574/648/552 indices; 512/216/184 triangles).

### Gate 1j - captured real-index DX11 stand-alone preview (implementation; test pending)
- [x] Add explicit `--lod-snapshot` Windows Unicode snapshot path support to isolated TerrainProbe.
- [x] Reuse shared bounded, checksummed, topology-aware snapshot decoder; no silent fallback on failure.
- [x] Build three GPU immutable meshes from real captured LOD0/1/2 index streams with the existing synthetic 17x17 vertex grid.
- [x] Add controls: 1/2/3 select captured LOD, W wireframe, R real-index mode, P synthetic patch mode.
- [x] Keep no-argument TerrainProbe mode unchanged; no DX9 runtime/GameLib code changed.
- [ ] Windows Release x64 clean rebuild and link of standalone DX11TerrainProbe.
- [ ] Visual wireframe screenshots for captured LOD0 512 triangles, LOD1 216 triangles, LOD2 184 triangles.
- [ ] Verify that capture snapshot SHA256 remains identical, normal DX9 client unchanged.
- [ ] Cross-check seam/winding behavior; inspect real vertex position/normal layout separately before any runtime integration.

## Gate 1k Windows verification update — 2026-11-05

- User fetched branch through `70921fc3` via fast-forward.
- `DX11LODSnapshotProbe.vcxproj` `/t:Rebuild` `Release|x64` completed with build exit **0**, compiling both legacy LOD codec and new `DX11LegacyPatchSnapshot.cpp`.
- No-argument verifier reported all original LOD tests `[OK]`, plus patch vertex encode, decode, roundtrip, FNV checksum-corruption rejection and invalid normal rejection `[OK]`, test exit **0**.
- All testing was synthetic/CPU only; `dx11_terrain_patch_capture.bin` has **not** been captured or verified. Next required gate is the opt-in GameLib + UserInterface build, then controlled test from the runtime working directory, without overwriting original DX9 EXE or existing LOD snapshot.

### Gate 1k - first real 17x17 position/normal capture
- [x] Source audit identified original height-map and normal-map vertex producer before DX9 upload.
- [x] Isolate snapshot hook to `TerrainPatch.cpp` with explicit compile + process environment switches.
- [x] Define fixed 289-position/normal float32 snapshot codec and checksum, no game shader changes.
- [x] Extend existing CPU verifier with synthetic roundtrip and corruption tests.
- [x] User Windows x64 Release standalone verifier rebuild and all self-tests (2026-11-05; build exit 0, test exit 0).
- [x] User Windows GameLib opt-in + UserInterface link and alternate-name test EXE (2026-11-05).
- [x] Actual real vertex capture file generated with opt-in flag, 6956-byte format (289 vertices; process PID 11484).
- [x] Independently verify captured positions/normals and file checksum (289 vertices, minZ 24249, maxZ 28585, exit 0).
- [ ] Full original DX9 game runtime regression after isolated capture.
- [ ] Visually render real terrain vertex heights combined with the verified real LOD snapshot.

### Gate 1L - verified real patch vertex GPU preview
- [x] Verify captured 289-vertex patch file against fixed snapshot decoder, checksum, finite values (user Windows exit 0).
- [x] Implement separate read-only Unicode real patch loader with fail-closed behavior.
- [x] Add rotated, recentered original Z-up to viewer Y-up CPU adapter and synthetic math regressions.
- [x] Compose captured original patch vertices with all 3 verified original index topology streams in isolated immutable DX11 meshes.
- [x] Retain unmodified no-argument synthetic, synthetic-patch and previous LOD-only modes.
- [x] User Windows Release x64 standalone DX11TerrainProbe compiled/ran: 7 screenshots show the expected D3D11 title and rendered terrain; no exact build-log exit code or process-exit code supplied in the screenshot turn.
- [x] User screenshots: real patch LOD0 (512 tris), LOD1 (216), LOD2 (184), V-toggle synthetic LOD2, synthetic full terrain, and synthetic 17x17 patch wireframe/solid. Real-patch SOLID mode not yet shown.
- [ ] Verify capture files unchanged (hash), clean DX11 test exit and original working DX9 runtime EXE unchanged.
- [ ] Verify original game patch neighborhood seams / winding and splat layers separately.

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

**Checkpoint (2026-11-05):** Real in-game 3-LOD capture is VERIFIED by the standalone decoder (exit code 0; 912 converted triangles). The real game remains on DX9 and in-game DX11 terrain rendering remains UNVERIFIED.
