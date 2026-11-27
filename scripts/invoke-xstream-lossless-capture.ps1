<#
.SYNOPSIS
  Run one pinned XStream observation through the lossless user-mode IOCTL tracer.
.DESCRIPTION
  RunAuthorized starts the pinned 32-bit XStream COM server suspended through
  the pinned trace launcher, waits for injection readiness, and then invokes
  the pinned single-waveform observer against that exact process. OfflineMock
  exercises the same process, evidence, and trace-validation path with a
  synthetic target and mock waveform state. Neither mode retries.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('OfflineMock','RunAuthorized')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [Parameter(Mandatory=$true)][string]$TraceLauncherPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedTraceLauncherHash,
    [Parameter(Mandatory=$true)][string]$TraceHookPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedTraceHookHash,
    [Parameter(Mandatory=$true)][string]$TraceValidatorPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedTraceValidatorHash,
    [Parameter(Mandatory=$true)][string]$WaveformRunnerPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedWaveformRunnerHash,
    [Parameter(Mandatory=$true)][string]$XStreamExecutablePath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedXStreamExecutableHash,
    [Parameter(Mandatory=$true)][string]$ExpectedInf,
    [Parameter(Mandatory=$true)][string]$ExpectedVersion,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedSysHash,
    [string]$DeviceInstanceId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
    [ValidateRange(30,600)][int]$TimeoutSeconds = 240,
    [ValidateRange(1,60)][int]$ReadinessTimeoutSeconds = 15,
    [switch]$OwnerAuthorized,
    [switch]$PhysicalDiskDisconnectedConfirmed,
    [string]$MockStatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'native-process.ps1')
. (Join-Path $PSScriptRoot 'xstream-process-diagnostics.ps1')

function New-ExclusiveTextFile {
    param([string]$Path,[string]$Text)
    $stream = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write,[IO.FileShare]::Read)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        $stream.Write($bytes,0,$bytes.Length)
        $stream.Flush($true)
    }
    finally { $stream.Dispose() }
}

