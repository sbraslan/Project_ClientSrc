// Exclusive-mode EterPack read-only terrain gate (no rendering).
// Sources: UserInterface.cpp::PackInitialize,
// PRTerrainLib/Terrain.cpp::LoadHeightMap, MapOutdoorLoad.cpp::LoadTerrain.
// NEVER copies extracted bytes to disk, never invokes CTerrain/Direct3D9.
#include <cstdint>
#include "StdAfx.h" // Reuse the project's original EterPack declaration context.
#include "DX11MapPackInspect.h"
#include "../EterBase/Debug.h"
#include "../EterPack/EterPackManager.h"
#include "../EterLib/Renderer/DX11RawTerrainHeight.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace m2::render
{
    namespace
    {
        constexpr std::size_t kMaxVirtualPath = 499u;
        constexpr std::size_t kMaxSettingSize = 256u * 1024u;
        constexpr std::size_t kMaxAtlasSize = 512u * 1024u;
        using PatchPair = std::array<std::array<LegacyPatchVertex,
            DX11LegacyTerrainPatch::kVertexCount>, 2>;

        bool TryGetEnvironment(const char* key, std::string& result) noexcept
        {
            char value[500]{};
            const DWORD count = ::GetEnvironmentVariableA(
                key, value, static_cast<DWORD>(sizeof(value)));
            if (!count || count >= sizeof(value))
                return false;
            try
            {
                result.assign(value, value + count);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool ExactEnabled(const char* key) noexcept
        {
            char value[2]{};
            const DWORD count =
                ::GetEnvironmentVariableA(key, value, sizeof(value));
            return count == 1u && value[0] == '1';
        }

        bool ValidateHeightPath(std::string& input,
                                std::string& setting,
                                unsigned& tileX,
                                unsigned& tileY)
        {
            if (input.size() < 20u || input.size() > kMaxVirtualPath)
                return false;
            for (char& ch : input)
            {
                const auto u = static_cast<unsigned char>(ch);
                if (u < 32u || u >= 127u)
                    return false; // Existing pack index uses ASCII virtual names.
                if (ch == '\\')
                    ch = '/';
                else
                    ch = static_cast<char>(std::tolower(u));
            }
            // Accept only a terrain tile path, not an arbitrary file.
            if (input.find("..") != std::string::npos ||
                input.front() == '/' || input.back() == '/' ||
                !input.ends_with("/height.raw"))
                return false;
            const std::size_t fileSlash = input.find_last_of('/');
            if (fileSlash == std::string::npos)
                return false;
            const std::size_t tileSlash = input.find_last_of(
                '/', fileSlash - 1u);
            if (tileSlash == std::string::npos)
                return false;
            const std::string tile = input.substr(
                tileSlash + 1u, fileSlash - tileSlash - 1u);
            if (tile.size() != 6u)
                return false;
            unsigned id = 0u;
            for (char c : tile)
            {
                if (c < '0' || c > '9')
                    return false;
                id = id * 10u + unsigned(c - '0');
            }
            tileX = id / 1000u;
            tileY = id % 1000u;
            setting = input.substr(0u, tileSlash) + "/Setting.txt";
            return true;
        }

        bool ParseHeightScale(const char* bytes, std::size_t count,
                              float& scale) noexcept
        {
            if (!bytes || !count || count > kMaxSettingSize)
                return false;
            try
            {
                std::istringstream text(std::string(bytes, count));
                std::string line;
                while (std::getline(text, line))
                {
                    // Source MapOutdoor::LoadSetting uses token
                    // 'heightscale'. Preserve its actual value.
                    for (char& c : line)
                        if (c == '"' || c == '\r' || c == '\t')
                            c = ' ';
                    std::istringstream tokens(line);
                    std::string key, value;
                    if (!(tokens >> key >> value))
                        continue;
                    for (char& c : key)
                        c = static_cast<char>(std::tolower(
                            static_cast<unsigned char>(c)));
                    if (key != "heightscale")
                        continue;
                    char* tail = nullptr;
                    const float parsed = std::strtof(value.c_str(), &tail);
                    if (!tail || tail == value.c_str() ||
                        *tail != '\0' || !std::isfinite(parsed) ||
                        parsed <= 0.0f)
                        return false;
                    scale = parsed;
                    return true;
                }
            }
            catch (...)
            {
                return false;
            }
            return false;
        }

        void InspectAtlas(CEterPackManager& pack)
        {
            constexpr const char* candidates[] = {
                "atlasinfo.txt", "locale/tr/atlasinfo.txt",
                "locale/en/atlasinfo.txt"
            };
            for (const char* name : candidates)
            {
                CMappedFile mapped;
                LPCVOID data = nullptr;
                if (!pack.Get(mapped, name, &data) || !data ||
                    mapped.Size() == 0u || mapped.Size() > kMaxAtlasSize)
                    continue;

                std::istringstream text(std::string(
                    static_cast<const char*>(data), mapped.Size()));
                TraceError("[DX11 MAP PACK] atlas source readable: %s (%lu bytes)",
                           name, static_cast<unsigned long>(mapped.Size()));
                std::string line;
                unsigned logged = 0u;
                while (logged < 16u && std::getline(text, line))
                {
                    std::istringstream tokens(line);
                    std::string mapName;
                    if (!(tokens >> mapName) || mapName.size() > 100u)
                        continue;
                    if (!std::all_of(mapName.begin(), mapName.end(),
                        [](char ch) {
                            const unsigned char c =
                                static_cast<unsigned char>(ch);
                            return (c >= 'a' && c <= 'z') ||
                                   (c >= 'A' && c <= 'Z') ||
                                   (c >= '0' && c <= '9') ||
                                   c == '_' || c == '/' || c == '-';
                        }))
                        continue;
                    TraceError("[DX11 MAP PACK] atlas map %u: %s",
                               logged, mapName.c_str());
                    ++logged;
                }
                return; // Avoid duplicate map names from multiple sources.
            }
            TraceError("[DX11 MAP PACK] atlasinfo.txt not accessible through "
                       "the current pack index; use an exact virtual map height.raw path");
        }
    }

    bool DX11MapPackInspectRequested() noexcept
    {
        return ExactEnabled("METIN2_DX11_MAP_PACK_INSPECT");
    }

    int RunDX11MapPackInspect() noexcept
    {
        try
        {
            auto& pack = CEterPackManager::Instance();
            TraceError("[DX11 MAP PACK] original EterPack initialized; "
                       "no D3D9 or D3D11 device created in inspection mode");
            InspectAtlas(pack);

            std::string path;
            if (!TryGetEnvironment("METIN2_DX11_MAP_HEIGHT_PATH", path))
            {
                TraceError("[DX11 MAP PACK] map path not specified; "
                           "discovery only. To inspect a real terrain, set "
                           "METIN2_DX11_MAP_HEIGHT_PATH to the exact virtual "
                           "map folder/000000/height.raw");
                return 0;
            }

            std::string settingsPath;
            unsigned tileX = 0u, tileY = 0u;
            if (!ValidateHeightPath(path, settingsPath, tileX, tileY))
            {
                TraceError("[DX11 MAP PACK] malformed virtual height.raw path; "
                           "expected map_name/000000/height.raw");
                return 22;
            }

            CMappedFile mappedHeight;
            LPCVOID rawData = nullptr;
            if (!pack.Get(mappedHeight, path.c_str(), &rawData) || !rawData)
            {
                TraceError("[DX11 MAP PACK] raw map NOT FOUND in pack or loose files: %s",
                           path.c_str());
                return 23;
            }
            if (mappedHeight.Size() != RawTerrainHeightTile::kRawBytes)
            {
                TraceError("[DX11 MAP PACK] raw map size invalid: %lu, expected %lu",
                           static_cast<unsigned long>(mappedHeight.Size()),
                           static_cast<unsigned long>(RawTerrainHeightTile::kRawBytes));
                return 24;
            }
            auto raw = std::make_unique<RawTerrainHeightTile>();
            if (!DX11RawTerrainHeight::Decode(
                    static_cast<const std::uint8_t*>(rawData),
                    mappedHeight.Size(), *raw))
            {
                TraceError("[DX11 MAP PACK] raw map CPU decoder rejected contents");
                return 25;
            }
            // Release CEterPack's mapped view before any further Get call.
            mappedHeight.Destroy();
            TraceError("[DX11 MAP PACK] ORIGINAL PACK height.raw decoded: %s "
                       "(34322 bytes, tileX=%u tileY=%u)",
                       path.c_str(), tileX, tileY);

            CMappedFile mappedSettings;
            LPCVOID textData = nullptr;
            if (!pack.Get(mappedSettings, settingsPath.c_str(), &textData) ||
                !textData || mappedSettings.Size() > kMaxSettingSize)
            {
                TraceError("[DX11 MAP PACK] map Setting.txt missing/oversized: %s",
                           settingsPath.c_str());
                return 26;
            }
            float heightScale = 0.0f;
            if (!ParseHeightScale(static_cast<const char*>(textData),
                                  mappedSettings.Size(), heightScale))
            {
                TraceError("[DX11 MAP PACK] cannot parse original heightscale in: %s",
                           settingsPath.c_str());
                return 27;
            }
            mappedSettings.Destroy();

            auto pair = std::make_unique<PatchPair>();
            if (!DX11RawTerrainHeight::BuildRightNeighborPair(
                    *raw, tileX, tileY, 0u, 0u, heightScale, *pair))
            {
                TraceError("[DX11 MAP PACK] failed to build original raw patch pair");
                return 28;
            }
            float minimum = (*pair)[0][0].position[2];
            float maximum = minimum;
            for (const auto& patch : *pair)
                for (const auto& v : patch)
                {
                    minimum = (std::min)(minimum, v.position[2]);
                    maximum = (std::max)(maximum, v.position[2]);
                }
            TraceError("[DX11 MAP PACK] ORIGINAL PACK terrain CPU gate PASS: "
                       "heightscale=%g, patch A/B source vertices=289+289, "
                       "shared edge=17/17, minZ=%g maxZ=%g",
                       heightScale, minimum, maximum);
            TraceError("[DX11 MAP PACK] read-only; zero terrain files written, "
                       "zero DX9/11 devices created");
            return 0;
        }
        catch (...)
        {
            TraceError("[DX11 MAP PACK] exception while inspecting map; "
                       "normal game resources untouched");
            return 29;
        }
    }
}
