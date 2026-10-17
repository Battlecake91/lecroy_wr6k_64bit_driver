<#
.SYNOPSIS
  Invoke one authorized signing workflow from an elevated Windows PowerShell 5.1 host.
.DESCRIPTION
  This launcher performs one child-process invocation, captures every child
  output stream, validates the resulting signed-package manifest and never
  retries. It does not elevate itself, select a certificate or change system
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
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$CertificateThumbprint,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][string]$RunLog,
    [Parameter(Mandatory=$true)][string]$AttemptMarker
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$log = $null
$logCreated = $false

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

function Assert-SignedManifest {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$ExpectedCommit,
        [Parameter(Mandatory=$true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory=$true)][string]$ExpectedConfiguration,
        [Parameter(Mandatory=$true)][string]$ExpectedDriverVer,
        [Parameter(Mandatory=$true)][string]$ExpectedThumbprint
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
    if ($manifest.Schema -ne 1 -or
        $manifest.PackageState -ne 'SignedCandidate' -or
        $manifest.InstallAuthorized -ne $false -or
        $manifest.Source.WorkflowCommit -ne $ExpectedCommit -or
        $manifest.Source.ReviewedDriverTreeCommit -ne $ExpectedSourceCommit -or
        $manifest.Source.DriverTree -notmatch '^[0-9a-fA-F]{40}$' -or
        $manifest.Source.IncludeTree -notmatch '^[0-9a-fA-F]{40}$' -or
        $manifest.Build.RuntimeConfiguration -ne $ExpectedConfiguration -or
        $manifest.Build.Configurations.Count -ne 2 -or
        $manifest.Regression.CompleteDry -ne 'PASS' -or
        $manifest.Regression.Expanded -ne 'PASS' -or
        $manifest.Regression.FunctionalTestsPassed -ne 394 -or
        $manifest.Regression.FunctionalTestsFailed -ne 0 -or
        $manifest.Regression.TotalStressIterations -ne 66000 -or
        $manifest.PackageIdentity.DriverVer -ne $ExpectedDriverVer -or
        $manifest.PackageIdentity.Signed -ne $true -or
        $manifest.PackageIdentity.Certificate.Thumbprint -ne $ExpectedThumbprint -or
        $manifest.PackageIdentity.SysSignatureStatus -ne 'Valid' -or
        $manifest.PackageIdentity.CatSignatureStatus -ne 'Valid' -or
        ($artifactNames -join ',') -ne ($requiredNames -join ',') -or
        $manifest.Safety.DriverStoreChanged -ne $false -or
        $manifest.Safety.PnpChanged -ne $false -or
        $manifest.Safety.DeviceOpened -ne $false -or
        $manifest.Safety.SigningPolicyChanged -ne $false) {
        throw 'Signed package manifest violates the launcher contract.'
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

    $head = (& git -C $repository rev-parse HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve repository HEAD.' }
    $status = (& git -C $repository status --porcelain --untracked-files=normal 2>&1 |
        Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to inspect repository status.' }
    if ($head -ne $ExpectedWorkflowCommit.ToLowerInvariant()) {
        throw "Workflow commit mismatch: expected $ExpectedWorkflowCommit, got $head"
    }
    if (-not [string]::IsNullOrEmpty($status)) {
        throw "Signing worktree is not clean:`n$status"
    }

    $markerText = @(
        'Authorized signing attempt marker'
        "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
        "WorkflowCommit=$head"
        "OutputRoot=$output"
        "CertificateThumbprint=$($CertificateThumbprint.ToUpperInvariant())"
    ) -join "`r`n"
    New-ExclusiveTextFile -Path $marker -Text ($markerText + "`r`n")
    try {
        New-ExclusiveTextFile -Path $log -Text ''
        $logCreated = $true
    }
    catch {
        throw "Unable to create the exclusive run log after marking the attempt: $($_.Exception.Message)"
    }

    $childPowerShell = Join-Path $PSHOME 'powershell.exe'
    if (-not (Test-Path -LiteralPath $childPowerShell -PathType Leaf)) {
        throw "Current-host child PowerShell is missing: $childPowerShell"
    }
    $childArguments = @(
        '-NoProfile'
        '-NonInteractive'
        '-ExecutionPolicy'
        'Bypass'
        '-File'
        ('"{0}"' -f $workflow)
        '-ExpectedDriverTreeCommit'
        $ExpectedDriverTreeCommit
        '-RuntimeConfiguration'
        $RuntimeConfiguration
        '-DriverVer'
        $DriverVer
        '-OutputRoot'
        ('"{0}"' -f $output)
        '-Sign'
        '-CertificateThumbprint'
        $CertificateThumbprint.ToUpperInvariant()
    )

    $childProcess = Start-Process -FilePath $childPowerShell `
        -ArgumentList $childArguments -WorkingDirectory $repository `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog
    $childExitCode = $childProcess.ExitCode
    $stdoutText = Get-Content -LiteralPath $stdoutLog -Raw
    $stderrText = Get-Content -LiteralPath $stderrLog -Raw
    Add-Content -LiteralPath $log -Encoding UTF8 -Value @(
        '=== CHILD STDOUT ==='
        $stdoutText
        '=== CHILD STDERR ==='
        $stderrText
        "=== CHILD EXIT CODE: $childExitCode ==="
    )
    if (-not [string]::IsNullOrEmpty($stdoutText)) {
        Write-Host $stdoutText
    }
    if (-not [string]::IsNullOrEmpty($stderrText)) {
        [Console]::Error.WriteLine($stderrText)
    }
    if ($childExitCode -ne 0) {
        [Console]::Error.WriteLine(
            "Signing workflow child exited with code $childExitCode. No retry was attempted.")
        exit $childExitCode
    }

    Assert-SignedManifest -Root $output `
        -ExpectedCommit $ExpectedWorkflowCommit.ToLowerInvariant() `
        -ExpectedSourceCommit $ExpectedDriverTreeCommit.ToLowerInvariant() `
        -ExpectedConfiguration $RuntimeConfiguration `
        -ExpectedDriverVer $DriverVer `
        -ExpectedThumbprint $CertificateThumbprint.ToUpperInvariant()
    Add-Content -LiteralPath $log -Encoding UTF8 `
        -Value 'ELEVATED SIGNING LAUNCHER PASS: manifest and frozen artifact hashes verified.'
    Write-Host 'ELEVATED SIGNING LAUNCHER PASS' -ForegroundColor Green
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
