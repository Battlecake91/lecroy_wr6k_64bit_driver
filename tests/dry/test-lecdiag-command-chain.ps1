<#
.SYNOPSIS
  Validate every approved R0/R1 lecdiag command through a mocked Win32 boundary.
#>
[CmdletBinding()]
param(
    [string]$EvidenceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo "scripts\native-process.ps1")

$source = Join-Path $repo "tools\lecdiag\lecdiag.c"
$mockSource = Join-Path $PSScriptRoot "lecdiag-mock-device.c"
$mockHeader = Join-Path $PSScriptRoot "lecdiag-mock-device.h"
$preserveEvidence = -not [string]::IsNullOrWhiteSpace($EvidenceRoot)
if ($preserveEvidence) {
    if (-not [System.IO.Path]::IsPathRooted($EvidenceRoot)) {
        throw "EvidenceRoot must be an absolute path."
    }
    if (Test-Path -LiteralPath $EvidenceRoot) {
        throw "EvidenceRoot already exists: $EvidenceRoot"
    }
    $tempRoot = $EvidenceRoot
}
else {
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("lecdiag-command-chain-" + [guid]::NewGuid().ToString("N"))
}
$mockExe = Join-Path $tempRoot "lecdiag-mock.exe"
$logs = Join-Path $tempRoot "logs"
New-Item -ItemType Directory -Path $logs -Force | Out-Null

try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vs) { throw "Visual Studio C++ tools not found." }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    $build = "`"$vcvars`" >nul && cd /d `"$tempRoot`" && cl /nologo /W4 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /FI`"$mockHeader`" /Fe:`"$mockExe`" `"$source`" `"$mockSource`""
    & $env:ComSpec /d /s /c $build
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $mockExe)) {
        throw "Mock lecdiag build failed."
    }

    $cases = @(
        @{ Name='r0-1-build'; Args=@('build'); Calls=@('ioctl=0xCFDC21C8 input=0 output=4') },
        @{ Name='r0-2-stats'; Args=@('stats'); Calls=@('ioctl=0x80006000 input=0 output=48') },
        @{ Name='r0-3-bars'; Args=@('bars'); Calls=@('ioctl=0x80006008 input=0 output=72') },
        @{ Name='r0-4-trace'; Args=@('trace'); Calls=@('ioctl=0x8000600C input=0 output=118800') },
        @{ Name='r0-5-noop'; Args=@('raw-ioctl','0x00222400','','0'); Calls=@('ioctl=0x00222400 input=0 output=0 input-hex=') },
        @{ Name='r0-6-noop'; Args=@('raw-ioctl','0xCFDC2184','','0'); Calls=@('ioctl=0xCFDC2184 input=0 output=0 input-hex=') },
        @{ Name='r0-7-trace-size'; Args=@('raw-ioctl','0x00223004','','4'); Calls=@('ioctl=0x00223004 input=0 output=4 input-hex=') },
        @{ Name='r0-8-register-size'; Args=@('raw-ioctl','0x00223040','','4'); Calls=@('ioctl=0x00223040 input=0 output=4 input-hex=') },
        @{ Name='r0-9-delay-zero'; Args=@('raw-ioctl','0x00222C00','00000000','0'); Calls=@('ioctl=0x00222C00 input=4 output=0 input-hex=00000000') },
        @{ Name='r1-pci'; Args=@('pci'); Calls=@('ioctl=0x80006014 input=0 output=84') },
        @{ Name='r1-start-register'; Args=@('start-register'); Calls=@('ioctl=0x00223044 input=0 output=4','ioctl=0xCFDC21C0 input=5 output=4 input-hex=0000000000') }
        @{ Name='no-launch-prepare'; Args=@('no-launch-prepare'); Calls=@(
            'ioctl=0xCFDC2124 input=12 output=4',
            'ioctl=0xCFDC2138 input=15 output=4',
            'ioctl=0xCFDC2128 input=4 output=0 input-hex=07000000') }
    )

    $passed = 0
    foreach ($case in $cases) {
        $result = Invoke-LecS65NativeProcess -Name $case.Name -FilePath $mockExe `
            -Arguments $case.Args -WorkingDirectory $tempRoot `
            -LogPath (Join-Path $logs ($case.Name + '.log')) -TimeoutSeconds 5
        if ($result.ExitCode -ne 0 -or $result.TimedOut) {
            throw "$($case.Name) did not complete successfully."
        }
        $actualCalls = @($result.StdOut -split "`r?`n" | Where-Object { $_ -like 'MOCK_DEVICE_IO_CONTROL *' })
        if ($actualCalls.Count -ne $case.Calls.Count) {
            throw "$($case.Name) reached $($actualCalls.Count) mock calls; expected $($case.Calls.Count)."
        }
        for ($i = 0; $i -lt $case.Calls.Count; ++$i) {
            if ($actualCalls[$i] -notlike ('*' + $case.Calls[$i] + '*')) {
                throw "$($case.Name) call mismatch: $($actualCalls[$i])"
            }
        }
        ++$passed
    }

    $negative = @(
        @{ Name='missing-empty'; Args=@('raw-ioctl','0x00222400','0') },
        @{ Name='separator-only'; Args=@('raw-ioctl','0x00222400','--','0') },
        @{ Name='odd-hex'; Args=@('raw-ioctl','0x00222400','0','0') },
        @{ Name='invalid-code'; Args=@('raw-ioctl','not-a-code','','0') },
        @{ Name='oversize-output'; Args=@('raw-ioctl','0x00222400','','65537') }
    )
    foreach ($case in $negative) {
        $result = Invoke-LecS65NativeProcess -Name $case.Name -FilePath $mockExe `
            -Arguments $case.Args -WorkingDirectory $tempRoot `
            -LogPath (Join-Path $logs ($case.Name + '.log')) -TimeoutSeconds 5 `
            -ReturnOnNonZeroExit
        if ($result.ExitCode -eq 0 -or $result.TimedOut -or
            $result.StdOut -match 'MOCK_DEVICE_IO_CONTROL') {
            throw "$($case.Name) did not fail before the mocked DeviceIoControl boundary."
        }
        ++$passed
    }

    $expectedLogs = 3 * ($cases.Count + $negative.Count)
    $actualLogs = @(Get-ChildItem -LiteralPath $logs -File).Count
    if ($actualLogs -ne $expectedLogs) {
        throw "Expected $expectedLogs complete runner logs, found $actualLogs."
    }
    if ($preserveEvidence) {
        [ordered]@{
            SchemaVersion = 1
            Result = "PASS"
            Passed = $passed
            Total = $cases.Count + $negative.Count
            PowerShellVersion = $PSVersionTable.PSVersion.ToString()
            ProductionSourceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
            MockExecutableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $mockExe).Hash
            PositiveCases = @($cases.Name)
            NegativeCases = @($negative.Name)
            LogFileCount = $actualLogs
        } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $tempRoot "result.json") -Encoding UTF8
    }
    Write-Host "LECDIAG COMMAND CHAIN: $passed/$($cases.Count + $negative.Count) passed; 0 failed."
}
finally {
    if (-not $preserveEvidence -and (Test-Path -LiteralPath $tempRoot)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
