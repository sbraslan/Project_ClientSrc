$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)

function ReadText([string]$Path) {
    [IO.File]::ReadAllText($Path)
}

function WriteText([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}

function ReplaceExact {
    param(
        [string]$File,
        [string]$Old,
        [string]$New
    )

    $path = Join-Path $Root $File
    $c = ReadText $path

    if ($c.Contains($Old)) {
        $c = $c.Replace($Old, $New)
        WriteText $path $c
        Write-Host "[OK] $File" -ForegroundColor Green
    }
    elseif ($c.Contains($New)) {
        Write-Host "[SKIP] $File" -ForegroundColor Yellow
    }
    else {
        Write-Host "[NOT FOUND] $File" -ForegroundColor DarkYellow
    }
}


# ============================================================
# 1. Legacy CRT compatibility helper
# ============================================================

$helper = @'
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
'@

$helperPath = Join-Path $Root "Src\EterBase\LegacyCRTCompat.h"
WriteText $helperPath $helper

Write-Host "[OK] LegacyCRTCompat.h created" -ForegroundColor Green


# ============================================================
# 2. Include helper through project PCH files
# ============================================================

$stdAfxFiles = Get-ChildItem `
    (Join-Path $Root "Src") `
    -Recurse `
    -File |
    Where-Object {
        $_.Name -ieq "StdAfx.h"
    }

foreach ($file in $stdAfxFiles) {

    $c = ReadText $file.FullName

    if ($c.Contains("LegacyCRTCompat.h")) {
        continue
    }

    $include = '#include "../EterBase/LegacyCRTCompat.h"'

    if ($c.Contains("#pragma once")) {
        $c = $c.Replace(
            "#pragma once",
            "#pragma once`r`n`r`n$include"
        )
    }
    else {
        $c = "$include`r`n$c"
    }

    WriteText $file.FullName $c
}

Write-Host "[OK] PCH includes" -ForegroundColor Green


# Headers that directly use helpers
$directHeaders = @(
    ".\Src\EterLib\Pool.h",
    ".\Src\EterPack\Inline.h",
    ".\Src\GameLib\ItemData.h",
    ".\Src\UserInterface\PythonGuild.h"
)

foreach ($relative in $directHeaders) {

    $path = Join-Path $Root $relative

    if (!(Test-Path $path)) {
        continue
    }

    $c = ReadText $path

    if ($c.Contains("LegacyCRTCompat.h")) {
        continue
    }

    if ($relative -like "*EterBase*") {
        $inc = '#include "LegacyCRTCompat.h"'
    }
    else {
        $inc = '#include "../EterBase/LegacyCRTCompat.h"'
    }

    if ($c.Contains("#pragma once")) {
        $c = $c.Replace(
            "#pragma once",
            "#pragma once`r`n`r`n$inc"
        )
    }
    else {
        $c = "$inc`r`n$c"
    }

    WriteText $path $c
}


# ============================================================
# 3. Pointer/offset special cases BEFORE bulk replacement
# ============================================================

ReplaceExact `
".\Src\EterBase\CPostIt.cpp" `
'strncpy(lpBuffer, lpszText, (nSize < strlen(lpszText)) ? nSize : strlen(lpszText));' `
'm2::crt::copy_n_s(lpBuffer, nSize, lpszText, (nSize < strlen(lpszText)) ? nSize : strlen(lpszText));'


ReplaceExact `
".\Src\EterBase\Utils.cpp" `
'strncpy(dir.data(), path, p - path);' `
'm2::crt::copy_n_s(dir.data(), dir.size(), path, static_cast<std::size_t>(p - path));'


ReplaceExact `
".\Src\UserInterface\PythonLocale.cpp" `
'strncpy(c, sFormat.c_str(), size);' `
'm2::crt::copy_n_s(c, size, sFormat.c_str(), size);'


ReplaceExact `
".\Src\GameLib\Property.cpp" `
'strcpy(szCRC + strlen(szCRC), c_pszFileName);' `
'm2::crt::append(szCRC, c_pszFileName);'


ReplaceExact `
".\Src\EterLib\Util.cpp" `
'strcpy(resultStr, "");' `
'resultStr[0] = ''\0'';'


ReplaceExact `
".\Src\EterLib\Util.cpp" `
'strcat(str, result);' `
'm2::crt::append_legacy(str, result);'


ReplaceExact `
".\Src\EterBase\Utils.cpp" `
'strcat(szName, Temp);' `
'm2::crt::append_legacy(szName, Temp);'


ReplaceExact `
".\Src\EterBase\Utils.cpp" `
'sprintf(szName + strlen(szName), "%u", Index);' `
@'
char indexBuffer[16];
m2::crt::format(indexBuffer, "%u", Index);
m2::crt::append_legacy(szName, indexBuffer);
'@


ReplaceExact `
".\Src\EterPythonLib\PythonGraphicModule.cpp" `
'sprintf(szPath + strlen(szPath),' `
'm2::crt::format_at(szPath, strlen(szPath),'


# This was a format-string vulnerability, not just C4996.
ReplaceExact `
".\Src\GameLib\ItemData.h" `
'void SetItemShiningTableData(uint8_t bIndex, const char* szEffectname) { sprintf(m_ItemShiningTable.szShinings[bIndex], szEffectname); }' `
'void SetItemShiningTableData(uint8_t bIndex, const char* szEffectname) { m2::crt::copy(m_ItemShiningTable.szShinings[bIndex], szEffectname); }'


# ============================================================
# 4. Bulk conversion of fixed-array CRT calls
# ============================================================

$sourceFiles = Get-ChildItem `
    (Join-Path $Root "Src") `
    -Recurse `
    -File `
    -Include *.cpp,*.cxx,*.cc,*.h,*.hpp,*.inl

$counts = @{
    strcpy  = 0
    strncpy = 0
    strcat  = 0
    sprintf = 0
    fopen   = 0
}

foreach ($file in $sourceFiles) {

    if ($file.FullName -eq $helperPath) {
        continue
    }

    $c = ReadText $file.FullName
    $original = $c

    # std::strcpy
    $n = ([regex]::Matches($c, 'std::strcpy\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            'std::strcpy\s*\(',
            'm2::crt::copy('
        )
        $counts.strcpy += $n
    }

    # strcpy
    $n = ([regex]::Matches($c, '(?<![\w:])strcpy\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            '(?<![\w:])strcpy\s*\(',
            'm2::crt::copy('
        )
        $counts.strcpy += $n
    }

    # strncpy - preserves old fixed-count semantics
    $n = ([regex]::Matches($c, '(?<![\w:])strncpy\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            '(?<![\w:])strncpy\s*\(',
            'm2::crt::copy_n('
        )
        $counts.strncpy += $n
    }

    # strcat
    $n = ([regex]::Matches($c, '(?<![\w:])strcat\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            '(?<![\w:])strcat\s*\(',
            'm2::crt::append('
        )
        $counts.strcat += $n
    }

    # sprintf
    $n = ([regex]::Matches($c, '(?<![\w:])sprintf\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            '(?<![\w:])sprintf\s*\(',
            'm2::crt::format('
        )
        $counts.sprintf += $n
    }

    # fopen
    $n = ([regex]::Matches($c, '(?<![\w:])fopen\s*\(')).Count
    if ($n) {
        $c = [regex]::Replace(
            $c,
            '(?<![\w:])fopen\s*\(',
            'm2::crt::open_file('
        )
        $counts.fopen += $n
    }

    if ($c -ne $original) {
        WriteText $file.FullName $c
    }
}


# ============================================================
# 5. sscanf -> sscanf_s
# Exact parser conversions with explicit string capacities
# ============================================================

ReplaceExact `
".\Src\EterLib\GrpDevice.cpp" `
'sscanf(szLine, "%s %x %x", szChkDriver, &dwChkHighVersion, &dwChkLowVersion);' `
'sscanf_s(szLine, "%255s %x %x", szChkDriver, static_cast<unsigned>(_countof(szChkDriver)), &dwChkHighVersion, &dwChkLowVersion);'


ReplaceExact `
".\Src\GameLib\MapManager.cpp" `
'sscanf(c_rstLine.c_str(), "%s %d %d %d %d", szMapName, &x, &y, &width, &height);' `
'sscanf_s(c_rstLine.c_str(), "%255s %d %d %d %d", szMapName, static_cast<unsigned>(_countof(szMapName)), &x, &y, &width, &height);'


ReplaceExact `
".\Src\GameLib\MapOutdoorLoad.cpp" `
'sscanf(c_rstLine.c_str(), "%s %d", szSungmaName, &iSungmaAttr);' `
'sscanf_s(c_rstLine.c_str(), "%255s %d", szSungmaName, static_cast<unsigned>(_countof(szSungmaName)), &iSungmaAttr);'


ReplaceExact `
".\Src\GameLib\RaceManager.cpp" `
'sscanf(c_rstLine.c_str(), "%s %s %s %d", szMode, szType, szFile, &nPercent);' `
'sscanf_s(c_rstLine.c_str(), "%255s %255s %255s %d", szMode, static_cast<unsigned>(_countof(szMode)), szType, static_cast<unsigned>(_countof(szType)), szFile, static_cast<unsigned>(_countof(szFile)), &nPercent);'


ReplaceExact `
".\Src\UserInterface\Locale.cpp" `
'sscanf(line, "%d %s", &code, name);' `
'sscanf_s(line, "%d %255s", &code, name, static_cast<unsigned>(_countof(name)));'


ReplaceExact `
".\Src\UserInterface\Locale.cpp" `
'sscanf(line, "%d %d %s", &id, &code, name);' `
'sscanf_s(line, "%d %d %255s", &id, &code, name, static_cast<unsigned>(_countof(name)));'


ReplaceExact `
".\Src\UserInterface\MarkManager.cpp" `
'sscanf(line, "%u %u", &guildID, &markID);' `
'sscanf_s(line, "%u %u", &guildID, &markID);'


ReplaceExact `
".\Src\UserInterface\PythonSystem.cpp" `
'if (sscanf(buf, " %s %s\n", command, value) == EOF)' `
'if (sscanf_s(buf, " %255s %255s\n", command, static_cast<unsigned>(_countof(command)), value, static_cast<unsigned>(_countof(value))) == EOF)'


# ============================================================
# 6. Final scan
# ============================================================

Write-Host ""
Write-Host "===== CONVERTED =====" -ForegroundColor Cyan
Write-Host "strcpy : $($counts.strcpy)"
Write-Host "strncpy: $($counts.strncpy)"
Write-Host "strcat : $($counts.strcat)"
Write-Host "sprintf: $($counts.sprintf)"
Write-Host "fopen  : $($counts.fopen)"


$remaining = Get-ChildItem `
    (Join-Path $Root "Src") `
    -Recurse `
    -File `
    -Include *.cpp,*.c,*.h,*.hpp,*.inl |
    Select-String -Pattern `
        "(?<![\w:])strcpy\s*\(",
        "(?<![\w:])strncpy\s*\(",
        "(?<![\w:])strcat\s*\(",
        "(?<![\w:])sprintf\s*\(",
        "(?<![\w:])fopen\s*\(",
        "(?<![\w:])sscanf\s*\(",
        "_snprintf\s*\(",
        "_vsnprintf\s*\(",
        "_strupr\s*\("


$remaining |
    ForEach-Object {
        "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()
    } |
    Set-Content (Join-Path $Root "C4996_FINAL_REMAINING.txt")


Write-Host ""
Write-Host "Remaining deprecated CRT calls: $($remaining.Count)" -ForegroundColor Yellow

Write-Host ""
git diff --check

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host " C4996 FINAL MIGRATION PASS TAMAMLANDI" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green