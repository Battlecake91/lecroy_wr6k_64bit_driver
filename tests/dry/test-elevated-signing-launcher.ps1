[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EvidenceLog,
    [Parameter(Mandatory=$true)][string]$ResultPath,
    [Parameter(Mandatory=$true)][string]$PowerShell7Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$principal = [Security.Principal.WindowsPrincipal](
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This suite must run with an elevated token.'
}
if ($PSVersionTable.PSVersion.Major -ne 5 -or
    $PSVersionTable.PSVersion.Minor -ne 1) {
    throw 'This suite requires Windows PowerShell 5.1.'
}
foreach ($path in @($EvidenceLog, $ResultPath)) {
    if (Test-Path -LiteralPath $path) { throw "Evidence path exists: $path" }
}
if (-not (Test-Path -LiteralPath $PowerShell7Path -PathType Leaf)) {
    throw "PowerShell 7 test host is missing: $PowerShell7Path"
}

Start-Transcript -LiteralPath $EvidenceLog | Out-Null
$script:Checks = 0
$script:Passed = 0
$script:Failed = 0
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$launcher = Join-Path $repo 'scripts\invoke-elevated-signing-workflow.ps1'
$mockSource = Join-Path $repo 'tests\dry\fixtures\mock-signing-workflow.ps1'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    'lecroy-elevated-launcher-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($tempRoot) | Out-Null

function Add-Result {
    param([string]$Name, [bool]$Passed)
    $script:Checks++
    if ($Passed) {
        Write-Host "[PASS] $Name" -ForegroundColor Green
        $script:Passed++
    }
    else {
        Write-Host "[FAIL] $Name" -ForegroundColor Red
        $script:Failed++
    }
}

function New-Fixture {
    param([string]$Name)
    $fixture = Join-Path $tempRoot $Name
    $scripts = Join-Path $fixture 'scripts'
    [System.IO.Directory]::CreateDirectory($scripts) | Out-Null
    Copy-Item -LiteralPath $mockSource `
        -Destination (Join-Path $scripts 'build-driver-package.ps1')
    & git -C $fixture init --quiet
    & git -C $fixture config user.email 'offline-test@example.invalid'
    & git -C $fixture config user.name 'Offline Test'
    & git -C $fixture add scripts
    & git -C $fixture commit --quiet -m fixture
    if ($LASTEXITCODE -ne 0) { throw "Unable to create fixture: $Name" }
    return $fixture
}

function Invoke-LauncherCase {
    param(
        [string]$Name,
        [string]$Scenario = 'Success',
        [ValidateSet('Sign', 'UnsignedQualification')][string]$Mode = 'Sign',
        [scriptblock]$Mutate,
        [switch]$OmitThumbprint,
        [string]$ExpectedCommitOverride,
        [string]$WorkflowOverride,
        [string]$LogOverride,
        [string]$HostOverride,
        [switch]$LogInsideRepository,
        [int]$ChildTimeoutSeconds = 30
    )

    $fixture = New-Fixture -Name $Name
    $head = (& git -C $fixture rev-parse HEAD).Trim()
    $workflow = Join-Path $fixture 'scripts\build-driver-package.ps1'
    $output = Join-Path $fixture 'candidate output'
    $log = Join-Path $tempRoot ("$Name-launcher.log")
    $marker = Join-Path $tempRoot ("$Name-attempt.marker")
    if ($LogInsideRepository) {
        $log = Join-Path $fixture 'launcher.log'
    }
    $counter = Join-Path $fixture 'invocations.txt'
    $descendantPid = Join-Path $tempRoot ("$Name-descendant.pid")
    if ($null -ne $Mutate) { & $Mutate $fixture $output $log $marker }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedCommitOverride)) {
        $head = $ExpectedCommitOverride
    }
    if (-not [string]::IsNullOrWhiteSpace($WorkflowOverride)) {
        $workflow = $WorkflowOverride
    }
    if (-not [string]::IsNullOrWhiteSpace($LogOverride)) {
        $log = $LogOverride
    }

    $oldScenario = $env:LECS65_MOCK_SCENARIO
    $oldCounter = $env:LECS65_MOCK_COUNTER
    $oldDescendantPid = $env:LECS65_MOCK_DESCENDANT_PID
    $env:LECS65_MOCK_SCENARIO = $Scenario
    $env:LECS65_MOCK_COUNTER = $counter
    $env:LECS65_MOCK_DESCENDANT_PID = $descendantPid
    try {
        $arguments = @(
            '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
            '-File', $launcher,
            '-RepositoryRoot', $fixture,
            '-WorkflowPath', $workflow,
            '-ExpectedWorkflowCommit', $head,
            '-ExpectedDriverTreeCommit', ('C' * 40),
            '-RuntimeConfiguration', 'Release',
            '-DriverVer', '10/17/2026,0.2026.1017.1',
            '-Mode', $Mode,
            '-OutputRoot', $output,
            '-RunLog', $log,
            '-AttemptMarker', $marker,
            '-ChildTimeoutSeconds', $ChildTimeoutSeconds
        )
        if ($Mode -eq 'Sign' -and -not $OmitThumbprint) {
            $arguments += @('-CertificateThumbprint', ('A' * 40))
        }
        $launchHost = if ([string]::IsNullOrWhiteSpace($HostOverride)) {
            Join-Path $PSHOME 'powershell.exe'
        }
        else { $HostOverride }
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $launchHost @arguments | Out-Host
            $exitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
    }
    finally {
        $env:LECS65_MOCK_SCENARIO = $oldScenario
        $env:LECS65_MOCK_COUNTER = $oldCounter
        $env:LECS65_MOCK_DESCENDANT_PID = $oldDescendantPid
    }
    $invocations = if (Test-Path -LiteralPath $counter) {
        @(Get-Content -LiteralPath $counter).Count
    }
    else { 0 }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Fixture = $fixture
        Output = $output
        Log = $log
        Marker = $marker
        Invocations = $invocations
        DescendantPid = $descendantPid
    }
}

try {
    $success = Invoke-LauncherCase -Name 'success'
    $successLog = Get-Content -LiteralPath $success.Log -Raw
    Add-Result 'complete elevated mock-signing path succeeds' (
        $success.ExitCode -eq 0 -and $success.Invocations -eq 1)
    Add-Result 'startup, working directory and arguments are forwarded' (
        $successLog -match 'MOCK WORKFLOW START' -and
        $successLog.Contains("MOCK WORKING DIRECTORY: $($success.Fixture)") -and
        $successLog -match 'configuration=Release' -and
        $successLog -match 'driverver=10/17/2026,0.2026.1017.1')
    Add-Result 'mock signing and package tools execute in order' (
        $successLog -match '(?s)InfVerif before catalog.*SignTool sign SYS.*Inf2Cat catalog generation.*SignTool sign CAT.*InfVerif final')
    Add-Result 'mock SYS CAT and membership verification is preserved' (
        $successLog -match 'Authenticode verify SYS: Valid' -and
        $successLog -match 'Authenticode verify CAT: Valid' -and
        $successLog -match 'catalog membership verify: Valid')
    Add-Result 'complete manifest and hash are accepted' (
        (Test-Path -LiteralPath (Join-Path $success.Output 'evidence\manifest.json')) -and
        $successLog -match 'MOCK WORKFLOW PASS')
    Add-Result 'attempt marker and combined child log are preserved' (
        (Test-Path -LiteralPath $success.Marker) -and
        (Test-Path -LiteralPath $success.Log) -and
        (Test-Path -LiteralPath ($success.Log + '.stdout.txt')) -and
        (Test-Path -LiteralPath ($success.Log + '.stderr.txt')))

    $unsigned = Invoke-LauncherCase -Name 'unsigned-success' `
        -Mode UnsignedQualification
    $unsignedLog = Get-Content -LiteralPath $unsigned.Log -Raw
    $unsignedManifest = Get-Content -LiteralPath `
        (Join-Path $unsigned.Output 'evidence\manifest.json') -Raw | ConvertFrom-Json
    Add-Result 'complete elevated unsigned path succeeds without certificate input' (
        $unsigned.ExitCode -eq 0 -and $unsigned.Invocations -eq 1 -and
        $unsignedManifest.PackageState -eq 'UnsignedValidationOnly' -and
        $unsignedManifest.PackageIdentity.Signed -eq $false -and
        $null -eq $unsignedManifest.PackageIdentity.Certificate)
    Add-Result 'unsigned path executes package tools without mock SignTool' (
        $unsignedLog -match '(?s)InfVerif before catalog.*Inf2Cat catalog generation.*InfVerif final' -and
        $unsignedLog -notmatch 'SignTool sign')

    $stderrSuccess = Invoke-LauncherCase -Name 'stderr-success' `
        -Mode UnsignedQualification -Scenario NativeToolStderrZero
    $stderrSuccessLog = Get-Content -LiteralPath $stderrSuccess.Log -Raw
    Add-Result 'diagnostic stderr with exit code zero remains successful' (
        $stderrSuccess.ExitCode -eq 0 -and
        $stderrSuccessLog -match 'diagnostic stderr with exit code zero')

    $invalidArgs = Invoke-LauncherCase -Name 'invalid-arguments' -OmitThumbprint
    Add-Result 'invalid PowerShell arguments stop before invocation' (
        $invalidArgs.ExitCode -ne 0 -and $invalidArgs.Invocations -eq 0)

    $wrongHost = Invoke-LauncherCase -Name 'wrong-host' `
        -HostOverride $PowerShell7Path
    Add-Result 'PowerShell 7 is rejected without a silent host switch' (
        $wrongHost.ExitCode -ne 0 -and $wrongHost.Invocations -eq 0)

    $missingWorkflowFixture = Join-Path $tempRoot 'missing-workflow-placeholder.ps1'
    $missingWorkflow = Invoke-LauncherCase -Name 'missing-workflow' `
        -WorkflowOverride $missingWorkflowFixture
    Add-Result 'missing workflow stops before invocation' (
        $missingWorkflow.ExitCode -ne 0 -and $missingWorkflow.Invocations -eq 0)

    $dirty = Invoke-LauncherCase -Name 'dirty-worktree' -Mutate {
        param($fixture, $output)
        Set-Content -LiteralPath (Join-Path $fixture 'dirty.txt') -Value dirty
    }
    Add-Result 'dirty Git worktree stops before invocation' (
        $dirty.ExitCode -ne 0 -and $dirty.Invocations -eq 0)

    $wrongCommit = Invoke-LauncherCase -Name 'wrong-commit' `
        -ExpectedCommitOverride ('0' * 40)
    Add-Result 'incorrect workflow commit stops before invocation' (
        $wrongCommit.ExitCode -ne 0 -and $wrongCommit.Invocations -eq 0)

    $existingOutput = Invoke-LauncherCase -Name 'existing-output' -Mutate {
        param($fixture, $output)
        [System.IO.Directory]::CreateDirectory($output) | Out-Null
    }
    Add-Result 'existing output directory stops before invocation' (
        $existingOutput.ExitCode -ne 0 -and $existingOutput.Invocations -eq 0)

    $existingMarker = Invoke-LauncherCase -Name 'existing-marker' -Mutate {
        param($fixture, $output, $log, $marker)
        Set-Content -LiteralPath $marker -Value existing
    }
    Add-Result 'existing attempt marker prevents duplicate invocation' (
        $existingMarker.ExitCode -ne 0 -and $existingMarker.Invocations -eq 0)

    $existingLog = Invoke-LauncherCase -Name 'existing-log' -Mutate {
        param($fixture, $output, $log, $marker)
        Set-Content -LiteralPath $log -Value existing
    }
    Add-Result 'existing log is never overwritten' (
        $existingLog.ExitCode -ne 0 -and $existingLog.Invocations -eq 0 -and
        (Get-Content -LiteralPath $existingLog.Log -Raw).Trim() -eq 'existing')

    $internalLog = Invoke-LauncherCase -Name 'internal-evidence' `
        -LogInsideRepository
    Add-Result 'attempt evidence inside the repository is rejected' (
        $internalLog.ExitCode -ne 0 -and $internalLog.Invocations -eq 0)

    foreach ($failure in @(
        @{ Name = 'native-tool-failure'; Scenario = 'NativeToolFailure'; Code = 77; Text = 'native tool nonzero exit' }
        @{ Name = 'symchk-failure'; Scenario = 'SymChkFailure'; Code = 78; Text = 'SymChk failure' }
        @{ Name = 'missing-pdb'; Scenario = 'MissingPdb'; Code = 79; Text = 'missing PDB' }
        @{ Name = 'signing-tool-failure'; Scenario = 'SigningToolFailure'; Code = 74; Text = 'SignTool signing failure' }
        @{ Name = 'missing-certificate'; Scenario = 'MissingCertificateMetadata'; Code = 72; Text = 'certificate metadata is missing' }
        @{ Name = 'infverif-failure'; Scenario = 'InfVerifFailure'; Code = 73; Text = 'InfVerif failure' }
        @{ Name = 'inf2cat-failure'; Scenario = 'Inf2CatFailure'; Code = 75; Text = 'Inf2Cat failure' }
        @{ Name = 'verification-failure'; Scenario = 'VerificationFailure'; Code = 76; Text = 'signature verification failure' }
        @{ Name = 'unexpected-termination'; Scenario = 'UnexpectedTermination'; Code = 71; Text = 'unexpected child-process termination' }
    )) {
        $case = Invoke-LauncherCase -Name $failure.Name -Scenario $failure.Scenario
        $caseLog = Get-Content -LiteralPath $case.Log -Raw
        Add-Result "$($failure.Name) propagates its exit code and preserves logs" (
            $case.ExitCode -eq $failure.Code -and $case.Invocations -eq 1 -and
            $caseLog -match $failure.Text)
    }

    $malformed = Invoke-LauncherCase -Name 'malformed-manifest' `
        -Scenario 'MalformedManifest'
    Add-Result 'malformed manifest fails after one child invocation' (
        $malformed.ExitCode -ne 0 -and $malformed.Invocations -eq 1 -and
        (Test-Path -LiteralPath $malformed.Log))

    $mismatch = Invoke-LauncherCase -Name 'manifest-mismatch' `
        -Mode UnsignedQualification -Scenario 'ManifestMismatch'
    Add-Result 'manifest hash mismatch fails after one child invocation' (
        $mismatch.ExitCode -ne 0 -and $mismatch.Invocations -eq 1 -and
        (Get-Content -LiteralPath $mismatch.Log -Raw) -match 'LAUNCHER FAILURE')

    $earlyExit = Invoke-LauncherCase -Name 'child-exits-before-output' `
        -Mode UnsignedQualification -Scenario ChildExitBeforeLogs
    Add-Result 'child exit before workflow output terminates and preserves empty logs' (
        $earlyExit.ExitCode -eq 80 -and $earlyExit.Invocations -eq 1 -and
        (Test-Path -LiteralPath $earlyExit.Log) -and
        (Test-Path -LiteralPath ($earlyExit.Log + '.stdout.txt')) -and
        (Test-Path -LiteralPath ($earlyExit.Log + '.stderr.txt')))

    $timeout = Invoke-LauncherCase -Name 'child-timeout' `
        -Mode UnsignedQualification -Scenario ChildTimeout `
        -ChildTimeoutSeconds 1
    Add-Result 'child timeout terminates without retry and preserves evidence' (
        $timeout.ExitCode -ne 0 -and $timeout.Invocations -eq 1 -and
        (Get-Content -LiteralPath $timeout.Log -Raw) -match 'PROCESS TIMEOUT')

    $descendant = Invoke-LauncherCase -Name 'descendant-cleanup' `
        -Mode UnsignedQualification -Scenario DescendantAlive
    $descendantId = if (Test-Path -LiteralPath $descendant.DescendantPid) {
        [int](Get-Content -LiteralPath $descendant.DescendantPid -Raw)
    } else { 0 }
    Start-Sleep -Milliseconds 250
    Add-Result 'descendant is killed with only the assigned child job' (
        $descendant.ExitCode -eq 81 -and $descendant.Invocations -eq 1 -and
        $descendantId -gt 0 -and
        $null -eq (Get-Process -Id $descendantId -ErrorAction SilentlyContinue))

    $logFailure = Invoke-LauncherCase -Name 'log-write-failure-case' `
        -LogOverride (Join-Path $tempRoot 'invalid:name.log')
    Add-Result 'log-file write failure stops before child invocation' (
        $logFailure.ExitCode -ne 0 -and $logFailure.Invocations -eq 0)

    Add-Result 'no test case automatically retried' (
        @(Get-ChildItem -LiteralPath $tempRoot -Filter 'invocations.txt' -Recurse |
            Where-Object { @(Get-Content -LiteralPath $_.FullName).Count -gt 1 }).Count -eq 0)
}
finally {
    [ordered]@{
        Host = "Windows PowerShell $($PSVersionTable.PSVersion)"
        Elevated = $true
        Passed = $script:Passed
        Total = $script:Checks
        Failed = $script:Failed
    } | ConvertTo-Json | Set-Content -LiteralPath $ResultPath -Encoding UTF8
    try { Stop-Transcript | Out-Null } catch { }
    $resolvedTemp = [System.IO.Path]::GetFullPath($tempRoot)
    $systemTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    if ($resolvedTemp.StartsWith($systemTemp,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}

Write-Host ("ELEVATED SIGNING LAUNCHER TESTS: {0}/{1} passed; {2} failed." -f
    $script:Passed, $script:Checks, $script:Failed)
if ($script:Failed -ne 0) { exit 1 }
