<#
.SYNOPSIS
  Observe one automatically acquired XStream waveform without invoking Acquire.
.DESCRIPTION
  RunAuthorized creates the pinned XStream COM local server exactly once, then
  observes its existing C1 result. It never invokes Acquisition.Acquire, changes
  a CVar, retries, reloads the driver or performs a second application launch.
  OfflineMock exercises the same evidence and authorization contract without
  starting XStream or opening a device.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('OfflineMock','RunAuthorized')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [Parameter(Mandatory=$true)][string]$XStreamExecutablePath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedXStreamExecutableHash,
    [Parameter(Mandatory=$true)][string]$ExpectedInf,
    [Parameter(Mandatory=$true)][string]$ExpectedVersion,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedSysHash,
    [string]$DeviceInstanceId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
    [string]$ExpectedDiskModel = 'HFM256GDHTNG-8510B',
    [string]$ExpectedDiskSerial = 'ACE4_2E00_9590_A2AA.',
    [ValidateRange(30,600)][int]$TimeoutSeconds = 240,
    [switch]$OwnerAuthorized,
    [switch]$PhysicalDiskDisconnectedConfirmed,
    [string]$MockStatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:LaunchCount = 0
$script:ExplicitAcquireCount = 0
$script:EvidenceInitialized = $false
$script:ResultPath = $null
$script:ExceptionPath = $null

function New-ExclusiveTextFile {
    param([string]$Path, [string]$Text)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally { $stream.Dispose() }
}

function Write-JsonExclusive {
    param([string]$Path, [object]$Value)
    New-ExclusiveTextFile -Path $Path `
        -Text (($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine)
}

function Get-RealState {
    $keys = @('DEVPKEY_Device_DriverInfPath',
        'DEVPKEY_Device_DriverVersion', 'DEVPKEY_Device_ProblemCode',
        'DEVPKEY_Device_Service')
    $properties = Get-PnpDeviceProperty -InstanceId $DeviceInstanceId `
        -KeyName $keys -ErrorAction Stop
    $map = @{}
    foreach ($property in $properties) { $map[$property.KeyName] = $property.Data }
    $serviceName = [string]$map['DEVPKEY_Device_Service']
    $sysPath = Join-Path $env:SystemRoot 'System32\drivers\LecS65AcqDrv.sys'
    $disks = @(Get-CimInstance Win32_DiskDrive | ForEach-Object {
        [pscustomobject]@{
            Model = ([string]$_.Model).Trim()
            Serial = ([string]$_.SerialNumber).Trim()
            Size = [uint64]$_.Size
            Status = [string]$_.Status
        }
    })
    $clients = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -in @('XStream.exe','lecroyxstreamdso.exe','lecdiag.exe',
            'lecwatch.exe','Acquisition.exe','WaveStudio.exe','ScopeExplorer.exe')
    } | Select-Object ProcessId,Name,ExecutablePath)
    return [pscustomobject]@{
        Driver = [pscustomobject]@{
            Inf = [string]$map['DEVPKEY_Device_DriverInfPath']
            Version = $map['DEVPKEY_Device_DriverVersion'].ToString()
            ProblemCode = [int]$map['DEVPKEY_Device_ProblemCode']
            DeviceStatus = (Get-PnpDevice -InstanceId $DeviceInstanceId).Status
            ServiceStatus = (Get-Service -Name $serviceName).Status.ToString()
            SysHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sysPath).Hash
        }
        Disks = $disks
        AcquisitionClients = $clients
    }
}

function Assert-Preflight {
    param([object]$State)
    if ($State.Driver.Inf -ne $ExpectedInf -or
        $State.Driver.Version -ne $ExpectedVersion -or
        [int]$State.Driver.ProblemCode -ne 0 -or
        $State.Driver.DeviceStatus -ne 'OK' -or
        $State.Driver.ServiceStatus -ne 'Running' -or
        $State.Driver.SysHash -ne $ExpectedSysHash.ToUpperInvariant()) {
        throw 'Installed driver identity or health does not match the approved baseline.'
    }
    if (@($State.AcquisitionClients).Count -ne 0) {
        throw 'An acquisition client is already running.'
    }
    $disks = @($State.Disks)
    if ($disks.Count -ne 1 -or
        $disks[0].Model -ne $ExpectedDiskModel -or
        $disks[0].Serial -ne $ExpectedDiskSerial -or
        $disks[0].Status -ne 'OK') {
        throw 'Passive disk inventory does not match the approved x64-only baseline.'
    }
}

function Get-XStreamObject {
    param([object]$Parent, [string]$Name)
    $errors = @()
    try { return $Parent.Item($Name) }
    catch { $errors += "Item: $($_.Exception.Message)" }
    try { return $Parent.Objects.Item($Name) }
    catch { $errors += "Objects.Item: $($_.Exception.Message)" }
    try { return $Parent.Object.Item($Name) }
    catch { $errors += "Object.Item: $($_.Exception.Message)" }
    throw "Could not resolve XStream object '$Name': $($errors -join ' | ')"
}

