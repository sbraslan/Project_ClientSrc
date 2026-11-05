// Standalone, CPU-only diagnostic. Does not load Metin2 or create a D3D device.
// Usage: DX11LODSnapshotProbe.exe [--verify dx11_terrain_lod_capture.bin]
#include "../../Src/EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include "../../Src/EterLib/Renderer/DX11LegacyTerrainIndices.h"
#include "../../Src/EterLib/Renderer/DX11LegacyPatchSnapshot.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using m2::render::DX11LegacyIndexSnapshot;
    using m2::render::DX11LegacyPatchSnapshot;
    using m2::render::LegacyPatchVertex;
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

    bool RunPatchSelfTest()
    {
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> source{};
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            source[i].position[0] = static_cast<float>(i % 17u);
            source[i].position[1] = -static_cast<float>(i / 17u);
            source[i].position[2] = static_cast<float>(i % 7u) * 0.25f;
            source[i].normal[0] = 0.0f;
            source[i].normal[1] = 0.0f;
            source[i].normal[2] = 1.0f;
        }

        std::vector<std::uint8_t> bytes;
        if (!Check("Patch vertex encode fixture",
                   DX11LegacyPatchSnapshot::Encode(
                       source.data(), source.size(), bytes) &&
                   bytes.size() == DX11LegacyPatchSnapshot::kFileBytes))
            return false;

        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> decoded{};
        if (!Check("Patch vertex decode fixture",
                   DX11LegacyPatchSnapshot::Decode(
                       bytes.data(), bytes.size(), decoded)))
            return false;
        if (!Check("Patch vertex roundtrip",
                   decoded[0].position[0] == source[0].position[0] &&
                   decoded[128].position[2] == source[128].position[2] &&
                   decoded.back().normal[2] == 1.0f))
            return false;

        const auto prior = decoded;
        bytes[bytes.size() / 2u] ^= 0x40u;
        if (!Check("Patch checksum corruption rejected",
                   !DX11LegacyPatchSnapshot::Decode(
                       bytes.data(), bytes.size(), decoded) &&
                   decoded[0].position[0] == prior[0].position[0] &&
                   decoded[128].position[2] == prior[128].position[2]))
            return false;

        std::vector<std::uint8_t> unchanged{13u, 7u};
        source[0].normal[2] = 0.0f;
        if (!Check("Patch invalid normal rejected without mutation",
                   !DX11LegacyPatchSnapshot::Encode(
                       source.data(), source.size(), unchanged) &&
                   unchanged == std::vector<std::uint8_t>{13u, 7u}))
            return false;

        return true;
    }

    bool VerifyPatchFile(const char* path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() !=
                      static_cast<std::streamoff>(DX11LegacyPatchSnapshot::kFileBytes))
            return false;
        std::vector<std::uint8_t> bytes(DX11LegacyPatchSnapshot::kFileBytes);
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size())))
            return false;
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> patch{};
        if (!DX11LegacyPatchSnapshot::Decode(bytes.data(), bytes.size(), patch))
            return false;

        float minZ = (std::numeric_limits<float>::max)();
        float maxZ = (std::numeric_limits<float>::lowest)();
        for (const auto& vertex : patch)
        {
            if (vertex.position[2] < minZ)
                minZ = vertex.position[2];
            if (vertex.position[2] > maxZ)
                maxZ = vertex.position[2];
        }
        std::cout << "Real terrain patch: vertices=" << patch.size()
                  << ", minZ=" << minZ << ", maxZ=" << maxZ
                  << ", height range=" << (maxZ - minZ)
                  << ", x/y/z original DX9 axes\n";
        return Check("Real terrain patch file validated", true);
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

        if (!RunPatchSelfTest())
            return false;
        std::cout << "[OK] Index + patch vertex snapshot codec self-tests complete; no game files written.\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc == 1)
        return RunSelfTest() ? 0 : 1;

    if (argc == 3 && std::string(argv[1]) == "--verify-patch")
    {
        if (!VerifyPatchFile(argv[2]))
        {
            std::cerr << "[FAIL] Invalid patch snapshot or file inaccessible.\n";
            return 5;
        }
        return 0;
    }

    if (argc != 3 || std::string(argv[1]) != "--verify")
    {
        std::cerr << "Usage: DX11LODSnapshotProbe.exe [--verify capture.bin | --verify-patch patch.bin]\n";
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
