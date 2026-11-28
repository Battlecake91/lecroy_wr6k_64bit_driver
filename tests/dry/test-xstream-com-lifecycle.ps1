$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'scripts\xstream-com-lifecycle.ps1')
. (Join-Path $repo 'scripts\xstream-process-diagnostics.ps1')
. (Join-Path $repo 'scripts\native-process.ps1')
$passed = 0
$failed = 0

function Check([string]$Name,[bool]$Condition) {
    if ($Condition) { $script:passed++; Write-Host "PASS: $Name" }
    else { $script:failed++; Write-Host "FAIL: $Name" }
}

function Invoke-ExpectedFailure([scriptblock]$Action) {
    try { & $Action; return $false }
    catch { return $true }
}

$child = [pscustomobject]@{ Name='Acquisition'; Ready=$true }
$root = [pscustomobject]@{ Child=$child }
$root | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    if ($Name -eq 'Acquisition') { return $this.Child }
    throw 'DISP_E_MEMBERNOTFOUND'
}
Check 'root automation object resolves Acquisition through Item' `
    ([object]::ReferenceEquals((Get-XStreamObject -Parent $root -Name 'Acquisition'),$child))

$delayed = [pscustomobject]@{ Calls=0; Child=$child }
$delayed | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    $this.Calls++
    if ($this.Calls -lt 3) { return $null }
    return $this.Child
}
$first = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
$second = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
$third = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
Check 'delayed Acquisition availability is observable without extra COM access paths' `
    ($null -eq $first -and $null -eq $second -and
     [object]::ReferenceEquals($third,$child) -and $delayed.Calls -eq 3)

$fallback = [pscustomobject]@{ Objects=[pscustomobject]@{ Child=$child } }
$fallback.Objects | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name) return $this.Child
}
$fallback | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name) throw 'ordinary dispatch member mismatch'
}
Check 'ordinary Item failure retains legacy Objects.Item fallback' `
    ([object]::ReferenceEquals((Get-XStreamObject -Parent $fallback -Name 'Acquisition'),$child))

$rpcError = New-Object System.Runtime.InteropServices.COMException(
    'The RPC server is unavailable.',[int32]-2147023174)
$rpcRoot = [pscustomobject]@{ Calls=0; Objects=$null; Object=$null }
$rpcRoot | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    $this.Calls++
    throw $script:rpcError
}
$script:rpcError = $rpcError
Check 'RPC server unavailable is classified from HRESULT 0x800706BA' `
    (Test-XStreamRpcServerUnavailable -Exception $rpcError)
Check 'RPC failure is rethrown without null fallback calls' `
    ((Invoke-ExpectedFailure {
        Get-XStreamObject -Parent $rpcRoot -Name 'Acquisition'
    }) -and $rpcRoot.Calls -eq 1)
Check 'inner RPC exception is classified' `
    (Test-XStreamRpcServerUnavailable -Exception (
        New-Object InvalidOperationException('wrapped',$rpcError)))

$ordinaryError = New-Object System.Exception('not an RPC failure')
Check 'ordinary failures are not misclassified as RPC loss' `
    (-not (Test-XStreamRpcServerUnavailable -Exception $ordinaryError))
Check 'PowerShell COM probe runs in STA for this test host' `
    ([Threading.Thread]::CurrentThread.GetApartmentState() -eq [Threading.ApartmentState]::STA)

$processIdentity = @{
    ExpectedProcessId = 123
    ExpectedExecutablePath = 'C:\XStream\lecroyxstreamdso.exe'
    ExpectedCreationDate = [datetime]'2026-11-28T00:00:00'
}
$captured = [pscustomobject]@{
    ProcessId = 123
    ExecutablePath = 'C:\XStream\LecroyXStreamDSO.exe'
    CreationDate = $processIdentity.ExpectedCreationDate
}
$newServer = [pscustomobject]@{
    ProcessId = 456
    ExecutablePath = $captured.ExecutablePath
    CreationDate = $captured.CreationDate.AddSeconds(3)
}
$reusedPid = [pscustomobject]@{
    ProcessId = $captured.ProcessId
    ExecutablePath = $captured.ExecutablePath
    CreationDate = $newServer.CreationDate
}
$unreadablePath = [pscustomobject]@{
    ProcessId = $captured.ProcessId
    ExecutablePath = $null
    CreationDate = $captured.CreationDate
}
Check 'COM activation identity permits the original captured process' `
    (Test-XStreamCapturedServerProcess -Processes @($captured) @processIdentity)
Check 'COM activation identity rejects a second same-image server' `
    (-not (Test-XStreamCapturedServerProcess -Processes @($captured,$newServer) @processIdentity))
