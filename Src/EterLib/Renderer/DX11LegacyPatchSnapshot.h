#pragma once

// Portable, opt-in snapshot of a single ORIGINAL 17x17 Metin2 terrain patch.
// No DX9 / D3D11 device dependency. All position and normal floats preserved.
#include "DX11LegacyTerrainPatch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2::render
{
    class DX11LegacyPatchSnapshot final
    {
    public:
        static constexpr std::size_t kVertexCount =
            DX11LegacyTerrainPatch::kVertexCount;
        static constexpr std::size_t kFloatCountPerVertex = 6;
        static constexpr std::size_t kHeaderBytes = 8u + 4u + 4u;
        static constexpr std::size_t kFileBytes =
            kHeaderBytes + kVertexCount * kFloatCountPerVertex * 4u + 4u;

        // Encode fails without changing output for malformed vertex data.
        static bool Encode(
            const LegacyPatchVertex* vertices,
            std::size_t count,
            std::vector<std::uint8_t>& output);

        // Decode checks fixed length, magic, version, checksum, and all
        // finite positions / nondegenerate normals before mutating output.
        static bool Decode(
            const std::uint8_t* bytes,
            std::size_t count,
            std::array<LegacyPatchVertex, kVertexCount>& output);
    };
}
