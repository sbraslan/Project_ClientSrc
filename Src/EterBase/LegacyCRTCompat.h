#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace m2
{
namespace crt
{

template <std::size_t N>
inline void copy_n(char (&dst)[N], const char* src, std::size_t count) noexcept
{
    const std::size_t limit = count < N ? count : N;

    if (!src)
    {
        if (limit)
            std::memset(dst, 0, limit);
        return;
    }

    std::size_t i = 0;

    while (i < limit && src[i] != '\0')
    {
        dst[i] = src[i];
        ++i;
    }

    if (i < limit)
        std::memset(dst + i, 0, limit - i);
}

inline void copy_n_s(
    char* dst,
    std::size_t dstSize,
    const char* src,
    std::size_t count) noexcept
{
    if (!dst || dstSize == 0)
        return;

    const std::size_t limit = count < dstSize ? count : dstSize;

    if (!src)
    {
        if (limit)
            std::memset(dst, 0, limit);
        return;
    }

    std::size_t i = 0;

    while (i < limit && src[i] != '\0')
    {
        dst[i] = src[i];
        ++i;
    }

    if (i < limit)
        std::memset(dst + i, 0, limit - i);
}

template <std::size_t N>
inline void copy(char (&dst)[N], const char* src) noexcept
{
    if (!src)
    {
        dst[0] = '\0';
        return;
    }

    const std::size_t srcLen = std::strlen(src);
    const std::size_t copyLen =
        srcLen < N - 1 ? srcLen : N - 1;

    if (copyLen)
        std::memcpy(dst, src, copyLen);

    dst[copyLen] = '\0';
}

template <std::size_t N>
inline void append(char (&dst)[N], const char* src) noexcept
{
    if (!src)
        return;

    std::size_t dstLen = 0;

    while (dstLen < N && dst[dstLen] != '\0')
        ++dstLen;

    if (dstLen >= N)
    {
        dst[N - 1] = '\0';
        return;
    }

    std::size_t i = 0;

    while (dstLen + i + 1 < N && src[i] != '\0')
    {
        dst[dstLen + i] = src[i];
        ++i;
    }

    dst[dstLen + i] = '\0';
}

/*
    Legacy pointer-only API.

    Used only where the original function does not expose destination
    capacity. This preserves the old caller contract without using
    deprecated CRT functions.
*/
inline void append_legacy(char* dst, const char* src) noexcept
{
    if (!dst || !src)
        return;

    const std::size_t dstLen = std::strlen(dst);
    const std::size_t srcLen = std::strlen(src);

    std::memcpy(dst + dstLen, src, srcLen + 1);
}

template <std::size_t N, typename... Args>
inline int format(
    char (&dst)[N],
    const char* fmt,
    Args... args) noexcept
{
    const int result =
        std::snprintf(dst, N, fmt, args...);

    dst[N - 1] = '\0';

    return result;
}

template <std::size_t N, typename... Args>
inline int format_at(
    char (&dst)[N],
    std::size_t offset,
    const char* fmt,
    Args... args) noexcept
{
    if (offset >= N)
        return -1;

    const int result =
        std::snprintf(
            dst + offset,
            N - offset,
            fmt,
            args...
        );

    dst[N - 1] = '\0';

    return result;
}

inline FILE* open_file(
    const char* filename,
    const char* mode) noexcept
{
    FILE* fp = nullptr;

    if (fopen_s(&fp, filename, mode) != 0)
        return nullptr;

    return fp;
}

}
}