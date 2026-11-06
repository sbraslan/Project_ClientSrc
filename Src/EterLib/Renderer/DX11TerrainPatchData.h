#pragma once

// D3D-neutral CPU -> terrain-renderer contract.
// Map names, packs, snapshot filenames and DX9 resources NEVER enter this API.
// A future live CTerrain loader can submit any resident patch using a key;
// the DX11 backend owns GPU buffers and their lifetime.
#include "DX11TerrainMesh.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace m2::render
{
    struct TerrainPatchKey
    {
        std::uint32_t tileX = 0;
        std::uint32_t tileY = 0;
        std::uint32_t patchX = 0;
        std::uint32_t patchY = 0;

        bool operator==(const TerrainPatchKey& rhs) const noexcept
        {
            return tileX == rhs.tileX && tileY == rhs.tileY &&
                   patchX == rhs.patchX && patchY == rhs.patchY;
        }
    };

    struct TerrainPatchKeyHash
    {
        std::size_t operator()(const TerrainPatchKey& key) const noexcept
        {
            std::size_t result = 0x9e3779b9u;
            const auto combine = [&result](std::uint32_t part) {
                result ^= static_cast<std::size_t>(part) +
                          static_cast<std::size_t>(0x9e3779b9u) +
                          (result << 6u) + (result >> 2u);
            };
            combine(key.tileX);
            combine(key.tileY);
            combine(key.patchX);
            combine(key.patchY);
            return result;
        }
    };

    // Vertex positions/normals/UVs must be ready for the target renderer.
    // The producer (legacy snapshot adapter, future live GameLib adapter)
    // owns memory until the synchronous Upload call returns.
    // All LOD indices are *triangle lists*, not legacy strips.
    struct TerrainPatchGeometryView
    {
        const TerrainVertex* vertices = nullptr;
        std::uint32_t vertexCount = 0;
        std::array<const std::uint16_t*, 3> lodIndices{};
        std::array<std::uint32_t, 3> lodIndexCounts{};
    };

    inline bool ValidateTerrainPatchGeometry(
        const TerrainPatchGeometryView& geometry) noexcept
    {
        constexpr std::uint32_t kMaxVertices = 65535u;
        constexpr std::uint32_t kMaxIndicesPerLOD = 393210u;
        if (!geometry.vertices || geometry.vertexCount < 3u ||
            geometry.vertexCount > kMaxVertices)
            return false;

        for (std::uint32_t vertexIndex = 0;
             vertexIndex < geometry.vertexCount; ++vertexIndex)
        {
            const auto& v = geometry.vertices[vertexIndex];
            for (float component : v.position)
                if (!std::isfinite(component))
                    return false;
            for (float component : v.normal)
                if (!std::isfinite(component))
                    return false;
            for (float component : v.uv)
                if (!std::isfinite(component))
                    return false;
        }

        for (std::size_t level = 0; level < geometry.lodIndices.size(); ++level)
        {
            const auto* indices = geometry.lodIndices[level];
            const auto count = geometry.lodIndexCounts[level];
            if (!indices || count < 3u ||
                count > kMaxIndicesPerLOD || count % 3u != 0u)
                return false;
            for (std::uint32_t index = 0; index < count; ++index)
                if (indices[index] >= geometry.vertexCount)
                    return false;
        }
        return true;
    }
}
