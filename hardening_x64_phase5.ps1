$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot

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
        Write-Host "[SKIP] $Relative" -ForegroundColor Yellow
    }
    else
    {
        Write-Host "[NOT FOUND] $Relative" -ForegroundColor DarkYellow
    }
}

function EnsureX64
{
    param([string]$Relative)

    $Path = Join-Path $Root $Relative
    $Text = ReadText $Path

    if ($Text.Contains("X64Compat.h"))
    {
        return
    }

    if ($Relative -like "Src\EterBase\*")
    {
        $Inc = '#include "X64Compat.h"'
    }
    else
    {
        $Inc = '#include "../EterBase/X64Compat.h"'
    }

    if ($Text.Contains('#include "StdAfx.h"'))
    {
        $Text = $Text.Replace(
            '#include "StdAfx.h"',
            "#include `"StdAfx.h`"`r`n$Inc"
        )
    }
    elseif ($Text.Contains('#include "Stdafx.h"'))
    {
        $Text = $Text.Replace(
            '#include "Stdafx.h"',
            "#include `"Stdafx.h`"`r`n$Inc"
        )
    }
    else
    {
        $Text = "$Inc`r`n$Text"
    }

    WriteText $Path $Text
}


Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " METIN2 X64 WARNING CLEANUP - PHASE 5" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan


# ============================================================
# ETERLIB
# ============================================================

$rel = "Src\EterLib\AttributeData.cpp"
EnsureX64 $rel
Patch $rel `
'return m_HeightDataVector.size();' `
'return m2::narrow<uint32_t>(m_HeightDataVector.size());'


$rel = "Src\EterLib\GrpDIB.cpp"
EnsureX64 $rel
Patch $rel `
'::TextOut(m_hDC, ix, iy, c_szText, strlen(c_szText));' `
'::TextOut(m_hDC, ix, iy, c_szText, m2::narrow<int>(strlen(c_szText)));'


$rel = "Src\EterLib\AttributeInstance.cpp"

Patch $rel `
'uint32_t dwVertexCount = c_pHeightData->v3VertexVector.size();' `
'const std::size_t dwVertexCount = c_pHeightData->v3VertexVector.size();'

Patch $rel `
'for (uint32_t j = 0; j < dwVertexCount; ++j)' `
'for (std::size_t j = 0; j < dwVertexCount; ++j)'


$rel = "Src\EterLib\GrpObjectInstance.cpp"
EnsureX64 $rel
Patch $rel `
'return m_StaticCollisionInstanceVector.size();' `
'return m2::narrow<uint32_t>(m_StaticCollisionInstanceVector.size());'


$rel = "Src\EterLib\GrpLightManager.cpp"
EnsureX64 $rel
Patch $rel `
'return m_dwSkipIndex + m_LightMap.size();' `
'return m2::narrow<TLightID>(static_cast<std::size_t>(m_dwSkipIndex) + m_LightMap.size());'


$rel = "Src\EterLib\GroupTextParseTree.cpp"
EnsureX64 $rel

Patch $rel `
'pGroupNode->m_map_columnNameToIndex.emplace(stTokenVector[i], i - 1);' `
'pGroupNode->m_map_columnNameToIndex.emplace(stTokenVector[i], m2::narrow<int>(i - 1));'

Patch $rel `
'return m_mapChildNodes.size();' `
'return m2::narrow<uint32_t>(m_mapChildNodes.size());'

Patch $rel `
'return m_map_rows.size();' `
'return m2::narrow<int>(m_map_rows.size());'

Patch $rel `
'return m_vec_values.size();' `
'return m2::narrow<int>(m_vec_values.size());'


$rel = "Src\EterLib\parser.cpp"
EnsureX64 $rel

Patch $rel `
'LocaleString_RightTrim(base, strlen(base));' `
'LocaleString_RightTrim(base, m2::narrow<int>(strlen(base)));'

Patch $rel `
'int str_len = stSource.length();' `
'const int str_len = m2::narrow<int>(stSource.length());'


$rel = "Src\EterLib\SkyBox.cpp"
EnsureX64 $rel
Patch $rel `
'ulVectorGradientColornum = c_rColorVector.size() - 1;' `
'ulVectorGradientColornum = m2::narrow<unsigned long>(c_rColorVector.size() - 1);'


