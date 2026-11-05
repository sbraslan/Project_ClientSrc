// Standalone, CPU-only diagnostic. Does not load Metin2 or create a D3D device.
// Usage: DX11LODSnapshotProbe.exe [--verify dx11_terrain_lod_capture.bin]
#include "../../Src/EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include "../../Src/EterLib/Renderer/DX11LegacyTerrainIndices.h"
#include "../../Src/EterLib/Renderer/DX11LegacyPatchSnapshot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <utility>
#include <string>
#include <vector>

namespace
{
    using m2::render::DX11LegacyIndexSnapshot;
    using m2::render::DX11LegacyPatchSnapshot;
    using m2::render::LegacyPatchVertex;
    using m2::render::LegacyIndexSnapshot;
    using m2::render::LegacyIndexView;
    using m2::render::DX11LegacyTerrainIndices;
    using m2::render::LegacyTerrainPrimitive;

    bool Check(const char* label, bool success)
    {
        std::cout << (success ? "[OK] " : "[FAIL] ") << label << '\n';
        return success;
    }

    bool VerifySnapshot(const LegacyIndexSnapshot& snapshot)
    {
        for (std::size_t lod = 0; lod < snapshot.lod.size(); ++lod)
        {
            std::vector<std::uint16_t> triangles;
            const auto primitive = lod == 0
                ? LegacyTerrainPrimitive::TriangleStrip
                : LegacyTerrainPrimitive::TriangleList;
            const auto& original = snapshot.lod[lod];

            if (!DX11LegacyTerrainIndices::ToTriangleList(
                    original.data(), original.size(),
                    DX11LegacyIndexSnapshot::kVertexCount,
                    primitive, triangles))
                return false;

            std::cout << "LOD " << lod
                      << ": source indices=" << original.size()
                      << ", converted triangles=" << triangles.size() / 3u
                      << ", topology=" << (lod == 0 ? "STRIP" : "LIST")
                      << '\n';
        }
        return true;
    }

    bool RunPatchSelfTest()
    {
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> source{};
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            source[i].position[0] = static_cast<float>(i % 17u);
            source[i].position[1] = -static_cast<float>(i / 17u);
            source[i].position[2] = static_cast<float>(i % 7u) * 0.25f;
            source[i].normal[0] = 0.0f;
            source[i].normal[1] = 0.0f;
            source[i].normal[2] = 1.0f;
        }

        std::vector<std::uint8_t> bytes;
        if (!Check("Patch vertex encode fixture",
                   DX11LegacyPatchSnapshot::Encode(
                       source.data(), source.size(), bytes) &&
                   bytes.size() == DX11LegacyPatchSnapshot::kFileBytes))
            return false;

        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> decoded{};
        if (!Check("Patch vertex decode fixture",
                   DX11LegacyPatchSnapshot::Decode(
                       bytes.data(), bytes.size(), decoded)))
            return false;
        if (!Check("Patch vertex roundtrip",
                   decoded[0].position[0] == source[0].position[0] &&
                   decoded[128].position[2] == source[128].position[2] &&
                   decoded.back().normal[2] == 1.0f))
            return false;

        const auto prior = decoded;
        bytes[bytes.size() / 2u] ^= 0x40u;
        if (!Check("Patch checksum corruption rejected",
                   !DX11LegacyPatchSnapshot::Decode(
                       bytes.data(), bytes.size(), decoded) &&
                   decoded[0].position[0] == prior[0].position[0] &&
                   decoded[128].position[2] == prior[128].position[2]))
            return false;

        std::vector<std::uint8_t> unchanged{13u, 7u};
        source[0].normal[2] = 0.0f;
        if (!Check("Patch invalid normal rejected without mutation",
                   !DX11LegacyPatchSnapshot::Encode(
                       source.data(), source.size(), unchanged) &&
                   unchanged == std::vector<std::uint8_t>{13u, 7u}))
            return false;

