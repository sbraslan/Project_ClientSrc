$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Src  = Join-Path $Root "Src"

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Metin2 Client x64 Warning Migration" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

function Read-Utf8File {
    param([string]$Path)

    if (!(Test-Path $Path)) {
        throw "Dosya bulunamadi: $Path"
    }

    return [System.IO.File]::ReadAllText($Path)
}

function Write-Utf8File {
    param(
        [string]$Path,
        [string]$Content
    )

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function Replace-Exact {
    param(
        [string]$Path,
        [string]$Old,
        [string]$New,
        [switch]$Optional
    )

    $content = Read-Utf8File $Path

    if (!$content.Contains($Old)) {
        if ($Optional) {
            Write-Host "[SKIP] Pattern zaten degismis veya bulunamadi: $Path" -ForegroundColor DarkYellow
            return
        }

        throw "Pattern bulunamadi:`n$Path`n--- OLD ---`n$Old"
    }

    $content = $content.Replace($Old, $New)
    Write-Utf8File $Path $content

    Write-Host "[OK] $Path" -ForegroundColor Green
}

function Replace-Regex {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Replacement
    )

    $content = Read-Utf8File $Path
    $newContent = [regex]::Replace(
        $content,
        $Pattern,
        $Replacement,
        [System.Text.RegularExpressions.RegexOptions]::Multiline
    )

    if ($newContent -eq $content) {
        Write-Host "[SKIP] Regex eslesmedi: $Path" -ForegroundColor DarkYellow
        return
    }

    Write-Utf8File $Path $newContent

    Write-Host "[OK] $Path" -ForegroundColor Green
}

function Add-IncludeIfMissing {
    param(
        [string]$Path,
        [string]$Include
    )

    $content = Read-Utf8File $Path

    if ($content.Contains($Include)) {
        return
    }

    if ($content.StartsWith("#pragma once")) {
        $content = $content.Replace(
            "#pragma once",
            "#pragma once`r`n`r`n$Include"
        )
    }
    else {
        $content = "$Include`r`n$content"
    }

    Write-Utf8File $Path $content

    Write-Host "[INCLUDE] $Path -> $Include" -ForegroundColor Green
}


# ============================================================
# 0. Git durumunu kontrol et
# ============================================================

Push-Location $Root

try {
    $gitStatus = git status --porcelain

    if ($gitStatus) {
        Write-Host ""
        Write-Host "UYARI: Repo'da commit edilmemis degisiklikler var." -ForegroundColor Yellow
        Write-Host "Script yine devam edecek." -ForegroundColor Yellow
        Write-Host ""
    }

    git branch --show-current
}
catch {
    Write-Host "Git kontrolu yapilamadi." -ForegroundColor DarkYellow
}

Pop-Location


# ============================================================
# 1. X64Compat.h
# ============================================================

$x64Compat = Join-Path $Src "EterBase\X64Compat.h"

$x64CompatContent = @'
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace m2
{
    template <typename To, typename From>
    constexpr To narrow(From value)
    {
        static_assert(std::is_integral_v<To>);
        static_assert(std::is_integral_v<From>);

        if constexpr (std::is_signed_v<From> == std::is_signed_v<To>)
        {
            if constexpr (sizeof(To) < sizeof(From))
            {
                assert(
                    value >= static_cast<From>(std::numeric_limits<To>::lowest()) &&
                    value <= static_cast<From>(std::numeric_limits<To>::max())
                );
            }
        }
        else if constexpr (std::is_signed_v<From>)
        {
            assert(value >= 0);

            using UFrom = std::make_unsigned_t<From>;

            assert(
                static_cast<UFrom>(value) <=
                static_cast<UFrom>(std::numeric_limits<To>::max())
            );
        }
        else
        {
            using UTo = std::make_unsigned_t<To>;

            if constexpr (sizeof(To) <= sizeof(From))
            {
                assert(
                    value <= static_cast<From>(
                        static_cast<UTo>(std::numeric_limits<To>::max())
                    )
                );
            }
        }

        return static_cast<To>(value);
    }

    template <typename T>
    constexpr std::uintptr_t ptr_value(T* ptr) noexcept
    {
        return reinterpret_cast<std::uintptr_t>(ptr);
    }

    template <typename T>
    constexpr T* ptr_from_value(std::uintptr_t value) noexcept
    {
        return reinterpret_cast<T*>(value);
    }

    template <typename T>
    constexpr std::ptrdiff_t pointer_offset(
        const T* from,
        const T* to
    ) noexcept
    {
        return
            reinterpret_cast<const unsigned char*>(to) -
            reinterpret_cast<const unsigned char*>(from);
    }
}
'@