function Get-XStreamControlValue {
    param([object]$Object, [string]$Name)
    return $Object.Item($Name).Value
}

function Get-XStreamResult {
    param([object]$Object)
    try { return $Object.Out.Result }
    catch { return (Get-XStreamObject -Parent $Object -Name 'Out').Result }
}

function Get-WaveformSummary {
    param([object[]]$Values, [int64]$ReportedSamples)
    $sha = [Security.Cryptography.SHA256]::Create()
    $finite = 0L
    $minimum = [double]::PositiveInfinity
    $maximum = [double]::NegativeInfinity
    $sum = 0.0
    try {
        foreach ($value in $Values) {
            $number = [double]$value
            $bytes = [BitConverter]::GetBytes($number)
            [void]$sha.TransformBlock($bytes, 0, $bytes.Length, $bytes, 0)
            if (-not [double]::IsNaN($number) -and
                -not [double]::IsInfinity($number)) {
                $finite++
                if ($number -lt $minimum) { $minimum = $number }
                if ($number -gt $maximum) { $maximum = $number }
                $sum += $number
            }
        }
        [void]$sha.TransformFinalBlock((New-Object byte[] 0), 0, 0)
        return [ordered]@{
            Observed = ($ReportedSamples -gt 0 -and $Values.Count -gt 0 -and $finite -gt 0)
            ReportedSamples = $ReportedSamples
            ReturnedValues = $Values.Count
            FiniteValues = $finite
            Minimum = if ($finite -gt 0) { $minimum } else { $null }
            Maximum = if ($finite -gt 0) { $maximum } else { $null }
            Mean = if ($finite -gt 0) { $sum / $finite } else { $null }
            PayloadDoubleSha256 = ([BitConverter]::ToString($sha.Hash)).Replace('-','')
        }
    }
    finally { $sha.Dispose() }
}

