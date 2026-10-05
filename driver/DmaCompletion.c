#if defined(LECS65_DMA_COMPLETION_HOST_TEST)
#include "../tests/dry/dma-completion-mock.h"
#else
#include <ntddk.h>
#endif

#include "DmaCompletion.h"

static LONG
LecDmaCompletionReadLong(_In_ const volatile LONG* Value)
{
    return InterlockedCompareExchange((volatile LONG*)Value, 0, 0);
}

static LONG64
LecDmaCompletionReadLong64(_In_ const volatile LONG64* Value)
{
    return InterlockedCompareExchange64((volatile LONG64*)Value, 0, 0);
}

static BOOLEAN
LecDmaCompletionGenerationMatches(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker,
    _In_ ULONGLONG Generation)
{
    return (BOOLEAN)(Generation != 0 &&
        (ULONGLONG)LecDmaCompletionReadLong64(&Tracker->Generation) ==
            Generation);
}

VOID
LecDmaCompletionInitialize(
    _Out_ PLECS65_DMA_COMPLETION_TRACKER Tracker)
{
    if (Tracker == NULL) {
        return;
    }

    Tracker->Generation = 0;
    Tracker->IrqGeneration = 0;
    Tracker->Phase = LecDmaCompletionNeverLaunched;
}

BOOLEAN
LecDmaCompletionPrepare(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    LONG phase;

    if (Tracker == NULL || Generation == 0) {
        return FALSE;
    }

    phase = LecDmaCompletionReadLong(&Tracker->Phase);
    if (phase == LecDmaCompletionArming ||
        phase == LecDmaCompletionDeviceActive ||
        phase == LecDmaCompletionUnknownActive ||
        phase == LecDmaCompletionQuarantined) {
        return FALSE;
    }

    (void)InterlockedExchange64(&Tracker->IrqGeneration, 0);
    (void)InterlockedExchange64(
        &Tracker->Generation, (LONG64)Generation);
    (void)InterlockedExchange(
        &Tracker->Phase, LecDmaCompletionNeverLaunched);
    return TRUE;
}

BOOLEAN
LecDmaCompletionArm(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    return (BOOLEAN)(InterlockedCompareExchange(
        &Tracker->Phase,
        LecDmaCompletionArming,
        LecDmaCompletionNeverLaunched) ==
        LecDmaCompletionNeverLaunched);
}

BOOLEAN
LecDmaCompletionCancelArm(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    return (BOOLEAN)(InterlockedCompareExchange(
        &Tracker->Phase,
        LecDmaCompletionNeverLaunched,
        LecDmaCompletionArming) ==
        LecDmaCompletionArming);
}

BOOLEAN
LecDmaCompletionIsArmed(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker,
    _In_ ULONGLONG Generation)
{
    return (BOOLEAN)(Tracker != NULL &&
        LecDmaCompletionGenerationMatches(Tracker, Generation) &&
        LecDmaCompletionReadLong(&Tracker->Phase) ==
            LecDmaCompletionArming);
}

BOOLEAN
LecDmaCompletionPublishDeviceActive(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    return (BOOLEAN)(InterlockedCompareExchange(
        &Tracker->Phase,
        LecDmaCompletionDeviceActive,
        LecDmaCompletionArming) ==
        LecDmaCompletionArming);
}

BOOLEAN
LecDmaCompletionObservePhysicalIrq(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    LONG phase;

    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    /*
     * An IRQ observed while launch is only armed predates the software GO
     * commit boundary.  Its provenance is not safe to attribute to this
     * transfer, so poison ownership without publishing signal evidence.
     */
    if (InterlockedCompareExchange(
            &Tracker->Phase,
            LecDmaCompletionUnknownActive,
            LecDmaCompletionArming) ==
        LecDmaCompletionArming) {
        return FALSE;
    }

    phase = InterlockedCompareExchange(
        &Tracker->Phase,
        LecDmaCompletionObserved,
        LecDmaCompletionDeviceActive);
    if (phase != LecDmaCompletionDeviceActive &&
        phase != LecDmaCompletionObserved) {
        return FALSE;
    }

    (void)InterlockedExchange64(
        &Tracker->IrqGeneration, (LONG64)Generation);
    return TRUE;
}

