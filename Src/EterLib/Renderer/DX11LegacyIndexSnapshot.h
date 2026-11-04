#pragma once

// Portable, read-only-compatible snapshot codec for the three ORIGINAL
// Metin2 terrain LOD index streams. No D3D9, D3D11 or GameLib dependency.
// No capture is enabled in the game by this file.
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2::render
{
    struct LegacyIndexView
    {
        const std::uint16_t* indices = nullptr;
        std::size_t count = 0;
    };

    struct LegacyIndexSnapshot
    {
        std::array<std::vector<std::uint16_t>, 3> lod;
    };

    class DX11LegacyIndexSnapshot final
    {
    public:
        static constexpr std::size_t kLODCount = 3;
        static constexpr std::size_t kVertexCount = 17 * 17;
        static constexpr std::size_t kMaxPerLOD = 16 * 16 * 4;

        // Explicit little-endian, versioned format with FNV-1a checksum.
        // Output is unchanged if validation or decoding fails.
        static bool Encode(
            const std::array<LegacyIndexView, kLODCount>& views,
            std::vector<std::uint8_t>& bytes);

        static bool Decode(
            const std::uint8_t* bytes,
            std::size_t byteCount,
            LegacyIndexSnapshot& snapshot);

        // These functions are for an explicitly opt-in, future diagnostic
        // path. No game code invokes them by default.
        static bool SaveFile(
            const char* path,
            const std::array<LegacyIndexView, kLODCount>& views);

        static bool LoadFile(
            const char* path,
            LegacyIndexSnapshot& snapshot);
    };
}
