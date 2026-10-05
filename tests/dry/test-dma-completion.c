#include <stdio.h>
#include "dma-completion-mock.h"
#include "../../driver/DmaCompletion.h"

static unsigned passed;
static unsigned failed;

typedef struct _RACE_CALL {
    PLECS65_DMA_COMPLETION_TRACKER Tracker;
    ULONGLONG Generation;
    HANDLE Start;
    BOOLEAN Result;
} RACE_CALL;

static void check(const char* label, int condition)
{
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (condition) {
        ++passed;
    }
    else {
        ++failed;
    }
}

static DWORD WINAPI observeWorker(void* argument)
{
    RACE_CALL* call = (RACE_CALL*)argument;
    WaitForSingleObject(call->Start, INFINITE);
    call->Result = LecDmaCompletionObservePhysicalIrq(
        call->Tracker,
        call->Generation);
    return 0;
}

static DWORD WINAPI quarantineWorker(void* argument)
{
    RACE_CALL* call = (RACE_CALL*)argument;
    WaitForSingleObject(call->Start, INFINITE);
    call->Result = LecDmaCompletionMarkQuarantined(
        call->Tracker,
        call->Generation);
    return 0;
}

static void reset(
    PLECS65_DMA_COMPLETION_TRACKER tracker,
    ULONGLONG generation)
{
    LecDmaCompletionInitialize(tracker);
    if (!LecDmaCompletionPrepare(tracker, generation)) {
        fprintf(
            stderr,
            "Failed to prepare generation %llu.\n",
            (unsigned long long)generation);
        ExitProcess(2);
    }
}

int main(void)
{
    LECS65_DMA_COMPLETION_TRACKER tracker;
    RACE_CALL call;
    HANDLE worker;
    unsigned attempt;

    (void)setvbuf(stdout, NULL, _IONBF, 0);

    reset(&tracker, 1);
    check("NeverLaunched no-launch mapping may be released",
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionNeverLaunched &&
        LecDmaCompletionMayReleaseMapping(&tracker, 1));
    check("early completion before DeviceActive is rejected",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 1) &&
        !LecDmaCompletionConsumeSignal(&tracker, 1));

    reset(&tracker, 2);
    check("physical IRQ is recorded only for DeviceActive generation",
        LecDmaCompletionMarkDeviceActive(&tracker, 2) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 2) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionObserved);
    check("IRQ immediately before timeout still becomes UnknownActive",
        LecDmaCompletionFinishWait(&tracker, 2, FALSE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 2));

    reset(&tracker, 3);
    check("completion event without physical IRQ becomes UnknownActive",
        LecDmaCompletionMarkDeviceActive(&tracker, 3) &&
        LecDmaCompletionFinishWait(&tracker, 3, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive);

    reset(&tracker, 4);
    check("matching completion can signal exactly once",
        LecDmaCompletionMarkDeviceActive(&tracker, 4) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 4) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 4) &&
        LecDmaCompletionConsumeSignal(&tracker, 4) &&
        !LecDmaCompletionConsumeSignal(&tracker, 4));
    check("repeated completion never establishes IdleProved",
        !LecDmaCompletionFinishWait(&tracker, 4, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionObserved &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 4));

    check("next transfer receives a distinct software generation",
        LecDmaCompletionPrepare(&tracker, 5) &&
        LecDmaCompletionMarkDeviceActive(&tracker, 5));
    check("stale IRQ generation cannot complete the current transfer",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 4) &&
        !LecDmaCompletionConsumeSignal(&tracker, 4) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionDeviceActive);
    check("wrong transfer generation cannot authorize mapping release",
        !LecDmaCompletionMayReleaseMapping(&tracker, 4) &&
        !LecDmaCompletionProveIdle(&tracker, 4));

    reset(&tracker, 6);
    check("CompletionObserved alone cannot release a WDM mapping",
        LecDmaCompletionMarkDeviceActive(&tracker, 6) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 6) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 6));
    check("only an explicit independent idle proof permits release",
        LecDmaCompletionProveIdle(&tracker, 6) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionIdleProved &&
        LecDmaCompletionMayReleaseMapping(&tracker, 6));

    reset(&tracker, 7);
    check("completion versus STOP/REMOVE fixture becomes DeviceActive",
        LecDmaCompletionMarkDeviceActive(&tracker, 7));
    call.Tracker = &tracker;
    call.Generation = 7;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Result = FALSE;
    worker = CreateThread(NULL, 0, observeWorker, &call, 0, NULL);
    if (call.Start == NULL || worker == NULL) {
        return 2;
    }
    SetEvent(call.Start);
    check("STOP/REMOVE uncertainty wins against racing completion",
        LecDmaCompletionFinishWait(&tracker, 7, FALSE));
    if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(worker);
    CloseHandle(call.Start);
    check("racing completion leaves terminal UnknownActive ownership",
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 7));

    call.Tracker = &tracker;
    call.Generation = 7;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Result = FALSE;
    worker = CreateThread(NULL, 0, quarantineWorker, &call, 0, NULL);
    if (call.Start == NULL || worker == NULL) {
        return 2;
    }
    SetEvent(call.Start);
    for (attempt = 0; attempt < 10000; ++attempt) {
        if (LecDmaCompletionMayReleaseMapping(&tracker, 7)) {
            break;
        }
    }
    if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(worker);
    CloseHandle(call.Start);
    check("quarantine racing release never exposes releasable ownership",
        attempt == 10000 && call.Result &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionQuarantined &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 7));
    check("late IRQ cannot recover or reuse quarantined generation",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 7) &&
        !LecDmaCompletionPrepare(&tracker, 8));

    printf("DMA COMPLETION: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
