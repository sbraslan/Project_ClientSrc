#include "DX11RenderResources.h"

#include <cstring>
#include <limits>
#include <utility>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace m2::render
{
    HRESULT DX11RenderResources::CreateImmutableMesh(
        ID3D11Device* device,
        const void* vertexData,
        UINT vertexStride,
        UINT vertexCount,
        const std::uint16_t* indexData,
        UINT indexCount,
        DX11MeshBuffers& output) noexcept
    {
        if (!device || !vertexData || !indexData ||
            vertexStride == 0 || vertexCount == 0 || indexCount == 0)
            return E_INVALIDARG;

        constexpr UINT maxSize = (std::numeric_limits<UINT>::max)();
        if (vertexCount > maxSize / vertexStride ||
            indexCount > maxSize / static_cast<UINT>(sizeof(std::uint16_t)))
            return E_INVALIDARG;

        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = vertexCount * vertexStride;
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertexData;

        DX11MeshBuffers created{};
        HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &created.vertices);
        if (FAILED(hr))
            return hr;

        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = indexCount * static_cast<UINT>(sizeof(std::uint16_t));
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indexData;

        hr = device->CreateBuffer(&ibDesc, &ibData, &created.indices);
        if (FAILED(hr))
            return hr;

        created.stride = vertexStride;
        created.indexCount = indexCount;
        output = std::move(created);
        return S_OK;
    }

    HRESULT DX11RenderResources::CreateRGBA8Texture(
        ID3D11Device* device,
        UINT width,
        UINT height,
        const std::uint8_t* rgba,
        DX11Texture& output) noexcept
    {
        if (!device || !rgba || width == 0 || height == 0 ||
            width > (std::numeric_limits<UINT>::max)() / 4u)
            return E_INVALIDARG;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initial{};
        initial.pSysMem = rgba;
        initial.SysMemPitch = width * 4u;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&desc, &initial, &texture);
        if (FAILED(hr))
            return hr;

        DX11Texture created{};
        hr = device->CreateShaderResourceView(texture.Get(), nullptr, &created.view);
        if (FAILED(hr))
            return hr;

        output = std::move(created);
        return S_OK;
    }

    namespace
    {
        HRESULT CompileStage(
            const char* source,
            const char* entry,
            const char* profile,
            Microsoft::WRL::ComPtr<ID3DBlob>& compiled) noexcept
        {
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            const HRESULT hr = D3DCompile(
                source, std::strlen(source), "Metin2DX11ResourceShader",
                nullptr, nullptr, entry, profile, flags, 0,
                &compiled, &errors);

            if (errors)
                OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));

            return hr;
        }
    }

    HRESULT DX11RenderResources::CreateShaderPipeline(
        ID3D11Device* device,
        const char* hlslSource,
        const char* vsEntry,
        const char* psEntry,
        const D3D11_INPUT_ELEMENT_DESC* elements,
        UINT elementCount,
        DX11ShaderPipeline& output) noexcept
    {
        if (!device || !hlslSource || !vsEntry || !psEntry ||
            !elements || elementCount == 0)
            return E_INVALIDARG;

        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
        HRESULT hr = CompileStage(hlslSource, vsEntry, "vs_5_0", vsBlob);
        if (FAILED(hr))
            return hr;

        hr = CompileStage(hlslSource, psEntry, "ps_5_0", psBlob);
        if (FAILED(hr))
            return hr;

        DX11ShaderPipeline created{};
        hr = device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            nullptr, &created.vertex);
        if (FAILED(hr))
            return hr;

        hr = device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
            nullptr, &created.pixel);
        if (FAILED(hr))
            return hr;

        hr = device->CreateInputLayout(
            elements, elementCount,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &created.inputLayout);
        if (FAILED(hr))
            return hr;

        output = std::move(created);
        return S_OK;
    }

    HRESULT DX11RenderResources::CreatePointSampler(
        ID3D11Device* device,
        Microsoft::WRL::ComPtr<ID3D11SamplerState>& output) noexcept
    {
        if (!device)
            return E_INVALIDARG;

        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        Microsoft::WRL::ComPtr<ID3D11SamplerState> created;
        const HRESULT hr = device->CreateSamplerState(&desc, &created);
        if (SUCCEEDED(hr))
            output = std::move(created);
        return hr;
    }
}
