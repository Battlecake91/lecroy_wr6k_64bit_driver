<#
.SYNOPSIS
  XStream end-to-end regression tests for the WR6k x64 replacement driver.
.DESCRIPTION
  Connects to the local XStream COM automation server and exercises observable
  application-to-driver-to-hardware behavior. Core tests do not require a known
  external signal because Acquire() is allowed to force a trigger on timeout.
  Optional amplitude/frequency assertions require a stable known C1 input.

  Any scope settings changed by this script are restored afterwards.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 5,

    [ValidateRange(0.000001, 1.0e12)]
    [double]$ExpectedFrequencyHz,

    [ValidateRange(0.01, 100.0)]
    [double]$FrequencyTolerancePercent = 10.0,

    [ValidateRange(0.000001, 1.0e6)]
    [double]$ExpectedAmplitudeVpp,

    [ValidateRange(0.01, 100.0)]
    [double]$AmplitudeTolerancePercent = 15.0,

    [switch]$SkipControlChanges
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:Checks = 0
$script:Passed = 0
$script:Failed = 0
$script:Skipped = 0

function Test-E2E {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][scriptblock]$Body
    )

    $script:Checks++
    try {
        & $Body
        $script:Passed++
        Write-Host ("[PASS] " + $Name) -ForegroundColor Green
    }
    catch {
        $script:Failed++
        Write-Host ("[FAIL] {0}: {1}" -f $Name, $_.Exception.Message) -ForegroundColor Red
    }
}

function Skip-E2E {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Reason
    )

    $script:Checks++
    $script:Skipped++
    Write-Host ("[SKIP] {0}: {1}" -f $Name, $Reason) -ForegroundColor Yellow
}

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Near {
    param(
        [double]$Actual,
        [double]$Expected,
        [double]$RelativeTolerance,
        [string]$Label
    )

    $denom = [Math]::Max([Math]::Abs($Expected), 1.0e-30)
    $relative = [Math]::Abs($Actual - $Expected) / $denom
    if ($relative -gt $RelativeTolerance) {
        throw ("{0}: actual={1:R}, expected={2:R}, relative error={3:P3}, tolerance={4:P3}" -f $Label, $Actual, $Expected, $relative, $RelativeTolerance)
    }
}

function Invoke-Acquire {
    param([Parameter(Mandatory=$true)]$Acquisition)
    $null = $Acquisition.Acquire([double]$TimeoutSeconds, 1)
}

function Get-WaveformArray {
    param([Parameter(Mandatory=$true)]$Result)

    try {
        return @($Result.DataArray(-1, -1, 0, 1))
    }
    catch {
        return @($Result.DataArray)
    }
}

function Count-FiniteValues {
    param([Parameter(Mandatory=$true)]$Values)

    $finite = 0
    foreach ($value in $Values) {
        try {
            $number = [double]$value
        }
        catch {
            continue
        }

        if (-not [double]::IsNaN($number) -and -not [double]::IsInfinity($number)) {
            $finite++
        }
    }

    return $finite
}

function Set-And-VerifyNumericControl {
    param(
        [Parameter(Mandatory=$true)]$Object,
        [Parameter(Mandatory=$true)][string]$Property,
        [Parameter(Mandatory=$true)][double]$Candidate
    )

    $Object.$Property = $Candidate
    $readback = [double]$Object.$Property
    $relative = [Math]::Abs($readback - $Candidate) / [Math]::Max([Math]::Abs($Candidate), 1.0e-30)

    if ($relative -gt 0.35) {
        throw ("{0} readback {1:R} is too far from requested {2:R}" -f $Property, $readback, $Candidate)
    }

    return $readback
}

$app = $null
$acq = $null
$c1 = $null
$horizontal = $null
$c1Result = $null
$connectedProgId = $null

