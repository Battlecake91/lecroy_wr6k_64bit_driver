<#
.SYNOPSIS
  Validate or execute one diagnostic capture/installation attempt.
.DESCRIPTION
  This fail-closed launcher has four explicit modes. Preflight is passive.
  CaptureQualification may start only the pinned capture executable.
  InstallWithCapture may invoke the pinned PnPUtil executable at most once,
  and only after capture readiness. RestartWithCapture may invoke exactly one
  pinned PnPUtil restart for the exact device instance. The launcher never
  elevates itself, retries, rolls back, opens a device or changes system policy.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Preflight', 'CaptureQualification', 'InstallWithCapture',
        'RestartWithCapture')]
    [string]$Mode,
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedLauncherCommit,
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedManifestHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedPackageWorkflowCommit,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverSourceCommit,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverTree,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedIncludeTree,
    [Parameter(Mandatory=$true)][string]$ExpectedDriverVer,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCertificateThumbprint,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedInfHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedCatHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedSysHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedPdbHash,
    [Parameter(Mandatory=$true)][string]$DebugViewPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedDebugViewHash,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [string]$AuthorizationId,
    [switch]$AuthorizeKernelCapture,
    [switch]$AuthorizeInstallation,
    [switch]$AuthorizePnpRestart,
    [string]$PnPUtilPath = "$env:SystemRoot\System32\pnputil.exe",
    [ValidatePattern('^$|^[0-9a-fA-F]{64}$')][string]$ExpectedPnPUtilHash,
    [ValidateRange(1, 3600)][int]$CaptureDurationSeconds = 300,
    [ValidateRange(1, 120)][int]$CaptureReadinessTimeoutSeconds = 60,
    [ValidateRange(1, 120)][int]$CaptureShutdownGraceSeconds = 15,
    [ValidateRange(100, 5000)][int]$CaptureStabilityMilliseconds = 500,
    [string]$DeviceInstanceId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
    [string]$ExpectedCurrentInf = 'oem17.inf',
    [string]$ExpectedCurrentVersion = '0.2026.1017.1',
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedCurrentSysHash =
        'F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D',
    [switch]$TestMode,
    [string]$MockStatePath,
    [ValidateSet('', 'AfterEvidenceInitialization', 'BeforeCaptureStart')]
    [string]$FailureInjection = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:EvidenceInitialized = $false
$script:StdoutPath = $null
$script:StderrPath = $null
$script:ExceptionPath = $null
$script:ResultPath = $null
$script:CaptureJob = $null
$script:CaptureResult = $null
$script:CaptureLogPath = $null
$script:InstallInvocations = 0
$script:PnpRestartInvocations = 0
$script:CaptureInvocations = 0

