[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$workflow = Join-Path $repo 'scripts\build-driver-package.ps1'
. (Join-Path $repo 'scripts\native-process.ps1')
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $workflow, [ref]$tokens, [ref]$errors)
if ($errors.Count -ne 0) {
    throw "Package workflow has parser errors: $($errors.Message -join '; ')"
}

$functionNames = @(
    'Invoke-ExternalCaptured'
    'Get-ContainedRelativeFilePath'
    'Get-FileRecord'
    'Test-AsciiMarker'
    'Assert-PnpDiagnosticMarkerState'
    'Get-ExpectedRegressionSuiteIds'
    'Get-RegressionSuiteOutcomes'
    'Assert-RegressionManifest'
    'Assert-ManifestComplete'
    'Assert-PackageSignatureState'
)
foreach ($definition in @($ast.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $functionNames -contains $node.Name
}, $true) | Sort-Object { $_.Extent.StartOffset })) {
    . ([scriptblock]::Create($definition.Extent.Text))
}
foreach ($name in $functionNames) {
    if (-not (Get-Command $name -CommandType Function -ErrorAction SilentlyContinue)) {
        throw "Unable to load workflow function: $name"
    }
}

$script:Checks = 0
$script:Passed = 0
$script:Failed = 0

function Test-Condition {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][bool]$Condition
    )

    $script:Checks++
    if ($Condition) {
        Write-Host "[PASS] $Name" -ForegroundColor Green
        $script:Passed++
    }
    else {
        Write-Host "[FAIL] $Name" -ForegroundColor Red
        $script:Failed++
    }
}

function Expect-Rejection {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][scriptblock]$Action
    )

    $script:Checks++
    try {
        & $Action
        Write-Host "[FAIL] $Name (unexpected success)" -ForegroundColor Red
        $script:Failed++
    }
    catch {
        Write-Host "[PASS] $Name" -ForegroundColor Green
        $script:Passed++
    }
}