try {
    Test-E2E "XStream COM automation connection" {
        $errors = @()

        foreach ($progId in @("LeCroy.XStreamDSO", "LeCroy.XStreamDSO.1")) {
            try {
                $script:app = New-Object -ComObject $progId
                $script:connectedProgId = $progId
                break
            }
            catch {
                $errors += ("{0}: {1}" -f $progId, $_.Exception.Message)
            }
        }

        if ($null -eq $script:app) {
            throw ("Could not create XStream COM server. " + ($errors -join " | "))
        }

        Write-Host ("  connected via {0}" -f $script:connectedProgId)
    }

    if ($null -eq $app) {
        throw "XStream connection failed; remaining E2E tests cannot run."
    }

    Test-E2E "Acquire C1 and Horizontal automation objects" {
        $script:acq = $app.Acquisition
        $script:c1 = $acq.C1
        $script:horizontal = $acq.Horizontal
        $script:c1Result = $c1.Out.Result

        Assert-True ($null -ne $acq) "Acquisition object missing"
        Assert-True ($null -ne $c1) "C1 object missing"
        Assert-True ($null -ne $horizontal) "Acquisition.Horizontal object missing"
        Assert-True ($null -ne $c1Result) "C1.Out.Result missing"
    }

    Test-E2E "Read C1 vertical scale and horizontal scale" {
        $verScale = [double]$c1.VerScale
        $horScale = [double]$horizontal.HorScale

        Assert-True ($verScale -gt 0) "C1.VerScale is not positive"
        Assert-True ($horScale -gt 0) "Horizontal.HorScale is not positive"

        Write-Host ("  C1 VerScale={0:R} V/div, HorScale={1:R} s/div" -f $verScale, $horScale)
    }

    Test-E2E "Forced-trigger acquisition completes" {
        Invoke-Acquire -Acquisition $acq
    }

    Test-E2E "C1 waveform sample count is nonzero" {
        $samples = [int64]$c1Result.Samples
        Assert-True ($samples -gt 0) ("C1 sample count is {0}" -f $samples)
        Write-Host ("  samples={0}" -f $samples)
    }

    Test-E2E "C1 waveform DataArray contains finite samples" {
        $data = Get-WaveformArray -Result $c1Result
        Assert-True ($data.Count -gt 0) "C1 DataArray is empty"

        $finite = Count-FiniteValues -Values $data
        Assert-True ($finite -gt 0) "C1 DataArray contains no finite numeric samples"

        Write-Host ("  DataArray values={0}, finite={1}" -f $data.Count, $finite)
    }

    if ($SkipControlChanges) {
        Skip-E2E "C1 vertical-scale roundtrip" "disabled by -SkipControlChanges"
        Skip-E2E "Horizontal timebase roundtrip" "disabled by -SkipControlChanges"
        Skip-E2E "C1 coupling roundtrip" "disabled by -SkipControlChanges"
        Skip-E2E "C1 bandwidth-limit roundtrip" "disabled by -SkipControlChanges"
    }
    else {
        Test-E2E "C1 vertical-scale roundtrip" {
            $original = [double]$c1.VerScale
            try {
                $candidate = $original * 2.0
                $readback = Set-And-VerifyNumericControl -Object $c1 -Property "VerScale" -Candidate $candidate
                Invoke-Acquire -Acquisition $acq
                Assert-True ([int64]$c1Result.Samples -gt 0) "Waveform invalid after vertical-scale change"
                Write-Host ("  {0:R} -> {1:R} V/div" -f $original, $readback)
            }
            finally {
                $c1.VerScale = $original
            }
        }

        Test-E2E "Horizontal timebase roundtrip" {
            $original = [double]$horizontal.HorScale
            try {
                $candidate = $original * 2.0
                $readback = Set-And-VerifyNumericControl -Object $horizontal -Property "HorScale" -Candidate $candidate
                Invoke-Acquire -Acquisition $acq
                Assert-True ([int64]$c1Result.Samples -gt 0) "Waveform invalid after timebase change"
                Write-Host ("  {0:R} -> {1:R} s/div" -f $original, $readback)
            }
            finally {
                $horizontal.HorScale = $original
            }
        }

        Test-E2E "C1 coupling roundtrip" {
            $original = [string]$c1.Coupling
            $candidate = if ($original -ieq "AC1M") { "DC1M" } else { "AC1M" }

            try {
                $c1.Coupling = $candidate
                $readback = [string]$c1.Coupling
                Assert-True ($readback -ieq $candidate) ("Coupling readback '{0}' != requested '{1}'" -f $readback, $candidate)
                Invoke-Acquire -Acquisition $acq
                Assert-True ([int64]$c1Result.Samples -gt 0) "Waveform invalid after coupling change"
                Write-Host ("  {0} -> {1}" -f $original, $readback)
            }
            finally {
                $c1.Coupling = $original
            }
        }

        Test-E2E "C1 bandwidth-limit roundtrip" {
            $original = [string]$c1.BandwidthLimit
            $candidate = if ($original -ieq "Full") { "20MHz" } else { "Full" }

            try {
                $c1.BandwidthLimit = $candidate
                $readback = [string]$c1.BandwidthLimit
                Assert-True ($readback -ieq $candidate) ("BandwidthLimit readback '{0}' != requested '{1}'" -f $readback, $candidate)
                Invoke-Acquire -Acquisition $acq
                Assert-True ([int64]$c1Result.Samples -gt 0) "Waveform invalid after bandwidth change"
                Write-Host ("  {0} -> {1}" -f $original, $readback)
            }
            finally {
                $c1.BandwidthLimit = $original
            }
        }
    }

    if ($PSBoundParameters.ContainsKey("ExpectedAmplitudeVpp")) {
        Test-E2E "C1 amplitude measurement against expected signal" {
            $p1 = $app.Measure.P1
            $oldView = $p1.View
            $oldEngine = [string]$p1.ParamEngine
            $oldSource = [string]$p1.Source1

            try {
                $p1.View = $true
                $p1.ParamEngine = "AMPL"
                $p1.Source1 = "C1"
                Invoke-Acquire -Acquisition $acq

                $value = [double]$p1.Out.Result.Value
                Assert-True (-not [double]::IsNaN($value) -and -not [double]::IsInfinity($value)) "Amplitude result is invalid"
                Assert-Near -Actual $value -Expected $ExpectedAmplitudeVpp -RelativeTolerance ($AmplitudeTolerancePercent / 100.0) -Label "Amplitude"
                Write-Host ("  amplitude={0:R} V" -f $value)
            }
            finally {
                $p1.ParamEngine = $oldEngine
                $p1.Source1 = $oldSource
                $p1.View = $oldView
            }
        }
    }
    else {
        Skip-E2E "C1 amplitude measurement against expected signal" "no -ExpectedAmplitudeVpp supplied"
    }

    if ($PSBoundParameters.ContainsKey("ExpectedFrequencyHz")) {
        Test-E2E "C1 frequency measurement against expected signal" {
            $p1 = $app.Measure.P1
            $oldView = $p1.View
            $oldEngine = [string]$p1.ParamEngine
            $oldSource = [string]$p1.Source1

            try {
                $p1.View = $true
                $p1.ParamEngine = "FREQ"
                $p1.Source1 = "C1"
                Invoke-Acquire -Acquisition $acq

                $value = [double]$p1.Out.Result.Value
                Assert-True (-not [double]::IsNaN($value) -and -not [double]::IsInfinity($value)) "Frequency result is invalid"
                Assert-Near -Actual $value -Expected $ExpectedFrequencyHz -RelativeTolerance ($FrequencyTolerancePercent / 100.0) -Label "Frequency"
                Write-Host ("  frequency={0:R} Hz" -f $value)
            }
            finally {
                $p1.ParamEngine = $oldEngine
                $p1.Source1 = $oldSource
                $p1.View = $oldView
            }
        }
    }
    else {
        Skip-E2E "C1 frequency measurement against expected signal" "no -ExpectedFrequencyHz supplied"
    }
}
finally {
    foreach ($com in @($c1Result, $c1, $horizontal, $acq, $app)) {
        if ($null -ne $com -and [System.Runtime.InteropServices.Marshal]::IsComObject($com)) {
            try {
                [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($com)
            }
            catch {
            }
        }
    }
}

Write-Host ""
Write-Host ("XSTREAM E2E: {0} passed; {1} failed; {2} skipped; {3} total." -f $script:Passed, $script:Failed, $script:Skipped, $script:Checks)

if ($script:Failed -ne 0) {
    throw ("XStream E2E FAILED: {0} check(s)." -f $script:Failed)
}

Write-Host "XStream E2E core regression passed." -ForegroundColor Green
