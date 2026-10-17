[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedDriverTreeCommit,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Debug', 'Release')][string]$RuntimeConfiguration,
    [Parameter(Mandatory=$true)][string]$DriverVer,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][switch]$Sign,
    [Parameter(Mandatory=$true)]
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

Write-Output "MOCK WORKFLOW START scenario=$scenario"
Write-Output "MOCK WORKING DIRECTORY: $((Get-Location).Path)"
Write-Output "MOCK ARGUMENTS: configuration=$RuntimeConfiguration driverver=$DriverVer thumbprint=$CertificateThumbprint"
if (-not $Sign) { throw 'Mock workflow requires explicit -Sign.' }
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
    -Value 'mock signed SYS bytes' -Encoding Ascii
Set-Content -LiteralPath (Join-Path $evidence 'LecS65AcqDrv.pdb') `
    -Value 'mock matching PDB bytes' -Encoding Ascii

Write-Output 'MOCK SignTool sign SYS'
if ($scenario -eq 'SigningToolFailure') {
    [Console]::Error.WriteLine('MOCK SignTool signing failure')
    exit 74
}
Write-Output 'MOCK Inf2Cat catalog generation'
if ($scenario -eq 'Inf2CatFailure') {
    [Console]::Error.WriteLine('MOCK Inf2Cat failure')
    exit 75
}
Set-Content -LiteralPath (Join-Path $package 'LecS65AcqDrv.cat') `
    -Value 'mock signed CAT bytes' -Encoding Ascii
Write-Output 'MOCK SignTool sign CAT'
Write-Output 'MOCK InfVerif final'
Write-Output 'MOCK Authenticode verify SYS: Valid'
Write-Output 'MOCK Authenticode verify CAT: Valid'
Write-Output 'MOCK catalog membership verify: Valid'
if ($scenario -eq 'VerificationFailure') {
    [Console]::Error.WriteLine('MOCK final signature verification failure')
    exit 76
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
    PackageState = 'SignedCandidate'
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
        Signed = $true
        Certificate = [ordered]@{
            Thumbprint = $CertificateThumbprint.ToUpperInvariant()
            MockOnly = $true
        }
        SysSignatureStatus = 'Valid'
        CatSignatureStatus = 'Valid'
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
$manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $evidence 'manifest.sha256') `
    -Value ("{0} *manifest.json" -f $manifestHash) -Encoding Ascii
Write-Output 'MOCK WORKFLOW PASS'
exit 0
