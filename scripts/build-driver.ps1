param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$BuildLecdiag
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repo "LecS65AcqDrv.sln"
$sys = Join-Path $repo ("x64\{0}\LecS65AcqDrv.sys" -f $Configuration)

if (-not (Test-Path $solution)) {
    throw "Solution not found: $solution"
}

function Find-MSBuild {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "msbuild.exe is not in PATH and vswhere.exe was not found."
    }

    $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if (-not $path) {
        throw "MSBuild was not found. Install Visual Studio/Build Tools with Desktop C++ and WDK support."
    }

    return $path
}

$msbuild = Find-MSBuild
Write-Host "MSBuild:"
Write-Host "  $msbuild"
Write-Host "Building $Configuration | x64..."

& $msbuild $solution /m /t:Build "/p:Configuration=$Configuration" "/p:Platform=x64" /nologo
if ($LASTEXITCODE -ne 0) {
    throw "Driver build failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $sys)) {
    throw "Build succeeded but driver output was not found: $sys"
}

Write-Host ""
Write-Host "Driver built:"
Write-Host "  $sys"

if ($BuildLecdiag) {
    Write-Host ""
    Write-Host "Building lecdiag..."
    & (Join-Path $PSScriptRoot "build-lecdiag.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "lecdiag build failed with exit code $LASTEXITCODE."
    }
}
