[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Join-Path $repo 'scripts\sign-frozen-driver-package.ps1'
$text = Get-Content -Raw -LiteralPath $script
$passed = 0
$failed = 0

function Test-Condition([string]$Name, [bool]$Condition) {
    if ($Condition) { ++$script:passed; Write-Host "[PASS] $Name" }
    else { ++$script:failed; Write-Host "[FAIL] $Name" }
}

Test-Condition 'sign mode requires explicit acknowledgement' (
    $text -match "Sign mode requires -SignAcknowledged")
Test-Condition 'sign mode requires elevated 64-bit PowerShell' (
    $text -match 'elevated 64-bit Windows PowerShell')
Test-Condition 'input artifacts are hash checked before copying' (
    $text -match 'Assert-Hash \$sourceInf' -and
    $text -match 'Assert-Hash \$sourceCat' -and
    $text -match 'Assert-Hash \$sourceSys' -and
    $text -match 'Assert-Hash \$sourcePdb' -and
    $text -match 'Assert-Hash \$sourceManifest')
Test-Condition 'frozen source is rechecked after signing' (
    $text -match "Frozen SYS after signing" -and
    $text -match "Frozen manifest after signing")
Test-Condition 'only copied SYS and generated CAT are signed' (
    ([regex]::Matches($text,"'SYS signing'").Count -eq 1) -and
    ([regex]::Matches($text,"'CAT signing'").Count -eq 1))
Test-Condition 'catalog is generated after SYS signing' (
    $text.IndexOf("'SYS signing'") -lt $text.IndexOf("'Catalog generation'") -and
    $text.IndexOf("'Catalog generation'") -lt $text.IndexOf("'CAT signing'"))
Test-Condition 'catalog membership and SYS/PDB identity are mandatory' (
    $text -match 'Catalog membership verification' -and
    $text -match 'Signed SYS/PDB identity')
Test-Condition 'Microsoft kernel policy success is rejected' (
    $text -match 'unexpectedly passed Microsoft kernel policy')
Test-Condition 'workflow cannot install or invoke PnPUtil' (
    $text -notmatch '(?i)pnputil|add-driver|/install|DeviceIoControl')
Test-Condition 'manifest records no rebuild and no runtime mutation' (
    $text -match 'RebuildPerformed = \$false' -and
    $text -match 'DriverStoreChanged=\$false' -and
    $text -match 'PnpChanged=\$false')
Test-Condition 'manifest remains compatible with the qualified install launcher' (
    $text -match "Schema = 2" -and
    $text -match "PackageState = 'SignedCandidate'" -and
    $text -match 'ReviewedDriverTreeCommit' -and
    $text -match 'PackageIdentity = \[ordered\]')

Write-Host ("FROZEN PACKAGE SIGNING CONTRACTS: {0}/{1} passed; {2} failed." -f
    $passed,($passed+$failed),$failed)
if ($failed) { throw 'Frozen package signing contract tests failed.' }
