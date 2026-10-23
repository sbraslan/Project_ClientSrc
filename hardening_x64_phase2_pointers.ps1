$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Src  = Join-Path $Root "Src"

function ReadText {
    param([string]$Path)
    return [System.IO.File]::ReadAllText($Path)
}

function WriteText {
    param(
        [string]$Path,
        [string]$Text
    )

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Replace-All {
    param(
        [string]$Rel,
        [string]$Old,
        [string]$New
    )

    $Path = Join-Path $Src $Rel

    if (!(Test-Path $Path)) {
        Write-Host "[MISSING] $Rel" -ForegroundColor Red
        return
    }

    $c = ReadText $Path

    if ($c.Contains($Old)) {
        $c = $c.Replace($Old, $New)
        WriteText $Path $c
        Write-Host "[OK] $Rel" -ForegroundColor Green
    }
    elseif ($c.Contains($New)) {
        Write-Host "[SKIP] $Rel zaten duzeltilmis" -ForegroundColor Yellow
    }
    else {
        Write-Host "[NOT FOUND] $Rel" -ForegroundColor DarkYellow
    }
}

function Regex-All {
    param(
        [string]$Rel,
        [string]$Pattern,
        [string]$Replacement
    )

    $Path = Join-Path $Src $Rel

    if (!(Test-Path $Path)) {
        Write-Host "[MISSING] $Rel" -ForegroundColor Red
        return
    }

    $c = ReadText $Path

    $n = [regex]::Replace(
        $c,
        $Pattern,
        $Replacement,
        [System.Text.RegularExpressions.RegexOptions]::Multiline
    )

    if ($n -ne $c) {
        WriteText $Path $n
        Write-Host "[OK] $Rel regex" -ForegroundColor Green
    }
    else {
        Write-Host "[SKIP] $Rel regex" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " METIN2 X64 POINTER HARDENING - PHASE 2" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan


# ============================================================
# 1. Merkezi Python pointer API
# ============================================================

$rel = "ScriptLib\PythonUtils.h"

$anchor = @'
bool PyTuple_GetBoolean(PyObject * poArgs, int pos, bool * ret);
'@

$replacement = @'
bool PyTuple_GetBoolean(PyObject * poArgs, int pos, bool * ret);

template <typename T>
bool PyTuple_GetPointer(PyObject * poArgs, int pos, T ** ret)
{
	if (!ret)
		return false;

#if defined(_WIN64)
	if (pos < 0 || pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);
	if (!poItem)
		return false;

	void * pValue = PyLong_AsVoidPtr(poItem);
	if (PyErr_Occurred())
		return false;

	if (!pValue)
		return false;

	*ret = static_cast<T *>(pValue);
	return true;
#else
	int handle = 0;
	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	*ret = reinterpret_cast<T *>(
		static_cast<std::intptr_t>(handle)
	);
	return true;
#endif
}

template <typename T>
PyObject * Py_BuildPointer(T * ptr)
{
#if defined(_WIN64)
	return PyLong_FromVoidPtr(static_cast<void *>(ptr));
#else
	return Py_BuildValue(
		"i",
		static_cast<int>(
			reinterpret_cast<std::intptr_t>(ptr)
		)
	);
#endif
}
'@

Replace-All $rel $anchor $replacement

$path = Join-Path $Src $rel
$c = ReadText $path

if (!$c.Contains("#include <cstdint>")) {
    $c = $c.Replace(
        "#pragma once",
        "#pragma once`r`n`r`n#include <cstdint>"
    )

    WriteText $path $c
}


# ============================================================
# 2. PythonGraphicImageModule
# ============================================================

$rel = "EterPythonLib\PythonGraphicImageModule.cpp"

$old = @'
bool PyTuple_GetImageInstance(PyObject * poArgs, int pos, CGraphicImageInstance ** ppRetImageInstance)
{
	int handle;

	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	*ppRetImageInstance = reinterpret_cast<CGraphicImageInstance *>(handle);
	return true;
}
'@

$new = @'
bool PyTuple_GetImageInstance(PyObject * poArgs, int pos, CGraphicImageInstance ** ppRetImageInstance)
{
	return PyTuple_GetPointer(poArgs, pos, ppRetImageInstance);
}
'@

Replace-All $rel $old $new


$old = @'
bool PyTuple_GetExpandedImageInstance(PyObject * poArgs, int pos, CGraphicExpandedImageInstance ** ppRetImageInstance)
{
	int handle;

	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	if (!reinterpret_cast<CGraphicImageInstance *>(handle)->IsType(CGraphicExpandedImageInstance::Type()))
		return false;

	*ppRetImageInstance = reinterpret_cast<CGraphicExpandedImageInstance *>(handle);

	return true;
}
'@

$new = @'
bool PyTuple_GetExpandedImageInstance(PyObject * poArgs, int pos, CGraphicExpandedImageInstance ** ppRetImageInstance)
{
	CGraphicImageInstance * pImageInstance = nullptr;

	if (!PyTuple_GetPointer(poArgs, pos, &pImageInstance))
		return false;

	if (!pImageInstance->IsType(CGraphicExpandedImageInstance::Type()))
		return false;

	*ppRetImageInstance =
		reinterpret_cast<CGraphicExpandedImageInstance *>(pImageInstance);

	return true;
}
'@

Replace-All $rel $old $new

Replace-All $rel `
    'return Py_BuildValue("i", pImageInstance);' `
    'return Py_BuildPointer(pImageInstance);'

Replace-All $rel `
    'return Py_BuildValue("i", pImageInstance->GetGraphicImagePointer());' `
    'return Py_BuildPointer(pImageInstance->GetGraphicImagePointer());'


$old = @'
PyObject * grpImageGenerateFromHandle(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BadArgument();

	CGraphicImageInstance * pImageInstance = CGraphicImageInstance::New();
	pImageInstance->SetImagePointer(reinterpret_cast<CGraphicImage *>(iHandle));

	return Py_BuildValue("i", pImageInstance);
}
'@

$new = @'
PyObject * grpImageGenerateFromHandle(PyObject * poSelf, PyObject * poArgs)
{
	CGraphicImage * pImage = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pImage))
		return Py_BadArgument();

	CGraphicImageInstance * pImageInstance = CGraphicImageInstance::New();
	pImageInstance->SetImagePointer(pImage);

	return Py_BuildPointer(pImageInstance);
}
'@

Replace-All $rel $old $new


# ============================================================
# 3. PythonGraphicThingModule
# ============================================================

$rel = "EterPythonLib\PythonGraphicThingModule.cpp"

$old = @'
bool PyTuple_GetThingInstance(PyObject * poArgs, int pos, CGraphicThingInstance ** ppRetThingInstance)
{
	int handle;
	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	*ppRetThingInstance = reinterpret_cast<CGraphicThingInstance *>(handle);
	return true;
}
'@

$new = @'
bool PyTuple_GetThingInstance(PyObject * poArgs, int pos, CGraphicThingInstance ** ppRetThingInstance)
{
	return PyTuple_GetPointer(poArgs, pos, ppRetThingInstance);
}
'@

Replace-All $rel $old $new

Replace-All $rel `
    'return Py_BuildValue("i", pThingInstance);' `
    'return Py_BuildPointer(pThingInstance);'


# ============================================================
# 4. PythonGraphicTextModule
# ============================================================

$rel = "EterPythonLib\PythonGraphicTextModule.cpp"

$old = @'
bool PyTuple_GetTextInstance(PyObject * poArgs, int pos, CGraphicTextInstance ** ppTextInstance)
{
	int handle;
	if (!PyTuple_GetInteger(poArgs, pos, &handle))
		return false;

	if (!handle)
		return false;

	*ppTextInstance = reinterpret_cast<CGraphicTextInstance *>(handle);

	return true;
}
'@

$new = @'
bool PyTuple_GetTextInstance(PyObject * poArgs, int pos, CGraphicTextInstance ** ppTextInstance)
{
	return PyTuple_GetPointer(poArgs, pos, ppTextInstance);
}
'@

Replace-All $rel $old $new

Replace-All $rel `
    'return Py_BuildValue("i", pTextInstance);' `
    'return Py_BuildPointer(pTextInstance);'


# ============================================================
# 5. PythonGraphicModule / CTextBar
# ============================================================

$rel = "EterPythonLib\PythonGraphicModule.cpp"

$path = Join-Path $Src $rel
$c = ReadText $path

$c = [regex]::Replace(
    $c,
    'int iHandle;\r?\n\s*if \(!PyTuple_GetInteger\(poArgs, 0, &iHandle\)\)\r?\n\s*return Py_BuildException\(\);',
    "CTextBar * pTextBar = nullptr;`r`n`tif (!PyTuple_GetPointer(poArgs, 0, &pTextBar))`r`n`t`treturn Py_BuildException();"
)

$c = [regex]::Replace(
    $c,
    '\r?\n\s*auto \* pTextBar = reinterpret_cast<CTextBar \*>\(iHandle\);',
    ''
)

$c = $c.Replace(
    'return Py_BuildValue("i", pTextBar);',
    'return Py_BuildPointer(pTextBar);'
)

$c = $c.Replace(
    'return Py_BuildValue("i", nullptr);',
    'return Py_BuildPointer<CTextBar>(nullptr);'
)

WriteText $path $c
Write-Host "[OK] PythonGraphicModule.cpp" -ForegroundColor Green


# ============================================================
# 6. PythonWindowManager
# ============================================================

$rel = "EterPythonLib\PythonWindowManagerModule.cpp"

$old = @'
bool PyTuple_GetWindow(PyObject * poArgs, int pos, UI::CWindow ** ppRetWindow)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, pos, &iHandle))
		return false;
	if (!iHandle)
		return false;

	*ppRetWindow = reinterpret_cast<UI::CWindow *>(iHandle);
	return true;
}
'@

