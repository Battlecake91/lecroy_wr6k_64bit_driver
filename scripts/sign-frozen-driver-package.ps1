<#
.SYNOPSIS
  Promote one hash-frozen unsigned WR6k package to a signed candidate.
.DESCRIPTION
  The script never builds source, installs a package, stages the Driver Store,
  opens a device, or changes PnP state. It verifies an immutable unsigned input,
  copies only INF/SYS/PDB into a new output root, signs the copied SYS, generates
  a matching catalog, signs the catalog, and freezes an independent manifest.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('ValidateSource','Sign')]
    [string]$Mode,

    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [string]$OutputRoot,

    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedInfHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedCatHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSysHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedPdbHash,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedManifestHash,

    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverTree,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedIncludeTree,
    [Parameter(Mandatory=$true)][string]$ExpectedDriverVer,

    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$CertificateThumbprint,
    [switch]$SignAcknowledged
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'native-process.ps1')

$repo = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding($false)
$tools = [ordered]@{
    InfVerif = 'C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\infverif.exe'
    Inf2Cat = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x86\Inf2Cat.exe'
    SignTool = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe'
    SymChk = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symchk.exe'
}

function Get-Hash([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Assert-Hash([string]$Path, [string]$Expected, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }
    $actual = Get-Hash $Path
    if ($actual -ne $Expected.ToUpperInvariant()) {
        throw "$Label hash mismatch: expected $Expected, got $actual."
    }
}

function Get-EkuOids($Certificate) {
    $extension = @($Certificate.Extensions |
        Where-Object { $_.Oid.Value -eq '2.5.29.37' })
    if ($extension.Count -ne 1) { return @() }
    $eku = New-Object `
        System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension(
            $extension[0].RawData, $extension[0].Critical)
    return @($eku.EnhancedKeyUsages | ForEach-Object Value)
}

function Invoke-PinnedTool {
    param([string]$Name, [string]$Path, [string[]]$Arguments,
          [string]$LogPath, [switch]$ReturnOnNonZeroExit)
    return Invoke-LecS65NativeProcess -Name $Name -FilePath $Path `
        -Arguments $Arguments -WorkingDirectory $repo -LogPath $LogPath `
        -TimeoutSeconds 900 -ReturnOnNonZeroExit:$ReturnOnNonZeroExit
}

function Get-ArtifactRecord([string]$Path, [string]$RelativePath) {
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        Name = $item.Name
        RelativePath = $RelativePath.Replace('\','/')
        Bytes = $item.Length
        SHA256 = Get-Hash $Path
        LastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

$source = [IO.Path]::GetFullPath($SourceRoot)
$sourcePackage = Join-Path $source 'package'
$sourceEvidence = Join-Path $source 'evidence'
$sourceInf = Join-Path $sourcePackage 'LecS65AcqDrv.inf'
$sourceCat = Join-Path $sourcePackage 'LecS65AcqDrv.cat'
$sourceSys = Join-Path $sourcePackage 'LecS65AcqDrv.sys'
$sourcePdb = Join-Path $sourceEvidence 'LecS65AcqDrv.pdb'
$sourceManifest = Join-Path $sourceEvidence 'manifest.json'

$expected = [ordered]@{
    INF = $ExpectedInfHash.ToUpperInvariant()
    CAT = $ExpectedCatHash.ToUpperInvariant()
    SYS = $ExpectedSysHash.ToUpperInvariant()
    PDB = $ExpectedPdbHash.ToUpperInvariant()
    Manifest = $ExpectedManifestHash.ToUpperInvariant()
}
Assert-Hash $sourceInf $expected.INF 'Frozen INF'
Assert-Hash $sourceCat $expected.CAT 'Frozen CAT'
Assert-Hash $sourceSys $expected.SYS 'Frozen SYS'
Assert-Hash $sourcePdb $expected.PDB 'Frozen PDB'
Assert-Hash $sourceManifest $expected.Manifest 'Frozen manifest'

$sourceManifestValue = Get-Content -Raw -LiteralPath $sourceManifest |
    ConvertFrom-Json
$release = @($sourceManifestValue.Build.Configurations |
    Where-Object Configuration -eq 'Release')
$markerText = [Text.Encoding]::ASCII.GetString(
    [IO.File]::ReadAllBytes($sourceSys))
if ($sourceManifestValue.Schema -ne 2 -or
    $sourceManifestValue.PackageState -ne 'UnsignedValidationOnly' -or
    $sourceManifestValue.InstallAuthorized -ne $false -or
    $sourceManifestValue.Source.DriverTree -ne $ExpectedDriverTree.ToLowerInvariant() -or
    $sourceManifestValue.Source.IncludeTree -ne $ExpectedIncludeTree.ToLowerInvariant() -or
    $sourceManifestValue.PackageIdentity.DriverVer -ne $ExpectedDriverVer -or
    $sourceManifestValue.PackageIdentity.Signed -ne $false -or
    $sourceManifestValue.Build.SDK_WDK -ne '10.0.28000.0' -or
    $sourceManifestValue.Build.MSVC -ne '14.41.34120' -or
    $sourceManifestValue.Build.RuntimeConfiguration -ne 'Release' -or
    $sourceManifestValue.Build.PnpStatusDiagnostics -ne $true -or
    $release.Count -ne 1 -or
    $release[0].SYS.SHA256 -ne $expected.SYS -or
    $release[0].PDB.SHA256 -ne $expected.PDB -or
    $sourceManifestValue.Regression.FunctionalTestsPassed -ne 397 -or
    $sourceManifestValue.Regression.TotalStressIterations -ne 66000 -or
    -not $markerText.Contains('PNP_STATUS:')) {
    throw 'Frozen source manifest or diagnostic identity is inconsistent.'
}

$sourceSnapshot = [ordered]@{}
foreach ($name in $expected.Keys) { $sourceSnapshot[$name] = $expected[$name] }
$validation = [ordered]@{
    Schema = 1
    CapturedUtc = [datetime]::UtcNow.ToString('o')
    Mode = $Mode
    SourceRoot = $source
    DriverTree = $sourceManifestValue.Source.DriverTree
    IncludeTree = $sourceManifestValue.Source.IncludeTree
    DriverVer = $sourceManifestValue.PackageIdentity.DriverVer
    PnpStatusDiagnostics = $true
    FunctionalTests = 397
    StressIterations = 66000
    SourceHashes = $sourceSnapshot
}
if ($Mode -eq 'ValidateSource') {
    $validation
    return
}

if (-not $SignAcknowledged) { throw 'Sign mode requires -SignAcknowledged.' }
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    throw 'Sign mode requires OutputRoot.'
}
if ([string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    throw 'Sign mode requires CertificateThumbprint.'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator) -or
    -not [Environment]::Is64BitProcess) {
    throw 'Sign mode requires elevated 64-bit Windows PowerShell.'
}

foreach ($tool in $tools.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $tool.Value -PathType Leaf)) {
        throw "Pinned tool is missing: $($tool.Value)"
    }
}
$thumbprint = $CertificateThumbprint.ToUpperInvariant()
$certificates = @(Get-ChildItem -LiteralPath 'Cert:\LocalMachine\My' |
    Where-Object Thumbprint -eq $thumbprint)
if ($certificates.Count -ne 1) {
    throw "Expected exactly one LocalMachine/My certificate for $thumbprint."
}
$certificate = $certificates[0]
$eku = @(Get-EkuOids $certificate)
$chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
try {
    $chain.ChainPolicy.RevocationMode =
        [Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
    $chainBuild = $chain.Build($certificate)
    $chainStatus = @($chain.ChainStatus | ForEach-Object {
        $_.Status.ToString()
    })
}
finally { $chain.Dispose() }
if (-not $certificate.HasPrivateKey -or -not $chainBuild -or
    $eku -notcontains '1.3.6.1.5.5.7.3.3' -or
    (Get-Date) -lt $certificate.NotBefore -or
    (Get-Date) -gt $certificate.NotAfter) {
    throw 'The selected signing certificate is not usable.'
}

$output = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $output) {
    throw "OutputRoot already exists: $output"
}
[IO.Directory]::CreateDirectory($output) | Out-Null
$package = Join-Path $output 'package'
$evidence = Join-Path $output 'evidence'
[IO.Directory]::CreateDirectory($package) | Out-Null
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$packageInf = Join-Path $package 'LecS65AcqDrv.inf'
$packageSys = Join-Path $package 'LecS65AcqDrv.sys'
$packageCat = Join-Path $package 'LecS65AcqDrv.cat'
$packagePdb = Join-Path $evidence 'LecS65AcqDrv.pdb'
$manifestPath = Join-Path $evidence 'manifest.json'
$manifestHashPath = Join-Path $evidence 'manifest.sha256'

Copy-Item -LiteralPath $sourceInf -Destination $packageInf
Copy-Item -LiteralPath $sourceSys -Destination $packageSys
Copy-Item -LiteralPath $sourcePdb -Destination $packagePdb
Copy-Item -LiteralPath $sourceCat `
    -Destination (Join-Path $evidence 'unsigned-source-LecS65AcqDrv.cat')
Copy-Item -LiteralPath $sourceManifest `
    -Destination (Join-Path $evidence 'unsigned-source-manifest.json')
Assert-Hash $packageInf $expected.INF 'Copied INF'
Assert-Hash $packageSys $expected.SYS 'Copied SYS'
Assert-Hash $packagePdb $expected.PDB 'Copied PDB'

$null = Invoke-PinnedTool 'INF validation before signing' $tools.InfVerif `
    @('/v',$packageInf) (Join-Path $evidence 'infverif-before-signing.log')
$null = Invoke-PinnedTool 'SYS signing' $tools.SignTool `
    @('sign','/fd','SHA256','/sm','/s','My','/sha1',$thumbprint,$packageSys) `
    (Join-Path $evidence 'signtool-sign-sys.log')
$null = Invoke-PinnedTool 'Catalog generation' $tools.Inf2Cat `
    @("/driver:$package",'/os:10_X64','/verbose') `
    (Join-Path $evidence 'inf2cat.log')
if (-not (Test-Path -LiteralPath $packageCat -PathType Leaf)) {
    throw 'Inf2Cat did not create the candidate catalog.'
}
$null = Invoke-PinnedTool 'CAT signing' $tools.SignTool `
    @('sign','/fd','SHA256','/sm','/s','My','/sha1',$thumbprint,$packageCat) `
    (Join-Path $evidence 'signtool-sign-cat.log')
$null = Invoke-PinnedTool 'Final INF validation' $tools.InfVerif `
    @('/v',$packageInf) (Join-Path $evidence 'infverif-final.log')
$null = Invoke-PinnedTool 'SYS Authenticode verification' $tools.SignTool `
    @('verify','/v','/pa',$packageSys) `
    (Join-Path $evidence 'signtool-verify-sys.log')
$null = Invoke-PinnedTool 'CAT Authenticode verification' $tools.SignTool `
    @('verify','/v','/pa',$packageCat) `
    (Join-Path $evidence 'signtool-verify-cat.log')
$null = Invoke-PinnedTool 'Catalog membership verification' $tools.SignTool `
    @('verify','/v','/pa','/c',$packageCat,$packageSys) `
    (Join-Path $evidence 'signtool-verify-membership.log')
$kernelPolicy = Invoke-PinnedTool 'Expected Microsoft kernel-policy rejection' `
    $tools.SignTool @('verify','/v','/kp',$packageSys) `
    (Join-Path $evidence 'signtool-verify-kp-expected-failure.log') `
    -ReturnOnNonZeroExit
if ($kernelPolicy.ExitCode -eq 0) {
    throw 'Local test certificate unexpectedly passed Microsoft kernel policy.'
}
$null = Invoke-PinnedTool 'Signed SYS/PDB identity' $tools.SymChk `
    @('/v',$packageSys,'/s',$evidence) `
    (Join-Path $evidence 'symchk-sys-pdb.log')

$sysSignature = Get-AuthenticodeSignature -LiteralPath $packageSys
$catSignature = Get-AuthenticodeSignature -LiteralPath $packageCat
if ($sysSignature.Status.ToString() -ne 'Valid' -or
    $catSignature.Status.ToString() -ne 'Valid' -or
    $sysSignature.SignerCertificate.Thumbprint -ne $thumbprint -or
    $catSignature.SignerCertificate.Thumbprint -ne $thumbprint) {
    throw 'Final Authenticode identity is invalid.'
}
if (-not ([Text.Encoding]::ASCII.GetString(
        [IO.File]::ReadAllBytes($packageSys))).Contains('PNP_STATUS:')) {
    throw 'Signed SYS lost the required diagnostic marker.'
}
Assert-Hash $sourceInf $expected.INF 'Frozen INF after signing'
Assert-Hash $sourceCat $expected.CAT 'Frozen CAT after signing'
Assert-Hash $sourceSys $expected.SYS 'Frozen SYS after signing'
Assert-Hash $sourcePdb $expected.PDB 'Frozen PDB after signing'
Assert-Hash $sourceManifest $expected.Manifest 'Frozen manifest after signing'

$toolRecords = @($tools.GetEnumerator() | ForEach-Object {
    $item = Get-Item -LiteralPath $_.Value
    [ordered]@{Name=$_.Key;Path=$_.Value;FileVersion=$item.VersionInfo.FileVersion;
        ProductVersion=$item.VersionInfo.ProductVersion;SHA256=Get-Hash $_.Value}
})
$manifest = [ordered]@{
    Schema = 1
    CreatedUtc = [datetime]::UtcNow.ToString('o')
    PackageState = 'SignedFrozenPromotion'
    InstallAuthorized = $false
    Promotion = [ordered]@{
        RebuildPerformed = $false
        SourceRoot = $source
        SourceManifestSHA256 = $expected.Manifest
        UnsignedSourceHashes = $sourceSnapshot
    }
    Source = [ordered]@{
        DriverTree = $ExpectedDriverTree.ToLowerInvariant()
        IncludeTree = $ExpectedIncludeTree.ToLowerInvariant()
        DriverVer = $ExpectedDriverVer
        Configuration = 'Release x64'
        PnpStatusDiagnostics = $true
        SDK_WDK = '10.0.28000.0'
        MSVC = '14.41.34120'
    }
    Certificate = [ordered]@{
        Subject=$certificate.Subject;Issuer=$certificate.Issuer
        Thumbprint=$thumbprint;EKU=$eku;HasPrivateKey=$certificate.HasPrivateKey
        ChainBuild=$chainBuild;ChainStatus=$chainStatus
        NotBeforeUtc=$certificate.NotBefore.ToUniversalTime().ToString('o')
        NotAfterUtc=$certificate.NotAfter.ToUniversalTime().ToString('o')
    }
    Verification = [ordered]@{
        InfVerif='PASS';SysAuthenticode='Valid';CatAuthenticode='Valid'
        CatalogMembership='PASS';LocalChain='PASS'
        MicrosoftKernelPolicy='EXPECTED_FAIL_LOCAL_TEST_CERTIFICATE'
        SysPdbIdentity='PASS';DiagnosticMarker='PNP_STATUS:'
    }
    Artifacts = @(
        Get-ArtifactRecord $packageInf 'package/LecS65AcqDrv.inf'
        Get-ArtifactRecord $packageCat 'package/LecS65AcqDrv.cat'
        Get-ArtifactRecord $packageSys 'package/LecS65AcqDrv.sys'
        Get-ArtifactRecord $packagePdb 'evidence/LecS65AcqDrv.pdb'
    )
    Tools = $toolRecords
    Safety = [ordered]@{
        DriverBuilt=$false;DriverStoreChanged=$false;PnpChanged=$false
        DeviceOpened=$false;SigningPolicyChanged=$false
    }
}
[IO.File]::WriteAllText($manifestPath,
    ($manifest | ConvertTo-Json -Depth 12),$utf8)
$manifestHash = Get-Hash $manifestPath
[IO.File]::WriteAllText($manifestHashPath,
    ($manifestHash + ' *manifest.json' + [Environment]::NewLine),$utf8)
foreach ($path in @($packageInf,$packageCat,$packageSys,$packagePdb,
        $manifestPath,$manifestHashPath)) {
    (Get-Item -LiteralPath $path).IsReadOnly = $true
}

[pscustomobject]@{
    Outcome='PASS';OutputRoot=$output;CertificateThumbprint=$thumbprint
    INF=Get-Hash $packageInf;CAT=Get-Hash $packageCat
    SYS=Get-Hash $packageSys;PDB=Get-Hash $packagePdb
    Manifest=$manifestHash
}
