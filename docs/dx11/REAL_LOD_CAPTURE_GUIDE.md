# Real Metin2 LOD capture — guarded diagnostic, not a DX11 gameplay renderer

Status (2026-11-05): **Opt-in runtime 3-LOD capture VERIFIED**. Real snapshot passed standalone decode and topology conversion (exit code 0). In-game DX11 rendering is NOT implemented or verified.

## Proven source location

`Src/GameLib/MapOutdoorIndexBuffer.cpp::CMapOutdoor::SetIndexBuffer()`
creates three CPU index arrays named `m_pwaIndices[0..2]` and their
`count[0..2]` lengths. For the normal non-`WORLD_EDITOR` renderer:

- LOD 0 is a triangle strip with degenerate row-joining indices.
- LOD 1 and LOD 2 are triangle lists with special edge stitching.
- These CPU arrays are copied into D3D9 `D3DUSAGE_WRITEONLY` index buffers
  and then deleted. The capture hook is placed **before** that loop.

## Protection: OFF by default in two independent ways

1. Compile-time: only `Release|x64` `GameLib` builds with the explicitly
   supplied property `/p:EnableDX11LODCapture=true` define
   `M2_DX11_LOD_CAPTURE` **for MapOutdoorIndexBuffer.cpp only**.
   Default builds leave the capture code out of the object file.
2. Run-time: even in that special build, the code activates only with the
   exact process environment value
   `METIN2_DX11_LOD_CAPTURE_ONCE=1`. Without it, it does nothing.

Other restrictions:

- Captures at most **once per process**. Does not modify original CPU index
  arrays, counts, index order, DX9 buffer creation or rendering state.
- Writes `dx11_terrain_lod_capture.bin` in the **game process working
  directory**, which can differ from the EXE folder. The file is
  automatically ignored by the Git repository.
- Uses **CREATE_NEW**, so an existing capture file will never be overwritten.
- The snapshot encoder validates all three LOD streams before file creation.
  Failure logs a `[DX11 LOD CAPTURE]` message and continues legacy DX9.
- File I/O can briefly affect load time; this is diagnostic-only and
  unsuitable for production distribution. Run in a controlled test client.
- Never include the capture file in Git commits or share it publicly
  without inspecting its contents.

## Gate A — build proof without activating in-game capture

From the EXISTING `Project_ClientSrc` root, after safe fetch and merge:

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

& $msbuild ".\Tools\DX11Smoke\DX11LODSnapshotProbe.vcxproj" /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "SnapshotProbe build failed" }
& ".\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe"
if ($LASTEXITCODE -ne 0) { throw "SnapshotProbe self-tests failed" }

& $msbuild ".\Src\EterLib\eterlib.vcxproj" /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "EterLib x64 build failed" }

# Default: diagnostic is not compiled.
& $msbuild ".\Src\GameLib\gamelib.vcxproj" /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:EnableDX11LODCapture=false
if ($LASTEXITCODE -ne 0) { throw "Default GameLib x64 build failed" }

