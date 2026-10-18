[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverTreeCommit,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Debug', 'Release')][string]$RuntimeConfiguration,
    [Parameter(Mandatory=$true)][string]$DriverVer,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [switch]$Sign,
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$CertificateThumbprint
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scenario = if ([string]::IsNullOrWhiteSpace($env:LECS65_MOCK_SCENARIO)) {
    'Success'
}
else {
    $env:LECS65_MOCK_SCENARIO
}
if ([string]::IsNullOrWhiteSpace($env:LECS65_MOCK_COUNTER)) {
    throw 'LECS65_MOCK_COUNTER is required.'
}
Add-Content -LiteralPath $env:LECS65_MOCK_COUNTER -Value 'invoked' -Encoding Ascii

if ($scenario -eq 'ChildExitBeforeLogs') {
    exit 80
}
if ($scenario -eq 'ChildTimeout') {
    Start-Sleep -Seconds 30
    exit 82
}
if ($scenario -eq 'DescendantAlive') {
    if ([string]::IsNullOrWhiteSpace($env:LECS65_MOCK_DESCENDANT_PID)) {
        throw 'LECS65_MOCK_DESCENDANT_PID is required.'
    }
    $child = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') `
        -ArgumentList @('-NoProfile', '-NonInteractive', '-Command',
            'Start-Sleep -Seconds 30') -PassThru -WindowStyle Hidden
    Set-Content -LiteralPath $env:LECS65_MOCK_DESCENDANT_PID `
        -Value $child.Id -Encoding Ascii
    [Console]::Error.WriteLine('MOCK descendant remains after child failure')
    exit 81
}

Write-Output "MOCK WORKFLOW START scenario=$scenario"
Write-Output "MOCK WORKING DIRECTORY: $((Get-Location).Path)"
Write-Output "MOCK ARGUMENTS: configuration=$RuntimeConfiguration driverver=$DriverVer signed=$([bool]$Sign) thumbprint=$CertificateThumbprint"
if ($Sign -and [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    throw 'Mock signing requires an explicit thumbprint.'
}
if (-not $Sign -and -not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
    throw 'Mock unsigned mode rejects a thumbprint.'
}
if ($scenario -eq 'NativeToolStderrZero') {
    [Console]::Error.WriteLine('MOCK diagnostic stderr with exit code zero')
}
if ($scenario -eq 'NativeToolFailure') {
    [Console]::Error.WriteLine('MOCK native tool nonzero exit')
    exit 77
}
if ($scenario -eq 'SymChkFailure') {
    [Console]::Error.WriteLine('MOCK SymChk failure')
    exit 78
}
if ($scenario -eq 'MissingPdb') {
    [Console]::Error.WriteLine('MOCK missing PDB')
    exit 79
}
if ($scenario -eq 'UnexpectedTermination') {
    [Console]::Error.WriteLine('MOCK unexpected child-process termination')
    exit 71
}
if ($scenario -eq 'MissingCertificateMetadata') {
    [Console]::Error.WriteLine('MOCK certificate metadata is missing')
    exit 72
}
if (Test-Path -LiteralPath $OutputRoot) {
    throw 'Mock output root already exists.'
}

$package = Join-Path $OutputRoot 'package'
$evidence = Join-Path $OutputRoot 'evidence'
[System.IO.Directory]::CreateDirectory($package) | Out-Null
[System.IO.Directory]::CreateDirectory($evidence) | Out-Null

Write-Output 'MOCK InfVerif before catalog'
if ($scenario -eq 'InfVerifFailure') {
    [Console]::Error.WriteLine('MOCK InfVerif failure')
    exit 73
}
Set-Content -LiteralPath (Join-Path $package 'LecS65AcqDrv.inf') `
    -Value "DriverVer=$DriverVer" -Encoding Ascii
Set-Content -LiteralPath (Join-Path $package 'LecS65AcqDrv.sys') `
    -Value $(if ($Sign) { 'mock signed SYS bytes' } else { 'mock unsigned SYS bytes' }) `
    -Encoding Ascii
Set-Content -LiteralPath (Join-Path $evidence 'LecS65AcqDrv.pdb') `
    -Value 'mock matching PDB bytes' -Encoding Ascii

if ($Sign) {
    Write-Output 'MOCK SignTool sign SYS'
    if ($scenario -eq 'SigningToolFailure') {
        [Console]::Error.WriteLine('MOCK SignTool signing failure')
        exit 74
    }
}
Write-Output 'MOCK Inf2Cat catalog generation'
if ($scenario -eq 'Inf2CatFailure') {
    [Console]::Error.WriteLine('MOCK Inf2Cat failure')
    exit 75
}
Set-Content -LiteralPath (Join-Path $package 'LecS65AcqDrv.cat') `
    -Value $(if ($Sign) { 'mock signed CAT bytes' } else { 'mock unsigned CAT bytes' }) `
    -Encoding Ascii
if ($Sign) { Write-Output 'MOCK SignTool sign CAT' }
Write-Output 'MOCK InfVerif final'
if ($Sign) {
    Write-Output 'MOCK Authenticode verify SYS: Valid'
    Write-Output 'MOCK Authenticode verify CAT: Valid'
    Write-Output 'MOCK catalog membership verify: Valid'
    if ($scenario -eq 'VerificationFailure') {
        [Console]::Error.WriteLine('MOCK final signature verification failure')
        exit 76
    }
}

$head = (& git rev-parse HEAD).Trim()
$artifactPaths = @(
    Join-Path $package 'LecS65AcqDrv.inf'
    Join-Path $package 'LecS65AcqDrv.cat'
    Join-Path $package 'LecS65AcqDrv.sys'
    Join-Path $evidence 'LecS65AcqDrv.pdb'
)
$artifacts = @($artifactPaths | ForEach-Object {
    $item = Get-Item -LiteralPath $_
    [ordered]@{
        Name = $item.Name
        RelativePath = $item.FullName.Substring(
            $OutputRoot.TrimEnd('\').Length + 1)
        Bytes = $item.Length
        SHA256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
})
$manifest = [ordered]@{
    Schema = 1
    PackageState = if ($Sign) { 'SignedCandidate' } else { 'UnsignedValidationOnly' }
    InstallAuthorized = $false
    Source = [ordered]@{
        WorkflowCommit = $head
        ReviewedDriverTreeCommit = $ExpectedDriverTreeCommit.ToLowerInvariant()
        DriverTree = ('D' * 40)
        IncludeTree = ('E' * 40)
    }
    Build = [ordered]@{
        RuntimeConfiguration = $RuntimeConfiguration
        Configurations = @(
            [ordered]@{ Configuration = 'Debug' }
            [ordered]@{ Configuration = 'Release' }
        )
    }
    Regression = [ordered]@{
        CompleteDry = 'PASS'
        Expanded = 'PASS'
        FunctionalTestsPassed = 394
        FunctionalTestsFailed = 0
        TotalStressIterations = 66000
    }
    PackageIdentity = [ordered]@{
        DriverVer = $DriverVer
        Signed = [bool]$Sign
        Certificate = if ($Sign) {
            [ordered]@{
                Thumbprint = $CertificateThumbprint.ToUpperInvariant()
                MockOnly = $true
            }
        } else { $null }
        SysSignatureStatus = if ($Sign) { 'Valid' } else { 'NotSigned' }
        CatSignatureStatus = if ($Sign) { 'Valid' } else { 'NotSigned' }
    }
    Artifacts = $artifacts
    Safety = [ordered]@{
        DriverStoreChanged = $false
        PnpChanged = $false
        DeviceOpened = $false
        SigningPolicyChanged = $false
    }
}
$manifestPath = Join-Path $evidence 'manifest.json'
if ($scenario -eq 'MalformedManifest') {
    Set-Content -LiteralPath $manifestPath -Value '{ malformed' -Encoding UTF8
}
else {
    $manifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $manifestPath -Encoding UTF8
}
if ($scenario -eq 'ManifestMismatch') {
    Add-Content -LiteralPath $manifestPath -Value ' ' -Encoding UTF8
}
$manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
if ($scenario -eq 'ManifestMismatch') {
    $manifestHash = ('0' * 64)
}
Set-Content -LiteralPath (Join-Path $evidence 'manifest.sha256') `
    -Value ("{0} *manifest.json" -f $manifestHash) -Encoding Ascii
Write-Output 'MOCK WORKFLOW PASS'
exit 0
