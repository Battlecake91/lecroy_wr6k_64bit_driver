<#
.SYNOPSIS
  Hardware-independent regression checks for the replacement driver source tree.
.DESCRIPTION
  This suite does not open a device, load a driver, access PCI hardware or start
  XStream. It verifies reconstructed ABI/source contracts that must remain stable
  while the driver is refactored.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$publicHeader = Join-Path $repo "include\LecS65LegacyIoctl.h"
$driverHeader = Join-Path $repo "driver\LecS65Drv.h"
$driverSource = Join-Path $repo "driver\Driver.c"
$ioctlSource = Join-Path $repo "driver\Ioctl.c"
$deviceSource = Join-Path $repo "driver\Device.c"
$acquisitionSource = Join-Path $repo "driver\Acquisition.c"
$lecwatchSource = Join-Path $repo "tools\lecwatch\lecwatch.c"
$lecwatchBuild = Join-Path $repo "scripts\build-lecwatch.ps1"

foreach ($path in @($publicHeader, $driverHeader, $driverSource, $ioctlSource, $deviceSource, $acquisitionSource, $lecwatchSource, $lecwatchBuild)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required source file missing: $path"
    }
}

$publicText = Get-Content -LiteralPath $publicHeader -Raw
$driverText = Get-Content -LiteralPath $driverHeader -Raw
$driverSourceText = Get-Content -LiteralPath $driverSource -Raw
$ioctlText = Get-Content -LiteralPath $ioctlSource -Raw
$deviceText = Get-Content -LiteralPath $deviceSource -Raw
$acquisitionText = Get-Content -LiteralPath $acquisitionSource -Raw
$lecwatchText = Get-Content -LiteralPath $lecwatchSource -Raw

$script:Checks = 0
$script:Passed = 0
$script:Failed = 0

function Test-Contract {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][scriptblock]$Body
    )

    $script:Checks++
    try {
        $ok = [bool](& $Body)
        if ($ok) {
            $script:Passed++
            Write-Host ("[PASS] " + $Name) -ForegroundColor Green
        }
        else {
            $script:Failed++
            Write-Host ("[FAIL] " + $Name) -ForegroundColor Red
        }
    }
    catch {
        $script:Failed++
        Write-Host ("[FAIL] {0}: {1}" -f $Name, $_.Exception.Message) -ForegroundColor Red
    }
}

function Get-HexDefine {
    param([string]$Text, [string]$Name)
    $escaped = [regex]::Escape($Name)
    $match = [regex]::Match(
        $Text,
        "(?m)^\s*#define\s+$escaped\s+.*?0x([0-9A-Fa-f]{8})")
    if (-not $match.Success) { return $null }

    # Return canonical text instead of a numeric PowerShell literal.
    # Windows PowerShell 5.1 parses 0x80000000..0xFFFFFFFF as signed Int32,
    # which makes CFDC/CFDD IOCTL values negative before a UInt32 cast.
    return $match.Groups[1].Value.ToUpperInvariant()
}

$represented = [ordered]@{
    "LECS65_IOCTL_00222400" = "00222400"
    "LECS65_IOCTL_DELAY_MILLISECONDS" = "00222C00"
    "LECS65_IOCTL_SET_FLAG_BYTE" = "00222C04"
    "LECS65_IOCTL_SET_TRACE_CONTROL" = "00223000"
    "LECS65_IOCTL_QUERY_BUFFER_A" = "00223004"
    "LECS65_IOCTL_SET_ONE_REGISTER" = "0022303C"
    "LECS65_IOCTL_QUERY_BUFFER_B" = "00223040"
    "LECS65_IOCTL_READ_START_REGISTER" = "00223044"
    "LECS65_IOCTL_GET_DALLAS_ID" = "00223080"
    "LECS65_IOCTL_READ_DALLAS_MEMORY" = "00223084"
    "LECS65_IOCTL_SET_THREE_EVENTS" = "00223100"
    "LECS65_IOCTL_CFDC2110" = "CFDC2110"
    "LECS65_IOCTL_REGISTER_TRANSFER" = "CFDC2124"
    "LECS65_IOCTL_UNREGISTER_TRANSFER" = "CFDC2128"
    "LECS65_IOCTL_CFDC212C" = "CFDC212C"
    "LECS65_IOCTL_ACQUIRE_BUFFERED" = "CFDC2138"
    "LECS65_IOCTL_SET_EVENT_0" = "CFDC2180"
    "LECS65_IOCTL_CFDC2184" = "CFDC2184"
    "LECS65_IOCTL_SET_EVENT_1" = "CFDC218C"
    "LECS65_IOCTL_CFDC2190" = "CFDC2190"
    "LECS65_IOCTL_CFDC2194" = "CFDC2194"
    "LECS65_IOCTL_REGISTER_READ" = "CFDC21C0"
    "LECS65_IOCTL_REGISTER_WRITE" = "CFDC21C4"
    "LECS65_IOCTL_GET_DRIVER_BUILD" = "CFDC21C8"
    "LECS65_IOCTL_CFDC2400" = "CFDC2400"
    "LECS65_IOCTL_ACQUIRE_NEITHER" = "CFDD219F"
}

