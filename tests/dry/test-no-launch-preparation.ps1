[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repo 'scripts\invoke-no-launch-preparation.ps1'
$mockTest = Join-Path $PSScriptRoot 'test-lecdiag-command-chain.ps1'
$temp = Join-Path ([IO.Path]::GetTempPath()) ('lec-no-launch-' + [guid]::NewGuid().ToString('N'))
$mockEvidence = Join-Path $temp 'mock-build'
$runEvidence = Join-Path $temp 'run'
$statePath = Join-Path $temp 'state.json'
$instance = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
$passed = 0

try {
    & $mockTest -EvidenceRoot $mockEvidence
    if ($LASTEXITCODE -ne 0) { throw 'Mock executable qualification failed.' }
    $mockExe = Join-Path $mockEvidence 'lecdiag-mock.exe'
    $mockHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $mockExe).Hash
    $sysHash = 'A' * 64
    [ordered]@{ InstanceId=$instance;Inf='oem999.inf';Version='0.2026.1024.1';
        ProblemCode=0;DeviceStatus='OK';ServiceName='LecS65AcqDrv';
        ServiceStatus='Running';SysHash=$sysHash } |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
    & $runner -Mode OfflineMock -EvidenceRoot $runEvidence `
        -ExecutablePath $mockExe -ExpectedExecutableHash $mockHash `
        -ExpectedInf 'oem999.inf' -ExpectedVersion '0.2026.1024.1' `
        -ExpectedSysHash $sysHash -MockStatePath $statePath
    if ($LASTEXITCODE -ne 0) { throw 'OfflineMock runner failed.' }
    $result = Get-Content -LiteralPath (Join-Path $runEvidence 'result.json') -Raw |
        ConvertFrom-Json
    if ($result.Result -ne 'PASS' -or [int]$result.InvocationCount -ne 1 -or
        $result.HardwareLaunchAuthorized -ne $false -or
        $result.HardwareLaunchObserved -ne $false) {
        throw 'Result contract mismatch.'
    }
    ++$passed
    $lecdiagSource = Get-Content -Raw `
        (Join-Path $repo 'tools\lecdiag\lecdiag.c')
    $runnerSource = Get-Content -Raw $runner
    if ($lecdiagSource -notmatch '(?s)NO_LAUNCH prepare: success=.*?if \(acquireOk \|\| acquireError != ERROR_NOT_SUPPORTED.*?goto Done;.*?LECS65_IOCTL_UNREGISTER_TRANSFER' -or
        $lecdiagSource -notmatch 'explicit cleanup was not issued' -or
        $runnerSource -notmatch 'registration-input: length=12 hex=' -or
        $runnerSource -notmatch 'prepare-input: length=15 hex=') {
        throw 'Conditional cleanup or exact request-evidence contract is missing.'
    }
    ++$passed
    try {
        & $runner -Mode OfflineMock -EvidenceRoot $runEvidence `
            -ExecutablePath $mockExe -ExpectedExecutableHash $mockHash `
            -ExpectedInf 'oem999.inf' -ExpectedVersion '0.2026.1024.1' `
            -ExpectedSysHash $sysHash -MockStatePath $statePath
        throw 'Existing evidence root was accepted.'
    } catch {
        if ($_.Exception.Message -notmatch 'EvidenceRoot already exists') { throw }
    }
    ++$passed
    $badEvidence = Join-Path $temp 'bad-hash'
    try {
        & $runner -Mode OfflineMock -EvidenceRoot $badEvidence `
            -ExecutablePath $mockExe -ExpectedExecutableHash ('B' * 64) `
            -ExpectedInf 'oem999.inf' -ExpectedVersion '0.2026.1024.1' `
            -ExpectedSysHash $sysHash -MockStatePath $statePath
        throw 'Incorrect executable hash was accepted.'
    } catch {
        if ($_.Exception.Message -notmatch 'hash mismatch') { throw }
    }
    ++$passed
    Write-Host "NO-LAUNCH PREPARATION TESTS: $passed/4 passed; 0 failed."
}
finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp -Recurse -Force
    }
}

