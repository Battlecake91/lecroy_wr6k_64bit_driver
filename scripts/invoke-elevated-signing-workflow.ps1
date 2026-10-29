<#
.SYNOPSIS
  Invoke one bounded package workflow from an elevated Windows PowerShell 5.1 host.
.DESCRIPTION
  This launcher performs one child-process invocation, captures every child
  output stream, validates the resulting package manifest and never retries.
  Signing and unsigned qualification are explicit, mutually exclusive modes.
  The launcher does not elevate itself, select a certificate or change system
  configuration.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$WorkflowPath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedWorkflowCommit,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverTreeCommit,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Debug', 'Release')][string]$RuntimeConfiguration,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01])/([0-9]{4}),([0-9]+\.){3}[0-9]+$')]
    [string]$DriverVer,
    [switch]$PnpStatusDiagnostics,
    [switch]$WdmV3MamDma,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Sign', 'UnsignedQualification')][string]$Mode,
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$CertificateThumbprint,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][string]$RunLog,
    [Parameter(Mandatory=$true)][string]$AttemptMarker,
    [ValidateRange(1, 86400)][int]$ChildTimeoutSeconds = 3600
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$log = $null
$logCreated = $false

. (Join-Path $PSScriptRoot 'native-process.ps1')

function Get-AbsolutePath {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Label
    )

    if (-not [System.IO.Path]::IsPathRooted($Path)) {
        throw "$Label must be an absolute path: $Path"
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Test-PathWithinRoot {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$Path
    )

    $separator = [System.IO.Path]::DirectorySeparatorChar
    $alternate = [System.IO.Path]::AltDirectorySeparatorChar
    $prefix = $Root.TrimEnd($separator, $alternate)
    if ($prefix.Length -eq 2 -and $prefix[1] -eq ':') {
        $prefix += $separator
    }
    elseif (-not $prefix.EndsWith([string]$separator)) {
        $prefix += $separator
    }
    return $Path.StartsWith($prefix,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function New-ExclusiveTextFile {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][AllowEmptyString()][string]$Text
    )

    $stream = [System.IO.File]::Open(
        $Path, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally {
        $stream.Dispose()
    }
}

function Invoke-LauncherGit {
    param(
        [Parameter(Mandatory=$true)][string]$Repository,
        [Parameter(Mandatory=$true)][string[]]$Arguments
    )

    $git = (Get-Command git.exe -ErrorAction Stop).Source
    $result = Invoke-LecS65NativeProcess -Name 'Launcher Git preflight' `
        -FilePath $git -Arguments (@('-C', $Repository) + $Arguments) `
        -WorkingDirectory $Repository -TimeoutSeconds 60
    return $result.StdOut.Trim()
}

function Assert-PackageManifest {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$ExpectedCommit,
        [Parameter(Mandatory=$true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory=$true)][string]$ExpectedConfiguration,
        [Parameter(Mandatory=$true)][string]$ExpectedDriverVer,
        [Parameter(Mandatory=$true)][bool]$ExpectedPnpStatusDiagnostics,
        [Parameter(Mandatory=$true)][bool]$ExpectedWdmV3MamDma,
        [Parameter(Mandatory=$true)][bool]$ExpectedSigned,
        [string]$ExpectedThumbprint
    )

    $manifestPath = Join-Path $Root 'evidence\manifest.json'
    $manifestHashPath = Join-Path $Root 'evidence\manifest.sha256'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $manifestHashPath -PathType Leaf)) {
        throw 'Signing workflow completed without a manifest and manifest hash.'
    }
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    }
    catch {
        throw "Signing workflow produced malformed manifest JSON: $($_.Exception.Message)"
    }

    $requiredNames = @('LecS65AcqDrv.cat', 'LecS65AcqDrv.inf',
        'LecS65AcqDrv.pdb', 'LecS65AcqDrv.sys')
    $artifactNames = @($manifest.Artifacts | ForEach-Object Name | Sort-Object)
    $expectedState = if ($ExpectedSigned) {
        'SignedCandidate'
    }
    else {
        'UnsignedValidationOnly'
    }
    $identityValid = if ($ExpectedSigned) {
        -not [string]::IsNullOrWhiteSpace($ExpectedThumbprint) -and
        $manifest.PackageIdentity.Signed -eq $true -and
        $null -ne $manifest.PackageIdentity.Certificate -and
        $manifest.PackageIdentity.Certificate.Thumbprint -eq $ExpectedThumbprint -and
        $manifest.PackageIdentity.SysSignatureStatus -eq 'Valid' -and
        $manifest.PackageIdentity.CatSignatureStatus -eq 'Valid'
    }
    else {
        $manifest.PackageIdentity.Signed -eq $false -and
        $null -eq $manifest.PackageIdentity.Certificate -and
        $manifest.PackageIdentity.SysSignatureStatus -eq 'NotSigned' -and
        $manifest.PackageIdentity.CatSignatureStatus -eq 'NotSigned'
    }

    $configurationModes = @($manifest.Build.Configurations |
        ForEach-Object { [bool]$_.PnpStatusDiagnostics } | Select-Object -Unique)
    $dmaModes = @($manifest.Build.Configurations |
        ForEach-Object { [bool]$_.WdmV3MamDma } | Select-Object -Unique)
    $expectedSuiteIds = @(
        'source-abi', 'pre-go-side-effects', 'mdl-only-registration',
        'dma-layout', 'dma-ownership',
        'dma-transaction', 'dma-v3-transfer', 'sg-bridge', 'sg-sync-v3',
        'pnp-publication', 'pnp-irp-lifetime', 'lecdiag-command-chain',
        'no-launch-preparation', 'runtime-lifetime', 'process-dma-lifetime',
        'runtime-baseline', 'frozen-package-signing', 'crash-dump-readiness',
        'system-image-readiness', 'recovery-layout', 'codex-continuation',
        'dma-completion', 'dma-interleavings', 'inten-functional'
    ) | Sort-Object
    $suites = @($manifest.Regression.Suites)
    $actualSuiteIds = @($suites | ForEach-Object Id | Sort-Object)
    $functionalPassed = ($suites | ForEach-Object { [int]$_.Passed } |
        Measure-Object -Sum).Sum
    $functionalFailed = ($suites | ForEach-Object { [int]$_.Failed } |
        Measure-Object -Sum).Sum
    $stressTotal = [int]$manifest.Regression.DefaultDmaStressIterations +
        [int]$manifest.Regression.DefaultIntenStressIterations +
        [int]$manifest.Regression.ExpandedDmaStressIterations +
        [int]$manifest.Regression.ExpandedIntenStressIterations
    if ($manifest.Schema -ne 2 -or
        $manifest.PackageState -ne $expectedState -or
        $manifest.InstallAuthorized -ne $false -or
        $manifest.Source.WorkflowCommit -ne $ExpectedCommit -or
        $manifest.Source.ReviewedDriverTreeCommit -ne $ExpectedSourceCommit -or
        $manifest.Source.DriverTree -notmatch '^[0-9a-fA-F]{40}$' -or
        $manifest.Source.IncludeTree -notmatch '^[0-9a-fA-F]{40}$' -or
        $manifest.Build.RuntimeConfiguration -ne $ExpectedConfiguration -or
        $manifest.Build.CleanRebuild -ne $true -or
        $manifest.Build.Configurations.Count -ne 2 -or
        $manifest.Build.DiagnosticMarker -ne 'PNP_STATUS:' -or
        [bool]$manifest.Build.PnpStatusDiagnostics -ne $ExpectedPnpStatusDiagnostics -or
        $configurationModes.Count -ne 1 -or
        $configurationModes[0] -ne $ExpectedPnpStatusDiagnostics -or
        [bool]$manifest.Build.WdmV3MamDma -ne $ExpectedWdmV3MamDma -or
        $manifest.Build.WdmV3MamDmaMarker -ne 'LECS65_WDM_V3_MAM_DMA_ENABLED' -or
        $dmaModes.Count -ne 1 -or
        $dmaModes[0] -ne $ExpectedWdmV3MamDma -or
        $manifest.Regression.CompleteDry -ne 'PASS' -or
        $manifest.Regression.Expanded -ne 'PASS' -or
        $manifest.Regression.SuiteContract -ne 'batch16-regression-v1' -or
        ($actualSuiteIds -join ',') -ne ($expectedSuiteIds -join ',') -or
        @($actualSuiteIds | Select-Object -Unique).Count -ne $expectedSuiteIds.Count -or
        @($suites | Where-Object {
            $_.Passed -ne $_.Total -or $_.Failed -ne 0 -or $_.Total -lt 1
        }).Count -ne 0 -or
        $manifest.Regression.FunctionalTestsPassed -ne $functionalPassed -or
        $manifest.Regression.FunctionalTestsFailed -ne $functionalFailed -or
        $manifest.Regression.TotalStressIterations -ne $stressTotal -or
        $manifest.Regression.DefaultDmaStressIterations -ne 5000 -or
        $manifest.Regression.DefaultIntenStressIterations -ne 1000 -or
        $manifest.Regression.ExpandedDmaStressIterations -ne 50000 -or
        $manifest.Regression.ExpandedIntenStressIterations -ne 10000 -or
        $manifest.PackageIdentity.DriverVer -ne $ExpectedDriverVer -or
        -not $identityValid -or
        ($artifactNames -join ',') -ne ($requiredNames -join ',') -or
        $manifest.Safety.DriverStoreChanged -ne $false -or
        $manifest.Safety.PnpChanged -ne $false -or
        $manifest.Safety.DeviceOpened -ne $false -or
        $manifest.Safety.SigningPolicyChanged -ne $false) {
        throw 'Package manifest violates the launcher contract.'
    }

    foreach ($artifact in $manifest.Artifacts) {
        if ([System.IO.Path]::IsPathRooted($artifact.RelativePath) -or
            $artifact.RelativePath -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Manifest artifact path is not bounded: $($artifact.RelativePath)"
        }
        $artifactPath = [System.IO.Path]::GetFullPath(
            (Join-Path $Root $artifact.RelativePath))
        if (-not (Test-PathWithinRoot -Root $Root -Path $artifactPath) -or
            -not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
            throw "Manifest artifact is missing or outside the output root: $artifactPath"
        }
        $actualHash = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash
        if ($actualHash -ne $artifact.SHA256) {
            throw "Manifest artifact hash mismatch: $($artifact.Name)"
        }
    }

    $packageNames = @(Get-ChildItem -LiteralPath (Join-Path $Root 'package') -File |
        Sort-Object Name | ForEach-Object Name)
    if (($packageNames -join ',') -ne
        'LecS65AcqDrv.cat,LecS65AcqDrv.inf,LecS65AcqDrv.sys') {
        throw "Unexpected package contents: $($packageNames -join ',')"
    }

    $manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    $hashText = (Get-Content -LiteralPath $manifestHashPath -Raw).Trim()
    if ($hashText -ne ("{0} *manifest.json" -f $manifestHash)) {
        throw 'Manifest hash file does not match manifest.json.'
    }
}

