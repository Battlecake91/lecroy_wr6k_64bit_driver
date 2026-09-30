$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repo "tools\lecwatch"
$src = Join-Path $srcDir "lecwatch.c"
$outDir = Join-Path $srcDir "build"
$out = Join-Path $outDir "lecwatch.exe"

if (-not (Test-Path $src)) {
    throw "lecwatch source not found: $src"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio C++ Build Tools."
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "Visual Studio C++ x64 tools were not found."
}

$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found: $vcvars"
}

$cmd = "`"$vcvars`" >nul && cd /d `"$srcDir`" && cl /nologo /W4 /O2 /MT /utf-8 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Fe:`"$out`" lecwatch.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib"
& $env:ComSpec /d /s /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "lecwatch build failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $out)) {
    throw "Build completed without producing $out"
}

$stream = [System.IO.File]::OpenRead($out)
$reader = $null
try {
    $reader = New-Object System.IO.BinaryReader($stream)
    $stream.Position = 0x3C
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) {
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

if ($machine -ne 0x8664) {
    throw ("Architecture verification failed: PE machine=0x{0:X4}, expected x64/0x8664" -f $machine)
}

$item = Get-Item $out
$hash = (Get-FileHash -Algorithm SHA256 -Path $out).Hash

Write-Host "Built native x64 live IOCTL monitor:"
Write-Host "  $out"
Write-Host ("  Bytes : {0}" -f $item.Length)
Write-Host ("  SHA256: {0}" -f $hash)
Write-Host "  Architecture: x64 (PE machine 0x8664)"
