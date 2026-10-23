$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Src  = Join-Path $Root "Src"

function Read-Utf8([string]$Path)
{
    return [System.IO.File]::ReadAllText($Path)
}

function Write-Utf8([string]$Path, [string]$Content)
{
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function Replace-Once
{
    param(
        [string]$Path,
        [string]$Old,
        [string]$New
    )

    $content = Read-Utf8 $Path

    if ($content.Contains($New))
    {
        Write-Host "[SKIP] $Path" -ForegroundColor DarkYellow
        return
    }

    if (!$content.Contains($Old))
    {
        Write-Host "[NOT FOUND] $Path" -ForegroundColor Red
        return
    }

    $content = $content.Replace($Old, $New)
    Write-Utf8 $Path $content

    Write-Host "[OK] $Path" -ForegroundColor Green
}

function Ensure-Include
{
    param(
        [string]$Path,
        [string]$Include
    )

    $content = Read-Utf8 $Path

    if ($content.Contains($Include))
    {
        return
    }

    if ($content.Contains("#pragma once"))
    {
        $content = $content.Replace(
            "#pragma once",
            "#pragma once`r`n`r`n$Include"
        )
    }
    else
    {
        $content = "$Include`r`n$content"
    }

    Write-Utf8 $Path $content
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Metin2 x64 Hardening - Phase 1" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan


# ------------------------------------------------------------
# 1. ParticleProperty.h
# tek warning noktasi tum solution boyunca tekrar ediyor
# ------------------------------------------------------------

$path = Join-Path $Src "EffectLib\ParticleProperty.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'return m_ImageVector.size();' `
    'return m2::narrow<uint32_t>(m_ImageVector.size());'


# ------------------------------------------------------------
# 2. MapOutdoor.h
# internal index/order => size_t
# public legacy count => checked uint32_t
# ------------------------------------------------------------

$path = Join-Path $Src "GameLib\MapOutdoor.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'uint32_t lhsTerrainNumOrder = 0, rhsTerrainNumOrder = 0;' `
    'std::size_t lhsTerrainNumOrder = 0, rhsTerrainNumOrder = 0;'

Replace-Once `
    $path `
    'uint32_t GetMonsterAreaInfoCount() { return m_MonsterAreaInfoPtrVector.size(); }' `
    'uint32_t GetMonsterAreaInfoCount() { return m2::narrow<uint32_t>(m_MonsterAreaInfoPtrVector.size()); }'


# ------------------------------------------------------------
# 3. PythonPlayer.h
# API uint32_t kaliyor, STL size checked narrow
# ------------------------------------------------------------

$path = Join-Path $Src "UserInterface\PythonPlayer.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'uint32_t GetShopSearchResultCount() noexcept { return m_ItemInstanceVector.size(); }' `
    'uint32_t GetShopSearchResultCount() noexcept { return m2::narrow<uint32_t>(m_ItemInstanceVector.size()); }'

Replace-Once `
    $path `
    'uint32_t GetShopSearchResultMaxCount() noexcept { return m_ItemInstanceVector.size(); }' `
    'uint32_t GetShopSearchResultMaxCount() noexcept { return m2::narrow<uint32_t>(m_ItemInstanceVector.size()); }'


# ------------------------------------------------------------
# 4. PythonWindow.h
# ------------------------------------------------------------

$path = Join-Path $Src "EterPythonLib\PythonWindow.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'int GetChildCount() const { return m_pChildList.size(); }' `
    'int GetChildCount() const { return m2::narrow<int>(m_pChildList.size()); }'


# Case-sensitive duplicate folder compatibility
$path2 = Join-Path $Src "eterPythonLib\PythonWindow.h"

if ((Test-Path $path2) -and ($path2 -ne $path))
{
    Ensure-Include $path2 '#include "../EterBase/X64Compat.h"'

    Replace-Once `
        $path2 `
        'int GetChildCount() const { return m_pChildList.size(); }' `
        'int GetChildCount() const { return m2::narrow<int>(m_pChildList.size()); }'
}


# ------------------------------------------------------------
# 5. FlyingData.h
# ------------------------------------------------------------

$path = Join-Path $Src "GameLib\FlyingData.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'int GetAttachDataCount() { return m_AttachDataVector.size(); }' `
    'int GetAttachDataCount() { return m2::narrow<int>(m_AttachDataVector.size()); }'


# ------------------------------------------------------------
# 6. Pool.h
# ------------------------------------------------------------

$path = Join-Path $Src "EterLib\Pool.h"

Ensure-Include $path '#include "../EterBase/X64Compat.h"'

Replace-Once `
    $path `
    'uint32_t GetCapacity() { return m_kVct_pkData.size(); }' `
    'uint32_t GetCapacity() { return m2::narrow<uint32_t>(m_kVct_pkData.size()); }'


# ------------------------------------------------------------
# 7. Utils.h string_join
#
# uint32/int kullanmaya gerek yok.
# Ayrica bos container'da "-1" underflow riskini de kaldiriyor.
# ------------------------------------------------------------

$path = Join-Path $Src "EterBase\Utils.h"

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

Replace-Once $path $old $new


Write-Host ""
Write-Host "=============================================" -ForegroundColor Green
Write-Host " PHASE 1 TAMAMLANDI" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green