$hostLabel = "PowerShell $($PSVersionTable.PSVersion) / CLR $([Environment]::Version)"
Write-Host "PACKAGE WORKFLOW COMPATIBILITY HOST: $hostLabel"

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    'lecroy package [compat] ' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($tempRoot) | Out-Null
try {
    $artifactRoot = Join-Path $tempRoot 'artifact root #1'
    $packageDir = Join-Path $artifactRoot 'package'
    $evidenceDir = Join-Path $artifactRoot 'evidence'
    $utilityDir = Join-Path $artifactRoot 'tools\utility [x64]'
    foreach ($directory in @($packageDir, $evidenceDir, $utilityDir)) {
        [System.IO.Directory]::CreateDirectory($directory) | Out-Null
    }

    $specialFile = Join-Path $utilityDir 'lec diag #1.exe'
    Set-Content -LiteralPath $specialFile -Value 'mock utility' -Encoding Ascii
    Assert-PnpDiagnosticMarkerState -Path $specialFile -ExpectedEnabled $false
    $diagnosticFile = Join-Path $utilityDir 'diagnostic.sys'
    Set-Content -LiteralPath $diagnosticFile -Value 'binary PNP_STATUS: marker' -Encoding Ascii
    Assert-PnpDiagnosticMarkerState -Path $diagnosticFile -ExpectedEnabled $true
    Test-Condition 'diagnostic marker state is checked from binary content' $true
    Expect-Rejection 'ambiguous diagnostic marker state is rejected' {
        Assert-PnpDiagnosticMarkerState -Path $specialFile -ExpectedEnabled $true
    }
    $relative = Get-ContainedRelativeFilePath -Root $artifactRoot -Path $specialFile
    Test-Condition 'spaces and special characters have a deterministic relative path' (
        $relative -eq 'tools\utility [x64]\lec diag #1.exe')

    $caseVariantRoot = $artifactRoot.ToUpperInvariant()
    Test-Condition 'Windows path comparison is case-insensitive' (
        (Get-ContainedRelativeFilePath -Root $caseVariantRoot -Path $specialFile) -eq
        'tools\utility [x64]\lec diag #1.exe')
    $normalizedInput = Join-Path $utilityDir '..\utility [x64]\lec diag #1.exe'
    Test-Condition 'in-root dot segments normalize deterministically' (
        (Get-ContainedRelativeFilePath -Root $artifactRoot -Path $normalizedInput) -eq
        'tools\utility [x64]\lec diag #1.exe')

    Expect-Rejection 'missing files are rejected' {
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path (Join-Path $artifactRoot 'missing.sys') | Out-Null
    }
    Expect-Rejection 'missing artifact roots are rejected' {
        Get-ContainedRelativeFilePath -Root (Join-Path $tempRoot 'missing root') `
            -Path $specialFile | Out-Null
    }

    $outsideFile = Join-Path $tempRoot 'outside.sys'
    Set-Content -LiteralPath $outsideFile -Value 'outside' -Encoding Ascii
    Expect-Rejection 'files outside the artifact root are rejected' {
        Get-ContainedRelativeFilePath -Root $artifactRoot -Path $outsideFile | Out-Null
    }
    Expect-Rejection 'parent traversal outside the artifact root is rejected' {
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path (Join-Path $artifactRoot '..\outside.sys') | Out-Null
    }

    $siblingRoot = $artifactRoot + '-sibling'
    [System.IO.Directory]::CreateDirectory($siblingRoot) | Out-Null
    $siblingFile = Join-Path $siblingRoot 'sibling.sys'
    Set-Content -LiteralPath $siblingFile -Value 'sibling' -Encoding Ascii
    Expect-Rejection 'sibling-prefix paths do not pass containment' {
        Get-ContainedRelativeFilePath -Root $artifactRoot -Path $siblingFile | Out-Null
    }

    $currentDrive = [System.IO.Path]::GetPathRoot($artifactRoot).Substring(0, 1)
    $differentDrive = if ($currentDrive -ieq 'Z') { 'Y' } else { 'Z' }
    Expect-Rejection 'different drive roots are rejected explicitly' {
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path ($differentDrive + ':\missing.sys') | Out-Null
    }

    $artifactPaths = [ordered]@{
        'LecS65AcqDrv.inf' = Join-Path $packageDir 'LecS65AcqDrv.inf'
        'LecS65AcqDrv.cat' = Join-Path $packageDir 'LecS65AcqDrv.cat'
        'LecS65AcqDrv.sys' = Join-Path $packageDir 'LecS65AcqDrv.sys'
        'LecS65AcqDrv.pdb' = Join-Path $evidenceDir 'LecS65AcqDrv.pdb'
    }
    foreach ($entry in $artifactPaths.GetEnumerator()) {
        Set-Content -LiteralPath $entry.Value -Value ("mock {0}" -f $entry.Key) `
            -Encoding Ascii
    }
    foreach ($logName in @('pe-debug.txt', 'pdb-debug-sources.txt',
            'pdb-debug-identity.txt')) {
        Set-Content -LiteralPath (Join-Path $evidenceDir $logName) `
            -Value 'mocked operation; no private key used' -Encoding Ascii
    }

    foreach ($mockOperation in @(
            'sign-sys', 'sign-cat', 'verify-sys', 'verify-cat',
            'verify-membership')) {
        $null = Invoke-ExternalCaptured -Name "Mock $mockOperation" `
            -FilePath $env:ComSpec `
            -Arguments @('/d', '/c', "echo mocked-$mockOperation") `
            -LogPath (Join-Path $evidenceDir "mock-$mockOperation.log")
    }
    Test-Condition 'mock signing and verification commands complete without key use' (
        @(Get-ChildItem -LiteralPath $evidenceDir -Filter 'mock-*.log' |
            Where-Object Name -notmatch '\.(stdout|stderr)\.txt$').Count -eq 5)

    $nativeFixture = Join-Path $tempRoot 'native stderr ünicode.ps1'
    Set-Content -LiteralPath $nativeFixture -Encoding UTF8 -Value @'
param([string]$Value)
if ($Value -cne 'space # ünicode') { exit 91 }
Write-Output 'stdout:argument-match'
[Console]::Error.WriteLine('diagnostic-stderr:argument-match')
exit 0
'@
    $nativeLog = Join-Path $evidenceDir 'native-stderr-zero.log'
    $nativeHost = (Get-Process -Id $PID).Path
    $nativeResult = Invoke-LecS65NativeProcess -Name 'Native stderr zero' `
        -FilePath $nativeHost `
        -Arguments @('-NoProfile', '-NonInteractive', '-File', $nativeFixture,
            '-Value', 'space # ünicode') `
        -WorkingDirectory $tempRoot -LogPath $nativeLog -TimeoutSeconds 30
    Test-Condition 'native stderr with exit zero is diagnostic and independently logged' (
        $nativeResult.ExitCode -eq 0 -and
        $nativeResult.StdOut -match 'stdout:argument-match' -and
        $nativeResult.StdErr -match 'diagnostic-stderr:argument-match' -and
        (Get-Content -LiteralPath ($nativeLog + '.stderr.txt') -Raw) -match
            'diagnostic-stderr')

    $emptyArgumentFixture = Join-Path $tempRoot 'native-empty-argument.ps1'
    Set-Content -LiteralPath $emptyArgumentFixture -Encoding UTF8 -Value @'
param(
    [string]$First,
    [AllowEmptyString()][string]$Second,
    [string]$Third
)
if ($First -cne 'before' -or $Second -cne '' -or $Third -cne 'after') {
    [Console]::Error.WriteLine(
        "argument mismatch: first=[$First] second=[$Second] third=[$Third]")
    exit 92
}
Write-Output 'empty-argument-preserved'
exit 0
'@
    $emptyArgumentLog = Join-Path $evidenceDir 'native-empty-argument.log'
    $emptyArgumentResult = Invoke-LecS65NativeProcess `
        -Name 'Native empty argument' -FilePath $nativeHost `
        -Arguments @('-NoProfile', '-NonInteractive', '-File',
            $emptyArgumentFixture, 'before', '', 'after') `
        -WorkingDirectory $tempRoot -LogPath $emptyArgumentLog `
        -TimeoutSeconds 30
    Test-Condition 'native runner preserves an explicitly empty argument' (
        $emptyArgumentResult.ExitCode -eq 0 -and
        $emptyArgumentResult.StdOut -match 'empty-argument-preserved' -and
        [string]::IsNullOrEmpty($emptyArgumentResult.StdErr))

    $startupLog = Join-Path $evidenceDir 'startup-failure.log'
    Expect-Rejection 'native startup failure is logged and rejected' {
        Invoke-LecS65NativeProcess -Name 'Missing native child' `
            -FilePath (Join-Path $tempRoot 'missing-child.exe') `
            -Arguments @() -WorkingDirectory $tempRoot -LogPath $startupLog `
            -TimeoutSeconds 5 | Out-Null
    }
    Test-Condition 'startup failure preserves separate evidence logs' (
        (Test-Path -LiteralPath $startupLog) -and
        (Test-Path -LiteralPath ($startupLog + '.stdout.txt')) -and
        (Get-Content -LiteralPath ($startupLog + '.stderr.txt') -Raw) -match
            'failed to start or supervise')

    $workflowRelativePaths = @(
        Get-ContainedRelativeFilePath -Root $artifactRoot -Path $specialFile
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path (Join-Path $evidenceDir 'pe-debug.txt')
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path (Join-Path $evidenceDir 'pdb-debug-sources.txt')
        Get-ContainedRelativeFilePath -Root $artifactRoot `
            -Path (Join-Path $evidenceDir 'pdb-debug-identity.txt')
    )
    Test-Condition 'all formerly failing workflow path sites are reproducible' (
        ($workflowRelativePaths -join ',') -eq
        'tools\utility [x64]\lec diag #1.exe,evidence\pe-debug.txt,' +
        'evidence\pdb-debug-sources.txt,evidence\pdb-debug-identity.txt')

    $expectedThumbprint = 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'
    $validSignature = [pscustomobject]@{
        Status = 'Valid'
        SignerCertificate = [pscustomobject]@{ Thumbprint = $expectedThumbprint }
    }
    Assert-PackageSignatureState -SigningRequested $true `
        -SysSignature $validSignature -CatSignature $validSignature `
        -ExpectedThumbprint $expectedThumbprint
    Test-Condition 'mocked signed SYS/CAT state is accepted' $true

    $notSigned = [pscustomobject]@{ Status = 'NotSigned'; SignerCertificate = $null }
    Assert-PackageSignatureState -SigningRequested $false `
        -SysSignature $notSigned -CatSignature $notSigned
    Test-Condition 'mocked unsigned validation state is accepted' $true

    Expect-Rejection 'wrong mocked signer thumbprint is rejected' {
        Assert-PackageSignatureState -SigningRequested $true `
            -SysSignature $validSignature -CatSignature $validSignature `
            -ExpectedThumbprint ('B' * 40)
    }
    Expect-Rejection 'signed artifact in unsigned mode is rejected' {
        Assert-PackageSignatureState -SigningRequested $false `
            -SysSignature $validSignature -CatSignature $notSigned
    }

    $artifacts = @($artifactPaths.Values | ForEach-Object {
        Get-FileRecord -Path $_ -Root $artifactRoot
    })
    $suiteOutcomes = @(Get-ExpectedRegressionSuiteIds | ForEach-Object {
        [ordered]@{ Id=$_; Passed=1; Total=1; Failed=0; Occurrences=1 }
    })
    $manifest = [ordered]@{
        Schema = 2
        Source = [ordered]@{
            WorkflowCommit = ('1' * 40)
            DriverTree = ('2' * 40)
            IncludeTree = ('3' * 40)
        }
        Build = [ordered]@{
            CleanRebuild = $true
            PnpStatusDiagnostics = $true
            DiagnosticMarker = 'PNP_STATUS:'
            WdmV3MamDma = $true
            WdmV3MamDmaMarker = 'LECS65_WDM_V3_MAM_DMA_ENABLED'
            FirstMamExperimentOnly = $true
            FirstMamExperimentMarker = 'LECS65_FIRST_MAM_EXPERIMENT_ONLY'
            MamLaunchLimit = 0
            MamLaunchLimitMarker = $null
            Configurations = @(
                [ordered]@{ Configuration = 'Debug'; PnpStatusDiagnostics = $true; WdmV3MamDma = $true; FirstMamExperimentOnly = $true; MamLaunchLimit = 0 }
                [ordered]@{ Configuration = 'Release'; PnpStatusDiagnostics = $true; WdmV3MamDma = $true; FirstMamExperimentOnly = $true; MamLaunchLimit = 0 }
            )
        }
        Regression = [ordered]@{
            CompleteDry = 'PASS'
            Expanded = 'PASS'
            SuiteContract = 'batch16-regression-v1'
            Suites = $suiteOutcomes
            FunctionalTestsPassed = $suiteOutcomes.Count
            FunctionalTestsFailed = 0
            DefaultDmaStressIterations = 5000
            DefaultIntenStressIterations = 1000
            ExpandedDmaStressIterations = 50000
            ExpandedIntenStressIterations = 10000
            TotalStressIterations = 66000
        }
        Artifacts = $artifacts
        InstallAuthorized = $false
        Safety = [ordered]@{
            DriverStoreChanged = $false
            PnpChanged = $false
            DeviceOpened = $false
        }
    }
    Assert-ManifestComplete -Manifest $manifest
    $manifestPath = Join-Path $evidenceDir 'manifest.json'
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    $roundTrip = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Assert-ManifestComplete -Manifest $roundTrip
    Test-Condition 'mock package sequence produces a complete round-trip manifest' (
        (Test-Path -LiteralPath $manifestPath -PathType Leaf) -and
        $roundTrip.Artifacts.Count -eq 4)

    Test-Condition 'all manifest paths stay beneath the output root' (
        @($roundTrip.Artifacts | Where-Object {
            [System.IO.Path]::IsPathRooted($_.RelativePath) -or
            $_.RelativePath.StartsWith('..')
        }).Count -eq 0)

    $incomplete = $roundTrip.PSObject.Copy()
    $incomplete.Artifacts = @($roundTrip.Artifacts | Select-Object -First 3)
    Expect-Rejection 'incomplete artifact manifests are rejected' {
        Assert-ManifestComplete -Manifest $incomplete
    }

    $ambiguous = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $ambiguous.Build.Configurations[1].PnpStatusDiagnostics = $false
    Expect-Rejection 'mixed diagnostic build modes are rejected' {
        Assert-ManifestComplete -Manifest $ambiguous
    }

    $ambiguousDma = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $ambiguousDma.Build.Configurations[1].WdmV3MamDma = $false
    Expect-Rejection 'mixed WDM-v3 MAM DMA build modes are rejected' {
        Assert-ManifestComplete -Manifest $ambiguousDma
    }

    $ambiguousExperiment = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $ambiguousExperiment.Build.Configurations[1].FirstMamExperimentOnly = $false
    Expect-Rejection 'mixed first-MAM experiment modes are rejected' {
        Assert-ManifestComplete -Manifest $ambiguousExperiment
    }

    $ambiguousLimit = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $ambiguousLimit.Build.Configurations[1].MamLaunchLimit = 1
    Expect-Rejection 'mixed MAM launch limits are rejected' {
        Assert-ManifestComplete -Manifest $ambiguousLimit
    }

    $invalidLimit = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $invalidLimit.Build.FirstMamExperimentOnly = $false
    $invalidLimit.Build.MamLaunchLimit = 1
    $invalidLimit.Build.MamLaunchLimitMarker = 'LECS65_MAM_LAUNCH_LIMIT=1'
    foreach ($configuration in $invalidLimit.Build.Configurations) {
        $configuration.FirstMamExperimentOnly = $false
        $configuration.MamLaunchLimit = 1
    }
    Assert-ManifestComplete -Manifest $invalidLimit
    Test-Condition 'bounded regular-MAM manifest is accepted' $true

    $suiteText = @'
DRY REGRESSION: 70/70 passed; 0 failed.
PRE-GO SIDE EFFECTS: 10/10 passed; 0 failed.
MDL-ONLY REGISTRATION: 15/15 passed; 0 failed.
DMA LAYOUT: 13/13 passed; 0 failed.
DMA OWNERSHIP: 17/17 passed; 0 failed.
DMA transaction tests: 134/134 checks passed; hardware launch unavailable
DMA V3 EXPLICIT TRANSFER: 74/74 passed; hardware launch absent
SG BRIDGE MOCK: 27/27 passed; 0 failed.
SG SYNC V3: 157/157 passed; 0 failed.
PNP PUBLICATION: 38/38 passed; 0 failed.
PNP IRP LIFETIME: 10/10 passed; 0 failed.
LECDIAG COMMAND CHAIN: 17/17 passed; 0 failed.
NO-LAUNCH PREPARATION TESTS: 3/3 passed; 0 failed.
RUNTIME LIFETIME AUDIT: 19/19 passed; 0 failed.
PROCESS DMA LIFETIME MODEL: 23/23 passed; 0 failed.
RUNTIME BASELINE TESTS: 6/6 passed; 0 failed.
FROZEN PACKAGE SIGNING CONTRACTS: 11/11 passed; 0 failed.
CRASH DUMP READINESS: 14/14 passed; 0 failed.
SYSTEM IMAGE READINESS: 21/21 passed; 0 failed.
RECOVERY LAYOUT: 6/6 passed; 0 failed.
CODEX CONTINUATION: 17/17 passed; 0 failed.
DMA COMPLETION: 39/39 passed; 0 failed.
DMA INTERLEAVINGS: 2/2 passed; 0 failed.
INTEN functional: 49 PASS, 0 FAIL, 0 SKIP
'@
    $parsedSuites = @(Get-RegressionSuiteOutcomes -Text $suiteText)
    $changedSuites = @(Get-RegressionSuiteOutcomes -Text (
        $suiteText.Replace('14/14', '15/15')))
    $parsedTotal = ($parsedSuites | ForEach-Object { [int]$_.Passed } |
        Measure-Object -Sum).Sum
    $changedTotal = ($changedSuites | ForEach-Object { [int]$_.Passed } |
        Measure-Object -Sum).Sum
    $workflowText = Get-Content -LiteralPath $workflow -Raw
    Test-Condition 'workflow keeps PowerShell 5.1 path and current regression guards' (
        -not $workflowText.Contains('[System.IO.Path]::GetRelativePath') -and
        $workflowText.Contains("SuiteContract = 'batch16-regression-v1'") -and
        $workflowText.Contains('Get-RegressionSuiteOutcomes') -and
        -not $workflowText.Contains('FunctionalTestsPassed -ne 776') -and
        -not $workflowText.Contains('FunctionalTestsPassed = 776') -and
        $parsedSuites.Count -eq 24 -and $parsedTotal -eq 792 -and
        $changedTotal -eq 793)
}
finally {
    $resolvedTemp = [System.IO.Path]::GetFullPath($tempRoot)
    $resolvedSystemTemp = [System.IO.Path]::GetFullPath(
        [System.IO.Path]::GetTempPath())
    if (-not $resolvedTemp.StartsWith($resolvedSystemTemp,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected test path: $resolvedTemp"
    }
    Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
}

Write-Host ''
Write-Host ("PACKAGE WORKFLOW COMPATIBILITY: {0}/{1} passed; {2} failed." -f
    $script:Passed, $script:Checks, $script:Failed)
if ($script:Failed -ne 0) {
    exit 1
}
