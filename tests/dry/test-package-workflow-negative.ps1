[CmdletBinding()]
param(
    [string]$DebugSys,
    [string]$ReleasePdb,
    [string]$SymChk = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symchk.exe'
)

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
    'Invoke-GitCapture'
    'Invoke-ExternalCaptured'
    'Assert-DriverVer'
    'New-ExclusiveOutputRoot'
    'Assert-CertificateMetadata'
    'Assert-ManifestComplete'
    'Get-ReviewedSourceIdentity'
    'Get-ToolRecord'
    'Assert-BuildPairPaths'
    'Assert-SysPdbIdentity'
)
foreach ($definition in @($ast.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $functionNames -contains $node.Name
}, $true) | Sort-Object { $_.Extent.StartOffset })) {
    . ([scriptblock]::Create($definition.Extent.Text))
}

$script:Checks = 0
$script:Passed = 0
$script:Failed = 0

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

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    'lecroy-package-negative-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($tempRoot) | Out-Null
try {
    $fixtureRepo = Join-Path $tempRoot 'source-fixture'
    [System.IO.Directory]::CreateDirectory((Join-Path $fixtureRepo 'driver')) | Out-Null
    [System.IO.Directory]::CreateDirectory((Join-Path $fixtureRepo 'include')) | Out-Null
    Set-Content -LiteralPath (Join-Path $fixtureRepo 'driver\fixture.txt') -Value driver
    Set-Content -LiteralPath (Join-Path $fixtureRepo 'include\fixture.txt') -Value include
    & git -C $fixtureRepo init --quiet
    & git -C $fixtureRepo config user.email 'offline-test@example.invalid'
    & git -C $fixtureRepo config user.name 'Offline Test'
    & git -C $fixtureRepo add driver include
    & git -C $fixtureRepo commit --quiet -m fixture
    $fixtureCommit = (& git -C $fixtureRepo rev-parse HEAD).Trim()

    Expect-Rejection 'wrong Git commit is rejected' {
        Get-ReviewedSourceIdentity -Repository $fixtureRepo `
            -ExpectedCommit ('0' * 40) | Out-Null
    }

    $dirtyMarker = Join-Path $fixtureRepo 'untracked-marker.txt'
    Set-Content -LiteralPath $dirtyMarker -Value dirty
    Expect-Rejection 'dirty worktree is rejected' {
        Get-ReviewedSourceIdentity -Repository $fixtureRepo `
            -ExpectedCommit $fixtureCommit | Out-Null
    }
    Remove-Item -LiteralPath $dirtyMarker -Force

    Expect-Rejection 'missing pinned WDK tool is rejected' {
        Get-ToolRecord -Name MissingTool `
            -Path (Join-Path $tempRoot 'missing-tool.exe') | Out-Null
    }

    $validMetadata = [pscustomobject]@{
        Thumbprint = 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'
        EkuOids = @('1.3.6.1.5.5.7.3.3')
        HasPrivateKey = $true
        ChainBuild = $true
        NotBefore = (Get-Date).AddDays(-1)
        NotAfter = (Get-Date).AddDays(1)
    }
    Expect-Rejection 'wrong certificate thumbprint is rejected' {
        Assert-CertificateMetadata -Metadata $validMetadata `
            -ExpectedThumbprint 'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB'
    }
    $missingKeyMetadata = $validMetadata.PSObject.Copy()
    $missingKeyMetadata.HasPrivateKey = $false
    Expect-Rejection 'missing private-key association is rejected from mocked metadata' {
        Assert-CertificateMetadata -Metadata $missingKeyMetadata `
            -ExpectedThumbprint $validMetadata.Thumbprint
    }

    Expect-Rejection 'invalid DriverVer syntax is rejected' {
        Assert-DriverVer -Value '2026-10-17,1.2.3'
    }
    Expect-Rejection 'out-of-range DriverVer component is rejected' {
        Assert-DriverVer -Value '10/17/2026,0.2026.1017.65536'
    }
    Expect-Rejection 'future DriverVer date is rejected' {
        Assert-DriverVer -Value '12/31/2099,0.2026.1231.1'
    }

    $existingOutput = Join-Path $tempRoot 'existing-output'
    [System.IO.Directory]::CreateDirectory($existingOutput) | Out-Null
    Expect-Rejection 'existing output directory is rejected without replacement' {
        New-ExclusiveOutputRoot -Path $existingOutput
    }

    Expect-Rejection 'missing SYS/PDB pair is rejected' {
        Assert-BuildPairPaths -SysPath (Join-Path $tempRoot 'missing.sys') `
            -PdbPath (Join-Path $tempRoot 'missing.pdb') -Label Missing
    }

    if ([string]::IsNullOrWhiteSpace($DebugSys) -or
        [string]::IsNullOrWhiteSpace($ReleasePdb)) {
        throw 'DebugSys and ReleasePdb are required for the mismatched identity test.'
    }
    Expect-Rejection 'incorrect SYS/PDB identity is rejected by SymChk' {
        Assert-SysPdbIdentity -SysPath $DebugSys -PdbPath $ReleasePdb `
            -SymChkPath $SymChk -LogPath (Join-Path $tempRoot 'symchk-mismatch.log') `
            -Label Mismatched
    }

    foreach ($mockFailure in @(
        @{ Name = 'InfVerif failure is fatal'; Code = 21 }
        @{ Name = 'Inf2Cat failure is fatal'; Code = 22 }
        @{ Name = 'mock signing-command failure is fatal'; Code = 23 }
        @{ Name = 'invalid catalog membership is fatal'; Code = 24 }
    )) {
        Expect-Rejection $mockFailure.Name {
            Invoke-ExternalCaptured -Name $mockFailure.Name `
                -FilePath $env:ComSpec `
                -Arguments @('/d', '/c', "exit $($mockFailure.Code)") `
                -LogPath (Join-Path $tempRoot ("mock-{0}.log" -f $mockFailure.Code))
        }
    }

    $incompleteManifest = [pscustomobject]@{
        Schema = 2
        Source = [pscustomobject]@{ WorkflowCommit = ''; DriverTree = ''; IncludeTree = '' }
        Build = [pscustomobject]@{ PnpStatusDiagnostics = $false; Configurations = @() }
        Regression = [pscustomobject]@{ FunctionalTestsPassed = 0; TotalStressIterations = 0 }
        Artifacts = @()
        InstallAuthorized = $false
        Safety = [pscustomobject]@{
            DriverStoreChanged = $false
            PnpChanged = $false
            DeviceOpened = $false
        }
    }
    Expect-Rejection 'incomplete manifest is rejected' {
        Assert-ManifestComplete -Manifest $incompleteManifest
    }
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

Write-Host ""
Write-Host ("PACKAGE NEGATIVE TESTS: {0}/{1} passed; {2} failed." -f
    $script:Passed, $script:Checks, $script:Failed)
if ($script:Failed -ne 0) {
    exit 1
}
