#include "DX11LegacyIndexSnapshot.h"
#include "DX11LegacyTerrainIndices.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace m2::render
{
    namespace
    {
        constexpr std::uint8_t kMagic[] =
            {'M', '2', 'L', 'O', 'D', '1', '1', 0};
        constexpr std::uint32_t kVersion = 1;
        constexpr std::size_t kHeaderSize = 8 + 4 + 3 * 4;
        constexpr std::size_t kChecksumSize = 4;
        constexpr std::size_t kMaxBytes =
            kHeaderSize + 3 * DX11LegacyIndexSnapshot::kMaxPerLOD * 2 +
            kChecksumSize;

        void WriteU16(std::vector<std::uint8_t>& b, std::uint16_t n)
        {
            b.push_back(static_cast<std::uint8_t>(n & 0xffu));
            b.push_back(static_cast<std::uint8_t>((n >> 8u) & 0xffu));
        }

        void WriteU32(std::vector<std::uint8_t>& b, std::uint32_t n)
        {
            for (unsigned i = 0; i < 4; ++i)
                b.push_back(static_cast<std::uint8_t>((n >> (8u * i)) & 0xffu));
        }

        std::uint16_t ReadU16(const std::uint8_t* b, std::size_t offset)
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(b[offset]) |
                (static_cast<std::uint16_t>(b[offset + 1u]) << 8u));
        }

        std::uint32_t ReadU32(const std::uint8_t* b, std::size_t offset)
        {
            std::uint32_t n = 0;
            for (unsigned i = 0; i < 4; ++i)
                n |= static_cast<std::uint32_t>(b[offset + i]) << (8u * i);
            return n;
        }

        std::uint32_t Checksum(const std::uint8_t* b, std::size_t size)
        {
            // FNV-1a protects against accidental corruption; not cryptographic.
            std::uint32_t hash = 2166136261u;
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= b[i];
                hash *= 16777619u;
            }
            return hash;
        }

        bool ValidLOD(std::size_t lod, const std::uint16_t* indices,
                      std::size_t count)
        {
            if (!indices || count < 3 ||
                count > DX11LegacyIndexSnapshot::kMaxPerLOD)
                return false;

            const auto primitive = lod == 0
                ? LegacyTerrainPrimitive::TriangleStrip
                : LegacyTerrainPrimitive::TriangleList;

            std::vector<std::uint16_t> converted;
            return DX11LegacyTerrainIndices::ToTriangleList(
                indices, count, DX11LegacyIndexSnapshot::kVertexCount,
                primitive, converted);
        }
    }

    bool DX11LegacyIndexSnapshot::Encode(
        const std::array<LegacyIndexView, kLODCount>& views,
        std::vector<std::uint8_t>& bytes)
    {
        std::size_t sum = 0;
        for (std::size_t lod = 0; lod < views.size(); ++lod)
        {
            if (!ValidLOD(lod, views[lod].indices, views[lod].count))
                return false;
            sum += views[lod].count;
        }

        std::vector<std::uint8_t> packed;
        packed.reserve(kHeaderSize + sum * 2 + kChecksumSize);
        packed.insert(packed.end(), std::begin(kMagic), std::end(kMagic));
        WriteU32(packed, kVersion);
        for (const auto& view : views)
            WriteU32(packed, static_cast<std::uint32_t>(view.count));

        for (const auto& view : views)
            for (std::size_t i = 0; i < view.count; ++i)
                WriteU16(packed, view.indices[i]);

        WriteU32(packed, Checksum(packed.data(), packed.size()));
        bytes.swap(packed);
        return true;
    }

    bool DX11LegacyIndexSnapshot::Decode(
        const std::uint8_t* bytes, std::size_t byteCount,
        LegacyIndexSnapshot& snapshot)
    {
        if (!bytes || byteCount < kHeaderSize + kChecksumSize ||
            byteCount > kMaxBytes ||
            std::memcmp(bytes, kMagic, sizeof(kMagic)) != 0 ||
            ReadU32(bytes, sizeof(kMagic)) != kVersion)
            return false;

        std::array<std::size_t, kLODCount> counts{};
        std::size_t total = 0;
        for (std::size_t lod = 0; lod < counts.size(); ++lod)
        {
            counts[lod] = ReadU32(bytes, sizeof(kMagic) + 4u + 4u * lod);
            if (counts[lod] < 3 || counts[lod] > kMaxPerLOD)
                return false;
            total += counts[lod];
        }

        if (byteCount != kHeaderSize + total * 2 + kChecksumSize)
            return false;

        if (ReadU32(bytes, byteCount - kChecksumSize) !=
            Checksum(bytes, byteCount - kChecksumSize))
            return false;

        LegacyIndexSnapshot parsed;
        std::size_t cursor = kHeaderSize;
        for (std::size_t lod = 0; lod < counts.size(); ++lod)
        {
            parsed.lod[lod].resize(counts[lod]);
            for (std::size_t i = 0; i < counts[lod]; ++i)
            {
                parsed.lod[lod][i] = ReadU16(bytes, cursor);
                cursor += 2;
            }

            if (!ValidLOD(lod, parsed.lod[lod].data(),
                          parsed.lod[lod].size()))
                return false;
        }

        snapshot = std::move(parsed);
        return true;
    }

    bool DX11LegacyIndexSnapshot::SaveFile(
        const char* path,
        const std::array<LegacyIndexView, kLODCount>& views)
    {
        if (!path || !*path)
            return false;

        std::vector<std::uint8_t> bytes;
        if (!Encode(views, bytes))
            return false;

        // Write only when the caller EXPLICITLY invokes this diagnostic API.
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.flush();
        return file.good();
    }

    bool DX11LegacyIndexSnapshot::LoadFile(
        const char* path, LegacyIndexSnapshot& snapshot)
    {
        if (!path || !*path)
            return false;

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streampos end = file.tellg();
        if (end < std::streampos(0) ||
            end > static_cast<std::streamoff>(kMaxBytes))
            return false;

        const auto length = static_cast<std::size_t>(
            static_cast<std::streamoff>(end));
        std::vector<std::uint8_t> bytes(length);
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size())))
            return false;

        return Decode(bytes.data(), bytes.size(), snapshot);
    }
}
