$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot

function ReadText([string]$Path)
{
    return [System.IO.File]::ReadAllText($Path)
}

function WriteText([string]$Path, [string]$Text)
{
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Patch
{
    param(
        [string]$Relative,
        [string]$Old,
        [string]$New
    )

    $Path = Join-Path $Root $Relative

    if (!(Test-Path $Path))
    {
        Write-Host "[MISSING] $Relative" -ForegroundColor Red
        return
    }

    $Text = ReadText $Path

    if ($Text.Contains($Old))
    {
        $Text = $Text.Replace($Old, $New)
        WriteText $Path $Text
        Write-Host "[OK] $Relative" -ForegroundColor Green
    }
    elseif ($Text.Contains($New))
    {
        Write-Host "[SKIP] $Relative zaten duzeltilmis" -ForegroundColor Yellow
    }
    else
    {
        Write-Host "[NOT FOUND] $Relative" -ForegroundColor DarkYellow
    }
}

function EnsureInclude
{
    param(
        [string]$Relative,
        [string]$Include
    )

    $Path = Join-Path $Root $Relative

    if (!(Test-Path $Path))
    {
        return
    }

    $Text = ReadText $Path

    if ($Text.Contains($Include))
    {
        return
    }

    if ($Text.Contains('#include "StdAfx.h"'))
    {
        $Text = $Text.Replace(
            '#include "StdAfx.h"',
            "#include `"StdAfx.h`"`r`n$Include"
        )
    }
    elseif ($Text.Contains('#include "Stdafx.h"'))
    {
        $Text = $Text.Replace(
            '#include "Stdafx.h"',
            "#include `"Stdafx.h`"`r`n$Include"
        )
    }
    else
    {
        $Text = "$Include`r`n$Text"
    }

    WriteText $Path $Text
}

Write-Host ""
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " METIN2 CLIENT x64 - FINAL ZERO WARNING PASS" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan


# ============================================================
# 0. BUILD BLOCKER - PythonChat C3892
# ============================================================

$rel = "Src\UserInterface\PythonChat.cpp"

Patch $rel `
'const int iLineIndex = m2::narrow<int>(pLineList->size());' `
'int iLineIndex = m2::narrow<int>(pLineList->size());'


# ============================================================
# 1. IME.cpp - kalan 2 warning
# ============================================================

$rel = "Src\EterLib\IME.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'WideCharToMultiByte(ms_uOutputCodePage, 0, str.c_str(), str.length(), nullptr, 0, nullptr, nullptr)' `
'WideCharToMultiByte(ms_uOutputCodePage, 0, str.c_str(), m2::narrow<int>(str.length()), nullptr, 0, nullptr, nullptr)'

Patch $rel `
'WideCharToMultiByte(ms_uOutputCodePage, 0, input.c_str(), input.length(), nullptr, 0, nullptr, nullptr)' `
'WideCharToMultiByte(ms_uOutputCodePage, 0, input.c_str(), m2::narrow<int>(input.length()), nullptr, 0, nullptr, nullptr)'


# ============================================================
# 2. GrpTextInstance - kalan 1 warning
# ============================================================

$rel = "Src\EterLib\GrpTextInstance.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'int wTextMax = (end - begin) * 2;' `
'const int wTextMax = m2::narrow<int>(m_stText.length() * 2);'

Patch $rel `
'MultiByteToWideChar(dataCodePage, 0, begin, token - begin, wText, wTextMax)' `
'MultiByteToWideChar(dataCodePage, 0, begin, m2::narrow<int>(token - begin), wText, wTextMax)'


# ============================================================
# 3. ItemData - kalan 2 warning
# ============================================================

$rel = "Src\GameLib\ItemData.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_pLODModelThingVector.size();' `
'return m2::narrow<uint32_t>(m_pLODModelThingVector.size());'

Patch $rel `
'return m_AttachingDataVector.size();' `
'return m2::narrow<uint32_t>(m_AttachingDataVector.size());'


# ============================================================
# 4. Granny Thing.cpp - size_t -> int
# ============================================================

$rel = "Src\EterGrnLib\Thing.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$old = @'
		int sepPos = fileName.rfind('\\');
		gs_modelLocalPath.assign(fileName, 0, sepPos + 1);
'@

$new = @'
		const auto sepPos = fileName.rfind('\\');

		if (sepPos != std::string::npos)
			gs_modelLocalPath.assign(fileName, 0, sepPos + 1);
		else
			gs_modelLocalPath.clear();
'@

if ($text.Contains($old))
{
    $text = $text.Replace($old, $new)
    WriteText $path $text
    Write-Host "[OK] Thing.cpp" -ForegroundColor Green
}
elseif ($text.Contains('const auto sepPos = fileName.rfind'))
{
    Write-Host "[SKIP] Thing.cpp" -ForegroundColor Yellow
}
else
{
    Write-Host "[NOT FOUND] Thing.cpp" -ForegroundColor DarkYellow
}


# ============================================================
# 5. CRC32
# ============================================================

$rel = "Src\EterBase\CRC32.cpp"

Patch $rel `
'int lenSt = strlen(rez);' `
'const size_t lenSt = strlen(rez);'


# ============================================================
# 6. Stl.cpp
# ============================================================

$rel = "Src\EterBase\Stl.cpp"

Patch $rel `
'uint32_t offset = positions[i - 1] + sizeS2;' `
'const std::string::size_type offset = positions[i - 1] + sizeS2;'


# ============================================================
# 7. FileDir.cpp
# ============================================================

$rel = "Src\EterBase\FileDir.cpp"

Patch $rel `
'int iPos = strFilter.find_first_of('';'', 0);' `
'const auto iPos = strFilter.find_first_of('';'', 0);'

Patch $rel `
'if (iPos > 0)' `
'if (iPos != std::string::npos && iPos > 0)'


# ============================================================
# 8. Poly.cpp
# ============================================================

$rel = "Src\EterBase\poly\Poly.cpp"

if (!(Test-Path (Join-Path $Root $rel)))
{
    $rel = "Src\EterBase\Poly\Poly.cpp"
}

EnsureInclude $rel '#include "../X64Compat.h"'

Patch $rel `
'return lSymbol.size() - MathSymbolCount;' `
'return m2::narrow<int>(lSymbol.size() - MathSymbolCount);'


# ============================================================
# 9. SpeedTree BoundaryShapeManager - 2 warning
# ============================================================

$rel = "Src\SpeedTreeLib\BoundaryShapeManager.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'for (uint32_t i = 0, j = m_vContour.size() - 1; i < m_vContour.size(); j = i++)' `
'for (std::size_t i = 0, j = m_vContour.size() - 1; i < m_vContour.size(); j = i++)'

Patch $rel `
'int nIndex = random_range(0, m_vBoundaries.size() - 1);' `
'const int nIndex = random_range(0, m2::narrow<long>(m_vBoundaries.size() - 1));'


# ============================================================
# 10. PRTerrain TextureSet - 2 warning
# ============================================================

$rel = "Src\PRTerrainLib\TextureSet.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_Textures.size();' `
'return m2::narrow<unsigned long>(m_Textures.size());'

Patch $rel `
'SetTexture(m_Textures.size() - 1, c_szFileName,' `
'SetTexture(m2::narrow<unsigned long>(m_Textures.size() - 1), c_szFileName,'


# ============================================================
# 11. EterPack.cpp - 2 warning
# ============================================================

$rel = "Src\EterPack\EterPack.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'__Write(c_szBuf, strlen(c_szBuf) + 1);' `
'__Write(c_szBuf, m2::narrow<int>(strlen(c_szBuf) + 1));'

Patch $rel `
'__WriteError(c_szBuf, strlen(c_szBuf) + 1);' `
'__WriteError(c_szBuf, m2::narrow<int>(strlen(c_szBuf) + 1));'


# ============================================================
# 12. HybridCrypt - kalan warning
# ============================================================

$rel = "Src\EterPack\EterPackPolicy_CSHybridCrypt.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'GetCRC32(fileNamelower.c_str(), fileNamelower.size())' `
'GetCRC32(fileNamelower.c_str(), m2::narrow<uint32_t>(fileNamelower.size()))'


# ============================================================
# 13. EffectMeshInstance - kalan warning
# ============================================================

$rel = "Src\EffectLib\EffectMeshInstance.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'rFrameController.SetMaxFrame(rTextureVector.size());' `
'rFrameController.SetMaxFrame(m2::narrow<uint32_t>(rTextureVector.size()));'


# ============================================================
# 14. miniaudio vendor warning
#
# Bu dosya C-compatible third-party kod.
# Warning satiri ma_uint64 -> ma_uint32 assignment.
# Sadece compiler'in raporladigi TEK satira intentional
# ma_uint32 conversion ekliyoruz.
# ============================================================

$mini = Join-Path $Root "Extern\miniaudio\miniaudio.h"

if (Test-Path $mini)
{
    $lines = [System.IO.File]::ReadAllLines($mini)

    $index = 80532 - 1

    if ($index -lt $lines.Length)
    {
        $line = $lines[$index]

        Write-Host "[INFO] miniaudio 80532 BEFORE:" -ForegroundColor Cyan
        Write-Host $line

        if ($line -notmatch '\(ma_uint32\)')
        {
            if ($line -match '^(\s*[^=]+?=\s*)([^;]+)(;.*)$')
            {
                $left  = $matches[1]
                $value = $matches[2]
                $tail  = $matches[3]

                $lines[$index] =
                    $left + "(ma_uint32)(" + $value.Trim() + ")" + $tail

                $utf8 = New-Object System.Text.UTF8Encoding($false)
                [System.IO.File]::WriteAllLines(
                    $mini,
                    $lines,
                    $utf8
                )

                Write-Host "[OK] miniaudio C4244 intentional ma_uint32 conversion" -ForegroundColor Green
                Write-Host "[INFO] miniaudio 80532 AFTER:" -ForegroundColor Cyan
                Write-Host $lines[$index]
            }
            else
            {
                Write-Host "[FAIL] miniaudio 80532 assignment format taninamadi" -ForegroundColor Red
            }
        }
        else
        {
            Write-Host "[SKIP] miniaudio zaten duzeltilmis" -ForegroundColor Yellow
        }
    }
    else
    {
        Write-Host "[FAIL] miniaudio 80532 bulunamadi" -ForegroundColor Red
    }
}
else
{
    Write-Host "[MISSING] Extern\miniaudio\miniaudio.h" -ForegroundColor Red
}


# ============================================================
# FINAL SANITY
# ============================================================

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " FINAL SANITY CHECK" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

$chat = ReadText (Join-Path $Root "Src\UserInterface\PythonChat.cpp")

if ($chat.Contains(
    'int iLineIndex = m2::narrow<int>(pLineList->size());'
))
{
    Write-Host "[PASS] PythonChat C3892" -ForegroundColor Green
}
else
{
    Write-Host "[FAIL] PythonChat C3892" -ForegroundColor Red
}

Write-Host ""
git diff --check

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host " FINAL X64 PATCH TAMAMLANDI" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green