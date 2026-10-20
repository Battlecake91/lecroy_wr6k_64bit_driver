<#
.SYNOPSIS
  Run the hardware-independent process/file-object DMA lifetime model.
.DESCRIPTION
  This model characterizes proposed ownership semantics. It neither opens the
  installed driver nor claims that the production implementation is complete.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$test = Join-Path $PSScriptRoot "test-process-dma-lifetime.c"
$outDir = Join-Path $PSScriptRoot "build"
$exe = Join-Path $outDir "test-process-dma-lifetime.exe"
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
if ($LASTEXITCODE -ne 0) { throw "Process DMA lifetime model build failed." }

& $exe
if ($LASTEXITCODE -ne 0) { throw "Process DMA lifetime model failed." }