# Dedicated diagnostic object: code is compiled but runtime capture remains off.
& $msbuild ".\Src\GameLib\gamelib.vcxproj" /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:EnableDX11LODCapture=true
if ($LASTEXITCODE -ne 0) { throw "Opt-in GameLib x64 build failed" }
```

These are **build-only** steps. Do not run the game with the environment
variable enabled until the linker path, DX9 baseline and file verifier
have been checked. A successful static-library build does not verify
that the game EXE links in the new diagnostic object and codec.

After testing the special build, restore default compiled objects:

```powershell
& $msbuild ".\Src\GameLib\gamelib.vcxproj" /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:EnableDX11LODCapture=false
if ($LASTEXITCODE -ne 0) { throw "Default GameLib rebuild failed" }
```

## Gate B — controlled real-client capture (COMPLETED for first real 3-LOD sample)

Only after Gate A and explicit game-link/runtime checks:

- Build/link the x64 **test** client using the opt-in GameLib and updated
  EterLib. Do not distribute this diagnostic build.
- Launch that test client from a known working directory with
  `METIN2_DX11_LOD_CAPTURE_ONCE=1` set **for that process only**.
- Confirm a single `dx11_terrain_lod_capture.bin` is created (not
  overwritten). The `[DX11 LOD CAPTURE]` log includes LOD counts.
- Validate the file with the isolated verifier:

```powershell
& ".\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe" --verify "C:\path\to\dx11_terrain_lod_capture.bin"
```

- Compare LOD0 strip triangles and LOD1/2 lists to the DX9 indexed draw
  counts, edge topology, normals and real terrain textures.
- Remove the environment variable, rebuild with the compile flag OFF, and
  repeat the ordinary DX9 login/map regression.

## Gate B update: protect the existing live DX9 client

The user's Windows logs confirm opt-in GameLib rebuild and
`UserInterface.vcxproj` x64 **link** succeeded, producing
`Project_ClientSrc/Src/bin/Release/x64/metin2release.exe`.
The prior DX9 EXE backup attempt **failed** because that build-output EXE
was missing *at the time of the backup*. The actual gameplay `syserr`
identifies the running executable at
`Project_Seferihisar/Project_Client/metin2release.exe`.

For a controlled local capture, preserve and SHA256-verify **that**
running `Project_Client/metin2release.exe`, not merely the compiled
output. Never overwrite the existing EXE with the diagnostic build.
Place a **copy** of the opt-in build beside the original under the
different filename `metin2release_dx11_lod_probe.exe` (do not
overwrite pre-existing files), and launch it using the **game runtime
directory** as `WorkingDirectory`. This keeps the installed packs,
locale and DLL search path together.

Use `METIN2_DX11_LOD_CAPTURE_ONCE=1` **only for the test process**;
unset it in the launching PowerShell immediately after process spawn.
If the game requires a specific EXE name or launcher arguments, the
alternate name may not start; stop and inspect diagnostics before
considering any replacement strategy. Never delete the original EXE.

The snapshot path is
`Project_Client/dx11_terrain_lod_capture.bin`, which is created
at most once using Win32 `CREATE_NEW`. If it exists, inspect it;
do **not** overwrite/delete/rotate it automatically. On successful
capture, verify with the independent
`Tools/DX11Smoke/bin/x64/Release/DX11LODSnapshotProbe.exe --verify`,
which reports source index counts and converted triangle counts for all
three original LODs. Also compare the original runtime executable's
SHA256 with its pre-capture value.

Important: the user's `client_syserr.txt` contains a separate
`game.py:1613 SaveScreen` UnicodeDecodeError related to legacy
path decoding, not verified to involve this capture. Keep any fix
to that code in a separate tested change. **Do not call the capture
successful until a real snapshot exists and the verifier passes.**

## Gate B diagnostic v2: compiled hook is present, capture file absent

User-reported SHA256 inspection confirmed the opt-in probe EXE and
compiled output EXE are byte-for-byte identical and both contain the
literal capture environment and log strings; the normal DX9 EXE
contains neither string. However, the prior live test exited 0 without
a `dx11_terrain_lod_capture.bin` or `[DX11 LOD CAPTURE]` log.

The hook now emits exactly one additional entry *per opt-in process*
after the original CPU LOD streams are created, independent of the
runtime capture flag:

```text
[DX11 LOD DIAG] hook reached | envLength=N | enabled=E | LOD0=N LOD1=N LOD2=N
```

Interpretation:

- **No `[DX11 LOD DIAG]` log:** either the new diagnostic EXE
  did not execute/enter a map, the `SetIndexBuffer` path did not
  execute, or logs were written somewhere other than the file checked.
  Confirm fresh syserr timestamps and `BOOTTRACE: AFTER SendEnterGamePacket`
  before inferring a linker problem.
- **`enabled=0`:** hook reached but the exact one-character runtime
  flag was not inherited. `envLength=0` means unset or empty;
  `envLength>1` indicates an unexpected or truncated value.
- **`enabled=1`:** capture was attempted. Expect a second log
  containing `created`, `validation failed`, `cannot create`,
  `write failed`, or `exception`. Also search the actual process
  working directory, not just the EXE folder, for the snapshot file.

Do not run the stale `metin2release_dx11_lod_probe.exe`: the
new one-shot diagnostics will not be present in it. Compile and
link with `/p:EnableDX11LODCapture=true`, then copy the newly
built EXE under a new distinct name. Verify SHA256 matching and leave
the original `Project_Client/metin2release.exe` untouched.
No game/renderer changes are enabled in ordinary builds.

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

## Gate C — standalone DX11 viewer for the verified REAL LOD indices (NEW; Windows build pending)

Commit series beginning `9d6773d` adds an **opt-in** loader to the
existing `Tools/DX11Smoke/DX11TerrainProbe.vcxproj` app.
Only `TerrainProbe.cpp` and its standalone project file were modified;
there is **no modification** to production GameLib rendering or its
DX9 shader/state code.

- Unmodified no-argument invocation remains the existing synthetic
  terrain viewer (P for synthetic 17x17 patch, W for wireframe).
- `--lod-snapshot "<FULL PATH TO CAPTURE.BIN>"` explicitly opens
  the file through Unicode `CreateFileW`, with a maximum byte bound
  and the existing shared snapshot decoder (magic/version, three
  counts, checksum, index bounds, strip/list topology). Invalid input
  terminates with an error; it never falls back silently.
- Only the **captured index topology** is real. The program builds
  three separate DX11 immutable index/vertex buffers using the
  snapshot's LOD0 strip, LOD1 list and LOD2 list, converted into
  triangle lists and assigned to the **pre-existing synthetic 17x17
  preview vertex heights, normals and procedural grass**.
- Explicit snapshot launch starts in **wireframe** mode on captured
  LOD0. Press `1`, `2`, `3` for actual LOD0/1/2 converted
  index streams; `W` toggles solid/wireframe, `P` switches to the
  existing synthetic 17x17 preview, and `R` toggles the real-index
  preview. The title announces `CAPTURED INDEX LOD` and
  `SYNTHETIC HEIGHTS` to prevent confusing it for real game terrain.
- The file is **read-only**; no copies of capture data are committed
  and nothing is written back into the game runtime directory.

Example PowerShell commands, run individually from any ordinary
(non-administrator) PowerShell window:

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11TerrainProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11TerrainProbe.exe' --lod-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_lod_capture.bin'
```

