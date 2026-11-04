#include "DX11LegacyTerrainIndices.h"

#include <limits>
#include <utility>

namespace m2::render
{
    bool DX11LegacyTerrainIndices::ToTriangleList(
        const std::uint16_t* originalIndices,
        std::size_t originalCount,
        std::size_t vertexCount,
        LegacyTerrainPrimitive sourcePrimitive,
        std::vector<std::uint16_t>& outIndices)
    {
        // Protect both 16-bit index addressability and pathological input.
        if (!originalIndices || originalCount < 3 ||
            originalCount > 65536u || vertexCount == 0 ||
            vertexCount > 65536u)
            return false;

        if (sourcePrimitive != LegacyTerrainPrimitive::TriangleStrip &&
            sourcePrimitive != LegacyTerrainPrimitive::TriangleList)
            return false;

        if (sourcePrimitive == LegacyTerrainPrimitive::TriangleList &&
            originalCount % 3u != 0)
            return false;

        // Validate *all* source indices, including strip connector vertices.
        for (std::size_t i = 0; i < originalCount; ++i)
        {
            if (originalIndices[i] >= vertexCount)
                return false;
        }

        std::vector<std::uint16_t> output;
        const std::size_t maxTriangles =
            sourcePrimitive == LegacyTerrainPrimitive::TriangleStrip
                ? originalCount - 2u : originalCount / 3u;
        output.reserve(maxTriangles * 3u);

        const auto emit = [&](std::uint16_t a,
                              std::uint16_t b,
                              std::uint16_t c)
        {
            // DX9 strips insert repeated vertices to stitch rows. Such
            // triangles have zero area and must not be emitted.
            if (a == b || b == c || a == c)
                return;
            output.push_back(a);
            output.push_back(b);
            output.push_back(c);
        };

        if (sourcePrimitive == LegacyTerrainPrimitive::TriangleStrip)
        {
            for (std::size_t i = 0; i + 2 < originalCount; ++i)
            {
                const std::uint16_t a = originalIndices[i];
                const std::uint16_t b = originalIndices[i + 1u];
                const std::uint16_t c = originalIndices[i + 2u];

                // Triangle strips alternate winding each new vertex.
                // Crucial: still toggle parity for DEGENERATE triangles!
                if ((i & 1u) == 0)
                    emit(a, b, c);
                else
                    emit(b, a, c);
            }
        }
        else
        {
            for (std::size_t i = 0; i < originalCount; i += 3u)
                emit(originalIndices[i], originalIndices[i + 1u],
                     originalIndices[i + 2u]);
        }

        if (output.empty())
            return false;

        outIndices.swap(output);
        return true;
    }
}
