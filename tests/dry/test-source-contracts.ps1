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
$completionSource = Join-Path $repo "driver\DmaCompletion.c"
$completionHeader = Join-Path $repo "driver\DmaCompletion.h"
$layoutSource = Join-Path $repo "driver\DmaLayout.c"
$layoutHeader = Join-Path $repo "driver\DmaLayout.h"
$adapterSource = Join-Path $repo "driver\DmaAdapterStage.c"
$adapterHeader = Join-Path $repo "driver\DmaAdapterStage.h"
$ownerSource = Join-Path $repo "driver\DmaMappingOwner.c"
$ownerHeader = Join-Path $repo "driver\DmaMappingOwner.h"
$scatterSource = Join-Path $repo "driver\DmaScatterGatherStage.c"
$scatterHeader = Join-Path $repo "driver\DmaScatterGatherStage.h"
$syncSource = Join-Path $repo "driver\DmaSyncStage.c"
$syncHeader = Join-Path $repo "driver\DmaSyncStage.h"
$pnpDmaSource = Join-Path $repo "driver\DmaPnpStage.c"
$pnpDmaHeader = Join-Path $repo "driver\DmaPnpStage.h"
$pnpPublicationSource = Join-Path $repo "driver\DmaPnpPublication.c"
$pnpPublicationHeader = Join-Path $repo "driver\DmaPnpPublication.h"
$pnpIrpSource = Join-Path $repo "driver\PnpIrpLifetime.c"
$driverProject = Join-Path $repo "driver\LecS65AcqDrv.vcxproj"
$lecwatchSource = Join-Path $repo "tools\lecwatch\lecwatch.c"
$lecwatchBuild = Join-Path $repo "scripts\build-lecwatch.ps1"

foreach ($path in @($publicHeader, $driverHeader, $driverSource, $ioctlSource, $deviceSource, $acquisitionSource, $completionSource, $completionHeader, $layoutSource, $layoutHeader, $adapterSource, $adapterHeader, $ownerSource, $ownerHeader, $scatterSource, $scatterHeader, $syncSource, $syncHeader, $pnpDmaSource, $pnpDmaHeader, $pnpPublicationSource, $pnpPublicationHeader, $pnpIrpSource, $driverProject, $lecwatchSource, $lecwatchBuild)) {
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
$completionText = Get-Content -LiteralPath $completionSource -Raw
$completionHeaderText = Get-Content -LiteralPath $completionHeader -Raw
$layoutText = Get-Content -LiteralPath $layoutSource -Raw
$layoutHeaderText = Get-Content -LiteralPath $layoutHeader -Raw
$adapterText = Get-Content -LiteralPath $adapterSource -Raw
$adapterHeaderText = Get-Content -LiteralPath $adapterHeader -Raw
$ownerText = Get-Content -LiteralPath $ownerSource -Raw
$ownerHeaderText = Get-Content -LiteralPath $ownerHeader -Raw
$scatterText = Get-Content -LiteralPath $scatterSource -Raw
$scatterHeaderText = Get-Content -LiteralPath $scatterHeader -Raw
$syncText = Get-Content -LiteralPath $syncSource -Raw
$syncHeaderText = Get-Content -LiteralPath $syncHeader -Raw
$pnpDmaText = Get-Content -LiteralPath $pnpDmaSource -Raw
$pnpDmaHeaderText = Get-Content -LiteralPath $pnpDmaHeader -Raw
$pnpPublicationText = Get-Content -LiteralPath $pnpPublicationSource -Raw
$pnpPublicationHeaderText = Get-Content -LiteralPath $pnpPublicationHeader -Raw
$pnpIrpText = Get-Content -LiteralPath $pnpIrpSource -Raw
$driverProjectText = Get-Content -LiteralPath $driverProject -Raw
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
    $gate = 'if\s*\(!DevExt->Started\s*\|\|\s*!DevExt->InterruptConnected\s*\|\|\s*InterlockedCompareExchange\(&DevExt->DmaUnknownActive,\s*0,\s*0\)\s*!=\s*0\)\s*\{\s*return STATUS_DEVICE_NOT_READY;'
    $mttr.Success -and $buffered.Success -and
    $mttr.Value -match $gate -and $buffered.Value -match $gate
}