**Do not count this gate as Windows-verified yet.** The code has been
reviewed against the project and snapshot codec, but the next
required evidence is user Windows Release x64 compile/link, a
screenshot showing each `CAPTURED INDEX LOD0/1/2` in wireframe,
a clean close/exit code, and confirmation that the snapshot SHA256 is
unchanged. Real in-game vertex positions, texture splatting and
terrain material data have NOT been captured or ported.

## Gate D — guarded REAL 17x17 vertex snapshot (source committed; Windows test PENDING)

The user's six images verify that isolated DX11 TerrainProbe displays the
**captured** LOD0/1/2 index topology (512/216/184 triangles) over the
**existing synthetic** 17x17 vertex heights. The ordinary large terrain
and synthetic patch previews also remain visible. This proves the
standalone DX11 index-data path only; not actual game terrain positions.

**Read-only source tracing** identified:
`GameLib/AreaTerrain.cpp::CTerrain::_CalculateTerrainPatch` obtains
the real heightmap / normal map vertices and calls
`CTerrainPatch::BuildTerrainVertexBuffer(akSrcTerrainVertex)`.
The existing entry point in `GameLib/TerrainPatch.cpp` was used for
the optional capture hook so the potentially legacy-encoded
`AreaTerrain.cpp` does **not** need rewriting.

Safety conditions:

- The compile property `/p:EnableDX11PatchCapture=true` affects
  only `TerrainPatch.cpp` in `GameLib` **Release|x64**.
  Both the define `M2_DX11_PATCH_CAPTURE` and removal of
  per-file PCH reuse are conditional; default builds remain OFF.
- The process environment variable must be exactly
  `METIN2_DX11_PATCH_CAPTURE_ONCE=1`; the hook records one
  `[DX11 PATCH DIAG]` entry in special builds and creates at most
  one snapshot. Normal game execution uses no new CPU path.
- Snapshot: `dx11_terrain_patch_capture.bin` is
  **6956 bytes**, magic `M2PVERT1`, version 1,
  289 original position+normal records, 6 IEEE754 float32 per record,
  little-endian and FNV-1a checksum. This records original
  game **Z-up** positions and normals without transforming data.
