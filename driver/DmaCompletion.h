#pragma once

/*
 * Software evidence for one live legacy DMA launch.
 *
 * CompletionObserved records a matching physical interrupt. It is not a
 * hardware-idle certificate. Only an independent board/platform contract may
 * authorize the IdleProved transition.
 */
typedef enum _LECS65_DMA_COMPLETION_PHASE {
    LecDmaCompletionNeverLaunched = 0,
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

BOOLEAN LecDmaCompletionMarkDeviceActive(
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
