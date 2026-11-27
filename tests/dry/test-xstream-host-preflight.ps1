param([string]$RetainedFailurePath, [switch]$CheckLiveVerifier)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'scripts\xstream-host-preflight.ps1')

$germanFlags = [regex]::Unescape('\u00dcberpr\u00fcfungskennzeichen')
$germanDrivers = [regex]::Unescape('\u00dcberpr\u00fcfte Treiber')
$baseline = @{
    BcdExitCode = 0
    BcdOutput = 'testsigning Yes'
    VerifierSettingsExitCode = 0
    VerifierSettingsOutput = "${germanFlags}: 0x00000000`r`n${germanDrivers}:`r`n    Keine"
    VerifierQueryExitCode = 0
    VerifierQueryOutput = 'No drivers are currently verified.'
}
$script:passed = 0
function Check-Readback {
    param([string]$Label, [hashtable]$Override, [bool]$Expected)
    $record = $baseline.Clone()
    foreach ($key in $Override.Keys) { $record[$key] = $Override[$key] }
    $accepted = $true
    try { Assert-XStreamHostPreflight -Readback ([pscustomobject]$record) }
    catch { $accepted = $false }
    if ($accepted -ne $Expected) { throw "Unexpected preflight result: $Label" }
    $script:passed++
    Write-Host "PASS $Label"
}

Check-Readback 'German settings and English live output' @{} $true
Check-Readback 'English settings' @{
    VerifierSettingsOutput = "Verifier Flags: 0x00000000`r`nVerified Drivers:`r`nNone"
} $true
Check-Readback 'BCD access denied' @{ BcdExitCode = 1 } $false
Check-Readback 'Test signing off' @{ BcdOutput = 'testsigning No' } $false
Check-Readback 'Conflicting BCD entries' @{ BcdOutput = "testsigning Yes`r`ntestsigning No" } $false
Check-Readback 'Nonzero settings flags' @{
    VerifierSettingsOutput = $baseline.VerifierSettingsOutput.Replace('0x00000000','0x00000080')
} $false
Check-Readback 'Driver follows None' @{
    VerifierSettingsOutput = $baseline.VerifierSettingsOutput + "`r`nLecS65AcqDrv.sys"
} $false
Check-Readback 'Multiple driver sections' @{
    VerifierSettingsOutput = $baseline.VerifierSettingsOutput + "`r`n${germanDrivers}:`r`nKeine"
} $false
Check-Readback 'Conflicting Verifier flags' @{
    VerifierSettingsOutput = "${germanFlags}: 0x00000080`r`n" + $baseline.VerifierSettingsOutput
} $false
Check-Readback 'Unknown settings output' @{ VerifierSettingsOutput = '' } $false
Check-Readback 'Active or unknown live verification' @{ VerifierQueryOutput = 'LecS65AcqDrv.sys' } $false
Check-Readback 'Failed settings command' @{ VerifierSettingsExitCode = 5 } $false
Check-Readback 'Failed live command' @{ VerifierQueryExitCode = 5 } $false

if ($RetainedFailurePath) {
    $failure = Get-Content -Raw -Encoding UTF8 -LiteralPath $RetainedFailurePath | ConvertFrom-Json
    $outputs = [regex]::Match($failure.Message, '(?s)Settings=(.*) Query=(.*)\z')
    if (-not $outputs.Success) { throw 'The retained failure lacks Verifier output.' }
    Check-Readback 'Retained Verifier text with simulated BCD and exit codes' @{
        VerifierSettingsOutput = $outputs.Groups[1].Value
        VerifierQueryOutput = $outputs.Groups[2].Value
    } $true
}
if ($CheckLiveVerifier) {
    $readback = Get-XStreamHostPreflight
    Check-Readback 'Passive live Verifier output with simulated BCD' @{
        VerifierSettingsOutput = $readback.VerifierSettingsOutput
        VerifierSettingsExitCode = $readback.VerifierSettingsExitCode
        VerifierQueryOutput = $readback.VerifierQueryOutput
        VerifierQueryExitCode = $readback.VerifierQueryExitCode
    } $true
}
Write-Host "XSTREAM HOST PREFLIGHT: $script:passed passed."