- File output is `CREATE_NEW` in the test process working
  directory: never delete or overwrite existing snapshots as
  part of scripts. Git ignores the file. DX9 vertex allocation,
  lighting and upload code remain unchanged.
- Standalone `DX11LODSnapshotProbe` now tests both the old
  3-LOD index codec **and** a synthetic patch vertex codec with
  checksum/corruption checks; `--verify-patch <file>` reports
  original vertex count and Z range. Existing
  `--verify <lod-file>` continues to work.

**Next Windows validation order:** Fetch the branch and rebuild ONLY
`Tools/DX11Smoke/DX11LODSnapshotProbe.vcxproj` first. Its
no-argument selftests MUST pass; re-verify the existing
`dx11_terrain_lod_capture.bin`. After that, compile the
`GameLib` opt-in patch flag and link `UserInterface`
without overwriting the existing working
`Project_Client/metin2release.exe`. As previously proven,
the exact process-only environment should be set with CMD
`set "METIN2_DX11_PATCH_CAPTURE_ONCE=1"`, launching the
separately named diagnostic EXE from `Project_Client`.
Only then run the verifier from the game working directory.
The special build must never be shipped as the production client.

**Windows compiler and in-game patch-capture results have NOT been
reported yet; do not mark this gate completed.** The next viewer
step should translate original game Z-up coordinates to DX11
Y-up after capture validation, using a separate tested adapter.

## Gate E — REAL 289 vertices + captured 3-LOD standalone DX11 view (COMMITTED; Windows build pending)

The user independently validated the captured vertex file with
`DX11LODSnapshotProbe.exe --verify-patch`:
`vertices=289 minZ=24249 maxZ=28585 height range=4336`, exit 0.
The real 3-LOD indices were already checked separately. The captures
are **read-only evidence** and must be retained; do not recapture.

The standalone `DX11TerrainProbe` now supports **two input files**:

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11TerrainProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11TerrainProbe.exe' --lod-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_lod_capture.bin' --patch-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_patch_capture.bin'
```

- Explicit **both files** means real captured vertex coordinates
  combined with original captured LOD topology, shown in initial
  wireframe LOD0 view. `1/2/3` cycle LODs, `W` toggles
  wireframe/solid, `V` switches real and synthetic heights for
  exact index-topology comparison, `R` toggles the original
  LOD mode and `P` selects synthetic 17x17.
- No arguments: original full synthetic mode. LOD-only file:
  previous verified original indices on **synthetic** 17x17 heights.
  Patch-only file: invalid; the real patch snapshot must be
  paired with a valid LOD snapshot. Missing/corrupt input
  returns a dialog and nonzero process status, never silently
  draws a different mesh.
- The game stores **Z as height**. The standalone DX11
  preview uses **Y as height**. The CPU adapter maps
  original `(x,y,z)->(x,z,-y)` for both positions and normals,
  recenters the sample patch, applies **uniform** resizing
  and chooses a fitting camera (no height exaggeration).
  A rotation math fixture rejects invalid normals and
  compares original equal-grid step lengths.
- Patches were captured independently: the LOD file is a
  globally initialized index sequence while the vertex
  snapshot was the **first generated terrain patch**. The
  preview is a *single captured patch + valid shared LOD
  structure*, not yet a full-world material/splat/render
  migration or a test of stitching adjacent patches.

**Test gate:** Confirm Release x64 compile/link exit 0.
Provide captured vertex LOD0/1/2 wireframe screenshots,
LOD0 solid and V toggle (synthetic comparison). Verify
snapshot SHA256 values remain unchanged and process closes
with exit code 0. If build fails, stop and inspect error;
DO NOT copy a probe into the production game directory.

## Gate F — screenshots confirm DX11 real vertex geometry; audit steep cliff (2026-11-05)

The user supplied seven independent DX11 TerrainProbe screenshots:
three **REAL PATCH VERTICES** wireframe modes (LOD0=512, LOD1=216,
LOD2=184 triangles), one original **captured indices + synthetic
heights** mode, full synthetic terrain wireframe, and 17x17 synthetic
patch wireframe/solid. The D3D11 overlay is visible and reports
~144 FPS **in this isolated test only**. Normal DX9 Metin2 gameplay
was not changed or benchmarked.

The real 289-vertex patch has a cliff-like silhouette, potentially
expected given its verified original source height range of 4336
DX9 units. Rather than alter the viewer's vertical scale or draw
mode without evidence, use the new **CPU-only** geometry audit:

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11LODSnapshotProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client'
& 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe' --audit-geometry 'dx11_terrain_lod_capture.bin' 'dx11_terrain_patch_capture.bin'
```