function Get-AbsolutePath {
    param([string]$Path, [string]$Label)
    if (-not [IO.Path]::IsPathRooted($Path)) {
        throw "$Label must be an absolute path: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

function New-ExclusiveFile {
    param([string]$Path)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    $stream.Dispose()
}

function Write-ExclusiveTextFile {
    param([string]$Path, [string]$Text)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally { $stream.Dispose() }
}

function Write-LauncherOutput {
    param([string]$Text)
    if ($script:EvidenceInitialized) {
        [IO.File]::AppendAllText($script:StdoutPath,
            $Text + [Environment]::NewLine, [Text.Encoding]::UTF8)
    }
    Write-Host $Text
}

function Write-LauncherError {
    param([string]$Text)
    if ($script:EvidenceInitialized) {
        [IO.File]::AppendAllText($script:StderrPath,
            $Text + [Environment]::NewLine, [Text.Encoding]::UTF8)
    }
    [Console]::Error.WriteLine($Text)
}

function Write-Result {
    param([string]$Status, [int]$ExitCode, [string]$Message)
    if (-not $script:EvidenceInitialized) { return }
    $result = [ordered]@{
        Schema = 1
        Mode = $Mode
        Status = $Status
        ExitCode = $ExitCode
        Message = $Message
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
        CaptureInvocations = $script:CaptureInvocations
        InstallInvocations = $script:InstallInvocations
        PnpRestartInvocations = $script:PnpRestartInvocations
    }
    $json = $result | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText($script:ResultPath,
        $json + [Environment]::NewLine, [Text.Encoding]::UTF8)
}

function Invoke-LauncherGit {
    param([string]$Repository, [string[]]$Arguments)
    $git = (Get-Command git.exe -ErrorAction Stop).Source
    $result = Invoke-LecS65NativeProcess -Name 'Diagnostic launcher Git preflight' `
        -FilePath $git -Arguments (@('-C', $Repository) + $Arguments) `
        -WorkingDirectory $Repository -TimeoutSeconds 60
    return $result.StdOut.Trim()
}

function Assert-FileHash {
    param([string]$Path, [string]$Expected, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    if ($actual -ne $Expected.ToUpperInvariant()) {
        throw "$Label SHA-256 mismatch: expected $Expected, got $actual"
    }
}

function Get-CurrentDriverState {
    if ($TestMode) {
        if (-not (Test-Path -LiteralPath $MockStatePath -PathType Leaf)) {
            throw "MockStatePath is missing: $MockStatePath"
        }
        return Get-Content -Raw -LiteralPath $MockStatePath | ConvertFrom-Json
    }

    $keys = @('DEVPKEY_Device_DriverInfPath',
        'DEVPKEY_Device_DriverVersion', 'DEVPKEY_Device_ProblemCode',
        'DEVPKEY_Device_Service')
    $properties = Get-PnpDeviceProperty -InstanceId $DeviceInstanceId `
        -KeyName $keys
    $map = @{}
    foreach ($property in $properties) { $map[$property.KeyName] = $property.Data }
    $serviceName = [string]$map['DEVPKEY_Device_Service']
    $loadedSysPath = "$env:SystemRoot\System32\drivers\LecS65AcqDrv.sys"
    $loadedSysHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $loadedSysPath).Hash
    return [pscustomobject]@{
        Inf = [string]$map['DEVPKEY_Device_DriverInfPath']
        Version = $map['DEVPKEY_Device_DriverVersion'].ToString()
        ProblemCode = [int]$map['DEVPKEY_Device_ProblemCode']
        DeviceStatus = (Get-PnpDevice -InstanceId $DeviceInstanceId).Status
        ServiceName = $serviceName
        ServiceStatus = (Get-Service -Name $serviceName).Status.ToString()
        LoadedSysHash = $loadedSysHash
    }
}

function Assert-PassiveIdentity {
    param([string]$Repository, [string]$Package, [string]$DebugView)

    $head = Invoke-LauncherGit -Repository $Repository -Arguments @('rev-parse', 'HEAD')
    $status = Invoke-LauncherGit -Repository $Repository `
        -Arguments @('status', '--porcelain', '--untracked-files=normal')
    if ($head -ne $ExpectedLauncherCommit.ToLowerInvariant()) {
        throw "Launcher commit mismatch: expected $ExpectedLauncherCommit, got $head"
    }
    if (-not [string]::IsNullOrEmpty($status)) {
        throw "Launcher worktree is not clean:`n$status"
    }

    $manifestPath = Join-Path $Package 'evidence\manifest.json'
    Assert-FileHash -Path $manifestPath -Expected $ExpectedManifestHash `
        -Label 'Package manifest'
    try { $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json }
    catch { throw "Package manifest is malformed: $($_.Exception.Message)" }
    if ($manifest.Schema -ne 2 -or
        $manifest.PackageState -ne 'SignedCandidate' -or
        $manifest.Source.WorkflowCommit -ne $ExpectedPackageWorkflowCommit -or
        $manifest.Source.ReviewedDriverTreeCommit -ne $ExpectedDriverSourceCommit -or
        $manifest.Source.DriverTree -ne $ExpectedDriverTree -or
        $manifest.Source.IncludeTree -ne $ExpectedIncludeTree -or
        $manifest.PackageIdentity.DriverVer -ne $ExpectedDriverVer -or
        $manifest.PackageIdentity.Signed -ne $true -or
        $manifest.PackageIdentity.Certificate.Thumbprint -ne
            $ExpectedCertificateThumbprint.ToUpperInvariant() -or
        $manifest.PackageIdentity.SysSignatureStatus -ne 'Valid' -or
        $manifest.PackageIdentity.CatSignatureStatus -ne 'Valid' -or
        $manifest.Build.RuntimeConfiguration -ne 'Release' -or
        $manifest.Build.PnpStatusDiagnostics -ne $true -or
        $manifest.Build.DiagnosticMarker -ne 'PNP_STATUS:') {
        throw 'Package manifest identity does not match the approved diagnostic candidate.'
    }

    $artifactExpectations = @{
        'LecS65AcqDrv.inf' = $ExpectedInfHash
        'LecS65AcqDrv.cat' = $ExpectedCatHash
        'LecS65AcqDrv.sys' = $ExpectedSysHash
        'LecS65AcqDrv.pdb' = $ExpectedPdbHash
    }
    foreach ($name in $artifactExpectations.Keys) {
        $entry = @($manifest.Artifacts | Where-Object { $_.Name -eq $name })
        if ($entry.Count -ne 1 -or
            $entry[0].SHA256 -ne $artifactExpectations[$name].ToUpperInvariant()) {
            throw "Manifest artifact identity mismatch: $name"
        }
        $path = Join-Path $Package $entry[0].RelativePath
        Assert-FileHash -Path $path -Expected $artifactExpectations[$name] `
            -Label $name
    }

    if (-not $TestMode) {
        foreach ($signedName in @('LecS65AcqDrv.sys', 'LecS65AcqDrv.cat')) {
            $signedPath = Join-Path $Package "package\$signedName"
            $signature = Get-AuthenticodeSignature -FilePath $signedPath
            if ($signature.Status -ne 'Valid' -or
                $signature.SignerCertificate.Thumbprint -ne
                    $ExpectedCertificateThumbprint.ToUpperInvariant()) {
                throw "Authenticode validation failed: $signedName"
            }
        }
    }

    Assert-FileHash -Path $DebugView -Expected $ExpectedDebugViewHash `
        -Label 'DebugView executable'
    $state = Get-CurrentDriverState
    if ($state.Inf -ne $ExpectedCurrentInf -or
        $state.Version -ne $ExpectedCurrentVersion -or
        [int]$state.ProblemCode -ne 0 -or
        $state.DeviceStatus -ne 'OK' -or
        $state.ServiceStatus -ne 'Running' -or
        $state.LoadedSysHash -ne $ExpectedCurrentSysHash.ToUpperInvariant()) {
        throw 'Current driver state does not match the approved preflight baseline.'
    }

    if (-not $TestMode) {
        $blockedNames = @('XStream.exe', 'lecdiag.exe', 'lecwatch.exe',
            'Acquisition.exe', 'WaveStudio.exe', 'ScopeExplorer.exe')
        $blocked = @(Get-CimInstance Win32_Process | Where-Object {
            $blockedNames -contains $_.Name
        } | Select-Object ProcessId, Name, ExecutablePath)
        if ($blocked.Count -ne 0) {
            throw "Blocked acquisition client is running: $($blocked.Name -join ', ')"
        }
        if (@(Get-Process -Name 'dbgview', 'dbgview64', 'dbgviewcli64' `
                -ErrorAction SilentlyContinue).Count -ne 0) {
            throw 'A DebugView process is already running.'
        }
    }
    return $state
}

