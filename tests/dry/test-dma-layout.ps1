<#
.SYNOPSIS
  Build and run hardware-independent native DMA descriptor layout tests.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot "test-dma-layout.c"
$source = Join-Path $repo "driver\DmaLayout.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-dma-layout.exe"
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /TC "/Fe:$exe" $test $source
        if ($LASTEXITCODE -ne 0) { throw "DMA layout test build failed." }
    }
    finally {
        Pop-Location
    }
}
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio vswhere.exe not found." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio C++ tools not found." }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found." }
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /TC /Fe:`"$exe`" `"$test`" `"$source`""
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "DMA layout test build failed." }
}
& $exe
if ($LASTEXITCODE -ne 0) { throw "DMA layout test execution failed." }
