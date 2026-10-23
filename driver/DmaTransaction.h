#pragma once

/*
 * Hardware-independent owner for one future WDM-v3 acquisition transaction.
 *
 * This module deliberately has no MMIO or hardware-launch entry point.  The
 * production driver can validate, map, build and publish a transaction, but
 * cannot transition it to DeviceActive.  Host tests may define
 * LECS65_DMA_TRANSACTION_TEST_HOOKS to model post-GO races without making a
 * launch primitive available to the driver binary.
 *
 * Callers serialize access at <= DISPATCH_LEVEL.  Mapping/allocation DDIs and
 * waits remain outside that lock and at their documented IRQL.  OwnerId and
 * FileId are opaque, nonzero identities borrowed from the request lifetime.
 */
#include <stdint.h>
#include "DmaLayout.h"

typedef enum _LECS65_DMA_TRANSACTION_RESULT {
    LecDmaTxOk = 0,
    LecDmaTxInvalid,
    LecDmaTxWrongOwner,
    LecDmaTxNotReady,
    LecDmaTxBusy,
    LecDmaTxAddressRange,
    LecDmaTxCapacity,
    LecDmaTxLaunchDisabled,
    LecDmaTxStaleGeneration,
    LecDmaTxMustQuarantine
} LECS65_DMA_TRANSACTION_RESULT;

typedef enum _LECS65_DMA_TRANSACTION_PHASE {
    LecDmaTxIdle = 0,
    LecDmaTxValidated,
    LecDmaTxOwnerBound,
    LecDmaTxAdapterAdmitted,
    LecDmaTxBufferOwned,
    LecDmaTxMappingPending,
    LecDmaTxMapped,
    LecDmaTxDescriptorsReady,
    LecDmaTxPublished,
    LecDmaTxArming,
    LecDmaTxDeviceActive,
    LecDmaTxCompletionObserved,
    LecDmaTxIdleProved,
    LecDmaTxCancelled,
    LecDmaTxReleased,
    LecDmaTxUnknownActive,
    LecDmaTxQuarantined
} LECS65_DMA_TRANSACTION_PHASE;

typedef struct _LECS65_DMA_TRANSACTION_DIAGNOSTICS {
    uint64_t Requests;
    uint64_t MappingAcquired;
    uint64_t MappingReleased;
    uint64_t DescriptorBuilds;
    uint64_t Publications;
    uint64_t LaunchAdmissions;
    uint64_t LaunchRejected;
    uint64_t CompletionObserved;
    uint64_t DuplicateOrStaleCompletion;
    uint64_t Cancellations;
    uint64_t Timeouts;
    uint64_t Quarantines;
    uint64_t PowerTeardowns;
    uint64_t PnpTeardowns;
} LECS65_DMA_TRANSACTION_DIAGNOSTICS;

typedef struct _LECS65_DMA_TRANSACTION {
    LECS65_DMA_TRANSACTION_PHASE Phase;
    uint64_t Generation;
    uintptr_t OwnerId;
    uintptr_t FileId;
    uint64_t MappingToken;
    uint32_t RequestedBytes;
    uint32_t DescriptorEntries;
    unsigned DeviceStarted;
    unsigned PoweredD0;
    unsigned PnpTeardown;
    unsigned PowerTeardown;
    unsigned MappingOwned;
    unsigned MappingCallbackPending;
    unsigned DescriptorsPublished;
    unsigned EverPossiblyLaunched;
    unsigned CompletionDelivered;
    unsigned QuarantineLatched;
    LECS65_DMA_TRANSACTION_DIAGNOSTICS Diagnostics;
} LECS65_DMA_TRANSACTION;

typedef void (*LECS65_DMA_PUBLICATION_BARRIER)(void* Context);

void LecDmaTransactionInitialize(LECS65_DMA_TRANSACTION* Transaction);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionSetLifecycle(
    LECS65_DMA_TRANSACTION* Transaction,
    unsigned DeviceStarted,
    unsigned PoweredD0);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBegin(
    LECS65_DMA_TRANSACTION* Transaction,
    uintptr_t OwnerId,
    uintptr_t FileId,
    uint32_t RequestedBytes,
    uint64_t* Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBindOwner(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    uintptr_t OwnerId,
    uintptr_t FileId);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionAdmitAdapter(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionAcquireBuffer(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginMapping(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionMappingComplete(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    uint64_t MappingToken,
    unsigned Succeeded);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBuildDescriptors(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    LECS65_DMA_LAYOUT_ENTRY* Table,
    uint32_t TableBytes,
    uint64_t TableDeviceAddress,
    const LECS65_DMA_MAPPED_SEGMENT* Segments,
    uint32_t SegmentCount);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionPublish(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    LECS65_DMA_PUBLICATION_BARRIER Barrier,
    void* BarrierContext);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginArm(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);

/* Always fails closed and rolls the software-only Arming state back. */
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionCommitHardwareLaunch(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionObserveCompletion(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionProvePhysicalIdle(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    unsigned IndependentIdleProof);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionCancel(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    unsigned Timeout);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginPowerDown(
    LECS65_DMA_TRANSACTION* Transaction);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionBeginPnpTeardown(
    LECS65_DMA_TRANSACTION* Transaction,
    unsigned SurpriseRemoval);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionReleaseMapping(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionQuarantine(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
int LecDmaTransactionMayRelease(const LECS65_DMA_TRANSACTION* Transaction);
void LecDmaTransactionSnapshot(
    const LECS65_DMA_TRANSACTION* Transaction,
    LECS65_DMA_TRANSACTION* Snapshot);

#ifdef LECS65_DMA_TRANSACTION_TEST_HOOKS
/* Model-only evidence injection. Never compiled into the driver project. */
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionTestRecordPossibleGo(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
#endif
