$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Src  = Join-Path $Root "Src"

function ReadText([string]$Path) {
    [System.IO.File]::ReadAllText($Path)
}

function WriteText([string]$Path, [string]$Text) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function ReplaceText {
    param(
        [string]$Path,
        [string]$Old,
        [string]$New
    )

    $c = ReadText $Path

    if ($c.Contains($New)) {
        Write-Host "[SKIP] $Path" -ForegroundColor Yellow
        return
    }

    if (!$c.Contains($Old)) {
        Write-Host "[NOT FOUND] $Path" -ForegroundColor Red
        return
    }

    $c = $c.Replace($Old, $New)
    WriteText $Path $c

    Write-Host "[OK] $Path" -ForegroundColor Green
}

function AddInclude {
    param(
        [string]$Path,
        [string]$Include
    )

    $c = ReadText $Path

    if ($c.Contains($Include)) {
        return
    }

    $c = $c.Replace(
        "#pragma once",
        "#pragma once`r`n`r`n$Include"
    )

    WriteText $Path $c
}

Write-Host ""
Write-Host "=== METIN2 X64 HARDENING PHASE 1 ===" -ForegroundColor Cyan


# ----------------------------------------------------------
# ParticleProperty.h
# ----------------------------------------------------------

$p = Join-Path $Src "EffectLib\ParticleProperty.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'return m_ImageVector.size();' `
    'return m2::narrow<uint32_t>(m_ImageVector.size());'


# ----------------------------------------------------------
# MapOutdoor.h
# ----------------------------------------------------------

$p = Join-Path $Src "GameLib\MapOutdoor.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'uint32_t lhsTerrainNumOrder = 0, rhsTerrainNumOrder = 0;' `
    'std::size_t lhsTerrainNumOrder = 0, rhsTerrainNumOrder = 0;'

ReplaceText `
    $p `
    'uint32_t GetMonsterAreaInfoCount() { return m_MonsterAreaInfoPtrVector.size(); }' `
    'uint32_t GetMonsterAreaInfoCount() { return m2::narrow<uint32_t>(m_MonsterAreaInfoPtrVector.size()); }'


# ----------------------------------------------------------
# PythonPlayer.h
# ----------------------------------------------------------

$p = Join-Path $Src "UserInterface\PythonPlayer.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'uint32_t GetShopSearchResultCount() noexcept { return m_ItemInstanceVector.size(); }' `
    'uint32_t GetShopSearchResultCount() noexcept { return m2::narrow<uint32_t>(m_ItemInstanceVector.size()); }'

ReplaceText `
    $p `
    'uint32_t GetShopSearchResultMaxCount() noexcept { return m_ItemInstanceVector.size(); }' `
    'uint32_t GetShopSearchResultMaxCount() noexcept { return m2::narrow<uint32_t>(m_ItemInstanceVector.size()); }'


# ----------------------------------------------------------
# PythonWindow.h
# ----------------------------------------------------------

$p = Join-Path $Src "EterPythonLib\PythonWindow.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'int GetChildCount() const { return m_pChildList.size(); }' `
    'int GetChildCount() const { return m2::narrow<int>(m_pChildList.size()); }'


# ----------------------------------------------------------
# FlyingData.h
# ----------------------------------------------------------

$p = Join-Path $Src "GameLib\FlyingData.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'int GetAttachDataCount() { return m_AttachDataVector.size(); }' `
    'int GetAttachDataCount() { return m2::narrow<int>(m_AttachDataVector.size()); }'


# ----------------------------------------------------------
# Pool.h
# ----------------------------------------------------------

$p = Join-Path $Src "EterLib\Pool.h"

AddInclude $p '#include "../EterBase/X64Compat.h"'

ReplaceText `
    $p `
    'uint32_t GetCapacity() { return m_kVct_pkData.size(); }' `
    'uint32_t GetCapacity() { return m2::narrow<uint32_t>(m_kVct_pkData.size()); }'


# ----------------------------------------------------------
# Utils.h
# ----------------------------------------------------------

$p = Join-Path $Src "EterBase\Utils.h"

$old = @'
	unsigned int capacity = sep.length() * container.size() - 1;

	// calculate string sequence
	{
		for (typename C::const_iterator i = container.begin(); i != container.end(); ++i)
			capacity += (*i).length();
	}
'@

$new = @'
	std::size_t capacity = 0;

	if (container.size() > 1)
		capacity = sep.length() * (container.size() - 1);

	// calculate string sequence
	{
		for (typename C::const_iterator i = container.begin(); i != container.end(); ++i)
			capacity += (*i).length();
	}
'@

ReplaceText $p $old $new


Write-Host ""
Write-Host "==================================" -ForegroundColor Green
Write-Host " PHASE 1 TAMAMLANDI" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Green