The audit reports original X/Y/Z size, Z/horizontal ratio, source
normal upward orientation, steep immediate-neighbor edges, and for
each converted LOD index set its projected triangle winding and
degenerate counts. Important: the original Y coordinate is negative
as the terrain rows advance; the audit reports source-space XY
winding to avoid mistaking the display rotation for source data.

Audit limitations: **one original patch and global LOD topology**,
not two adjacent map patches. Consistent winding is necessary but not
sufficient proof that DX11 rasterizer culling matches the DX9 render
state. No actual terrain textures, splats, material layers, seam
neighbors, or production renderer have been migrated. Do not modify
the DX9 game or recapture snapshots on this gate.

Windows Release x64 rebuild and actual CPU geometry audit **PASSED**,
exit code `0`, on 2026-11-05. The real captured patch spans
3200 x 3200 original XY units and 4336 Z units (ratio 1.355);
normal mean Z-up 0.750077. All 544 neighbor edges are valid:
30 >=60 degrees, 18 >=75 degrees, maximum 82.5454 degrees.
All converted triangles have **positive projected original XY winding**
(LOD0=512, LOD1=216, LOD2=184; negative=0 and projected degenerate=0).

**Culling caveat:** `Tools/DX11Smoke/TerrainProbe.cpp` currently
sets `D3D11_CULL_NONE` in both wireframe and solid modes. This
means screen visibility does not yet prove the matching front-facing
rasterizer policy. A separate controlled comparison of DX9 cull state
vs D3D11 front-face mode is required before an in-game renderer swap.

**Next test:** show the captured patch in solid mode (both snapshots
loaded, press W), compare normals and front/back winding without
altering saved captures, then test two adjacent real patches with
their actual IDs/coordinates before edge stitch validation.
The existing captured index snapshot is a global topology sample,
not a second real patch.

## Gate G — optional DX11 backface culling comparison (source committed, Windows test pending)

Two user screenshots confirm that the original **real 289-vertex
patch** + **captured LOD0 512 triangles** render in both the existing
DX11 **wireframe and solid** modes. The solid checker/grass texture
is generated by the standalone probe; it is **not** a captured
Metin2 splat layer.

Source check: `Src/EterLib/StateManager.cpp::SetDefaultState`
sets legacy DX9 culling to `D3DCULL_CW`. The checked
terrain RenderHTP/RenderSTP code does not explicitly replace that
state. However, actual state can be changed elsewhere; DX9 projected
winding must be compared with DX11 rasterizer conventions. The
standalone test previously used `D3D11_CULL_NONE`, so its prior
visibility did not prove correct backface culling.

**New optional key C** cycles **OFF (default) -> BACK/FRONT_CW ->
BACK/FRONT_CCW -> OFF** in the isolated TerrainProbe. Separate
wireframe and solid rasterizer states are created for each mode.
The title includes `C:OFF`, `C:BACK/FRONT_CW` or
`C:BACK/FRONT_CCW` so screenshots can verify the selected
state. The original default `C:OFF` and previously working
`W`, `1/2/3`, `V`, `P`, `R` commands remain unchanged.
No index conversion or vertex data is modified.

