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
$transactionSource = Join-Path $repo "driver\DmaTransaction.c"
$transactionHeader = Join-Path $repo "driver\DmaTransaction.h"
$v3TransferSource = Join-Path $repo "driver\DmaV3Transfer.c"
$v3TransferHeader = Join-Path $repo "driver\DmaV3Transfer.h"
$layoutSource = Join-Path $repo "driver\DmaLayout.c"
$layoutHeader = Join-Path $repo "driver\DmaLayout.h"
$experimentGateSource = Join-Path $repo "driver\DmaExperimentGate.c"
$experimentGateHeader = Join-Path $repo "driver\DmaExperimentGate.h"
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
$driverInf = Join-Path $repo "driver\LecS65AcqDrv.inf"
$lecwatchSource = Join-Path $repo "tools\lecwatch\lecwatch.c"
$lecdiagSource = Join-Path $repo "tools\lecdiag\lecdiag.c"
$lecwatchBuild = Join-Path $repo "scripts\build-lecwatch.ps1"
$packageBuild = Join-Path $repo "scripts\build-driver-package.ps1"
$nativeProcess = Join-Path $repo "scripts\native-process.ps1"
$diagnosticLauncher = Join-Path $repo "scripts\invoke-diagnostic-capture-install.ps1"
$code52Collector = Join-Path $repo "scripts\collect-code52-signing-diagnostics.ps1"
$runtimeBaseline = Join-Path $repo "scripts\test-runtime-baseline.ps1"
$processDmaModel = Join-Path $repo "tests\dry\test-process-dma-lifetime.c"
$codexContinuation = Join-Path $repo "scripts\manage-codex-continuation.ps1"
$mockDeviceSource = Join-Path $repo "tests\dry\lecdiag-mock-device.c"

