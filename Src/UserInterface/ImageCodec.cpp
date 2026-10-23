#include "StdAfx.h"
#include "ImageCodec.h"

#if !defined(_WIN64)
#error "ImageCodec.cpp is only used by the native Win64 client."
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

namespace
{
void ConvertRgbaToBgra(const stbi_uc* source, size_t pixelCount, std::vector<uint8_t>* target)
{
    target->resize(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const size_t offset = i * 4;
        (*target)[offset + 0] = source[offset + 2];
        (*target)[offset + 1] = source[offset + 1];
        (*target)[offset + 2] = source[offset + 0];
        (*target)[offset + 3] = source[offset + 3];
    }
}

void ConvertBgraToRgba(const uint8_t* source, size_t pixelCount, std::vector<uint8_t>* target)
{
    target->resize(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const size_t offset = i * 4;
        (*target)[offset + 0] = source[offset + 2];
        (*target)[offset + 1] = source[offset + 1];
        (*target)[offset + 2] = source[offset + 0];
        (*target)[offset + 3] = source[offset + 3];
    }
}
}

namespace ImageCodec
{
bool GetInfo(const char* fileName, int* width, int* height)
{
    if (!fileName || !width || !height)
        return false;

    int components = 0;
    return stbi_info(fileName, width, height, &components) != 0;
}

bool LoadBGRA(const char* fileName, int* width, int* height, std::vector<uint8_t>* pixels)
{
    if (!fileName || !width || !height || !pixels)
        return false;

    int components = 0;
    stbi_uc* rgba = stbi_load(fileName, width, height, &components, STBI_rgb_alpha);
    if (!rgba)
        return false;

    const size_t pixelCount = static_cast<size_t>(*width) * static_cast<size_t>(*height);
    ConvertRgbaToBgra(rgba, pixelCount, pixels);
    stbi_image_free(rgba);
    return true;
}

bool SaveTgaBGRA(const char* fileName, int width, int height, const void* pixels)
{
    if (!fileName || !pixels || width <= 0 || height <= 0)
        return false;

    std::vector<uint8_t> rgba;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    ConvertBgraToRgba(static_cast<const uint8_t*>(pixels), pixelCount, &rgba);
    return stbi_write_tga(fileName, width, height, 4, rgba.data()) != 0;
}
}