function Start-CaptureJob {
    param([string]$Executable, [string[]]$Arguments,
        [string]$WorkingDirectory, [string]$LogPath, [string]$RunnerPath,
        [string]$LiveStdOutPath, [string]$LiveStdErrPath,
        [string]$ProcessInfoPath)
    $script:CaptureInvocations++
    $argumentJson = ConvertTo-Json -InputObject @($Arguments) -Compress
    return Start-Job -ScriptBlock {
        param($Runner, $File, $ArgumentJson, $Working, $Log, $Timeout,
            $LiveOut, $LiveErr, $ProcessInfo)
        $ErrorActionPreference = 'Stop'
        . $Runner
        $parsedArguments = $ArgumentJson | ConvertFrom-Json
        [string[]]$nativeArguments = @($parsedArguments |
            ForEach-Object { [string]$_ })
        Invoke-LecS65NativeProcess -Name 'Diagnostic kernel capture' `
            -FilePath $File -Arguments $nativeArguments -WorkingDirectory $Working `
            -LogPath $Log -TimeoutSeconds $Timeout -ReturnOnNonZeroExit `
            -LiveStdOutPath $LiveOut -LiveStdErrPath $LiveErr `
            -ProcessInfoPath $ProcessInfo
    } -ArgumentList $RunnerPath, $Executable, $argumentJson, $WorkingDirectory,
        $LogPath, ($CaptureDurationSeconds + $CaptureShutdownGraceSeconds),
        $LiveStdOutPath, $LiveStdErrPath, $ProcessInfoPath
}