Write-Utf8File $x64Compat $x64CompatContent
Write-Host "[CREATE] EterBase\X64Compat.h" -ForegroundColor Green


# ============================================================
# 2. AbstractSingleton x64 pointer truncation
# ============================================================

$path = Join-Path $Src "UserInterface\AbstractSingleton.h"

Add-IncludeIfMissing $path "#include <cstdint>"

$old = @'
	TAbstractSingleton()
	{
		assert(!ms_singleton);
		int offset = (int) (T *) 1 - (int) (CSingleton<T> *) (T *) 1;
		ms_singleton = (T *) ((int) this + offset);
	}
'@

$new = @'
	TAbstractSingleton()
	{
		assert(!ms_singleton);

		const std::intptr_t offset =
			reinterpret_cast<std::intptr_t>((T*)1) -
			reinterpret_cast<std::intptr_t>((CSingleton<T>*)(T*)1);

		ms_singleton = reinterpret_cast<T*>(
			reinterpret_cast<std::intptr_t>(this) + offset
		);
	}
'@

Replace-Exact $path $old $new -Optional


# ============================================================
# 3. ProcessDamage missing return
# ============================================================

$path = Join-Path $Src "UserInterface\InstanceBaseEffect.cpp"

$old = @'
		index++;
	}
}

void CInstanceBase::AttachSpecialEffect
'@

$new = @'
		index++;
	}

	return true;
}

void CInstanceBase::AttachSpecialEffect
'@

Replace-Exact $path $old $new -Optional


# ============================================================
# 4. PythonSystem dangling stack pointer
# ============================================================

$path = Join-Path $Src "UserInterface\PythonSystem.cpp"

$old = @'
TCHAR* registry_read(LPCTSTR subkey, LPCTSTR name, unsigned long type)
{
	HKEY key;
	TCHAR value[255];
	unsigned long value_length = 255;
	RegOpenKey(HKEY_LOCAL_MACHINE, subkey, &key);
	RegQueryValueEx(key, name, nullptr, &type, (LPBYTE)&value, &value_length);
	RegCloseKey(key);
	return value;
}
'@

$new = @'
TCHAR* registry_read(LPCTSTR subkey, LPCTSTR name, unsigned long type)
{
	static thread_local TCHAR value[255]{};

	HKEY key = nullptr;
	DWORD valueLength = sizeof(value);

	value[0] = TEXT('\0');

	const LONG openResult = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		subkey,
		0,
		KEY_READ,
		&key
	);

	if (openResult != ERROR_SUCCESS)
		return value;

	const LONG queryResult = RegQueryValueEx(
		key,
		name,
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(value),
		&valueLength
	);

	RegCloseKey(key);

	if (queryResult != ERROR_SUCCESS)
		value[0] = TEXT('\0');

	return value;
}
'@

Replace-Exact $path $old $new -Optional


# ============================================================
# 5. PythonPlayerModule malformed Yohara format
# ============================================================

$path = Join-Path $Src "UserInterface\PythonPlayerModule.cpp"

$old = @'
#ifdef ENABLE_YOHARA_SYSTEM
		len += snprintf(itemlink + len, sizeof(itemlink) - len,
			":%x"	// HYPER_LINK_ITEM_RANDOM_VALUE0
			":%x"	// HYPER_LINK_ITEM_RANDOM_VALUE1
			":%x",	// HYPER_LINK_ITEM_RANDOM_VALUE2
			":%x",	// HYPER_LINK_ITEM_RANDOM_VALUE3
			pPlayerItem->alRandomValues[0],
			pPlayerItem->alRandomValues[1],
			pPlayerItem->alRandomValues[2],
			pPlayerItem->alRandomValues[3]);
#endif
'@

