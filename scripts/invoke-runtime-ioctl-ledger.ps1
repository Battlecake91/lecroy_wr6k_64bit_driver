<#
.SYNOPSIS
  Coordinate the WR6k R0/R1 qualification as independently launched stages.
.DESCRIPTION
  Create freezes the exact runtime identity and ordered stage plan. DispatchNext
  launches only the next incomplete stage and returns without waiting for it.
  Collect classifies the durable child evidence. A command with an attempt
  marker but no conclusive result is permanently uncertain and is never
  dispatched again. Status is read-only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Create','DispatchNext','Collect','Status')]
    [string]$Action,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [string]$ExpectedInf = 'oem103.inf',
    [string]$ExpectedVersion = '0.2026.1024.1',
    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$ExpectedSysHash =
        '73B235BD4D19354612752A87BA499C06D15336F63A6B73EDDC0C674C3086D908',
    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$ExpectedLecdiagHash =
        'EF49C33B82237AD27EEBF419CC12B3909777A3F566F1E2E3B393A4E0A17F7229',
    [switch]$OwnerAuthorized,
    [switch]$TestMode,
    [string]$MockStatePath,
    [string]$MockExecutablePath,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')][string]$MockExecutableSha256 = '',
    [string]$MockScenarioPath,
    [ValidateSet('','AfterDispatchMarker','AfterChildStart')]
    [string]$FailureInjection = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$harness = Join-Path $PSScriptRoot 'invoke-runtime-ioctl-qualification.ps1'
$lecdiag = Join-Path $repo 'tools\lecdiag\build\lecdiag.exe'
$ps51 = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$stageNames = @(
    'preflight',
    'r0-1-build','r0-2-stats','r0-3-bars','r0-4-trace','r0-5-noop',
    'r0-6-noop','r0-7-trace-size','r0-8-register-size','r0-9-delay-zero',
    'r1-pci','r1-start-register'
)

function Write-ExclusiveText([string]$Path, [AllowEmptyString()][string]$Text) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = $utf8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

