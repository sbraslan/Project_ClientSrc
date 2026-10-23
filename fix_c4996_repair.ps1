$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Read-File($rel) {
    [IO.File]::ReadAllText((Join-Path $root $rel))
}

function Write-File($rel, $text) {
    [IO.File]::WriteAllText((Join-Path $root $rel), $text, $utf8)
}

function Replace-Literal($rel, $old, $new) {
    $s = Read-File $rel

    if ($s.Contains($old)) {
        $s = $s.Replace($old, $new)
        Write-File $rel $s
        Write-Host "[OK] $rel" -ForegroundColor Green
        return
    }

    if ($s.Contains($new)) {
        Write-Host "[SKIP] $rel" -ForegroundColor Yellow
        return
    }

    Write-Host "[NOT FOUND] $rel -> $old" -ForegroundColor DarkYellow
}

# ============================================================
# 1) DXTCImage.cpp
# strPixelFormat gerçekte CHAR* ama hedef buffer 32 byte.
# ============================================================

$f = "Src\EterImageLib\DXTCImage.cpp"
$s = Read-File $f

foreach ($dxt in @("DXT1","DXT2","DXT3","DXT4","DXT5")) {
    $s = $s.Replace(
        "m2::crt::copy_n(strPixelFormat, `"$dxt`", 31);",
        "strcpy_s(strPixelFormat, 32, `"$dxt`");"
    )
}

$s = $s.Replace(
    'm2::crt::copy(strPixelFormat, "Format Unknown");',
    'strcpy_s(strPixelFormat, 32, "Format Unknown");'
)

Write-File $f $s
Write-Host "[OK] DXTCImage pointer fixes" -ForegroundColor Green


# ============================================================
# 2) CRC32.cpp
# malloc + pointer helper kullanımını kaldır.
# Ayrıca mevcut memory-leak davranışını da ortadan kaldır.
# ============================================================

$f = "Src\EterBase\CRC32.cpp"
$s = Read-File $f