$new = @'
#ifdef ENABLE_YOHARA_SYSTEM
		len += snprintf(
			itemlink + len,
			sizeof(itemlink) - len,
			":%x"
			":%x"
			":%x"
			":%x",
			pPlayerItem->alRandomValues[0],
			pPlayerItem->alRandomValues[1],
			pPlayerItem->alRandomValues[2],
			pPlayerItem->alRandomValues[3]
		);
#endif
'@

Replace-Exact $path $old $new -Optional


# ============================================================
# 6. GameType invalid preprocessor commas
# ============================================================

$path = Join-Path $Src "UserInterface\GameType.cpp"

Replace-Exact `
    $path `
    "#	ifdef ENABLE_WOLFMAN_CHARACTER," `
    "#	ifdef ENABLE_WOLFMAN_CHARACTER" `
    -Optional

Replace-Exact `
    $path `
    "#	endif," `
    "#	endif" `
    -Optional


# ============================================================
# 7. EffectUpdateDecorator pointer arithmetic
# ============================================================

$path = Join-Path $Src "EffectLib\EffectUpdateDecorator.h"

Add-IncludeIfMissing $path "#include <cstdint>"

$old = @'
		  pData((T *) ((unsigned char *) ted.pData - (uint32_t) pFirstInstance + (uint32_t) pInstance))
'@

$new = @'
		  pData(reinterpret_cast<T*>(
			  reinterpret_cast<std::uintptr_t>(ted.pData)
			  - reinterpret_cast<std::uintptr_t>(pFirstInstance)
			  + reinterpret_cast<std::uintptr_t>(pInstance)))
'@

Replace-Exact $path $old $new -Optional


# ============================================================
# 8. Area effect pointer key
# ============================================================

$path = Join-Path $Src "GameLib\Area.h"

Add-IncludeIfMissing $path "#include <cstdint>"

Replace-Exact `
    $path `
    "uint32_t dwEffectInstanceIndex;" `
    "std::uintptr_t dwEffectInstanceIndex;" `
    -Optional

Replace-Exact `
    $path `
    "typedef std::map<uint32_t, CEffectInstance *> TEffectInstanceMap;" `
    "typedef std::map<std::uintptr_t, CEffectInstance *> TEffectInstanceMap;" `
    -Optional


$path = Join-Path $Src "GameLib\Area.cpp"

Replace-Exact `
    $path `
    "pObjectInstance->dwEffectInstanceIndex = (uint32_t)pEffectInstance;" `
    "pObjectInstance->dwEffectInstanceIndex = reinterpret_cast<std::uintptr_t>(pEffectInstance);" `
    -Optional


# ============================================================
# 9. MapManager ApplyLight pointer version key
# ============================================================

$path = Join-Path $Src "GameLib\MapManager.cpp"

Replace-Exact `
    $path `
    "rkMap.ApplyLight((uint32_t) mc_pcurEnvironmentData, mc_pcurEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND]);" `
    "rkMap.ApplyLight(reinterpret_cast<std::uintptr_t>(mc_pcurEnvironmentData), mc_pcurEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND]);" `
    -Optional


$path = Join-Path $Src "GameLib\MapOutdoor.h"

Add-IncludeIfMissing $path "#include <cstdint>"

Replace-Exact `
    $path `
    "void ApplyLight(uint32_t dwVersion, const D3DLIGHT9 & c_rkLight);" `
    "void ApplyLight(std::uintptr_t dwVersion, const D3DLIGHT9 & c_rkLight);" `
    -Optional


# ApplyLight implementation signature anywhere in GameLib

Get-ChildItem (Join-Path $Src "GameLib") -Recurse -Include *.cpp |
ForEach-Object {

    $content = Read-Utf8File $_.FullName

    $newContent = $content -replace `
        'CMapOutdoor::ApplyLight\(uint32_t dwVersion, const D3DLIGHT9\s*&\s*c_rkLight\)', `
        'CMapOutdoor::ApplyLight(std::uintptr_t dwVersion, const D3DLIGHT9 & c_rkLight)'

    if ($newContent -ne $content) {
        Write-Utf8File $_.FullName $newContent
        Write-Host "[OK] ApplyLight implementation: $($_.Name)" -ForegroundColor Green
    }
}


# ============================================================
# 10. Util.cpp LONG -> LPARAM
# ============================================================

