function Assert-XStreamHostPreflight {
    param([Parameter(Mandatory=$true)][psobject]$Readback)

    $testSigning = [regex]::Matches([string]$Readback.BcdOutput,
        '(?im)^\s*testsigning[ \t]+([^\r\n]+)')
    if ($Readback.BcdExitCode -ne 0 -or $testSigning.Count -ne 1 -or
        $testSigning[0].Groups[1].Value.Trim() -ine 'Yes') {
        throw 'BCD does not establish testsigning Yes for the current boot entry.'
    }

    # ASCII source works with the legacy Windows PowerShell script decoder.
    $settings = [string]$Readback.VerifierSettingsOutput
    $flags = [regex]::Matches($settings,
        '(?im)^[ \t]*(?:Verifier Flags|\u00dcberpr\u00fcfungskennzeichen):[ \t]*([^\r\n]+)')
    $verified = '(?im)^[ \t]*(?:Verified Drivers|\u00dcberpr\u00fcfte Treiber):'
    if ($Readback.VerifierSettingsExitCode -ne 0 -or
        $Readback.VerifierQueryExitCode -ne 0 -or $flags.Count -ne 1 -or
        $flags[0].Groups[1].Value.Trim() -ne '0x00000000' -or
        [regex]::Matches($settings, $verified).Count -ne 1 -or
        $settings -notmatch ($verified + '\s*(?:None|Keine)\s*\z') -or
        $Readback.VerifierQueryOutput -notmatch '\A\s*No drivers are currently verified\.\s*\z') {
        throw 'Driver Verifier inactivity could not be established from settings and live readback.'
    }
}

function Get-XStreamHostPreflight {
    # Capture native errors in the readback even when a command exits nonzero.
    $ErrorActionPreference = 'Continue'
    $bcd = Join-Path $env:SystemRoot 'System32\bcdedit.exe'
    $verifier = Join-Path $env:SystemRoot 'System32\verifier.exe'
    $bcdOutput = & $bcd /enum '{current}' 2>&1 | Out-String
    $bcdExit = $LASTEXITCODE
    $verifierSettings = & $verifier /querysettings 2>&1 | Out-String
    $settingsExit = $LASTEXITCODE
    $verifierQuery = & $verifier /query 2>&1 | Out-String
    $queryExit = $LASTEXITCODE
    [pscustomobject]@{
        CapturedUtc = [DateTime]::UtcNow.ToString('o')
        BcdOutput = $bcdOutput
        BcdExitCode = $bcdExit
        VerifierSettingsOutput = $verifierSettings
        VerifierSettingsExitCode = $settingsExit
        VerifierQueryOutput = $verifierQuery
        VerifierQueryExitCode = $queryExit
    }
}
