#include "DX11TerrainPatchCache.h"

#include <new>
#include <utility>

namespace m2::render
{
    HRESULT DX11TerrainPatchCache::Upsert(
        ID3D11Device* device, const TerrainPatchKey& key,
        const TerrainPatchGeometryView& geometry) noexcept
    {
        if (!device || !ValidateTerrainPatchGeometry(geometry))
            return E_INVALIDARG;

        // A live map may replace an existing tile. New GPU resources must
        // succeed BEFORE swapping out the previously valid patch.
        const auto old = patches_.find(key);
        if (old == patches_.end() &&
            patches_.size() >= kMaxResidentPatches)
            return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);

        GpuPatch next{};
        for (std::size_t lod = 0u; lod < next.lods.size(); ++lod)
        {
            const HRESULT hr = DX11RenderResources::CreateImmutableMesh(
                device, geometry.vertices,
                static_cast<UINT>(sizeof(TerrainVertex)),
                geometry.vertexCount,
                geometry.lodIndices[lod], geometry.lodIndexCounts[lod],
                next.lods[lod]);
            if (FAILED(hr))
                return hr;
        }

        try
        {
            if (old != patches_.end())
            {
                old->second = std::move(next);
                return S_OK;
            }
            patches_.emplace(key, std::move(next));
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        catch (...)
        {
            return E_FAIL;
        }
    }

    HRESULT DX11TerrainPatchCache::Draw(
        ID3D11DeviceContext* context, const TerrainPatchKey& key,
        unsigned lod) const noexcept
    {
        if (!context || lod >= 3u)
            return E_INVALIDARG;
        const auto it = patches_.find(key);
        if (it == patches_.end())
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        const auto& mesh = it->second.lods[lod];
        if (!mesh.vertices || !mesh.indices || !mesh.indexCount)
            return E_UNEXPECTED;

        ID3D11Buffer* vertexBuffer = mesh.vertices.Get();
        const UINT offset = 0u;
        context->IASetVertexBuffers(
            0u, 1u, &vertexBuffer, &mesh.stride, &offset);
        context->IASetIndexBuffer(
            mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0u);
        context->DrawIndexed(mesh.indexCount, 0u, 0);
        return S_OK;
    }

    bool DX11TerrainPatchCache::Remove(
        const TerrainPatchKey& key) noexcept
    {
        return patches_.erase(key) == 1u;
    }

    unsigned DX11TerrainPatchCache::TriangleCount(
        const TerrainPatchKey& key, unsigned lod) const noexcept
    {
        if (lod >= 3u)
            return 0u;
        const auto it = patches_.find(key);
        return it != patches_.end()
            ? it->second.lods[lod].indexCount / 3u : 0u;
    }
}
