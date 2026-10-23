$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Src  = Join-Path $Root "Src"

function ReadText([string]$Path)
{
    [System.IO.File]::ReadAllText($Path)
}

function WriteText([string]$Path, [string]$Text)
{
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Patch
{
    param(
        [string]$RelativePath,
        [string]$Old,
        [string]$New
    )

    $Path = Join-Path $Root $RelativePath

    if (!(Test-Path $Path))
    {
        Write-Host "[MISSING] $RelativePath" -ForegroundColor Red
        return
    }

    $Text = ReadText $Path

    if ($Text.Contains($Old))
    {
        $Text = $Text.Replace($Old, $New)
        WriteText $Path $Text
        Write-Host "[OK] $RelativePath" -ForegroundColor Green
    }
    elseif ($Text.Contains($New))
    {
        Write-Host "[SKIP] $RelativePath zaten duzeltilmis" -ForegroundColor Yellow
    }
    else
    {
        Write-Host "[NOT FOUND] $RelativePath -> $Old" -ForegroundColor DarkYellow
    }
}

function AddInclude
{
    param(
        [string]$RelativePath,
        [string]$Include
    )

    $Path = Join-Path $Root $RelativePath
    $Text = ReadText $Path

    if ($Text.Contains($Include))
    {
        return
    }

    if ($Text.Contains("#pragma once"))
    {
        $Text = $Text.Replace(
            "#pragma once",
            "#pragma once`r`n`r`n$Include"
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
Write-Host " METIN2 X64 HARDENING PHASE 3" -ForegroundColor Cyan
Write-Host " C4267 / C4477 / C4838 / C5055 / C4312" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan


# ============================================================
# 1. Kalan tek C4312
# PythonGraphicImageModule
# ============================================================

$rel = "Src\EterPythonLib\PythonGraphicImageModule.cpp"

Patch $rel `
'int iHandle;' `
'CGraphicImage * pImage = nullptr;'

Patch $rel `
'if (!PyTuple_GetInteger(poArgs, 0, &iHandle))' `
'if (!PyTuple_GetPointer(poArgs, 0, &pImage))'

Patch $rel `
'pImageInstance->SetImagePointer(reinterpret_cast<CGraphicImage *>(iHandle));' `
'pImageInstance->SetImagePointer(pImage);'

Patch $rel `
'return Py_BuildValue("i", pImageInstance);' `
'return Py_BuildPointer(pImageInstance);'


# ============================================================
# 2. FileLoader.cpp - yoğun C4267 grubu
# ============================================================

$rel = "Src\EterBase\FileLoader.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

if (!$text.Contains('#include "X64Compat.h"'))
{
    $text = $text.Replace(
        '#include "FileLoader.h"',
        "#include `"FileLoader.h`"`r`n#include `"X64Compat.h`""
    )
}

$text = $text.Replace(
    'const int c_iLineLength = c_rstLine.length();',
    'const std::string::size_type c_iLineLength = c_rstLine.length();'
)

$text = $text.Replace(
    'int basePos = 0;',
    'std::string::size_type basePos = 0;'
)

$text = $text.Replace(
    'uint32_t basePos = 0;',
    'std::string::size_type basePos = 0;'
)

$text = $text.Replace(
    "int beginPos = c_rstLine.find_first_of('\t', basePos);",
    "const auto beginPos = c_rstLine.find_first_of('\t', basePos);"
)

$text = $text.Replace(
    'int beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);',
    'const auto beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);'
)

$text = $text.Replace(
    'if (beginPos < 0)',
    'if (beginPos == std::string::npos)'
)

$text = $text.Replace(
    'int endPos;',
    'std::string::size_type endPos;'
)

$text = $text.Replace(
    'if (endPos < 0)',
    'if (endPos == std::string::npos)'
)

$text = $text.Replace(
    'if (int(c_rstLine.find_first_not_of(c_szDelimeter, basePos)) < 0)',
    'if (c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)'
)

$text = $text.Replace(
    'return m_stLineVector.size();',
    'return m2::narrow<uint32_t>(m_stLineVector.size());'
)

$text = $text.Replace(
    'int ret = fread(pvDst, size, 1, m_fp);',
    'const size_t ret = fread(pvDst, size, 1, m_fp);'
)

WriteText $path $text

Write-Host "[OK] FileLoader.cpp x64 string/index cleanup" -ForegroundColor Green


# ============================================================
# 3. PythonSkill.cpp - 13 adet C4267
# ============================================================

$rel = "Src\UserInterface\PythonSkill.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'uint32_t basePos = 0;',
    'std::string::size_type basePos = 0;'
)

$text = $text.Replace(
    'int beginPos = stLine.find_first_not_of(c_szDelimeter, basePos);',
    'const auto beginPos = stLine.find_first_not_of(c_szDelimeter, basePos);'
)

$text = $text.Replace(
    'if (beginPos < 0)',
    'if (beginPos == std::string::npos)'
)

$text = $text.Replace(
    'int endPos;',
    'std::string::size_type endPos;'
)

$text = $text.Replace(
    'if (endPos < 0)',
    'if (endPos == std::string::npos)'
)

$text = $text.Replace(
    'if (int(stLine.find_first_not_of(c_szDelimeter, basePos)) < 0)',
    'if (stLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)'
)

$text = $text.Replace(
    'void string_replace_word(const char * base, int base_len, const char * src, int src_len, const char * dst, int dst_len,',
    'void string_replace_word(const char * base, std::size_t base_len, const char * src, std::size_t src_len, const char * dst, std::size_t dst_len,'
)

$text = $text.Replace(
    'int prev = 0;',
    'std::size_t prev = 0;'
)

$text = $text.Replace(
    'int cur = 0;',
    'std::size_t cur = 0;'
)

$text = $text.Replace(
    'uint32_t dwSize = pConditionDataVector->size();',
    'const std::size_t dwSize = pConditionDataVector->size();'
)

$text = $text.Replace(
    'uint32_t dwSize = pAffectDataVector->size() / 3;',
    'const std::size_t dwSize = pAffectDataVector->size() / 3;'
)

$text = $text.Replace(
    'for (uint32_t i = 0; i < dwSize; ++i)',
    'for (std::size_t i = 0; i < dwSize; ++i)'
)

$text = $text.Replace(
    'int first = strDescription.find("%.0f");',
    'const auto first = strDescription.find("%.0f");'
)

$text = $text.Replace(
    'if (first >= 0)',
    'if (first != std::string::npos)'
)

$text = $text.Replace(
    'int second = strDescription.find("%.0f", first);',
    'const auto second = strDescription.find("%.0f", first);'
)

$text = $text.Replace(
    'if (second >= 0)',
    'if (second != std::string::npos)'
)

WriteText $path $text

Write-Host "[OK] PythonSkill.cpp size_t cleanup" -ForegroundColor Green


# ============================================================
# 4. InstanceBase - string length ve refine size_t
# ============================================================

$rel = "Src\UserInterface\InstanceBase.h"

AddInclude $rel '#include <cstddef>'

Patch $rel `
'void SetNameString(const char * c_szName, int len);' `
'void SetNameString(const char * c_szName, std::size_t len);'


$rel = "Src\UserInterface\InstanceBase.cpp"

Patch $rel `
'void CInstanceBase::SetNameString(const char * c_szName, int len)' `
'void CInstanceBase::SetNameString(const char * c_szName, std::size_t len)'

Patch $rel `
'int iFindingPos = strName.find_first_of('' '', 0);' `
'const auto iFindingPos = strName.find_first_of('' '', 0);'

Patch $rel `
'if (iFindingPos > 0)' `
'if (iFindingPos != std::string::npos && iFindingPos > 0)'


$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'auto refine = std::max<size_t>(pItem->GetRefine() + pItem->GetSocketCount(), METIN_SOCKET_MAX_NUM) - METIN_SOCKET_MAX_NUM;',
    'const uint32_t refine = std::max<uint32_t>(pItem->GetRefine() + static_cast<uint32_t>(pItem->GetSocketCount()), static_cast<uint32_t>(METIN_SOCKET_MAX_NUM)) - static_cast<uint32_t>(METIN_SOCKET_MAX_NUM);'
)

$text = $text.Replace(
    'auto refine = std::max<size_t>(pItem->GetRefine() + pItem->GetSocketCount(), CItemData::ITEM_SOCKET_MAX_NUM) - CItemData::ITEM_SOCKET_MAX_NUM;',
    'const uint32_t refine = std::max<uint32_t>(pItem->GetRefine() + static_cast<uint32_t>(pItem->GetSocketCount()), static_cast<uint32_t>(CItemData::ITEM_SOCKET_MAX_NUM)) - static_cast<uint32_t>(CItemData::ITEM_SOCKET_MAX_NUM);'
)

WriteText $path $text

Write-Host "[OK] InstanceBase.cpp refine/string width" -ForegroundColor Green


# ============================================================
# 5. PythonEventManager - 7 C4267
# ============================================================

$rel = "Src\UserInterface\PythonEventManager.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

if (!$text.Contains('../EterBase/X64Compat.h'))
{
    $text = $text.Replace(
        '#include "StdAfx.h"',
        "#include `"StdAfx.h`"`r`n#include `"../EterBase/X64Compat.h`""
    )
}

$text = $text.Replace(
    'for (uint32_t i = 0; i < m_EventSetVector.size(); ++i)',
    'for (std::size_t i = 0; i < m_EventSetVector.size(); ++i)'
)

$text = $text.Replace(
    'return i;',
    'return m2::narrow<uint32_t>(i);'
)

$text = $text.Replace(
    'return m_EventSetVector.size() - 1;',
    'return m2::narrow<uint32_t>(m_EventSetVector.size() - 1);'
)

$text = $text.Replace(
    'pEventSet->iCurrentLetter += c_rstValue.length();',
    'pEventSet->iCurrentLetter += m2::narrow<int>(c_rstValue.length());'
)

$text = $text.Replace(
    'pEventSet->iCurrentLetter += strlen(pItemData->GetName());',
    'pEventSet->iCurrentLetter += m2::narrow<int>(strlen(pItemData->GetName()));'
)

$text = $text.Replace(
    'pEventSet->iCurrentLetter += strlen(c_szName);',
    'pEventSet->iCurrentLetter += m2::narrow<int>(strlen(c_szName));'
)

$text = $text.Replace(
    'Py_BuildValue("(i)", rArgumentList.size())',
    'Py_BuildValue("(i)", m2::narrow<int>(rArgumentList.size()))'
)

$text = $text.Replace(
    'pEventSet->nAnswer = rArgumentList.size();',
    'pEventSet->nAnswer = m2::narrow<int>(rArgumentList.size());'
)

$text = $text.Replace(
    'return pEventSet->ScriptTextLineList.size();',
    'return m2::narrow<int>(pEventSet->ScriptTextLineList.size());'
)

$text = $text.Replace(
    'return pEventSet->ScriptTextLineList.size() + pEventSet->iAdjustLine;',
    'return m2::narrow<int>(pEventSet->ScriptTextLineList.size()) + pEventSet->iAdjustLine;'
)

WriteText $path $text

Write-Host "[OK] PythonEventManager.cpp" -ForegroundColor Green


# ============================================================
# 6. Bütün C4477 format warningleri
# ============================================================

Patch "Src\EterLib\JpegFile.cpp" `
'len = destlen - dmgr->free_in_buffer;' `
'len = destlen - static_cast<int>(dmgr->free_in_buffer);'

Patch "Src\EterLib\JpegFile.cpp" `
'printf("fill %u\n", sizeX - mgr->bytes_in_buffer);' `
'printf("fill %zu\n", static_cast<size_t>(sizeX) - mgr->bytes_in_buffer);'

Patch "Src\EterLib\JpegFile.cpp" `
'printf("skip %u +%ld\n", sizeX - mgr->bytes_in_buffer, num_bytes);' `
'printf("skip %zu +%ld\n", static_cast<size_t>(sizeX) - mgr->bytes_in_buffer, num_bytes);'

Patch "Src\EterLib\JpegFile.cpp" `
'printf("resync %u\n", sizeX - mgr->bytes_in_buffer);' `
'printf("resync %zu\n", static_cast<size_t>(sizeX) - mgr->bytes_in_buffer);'


Patch "Src\MilesLib\Type.cpp" `
'fprintf(fPtr.get(), "SoundDataCount    %u\n", rSoundDataVector.size());' `
'fprintf(fPtr.get(), "SoundDataCount    %zu\n", rSoundDataVector.size());'

Patch "Src\MilesLib\Type.cpp" `
'for (uint32_t i = 0; i < rSoundDataVector.size(); ++i)' `
'for (std::size_t i = 0; i < rSoundDataVector.size(); ++i)'

Patch "Src\MilesLib\Type.cpp" `
'fprintf(fPtr.get(), "SoundData%02u       %f \"%s\"\n", i, rSoundData.fTime, rSoundData.strSoundFileName.c_str());' `
'fprintf(fPtr.get(), "SoundData%02zu       %f \"%s\"\n", i, rSoundData.fTime, rSoundData.strSoundFileName.c_str());'


Patch "Src\EffectLib\EffectManager.cpp" `
'Effect: Inst - ED %u, EI %u Pool - PSI %u, MI %u, LI %u, PI %u, EI %u, ED %u, PSD %u, EM %u, LD %u' `
'Effect: Inst - ED %zu, EI %zu Pool - PSI %u, MI %u, LI %u, PI %u, EI %u, ED %u, PSD %u, EM %u, LD %u'


Patch "Src\UserInterface\PythonCharacterManager.cpp" `
'Container - Live %u, Dead %u' `
'Container - Live %zu, Dead %zu'


Patch "Src\UserInterface\PythonItem.cpp" `
'Item: Inst %u, Pool %u' `
'Item: Inst %zu, Pool %u'


Patch "Src\UserInterface\PythonTextTail.cpp" `
'TextTail: ChatTail %u, ChrTail (Map %u, List %u), ItemTail (Map %u, List %u), Pool %u' `
'TextTail: ChatTail %zu, ChrTail (Map %zu, List %zu), ItemTail (Map %zu, List %zu), Pool %u'


# ============================================================
# 7. Bütün C4838 narrowing warningleri
# ============================================================

Patch "Src\EterLib\DibBar.cpp" `
'RECT rect = {0, 0, m_dwWidth, m_dwHeight};' `
'RECT rect = {0, 0, static_cast<LONG>(m_dwWidth), static_cast<LONG>(m_dwHeight)};'

Patch "Src\EterLib\DibBar.cpp" `
'RECT posRect = {dwxPos, dwyPos, dwxPos + dwImageWidth, dwyPos + dwImageHeight};' `
'RECT posRect = {static_cast<LONG>(dwxPos), static_cast<LONG>(dwyPos), static_cast<LONG>(dwxPos + dwImageWidth), static_cast<LONG>(dwyPos + dwImageHeight)};'


Patch "Src\EterLib\GrpScreen.cpp" `
'RECT rcTop = {0, 0, ms_d3dPresentParameter.BackBufferWidth, g_rcBrowser.top};' `
'RECT rcTop = {0, 0, static_cast<LONG>(ms_d3dPresentParameter.BackBufferWidth), g_rcBrowser.top};'

Patch "Src\EterLib\GrpScreen.cpp" `
'RECT rcBottom = {0, g_rcBrowser.bottom, ms_d3dPresentParameter.BackBufferWidth, ms_d3dPresentParameter.BackBufferHeight};' `
'RECT rcBottom = {0, g_rcBrowser.bottom, static_cast<LONG>(ms_d3dPresentParameter.BackBufferWidth), static_cast<LONG>(ms_d3dPresentParameter.BackBufferHeight)};'

Patch "Src\EterLib\GrpScreen.cpp" `
'RECT rcRight = {g_rcBrowser.right, g_rcBrowser.top, ms_d3dPresentParameter.BackBufferWidth, g_rcBrowser.bottom};' `
'RECT rcRight = {g_rcBrowser.right, g_rcBrowser.top, static_cast<LONG>(ms_d3dPresentParameter.BackBufferWidth), g_rcBrowser.bottom};'


Patch "Src\UserInterface\PythonChat.cpp" `
'RECT Rect = {fx, fy, fx + m_fWidth, fy + m_fHeight};' `
'RECT Rect = {static_cast<LONG>(fx), static_cast<LONG>(fy), static_cast<LONG>(fx + m_fWidth), static_cast<LONG>(fy + m_fHeight)};'


# ============================================================
# 8. Bütün C5055 enum/float warningleri
# ============================================================

Patch "Src\EterLib\GrpMarkInstance.cpp" `
'float fRenderWidth = MARK_WIDTH * m_fScale;' `
'float fRenderWidth = static_cast<float>(MARK_WIDTH) * m_fScale;'

Patch "Src\EterLib\GrpMarkInstance.cpp" `
'float fRenderHeight = MARK_HEIGHT * m_fScale;' `
'float fRenderHeight = static_cast<float>(MARK_HEIGHT) * m_fScale;'


Patch "Src\GameLib\AreaTerrain.cpp" `
'normal.z = 2.0f * CELLSCALE;' `
'normal.z = 2.0f * static_cast<float>(CELLSCALE);'


Patch "Src\GameLib\MapOutdoorLoad.cpp" `
'((float) CTerrainImpl::PATCH_XSIZE / CTerrainImpl::XSIZE)' `
'(static_cast<float>(CTerrainImpl::PATCH_XSIZE) / static_cast<float>(CTerrainImpl::XSIZE))'

Patch "Src\GameLib\MapOutdoorLoad.cpp" `
'((float) CTerrainImpl::PATCH_YSIZE / CTerrainImpl::XSIZE)' `
'(static_cast<float>(CTerrainImpl::PATCH_YSIZE) / static_cast<float>(CTerrainImpl::XSIZE))'


Patch "Src\GameLib\MapOutdoorRenderSTP.cpp" `
'((float) CTerrainImpl::PATCH_XSIZE / (CTerrainImpl::XSIZE))' `
'(static_cast<float>(CTerrainImpl::PATCH_XSIZE) / static_cast<float>(CTerrainImpl::XSIZE))'

Patch "Src\GameLib\MapOutdoorRenderSTP.cpp" `
'((float) CTerrainImpl::PATCH_YSIZE / (CTerrainImpl::YSIZE))' `
'(static_cast<float>(CTerrainImpl::PATCH_YSIZE) / static_cast<float>(CTerrainImpl::YSIZE))'


Patch "Src\EterGrnLib\ModelInstanceUpdate.cpp" `
'GetLocalTime() * ANIFPS_MAX' `
'GetLocalTime() * static_cast<float>(ANIFPS_MAX)'


Patch "Src\EterGrnLib\LODController.cpp" `
'(CGrannyModelInstance::ANIFPS_MAX - CGrannyModelInstance::ANIFPS_MIN) * fLODFactor / m_fLODDistance +' `
'(static_cast<float>(CGrannyModelInstance::ANIFPS_MAX - CGrannyModelInstance::ANIFPS_MIN) * fLODFactor / m_fLODDistance) +'


Patch "Src\UserInterface\PythonMiniMap.cpp" `
'static_cast<float>(sTerrainCountX) * CTerrainImpl::TERRAIN_XSIZE' `
'static_cast<float>(sTerrainCountX) * static_cast<float>(CTerrainImpl::TERRAIN_XSIZE)'

Patch "Src\UserInterface\PythonMiniMap.cpp" `
'static_cast<float>(sTerrainCountY) * CTerrainImpl::TERRAIN_YSIZE' `
'static_cast<float>(sTerrainCountY) * static_cast<float>(CTerrainImpl::TERRAIN_YSIZE)'


Write-Host ""
Write-Host "==================================================" -ForegroundColor Green
Write-Host " PHASE 3 TAMAMLANDI" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Simdi Release | x64 -> Clean Solution -> Rebuild Solution"
Write-Host ""