function Test-ConfirmedKernelCaptureLine {
    param([string]$Line)
    if ([string]::IsNullOrWhiteSpace($Line)) { return $false }
    return $Line -match (
        '^(?:\d+\s+\d{1,2}:\d{2}:\d{2}\.\d+\s+\d+\s+-\s+' +
        '(?:PNP_STATUS|STORMINI):|MOCK_KERNEL:)')
}

function Assert-CaptureJobRunning {
    param([System.Management.Automation.Job]$Job, [string]$Boundary)
    if ($null -eq $Job -or $Job.State -ne 'Running') {
        throw "Capture collector exited before $Boundary."
    }
}

function Complete-CaptureJob {
    param([System.Management.Automation.Job]$Job, [int]$TimeoutSeconds)
    $completed = Wait-Job -Job $Job -Timeout $TimeoutSeconds
    if ($null -eq $completed) {
        throw "Capture child exceeded its $TimeoutSeconds-second completion bound."
    }
    $items = @(Receive-Job -Job $Job -ErrorAction SilentlyContinue)
    $result = @($items | Where-Object {
        $null -ne $_.PSObject.Properties['ExitCode']
    } | Select-Object -Last 1)
    if ($result.Count -ne 1) {
        if ($null -ne $script:CaptureLogPath -and
            (Test-Path -LiteralPath $script:CaptureLogPath -PathType Leaf) -and
            (Get-Content -Raw -LiteralPath $script:CaptureLogPath) -match
                'PROCESS TIMEOUT') {
            throw 'Capture child timed out and its bounded process tree was terminated.'
        }
        $reason = $Job.ChildJobs[0].JobStateInfo.Reason
        if ($null -ne $reason) {
            throw "Capture child failed without a native-process result: $($reason.Message)"
        }
        throw 'Capture child ended without a native-process result.'
    }
    if ([int]$result[0].ExitCode -ne 0) {
        throw "Capture child exited with code $($result[0].ExitCode)."
    }
    return $result[0]
}

