<#
.SYNOPSIS
  Batch only low-impact LeCroy WR6k native-x64 ABI boundary checks.
.DESCRIPTION
  XStream must be closed. Uses lecdiag built for the currently installed
  replacement driver. DOES NOT build, sign, install, write Dallas memory,
  program FPGA/GPIO registers, or inject any nonzero software IRQ mask.

  The only valid CFDC2400 payload in this batch is four zero bytes.
  Its original derived-subobject virtual hook unconditionally processes
  existing pending IRQ sources synchronously. Thus even mask=0 is not
  described as a passive read.

  CFDC2194 malformed output lengths fail before consuming the latch.
  Its successful 29-byte read/clear is OPTIONAL because it consumes status.
.PARAMETER IncludeErrorStatus
  With XStream closed, additionally execute one consuming 29-byte
  CFDC2194 read/clear using lecdiag error-status.
#>
[CmdletBinding()]
param(
    [switch]$IncludeErrorStatus
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"

$repo = Split-Path -Parent $PSScriptRoot
$diag = Join-Path $repo "tools\lecdiag\build\lecdiag.exe"
if (-not (Test-Path -LiteralPath $diag -PathType Leaf)) {
    throw "lecdiag missing: $diag. Build the x64 diagnostic first."
}

if (Get-Process -Name "XStream" -ErrorAction SilentlyContinue) {
    throw "Close XStream before running this batch. No processes were terminated."
}

$script:CheckCount = 0
$script:PassedCount = 0
$script:FailedCount = 0

function Invoke-LecdiagCheck {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string[]]$Command,
        [Parameter(Mandatory=$true)][int]$ExpectedExit,
        [Parameter(Mandatory=$true)][string]$ExpectedPattern
    )

    $script:CheckCount++
    Write-Host ""
    Write-Host ("[{0:00}] {1}" -f $script:CheckCount, $Name)
    Write-Host ("lecdiag {0}" -f ($Command -join " "))

    # On Windows PowerShell 5.1 an expected negative native test can write
    # stderr as an ErrorRecord. Continue and inspect BOTH output and exit.
    $combined = & $diag @Command 2>&1 | Out-String
    $exitCode = $LASTEXITCODE

    if (-not [string]::IsNullOrWhiteSpace($combined)) {
        Write-Host ($combined.TrimEnd())
    }

    if ($exitCode -eq $ExpectedExit -and
        $combined -match $ExpectedPattern) {
        $script:PassedCount++
        Write-Host "CHECK PASS" -ForegroundColor Green
    }
    else {
        $script:FailedCount++
        Write-Host ("CHECK FAIL: exit={0}, expected={1}, pattern={2}" -f
            $exitCode, $ExpectedExit, $ExpectedPattern) -ForegroundColor Red
    }
}

Write-Host "LeCroy WR6k x64 safe IOCTL batch (XStream closed)" -ForegroundColor Cyan
Write-Host "No nonzero IRQ injection, Dallas writes, FPGA writes or driver reload."
Write-Host "CFDC2400 zero mask still invokes existing pending DPC processing."

Invoke-LecdiagCheck -Name "Legacy driver-build ABI" `
    -Command @("build") -ExpectedExit 0 `
    -ExpectedPattern 'driver build: 1002 \(returned=4\)'

Invoke-LecdiagCheck -Name "Passive PCI identity" `
    -Command @("pci") -ExpectedExit 0 `
    -ExpectedPattern 'vendor/device:\s*1570:0005'

Invoke-LecdiagCheck -Name "START/FVER equals passive BAR0 register read" `
    -Command @("start-register") -ExpectedExit 0 `
    -ExpectedPattern 'PASS: legacy 4-byte output matches physical register read\.'

Invoke-LecdiagCheck -Name "CFDC2400 zero DWORD, no output buffer" `
    -Command @("raw-ioctl", "0xCFDC2400", "00000000", "0") `
    -ExpectedExit 0 `
    -ExpectedPattern 'IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0'

Invoke-LecdiagCheck -Name "CFDC2400 zero DWORD, four-byte output capacity but zero returned" `
    -Command @("raw-ioctl", "0xCFDC2400", "00000000", "4") `
    -ExpectedExit 0 `
    -ExpectedPattern 'IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=4 returned=0'

# These requests are rejected before pending-bit OR / synchronous DPC.
# ERROR_INVALID_PARAMETER == Win32 error 87 for STATUS_INVALID_PARAMETER.
Invoke-LecdiagCheck -Name "CFDC2400 rejects 3-byte input (expected error)" `
    -Command @("raw-ioctl", "0xCFDC2400", "000000", "0") `
    -ExpectedExit 1 `
    -ExpectedPattern 'raw DeviceIoControl failed:\s*87\b'

Invoke-LecdiagCheck -Name "CFDC2400 rejects 5-byte input (expected error)" `
    -Command @("raw-ioctl", "0xCFDC2400", "0000000000", "0") `
    -ExpectedExit 1 `
    -ExpectedPattern 'raw DeviceIoControl failed:\s*87\b'

# Incorrect output sizes are rejected by the CFDC2194 kernel case BEFORE
# the InterlockedExchange that clears/consumes its pending status latch.
# A nonempty one-byte input is used only because lecdiag raw-ioctl's hex
# parser deliberately rejects empty hex strings.
Invoke-LecdiagCheck -Name "CFDC2194 rejects 28-byte output (no latch consumption)" `
    -Command @("raw-ioctl", "0xCFDC2194", "00", "28") `
    -ExpectedExit 1 `
    -ExpectedPattern 'raw DeviceIoControl failed:'

Invoke-LecdiagCheck -Name "CFDC2194 rejects 30-byte output (no latch consumption)" `
    -Command @("raw-ioctl", "0xCFDC2194", "00", "30") `
    -ExpectedExit 1 `
    -ExpectedPattern 'raw DeviceIoControl failed:'

if ($IncludeErrorStatus) {
    Write-Host ""
    Write-Host "NOTE: next optional step CONSUMES the CFDC2194 status latch."
    Invoke-LecdiagCheck -Name "CFDC2194 29-byte read/clear (consuming, explicit option)" `
        -Command @("error-status") -ExpectedExit 0 `
        -ExpectedPattern 'PASS: original 29-byte response layout verified\.'
}

Write-Host ""
Write-Host ("SAFE ABI BATCH: {0}/{1} passed; {2} failed." -f
    $script:PassedCount, $script:CheckCount, $script:FailedCount)

if ($script:FailedCount -ne 0) {
    throw ("Safe ABI batch FAILED: {0} check(s). Do not promote this to a validated baseline." -f
        $script:FailedCount)
}

Write-Host "All requested safe ABI checks passed." -ForegroundColor Green
