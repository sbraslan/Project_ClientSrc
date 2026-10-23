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

function EnsureInclude
{
    param(
        [string]$Relative,
        [string]$Include
    )

    $Path = Join-Path $Root $Relative
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
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " METIN2 X64 WARNING HARDENING - PHASE 4" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan


# ============================================================
# PythonNetworkStreamPhaseLoading
# ============================================================

$rel = "Src\UserInterface\PythonNetworkStreamPhaseLoading.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_kInsultChecker.IsInsultIn(c_szMsg, strlen(c_szMsg));' `
'return m_kInsultChecker.IsInsultIn(c_szMsg, m2::narrow<uint32_t>(strlen(c_szMsg)));'

Patch $rel `
'for (int k = 0; k < items.size(); k++)' `
'for (std::size_t k = 0; k < items.size(); ++k)'

Patch $rel `
'std::uniform_int_distribution<std::mt19937::result_type> dist(0, m_kMap_dwMapIndex_vecTipList.begin()->second.size() - 1);' `
'std::uniform_int_distribution<std::mt19937::result_type> dist(0, m2::narrow<std::mt19937::result_type>(m_kMap_dwMapIndex_vecTipList.begin()->second.size() - 1));'

Patch $rel `
'std::uniform_int_distribution<std::mt19937::result_type> dist(0, it->second.size() - 1);' `
'std::uniform_int_distribution<std::mt19937::result_type> dist(0, m2::narrow<std::mt19937::result_type>(it->second.size() - 1));'


# ============================================================
# Utils.cpp
# ============================================================

$rel = "Src\EterBase\Utils.cpp"
EnsureInclude $rel '#include "X64Compat.h"'

Patch $rel `
'for (i = strlen(sz_Name) - 1; i >= 0; --i)' `
'for (i = m2::narrow<int>(strlen(sz_Name)) - 1; i >= 0; --i)'

Patch $rel `
'uint32_t length = std::min(strGlobalPath.length(), strFullPathFileName.length());' `
'const std::size_t length = std::min(strGlobalPath.length(), strFullPathFileName.length());'

Patch $rel `
'for (uint32_t dwPos = 0; dwPos < length; ++dwPos)' `
'for (std::size_t dwPos = 0; dwPos < length; ++dwPos)'

Patch $rel `
'int len = strlen(path) + 1;' `
'const std::size_t len = strlen(path) + 1;'

$path = Join-Path $Root $rel
$text = ReadText $path

$old = @'
void StringExceptCharacter(std::string * pstrString, const char * c_szCharacter)
{
	int icurPos = 0;
	int iNextPos;

	while ((iNextPos = pstrString->find_first_of(c_szCharacter, icurPos)) >= 0)
	{
		std::string strFront = pstrString->substr(icurPos, iNextPos - icurPos);
		std::string strBack = pstrString->substr(iNextPos + 1, pstrString->length() - iNextPos - 1);
		*pstrString = strFront + strBack;
	}
}
'@

$new = @'
void StringExceptCharacter(std::string * pstrString, const char * c_szCharacter)
{
	std::string::size_type currentPos = 0;

	while (true)
	{
		const auto nextPos =
			pstrString->find_first_of(c_szCharacter, currentPos);

		if (nextPos == std::string::npos)
			break;

		pstrString->erase(nextPos, 1);
		currentPos = nextPos;
	}
}
'@

if ($text.Contains($old))
{
    $text = $text.Replace($old, $new)
    WriteText $path $text
    Write-Host "[OK] Utils StringExceptCharacter" -ForegroundColor Green
}

$path = Join-Path $Root $rel
$text = ReadText $path

$old = @'
	std::string strLine = c_szLine;

	uint32_t basePos = 0;

	do
	{
		int beginPos = strLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos < 0)
			return false;

		int endPos;

		if (strLine[beginPos] == '"')
		{
			++beginPos;
			endPos = strLine.find_first_of('"', beginPos);

			if (endPos < 0)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = strLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pkVec_strToken->emplace_back(strLine.substr(beginPos, endPos - beginPos));
	} while (basePos < strLine.length());
'@

$new = @'
	std::string strLine = c_szLine;

	std::string::size_type basePos = 0;

	do
	{
		auto beginPos =
			strLine.find_first_not_of(c_szDelimeter, basePos);

		if (beginPos == std::string::npos)
			return false;

		std::string::size_type endPos;

		if (strLine[beginPos] == '"')
		{
			++beginPos;
			endPos = strLine.find_first_of('"', beginPos);

			if (endPos == std::string::npos)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = strLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pkVec_strToken->emplace_back(
			strLine.substr(beginPos, endPos - beginPos)
		);
	} while (basePos != std::string::npos &&
			 basePos < strLine.length());
'@

if ($text.Contains($old))
{
    $text = $text.Replace($old, $new)
    WriteText $path $text
    Write-Host "[OK] Utils SplitLine" -ForegroundColor Green
}

Patch $rel `
'ULONG len = strlen(CmdLine);' `
'const std::size_t len = strlen(CmdLine);'

Patch $rel `
'ULONG i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);' `
'std::size_t i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);'


# ============================================================
# EterPackPolicy_CSHybridCrypt
# serialized 32-bit fields remain 32-bit -> checked narrowing
# ============================================================

$rel = "Src\EterPack\EterPackPolicy_CSHybridCrypt.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'uint32_t dwfileNameCrc = GetCRC32(fileNamelower.c_str(), fileNamelower.size());' `
'uint32_t dwfileNameCrc = GetCRC32(fileNamelower.c_str(), m2::narrow<uint32_t>(fileNamelower.size()));'

Patch $rel `
'uint32_t dwCryptKeySize = m_mapHybridCryptKey.size();' `
'const uint32_t dwCryptKeySize = m2::narrow<uint32_t>(m_mapHybridCryptKey.size());'

Patch $rel `
'iSDBSize = vecSDB.size();' `
'iSDBSize = m2::narrow<int>(vecSDB.size());'

Patch $rel `
'uint32_t dwSDBMapSize = m_mapSDBMap.size();' `
'const uint32_t dwSDBMapSize = m2::narrow<uint32_t>(m_mapSDBMap.size());'

Patch $rel `
'uint32_t dwMapNameSize = strRelatedMapName.size();' `
'const uint32_t dwMapNameSize = m2::narrow<uint32_t>(strRelatedMapName.size());'

Patch $rel `
'auto bSDBSize = static_cast<uint8_t>(sdbVector.size());' `
'const uint8_t bSDBSize = m2::narrow<uint8_t>(sdbVector.size());'


# ============================================================
# PythonNetworkStreamPhaseGame
# PACKET ABI korunuyor
# ============================================================

$rel = "Src\UserInterface\PythonNetworkStreamPhaseGame.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'int iTextLen = strlen(c_szChat) + 1;' `
'const int iTextLen = m2::narrow<int>(strlen(c_szChat) + 1);'

Patch $rel `
'ChatPacket.length = sizeof(ChatPacket) + iTextLen;' `
'ChatPacket.length = m2::narrow<uint16_t>(sizeof(ChatPacket) + static_cast<std::size_t>(iTextLen));'

Patch $rel `
'__FilterInsult(p, strlen(p));' `
'__FilterInsult(p, m2::narrow<uint32_t>(strlen(p)));'

Patch $rel `
'WhisperPacket.wSize = sizeof(WhisperPacket) + iTextLen;' `
'WhisperPacket.wSize = m2::narrow<uint16_t>(sizeof(WhisperPacket) + static_cast<std::size_t>(iTextLen));'

Patch $rel `
'if (!Recv(table_size, &table))' `
'if (!Recv(m2::narrow<int>(table_size), &table))'


# ============================================================
# IME.cpp
# ============================================================

$rel = "Src\EterLib\IME.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'WideCharToMultiByte(ms_uOutputCodePage, 0, &ms_wstrReading[0], ms_wstrReading.size(), reading, sizeof(reading), nullptr, nullptr);' `
'WideCharToMultiByte(ms_uOutputCodePage, 0, &ms_wstrReading[0], m2::narrow<int>(ms_wstrReading.size()), reading, sizeof(reading), nullptr, nullptr);'

Patch $rel `
'return rstrText.size();' `
'return m2::narrow<int>(rstrText.size());'

Patch $rel `
'int wTextLen = wcslen(wszText);' `
'const int wTextLen = m2::narrow<int>(wcslen(wszText));'


# ============================================================
# TextFileLoader
# ============================================================

$rel = "Src\EterLib\TextFileLoader.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'GenNameKey(c_rstGroupName.c_str(), c_rstGroupName.length())' `
'GenNameKey(c_rstGroupName.c_str(), m2::narrow<uint32_t>(c_rstGroupName.length()))'

Patch $rel `
'GenNameKey(m_strGroupName.c_str(), m_strGroupName.length())' `
'GenNameKey(m_strGroupName.c_str(), m2::narrow<uint32_t>(m_strGroupName.length()))'

Patch $rel `
'return m_pcurNode->ChildNodeVector.size();' `
'return m2::narrow<uint32_t>(m_pcurNode->ChildNodeVector.size());'

Patch $rel `
'SGroupNode::GenNameKey(c_szKey, strlen(c_szKey))' `
'SGroupNode::GenNameKey(c_szKey, m2::narrow<uint32_t>(strlen(c_szKey)))'


# ============================================================
# NetStream
# ============================================================

$rel = "Src\EterLib\NetStream.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'const int restSize = m_recvBuf.size() - m_recvBufInputPos;' `
'const int restSize = m2::narrow<int>(m_recvBuf.size() - static_cast<std::size_t>(m_recvBufInputPos));'

Patch $rel `
'const int recvSize = recv(m_sock, m_recvBuf.data() + m_recvBufInputPos, m_recvBuf.size() - m_recvBufInputPos, 0);' `
'const int recvSize = recv(m_sock, m_recvBuf.data() + m_recvBufInputPos, m2::narrow<int>(m_recvBuf.size() - static_cast<std::size_t>(m_recvBufInputPos)), 0);'

Patch $rel `
'const int sendBufRestSize = m_sendBuf.size() - m_sendBufInputPos;' `
'const int sendBufRestSize = m2::narrow<int>(m_sendBuf.size() - static_cast<std::size_t>(m_sendBufInputPos));'


# ============================================================
# GrpTextInstance
# ============================================================

$rel = "Src\EterLib\GrpTextInstance.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'gs_hyperlinkText.length(), buf, len' `
'm2::narrow<int>(gs_hyperlinkText.length()), buf, len'

Patch $rel `
'emojiBuffer.length(), retBuf, sizeof(retBuf) - 1' `
'm2::narrow<int>(emojiBuffer.length()), retBuf, sizeof(retBuf) - 1'


# ============================================================
# Area
# ============================================================

$rel = "Src\GameLib\Area.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_kRenderedThingInstanceCRCWithNumberVector.size();' `
'return m2::narrow<uint32_t>(m_kRenderedThingInstanceCRCWithNumberVector.size());'

Patch $rel `
'return m_kRenderedGrapphicThingInstanceVector.size();' `
'return m2::narrow<uint32_t>(m_kRenderedGrapphicThingInstanceVector.size());'

Patch $rel `
'int p = s + 1;' `
'const std::string::size_type p = s + 1;'

Patch $rel `
'return m_ObjectDataVector.size();' `
'return m2::narrow<uint32_t>(m_ObjectDataVector.size());'

Patch $rel `
'return m_ObjectInstanceVector.size();' `
'return m2::narrow<uint32_t>(m_ObjectInstanceVector.size());'


# ============================================================
# ItemManager
# ============================================================

$rel = "Src\GameLib\ItemManager.cpp"

Patch $rel `
'int n = m_vec_ItemRange.size();' `
'const std::size_t n = m_vec_ItemRange.size();'

Patch $rel `
'for (int i = 0; i < n; i++)' `
'for (std::size_t i = 0; i < n; ++i)'

$path = Join-Path $Root $rel
$text = ReadText $path

$text = $text.Replace(
    'uint32_t uSrcLen = c_rstSrc.length();',
    'const std::string::size_type uSrcLen = c_rstSrc.length();'
)

$text = $text.Replace(
    'uint32_t uLeftCut = 1;',
    'const std::string::size_type uLeftCut = 1;'
)

$text = $text.Replace(
    'uint32_t uRightCut = uSrcLen;',
    'std::string::size_type uRightCut = uSrcLen;'
)

$text = $text.Replace(
    'unsigned long i, len;',
    'std::size_t i, len;'
)

WriteText $path $text
Write-Host "[OK] ItemManager size_t cleanup" -ForegroundColor Green


# ============================================================
# Granny Material
# ============================================================

$rel = "Src\EterGrnLib\Material.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'uint32_t size = m_mtrlVector.size();' `
'const std::size_t size = m_mtrlVector.size();'

Patch $rel `
'uint32_t i;' `
'std::size_t i;'

Patch $rel `
'for (uint32_t i = 0; i < size; ++i)' `
'for (std::size_t i = 0; i < size; ++i)'

Patch $rel `
'return i;' `
'return m2::narrow<uint32_t>(i);'

Patch $rel `
'return size;' `
'return m2::narrow<uint32_t>(size);'

Patch $rel `
'return m_mtrlVector.size();' `
'return m2::narrow<uint32_t>(m_mtrlVector.size());'


# ============================================================
# Granny ThingInstance
# ============================================================

$rel = "Src\EterGrnLib\ThingInstance.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'int max = m_LODControllerVector.size();' `
'const std::size_t max = m_LODControllerVector.size();'

Patch $rel `
'return iModelInstance < max;' `
'return static_cast<std::size_t>(iModelInstance) < max;'

Patch $rel `
'int max = m_modelThingSetVector.size();' `
'const std::size_t max = m_modelThingSetVector.size();'

Patch $rel `
'return iModelThing < max;' `
'return static_cast<std::size_t>(iModelThing) < max;'

Patch $rel `
'int nLODCount = m_LODControllerVector.size();' `
'const std::size_t nLODCount = m_LODControllerVector.size();'

Patch $rel `
'if (iModelInstanceIndex >= nLODCount)' `
'if (iModelInstanceIndex < 0 || static_cast<std::size_t>(iModelInstanceIndex) >= nLODCount)'

Patch $rel `
'return m_LODControllerVector.size();' `
'return m2::narrow<uint32_t>(m_LODControllerVector.size());'


# ============================================================
# EffectMesh
# ============================================================

$rel = "Src\EffectLib\EffectMesh.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_pEffectMeshDataVector.size();' `
'return m2::narrow<uint32_t>(m_pEffectMeshDataVector.size());'

Patch $rel `
'GetFileExtension(pMeshData->szDiffuseMapFileName, strlen(pMeshData->szDiffuseMapFileName), &strExtension);' `
'GetFileExtension(pMeshData->szDiffuseMapFileName, m2::narrow<int>(strlen(pMeshData->szDiffuseMapFileName)), &strExtension);'

Patch $rel `
'return m_MeshDataVector.size();' `
'return m2::narrow<int>(m_MeshDataVector.size());'


# ============================================================
# MarkManager
# serialized counts remain uint32_t
# ============================================================

$rel = "Src\UserInterface\MarkManager.cpp"
EnsureInclude $rel '#include "../EterBase/X64Compat.h"'

Patch $rel `
'return m_mapIdx_Image.size();' `
'return m2::narrow<uint32_t>(m_mapIdx_Image.size());'

Patch $rel `
'return m_mapGID_MarkID.size();' `
'return m2::narrow<uint32_t>(m_mapGID_MarkID.size());'

Patch $rel `
'uint32_t symbolCount = m_mapSymbol.size();' `
'const uint32_t symbolCount = m2::narrow<uint32_t>(m_mapSymbol.size());'

Patch $rel `
'uint32_t dwSize = it.second.raw.size();' `
'const uint32_t dwSize = m2::narrow<uint32_t>(it.second.raw.size());'


# ============================================================
# Kalan tek C4477 - MilesLib
# ============================================================

Patch "Src\MilesLib\Type.cpp" `
'"%02d"' `
'"%02zu"'


# ============================================================
# Kalan C5055 - LODController
# ============================================================

$rel = "Src\EterGrnLib\LODController.cpp"

Patch $rel `
'CGrannyModelInstance::ANIFPS_MIN);' `
'static_cast<float>(CGrannyModelInstance::ANIFPS_MIN));'


Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host " PHASE 4 TAMAMLANDI" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""