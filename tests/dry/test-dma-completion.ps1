<#
.SYNOPSIS
  Build and run hardware-independent live DMA completion-state tests.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 100000)]
    [int]$StressIterations = 1000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot "test-dma-completion.c"
$source = Join-Path $repo "driver\DmaCompletion.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-dma-completion.exe"
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$sources = @($test, $source)
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /DLECS65_DMA_COMPLETION_HOST_TEST /TC "/Fe:$exe" $sources
        if ($LASTEXITCODE -ne 0) { throw "DMA completion test build failed." }
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
    $quotedSources = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /DLECS65_DMA_COMPLETION_HOST_TEST /TC /Fe:`"$exe`" $quotedSources"
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "DMA completion test build failed." }
}

& $exe
if ($LASTEXITCODE -ne 0) { throw "DMA completion test execution failed." }

$raceTest = Join-Path $PSScriptRoot "test-dma-completion-races.c"
$raceExe = Join-Path $outDir "test-dma-completion-races.exe"
$raceSources = @($raceTest, $source)
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /O2 /DLECS65_DMA_COMPLETION_HOST_TEST /DLECS65_DMA_COMPLETION_INTERLEAVING_TEST /TC "/Fe:$raceExe" $raceSources
        if ($LASTEXITCODE -ne 0) { throw "DMA race test build failed." }
    }
    finally {
        Pop-Location
    }
}
else {
    $quotedSources = ($raceSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /O2 /DLECS65_DMA_COMPLETION_HOST_TEST /DLECS65_DMA_COMPLETION_INTERLEAVING_TEST /TC /Fe:`"$raceExe`" $quotedSources"
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "DMA race test build failed." }
}
& $raceExe $StressIterations
if ($LASTEXITCODE -ne 0) { throw "DMA race test execution failed." }

$intenSources = @((Join-Path $PSScriptRoot "test-interrupt-control.c"),
    (Join-Path $repo "driver\InterruptControl.c"))
$intenExe = Join-Path $outDir "test-interrupt-control.exe"
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /O2 /DLECS65_INTEN_HOST_TEST /TC "/Fe:$intenExe" $intenSources
        if ($LASTEXITCODE -ne 0) { throw "INTEN test build failed." }
    }
    finally { Pop-Location }
}
else {
    $quotedSources = ($intenSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /O2 /DLECS65_INTEN_HOST_TEST /TC /Fe:`"$intenExe`" $quotedSources"
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "INTEN test build failed." }
}
& $intenExe $StressIterations
if ($LASTEXITCODE -ne 0) { throw "INTEN test execution failed." }
