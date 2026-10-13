#pragma once

/*
 * Software evidence for one live legacy DMA launch.
 *
 * CompletionObserved records a matching physical interrupt. It is not a
 * hardware-idle certificate. Only an independent board/platform contract may
 * authorize the IdleProved transition.
 *
 * These interlocked fields are not a lock-free multi-generation owner API.
 * Initialize only unpublished storage. The transfer owner serializes Prepare,
 * arming and finalization with TransferMutex; selection/deselection and DPC
 * event access also share DmaCompletionLock. The interrupt synchronization
 * domain excludes an in-flight old ISR across GO/active publication.
 * Observe/Consume may finish concurrently with failed finalization: their
 * return value alone must never authorize resource release. UnknownActive
 * remains authoritative even if an in-flight operation returns TRUE.
 */
typedef enum _LECS65_DMA_COMPLETION_PHASE {
    LecDmaCompletionNeverLaunched = 0,
    LecDmaCompletionArming,
    LecDmaCompletionDeviceActive,
    LecDmaCompletionObserved,
    LecDmaCompletionIdleProved,
    LecDmaCompletionUnknownActive,
    LecDmaCompletionQuarantined
} LECS65_DMA_COMPLETION_PHASE;

typedef struct _LECS65_DMA_COMPLETION_TRACKER {
    volatile LONG Phase;
    volatile LONG64 Generation;
    volatile LONG64 IrqGeneration;
} LECS65_DMA_COMPLETION_TRACKER, *PLECS65_DMA_COMPLETION_TRACKER;

VOID LecDmaCompletionInitialize(
    _Out_ PLECS65_DMA_COMPLETION_TRACKER Tracker);

BOOLEAN LecDmaCompletionPrepare(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionArm(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionCancelArm(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionIsArmed(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionPublishDeviceActive(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionObservePhysicalIrq(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionConsumeSignal(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionFinishWait(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation,
    _In_ BOOLEAN WaitSucceeded);

/* After GO: TRUE requires quarantine; FALSE is legacy success, not idle. */
BOOLEAN LecDmaCompletionFinishCleanup(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation,
    _In_ BOOLEAN WaitSucceeded,
    _In_ BOOLEAN InterruptMaskSucceeded);

BOOLEAN LecDmaCompletionProveIdle(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionMarkQuarantined(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation);

BOOLEAN LecDmaCompletionMayReleaseMapping(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker,
    _In_ ULONGLONG Generation);

LECS65_DMA_COMPLETION_PHASE LecDmaCompletionReadPhase(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker);
