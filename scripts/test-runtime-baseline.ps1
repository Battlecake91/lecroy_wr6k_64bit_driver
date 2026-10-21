<#
.SYNOPSIS
  Validate the passive runtime identity baseline before an authorized batch.
.DESCRIPTION
  Selects the WR6k by exact hardware and instance identity. PCI location/BDF
  is evidence only: a change requires explicit acknowledgement and never
  changes which device is selected. No device interface is opened.
#>
[CmdletBinding()]
param(
    [string]$ExpectedInstanceId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
    [string]$ExpectedHardwareId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00',
    [Parameter(Mandatory=$true)][string]$ExpectedInf,
    [Parameter(Mandatory=$true)][string]$ExpectedVersion,
    [Parameter(Mandatory=$true)][string]$ExpectedSysHash,
    [Parameter(Mandatory=$true)][string]$PreviousLocation,
    [switch]$AcknowledgeLocationChange,
    [string]$MockStatePath,
    [Parameter(Mandatory=$true)][string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$interfaceClasses = @(
    '{958695A4-693A-435E-8297-66F805D8E46A}',
    '{7AC34BE9-F766-4F15-9E88-854BA5E2146E}',
    '{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}',
    '{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}',
    '{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}'
)

function Write-Result([object]$Value) {
    $parent = Split-Path -Parent $OutputPath
    if ($parent -and -not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Output parent does not exist: $parent"
    }
    if (Test-Path -LiteralPath $OutputPath) {
        throw "Output path already exists: $OutputPath"
    }
    $json = $Value | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($OutputPath, $json,
        (New-Object System.Text.UTF8Encoding($false)))
}

function Get-RealState {
    $matches = @()
    foreach ($device in @(Get-PnpDevice -PresentOnly -ErrorAction Stop)) {
        $idProperties = @(Get-PnpDeviceProperty -InstanceId $device.InstanceId `
            -KeyName 'DEVPKEY_Device_HardwareIds' -ErrorAction SilentlyContinue |
            Where-Object { $_.PSObject.Properties.Name -contains 'Data' })
        $hardwareIds = @($idProperties | ForEach-Object { @($_.Data) })
        if ($hardwareIds -contains $ExpectedHardwareId) {
            $matches += $device
        }
    }

    $devices = foreach ($device in $matches) {
        $keys = @(
            'DEVPKEY_Device_HardwareIds', 'DEVPKEY_Device_DriverInfPath',
            'DEVPKEY_Device_DriverVersion', 'DEVPKEY_Device_ProblemCode',
            'DEVPKEY_Device_Service', 'DEVPKEY_Device_LocationInfo'
        )
        $properties = Get-PnpDeviceProperty -InstanceId $device.InstanceId `
            -KeyName $keys -ErrorAction Stop
        $map = @{}
        foreach ($property in $properties) {
            $map[$property.KeyName] = $property.Data
        }
        $serviceName = [string]$map['DEVPKEY_Device_Service']
        $sysPath = Join-Path $env:SystemRoot 'System32\drivers\LecS65AcqDrv.sys'
        $interfaces = foreach ($class in $interfaceClasses) {
            $interfaceOutput = & "$env:SystemRoot\System32\pnputil.exe" `
                /enum-interfaces /class $class 2>&1
            $interfaceExit = $LASTEXITCODE
            $interfaceText = @($interfaceOutput) -join [Environment]::NewLine
            $instanceCount = [regex]::Matches($interfaceText,
                [regex]::Escape($device.InstanceId),
                [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
            $pathCount = [regex]::Matches($interfaceText,
                '(?im)^\s*(Interface path|Schnittstellenpfad)\s*:').Count
            $published = $interfaceExit -eq 0 -and $instanceCount -ge 1 -and
                $pathCount -eq 1 -and
                $interfaceText -match '(?im)^\s*(Interface status|Schnittstellestatus)\s*:\s*(Enabled|Aktiviert)\s*$'
            [pscustomobject]@{ ClassGuid = $class; Published = $published }
        }
        [pscustomobject]@{
            InstanceId = $device.InstanceId
            HardwareIds = @($map['DEVPKEY_Device_HardwareIds'])
            Inf = [string]$map['DEVPKEY_Device_DriverInfPath']
            Version = $map['DEVPKEY_Device_DriverVersion'].ToString()
            ProblemCode = [int]$map['DEVPKEY_Device_ProblemCode']
            DeviceStatus = $device.Status
            ServiceName = $serviceName
            ServiceStatus = (Get-Service -Name $serviceName -ErrorAction Stop).Status.ToString()
            SysHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sysPath).Hash
            Location = [string]$map['DEVPKEY_Device_LocationInfo']
            Interfaces = @($interfaces)
        }
    }
    return [pscustomobject]@{ Devices = @($devices) }
}

$state = if ($MockStatePath) {
    if (-not (Test-Path -LiteralPath $MockStatePath -PathType Leaf)) {
        throw "Mock state is missing: $MockStatePath"
    }
    Get-Content -Raw -LiteralPath $MockStatePath | ConvertFrom-Json
}
else {
    Get-RealState
}

$matching = @($state.Devices | Where-Object {
    @($_.HardwareIds) -contains $ExpectedHardwareId
})
$errors = @()
$review = @()
if ($matching.Count -ne 1) {
    $errors += "Expected exactly one present device with hardware ID $ExpectedHardwareId; found $($matching.Count)."
}

$device = if ($matching.Count -eq 1) { $matching[0] } else { $null }
if ($device) {
    if ($device.InstanceId -ne $ExpectedInstanceId) {
        $errors += "Stable instance ID mismatch: expected $ExpectedInstanceId, got $($device.InstanceId)."
    }
    if ($device.Inf -ne $ExpectedInf) { $errors += "INF mismatch: $($device.Inf)." }
    if ($device.Version -ne $ExpectedVersion) { $errors += "Version mismatch: $($device.Version)." }
    if ([int]$device.ProblemCode -ne 0 -or $device.DeviceStatus -ne 'OK') {
        $errors += "Device is not healthy: status=$($device.DeviceStatus), problem=$($device.ProblemCode)."
    }
    if ($device.ServiceStatus -ne 'Running') {
        $errors += "Service is not running: $($device.ServiceStatus)."
    }
    if ($device.SysHash -ne $ExpectedSysHash.ToUpperInvariant()) {
        $errors += "Loaded SYS hash mismatch: $($device.SysHash)."
    }
    $published = @($device.Interfaces | Where-Object Published)
    if ($published.Count -ne 5) {
        $errors += "Expected five published interface classes; found $($published.Count)."
    }
    if ($device.Location -ne $PreviousLocation) {
        $review += "PCI location changed from '$PreviousLocation' to '$($device.Location)'."
        if (-not $AcknowledgeLocationChange) {
            $errors += 'Location change requires -AcknowledgeLocationChange after stable identity review.'
        }
    }
}

$outcome = if ($errors.Count -eq 0) { 'PASS' } else { 'FAIL' }
$result = [ordered]@{
    Schema = 1
    CapturedUtc = [datetime]::UtcNow.ToString('o')
    Outcome = $outcome
    SelectionBasis = 'ExactHardwareIdAndInstanceId'
    BdfPolicy = 'EvidenceOnlyNeverSelectionKey'
    Device = $device
    Review = @($review)
    Errors = @($errors)
}
Write-Result $result
if ($outcome -ne 'PASS') {
    throw ($errors -join ' ')
}
Write-Host "RUNTIME BASELINE: PASS"