$new = @'
bool PyTuple_GetWindow(PyObject * poArgs, int pos, UI::CWindow ** ppRetWindow)
{
	return PyTuple_GetPointer(poArgs, pos, ppRetWindow);
}
'@

Replace-All $rel $old $new

Replace-All $rel `
    'return Py_BuildValue("i", pWindow);' `
    'return Py_BuildPointer(pWindow);'


$path = Join-Path $Src $rel
$c = ReadText $path

$c = [regex]::Replace(
    $c,
    'int iImageHandle;\r?\n\s*if \(!PyTuple_GetInteger\(poArgs, 5, &iImageHandle\)\)\r?\n\s*return Py_BuildException\(\);',
    "CGraphicImage * pImage = nullptr;`r`n`tif (!PyTuple_GetPointer(poArgs, 5, &pImage))`r`n`t`treturn Py_BuildException();"
)

$c = $c.Replace(
    '(CGraphicImage *)iImageHandle',
    'pImage'
)

WriteText $path $c
Write-Host "[OK] Window image handles" -ForegroundColor Green


# ============================================================
# 7. Item icon pointer API
# ============================================================

$rel = "UserInterface\PythonItemModule.cpp"

Replace-All $rel `
    'return Py_BuildValue("i", pItemData->GetIconImage());' `
    'return Py_BuildPointer(pItemData->GetIconImage());'

