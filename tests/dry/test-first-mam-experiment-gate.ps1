<#
.SYNOPSIS
  Build and run the hardware-independent production first-MAM gate tests.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot 'test-first-mam-experiment-gate.c'
$source = Join-Path $repo 'driver\DmaExperimentGate.c'
$outDir = Join-Path $PSScriptRoot 'build'
$exe = Join-Path $outDir 'test-first-mam-experiment-gate.exe'
New-Item -Path $outDir -ItemType Directory -Force | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$command = '"{0}" >nul && cd /d "{1}" && cl /nologo /W4 /WX /TC /Fe:"{2}" "{3}" "{4}"' -f `
    $vcvars, $outDir, $exe, $test, $source
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'First MAM gate test build failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'First MAM gate test execution failed.' }
