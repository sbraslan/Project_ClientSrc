#include "DX11TerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace m2::render
{
    bool DX11TerrainMesh::Build(
        const HeightField& source,
        std::vector<TerrainVertex>& vertices,
        std::vector<std::uint16_t>& indices)
    {
        const std::uint32_t cols = source.columns;
        const std::uint32_t rows = source.rows;

        // Guard 16-bit vertex indices and all size calculations before allocating.
        if (cols < 2 || rows < 2 || cols > 65535u / rows ||
            !std::isfinite(source.cellSize) || source.cellSize <= 0.0f ||
            !std::isfinite(source.textureRepeats) || source.textureRepeats <= 0.0f)
            return false;

        const std::size_t vertexCount =
            static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
        if (source.heights.size() != vertexCount)
            return false;

        for (const float height : source.heights)
        {
            if (!std::isfinite(height))
                return false;
        }

        std::vector<TerrainVertex> builtVertices(vertexCount);
        std::vector<std::uint16_t> builtIndices;
        builtIndices.reserve(
            static_cast<std::size_t>(cols - 1) * (rows - 1) * 6);

        const float halfX = 0.5f * static_cast<float>(cols - 1);
        const float halfZ = 0.5f * static_cast<float>(rows - 1);

        const auto heightAt = [&](std::uint32_t x, std::uint32_t z) -> float
        {
            return source.heights[static_cast<std::size_t>(z) * cols + x];
        };

        for (std::uint32_t z = 0; z < rows; ++z)
        {
            for (std::uint32_t x = 0; x < cols; ++x)
            {
                const std::uint32_t lx = x == 0 ? 0 : x - 1;
                const std::uint32_t rx = std::min(x + 1, cols - 1);
                const std::uint32_t dz = z == 0 ? 0 : z - 1;
                const std::uint32_t uz = std::min(z + 1, rows - 1);

                const float slopeX =
                    (heightAt(rx, z) - heightAt(lx, z)) /
                    (static_cast<float>(rx - lx) * source.cellSize);
                const float slopeZ =
                    (heightAt(x, uz) - heightAt(x, dz)) /
                    (static_cast<float>(uz - dz) * source.cellSize);

                const float nx = -slopeX;
                const float ny = 1.0f;
                const float nz = -slopeZ;
                const float length = std::sqrt(nx * nx + ny * ny + nz * nz);

                TerrainVertex& vertex =
                    builtVertices[static_cast<std::size_t>(z) * cols + x];
                vertex.position[0] = (static_cast<float>(x) - halfX) * source.cellSize;
                vertex.position[1] = heightAt(x, z);
                vertex.position[2] = (static_cast<float>(z) - halfZ) * source.cellSize;
                vertex.normal[0] = nx / length;
                vertex.normal[1] = ny / length;
                vertex.normal[2] = nz / length;
                vertex.uv[0] = source.textureRepeats *
                    static_cast<float>(x) / static_cast<float>(cols - 1);
                vertex.uv[1] = source.textureRepeats *
                    static_cast<float>(z) / static_cast<float>(rows - 1);
            }
        }

        for (std::uint32_t z = 0; z + 1 < rows; ++z)
        {
            for (std::uint32_t x = 0; x + 1 < cols; ++x)
            {
                const std::uint16_t tl =
                    static_cast<std::uint16_t>(z * cols + x);
                const std::uint16_t tr =
                    static_cast<std::uint16_t>(z * cols + x + 1);
                const std::uint16_t bl =
                    static_cast<std::uint16_t>((z + 1) * cols + x);
                const std::uint16_t br =
                    static_cast<std::uint16_t>((z + 1) * cols + x + 1);

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
