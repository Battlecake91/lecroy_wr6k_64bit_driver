<# Build and run explicit WDM-v3 no-launch transfer ownership tests. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $PSScriptRoot 'build'
$exe = Join-Path $out 'test-dma-v3-transfer.exe'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$sources = @((Join-Path $PSScriptRoot 'test-dma-v3-transfer.c'),
    (Join-Path $repo 'driver\DmaV3Transfer.c'),
    (Join-Path $repo 'driver\DmaAdapterStage.c'),
    (Join-Path $repo 'driver\DmaLayout.c'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$cmd = "`"$vcvars`" >nul && cd /d `"$out`" && cl /nologo /W4 /WX /TC /DLECS65_SG_HOST_TEST /DLECS65_SG_TEST_ALLOCATION_HOOKS /Fe:`"$exe`" $quoted"
& $env:ComSpec /d /s /c $cmd
if ($LASTEXITCODE -ne 0) { throw 'DMA v3 transfer test build failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'DMA v3 transfer test failed.' }