Test-Contract "all dispatch paths participate in remove-lock ownership" {
    $driverText -match 'IO_REMOVE_LOCK RemoveLock' -and
    $driverSourceText -match 'IoInitializeRemoveLock\(' -and
    ([regex]::Matches($driverSourceText, 'IoAcquireRemoveLock\(').Count -ge 3) -and
    $driverProjectText -match 'ClCompile Include="PnpIrpLifetime.c"' -and
    $pnpIrpText -match 'LecReleaseForwardedIrpLock' -and
    $pnpIrpText -match 'STATUS_CONTINUE_COMPLETION' -and
    $pnpIrpText -match 'IoReleaseRemoveLock' -and
    $deviceText -match 'IoAcquireRemoveLock\(' -and
    $deviceText -match 'IoReleaseRemoveLockAndWait\(' -and
    $ioctlText -match 'IoAcquireRemoveLock\(' -and
    $ioctlText -match 'IoReleaseRemoveLock\('
}

Test-Contract "AddDevice partial initialization has exact cleanup" {
    $addDevice = [regex]::Match(
        $driverSourceText,
        '(?s)LecS65AddDevice\s*\(.*?(?=VOID\s+LecS65Unload)')
    $addDevice.Success -and
    $addDevice.Value -match '(?s)IoAttachDeviceToDeviceStack.*?if \(devExt->LowerDeviceObject == NULL\).*?IoDeleteDevice\(deviceObject\).*?return STATUS_NO_SUCH_DEVICE' -and
    $addDevice.Value -match '(?s)LecDmaPnpPublicationCreate.*?if \(!NT_SUCCESS\(status\)\).*?IoDetachDevice.*?LowerDeviceObject = NULL.*?IoDeleteDevice\(deviceObject\).*?return status' -and
    $addDevice.Value -match '(?s)IoRegisterDeviceInterface.*?if \(NT_SUCCESS\(status\)\).*?InterfaceRegistered\[i\] = TRUE' -and
    $deviceText -match '(?s)if \(devExt->InterfaceRegistered\[i\]\).*?RtlFreeUnicodeString.*?InterfaceRegistered\[i\] = FALSE' -and
    $pnpPublicationText -match '(?s)if \(\*Publication != NULL\).*?STATUS_INVALID_DEVICE_STATE'
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

Test-Contract "uncertain DMA is latched after both launch paths" {
    $driverText -match 'volatile LONG DmaUnknownActive' -and
    $driverText -match 'BOOLEAN DmaUnsafeToFree' -and
    $acquisitionText -match 'Transfer->DmaUnsafeToFree = TRUE' -and
    $acquisitionText -match 'InterlockedExchange\(&DevExt->DmaUnknownActive, 1\)' -and
    ([regex]::Matches($ioctlText, 'LecMarkDmaUnknownActive\(').Count -eq 4) -and
    ([regex]::Matches($ioctlText, 'LecDmaCompletionFinishWait\(').Count -eq 4) -and
    ([regex]::Matches($ioctlText, 'LecArmSelectedDma\(').Count -eq 2) -and
    ([regex]::Matches($ioctlText, 'LecLaunchSelectedDma\(').Count -eq 2) -and
    ([regex]::Matches($ioctlText, 'LecCancelSelectedDmaArm\(').Count -eq 2) -and
    $deviceText -match 'DmaUnknownActive'
}

Test-Contract "unknown DMA memory cannot be released by cleanup" {
    $acquisitionText -match 'if \(Transfer->DmaUnsafeToFree\)' -and
    $acquisitionText -match 'transfer->DmaUnsafeToFree\) \{' -and
    $acquisitionText -match '!candidate->DmaUnsafeToFree' -and
    $acquisitionText -match 'LecDmaCompletionMarkQuarantined'
}

Test-Contract "live DMA completion has explicit evidence states and generations" {
    $driverProjectText -match 'ClCompile Include="DmaCompletion.c"' -and
    $completionHeaderText -match 'LecDmaCompletionNeverLaunched' -and
    $completionHeaderText -match 'LecDmaCompletionArming' -and
    $completionHeaderText -match 'LecDmaCompletionDeviceActive' -and
    $completionHeaderText -match 'LecDmaCompletionObserved' -and
    $completionHeaderText -match 'LecDmaCompletionIdleProved' -and
    $completionHeaderText -match 'LecDmaCompletionUnknownActive' -and
    $completionHeaderText -match 'LecDmaCompletionQuarantined' -and
    $completionText -match 'LecDmaCompletionObservePhysicalIrq' -and
    $completionText -match 'LecDmaCompletionMayReleaseMapping' -and
    $ioctlText -match 'dmaGeneration' -and
    $acquisitionText -match 'DmaActiveGeneration'
}

Test-Contract "DMA GO commit and active publication exclude the ISR" {
    $launchCallback = [regex]::Match(
        $acquisitionText,
        '(?s)LecCommitDmaLaunchSynchronized\s*\(.*?\n\}')
    $launchEntry = [regex]::Match(
        $acquisitionText,
        '(?s)LecLaunchSelectedDma\s*\(.*?\n\}')
    $armed = $launchCallback.Value.IndexOf('LecDmaCompletionIsArmed')
    $completionControl = $launchCallback.Value.IndexOf(
        'WRITE_REGISTER_ULONG(launch->CompletionControl')
    $go = $launchCallback.Value.IndexOf(
        'WRITE_REGISTER_ULONG(launch->GoRegister')
    $published = $launchCallback.Value.IndexOf(
        'LecDmaCompletionPublishDeviceActive')
    $launchCallback.Success -and
    $launchEntry.Success -and
    $armed -ge 0 -and
    $completionControl -gt $armed -and
    $go -gt $completionControl -and
    $published -gt $go -and
    $launchEntry.Value -match 'KeSynchronizeExecution\(' -and
    $ioctlText -notmatch 'WRITE_REGISTER_ULONG\(iimcl, 1UL\)' -and
    $ioctlText -notmatch 'WRITE_REGISTER_ULONG\(mttrgo,' -and
    $ioctlText -notmatch 'WRITE_REGISTER_ULONG\(mamRgo, launchCount\)'
}

Test-Contract "wrong-generation completion consumers cannot erase evidence" {
    $consume = [regex]::Match(
        $completionText,
        '(?s)LecDmaCompletionConsumeSignal\s*\(.*?\n\}')
    $consume.Success -and
    $consume.Value -match 'InterlockedCompareExchange64\(' -and
    $consume.Value -notmatch 'InterlockedExchange64\('
}

Test-Contract "completion IRQ never directly proves idle or releases mappings" {
    $observeCompletion = [regex]::Match(
        $completionText,
        '(?s)LecDmaCompletionObservePhysicalIrq\s*\(.*?\n\}')
    $observeCompletion.Success -and
    $observeCompletion.Value -match 'LecDmaCompletionObserved' -and
    $observeCompletion.Value -notmatch 'LecDmaCompletionIdleProved' -and
    $completionText -match '(?s)LecDmaCompletionMayReleaseMapping.*?LecDmaCompletionNeverLaunched.*?LecDmaCompletionIdleProved' -and
    ($driverSourceText + $deviceText + $acquisitionText + $ioctlText) -notmatch 'LecDmaCompletionProveIdle' -and
    $acquisitionText -match 'KeAcquireSpinLockAtDpcLevel\(&devExt->DmaCompletionLock\)' -and
    $acquisitionText -match 'LecDeselectDmaTransfer'
}

Test-Contract "serialized MAM launch rechecks terminal DMA admission" {
    $mam = [regex]::Match(
        $ioctlText,
        '(?s)LecIoctlAcquireBufferedOneChannel\s*\(.*?(?=static\s+NTSTATUS\s+LecIoctl)')
    $serialized = $mam.Value.IndexOf('KeWaitForSingleObject(')
    $faultRecheck = $mam.Value.IndexOf('DmaUnknownActive', $serialized + 1)
    $firstMamMmio = $mam.Value.IndexOf('LecResolveRegister', $serialized + 1)
    $mam.Success -and
    $serialized -ge 0 -and
    $faultRecheck -gt $serialized -and
    $firstMamMmio -gt $faultRecheck
}

Test-Contract "synthetic completion bit cannot satisfy a DMA wait" {
    $acquisitionText -match 'if \(\(PendingMask & 0x01UL\) != 0\)\s*\{\s*return STATUS_INVALID_PARAMETER;' -and
    $acquisitionText -match 'LecInjectLegacyPendingAndDispatch'
}

Test-Contract "adapter-logical descriptor encoder is compiled but not activated" {
    $driverProjectText -match 'ClCompile Include="DmaLayout.c"' -and
    $layoutHeaderText -match 'LecDmaEncodeMappedSegments' -and
    $layoutText -match 'LECS65_DMA_LAYOUT_SLOTS_PER_PAGE' -and
    $layoutText -match 'TableDeviceAddress' -and
    $layoutText -notmatch 'MmGetMdlPfnArray|MmGetPhysicalAddress' -and
    $acquisitionText -match 'MmGetMdlPfnArray' -and
    $ioctlText -notmatch 'LecDmaEncodeMappedSegments'
}

Test-Contract "WDM DMA adapter stage is compiled but cannot alter active acquisitions" {
    $driverProjectText -match 'ClCompile Include="DmaAdapterStage.c"' -and
    $adapterHeaderText -match 'LecDmaCreateAdapterContext' -and
    $adapterText -match 'IoGetDmaAdapter\(' -and
    $adapterText -match 'DEVICE_DESCRIPTION_VERSION3' -and
    $adapterText -match 'DmaAddressWidth = 32' -and
    $adapterText -match 'AllocateCommonBuffer\(' -and
    $adapterText -match 'FreeCommonBuffer\(' -and
    $adapterText -match 'PutDmaAdapter\(' -and
    $adapterText -match 'RTL_SIZEOF_THROUGH_FIELD\(DMA_OPERATIONS, FreeAdapterObject\)' -and
    $adapterText -match 'LecDmaClaimSynchronousOwner' -and
    $adapterText -match 'Context->TableAllocationPending' -and
    $adapterText -match 'Context->Quarantined = TRUE;' -and
    $acquisitionText -notmatch 'LecDmaCreateAdapterContext|LecDmaAllocateCommonTable' -and
    $deviceText -notmatch 'LecDmaCreateAdapterContext|LecDmaAllocateCommonTable' -and
    $ioctlText -notmatch 'LecDmaCreateAdapterContext|LecDmaAllocateCommonTable'
}

Test-Contract "DMA mapping callback ownership model is isolated and fail-closed" {
    $driverProjectText -match 'ClCompile Include="DmaMappingOwner.c"' -and
    $ownerHeaderText -match 'LecMapAwaitingCallback' -and
    $ownerHeaderText -match 'LecMapUnknownActive' -and
    $ownerText -match 'LecMapOwnerCallback' -and
    $ownerText -match 'LecMapOwnerIdleProved' -and
    $ownerText -match 'o->Phase = LecMapUnknownActive;' -and
    $ownerText -match 'o->Phase == LecMapReady && o->CallbackSeen' -and
    $acquisitionText -notmatch 'LecMapOwnerRequest|LecMapOwnerLaunch' -and
    $deviceText -notmatch 'LecMapOwnerRequest|LecMapOwnerLaunch'
}

Test-Contract "WDM SG callback bridge is fail-closed pending real rundown" {
    $driverProjectText -match 'ClCompile Include="DmaScatterGatherStage.c"' -and
    $scatterText -match 'GetScatterGatherList\(' -and
    $scatterText -notmatch '->PutScatterGatherList\(' -and
    $scatterText -notmatch 'ObDereferenceObject\(' -and
    $scatterText -notmatch 'ExFreePoolWithTag\(' -and
    $scatterText -notmatch 'LecSgStagePeek\(' -and
    $scatterText -match 'LecSgStageCopySegments\(' -and
    $scatterText -match 'KeAcquireSpinLock\(&Stage->Lock' -and
    $scatterText -match 'Stage->SubmissionReturned = TRUE;' -and
    $scatterText -match '(?s)LecSgStageMarkLaunched\(.*?return FALSE;' -and
    $scatterText -notmatch 'LecMapOwnerLaunch\(&Stage->Owner\)' -and
    $scatterText -match 'LecMapOwnerUncertain\(&Stage->Owner\)' -and
    $scatterText -match 'return STATUS_DEVICE_BUSY;' -and
    $scatterText -match 'KeRaiseIrql\(DISPATCH_LEVEL' -and
    $scatterHeaderText -match 'LecSgStageMarkIdleProved' -and
    $acquisitionText -notmatch 'LecSgStageMap|LecSgStageRelease' -and
    $deviceText -notmatch 'LecSgStageMap|LecSgStageRelease' -and
    $ioctlText -notmatch 'LecSgStageMap|LecSgStageRelease'
}

Test-Contract "sync DMA v3 no-launch owner drains before teardown" {
    $driverProjectText -match 'ClCompile Include="DmaSyncStage.c"' -and
    $adapterText -match 'DEVICE_DESCRIPTION_VERSION3' -and
    $syncText -match 'DMA_SYNCHRONOUS_CALLBACK, NULL, NULL, FALSE' -and
    $syncText -match 'InitializeDmaTransferContext' -and
    $syncText -match 'GetScatterGatherListEx' -and
    $syncText -match 'FreeAdapterObject' -and
    $syncText -match 'LecSgSyncOwnerStop' -and
    $syncText -match 'Owner->Outstanding' -and
    $syncText -match 'LecSgSyncOwnerStopping' -and
    $syncText -match 'LecSgSyncOwnerConstruct' -and
    $syncText -match 'LecSgSyncOwnerDrainNoLaunch' -and
    $syncText -match 'LecSgSyncOwnerDestroy' -and
    $syncText -match 'NumberOfMapRegisters' -and
    $syncText -match 'DescriptorSlotCapacity' -and
    $syncText -match 'STATUS_DELETE_PENDING' -and
    $syncText -match 'DeallocateObject' -and
    $syncText -notmatch 'LecMapOwnerLaunch' -and
    $acquisitionText -notmatch 'LecSgSync|GetScatterGatherListEx' -and
    $deviceText -notmatch 'LecSgSync|GetScatterGatherListEx' -and
    $ioctlText -notmatch 'LecSgSync|GetScatterGatherListEx'
}

Test-Contract "PnP DMA parent publishes lifetime only and cannot activate DMA" {
    $driverProjectText -match 'ClCompile Include="DmaPnpStage.c"' -and
    $driverProjectText -match 'ClInclude Include="DmaPnpStage.h"' -and
    $driverProjectText -match 'ClCompile Include="DmaPnpPublication.c"' -and
    $driverProjectText -match 'ClInclude Include="DmaPnpPublication.h"' -and
    $pnpDmaHeaderText -match 'One instance represents one physical-device ownership domain' -and
    $pnpDmaHeaderText -match 'PLECS65_DMA_ADAPTER_CONTEXT AdapterContext' -and
    $pnpDmaHeaderText -match 'LECS65_SG_SYNC_OWNER SyncOwner' -and
    $pnpDmaText -match 'LecDmaCreateAdapterContext' -and
    $pnpDmaText -match 'LecSgSyncOwnerInit' -and
    $pnpDmaText -notmatch 'LecMapOwnerLaunch' -and
    $driverSourceText -match 'LecDmaPnpPublicationCreate' -and
    $acquisitionText -match 'LecDmaPnpPublicationRetainLegacyTransfer' -and
    $deviceText -match 'LecDmaPnpPublicationRemove' -and
    ($driverSourceText + $acquisitionText + $deviceText + $ioctlText) -notmatch
        'LecDmaPnpStartNoLaunch|LecDmaPnpMapNoLaunch|LecSgSyncMapNoLaunch|LecDmaCreateAdapterContext' -and
    $acquisitionText -match 'MmGetMdlPfnArray'
}

Test-Contract "PnP DMA parent gates rundown and unknown-active retention" {
    $pnpDmaHeaderText -match 'ULONG ActiveCalls' -and
    $pnpDmaHeaderText -match 'BOOLEAN AdmissionOpen' -and
    $pnpDmaText -match 'LecDmaPnpBeginTeardown' -and
    $pnpDmaText -match 'LecDmaPnpRecordQuiescence' -and
    $pnpDmaText -match 'LecDmaPnpFinishTeardownNoLaunch' -and
    $pnpDmaText -match 'LecDmaPnpInterruptDisconnected' -and
    $pnpDmaText -match 'LecDmaPnpDpcDrained' -and
    $pnpDmaText -match 'LecDmaPnpTimerStopped' -and
    $pnpDmaText -match 'LecSgSyncOwnerDrainNoLaunch' -and
    $pnpDmaText -match 'LecDmaReleaseAdapterContext\(context, TRUE\)' -and
    $pnpDmaText -match 'LecDmaQuarantineAdapterContext\(context\)' -and
    $adapterText -match 'LecDmaQuarantineAdapterContext' -and
    $pnpDmaText -notmatch 'KeWaitForSingleObject|KeDelayExecutionThread'
}

Test-Contract "PnP DMA parent uses factory lifetime and atomic quarantine" {
    $pnpDmaHeaderText -match 'LecDmaPnpStageCreate' -and
    $pnpDmaHeaderText -match 'LecDmaPnpStageDestroy' -and
    $pnpDmaHeaderText -notmatch 'LecDmaPnpStageConstruct' -and
    $pnpDmaHeaderText -match 'volatile LONG QuarantineRequested' -and
    $pnpDmaHeaderText -match 'LECS65_DMA_PNP_CLEANUP_PHASE' -and
    $pnpDmaText -match 'InterlockedExchange\(&Stage->QuarantineRequested, 1\)' -and
    $pnpDmaText -match 'LecSgSyncOwnerQuarantine' -and
    $syncHeaderText -match 'ULONG ReleasesInFlight' -and
    $syncText -match 'LecSgSyncCommitReleaseLocked'
}

Test-Contract "live PnP publication is remove-lock ordered and fail-closed" {
    $pnpPublicationHeaderText -match 'allocates only nonpaged software objects' -and
    $pnpPublicationText -match 'LecDmaPnpStageCreate' -and
    $pnpPublicationText -match 'LecDmaPnpPublicationUnpublishing' -and
    $pnpPublicationText -match 'KeWaitForSingleObject' -and
    $pnpPublicationText -match 'RetainedLegacyTransferCount' -and
    $pnpPublicationText -match 'LecDmaPnpQuarantineUnknownActive' -and
    $pnpPublicationText -match 'ObReferenceObject\(PhysicalDeviceObject\)' -and
    $pnpPublicationText -match 'ObDereferenceObject\(Publication->PhysicalDeviceObject\)' -and
    $pnpPublicationText -match 'InterlockedCompareExchange\(&Reference->State, 2, 1\)' -and
    ([regex]::Matches($driverSourceText, 'LecDmaPnpPublicationCreate\(').Count -eq 1) -and
    $deviceText -match '(?s)IoReleaseRemoveLockAndWait.*?LecReleaseAllTransfers.*?DmaPnpPublication = NULL.*?LecDmaPnpPublicationRemove.*?LecUnmapBars' -and
    $pnpPublicationText -match 'LECS65_TRANSFER_QUARANTINE_TRANSFERRING' -and
    $pnpPublicationText -match '(?s)InsertTailList.*?LECS65_TRANSFER_QUARANTINE_PUBLICATION.*?LecDmaPnpPublicationQuarantineParent' -and
    $acquisitionText -match '(?s)Transfer->DmaUnsafeToFree = TRUE;.*?DmaUnknownActive.*?LecSetIoctlAdmission.*?LecDmaPnpPublicationQuarantine' -and
    $ioctlText -match 'LecMarkDmaUnknownActive'
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
