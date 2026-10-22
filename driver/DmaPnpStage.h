#pragma once

#include "DmaSyncStage.h"

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

typedef struct _LECS65_DMA_PNP_STAGE {
    KSPIN_LOCK Lock;
    PDEVICE_OBJECT PhysicalDeviceObject; /* borrowed identity; context refs it */
    PLECS65_DMA_ADAPTER_CONTEXT AdapterContext;
    LECS65_SG_SYNC_OWNER SyncOwner;
    LECS65_DMA_PNP_STATE State;
    ULONG ActiveCalls;
    ULONG Generation;
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
} LECS65_DMA_PNP_STAGE, *PLECS65_DMA_PNP_STAGE;

typedef struct _LECS65_DMA_PNP_SNAPSHOT {
    LECS65_DMA_PNP_STATE State;
    ULONG ActiveCalls;
    ULONG Generation;
    BOOLEAN AdmissionOpen;
    BOOLEAN HasAdapterContext;
    BOOLEAN CleanupActive;
    BOOLEAN SyncOwnerDetached;
    BOOLEAN InterruptDisconnected;
    BOOLEAN DpcDrained;
    BOOLEAN TimerStopped;
    BOOLEAN UnknownActive;
    BOOLEAN LateQuarantine;
    LECS65_DMA_PNP_CLEANUP_PHASE CleanupPhase;
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

/* Terminal fail-closed transition; never drains or releases child resources. */
NTSTATUS LecDmaPnpQuarantineUnknownActive(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);

VOID LecDmaPnpSnapshot(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _Out_ PLECS65_DMA_PNP_SNAPSHOT Snapshot);

BOOLEAN LecDmaPnpCanDestroy(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);
