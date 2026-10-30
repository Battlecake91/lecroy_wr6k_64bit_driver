#pragma once

/*
 * Hardware-independent owner for one WDM-v3 acquisition generation.
 *
 * This module deliberately has no MMIO or hardware-launch entry point.  The
 * production driver can validate, map, build and publish a transaction, but
 * cannot transition it to DeviceActive in the default build.  A reviewed
 * development build may define LECS65_ENABLE_WDM_V3_MAM_DMA=1 and
 * supply one explicit hardware-commit callback.  The default is zero and the
 * production caller supplies no callback, so this module cannot launch DMA in
 * the installed configuration.
 *
 * Callers serialize access at <= DISPATCH_LEVEL.  Mapping/allocation DDIs and
 * waits remain outside that lock and at their documented IRQL.  OwnerId and
 * FileId are opaque, nonzero identities borrowed from the request lifetime.
 */
#include <stdint.h>
#include "DmaLayout.h"

#ifndef LECS65_ENABLE_WDM_V3_MAM_DMA
#define LECS65_ENABLE_WDM_V3_MAM_DMA 0
#endif

#ifndef LECS65_FIRST_MAM_EXPERIMENT_ONLY
#define LECS65_FIRST_MAM_EXPERIMENT_ONLY 0
#endif

#ifndef LECS65_MAM_LAUNCH_LIMIT
#define LECS65_MAM_LAUNCH_LIMIT 0
#endif

#if LECS65_MAM_LAUNCH_LIMIT < 0
#error LECS65_MAM_LAUNCH_LIMIT must be nonnegative.
#endif

#if LECS65_MAM_LAUNCH_LIMIT > 0 && !LECS65_ENABLE_WDM_V3_MAM_DMA
#error LECS65_MAM_LAUNCH_LIMIT requires LECS65_ENABLE_WDM_V3_MAM_DMA.
#endif

#if LECS65_MAM_LAUNCH_LIMIT > 0 && LECS65_FIRST_MAM_EXPERIMENT_ONLY
#error LECS65_MAM_LAUNCH_LIMIT and LECS65_FIRST_MAM_EXPERIMENT_ONLY are mutually exclusive.
#endif

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
    LecDmaTxCompletionRetired,
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
    uint64_t LaunchCommits;
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
typedef unsigned (*LECS65_DMA_HARDWARE_COMMIT)(
    void* Context,
    uint64_t Generation);

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

/*
 * The default build always fails closed and rolls Arming back.  With the
 * development gate enabled, PossibleGo is committed before invoking the
 * callback.  A callback failure is therefore terminal UnknownActive rather
 * than a reason to release resources.
 */
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionCommitHardwareLaunch(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    LECS65_DMA_HARDWARE_COMMIT HardwareCommit,
    void* HardwareContext);

LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionObserveCompletion(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation);
LECS65_DMA_TRANSACTION_RESULT LecDmaTransactionRetireNormalCompletion(
    LECS65_DMA_TRANSACTION* Transaction,
    uint64_t Generation,
    unsigned CompletionCleanupSucceeded);
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
