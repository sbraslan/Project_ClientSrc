// Real saved Metin2 terrain geometry, rendered inside the EXCLUSIVE
// main-DX11-HWND mode. Read-only capture files; NO D3D9 and NO live map.
#include <cstdint>
#include "DX11ExclusiveTerrainPreview.h"
#include "../EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include "../EterLib/Renderer/DX11LegacyPatchSnapshot.h"
#include "../EterLib/Renderer/DX11LegacyTerrainIndices.h"
#include "../EterLib/Renderer/DX11TerrainMesh.h"

#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <vector>

namespace m2::render
{
    namespace
    {
        using Patch = std::array<LegacyPatchVertex,
                                 DX11LegacyPatchSnapshot::kVertexCount>;

        constexpr char kTerrainShader[] = R"HLSL(
            cbuffer Scene : register(b0)
            {
                float4x4 ViewProjection;
                float4 PatchTint;
            };
            struct VSInput
            {
                float3 position : POSITION;
                float3 normal : NORMAL;
                float2 uv : TEXCOORD0;
            };
            struct PSInput
            {
                float4 position : SV_POSITION;
                float3 normal : NORMAL;
                float height : TEXCOORD0;
            };
            PSInput VSMain(VSInput input)
            {
                PSInput o;
                o.position = mul(float4(input.position, 1.0f),
                                 ViewProjection);
                o.normal = input.normal;
                o.height = input.position.y;
                return o;
            }
            float4 PSMain(PSInput input) : SV_TARGET
            {
                float3 n = normalize(input.normal);
                float3 l = normalize(float3(-0.4f, 0.85f, -0.25f));
                float brightness = 0.36f + 0.64f * saturate(dot(n, l));
                float h = 0.88f + 0.12f * saturate(input.height / 3.5f);
                return float4(PatchTint.rgb * brightness * h, 1.0f);
            }
        )HLSL";

        struct alignas(16) SceneConstants
        {
            DirectX::XMFLOAT4X4 viewProjection{};
            DirectX::XMFLOAT4 patchTint{};
        };
        static_assert(sizeof(SceneConstants) % 16u == 0);

        bool ReadPatch(const char* fileName, Patch& output)
        {
            std::ifstream file(fileName, std::ios::binary | std::ios::ate);
            if (!file || file.tellg() !=
                static_cast<std::streamoff>(DX11LegacyPatchSnapshot::kFileBytes))
                return false;
            std::array<std::uint8_t, DX11LegacyPatchSnapshot::kFileBytes> bytes{};
            file.seekg(0, std::ios::beg);
            if (!file.read(reinterpret_cast<char*>(bytes.data()),
                           static_cast<std::streamsize>(bytes.size())))
                return false;
            return DX11LegacyPatchSnapshot::Decode(
                bytes.data(), bytes.size(), output);
        }

