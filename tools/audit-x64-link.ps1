param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msvc = Get-ChildItem "$vsPath\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$dumpbin = Join-Path $msvc.FullName "bin\Hostx64\x64\dumpbin.exe"

if (-not (Test-Path $dumpbin)) {
    throw "dumpbin.exe not found: $dumpbin"
}

$objRoot = Join-Path $repo "Src\obj"
$libRoots = @(
    (Join-Path $repo "Src\bin\lib\$Configuration\$Platform"),
    (Join-Path $repo "extern\lib\x64"),
    (Join-Path $repo "Extern\lib\x64"),
    (Join-Path $repo "Extern\Python315\libs"),
    (Join-Path $repo "vcpkg_installed\x64-windows-static\lib"),
    (Join-Path $repo "vcpkg_installed\x64-windows-static\debug\lib")
) | Where-Object { Test-Path $_ } | Select-Object -Unique

$scan = @()
if (Test-Path $objRoot) {
    $scan += Get-ChildItem $objRoot -Recurse -File -Filter *.obj -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\$Configuration\\$Platform\\" }
}
$binLib = Join-Path $repo "Src\bin\lib\$Configuration\$Platform"
if (Test-Path $binLib) {
    $scan += Get-ChildItem $binLib -Recurse -File -Filter *.lib -ErrorAction SilentlyContinue
}

$requests = foreach ($file in $scan) {
    $directives = & $dumpbin /nologo /directives $file.FullName 2>$null
    foreach ($line in $directives) {
        foreach ($m in [regex]::Matches($line, '/DEFAULTLIB:"?([^"\s]+\.lib)"?', 'IgnoreCase')) {
            [PSCustomObject]@{
                Library   = $m.Groups[1].Value
                Requester = $file.FullName
            }
        }
    }
}

$systemLibs = @(
    "kernel32.lib","user32.lib","gdi32.lib","advapi32.lib","shell32.lib",
    "ole32.lib","oleaut32.lib","uuid.lib","comdlg32.lib","winmm.lib",
    "imm32.lib","imagehlp.lib","version.lib","dinput8.lib","dxguid.lib",
    "ws2_32.lib","strmiids.lib","ddraw.lib","dmoguids.lib","oldnames.lib",
    "libcmt.lib","libcmtd.lib","libcpmt.lib","libcpmtd.lib","libucrt.lib",
    "libucrtd.lib","libvcruntime.lib","libvcruntimed.lib","msvcrt.lib"
)

function Get-Machine([string]$Path) {
    $h = & $dumpbin /headers $Path 2>$null | Select-String -Pattern "machine \(" | Select-Object -First 1
    if (-not $h) { return "UNKNOWN" }
    $s = $h.ToString()
    if ($s -match "8664|x64") { return "x64" }
    if ($s -match "14C|x86") { return "x86" }
    if ($s -match "AA64|ARM64") { return "ARM64" }
    return $s.Trim()
}

$allCandidates = @()
foreach ($root in $libRoots) {
    $allCandidates += Get-ChildItem $root -Recurse -File -Filter *.lib -ErrorAction SilentlyContinue
}

$report = foreach ($g in ($requests | Group-Object Library | Sort-Object Name)) {
    $name = $g.Name
    $matches = $allCandidates | Where-Object { $_.Name -ieq $name }
    $machines = @()
    foreach ($m in $matches) {
        $machines += [PSCustomObject]@{ Path = $m.FullName; Machine = Get-Machine $m.FullName }
    }

    [PSCustomObject]@{
        Library       = $name
        Kind          = if ($systemLibs -contains $name.ToLowerInvariant()) { "SYSTEM" } else { "EXTERNAL" }
        RequestCount  = $g.Count
        HasX64        = [bool]($machines | Where-Object Machine -eq "x64")
        HasX86        = [bool]($machines | Where-Object Machine -eq "x86")
        CandidatePaths= ($machines | ForEach-Object { "$($_.Machine):$($_.Path)" }) -join " | "
        Requesters    = ($g.Group.Requester | Sort-Object -Unique) -join " | "
    }
}

$out = Join-Path $repo "x64_link_dependency_audit.csv"
$report | Export-Csv $out -NoTypeInformation -Encoding UTF8
$report | Format-Table Library,Kind,RequestCount,HasX64,HasX86 -AutoSize
Write-Host ""
Write-Host "Audit saved: $out"