        return true;
    }

    bool LoadPatchFile(
        const char* path,
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount>& patch)
    {
        // Read a fixed-size, checksummed file into temporary storage.
        // The user's original capture is never modified.
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() !=
                      static_cast<std::streamoff>(DX11LegacyPatchSnapshot::kFileBytes))
            return false;
        std::vector<std::uint8_t> bytes(DX11LegacyPatchSnapshot::kFileBytes);
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size())))
            return false;
        return DX11LegacyPatchSnapshot::Decode(bytes.data(), bytes.size(), patch);
    }

    bool VerifyPatchFile(const char* path)
    {
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> patch{};
        if (!LoadPatchFile(path, patch))
            return false;

        float minZ = (std::numeric_limits<float>::max)();
        float maxZ = (std::numeric_limits<float>::lowest)();
        for (const auto& vertex : patch)
        {
            minZ = (std::min)(minZ, vertex.position[2]);
            maxZ = (std::max)(maxZ, vertex.position[2]);
        }
        std::cout << "Real terrain patch: vertices=" << patch.size()
                  << ", minZ=" << minZ << ", maxZ=" << maxZ
                  << ", height range=" << (maxZ - minZ)
                  << ", x/y/z original DX9 axes\n";
        return Check("Real terrain patch file validated", true);
    }

    // Source vertex order is 17 columns across increasing DX9 X, then
    // 17 rows of decreasing DX9 Y. Compare actual shared border vertices,
    // rather than assuming consecutive capture calls are neighbors.
    bool CompareAdjacentPatches(
        const std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount>& a,
        const std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount>& b,
        bool report)
    {
        constexpr std::size_t side = 17;
        constexpr float expectedPatchSize = 3200.0f;
        constexpr double coordinateTolerance = 0.05;
        constexpr double normalWarningTolerance = 0.15;
        const double dx = double(b[0].position[0]) - a[0].position[0];
        const double dy = double(b[0].position[1]) - a[0].position[1];

        const auto near = [](double lhs, double rhs) -> bool
        {
            return std::abs(lhs - rhs) <= 0.05;
        };
        // Direction: 0 right, 1 left, 2 down (negative Y), 3 up.
        int direction = -1;
        if (near(dx, expectedPatchSize) && near(dy, 0.0))
            direction = 0;
        else if (near(dx, -expectedPatchSize) && near(dy, 0.0))
            direction = 1;
        else if (near(dx, 0.0) && near(dy, -expectedPatchSize))
            direction = 2;
        else if (near(dx, 0.0) && near(dy, expectedPatchSize))
            direction = 3;

        if (direction < 0)
        {
            if (report)
                std::cout << "[FAIL] Patch origins are not horizontally adjacent"
                          << ": deltaX=" << dx << " deltaY=" << dy << '\n';
            return false;
        }

        const char* directions[] = {"right", "left", "down(-Y)", "up(+Y)"};
        double maxGap = 0.0;
        double maxNormalGap = 0.0;
        unsigned mismatched = 0;
        for (std::size_t i = 0; i < side; ++i)
        {
            std::size_t ia = 0;
            std::size_t ib = 0;
            if (direction == 0)
            {
                ia = i * side + (side - 1);
                ib = i * side;
            }
            else if (direction == 1)
            {
                ia = i * side;
                ib = i * side + (side - 1);
            }
            else if (direction == 2)
            {
                ia = (side - 1) * side + i;
                ib = i;
            }
            else
            {
                ia = i;
                ib = (side - 1) * side + i;
            }
            double gapSquared = 0.0;
            double normalSquared = 0.0;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                const double diff = double(a[ia].position[axis]) -
                                    b[ib].position[axis];
                gapSquared += diff * diff;
                const double ndiff = double(a[ia].normal[axis]) -
                                     b[ib].normal[axis];
                normalSquared += ndiff * ndiff;
            }
            const double gap = std::sqrt(gapSquared);
            maxGap = (std::max)(maxGap, gap);
            maxNormalGap = (std::max)(maxNormalGap, std::sqrt(normalSquared));
            if (!std::isfinite(gap) || gap > coordinateTolerance)
                ++mismatched;
        }

        if (report)
        {
            std::cout << "Adjacent patch relation=" << directions[direction]
                      << ", tested boundary vertices=" << side
                      << ", max position gap=" << maxGap
                      << ", mismatched points=" << mismatched
                      << ", max normal delta=" << maxNormalGap << '\n';
            if (maxNormalGap > normalWarningTolerance)
                std::cout << "[WARN] Boundary normals differ; inspect shading continuity.\n";
            std::cout << (mismatched ? "[FAIL]" : "[OK]")
                      << " Real patch border position agreement\n";
        }
        return mismatched == 0;
    }

    bool RunAdjacentSelfTest()
    {
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> first{};
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> second{};
        for (std::size_t i = 0; i < first.size(); ++i)
        {
            const float col = static_cast<float>(i % 17u);
            const float row = static_cast<float>(i / 17u);
            first[i].position[0] = col * 200.0f;
            first[i].position[1] = -row * 200.0f;
            first[i].position[2] = col + row;
            first[i].normal[2] = 1.0f;
            second[i].position[0] = 3200.0f + col * 200.0f;
            second[i].position[1] = -row * 200.0f;
            second[i].position[2] = 16.0f + col + row;
            second[i].normal[2] = 1.0f;
        }
        if (!Check("Adjacent patch valid right edge",
                   CompareAdjacentPatches(first, second, false)) ||
            !Check("Adjacent patch valid left edge",
                   CompareAdjacentPatches(second, first, false)))
            return false;

        auto lower = first;
        for (std::size_t i = 0; i < lower.size(); ++i)
        {
            lower[i].position[1] -= 3200.0f;
            lower[i].position[2] += 16.0f;
        }
        if (!Check("Adjacent patch valid down(-Y) edge",
                   CompareAdjacentPatches(first, lower, false)) ||
            !Check("Adjacent patch valid up(+Y) edge",
                   CompareAdjacentPatches(lower, first, false)))
            return false;
        second[0].position[2] += 1.0f;
        if (!Check("Adjacent patch seam height mismatch rejected",
                   !CompareAdjacentPatches(first, second, false)))
            return false;
        second[0].position[2] -= 1.0f;
        second[0].position[0] += 100.0f;
        if (!Check("Nonadjacent patch origins rejected",
                   !CompareAdjacentPatches(first, second, false)))
            return false;
        return true;
    }

    bool AuditAdjacent(const char* firstPath, const char* secondPath)
    {
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> first{};
        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> second{};
        if (!LoadPatchFile(firstPath, first) || !LoadPatchFile(secondPath, second))
        {
            std::cerr << "[FAIL] Adjacent patch source(s) invalid or inaccessible.\n";
            return false;
        }
        const bool valid = CompareAdjacentPatches(first, second, true);
        if (valid)
            std::cout << "[OK] Read-only adjacent patch audit complete. No files written.\n";
        return valid;
    }

    bool AuditGeometry(const char* lodPath, const char* patchPath)
    {
        LegacyIndexSnapshot lod;
        if (!DX11LegacyIndexSnapshot::LoadFile(lodPath, lod))
        {
            std::cerr << "[FAIL] LOD source invalid or inaccessible.\n";
            return false;
        }

        std::array<LegacyPatchVertex, DX11LegacyPatchSnapshot::kVertexCount> patch{};
        if (!LoadPatchFile(patchPath, patch))
        {
            std::cerr << "[FAIL] Patch vertex source invalid or inaccessible.\n";
            return false;
        }

        double minXYZ[3]{patch[0].position[0],
                         patch[0].position[1],
                         patch[0].position[2]};
        double maxXYZ[3]{minXYZ[0], minXYZ[1], minXYZ[2]};
        double normalUpTotal = 0.0;
        for (const auto& vertex : patch)
        {
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                minXYZ[axis] = (std::min)(
                    minXYZ[axis], static_cast<double>(vertex.position[axis]));
                maxXYZ[axis] = (std::max)(
                    maxXYZ[axis], static_cast<double>(vertex.position[axis]));
            }
            const double nx = vertex.normal[0];
            const double ny = vertex.normal[1];
            const double nz = vertex.normal[2];
            normalUpTotal += nz / std::sqrt(nx * nx + ny * ny + nz * nz);
        }

        const double widthX = maxXYZ[0] - minXYZ[0];
        const double widthY = maxXYZ[1] - minXYZ[1];
        const double height = maxXYZ[2] - minXYZ[2];
        const double horizontalSize = (std::max)(widthX, widthY);
        if (!(widthX > 0.0 && widthY > 0.0 && std::isfinite(height)))
        {
            std::cerr << "[FAIL] Captured patch has invalid horizontal span.\n";
            return false;
        }
        std::cout << "Original DX9 patch extent: X=" << widthX
                  << ", Y=" << widthY << ", Z(height)=" << height
                  << ", Z/max(X,Y)=" << height / horizontalSize << '\n';
        std::cout << "Mean normalized original Z-up normal component="
                  << normalUpTotal / patch.size() << '\n';

        constexpr std::size_t side = 17u;
        std::size_t edges = 0;
        std::size_t steep60 = 0;
        std::size_t steep75 = 0;
        double maxSlopeDegrees = 0.0;
        constexpr double radiansToDegrees = 57.29577951308232;
        const auto visitEdge = [&](std::size_t a, std::size_t b) -> bool
        {
            const auto& lhs = patch[a];
            const auto& rhs = patch[b];
            const double dx = double(rhs.position[0]) - lhs.position[0];
            const double dy = double(rhs.position[1]) - lhs.position[1];
            const double dz = double(rhs.position[2]) - lhs.position[2];
            const double planar = std::hypot(dx, dy);
            if (!std::isfinite(planar) || planar <= 0.0 ||
                !std::isfinite(dz))
                return false;
            const double slope = std::atan(std::abs(dz) / planar) *
                                 radiansToDegrees;
            if (!std::isfinite(slope))
                return false;
            ++edges;
            if (slope >= 60.0)
                ++steep60;
            if (slope >= 75.0)
                ++steep75;
            maxSlopeDegrees = (std::max)(maxSlopeDegrees, slope);
            return true;
        };
        for (std::size_t row = 0; row < side; ++row)
        {
            for (std::size_t col = 0; col < side; ++col)
            {
                const std::size_t here = row * side + col;
                if (col + 1 < side && !visitEdge(here, here + 1))
                    return false;
                if (row + 1 < side && !visitEdge(here, here + side))
                    return false;
            }
        }
        std::cout << "Grid-neighbor slopes: edges=" << edges
                  << ", maximum degrees=" << maxSlopeDegrees
                  << ", >=60 degrees=" << steep60
                  << ", >=75 degrees=" << steep75 << '\n';

        for (std::size_t level = 0; level < lod.lod.size(); ++level)
        {
            std::vector<std::uint16_t> triangles;
            const auto topology = level == 0
                ? LegacyTerrainPrimitive::TriangleStrip
                : LegacyTerrainPrimitive::TriangleList;
            const auto& raw = lod.lod[level];
            if (!DX11LegacyTerrainIndices::ToTriangleList(
                    raw.data(), raw.size(), patch.size(), topology, triangles))
                return false;

            std::size_t windingPositive = 0;
            std::size_t windingNegative = 0;
            std::size_t projectedDegenerate = 0;
            for (std::size_t i = 0; i < triangles.size(); i += 3)
            {
                const auto& a = patch[triangles[i]].position;
                const auto& b = patch[triangles[i + 1]].position;
                const auto& c = patch[triangles[i + 2]].position;
                const double abX = double(b[0]) - a[0];
                const double abY = double(b[1]) - a[1];
                const double acX = double(c[0]) - a[0];
                const double acY = double(c[1]) - a[1];
                const double area2 = abX * acY - abY * acX;
                if (std::abs(area2) <= 1.0e-8)
                    ++projectedDegenerate;
                else if (area2 > 0)
                    ++windingPositive;
                else
                    ++windingNegative;
            }
            std::cout << "LOD" << level
                      << ": convertedTriangles=" << triangles.size() / 3u
                      << ", projectedWindingPositive=" << windingPositive
                      << ", projectedWindingNegative=" << windingNegative
                      << ", projectedDegenerate=" << projectedDegenerate
                      << '\n';
            if (windingPositive && windingNegative)
                std::cout << "[WARN] Mixed projected winding in LOD"
                          << level << "; review source edge stitching.\n";
            if (projectedDegenerate)
                std::cout << "[WARN] Degenerate projected triangles in LOD"
                          << level << ".\n";
        }
        std::cout << "[OK] Read-only captured geometry audit complete. "
                     "No runtime files were written.\n";
        return true;
    }

    bool RunSelfTest()
    {
        // Small, deliberately synthetic example; this is NOT captured map data.
        constexpr std::uint16_t lod0[] = {0, 17, 1, 18, 18, 18, 19, 20};
        constexpr std::uint16_t lod1[] = {0, 17, 1, 1, 17, 18};
        constexpr std::uint16_t lod2[] = {0, 34, 2, 2, 34, 36};
        const std::array<LegacyIndexView, 3> source{{
            {lod0, sizeof(lod0) / sizeof(lod0[0])},
            {lod1, sizeof(lod1) / sizeof(lod1[0])},
            {lod2, sizeof(lod2) / sizeof(lod2[0])}
        }};

        std::vector<std::uint8_t> encoded;
        if (!Check("Encode fixture", DX11LegacyIndexSnapshot::Encode(source, encoded)))
            return false;

        LegacyIndexSnapshot decoded;
        if (!Check("Decode fixture", DX11LegacyIndexSnapshot::Decode(
                encoded.data(), encoded.size(), decoded)))
            return false;
        if (!Check("Preserve original LOD bytes",
                   decoded.lod[0] == std::vector<std::uint16_t>(
                       std::begin(lod0), std::end(lod0)) &&
                   decoded.lod[1] == std::vector<std::uint16_t>(
                       std::begin(lod1), std::end(lod1)) &&
                   decoded.lod[2] == std::vector<std::uint16_t>(
                       std::begin(lod2), std::end(lod2))))
            return false;

        if (!Check("Decode converted topology", VerifySnapshot(decoded)))
            return false;

        // Failed input must not change the previously decoded snapshot.
        const LegacyIndexSnapshot saved = decoded;
        encoded[encoded.size() / 2u] ^= 0x20u;
        if (!Check("Checksum corruption rejected",
                !DX11LegacyIndexSnapshot::Decode(encoded.data(), encoded.size(), decoded) &&
                decoded.lod == saved.lod))
            return false;

        // Boundary tests on a well-formed output preserving the last bytes.
        std::vector<std::uint8_t> untouched{42, 84};
        const std::array<LegacyIndexView, 3> invalid{{
            {nullptr, 0}, {lod1, 6}, {lod2, 6}
        }};
        if (!Check("Invalid source rejected without mutation",
                !DX11LegacyIndexSnapshot::Encode(invalid, untouched) &&
                untouched == std::vector<std::uint8_t>{42, 84}))
            return false;

        if (!RunPatchSelfTest() || !RunAdjacentSelfTest())
            return false;
        std::cout << "[OK] Index + patch vertex snapshot and border seam self-tests complete; no game files written.\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc == 1)
        return RunSelfTest() ? 0 : 1;

    if (argc == 4 && std::string(argv[1]) == "--audit-adjacent")
    {
        return AuditAdjacent(argv[2], argv[3]) ? 0 : 7;
    }

    if (argc == 4 && std::string(argv[1]) == "--audit-geometry")
    {
        return AuditGeometry(argv[2], argv[3]) ? 0 : 6;
    }

    if (argc == 3 && std::string(argv[1]) == "--verify-patch")
    {
        if (!VerifyPatchFile(argv[2]))
        {
            std::cerr << "[FAIL] Invalid patch snapshot or file inaccessible.\n";
            return 5;
        }
        return 0;
    }

    if (argc != 3 || std::string(argv[1]) != "--verify")
    {
        std::cerr << "Usage: DX11LODSnapshotProbe.exe [--verify capture.bin | --verify-patch patch.bin | --audit-geometry lod.bin patch.bin | --audit-adjacent patchA.bin patchB.bin]\n";
        return 2;
    }

    LegacyIndexSnapshot snapshot;
    if (!DX11LegacyIndexSnapshot::LoadFile(argv[2], snapshot))
    {
        std::cerr << "[FAIL] Invalid LOD snapshot or file inaccessible.\n";
        return 3;
    }

    return Check("Capture file validated", VerifySnapshot(snapshot)) ? 0 : 4;
}
