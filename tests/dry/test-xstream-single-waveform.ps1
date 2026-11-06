<#
.SYNOPSIS
  Exercise the single-waveform XStream runner without starting XStream.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1'
$temp = Join-Path ([IO.Path]::GetTempPath()) `
    ('lec-xstream-single-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
$passed = 0
$failed = 0

function Add-Result([string]$Name, [bool]$Condition) {
    if ($Condition) { $script:passed++; Write-Host "[PASS] $Name" }
    else { $script:failed++; Write-Host "[FAIL] $Name" }
}

function Write-Json([string]$Path, [object]$Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 8),
        (New-Object Text.UTF8Encoding($false)))
}

try {
    $exe = Join-Path $temp 'lecroyxstreamdso.exe'
    [IO.File]::WriteAllText($exe, 'mock xstream executable')
    $exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
    $baseState = [ordered]@{
        Driver = [ordered]@{
            Inf='oem107.inf'; Version='0.2026.1030.1'; ProblemCode=0
            DeviceStatus='OK'; ServiceStatus='Running'; SysHash=('1' * 64)
        }
        Disks = @([ordered]@{
            Model='HFM256GDHTNG-8510B'; Serial='ACE4_2E00_9590_A2AA.'
            Size=256052966400; Status='OK'
        })
        AcquisitionClients=@()
        MockWaveformObserved=$true
        MockReportedSamples=16
        MockReturnedValues=16
    }
    $caseNumber = 0
    function Invoke-Case {
        param([string]$Name, [object]$State=$baseState,
            [switch]$NoAuthorization, [switch]$NoDiskConfirmation,
            [string]$Hash=$exeHash, [switch]$PrecreateEvidence,
            [int]$ExpectedPrelaunchedProcessId=0)
        $script:caseNumber++
        $statePath = Join-Path $temp ("state-$($script:caseNumber).json")
        $evidence = Join-Path $temp ("evidence-$($script:caseNumber)")
        Write-Json $statePath $State
        if ($PrecreateEvidence) { [IO.Directory]::CreateDirectory($evidence)|Out-Null }
        $arguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
            '-File',$runner,'-Mode','OfflineMock','-EvidenceRoot',$evidence,
            '-XStreamExecutablePath',$exe,'-ExpectedXStreamExecutableHash',$Hash,
            '-ExpectedInf','oem107.inf','-ExpectedVersion','0.2026.1030.1',
            '-ExpectedSysHash',('1' * 64),'-MockStatePath',$statePath,
            '-TimeoutSeconds','30')
        if ($ExpectedPrelaunchedProcessId -gt 0) {
            $arguments += @('-ExpectedPrelaunchedProcessId',
                $ExpectedPrelaunchedProcessId.ToString())
        }
        if (-not $NoAuthorization) { $arguments += '-OwnerAuthorized' }
        if (-not $NoDiskConfirmation) {
            $arguments += '-PhysicalDiskDisconnectedConfirmed'
        }
        $process = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') `
            -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
        $resultPath = Join-Path $evidence 'result.json'
        $result = if (Test-Path -LiteralPath $resultPath) {
            Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
        } else { $null }
        return [pscustomobject]@{
            Name=$Name; ExitCode=$process.ExitCode; Evidence=$evidence; Result=$result
        }
    }

    $success = Invoke-Case 'success'
    Add-Result 'offline mock observes one waveform without explicit Acquire' (
        $success.ExitCode -eq 0 -and $success.Result.Result -eq 'PASS' -and
        [int]$success.Result.XStreamLaunchCount -eq 1 -and
        [int]$success.Result.ExplicitAcquireCount -eq 0 -and
        [bool]$success.Result.Waveform.Observed)
    Add-Result 'success preserves dispatch, preflight and compact result evidence' (
        (Test-Path (Join-Path $success.Evidence 'xstream-dispatch.json')) -and
        (Test-Path (Join-Path $success.Evidence 'preflight.json')) -and
        -not (Test-Path (Join-Path $success.Evidence 'waveform.raw')))

    $noAuth = Invoke-Case 'no-auth' -NoAuthorization
    Add-Result 'missing owner authorization fails before launch' (
        $noAuth.ExitCode -ne 0 -and [int]$noAuth.Result.XStreamLaunchCount -eq 0)
    $noDisk = Invoke-Case 'no-disk' -NoDiskConfirmation
    Add-Result 'missing physical isolation confirmation fails before launch' (
        $noDisk.ExitCode -ne 0 -and [int]$noDisk.Result.XStreamLaunchCount -eq 0)
    $badHash = Invoke-Case 'bad-hash' -Hash ('0' * 64)
    Add-Result 'wrong XStream hash fails before launch' (
        $badHash.ExitCode -ne 0 -and [int]$badHash.Result.XStreamLaunchCount -eq 0)

    $extraDisk = $baseState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $extraDisk.Disks = @($extraDisk.Disks) + [pscustomobject]@{
        Model='productive-x86'; Serial='protected'; Size=1; Status='OK'
    }
    $diskCase = Invoke-Case 'extra-disk' -State $extraDisk
    Add-Result 'unexpected second disk fails before launch' (
        $diskCase.ExitCode -ne 0 -and [int]$diskCase.Result.XStreamLaunchCount -eq 0)

    $clientState = $baseState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $clientState.AcquisitionClients = @([pscustomobject]@{
        ProcessId=123; Name='lecroyxstreamdso.exe'; ExecutablePath=$exe
    })
    $clientCase = Invoke-Case 'existing-client' -State $clientState
    Add-Result 'existing acquisition client fails before launch' (
        $clientCase.ExitCode -ne 0 -and [int]$clientCase.Result.XStreamLaunchCount -eq 0)

    $prelaunchedState = $baseState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $prelaunchedState.AcquisitionClients = @([pscustomobject]@{
        ProcessId=528; Name='lecroyxstreamdso.exe'; ExecutablePath=$exe
    })
    $prelaunchedCase = Invoke-Case 'expected-prelaunched-client' `
        -State $prelaunchedState -ExpectedPrelaunchedProcessId 528
    Add-Result 'pinned suspended-launch target is accepted as the sole expected client' (
        $prelaunchedCase.ExitCode -eq 0 -and
        $prelaunchedCase.Result.Result -eq 'PASS' -and
        [int]$prelaunchedCase.Result.XStreamLaunchCount -eq 1 -and
        [int]$prelaunchedCase.Result.ExplicitAcquireCount -eq 0)

    $wrongPidCase = Invoke-Case 'wrong-prelaunched-pid' `
        -State $prelaunchedState -ExpectedPrelaunchedProcessId 529
    Add-Result 'mismatched prelaunched process id fails before application launch' (
        $wrongPidCase.ExitCode -ne 0 -and
        [int]$wrongPidCase.Result.XStreamLaunchCount -eq 0)

    $wrongPathState = $prelaunchedState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $wrongPathState.AcquisitionClients[0].ExecutablePath = `
        (Join-Path $temp 'other-xstream.exe')
    $wrongPathCase = Invoke-Case 'wrong-prelaunched-path' `
        -State $wrongPathState -ExpectedPrelaunchedProcessId 528
    Add-Result 'wrong prelaunched executable path fails before application launch' (
        $wrongPathCase.ExitCode -ne 0 -and
        [int]$wrongPathCase.Result.XStreamLaunchCount -eq 0)

    $competingState = $prelaunchedState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $competingState.AcquisitionClients = @($competingState.AcquisitionClients) +
        [pscustomobject]@{
            ProcessId=123; Name='lecdiag.exe'; ExecutablePath=(Join-Path $temp 'lecdiag.exe')
        }
    $competingCase = Invoke-Case 'competing-client-with-prelaunch' `
        -State $competingState -ExpectedPrelaunchedProcessId 528
    Add-Result 'a competing client still blocks the expected prelaunched target' (
        $competingCase.ExitCode -ne 0 -and
        [int]$competingCase.Result.XStreamLaunchCount -eq 0)

    $emptyState = $baseState | ConvertTo-Json -Depth 8 | ConvertFrom-Json
    $emptyState.MockWaveformObserved = $false
    $emptyCase = Invoke-Case 'empty-waveform' -State $emptyState
    Add-Result 'missing waveform consumes one application attempt and fails closed' (
        $emptyCase.ExitCode -ne 0 -and
        [int]$emptyCase.Result.XStreamLaunchCount -eq 1 -and
        [int]$emptyCase.Result.ExplicitAcquireCount -eq 0)

    $collision = Invoke-Case 'collision' -PrecreateEvidence
    Add-Result 'existing evidence directory prevents launch' (
        $collision.ExitCode -ne 0 -and $null -eq $collision.Result)
}
finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ("XSTREAM SINGLE-WAVEFORM TESTS: {0}/{1} passed; {2} failed." -f `
    $passed, ($passed + $failed), $failed)
if ($failed -ne 0) { throw "$failed XStream single-waveform test(s) failed." }
