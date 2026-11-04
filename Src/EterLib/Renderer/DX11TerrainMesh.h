#pragma once

// CPU-only height-field mesh. Deliberately independent of Metin2's D3D9
// terrain classes; a real CTerrainPatch adapter will be a separate change.
#include <cstdint>
#include <vector>

namespace m2::render
{
    struct TerrainVertex
    {
        float position[3];
        float normal[3];
        float uv[2];
    };

    struct HeightField
    {
        std::uint32_t columns = 0;
        std::uint32_t rows = 0;
        float cellSize = 1.0f;
        float textureRepeats = 1.0f;
        std::vector<float> heights; // row-major, z * columns + x
    };

    class DX11TerrainMesh final
    {
    public:
        // Index buffer uses uint16_t, like the existing terrain code.
        // Returns false without altering outputs for invalid inputs.
        // Allocation failures may throw; the calling layer handles these.
        static bool Build(
            const HeightField& source,
            std::vector<TerrainVertex>& vertices,
            std::vector<std::uint16_t>& indices);
    };
}
