#include "DmaTransaction.h"
#include <string.h>

static LECS65_DMA_TRANSACTION_RESULT
LecDmaTxCheckGeneration(const LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (tx == 0 || generation == 0 || generation != tx->Generation) {
        return LecDmaTxStaleGeneration;
    }
    return LecDmaTxOk;
}

static void LecDmaTxMakeUnknown(LECS65_DMA_TRANSACTION* tx)
{
    tx->Phase = LecDmaTxUnknownActive;
    tx->QuarantineLatched = 1;
}

void LecDmaTransactionInitialize(LECS65_DMA_TRANSACTION* tx)
{
    if (tx != 0) {
        memset(tx, 0, sizeof(*tx));
        tx->Phase = LecDmaTxIdle;
    }
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionSetLifecycle(
    LECS65_DMA_TRANSACTION* tx, unsigned started, unsigned poweredD0)
{
    if (tx == 0 || tx->QuarantineLatched) return LecDmaTxInvalid;
    if ((!started || !poweredD0) && tx->EverPossiblyLaunched &&
        tx->Phase != LecDmaTxIdleProved && tx->Phase != LecDmaTxReleased) {
        LecDmaTxMakeUnknown(tx);
        return LecDmaTxMustQuarantine;
    }
    tx->DeviceStarted = started != 0;
    tx->PoweredD0 = poweredD0 != 0;
    if (started && poweredD0) tx->PowerTeardown = 0;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBegin(
    LECS65_DMA_TRANSACTION* tx, uintptr_t owner, uintptr_t file,
    uint32_t bytes, uint64_t* generation)
{
    uint64_t next;
    if (tx == 0 || generation == 0 || owner == 0 || file == 0 || bytes == 0)
        return LecDmaTxInvalid;
    if (tx->QuarantineLatched || tx->PnpTeardown || tx->PowerTeardown ||
        !tx->DeviceStarted || !tx->PoweredD0) return LecDmaTxNotReady;
    if (tx->Phase != LecDmaTxIdle && tx->Phase != LecDmaTxReleased)
        return LecDmaTxBusy;
    next = tx->Generation + 1;
    if (next == 0) next = 1;
    tx->Generation = next;
    tx->OwnerId = owner;
    tx->FileId = file;
    tx->RequestedBytes = bytes;
    tx->MappingToken = 0;
    tx->DescriptorEntries = 0;
    tx->MappingOwned = 0;
    tx->MappingCallbackPending = 0;
    tx->DescriptorsPublished = 0;
    tx->EverPossiblyLaunched = 0;
    tx->CompletionDelivered = 0;
    tx->Phase = LecDmaTxValidated;
    tx->Diagnostics.Requests++;
    *generation = next;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBindOwner(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation, uintptr_t owner, uintptr_t file)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxValidated) return LecDmaTxNotReady;
    if (owner != tx->OwnerId || file != tx->FileId) return LecDmaTxWrongOwner;
    tx->Phase = LecDmaTxOwnerBound;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionAdmitAdapter(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxOwnerBound || !tx->DeviceStarted || !tx->PoweredD0 ||
        tx->PnpTeardown || tx->PowerTeardown || tx->QuarantineLatched)
        return LecDmaTxNotReady;
    tx->Phase = LecDmaTxAdapterAdmitted;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionAcquireBuffer(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxAdapterAdmitted) return LecDmaTxNotReady;
    tx->Phase = LecDmaTxBufferOwned;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginMapping(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxBufferOwned || tx->PnpTeardown || tx->PowerTeardown)
        return LecDmaTxNotReady;
    tx->MappingCallbackPending = 1;
    tx->Phase = LecDmaTxMappingPending;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionMappingComplete(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation, uint64_t token, unsigned succeeded)
{
    int cancelled;
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    cancelled = tx->Phase == LecDmaTxCancelled;
    if ((!cancelled && tx->Phase != LecDmaTxMappingPending) || !tx->MappingCallbackPending)
        return LecDmaTxNotReady;
    tx->MappingCallbackPending = 0;
    if (!succeeded) {
        tx->Phase = LecDmaTxCancelled;
        return LecDmaTxOk;
    }
    if (token == 0) {
        tx->Phase = LecDmaTxCancelled;
        return LecDmaTxInvalid;
    }
    tx->MappingToken = token;
    tx->MappingOwned = 1;
    tx->Diagnostics.MappingAcquired++;
    tx->Phase = cancelled ? LecDmaTxCancelled : LecDmaTxMapped;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBuildDescriptors(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation,
    LECS65_DMA_LAYOUT_ENTRY* table, uint32_t tableBytes,
    uint64_t tableAddress, const LECS65_DMA_MAPPED_SEGMENT* segments,
    uint32_t segmentCount)
{
    uint32_t used = 0;
    LECS65_DMA_LAYOUT_RESULT result;
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxMapped || !tx->MappingOwned) return LecDmaTxNotReady;
    result = LecDmaEncodeMappedSegments(table, tableBytes, tableAddress,
        segments, segmentCount, tx->RequestedBytes, &used);
    if (result == LecDmaLayoutAddressRange) return LecDmaTxAddressRange;
    if (result == LecDmaLayoutCapacity) return LecDmaTxCapacity;
    if (result != LecDmaLayoutOk) return LecDmaTxInvalid;
    tx->DescriptorEntries = used;
    tx->Phase = LecDmaTxDescriptorsReady;
    tx->Diagnostics.DescriptorBuilds++;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionPublish(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation,
    LECS65_DMA_PUBLICATION_BARRIER barrier, void* context)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxDescriptorsReady || barrier == 0 ||
        tx->DescriptorEntries == 0 || tx->PnpTeardown || tx->PowerTeardown)
        return LecDmaTxNotReady;
    barrier(context);
    tx->DescriptorsPublished = 1;
    tx->Phase = LecDmaTxPublished;
    tx->Diagnostics.Publications++;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginArm(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxPublished || !tx->DescriptorsPublished ||
        !tx->DeviceStarted || !tx->PoweredD0 || tx->PnpTeardown ||
        tx->PowerTeardown || tx->QuarantineLatched) return LecDmaTxNotReady;
    tx->Phase = LecDmaTxArming;
    tx->Diagnostics.LaunchAdmissions++;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionCommitHardwareLaunch(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxArming) return LecDmaTxNotReady;
    tx->Phase = LecDmaTxPublished;
    tx->Diagnostics.LaunchRejected++;
    return LecDmaTxLaunchDisabled;
}

#ifdef LECS65_DMA_TRANSACTION_TEST_HOOKS
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionTestRecordPossibleGo(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (tx->Phase != LecDmaTxArming) return LecDmaTxNotReady;
    tx->EverPossiblyLaunched = 1;
    tx->Phase = LecDmaTxDeviceActive;
    return LecDmaTxOk;
}
#endif

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionObserveCompletion(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (tx == 0 || generation != tx->Generation) {
        if (tx != 0) tx->Diagnostics.DuplicateOrStaleCompletion++;
        return LecDmaTxStaleGeneration;
    }
    if (tx->Phase != LecDmaTxDeviceActive || tx->CompletionDelivered) {
        tx->Diagnostics.DuplicateOrStaleCompletion++;
        return LecDmaTxNotReady;
    }
    tx->CompletionDelivered = 1;
    tx->Phase = LecDmaTxCompletionObserved;
    tx->Diagnostics.CompletionObserved++;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionProvePhysicalIdle(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation, unsigned proof)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (!proof || tx->Phase != LecDmaTxCompletionObserved) return LecDmaTxNotReady;
    tx->Phase = LecDmaTxIdleProved;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionCancel(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation, unsigned timeout)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    tx->Diagnostics.Cancellations++;
    if (timeout) tx->Diagnostics.Timeouts++;
    if (tx->EverPossiblyLaunched && tx->Phase != LecDmaTxIdleProved &&
        tx->Phase != LecDmaTxReleased) {
        LecDmaTxMakeUnknown(tx);
        return LecDmaTxMustQuarantine;
    }
    if (tx->Phase == LecDmaTxReleased || tx->Phase == LecDmaTxIdle)
        return LecDmaTxNotReady;
    tx->Phase = LecDmaTxCancelled;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginPowerDown(
    LECS65_DMA_TRANSACTION* tx)
{
    if (tx == 0) return LecDmaTxInvalid;
    tx->PowerTeardown = 1;
    tx->PoweredD0 = 0;
    tx->Diagnostics.PowerTeardowns++;
    if (tx->EverPossiblyLaunched && tx->Phase != LecDmaTxIdleProved &&
        tx->Phase != LecDmaTxReleased) {
        LecDmaTxMakeUnknown(tx);
        return LecDmaTxMustQuarantine;
    }
    if (tx->Phase != LecDmaTxIdle && tx->Phase != LecDmaTxReleased)
        tx->Phase = LecDmaTxCancelled;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginPnpTeardown(
    LECS65_DMA_TRANSACTION* tx, unsigned surprise)
{
    if (tx == 0) return LecDmaTxInvalid;
    (void)surprise;
    tx->PnpTeardown = 1;
    tx->DeviceStarted = 0;
    tx->Diagnostics.PnpTeardowns++;
    if (tx->EverPossiblyLaunched && tx->Phase != LecDmaTxIdleProved &&
        tx->Phase != LecDmaTxReleased) {
        LecDmaTxMakeUnknown(tx);
        return LecDmaTxMustQuarantine;
    }
    if (tx->Phase != LecDmaTxIdle && tx->Phase != LecDmaTxReleased)
        tx->Phase = LecDmaTxCancelled;
    return LecDmaTxOk;
}

int LecDmaTransactionMayRelease(const LECS65_DMA_TRANSACTION* tx)
{
    if (tx == 0 || !tx->MappingOwned || tx->MappingCallbackPending ||
        tx->QuarantineLatched || (tx->EverPossiblyLaunched &&
        tx->Phase != LecDmaTxIdleProved)) return 0;
    return tx->Phase == LecDmaTxMapped || tx->Phase == LecDmaTxDescriptorsReady ||
        tx->Phase == LecDmaTxPublished || tx->Phase == LecDmaTxArming ||
        tx->Phase == LecDmaTxCancelled || tx->Phase == LecDmaTxIdleProved;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionReleaseMapping(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (!LecDmaTransactionMayRelease(tx))
        return tx->QuarantineLatched ? LecDmaTxMustQuarantine : LecDmaTxNotReady;
    tx->MappingOwned = 0;
    tx->MappingToken = 0;
    tx->DescriptorsPublished = 0;
    tx->DescriptorEntries = 0;
    tx->Phase = LecDmaTxReleased;
    tx->Diagnostics.MappingReleased++;
    return LecDmaTxOk;
}

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionQuarantine(
    LECS65_DMA_TRANSACTION* tx, uint64_t generation)
{
    if (LecDmaTxCheckGeneration(tx, generation) != LecDmaTxOk)
        return LecDmaTxStaleGeneration;
    if (!tx->QuarantineLatched && tx->Phase != LecDmaTxUnknownActive)
        return LecDmaTxNotReady;
    tx->QuarantineLatched = 1;
    tx->Phase = LecDmaTxQuarantined;
    tx->Diagnostics.Quarantines++;
    return LecDmaTxOk;
}

void LecDmaTransactionSnapshot(
    const LECS65_DMA_TRANSACTION* tx, LECS65_DMA_TRANSACTION* snapshot)
{
    if (tx != 0 && snapshot != 0) *snapshot = *tx;
}
