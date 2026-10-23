# x64 Modern Stack Migration

Baseline
- Stable Win32 checkpoint: 5bfdb3eda4bd0ae7688944c1e6d42d3fee30be13
- Tag: pre-client-x64-win32-stable-2026-10-23
- Migration branch: migration/client-x64
- Win32 Debug/Release/Distribute baseline must remain buildable until x64 reaches game phase.

Architecture
- Client target: Windows x86-64, MSVC v143, C++20.
- x64 outputs are isolated under platform-specific bin/obj paths.
- x64 external libraries must live separately from legacy x86 libraries.
- Never link an x86 static/import library into an x64 configuration.
- Wire/network layouts remain fixed-width and must not inherit pointer-size changes.

Dependency targets
- Python: CPython 3.15.1 x64. Port Python 2 C-API bindings; do not use a Python 2.7 x64 stopgap as the final state.
- libjpeg: libjpeg-turbo 3.2.0 x64.
- Crypto++: 8.9 x64.
- LZ4: 1.10.0 x64.
- Snappy: 1.2.2 x64.
- LZO: latest compatible LZO2 x64 release/build.
- DevIL/ILU: latest maintained compatible x64 build, or replace if maintenance/API compatibility blocks migration.
- CEF: current stable x64 CEF build pinned during the browser migration wave.
- Miles: Miles Sound System 10 x64, subject to a valid RAD license/SDK; migrate the MilesLib wrapper to the current API.
- Granny: latest licensed Granny x64 SDK supplied by RAD; preserve .gr2 data compatibility and validate existing assets.
- SpeedTree: latest licensed SpeedTree Games x64 SDK compatible with the project asset migration; old SpeedTreeRT x86 binary is not accepted in the x64 build.
- DIO pack library: excluded; ENABLE_PACK_TYPE_DIO remains disabled.

Migration order
1. Native x64 Visual Studio configurations.
2. Pointer/handle/cast compile cleanup.
3. Open-source dependency x64 rebuild/upgrades.
4. CPython 3.15 C-API and script migration.
5. CEF x64 migration.
6. Granny, Miles and SpeedTree licensed SDK migrations.
7. Final UserInterface x64 link.
8. Login/character/game/map regression testing against the stable server protocol.
9. Debug/Release/Distribute x64 validation.

Acceptance
- No x86 library appears in the x64 linker map.
- Debug, Release and Distribute x64 build successfully.
- Client reaches game phase and performs map changes.
- Packet static_assert checks remain valid.
- Win32 baseline checkpoint remains untouched.