try {
    if (-not [IO.Path]::IsPathRooted($EvidenceRoot)) {
        throw 'EvidenceRoot must be absolute.'
    }
    if (Test-Path -LiteralPath $EvidenceRoot) {
        throw "EvidenceRoot already exists: $EvidenceRoot"
    }
    [IO.Directory]::CreateDirectory($EvidenceRoot) | Out-Null
    $script:EvidenceInitialized = $true
    $script:ResultPath = Join-Path $EvidenceRoot 'result.json'
    $script:ExceptionPath = Join-Path $EvidenceRoot 'exception.txt'

    if (-not [IO.Path]::IsPathRooted($XStreamExecutablePath) -or
        -not (Test-Path -LiteralPath $XStreamExecutablePath -PathType Leaf)) {
        throw 'Pinned XStream executable is missing or not absolute.'
    }
    $xstreamHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $XStreamExecutablePath).Hash
    if ($xstreamHash -ne $ExpectedXStreamExecutableHash.ToUpperInvariant()) {
        throw 'Pinned XStream executable hash mismatch.'
    }
    if (-not $OwnerAuthorized -or -not $PhysicalDiskDisconnectedConfirmed) {
        throw 'Explicit owner authorization and physical disk isolation confirmation are required.'
    }
    if ($Mode -eq 'RunAuthorized' -and
        ($PSVersionTable.PSVersion.Major -ne 5 -or
         $PSVersionTable.PSVersion.Minor -ne 1 -or
         -not [Environment]::Is64BitProcess)) {
        throw 'RunAuthorized requires 64-bit Windows PowerShell 5.1.'
    }
    if ($Mode -eq 'OfflineMock' -and
        [string]::IsNullOrWhiteSpace($MockStatePath)) {
        throw 'OfflineMock requires MockStatePath.'
    }
    $state = if ($Mode -eq 'OfflineMock') {
        Get-Content -Raw -LiteralPath $MockStatePath | ConvertFrom-Json
    } else { Get-RealState }
    Assert-Preflight -State $state
    Write-JsonExclusive -Path (Join-Path $EvidenceRoot 'preflight.json') `
        -Value $state

    $marker = [ordered]@{
        Schema = 1
        CreatedUtc = [datetime]::UtcNow.ToString('o')
        OwnerAuthorized = $true
        PhysicalDiskDisconnectedConfirmed = $true
        XStreamExecutableHash = $xstreamHash
        ExplicitAcquirePermitted = $false
        MaximumApplicationLaunches = 1
    }
    Write-JsonExclusive -Path (Join-Path $EvidenceRoot 'xstream-dispatch.json') `
        -Value $marker

    if ($Mode -eq 'OfflineMock') {
        $script:LaunchCount = 1
        if (-not [bool]$state.MockWaveformObserved) {
            throw 'Mock XStream launch produced no waveform.'
        }
        $waveform = [ordered]@{
            Observed = $true
            ReportedSamples = [int64]$state.MockReportedSamples
            ReturnedValues = [int64]$state.MockReturnedValues
            FiniteValues = [int64]$state.MockReturnedValues
            Minimum = -1.0
            Maximum = 1.0
            Mean = 0.0
            PayloadDoubleSha256 = ('A' * 64)
        }
        $configuration = [ordered]@{
            Channel = 'C1'; Trigger = 'ObservedOnly'; Timebase = 0.001
            ExplicitAcquireInvoked = $false
        }
        $closeRequested = $true
        $processRemaining = $false
    }
    else {
        $existingIds = @(Get-Process -Name 'lecroyxstreamdso' `
            -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
        if ($existingIds.Count -ne 0) { throw 'XStream is already running.' }
        $script:LaunchCount++
        $app = $null; $acq = $null; $c1 = $null; $horizontal = $null
        $trigger = $null; $c1Result = $null
        try {
            $app = New-Object -ComObject 'LeCroy.XStreamDSO'
            $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
            $lastError = 'automation model not ready'
            $values = @()
            while ([datetime]::UtcNow -lt $deadline) {
                try {
                    $acq = Get-XStreamObject -Parent $app -Name 'Acquisition'
                    $c1 = Get-XStreamObject -Parent $acq -Name 'C1'
                    $horizontal = Get-XStreamObject -Parent $acq -Name 'Horizontal'
                    $trigger = Get-XStreamObject -Parent $acq -Name 'Trigger'
                    $c1Result = Get-XStreamResult -Object $c1
                    $reported = [int64]$c1Result.Samples
                    if ($reported -gt 0) {
                        $values = @($c1Result.DataArray(-1, -1, 0, 1))
                        if ($values.Count -gt 0) { break }
                    }
                }
                catch { $lastError = $_.Exception.Message }
                Start-Sleep -Milliseconds 250
            }
            if ($values.Count -eq 0) {
                throw "No automatic C1 waveform became observable: $lastError"
            }
            $waveform = Get-WaveformSummary -Values $values `
                -ReportedSamples ([int64]$c1Result.Samples)
            if (-not $waveform.Observed) { throw 'Observed waveform is empty or non-finite.' }
            $configuration = [ordered]@{
                Channel = 'C1'
                VerScale = [double](Get-XStreamControlValue $c1 'VerScale')
                Coupling = [string](Get-XStreamControlValue $c1 'Coupling')
                BandwidthLimit = [string](Get-XStreamControlValue $c1 'BandwidthLimit')
                Timebase = [double](Get-XStreamControlValue $horizontal 'HorScale')
                TriggerSource = [string](Get-XStreamControlValue $trigger 'Source')
                TriggerMode = [string](Get-XStreamControlValue $trigger 'Mode')
                ExplicitAcquireInvoked = $false
            }
        }
        finally {
            foreach ($com in @($c1Result,$trigger,$horizontal,$c1,$acq,$app)) {
                if ($null -ne $com -and
                    [Runtime.InteropServices.Marshal]::IsComObject($com)) {
                    try { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($com) }
                    catch { }
                }
            }
        }
        $newProcesses = @(Get-Process -Name 'lecroyxstreamdso' `
            -ErrorAction SilentlyContinue | Where-Object { $existingIds -notcontains $_.Id })
        $closeRequested = $false
        foreach ($process in $newProcesses) {
            if ($process.CloseMainWindow()) { $closeRequested = $true }
        }
        if ($closeRequested) {
            $closeDeadline = [datetime]::UtcNow.AddSeconds(30)
            while ([datetime]::UtcNow -lt $closeDeadline -and
                @(Get-Process -Name 'lecroyxstreamdso' `
                    -ErrorAction SilentlyContinue).Count -ne 0) {
                Start-Sleep -Milliseconds 250
            }
        }
        $processRemaining = @(Get-Process -Name 'lecroyxstreamdso' `
            -ErrorAction SilentlyContinue).Count -ne 0
    }

    $result = [ordered]@{
        Schema = 1
        Result = 'PASS'
        CompletedUtc = [datetime]::UtcNow.ToString('o')
        XStreamLaunchCount = $script:LaunchCount
        ExplicitAcquireCount = $script:ExplicitAcquireCount
        Configuration = $configuration
        Waveform = $waveform
        OrderlyCloseRequested = $closeRequested
        XStreamProcessRemaining = $processRemaining
    }
    Write-JsonExclusive -Path $script:ResultPath -Value $result
    Write-Host 'XSTREAM SINGLE WAVEFORM: PASS'
    exit 0
}
catch {
    if ($script:EvidenceInitialized) {
        try {
            New-ExclusiveTextFile -Path $script:ExceptionPath `
                -Text (($_ | Format-List * -Force | Out-String))
        } catch { }
        if (-not (Test-Path -LiteralPath $script:ResultPath)) {
            try {
                Write-JsonExclusive -Path $script:ResultPath -Value ([ordered]@{
                    Schema=1; Result='FAIL'; CompletedUtc=[datetime]::UtcNow.ToString('o')
                    Message=$_.Exception.Message; XStreamLaunchCount=$script:LaunchCount
                    ExplicitAcquireCount=$script:ExplicitAcquireCount
                })
            } catch { }
        }
    }
    [Console]::Error.WriteLine("XSTREAM SINGLE WAVEFORM FAILURE: $($_.Exception.Message)")
    exit 1
}
