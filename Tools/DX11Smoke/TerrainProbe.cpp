// Isolated 3D heightfield proof of concept. NO Metin2 DX9 gameplay changes.
// Reuses the EterLib DX11 device and immutable resource classes.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "../../Src/EterLib/Renderer/DX11RenderDevice.h"
#include "../../Src/EterLib/Renderer/DX11RenderResources.h"
#include "../../Src/EterLib/Renderer/DX11TerrainMesh.h"
#include "../../Src/EterLib/Renderer/DX11LegacyTerrainPatch.h"
#include "../../Src/EterLib/Renderer/DX11LegacyTerrainIndices.h"
#include "../../Src/EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include "../../Src/EterLib/Renderer/DX11LegacyPatchSnapshot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr wchar_t kClassName[] = L"Metin2DX11TerrainProbe";
    constexpr wchar_t kTitle[] = L"Metin2 DX11 - Terrain (W: Wireframe, P: 17x17 Patch Preview)";
    constexpr UINT kGridSide = 49;
    constexpr float kCellSize = 0.5f;

    m2::render::DX11RenderDevice g_renderer;
    m2::render::DX11MeshBuffers g_mesh;
    m2::render::DX11MeshBuffers g_patchMesh;
    std::array<m2::render::DX11MeshBuffers, 3> g_realLODMeshes;
    std::array<m2::render::DX11MeshBuffers, 3> g_realPatchLODMeshes;
    m2::render::LegacyIndexSnapshot g_realSnapshot;
    std::array<m2::render::LegacyPatchVertex,
               m2::render::DX11LegacyPatchSnapshot::kVertexCount> g_realPatchSnapshot{};
    m2::render::DX11Texture g_grass;
    m2::render::DX11ShaderPipeline g_pipeline;
    ComPtr<ID3D11Buffer> g_sceneConstants;
    ComPtr<ID3D11SamplerState> g_sampler;
    ComPtr<ID3D11RasterizerState> g_solid;
    ComPtr<ID3D11RasterizerState> g_wire;
    // Optional culling tests; default no-cull path is unchanged.
    // Index 0 = D3D11 FrontCounterClockwise FALSE, index 1 = TRUE.
    std::array<ComPtr<ID3D11RasterizerState>, 2> g_cullSolid;
    std::array<ComPtr<ID3D11RasterizerState>, 2> g_cullWire;
    ComPtr<ID3D11DepthStencilState> g_depthState;

    struct DepthSurface
    {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11DepthStencilView> view;

        void Reset() noexcept
        {
            view.Reset();
            texture.Reset();
        }

        HRESULT Create(ID3D11Device* device, UINT width, UINT height) noexcept
        {
            if (!device || width == 0 || height == 0)
                return E_INVALIDARG;

            Reset();
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

            HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture);
            if (FAILED(hr))
                return hr;
            hr = device->CreateDepthStencilView(texture.Get(), nullptr, &view);
            if (FAILED(hr))
                Reset();
            return hr;
        }
    };
    DepthSurface g_depth;

    struct alignas(16) SceneConstants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 lightDirection;
    };
    static_assert(sizeof(SceneConstants) % 16 == 0, "DX11 constant buffer alignment");

    bool g_ready = false;
    bool g_wireframe = false;
    bool g_patchPreview = false;
    bool g_realLODLoaded = false;
    bool g_realLODPreview = false;
    bool g_realPatchLoaded = false;
    bool g_realPatchPreview = false;
    // C toggles: 0 = no culling, 1 = backface cull/front CW,
    // 2 = backface cull/front CCW. Never silently change default.
    unsigned g_cullingMode = 0;
    float g_realPatchViewDistance = 21.0f;
    std::size_t g_activeRealLOD = 0;

    constexpr char kTerrainShader[] = R"HLSL(
        cbuffer Scene : register(b0)
        {
            float4x4 ViewProjection;
            float4 LightDirection;
        };
        Texture2D TerrainTexture : register(t0);
        SamplerState TerrainSampler : register(s0);
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
            float2 uv : TEXCOORD0;
            float height : TEXCOORD1;
        };
        PSInput VSMain(VSInput input)
        {
            PSInput output;
            output.position = mul(float4(input.position, 1.0f), ViewProjection);
            output.normal = input.normal;
            output.uv = input.uv;
            output.height = input.position.y;
            return output;
        }
        float4 PSMain(PSInput input) : SV_TARGET
        {
            const float3 normal = normalize(input.normal);
            const float3 light = normalize(LightDirection.xyz);
            const float diffuse = saturate(dot(normal, light));
            const float ambient = 0.38f;
            const float heightTint = 0.90f + 0.10f * saturate(input.height / 3.5f);
            const float3 grass = TerrainTexture.Sample(TerrainSampler, input.uv).rgb;
            return float4(grass * (ambient + 0.62f * diffuse) * heightTint, 1.0f);
        }
    )HLSL";

    void LogFailure(const wchar_t* operation, HRESULT hr)
    {
        wchar_t output[256]{};
        wsprintfW(output, L"%s failed (HRESULT 0x%08X)\n",
                  operation, static_cast<unsigned>(hr));
        OutputDebugStringW(output);
    }

    // Read ONLY a caller-selected, bounded snapshot file. The filename is
    // wide to preserve non-ASCII Windows paths (e.g. Masaustu with umlaut).
    // The shared codec verifies magic, version, lengths, checksum and all
    // index topology before any D3D11 resource can be created.
    bool LoadCaptureFile(const std::wstring& path,
                         m2::render::LegacyIndexSnapshot& result)
    {
        using m2::render::DX11LegacyIndexSnapshot;
        constexpr std::size_t kMaxSnapshotBytes =
            8u + 4u + DX11LegacyIndexSnapshot::kLODCount * 4u +
            DX11LegacyIndexSnapshot::kLODCount *
                DX11LegacyIndexSnapshot::kMaxPerLOD * sizeof(std::uint16_t) +
            4u;

        HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER length{};
        bool valid = ::GetFileSizeEx(file, &length) != FALSE &&
                     length.QuadPart >= 28 &&
                     length.QuadPart <= static_cast<LONGLONG>(kMaxSnapshotBytes);
        if (valid)
        {
            std::vector<std::uint8_t> bytes(
                static_cast<std::size_t>(length.QuadPart));
            DWORD bytesRead = 0;
            valid = ::ReadFile(file, bytes.data(),
                               static_cast<DWORD>(bytes.size()),
                               &bytesRead, nullptr) != FALSE &&
                    bytesRead == bytes.size();
            if (valid)
                valid = DX11LegacyIndexSnapshot::Decode(
                    bytes.data(), bytes.size(), result);
        }

        return ::CloseHandle(file) != FALSE && valid;
    }

    // Verified real 17x17 positions/normals (M2PVERT1, exactly 6956 bytes).
    // Unlike the original DX9 coordinates, the independent DX11 test scene
    // uses Y as its vertical axis. File read is strictly read-only.
    bool LoadPatchCaptureFile(
        const std::wstring& path,
        std::array<m2::render::LegacyPatchVertex,
                   m2::render::DX11LegacyPatchSnapshot::kVertexCount>& output)
    {
        constexpr std::size_t expected =
            m2::render::DX11LegacyPatchSnapshot::kFileBytes;
        HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER length{};
        bool valid = ::GetFileSizeEx(file, &length) != FALSE &&
                     length.QuadPart == static_cast<LONGLONG>(expected);
        if (valid)
        {
            std::array<std::uint8_t, expected> bytes{};
            DWORD bytesRead = 0;
            valid = ::ReadFile(file, bytes.data(),
                               static_cast<DWORD>(bytes.size()),
                               &bytesRead, nullptr) != FALSE &&
                    bytesRead == bytes.size();
            if (valid)
                valid = m2::render::DX11LegacyPatchSnapshot::Decode(
                    bytes.data(), bytes.size(), output);
        }
        const bool closed = ::CloseHandle(file) != FALSE;
        return closed && valid;
    }

    // Patch vertices are captured from the original game's local+world
    // coordinates: (DX9 X, DX9 Y, DX9 Z=height), Z-up. Convert to viewer
    // (X, Z, -Y) -- a pure rotation with positive determinant, NOT a
    // mirror that would reverse triangle winding. Rotate normals likewise.
    // Recenter and use ONE uniform scale on all axes; no vertical exaggeration.
    // This lets tiny 17x17 patches with large DX9 world coordinates fit
    // the independent camera without changing the physical slope ratios.
    bool BuildCapturedPatchPreview(
        const std::array<m2::render::LegacyPatchVertex,
                         m2::render::DX11LegacyPatchSnapshot::kVertexCount>& source,
        std::vector<m2::render::TerrainVertex>& output,
        float& viewDistance)
    {
        double minXYZ[3]{source[0].position[0],
                         source[0].position[1],
                         source[0].position[2]};
        double maxXYZ[3]{minXYZ[0], minXYZ[1], minXYZ[2]};
        for (const auto& v : source)
        {
            double normalLength = 0.0;
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(v.position[axis]) ||
                    !std::isfinite(v.normal[axis]))
                    return false;
                const double p = v.position[axis];
                minXYZ[axis] = (std::min)(minXYZ[axis], p);
                maxXYZ[axis] = (std::max)(maxXYZ[axis], p);
                normalLength += static_cast<double>(v.normal[axis]) *
                                v.normal[axis];
            }
            if (!std::isfinite(normalLength) || normalLength <= 1.0e-8)
                return false;
        }

        const double spanX = maxXYZ[0] - minXYZ[0];
        const double spanY = maxXYZ[1] - minXYZ[1];
        const double horizontalSpan = (std::max)(spanX, spanY);
        if (!std::isfinite(horizontalSpan) || horizontalSpan <= 0.0)
            return false;

        // Same units in X, Y and Z. The horizontal patch spans <=19.2
        // viewer units; a steep original height range remains steep.
        const double scale = 19.2 / horizontalSpan;
        const double centerX = (maxXYZ[0] + minXYZ[0]) * 0.5;
        const double centerY = (maxXYZ[1] + minXYZ[1]) * 0.5;
        const double centerZ = (maxXYZ[2] + minXYZ[2]) * 0.5;
        std::vector<m2::render::TerrainVertex> transformed(source.size());
        double radiusSquaredMax = 0.0;
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            const auto& vertex = source[i];
            auto& out = transformed[i];
            const double x = (static_cast<double>(vertex.position[0]) - centerX) * scale;
            const double y = (static_cast<double>(vertex.position[2]) - centerZ) * scale;
            const double z = (centerY - static_cast<double>(vertex.position[1])) * scale;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                return false;
            out.position[0] = static_cast<float>(x);
            out.position[1] = static_cast<float>(y);
            out.position[2] = static_cast<float>(z);
            out.normal[0] = vertex.normal[0];
            out.normal[1] = vertex.normal[2];
            out.normal[2] = -vertex.normal[1];

            const std::uint32_t gridX = static_cast<std::uint32_t>(i % 17u);
            const std::uint32_t gridZ = static_cast<std::uint32_t>(i / 17u);
            out.uv[0] = static_cast<float>(gridX) * (2.0f / 16.0f);
            out.uv[1] = static_cast<float>(gridZ) * (2.0f / 16.0f);
            radiusSquaredMax = (std::max)(radiusSquaredMax,
                                         x * x + y * y + z * z);
        }

        const double suggestedDistance = (std::max)(
            21.0, std::sqrt(radiusSquaredMax) * 3.3);
        if (!std::isfinite(suggestedDistance) || suggestedDistance > 100000.0)
            return false;
        const float candidateDistance = static_cast<float>(suggestedDistance);
        output.swap(transformed);
        viewDistance = candidateDistance;
        return true;
    }

    bool ValidateCapturedPatchTransform()
    {
        std::array<m2::render::LegacyPatchVertex,
                   m2::render::DX11LegacyPatchSnapshot::kVertexCount> fixture{};
        for (std::size_t i = 0; i < fixture.size(); ++i)
        {
            fixture[i].position[0] = 18000.0f +
                100.0f * static_cast<float>(i % 17u);
            fixture[i].position[1] = -12000.0f -
                100.0f * static_cast<float>(i / 17u);
            fixture[i].position[2] = 24249.0f +
                10.0f * static_cast<float>(i % 5u);
            fixture[i].normal[0] = 0.0f;
            fixture[i].normal[1] = 0.0f;
            fixture[i].normal[2] = 1.0f;
        }
        std::vector<m2::render::TerrainVertex> vertices;
        float distance = 0.0f;
        if (!BuildCapturedPatchPreview(fixture, vertices, distance) ||
            vertices.size() != fixture.size() || distance < 21.0f)
            return false;

        // Equivalent DX9 X and -Y cell spacing must survive at equal scale.
        const float deltaX = vertices[1].position[0] - vertices[0].position[0];
        const float deltaZ = vertices[17].position[2] - vertices[0].position[2];
        if (std::abs(deltaX - deltaZ) > 0.0001f ||
            std::abs(vertices[0].normal[1] - 1.0f) > 0.0001f ||
            std::abs(vertices[0].normal[2]) > 0.0001f)
            return false;

        // Malformed source does not overwrite a last successfully built mesh.
        fixture[0].normal[2] = 0.0f;
        return !BuildCapturedPatchPreview(fixture, vertices, distance) &&
               vertices.size() == fixture.size();
    }

    m2::render::HeightField CreateTestHeights()
    {
        m2::render::HeightField field;
        field.columns = kGridSide;
        field.rows = kGridSide;
        field.cellSize = kCellSize;
        field.textureRepeats = 6.0f;
        field.heights.resize(static_cast<std::size_t>(kGridSide) * kGridSide);

        for (UINT z = 0; z < kGridSide; ++z)
        {
            for (UINT x = 0; x < kGridSide; ++x)
            {
                const float fx = (static_cast<float>(x) - 24.0f) * kCellSize;
                const float fz = (static_cast<float>(z) - 24.0f) * kCellSize;
                const float ridge =
                    2.4f * std::exp(-((fx - 3.0f) * (fx - 3.0f) +
                                          (fz - 1.0f) * (fz - 1.0f)) / 22.0f);
                const float hills =
                    0.9f * std::sin(fx * 0.48f) * std::cos(fz * 0.42f);
                field.heights[static_cast<std::size_t>(z) * kGridSide + x] =
                    ridge + hills;
            }
        }
        return field;
    }

    bool MakeLegacyPatchPreview(
        std::vector<m2::render::TerrainVertex>& vertices,
        std::vector<std::uint16_t>& indices)
    {
        using m2::render::DX11LegacyTerrainPatch;

        m2::render::HeightField heightfield;
        heightfield.columns = DX11LegacyTerrainPatch::kPatchSide;
        heightfield.rows = DX11LegacyTerrainPatch::kPatchSide;
        heightfield.cellSize = 1.2f;
        heightfield.textureRepeats = 2.0f;
        heightfield.heights.resize(DX11LegacyTerrainPatch::kVertexCount);

        for (std::uint32_t z = 0; z < heightfield.rows; ++z)
        {
            for (std::uint32_t x = 0; x < heightfield.columns; ++x)
            {
                const float fx = (static_cast<float>(x) - 8.0f) * heightfield.cellSize;
                const float fz = (static_cast<float>(z) - 8.0f) * heightfield.cellSize;
                heightfield.heights[static_cast<std::size_t>(z) * heightfield.columns + x] =
                    2.0f * std::exp(-(fx * fx + fz * fz) / 28.0f) +
                    0.25f * std::sin(fx * 0.6f);
            }
        }

        std::vector<m2::render::TerrainVertex> sourceVertices;
        std::vector<std::uint16_t> sourceIndices;
        if (!m2::render::DX11TerrainMesh::Build(
                heightfield, sourceVertices, sourceIndices))
            return false;

        std::vector<m2::render::LegacyPatchVertex> source(
            DX11LegacyTerrainPatch::kVertexCount);
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                source[i].position[axis] = sourceVertices[i].position[axis];
                source[i].normal[axis] = sourceVertices[i].normal[axis];
            }
        }

        if (!DX11LegacyTerrainPatch::Convert(
                source.data(), source.size(), 2.0f, vertices, indices))
            return false;

        if (vertices.size() != DX11LegacyTerrainPatch::kVertexCount ||
            indices.size() != DX11LegacyTerrainPatch::kIndexCount)
            return false;

        // Cross-check that the adapter preserved world-space vertex data.
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                if (vertices[i].position[axis] != source[i].position[axis] ||
                    vertices[i].normal[axis] != source[i].normal[axis])
                    return false;
            }
        }
        for (const std::uint16_t index : indices)
        {
            if (index >= vertices.size())
                return false;
        }

        // Invalid normal must be rejected without mutating the output.
        for (float& component : source.front().normal)
            component = 0.0f;
        if (DX11LegacyTerrainPatch::Convert(
                source.data(), source.size(), 2.0f, vertices, indices))
            return false;
        return vertices.size() == DX11LegacyTerrainPatch::kVertexCount &&
               indices.size() == DX11LegacyTerrainPatch::kIndexCount;
    }

    bool ValidateLegacyIndexTopology()
    {
        using m2::render::DX11LegacyTerrainIndices;
        using m2::render::LegacyTerrainPrimitive;

        // First four LOD0 strip indices match the original terrain grid:
        // 0, rowStride(17), 1, rowStride(17)+1.
        constexpr std::uint16_t strip[] = {0, 17, 1, 18};
        std::vector<std::uint16_t> result;
        if (!DX11LegacyTerrainIndices::ToTriangleList(
                strip, ARRAYSIZE(strip), 289,
                LegacyTerrainPrimitive::TriangleStrip, result))
            return false;
        if (result != std::vector<std::uint16_t>{0, 17, 1, 1, 17, 18})
            return false;

        // Real strip rows use repeated indices as degenerate connectors.
        // Parity must be preserved across every skipped triangle.
        constexpr std::uint16_t stitched[] = {0, 1, 2, 2, 2, 3, 4, 5};
        if (!DX11LegacyTerrainIndices::ToTriangleList(
                stitched, ARRAYSIZE(stitched), 289,
                LegacyTerrainPrimitive::TriangleStrip, result))
            return false;
        if (result != std::vector<std::uint16_t>{
                0, 1, 2, 2, 3, 4, 4, 3, 5})
            return false;

        constexpr std::uint16_t list[] = {0, 17, 1, 1, 17, 18};
        if (!DX11LegacyTerrainIndices::ToTriangleList(
                list, ARRAYSIZE(list), 289,
                LegacyTerrainPrimitive::TriangleList, result))
            return false;
        if (result != std::vector<std::uint16_t>{0, 17, 1, 1, 17, 18})
            return false;

        // Reject malformed and out-of-range input, retaining the last mesh.
        constexpr std::uint16_t bad[] = {0, 17, 289};
        if (DX11LegacyTerrainIndices::ToTriangleList(
                bad, ARRAYSIZE(bad), 289,
                LegacyTerrainPrimitive::TriangleList, result) ||
            result != std::vector<std::uint16_t>{0, 17, 1, 1, 17, 18})
            return false;

        if (DX11LegacyTerrainIndices::ToTriangleList(
                list, ARRAYSIZE(list) - 1, 289,
                LegacyTerrainPrimitive::TriangleList, result))
            return false;

        return result == std::vector<std::uint16_t>{0, 17, 1, 1, 17, 18};
    }

    bool ValidateTerrainBuilder()
    {
        const m2::render::HeightField field = CreateTestHeights();
        std::vector<m2::render::TerrainVertex> vertices;
        std::vector<std::uint16_t> indices;
        if (!m2::render::DX11TerrainMesh::Build(field, vertices, indices))
            return false;

        if (vertices.size() != kGridSide * kGridSide ||
            indices.size() != (kGridSide - 1) * (kGridSide - 1) * 6)
            return false;

        for (const std::uint16_t index : indices)
        {
            if (index >= vertices.size())
                return false;
        }

        m2::render::HeightField flat;
        flat.columns = 2;
        flat.rows = 2;
        flat.heights = {0.0f, 0.0f, 0.0f, 0.0f};
        if (!m2::render::DX11TerrainMesh::Build(flat, vertices, indices) ||
            vertices.size() != 4 || indices.size() != 6)
            return false;

        for (const auto& vertex : vertices)
        {
            if (std::abs(vertex.normal[0]) > 0.0001f ||
                std::abs(vertex.normal[1] - 1.0f) > 0.0001f ||
                std::abs(vertex.normal[2]) > 0.0001f)
                return false;
        }

        // Invalid input must be rejected without overwriting the last mesh.
        flat.heights.pop_back();
        if (m2::render::DX11TerrainMesh::Build(flat, vertices, indices) ||
            vertices.size() != 4 || indices.size() != 6)
            return false;

        std::vector<m2::render::TerrainVertex> patchVertices;
        std::vector<std::uint16_t> patchIndices;
        return MakeLegacyPatchPreview(patchVertices, patchIndices);
    }

    HRESULT CreateTerrain()
    {
        using m2::render::DX11RenderResources;
        ID3D11Device* device = g_renderer.NativeDevice();
        if (!device)
            return E_UNEXPECTED;

        const m2::render::HeightField heightfield = CreateTestHeights();
        std::vector<m2::render::TerrainVertex> vertices;
        std::vector<std::uint16_t> indices;
        if (!m2::render::DX11TerrainMesh::Build(heightfield, vertices, indices))
            return E_FAIL;

        HRESULT hr = DX11RenderResources::CreateImmutableMesh(
            device, vertices.data(),
            static_cast<UINT>(sizeof(m2::render::TerrainVertex)),
            static_cast<UINT>(vertices.size()), indices.data(),
            static_cast<UINT>(indices.size()), g_mesh);
        if (FAILED(hr))
            return hr;

        // Secondary 17x17 *synthetic* legacy-layout patch preview.
        // The real GameLib patch/LOD/splat source is not connected.
        std::vector<m2::render::TerrainVertex> patchVertices;
        std::vector<std::uint16_t> patchIndices;
        if (!MakeLegacyPatchPreview(patchVertices, patchIndices))
            return E_FAIL;
        hr = DX11RenderResources::CreateImmutableMesh(
            device, patchVertices.data(),
            static_cast<UINT>(sizeof(m2::render::TerrainVertex)),
            static_cast<UINT>(patchVertices.size()), patchIndices.data(),
            static_cast<UINT>(patchIndices.size()), g_patchMesh);
        if (FAILED(hr))
            return hr;

        // Captured REAL LOD index streams on the existing SYNTHETIC 17x17
        // preview vertices. The .bin has indices only: this intentionally
        // does not claim to render original in-game heights or textures.
        if (g_realLODLoaded)
        {
            if (patchVertices.size() !=
                m2::render::DX11LegacyIndexSnapshot::kVertexCount)
                return E_INVALIDARG;

            for (std::size_t lod = 0; lod < g_realLODMeshes.size(); ++lod)
            {
                const auto& original = g_realSnapshot.lod[lod];
                const auto primitive = lod == 0
                    ? m2::render::LegacyTerrainPrimitive::TriangleStrip
                    : m2::render::LegacyTerrainPrimitive::TriangleList;
                std::vector<std::uint16_t> triangles;
                if (!m2::render::DX11LegacyTerrainIndices::ToTriangleList(
                        original.data(), original.size(), patchVertices.size(),
                        primitive, triangles))
                    return E_INVALIDARG;

                hr = DX11RenderResources::CreateImmutableMesh(
                    device, patchVertices.data(),
                    static_cast<UINT>(sizeof(m2::render::TerrainVertex)),
                    static_cast<UINT>(patchVertices.size()), triangles.data(),
                    static_cast<UINT>(triangles.size()), g_realLODMeshes[lod]);
                if (FAILED(hr))
                    return hr;
            }
        }

        // An independent GPU mesh for each REAL LOD plus the captured REAL
        // source patch vertices. The old synthetic LOD meshes remain intact
        // for toggled comparison. Do not assume the LOD indices and vertices
        // originate from the exact same world-space patch.
        if (g_realPatchLoaded)
        {
            if (!g_realLODLoaded)
                return E_INVALIDARG;
            std::vector<m2::render::TerrainVertex> originalPatchVertices;
            if (!BuildCapturedPatchPreview(
                    g_realPatchSnapshot, originalPatchVertices,
                    g_realPatchViewDistance))
                return E_INVALIDARG;

            for (std::size_t lod = 0; lod < g_realPatchLODMeshes.size(); ++lod)
            {
                const auto& rawIndices = g_realSnapshot.lod[lod];
                const auto primitive = lod == 0
                    ? m2::render::LegacyTerrainPrimitive::TriangleStrip
                    : m2::render::LegacyTerrainPrimitive::TriangleList;
                std::vector<std::uint16_t> triangles;
                if (!m2::render::DX11LegacyTerrainIndices::ToTriangleList(
                        rawIndices.data(), rawIndices.size(),
                        originalPatchVertices.size(), primitive, triangles))
                    return E_INVALIDARG;

                hr = DX11RenderResources::CreateImmutableMesh(
                    device, originalPatchVertices.data(),
                    static_cast<UINT>(sizeof(m2::render::TerrainVertex)),
                    static_cast<UINT>(originalPatchVertices.size()),
                    triangles.data(), static_cast<UINT>(triangles.size()),
                    g_realPatchLODMeshes[lod]);
                if (FAILED(hr))
                    return hr;
            }
        }

        constexpr D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        hr = DX11RenderResources::CreateShaderPipeline(
            device, kTerrainShader, "VSMain", "PSMain",
            layout, ARRAYSIZE(layout), g_pipeline);
        if (FAILED(hr))
            return hr;

        constexpr UINT textureSide = 128;
        std::array<std::uint8_t, textureSide * textureSide * 4> pixels{};
        for (UINT z = 0; z < textureSide; ++z)
        {
            for (UINT x = 0; x < textureSide; ++x)
            {
                const std::size_t i =
                    (static_cast<std::size_t>(z) * textureSide + x) * 4;
                const UINT noise = (x * 23u + z * 53u + ((x * z) % 37u)) % 34u;
                const bool patch = (((x / 16u) + (z / 16u)) % 2u) == 0u;
                pixels[i + 0] = static_cast<std::uint8_t>((patch ? 59 : 73) + noise);
                pixels[i + 1] = static_cast<std::uint8_t>((patch ? 121 : 139) + noise);
                pixels[i + 2] = static_cast<std::uint8_t>((patch ? 45 : 52) + noise / 2u);
                pixels[i + 3] = 255;
            }
        }
        hr = DX11RenderResources::CreateRGBA8Texture(
            device, textureSide, textureSide, pixels.data(), g_grass);
        if (FAILED(hr))
            return hr;

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&sampler, &g_sampler);
        if (FAILED(hr))
            return hr;

        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&raster, &g_solid);
        if (FAILED(hr))
            return hr;
        raster.FillMode = D3D11_FILL_WIREFRAME;
        hr = device->CreateRasterizerState(&raster, &g_wire);
        if (FAILED(hr))
            return hr;

        // Do not assume DX9 D3DCULL_CW maps directly to a DX11 setting.
        // DX11 front-facing is evaluated in render-target coordinates,
        // so offer BOTH D3D11 front-face conventions, with identical
        // camera/vertices/indices and no hidden winding rewrite.
        raster.CullMode = D3D11_CULL_BACK;
        for (std::size_t face = 0; face < g_cullSolid.size(); ++face)
        {
            raster.FrontCounterClockwise = face == 0 ? FALSE : TRUE;
            raster.FillMode = D3D11_FILL_SOLID;
            hr = device->CreateRasterizerState(&raster, &g_cullSolid[face]);
            if (FAILED(hr))
                return hr;
            raster.FillMode = D3D11_FILL_WIREFRAME;
            hr = device->CreateRasterizerState(&raster, &g_cullWire[face]);
            if (FAILED(hr))
                return hr;
        }

        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = device->CreateDepthStencilState(&depthDesc, &g_depthState);
        if (FAILED(hr))
            return hr;

        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = static_cast<UINT>(sizeof(SceneConstants));
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return device->CreateBuffer(&cb, nullptr, &g_sceneConstants);
    }

    void ReleaseTerrain()
    {
        if (auto* context = g_renderer.NativeContext())
        {
            context->ClearState();
            context->Flush();
        }
        g_depth.Reset();
        g_sceneConstants.Reset();
        g_depthState.Reset();
        for (auto& state : g_cullWire)
            state.Reset();
        for (auto& state : g_cullSolid)
            state.Reset();
        g_wire.Reset();
        g_solid.Reset();
        g_sampler.Reset();
        g_grass.Reset();
        g_pipeline.Reset();
        for (auto& mesh : g_realLODMeshes)
            mesh.Reset();
        for (auto& mesh : g_realPatchLODMeshes)
            mesh.Reset();
        g_patchMesh.Reset();
        g_mesh.Reset();
    }

    HRESULT DrawTerrain(HWND window)
    {
        ID3D11DeviceContext* context = g_renderer.NativeContext();
        ID3D11RenderTargetView* rtv = g_renderer.NativeRenderTargetView();
        ID3D11DepthStencilView* dsv = g_depth.view.Get();

        if (!context || !rtv || !dsv)
            return E_UNEXPECTED;

        RECT rect{};
        GetClientRect(window, &rect);
        const LONG width = rect.right - rect.left;
        const LONG height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0)
            return S_OK;

        // Left-handed row-vector view/projection (DirectXMath conventions).
        // HLSL expects column-major storage, so upload the transpose.
        const bool showRealVertices =
            g_realLODLoaded && g_realLODPreview &&
            g_realPatchLoaded && g_realPatchPreview;
        const float distance = showRealVertices
            ? g_realPatchViewDistance : 21.0f;
        const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
            showRealVertices
                ? DirectX::XMVectorSet(
                    0.0f, distance * 0.48f, -distance, 1.0f)
                : DirectX::XMVectorSet(0.0f, 12.0f, -21.0f, 1.0f),
            showRealVertices
                ? DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
                : DirectX::XMVectorSet(0.0f, 0.5f, 0.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const float farPlane = showRealVertices
            ? (std::max)(100.0f, distance * 4.0f) : 100.0f;
        const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XM_PIDIV4, aspect, 0.1f, farPlane);

        SceneConstants scene{};
        DirectX::XMStoreFloat4x4(
            &scene.viewProjection, DirectX::XMMatrixTranspose(view * projection));
        scene.lightDirection = DirectX::XMFLOAT4(-0.4f, 0.85f, -0.25f, 0.0f);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context->Map(g_sceneConstants.Get(), 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr))
            return hr;
        std::memcpy(mapped.pData, &scene, sizeof(scene));
        context->Unmap(g_sceneConstants.Get(), 0);

        constexpr float clear[4] = {0.08f, 0.13f, 0.21f, 1.0f};
        hr = g_renderer.Clear(clear);
        if (FAILED(hr))
            return hr;

        // The device's Clear binds the RTV without a DSV. Rebind both now.
        context->OMSetRenderTargets(1, &rtv, dsv);
        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        context->OMSetDepthStencilState(g_depthState.Get(), 0);
        ID3D11RasterizerState* raster = nullptr;
        if (g_cullingMode == 0)
            raster = g_wireframe ? g_wire.Get() : g_solid.Get();
        else
        {
            const std::size_t frontFace = g_cullingMode - 1u;
            raster = g_wireframe
                ? g_cullWire[frontFace].Get()
                : g_cullSolid[frontFace].Get();
        }
        context->RSSetState(raster);

        const m2::render::DX11MeshBuffers& active =
            g_realLODPreview && g_realLODLoaded
                ? (showRealVertices
                    ? g_realPatchLODMeshes[g_activeRealLOD]
                    : g_realLODMeshes[g_activeRealLOD])
                : (g_patchPreview ? g_patchMesh : g_mesh);
        ID3D11Buffer* vb = active.vertices.Get();
        const UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vb, &active.stride, &offset);
        context->IASetIndexBuffer(active.indices.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetInputLayout(g_pipeline.inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11Buffer* camera = g_sceneConstants.Get();
        ID3D11ShaderResourceView* texture = g_grass.view.Get();
        ID3D11SamplerState* sampler = g_sampler.Get();
        context->VSSetConstantBuffers(0, 1, &camera);
        context->PSSetConstantBuffers(0, 1, &camera);
        context->VSSetShader(g_pipeline.vertex.Get(), nullptr, 0);
        context->PSSetShader(g_pipeline.pixel.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, &texture);
        context->PSSetSamplers(0, 1, &sampler);

        context->DrawIndexed(active.indexCount, 0, 0);
        return g_renderer.Present(true);
    }

    void SetWindowTitleWithCull(HWND window, const wchar_t* original)
    {
        // Visible in screenshots: no-cull, front CW or front CCW.
        const wchar_t* status = L"OFF";
        if (g_cullingMode == 1)
            status = L"BACK/FRONT_CW";
        else if (g_cullingMode == 2)
            status = L"BACK/FRONT_CCW";

        wchar_t caption[360]{};
        swprintf_s(caption, ARRAYSIZE(caption),
                   L"%s | C:%s", original, status);
        SetWindowTextW(window, caption);
    }

    void UpdateWindowTitle(HWND window)
    {
        // Explicitly identify the active scene; title alone previously only
        // listed controls, so screenshots couldn't prove the P key was tested.
        if (g_realLODLoaded && g_realLODPreview)
        {
            wchar_t caption[256]{};
            if (g_realPatchLoaded && g_realPatchPreview)
                swprintf_s(caption, ARRAYSIZE(caption),
                    L"Metin2 DX11 | REAL PATCH VERTICES | CAPTURED LOD%u: %u tris | Z->Y | %s | 1/2/3 V R P W",
                    static_cast<unsigned>(g_activeRealLOD),
                    static_cast<unsigned>(g_realPatchLODMeshes[g_activeRealLOD].indexCount / 3u),
                    g_wireframe ? L"Wireframe" : L"Solid");
            else
                swprintf_s(caption, ARRAYSIZE(caption),
                    L"Metin2 DX11 | CAPTURED INDEX LOD%u: %u triangles | SYNTHETIC HEIGHTS | %s | 1/2/3 R P W",
                    static_cast<unsigned>(g_activeRealLOD),
                    static_cast<unsigned>(g_realLODMeshes[g_activeRealLOD].indexCount / 3u),
                    g_wireframe ? L"Wireframe" : L"Solid");
            SetWindowTitleWithCull(window, caption);
            return;
        }

        const wchar_t* caption = nullptr;
        if (g_patchPreview)
            caption = g_wireframe
                ? L"Metin2 DX11 | 17x17 Patch Preview | Wireframe | P / W"
                : L"Metin2 DX11 | 17x17 Patch Preview | Solid | P / W";
        else
            caption = g_wireframe
                ? L"Metin2 DX11 | Full Synthetic Terrain | Wireframe | P / W"
                : L"Metin2 DX11 | Full Synthetic Terrain | Solid | P / W";

        SetWindowTitleWithCull(window, caption);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message)
        {
        case WM_KEYDOWN:
            // Ignore auto-repeat; each physical key press changes mode once.
            if ((lparam & (1LL << 30)) != 0)
                return 0;
            if (wparam == 'W')
            {
                g_wireframe = !g_wireframe;
                UpdateWindowTitle(window);
            }
            else if (wparam == 'C')
            {
                g_cullingMode = (g_cullingMode + 1u) % 3u;
                UpdateWindowTitle(window);
            }
            else if (wparam == 'P')
            {
                g_patchPreview = !g_patchPreview;
                g_realLODPreview = false;
                UpdateWindowTitle(window);
            }
            else if (g_realLODLoaded && g_realPatchLoaded && wparam == 'V')
            {
                g_realPatchPreview = !g_realPatchPreview;
                g_realLODPreview = true;
                g_patchPreview = false;
                UpdateWindowTitle(window);
            }
            else if (g_realLODLoaded && wparam == 'R')
            {
                g_realLODPreview = !g_realLODPreview;
                if (g_realLODPreview)
                    g_patchPreview = false;
                UpdateWindowTitle(window);
            }
            else if (g_realLODLoaded && wparam >= '1' && wparam <= '3')
            {
                g_activeRealLOD = static_cast<std::size_t>(wparam - '1');
                g_realLODPreview = true;
                g_patchPreview = false;
                UpdateWindowTitle(window);
            }
            return 0;
        case WM_SIZE:
            if (g_ready && wparam != SIZE_MINIMIZED)
            {
                const UINT width = LOWORD(lparam);
                const UINT height = HIWORD(lparam);
                if (width && height)
                {
                    g_depth.Reset();
                    HRESULT hr = g_renderer.Resize(width, height);
                    if (SUCCEEDED(hr))
                        hr = g_depth.Create(g_renderer.NativeDevice(), width, height);
                    if (FAILED(hr))
                    {
                        LogFailure(L"terrain resize/depth", hr);
                        PostQuitMessage(4);
                    }
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    // The no-argument synthetic TerrainProbe is intentionally unchanged.
    // Real LOD is an explicit, read-only option with a quoted Unicode path.
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv)
        return 7;

    std::wstring snapshotPath;
    std::wstring realPatchPath;
    bool validArgs = (argc == 1 || argc == 3 || argc == 5);
    for (int i = 1; validArgs && i < argc; i += 2)
    {
        if (!argv[i + 1][0])
            validArgs = false;
        else if (std::wcscmp(argv[i], L"--lod-snapshot") == 0 &&
                 snapshotPath.empty())
            snapshotPath = argv[i + 1];
        else if (std::wcscmp(argv[i], L"--patch-snapshot") == 0 &&
                 realPatchPath.empty())
            realPatchPath = argv[i + 1];
        else
            validArgs = false;
    }
    ::LocalFree(argv);
    // The real patch must always be accompanied by a checked real LOD set;
    // a single bare vertex snapshot is not silently mixed with toy indices.
    if (!realPatchPath.empty() && snapshotPath.empty())
        validArgs = false;

    if (!validArgs)
    {
        MessageBoxW(nullptr,
            L"Usage: DX11TerrainProbe.exe [--lod-snapshot \"LOD.bin\"] [--patch-snapshot \"PATCH.bin\"]\n"
            L"A patch snapshot requires a LOD snapshot. Both files are read-only.",
            kTitle, MB_OK | MB_ICONERROR);
        return 7;
    }

    if (!snapshotPath.empty())
    {
        if (!LoadCaptureFile(snapshotPath, g_realSnapshot))
        {
            MessageBoxW(nullptr,
                L"Real LOD snapshot could not be opened or validated. No fallback to synthetic indices.",
                kTitle, MB_OK | MB_ICONERROR);
            return 8;
        }
        g_realLODLoaded = true;
        g_realLODPreview = true;
        g_wireframe = true;
    }
    if (!realPatchPath.empty())
    {
        if (!LoadPatchCaptureFile(realPatchPath, g_realPatchSnapshot))
        {
            MessageBoxW(nullptr,
                L"Real patch position/normal snapshot could not be opened or validated. No fallback.",
                kTitle, MB_OK | MB_ICONERROR);
            return 9;
        }
        g_realPatchLoaded = true;
        g_realPatchPreview = true;
    }

    if (!ValidateTerrainBuilder() || !ValidateLegacyIndexTopology() ||
        !ValidateCapturedPatchTransform())
    {
        MessageBoxW(nullptr, L"Terrain mesh / LOD topology CPU self-check failed.", kTitle,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc))
        return 2;

    HWND window = CreateWindowExW(
        0, kClassName, kTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1080, 680, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 3;

    ShowWindow(window, showCommand);
    RECT rect{};
    GetClientRect(window, &rect);
    const UINT width = static_cast<UINT>(rect.right - rect.left);
    const UINT height = static_cast<UINT>(rect.bottom - rect.top);

    HRESULT hr = g_renderer.Initialize(window, width, height);
    if (SUCCEEDED(hr))
        hr = g_depth.Create(g_renderer.NativeDevice(), width, height);
    if (SUCCEEDED(hr))
        hr = CreateTerrain();

    if (FAILED(hr))
    {
        LogFailure(L"terrain initialization", hr);
        ReleaseTerrain();
        g_renderer.Shutdown();
        MessageBoxW(window, L"Terrain DX11 initialization failed. Check debug output.",
                    kTitle, MB_OK | MB_ICONERROR);
        DestroyWindow(window);
        return 5;
    }

    g_ready = true;
    UpdateWindowTitle(window);
    MSG message{};
    int exitCode = 0;

    while (true)
    {
        bool quitting = false;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                exitCode = static_cast<int>(message.wParam);
                quitting = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (quitting)
            break;

        if (IsIconic(window))
        {
            WaitMessage();
            continue;
        }

        hr = DrawTerrain(window);
        if (FAILED(hr))
        {
            LogFailure(L"terrain frame", hr);
            if (hr == DXGI_ERROR_DEVICE_REMOVED)
                LogFailure(L"device removed reason", g_renderer.GetDeviceRemovedReason());
            exitCode = 6;
            break;
        }
    }

    g_ready = false;
    ReleaseTerrain();
    g_renderer.Shutdown();
    if (IsWindow(window))
        DestroyWindow(window);
    return exitCode;
}
