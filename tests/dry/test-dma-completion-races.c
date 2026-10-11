#include <stdio.h>
#include <stdlib.h>
#include <intrin.h>
#include "dma-completion-mock.h"
#include "../../driver/DmaCompletion.h"

typedef enum { Observe, Consume, MaskFailure, Timeout } OPERATION;
typedef struct {
    PLECS65_DMA_COMPLETION_TRACKER Tracker;
    ULONGLONG Generation;
    HANDLE Start;
    OPERATION Operation;
    BOOLEAN Result;
} CALL;

static DWORD hookThread;
static volatile LONG* hookPhase;
static unsigned hookKind;
static HANDLE hookEntered;
static HANDLE hookResume;

/* The production C still executes native Windows interlocked instructions.
 * Only the selected test thread is paused after one completed instruction. */
LONG LecTestCompareExchange(volatile LONG* value, LONG exchange, LONG compare)
{
    LONG result = _InterlockedCompareExchange(value, exchange, compare);
    if (hookKind != 0 && GetCurrentThreadId() == hookThread &&
        value == hookPhase &&
        ((hookKind == 1 && result == LecDmaCompletionDeviceActive &&
          exchange == LecDmaCompletionObserved &&
          compare == LecDmaCompletionDeviceActive) ||
         (hookKind == 2 && result == LecDmaCompletionObserved &&
          exchange == 0 && compare == 0))) {
        if (!SetEvent(hookEntered) ||
            WaitForSingleObject(hookResume, 5000) != WAIT_OBJECT_0) {
            ExitProcess(2);
        }
    }
    return result;
}

static void activate(PLECS65_DMA_COMPLETION_TRACKER tracker, ULONGLONG generation)
{
    LecDmaCompletionInitialize(tracker);
    if (!LecDmaCompletionPrepare(tracker, generation) ||
        !LecDmaCompletionArm(tracker, generation) ||
        !LecDmaCompletionPublishDeviceActive(tracker, generation)) {
        ExitProcess(2);
    }
}

static DWORD WINAPI worker(void* argument)
{
    CALL* call = (CALL*)argument;
    if (WaitForSingleObject(call->Start, 5000) != WAIT_OBJECT_0) {
        ExitProcess(2);
    }
    switch (call->Operation) {
    case Observe:
        call->Result = LecDmaCompletionObservePhysicalIrq(call->Tracker, call->Generation);
        break;
    case Consume:
        call->Result = LecDmaCompletionConsumeSignal(call->Tracker, call->Generation);
        break;
    case MaskFailure:
        call->Result = LecDmaCompletionFinishCleanup(call->Tracker, call->Generation, TRUE, FALSE);
        break;
    case Timeout:
        call->Result = LecDmaCompletionFinishCleanup(call->Tracker, call->Generation, FALSE, TRUE);
        break;
    }
    return 0;
}

static BOOLEAN pausedCase(unsigned kind)
{
    LECS65_DMA_COMPLETION_TRACKER tracker;
    CALL call;
    HANDLE thread;
    BOOLEAN poisoned;
    BOOLEAN safe;

    activate(&tracker, 100);
    if (kind == 2 && !LecDmaCompletionObservePhysicalIrq(&tracker, 100)) {
        ExitProcess(2);
    }
    call.Tracker = &tracker;
    call.Generation = 100;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Operation = kind == 1 ? Observe : Consume;
    call.Result = FALSE;
    hookEntered = CreateEvent(NULL, TRUE, FALSE, NULL);
    hookResume = CreateEvent(NULL, TRUE, FALSE, NULL);
    hookPhase = &tracker.Phase;
    hookKind = kind;
    thread = CreateThread(NULL, 0, worker, &call, 0, NULL);
    if (call.Start == NULL || hookEntered == NULL || hookResume == NULL || thread == NULL) {
        ExitProcess(2);
    }
    hookThread = GetThreadId(thread);
    SetEvent(call.Start);
    if (WaitForSingleObject(hookEntered, 5000) != WAIT_OBJECT_0) {
        ExitProcess(2);
    }
    poisoned = LecDmaCompletionFinishCleanup(&tracker, 100, FALSE, FALSE);
    safe = (BOOLEAN)(poisoned &&
        LecDmaCompletionReadPhase(&tracker) == LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 100));
    SetEvent(hookResume);
    if (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) {
        ExitProcess(2);
    }
    hookKind = 0;
    safe = (BOOLEAN)(safe && call.Result &&
        LecDmaCompletionReadPhase(&tracker) == LecDmaCompletionUnknownActive &&
        !LecDmaCompletionConsumeSignal(&tracker, 100) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 100) &&
        !LecDmaCompletionPrepare(&tracker, 101));
    CloseHandle(thread);
    CloseHandle(call.Start);
    CloseHandle(hookEntered);
    CloseHandle(hookResume);
    return safe;
}