$rel = "Src\EterLib\Util.cpp"
EnsureX64 $rel
Patch $rel `
'int length = strlen(str);' `
'const int length = m2::narrow<int>(strlen(str));'


$rel = "Src\EterLib\GrpExpandedImageInstance.cpp"
EnsureX64 $rel
Patch $rel `
'CGraphicBase::SetPDTStream(&vertices[0], vertices.size())' `
'CGraphicBase::SetPDTStream(&vertices[0], m2::narrow<uint32_t>(vertices.size()))'


$rel = "Src\EterLib\GrpImageInstance.cpp"
EnsureX64 $rel
Patch $rel `
'CGraphicBase::SetPDTStream(&vertices[0], vertices.size())' `
'CGraphicBase::SetPDTStream(&vertices[0], m2::narrow<uint32_t>(vertices.size()))'


$rel = "Src\EterLib\ResourceManager.cpp"
EnsureX64 $rel
Patch $rel `
'filesize = fPtr2.size();' `
'filesize = m2::narrow<int>(fPtr2.size());'


$rel = "Src\EterLib\GrpSubImage.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    "int nPos = c_rstSubFileName.find_last_of('\\', -1);",
    "const auto nPos = c_rstSubFileName.find_last_of('\\');"
)

$text = $text.Replace(
    "if (nPos >= 0)",
    "if (nPos != std::string::npos)"
)

$text = $text.Replace(
    "nPos++;",
    "const auto fileNamePos = nPos + 1;"
)

$text = $text.Replace(
    "memcpy(szFileName, c_rstSubFileName.c_str(), nPos);",
    "memcpy(szFileName, c_rstSubFileName.c_str(), fileNamePos);"
)

$text = $text.Replace(
    "szFileName[nPos] = '\0';",
    "szFileName[fileNamePos] = '\0';"
)

$text = $text.Replace(
    "memcpy(szFileName + nPos, c_rstImage.c_str(), c_rstImage.length());",
    "memcpy(szFileName + fileNamePos, c_rstImage.c_str(), c_rstImage.length());"
)

WriteText $path $text
Write-Host "[OK] GrpSubImage size_type" -ForegroundColor Green


$rel = "Src\EterLib\GrpFontTexture.cpp"
EnsureX64 $rel
Patch $rel `
'rNewCharInfo.index = m_pFontTextureVector.size() - 1;' `
'rNewCharInfo.index = m2::narrow<int16_t>(m_pFontTextureVector.size() - 1);'


$rel = "Src\EterLib\TextBar.cpp"
EnsureX64 $rel
Patch $rel `
'GetTextExtentPoint32(hDC, c_szText, strlen(c_szText), p_size);' `
'GetTextExtentPoint32(hDC, c_szText, m2::narrow<int>(strlen(c_szText)), p_size);'


# ============================================================
# GAMELIB
# ============================================================

$rel = "Src\GameLib\ActorInstanceCollisionDetection.cpp"
EnsureX64 $rel
Patch $rel `
'const int iCurrentHitCount = rHittedInstanceMap.size();' `
'const int iCurrentHitCount = m2::narrow<int>(rHittedInstanceMap.size());'

Patch $rel `
'const int iCurrentHitCount = itHitData->second.size();' `
'const int iCurrentHitCount = m2::narrow<int>(itHitData->second.size());'


$rel = "Src\GameLib\RaceMotionData.cpp"
EnsureX64 $rel
Patch $rel `
'return m_MotionEventDataVector.size();' `
'return m2::narrow<uint32_t>(m_MotionEventDataVector.size());'


$rel = "Src\GameLib\RaceDataFile.cpp"

Patch $rel `
'uint32_t uLineCount = pSmokeTokenVector->size() / 2;' `
'const std::size_t uLineCount = pSmokeTokenVector->size() / 2;'

Patch $rel `
'for (uint32_t uLine = 0; uLine < uLineCount; ++uLine)' `
'for (std::size_t uLine = 0; uLine < uLineCount; ++uLine)'


