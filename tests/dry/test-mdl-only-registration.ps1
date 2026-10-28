[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -LiteralPath (Join-Path $repo 'driver\LecS65Drv.h') -Raw
$acquisition = Get-Content -LiteralPath (Join-Path $repo 'driver\Acquisition.c') -Raw
$ioctl = Get-Content -LiteralPath (Join-Path $repo 'driver\Ioctl.c') -Raw
$pnp = Get-Content -LiteralPath (Join-Path $repo 'driver\DmaPnpStage.c') -Raw
$syncTests = Get-Content -LiteralPath (Join-Path $repo 'tests\dry\test-sg-sync.c') -Raw
$processTests = Get-Content -LiteralPath (Join-Path $repo 'tests\dry\test-process-dma-lifetime.c') -Raw
$completionTests = Get-Content -LiteralPath (Join-Path $repo 'tests\dry\test-dma-completion.c') -Raw
$passed = 0
$failed = 0

function Get-FunctionText([string]$Text, [string]$Name) {
    $match = [regex]::Match($Text,
        ('(?ms)^\s*(?:static\s+)?(?:NTSTATUS|VOID|PLECS65_TRANSFER)\s+{0}\s*\(.*?^\}}' -f [regex]::Escape($Name)))
    if (-not $match.Success) { throw "Function not found: $Name" }
    $match.Value
}

function Test-Contract([string]$Name, [scriptblock]$Predicate) {
    try {
        if (-not (& $Predicate)) { throw 'predicate returned false' }
        Write-Host "[PASS] $Name"
        $script:passed++
    }
    catch {
        Write-Host "[FAIL] $Name`: $($_.Exception.Message)" -ForegroundColor Red
        $script:failed++
    }
}

$register = Get-FunctionText $acquisition 'LecRegisterTransfer'
$unregister = Get-FunctionText $acquisition 'LecUnregisterTransfer'
$buildMdl = Get-FunctionText $acquisition 'LecBuildSourceMdlChain'
$acquire = Get-FunctionText $ioctl 'LecIoctlAcquireBufferedOneChannel'
$safeAcquire = [regex]::Match($acquire,
    '(?ms)#if\s+!LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA(?<safe>.*?)#else').Groups['safe'].Value

Test-Contract 'production compile gate disables legacy PFN DMA' {
    $header -match '(?m)^#define\s+LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA\s+0\s*$'
}
Test-Contract 'production registration builds MDLs but not PFN descriptors' {
    $register -match 'LecBuildSourceMdlChain' -and
    $register -match '(?s)#if\s+LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?LecBuildDescriptorTable' -and
    $register -notmatch '(?s)#if\s+!LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?LecBuildDescriptorTable'
}
Test-Contract 'above-4GiB CPU PFN rejection is isolated to unsafe builder' {
    $acquisition -match 'DMA source page above 4GiB' -and
    $register -match '(?s)#if\s+LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?LecBuildDescriptorTable'
}
Test-Contract 'multiple MDLs remain locked as an ordered chain' {
    $buildMdl -match 'while \(offset < Length\)' -and
    $buildMdl -match 'mdl->Next = NULL' -and
    $buildMdl -match 'tail->Next = mdl' -and
    $buildMdl -match 'MmProbeAndLockPages\(mdl, AccessMode, IoWriteAccess\)'
}
Test-Contract 'short and misaligned registrations fail before publication' {
    $register -match 'TotalBytes < sizeof\(ULONG\)' -and
    $register -match 'dataBytes == 0 \|\| \(dataBytes & 3U\) != 0' -and
    $register -match '(?s)LecBuildSourceMdlChain.*?if \(!NT_SUCCESS\(status\)\).*?return status;.*?InsertTailList'
}
Test-Contract 'registration rollback unlocks every completed MDL' {
    $buildMdl -match 'LecFreeMdlChain\(first\)' -and
    $acquisition -match 'MDL_PAGES_LOCKED' -and
    $acquisition -match 'MmUnlockPages\(Mdl\)'
}
Test-Contract 'repeated registrations receive nonzero process-owned tokens' {
    $register -match 'OwnerProcessId' -and
    $register -match 'if \(DevExt->NextTransferToken == 0\)' -and
    $register -match 'LecFindTransferOwned'
}
Test-Contract 'safe preparation has no legacy descriptor dependency' {
    -not [string]::IsNullOrWhiteSpace($safeAcquire) -and
    $safeAcquire -match 'transfer->DataBytes != requestedBytes' -and
    $safeAcquire -notmatch 'TotalDwords|DescriptorTablePhysical|DescriptorBuffer|MmGetMdlPfnArray'
}
Test-Contract 'Windows DMA API is the only safe device-address provider' {
    $acquire -match 'LecDmaPnpPrepareLegacyNoLaunch' -and
    $acquire -match 'LecDmaPnpExecuteLegacyTransfer' -and
    $pnp -match 'LecSgSyncMapNoLaunch' -and
    $pnp -match 'LecSgSyncCopySegments' -and
    $safeAcquire -notmatch 'MmGetPhysicalAddress|MmGetMdlPfnArray'
}
Test-Contract 'mapping failure cannot expose an active transfer' {
    $pnp -match 'LecDmaTransactionMappingComplete' -and
    $pnp -match 'CancelAndRelease' -and
    $pnp -match 'LecDmaPnpQuarantineUnknownActive' -and
    $syncTests -match 'invalid SG byte coverage is freed before publication'
}
Test-Contract 'unregister serializes with preparation and refuses uncertainty' {
    $safeAcquire -match 'TransferMutex' -and
    $unregister -match 'TransferMutex' -and
    $unregister -match 'DevExt->CurrentTransfer == transfer' -and
    $unregister -match 'transfer->DmaUnsafeToFree' -and
    $unregister -match 'STATUS_DEVICE_BUSY'
}
Test-Contract 'process and FileObject lifetime coverage remains explicit' {
    $safeAcquire -match '\(ULONG_PTR\)OwnerProcessId' -and
    $safeAcquire -match '\(ULONG_PTR\)FileObject' -and
    $processTests -match 'two handles from one process share process ownership' -and
    $processTests -match 'final CLOSE retains UnknownActive'
}
Test-Contract 'stale completion and duplicate completion remain rejected' {
    $completionTests -match 'stale IRQ generation cannot complete' -and
    $completionTests -match 'duplicate IRQ after consumption cannot republish completion'
}
Test-Contract 'PnP and POWER tests cover closed admission and teardown' {
    $syncTests -match 'power-down closes admission without touching hardware' -and
    $syncTests -match 'STOP drains only no-launch mappings' -and
    $syncTests -match 'REMOVE drains outstanding no-launch mapping'
}
Test-Contract 'launch remains unconditional software rejection' {
    $pnp -match 'LecDmaTransactionCommitHardwareLaunch' -and
    $pnp -match 'Result->LaunchDenied' -and
    $pnp -match 'status = STATUS_NOT_SUPPORTED' -and
    $pnp -notmatch 'WRITE_REGISTER|READ_REGISTER|MAMRGO|MTTRGO'
}

Write-Host ("MDL-ONLY REGISTRATION: {0}/{1} passed; {2} failed." -f
    $script:passed, ($script:passed + $script:failed), $script:failed)
if ($script:failed -ne 0) { exit 1 }
