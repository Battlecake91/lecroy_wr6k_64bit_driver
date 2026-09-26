param()

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repo "tools\lecdiag"
$src = Join-Path $srcDir "lecdiag.c"
$outDir = Join-Path $srcDir "build"
$out = Join-Path $outDir "lecdiag.exe"

if (-not (Test-Path $src)) {
    throw "lecdiag source not found: $src"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Invoke-LecdiagBuildWithCl {
    param([string]$ClPath = "cl.exe")

    Push-Location $srcDir
    try {
        & $ClPath /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /Fe:build\lecdiag.exe lecdiag.c
        if ($LASTEXITCODE -ne 0) {
            throw "cl.exe failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Invoke-LecdiagBuildWithCl $cl.Source
}
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "cl.exe is not in PATH and vswhere.exe was not found. Install Visual Studio C++ tools or run from an x64 Native Tools prompt."
    }

    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) {
        throw "Visual Studio C++ x64 build tools were not found."
    }

    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found: $vcvars"
    }

    $cmd = "`"$vcvars`" >nul && cd /d `"$srcDir`" && cl /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /Fe:build\lecdiag.exe lecdiag.c"
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "lecdiag build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path $out)) {
    throw "Build completed without producing $out"
}

Write-Host "Built:"
Write-Host "  $out"