function Assert-PinnedFile {
    param([string]$Path,[string]$ExpectedHash,[string]$Label)
    if (-not [IO.Path]::IsPathRooted($Path) -or
        -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing or not an absolute file path."
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    if ($actual -ne $ExpectedHash.ToUpperInvariant()) {
        throw "$Label SHA-256 mismatch."
    }
}

function Write-JsonExclusive {
    param([string]$Path,[object]$Value)
    New-ExclusiveTextFile -Path $Path -Text (($Value | ConvertTo-Json -Depth 8) + "`r`n")
}

$resultPath = $null
$exceptionPath = $null
$traceProcess = $null
$stdoutTask = $null
$stderrTask = $null
$traceExitCode = $null
$traceStopwatch = $null
$targetPid = 0
$targetProcessRemaining = $false
$observerResult = $null
$observerProcessSnapshot = $null
$observerTargetSnapshot = $null
$observerStateAtCompletion = $null
$causalAssessment = 'INDETERMINATE'
$waveformEvidence = $null
$supervisorEventsPath = $null
$observerProcessInfoPath = $null
$observerLifecyclePath = $null
$captureMetadata = $null
$captureClassification = $null
$captureClassificationPath = $null
$captureResultPath = $null
$childPowerShellPath = Join-Path $env:SystemRoot `
    'System32\WindowsPowerShell\v1.0\powershell.exe'
$childPowerShellEnvironment = @{
    PSModulePath = @(
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules')
        (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}
try {
    if (-not [IO.Path]::IsPathRooted($EvidenceRoot)) {
        throw 'EvidenceRoot must be an absolute private path.'
    }
    $EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
    $repositoryRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
    $repositoryPrefix = $repositoryRoot.TrimEnd('\') + '\'
    if ($EvidenceRoot.Equals($repositoryRoot,
            [StringComparison]::OrdinalIgnoreCase) -or
        $EvidenceRoot.StartsWith($repositoryPrefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'EvidenceRoot must remain outside the public repository.'
    }
    if (Test-Path -LiteralPath $EvidenceRoot) {
        throw "Evidence root already exists: $EvidenceRoot"
    }
    [IO.Directory]::CreateDirectory($EvidenceRoot) | Out-Null
    $resultPath = Join-Path $EvidenceRoot 'result.json'
    $exceptionPath = Join-Path $EvidenceRoot 'exception.txt'
    $supervisorEventsPath = Join-Path $EvidenceRoot 'xstream-supervisor-events.jsonl'
    $observerProcessInfoPath = Join-Path $EvidenceRoot 'waveform-observer-process.txt'
    New-XStreamLifecycleJournal -Path $supervisorEventsPath
    Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
        -Event 'supervisor_started' -Data @{ TimeoutSeconds=$TimeoutSeconds }

    Assert-PinnedFile $TraceLauncherPath $ExpectedTraceLauncherHash 'Trace launcher'
    Assert-PinnedFile $TraceHookPath $ExpectedTraceHookHash 'Trace hook'
    if ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($TraceLauncherPath)) -ne
        [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($TraceHookPath))) {
        throw 'Trace launcher and hook must be in the same pinned directory.'
    }
    Assert-PinnedFile $TraceValidatorPath $ExpectedTraceValidatorHash 'Trace validator'
    Assert-PinnedFile $WaveformRunnerPath $ExpectedWaveformRunnerHash 'Waveform runner'
    Assert-PinnedFile $XStreamExecutablePath $ExpectedXStreamExecutableHash 'XStream target'
    if (-not $OwnerAuthorized -or -not $PhysicalDiskDisconnectedConfirmed) {
        throw 'Explicit owner authorization and physical disk isolation confirmation are required.'
    }
    if ($Mode -eq 'RunAuthorized' -and
        ($PSVersionTable.PSVersion.Major -ne 5 -or
         $PSVersionTable.PSVersion.Minor -ne 1 -or
         -not [Environment]::Is64BitProcess)) {
        throw 'RunAuthorized requires 64-bit Windows PowerShell 5.1.'
    }
    if (-not (Test-Path -LiteralPath $childPowerShellPath -PathType Leaf)) {
        throw 'The explicit Windows PowerShell 5.1 child executable is missing.'
    }
    if ($Mode -eq 'RunAuthorized') {
        $principal = New-Object Security.Principal.WindowsPrincipal(
            [Security.Principal.WindowsIdentity]::GetCurrent())
        if (-not $principal.IsInRole(
                [Security.Principal.WindowsBuiltInRole]::Administrator)) {
            throw 'RunAuthorized requires an elevated token.'
        }
    }
    if ($Mode -eq 'OfflineMock' -and
        [string]::IsNullOrWhiteSpace($MockStatePath)) {
        throw 'OfflineMock requires MockStatePath.'
    }

    $rawTrace = Join-Path $EvidenceRoot 'xstream-ioctl-private.jsonl'
    $captureResultPath = "$rawTrace.capture-result.json"
    $captureClassificationPath = Join-Path $EvidenceRoot 'capture-classification.json'
    $readyMarker = Join-Path $EvidenceRoot 'xstream-trace-ready.txt'
    $traceStdout = Join-Path $EvidenceRoot 'trace-launcher.stdout.txt'
    $traceStderr = Join-Path $EvidenceRoot 'trace-launcher.stderr.txt'
    $traceArguments = @('--embedding','--timeout-seconds',
        ($TimeoutSeconds + 60).ToString(),$XStreamExecutablePath,$rawTrace,$readyMarker)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $TraceLauncherPath
    $start.Arguments = [LecS65.NativeProcessRunner]::BuildArgumentString($traceArguments)
    $start.WorkingDirectory = [IO.Path]::GetDirectoryName(
        [IO.Path]::GetFullPath($XStreamExecutablePath))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $traceProcess = [Diagnostics.Process]::new()
    $traceProcess.StartInfo = $start
    $traceStopwatch = [Diagnostics.Stopwatch]::StartNew()
    if (-not $traceProcess.Start()) { throw 'Trace launcher did not start.' }
    Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
        -Event 'trace_launcher_started' -Data @{ ProcessId=$traceProcess.Id }
    $stdoutTask = $traceProcess.StandardOutput.ReadToEndAsync()
    $stderrTask = $traceProcess.StandardError.ReadToEndAsync()

    $deadline = [datetime]::UtcNow.AddSeconds($ReadinessTimeoutSeconds)
    while (-not (Test-Path -LiteralPath $readyMarker -PathType Leaf)) {
        if ($traceProcess.HasExited) {
            throw "Trace launcher exited before readiness with code $($traceProcess.ExitCode)."
        }
        if ([datetime]::UtcNow -ge $deadline) {
            throw 'Trace launcher did not produce readiness evidence before timeout.'
        }
        Start-Sleep -Milliseconds 100
    }
    $ready = @{}
    foreach ($line in Get-Content -LiteralPath $readyMarker) {
        $parts = $line -split '=',2
        if ($parts.Count -eq 2) { $ready[$parts[0]] = $parts[1] }
    }
    if ($ready.CaptureReady -ne 'true' -or $ready.Embedding -ne 'true' -or
        -not [int]::TryParse([string]$ready.Pid,[ref]$targetPid) -or
        $targetPid -le 0) {
        throw 'Trace readiness marker is malformed.'
    }
    Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
        -Event 'target_capture_ready' -Data @{ TargetProcessId=$targetPid }

    $waveformEvidence = Join-Path $EvidenceRoot 'waveform-evidence'
    $waveformMode = if ($Mode -eq 'RunAuthorized') {'RunAuthorized'} else {'OfflineMock'}
    $waveformArguments = @('-NoProfile','-NonInteractive','-STA','-ExecutionPolicy','Bypass',
        '-File',$WaveformRunnerPath,'-Mode',$waveformMode,
        '-EvidenceRoot',$waveformEvidence,
        '-XStreamExecutablePath',$XStreamExecutablePath,
        '-ExpectedXStreamExecutableHash',$ExpectedXStreamExecutableHash,
        '-ExpectedInf',$ExpectedInf,'-ExpectedVersion',$ExpectedVersion,
        '-ExpectedSysHash',$ExpectedSysHash,'-DeviceInstanceId',$DeviceInstanceId,
        '-TimeoutSeconds',$TimeoutSeconds.ToString(),'-OwnerAuthorized',
        '-PhysicalDiskDisconnectedConfirmed')
    if ($Mode -eq 'RunAuthorized') {
        $waveformArguments += @('-ExpectedPrelaunchedProcessId',$targetPid.ToString())
    }
    else { $waveformArguments += @('-MockStatePath',$MockStatePath) }
    $observerLifecyclePath = Join-Path $waveformEvidence 'observer-lifecycle.jsonl'
    $observerDispatchUtc = [DateTime]::UtcNow.ToString('o')
    Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
        -Event 'waveform_observer_dispatched' -Data @{
            DispatchUtc=$observerDispatchUtc
            TargetProcessId=$targetPid
            ObserverTimeoutSeconds=($TimeoutSeconds + 5)
            TargetTimeoutSeconds=($TimeoutSeconds + 60)
        }
    $waveform = Invoke-LecS65NativeProcess -Name 'Pinned XStream waveform observer' `
        -FilePath $childPowerShellPath -Arguments $waveformArguments `
        -WorkingDirectory (Split-Path $WaveformRunnerPath -Parent) `
        -LogPath (Join-Path $EvidenceRoot 'waveform-runner.log') `
        -EnvironmentVariables $childPowerShellEnvironment `
        -TimeoutSeconds ($TimeoutSeconds + 5) -StreamDrainTimeoutSeconds 2 `
        -ProcessInfoPath $observerProcessInfoPath `
        -ReturnOnTimeout -ReturnOnNonZeroExit
    $observerResult = $waveform
    $observerProcessSnapshot = if (Test-Path -LiteralPath $observerProcessInfoPath) {
        $processInfo = @{}
        foreach ($line in Get-Content -LiteralPath $observerProcessInfoPath) {
            $separator = $line.IndexOf('=')
            if ($separator -gt 0) {
                $processInfo[$line.Substring(0,$separator)] = $line.Substring($separator + 1)
            }
        }
        [pscustomobject]$processInfo
    } else { $null }
    $observerTargetSnapshot = if ($targetPid -gt 0) {
        Get-XStreamProcessSnapshot -ProcessId $targetPid
    } else { $null }
    if (Test-Path -LiteralPath $observerLifecyclePath -PathType Leaf) {
        try {
            $lastObserverLine = Get-Content -LiteralPath $observerLifecyclePath -Tail 1
            if ($lastObserverLine) { $observerStateAtCompletion = $lastObserverLine | ConvertFrom-Json }
        } catch { }
    }
    Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
        -Event 'waveform_observer_supervised_result' -Data @{
            CompletedUtc=[DateTime]::UtcNow.ToString('o')
            ProcessId=$waveform.ProcessId
            ExitCode=$waveform.ExitCode
            TimedOut=$waveform.TimedOut
            ProcessTreeClosed=$waveform.ProcessTreeClosed
            ObserverState=$observerStateAtCompletion
            TargetSnapshot=$observerTargetSnapshot
        }
    if ($waveform.TimedOut) {
        throw 'Waveform observer exceeded its independent process deadline; see lifecycle and process snapshots.'
    }
    if ($waveform.ExitCode -ne 0) {
        throw "Waveform observer exited with code $($waveform.ExitCode)."
    }

    if (-not $traceProcess.WaitForExit(90000)) {
        throw 'Instrumented XStream process remained after the observer completed.'
    }
    $traceExitCode = $traceProcess.ExitCode
    New-ExclusiveTextFile $traceStdout $stdoutTask.Result
    New-ExclusiveTextFile $traceStderr $stderrTask.Result
    if (-not (Test-Path -LiteralPath $captureResultPath -PathType Leaf)) {
        throw 'Trace launcher exited without durable capture-finalization metadata.'
    }
    $captureMetadata = Get-Content -Raw -LiteralPath $captureResultPath | ConvertFrom-Json
    $waveformResult = Get-Content -Raw -LiteralPath `
        (Join-Path $waveformEvidence 'result.json') | ConvertFrom-Json
    $causalAssessment = Get-XStreamFailureClassification `
        -ObserverState 'ObserverCompleted' `
        -ObserverRpcUtc $waveformResult.RpcServerUnavailableUtc `
        -ObserverFailureUtc $waveformResult.CompletedUtc `
        -ObserverTimedOut $false `
        -TargetStateAtObserverTimeout ([string]$(if ($observerTargetSnapshot) { $observerTargetSnapshot.State } else { 'UNKNOWN' })) `
        -TargetTermination ([string]$captureMetadata.termination) `
        -TargetTerminationRequestedUtc ([string]$captureMetadata.termination_requested_utc) `
        -TargetExitObservedUtc ([string]$captureMetadata.target_exit_observed_utc) `
        -TargetExitCode ([int]$captureMetadata.target_exit_code) `
        -PartialTrace (-not [bool]$captureMetadata.footer_present)

    $classificationArguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
        '-File',$TraceValidatorPath,'-TracePath',$rawTrace,
        '-CaptureResultPath',$captureResultPath,'-ResultPath',$captureClassificationPath,
        '-PrivateEvidenceRoot',$EvidenceRoot,'-RepositoryRoot',$repositoryRoot,
        '-ClassifyOnly')
    $classification = Invoke-LecS65NativeProcess -Name 'XStream capture classifier' `
        -FilePath $childPowerShellPath -Arguments $classificationArguments `
        -WorkingDirectory (Split-Path $TraceValidatorPath -Parent) `
        -LogPath (Join-Path $EvidenceRoot 'capture-classifier.log') `
        -EnvironmentVariables $childPowerShellEnvironment `
        -TimeoutSeconds 30 -ReturnOnNonZeroExit
    if ($classification.ExitCode -ne 0) {
        throw "Capture classification exited with code $($classification.ExitCode)."
    }
    $captureClassification = Get-Content -Raw -LiteralPath `
        $captureClassificationPath | ConvertFrom-Json
    if ($captureClassification.Classification -ne 'COMPLETE_NORMAL_CAPTURE') {
        throw "Capture is not complete: $($captureClassification.Classification)."
    }
    if ($traceExitCode -ne 0) {
        throw "Trace launcher exited with code $traceExitCode."
    }

    $validationPath = Join-Path $EvidenceRoot 'capture-validation.json'
    $validatorArguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
        '-File',$TraceValidatorPath,'-TracePath',$rawTrace,
        '-CaptureResultPath',$captureResultPath,
        '-ResultPath',$validationPath,'-PrivateEvidenceRoot',$EvidenceRoot,
        '-RepositoryRoot',$repositoryRoot)
    $validation = Invoke-LecS65NativeProcess -Name 'Lossless XStream trace validator' `
        -FilePath $childPowerShellPath -Arguments $validatorArguments `
        -WorkingDirectory (Split-Path $TraceValidatorPath -Parent) `
        -LogPath (Join-Path $EvidenceRoot 'capture-validator.log') `
        -EnvironmentVariables $childPowerShellEnvironment `
        -TimeoutSeconds 30 -ReturnOnNonZeroExit
    if ($validation.ExitCode -ne 0) {
        throw "Trace validator exited with code $($validation.ExitCode)."
    }
    $capture = Get-Content -Raw -LiteralPath $validationPath | ConvertFrom-Json
    if ($capture.Result -ne 'PASS') { throw 'Lossless trace validation failed.' }
    $result = [ordered]@{
        Schema = 1; Result = 'PASS'; Mode = $Mode
        CompletedUtc = [datetime]::UtcNow.ToString('o')
        TraceLauncherExitCode = $traceExitCode
        TargetProcessId = $targetPid
        TargetProcessRemaining = $false
        CaptureFinalization = $captureMetadata
        CaptureClassification = $captureClassification
        Capture = $capture
        Waveform = $waveformResult
        ObserverProcess = $observerProcessSnapshot
        TargetSnapshotAtObserverCompletion = $observerTargetSnapshot
        CausalAssessment = $causalAssessment
        SupervisorEventsPath = $supervisorEventsPath
        RetryCount = 0
    }
    Write-JsonExclusive $resultPath $result
    exit 0
}
catch {
    if ($targetPid -gt 0) {
        if ($null -eq $observerTargetSnapshot) {
            $observerTargetSnapshot = Get-XStreamProcessSnapshot -ProcessId $targetPid
        }
        try {
            Write-XStreamLifecycleEvent -Path $supervisorEventsPath `
                -Event 'failure_target_snapshot_before_cleanup' -Data @{
                    Snapshot=$observerTargetSnapshot
                    ObserverProcess=$observerProcessSnapshot
                }
        } catch { }
        if ($null -eq $observerResult -and $null -ne $observerTargetSnapshot -and
            $observerTargetSnapshot.State -eq 'RUNNING') {
            $target = @(Get-Process -Id $targetPid -ErrorAction SilentlyContinue)
            foreach ($process in $target) {
                try { [void]$process.CloseMainWindow() } catch {}
            }
        }
        if ($null -ne $traceProcess -and -not $traceProcess.HasExited -and
            $null -ne $traceStopwatch) {
            $targetBoundMilliseconds = ($TimeoutSeconds + 60) * 1000
            $waitMilliseconds = [Math]::Max(1000,
                $targetBoundMilliseconds - [int]$traceStopwatch.ElapsedMilliseconds + 15000)
            try { [void]$traceProcess.WaitForExit($waitMilliseconds) } catch { }
        }
        $targetProcessRemaining = @(
            Get-Process -Id $targetPid -ErrorAction SilentlyContinue).Count -ne 0
    }
    if ($null -ne $traceProcess -and $traceProcess.HasExited) {
        $traceExitCode = $traceProcess.ExitCode
        if ($null -ne $stdoutTask -and -not (Test-Path -LiteralPath `
                (Join-Path $EvidenceRoot 'trace-launcher.stdout.txt'))) {
            try { New-ExclusiveTextFile (Join-Path $EvidenceRoot 'trace-launcher.stdout.txt') $stdoutTask.Result } catch {}
        }
        if ($null -ne $stderrTask -and -not (Test-Path -LiteralPath `
                (Join-Path $EvidenceRoot 'trace-launcher.stderr.txt'))) {
            try { New-ExclusiveTextFile (Join-Path $EvidenceRoot 'trace-launcher.stderr.txt') $stderrTask.Result } catch {}
        }
    }
    if ($null -ne $captureResultPath -and
        (Test-Path -LiteralPath $captureResultPath -PathType Leaf) -and
        (Test-Path -LiteralPath $rawTrace -PathType Leaf) -and
        -not (Test-Path -LiteralPath $captureClassificationPath)) {
        try {
            $classifyArguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
                '-File',$TraceValidatorPath,'-TracePath',$rawTrace,
                '-CaptureResultPath',$captureResultPath,'-ResultPath',$captureClassificationPath,
                '-PrivateEvidenceRoot',$EvidenceRoot,'-RepositoryRoot',$repositoryRoot,
                '-ClassifyOnly')
            [void](Invoke-LecS65NativeProcess -Name 'XStream failure capture classifier' `
                -FilePath $childPowerShellPath -Arguments $classifyArguments `
                -WorkingDirectory (Split-Path $TraceValidatorPath -Parent) `
                -LogPath (Join-Path $EvidenceRoot 'capture-classifier.log') `
                -EnvironmentVariables $childPowerShellEnvironment `
                -TimeoutSeconds 30 -ReturnOnNonZeroExit)
        } catch { }
    }
    if ($null -ne $captureResultPath -and
        (Test-Path -LiteralPath $captureResultPath -PathType Leaf)) {
        try { $captureMetadata = Get-Content -Raw -LiteralPath $captureResultPath | ConvertFrom-Json } catch { }
    }
    $waveformResult = $null
    if ($null -ne $observerResult -and
        (Test-Path -LiteralPath (Join-Path $waveformEvidence 'result.json') -PathType Leaf)) {
        try { $waveformResult = Get-Content -Raw -LiteralPath `
            (Join-Path $waveformEvidence 'result.json') | ConvertFrom-Json } catch { }
    }
    $lastObserverEvent = $observerStateAtCompletion
    if ($null -eq $lastObserverEvent -and
        (Test-Path -LiteralPath $observerLifecyclePath -PathType Leaf)) {
        try {
            $lastLine = Get-Content -LiteralPath $observerLifecyclePath -Tail 1
            if ($lastLine) { $lastObserverEvent = $lastLine | ConvertFrom-Json }
        } catch { }
    }
    $lastObserverEventName = if ($null -ne $lastObserverEvent) {
        [string]$lastObserverEvent.Event
    } else { '' }
    $observerState = if ($lastObserverEventName -eq 'acquisition_lookup_started') {
        'AcquisitionLookupInProgress'
    } elseif ($lastObserverEventName -eq 'acquisition_lookup_exception') {
        'AcquisitionLookupException'
    } elseif ($lastObserverEventName -eq 'com_root_activation_started') {
        'ComRootActivationInProgress'
    } else { $lastObserverEventName }
    $observerRpcUtc = ''
    $observerFailureUtc = ''
    if ($null -ne $waveformResult) {
        $observerRpcUtc = [string]$waveformResult.RpcServerUnavailableUtc
        $observerFailureUtc = [string]$waveformResult.CompletedUtc
    }
    $targetTermination = 'unknown_termination'
    $targetTerminationRequestedUtc = ''
    $targetExitObservedUtc = ''
    $targetExitCode = -1
    $partialTrace = $false
    if ($null -ne $captureMetadata) {
        $targetTermination = [string]$captureMetadata.termination
        $targetTerminationRequestedUtc = [string]$captureMetadata.termination_requested_utc
        $targetExitObservedUtc = [string]$captureMetadata.target_exit_observed_utc
        if ($null -ne $captureMetadata.target_exit_code) {
            $targetExitCode = [int64]$captureMetadata.target_exit_code
        }
        $partialTrace = -not [bool]$captureMetadata.footer_present
    }
    $causalAssessment = Get-XStreamFailureClassification `
        -ObserverState $observerState `
        -ObserverRpcUtc $observerRpcUtc `
        -ObserverFailureUtc $observerFailureUtc `
        -ObserverTimedOut ([bool]($null -ne $observerResult -and $observerResult.TimedOut)) `
        -TargetStateAtObserverTimeout ([string]$(if ($observerTargetSnapshot) { $observerTargetSnapshot.State } else { 'UNKNOWN' })) `
        -TargetTermination $targetTermination `
        -TargetTerminationRequestedUtc $targetTerminationRequestedUtc `
        -TargetExitObservedUtc $targetExitObservedUtc `
        -TargetExitCode $targetExitCode `
        -PartialTrace $partialTrace
    if ($null -ne $captureClassificationPath -and
        (Test-Path -LiteralPath $captureClassificationPath -PathType Leaf)) {
        try { $captureClassification = Get-Content -Raw -LiteralPath $captureClassificationPath | ConvertFrom-Json } catch { }
    }
    if ($null -ne $exceptionPath -and -not (Test-Path -LiteralPath $exceptionPath)) {
        try { New-ExclusiveTextFile $exceptionPath ($_ | Out-String) } catch {}
    }
    if ($null -ne $resultPath -and -not (Test-Path -LiteralPath $resultPath)) {
        try {
            Write-JsonExclusive $resultPath ([ordered]@{
                Schema=1;Result='FAIL';Mode=$Mode
                CompletedUtc=[datetime]::UtcNow.ToString('o')
                TraceLauncherExitCode=$traceExitCode;TargetProcessId=$targetPid
                TargetProcessRemaining=$targetProcessRemaining
                CaptureFinalization=$captureMetadata
                CaptureClassification=$captureClassification
                Waveform=$waveformResult
                ObserverProcess=$observerProcessSnapshot
                ObserverState=$observerState
                TargetSnapshotAtObserverCompletion=$observerTargetSnapshot
                CausalAssessment=$causalAssessment
                SupervisorEventsPath=$supervisorEventsPath
                RetryCount=0;Message=$_.Exception.Message
            })
        } catch {}
    }
    Write-Error $_
    exit 1
}
finally {
    if ($null -ne $traceProcess) { $traceProcess.Dispose() }
}
