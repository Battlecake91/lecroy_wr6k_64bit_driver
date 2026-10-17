<#
.SYNOPSIS
  Build, regress and stage a hash-frozen WR6k driver package without installing it.
.DESCRIPTION
  The default mode creates an unsigned validation package. Signing is opt-in and
  requires an exact LocalMachine certificate thumbprint. This script never
  installs a package, changes PnP state or opens a device.
.PARAMETER ExpectedDriverTreeCommit
  Reviewed commit whose driver/ and include/ trees must match the current tree.
.PARAMETER RuntimeConfiguration
  Debug or Release binary to place in the package. Both configurations are built.
.PARAMETER DriverVer
  Explicit INF identity in MM/dd/yyyy,a.b.c.d form. It is never synthesized.
.PARAMETER OutputRoot
  New output directory. The script refuses to overwrite an existing path.
.PARAMETER Sign
  Opt in to SYS/CAT signing. Never use without separate owner authorization.
.PARAMETER CertificateThumbprint
  Exact SHA-1 thumbprint in Cert:\LocalMachine\My. Required with -Sign.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$ExpectedDriverTreeCommit,

    [Parameter(Mandatory=$true)]
    [ValidateSet('Debug', 'Release')]
    [string]$RuntimeConfiguration,

    [Parameter(Mandatory=$true)]
    [ValidatePattern('^(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01])/([0-9]{4}),([0-9]+\.){3}[0-9]+$')]
    [string]$DriverVer,

    [Parameter(Mandatory=$true)]
    [string]$OutputRoot,

    [switch]$Sign,

    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$CertificateThumbprint
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$output = [System.IO.Path]::GetFullPath($OutputRoot)
$packageDir = Join-Path $output 'package'
$evidenceDir = Join-Path $output 'evidence'
$transcriptPath = Join-Path $evidenceDir 'workflow-transcript.txt'
$manifestPath = Join-Path $evidenceDir 'manifest.json'
$manifestHashPath = Join-Path $evidenceDir 'manifest.sha256'

$toolPaths = [ordered]@{
    MSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
    CL = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\cl.exe'
    LINK = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\link.exe'
    DUMPBIN = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\dumpbin.exe'
    InfVerif = 'C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\infverif.exe'
    Inf2Cat = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x86\Inf2Cat.exe'
    SignTool = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe'
    SourceTool = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\srcsrv\srctool.exe'
    SymChk = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symchk.exe'
}

function Invoke-GitCapture {
    param([Parameter(Mandatory=$true)][string[]]$Arguments)

    $result = @(& git @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed:`n$($result -join [Environment]::NewLine)"
    }
    return ($result -join "`n").Trim()
}

function Invoke-ExternalCaptured {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$Arguments,
        [Parameter(Mandatory=$true)][string]$LogPath,
        [switch]$AllowPositiveExitCode
    )

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Name) -ForegroundColor Cyan
    $result = @(& $FilePath @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    $result | Tee-Object -LiteralPath $LogPath | Out-Host
    if ($exitCode -ne 0 -and -not ($AllowPositiveExitCode -and $exitCode -gt 0)) {
        throw "$Name failed with exit code $exitCode."
    }
    return $result
}

function Invoke-ScriptCaptured {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][hashtable]$Arguments,
        [Parameter(Mandatory=$true)][string]$LogPath
    )

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Name) -ForegroundColor Cyan
    $global:LASTEXITCODE = 0
    $result = @(& $FilePath @Arguments 2>&1)
    $invocationSucceeded = $?
    $exitCode = $LASTEXITCODE
    $result | Tee-Object -LiteralPath $LogPath | Out-Host
    if (-not $invocationSucceeded -or $exitCode -ne 0) {
        throw "$Name failed with exit code $exitCode."
    }
    return $result
}

function Assert-DriverVer {
    param([Parameter(Mandatory=$true)][string]$Value)

    if ($Value -notmatch '^(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01])/([0-9]{4}),([0-9]+\.){3}[0-9]+$') {
        throw "Invalid DriverVer syntax: $Value"
    }
    $parts = $Value.Split(',', 2)
    $date = [datetime]::ParseExact(
        $parts[0], 'MM/dd/yyyy',
        [System.Globalization.CultureInfo]::InvariantCulture,
        [System.Globalization.DateTimeStyles]::None)
    $versionParts = @($parts[1].Split('.') | ForEach-Object { [uint32]$_ })
    if ($versionParts.Count -ne 4 -or @($versionParts | Where-Object { $_ -gt 65535 }).Count -ne 0) {
        throw "DriverVer components must be in the range 0..65535: $Value"
    }
    if ($date.Date -gt (Get-Date).ToUniversalTime().Date) {
        throw "DriverVer date is in the future in UTC: $Value"
    }
}