foreach ($path in @($publicHeader, $driverHeader, $driverSource, $ioctlSource, $deviceSource, $acquisitionSource, $completionSource, $completionHeader, $transactionSource, $transactionHeader, $v3TransferSource, $v3TransferHeader, $layoutSource, $layoutHeader, $experimentGateSource, $experimentGateHeader, $adapterSource, $adapterHeader, $ownerSource, $ownerHeader, $scatterSource, $scatterHeader, $syncSource, $syncHeader, $pnpDmaSource, $pnpDmaHeader, $pnpPublicationSource, $pnpPublicationHeader, $pnpIrpSource, $driverProject, $driverInf, $lecwatchSource, $lecdiagSource, $lecwatchBuild, $packageBuild, $nativeProcess, $diagnosticLauncher, $code52Collector, $runtimeBaseline, $processDmaModel, $codexContinuation, $mockDeviceSource)) {
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
$interruptText = Get-Content -LiteralPath (Join-Path $repo "driver\InterruptControl.c") -Raw
$experimentGateText = Get-Content -LiteralPath $experimentGateSource -Raw
$completionHeaderText = Get-Content -LiteralPath $completionHeader -Raw
$transactionText = Get-Content -LiteralPath $transactionSource -Raw
$transactionHeaderText = Get-Content -LiteralPath $transactionHeader -Raw
$v3TransferText = Get-Content -LiteralPath $v3TransferSource -Raw
$v3TransferHeaderText = Get-Content -LiteralPath $v3TransferHeader -Raw
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
$driverInfText = Get-Content -LiteralPath $driverInf -Raw
$lecwatchText = Get-Content -LiteralPath $lecwatchSource -Raw
$code52CollectorText = Get-Content -LiteralPath $code52Collector -Raw
$runtimeBaselineText = Get-Content -LiteralPath $runtimeBaseline -Raw
$processDmaModelText = Get-Content -LiteralPath $processDmaModel -Raw
$codexContinuationText = Get-Content -LiteralPath $codexContinuation -Raw
$lecdiagText = Get-Content -LiteralPath $lecdiagSource -Raw
$packageBuildText = Get-Content -LiteralPath $packageBuild -Raw
$nativeProcessText = Get-Content -LiteralPath $nativeProcess -Raw
$diagnosticLauncherText = Get-Content -LiteralPath $diagnosticLauncher -Raw
$mockDeviceText = Get-Content -LiteralPath $mockDeviceSource -Raw

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

Test-Contract "Debug and Release x64 disable manifests for driver linking" {
    $projectXml = [xml]$driverProjectText
    $namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
    $namespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
    foreach ($configuration in @('Debug', 'Release')) {
        $group = $projectXml.SelectSingleNode(
            "//msb:PropertyGroup[@Label='Configuration' and contains(@Condition, '$configuration|x64')]",
            $namespace)
        if ($null -eq $group -or
            $group.PlatformToolset -ne 'v143' -or
            $group.GenerateManifest -ne 'false' -or
            $group.EmbedManifest -ne 'false') {
            return $false
        }
    }
    return $true
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

Test-Contract "legacy two-stage query sizes agree across public ABI, driver and mock" {
    (Get-HexDefine $publicText "LECS65_LEGACY_TRACE_BLOCK_BYTES") -eq "00000110" -and
    (Get-HexDefine $publicText "LECS65_LEGACY_REGISTER_ENTRY_BYTES") -eq "0000010A" -and
    (Get-HexDefine $publicText "LECS65_LEGACY_REGISTER_LIST_BYTES") -eq "00002CAE" -and
    $publicText -match 'LECS65_LEGACY_REGISTER_COUNT\s+UINT32_C\(43\)' -and
    $ioctlText -match 'C_ASSERT\(LECS65_LEGACY_TRACE_BLOCK_BYTES == 0x110UL\)' -and
    $ioctlText -match 'C_ASSERT\(LECS65_LEGACY_REGISTER_LIST_BYTES == 0x2CAEUL\)' -and
    $mockDeviceText -match '(?s)case 0x00223004:.*?\*\(DWORD\*\)OutputBuffer = 0x110;' -and
    $mockDeviceText -match '(?s)case 0x00223040:.*?\*\(DWORD\*\)OutputBuffer = 0x2CAE;'
}

Test-Contract "replacement publishes the exact five legacy-compatible interface classes" {
    $interfaceBlock = [regex]::Match(
        $driverSourceText,
        '(?s)g_LecS65InterfaceGuids\s*\[LECS65_INTERFACE_COUNT\]\s*=\s*\{(.*?)\};')
    if (-not $interfaceBlock.Success) {
        return $false
    }

    $normalized = ($interfaceBlock.Groups[1].Value -replace '\s', '').ToLowerInvariant()
    $expected = @(
        '{0x958695a4,0x693a,0x435e,{0x82,0x97,0x66,0xf8,0x05,0xd8,0xe4,0x6a}}',
        '{0x7ac34be9,0xf766,0x4f15,{0x9e,0x88,0x85,0x4b,0xa5,0xe2,0x14,0x6e}}',
        '{0x8d1103b8,0x5bf4,0x4b5c,{0xb2,0x1e,0xee,0xaa,0xce,0x97,0xd4,0x18}}',
        '{0x9007c2bc,0xedfd,0x4f2f,{0xa0,0x59,0xdf,0x11,0x31,0xcb,0x1a,0xe5}}',
        '{0xfc5df040,0xd6cd,0x4ba0,{0xb5,0xe0,0x25,0x61,0x97,0x29,0x63,0xa2}}'
    )

    $driverText -match '(?m)^\s*#define\s+LECS65_INTERFACE_COUNT\s+5\s*$' -and
    ([regex]::Matches($interfaceBlock.Groups[1].Value, '\{\s*0x[0-9A-Fa-f]{8}\s*,').Count -eq 5) -and
    (@($expected | Where-Object { -not $normalized.Contains($_) }).Count -eq 0) -and
    $driverSourceText -match '(?s)for\s*\(i\s*=\s*0;\s*i\s*<\s*LECS65_INTERFACE_COUNT;\s*\+\+i\).*?IoRegisterDeviceInterface\s*\(' -and
    $driverInfText -notmatch '(?im)^\s*AddInterface\s*='
}

Test-Contract "START_DEVICE requires a connected IRQ before publishing device interfaces" {
    $startMatch = [regex]::Match(
        $deviceText,
        '(?s)case IRP_MN_START_DEVICE:(.*?)case IRP_MN_STOP_DEVICE:')
    $startMatch.Success -and
    $startMatch.Groups[1].Value -match 'irqStatus = LecConnectInterrupt\(devExt\)' -and
    $startMatch.Groups[1].Value -match 'status = irqStatus' -and
    $startMatch.Groups[1].Value -match 'LecUnmapBars\(devExt\)' -and
    $startMatch.Groups[1].Value -match '(?s)else\s*\{\s*devExt->Started = TRUE;\s*LecSetIoctlAdmission\(devExt, TRUE\);\s*LecEnableInterfaces\(devExt, Irp\)'
}

Test-Contract "PnP status diagnostics are opt-in and cover every attribution boundary" {
    $driverProjectText -match '<LecPnpStatusDiagnostics Condition=.*?>false</LecPnpStatusDiagnostics>' -and
    $driverProjectText -match 'Condition="''\$\(LecPnpStatusDiagnostics\)''==''true''"' -and
    $driverProjectText -match 'LECS65_PNP_STATUS_DIAGNOSTICS=1' -and
    $pnpIrpText -match 'stage=lower-wait dispatch=' -and
    $pnpIrpText -match 'stage=lower-complete final=' -and
    $pnpIrpText -match 'stage=lower-dispatch returned=' -and
    $deviceText -match 'stage=local-start source=resources status=' -and
    $deviceText -match 'stage=interrupt-connect status=' -and
    $deviceText -match 'stage=final status=' -and
    $deviceText -match 'stage=interface-enable index=' -and
    $pnpIrpText -notmatch 'WRITE_REGISTER_|READ_REGISTER_|DeviceIoControl|IoBuildDeviceIoControlRequest'
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

Test-Contract "create and close keep remove-lock and file ownership balanced" {
    $create = [regex]::Match($driverSourceText,
        '(?s)LecS65Create\s*\(.*?(?=NTSTATUS\s+LecS65Close)')
    $close = [regex]::Match($driverSourceText,
        '(?s)LecS65Close\s*\(.*?\n\}')
    $create.Success -and $close.Success -and
    $create.Value -match '(?s)IoAcquireRemoveLock.*?LecAssociateFileContext.*?IoReleaseRemoveLock.*?IoCompleteRequest' -and
    $close.Value -match '(?s)IoAcquireRemoveLock.*?LecCloseFileContext.*?IoReleaseRemoveLock.*?IoCompleteRequest' -and
    $acquisitionText -match '(?s)LecCloseFileContext.*?RemoveEntryList\(&candidate->Link\).*?ownerStillOpen.*?!transfer->DmaUnsafeToFree' -and
    $ioctlText -match '(?s)LecGetFileOwnerProcessId.*?ownerProcessId.*?LecRegisterTransfer' -and
    $deviceText -match '(?s)IoReleaseRemoveLockAndWait.*?LecReleaseAllTransfers.*?LecReleaseAllFileContexts'
}

Test-Contract "legacy event replacement and teardown retain exact object lifetime" {
    $capture = [regex]::Match($ioctlText,
        '(?s)LecCaptureLegacyEvent\s*\(.*?\n\}')
    $release = [regex]::Match($deviceText,
        '(?s)LecReleaseLegacyEvents\s*\(.*?\n\}')
    $capture.Success -and $release.Success -and
    $capture.Value -match '(?s)ObReferenceObjectByHandle.*?EVENT_MODIFY_STATE.*?KeAcquireSpinLock.*?oldEvent = \*Slot.*?\*Slot = eventObject.*?KeReleaseSpinLock.*?ObDereferenceObject\(oldEvent\)' -and
    $release.Value -match '(?s)KeAcquireSpinLock.*?LegacyEvent0 = NULL.*?LegacyEvent4 = NULL.*?KeReleaseSpinLock.*?ObDereferenceObject' -and
    ([regex]::Matches($deviceText,
        '(?s)LecQuiesceDeferredWork\(.*?LecReleaseLegacyEvents').Count -ge 3)
}

Test-Contract "legacy event and interface audit models match production ordering" {
    $auditModel = Get-Content -LiteralPath (Join-Path $PSScriptRoot "test-runtime-lifetime-audit.c") -Raw
    $auditModel -match 'reference-before-publish' -and
    $auditModel -match 'DPC signaling while the production LegacyEventLock is held' -and
    $auditModel -match 'middle interface failure preserves current continue-on-error policy' -and
    $auditModel -match 'partial publication teardown is idempotent' -and
    $auditModel -match 'first same-process CLOSE preserves inactive DMA registration' -and
    $auditModel -match 'UnknownActive remains quarantined while physical bus-idle is unproven' -and
    $deviceText -match '(?s)IoSetDeviceInterfaceState\(&DevExt->InterfaceLink\[i\], TRUE\).*?if \(NT_SUCCESS\(status\)\).*?InterfaceEnabled\[i\] = TRUE.*?else.*?enable failed' -and
    $ioctlText -match '(?s)ObReferenceObjectByHandle.*?KeAcquireSpinLock\(&DevExt->LegacyEventLock.*?\*Slot = eventObject.*?KeReleaseSpinLock.*?ObDereferenceObject\(oldEvent\)'
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

Test-Contract "uncertain DMA or mapping ownership is latched on every path" {
    $driverText -match 'volatile LONG DmaUnknownActive' -and
    $driverText -match 'BOOLEAN DmaUnsafeToFree' -and
    $acquisitionText -match 'Transfer->DmaUnsafeToFree = TRUE' -and
    $acquisitionText -match 'LecQuarantineInterruptAdmission\(DevExt\)' -and
    $interruptText -match 'InterlockedExchange\(&DevExt->DmaUnknownActive, 1\)' -and
    ([regex]::Matches($ioctlText, 'LecMarkDmaUnknownActive\(').Count -eq 5) -and
    ([regex]::Matches($ioctlText, 'LecDmaCompletionFinishWait\(').Count -eq 3) -and
    ([regex]::Matches($ioctlText, 'LecDmaCompletionFinishCleanup\(').Count -eq 3) -and
    ([regex]::Matches($ioctlText, 'LecArmSelectedDma\(').Count -eq 3) -and
    ([regex]::Matches($ioctlText, 'LecLaunchSelectedDma\(').Count -eq 3) -and
    ([regex]::Matches($ioctlText, 'LecCancelSelectedDmaArm\(').Count -eq 3) -and
    $deviceText -match 'DmaUnknownActive'
}

Test-Contract "unknown DMA memory cannot be released by cleanup" {
    $acquisitionText -match 'if \(Transfer->DmaUnsafeToFree\)' -and
    $acquisitionText -match 'transfer->DmaUnsafeToFree\) \{' -and
    $acquisitionText -match '!transfer->DmaUnsafeToFree' -and
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

Test-Contract "both launched cleanup paths finalize after masking and before deselection" {
    foreach ($name in @('LecExecuteLegacyMttTransferLocked', 'LecIoctlAcquireBufferedOneChannel')) {
        $body = [regex]::Match($ioctlText, "(?s)$name\s*\(.*?\n\}")
        if (-not $body.Success) { return $false }
        $cleanup = $body.Value.LastIndexOf('disableStatus = LecUpdateInterruptMask')
        $finish = $body.Value.IndexOf('LecDmaCompletionFinishCleanup')
        $poison = $body.Value.IndexOf('LecMarkDmaUnknownActive', $finish)
        $deselect = $body.Value.IndexOf('LecDeselectDmaTransfer')
        if ($cleanup -lt 0 -or $finish -le $cleanup -or
            $poison -le $finish -or $deselect -le $poison -or
            $body.Value -notmatch 'NTSTATUS disableStatus = STATUS_SUCCESS' -or
            $body.Value -notmatch '(?s)FinishCleanup\(.*?status == STATUS_SUCCESS.*?NT_SUCCESS\(disableStatus\)' -or
            $body.Value -notmatch '(?s)NT_SUCCESS\(status\).*?!NT_SUCCESS\(disableStatus\).*?status = disableStatus') {
            return $false
        }
    }
    return $true
}

Test-Contract "failed cleanup uses the same irreversible uncertainty transition as failed wait" {
    $finish = [regex]::Match($completionText,
        '(?s)LecDmaCompletionFinishCleanup\s*\(.*?\n\}')
    $finish.Success -and
    $finish.Value -match 'LecDmaCompletionFinishWait\(' -and
    $finish.Value -match 'WaitSucceeded && InterruptMaskSucceeded' -and
    $finish.Value -notmatch 'IdleProved|Prepare|Initialize'
}

Test-Contract "busy selection cannot reset the current completion tracker" {
    $select = [regex]::Match($acquisitionText,
        '(?s)LecSelectDmaTransfer\s*\(.*?\n\}')
    $select.Success -and
    $select.Value -match '(?s)KeAcquireSpinLock.*?CurrentTransfer != NULL.*?DmaActiveGeneration != 0.*?STATUS_DEVICE_BUSY.*?else if \(!LecDmaCompletionPrepare.*?else.*?CurrentTransfer = Transfer.*?KeReleaseSpinLock'
}

Test-Contract "DPC event signaling and transfer deselection share the lifetime lock" {
    $dpc = [regex]::Match($acquisitionText, '(?s)LecInterruptDpc\s*\(.*?\n\}')
    $deselect = [regex]::Match($acquisitionText, '(?s)LecDeselectDmaTransfer\s*\(.*?\n\}')
    $dpc.Success -and $deselect.Success -and
    $dpc.Value -match '(?s)KeAcquireSpinLockAtDpcLevel\(&devExt->DmaCompletionLock\).*?CurrentTransfer.*?transfer->DmaGeneration == generation.*?LecDmaCompletionConsumeSignal.*?KeSetEvent\(\s*&transfer->CompletionEvent.*?KeReleaseSpinLockFromDpcLevel\(&devExt->DmaCompletionLock\)' -and
    ([regex]::Matches($dpc.Value, 'transfer->').Count -eq 2) -and
    $deselect.Value -match '(?s)KeAcquireSpinLock\(&DevExt->DmaCompletionLock.*?CurrentTransfer == Transfer.*?DmaActiveGeneration == Generation.*?CurrentTransfer = NULL.*?DmaActiveGeneration = 0.*?KeReleaseSpinLock'
}

Test-Contract "unregister unlinks once under transfer ownership before freeing" {
    $unregister = [regex]::Match($acquisitionText, '(?s)LecUnregisterTransfer\s*\(.*?\n\}')
    $unregister.Success -and
    $unregister.Value -match '(?s)&DevExt->TransferMutex.*?LecFindTransferOwned.*?transfer == NULL.*?CurrentTransfer == transfer.*?DmaUnsafeToFree.*?STATUS_DEVICE_BUSY.*?RemoveEntryList.*?KeReleaseMutex.*?LecFreeTransfer'
}

Test-Contract "quarantine poisons ownership and closes admission before fallible notification" {
    $poison = [regex]::Match($acquisitionText, '(?s)LecMarkDmaUnknownActive\s*\(.*?\n\}')
    $poison.Success -and
    $poison.Value -match '(?s)DmaUnsafeToFree = TRUE.*?LecQuarantineInterruptAdmission\(DevExt\).*?LecSetIoctlAdmission\(DevExt, FALSE\).*?LecDmaPnpPublicationQuarantine'
}

Test-Contract "every PnP teardown drains IOCTL and deferred work before transfer release" {
    foreach ($kind in @('STOP_DEVICE', 'SURPRISE_REMOVAL', 'REMOVE_DEVICE')) {
        $body = [regex]::Match($deviceText, "(?s)case IRP_MN_${kind}:(.*?)(?=case IRP_MN_|default:)")
        if (-not $body.Success -or $body.Value -notmatch
            '(?s)LecSetIoctlAdmission\(devExt, FALSE\).*?LecDrainIoctls\(devExt\).*?LecQuiesceDeferredWork.*?LecReleaseAllTransfers.*?LecUnmapBars') {
            return $false
        }
    }
    return $true
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

Test-Contract "adapter-logical descriptor encoder is active only behind no-launch gate" {
    $driverProjectText -match 'ClCompile Include="DmaLayout.c"' -and
    $layoutHeaderText -match 'LecDmaEncodeMappedSegments' -and
    $layoutText -match 'LECS65_DMA_LAYOUT_SLOTS_PER_PAGE' -and
    $layoutText -match 'TableDeviceAddress' -and
    $layoutText -notmatch 'MmGetMdlPfnArray|MmGetPhysicalAddress' -and
    $pnpDmaText -match 'LecDmaTransactionBuildDescriptors' -and
    $pnpDmaText -match 'LecDmaTransactionCommitHardwareLaunch' -and
    $acquisitionText -match 'MmGetMdlPfnArray' -and
    $ioctlText -notmatch 'LecDmaEncodeMappedSegments'
}

Test-Contract "WDM DMA adapter ownership is centralized under the PnP parent" {
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
    $deviceText -match 'LecDmaPnpPublicationStartNoLaunch' -and
    $ioctlText -match 'LecDmaPnpPrepareLegacyNoLaunch' -and
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
    $ioctlText -notmatch 'LecSgSync|GetScatterGatherListEx' -and
    $ioctlText -match 'LecDmaPnpPrepareLegacyNoLaunch'
}

Test-Contract "PnP DMA parent owns the live no-launch preparation path" {
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
    $deviceText -match 'LecDmaPnpPublicationStartNoLaunch' -and
    $deviceText -match 'LecDmaPnpPublicationRemove' -and
    $ioctlText -match 'LecDmaPnpPrepareLegacyNoLaunch' -and
    $pnpDmaText -match 'LecDmaTransactionCommitHardwareLaunch' -and
    $pnpDmaText -match 'LecDmaTxLaunchDisabled' -and
    $pnpDmaText -notmatch 'WRITE_REGISTER|READ_REGISTER|MAMRGO|MTTRGO' -and
    $acquisitionText -match 'MmGetMdlPfnArray'
}

Test-Contract "legacy PFN DMA launches are fail-closed by default" {
    $driverText -match '#define LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA 0' -and
    ([regex]::Matches($ioctlText,
        '#if !LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA').Count -ge 2) -and
    $driverProjectText -match '<LecWdmV3MamDma Condition=.*?>false</LecWdmV3MamDma>' -and
    $ioctlText -match 'LecDmaPnpPrepareLegacyNoLaunch' -and
    $ioctlText -match '(?s)#if !LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?transfer->DataBytes != requestedBytes.*?LecDmaPnpPrepareLegacyNoLaunch' -and
    $ioctlText -notmatch '(?s)#if !LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?transfer->TotalDwords != requestedBytes.*?#else' -and
    $pnpDmaText -match 'Result->LaunchDenied' -and
    $pnpDmaText -match 'status = STATUS_NOT_SUPPORTED;' -and
    $acquisitionText -match '(?s)#if LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA\s*/\*.*?LecBuildDescriptorTable\(transfer\);.*?#endif' -and
    $acquisitionText -match 'MmGetMdlByteOffset\(Transfer->DescriptorMdl\) != 0' -and
    $acquisitionText -match 'physical & \(sizeof\(ULONG\) - 1\)'
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
    $acquisitionText -match '(?s)Transfer->DmaUnsafeToFree = TRUE;.*?LecQuarantineInterruptAdmission.*?LecSetIoctlAdmission.*?LecDmaPnpPublicationQuarantine' -and
    $ioctlText -match 'LecMarkDmaUnknownActive'
}

Test-Contract "generic INTEN accesses route through shared ownership helper" {
    $ioctlText -match '(?s)LecIoctlRegisterWrite\(.*?bar == 0 && offset == 0x084.*?return LecWriteInterruptMask' -and
    $ioctlText -match '(?s)LecIoctlRegisterRead\(.*?bar == 0 && offset == 0x084.*?LecReadInterruptMask' -and
    $ioctlText -match '(?s)LecCommitLegacyInterruptMask\(.*?return LecWriteInterruptMask' -and
    ([regex]::Matches($ioctlText, 'InterruptEnableShadow').Count -eq 1)
}

Test-Contract "INTEN RMW and terminal closure share a resident synchronization domain" {
    $driverProjectText -match 'InterruptControl.c' -and
    $driverSourceText -match 'KeInitializeSpinLock\(&devExt->InterruptControlLock\)' -and
    $driverSourceText -match 'LecSetInterruptAdmission\(DevExt, Enable\)' -and
    $interruptText -match '(?s)KeAcquireSpinLock\(&devExt->InterruptControlLock.*?KeSynchronizeExecution.*?KeReleaseSpinLock' -and
    $interruptText -match '(?s)LecQuarantineInterruptAdmission.*?KeAcquireSpinLock.*?DmaUnknownActive, 1.*?InterruptWriteAdmission = FALSE.*?KeReleaseSpinLock' -and
    $interruptText -notmatch 'KeWaitFor|KeDelay|AllocatePool'
}

Test-Contract "INTEN acquisition bit and narrow teardown exceptions remain explicit" {
    $interruptText -match '\(\(newMask \^ oldMask\) & 1\)' -and
    $interruptText -match 'STATUS_DEVICE_BUSY' -and
    $interruptText -match '(?s)WRITE_REGISTER_ULONG\(reg, newMask\).*?InterlockedExchange' -and
    $interruptText -match 'op->ClearBits == 1 && op->SetBits == 0' -and
    $interruptText -match 'devExt->InterruptHardwareUnavailable' -and
    $deviceText -match '(?s)case IRP_MN_SURPRISE_REMOVAL:.*?LecInvalidateInterruptHardware.*?LecSetIoctlAdmission.*?LecDrainIoctls' -and
    $acquisitionText -match 'LecMaskInterruptsForPnp\(DevExt\)'
}

Test-Contract "surprise invalidation gates ISR and HWInt DPC before hardware access" {
    $isr = [regex]::Match($acquisitionText,
        '(?s)BOOLEAN\s+LecInterruptService\(.*?(?=\r?\nVOID\s+LecInterruptDpc)')
    $dpc = [regex]::Match($acquisitionText,
        '(?s)VOID\s+LecInterruptDpc\(.*?(?=\r?\nNTSTATUS\s+LecInjectLegacyPendingAndDispatch)')
    $isr.Success -and $dpc.Success -and
    $isr.Value -match '(?s)if \(LecIsInterruptHardwareUnavailable\(devExt\).*?return FALSE;.*?READ_REGISTER_ULONG' -and
    $dpc.Value -match '(?s)pending & 0x08UL.*?!LecIsInterruptHardwareUnavailable\(devExt\).*?READ_REGISTER_ULONG' -and
    $driverText -match 'volatile LONG InterruptHardwareUnavailable' -and
    $interruptText -match 'InterlockedExchange\(&DevExt->InterruptHardwareUnavailable, 1\)' -and
    $interruptText -match '(?s)LecIsInterruptHardwareUnavailable\(.*?InterlockedCompareExchange' -and
    $interruptText -notmatch 'InterlockedExchange\(&DevExt->InterruptHardwareUnavailable, 0\)'
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

Test-Contract "package workflow is installation- and device-operation-free" {
    ($packageBuildText + $nativeProcessText) -notmatch '(?i)pnputil|Get-PnpDevice|DeviceIoControl|restart-device|disable-device|enable-device|add-driver' -and
    $packageBuildText -match 'InstallAuthorized = \$false' -and
    $packageBuildText -match 'DriverStoreChanged = \$false' -and
    $packageBuildText -match 'PnpChanged = \$false' -and
    $packageBuildText -match 'DeviceOpened = \$false' -and
    $packageBuildText -match 'Invoke-LecS65NativeProcess' -and
    [regex]::Matches($packageBuildText,
        '@\(& \$FilePath @Arguments 2>&1\)').Count -eq 1 -and
    $nativeProcessText -match 'RedirectStandardOutput = true' -and
    $nativeProcessText -match 'RedirectStandardError = true' -and
    $nativeProcessText -match 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' -and
    $nativeProcessText -match 'WaitForExit\(timeoutMilliseconds\)' -and
    $nativeProcessText -match 'BeginOutputReadLine\(\)' -and
    $nativeProcessText -match 'BeginErrorReadLine\(\)' -and
    $nativeProcessText -match 'ProcessTreeClosed = true'
}

Test-Contract "native process runner admits and quotes explicit empty arguments" {
    $nativeProcessText -match '\[AllowEmptyCollection\(\)\]\[AllowEmptyString\(\)\]\[string\[\]\]\$Arguments' -and
    $nativeProcessText -match 'if \(value\.Length == 0\)\s*return "\\"\\"";'
}

Test-Contract "lecdiag raw IOCTL parser accepts an explicit zero-length input" {
    $lecdiagText -match '\*bytesOut\s*=\s*NULL\s*;' -and
    $lecdiagText -match '\*lengthOut\s*=\s*0\s*;' -and
    $lecdiagText -match "(?s)if\s*\(text\[0\]\s*==\s*'\\0'\)\s*\{\s*return\s+0\s*;\s*\}" -and
    $lecdiagText -match 'digits\s*==\s*0\s*\|\|'
}

Test-Contract "lecdiag rejects matching all-ones START reads as untrustworthy" {
    $lecdiagText -match 'legacy == 0xFFFFFFFFUL' -and
    $lecdiagText -match 'UNTRUSTWORTHY: both START reads returned the all-ones bus value'
}

Test-Contract "package workflow pins reviewed source and toolchain" {
    $packageBuildText -match 'ExpectedDriverTreeCommit' -and
    $packageBuildText -match "'merge-base', '--is-ancestor'" -and
    $packageBuildText -match "'rev-parse', 'HEAD:driver'" -and
    $packageBuildText -match "'rev-parse', 'HEAD:include'" -and
    $packageBuildText -match 'MSVC\\14\.41\.34120' -and
    $packageBuildText -match '10\.0\.28000\.0' -and
    $packageBuildText -match 'Current driver/include trees differ'
}

Test-Contract "package identity is explicit and output is non-overwriting" {
    $packageBuildText -match '\[Parameter\(Mandatory=\$true\)\][\s\S]*?\[string\]\$DriverVer' -and
    $packageBuildText -match 'OutputRoot already exists; refusing to overwrite' -and
    $packageBuildText -notmatch 'Remove-Item' -and
    $packageBuildText -match 'Source INF must contain exactly one DriverVer line' -and
    $packageBuildText -match 'PackageState = if \(\$Sign\)' -and
    $packageBuildText -match '\[switch\]\$PnpStatusDiagnostics' -and
    $packageBuildText -match 'Assert-PnpDiagnosticMarkerState' -and
    $packageBuildText -match '\[switch\]\$WdmV3MamDma' -and
    $packageBuildText -match 'Assert-WdmV3MamDmaMarkerState' -and
    $packageBuildText -match 'Rebuild = \$true' -and
    $packageBuildText -match "DiagnosticMarker = 'PNP_STATUS:'" -and
    $packageBuildText -match 'PnpStatusDiagnostics = \[bool\]\$PnpStatusDiagnostics' -and
    $packageBuildText -match 'WdmV3MamDma = \[bool\]\$WdmV3MamDma'
}

Test-Contract "first MAM experiment build is exact-once and narrowly shaped" {
    $driverProjectText -match '<LecFirstMamExperimentOnly Condition=.*?>false</LecFirstMamExperimentOnly>' -and
    $driverProjectText -match 'LECS65_FIRST_MAM_EXPERIMENT_ONLY=1' -and
    $driverProjectText -match 'ClCompile Include="DmaExperimentGate.c"' -and
    $ioctlText -match '#if LECS65_FIRST_MAM_EXPERIMENT_ONLY' -and
    $ioctlText -match 'LecFirstMamValidateAndConsume' -and
    $ioctlText -match 'LecValidateFirstMamExperiment' -and
    $pnpDmaText -match 'bridge\.Validate\(ExecuteContext, &bridge\.Shape\)' -and
    $experimentGateText -match 'Shape->Channel != LECS65_FIRST_MAM_CHANNEL' -and
    $experimentGateText -match 'Shape->SegmentCount != 1U' -and
    $experimentGateText -match 'Shape->FirstSegmentBytes != Shape->TransferBytes' -and
    $experimentGateText -match 'Shape->DescriptorEntries != 2U' -and
    $experimentGateText -match 'descriptors\[1\]\.DeviceAddress != 0' -and
    $experimentGateText -match 'CompareExchange\(Consumed, 1, 0\)' -and
    $packageBuildText -match '\[switch\]\$FirstMamExperimentOnly' -and
    $packageBuildText -match 'Assert-FirstMamExperimentMarkerState' -and
    $packageBuildText -match 'FirstMamExperimentOnly requires WdmV3MamDma'
}

Test-Contract "package signing is opt-in and thumbprint-bound" {
    $packageBuildText -match '\[switch\]\$Sign' -and
    $packageBuildText -match 'CertificateThumbprint is required with -Sign' -and
    $packageBuildText -match "'/sha1'" -and
    $packageBuildText -notmatch "'/n'" -and
    $packageBuildText.Contains('Cert:\LocalMachine\My') -and
    $packageBuildText -match 'UnsignedValidationOnly'
}

Test-Contract "diagnostic launcher keeps every active capture mode explicitly gated" {
    $diagnosticLauncherText -match "'RestartWithCapture'" -and
    $diagnosticLauncherText -match "'NoLaunchWithCapture'" -and
    $diagnosticLauncherText -match '\[switch\]\$AuthorizeKernelCapture' -and
    $diagnosticLauncherText -match '\[switch\]\$AuthorizeInstallation' -and
    $diagnosticLauncherText -match '\[switch\]\$AuthorizePnpRestart' -and
    $diagnosticLauncherText -match '\[switch\]\$AuthorizeNoLaunch' -and
    $diagnosticLauncherText -match 'Preflight forbids active authorization switches' -and
    $diagnosticLauncherText -match 'CAPTURE READY' -and
    $diagnosticLauncherText -match 'InstallInvocations\+\+' -and
    $diagnosticLauncherText -match 'PnpRestartInvocations\+\+' -and
    $diagnosticLauncherText -match 'NoLaunchInvocations\+\+' -and
    $diagnosticLauncherText -match 'No-launch runner result contract mismatch' -and
    $diagnosticLauncherText -match '-Arguments @\(''/restart-device'', \$DeviceInstanceId\)' -and
    $diagnosticLauncherText -match 'no retry was attempted' -and
    $diagnosticLauncherText -notmatch 'scan-devices|disable-device|enable-device|DeviceIoControl|CreateFile'
}

Test-Contract "real capture readiness accounts for buffered DebugView logging" {
    $diagnosticLauncherText -match 'CaptureReadinessTimeoutSeconds = 60' -and
    $diagnosticLauncherText -match 'CaptureReadinessTimeoutSeconds -lt 60' -and
    $diagnosticLauncherText -match 'CaptureDurationSeconds -lt\s*\(\$CaptureReadinessTimeoutSeconds \+ 120\)' -and
    $diagnosticLauncherText -match 'Test-ConfirmedKernelCaptureLine' -and
    $diagnosticLauncherText -match '\\d\{1,2\}:\\d\{2\}:\\d\{2\}\\.\\d\+' -and
    $diagnosticLauncherText -match 'CaptureStabilityMilliseconds' -and
    $diagnosticLauncherText -match 'ExpectedRemainingSeconds' -and
    $diagnosticLauncherText -match 'Assert-CaptureJobRunning' -and
    $diagnosticLauncherText -match "'3000000'" -and
    $nativeProcessText -match 'FileMode\.CreateNew' -and
    $nativeProcessText -match 'ProcessId=' -and
    $nativeProcessText -match 'LiveStdOutPath' -and
    $nativeProcessText -match 'LiveStdErrPath'
}

Test-Contract "Code 52 collector is read-only and fail-closed" {
    $code52CollectorText -match "(?s)bcdedit\.exe.*?@\('/enum','\{current\}'\)" -and
    $code52CollectorText -match "(?s)pnputil\.exe.*?@\('/enum-devices'" -and
    $code52CollectorText -match 'OutputRoot already exists' -and
    $code52CollectorText -notmatch "(?i)@\('/set'|@\('/deletevalue'|/add-driver|/delete-driver|Import-Certificate|Set-Acl|DeviceIoControl|restart-device|disable-device|enable-device"
}

Test-Contract "runtime baseline uses stable identity and treats BDF as evidence" {
    $runtimeBaselineText -match "SelectionBasis = 'ExactHardwareIdAndInstanceId'" -and
    $runtimeBaselineText -match "BdfPolicy = 'EvidenceOnlyNeverSelectionKey'" -and
    $runtimeBaselineText -match 'Expected exactly one present device with hardware ID' -and
    $runtimeBaselineText -match "Properties\.Name -contains 'Data'" -and
    $runtimeBaselineText -match '/enum-interfaces /class' -and
    $runtimeBaselineText -match 'Interface status\|Schnittstellestatus' -and
    $runtimeBaselineText -match 'Stable instance ID mismatch' -and
    $runtimeBaselineText -match 'AcknowledgeLocationChange' -and
    $runtimeBaselineText -notmatch 'DeviceIoControl|CreateFile|pnputil.*?/add-driver|restart-device|disable-device|enable-device'
}

Test-Contract "process DMA model covers file association and irreversible quarantine" {
    $processDmaModelText -match 'CREATE process-allocation failure rolls back without association' -and
    $processDmaModelText -match 'Association, never caller context, owns CLOSE' -and
    $processDmaModelText -match 'first-CLOSE-before-register ordering accepts remaining handle' -and
    $processDmaModelText -match 'surprise removal closes CREATE admission' -and
    $processDmaModelText -match 'UnknownActive remains quarantined without physical idle proof' -and
    $processDmaModelText -match 'CREATE serialized after removal is rejected without association'
}

Test-Contract "WDM-v3 MAM transaction has an explicit default-off development gate" {
    $driverProjectText -match 'DmaTransaction\.c' -and
    $transactionHeaderText -match 'LecDmaTransactionCommitHardwareLaunch' -and
    $transactionHeaderText -match '#define LECS65_ENABLE_WDM_V3_MAM_DMA 0' -and
    $transactionText -match '#if !LECS65_ENABLE_WDM_V3_MAM_DMA' -and
    $transactionText -match 'return LecDmaTxLaunchDisabled' -and
    $transactionText -notmatch 'WRITE_REGISTER|READ_REGISTER|DeviceIoControl|MapTransferEx' -and
    $driverProjectText -match '<LecWdmV3MamDma Condition=.*?>false</LecWdmV3MamDma>' -and
    $driverProjectText -match 'LECS65_ENABLE_WDM_V3_MAM_DMA=1' -and
    $ioctlText -match 'LecDmaPnpExecuteLegacyTransfer' -and
    $ioctlText -match 'WRITE_REGISTER_ULONG\(sgta, DescriptorTable.LowPart\)'
}

Test-Contract "DMA transaction keeps IRQ evidence separate from physical idle" {
    $transactionText -match 'LecDmaTransactionObserveCompletion' -and
    $transactionText -match 'LecDmaTransactionProvePhysicalIdle' -and
    $transactionText -match 'if \(!proof \|\| tx->Phase != LecDmaTxCompletionObserved\)' -and
    $transactionText -match 'LecDmaTxMakeUnknown\(tx\)' -and
    $transactionHeaderText -match 'MappingAcquired' -and
    $transactionHeaderText -match 'PowerTeardowns' -and
    $transactionHeaderText -match 'PnpTeardowns'
}

Test-Contract "Codex continuation uses exact supported one-shot resume" {
    $codexContinuationText -match "@\('exec','--sandbox','workspace-write','--cd'" -and
    $codexContinuationText -match "'resume','--strict-config'" -and
    $codexContinuationText -match 'attempt-started\.json' -and
    $codexContinuationText -match 'Disable-OneTimeTask' -and
    $codexContinuationText -match 'RedirectStandardInput = \$true' -and
    $codexContinuationText -match 'WaitForExit\(\[int\]\$manifest\.Codex\.TimeoutSeconds \* 1000\)' -and
    $codexContinuationText -notmatch "'--last'|dangerously-bypass|codex://threads"
}

Test-Contract "explicit WDM-v3 mapping path is complete and no-launch" {
    $driverProjectText -match 'DmaV3Transfer\.c' -and
    $v3TransferText -match 'GetDmaTransferInfo' -and
    $v3TransferText -match 'AllocateAdapterChannelEx' -and
    $v3TransferText -match 'MapTransferEx' -and
    $v3TransferText -match 'LecDmaClaimSynchronousOwner' -and
    $v3TransferText -match 'LecDmaEncodeMappedSegments' -and
    $v3TransferText -match 'FlushAdapterBuffersEx' -and
    $v3TransferText -match 'FreeAdapterObject' -and
    $v3TransferText -match 'mappedLength != Length' -and
    $v3TransferText -match 'LecDmaV3MapQuarantined' -and
    ($v3TransferText + $v3TransferHeaderText) -notmatch 'WRITE_REGISTER|READ_REGISTER|HardwareLaunch|DeviceActive'
}

Write-Host ""
Write-Host ("DRY REGRESSION: {0}/{1} passed; {2} failed." -f
    $script:Passed, $script:Checks, $script:Failed)

if ($script:Failed -ne 0) {
    throw ("Dry regression FAILED: {0} contract(s) changed." -f $script:Failed)
}

Write-Host "All hardware-independent contracts passed." -ForegroundColor Green
