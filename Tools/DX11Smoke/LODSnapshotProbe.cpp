// Standalone, CPU-only diagnostic. Does not load Metin2 or create a D3D device.
// Usage: DX11LODSnapshotProbe.exe [--verify dx11_terrain_lod_capture.bin]
#include "../../Src/EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include "../../Src/EterLib/Renderer/DX11LegacyTerrainIndices.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    using m2::render::DX11LegacyIndexSnapshot;
    using m2::render::LegacyIndexSnapshot;
    using m2::render::LegacyIndexView;
    using m2::render::DX11LegacyTerrainIndices;
    using m2::render::LegacyTerrainPrimitive;

    bool Check(const char* label, bool success)
    {
        std::cout << (success ? "[OK] " : "[FAIL] ") << label << '\n';
        return success;
    }

    bool VerifySnapshot(const LegacyIndexSnapshot& snapshot)
    {
        for (std::size_t lod = 0; lod < snapshot.lod.size(); ++lod)
        {
            std::vector<std::uint16_t> triangles;
            const auto primitive = lod == 0
                ? LegacyTerrainPrimitive::TriangleStrip
                : LegacyTerrainPrimitive::TriangleList;
            const auto& original = snapshot.lod[lod];

            if (!DX11LegacyTerrainIndices::ToTriangleList(
                    original.data(), original.size(),
                    DX11LegacyIndexSnapshot::kVertexCount,
                    primitive, triangles))
                return false;

            std::cout << "LOD " << lod
                      << ": source indices=" << original.size()
                      << ", converted triangles=" << triangles.size() / 3u
                      << ", topology=" << (lod == 0 ? "STRIP" : "LIST")
                      << '\n';
        }
        return true;
    }

    bool RunSelfTest()
    {
        // Small, deliberately synthetic example; this is NOT captured map data.
        constexpr std::uint16_t lod0[] = {0, 17, 1, 18, 18, 18, 19, 20};
        constexpr std::uint16_t lod1[] = {0, 17, 1, 1, 17, 18};
        constexpr std::uint16_t lod2[] = {0, 34, 2, 2, 34, 36};
        const std::array<LegacyIndexView, 3> source{{
            {lod0, sizeof(lod0) / sizeof(lod0[0])},
            {lod1, sizeof(lod1) / sizeof(lod1[0])},
            {lod2, sizeof(lod2) / sizeof(lod2[0])}
        }};

        std::vector<std::uint8_t> encoded;
        if (!Check("Encode fixture", DX11LegacyIndexSnapshot::Encode(source, encoded)))
            return false;

        LegacyIndexSnapshot decoded;
        if (!Check("Decode fixture", DX11LegacyIndexSnapshot::Decode(
                encoded.data(), encoded.size(), decoded)))
            return false;
        if (!Check("Preserve original LOD bytes",
                   decoded.lod[0] == std::vector<std::uint16_t>(
                       std::begin(lod0), std::end(lod0)) &&
                   decoded.lod[1] == std::vector<std::uint16_t>(
                       std::begin(lod1), std::end(lod1)) &&
                   decoded.lod[2] == std::vector<std::uint16_t>(
                       std::begin(lod2), std::end(lod2))))
            return false;

        if (!Check("Decode converted topology", VerifySnapshot(decoded)))
            return false;

        // Failed input must not change the previously decoded snapshot.
        const LegacyIndexSnapshot saved = decoded;
        encoded[encoded.size() / 2u] ^= 0x20u;
        if (!Check("Checksum corruption rejected",
                !DX11LegacyIndexSnapshot::Decode(encoded.data(), encoded.size(), decoded) &&
                decoded.lod == saved.lod))
            return false;

        // Boundary tests on a well-formed output preserving the last bytes.
        std::vector<std::uint8_t> untouched{42, 84};
        const std::array<LegacyIndexView, 3> invalid{{
            {nullptr, 0}, {lod1, 6}, {lod2, 6}
        }};
        if (!Check("Invalid source rejected without mutation",
                !DX11LegacyIndexSnapshot::Encode(invalid, untouched) &&
                untouched == std::vector<std::uint8_t>{42, 84}))
            return false;

        std::cout << "[OK] Snapshot codec self-test complete; no game files written.\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc == 1)
        return RunSelfTest() ? 0 : 1;

    if (argc != 3 || std::string(argv[1]) != "--verify")
    {
        std::cerr << "Usage: DX11LODSnapshotProbe.exe [--verify capture.bin]\n";
        return 2;
    }

    LegacyIndexSnapshot snapshot;
    if (!DX11LegacyIndexSnapshot::LoadFile(argv[2], snapshot))
    {
        std::cerr << "[FAIL] Invalid LOD snapshot or file inaccessible.\n";
        return 3;
    }

    return Check("Capture file validated", VerifySnapshot(snapshot)) ? 0 : 4;
}