$rel = "Src\GameLib\WeaponTrace.cpp"
EnsureX64 $rel
Patch $rel `
'int n = m_LongTimePointList.size() - 1;' `
'const int n = m2::narrow<int>(m_LongTimePointList.size() - 1);'


$rel = "Src\GameLib\RaceData.cpp"
EnsureX64 $rel
Patch $rel `
'return m_AttachingDataVector.size();' `
'return m2::narrow<uint32_t>(m_AttachingDataVector.size());'


$rel = "Src\GameLib\Property.cpp"
EnsureX64 $rel
Patch $rel `
'return m_stTokenMap.size();' `
'return m2::narrow<uint32_t>(m_stTokenMap.size());'

Patch $rel `
'file.Write(m_stCRC.c_str(), m_stCRC.length());' `
'file.Write(m_stCRC.c_str(), m2::narrow<int>(m_stCRC.length()));'


$rel = "Src\GameLib\PropertyLoader.cpp"
EnsureX64 $rel
Patch $rel `
'GetFileExtension(strFileName.c_str(), strFileName.length(), &stExt);' `
'GetFileExtension(strFileName.c_str(), m2::narrow<int>(strFileName.length()), &stExt);'


$rel = "Src\GameLib\FlyingData.cpp"
EnsureX64 $rel
Patch $rel `
'return m_AttachDataVector.size() - 1;' `
'return m2::narrow<uint32_t>(m_AttachDataVector.size() - 1);'


$rel = "Src\GameLib\GameType.cpp"
EnsureX64 $rel
Patch $rel `
'uint32_t dwAttachingDataCount = c_rAttachingDataVector.size();' `
'const uint32_t dwAttachingDataCount = m2::narrow<uint32_t>(c_rAttachingDataVector.size());'


# ============================================================
# ETERPYTHONLIB
# ============================================================

$rel = "Src\EterPythonLib\PythonGraphic.cpp"
EnsureX64 $rel

Patch $rel `
'SetPDTStream(&vertices[0], vertices.size())' `
'SetPDTStream(&vertices[0], m2::narrow<uint32_t>(vertices.size()))'

Patch $rel `
'SetPDTStream(vertices.data(), vertices.size())' `
'SetPDTStream(vertices.data(), m2::narrow<uint32_t>(vertices.size()))'


$rel = "Src\EterPythonLib\PythonGridSlotWindow.cpp"
EnsureX64 $rel
Patch $rel `
'uint16_t wCellMaxPerPage = m_SlotVector.size();' `
'const uint16_t wCellMaxPerPage = m2::narrow<uint16_t>(m_SlotVector.size());'


$rel = "Src\EterPythonLib\PythonSlotWindow.cpp"
EnsureX64 $rel

Patch $rel `
'return m_SlotList.size();' `
'return m2::narrow<uint32_t>(m_SlotList.size());'

Patch $rel `
'return m_dwSelectedSlotIndexList.size();' `
'return m2::narrow<uint32_t>(m_dwSelectedSlotIndexList.size());'


$rel = "Src\EterPythonLib\PythonWindow.cpp"
EnsureX64 $rel

Patch $rel `
'return std::max<size_t>(0, GetWidth() + m_renderingRect.right + m_renderingRect.left);' `
'return m2::narrow<int>(std::max<size_t>(0, GetWidth() + m_renderingRect.right + m_renderingRect.left));'

Patch $rel `
'return std::max<size_t>(0, GetHeight() + m_renderingRect.bottom + m_renderingRect.top);' `
'return m2::narrow<int>(std::max<size_t>(0, GetHeight() + m_renderingRect.bottom + m_renderingRect.top));'

Patch $rel `
'const int iSize = m_ImageVector.size();' `
'const std::size_t iSize = m_ImageVector.size();'

Patch $rel `
'for (int i = 0; i < iSize; i++)' `
'for (std::size_t i = 0; i < iSize; ++i)'


# ============================================================
# ETERGRNLIB
# ============================================================

$rel = "Src\EterGrnLib\LODController.cpp"
EnsureX64 $rel