Replace-All $rel `
    'return Py_BuildValue("i", pImageInstance);' `
    'return Py_BuildPointer(pImageInstance);'


$old = @'
PyObject * itemDeleteIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BadArgument();

	CGraphicImageInstance::Delete((CGraphicImageInstance *) iHandle);

	return Py_BuildNone();
}
'@

$new = @'
PyObject * itemDeleteIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	CGraphicImageInstance * pImageInstance = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pImageInstance))
		return Py_BadArgument();

	CGraphicImageInstance::Delete(pImageInstance);

	return Py_BuildNone();
}
'@

Replace-All $rel $old $new


# ============================================================
# 8. Skill icon pointer API
# ============================================================

$rel = "UserInterface\PythonSkill.cpp"

Replace-All $rel `
    'return Py_BuildValue("i", c_pSkillData->pImage);' `
    'return Py_BuildPointer(c_pSkillData->pImage);'

Replace-All $rel `
    'return Py_BuildValue("i", c_pSkillData->GradeData[iGradeIndex].pImage);' `
    'return Py_BuildPointer(c_pSkillData->GradeData[iGradeIndex].pImage);'

Replace-All $rel `
    'return Py_BuildValue("i", pImageInstance);' `
    'return Py_BuildPointer(pImageInstance);'

Replace-All $rel `
    'return Py_BuildValue("i", c_pSkillData->pImage);' `
    'return Py_BuildPointer(c_pSkillData->pImage);'


$old = @'
PyObject * skillDeleteIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BadArgument();

	CGraphicImageInstance::Delete((CGraphicImageInstance *) iHandle);
	return Py_BuildNone();
}
'@

$new = @'
PyObject * skillDeleteIconInstance(PyObject * poSelf, PyObject * poArgs)
{
	CGraphicImageInstance * pImageInstance = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pImageInstance))
		return Py_BadArgument();

	CGraphicImageInstance::Delete(pImageInstance);
	return Py_BuildNone();
}
'@

Replace-All $rel $old $new