function Write-ExclusiveJson([string]$Path, [object]$Value) {
    Write-ExclusiveText $Path (($Value | ConvertTo-Json -Depth 16) + "`r`n")
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required JSON is missing: $Path"
    }
    Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Get-Hash([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Assert-Host([bool]$RequireElevation) {
    if (-not [Environment]::Is64BitProcess -or
        $PSVersionTable.PSVersion.Major -ne 5 -or
        $PSVersionTable.PSEdition -ne 'Desktop') {
        throw '64-bit Windows PowerShell 5.1 is required.'
    }
    if ($RequireElevation -and -not $TestMode) {
        $principal = New-Object Security.Principal.WindowsPrincipal(
            [Security.Principal.WindowsIdentity]::GetCurrent())
        if (-not $principal.IsInRole(
                [Security.Principal.WindowsBuiltInRole]::Administrator)) {
            throw 'An elevated token is required.'
        }
    }
}

function Get-Plan {
    $plan = Read-Json (Join-Path $EvidenceRoot 'plan.json')
    if ($plan.Schema -ne 1 -or (@($plan.Stages) -join ',') -ne ($stageNames -join ',')) {
        throw 'The runtime ledger plan is malformed or has an unexpected stage set.'
    }
    if ($plan.RepositoryRoot -ne $repo -or $plan.HarnessPath -ne $harness) {
        throw 'The runtime ledger repository identity changed.'
    }
    if ((Get-Hash $harness) -ne $plan.HarnessSha256) {
        throw 'The runtime harness changed after ledger creation.'
    }
    $currentHead = (& git -C $repo rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $currentHead -ne $plan.GitHead) {
        throw 'The repository revision changed after ledger creation.'
    }
    $dirty = @(& git -C $repo status --porcelain)
    if ($dirty.Count -ne 0) { throw 'The repository worktree is dirty.' }
    $expectedExecutable = if ([bool]$plan.TestMode) { $plan.ExecutablePath } else { $lecdiag }
    if ((Get-Hash $expectedExecutable) -ne $plan.ExecutableSha256) {
        throw 'The diagnostic executable changed after ledger creation.'
    }
    $plan
}

function Get-DispatchRecords {
    @(Get-ChildItem -LiteralPath (Join-Path $EvidenceRoot 'dispatches') -Directory `
        -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
        $attempt = Read-Json (Join-Path $_.FullName 'dispatch.json')
        $completionPath = Join-Path $_.FullName 'completion.json'
        [pscustomobject]@{
            Directory=$_.FullName; Attempt=$attempt
            Completion=if (Test-Path -LiteralPath $completionPath) {
                Read-Json $completionPath
            } else { $null }
        }
    })
}

function Get-StageState([string]$Stage, [object[]]$Records) {
    $matching = @($Records | Where-Object { $_.Attempt.Stage -eq $Stage })
    $pass = @($matching | Where-Object {
        $null -ne $_.Completion -and $_.Completion.Classification -eq 'Pass'
    })
    $blocking = @($matching | Where-Object {
        $null -ne $_.Completion -and $_.Completion.Classification -in
            @('FailedConsumed','UncertainConsumed')
    })
    $open = @($matching | Where-Object { $null -eq $_.Completion })
    [pscustomobject]@{
        Passed=($pass.Count -eq 1); Blocking=($blocking.Count -ne 0)
        Open=@($open); Attempts=$matching.Count
    }
}

function Get-NextStage([object[]]$Records) {
    foreach ($stage in $stageNames) {
        $state = Get-StageState $stage $Records
        if ($state.Blocking) { throw "Stage $stage is permanently blocked by consumed failure evidence." }
        if ($state.Open.Count -ne 0) { throw "Stage $stage has an unresolved dispatch; run Collect." }
        if (-not $state.Passed) { return $stage }
    }
    $null
}

function New-ChildScript([object]$Plan, [string]$Stage, [string]$DispatchRoot,
    [string]$ResultRoot) {
    $escape = { param([string]$Value) "'" + $Value.Replace("'", "''") + "'" }
    $args = @()
    if ([bool]$Plan.TestMode) {
        $args += "-Mode OfflineMock"
        $args += "-MockStatePath $(& $escape $Plan.MockStatePath)"
        $args += "-MockExecutablePath $(& $escape $Plan.ExecutablePath)"
        $args += "-MockExecutableSha256 $(& $escape $Plan.ExecutableSha256)"
        if (-not [string]::IsNullOrWhiteSpace([string]$Plan.MockScenarioPath)) {
            $args += "-MockScenarioPath $(& $escape $Plan.MockScenarioPath)"
        }
        if ($Stage -eq 'preflight') { $args += '-PreflightOnly' }
    } else {
        $args += $(if ($Stage -eq 'preflight') { '-Mode Preflight' } else { '-Mode RunAuthorized -OwnerAuthorized' })
    }
    if ($Stage -ne 'preflight') { $args += "-CommandName $(& $escape $Stage)" }
    $args += "-EvidenceRoot $(& $escape $ResultRoot)"
    $args += "-ExpectedInf $(& $escape $Plan.ExpectedInf)"
    $args += "-ExpectedVersion $(& $escape $Plan.ExpectedVersion)"
    $args += "-ExpectedSysHash $(& $escape $Plan.ExpectedSysHash)"
    $args += "-ExpectedLecdiagHash $(& $escape $Plan.ExpectedLecdiagHash)"
    $exitPath = Join-Path $DispatchRoot 'child-exit.json'
    @"
`$ErrorActionPreference = 'Stop'
`$started = [datetime]::UtcNow
`$code = 1
try {
    Import-Module Microsoft.PowerShell.Utility -ErrorAction Stop
    & $(& $escape $harness) $($args -join ' ')
    `$code = `$LASTEXITCODE
} catch {
    `$code = 1
    [IO.File]::WriteAllText($(& $escape (Join-Path $DispatchRoot 'child-exception.txt')), (`$_ | Out-String), (New-Object Text.UTF8Encoding(`$false)))
} finally {
    `$value = [ordered]@{ StartedUtc=`$started.ToString('o'); CompletedUtc=[datetime]::UtcNow.ToString('o'); ExitCode=`$code }
    [IO.File]::WriteAllText($(& $escape $exitPath), ((`$value | ConvertTo-Json) + "`r`n"), (New-Object Text.UTF8Encoding(`$false)))
}
exit `$code
"@
}

function Get-StatusObject([object]$Plan) {
    $records = @(Get-DispatchRecords)
    $stages = @($stageNames | ForEach-Object {
        $state = Get-StageState $_ $records
        [ordered]@{ Name=$_; Passed=$state.Passed; Blocking=$state.Blocking;
            OpenDispatches=$state.Open.Count; Dispatches=$state.Attempts }
    })
    $next = $null
    $blocked = $false
    try { $next = Get-NextStage $records } catch { $blocked=$true }
    [ordered]@{
        Schema=1; CapturedUtc=[datetime]::UtcNow.ToString('o')
        GitHead=$Plan.GitHead; NextStage=$next; Blocked=$blocked
        Complete=(@($stages | Where-Object { -not $_.Passed }).Count -eq 0)
        Stages=$stages
    }
}

if (-not [IO.Path]::IsPathRooted($EvidenceRoot)) {
    throw 'EvidenceRoot must be absolute.'
}

if ($Action -eq 'Create') {
    Assert-Host $false
    if (Test-Path -LiteralPath $EvidenceRoot) {
        throw "EvidenceRoot already exists: $EvidenceRoot"
    }
    if (-not (Test-Path -LiteralPath $harness -PathType Leaf)) { throw 'Runtime harness is missing.' }
    $executable = if ($TestMode) { $MockExecutablePath } else { $lecdiag }
    if ($TestMode) {
        foreach ($value in @($MockStatePath,$MockExecutablePath,$MockExecutableSha256)) {
            if ([string]::IsNullOrWhiteSpace($value)) { throw 'TestMode requires mock state, executable and hash.' }
        }
        if ((Get-Hash $MockExecutablePath) -ne $MockExecutableSha256.ToUpperInvariant()) {
            throw 'Mock executable hash mismatch.'
        }
    }
    $head = (& git -C $repo rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve repository HEAD.' }
    if (@(& git -C $repo status --porcelain).Count -ne 0) { throw 'The repository worktree is dirty.' }
    [void][IO.Directory]::CreateDirectory($EvidenceRoot)
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceRoot 'dispatches'))
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'plan.json') ([ordered]@{
        Schema=1; CreatedUtc=[datetime]::UtcNow.ToString('o'); RepositoryRoot=$repo
        GitHead=$head; HarnessPath=$harness; HarnessSha256=Get-Hash $harness
        ExecutablePath=[IO.Path]::GetFullPath($executable); ExecutableSha256=Get-Hash $executable
        ExpectedInf=$ExpectedInf; ExpectedVersion=$ExpectedVersion
        ExpectedSysHash=$ExpectedSysHash.ToUpperInvariant()
        ExpectedLecdiagHash=$ExpectedLecdiagHash.ToUpperInvariant()
        TestMode=[bool]$TestMode; MockStatePath=$MockStatePath
        MockScenarioPath=$MockScenarioPath; Stages=$stageNames
    })
    Write-Output 'Runtime ledger created. Dispatch preflight separately.'
    exit 0
}

Assert-Host ($Action -eq 'DispatchNext')
$plan = Get-Plan

if ($Action -eq 'Status') {
    Get-StatusObject $plan | ConvertTo-Json -Depth 8
    exit 0
}

if ($Action -eq 'DispatchNext') {
    $lockPath = Join-Path $EvidenceRoot 'dispatch.lock'
    $lock = $null
    try {
        $lock = [IO.File]::Open($lockPath, [IO.FileMode]::CreateNew,
            [IO.FileAccess]::Write, [IO.FileShare]::None)
        $records = @(Get-DispatchRecords)
        $stage = Get-NextStage $records
        if ($null -eq $stage) { throw 'All runtime stages are already complete.' }
        if ($stage -ne 'preflight' -and -not $OwnerAuthorized) {
            throw "Dispatching $stage requires OwnerAuthorized."
        }
        $index = [array]::IndexOf($stageNames, $stage)
        $invocation = [guid]::NewGuid().ToString('N')
        $dispatchRoot = Join-Path (Join-Path $EvidenceRoot 'dispatches') `
            ('{0:D2}-{1}-{2}' -f $index,$stage,$invocation)
        [void][IO.Directory]::CreateDirectory($dispatchRoot)
        $resultRoot = Join-Path $dispatchRoot 'result'
        Write-ExclusiveJson (Join-Path $dispatchRoot 'dispatch.json') ([ordered]@{
            Schema=1; Stage=$stage; Index=$index; InvocationId=$invocation
            DispatchMarkedUtc=[datetime]::UtcNow.ToString('o'); OwnerAuthorized=[bool]$OwnerAuthorized
        })
        if ($FailureInjection -eq 'AfterDispatchMarker') { exit 97 }
        $childScript = Join-Path $dispatchRoot 'child.ps1'
        Write-ExclusiveText $childScript (New-ChildScript $plan $stage $dispatchRoot $resultRoot)
        $process = Start-Process -FilePath $ps51 -ArgumentList @('-NoLogo','-NoProfile',
            '-NonInteractive','-ExecutionPolicy','Bypass','-File',$childScript) `
            -WindowStyle Hidden -PassThru
        Write-ExclusiveJson (Join-Path $dispatchRoot 'process.json') ([ordered]@{
            ProcessId=$process.Id; StartedUtc=$process.StartTime.ToUniversalTime().ToString('o')
            Executable=$ps51; ChildScript=$childScript
        })
        if ($FailureInjection -eq 'AfterChildStart') { exit 98 }
        Write-Output "Dispatched $stage as PID $($process.Id); run Collect after it exits."
    } finally {
        if ($null -ne $lock) { $lock.Dispose() }
        if (Test-Path -LiteralPath $lockPath) { Remove-Item -LiteralPath $lockPath -Force }
    }
    exit 0
}

if ($Action -eq 'Collect') {
    $records = @(Get-DispatchRecords)
    $open = @($records | Where-Object { $null -eq $_.Completion })
    if ($open.Count -ne 1) { throw "Collect requires exactly one unresolved dispatch; found $($open.Count)." }
    $record = $open[0]
    $processPath = Join-Path $record.Directory 'process.json'
    if (Test-Path -LiteralPath $processPath) {
        $processInfo = Read-Json $processPath
        $running = Get-Process -Id ([int]$processInfo.ProcessId) -ErrorAction SilentlyContinue
        if ($null -ne $running) {
            $actualStart = $running.StartTime.ToUniversalTime().ToString('o')
            if ($actualStart -eq $processInfo.StartedUtc) {
                Write-Output "Stage $($record.Attempt.Stage) is still running; no state changed."
                exit 2
            }
        }
    }
    $resultRoot = Join-Path $record.Directory 'result'
    $outcomePath = Join-Path $resultRoot 'outcome.json'
    $internalAttempts = @(Get-ChildItem -LiteralPath $resultRoot -Filter '*.attempted' `
        -ErrorAction SilentlyContinue)
    $internalResults = @(Get-ChildItem -LiteralPath $resultRoot -Filter '*.result.json' `
        -ErrorAction SilentlyContinue)
    $classification = $null
    $message = $null
    if (Test-Path -LiteralPath $outcomePath) {
        $outcome = Read-Json $outcomePath
        if ($outcome.Outcome -eq 'PASS' -and
            (($record.Attempt.Stage -eq 'preflight' -and $internalAttempts.Count -eq 0) -or
             ($record.Attempt.Stage -ne 'preflight' -and $internalAttempts.Count -eq 1 -and
              $internalResults.Count -eq 1))) {
            $classification = 'Pass'; $message = 'Durable harness evidence is complete.'
        } elseif ($internalAttempts.Count -eq 0) {
            $classification = 'NoHardwareDispatch'; $message = 'Harness failed before a command attempt marker.'
        } else {
            $classification = 'FailedConsumed'; $message = 'The dispatched command produced a conclusive failure.'
        }
    } elseif ($internalAttempts.Count -eq 0) {
        $classification = 'NoHardwareDispatch'; $message = 'Child ended before a hardware attempt marker.'
    } else {
        $classification = 'UncertainConsumed'; $message = 'A command attempt marker exists without conclusive completion.'
    }
    Write-ExclusiveJson (Join-Path $record.Directory 'completion.json') ([ordered]@{
        Schema=1; Stage=$record.Attempt.Stage; InvocationId=$record.Attempt.InvocationId
        CollectedUtc=[datetime]::UtcNow.ToString('o'); Classification=$classification
        Message=$message; HardwareAttemptMarkers=$internalAttempts.Count
        ResultRecords=$internalResults.Count
        OutcomeSha256=if (Test-Path -LiteralPath $outcomePath) { Get-Hash $outcomePath } else { $null }
    })
    $status = Get-StatusObject $plan
    $snapshot = Join-Path $EvidenceRoot ('status-{0}-{1}.json' -f
        ([datetime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')),[guid]::NewGuid().ToString('N'))
    Write-ExclusiveJson $snapshot $status
    $status | ConvertTo-Json -Depth 8
    if ($classification -ne 'Pass' -and $classification -ne 'NoHardwareDispatch') { exit 1 }
    exit 0
}

throw "Unhandled action: $Action"
