#pragma once

// A DX9-header-free *CPU* bridge to the legacy Metin2 terrain patch layout.
// Src/GameLib/TerrainPatch.h:
//   struct HardwareTransformPatch_SSourceVertex
//   { D3DXVECTOR3 kPosition; D3DXVECTOR3 kNormal; };
// Src/PRTerrainLib/TerrainType.h: TERRAIN_PATCHSIZE == 16.
//
// WARNING: This converts only position+normal vertex data. Real game patch
// index LOD, coordinate transform, splat layers and culling are NOT ported.
#include "DX11TerrainMesh.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2::render
{
    struct LegacyPatchVertex
    {
        float position[3];
        float normal[3];
    };
    static_assert(sizeof(LegacyPatchVertex) == 6 * sizeof(float),
                  "Legacy terrain position+normal must remain tightly packed");

    class DX11LegacyTerrainPatch final
    {
    public:
        static constexpr std::uint32_t kPatchCells = 16u;
        static constexpr std::uint32_t kPatchSide = kPatchCells + 1u;
        static constexpr std::size_t kVertexCount =
            static_cast<std::size_t>(kPatchSide) * kPatchSide;
        static constexpr std::size_t kIndexCount =
            static_cast<std::size_t>(kPatchCells) * kPatchCells * 6u;

        // The caller must supply the original patch vertices in row-major
        // grid order (17x17). This is a candidate GPU upload layout, not yet
        // asserted to match all legacy LOD/index-buffer paths in GameLib.
        // Outputs are not modified on failure.
        static bool Convert(
            const LegacyPatchVertex* source,
            std::size_t sourceVertexCount,
            float textureRepeats,
            std::vector<TerrainVertex>& vertices,
            std::vector<std::uint16_t>& indices);
    };
}