BOOLEAN
LecDmaCompletionConsumeSignal(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    LONG64 irqGeneration;

    if (Tracker == NULL || Generation == 0) {
        return FALSE;
    }

    irqGeneration = LecDmaCompletionReadLong64(
        &Tracker->IrqGeneration);
    if ((ULONGLONG)irqGeneration != Generation ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation) ||
        LecDmaCompletionReadLong(&Tracker->Phase) !=
            LecDmaCompletionObserved) {
        return FALSE;
    }

    return (BOOLEAN)(InterlockedCompareExchange64(
        &Tracker->IrqGeneration,
        0,
        (LONG64)Generation) == (LONG64)Generation);
}

BOOLEAN
LecDmaCompletionFinishWait(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation,
    _In_ BOOLEAN WaitSucceeded)
{
    LONG phase;

    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return TRUE;
    }

    phase = LecDmaCompletionReadLong(&Tracker->Phase);
    if (WaitSucceeded && phase == LecDmaCompletionObserved) {
        return FALSE;
    }

    for (;;) {
        if (phase == LecDmaCompletionUnknownActive ||
            phase == LecDmaCompletionQuarantined) {
            return TRUE;
        }
        if (phase != LecDmaCompletionArming &&
            phase != LecDmaCompletionDeviceActive &&
            phase != LecDmaCompletionObserved) {
            return TRUE;
        }
        if (InterlockedCompareExchange(
                &Tracker->Phase,
                LecDmaCompletionUnknownActive,
                phase) == phase) {
            return TRUE;
        }
        phase = LecDmaCompletionReadLong(&Tracker->Phase);
    }
}

BOOLEAN
LecDmaCompletionProveIdle(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    LONG phase;

    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    phase = LecDmaCompletionReadLong(&Tracker->Phase);
    while (phase == LecDmaCompletionDeviceActive ||
           phase == LecDmaCompletionObserved) {
        if (InterlockedCompareExchange(
                &Tracker->Phase,
                LecDmaCompletionIdleProved,
                phase) == phase) {
            return TRUE;
        }
        phase = LecDmaCompletionReadLong(&Tracker->Phase);
    }
    return (BOOLEAN)(phase == LecDmaCompletionIdleProved);
}

BOOLEAN
LecDmaCompletionMarkQuarantined(
    _Inout_ PLECS65_DMA_COMPLETION_TRACKER Tracker,
    _In_ ULONGLONG Generation)
{
    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    if (InterlockedCompareExchange(
            &Tracker->Phase,
            LecDmaCompletionQuarantined,
            LecDmaCompletionUnknownActive) ==
        LecDmaCompletionUnknownActive) {
        return TRUE;
    }
    return (BOOLEAN)(LecDmaCompletionReadLong(&Tracker->Phase) ==
        LecDmaCompletionQuarantined);
}

BOOLEAN
LecDmaCompletionMayReleaseMapping(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker,
    _In_ ULONGLONG Generation)
{
    LONG phase;

    if (Tracker == NULL ||
        !LecDmaCompletionGenerationMatches(Tracker, Generation)) {
        return FALSE;
    }

    phase = LecDmaCompletionReadLong(&Tracker->Phase);
    return (BOOLEAN)(phase == LecDmaCompletionNeverLaunched ||
        phase == LecDmaCompletionIdleProved);
}

LECS65_DMA_COMPLETION_PHASE
LecDmaCompletionReadPhase(
    _In_ const LECS65_DMA_COMPLETION_TRACKER* Tracker)
{
    if (Tracker == NULL) {
        return LecDmaCompletionQuarantined;
    }
    return (LECS65_DMA_COMPLETION_PHASE)LecDmaCompletionReadLong(
        &Tracker->Phase);
}
