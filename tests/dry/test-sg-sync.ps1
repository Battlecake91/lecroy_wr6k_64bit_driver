<#
.SYNOPSIS
  Build and run hardware-independent native WDM v3 synchronous SG tests.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot "test-sg-sync.c"
$source = Join-Path $repo "driver\DmaSyncStage.c"
$adapterSource = Join-Path $repo "driver\DmaAdapterStage.c"
$pnpSource = Join-Path $repo "driver\DmaPnpStage.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-sg-sync.exe"
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($cl) {
    Push-Location $outDir
    try {
        & $cl.Source /nologo /W4 /WX /DLECS65_SG_HOST_TEST /DLECS65_SG_TEST_ALLOCATION_HOOKS /TC "/Fe:$exe" $test $source $adapterSource $pnpSource
        if ($LASTEXITCODE -ne 0) { throw "SG v3 synchronous test build failed." }
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
    $cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /DLECS65_SG_HOST_TEST /DLECS65_SG_TEST_ALLOCATION_HOOKS /TC /Fe:`"$exe`" `"$test`" `"$source`" `"$adapterSource`" `"$pnpSource`""
    & $env:ComSpec /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "SG v3 synchronous test build failed." }
}
& $exe
if ($LASTEXITCODE -ne 0) { throw "SG v3 synchronous test execution failed." }
