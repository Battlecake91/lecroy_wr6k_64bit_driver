$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repo "tools\xstream-ioctl-trace"
$outDir = Join-Path $srcDir "build\x86"
$hookSrc = Join-Path $srcDir "xstream_io_hook.c"
$launcherSrc = Join-Path $srcDir "xstream_trace_launcher.c"
$hookOut = Join-Path $outDir "xstream_io_hook.dll"
$launcherOut = Join-Path $outDir "xstream_trace_launcher.exe"

foreach ($path in @($hookSrc, $launcherSrc)) {
    if (-not (Test-Path $path)) {
        throw "Source file not found: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio C++ Build Tools."
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "Visual Studio C++ x86/x64 build tools were not found."
}

$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars32.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars32.bat not found: $vcvars"
}

$cmd = '"' + $vcvars + '" >nul && cd /d "' + $srcDir + '" && cl /nologo /W4 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /LD /Fe:"' + $hookOut + '" xstream_io_hook.c && cl /nologo /W4 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /Fe:"' + $launcherOut + '" xstream_trace_launcher.c'
& $env:ComSpec /d /s /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "XStream IOCTL tracer build failed with exit code $LASTEXITCODE."
}

function Get-PeMachine {
    param([string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = $null
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Not a valid PE image: $Path"
        }
        return $reader.ReadUInt16()
    }
    finally {
        if ($reader -ne $null) {
            $reader.Dispose()
        }
        else {
            $stream.Dispose()
        }
    }
}

foreach ($output in @($hookOut, $launcherOut)) {
    if (-not (Test-Path $output)) {
        throw "Build completed without producing $output"
    }

    $machine = Get-PeMachine $output
    if ($machine -ne 0x014C) {
        throw ("Architecture verification failed for {0}: PE machine=0x{1:X4}, expected x86/0x014C" -f $output, $machine)
    }
}

Write-Host "Built x86 XStream IOCTL tracer:"
Write-Host "  $launcherOut"
Write-Host "  $hookOut"
Write-Host "  Architecture: x86 (PE machine 0x014C)"
Write-Host ""
Write-Host "Artifact verification:"
foreach ($output in @($launcherOut, $hookOut)) {
    $item = Get-Item $output
    $hash = (Get-FileHash -Algorithm SHA256 -Path $output).Hash
    Write-Host ("  {0}" -f $item.Name)
    Write-Host ("    Bytes : {0}" -f $item.Length)
    Write-Host ("    SHA256: {0}" -f $hash)
}
