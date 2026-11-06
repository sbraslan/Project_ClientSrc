#pragma once

// Explicit opt-in renderer of two ORIGINAL, saved Metin2 terrain patches.
// This module never accesses GameLib, creates D3D9, rewrites snapshots,
// or alters the normal DX9 client. Scene/materials remain diagnostic-only.
#include "../EterLib/Renderer/DX11RenderDevice.h"
#include "../EterLib/Renderer/DX11RenderResources.h"
#if defined(M2_DX11_GENERIC_TERRAIN) && defined(_WIN64)
#include "../EterLib/Renderer/DX11TerrainPatchCache.h"
#endif

#include <array>
#include <cstddef>
#include <wrl/client.h>

namespace m2::render
{
    class DX11ExclusiveTerrainPreview final
    {
    public:
        HRESULT Initialize(DX11RenderDevice& renderer,
                           unsigned width, unsigned height) noexcept;
        HRESULT Draw(DX11RenderDevice& renderer, HWND window) noexcept;
        void BeforeResize(DX11RenderDevice& renderer) noexcept;
        HRESULT AfterResize(DX11RenderDevice& renderer,
                            unsigned width, unsigned height) noexcept;
        void Shutdown() noexcept;

        void ToggleWireframe() noexcept { wireframe_ = !wireframe_; }
        void SetSharedLOD(unsigned lod) noexcept
        {
            if (lod < 3u) lodA_ = lodB_ = lod;
        }
        void CycleNeighborLOD() noexcept { lodB_ = (lodB_ + 1u) % 3u; }
        unsigned NeighborLOD() const noexcept { return lodB_; }
        unsigned MainLOD() const noexcept { return lodA_; }
        UINT TriangleCount(unsigned patch, unsigned lod) const noexcept
        {
            if (patch >= 2u || lod >= 3u)
                return 0u;
#if defined(M2_DX11_GENERIC_TERRAIN) && defined(_WIN64)
            if (genericEnabled_)
                return genericCache_.TriangleCount(FixtureKey(patch), lod);
#endif
            return meshes_[patch][lod].indexCount / 3u;
        }
#if defined(M2_DX11_GENERIC_TERRAIN) && defined(_WIN64)
        bool GenericPipelineEnabled() const noexcept { return genericEnabled_; }
        std::size_t GenericResidentCount() const noexcept
        {
            return genericCache_.Size();
        }
#endif

    private:
#if defined(M2_DX11_GENERIC_TERRAIN) && defined(_WIN64)
        static TerrainPatchKey FixtureKey(unsigned patch) noexcept
        {
            // Only the fixture assigns these keys. Production map
            // streamers supply their own tile/patch coordinates.
            return TerrainPatchKey{0u, 0u, patch, 0u};
        }
#endif
        HRESULT InitializeResources(DX11RenderDevice& renderer);
        HRESULT CreateDepth(ID3D11Device* device,
                            unsigned width, unsigned height) noexcept;

        std::array<std::array<DX11MeshBuffers, 3>, 2> meshes_{};
#if defined(M2_DX11_GENERIC_TERRAIN) && defined(_WIN64)
        DX11TerrainPatchCache genericCache_{};
        bool genericEnabled_ = false;
#endif
        DX11ShaderPipeline shader_{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> sceneBuffer_;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> solid_;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> wire_;
        float cameraDistance_ = 21.0f;
        unsigned lodA_ = 0;
        unsigned lodB_ = 0;
        bool wireframe_ = false;
        bool ready_ = false;
    };
}