Test-Contract "legacy driver build remains 1002" {
    $publicText -match 'LECS65_LEGACY_DRIVER_BUILD\s+.*?1002' -and
    $driverText -match 'LECS65_LEGACY_DRIVER_BUILD\s+.*?1002'
}

Test-Contract "26 represented driver IOCTL constants keep their numeric values" {
    foreach ($entry in $represented.GetEnumerator()) {
        $actual = Get-HexDefine -Text $driverText -Name $entry.Key
        if ($null -eq $actual -or $actual -ne [string]$entry.Value) {
            return $false
        }
    }
    return $true
}

Test-Contract "selected native IOCTLs still have dispatch references in Ioctl.c" {
    foreach ($entry in $represented.GetEnumerator()) {
        $pattern = [regex]::Escape([string]$entry.Key)
        if ([regex]::Matches($ioctlText, $pattern).Count -lt 1) {
            return $false
        }
    }
    return $true
}

Test-Contract "public ABI keeps the three known hazardous controls documented" {
    (Get-HexDefine $publicText "LECS65_IOCTL_0022303C") -eq "0022303C" -and
    (Get-HexDefine $publicText "LECS65_IOCTL_WRITE_DALLAS_MEMORY") -eq "00223088" -and
    (Get-HexDefine $publicText "LECS65_IOCTL_PROG_SERTRIG_FPGA") -eq "CFDC2130"
}

Test-Contract "remaining hazardous controls stay absent from native driver header" {
    $driverText -notmatch 'LECS65_IOCTL_WRITE_DALLAS_MEMORY|0x00223088' -and
    $driverText -notmatch 'LECS65_IOCTL_PROG_SERTRIG_FPGA|0xCFDC2130'
}

Test-Contract "remaining hazardous controls stay absent from Ioctl.c dispatch/source" {
    $ioctlText -notmatch 'LECS65_IOCTL_WRITE_DALLAS_MEMORY|0x00223088' -and
    $ioctlText -notmatch 'LECS65_IOCTL_PROG_SERTRIG_FPGA|0xCFDC2130'
}

Test-Contract "SetOneRegister stays bounded to the known 43-entry native table" {
    $driverText -match 'LECS65_IOCTL_SET_ONE_REGISTER\s+.*?0x0022303C' -and
    $ioctlText -match 'case\s+LECS65_IOCTL_SET_ONE_REGISTER' -and
    $ioctlText -match 'InputLength\s*!=\s*LECS65_LEGACY_REGISTER_ENTRY_BYTES' -and
    $ioctlText -match 'OutputLength\s*!=\s*0' -and
    $ioctlText -match 'index\s*>?=\s*LECS65_LEGACY_REGISTER_COUNT' -and
    $ioctlText -match 'entry\s*=\s*&g_LecLegacyRegisterList\[index\]' -and
    $ioctlText -match 'LecReadU32\(Buffer\s*\+\s*0x101\)' -and
    $ioctlText -match 'LecReadU32\(Buffer\s*\+\s*0x106\)'
}

Test-Contract "START_DEVICE requires a connected IRQ before publishing device interfaces" {
    $startMatch = [regex]::Match(
        $deviceText,
        '(?s)case IRP_MN_START_DEVICE:(.*?)case IRP_MN_STOP_DEVICE:')
    $startMatch.Success -and
    $startMatch.Groups[1].Value -match 'irqStatus = LecConnectInterrupt\(devExt\)' -and
    $startMatch.Groups[1].Value -match 'status = irqStatus' -and
    $startMatch.Groups[1].Value -match 'LecUnmapBars\(devExt\)' -and
    $startMatch.Groups[1].Value -match '(?s)else\s*\{\s*devExt->Started = TRUE;\s*LecSetIoctlAdmission\(devExt, TRUE\);\s*LecEnableInterfaces\(devExt\)'
}

Test-Contract "both native DMA launch paths reject absent interrupts" {
    $mttr = [regex]::Match(
        $ioctlText,
        '(?s)LecExecuteLegacyMttTransferLocked\s*\(.*?(?=static\s+(?:NTSTATUS|BOOLEAN|VOID)|\z)')
    $buffered = [regex]::Match(
        $ioctlText,
        '(?s)LecIoctlAcquireBufferedOneChannel\s*\(.*?(?=static\s+(?:NTSTATUS|BOOLEAN|VOID)|\z)')
    $gate = 'if\s*\(!DevExt->Started\s*\|\|\s*!DevExt->InterruptConnected\)\s*\{\s*return STATUS_DEVICE_NOT_READY;'
    $mttr.Success -and $buffered.Success -and
    $mttr.Value -match $gate -and $buffered.Value -match $gate
}

Test-Contract "all dispatch paths participate in remove-lock ownership" {
    $driverText -match 'IO_REMOVE_LOCK RemoveLock' -and
    $driverSourceText -match 'IoInitializeRemoveLock\(' -and
    ([regex]::Matches($driverSourceText, 'IoAcquireRemoveLock\(').Count -ge 3) -and
    $driverSourceText -match 'LecReleaseForwardedIrpLock' -and
    $driverSourceText -match 'STATUS_CONTINUE_COMPLETION' -and
    $deviceText -match 'IoAcquireRemoveLock\(' -and
    $deviceText -match 'IoReleaseRemoveLockAndWait\(' -and
    $ioctlText -match 'IoAcquireRemoveLock\(' -and
    $ioctlText -match 'IoReleaseRemoveLock\('
}