Check 'COM activation identity rejects a successor as the sole server' `
    (-not (Test-XStreamCapturedServerProcess -Processes @($newServer) @processIdentity))
Check 'COM activation identity rejects PID reuse' `
    (-not (Test-XStreamCapturedServerProcess -Processes @($reusedPid) @processIdentity))
Check 'COM activation identity rejects an unverified image path' `
    (-not (Test-XStreamCapturedServerProcess -Processes @($unreadablePath) @processIdentity))
Check 'COM activation identity rejects an absent capture target' `
    (-not (Test-XStreamCapturedServerProcess -Processes @() @processIdentity))

$diagnosticRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('lec-xstream-lifecycle-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($diagnosticRoot) | Out-Null
try {
    $journalPath = Join-Path $diagnosticRoot 'observer-lifecycle.jsonl'
    New-XStreamLifecycleJournal -Path $journalPath
    Write-XStreamLifecycleEvent -Path $journalPath -Event 'acquisition_lookup_started' `
        -Data @{ Operation='Acquisition'; PayloadIncluded=$false }
    $journalRecord = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
    Check 'COM lookup state is durable before a blocking dispatch call' `
        ($journalRecord.Event -eq 'acquisition_lookup_started' -and
         $journalRecord.TimestampUtc -and -not $journalRecord.Data.PayloadIncluded)
} finally { Remove-Item -LiteralPath $diagnosticRoot -Recurse -Force }

$rpc = '2026-11-27T00:04:00.0000000Z'
$termination = '2026-11-27T00:04:00.0000000Z'
Check 'normal Acquisition property return is distinguished' `
    ((Get-XStreamFailureClassification -ObserverState 'AcquisitionLookupReturned') -eq
        'ACQUISITION_LOOKUP_RETURNED')
Check 'blocked Acquisition lookup with a live target is distinguished' `
    ((Get-XStreamFailureClassification -ObserverState 'AcquisitionLookupInProgress' `
        -ObserverTimedOut $true -TargetStateAtObserverTimeout 'RUNNING') -eq
        'ACQUISITION_COM_CALL_BLOCKED_WHILE_TARGET_ALIVE')
Check 'target termination after the observer snapshot does not erase the observed live-server state' `
    ((Get-XStreamFailureClassification -ObserverState 'AcquisitionLookupInProgress' `
        -ObserverTimedOut $true -TargetStateAtObserverTimeout 'RUNNING' `
        -TargetTermination 'launcher_timeout_terminated' `
        -TargetTerminationRequestedUtc '2026-11-27T00:05:00Z' `
        -TargetExitObservedUtc '2026-11-27T00:05:01Z' -TargetExitCode 1460) -eq
        'ACQUISITION_COM_CALL_BLOCKED_WHILE_TARGET_ALIVE')
Check 'natural target exit before observer result is attributed' `
    ((Get-XStreamFailureClassification -TargetTermination 'process_exited' `
        -TargetExitObservedUtc '2026-11-27T00:03:00.0000000Z' -TargetExitCode 0) -eq
        'TARGET_EXITED_NATURALLY_BEFORE_OBSERVER_RESULT')
Check 'nonzero target exit before observer result is attributed as process failure' `
    ((Get-XStreamFailureClassification -TargetTermination 'process_exited' `
        -TargetExitObservedUtc '2026-11-27T00:03:00.0000000Z' -TargetExitCode 3221225477) -eq
        'TARGET_CRASHED_BEFORE_OBSERVER_RESULT')
Check 'RPC recorded after launcher termination is not called spontaneous' `
    ((Get-XStreamFailureClassification -ObserverState 'AcquisitionLookupException' `
        -ObserverRpcUtc $rpc -TargetTermination 'launcher_timeout_terminated' `
        -TargetTerminationRequestedUtc $termination -TargetExitObservedUtc '2026-11-27T00:04:01Z') -eq
        'RPC_OR_OBSERVER_FAILURE_AFTER_LAUNCHER_TERMINATION')
Check 'RPC recorded before launcher termination remains independently attributable' `
    ((Get-XStreamFailureClassification -ObserverRpcUtc '2026-11-27T00:03:59Z' `
        -TargetTermination 'launcher_timeout_terminated' `
        -TargetTerminationRequestedUtc $termination -TargetExitObservedUtc '2026-11-27T00:04:01Z') -eq
        'OBSERVER_FAILURE_BEFORE_RECORDED_TARGET_TERMINATION')
