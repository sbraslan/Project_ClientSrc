#pragma once

#include <cstdint>
#include <vector>

namespace ImageCodec
{
bool GetInfo(const char* fileName, int* width, int* height);
bool LoadBGRA(const char* fileName, int* width, int* height, std::vector<uint8_t>* pixels);
bool SaveTgaBGRA(const char* fileName, int width, int height, const void* pixels);
}