if (!$s.Contains("#include <string>")) {
    $s = $s.Replace(
        '#include "CRC32.h"',
        "#include `"CRC32.h`"`r`n#include <string>"
    )
}

$pattern = '(?s)char\*\s+rez\s*=\s*\(char\*\)buf;\s*if\s*\(name\s*!=\s*nullptr\)\s*\{\s*rez\s*=\s*\(char\*\)malloc\(1\s*\+\s*strlen\(buf\)\s*\+\s*strlen\(name\)\);\s*m2::crt::copy\(rez,\s*name\);\s*m2::crt::append\(rez,\s*buf\);\s*\}\s*int\s+lenSt\s*=\s*strlen\(rez\);\s*len\s*=\s*strlen\(rez\);'

$replacement = @'
std::string rezStorage;
	const char* rez = buf;

	if (name != nullptr)
	{
		rezStorage.reserve(strlen(name) + strlen(buf));
		rezStorage.assign(name);
		rezStorage.append(buf);
		rez = rezStorage.c_str();
	}

	len = strlen(rez);
'@

$new = [regex]::Replace($s, $pattern, $replacement, 1)

if ($new -eq $s) {
    Write-Host "[WARN] CRC32 block otomatik bulunamadi" -ForegroundColor Yellow
}
else {
    Write-File $f $new
    Write-Host "[OK] CRC32 pointer fix" -ForegroundColor Green
}


# ============================================================
# 3) IME wcscpy
# ============================================================

$f = "Src\EterLib\IME.cpp"
$s = Read-File $f

$s = $s.Replace(
    'wcscpy(m_wText + ms_curpos, m_wText + ms_curpos + eraseCount);',
    'wmemmove(m_wText + ms_curpos, m_wText + ms_curpos + eraseCount, static_cast<size_t>(ms_lastpos - ms_curpos - eraseCount + 1));'
)

$s = [regex]::Replace(
    $s,
    'wcscpy\(ms_wszCandidate\[j\],\s*(.*?)\);',
    'wcsncpy_s(ms_wszCandidate[j], _countof(ms_wszCandidate[j]), $1, _TRUNCATE);'
)

$s = [regex]::Replace(
    $s,
    'wcscpy\(CIME::ms_wszCandidate\[j\],\s*(.*?)\);',
    'wcsncpy_s(CIME::ms_wszCandidate[j], _countof(CIME::ms_wszCandidate[j]), $1, _TRUNCATE);'
)

Write-File $f $s
Write-Host "[OK] IME wcscpy fixes" -ForegroundColor Green


# ============================================================
# 4) localtime -> localtime_s
# ============================================================

$f = "Src\EterBase\Debug.cpp"
$s = Read-File $f

$s = $s.Replace(
    'struct tm ctm = *localtime(&ct);',
    "struct tm ctm{};`r`n`tlocaltime_s(&ctm, &ct);"
)

$s = $s.Replace(
    'struct tm * timeinfo = localtime(&rawtime);',
    "struct tm timeinfoStorage{};`r`n`tlocaltime_s(&timeinfoStorage, &rawtime);`r`n`tstruct tm * timeinfo = &timeinfoStorage;"
)

Write-File $f $s


$f = "Src\GameLib\MapOutdoorLoad.cpp"
$s = Read-File $f
$s = $s.Replace(
    'struct tm tm = *localtime(&ct);',
    "struct tm tm{};`r`n`t`tlocaltime_s(&tm, &ct);"
)
Write-File $f $s


$f = "Src\GameLib\Property.cpp"
$s = Read-File $f
$s = $s.Replace(
    "struct tm tm;`r`n`t" + 'tm = *localtime(&ct);',
    "struct tm tm{};`r`n`tlocaltime_s(&tm, &ct);"
)
Write-File $f $s


$f = "Src\EterPythonLib\PythonGraphicModule.cpp"
$s = Read-File $f
$s = $s.Replace(
    'struct tm * tmNow = localtime(&ct);',
    "struct tm tmNowStorage{};`r`n`tlocaltime_s(&tmNowStorage, &ct);`r`n`tstruct tm * tmNow = &tmNowStorage;"
)
Write-File $f $s

Write-Host "[OK] localtime_s fixes" -ForegroundColor Green


# ============================================================
# 5) freopen -> freopen_s
# ============================================================

$f = "Src\EterBase\Debug.cpp"
$s = Read-File $f

$s = $s.Replace(
    'if (freopen(szSyserrPath, "w", stderr) == nullptr)',
    "FILE* redirectedStderr = nullptr;`r`n`tif (freopen_s(&redirectedStderr, szSyserrPath, `"w`", stderr) != 0 || redirectedStderr == nullptr)"
)

$s = $s.Replace(
@'
	freopen("CONOUT$", "a", stdout);
	freopen("CONIN$", "r", stdin);
'@,
@'
	FILE* consoleOut = nullptr;
	FILE* consoleIn = nullptr;

	freopen_s(&consoleOut, "CONOUT$", "a", stdout);
	freopen_s(&consoleIn, "CONIN$", "r", stdin);
'@
)

Write-File $f $s
Write-Host "[OK] freopen_s fixes" -ForegroundColor Green


# ============================================================
# 6) _ecvt -> _ecvt_s
# ============================================================

$f = "Src\EterBase\Utils.cpp"
$s = Read-File $f

$s = $s.Replace(
    'm2::crt::copy(Temp, _ecvt(Index, 256, &dec, &sign));',
    '_ecvt_s(Temp, sizeof(Temp), static_cast<double>(Index), 256, &dec, &sign);'
)

$s = $s.Replace(
    'strcpy(Temp, _ecvt(Index, 256, &dec, &sign));',
    '_ecvt_s(Temp, sizeof(Temp), static_cast<double>(Index), 256, &dec, &sign);'
)

Write-File $f $s
Write-Host "[OK] ecvt_s fix" -ForegroundColor Green


# ============================================================
# 7) RaceMotionData size_t format
# ============================================================

$f = "Src\GameLib\RaceMotionData.cpp"
$s = Read-File $f

$s = $s.Replace(
    '"    MotionEventDataCount     %d\n", m_MotionEventDataVector.size()',
    '"    MotionEventDataCount     %zu\n", m_MotionEventDataVector.size()'
)

Write-File $f $s
Write-Host "[OK] RaceMotionData format fix" -ForegroundColor Green


# ============================================================
# 8) Verification
# ============================================================

Write-Host ""
Write-Host "===== TARGETED LEFTOVERS =====" -ForegroundColor Cyan

$hits = Get-ChildItem ".\Src" -Recurse -File -Include *.cpp,*.h |
    Select-String -Pattern `
        "\bwcscpy\s*\(",
        "\blocaltime\s*\(",
        "\bfreopen\s*\(",
        "\b_ecvt\s*\(",
        "m2::crt::copy_n\(strPixelFormat",
        "m2::crt::copy\(strPixelFormat",
        "m2::crt::copy\(rez",
        "m2::crt::append\(rez"

$hits | ForEach-Object {
    "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()
}

Write-Host ""
Write-Host "Targeted leftovers: $($hits.Count)" -ForegroundColor Yellow

Write-Host ""
git diff --check

Write-Host ""
Write-Host "REPAIR PASS COMPLETE" -ForegroundColor Green