<#
.SYNOPSIS
  Run hardware-independent event interleaving and interface-fault models.
.DESCRIPTION
  This characterization test does not load or open the driver. Source
  contracts separately bind the modeled ordering to the production paths.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$test = Join-Path $PSScriptRoot "test-runtime-lifetime-audit.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-runtime-lifetime-audit.exe"
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "Visual Studio vswhere.exe not found."
}
$vs = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vs) { throw "Visual Studio C++ tools not found." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$cmd = "`"$vcvars`" >nul && cd /d `"$outDir`" && cl /nologo /W4 /WX /TC /Fe:`"$exe`" `"$test`""
& $env:ComSpec /d /s /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Runtime lifetime audit build failed." }

& $exe
if ($LASTEXITCODE -ne 0) { throw "Runtime lifetime audit failed." }
