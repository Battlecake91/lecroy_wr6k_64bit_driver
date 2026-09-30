<#
.SYNOPSIS
  Unified LeCroy WR6k replacement-driver regression runner.
.PARAMETER Mode
  Dry      - build plus hardware-independent source/ABI regression checks.
  Hardware - safe real-PCI health/ABI regression checks. XStream must be closed.
  XStream  - XStream COM end-to-end regression against the installed driver.
  All      - Dry, Hardware, then XStream. XStream must be closed before start.
.PARAMETER SkipBuild
  Skip the driver/lecdiag build in Dry/All mode.
.PARAMETER IncludeErrorStatus
  Forward the optional consuming CFDC2194 status check to the hardware suite.
#>
[CmdletBinding()]
param(
    [ValidateSet("Dry", "Hardware", "XStream", "All")]
    [string]$Mode = "Dry",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$SkipBuild,
    [switch]$IncludeErrorStatus,

    [ValidateRange(1, 60)]
    [int]$XStreamTimeoutSeconds = 5,

    [double]$ExpectedFrequencyHz,

    [double]$ExpectedAmplitudeVpp,

    [string]$ExpectedProbeName,

    [switch]$SkipXStreamControlChanges,

    [switch]$TraceXStreamActions
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$drySuite = Join-Path $repo "tests\dry\test-source-contracts.ps1"
$hardwareSuite = Join-Path $PSScriptRoot "test-safe-ioctl-batch.ps1"
$xstreamSuite = Join-Path $repo "tests\xstream\test-xstream-e2e.ps1"

function Invoke-Step {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][scriptblock]$Body
    )

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Name) -ForegroundColor Cyan
    & $Body
}

Write-Host ("LeCroy WR6k regression runner: mode={0}" -f $Mode) -ForegroundColor Cyan

if ($Mode -eq "Dry" -or $Mode -eq "All") {
    if (-not $SkipBuild) {
        Invoke-Step "Build driver and lecdiag (no hardware access)" {
            $buildScript = Join-Path $PSScriptRoot "build-driver.ps1"
            & $buildScript -Configuration $Configuration -BuildLecdiag
        }
    }
    else {
        Write-Host "Build skipped by request."
    }

    Invoke-Step "Dry source/ABI contracts" {
        & $drySuite
    }
}

if ($Mode -eq "Hardware" -or $Mode -eq "All") {
    Invoke-Step "Safe hardware ABI regression" {
        if ($IncludeErrorStatus) {
            & $hardwareSuite -IncludeErrorStatus
        }
        else {
            & $hardwareSuite
        }
    }
}

if ($Mode -eq "XStream" -or $Mode -eq "All") {
    Invoke-Step "XStream end-to-end regression" {
        $args = @{
            TimeoutSeconds = $XStreamTimeoutSeconds
        }

        if ($PSBoundParameters.ContainsKey("ExpectedFrequencyHz")) {
            $args.ExpectedFrequencyHz = $ExpectedFrequencyHz
        }
        if ($PSBoundParameters.ContainsKey("ExpectedAmplitudeVpp")) {
            $args.ExpectedAmplitudeVpp = $ExpectedAmplitudeVpp
        }
        if ($PSBoundParameters.ContainsKey("ExpectedProbeName")) {
            $args.ExpectedProbeName = $ExpectedProbeName
        }
        if ($SkipXStreamControlChanges) {
            $args.SkipControlChanges = $true
        }
        if ($TraceXStreamActions) {
            $args.TraceActions = $true
        }

        & $xstreamSuite @args
    }
}

Write-Host ""
Write-Host ("REGRESSION SUITE PASS: {0}" -f $Mode) -ForegroundColor Green
