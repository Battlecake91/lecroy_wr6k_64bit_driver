<#
.SYNOPSIS
  Build and run hardware-independent CFDC2110 family-2 opcode-0x03 tests.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test = Join-Path $PSScriptRoot 'test-cfdc2110-opcode03.c'
$source = Join-Path $repo 'driver\CfDc2110Protocol.c'
$outDir = Join-Path $PSScriptRoot 'build'
$exe = Join-Path $outDir 'test-cfdc2110-opcode03.exe'
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
if ($LASTEXITCODE -ne 0) { throw 'CFDC2110 opcode-0x03 test build failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'CFDC2110 opcode-0x03 test execution failed.' }
