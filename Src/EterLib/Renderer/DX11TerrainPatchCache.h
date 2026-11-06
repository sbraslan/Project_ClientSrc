#pragma once

// Shared DX11 terrain GPU cache, not a map-specific test renderer.
// Synchronous immutable Upsert/Remove support loading/unloading any patch.
// It never owns map files and does not instantiate D3D9.
#include "DX11RenderResources.h"
#include "DX11TerrainPatchData.h"

#include <array>
#include <cstddef>
#include <unordered_map>

namespace m2::render
{
    class DX11TerrainPatchCache final
    {
    public:
        // Full three-LOD upload. On failure, existing patch stays intact.
        HRESULT Upsert(ID3D11Device* device,
                       const TerrainPatchKey& key,
                       const TerrainPatchGeometryView& geometry) noexcept;

        // Caller binds shaders, scene constant buffers, raster/depth state,
        // render target and topology before Draw; this class only binds
        // the patch VB/IB and emits DrawIndexed for selected LOD.
        HRESULT Draw(ID3D11DeviceContext* context,
                     const TerrainPatchKey& key,
                     unsigned lod) const noexcept;

        bool Remove(const TerrainPatchKey& key) noexcept;
        void Clear() noexcept { patches_.clear(); }

        std::size_t Size() const noexcept { return patches_.size(); }
        bool Contains(const TerrainPatchKey& key) const noexcept
        {
            return patches_.find(key) != patches_.end();
        }
        unsigned TriangleCount(const TerrainPatchKey& key,
                               unsigned lod) const noexcept;

    private:
        struct GpuPatch
        {
            std::array<DX11MeshBuffers, 3> lods{};
        };

        // Bounds the number of *resident* GPU patches. The higher-level
        // world streamer will evict distant chunks via Remove().
        static constexpr std::size_t kMaxResidentPatches = 4096u;
        std::unordered_map<TerrainPatchKey, GpuPatch,
                           TerrainPatchKeyHash> patches_;
    };
}
