[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EvidenceLog,
    [Parameter(Mandatory=$true)][string]$ResultPath,
    [switch]$AllowNonElevatedTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$principal = [Security.Principal.WindowsPrincipal](
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator) -and
    -not $AllowNonElevatedTestMode) {
    throw 'This suite requires an elevated token.'
}
if ($PSVersionTable.PSVersion.Major -ne 5 -or
    $PSVersionTable.PSVersion.Minor -ne 1 -or
    -not [Environment]::Is64BitProcess) {
    throw 'This suite requires 64-bit Windows PowerShell 5.1.'
}
foreach ($path in @($EvidenceLog, $ResultPath)) {
    if (Test-Path -LiteralPath $path) { throw "Evidence path exists: $path" }
}

Start-Transcript -LiteralPath $EvidenceLog | Out-Null
$script:Passed = 0
$script:Failed = 0
$script:Checks = 0
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'scripts\native-process.ps1')
$launcherSource = Join-Path $repo 'scripts\invoke-diagnostic-capture-install.ps1'
$runnerSource = Join-Path $repo 'scripts\native-process.ps1'
$noLaunchRunnerSource = Join-Path $repo 'scripts\invoke-no-launch-preparation.ps1'
$xstreamRunnerSource = Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1'
$lecdiagMockTest = Join-Path $PSScriptRoot 'test-lecdiag-command-chain.ps1'
$mockSource = Join-Path $PSScriptRoot 'fixtures\mock-diagnostic-native.cs'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('lecroy-diagnostic-launcher-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($tempRoot) | Out-Null

$tokens = $null
$parseErrors = $null
$launcherAst = [Management.Automation.Language.Parser]::ParseFile(
    $launcherSource, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    throw "Launcher parser errors: $($parseErrors.Message -join '; ')"
}
foreach ($definition in @($launcherAst.FindAll({
    param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -in @('Get-AbsolutePath', 'Resolve-PnPUtilPath')
}, $true) | Sort-Object { $_.Extent.StartOffset })) {
    . ([scriptblock]::Create($definition.Extent.Text))
}

function Add-Result {
    param([string]$Name, [bool]$Passed)
    $script:Checks++
    if ($Passed) {
        $script:Passed++
        Write-Host "[PASS] $Name" -ForegroundColor Green
    }
    else {
        $script:Failed++
        Write-Host "[FAIL] $Name" -ForegroundColor Red
    }
}

function New-TestContext {
    $fixture = Join-Path $tempRoot 'repo'
    $scripts = Join-Path $fixture 'scripts'
    [IO.Directory]::CreateDirectory($scripts) | Out-Null
    Copy-Item -LiteralPath $launcherSource -Destination $scripts
    Copy-Item -LiteralPath $runnerSource -Destination $scripts
    Copy-Item -LiteralPath $noLaunchRunnerSource -Destination $scripts
    Copy-Item -LiteralPath $xstreamRunnerSource -Destination $scripts
    & git -C $fixture init --quiet
    & git -C $fixture config user.email 'offline-test@example.invalid'
    & git -C $fixture config user.name 'Offline Test'
    & git -C $fixture add scripts
    & git -C $fixture commit --quiet -m fixture
    if ($LASTEXITCODE -ne 0) { throw 'Unable to create launcher fixture.' }
    $head = (& git -C $fixture rev-parse HEAD).Trim()

    $package = Join-Path $tempRoot 'package'
    [IO.Directory]::CreateDirectory((Join-Path $package 'package')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $package 'evidence')) | Out-Null
    $artifacts = @(
        @{ Name='LecS65AcqDrv.inf'; RelativePath='package\LecS65AcqDrv.inf'; Text='inf' }
        @{ Name='LecS65AcqDrv.cat'; RelativePath='package\LecS65AcqDrv.cat'; Text='cat' }
        @{ Name='LecS65AcqDrv.sys'; RelativePath='package\LecS65AcqDrv.sys'; Text='sys' }
        @{ Name='LecS65AcqDrv.pdb'; RelativePath='evidence\LecS65AcqDrv.pdb'; Text='pdb' }
    )
    foreach ($item in $artifacts) {
        Set-Content -LiteralPath (Join-Path $package $item.RelativePath) `
            -Value $item.Text -Encoding ASCII
        $artifactPath = Join-Path $package $item.RelativePath
        $item.Hash = (Get-FileHash -Algorithm SHA256 `
            -LiteralPath $artifactPath).Hash
    }
    $manifest = [ordered]@{
        Schema=2; PackageState='SignedCandidate'
        Source=[ordered]@{WorkflowCommit=('1'*40);ReviewedDriverTreeCommit=('2'*40);DriverTree=('3'*40);IncludeTree=('4'*40)}
        Build=[ordered]@{RuntimeConfiguration='Release';PnpStatusDiagnostics=$true;DiagnosticMarker='PNP_STATUS:';WdmV3MamDma=$true;FirstMamExperimentOnly=$false;MamLaunchLimit=1}
        PackageIdentity=[ordered]@{DriverVer='10/18/2026,0.2026.1018.1';Signed=$true;Certificate=[ordered]@{Thumbprint=('5'*40)};SysSignatureStatus='Valid';CatSignatureStatus='Valid'}
        Artifacts=@($artifacts | ForEach-Object {[ordered]@{Name=$_.Name;RelativePath=$_.RelativePath;SHA256=$_.Hash}})
    }
    $manifestPath = Join-Path $package 'evidence\manifest.json'
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    $mockExe = Join-Path $tempRoot 'mock-diagnostic-native.exe'
    Add-Type -TypeDefinition (Get-Content -Raw -LiteralPath $mockSource) `
        -OutputAssembly $mockExe -OutputType ConsoleApplication
    $mockHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $mockExe).Hash
    $noLaunchBuild = Join-Path $tempRoot 'no-launch-mock-build'
    & $lecdiagMockTest -EvidenceRoot $noLaunchBuild | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'No-launch mock executable qualification failed.'
    }
    $noLaunchExe = Join-Path $noLaunchBuild 'lecdiag-mock.exe'
    $noLaunchHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $noLaunchExe).Hash
    $noLaunchRunner = Join-Path $scripts 'invoke-no-launch-preparation.ps1'
    $noLaunchRunnerHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $noLaunchRunner).Hash
    $xstreamRunner = Join-Path $scripts 'invoke-xstream-single-waveform.ps1'
    $xstreamRunnerHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $xstreamRunner).Hash
    $statePath = Join-Path $tempRoot 'state.json'
    [ordered]@{InstanceId='PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0';Inf='oem17.inf';Version='0.2026.1017.1';ProblemCode=0;DeviceStatus='OK';ServiceName='LecS65AcqDrv';ServiceStatus='Running';LoadedSysHash=('6'*64);SysHash=('6'*64);Driver=[ordered]@{Inf='oem17.inf';Version='0.2026.1017.1';ProblemCode=0;DeviceStatus='OK';ServiceStatus='Running';SysHash=('6'*64)};Disks=@([ordered]@{Model='HFM256GDHTNG-8510B';Serial='ACE4_2E00_9590_A2AA.';Size=256052966400;Status='OK'});AcquisitionClients=@();MockWaveformObserved=$true;MockReportedSamples=16;MockReturnedValues=16} |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
    return [pscustomobject]@{
        Repository=$fixture; Launcher=(Join-Path $scripts 'invoke-diagnostic-capture-install.ps1')
        Head=$head; Package=$package; ManifestPath=$manifestPath
        Artifacts=$artifacts; MockExe=$mockExe; MockHash=$mockHash; State=$statePath
        NoLaunchExe=$noLaunchExe;NoLaunchHash=$noLaunchHash
        NoLaunchRunner=$noLaunchRunner;NoLaunchRunnerHash=$noLaunchRunnerHash
        XStreamRunner=$xstreamRunner;XStreamRunnerHash=$xstreamRunnerHash
    }
}

$context = New-TestContext
$script:CaseNumber = 0
function Invoke-Case {
    param(
        [string]$Name,
        [ValidateSet('Preflight','CaptureQualification','InstallWithCapture',
            'RestartWithCapture','NoLaunchWithCapture','XStreamWithCapture')]
        [string]$Mode='Preflight',
        [string]$Scenario='Success',
        [string]$FailureInjection='',
        [string]$ExpectedLauncherCommit,
        [string]$ExpectedWorkflowCommit=('1'*40),
        [string]$InfHash,
        [string]$DebugViewHash,
        [switch]$ExistingCaptureMarker,
        [switch]$ExistingCollector,
        [switch]$EvidencePathIsFile,
        [AllowEmptyString()][string]$PnPUtilPath,
        [switch]$OmitPnPUtilPath,
        [switch]$OmitXStreamAuthorization,
        [switch]$OmitPhysicalDiskConfirmation,
        [int]$ReadinessTimeout=8,
        [int]$Duration=2
    )
    $script:CaseNumber++
    $caseRoot = Join-Path $tempRoot ("case-{0:D2}-{1}" -f $script:CaseNumber,$Name)
    if ($EvidencePathIsFile) {
        Set-Content -LiteralPath $caseRoot -Value 'not a directory'
    }
    else {
        [IO.Directory]::CreateDirectory($caseRoot) | Out-Null
        if ($ExistingCaptureMarker) {
            Set-Content -LiteralPath (Join-Path $caseRoot 'capture-attempt.marker') -Value existing
        }
    }
    $counter = Join-Path $tempRoot ("$Name-counter.txt")
    $oldScenario = $env:LECS65_DIAGNOSTIC_MOCK_SCENARIO
    $oldCounter = $env:LECS65_DIAGNOSTIC_MOCK_COUNTER
    $env:LECS65_DIAGNOSTIC_MOCK_SCENARIO = $Scenario
    $env:LECS65_DIAGNOSTIC_MOCK_COUNTER = $counter
    try {
        $artifact = @{}; foreach($item in $context.Artifacts){$artifact[$item.Name]=$item.Hash}
        $arguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$context.Launcher,
            '-Mode',$Mode,'-RepositoryRoot',$context.Repository,
            '-ExpectedLauncherCommit',$(if($ExpectedLauncherCommit){$ExpectedLauncherCommit}else{$context.Head}),
            '-PackageRoot',$context.Package,
            '-ExpectedManifestHash',(Get-FileHash -Algorithm SHA256 -LiteralPath $context.ManifestPath).Hash,
            '-ExpectedPackageWorkflowCommit',$ExpectedWorkflowCommit,
            '-ExpectedDriverSourceCommit',('2'*40),'-ExpectedDriverTree',('3'*40),'-ExpectedIncludeTree',('4'*40),
            '-ExpectedDriverVer','10/18/2026,0.2026.1018.1','-ExpectedCertificateThumbprint',('5'*40),
            '-ExpectedInfHash',$(if($InfHash){$InfHash}else{$artifact['LecS65AcqDrv.inf']}),
            '-ExpectedCatHash',$artifact['LecS65AcqDrv.cat'],'-ExpectedSysHash',$artifact['LecS65AcqDrv.sys'],
            '-ExpectedPdbHash',$artifact['LecS65AcqDrv.pdb'],'-DebugViewPath',$context.MockExe,
            '-ExpectedDebugViewHash',$(if($DebugViewHash){$DebugViewHash}else{$context.MockHash}),
            '-EvidenceRoot',$caseRoot,'-ExpectedCurrentSysHash',('6'*64),
            '-TestMode','-AllowNonElevatedTestMode','-MockStatePath',$context.State,
            '-CaptureReadinessTimeoutSeconds',$ReadinessTimeout,'-CaptureDurationSeconds',$Duration,
            '-CaptureShutdownGraceSeconds','2','-CaptureStabilityMilliseconds','250')
        if ($ExistingCollector) { $arguments += '-MockExistingCollector' }
        if ($FailureInjection) { $arguments += @('-FailureInjection',$FailureInjection) }
        if ($Mode -ne 'Preflight') {
            $arguments += @('-AuthorizationId',"mock-$Name",'-AuthorizeKernelCapture')
        }
        if ($Mode -eq 'InstallWithCapture') {
            $arguments += @('-AuthorizeInstallation',
                '-ExpectedPnPUtilHash',$context.MockHash)
            if (-not $OmitPnPUtilPath) {
                $resolvedPnpPath = if ($PSBoundParameters.ContainsKey('PnPUtilPath')) {
                    $PnPUtilPath
                } else {
                    $context.MockExe
                }
                $arguments += @('-PnPUtilPath',$resolvedPnpPath)
            }
        }
        elseif ($Mode -eq 'RestartWithCapture') {
            $arguments += @('-AuthorizePnpRestart','-PnPUtilPath',$context.MockExe,
                '-ExpectedPnPUtilHash',$context.MockHash)
        }
        elseif ($Mode -eq 'NoLaunchWithCapture') {
            $arguments += @('-AuthorizeNoLaunch',
                '-NoLaunchRunnerPath',$context.NoLaunchRunner,
                '-ExpectedNoLaunchRunnerHash',$context.NoLaunchRunnerHash,
                '-NoLaunchExecutablePath',$context.NoLaunchExe,
                '-ExpectedNoLaunchExecutableHash',$context.NoLaunchHash)
        }
        elseif ($Mode -eq 'XStreamWithCapture') {
            if (-not $OmitXStreamAuthorization) { $arguments += '-AuthorizeXStream' }
            if (-not $OmitPhysicalDiskConfirmation) {
                $arguments += '-ConfirmPhysicalDiskDisconnected'
            }
            $arguments += @('-XStreamRunnerPath',$context.XStreamRunner,
                '-ExpectedXStreamRunnerHash',$context.XStreamRunnerHash,
                '-XStreamExecutablePath',$context.MockExe,
                '-ExpectedXStreamExecutableHash',$context.MockHash,
                '-XStreamTimeoutSeconds','30')
        }
        $hostPath = Join-Path $PSHOME 'powershell.exe'
        $outerLog = Join-Path $tempRoot ("$Name-outer.log")
        $result = Invoke-LecS65NativeProcess -Name "Case $Name" -FilePath $hostPath `
            -Arguments $arguments -WorkingDirectory $context.Repository `
            -LogPath $outerLog -TimeoutSeconds 30 -ReturnOnNonZeroExit
    }
    finally {
        $env:LECS65_DIAGNOSTIC_MOCK_SCENARIO = $oldScenario
        $env:LECS65_DIAGNOSTIC_MOCK_COUNTER = $oldCounter
    }
    $invocations = if(Test-Path -LiteralPath $counter){@(Get-Content -LiteralPath $counter).Count}else{0}
    $records = if(Test-Path -LiteralPath $counter){@(Get-Content -LiteralPath $counter)}else{@()}
    return [pscustomobject]@{ExitCode=$result.ExitCode;Evidence=$caseRoot;OuterLog=$outerLog;Invocations=$invocations;Records=$records}
}

try {
    $success = Invoke-Case -Name 'preflight-success'
    Add-Result 'successful elevated passive preflight' ($success.ExitCode -eq 0 -and $success.Invocations -eq 0)
    Add-Result 'preflight creates complete launcher evidence' (
        (Test-Path (Join-Path $success.Evidence 'launcher.stdout.txt')) -and
        (Test-Path (Join-Path $success.Evidence 'launcher.stderr.txt')) -and
        (Test-Path (Join-Path $success.Evidence 'launcher.exception.txt')) -and
        ((Get-Content -Raw (Join-Path $success.Evidence 'result.json')|ConvertFrom-Json).Status -eq 'PASS'))

    $capture = Invoke-Case -Name 'capture-success' -Mode CaptureQualification
    Add-Result 'mock capture qualification succeeds without PnPUtil' (
        $capture.ExitCode -eq 0 -and $capture.Invocations -eq 1 -and
        @($capture.Records) -contains 'capture')

    $delayed = Invoke-Case -Name 'delayed-file-flush' -Mode CaptureQualification `
        -Scenario DelayedFileFlush
    $delayedReady = Get-Content -Raw (Join-Path $delayed.Evidence 'capture-ready.json') |
        ConvertFrom-Json
    Add-Result 'delayed raw-log flush does not hide confirmed live kernel output' (
        $delayed.ExitCode -eq 0 -and $delayedReady.ConfirmedKernelMessage -and
        $delayedReady.CaptureInitialized -and
        $delayedReady.ConfirmedKernelMessageText -match 'STORMINI')

    foreach($case in @(
        @{Name='timestamp-prefix';Scenario='Success'}
        @{Name='millisecond-prefix';Scenario='Success'}
        @{Name='pnp-status-prefix';Scenario='PnpStatusPrefix'}
        @{Name='split-line';Scenario='SplitKernelLine'}
        @{Name='delayed-stdout';Scenario='DelayedStdout'}
        @{Name='large-burst';Scenario='LargeKernelBurst'}
    )) {
        $stream = Invoke-Case -Name $case.Name -Mode CaptureQualification `
            -Scenario $case.Scenario
        $readyRecord = if(Test-Path (Join-Path $stream.Evidence 'capture-ready.json')) {
            Get-Content -Raw (Join-Path $stream.Evidence 'capture-ready.json') |
                ConvertFrom-Json
        } else { $null }
        Add-Result "$($case.Name) establishes production capture readiness" (
            $stream.ExitCode -eq 0 -and $null -ne $readyRecord -and
            $readyRecord.ConfirmedKernelMessage -and
            $readyRecord.CollectorProcessHealthy -and
            (Test-Path (Join-Path $stream.Evidence 'capture-process.txt')))
    }

    $wrongSource = Invoke-Case -Name 'incorrect-source' `
        -Mode CaptureQualification -Scenario IncorrectMessageSource `
        -ReadinessTimeout 2 -Duration 1
    Add-Result 'arbitrary embedded marker cannot establish readiness' (
        $wrongSource.ExitCode -ne 0 -and $wrongSource.Invocations -eq 1 -and
        -not (Test-Path (Join-Path $wrongSource.Evidence 'capture-ready.json')))

    $stderrCase = Invoke-Case -Name 'collector-stderr' `
        -Mode CaptureQualification -Scenario CollectorStderr
    Add-Result 'collector stderr is independent and fails readiness closed' (
        $stderrCase.ExitCode -ne 0 -and
        (Get-Content -Raw (Join-Path $stderrCase.Evidence 'capture-live.stderr.txt')) `
            -match 'MOCK COLLECTOR ERROR')

    $afterReadyExit = Invoke-Case -Name 'exit-after-readiness' `
        -Mode RestartWithCapture -Scenario ExitAfterReadiness `
        -ReadinessTimeout 3 -Duration 1
    Add-Result 'collector exit during stability gate prevents PnP restart' (
        $afterReadyExit.ExitCode -ne 0 -and
        @($afterReadyExit.Records | Where-Object {$_ -eq 'restart'}).Count -eq 0)

    $confirmedExit = Invoke-Case -Name 'exit-after-confirmed-readiness' `
        -Mode CaptureQualification -Scenario ExitAfterConfirmedReadiness `
        -ReadinessTimeout 3 -Duration 2
    Add-Result 'collector exit after confirmed readiness is preserved as failure' (
        $confirmedExit.ExitCode -ne 0 -and
        (Test-Path (Join-Path $confirmedExit.Evidence 'capture-ready.json')) -and
        (Get-Content -Raw (Join-Path $confirmedExit.Evidence `
            'launcher.exception.txt')) -match 'exited with code 44')

    $install = Invoke-Case -Name 'install-success' -Mode InstallWithCapture
    Add-Result 'mock capture/install path invokes each child exactly once' (
        $install.ExitCode -eq 0 -and $install.Invocations -eq 2 -and
        ($install.Records -join ',') -eq 'capture,install')

    $literalEnvironment = Invoke-Case -Name 'literal-environment-path' `
        -Mode InstallWithCapture -PnPUtilPath '$env:SystemRoot\System32\pnputil.exe'
    $literalResult = Get-Content -Raw `
        (Join-Path $literalEnvironment.Evidence 'result.json') | ConvertFrom-Json
    Add-Result 'literal unexpanded environment-variable path fails before install' (
        $literalEnvironment.ExitCode -ne 0 -and
        $literalResult.InstallInvocations -eq 0 -and
        @($literalEnvironment.Records | Where-Object { $_ -eq 'install' }).Count -eq 0)

    $percentEnvironment = Invoke-Case -Name 'percent-environment-path' `
        -Mode InstallWithCapture -PnPUtilPath '%SystemRoot%\System32\pnputil.exe'
    $percentResult = Get-Content -Raw `
        (Join-Path $percentEnvironment.Evidence 'result.json') | ConvertFrom-Json
    Add-Result 'percent-style unexpanded environment path fails before install' (
        $percentEnvironment.ExitCode -ne 0 -and
        $percentResult.InstallInvocations -eq 0)

    $relativePath = Invoke-Case -Name 'relative-pnputil-path' `
        -Mode InstallWithCapture -PnPUtilPath '.\pnputil.exe'
    $relativeResult = Get-Content -Raw `
        (Join-Path $relativePath.Evidence 'result.json') | ConvertFrom-Json
    Add-Result 'relative PnPUtil path fails before install' (
        $relativePath.ExitCode -ne 0 -and
        $relativeResult.InstallInvocations -eq 0)

    $missingPath = Join-Path $tempRoot 'missing\pnputil.exe'
    $missing = Invoke-Case -Name 'missing-pnputil-path' `
        -Mode InstallWithCapture -PnPUtilPath $missingPath
    $missingResult = Get-Content -Raw `
        (Join-Path $missing.Evidence 'result.json') | ConvertFrom-Json
    Add-Result 'missing absolute PnPUtil path fails before install' (
        $missing.ExitCode -ne 0 -and
        $missingResult.InstallInvocations -eq 0)

    $spaceDirectory = Join-Path $tempRoot 'native tool with spaces'
    [IO.Directory]::CreateDirectory($spaceDirectory) | Out-Null
    $spacePnp = Join-Path $spaceDirectory 'mock pnputil.exe'
    Copy-Item -LiteralPath $context.MockExe -Destination $spacePnp
    $spacePath = Invoke-Case -Name 'space-pnputil-path' `
        -Mode InstallWithCapture -PnPUtilPath $spacePnp
    Add-Result 'absolute PnPUtil path containing spaces is serialized exactly once' (
        $spacePath.ExitCode -eq 0 -and
        ($spacePath.Records -join ',') -eq 'capture,install')

    $oldParentPath = $env:LECS65_TEST_PNPUTIL
    try {
        $env:LECS65_TEST_PNPUTIL = $spacePnp
        $expandedParentPath = [Environment]::ExpandEnvironmentVariables(
            '%LECS65_TEST_PNPUTIL%')
        $parentExpanded = Invoke-Case -Name 'parent-expanded-pnputil-path' `
            -Mode InstallWithCapture -PnPUtilPath $expandedParentPath
        Add-Result 'parent-expanded environment path crosses the child boundary exactly' (
            $parentExpanded.ExitCode -eq 0 -and
            ($parentExpanded.Records -join ',') -eq 'capture,install')
    }
    finally {
        $env:LECS65_TEST_PNPUTIL = $oldParentPath
    }

    $mockWindowsRoot = Join-Path $tempRoot 'mock Windows root'
    $mockSystem32 = Join-Path $mockWindowsRoot 'System32'
    [IO.Directory]::CreateDirectory($mockSystem32) | Out-Null
    Copy-Item -LiteralPath $context.MockExe `
        -Destination (Join-Path $mockSystem32 'pnputil.exe')
    $oldSystemRoot = $env:SystemRoot
    try {
        $env:SystemRoot = $mockWindowsRoot
        $defaultResolved = Resolve-PnPUtilPath -Path ''
    }
    finally {
        $env:SystemRoot = $oldSystemRoot
    }
    Add-Result 'omitted PnPUtil path resolves from SystemRoot in production code' (
        $defaultResolved -eq (Join-Path $mockSystem32 'pnputil.exe'))

    $restart = Invoke-Case -Name 'restart-success' -Mode RestartWithCapture
    Add-Result 'mock capture/restart path invokes each child exactly once' (
        $restart.ExitCode -eq 0 -and $restart.Invocations -eq 2 -and
        ($restart.Records -join ',') -eq 'capture,restart' -and
        (Get-Content -Raw (Join-Path $restart.Evidence 'result.json') |
            ConvertFrom-Json).PnpRestartInvocations -eq 1)

    $noLaunch = Invoke-Case -Name 'no-launch-success' `
        -Mode NoLaunchWithCapture -Scenario DelayedFileFlush
    $noLaunchOuter = Get-Content -Raw `
        (Join-Path $noLaunch.Evidence 'result.json') | ConvertFrom-Json
    $noLaunchInner = Get-Content -Raw `
        (Join-Path $noLaunch.Evidence 'no-launch-evidence\result.json') |
        ConvertFrom-Json
    Add-Result 'exact production no-launch path succeeds after live stdout readiness' (
        $noLaunch.ExitCode -eq 0 -and $noLaunch.Invocations -eq 1 -and
        ($noLaunch.Records -join ',') -eq 'capture' -and
        $noLaunchOuter.NoLaunchInvocations -eq 1 -and
        $noLaunchInner.Result -eq 'PASS' -and
        $noLaunchInner.HardwareLaunchObserved -eq $false)

    $xstream = Invoke-Case -Name 'xstream-success' `
        -Mode XStreamWithCapture -Scenario DelayedFileFlush
    $xstreamOuter = Get-Content -Raw `
        (Join-Path $xstream.Evidence 'result.json') | ConvertFrom-Json
    $xstreamInner = Get-Content -Raw `
        (Join-Path $xstream.Evidence 'xstream-evidence\result.json') |
        ConvertFrom-Json
    Add-Result 'exact production XStream path launches one observer after capture readiness' (
        $xstream.ExitCode -eq 0 -and $xstream.Invocations -eq 1 -and
        ($xstream.Records -join ',') -eq 'capture' -and
        $xstreamOuter.XStreamInvocations -eq 1 -and
        $xstreamInner.XStreamLaunchCount -eq 1 -and
        $xstreamInner.ExplicitAcquireCount -eq 0 -and
        $xstreamInner.Waveform.Observed)
    $xstreamNoAuth = Invoke-Case -Name 'xstream-no-auth' `
        -Mode XStreamWithCapture -OmitXStreamAuthorization
    Add-Result 'XStream mode rejects missing owner authorization before capture' (
        $xstreamNoAuth.ExitCode -ne 0 -and $xstreamNoAuth.Invocations -eq 0)
    $xstreamNoDisk = Invoke-Case -Name 'xstream-no-disk-confirmation' `
        -Mode XStreamWithCapture -OmitPhysicalDiskConfirmation
    Add-Result 'XStream mode rejects missing physical-isolation confirmation before capture' (
        $xstreamNoDisk.ExitCode -ne 0 -and $xstreamNoDisk.Invocations -eq 0)

    $early = Invoke-Case -Name 'early-exception' -Mode CaptureQualification `
        -FailureInjection BeforeCaptureStart
    Add-Result 'early terminating exception is persisted before child logs' (
        $early.ExitCode -ne 0 -and $early.Invocations -eq 0 -and
        (Get-Content -Raw (Join-Path $early.Evidence 'launcher.exception.txt')) -match 'Injected failure')

    foreach($case in @(
        @{Name='debugview-startup';Scenario='DebugViewStartupFailure';Text='readiness'}
        @{Name='missing-ready';Scenario='MissingCaptureReady';Text='readiness'}
        @{Name='capture-exit';Scenario='UnexpectedCaptureExit';Text='readiness'}
        @{Name='capture-timeout';Scenario='CaptureTimeout';Text='timed out'}
    )) {
        $failure = Invoke-Case -Name $case.Name -Mode CaptureQualification `
            -Scenario $case.Scenario -ReadinessTimeout 5 -Duration 1
        Add-Result "$($case.Name) fails once with preserved evidence" (
            $failure.ExitCode -ne 0 -and $failure.Invocations -eq 1 -and
            (Test-Path (Join-Path $failure.Evidence 'result.json')) -and
            (Get-Content -Raw (Join-Path $failure.Evidence 'launcher.exception.txt')) -match $case.Text)
    }

    $pnpFailure = Invoke-Case -Name 'pnp-failure' -Mode InstallWithCapture `
        -Scenario PnPUtilFailure
    Add-Result 'nonzero PnPUtil result stops after one install invocation' (
        $pnpFailure.ExitCode -ne 0 -and $pnpFailure.Invocations -eq 2 -and
        @($pnpFailure.Records | Where-Object {$_ -eq 'install'}).Count -eq 1)

    $restartFailure = Invoke-Case -Name 'restart-failure' `
        -Mode RestartWithCapture -Scenario PnPUtilFailure
    Add-Result 'nonzero PnP restart stops after one restart invocation' (
        $restartFailure.ExitCode -ne 0 -and
        $restartFailure.Invocations -eq 2 -and
        @($restartFailure.Records | Where-Object {$_ -eq 'restart'}).Count -eq 1)
    Start-Sleep -Milliseconds 500
    Add-Result 'PnP failure shuts down the owned capture child' (
        @(Get-Process | Where-Object {$_.Path -eq $context.MockExe}).Count -eq 0)

    $badPackage = Invoke-Case -Name 'bad-package-hash' -InfHash ('0'*64)
    Add-Result 'incorrect package hash stops before active work' ($badPackage.ExitCode -ne 0 -and $badPackage.Invocations -eq 0)
    $badTool = Invoke-Case -Name 'bad-tool-hash' -DebugViewHash ('0'*64)
    Add-Result 'incorrect DebugView hash stops before active work' ($badTool.ExitCode -ne 0 -and $badTool.Invocations -eq 0)
    $badHead = Invoke-Case -Name 'bad-head' -ExpectedLauncherCommit ('0'*40)
    Add-Result 'incorrect launcher revision stops before active work' ($badHead.ExitCode -ne 0 -and $badHead.Invocations -eq 0)
    $badSource = Invoke-Case -Name 'bad-source' -ExpectedWorkflowCommit ('0'*40)
    Add-Result 'incorrect package workflow revision stops before active work' ($badSource.ExitCode -ne 0 -and $badSource.Invocations -eq 0)

    $marker = Invoke-Case -Name 'existing-marker' -Mode CaptureQualification -ExistingCaptureMarker
    Add-Result 'existing attempt marker prevents capture' ($marker.ExitCode -ne 0 -and $marker.Invocations -eq 0)
    $duplicate = Invoke-Case -Name 'duplicate-collector' `
        -Mode CaptureQualification -ExistingCollector
    Add-Result 'existing collector prevents duplicate capture' (
        $duplicate.ExitCode -ne 0 -and $duplicate.Invocations -eq 0 -and
        (Get-Content -Raw (Join-Path $duplicate.Evidence `
            'launcher.exception.txt')) -match 'already running')
    $logFailure = Invoke-Case -Name 'log-failure' -EvidencePathIsFile
    Add-Result 'log creation failure stops before active work' ($logFailure.ExitCode -ne 0 -and $logFailure.Invocations -eq 0)

    Add-Result 'no case retries any operation' (
        @(Get-ChildItem -LiteralPath $tempRoot -Filter '*-counter.txt' |
            Where-Object {
                $lines=@(Get-Content -LiteralPath $_.FullName)
                @($lines|Where-Object{$_ -eq 'capture'}).Count -gt 1 -or
                (@($lines|Where-Object{$_ -eq 'install'}).Count -gt 1) -or
                (@($lines|Where-Object{$_ -eq 'restart'}).Count -gt 1)
            }).Count -eq 0)
    Start-Sleep -Milliseconds 500
    Add-Result 'no mock native process remains orphaned' (
        @(Get-Process | Where-Object {$_.Path -eq $context.MockExe}).Count -eq 0)
}
finally {
    [ordered]@{Host="Windows PowerShell $($PSVersionTable.PSVersion)";Elevated=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator);TestOnlyElevationOverride=[bool]$AllowNonElevatedTestMode;Passed=$script:Passed;Total=$script:Checks;Failed=$script:Failed} |
        ConvertTo-Json | Set-Content -LiteralPath $ResultPath -Encoding UTF8
    try { Stop-Transcript | Out-Null } catch { }
}

Write-Host ("DIAGNOSTIC LAUNCHER TESTS: {0}/{1} passed; {2} failed." -f $script:Passed,$script:Checks,$script:Failed)
if($script:Failed -ne 0){exit 1}
