// Selftest and optional READ-ONLY loose height.raw inspection.
// Does not start D3D9/D3D11 or require any game resources.
// Invoke without arguments to exercise synthetic edge/bounds fixtures.
#include "../../Src/EterLib/Renderer/DX11RawTerrainHeight.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using m2::render::DX11RawTerrainHeight;
    using m2::render::RawTerrainHeightTile;
    using m2::render::LegacyPatchVertex;

    constexpr std::size_t kPatchVertices =
        m2::render::DX11LegacyTerrainPatch::kVertexCount;
    using Pair = std::array<std::array<LegacyPatchVertex,
                                       kPatchVertices>, 2>;

    bool SyntheticTest()
    {
        std::vector<std::uint8_t> bytes(RawTerrainHeightTile::kRawBytes);
        for (unsigned y = 0u; y < RawTerrainHeightTile::kRawSide; ++y)
            for (unsigned x = 0u; x < RawTerrainHeightTile::kRawSide; ++x)
            {
                const auto value = static_cast<std::uint16_t>(
                    1000u + 2u * x + 3u * y);
                const auto offset = (static_cast<std::size_t>(y) *
                                     RawTerrainHeightTile::kRawSide + x) * 2u;
                bytes[offset] = static_cast<std::uint8_t>(value);
                bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
            }

        RawTerrainHeightTile tile{};
        tile.samples[0] = 7777u;
        if (DX11RawTerrainHeight::Decode(bytes.data(), bytes.size()-1u, tile)
            || tile.samples[0] != 7777u)
        {
            std::cout << "[FAIL] size rejection mutated output\n";
            return false;
        }
        if (!DX11RawTerrainHeight::Decode(bytes.data(), bytes.size(), tile) ||
            tile.samples[0] != 1000u ||
            tile.samples[RawTerrainHeightTile::kRawSide + 1u] != 1005u)
        {
            std::cout << "[FAIL] 131x131 little-endian decode\n";
            return false;
        }
        std::cout << "[OK] Exact 34322-byte original height.raw decode\n";

        Pair pair{};
        if (!DX11RawTerrainHeight::BuildRightNeighborPair(
                tile, 0u, 0u, 0u, 0u, 1.0f, pair))
        {
            std::cout << "[FAIL] original source patch layout\n";
            return false;
        }
        if (pair[0][0].position[0] != 0.0f ||
            pair[0][0].position[1] != 0.0f ||
            pair[0][0].position[2] != 1005.0f ||
            pair[1][0].position[0] != 3200.0f ||
            pair[1][0].position[1] != 0.0f ||
            pair[1][0].position[2] != 1037.0f ||
            pair[0][17u].position[1] != -200.0f)
        {
            std::cout << "[FAIL] world coordinate / 1-cell border / height\n";
            return false;
        }
        std::cout << "[OK] Original X, -Y, heightscale and 17x17 layout\n";

        for (std::size_t row = 0; row <= 16u; ++row)
        {
            const auto& a = pair[0][row * 17u + 16u];
            const auto& b = pair[1][row * 17u];
            for (unsigned c = 0; c < 3u; ++c)
                if (a.position[c] != b.position[c] ||
                    a.normal[c] != b.normal[c])
                {
                    std::cout << "[FAIL] original shared edge differs\n";
                    return false;
                }
        }
        std::cout << "[OK] Exact 17-point border positions and normals\n";

        // Invalid coordinates and heightscale must preserve prior output.
        const float before = pair[0][0].position[2];
        if (DX11RawTerrainHeight::BuildRightNeighborPair(
                tile, 0u, 0u, 7u, 0u, 1.0f, pair) ||
            DX11RawTerrainHeight::BuildRightNeighborPair(
                tile, 0u, 0u, 0u, 8u, 1.0f, pair) ||
            DX11RawTerrainHeight::BuildRightNeighborPair(
                tile, 0u, 0u, 0u, 0u,
                std::numeric_limits<float>::quiet_NaN(), pair) ||
            DX11RawTerrainHeight::BuildRightNeighborPair(
                tile, 0u, 0u, 0u, 0u, 0.0f, pair) ||
            pair[0][0].position[2] != before)
        {
            std::cout << "[FAIL] invalid raw tile input not rejected safely\n";
            return false;
        }
        std::cout << "[OK] Invalid scale, adjacent patch and bounds rejected\n";
        return true;
    }

    bool ReadLooseFile(const std::filesystem::path& path,
                       RawTerrainHeightTile& tile)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() !=
            static_cast<std::streamoff>(RawTerrainHeightTile::kRawBytes))
            return false;
        std::array<std::uint8_t, RawTerrainHeightTile::kRawBytes> bytes{};
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size())))
            return false;
        return DX11RawTerrainHeight::Decode(bytes.data(), bytes.size(), tile);
    }

    bool ParseScale(const wchar_t* input, float& output)
    {
        if (!input || !*input)
            return false;
        wchar_t* end = nullptr;
        const float parsed = std::wcstof(input, &end);
        if (!end || end == input || *end != L'\0' ||
            !std::isfinite(parsed) || parsed <= 0.0f)
            return false;
        output = parsed;
        return true;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (!SyntheticTest())
        return 1;
    if (argc == 1)
    {
        std::cout << "[OK] Original map height CPU-only fixtures complete;"
                     " no files written and no D3D devices created.\n";
        return 0;
    }
    if (argc != 4 || std::wstring(argv[1]) != L"--inspect")
    {
        std::cout << "Usage: DX11RawTerrainProbe.exe "
                     "--inspect <loose height.raw> <heightscale>\n";
        return 2;
    }

    float scale = 0.0f;
    if (!ParseScale(argv[3], scale))
    {
        std::cout << "[FAIL] heightscale must be finite and greater than 0\n";
        return 3;
    }
    RawTerrainHeightTile tile{};
    if (!ReadLooseFile(std::filesystem::path(argv[2]), tile))
    {
        std::cout << "[FAIL] loose height.raw unavailable or invalid size.\n"
                     "Original game may store it in EterPack; this CPU"
                     " probe cannot decode packed resources.\n";
        return 4;
    }
    Pair pair{};
    if (!DX11RawTerrainHeight::BuildRightNeighborPair(
            tile, 0u, 0u, 0u, 0u, scale, pair))
    {
        std::cout << "[FAIL] patch pair construction\n";
        return 5;
    }

    float minZ = pair[0][0].position[2];
    float maxZ = minZ;
    for (const auto& patch : pair)
        for (const auto& vertex : patch)
        {
            if (vertex.position[2] < minZ) minZ = vertex.position[2];
            if (vertex.position[2] > maxZ) maxZ = vertex.position[2];
        }
    std::cout << "[OK] Valid loose height.raw. Patches (0,0) + (1,0)"
                 "; world Z min=" << minZ << " max=" << maxZ
              << "; shared border 17/17 verified\n";
    std::cout << "[OK] Read-only. No game files changed.\n";
    return 0;
}
