param(
    [ValidateSet("x64", "x86")]
    [string]$Architecture = "x64"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repo "tools\lecdiag"
$src = Join-Path $srcDir "lecdiag.c"
$outDir = Join-Path $srcDir "build"
if ($Architecture -eq "x64") {
    $out = Join-Path $outDir "lecdiag.exe"
}
else {
    $outDir = Join-Path $outDir "x86"
    $out = Join-Path $outDir "lecdiag.exe"
}

if (-not (Test-Path $src)) {
    throw "lecdiag source not found: $src"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Invoke-LecdiagBuildWithCl {
    param(
        [string]$ClPath = "cl.exe",
        [string]$OutputPath
    )

    Push-Location $srcDir
    try {
        & $ClPath /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /Fe:$OutputPath lecdiag.c
        if ($LASTEXITCODE -ne 0) {
            throw "cl.exe failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl -and $Architecture -eq "x64") {
    Invoke-LecdiagBuildWithCl $cl.Source $out
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

    $vcvarsName = if ($Architecture -eq "x86") { "vcvars32.bat" } else { "vcvars64.bat" }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\$vcvarsName"
    if (-not (Test-Path $vcvars)) {
        throw "$vcvarsName not found: $vcvars"
    }

    $cmd = "`"$vcvars`" >nul && cd /d `"$srcDir`" && cl /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /Fe:`"$out`" lecdiag.c"
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "lecdiag build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path $out)) {
    throw "Build completed without producing $out"
}

# Verify the PE machine field so an incorrectly selected compiler environment
# cannot silently produce the wrong architecture.
$stream = [System.IO.File]::OpenRead($out)
$reader = $null
try {
    $reader = New-Object System.IO.BinaryReader($stream)
    $stream.Position = 0x3C
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    $signature = $reader.ReadUInt32()
    if ($signature -ne 0x00004550) {
        throw "Output is not a valid PE image: $out"
    }
    $machine = $reader.ReadUInt16()
}
finally {
    if ($reader -ne $null) {
        $reader.Dispose()
    }
    else {
        $stream.Dispose()
    }
}

$expectedMachine = if ($Architecture -eq "x86") { 0x014C } else { 0x8664 }
if ($machine -ne $expectedMachine) {
    throw ("Architecture verification failed: requested {0}, PE machine=0x{1:X4}, expected=0x{2:X4}" -f $Architecture, $machine, $expectedMachine)
}

Write-Host "Built:"
Write-Host "  $out"
Write-Host ("  Architecture: {0} (PE machine 0x{1:X4})" -f $Architecture, $machine)
