#include "DX11LegacyTerrainPatch.h"

#include <cmath>
#include <utility>

namespace m2::render
{
    bool DX11LegacyTerrainPatch::Convert(
        const LegacyPatchVertex* source,
        std::size_t sourceVertexCount,
        float textureRepeats,
        std::vector<TerrainVertex>& vertices,
        std::vector<std::uint16_t>& indices)
    {
        if (!source || sourceVertexCount != kVertexCount ||
            !std::isfinite(textureRepeats) || textureRepeats <= 0.0f)
            return false;

        // Reject NaNs, infinities and degenerate normals BEFORE allocating.
        for (std::size_t i = 0; i < kVertexCount; ++i)
        {
            const LegacyPatchVertex& vertex = source[i];
            float normalSquared = 0.0f;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(vertex.position[axis]) ||
                    !std::isfinite(vertex.normal[axis]))
                    return false;
                normalSquared += vertex.normal[axis] * vertex.normal[axis];
            }
            if (!std::isfinite(normalSquared) || normalSquared <= 0.0f)
                return false;
        }

        std::vector<TerrainVertex> builtVertices(kVertexCount);
        std::vector<std::uint16_t> builtIndices;
        builtIndices.reserve(kIndexCount);

        for (std::uint32_t z = 0; z < kPatchSide; ++z)
        {
            for (std::uint32_t x = 0; x < kPatchSide; ++x)
            {
                const std::size_t offset =
                    static_cast<std::size_t>(z) * kPatchSide + x;
                const LegacyPatchVertex& input = source[offset];
                TerrainVertex& output = builtVertices[offset];

                // Preserve the original patch world-space coordinates and
                // normals. A future GameLib adapter must verify handedness
                // against the DX9 camera and actual patch source order.
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    output.position[axis] = input.position[axis];
                    output.normal[axis] = input.normal[axis];
                }
                output.uv[0] = static_cast<float>(x) *
                               textureRepeats / static_cast<float>(kPatchCells);
                output.uv[1] = static_cast<float>(z) *
                               textureRepeats / static_cast<float>(kPatchCells);
            }
        }

        // Preview-only level-0 topology. The legacy terrain renderer uses
        // additional LOD and splat geometry: don't substitute this live.
        for (std::uint32_t z = 0; z < kPatchCells; ++z)
        {
            for (std::uint32_t x = 0; x < kPatchCells; ++x)
            {
                const std::uint16_t tl =
                    static_cast<std::uint16_t>(z * kPatchSide + x);
                const std::uint16_t tr =
                    static_cast<std::uint16_t>(z * kPatchSide + x + 1);
                const std::uint16_t bl =
                    static_cast<std::uint16_t>((z + 1) * kPatchSide + x);
                const std::uint16_t br =
                    static_cast<std::uint16_t>((z + 1) * kPatchSide + x + 1);

                builtIndices.push_back(tl);
                builtIndices.push_back(bl);
                builtIndices.push_back(tr);
                builtIndices.push_back(tr);
                builtIndices.push_back(bl);
                builtIndices.push_back(br);
            }
        }

        vertices.swap(builtVertices);
        indices.swap(builtIndices);
        return true;
    }
}
