#pragma once

// Standalone DX11 GPU resource utilities. This header is intentionally
// independent of Metin2's DX9 StdAfx and fixed-function state manager.
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>

namespace m2::render
{
    struct DX11MeshBuffers
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertices;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indices;
        UINT stride = 0;
        UINT indexCount = 0;

        void Reset() noexcept
        {
            vertices.Reset();
            indices.Reset();
            stride = 0;
            indexCount = 0;
        }
    };

    struct DX11Texture
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        void Reset() noexcept { view.Reset(); }
    };

    struct DX11ShaderPipeline
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

        void Reset() noexcept
        {
            inputLayout.Reset();
            pixel.Reset();
            vertex.Reset();
        }
    };

    class DX11RenderResources final
    {
    public:
        // Index type is uint16_t (matches the legacy Metin2 index width).
        // All allocations are immutable to start; dynamic streaming comes later.
        static HRESULT CreateImmutableMesh(
            ID3D11Device* device,
            const void* vertexData,
            UINT vertexStride,
            UINT vertexCount,
            const std::uint16_t* indexData,
            UINT indexCount,
            DX11MeshBuffers& output) noexcept;

        // Packed 8-bit RGBA rows, no row padding. Existing pack decoding is
        // intentionally NOT called or changed by this early test implementation.
        static HRESULT CreateRGBA8Texture(
            ID3D11Device* device,
            UINT width,
            UINT height,
            const std::uint8_t* rgba,
            DX11Texture& output) noexcept;

        // Compile source HLSL to Shader Model 5 bytecode and create an explicit
        // input layout. NOT compatible with DX9 .psc/.vsc shader binaries.
        static HRESULT CreateShaderPipeline(
            ID3D11Device* device,
            const char* hlslSource,
            const char* vsEntry,
            const char* psEntry,
            const D3D11_INPUT_ELEMENT_DESC* elements,
            UINT elementCount,
            DX11ShaderPipeline& output) noexcept;

        static HRESULT CreatePointSampler(
            ID3D11Device* device,
            Microsoft::WRL::ComPtr<ID3D11SamplerState>& output) noexcept;
    };
}