Patch $rel `
'm_bLODLevel = static_cast<uint8_t>(MIN(m_que_pkModelInst.size() - 1, bLodLevel));' `
'm_bLODLevel = m2::narrow<uint8_t>(std::min<std::size_t>(m_que_pkModelInst.size() - 1, static_cast<std::size_t>(bLodLevel)));'


$rel = "Src\EterGrnLib\Thing.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$old = @'
		int sepPos = fileName.rfind('\');
		gs_modelLocalPath.assign(fileName, 0, sepPos + 1);
'@

$new = @'
		const auto sepPos = fileName.rfind('\');
		if (sepPos != std::string::npos)
			gs_modelLocalPath.assign(fileName, 0, sepPos + 1);
		else
			gs_modelLocalPath.clear();
'@

if ($text.Contains($old))
{
    $text = $text.Replace($old, $new)
    WriteText $path $text
    Write-Host "[OK] Thing.cpp path index" -ForegroundColor Green
}


# ============================================================
# ETERBASE
# ============================================================

$rel = "Src\EterBase\Stl.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'int iPos = 0;',
    'std::string::size_type iPos = 0;'
)

$text = $text.Replace(
    'uint32_t sizeS2 = delimiter.size();',
    'const std::string::size_type sizeS2 = delimiter.size();'
)

$text = $text.Replace(
    'uint32_t isize = input.size();',
    'const std::string::size_type isize = input.size();'
)

$text = $text.Replace(
    'std::vector<int> positions;',
    'std::vector<std::string::size_type> positions;'
)

$text = $text.Replace(
    'int newPos = input.find(delimiter, 0);',
    'auto newPos = input.find(delimiter, 0);'
)

$text = $text.Replace(
    'if (newPos < 0)',
    'if (newPos == std::string::npos)'
)

$text = $text.Replace(
    'while (newPos >= iPos)',
    'while (newPos != std::string::npos && newPos >= iPos)'
)

WriteText $path $text
Write-Host "[OK] Stl.cpp split_string" -ForegroundColor Green


$rel = "Src\EterBase\Stl.h"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'int max = m_dataVector.size();',
    'const std::size_t max = m_dataVector.size();'
)

$text = $text.Replace(
    'if (m_pos >= max)',
    'if (static_cast<std::size_t>(m_pos) >= max)'
)

WriteText $path $text
Write-Host "[OK] Stl.h stack_pool" -ForegroundColor Green


$rel = "Src\EterBase\MappedFile.cpp"
EnsureX64 $rel
Patch $rel `
'm_seekPosition = std::max<size_t>(0, Size() - offset);' `
'm_seekPosition = m2::narrow<uint32_t>(std::max<size_t>(0, Size() - offset));'


$rel = "Src\EterBase\poly\Poly.cpp"
EnsureX64 $rel
Patch $rel `
'return m_tokenVector.size();' `
'return m2::narrow<int>(m_tokenVector.size());'


$rel = "Src\EterBase\CRC32.cpp"
EnsureX64 $rel
Patch $rel `
'int len = strlen(src);' `
'const int len = m2::narrow<int>(strlen(src));'


$rel = "Src\EterBase\FileDir.cpp"
EnsureX64 $rel
Patch $rel `
'int iLength = strlen(c_szPathName);' `
'const int iLength = m2::narrow<int>(strlen(c_szPathName));'


# ============================================================
# ETERPACK
# ============================================================

$rel = "Src\EterPack\EterPackPolicy_CSHybridCrypt.cpp"
EnsureX64 $rel

Patch $rel `
'uint32_t dwExtHash = stringhash::GetHash(extName);' `
'const uint32_t dwExtHash = m2::narrow<uint32_t>(stringhash::GetHash(extName));'

Patch $rel `
'uint32_t dwfileNameCrc = GetCRC32(fileNamelower.c_str(), fileNamelower.size());' `
'const uint32_t dwfileNameCrc = GetCRC32(fileNamelower.c_str(), m2::narrow<uint32_t>(fileNamelower.size()));'

Patch $rel `
'uint32_t dwFileNameHash = stringhash::GetHash(fileNamelower);' `
'const uint32_t dwFileNameHash = m2::narrow<uint32_t>(stringhash::GetHash(fileNamelower));'


$rel = "Src\EterPack\EterPackManager.cpp"
EnsureX64 $rel