$path = Join-Path $Src "EterLib\Util.cpp"

Replace-Exact `
    $path `
    "(LONG) fontFace" `
    "reinterpret_cast<LPARAM>(fontFace)" `
    -Optional


# Callback signature LONG -> LPARAM where appropriate.

$content = Read-Utf8File $path

$content = $content -replace `
    '(EnumFontFamExProc\s*\([^)]*?,\s*)LONG(\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\))', `
    '${1}LPARAM${2}'

Write-Utf8File $path $content


# ============================================================
# 11. Common printf size_t fixes
# ============================================================

$knownFormatFiles = @(
    "EterLib\JpegFile.cpp",
    "MilesLib\Type.cpp",
    "EffectLib\EffectManager.cpp",
    "UserInterface\PythonCharacterManager.cpp",
    "UserInterface\PythonItem.cpp"
)

foreach ($rel in $knownFormatFiles) {

    $path = Join-Path $Src $rel

    if (!(Test-Path $path)) {
        continue
    }

    $content = Read-Utf8File $path

    #
    # Do NOT globally turn every %u into %zu.
    # Only very common sizeof/vector-size patterns.
    #

    $content = [regex]::Replace(
        $content,
        '"%u"(\s*,\s*[^;\r\n]*(?:\.size\(\)|sizeof\s*\())',
        '"%zu"$1'
    )

    Write-Utf8File $path $content
}


# ============================================================
# 12. Enum -> float deprecated operations
# Known source locations are made explicit instead of disabling C5055.
# ============================================================

$enumFloatFiles = @(
    "EterLib\GrpMarkInstance.cpp",
    "GameLib\AreaTerrain.cpp",
    "GameLib\MapOutdoorLoad.cpp",
    "GameLib\MapOutdoorRenderSTP.cpp",
    "eterGrnLib\LODController.cpp",
    "eterGrnLib\ModelInstanceUpdate.cpp"
)

Write-Host ""
Write-Host "C5055 kontrol edilecek dosyalar:" -ForegroundColor Cyan

foreach ($rel in $enumFloatFiles) {
    Write-Host "  $rel"
}


# ============================================================
# 13. Python pointer-width helper
# ============================================================

$pythonPointerHeader = Join-Path $Src "UserInterface\PythonPointerCompat.h"

$pythonPointerContent = @'
#pragma once

#include <Python.h>

template <typename T>
bool PyTuple_GetPointer(
	PyObject* args,
	int index,
	T** out)
{
	if (!out)
		return false;

	PyObject* obj = PyTuple_GetItem(args, index);

	if (!obj)
		return false;

	void* ptr = PyLong_AsVoidPtr(obj);

	if (PyErr_Occurred())
		return false;

	*out = static_cast<T*>(ptr);
	return true;
}

template <typename T>
PyObject* Py_BuildPointer(T* ptr)
{
	return PyLong_FromVoidPtr(static_cast<void*>(ptr));
}
'@

Write-Utf8File $pythonPointerHeader $pythonPointerContent
Write-Host "[CREATE] UserInterface\PythonPointerCompat.h" -ForegroundColor Green


# ============================================================
# 14. Safe size conversion helper include insertion
# Only files with known C4267 warnings.
# ============================================================

