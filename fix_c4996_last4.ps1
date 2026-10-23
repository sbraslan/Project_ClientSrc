$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Read-File($rel) {
    [IO.File]::ReadAllText((Join-Path $root $rel))
}

function Write-File($rel, $text) {
    [IO.File]::WriteAllText((Join-Path $root $rel), $text, $utf8)
}


# ============================================================
# 1) CRC32.cpp
# malloc + pointer helper yerine std::string storage
# ============================================================

$f = "Src\EterBase\CRC32.cpp"
$s = Read-File $f

if (!$s.Contains("#include <string>")) {
    $s = $s.Replace(
        '#include "CRC32.h"',
        "#include `"CRC32.h`"`r`n#include <string>"
    )
}

$old = @'
	char* rez = (char*)buf;
	if (name != nullptr)
	{
		rez = (char*)malloc(1 + strlen(buf) + strlen(name));
		m2::crt::copy(rez, name);
		m2::crt::append(rez, buf);
	}
	int lenSt = strlen(rez);
	len = strlen(rez);
'@

$new = @'
	std::string rezStorage;
	const char* rez = buf;

	if (name != nullptr)
	{
		rezStorage.reserve(strlen(name) + strlen(buf));
		rezStorage.append(name);
		rezStorage.append(buf);
		rez = rezStorage.c_str();
	}

	len = strlen(rez);
'@

if ($s.Contains($old)) {
    $s = $s.Replace($old, $new)
    Write-File $f $s
    Write-Host "[OK] CRC32 pointer block fixed" -ForegroundColor Green
}
else {
    Write-Host "[FAIL] CRC32 hedef blok bulunamadi" -ForegroundColor Red
    Select-String (Join-Path $root $f) -Pattern "rez|crt::copy|crt::append"
}


# ============================================================
# 2) PythonGameEventManager localtime
# ============================================================

$f = "Src\UserInterface\PythonGameEventManager.cpp"
$s = Read-File $f

$old = @'
		const tm* t = localtime(&eventStartTime);

		m_map_Event[t->tm_mday].emplace_back(eventTable);
'@

$new = @'
		tm eventStartTm{};
		if (localtime_s(&eventStartTm, &eventStartTime) != 0)
			continue;

		m_map_Event[eventStartTm.tm_mday].emplace_back(eventTable);
'@

if ($s.Contains($old)) {
    $s = $s.Replace($old, $new)
    Write-File $f $s
    Write-Host "[OK] PythonGameEventManager localtime_s" -ForegroundColor Green
}


# ============================================================
# 3) WeatherInfo localtime
# ============================================================

$f = "Src\UserInterface\WeatherInfo.cpp"
$s = Read-File $f

$old = 'struct tm tm = *localtime(&ct);'
$new = @'
struct tm tm{};
	localtime_s(&tm, &ct);
'@

if ($s.Contains($old)) {
    $s = $s.Replace($old, $new.TrimEnd())
    Write-File $f $s
    Write-Host "[OK] WeatherInfo localtime_s" -ForegroundColor Green
}


# ============================================================
# 4) PythonTicket trailing whitespace temizliği
# ============================================================

$f = "Src\UserInterface\PythonTicket.cpp"
$s = Read-File $f

$s = [regex]::Replace(
    $s,
    '[ \t]+(?=\r?$)',
    '',
    [System.Text.RegularExpressions.RegexOptions]::Multiline
)

Write-File $f $s
Write-Host "[OK] PythonTicket whitespace cleaned" -ForegroundColor Green


# ============================================================
# 5) Verification
# ============================================================

Write-Host ""
Write-Host "===== VERIFY =====" -ForegroundColor Cyan

$hits = Get-ChildItem ".\Src" -Recurse -File -Include *.cpp,*.h |
    Select-String -Pattern `
        "m2::crt::copy\(rez",
        "m2::crt::append\(rez",
        "\blocaltime\s*\(",
        "\bwcscpy\s*\(",
        "\bfreopen\s*\(",
        "\b_ecvt\s*\("

$hits | ForEach-Object {
    "{0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()
}

Write-Host ""
Write-Host "Remaining targeted CRT hits: $($hits.Count)" -ForegroundColor Yellow

Write-Host ""
git diff --check