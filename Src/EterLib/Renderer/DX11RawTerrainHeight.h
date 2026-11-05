#pragma once

// Pure CPU implementation of the original Metin2 height.raw tile format.
// Sources: PRTerrainLib/Terrain.h, Terrain.cpp and
// GameLib/AreaTerrain.cpp::_CalculateTerrainPatch.
// No D3D9/D3D11/Win32/pack-manager dependency.
// Pack resources must be exported/read separately. Never modify raw inputs.

#include "DX11LegacyTerrainPatch.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace m2::render
{
    struct RawTerrainHeightTile
    {
        static constexpr std::size_t kRawSide = 131u;
        static constexpr std::size_t kRawCount = kRawSide * kRawSide;
        static constexpr std::size_t kRawBytes = kRawCount * 2u;

        // Includes one-cell border around the logical 129x129 vertices.
        // Stored row-major, little-endian unsigned 16-bit values.
        std::array<std::uint16_t, kRawCount> samples{};
    };

    class DX11RawTerrainHeight final
    {
    public:
        static constexpr unsigned kPatchCells = 16u;
        static constexpr unsigned kPatchesPerTile = 8u;
        static constexpr unsigned kCellScale = 200u;
        static constexpr unsigned kTileCells = 128u;

        // Exactly 34322 bytes. No native checksum in original height.raw:
        // validates dimensions only; malformed contents cannot be detected
        // reliably without an independent reference.
        // On failure, output is NOT changed.
        static bool Decode(const std::uint8_t* bytes, std::size_t length,
                           RawTerrainHeightTile& output) noexcept;

        // Orig. source X right / Y down, patch row-major 17x17; original
        // position uses negative world Y. Height = sample * heightScale.
        // Normal uses original finite-difference + signed-char rounding
        // conversion. Does not upload a D3D9 vertex buffer or allocate.
        // On failure, output is NOT changed.
        static bool BuildPatch(
            const RawTerrainHeightTile& tile,
            unsigned tileX, unsigned tileY,
            unsigned patchX, unsigned patchY,
            float heightScale,
            std::array<LegacyPatchVertex,
                       DX11LegacyTerrainPatch::kVertexCount>& output) noexcept;

        // Horizontal pair within one tile: patchX must be 0..6.
        // Validates all 17 shared border source positions AND normals.
        // On failure, output is NOT changed.
        static bool BuildRightNeighborPair(
            const RawTerrainHeightTile& tile,
            unsigned tileX, unsigned tileY,
            unsigned patchX, unsigned patchY,
            float heightScale,
            std::array<std::array<LegacyPatchVertex,
                                  DX11LegacyTerrainPatch::kVertexCount>, 2>&
                output) noexcept;
    };
}