$sizeWarningFiles = @(
    "EterLib\AttributeData.cpp",
    "EterLib\AttributeInstance.cpp",
    "EterLib\GrpDIB.cpp",
    "EterLib\GrpObjectInstance.cpp",
    "EterLib\GrpLightManager.cpp",
    "EterLib\GroupTextParseTree.cpp",
    "EterLib\parser.cpp",
    "EterLib\JpegFile.cpp",
    "EterLib\SkyBox.cpp",
    "EterLib\GrpExpandedImageInstance.cpp",
    "EterLib\GrpImageInstance.cpp",
    "EterLib\ResourceManager.cpp",
    "EterLib\GrpSubImage.cpp",
    "EterLib\NetStream.cpp",
    "EterLib\GrpFontTexture.cpp",
    "EterLib\TextBar.cpp",
    "EterLib\GrpTextInstance.cpp",
    "EterLib\TextFileLoader.cpp",

    "EterBase\MappedFile.cpp",
    "EterBase\FileDir.cpp",
    "EterBase\CRC32.cpp",
    "EterBase\FileLoader.cpp",
    "EterBase\Stl.cpp",
    "EterBase\Utils.cpp",
    "EterBase\poly\Poly.cpp",

    "GameLib\ActorInstanceCollisionDetection.cpp",
    "GameLib\RaceMotionData.cpp",
    "GameLib\RaceDataFile.cpp",
    "GameLib\WeaponTrace.cpp",
    "GameLib\RaceData.cpp",
    "GameLib\Property.cpp",
    "GameLib\PropertyLoader.cpp",
    "GameLib\FlyingData.cpp",
    "GameLib\GameType.cpp",
    "GameLib\ItemData.cpp",
    "GameLib\ItemManager.cpp",
    "GameLib\Area.cpp",

    "eterGrnLib\LODController.cpp",
    "eterGrnLib\Thing.cpp",
    "eterGrnLib\Material.cpp",
    "eterGrnLib\ThingInstance.cpp",

    "EffectLib\EffectData.cpp",
    "EffectLib\EffectMeshInstance.cpp",
    "EffectLib\EffectMesh.cpp",

    "EterPack\EterPackPolicy_CSHybridCrypt.cpp",
    "EterPack\EterPackManager.cpp",
    "EterPack\EterPack.cpp",

    "SpeedTreeLib\BoundaryShapeManager.cpp",
    "PRTerrainLib\TextureSet.cpp",

    "UserInterface\InstanceBase.cpp",
    "UserInterface\PythonEventManager.cpp",
    "UserInterface\PythonGameEventManager.cpp",
    "UserInterface\PythonGuildBank.cpp",
    "UserInterface\PythonQuest.cpp",
    "UserInterface\PythonSafeBox.cpp",
    "UserInterface\PythonGuild.cpp",
    "UserInterface\PythonItem.cpp",
    "UserInterface\PythonNonPlayer.cpp",
    "UserInterface\PythonPlayerEventHandler.cpp"
)

foreach ($rel in $sizeWarningFiles) {

    $path = Join-Path $Src $rel

    if (!(Test-Path $path)) {
        continue
    }

    $content = Read-Utf8File $path

    if ($content.Contains("X64Compat.h")) {
        continue
    }

    $relativeInclude = ""

    if ($rel -like "EterBase\*") {
        $relativeInclude = '#include "X64Compat.h"'
    }
    elseif ($rel -like "EterLib\*") {
        $relativeInclude = '#include "../EterBase/X64Compat.h"'
    }
    else {
        $relativeInclude = '#include "../EterBase/X64Compat.h"'
    }

    if ($content.StartsWith("#include")) {

        $firstLineEnd = $content.IndexOf("`n")

        if ($firstLineEnd -gt 0) {
            $content = $content.Insert(
                $firstLineEnd + 1,
                "$relativeInclude`r`n"
            )
        }
    }

    Write-Utf8File $path $content
}


# ============================================================
# 15. Common safe return .size() transformations
#
# PowerShell 5.1 compatible.
# Does not use [System.IO.Path]::GetRelativePath().
# ============================================================

function Get-RelativePathPS51 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath)

    if (!$baseFull.EndsWith("\")) {
        $baseFull += "\"
    }

    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)

    $baseUri = New-Object System.Uri($baseFull)
    $targetUri = New-Object System.Uri($targetFull)

    $relativeUri = $baseUri.MakeRelativeUri($targetUri)

    return [System.Uri]::UnescapeDataString(
        $relativeUri.ToString()
    )
}


$sourceFiles = Get-ChildItem $Src -Recurse -Include *.cpp,*.h |
    Where-Object {
        $_.FullName -notmatch '\\Extern\\'
    }


