param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [switch]$Run
)

$ErrorActionPreference = "Stop"
$project = Join-Path $PSScriptRoot "DX11Smoke.vcxproj"
if (-not (Test-Path -LiteralPath $project)) {
    throw "DX11Smoke.vcxproj bulunamadi."
}

$msbuildCommand = Get-Command "MSBuild.exe" -ErrorAction SilentlyContinue
$msbuild = if ($msbuildCommand) { $msbuildCommand.Source } else { $null }

if (-not $msbuild) {
    $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "MSBuild ve vswhere bulunamadi. Visual Studio Installer -> Desktop development with C++ (MSVC v143 + Windows SDK) kontrol et."
    }

    $candidate = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "vswhere MSBuild aramasi basarisiz."
    }
    $msbuild = @($candidate | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1)[0]
}

if (-not $msbuild) {
    throw "MSBuild.exe bulunamadi."
}

Write-Host "[OK] MSBuild: $msbuild" -ForegroundColor Green
Write-Host "[INFO] Compiling DX11 smoke test ($Configuration x64)..."

& $msbuild $project /m "/p:Configuration=$Configuration" "/p:Platform=x64" /nologo
if ($LASTEXITCODE -ne 0) {
    throw "DX11 smoke build BASARISIZ; build hatalarini incele."
}

$exe = Join-Path $PSScriptRoot "bin\x64\$Configuration\DX11Smoke.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "MSBuild basarili gorunuyor fakat DX11Smoke.exe bulunamadi: $exe"
}

Write-Host "[OK] DX11 smoke build BASARILI" -ForegroundColor Green
Write-Host "[OK] Test executable: $exe"

if ($Run) {
    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "DX11Smoke.exe hata kodu ile sonlandi: $LASTEXITCODE"
    }
    Write-Host "[OK] DX11Smoke.exe normal sonlandi." -ForegroundColor Green
}
