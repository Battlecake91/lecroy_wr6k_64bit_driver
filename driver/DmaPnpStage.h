#pragma once

#include "DmaSyncStage.h"
#include "DmaTransaction.h"

/*
 * INACTIVE software-only parent for the staged DMA implementation.
 *
 * One instance represents one physical-device ownership domain and owns at
 * most one adapter context and one synchronous SG owner. Future PnP wiring
 * must publish exactly one independently resident instance for each PDO;
 * callers must not bypass this parent with LecDmaCreateAdapterContext.
 *
 * This interface has no hardware-launch operation. Its teardown path may
 * release only mappings created by LecDmaPnpMapNoLaunch. Unknown-active DMA
 * is terminal and retains every child resource.
 */
typedef enum _LECS65_DMA_PNP_STATE {
    LecDmaPnpUnconstructed = 0,
    LecDmaPnpStopped,
    LecDmaPnpStarting,
    LecDmaPnpStarted,
    LecDmaPnpStopping,
    LecDmaPnpSurprisePending,
    LecDmaPnpSurpriseRemoved,
    LecDmaPnpRemoving,
    LecDmaPnpRemoved,
    LecDmaPnpQuarantined,
    LecDmaPnpDestroying
} LECS65_DMA_PNP_STATE;

typedef enum _LECS65_DMA_PNP_CLEANUP_PHASE {
    LecDmaPnpCleanupIdle = 0,
    LecDmaPnpCleanupMappings,
    LecDmaPnpCleanupOwner,
    LecDmaPnpCleanupAdapterPending,
    LecDmaPnpCleanupAdapterCommitted
} LECS65_DMA_PNP_CLEANUP_PHASE;

typedef enum _LECS65_DMA_PNP_TEARDOWN_REASON {
    LecDmaPnpTeardownStop = 0,
    LecDmaPnpTeardownSurprise,
    LecDmaPnpTeardownRemove
} LECS65_DMA_PNP_TEARDOWN_REASON;

typedef enum _LECS65_DMA_PNP_QUIESCE_STEP {
    LecDmaPnpInterruptDisconnected = 0,
    LecDmaPnpDpcDrained,
    LecDmaPnpTimerStopped
} LECS65_DMA_PNP_QUIESCE_STEP;

typedef enum _LECS65_DMA_POWER_STATE {
    LecDmaPowerUnknown = 0,
    LecDmaPowerD0,
    LecDmaPowerDownPending,
    LecDmaPowerDx,
    LecDmaPowerResumePending
} LECS65_DMA_POWER_STATE;

typedef struct _LECS65_DMA_PNP_STAGE {
    KSPIN_LOCK Lock;
    PDEVICE_OBJECT PhysicalDeviceObject; /* borrowed identity; context refs it */
    PLECS65_DMA_ADAPTER_CONTEXT AdapterContext;
    LECS65_SG_SYNC_OWNER SyncOwner;
    LECS65_DMA_PNP_STATE State;
    ULONG ActiveCalls;
    ULONG Generation;
    ULONG PowerGeneration;
    volatile LONG QuarantineRequested;
    LECS65_DMA_PNP_CLEANUP_PHASE CleanupPhase;
    BOOLEAN AdmissionOpen;
    BOOLEAN CleanupActive;
    BOOLEAN SyncOwnerDetached;
    BOOLEAN InterruptDisconnected;
    BOOLEAN DpcDrained;
    BOOLEAN TimerStopped;
    BOOLEAN UnknownActive;
    BOOLEAN LateQuarantine;
    BOOLEAN QueryPending;
    LECS65_DMA_POWER_STATE PowerState;
    /* Persistent serialized owner for the development-gated MAM path. */
    LECS65_DMA_TRANSACTION LegacyTransferTransaction;
    ULONG_PTR LegacyTransferOwnerId;
    ULONG_PTR LegacyTransferFileId;
    ULONGLONG LegacyTransferMappingToken;
    BOOLEAN LegacyTransferActive;
    ULONGLONG CompletedTransferCount;
} LECS65_DMA_PNP_STAGE, *PLECS65_DMA_PNP_STAGE;

typedef struct _LECS65_DMA_PNP_SNAPSHOT {
    LECS65_DMA_PNP_STATE State;
    ULONG ActiveCalls;
    ULONG Generation;
    ULONG PowerGeneration;
    BOOLEAN AdmissionOpen;
    BOOLEAN HasAdapterContext;
    BOOLEAN CleanupActive;
    BOOLEAN SyncOwnerDetached;
    BOOLEAN InterruptDisconnected;
    BOOLEAN DpcDrained;
    BOOLEAN TimerStopped;
    BOOLEAN UnknownActive;
    BOOLEAN LateQuarantine;
    BOOLEAN QueryPending;
    LECS65_DMA_POWER_STATE PowerState;
    LECS65_DMA_PNP_CLEANUP_PHASE CleanupPhase;
    BOOLEAN LegacyTransferActive;
    ULONGLONG CompletedTransferCount;
    LECS65_DMA_TRANSACTION_PHASE LegacyTransferPhase;
} LECS65_DMA_PNP_SNAPSHOT, *PLECS65_DMA_PNP_SNAPSHOT;

/*
 * Allocates and constructs unique, unpublished nonpaged storage. The caller
 * must publish it only once and must complete external/remove-lock rundown
 * before Destroy. Destroy accepts only an empty Stopped or Removed stage;
 * its caller proves that the pointer is unpublished and all external calls
 * have completed. Neither routine allocates an adapter or DMA mapping.
 */