# ============================================================
# 9. TextLineLoader pointer
# ============================================================

$rel = "UserInterface\PythonApplicationModule.cpp"

Replace-All $rel `
    'return Py_BuildValue("i", (int) pTextLineLoader);' `
    'return Py_BuildPointer(pTextLineLoader);'


$old = @'
PyObject * appCloseTextFile(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BuildException();

	auto * pTextFileLoader = (CTextLineLoader *) iHandle;
	delete pTextFileLoader;

	return Py_BuildNone();
}
'@

$new = @'
PyObject * appCloseTextFile(PyObject * poSelf, PyObject * poArgs)
{
	CTextLineLoader * pTextFileLoader = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pTextFileLoader))
		return Py_BuildException();

	delete pTextFileLoader;

	return Py_BuildNone();
}
'@

Replace-All $rel $old $new


$old = @'
PyObject * appGetTextFileLineCount(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BuildException();

	auto * pTextFileLoader = (CTextLineLoader *) iHandle;
	return Py_BuildValue("i", pTextFileLoader->GetLineCount());
}
'@

$new = @'
PyObject * appGetTextFileLineCount(PyObject * poSelf, PyObject * poArgs)
{
	CTextLineLoader * pTextFileLoader = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pTextFileLoader))
		return Py_BuildException();

	return Py_BuildValue("i", pTextFileLoader->GetLineCount());
}
'@

Replace-All $rel $old $new


$old = @'
PyObject * appGetTextFileLine(PyObject * poSelf, PyObject * poArgs)
{
	int iHandle;
	if (!PyTuple_GetInteger(poArgs, 0, &iHandle))
		return Py_BuildException();
	int iLineIndex;
	if (!PyTuple_GetInteger(poArgs, 1, &iLineIndex))
		return Py_BuildException();

	auto * pTextFileLoader = (CTextLineLoader *) iHandle;
	return Py_BuildValue("s", pTextFileLoader->GetLine(iLineIndex));
}
'@

$new = @'
PyObject * appGetTextFileLine(PyObject * poSelf, PyObject * poArgs)
{
	CTextLineLoader * pTextFileLoader = nullptr;

	if (!PyTuple_GetPointer(poArgs, 0, &pTextFileLoader))
		return Py_BuildException();

	int iLineIndex;
	if (!PyTuple_GetInteger(poArgs, 1, &iLineIndex))
		return Py_BuildException();

	return Py_BuildValue("s", pTextFileLoader->GetLine(iLineIndex));
}
'@

Replace-All $rel $old $new


# ============================================================
# 10. Wiki render-target pointer
# ============================================================

Replace-All `
    "UserInterface\PythonWikiRenderTarget.h" `
    'void		RegisterRenderModule(int module_id, int module_wnd);' `
    'void		RegisterRenderModule(int module_id, UI::CUiWikiRenderTarget * module_wnd);'


Replace-All `
    "UserInterface\PythonWikiRenderTarget.cpp" `
    'void CPythonWikiRenderTarget::RegisterRenderModule(int module_id, int module_wnd)' `
    'void CPythonWikiRenderTarget::RegisterRenderModule(int module_id, UI::CUiWikiRenderTarget * module_wnd)'