**Test (PowerShell 5.1, individual commands):**

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11TerrainProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11TerrainProbe.exe' --lod-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_lod_capture.bin' --patch-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_patch_capture.bin'
```

**User screenshots on 2026-11-05 VERIFIED the LOD0 comparison:**

- C:OFF wireframe and C:OFF solid both render the full captured
  cliff face (512 triangles).
- C:BACK/FRONT_CW solid retains the full visible terrain surface.
- C:BACK/FRONT_CCW solid removes nearly all of that face,
  leaving thin fragments.

For the isolated DX11 camera/axis transformation, **backface culling
with front clockwise (`FrontCounterClockwise=FALSE`)** is the
observed correct visible-facing policy. Do not confuse the DX9
`D3DCULL_CW` (which winding gets culled) with the DX11
`FrontCounterClockwise` flag (which winding is front-facing).
This does **not** yet establish production-game global raster state.

**Next without rebuilding:** launch the existing standalone probe with
both already captured read-only snapshots, select Solid and
C:BACK/FRONT_CW, then test **LOD1 via 2** and **LOD2 via 3**.
Send screenshots of both before integrating any in-game changes.
After that, capture two spatially adjacent real patches with
recorded origins in a separate compile+runtime-gated diagnostic,
without overwriting existing snapshots or default DX9 game behavior.

## Gate H — source-neighbor 17x17 seam capture (STAGED; Windows tests required)

User screenshots verify LOD0/1/2, all **solid**, with
`C:BACK/FRONT_CW`: 512/216/184 triangles visible in
the isolated D3D11 viewer. The culling comparison for the
sample's current camera is now passed.

A new **separate opt-in** capture pathway and CPU-only seam
validator have been committed. No default game/renderer code is
enabled, and previous LOD and single-patch vertex files are
left unchanged. Its separate runtime files are:

```text
dx11_terrain_adjacent_A.bin
dx11_terrain_adjacent_B.bin
```

Each uses the established `M2PVERT1` original position/normal
snapshot (6956 bytes, 289 source vertices). In the special
build only, the sampler compares world coordinate origins,
accepting exact +/-3200 game units on **one** X or Y axis,
rather than assuming two consecutive callback invocations
are neighbors. The search is limited to 256 distinct
positions and saves one pair with two `CREATE_NEW` writes.
A single failed acquisition leaves normal DX9 drawing
untouched, and existing files are never overwritten.

Compile-time gate: opt-in Release x64 **only**
`/p:EnableDX11AdjacentCapture=true` for `GameLib` and
`UserInterface`; runtime gate:
`METIN2_DX11_ADJACENT_CAPTURE_ONCE=1` in the exact
elevated probe process. The original DX9 executable stays
in the runtime folder and is hash-checked; the test EXE
must use a distinct basename. As with previous captures,
execute the test EXE from the original game working directory,
then close the game normally before reading `client_syserr.txt`.

**Before any live run, test only the standalone CPU verifier:**

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11LODSnapshotProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe'
```

Self-test should pass normal index/vertex codec tests PLUS
six new neighbor checks: valid right/left/down/up edges,
deliberately mismatched seam height rejection, and
non-neighbor origins rejection. The live pair acquisition
and `--audit-adjacent patchA.bin patchB.bin` must
remain **PENDING** until this fresh Windows rebuild passes.

The new validator refuses non-neighbor origin and compares
all 17 original border positions, their maximum gap and
normal differences. It detects and reports discontinuities
without fixing them. If two neighbors fall across separate
game-map instances with coincident world coordinates, a
future map identifier must be added before claiming
full-world provenance. Texture splatting and DX9 renderer
state are unchanged.

## Gate I — two verified real neighbors together in an isolated DX11 viewer (STAGED; Windows test pending)

The user ran the special adjacent capture with a verified original
DX9 backup and obtained two NEW original-source `M2PVERT1`
files. Same live game session: A origin `(0,0)`, B origin
`(3200,0)`; each is exactly 6956 bytes. The independent
CPU verifier accepted both files and compared the 17 shared
border positions and normals: maximum position gap **0**,
mismatch count **0**, maximum normal delta **0**, audit exit **0**.
Do not overwrite or recapture these snapshots.