try {
    $principal = [Security.Principal.WindowsPrincipal](
        [Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'The diagnostic launcher requires an elevated token.'
    }
    if ($PSVersionTable.PSVersion.Major -ne 5 -or
        $PSVersionTable.PSVersion.Minor -ne 1 -or
        -not [Environment]::Is64BitProcess) {
        throw 'The diagnostic launcher requires 64-bit Windows PowerShell 5.1.'
    }
    if (-not $TestMode -and $Mode -ne 'Preflight') {
        if ($CaptureReadinessTimeoutSeconds -lt 60) {
            throw 'Real capture modes require at least 60 seconds for buffered kernel-log readiness.'
        }
        if ($CaptureDurationSeconds -lt
            ($CaptureReadinessTimeoutSeconds + 120)) {
            throw 'Real capture modes require at least 120 seconds after the readiness deadline.'
        }
    }

    $repository = Get-AbsolutePath $RepositoryRoot 'RepositoryRoot'
    $package = Get-AbsolutePath $PackageRoot 'PackageRoot'
    $debugView = Get-AbsolutePath $DebugViewPath 'DebugViewPath'
    $evidence = Get-AbsolutePath $EvidenceRoot 'EvidenceRoot'
    if (-not (Test-Path -LiteralPath $evidence)) {
        [IO.Directory]::CreateDirectory($evidence) | Out-Null
    }
    elseif (-not (Test-Path -LiteralPath $evidence -PathType Container)) {
        throw "EvidenceRoot is not a directory: $evidence"
    }
    $script:StdoutPath = Join-Path $evidence 'launcher.stdout.txt'
    $script:StderrPath = Join-Path $evidence 'launcher.stderr.txt'
    $script:ExceptionPath = Join-Path $evidence 'launcher.exception.txt'
    $script:ResultPath = Join-Path $evidence 'result.json'
    foreach ($path in @($script:StdoutPath, $script:StderrPath,
            $script:ExceptionPath, $script:ResultPath)) {
        if (Test-Path -LiteralPath $path) {
            throw "Evidence path already exists: $path"
        }
    }
    New-ExclusiveFile $script:StdoutPath
    New-ExclusiveFile $script:StderrPath
    New-ExclusiveFile $script:ExceptionPath
    New-ExclusiveFile $script:ResultPath
    $script:EvidenceInitialized = $true
    if ($FailureInjection -eq 'AfterEvidenceInitialization') {
        throw 'Injected failure after evidence initialization.'
    }

    . (Join-Path $PSScriptRoot 'native-process.ps1')
    if (-not (Test-Path -LiteralPath $repository -PathType Container) -or
        -not (Test-Path -LiteralPath $package -PathType Container)) {
        throw 'RepositoryRoot or PackageRoot is missing.'
    }
    if ($TestMode -and [string]::IsNullOrWhiteSpace($MockStatePath)) {
        throw 'MockStatePath is required in TestMode.'
    }
    if (-not $TestMode -and $FailureInjection -ne '') {
        throw 'FailureInjection is allowed only in TestMode.'
    }

    if ($Mode -eq 'Preflight') {
        if ($AuthorizeKernelCapture -or $AuthorizeInstallation -or
            $AuthorizePnpRestart -or
            -not [string]::IsNullOrWhiteSpace($AuthorizationId)) {
            throw 'Preflight forbids active authorization switches.'
        }
    }
    elseif ($Mode -eq 'CaptureQualification') {
        if (-not $AuthorizeKernelCapture -or $AuthorizeInstallation -or
            $AuthorizePnpRestart -or
            [string]::IsNullOrWhiteSpace($AuthorizationId)) {
            throw 'CaptureQualification requires only explicit capture authorization.'
        }
    }
    elseif ($Mode -eq 'InstallWithCapture') {
        if (-not $AuthorizeKernelCapture -or -not $AuthorizeInstallation -or
            $AuthorizePnpRestart -or
            [string]::IsNullOrWhiteSpace($AuthorizationId)) {
            throw 'InstallWithCapture requires explicit capture and installation authorization.'
        }
        if ([string]::IsNullOrWhiteSpace($ExpectedPnPUtilHash)) {
            throw 'InstallWithCapture requires ExpectedPnPUtilHash.'
        }
    }
    else {
        if (-not $AuthorizeKernelCapture -or $AuthorizeInstallation -or
            -not $AuthorizePnpRestart -or
            [string]::IsNullOrWhiteSpace($AuthorizationId)) {
            throw 'RestartWithCapture requires explicit capture and PnP restart authorization.'
        }
        if ([string]::IsNullOrWhiteSpace($ExpectedPnPUtilHash)) {
            throw 'RestartWithCapture requires ExpectedPnPUtilHash.'
        }
    }

    Write-LauncherOutput "BEGIN $Mode $([DateTime]::UtcNow.ToString('o'))"
    $state = Assert-PassiveIdentity -Repository $repository -Package $package `
        -DebugView $debugView
    Write-LauncherOutput 'PASSIVE PREFLIGHT PASS'
    if ($Mode -eq 'Preflight') {
        Write-Result 'PASS' 0 'Passive preflight completed; no active operation was invoked.'
        Write-LauncherOutput 'DIAGNOSTIC LAUNCHER PASS (Preflight)'
        exit 0
    }
    if ($FailureInjection -eq 'BeforeCaptureStart') {
        throw 'Injected failure before capture start.'
    }

    $captureMarker = Join-Path $evidence 'capture-attempt.marker'
    $installMarker = Join-Path $evidence 'install-attempt.marker'
    $restartMarker = Join-Path $evidence 'pnp-restart-attempt.marker'
    foreach ($marker in @($captureMarker, $installMarker, $restartMarker)) {
        if (Test-Path -LiteralPath $marker) {
            throw "Attempt marker already exists: $marker"
        }
    }
    $markerText = @(
        "Mode=$Mode"
        "AuthorizationId=$AuthorizationId"
        "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "LauncherCommit=$ExpectedLauncherCommit"
        "ManifestHash=$ExpectedManifestHash"
    ) -join "`r`n"
    Write-ExclusiveTextFile -Path $captureMarker `
        -Text ($markerText + "`r`n")

    $rawLog = Join-Path $evidence 'kernel-capture.log'
    if (Test-Path -LiteralPath $rawLog) {
        throw "Raw capture log already exists: $rawLog"
    }
    $captureArguments = @('--kernel', '--verbose-kernel', '--no-win32',
        '--clock-ms', '--format', 'text', '--duration',
        $CaptureDurationSeconds.ToString([Globalization.CultureInfo]::InvariantCulture),
        '--max-lines', '3000000', '--log', $rawLog, '--no-banner')
    $captureLog = Join-Path $evidence 'capture-child.log'
    $captureLiveOut = Join-Path $evidence 'capture-live.stdout.txt'
    $captureLiveErr = Join-Path $evidence 'capture-live.stderr.txt'
    $captureProcessInfo = Join-Path $evidence 'capture-process.txt'
    $script:CaptureLogPath = $captureLog
    $runner = Join-Path $PSScriptRoot 'native-process.ps1'
    $script:CaptureJob = Start-CaptureJob -Executable $debugView `
        -Arguments $captureArguments -WorkingDirectory (Split-Path $debugView -Parent) `
        -LogPath $captureLog -RunnerPath $runner -LiveStdOutPath $captureLiveOut `
        -LiveStdErrPath $captureLiveErr -ProcessInfoPath $captureProcessInfo

    $captureStartedUtc = [DateTime]::UtcNow
    $deadline = [DateTime]::UtcNow.AddSeconds($CaptureReadinessTimeoutSeconds)
    $ready = $false
    $captureInitialized = $false
    $rawLogFlushed = $false
    $confirmedKernelMessage = $false
    $kernelMessage = $null
    $messageFirstSeenUtc = $null
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($script:CaptureJob.State -ne 'Running') { break }
        $captureInitialized = (
            (Test-Path -LiteralPath $captureLiveOut -PathType Leaf) -and
            (Test-Path -LiteralPath $captureProcessInfo -PathType Leaf) -and
            (Get-Item -LiteralPath $captureProcessInfo).Length -gt 0)
        if ((Test-Path -LiteralPath $rawLog -PathType Leaf) -and
            (Get-Item -LiteralPath $rawLog).Length -gt 0) {
            $rawLogFlushed = $true
        }
        if ($captureInitialized -and (Get-Item -LiteralPath $captureLiveOut).Length -gt 0) {
            $kernelMessage = @(Get-Content -LiteralPath $captureLiveOut |
                ForEach-Object { [string]$_ } |
                Where-Object { Test-ConfirmedKernelCaptureLine $_ } |
                Select-Object -First 1)
            if ($kernelMessage.Count -eq 1) {
                $kernelMessage = [string]$kernelMessage[0]
                $confirmedKernelMessage = $true
                if ($null -eq $messageFirstSeenUtc) {
                    $messageFirstSeenUtc = [DateTime]::UtcNow
                }
                if (([DateTime]::UtcNow - $messageFirstSeenUtc).TotalMilliseconds `
                        -ge $CaptureStabilityMilliseconds) {
                    $ready = $true
                    break
                }
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) {
        throw 'Capture readiness was not established before the deadline.'
    }
    Assert-CaptureJobRunning -Job $script:CaptureJob -Boundary 'the readiness record'
    $expectedRemainingSeconds = $CaptureDurationSeconds -
        [int]([DateTime]::UtcNow - $captureStartedUtc).TotalSeconds
    if (-not $TestMode -and $expectedRemainingSeconds -lt 120) {
        throw 'Capture has less than 120 seconds of expected duration remaining.'
    }
    if ((Get-Item -LiteralPath $captureLiveErr).Length -gt 0) {
        throw 'Capture collector reported stderr before readiness.'
    }
    $readyRecord = [ordered]@{
        ReadyUtc = [DateTime]::UtcNow.ToString('o')
        Kernel = $true
        VerboseKernel = $true
        Win32 = $false
        CollectorProcessHealthy = ($script:CaptureJob.State -eq 'Running')
        CaptureInitialized = $captureInitialized
        RawLogFlushed = $rawLogFlushed
        ConfirmedKernelMessage = $confirmedKernelMessage
        ConfirmedKernelMessageText = $kernelMessage
        MessageFirstSeenUtc = $messageFirstSeenUtc.ToString('o')
        ExpectedRemainingSeconds = $expectedRemainingSeconds
        StabilityMilliseconds = $CaptureStabilityMilliseconds
        ProcessInfo = $captureProcessInfo
        LiveStdOut = $captureLiveOut
        RawLog = $rawLog
        CaptureJobState = $script:CaptureJob.State.ToString()
    } | ConvertTo-Json
    Write-ExclusiveTextFile -Path (Join-Path $evidence 'capture-ready.json') `
        -Text ($readyRecord + [Environment]::NewLine)
    Write-LauncherOutput 'CAPTURE READY'

    if ($Mode -eq 'InstallWithCapture') {
        if ($script:CaptureJob.State -ne 'Running') {
            throw 'Capture ended before the installation gate.'
        }
        $pnp = Get-AbsolutePath $PnPUtilPath 'PnPUtilPath'
        Assert-FileHash -Path $pnp -Expected $ExpectedPnPUtilHash `
            -Label 'PnPUtil executable'
        Write-ExclusiveTextFile -Path $installMarker `
            -Text ($markerText + "`r`nInstallApproved=true`r`n")
        $script:InstallInvocations++
        $installResult = Invoke-LecS65NativeProcess `
            -Name 'Approved one-time driver installation' -FilePath $pnp `
            -Arguments @('/add-driver',
                (Join-Path $package 'package\LecS65AcqDrv.inf'), '/install') `
            -WorkingDirectory (Join-Path $package 'package') `
            -LogPath (Join-Path $evidence 'install-child.log') `
            -TimeoutSeconds 120 -ReturnOnNonZeroExit
        if ($installResult.ExitCode -ne 0) {
            throw "PnPUtil exited with code $($installResult.ExitCode); no retry was attempted."
        }
        Write-LauncherOutput 'INSTALL CHILD PASS'
    }
    elseif ($Mode -eq 'RestartWithCapture') {
        Assert-CaptureJobRunning -Job $script:CaptureJob `
            -Boundary 'the PnP restart gate'
        $pnp = Get-AbsolutePath $PnPUtilPath 'PnPUtilPath'
        Assert-FileHash -Path $pnp -Expected $ExpectedPnPUtilHash `
            -Label 'PnPUtil executable'
        Assert-CaptureJobRunning -Job $script:CaptureJob `
            -Boundary 'the PnP restart marker'
        Write-ExclusiveTextFile -Path $restartMarker `
            -Text ($markerText + "`r`nPnpRestartApproved=true`r`nDeviceInstanceId=$DeviceInstanceId`r`n")
        Assert-CaptureJobRunning -Job $script:CaptureJob `
            -Boundary 'PnP restart dispatch'
        $script:PnpRestartInvocations++
        $restartResult = Invoke-LecS65NativeProcess `
            -Name 'Approved one-time PnP restart' -FilePath $pnp `
            -Arguments @('/restart-device', $DeviceInstanceId) `
            -WorkingDirectory (Split-Path $pnp -Parent) `
            -LogPath (Join-Path $evidence 'pnp-restart-child.log') `
            -TimeoutSeconds 120 -ReturnOnNonZeroExit
        if ($restartResult.ExitCode -ne 0) {
            throw "PnPUtil restart exited with code $($restartResult.ExitCode); no retry was attempted."
        }
        Write-LauncherOutput 'PNP RESTART CHILD PASS'
    }

    $script:CaptureResult = Complete-CaptureJob -Job $script:CaptureJob `
        -TimeoutSeconds ($CaptureDurationSeconds + $CaptureShutdownGraceSeconds)
    Write-LauncherOutput 'CAPTURE CHILD PASS'
    Write-Result 'PASS' 0 "$Mode completed without retry."
    Write-LauncherOutput "DIAGNOSTIC LAUNCHER PASS ($Mode)"
    exit 0
}
catch {
    $message = $_.Exception.Message
    if ($script:EvidenceInitialized) {
        try {
            $detail = $_ | Format-List * -Force | Out-String
            [IO.File]::WriteAllText($script:ExceptionPath, $detail,
                [Text.Encoding]::UTF8)
        } catch { }
        try { Write-Result 'FAIL' 1 $message } catch { }
    }
    Write-LauncherError ("LAUNCHER FAILURE: {0}" -f $message)
    exit 1
}
finally {
    if ($null -ne $script:CaptureJob) {
        try {
            if ($script:CaptureJob.State -eq 'Running') {
                Stop-Job -Job $script:CaptureJob
                Wait-Job -Job $script:CaptureJob -Timeout 10 | Out-Null
            }
        } catch { }
        try { Remove-Job -Job $script:CaptureJob -Force } catch { }
    }
}