Replace-All `
    "UserInterface\PythonWikiRenderTarget.cpp" `
    'UI::CUiWikiRenderTarget* _hwn = (UI::CUiWikiRenderTarget*)module_wnd;' `
    'UI::CUiWikiRenderTarget * _hwn = module_wnd;'


$old = @'
	int hwnd;
	if (!PyTuple_GetInteger(poArgs, 1, &hwnd))
		return Py_BadArgument();

	CPythonWikiRenderTarget::Instance().RegisterRenderModule(moduleID, hwnd);
'@

$new = @'
	UI::CUiWikiRenderTarget * hwnd = nullptr;
	if (!PyTuple_GetPointer(poArgs, 1, &hwnd))
		return Py_BadArgument();

	CPythonWikiRenderTarget::Instance().RegisterRenderModule(moduleID, hwnd);
'@

Replace-All "UserInterface\PythonWikiModule.cpp" $old $new


# ============================================================
# 11. Random seed: HANDLE truncation yok
# ============================================================

$rel = "UserInterface\UserInterface.cpp"

$old = @'
	uint32_t dwRandSeed = time(nullptr) + uint32_t(GetCurrentProcess());
	srandom(dwRandSeed);
	srand(random());
'@

$new = @'
	const std::uint64_t currentTime =
		static_cast<std::uint64_t>(time(nullptr));

	const uint32_t dwRandSeed =
		static_cast<uint32_t>(
			(currentTime ^ (currentTime >> 32)) +
			static_cast<std::uint64_t>(GetCurrentProcessId())
		);

	srandom(dwRandSeed);
	srand(random());
'@

Replace-All $rel $old $new


# ============================================================
# 12. IME - HKL sabitleri pointer-width safe
# ============================================================

$rel = "EterLib\IME.cpp"
$path = Join-Path $Src $rel
$c = ReadText $path

$hklValues = @(
    "0xE0060404",
    "0xE0080404",
    "0xE0090404",
    "0xE00A0404",
    "0xE00B0404",
    "0xE00E0804",
    "0xE0210804",
    "0xE0220804",
    "0xE0230804"
)

foreach ($value in $hklValues) {
    $c = $c.Replace(
        "((HKL) $value)",
        "reinterpret_cast<HKL>(static_cast<ULONG_PTR>(${value}u))"
    )
}

$c = $c.Replace(
    '(LPWSTR)((uint32_t) lpCandidateList + lpCandidateList->dwOffset[i])',
    'reinterpret_cast<LPWSTR>(reinterpret_cast<BYTE *>(lpCandidateList) + lpCandidateList->dwOffset[i])'
)

$c = $c.Replace(
    'const uint32_t dwLang = ((uint32_t) hkl & 0xffff);',
    'const uint32_t dwLang = static_cast<uint32_t>(reinterpret_cast<ULONG_PTR>(hkl) & 0xffffu);'
)

$c = $c.Replace(
    '(0xF0000000 & (uint32_t) ms_hklCurrent)',
    '(reinterpret_cast<ULONG_PTR>(ms_hklCurrent) & static_cast<ULONG_PTR>(0xF0000000u))'
)

WriteText $path $c
Write-Host "[OK] IME pointer-width conversions" -ForegroundColor Green


# ============================================================
# 13. IME GetVersionExA x64 build path
# x64 Windows her zaman NT/Unicode IMM platformudur.
# Legacy Win32 yolunu koruyoruz.
# ============================================================

$old = @'
	ms_stOSVI.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	GetVersionExA(&ms_stOSVI);

	bool bUnicodeImm = false;
	// IMM in NT or Win98 supports Unicode
	if (ms_stOSVI.dwPlatformId == VER_PLATFORM_WIN32_NT || (ms_stOSVI.dwMajorVersion > 4) ||
		((ms_stOSVI.dwMajorVersion == 4) && (ms_stOSVI.dwMinorVersion > 0)))
		bUnicodeImm = true;
'@

$new = @'
	bool bUnicodeImm = true;

#if !defined(_WIN64)
	ms_stOSVI.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	GetVersionExA(&ms_stOSVI);

	// Legacy Win32 compatibility.
	bUnicodeImm =
		ms_stOSVI.dwPlatformId == VER_PLATFORM_WIN32_NT ||
		ms_stOSVI.dwMajorVersion > 4 ||
		(ms_stOSVI.dwMajorVersion == 4 && ms_stOSVI.dwMinorVersion > 0);
#endif
'@

Replace-All $rel $old $new


$old = @'
				OSVERSIONINFOA osi;
				osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
				GetVersionExA(&osi);

				const int nTcharSize = (osi.dwPlatformId == VER_PLATFORM_WIN32_NT) ? sizeof(wchar_t) : sizeof(char);
'@

$new = @'
#if defined(_WIN64)
				const int nTcharSize = sizeof(wchar_t);
				const bool bPlatformUnicodeIME = true;
#else
				OSVERSIONINFOA osi{};
				osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
				GetVersionExA(&osi);

				const int nTcharSize =
					(osi.dwPlatformId == VER_PLATFORM_WIN32_NT)
						? sizeof(wchar_t)
						: sizeof(char);

				const bool bPlatformUnicodeIME =
					osi.dwPlatformId == VER_PLATFORM_WIN32_NT;
#endif
'@

Replace-All $rel $old $new

Replace-All `
    $rel `
    'bUnicodeIme = (osi.dwPlatformId == VER_PLATFORM_WIN32_NT) ? true : false;' `
    'bUnicodeIme = bPlatformUnicodeIME;'


Write-Host ""
Write-Host "=============================================" -ForegroundColor Green
Write-Host " PHASE 2 POINTER HARDENING TAMAMLANDI" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green