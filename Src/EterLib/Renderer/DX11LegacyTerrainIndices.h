#pragma once

// CPU-only DX9 terrain index adapter. The original GameLib renderer draws
// LOD 0 as a triangle strip and LOD 1/2 as triangle lists.
// Never modify MapOutdoorIndexBuffer.cpp or active DX9 rendering here.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2::render
{
    enum class LegacyTerrainPrimitive : std::uint8_t
    {
        TriangleStrip = 0,
        TriangleList = 1
    };

    class DX11LegacyTerrainIndices final
    {
    public:
        // Convert legacy 16-bit indices to a DX11-ready triangle list.
        // Maintains strip winding parity; ignores degenerate connector
        // triangles in strips; does not change outIndices on failure.
        // This only converts topology, NOT world orientation/LOD scheduling.
        static bool ToTriangleList(
            const std::uint16_t* originalIndices,
            std::size_t originalCount,
            std::size_t vertexCount,
            LegacyTerrainPrimitive sourcePrimitive,
            std::vector<std::uint16_t>& outIndices);
    };
}
