#include "DX11LegacyPatchSnapshot.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace m2::render
{
    namespace
    {
        constexpr std::uint8_t kMagic[8] =
            {'M', '2', 'P', 'V', 'E', 'R', 'T', '1'};
        constexpr std::uint32_t kVersion = 1u;

        static_assert(sizeof(float) == 4u, "Snapshot requires float32");
        static_assert(std::numeric_limits<float>::is_iec559,
                      "Snapshot requires IEEE754 float32");
        static_assert(sizeof(LegacyPatchVertex) == 24u,
                      "Snapshot expects tightly packed six float32 values");

        void AppendU32(std::vector<std::uint8_t>& buffer, std::uint32_t value)
        {
            for (unsigned shift = 0; shift < 32; shift += 8)
                buffer.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        std::uint32_t ReadU32(const std::uint8_t* data, std::size_t offset)
        {
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i)
                value |= static_cast<std::uint32_t>(data[offset + i]) <<
                         (i * 8u);
            return value;
        }

        std::uint32_t Checksum(const std::uint8_t* bytes, std::size_t length)
        {
            // FNV-1a detects accidental corruption; not a security MAC.
            std::uint32_t hash = 2166136261u;
            for (std::size_t i = 0; i < length; ++i)
            {
                hash ^= bytes[i];
                hash *= 16777619u;
            }
            return hash;
        }

        bool ValidVertex(const LegacyPatchVertex& vertex)
        {
            float squaredLength = 0.0f;
            for (unsigned i = 0; i < 3; ++i)
            {
                if (!std::isfinite(vertex.position[i]) ||
                    !std::isfinite(vertex.normal[i]))
                    return false;
                squaredLength += vertex.normal[i] * vertex.normal[i];
            }
            return std::isfinite(squaredLength) &&
                   squaredLength > 1.0e-8f;
        }
    }

    bool DX11LegacyPatchSnapshot::Encode(
        const LegacyPatchVertex* vertices,
        std::size_t count,
        std::vector<std::uint8_t>& output)
    {
        if (!vertices || count != kVertexCount)
            return false;
        for (std::size_t i = 0; i < count; ++i)
            if (!ValidVertex(vertices[i]))
                return false;

        std::vector<std::uint8_t> buffer;
        buffer.reserve(kFileBytes);
        buffer.insert(buffer.end(), kMagic, kMagic + sizeof(kMagic));
        AppendU32(buffer, kVersion);
        AppendU32(buffer, static_cast<std::uint32_t>(count));
        for (std::size_t i = 0; i < count; ++i)
        {
            for (unsigned axis = 0; axis < 6; ++axis)
            {
                const float value = axis < 3
                    ? vertices[i].position[axis]
                    : vertices[i].normal[axis - 3];
                std::uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                AppendU32(buffer, bits);
            }
        }
        AppendU32(buffer, Checksum(buffer.data(), buffer.size()));
        if (buffer.size() != kFileBytes)
            return false;
        output.swap(buffer);
        return true;
    }

    bool DX11LegacyPatchSnapshot::Decode(
        const std::uint8_t* bytes,
        std::size_t count,
        std::array<LegacyPatchVertex, kVertexCount>& output)
    {
        if (!bytes || count != kFileBytes ||
            std::memcmp(bytes, kMagic, sizeof(kMagic)) != 0 ||
            ReadU32(bytes, 8u) != kVersion ||
            ReadU32(bytes, 12u) != kVertexCount ||
            ReadU32(bytes, count - 4u) != Checksum(bytes, count - 4u))
            return false;

        std::array<LegacyPatchVertex, kVertexCount> parsed{};
        std::size_t cursor = kHeaderBytes;
        for (auto& vertex : parsed)
        {
            for (unsigned axis = 0; axis < 6; ++axis)
            {
                const std::uint32_t bits = ReadU32(bytes, cursor);
                cursor += 4u;
                float value = 0.0f;
                std::memcpy(&value, &bits, sizeof(value));
                if (axis < 3)
                    vertex.position[axis] = value;
                else
                    vertex.normal[axis - 3] = value;
            }
            if (!ValidVertex(vertex))
                return false;
        }

        output = std::move(parsed);
        return true;
    }
}
