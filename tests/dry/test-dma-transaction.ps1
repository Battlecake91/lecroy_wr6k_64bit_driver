<#
.SYNOPSIS
  Build and run the hardware-independent WDM-v3 DMA transaction tests.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot "test-dma-transaction.c"
$gateTest = Join-Path $PSScriptRoot "test-dma-production-gate.c"
$transaction = Join-Path $repo "driver\DmaTransaction.c"
$layout = Join-Path $repo "driver\DmaLayout.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-dma-transaction.exe"
$gateExe = Join-Path $outDir "test-dma-production-gate.exe"
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /TC `
            /DLECS65_ENABLE_WDM_V3_MAM_DMA=1 `
            "/Fe:$exe" $test $transaction $layout
        if ($LASTEXITCODE -ne 0) { throw "DMA transaction test build failed." }
        & $cl.Source /nologo /W4 /WX /TC `
            "/Fe:$gateExe" $gateTest $transaction $layout
        if ($LASTEXITCODE -ne 0) { throw "DMA production gate test build failed." }
    }
    finally { Pop-Location }
}
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio vswhere.exe not found." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio C++ tools not found." }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /TC /DLECS65_ENABLE_WDM_V3_MAM_DMA=1 /Fe:`"$exe`" `"$test`" `"$transaction`" `"$layout`" && cl /nologo /W4 /WX /TC /Fe:`"$gateExe`" `"$gateTest`" `"$transaction`" `"$layout`""
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "DMA transaction test build failed." }
}
& $exe
if ($LASTEXITCODE -ne 0) { throw "DMA transaction test execution failed." }
& $gateExe
if ($LASTEXITCODE -ne 0) { throw "DMA production gate test execution failed." }