Check 'a launcher timeout/observer timeout race records the actual event order' `
    ((Get-XStreamFailureClassification -ObserverRpcUtc $rpc `
        -TargetTermination 'launcher_timeout_terminated' `
        -TargetTerminationRequestedUtc $termination -TargetExitObservedUtc '2026-11-27T00:04:01Z') -eq
        'RPC_OR_OBSERVER_FAILURE_AFTER_LAUNCHER_TERMINATION')
Check 'launcher-induced process exit is distinct from natural exit' `
    ((Get-XStreamFailureClassification -TargetTermination 'launcher_timeout_terminated' `
        -TargetExitObservedUtc '2026-11-27T00:04:01Z') -eq
        'TARGET_EXITED_DURING_LAUNCHER_TERMINATION')
Check 'partial trace without a timestamped observer error stays indeterminate' `
    ((Get-XStreamFailureClassification -ObserverState 'AcquisitionLookupInProgress' `
        -PartialTrace $true) -eq 'PARTIAL_TRACE_WITHOUT_OBSERVER_FAILURE_TIME')
Check 'successful observer completion is not classified as a failure after process exit' `
    ((Get-XStreamFailureClassification -ObserverState 'observer_completed' `
        -ObserverFailureUtc '2026-11-27T00:05:00Z' `
        -TargetTermination 'process_exited' `
        -TargetExitObservedUtc '2026-11-27T00:04:59Z' -TargetExitCode 0) -eq
        'OBSERVER_COMPLETED')
$selfSnapshot = Get-XStreamProcessSnapshot -ProcessId $PID
Check 'bounded process/thread snapshot identifies its own live synthetic host' `
    ($selfSnapshot.State -eq 'RUNNING' -and $selfSnapshot.ThreadCount -gt 0)
$missingSnapshot = Get-XStreamProcessSnapshot -ProcessId 2147483000
Check 'missing process snapshot is explicit rather than assumed terminated' `
    ($missingSnapshot.State -eq 'NOT_RUNNING')
$nativeProcessRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('lec-xstream-process-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($nativeProcessRoot) | Out-Null
$normalProcessInfo = Join-Path $nativeProcessRoot 'native-process-normal.txt'
$normalProcess = Invoke-LecS65NativeProcess -Name 'Synthetic normal process' `
    -FilePath $env:ComSpec -Arguments @('/d','/c','exit 0') `
    -WorkingDirectory $nativeProcessRoot -ProcessInfoPath $normalProcessInfo `
    -TimeoutSeconds 10 -SuppressConsoleOutput
$normalMetadata = @{}
foreach ($line in Get-Content -LiteralPath $normalProcessInfo) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) { $normalMetadata[$line.Substring(0,$separator)] = $line.Substring($separator+1) }
}
Check 'native process normal-exit timestamps and status are durably captured' `
    ($normalProcess.ExitCode -eq 0 -and -not $normalProcess.TimedOut -and
     $normalMetadata.WaitStartedUtc -and $normalMetadata.DeadlineUtc -and
     $normalMetadata.WaitCompletedUtc -and $normalMetadata.JobCloseReturnedUtc -and
     $normalMetadata.ProcessExitObservedUtc -and $normalMetadata.TimedOut -eq 'false')
$timeoutProcessInfo = Join-Path $nativeProcessRoot 'native-process-timeout.txt'
$timeoutPowerShell = Join-Path $env:SystemRoot `
    'System32\WindowsPowerShell\v1.0\powershell.exe'
$timeoutProcess = Invoke-LecS65NativeProcess -Name 'Synthetic blocked child' `
    -FilePath $timeoutPowerShell -Arguments @('-NoProfile','-Command',
        'Start-Sleep -Seconds 10') -WorkingDirectory $nativeProcessRoot `
    -ProcessInfoPath $timeoutProcessInfo -TimeoutSeconds 1 `
    -StreamDrainTimeoutSeconds 2 -SuppressConsoleOutput `
    -ReturnOnTimeout -ReturnOnNonZeroExit
$timeoutMetadata = @{}
foreach ($line in Get-Content -LiteralPath $timeoutProcessInfo) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) { $timeoutMetadata[$line.Substring(0,$separator)] = $line.Substring($separator+1) }
}
Check 'native process timeout records cleanup request, API result and exit observation' `
    ($timeoutProcess.TimedOut -and $timeoutProcess.ProcessTreeClosed -and
     $timeoutMetadata.TerminationRequestedUtc -and
     $timeoutMetadata.JobCloseReturnedUtc -and
     $timeoutMetadata.JobCloseSucceeded -eq 'true' -and
     $timeoutMetadata.ProcessExitObservedUtc -and
     $timeoutMetadata.TimedOut -eq 'true')

Write-Host "XSTREAM COM LIFECYCLE: $passed/$($passed+$failed) passed; $failed failed."
[System.IO.Directory]::Delete($nativeProcessRoot,$true)
if ($failed -ne 0) { exit 1 }
