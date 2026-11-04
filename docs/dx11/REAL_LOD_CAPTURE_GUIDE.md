# Real Metin2 LOD capture — guarded diagnostic, not a DX11 gameplay renderer

Status: **code staged, Windows build/runtime validation PENDING**.

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

## Gate B — controlled real-client capture (DO NOT perform yet)

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

## Not yet done

There is **no live DX11 GameLib renderer**, no terrain texture splatting
migration, no proof that Granny/SpeedTree are DX11-ready, and no confirmed
real capture file. This is only a diagnostic path for obtaining real terrain
topology without altering the original DX9 drawing operations.