try {
    $principal = [Security.Principal.WindowsPrincipal](
        [Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'The signing launcher requires an elevated token.'
    }
    if ($PSVersionTable.PSVersion.Major -ne 5 -or
        $PSVersionTable.PSVersion.Minor -ne 1) {
        throw "The signing launcher requires Windows PowerShell 5.1; current host is $($PSVersionTable.PSVersion)."
    }
    if (-not [Environment]::Is64BitProcess) {
        throw 'The signing launcher requires 64-bit Windows PowerShell.'
    }
    if ($Mode -eq 'Sign' -and
        [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        throw 'CertificateThumbprint is required in Sign mode.'
    }
    if ($Mode -eq 'UnsignedQualification' -and
        -not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        throw 'CertificateThumbprint is forbidden in UnsignedQualification mode.'
    }

    $repository = Get-AbsolutePath -Path $RepositoryRoot -Label 'RepositoryRoot'
    $workflow = Get-AbsolutePath -Path $WorkflowPath -Label 'WorkflowPath'
    $output = Get-AbsolutePath -Path $OutputRoot -Label 'OutputRoot'
    $log = Get-AbsolutePath -Path $RunLog -Label 'RunLog'
    $marker = Get-AbsolutePath -Path $AttemptMarker -Label 'AttemptMarker'
    $stdoutLog = $log + '.stdout.txt'
    $stderrLog = $log + '.stderr.txt'
    if (-not (Test-Path -LiteralPath $repository -PathType Container)) {
        throw "RepositoryRoot does not exist: $repository"
    }
    if (-not (Test-PathWithinRoot -Root $repository -Path $workflow) -or
        -not (Test-Path -LiteralPath $workflow -PathType Leaf)) {
        throw "WorkflowPath is missing or outside RepositoryRoot: $workflow"
    }
    foreach ($externalEvidencePath in @($log, $stdoutLog, $stderrLog, $marker)) {
        if (Test-PathWithinRoot -Root $repository -Path $externalEvidencePath) {
            throw "Attempt evidence must be outside RepositoryRoot: $externalEvidencePath"
        }
    }
    if (Test-Path -LiteralPath $output) {
        throw "OutputRoot already exists; refusing a duplicate attempt: $output"
    }
    foreach ($newFile in @($log, $stdoutLog, $stderrLog, $marker)) {
        if (Test-Path -LiteralPath $newFile) {
            throw "Attempt evidence path already exists: $newFile"
        }
        $parent = Split-Path -Parent $newFile
        if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
            throw "Attempt evidence parent does not exist: $parent"
        }
    }

    $head = Invoke-LauncherGit -Repository $repository `
        -Arguments @('rev-parse', 'HEAD')
    $status = Invoke-LauncherGit -Repository $repository `
        -Arguments @('status', '--porcelain', '--untracked-files=normal')
    if ($head -ne $ExpectedWorkflowCommit.ToLowerInvariant()) {
        throw "Workflow commit mismatch: expected $ExpectedWorkflowCommit, got $head"
    }
    if (-not [string]::IsNullOrEmpty($status)) {
        throw "Signing worktree is not clean:`n$status"
    }

    $markerText = @(
        'Authorized package-workflow attempt marker'
        "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "WorkflowCommit=$head"
        "Mode=$Mode"
        "PnpStatusDiagnostics=$([bool]$PnpStatusDiagnostics)"
        "WdmV3MamDma=$([bool]$WdmV3MamDma)"
        "OutputRoot=$output"
    ) -join "`r`n"
    if ($Mode -eq 'Sign') {
        $markerText += "`r`nCertificateThumbprint=$($CertificateThumbprint.ToUpperInvariant())"
    }
    New-ExclusiveTextFile -Path $marker -Text ($markerText + "`r`n")

    $childPowerShell = Join-Path $PSHOME 'powershell.exe'
    $childArguments = @(
        '-NoProfile'
        '-NonInteractive'
        '-ExecutionPolicy'
        'Bypass'
        '-File'
        $workflow
        '-ExpectedDriverTreeCommit'
        $ExpectedDriverTreeCommit
        '-RuntimeConfiguration'
        $RuntimeConfiguration
        '-DriverVer'
        $DriverVer
        '-OutputRoot'
        $output
    )
    if ($PnpStatusDiagnostics) {
        $childArguments += '-PnpStatusDiagnostics'
    }
    if ($WdmV3MamDma) {
        $childArguments += '-WdmV3MamDma'
    }
    if ($Mode -eq 'Sign') {
        $childArguments += @(
            '-Sign'
            '-CertificateThumbprint'
            $CertificateThumbprint.ToUpperInvariant()
        )
    }

    $logCreated = $true
    $childResult = Invoke-LecS65NativeProcess `
        -Name "$Mode package workflow" -FilePath $childPowerShell `
        -Arguments $childArguments -WorkingDirectory $repository `
        -LogPath $log -TimeoutSeconds $ChildTimeoutSeconds `
        -ReturnOnNonZeroExit
    $childExitCode = $childResult.ExitCode
    if ($childExitCode -ne 0) {
        [Console]::Error.WriteLine(
            "Package workflow child exited with code $childExitCode. No retry was attempted.")
        exit $childExitCode
    }

    Assert-PackageManifest -Root $output `
        -ExpectedCommit $ExpectedWorkflowCommit.ToLowerInvariant() `
        -ExpectedSourceCommit $ExpectedDriverTreeCommit.ToLowerInvariant() `
        -ExpectedConfiguration $RuntimeConfiguration `
        -ExpectedDriverVer $DriverVer `
        -ExpectedPnpStatusDiagnostics ([bool]$PnpStatusDiagnostics) `
        -ExpectedWdmV3MamDma ([bool]$WdmV3MamDma) `
        -ExpectedSigned ($Mode -eq 'Sign') `
        -ExpectedThumbprint $(if ($Mode -eq 'Sign') {
            $CertificateThumbprint.ToUpperInvariant()
        } else { $null })
    Add-Content -LiteralPath $log -Encoding UTF8 `
        -Value ("ELEVATED PACKAGE LAUNCHER PASS ({0}): manifest and frozen artifact hashes verified." -f $Mode)
    Write-Host ("ELEVATED PACKAGE LAUNCHER PASS ({0})" -f $Mode) `
        -ForegroundColor Green
    exit 0
}
catch {
    if ($logCreated -and $null -ne $log -and
        (Test-Path -LiteralPath $log -PathType Leaf)) {
        try {
            Add-Content -LiteralPath $log -Encoding UTF8 `
                -Value ("LAUNCHER FAILURE: {0}" -f $_.Exception.Message)
        }
        catch { }
    }
    Write-Error $_
    exit 1
}