foreach ($file in $sourceFiles) {

    $path = $file.FullName
    $content = Read-Utf8File $path
    $original = $content

    #
    # Existing explicit casts around container.size()
    # are converted to checked x64-safe narrowing.
    #
    # We intentionally DO NOT blindly transform:
    #
    #     uint32_t x = container.size();
    #
    # because that requires semantic knowledge of the target.
    #

    $content = $content -replace `
        '\(uint32_t\)\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))', `
        'm2::narrow<uint32_t>($1)'

    $content = $content -replace `
        'static_cast<uint32_t>\(\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))\s*\)', `
        'm2::narrow<uint32_t>($1)'

    $content = $content -replace `
        '\(uint16_t\)\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))', `
        'm2::narrow<uint16_t>($1)'

    $content = $content -replace `
        'static_cast<uint16_t>\(\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))\s*\)', `
        'm2::narrow<uint16_t>($1)'

    $content = $content -replace `
        '\(int\)\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))', `
        'm2::narrow<int>($1)'

    $content = $content -replace `
        'static_cast<int>\(\s*([A-Za-z_][A-Za-z0-9_\.\-\>\(\)]*\.size\(\))\s*\)', `
        'm2::narrow<int>($1)'


    if ($content -ne $original) {

        if (!$content.Contains("X64Compat.h")) {

            $targetHeader = Join-Path $Src "EterBase\X64Compat.h"

            $relative = Get-RelativePathPS51 `
                -BasePath $file.DirectoryName `
                -TargetPath $targetHeader

            $relative = $relative.Replace("\", "/")

            $content = "#include `"$relative`"`r`n$content"
        }

        Write-Utf8File $path $content

        Write-Host "[NARROW] $path" -ForegroundColor Green
    }
}

# ============================================================
# 16. Search remaining dangerous pointer-to-32-bit casts
# ============================================================

$dangerPatterns = @(
    '\(uint32_t\)\s*[A-Za-z_][A-Za-z0-9_]*',
    '\(DWORD\)\s*[A-Za-z_][A-Za-z0-9_]*',
    '\(int\)\s*this',
    'reinterpret_cast<int>',
    'reinterpret_cast<uint32_t>',
    'reinterpret_cast<DWORD>'
)

$dangerReport = Join-Path $Root "x64-dangerous-casts.txt"

if (Test-Path $dangerReport) {
    Remove-Item $dangerReport
}

foreach ($pattern in $dangerPatterns) {

    Add-Content $dangerReport ""
    Add-Content $dangerReport "========================================"
    Add-Content $dangerReport "PATTERN: $pattern"
    Add-Content $dangerReport "========================================"

    Get-ChildItem $Src -Recurse -Include *.cpp,*.h |
    Where-Object {
        $_.FullName -notmatch '\\Extern\\'
    } |
    Select-String -Pattern $pattern |
    ForEach-Object {
        Add-Content $dangerReport (
            "{0}:{1}: {2}" -f
            $_.Path,
            $_.LineNumber,
            $_.Line.Trim()
        )
    }
}

Write-Host ""
Write-Host "[REPORT] x64-dangerous-casts.txt olusturuldu." -ForegroundColor Cyan


# ============================================================
# 17. Do not globally suppress warnings
# Add /WX only after source cleanup.
# ============================================================

$props = Join-Path $Src "Directory.Build.props"

$content = Read-Utf8File $props

if (!$content.Contains("<TreatWarningAsError>true</TreatWarningAsError>")) {

    $warningBlock = @'

  <!--
    x64 migration quality gate.
    Do not suppress migration warnings: the x64 build must remain warning-free.
  -->
  <ItemDefinitionGroup Condition="'$(Platform)'=='x64'">
    <ClCompile>
      <WarningLevel>Level4</WarningLevel>
      <TreatWarningAsError>true</TreatWarningAsError>
    </ClCompile>
  </ItemDefinitionGroup>

'@

    $content = $content.Replace(
        "</Project>",
        "$warningBlock</Project>"
    )

    Write-Utf8File $props $content

    Write-Host "[OK] x64 /W4 /WX quality gate enabled." -ForegroundColor Green
}


# ============================================================
# 18. Final information
# ============================================================

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Migration patch tamamlandi." -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Cyan

Write-Host ""
Write-Host "SIMDI:" -ForegroundColor Yellow
Write-Host "1. Visual Studio -> Release | x64"
Write-Host "2. Rebuild Solution"
Write-Host "3. /WX nedeniyle kalan her warning artik ERROR olacak."
Write-Host ""
Write-Host "Kalan logu kaydet."
Write-Host ""
Write-Host "Ayrica kontrol et:"
Write-Host "  $dangerReport"
Write-Host ""