function New-ExclusiveOutputRoot {
    param([Parameter(Mandatory=$true)][string]$Path)

    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }
    try {
        New-Item -ItemType Directory -Path $Path -ErrorAction Stop | Out-Null
    }
    catch {
        throw "OutputRoot already exists or could not be created exclusively: $Path"
    }
}

function Get-CodeSigningEkuOids {
    param([Parameter(Mandatory=$true)]$Certificate)

    $extension = @($Certificate.Extensions |
        Where-Object { $_.Oid.Value -eq '2.5.29.37' })
    if ($extension.Count -ne 1) {
        return @()
    }
    $eku = [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new(
        $extension[0].RawData, $extension[0].Critical)
    return @($eku.EnhancedKeyUsages | ForEach-Object { $_.Value })
}

function Assert-CertificateMetadata {
    param(
        [Parameter(Mandatory=$true)]$Metadata,
        [Parameter(Mandatory=$true)][string]$ExpectedThumbprint
    )

    if ($Metadata.Thumbprint -ne $ExpectedThumbprint.ToUpperInvariant() -or
        $Metadata.EkuOids -notcontains '1.3.6.1.5.5.7.3.3' -or
        -not $Metadata.HasPrivateKey -or
        -not $Metadata.ChainBuild -or
        (Get-Date) -lt $Metadata.NotBefore -or
        (Get-Date) -gt $Metadata.NotAfter) {
        throw 'The selected certificate metadata is not currently usable for local code signing.'
    }
}

function Get-ContainedRelativeFilePath {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$Path
    )

    $rootFull = [System.IO.Path]::GetFullPath($Root)
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    $rootPathRoot = [System.IO.Path]::GetPathRoot($rootFull)
    $pathPathRoot = [System.IO.Path]::GetPathRoot($pathFull)
    if (-not [string]::Equals($rootPathRoot, $pathPathRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is on a different root than the artifact root: $pathFull"
    }
    if (-not (Test-Path -LiteralPath $rootFull -PathType Container)) {
        throw "Artifact root does not exist or is not a directory: $rootFull"
    }
    if (-not (Test-Path -LiteralPath $pathFull -PathType Leaf)) {
        throw "Artifact file does not exist: $pathFull"
    }

    $separator = [System.IO.Path]::DirectorySeparatorChar
    $alternateSeparator = [System.IO.Path]::AltDirectorySeparatorChar
    $rootPrefix = $rootFull.TrimEnd($separator, $alternateSeparator)
    if ($rootPrefix.Length -eq 2 -and $rootPrefix[1] -eq ':') {
        $rootPrefix += $separator
    }
    elseif (-not $rootPrefix.EndsWith([string]$separator)) {
        $rootPrefix += $separator
    }
    if (-not $pathFull.StartsWith($rootPrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Artifact file escapes the intended root: $pathFull"
    }

    $relative = $pathFull.Substring($rootPrefix.Length)
    if ([string]::IsNullOrWhiteSpace($relative) -or
        [System.IO.Path]::IsPathRooted($relative)) {
        throw "Unable to derive a bounded relative artifact path: $pathFull"
    }
    return $relative.Replace($alternateSeparator, $separator)
}

function Assert-PackageSignatureState {
    param(
        [Parameter(Mandatory=$true)][bool]$SigningRequested,
        [Parameter(Mandatory=$true)]$SysSignature,
        [Parameter(Mandatory=$true)]$CatSignature,
        [string]$ExpectedThumbprint
    )

    if ($SigningRequested) {
        if ([string]::IsNullOrWhiteSpace($ExpectedThumbprint) -or
            $SysSignature.Status.ToString() -ne 'Valid' -or
            $CatSignature.Status.ToString() -ne 'Valid' -or
            $null -eq $SysSignature.SignerCertificate -or
            $null -eq $CatSignature.SignerCertificate -or
            $SysSignature.SignerCertificate.Thumbprint -ne $ExpectedThumbprint -or
            $CatSignature.SignerCertificate.Thumbprint -ne $ExpectedThumbprint) {
            throw 'Final SYS/CAT signature verification failed.'
        }
    }
    elseif ($SysSignature.Status.ToString() -ne 'NotSigned' -or
        $CatSignature.Status.ToString() -ne 'NotSigned') {
        throw 'Unsigned validation mode produced an unexpectedly signed artifact.'
    }
}

function Assert-ManifestComplete {
    param([Parameter(Mandatory=$true)]$Manifest)

    $artifactNames = @($Manifest.Artifacts | ForEach-Object Name | Sort-Object)
    $requiredArtifacts = @('LecS65AcqDrv.cat', 'LecS65AcqDrv.inf',
        'LecS65AcqDrv.pdb', 'LecS65AcqDrv.sys')
    if ($Manifest.Schema -ne 1 -or
        [string]::IsNullOrWhiteSpace($Manifest.Source.WorkflowCommit) -or
        [string]::IsNullOrWhiteSpace($Manifest.Source.DriverTree) -or
        [string]::IsNullOrWhiteSpace($Manifest.Source.IncludeTree) -or
        $Manifest.Build.Configurations.Count -ne 2 -or
        $Manifest.Regression.FunctionalTestsPassed -ne 394 -or
        $Manifest.Regression.TotalStressIterations -ne 66000 -or
        ($artifactNames -join ',') -ne ($requiredArtifacts -join ',') -or
        @($Manifest.Artifacts | Where-Object { $_.SHA256 -notmatch '^[0-9A-F]{64}$' }).Count -ne 0 -or
        $Manifest.InstallAuthorized -ne $false -or
        $Manifest.Safety.DriverStoreChanged -ne $false -or
        $Manifest.Safety.PnpChanged -ne $false -or
        $Manifest.Safety.DeviceOpened -ne $false) {
        throw 'Package manifest is incomplete or violates the offline safety contract.'
    }
}

function Get-ReviewedSourceIdentity {
    param(
        [Parameter(Mandatory=$true)][string]$Repository,
        [Parameter(Mandatory=$true)][string]$ExpectedCommit
    )

    $head = (& git -C $Repository rev-parse HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve package-workflow HEAD.' }
    $status = (& git -C $Repository status --porcelain --untracked-files=normal 2>&1 |
        Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to inspect package-workflow status.' }
    if (-not [string]::IsNullOrEmpty($status)) {
        throw "The package worktree is not clean:`n$status"
    }
    & git -C $Repository merge-base --is-ancestor $ExpectedCommit HEAD
    if ($LASTEXITCODE -ne 0) {
        throw "Expected driver-tree commit is not an ancestor of HEAD: $ExpectedCommit"
    }
    $expectedDriverTree = (& git -C $Repository rev-parse "$ExpectedCommit`:driver").Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve expected driver tree.' }
    $expectedIncludeTree = (& git -C $Repository rev-parse "$ExpectedCommit`:include").Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve expected include tree.' }
    $currentDriverTree = (& git -C $Repository rev-parse 'HEAD:driver').Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve current driver tree.' }
    $currentIncludeTree = (& git -C $Repository rev-parse 'HEAD:include').Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve current include tree.' }
    if ($expectedDriverTree -ne $currentDriverTree -or
        $expectedIncludeTree -ne $currentIncludeTree) {
        throw 'Current driver/include trees differ from the reviewed development revision.'
    }
    return [pscustomobject]@{
        Head = $head
        DriverTree = $currentDriverTree
        IncludeTree = $currentIncludeTree
    }
}

function Assert-BuildPairPaths {
    param(
        [Parameter(Mandatory=$true)][string]$SysPath,
        [Parameter(Mandatory=$true)][string]$PdbPath,
        [Parameter(Mandatory=$true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $SysPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $PdbPath -PathType Leaf)) {
        throw "Missing $Label SYS/PDB build output."
    }
}

function Assert-SysPdbIdentity {
    param(
        [Parameter(Mandatory=$true)][string]$SysPath,
        [Parameter(Mandatory=$true)][string]$PdbPath,
        [Parameter(Mandatory=$true)][string]$SymChkPath,
        [Parameter(Mandatory=$true)][string]$LogPath,
        [Parameter(Mandatory=$true)][string]$Label
    )

    $null = Invoke-ExternalCaptured -Name "$Label SYS/PDB identity" `
        -FilePath $SymChkPath `
        -Arguments @('/v', $SysPath, '/s', (Split-Path -Parent $PdbPath)) `
        -LogPath $LogPath
}

function Get-FileRecord {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Root
    )

    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        Name = $item.Name
        RelativePath = Get-ContainedRelativeFilePath -Root $Root -Path $item.FullName
        Bytes = $item.Length
        SHA256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Get-ToolRecord {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required pinned tool is missing: $Path"
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        Name = $Name
        Path = $Path
        FileVersion = $item.VersionInfo.FileVersion
        ProductVersion = $item.VersionInfo.ProductVersion
        SHA256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Get-PeEvidence {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Label
    )

    $log = Join-Path $evidenceDir ("pe-{0}.txt" -f $Label.ToLowerInvariant())
    $lines = Invoke-ExternalCaptured -Name "$Label PE inspection" `
        -FilePath $toolPaths.DUMPBIN `
        -Arguments @('/headers', '/imports', '/exports', $Path) `
        -LogPath $log
    $text = $lines -join "`n"

    if ($text -notmatch '(?im)^\s*8664 machine \(x64\)\s*$') {
        throw "$Label is not AMD64 machine 0x8664."
    }
    if ($text -notmatch '(?im)^\s*1 subsystem \(Native\)\s*$') {
        throw "$Label is not a Native subsystem image."
    }
    if ($text -notmatch '(?im)entry point .*DriverEntry') {
        throw "$Label does not resolve its entry point to DriverEntry."
    }

    $imports = @([regex]::Matches($text, '(?im)^\s+([A-Za-z0-9_.-]+\.(?:dll|exe))\s*$') |
        ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() } |
        Sort-Object -Unique)
    $expectedImports = @('hal.dll', 'ntoskrnl.exe')
    if (($imports -join ',') -ne ($expectedImports -join ',')) {
        throw "$Label imports '$($imports -join ',')', expected only '$($expectedImports -join ',')'."
    }

    $rsds = [regex]::Match($text,
        '(?im)Format: RSDS, \{(?<Guid>[0-9A-F-]{36})\}, (?<Age>[0-9]+), (?<Path>.+\.pdb)\s*$')
    if (-not $rsds.Success) {
        throw "$Label does not contain parseable RSDS/PDB metadata."
    }

    return [ordered]@{
        Machine = '0x8664'
        Subsystem = 'Native'
        EntryPoint = 'DriverEntry'
        Imports = $imports
        RsdsGuid = $rsds.Groups['Guid'].Value.ToUpperInvariant()
        RsdsAge = [int]$rsds.Groups['Age'].Value
        EmbeddedPdbPath = $rsds.Groups['Path'].Value.Trim()
        DumpbinLog = Get-ContainedRelativeFilePath -Root $output -Path $log
    }
}

if ($Sign -and [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    throw '-CertificateThumbprint is required with -Sign.'
}
if (-not $Sign -and -not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    throw '-CertificateThumbprint is accepted only with -Sign.'
}

Assert-DriverVer -Value $DriverVer

if (Test-Path -LiteralPath $output) {
    throw "OutputRoot already exists; refusing to overwrite it: $output"
}

$sourceIdentity = Get-ReviewedSourceIdentity -Repository $repo `
    -ExpectedCommit $ExpectedDriverTreeCommit
$head = $sourceIdentity.Head
$currentDriverTree = $sourceIdentity.DriverTree
$currentIncludeTree = $sourceIdentity.IncludeTree

$tools = @()
foreach ($entry in $toolPaths.GetEnumerator()) {
    $tools += Get-ToolRecord -Name $entry.Key -Path $entry.Value
}
if (($tools | Where-Object Name -eq 'CL').ProductVersion -notlike '14.41.34123*') {
    throw 'Pinned CL is not the approved MSVC 14.41 toolset.'
}
if (($tools | Where-Object Name -eq 'InfVerif').ProductVersion -notlike '10.0.28000.2526*' -or
    ($tools | Where-Object Name -eq 'SignTool').ProductVersion -notlike '10.0.28000.2526*') {
    throw 'Pinned SDK/WDK tools are not the approved 10.0.28000.0 payload.'
}

New-ExclusiveOutputRoot -Path $output
New-Item -ItemType Directory -Path $packageDir | Out-Null
New-Item -ItemType Directory -Path $evidenceDir | Out-Null
Start-Transcript -LiteralPath $transcriptPath | Out-Null

try {
    Write-Host "Source HEAD: $head"
    Write-Host "Reviewed driver-tree commit: $ExpectedDriverTreeCommit"
    Write-Host "Driver tree: $currentDriverTree"
    Write-Host "Include tree: $currentIncludeTree"
    Write-Host "Package identity: DriverVer=$DriverVer"
    Write-Host "Runtime configuration: $RuntimeConfiguration"
    Write-Host ("Signing requested: {0}" -f [bool]$Sign)

    $buildDriver = Join-Path $PSScriptRoot 'build-driver.ps1'
    $dryRunner = Join-Path $PSScriptRoot 'test-driver.ps1'
    $expandedRunner = Join-Path $repo 'tests\dry\test-dma-completion.ps1'
    $dryLog = Join-Path $evidenceDir 'regression-dry.log'
    $expandedLog = Join-Path $evidenceDir 'regression-expanded.log'

    $null = Invoke-ScriptCaptured -Name 'Debug x64 driver and tools build' `
        -FilePath $buildDriver `
        -Arguments @{ Configuration = 'Debug'; BuildLecdiag = $true; BuildLecwatch = $true } `
        -LogPath (Join-Path $evidenceDir 'build-debug.log')
    $null = Invoke-ScriptCaptured -Name 'Release x64 driver build' `
        -FilePath $buildDriver `
        -Arguments @{ Configuration = 'Release' } `
        -LogPath (Join-Path $evidenceDir 'build-release.log')
    $null = Invoke-ScriptCaptured -Name 'Complete Dry regression' `
        -FilePath $dryRunner `
        -Arguments @{ Mode = 'Dry'; SkipBuild = $true } `
        -LogPath $dryLog
    $null = Invoke-ScriptCaptured -Name 'Expanded DMA and INTEN stress' `
        -FilePath $expandedRunner `
        -Arguments @{ StressIterations = 10000 } `
        -LogPath $expandedLog

    $expectedRegressionEvidence = [ordered]@{
        $transcriptPath = @(
            'DRY REGRESSION: 52/52 passed; 0 failed.'
            'REGRESSION SUITE PASS: Dry'
        )
        $dryLog = @(
            'DMA LAYOUT: 13/13 passed; 0 failed.'
            'DMA OWNERSHIP: 17/17 passed; 0 failed.'
            'SG BRIDGE MOCK: 27/27 passed; 0 failed.'
            'SG SYNC V3: 149/149 passed; 0 failed.'
            'PNP PUBLICATION: 38/38 passed; 0 failed.'
            'PNP IRP LIFETIME: 8/8 passed; 0 failed.'
            'DMA COMPLETION: 39/39 passed; 0 failed.'
            'DMA INTERLEAVINGS: 2/2 passed; 0 failed.'
            'INTEN functional: 49 PASS, 0 FAIL, 0 SKIP'
            'DMA STRESS TOTAL: 5000/5000 passed; 0 failed.'
            'INTEN RMW stress: 1000/1000 iterations completed'
        )
        $expandedLog = @(
            'DMA STRESS TOTAL: 50000/50000 passed; 0 failed.'
            'INTEN RMW stress: 10000/10000 iterations completed'
        )
    }
    foreach ($log in $expectedRegressionEvidence.Keys) {
        $logText = Get-Content -LiteralPath $log -Raw
        foreach ($expectedLine in $expectedRegressionEvidence[$log]) {
            if (-not $logText.Contains($expectedLine)) {
                throw "Missing expected regression evidence in $log`: $expectedLine"
            }
        }
    }

    $builtUtilities = @(
        foreach ($utility in @(
            @{ Name = 'lecdiag'; Path = (Join-Path $repo 'tools\lecdiag\build\lecdiag.exe') }
            @{ Name = 'lecwatch'; Path = (Join-Path $repo 'tools\lecwatch\build\lecwatch.exe') }
        )) {
            if (-not (Test-Path -LiteralPath $utility.Path -PathType Leaf)) {
                throw "Missing utility build output: $($utility.Path)"
            }
            [ordered]@{
                Name = $utility.Name
                Path = Get-ContainedRelativeFilePath -Root $repo -Path $utility.Path
                Bytes = (Get-Item -LiteralPath $utility.Path).Length
                SHA256 = (Get-FileHash -LiteralPath $utility.Path -Algorithm SHA256).Hash
                Machine = '0x8664'
            }
        }
    )

    $buildArtifacts = @()
    $peEvidence = [ordered]@{}
    foreach ($configuration in @('Debug', 'Release')) {
        $sys = Join-Path $repo "x64\$configuration\LecS65AcqDrv.sys"
        $pdb = Join-Path $repo "x64\$configuration\LecS65AcqDrv.pdb"
        Assert-BuildPairPaths -SysPath $sys -PdbPath $pdb -Label $configuration
        $pe = Get-PeEvidence -Path $sys -Label $configuration
        if ([System.IO.Path]::GetFileName($pe.EmbeddedPdbPath) -ne
            [System.IO.Path]::GetFileName($pdb)) {
            throw "$configuration SYS does not reference the matching PDB name."
        }
        $pdbLog = Join-Path $evidenceDir ("pdb-{0}-sources.txt" -f $configuration.ToLowerInvariant())
        $pdbOutput = Invoke-ExternalCaptured -Name "$configuration PDB readability" `
            -FilePath $toolPaths.SourceTool -Arguments @('-r', $pdb) -LogPath $pdbLog `
            -AllowPositiveExitCode
        if (($pdbOutput -join "`n") -notmatch '[1-9][0-9]* source files are indexed') {
            throw "$configuration PDB did not report indexed source files."
        }
        $symbolMatchLog = Join-Path $evidenceDir (
            "pdb-{0}-identity.txt" -f $configuration.ToLowerInvariant())
        Assert-SysPdbIdentity -SysPath $sys -PdbPath $pdb `
            -SymChkPath $toolPaths.SymChk -LogPath $symbolMatchLog `
            -Label $configuration
        $buildArtifacts += [ordered]@{
            Configuration = $configuration
            SYS = [ordered]@{
                Bytes = (Get-Item -LiteralPath $sys).Length
                SHA256 = (Get-FileHash -LiteralPath $sys -Algorithm SHA256).Hash
                Signature = (Get-AuthenticodeSignature -LiteralPath $sys).Status.ToString()
            }
            PDB = [ordered]@{
                Bytes = (Get-Item -LiteralPath $pdb).Length
                SHA256 = (Get-FileHash -LiteralPath $pdb -Algorithm SHA256).Hash
                ReadabilityLog = Get-ContainedRelativeFilePath -Root $output -Path $pdbLog
                IdentityLog = Get-ContainedRelativeFilePath -Root $output -Path $symbolMatchLog
            }
        }
        $peEvidence[$configuration] = $pe
    }

    $sourceInf = Join-Path $repo 'driver\LecS65AcqDrv.inf'
    $selectedSys = Join-Path $repo "x64\$RuntimeConfiguration\LecS65AcqDrv.sys"
    $selectedPdb = Join-Path $repo "x64\$RuntimeConfiguration\LecS65AcqDrv.pdb"
    $packageInf = Join-Path $packageDir 'LecS65AcqDrv.inf'
    $packageSys = Join-Path $packageDir 'LecS65AcqDrv.sys'
    $packageCat = Join-Path $packageDir 'LecS65AcqDrv.cat'
    $evidencePdb = Join-Path $evidenceDir 'LecS65AcqDrv.pdb'

    Copy-Item -LiteralPath $selectedSys -Destination $packageSys
    Copy-Item -LiteralPath $selectedPdb -Destination $evidencePdb
    $infText = Get-Content -LiteralPath $sourceInf -Raw
    if ([regex]::Matches($infText, '(?m)^DriverVer=.*$').Count -ne 1) {
        throw 'Source INF must contain exactly one DriverVer line.'
    }
    $infText = [regex]::Replace(
        $infText,
        '(?m)^DriverVer=.*$',
        "DriverVer=$DriverVer")
    Set-Content -LiteralPath $packageInf -Value $infText -Encoding Ascii

    if ([regex]::Matches($infText, '(?m)^CatalogFile=LecS65AcqDrv\.cat\r?$').Count -ne 1 -or
        [regex]::Matches($infText, 'PCI\\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00').Count -ne 1 -or
        [regex]::Matches($infText, '(?m)^StartType=3\r?$').Count -ne 1 -or
        [regex]::Matches($infText, '(?m)^ServiceBinary=%12%\\LecS65AcqDrv\.sys\r?$').Count -ne 1) {
        throw 'Staged INF identity/service contract changed unexpectedly.'
    }

    $null = Invoke-ExternalCaptured -Name 'Staged INF validation' `
        -FilePath $toolPaths.InfVerif -Arguments @('/v', $packageInf) `
        -LogPath (Join-Path $evidenceDir 'infverif-before-catalog.log')

    $preSigningSysHash = (Get-FileHash -LiteralPath $packageSys -Algorithm SHA256).Hash
    $certificateRecord = $null
    $normalizedThumbprint = $null
    if ($Sign) {
        $normalizedThumbprint = $CertificateThumbprint.ToUpperInvariant()
        $certificate = @(Get-ChildItem -LiteralPath 'Cert:\LocalMachine\My' |
            Where-Object Thumbprint -eq $normalizedThumbprint)
        if ($certificate.Count -ne 1) {
            throw "Expected exactly one LocalMachine/My certificate for $normalizedThumbprint."
        }
        $certificate = $certificate[0]
        $eku = @(Get-CodeSigningEkuOids -Certificate $certificate)
        $chain = [System.Security.Cryptography.X509Certificates.X509Chain]::new()
        try {
            $chain.ChainPolicy.RevocationMode =
                [System.Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
            $chainBuild = $chain.Build($certificate)
            $chainStatus = @($chain.ChainStatus | ForEach-Object {
                $_.Status.ToString()
            })
        }
        finally {
            if ($null -ne $chain) { $chain.Dispose() }
        }
        $certificateMetadata = [pscustomobject]@{
            Thumbprint = $certificate.Thumbprint
            EkuOids = $eku
            HasPrivateKey = $certificate.HasPrivateKey
            ChainBuild = $chainBuild
            NotBefore = $certificate.NotBefore
            NotAfter = $certificate.NotAfter
        }
        Assert-CertificateMetadata -Metadata $certificateMetadata `
            -ExpectedThumbprint $normalizedThumbprint
        $certificateRecord = [ordered]@{
            Subject = $certificate.Subject
            Issuer = $certificate.Issuer
            Thumbprint = $certificate.Thumbprint
            NotBeforeUtc = $certificate.NotBefore.ToUniversalTime().ToString('o')
            NotAfterUtc = $certificate.NotAfter.ToUniversalTime().ToString('o')
            EKU = $eku
            HasPrivateKey = $certificate.HasPrivateKey
            ChainBuild = $chainBuild
            ChainStatus = $chainStatus
        }
        $null = Invoke-ExternalCaptured -Name 'SYS signing' `
            -FilePath $toolPaths.SignTool `
            -Arguments @('sign', '/fd', 'SHA256', '/sm', '/s', 'My', '/sha1',
                $normalizedThumbprint, $packageSys) `
            -LogPath (Join-Path $evidenceDir 'signtool-sign-sys.log')
    }

    $postSigningSysHash = (Get-FileHash -LiteralPath $packageSys -Algorithm SHA256).Hash
    $null = Invoke-ExternalCaptured -Name 'Catalog generation' `
        -FilePath $toolPaths.Inf2Cat `
        -Arguments @("/driver:$packageDir", '/os:10_X64', '/verbose') `
        -LogPath (Join-Path $evidenceDir 'inf2cat.log')
    if (-not (Test-Path -LiteralPath $packageCat -PathType Leaf)) {
        throw 'Inf2Cat completed without creating LecS65AcqDrv.cat.'
    }

    if ($Sign) {
        $null = Invoke-ExternalCaptured -Name 'CAT signing' `
            -FilePath $toolPaths.SignTool `
            -Arguments @('sign', '/fd', 'SHA256', '/sm', '/s', 'My', '/sha1',
                $normalizedThumbprint, $packageCat) `
            -LogPath (Join-Path $evidenceDir 'signtool-sign-cat.log')
    }

    $null = Invoke-ExternalCaptured -Name 'Final INF validation' `
        -FilePath $toolPaths.InfVerif -Arguments @('/v', $packageInf) `
        -LogPath (Join-Path $evidenceDir 'infverif-final.log')

    $sysSignature = Get-AuthenticodeSignature -LiteralPath $packageSys
    $catSignature = Get-AuthenticodeSignature -LiteralPath $packageCat
    Assert-PackageSignatureState -SigningRequested ([bool]$Sign) `
        -SysSignature $sysSignature -CatSignature $catSignature `
        -ExpectedThumbprint $normalizedThumbprint
    if ($Sign) {
        $null = Invoke-ExternalCaptured -Name 'SYS Authenticode verification' `
            -FilePath $toolPaths.SignTool -Arguments @('verify', '/v', '/pa', $packageSys) `
            -LogPath (Join-Path $evidenceDir 'signtool-verify-sys.log')
        $null = Invoke-ExternalCaptured -Name 'CAT Authenticode verification' `
            -FilePath $toolPaths.SignTool -Arguments @('verify', '/v', '/pa', $packageCat) `
            -LogPath (Join-Path $evidenceDir 'signtool-verify-cat.log')
        $null = Invoke-ExternalCaptured -Name 'Catalog membership verification' `
            -FilePath $toolPaths.SignTool `
            -Arguments @('verify', '/v', '/pa', '/c', $packageCat, $packageSys) `
            -LogPath (Join-Path $evidenceDir 'signtool-verify-membership.log')
    }

    $packageFiles = @(Get-ChildItem -LiteralPath $packageDir -File |
        Sort-Object Name | ForEach-Object Name)
    if (($packageFiles -join ',') -ne
        'LecS65AcqDrv.cat,LecS65AcqDrv.inf,LecS65AcqDrv.sys') {
        throw "Unexpected package contents: $($packageFiles -join ',')"
    }

    $manifest = [ordered]@{
        Schema = 1
        CreatedUtc = (Get-Date).ToUniversalTime().ToString('o')
        PackageState = if ($Sign) { 'SignedCandidate' } else { 'UnsignedValidationOnly' }
        InstallAuthorized = $false
        Source = [ordered]@{
            WorkflowCommit = $head
            ReviewedDriverTreeCommit = $ExpectedDriverTreeCommit.ToLowerInvariant()
            DriverTree = $currentDriverTree
            IncludeTree = $currentIncludeTree
        }
        Build = [ordered]@{
            SDK_WDK = '10.0.28000.0'
            MSVC = '14.41.34120'
            RuntimeConfiguration = $RuntimeConfiguration
            Configurations = $buildArtifacts
        }
        Regression = [ordered]@{
            CompleteDry = 'PASS'
            Expanded = 'PASS'
            FunctionalTestsPassed = 394
            FunctionalTestsFailed = 0
            DefaultDmaStressIterations = 5000
            DefaultIntenStressIterations = 1000
            ExpandedDmaStressIterations = 50000
            ExpandedIntenStressIterations = 10000
            TotalStressIterations = 66000
            DryLog = 'evidence/regression-dry.log'
            ExpandedLog = 'evidence/regression-expanded.log'
        }
        PackageIdentity = [ordered]@{
            DriverVer = $DriverVer
            HardwareId = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00'
            Catalog = 'LecS65AcqDrv.cat'
            Signed = [bool]$Sign
            Certificate = $certificateRecord
            PreSigningSysSHA256 = $preSigningSysHash
            PostSigningSysSHA256 = $postSigningSysHash
            SysSignatureStatus = $sysSignature.Status.ToString()
            CatSignatureStatus = $catSignature.Status.ToString()
        }
        PE = $peEvidence
        BuiltUtilities = $builtUtilities
        Tools = $tools
        Artifacts = @(
            Get-FileRecord -Path $packageInf -Root $output
            Get-FileRecord -Path $packageCat -Root $output
            Get-FileRecord -Path $packageSys -Root $output
            Get-FileRecord -Path $evidencePdb -Root $output
        )
        Safety = [ordered]@{
            DriverStoreChanged = $false
            PnpChanged = $false
            DeviceOpened = $false
            SigningPolicyChanged = $false
        }
    }
    Assert-ManifestComplete -Manifest $manifest
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    $manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    Set-Content -LiteralPath $manifestHashPath `
        -Value ("{0} *manifest.json" -f $manifestHash) -Encoding Ascii

    Write-Host ""
    Write-Host 'PACKAGE WORKFLOW PASS' -ForegroundColor Green
    Write-Host "  Output: $output"
    Write-Host "  State : $($manifest.PackageState)"
    Write-Host "  Manifest SHA256: $manifestHash"
}
finally {
    try { Stop-Transcript | Out-Null } catch { }
}
