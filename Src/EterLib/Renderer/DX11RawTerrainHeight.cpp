#include "DX11RawTerrainHeight.h"

#include <cmath>
#include <utility>

namespace m2::render
{
    namespace
    {
        constexpr std::size_t kSide = RawTerrainHeightTile::kRawSide;
        constexpr float kLegacySignedByteScale = 0.0078740f;

        std::uint16_t HeightAt(const RawTerrainHeightTile& tile,
                               unsigned x, unsigned y) noexcept
        {
            // x/y are logical terrain vertices (0..128).
            // Original CTerrainImpl::GetHeightMapValue adds +1 for border.
            return tile.samples[(static_cast<std::size_t>(y) + 1u) *
                                kSide + static_cast<std::size_t>(x) + 1u];
        }

        float SignedOriginalNormal(float value) noexcept
        {
            // The x64 legacy PR_FLOAT_TO_INT uses floor. Original
            // 127-normalized components are stored in signed char.
            const float rounded = std::floor(value);
            const float clamped = (rounded < -127.0f)
                ? -127.0f : ((rounded > 127.0f) ? 127.0f : rounded);
            return clamped * kLegacySignedByteScale;
        }
    }

    bool DX11RawTerrainHeight::Decode(
        const std::uint8_t* bytes, std::size_t length,
        RawTerrainHeightTile& output) noexcept
    {
        if (!bytes || length != RawTerrainHeightTile::kRawBytes)
            return false;

        RawTerrainHeightTile candidate{};
        for (std::size_t i = 0; i < candidate.samples.size(); ++i)
            candidate.samples[i] = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[2u * i]) |
                static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(bytes[2u * i + 1u]) << 8u));
        output = std::move(candidate);
        return true;
    }

    bool DX11RawTerrainHeight::BuildPatch(
        const RawTerrainHeightTile& tile,
        unsigned tileX, unsigned tileY,
        unsigned patchX, unsigned patchY,
        float heightScale,
        std::array<LegacyPatchVertex,
                   DX11LegacyTerrainPatch::kVertexCount>& output) noexcept
    {
        if (patchX >= kPatchesPerTile || patchY >= kPatchesPerTile ||
            !std::isfinite(heightScale) || heightScale <= 0.0f)
            return false;

        // Original source computes finite-difference normals at tile
        // logical vertices and then quantizes to signed char. Values
        // at logical 0 and 128 may read border -1 and 129 respectively.
        std::array<LegacyPatchVertex,
                   DX11LegacyTerrainPatch::kVertexCount> candidate{};

        const double startX = static_cast<double>(tileX) *
                              kTileCells * kCellScale;
        const double startY = static_cast<double>(tileY) *
                              kTileCells * kCellScale;
        const unsigned sourceX = patchX * kPatchCells;
        const unsigned sourceY = patchY * kPatchCells;

        for (unsigned localY = 0; localY <= kPatchCells; ++localY)
        {
            const unsigned y = sourceY + localY;
            for (unsigned localX = 0; localX <= kPatchCells; ++localX)
            {
                const unsigned x = sourceX + localX;
                const auto index = static_cast<std::size_t>(localY) *
                                   (kPatchCells + 1u) + localX;
                LegacyPatchVertex& vertex = candidate[index];

                // The exact CPU source in AreaTerrain.cpp:
                //   x = +(tileX * 128 + sourceX) * 200
                //   y = -(tileY * 128 + sourceY) * 200
                //   z = unsigned height.raw * heightScale.
                vertex.position[0] = static_cast<float>(
                    startX + double(x) * kCellScale);
                vertex.position[1] = static_cast<float>(
                    -(startY + double(y) * kCellScale));
                vertex.position[2] =
                    static_cast<float>(HeightAt(tile, x, y)) * heightScale;

                // CalculateNormal (AreaTerrain.cpp) reads border samples
                // via logical +/-1. Our raw index offset +1 converts
                // logical -1 -> raw 0 and logical 129 -> raw 130.
                const unsigned rawX = x + 1u;
                const unsigned rawY = y + 1u;
                const auto sample = [&tile](unsigned ix, unsigned iy) {
                    return float(tile.samples[
                        static_cast<std::size_t>(iy) * kSide + ix]);
                };
                const float normalX = -heightScale *
                    (sample(rawX - 1u, rawY) -
                     sample(rawX + 1u, rawY));
                const float normalY = -heightScale *
                    (sample(rawX, rawY - 1u) -
                     sample(rawX, rawY + 1u));
                const float normalZ = 2.0f * kCellScale;
                const float lengthSquared =
                    normalX * normalX + normalY * normalY +
                    normalZ * normalZ;
                if (!std::isfinite(lengthSquared) ||
                    lengthSquared <= 0.0f)
                    return false;
                const float factor = 127.0f / std::sqrt(lengthSquared);
                // Original CalculateNormal stores qx,qy,qz in signed
                // bytes. _CalculateTerrainPatch then negates x only.
                vertex.normal[0] = -SignedOriginalNormal(normalX * factor);
                vertex.normal[1] = SignedOriginalNormal(normalY * factor);
                vertex.normal[2] = SignedOriginalNormal(normalZ * factor);

                if (!std::isfinite(vertex.position[0]) ||
                    !std::isfinite(vertex.position[1]) ||
                    !std::isfinite(vertex.position[2]) ||
                    !std::isfinite(vertex.normal[0]) ||
                    !std::isfinite(vertex.normal[1]) ||
                    !std::isfinite(vertex.normal[2]))
                    return false;
            }
        }

        output = std::move(candidate);
        return true;
    }

    bool DX11RawTerrainHeight::BuildRightNeighborPair(
        const RawTerrainHeightTile& tile,
        unsigned tileX, unsigned tileY,
        unsigned patchX, unsigned patchY,
        float heightScale,
        std::array<std::array<LegacyPatchVertex,
                             DX11LegacyTerrainPatch::kVertexCount>, 2>&
            output) noexcept
    {
        if (patchX >= kPatchesPerTile - 1u)
            return false;

        std::array<std::array<LegacyPatchVertex,
                             DX11LegacyTerrainPatch::kVertexCount>, 2>
            candidate{};
        if (!BuildPatch(tile, tileX, tileY, patchX, patchY,
                        heightScale, candidate[0]) ||
            !BuildPatch(tile, tileX, tileY, patchX + 1u, patchY,
                        heightScale, candidate[1]))
            return false;

        for (unsigned y = 0; y <= kPatchCells; ++y)
        {
            const auto& rightA =
                candidate[0][y * (kPatchCells + 1u) + kPatchCells];
            const auto& leftB = candidate[1][y * (kPatchCells + 1u)];
            for (unsigned axis = 0; axis < 3u; ++axis)
                if (rightA.position[axis] != leftB.position[axis] ||
                    rightA.normal[axis] != leftB.normal[axis])
                    return false;
        }
        output = std::move(candidate);
        return true;
    }
}
