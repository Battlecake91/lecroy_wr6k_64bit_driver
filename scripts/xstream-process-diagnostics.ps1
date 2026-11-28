Set-StrictMode -Version Latest

function New-XStreamLifecycleJournal {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$Path)

    if (-not [IO.Path]::IsPathRooted($Path)) {
        throw 'Lifecycle journal path must be absolute.'
    }
    $stream = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
    try { $stream.Flush($true) }
    finally { $stream.Dispose() }
    $script:XStreamLifecycleJournalSequence = 0
}

function Write-XStreamLifecycleEvent {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Event,
        [hashtable]$Data = @{}
    )

    $script:XStreamLifecycleJournalSequence++
    $record = [ordered]@{
        Schema = 1
        Sequence = $script:XStreamLifecycleJournalSequence
        Event = $Event
        TimestampUtc = [DateTime]::UtcNow.ToString('o')
        Data = $Data
    }
    $line = ($record | ConvertTo-Json -Depth 8 -Compress) + "`r`n"
    $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($line)
    $stream = [IO.File]::Open($Path,[IO.FileMode]::Append,
        [IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
    try {
        $stream.Write($bytes,0,$bytes.Length)
        $stream.Flush($true)
    }
    finally { $stream.Dispose() }
}

function Get-XStreamProcessSnapshot {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][int]$ProcessId)

    $capturedUtc = [DateTime]::UtcNow.ToString('o')
    try {
        $process = Get-Process -Id $ProcessId -ErrorAction Stop
        $threads = @()
        foreach ($thread in $process.Threads) {
            $waitReason = $null
            try {
                if ($thread.ThreadState -eq [Diagnostics.ThreadState]::Wait) {
                    $waitReason = $thread.WaitReason.ToString()
                }
            }
            catch { $waitReason = 'Unavailable' }
            $threads += [pscustomobject]@{
                ThreadId = [int]$thread.Id
                State = $thread.ThreadState.ToString()
                WaitReason = $waitReason
                UserProcessorTimeMs = [int64]$thread.UserProcessorTime.TotalMilliseconds
                PrivilegedProcessorTimeMs = [int64]$thread.PrivilegedProcessorTime.TotalMilliseconds
            }
        }
        $responding = $null
        try { $responding = [bool]$process.Responding } catch { }
        [pscustomobject]@{
            CapturedUtc = $capturedUtc
            ProcessId = $ProcessId
            State = 'RUNNING'
            ProcessName = $process.ProcessName
            StartTimeUtc = $process.StartTime.ToUniversalTime().ToString('o')
            Responding = $responding
            UserProcessorTimeMs = [int64]$process.UserProcessorTime.TotalMilliseconds
            PrivilegedProcessorTimeMs = [int64]$process.PrivilegedProcessorTime.TotalMilliseconds
            ThreadCount = $threads.Count
            Threads = $threads
        }
    }
    catch {
        $notFound = $_.FullyQualifiedErrorId -like 'NoProcessFoundForGivenId*' -or
            $_.Exception -is [ComponentModel.Win32Exception] -or
            $_.Exception.Message -match 'Cannot find a process|No process with the specified identifier'
        $state = if ($notFound) { 'NOT_RUNNING' } else { 'UNKNOWN' }
        [pscustomobject]@{
            CapturedUtc = $capturedUtc
            ProcessId = $ProcessId
            State = $state
            Error = $_.Exception.Message
            ThreadCount = 0
            Threads = @()
        }
    }
}

function Get-XStreamFailureClassification {
    [CmdletBinding()]
    param(
        [string]$ObserverState,
        [string]$ObserverRpcUtc,
        [string]$ObserverFailureUtc,
        [bool]$ObserverTimedOut,
        [string]$TargetStateAtObserverTimeout,
        [string]$TargetTermination,
        [string]$TargetTerminationRequestedUtc,
        [string]$TargetExitObservedUtc,
        [int64]$TargetExitCode = -1,
        [bool]$PartialTrace
    )

    $failureUtc = $ObserverRpcUtc
    if ([string]::IsNullOrWhiteSpace($failureUtc)) { $failureUtc = $ObserverFailureUtc }
    if ([string]::IsNullOrWhiteSpace($ObserverRpcUtc) -and
        $ObserverState -in @('ObserverCompleted','observer_completed')) {
        return 'OBSERVER_COMPLETED'
    }
    if ($ObserverTimedOut -and $ObserverState -eq 'AcquisitionLookupInProgress' -and
        $TargetStateAtObserverTimeout -eq 'RUNNING') {
        return 'ACQUISITION_COM_CALL_BLOCKED_WHILE_TARGET_ALIVE'
    }
    if (-not [string]::IsNullOrWhiteSpace($failureUtc)) {
        $failure = [DateTime]::Parse($failureUtc).ToUniversalTime()
        if (-not [string]::IsNullOrWhiteSpace($TargetTerminationRequestedUtc)) {
            $termination = [DateTime]::Parse($TargetTerminationRequestedUtc).ToUniversalTime()
            if ($failure -ge $termination -and $TargetTermination -like 'launcher_*') {
                return 'RPC_OR_OBSERVER_FAILURE_AFTER_LAUNCHER_TERMINATION'
            }
        }
        if (-not [string]::IsNullOrWhiteSpace($TargetExitObservedUtc) -and
            [DateTime]::Parse($TargetExitObservedUtc).ToUniversalTime() -le $failure) {
            if ($TargetExitCode -gt 0) { return 'TARGET_FAILED_BEFORE_OBSERVER_FAILURE' }
            return 'OBSERVER_FAILURE_AFTER_TARGET_EXIT'
        }
        return 'OBSERVER_FAILURE_BEFORE_RECORDED_TARGET_TERMINATION'
    }
    if (-not [string]::IsNullOrWhiteSpace($TargetExitObservedUtc)) {
        if ($TargetTermination -like 'launcher_*') {
            return 'TARGET_EXITED_DURING_LAUNCHER_TERMINATION'
        }
        if ($TargetExitCode -gt 0) { return 'TARGET_CRASHED_BEFORE_OBSERVER_RESULT' }
        if ($TargetTermination -eq 'process_exited') {
            return 'TARGET_EXITED_NATURALLY_BEFORE_OBSERVER_RESULT'
        }
    }
    if ($ObserverTimedOut) { return 'OBSERVER_TIMEOUT_CAUSALITY_UNRESOLVED' }
    if ($ObserverState -eq 'AcquisitionLookupReturned') {
        return 'ACQUISITION_LOOKUP_RETURNED'
    }
    if ($ObserverState -eq 'ObserverCompleted') { return 'OBSERVER_COMPLETED' }
    if ($PartialTrace) { return 'PARTIAL_TRACE_WITHOUT_OBSERVER_FAILURE_TIME' }
    return 'INDETERMINATE'
}