Patch $rel `
'CompareName(c_rstrName.c_str(), c_rstrName.length(), strFileName.c_str())' `
'CompareName(c_rstrName.c_str(), m2::narrow<uint32_t>(c_rstrName.length()), strFileName.c_str())'

Patch $rel `
'uint32_t dwPackNamehash = stringhash::GetHash(noPathName);' `
'const uint32_t dwPackNamehash = m2::narrow<uint32_t>(stringhash::GetHash(noPathName));'


# ============================================================
# EFFECTLIB
# ============================================================

$rel = "Src\EffectLib\EffectData.cpp"
EnsureX64 $rel

Patch $rel `
'return m_LightVector.size();' `
'return m2::narrow<uint32_t>(m_LightVector.size());'

Patch $rel `
'return m_ParticleVector.size();' `
'return m2::narrow<uint32_t>(m_ParticleVector.size());'

Patch $rel `
'return m_MeshVector.size();' `
'return m2::narrow<uint32_t>(m_MeshVector.size());'


$rel = "Src\EffectLib\EffectMeshInstance.cpp"
EnsureX64 $rel
Patch $rel `
'SetPDTStream(&vertices[0], vertices.size())' `
'SetPDTStream(&vertices[0], m2::narrow<uint32_t>(vertices.size()))'


# ============================================================
# USERINTERFACE
# ============================================================

$rel = "Src\UserInterface\PythonGuildBank.cpp"
EnsureX64 $rel
Patch $rel `
'return m_GuildBankItemInstanceVector.size();' `
'return m2::narrow<uint32_t>(m_GuildBankItemInstanceVector.size());'


$rel = "Src\UserInterface\PythonGameEventManager.cpp"
EnsureX64 $rel
Patch $rel `
'return m_map_Event[iDay].size();' `
'return m2::narrow<int>(m_map_Event[iDay].size());'


$rel = "Src\UserInterface\PythonItem.cpp"
EnsureX64 $rel
Patch $rel `
'return m_NoGradeNameItemData.size();' `
'return m2::narrow<uint32_t>(m_NoGradeNameItemData.size());'


$rel = "Src\UserInterface\PythonQuest.cpp"
EnsureX64 $rel
Patch $rel `
'return m_QuestInstanceContainer.size();' `
'return m2::narrow<int>(m_QuestInstanceContainer.size());'


$rel = "Src\UserInterface\PythonNonPlayer.cpp"
EnsureX64 $rel
Patch $rel `
'int iSmallLen = std::min(stRealName1.length(), stRealName2.length());' `
'const int iSmallLen = m2::narrow<int>(std::min(stRealName1.length(), stRealName2.length()));'


$rel = "Src\UserInterface\PythonPlayerEventHandler.cpp"
EnsureX64 $rel

Patch $rel `
'unsigned int uiVictimCount = m_kVctkVictim.size();' `
'const unsigned int uiVictimCount = m2::narrow<unsigned int>(m_kVctkVictim.size());'

Patch $rel `
'kPacketSyncPos.wSize = sizeof(kPacketSyncPos) + sizeof(TPacketCGSyncPositionElement) * uiVictimCount;' `
'kPacketSyncPos.wSize = m2::narrow<uint16_t>(sizeof(kPacketSyncPos) + sizeof(TPacketCGSyncPositionElement) * static_cast<std::size_t>(uiVictimCount));'


$rel = "Src\UserInterface\PythonSafeBox.cpp"
EnsureX64 $rel

Patch $rel `
'return m_ItemInstanceVector.size();' `
'return m2::narrow<int>(m_ItemInstanceVector.size());'

Patch $rel `
'return m_MallItemInstanceVector.size();' `
'return m2::narrow<uint32_t>(m_MallItemInstanceVector.size());'


$rel = "Src\UserInterface\PythonGuild.cpp"
EnsureX64 $rel

Patch $rel `
'return m_GuildMemberDataVector.size();' `
'return m2::narrow<uint32_t>(m_GuildMemberDataVector.size());'

Patch $rel `
'm_dwMemberLevelAverage = m_dwMemberLevelSummary / m_GuildMemberDataVector.size();' `
'm_dwMemberLevelAverage = m_dwMemberLevelSummary / m2::narrow<uint32_t>(m_GuildMemberDataVector.size());'