static void race(CALL* calls)
{
    HANDLE threads[2];
    calls[0].Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    calls[1].Start = calls[0].Start;
    calls[0].Result = calls[1].Result = FALSE;
    threads[0] = CreateThread(NULL, 0, worker, &calls[0], 0, NULL);
    threads[1] = CreateThread(NULL, 0, worker, &calls[1], 0, NULL);
    if (calls[0].Start == NULL || threads[0] == NULL || threads[1] == NULL) {
        ExitProcess(2);
    }
    SetEvent(calls[0].Start);
    if (WaitForMultipleObjects(2, threads, TRUE, 5000) != WAIT_OBJECT_0) {
        ExitProcess(2);
    }
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(calls[0].Start);
}

int main(int argc, char** argv)
{
    static const char* names[] = {
        "simultaneous observations", "observation versus mask failure",
        "observation versus timeout", "consumption versus mask failure",
        "stale observation versus new generation"
    };
    unsigned iterations = argc == 2 ? (unsigned)strtoul(argv[1], NULL, 10) : 1000;
    unsigned deterministic = 0;
    unsigned totalFailed = 0;
    unsigned scenario;
    unsigned iteration;
    BOOLEAN ok;
    LECS65_DMA_COMPLETION_TRACKER tracker;
    CALL calls[2];

    if (iterations == 0 || iterations > 100000) {
        return 2;
    }
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    ok = pausedCase(1);
    deterministic += ok ? 1 : 0;
    printf("[%s] delayed marker publication cannot undo UnknownActive\n", ok ? "PASS" : "FAIL");
    ok = pausedCase(2);
    deterministic += ok ? 1 : 0;
    printf("[%s] in-flight consumer success is not an ownership certificate\n", ok ? "PASS" : "FAIL");
    printf("DMA INTERLEAVINGS: %u/2 passed; %u failed.\n", deterministic, 2 - deterministic);

    for (scenario = 0; scenario < 5; ++scenario) {
        unsigned failed = 0;
        for (iteration = 0; iteration < iterations; ++iteration) {
            activate(&tracker, 200);
            calls[0].Tracker = calls[1].Tracker = &tracker;
            calls[0].Generation = calls[1].Generation = 200;
            calls[0].Operation = Observe;
            calls[1].Operation = scenario == 0 || scenario == 4 ? Observe :
                scenario == 2 ? Timeout : MaskFailure;
            if (scenario == 3 || scenario == 4) {
                if (!LecDmaCompletionObservePhysicalIrq(&tracker, 200)) {
                    return 2;
                }
                if (scenario == 3) {
                    calls[0].Operation = Consume;
                }
                else {
                    if (!LecDmaCompletionPrepare(&tracker, 201) ||
                        !LecDmaCompletionArm(&tracker, 201) ||
                        !LecDmaCompletionPublishDeviceActive(&tracker, 201)) {
                        return 2;
                    }
                    calls[1].Generation = 201;
                }
            }
            race(calls);
            if (scenario == 0) {
                ok = (BOOLEAN)(calls[0].Result != calls[1].Result &&
                    LecDmaCompletionConsumeSignal(&tracker, 200) &&
                    !LecDmaCompletionObservePhysicalIrq(&tracker, 200) &&
                    !LecDmaCompletionConsumeSignal(&tracker, 200));
            }
            else if (scenario == 4) {
                ok = (BOOLEAN)(!calls[0].Result && calls[1].Result &&
                    LecDmaCompletionConsumeSignal(&tracker, 201) &&
                    !LecDmaCompletionFinishCleanup(&tracker, 201, TRUE, TRUE));
            }
            else {
                ok = (BOOLEAN)(calls[1].Result &&
                    LecDmaCompletionReadPhase(&tracker) == LecDmaCompletionUnknownActive &&
                    !LecDmaCompletionConsumeSignal(&tracker, 200) &&
                    !LecDmaCompletionMayReleaseMapping(&tracker, 200) &&
                    LecDmaCompletionMarkQuarantined(&tracker, 200) &&
                    !LecDmaCompletionPrepare(&tracker, 201));
            }
            failed += ok ? 0 : 1;
        }
        totalFailed += failed;
        printf("DMA STRESS %s: %u/%u passed; %u failed.\n",
            names[scenario], iterations - failed, iterations, failed);
    }
    printf("DMA STRESS TOTAL: %u/%u passed; %u failed.\n",
        5 * iterations - totalFailed, 5 * iterations, totalFailed);
    return deterministic == 2 && totalFailed == 0 ? 0 : 1;
}