Test-Contract "IOCTL admission counter releases at dispatch completion" {
    $driverSourceText -match 'LecEnterIoctl' -and
    $driverSourceText -match 'LecLeaveIoctl' -and
    $driverSourceText -match 'LecDrainIoctls' -and
    $ioctlText -match 'if \(!LecEnterIoctl\(devExt\)\)' -and
    $ioctlText -match 'LecLeaveIoctl\(devExt\)' -and
    $driverSourceText -match 'KeWaitForSingleObject\(\s*&DevExt->IoIdleEvent'
}

Test-Contract "PnP remove is drained before deleting device" {
    $remove = [regex]::Match(
        $deviceText,
        '(?s)case IRP_MN_REMOVE_DEVICE:(.*?)default:')
    $remove.Success -and
    $remove.Value -match 'LecSetIoctlAdmission\(devExt, FALSE\)' -and
    $remove.Value -match 'LecDrainIoctls\(devExt\)' -and
    $remove.Value -match 'LecQuiesceDeferredWork\(devExt, devExt->Started\)' -and
    $remove.Value -match 'IoReleaseRemoveLockAndWait\(&devExt->RemoveLock, Irp\)' -and
    $remove.Value.IndexOf('IoReleaseRemoveLockAndWait') -lt
        $remove.Value.IndexOf('IoDeleteDevice')
}

Test-Contract "STOP and surprise removal drain IOCTLs before BAR release" {
    $stop = [regex]::Match(
        $deviceText, '(?s)case IRP_MN_STOP_DEVICE:(.*?)case IRP_MN_SURPRISE_REMOVAL:')
    $surprise = [regex]::Match(
        $deviceText, '(?s)case IRP_MN_SURPRISE_REMOVAL:(.*?)case IRP_MN_REMOVE_DEVICE:')
    $stop.Success -and $surprise.Success -and
    $stop.Value -match 'LecQuiesceDeferredWork\(devExt, TRUE\)' -and
    $surprise.Value -match 'LecQuiesceDeferredWork\(devExt, FALSE\)' -and
    $stop.Value.IndexOf('LecDrainIoctls') -lt $stop.Value.IndexOf('LecReleaseAllTransfers') -and
    $surprise.Value.IndexOf('LecDrainIoctls') -lt $surprise.Value.IndexOf('LecReleaseAllTransfers')
}

Test-Contract "IRQ DPC and timer quiescence precedes resource cleanup" {
    $acquisitionText -match 'KeSynchronizeExecution\(' -and
    $acquisitionText -match 'KeRemoveQueueDpc\(' -and
    $acquisitionText -match 'KeFlushQueuedDpcs\(' -and
    $acquisitionText -match 'KeCancelTimer\(' -and
    $acquisitionText -match 'LegacyMamShadowInitialized = FALSE' -and
    $acquisitionText -match 'LegacyMamSeqShadowInitialized = FALSE'
}

Test-Contract "public packed register ABI size guards are still present" {
    $required = @(
        'sizeof\(LECS65_REG_READ_LEGACY\) == 4',
        'sizeof\(LECS65_REG_READ_EXT\) == 5',
        'sizeof\(LECS65_REG_WRITE_LEGACY\) == 8',
        'sizeof\(LECS65_REG_WRITE_EXT\) == 9',
        'sizeof\(LECS65_REGISTER_TRANSFER32\) == 12',
        'sizeof\(LECS65_UNREGISTER_TRANSFER32\) == 4',
        'sizeof\(LECS65_CHANNEL_PAIR32\) == 2'
    )
    foreach ($pattern in $required) {
        if ($publicText -notmatch $pattern) { return $false }
    }
    return $true
}

Test-Contract "private debug IOCTL range remains separate from legacy numeric ABI" {
    $driverText -match 'CTL_CODE\(0x8000, 0x800' -and
    $driverText -match 'CTL_CODE\(0x8000, 0x805'
}

Test-Contract "lecwatch remains read-only and trace-only" {
    [regex]::Matches($lecwatchText, 'DeviceIoControl\s*\(').Count -eq 1 -and
    $lecwatchText -match 'LECS65_IOCTL_DEBUG_GET_TRACE' -and
    $lecwatchText -match 'is_sensitive_ioctl' -and
    $lecwatchText -notmatch 'LECS65_IOCTL_DEBUG_CLEAR_TRACE'
}

Write-Host ""
Write-Host ("DRY REGRESSION: {0}/{1} passed; {2} failed." -f
    $script:Passed, $script:Checks, $script:Failed)

if ($script:Failed -ne 0) {
    throw ("Dry regression FAILED: {0} contract(s) changed." -f $script:Failed)
}

Write-Host "All hardware-independent contracts passed." -ForegroundColor Green