        // This first in-exe preview intentionally accepts only the actual
        // verified RIGHT-neighbor pair A (0,-0) + B (3200,-0).
        bool BuildMeshVertices(
            const std::array<Patch, 2>& snapshots,
            std::array<std::vector<TerrainVertex>, 2>& transformed,
            float& cameraDistance)
        {
            constexpr std::size_t side = 17u;
            const auto nearlySame = [](double lhs, double rhs) {
                return std::isfinite(lhs) && std::isfinite(rhs) &&
                       std::abs(lhs - rhs) <= 0.05;
            };
            const double deltaX =
                double(snapshots[1][0].position[0]) -
                double(snapshots[0][0].position[0]);
            const double deltaY =
                double(snapshots[1][0].position[1]) -
                double(snapshots[0][0].position[1]);
            if (!nearlySame(deltaX, 3200.0) || !nearlySame(deltaY, 0.0))
                return false;
            for (std::size_t row = 0; row < side; ++row)
                for (unsigned axis = 0; axis < 3u; ++axis)
                    if (!nearlySame(
                            snapshots[0][row * side + 16u].position[axis],
                            snapshots[1][row * side].position[axis]))
                        return false;

            double lower[3] = {
                snapshots[0][0].position[0],
                snapshots[0][0].position[1],
                snapshots[0][0].position[2]
            };
            double upper[3] = {lower[0], lower[1], lower[2]};
            for (const auto& patch : snapshots)
                for (const auto& vertex : patch)
                    for (unsigned axis = 0; axis < 3u; ++axis)
                    {
                        const double value = vertex.position[axis];
                        if (!std::isfinite(value) ||
                            !std::isfinite(vertex.normal[axis]))
                            return false;
                        lower[axis] = (std::min)(lower[axis], value);
                        upper[axis] = (std::max)(upper[axis], value);
                    }

            const double horizontalSpan = (std::max)(
                upper[0] - lower[0], upper[1] - lower[1]);
            if (!std::isfinite(horizontalSpan) || horizontalSpan <= 0.0)
                return false;
            const double scale = 19.2 / horizontalSpan;
            const double centerX = (lower[0] + upper[0]) * 0.5;
            const double centerY = (lower[1] + upper[1]) * 0.5;
            const double centerZ = (lower[2] + upper[2]) * 0.5;

            std::array<std::vector<TerrainVertex>, 2> candidate;
            double maximumRadiusSquared = 0.0;
            for (std::size_t patch = 0; patch < 2u; ++patch)
            {
                candidate[patch].resize(DX11LegacyPatchSnapshot::kVertexCount);
                for (std::size_t v = 0; v < candidate[patch].size(); ++v)
                {
                    const auto& src = snapshots[patch][v];
                    auto& dst = candidate[patch][v];
                    const double x =
                        (double(src.position[0]) - centerX) * scale;
                    const double y =
                        (double(src.position[2]) - centerZ) * scale;
                    const double z =
                        (centerY - double(src.position[1])) * scale;
                    if (!std::isfinite(x) || !std::isfinite(y) ||
                        !std::isfinite(z))
                        return false;
                    dst.position[0] = static_cast<float>(x);
                    dst.position[1] = static_cast<float>(y);
                    dst.position[2] = static_cast<float>(z);
                    dst.normal[0] = src.normal[0];
                    dst.normal[1] = src.normal[2];
                    dst.normal[2] = -src.normal[1];
                    dst.uv[0] = float(v % side) / 8.0f;
                    dst.uv[1] = float(v / side) / 8.0f;
                    maximumRadiusSquared = (std::max)(
                        maximumRadiusSquared, x * x + y * y + z * z);
                }
            }
            const double suggested = (std::max)(
                21.0, std::sqrt(maximumRadiusSquared) * 3.3);
            if (!std::isfinite(suggested) || suggested > 100000.0)
                return false;
            transformed.swap(candidate);
            cameraDistance = static_cast<float>(suggested);
            return true;
        }
    }

    HRESULT DX11ExclusiveTerrainPreview::InitializeResources(
        DX11RenderDevice& renderer)
    {
        ID3D11Device* device = renderer.NativeDevice();
        if (!device)
            return E_UNEXPECTED;

        LegacyIndexSnapshot lodSnapshot;
        if (!DX11LegacyIndexSnapshot::LoadFile(
                "dx11_terrain_lod_capture.bin", lodSnapshot))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

        std::array<Patch, 2> patches{};
        if (!ReadPatch("dx11_terrain_adjacent_A.bin", patches[0]) ||
            !ReadPatch("dx11_terrain_adjacent_B.bin", patches[1]))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

        std::array<std::vector<TerrainVertex>, 2> vertices;
        if (!BuildMeshVertices(patches, vertices, cameraDistance_))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

        for (std::size_t patch = 0; patch < 2u; ++patch)
            for (std::size_t lod = 0; lod < 3u; ++lod)
            {
                const auto primitive = lod == 0u
                    ? LegacyTerrainPrimitive::TriangleStrip
                    : LegacyTerrainPrimitive::TriangleList;
                const auto& raw = lodSnapshot.lod[lod];
                std::vector<std::uint16_t> triangles;
                if (!DX11LegacyTerrainIndices::ToTriangleList(
                        raw.data(), raw.size(), vertices[patch].size(),
                        primitive, triangles))
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                const HRESULT hr = DX11RenderResources::CreateImmutableMesh(
                    device, vertices[patch].data(),
                    static_cast<UINT>(sizeof(TerrainVertex)),
                    static_cast<UINT>(vertices[patch].size()),
                    triangles.data(), static_cast<UINT>(triangles.size()),
                    meshes_[patch][lod]);
                if (FAILED(hr))
                    return hr;
            }

        constexpr D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        HRESULT hr = DX11RenderResources::CreateShaderPipeline(
            device, kTerrainShader, "VSMain", "PSMain",
            layout, ARRAYSIZE(layout), shader_);
        if (FAILED(hr))
            return hr;

        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = static_cast<UINT>(sizeof(SceneConstants));
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&cb, nullptr, &sceneBuffer_);
        if (FAILED(hr))
            return hr;

        D3D11_RASTERIZER_DESC rs{};
        rs.FillMode = D3D11_FILL_SOLID;
        rs.CullMode = D3D11_CULL_BACK;
        // Prior real terrain screenshots verified FRONT_CW.
        rs.FrontCounterClockwise = FALSE;
        rs.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&rs, &solid_);
        if (FAILED(hr))
            return hr;
        rs.FillMode = D3D11_FILL_WIREFRAME;
        hr = device->CreateRasterizerState(&rs, &wire_);
        if (FAILED(hr))
            return hr;

        D3D11_DEPTH_STENCIL_DESC ds{};
        ds.DepthEnable = TRUE;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        return device->CreateDepthStencilState(&ds, &depthState_);
    }

    HRESULT DX11ExclusiveTerrainPreview::CreateDepth(
        ID3D11Device* device, unsigned width, unsigned height) noexcept
    {
        if (!device || !width || !height)
            return E_INVALIDARG;
        depthView_.Reset();
        depth_.Reset();
        D3D11_TEXTURE2D_DESC d{};
        d.Width = width;
        d.Height = height;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        HRESULT hr = device->CreateTexture2D(&d, nullptr, &depth_);
        if (SUCCEEDED(hr))
            hr = device->CreateDepthStencilView(depth_.Get(), nullptr,
                                                 &depthView_);
        if (FAILED(hr))
        {
            depthView_.Reset();
            depth_.Reset();
        }
        return hr;
    }

    HRESULT DX11ExclusiveTerrainPreview::Initialize(
        DX11RenderDevice& renderer, unsigned width, unsigned height) noexcept
    {
        Shutdown();
        try
        {
            HRESULT hr = InitializeResources(renderer);
            if (SUCCEEDED(hr))
                hr = CreateDepth(renderer.NativeDevice(), width, height);
            if (FAILED(hr))
            {
                Shutdown();
                return hr;
            }
            ready_ = true;
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            Shutdown();
            return E_OUTOFMEMORY;
        }
        catch (...)
        {
            Shutdown();
            return E_FAIL;
        }
    }

    void DX11ExclusiveTerrainPreview::BeforeResize(
        DX11RenderDevice& renderer) noexcept
    {
        if (auto* ctx = renderer.NativeContext())
            ctx->OMSetRenderTargets(0u, nullptr, nullptr);
        depthView_.Reset();
        depth_.Reset();
    }

    HRESULT DX11ExclusiveTerrainPreview::AfterResize(
        DX11RenderDevice& renderer, unsigned width, unsigned height) noexcept
    {
        return ready_ ? CreateDepth(renderer.NativeDevice(), width, height)
                      : E_UNEXPECTED;
    }

    HRESULT DX11ExclusiveTerrainPreview::Draw(
        DX11RenderDevice& renderer, HWND window) noexcept
    {
        auto* ctx = renderer.NativeContext();
        auto* rtv = renderer.NativeRenderTargetView();
        if (!ready_ || !ctx || !rtv || !depthView_ || !window)
            return E_UNEXPECTED;

        RECT client{};
        if (!::GetClientRect(window, &client))
            return E_FAIL;
        const LONG width = client.right - client.left;
        const LONG height = client.bottom - client.top;
        if (width <= 0 || height <= 0)
            return S_OK;

        const float clear[4]{0.08f, 0.13f, 0.21f, 1.0f};
        HRESULT hr = renderer.Clear(clear);
        if (FAILED(hr))
            return hr;

        const auto view = DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(
                0.0f, cameraDistance_ * 0.48f, -cameraDistance_, 1.0f),
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const auto projection = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XM_PIDIV4, float(width) / float(height),
            0.1f, (std::max)(100.0f, cameraDistance_ * 4.0f));
        SceneConstants scene{};
        DirectX::XMStoreFloat4x4(
            &scene.viewProjection, DirectX::XMMatrixTranspose(view * projection));

        ctx->OMSetRenderTargets(1, &rtv, depthView_.Get());
        ctx->ClearDepthStencilView(depthView_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        ctx->OMSetDepthStencilState(depthState_.Get(), 0);
        ctx->RSSetState(wireframe_ ? wire_.Get() : solid_.Get());
        ctx->IASetInputLayout(shader_.inputLayout.Get());
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(shader_.vertex.Get(), nullptr, 0);
        ctx->PSSetShader(shader_.pixel.Get(), nullptr, 0);
        ID3D11Buffer* constantBuffer = sceneBuffer_.Get();
        ctx->VSSetConstantBuffers(0, 1, &constantBuffer);
        ctx->PSSetConstantBuffers(0, 1, &constantBuffer);

        for (unsigned patch = 0; patch < 2u; ++patch)
        {
            scene.patchTint = patch == 0u
                ? DirectX::XMFLOAT4(0.32f, 0.71f, 0.39f, 1.0f)
                : DirectX::XMFLOAT4(0.43f, 0.65f, 0.32f, 1.0f);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            hr = ctx->Map(sceneBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                          0, &mapped);
            if (FAILED(hr))
                return hr;
            std::memcpy(mapped.pData, &scene, sizeof(scene));
            ctx->Unmap(sceneBuffer_.Get(), 0);

            const auto& mesh = meshes_[patch][patch == 0u ? lodA_ : lodB_];
            ID3D11Buffer* vb = mesh.vertices.Get();
            const UINT offset = 0u;
            ctx->IASetVertexBuffers(0, 1, &vb, &mesh.stride, &offset);
            ctx->IASetIndexBuffer(mesh.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
            ctx->DrawIndexed(mesh.indexCount, 0, 0);
        }
        return renderer.Present(true);
    }

    void DX11ExclusiveTerrainPreview::Shutdown() noexcept
    {
        ready_ = false;
        depthView_.Reset();
        depth_.Reset();
        depthState_.Reset();
        sceneBuffer_.Reset();
        wire_.Reset();
        solid_.Reset();
        shader_.Reset();
        for (auto& patch : meshes_)
            for (auto& mesh : patch)
                mesh.Reset();
        cameraDistance_ = 21.0f;
        lodA_ = lodB_ = 0u;
        wireframe_ = false;
    }
}