$rel = "Src\UserInterface\PythonChat.cpp"
EnsureX64 $rel

Patch $rel `
'int iLineIndex = pLineList->size();' `
'const int iLineIndex = m2::narrow<int>(pLineList->size());'

Patch $rel `
'return pLineList->size();' `
'return m2::narrow<int>(pLineList->size());'

Patch $rel `
'int icurLineCount = TempChatLineDeque.size();' `
'const int icurLineCount = m2::narrow<int>(TempChatLineDeque.size());'


$rel = "Src\UserInterface\EventHandler.h"

Patch $rel `
'int size = EventInfoMap.size();' `
'const std::size_t size = EventInfoMap.size();'


$rel = "Src\UserInterface\PythonMiniMap.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    "int iPos = aAtlasMarkInfo.m_strText.find(' ');",
    "const auto iPos = aAtlasMarkInfo.m_strText.find(' ');"
)

$text = $text.Replace(
    "if (iPos >= 0)",
    "if (iPos != std::string::npos)"
)

WriteText $path $text
Write-Host "[OK] PythonMiniMap string index" -ForegroundColor Green


$rel = "Src\UserInterface\GuildMarkUploader.cpp"
EnsureX64 $rel
Patch $rel `
'if (!Send(m_pbySymbolBuf.size(), m_pbySymbolBuf.data()))' `
'if (!Send(m2::narrow<int>(m_pbySymbolBuf.size()), m_pbySymbolBuf.data()))'


$rel = "Src\UserInterface\InsultChecker.cpp"
EnsureX64 $rel

Patch $rel `
'LocaleService_StringCompareCI(c_szWord, rstInsult.c_str(), rstInsult.length())' `
'LocaleService_StringCompareCI(c_szWord, rstInsult.c_str(), m2::narrow<int>(rstInsult.length()))'

Patch $rel `
'*puInsultLen = rstInsult.length();' `
'*puInsultLen = m2::narrow<uint32_t>(rstInsult.length());'


$rel = "Src\UserInterface\PythonApplicationModule.cpp"
EnsureX64 $rel
Patch $rel `
'tea_decrypt(reinterpret_cast<uint32_t *>(buf), reinterpret_cast<const uint32_t *>(enc), reinterpret_cast<const uint32_t *>(key), size);' `
'tea_decrypt(reinterpret_cast<uint32_t *>(buf), reinterpret_cast<const uint32_t *>(enc), reinterpret_cast<const uint32_t *>(key), m2::narrow<int>(size));'


$rel = "Src\UserInterface\PythonIMEModule.cpp"
EnsureX64 $rel
Patch $rel `
'CPythonIME::Instance().SetText(szText, strlen(szText));' `
'CPythonIME::Instance().SetText(szText, m2::narrow<int>(strlen(szText)));'


$rel = "Src\UserInterface\MovieMan.cpp"
EnsureX64 $rel
Patch $rel `
'GetFileExtension(cpFilename, strlen(cpFilename), &ext);' `
'GetFileExtension(cpFilename, m2::narrow<int>(strlen(cpFilename)), &ext);'


# ============================================================
# PythonNetworkStreamCommand parser cleanup
# ============================================================

$rel = "Src\UserInterface\PythonNetworkStreamCommand.cpp"

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'uint32_t basePos = 0;',
    'std::string::size_type basePos = 0;'
)

$text = $text.Replace(
    'int beginPos = strLine.find_first_not_of(c_szDelimeter, basePos);',
    'auto beginPos = strLine.find_first_not_of(c_szDelimeter, basePos);'
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
    'if (int(strLine.find_first_not_of(c_szDelimeter, basePos)) < 0)',
    'if (strLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)'
)

WriteText $path $text
Write-Host "[OK] PythonNetworkStreamCommand parser" -ForegroundColor Green


# ============================================================
# Miles C4477
# ============================================================

Patch "Src\MilesLib\Type.cpp" `
'"sounddata%02d", i' `
'"sounddata%02zu", i'


Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host " PHASE 5 TAMAMLANDI" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""