NTSTATUS LecDmaPnpStageCreate(
    _In_opt_ PDEVICE_OBJECT PhysicalDeviceObject,
    _Outptr_ PLECS65_DMA_PNP_STAGE* Stage);

NTSTATUS LecDmaPnpStageDestroy(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);

/*
 * Inactive START transaction. All potentially blocking/callback DDIs execute
 * without the parent spin lock. Any failed partial initialization is rolled
 * back because no launch operation exists in this layer.
 */
NTSTATUS LecDmaPnpStartNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONG MaximumTransferBytes,
    _In_ ULONG DescriptorTableBytes);

/*
 * Software admission gates for future live PnP/POWER integration. They never
 * forward an IRP, wait, touch MMIO or imply hardware quiescence. QUERY and
 * power-down close new mapping admission atomically; only an exact Started+D0
 * state may reopen it. Existing no-launch mappings remain releasable.
 */
NTSTATUS LecDmaPnpBeginQuery(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpCancelQuery(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpBeginPowerDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpCancelPowerDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpRecordPoweredDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpBeginResumeD0(_Inout_ PLECS65_DMA_PNP_STAGE Stage);
NTSTATUS LecDmaPnpCompleteResumeD0(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ BOOLEAN LowerAndRestoreSucceeded);

/* Closes mapping admission before requesting child-owner STOP. Nonblocking. */
NTSTATUS LecDmaPnpBeginTeardown(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_TEARDOWN_REASON Reason);

/* Records software rundown only. None of these steps proves DMA bus idle. */
NTSTATUS LecDmaPnpRecordQuiescence(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_QUIESCE_STEP Step);

/*
 * Drains and releases only the synchronous no-launch child. BUSY means the
 * complete parent, adapter, descriptor table and borrowed MDLs must survive.
 */
NTSTATUS LecDmaPnpFinishTeardownNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);

/* Parent-rundown wrappers around the actual synchronous stage. */
NTSTATUS LecDmaPnpMapNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Out_ ULONGLONG* Token);

NTSTATUS LecDmaPnpCopySegments(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONGLONG Token,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied);

NTSTATUS LecDmaPnpReleaseNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONGLONG Token);

typedef struct _LECS65_DMA_PREPARE_RESULT {
    LECS65_DMA_TRANSACTION_RESULT TransactionResult;
    ULONG SegmentCount;
    ULONG DescriptorEntries;
    ULONGLONG MappingToken;
    BOOLEAN LaunchDenied;
    BOOLEAN MappingReleased;
    BOOLEAN GoPossible;
    BOOLEAN CompletionObserved;
    BOOLEAN Quarantined;
} LECS65_DMA_PREPARE_RESULT, *PLECS65_DMA_PREPARE_RESULT;

#define LECS65_DMA_MIN_OBSERVED_TRANSFER_BYTES 0x10UL

typedef struct _LECS65_DMA_LEGACY_TRANSFER_OUTCOME {
    NTSTATUS Status;
    BOOLEAN GoCommitted;
    BOOLEAN CompletionObserved;
} LECS65_DMA_LEGACY_TRANSFER_OUTCOME,
    *PLECS65_DMA_LEGACY_TRANSFER_OUTCOME;

typedef NTSTATUS (*LECS65_DMA_LEGACY_TRANSFER_EXECUTE)(
    _Inout_ PVOID Context,
    _In_ ULONGLONG Generation,
    _In_ PHYSICAL_ADDRESS DescriptorTable,
    _In_ ULONG DescriptorEntries,
    _In_ ULONG TransferBytes,
    _Out_ PLECS65_DMA_LEGACY_TRANSFER_OUTCOME Outcome);

/*
 * Production integration point for the bounded legacy CFDC2138 path.
 * The caller supplies an already locked, process-independent MDL chain and
 * stable nonzero request identities. The complete Windows DMA mapping and
 * descriptor transaction is exercised, but CommitHardwareLaunch is a closed
 * gate. No MMIO operation is available through this function.
 */
NTSTATUS LecDmaPnpPrepareLegacyNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _In_ ULONG_PTR OwnerId,
    _In_ ULONG_PTR FileId,
    _In_opt_ volatile BOOLEAN* CancelRequested,
    _Out_ PLECS65_DMA_PREPARE_RESULT Result);

/*
 * Persistent owner for the development-gated single-channel MAM path. The
 * default build gate prevents Execute from being called. Exact normal
 * completion permits WDM mapping retirement and another generation; timeout,
 * cancellation or uncertain post-GO state permanently quarantines ownership.
 */
NTSTATUS LecDmaPnpExecuteLegacyTransfer(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _In_ ULONG_PTR OwnerId,
    _In_ ULONG_PTR FileId,
    _In_opt_ volatile BOOLEAN* CancelRequested,
    _In_ LECS65_DMA_LEGACY_TRANSFER_EXECUTE Execute,
    _Inout_ PVOID ExecuteContext,
    _Out_ PLECS65_DMA_PREPARE_RESULT Result);

/* Terminal fail-closed transition; never drains or releases child resources. */
NTSTATUS LecDmaPnpQuarantineUnknownActive(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);

VOID LecDmaPnpSnapshot(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _Out_ PLECS65_DMA_PNP_SNAPSHOT Snapshot);

BOOLEAN LecDmaPnpCanDestroy(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);