The standalone `DX11TerrainProbe` now has a strict new optional
dual-patch command-line mode. The loader reads the LOD, A and B
snapshots, checks their checksums and valid 17-vertex shared
edge, and refuses invalid inputs before creating the D3D11
renderer. The two original arrays are not modified or welded.
They are independently uploaded to GPU index/vertex buffers
with a **single shared world-space recentering and scale**.
This is crucial for genuine visual seam comparisons; independent
recentering would create artificial disconnections or hide cracks.
Positions/normals use original `(X,Y,Z)` to preview
`(X,Z,-Y)` rotation without relative-height exaggeration.

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11TerrainProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11TerrainProbe.exe' --lod-snapshot 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_lod_capture.bin' --adjacent-a 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_adjacent_A.bin' --adjacent-b 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client\dx11_terrain_adjacent_B.bin'
```

New view title: `TWO REAL NEIGHBORS | A:LOD0 (512 tris)
B:LOD0 (512 tris) | Wireframe`. Controls:
`1/2/3` make both A and B LOD0/1/2,
`M` changes ONLY B's LOD while keeping A fixed,
`W` toggles wireframe/solid,
`C` cycles no-cull/CW/CCW states, and
`J` toggles paired versus original LOD-only synthetic
viewer. Existing original standalone synthetic and
single-real-patch commands are unchanged.

Compare equal LOD0 on both, then A=LOD0/B=LOD1 and
A=LOD0/B=LOD2 in wireframe and solid with
`C:BACK/FRONT_CW`. Exact border vertices matching at
source does NOT alone establish there are no cracks
between different LOD index triangulations (T-junctions).
Photographic/GPU evidence is still pending.

**No additional in-game capture or GameLib build is needed.**
Build ONLY the standalone DX11TerrainProbe and use existing
read-only `.bin` files. Do not claim this Windows build
passes until the user supplies a build log and screenshots.
Do not commit binary runtime snapshots.

## Gate J — two real neighboring patches visually render, mixed-LOD border audit pending (2026-11-05)

Ten screenshots show the **two verified neighboring world-space patches**
render in a single independent Direct3D11 TerrainProbe. Same-LOD
wireframe A/B=LOD0, LOD1, LOD2, and solid LOD2 were confirmed,
as were mixed **A:LOD0/B:LOD1** and **A:LOD0/B:LOD2**
in solid mode with `C:BACK/FRONT_CW`. LOD2 FRONT_CCW culling
makes most of the surface disappear, consistent with prior
face-direction tests. No large visible crack in the selected
camera view, but small LOD-edge T-junction cracks remain possible.
This is not evidence of a live DX11 Metin2 game or production FPS.

**NEW NUMERICAL CHECK, CPU-ONLY** (no extra game capture):

```powershell
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc'
git pull --ff-only origin modernization/dx11-renderer
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\Tools\DX11Smoke\DX11LODSnapshotProbe.vcxproj' /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
& '.\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe'
Set-Location -LiteralPath 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_Client'
& 'C:\Users\skill\OneDrive\Masaüstü\Project_Seferihisar\Project_ClientSrc\Tools\DX11Smoke\bin\x64\Release\DX11LODSnapshotProbe.exe' --audit-lod-seams 'dx11_terrain_lod_capture.bin' 'dx11_terrain_adjacent_A.bin' 'dx11_terrain_adjacent_B.bin'
```

The tool validates input checksums, existing source border agreement,
and all **nine** LOD combinations. It extracts actual outer triangle
edge intervals on both sides and compares the two **piecewise-linear
surface boundaries**, instead of merely comparing original 17 stored
vertices (which previously all matched). It reports border segment
counts, maximum geometric separation, and samples separated by more
than 0.05 original game units.

**Interpretation:** `[OK]` means the two interpolated geometric
boundary lines agree within tolerance. `[WARN]` indicates a potential
LOD transition crack and is a **blocking finding**, even if the
process completes the diagnostic with `AUDIT EXIT CODE: 0`.
`[FAIL]` for invalid file, missing boundary coverage or overlapping
edges is a structural failure; stop. A successfully completed audit
alone must not be mistaken for every LOD combination being clean.

New synthetic tests check that a fine/coarse straight border agrees,
a shared intermediate height bump exposes a T-junction gap,
and missing border coverage is rejected. These new tests and
the live nine-pair audit are **NOT YET Windows-verified**.
Neither `Project_Client/metin2release.exe` nor any of the
original four saved `.bin` captures should be rewritten.

## Not yet done

There is **no live DX11 GameLib renderer**, no terrain texture splatting
migration, and no proof that Granny/SpeedTree are DX11-ready. The **real
LOD capture file is now verified**; its geometry has not yet been visualized
in the standalone DX11 terrain probe. This is only a diagnostic path for obtaining real terrain
topology without altering the original DX9 drawing operations.
