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

static DWORD WINAPI consumeWorker(void* argument)
{
    RACE_CALL* call = (RACE_CALL*)argument;
    WaitForSingleObject(call->Start, INFINITE);
    call->Result = LecDmaCompletionConsumeSignal(
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

static BOOLEAN activate(
    PLECS65_DMA_COMPLETION_TRACKER tracker,
    ULONGLONG generation)
{
    return (BOOLEAN)(LecDmaCompletionArm(tracker, generation) &&
        LecDmaCompletionPublishDeviceActive(tracker, generation));
}

int main(void)
{
    LECS65_DMA_COMPLETION_TRACKER tracker;
    RACE_CALL call;
    RACE_CALL consumers[2];
    HANDLE worker;
    HANDLE workers[2];
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
    check("pre-GO IRQ poisons armed ownership without signaling",
        LecDmaCompletionArm(&tracker, 2) &&
        !LecDmaCompletionObservePhysicalIrq(&tracker, 2) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionConsumeSignal(&tracker, 2) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 2));

    reset(&tracker, 3);
    check("IRQ between modeled GO and active publication fails closed",
        LecDmaCompletionArm(&tracker, 3) &&
        !LecDmaCompletionObservePhysicalIrq(&tracker, 3) &&
        !LecDmaCompletionPublishDeviceActive(&tracker, 3) &&
        LecDmaCompletionFinishWait(&tracker, 3, FALSE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive);

    reset(&tracker, 4);
    check("cancelled pre-launch arm restores releasable no-launch state",
        LecDmaCompletionArm(&tracker, 4) &&
        LecDmaCompletionCancelArm(&tracker, 4) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionNeverLaunched &&
        LecDmaCompletionMayReleaseMapping(&tracker, 4));

    reset(&tracker, 5);
    call.Tracker = &tracker;
    call.Generation = 5;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Result = FALSE;
    worker = CreateThread(NULL, 0, observeWorker, &call, 0, NULL);
    if (call.Start == NULL || worker == NULL) {
        return 2;
    }
    check("genuine fast IRQ is accepted after synchronized publication",
        LecDmaCompletionArm(&tracker, 5) &&
        LecDmaCompletionPublishDeviceActive(&tracker, 5) &&
        SetEvent(call.Start));
    if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(worker);
    CloseHandle(call.Start);
    check("queued fast IRQ records matching completion evidence",
        call.Result &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionObserved &&
        LecDmaCompletionConsumeSignal(&tracker, 5));

    reset(&tracker, 6);
    check("physical IRQ is recorded only for DeviceActive generation",
        activate(&tracker, 6) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 6) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionObserved);
    check("IRQ immediately before timeout still becomes UnknownActive",
        LecDmaCompletionFinishWait(&tracker, 6, FALSE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 6));

    reset(&tracker, 7);
    check("completion event without physical IRQ becomes UnknownActive",
        activate(&tracker, 7) &&
        LecDmaCompletionFinishWait(&tracker, 7, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive);

    reset(&tracker, 8);
    check("matching completion can signal exactly once",
        activate(&tracker, 8) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 8) &&
        !LecDmaCompletionObservePhysicalIrq(&tracker, 8) &&
        LecDmaCompletionConsumeSignal(&tracker, 8) &&
        !LecDmaCompletionConsumeSignal(&tracker, 8));
    check("duplicate IRQ after consumption cannot republish completion",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 8) &&
        !LecDmaCompletionConsumeSignal(&tracker, 8));
    check("repeated completion never establishes IdleProved",
        !LecDmaCompletionFinishWait(&tracker, 8, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionObserved &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 8));

    reset(&tracker, 9);
    check("wrong-generation consumer preserves matching IRQ evidence",
        activate(&tracker, 9) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 9) &&
        !LecDmaCompletionConsumeSignal(&tracker, 8) &&
        LecDmaCompletionConsumeSignal(&tracker, 9));

    reset(&tracker, 10);
    if (!activate(&tracker, 10) ||
        !LecDmaCompletionObservePhysicalIrq(&tracker, 10)) {
        return 2;
    }
    consumers[0].Tracker = &tracker;
    consumers[0].Generation = 10;
    consumers[0].Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    consumers[0].Result = FALSE;
    consumers[1] = consumers[0];
    workers[0] = CreateThread(
        NULL, 0, consumeWorker, &consumers[0], 0, NULL);
    workers[1] = CreateThread(
        NULL, 0, consumeWorker, &consumers[1], 0, NULL);
    if (consumers[0].Start == NULL ||
        workers[0] == NULL || workers[1] == NULL) {
        return 2;
    }
    SetEvent(consumers[0].Start);
    if (WaitForMultipleObjects(2, workers, TRUE, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(workers[0]);
    CloseHandle(workers[1]);
    CloseHandle(consumers[0].Start);
    check("concurrent consumers permit exactly one matching signal",
        consumers[0].Result != consumers[1].Result &&
        !LecDmaCompletionConsumeSignal(&tracker, 10));

    check("next transfer receives a distinct software generation",
        LecDmaCompletionPrepare(&tracker, 11) &&
        activate(&tracker, 11));
    check("stale IRQ generation cannot complete the current transfer",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 10) &&
        !LecDmaCompletionConsumeSignal(&tracker, 10) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionDeviceActive);
    check("wrong transfer generation cannot authorize mapping release",
        !LecDmaCompletionMayReleaseMapping(&tracker, 10) &&
        !LecDmaCompletionProveIdle(&tracker, 10));

    reset(&tracker, 12);
    check("CompletionObserved alone cannot release a WDM mapping",
        activate(&tracker, 12) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 12) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 12));
    check("only an explicit independent idle proof permits release",
        LecDmaCompletionProveIdle(&tracker, 12) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionIdleProved &&
        LecDmaCompletionMayReleaseMapping(&tracker, 12));

    reset(&tracker, 13);
    check("completion versus STOP/REMOVE fixture becomes DeviceActive",
        activate(&tracker, 13));
    call.Tracker = &tracker;
    call.Generation = 13;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Result = FALSE;
    worker = CreateThread(NULL, 0, observeWorker, &call, 0, NULL);
    if (call.Start == NULL || worker == NULL) {
        return 2;
    }
    SetEvent(call.Start);
    check("STOP/REMOVE uncertainty wins against racing completion",
        LecDmaCompletionFinishWait(&tracker, 13, FALSE));
    if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(worker);
    CloseHandle(call.Start);
    check("racing completion leaves terminal UnknownActive ownership",
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 13));

    call.Tracker = &tracker;
    call.Generation = 13;
    call.Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    call.Result = FALSE;
    worker = CreateThread(NULL, 0, quarantineWorker, &call, 0, NULL);
    if (call.Start == NULL || worker == NULL) {
        return 2;
    }
    SetEvent(call.Start);
    for (attempt = 0; attempt < 10000; ++attempt) {
        if (LecDmaCompletionMayReleaseMapping(&tracker, 13)) {
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
        !LecDmaCompletionMayReleaseMapping(&tracker, 13));
    check("late IRQ cannot recover or reuse quarantined generation",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 13) &&
        !LecDmaCompletionPrepare(&tracker, 14));

    reset(&tracker, 20);
    check("successful wait and mask cleanup preserve legacy success",
        activate(&tracker, 20) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 20) &&
        LecDmaCompletionConsumeSignal(&tracker, 20) &&
        !LecDmaCompletionFinishCleanup(&tracker, 20, TRUE, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) == LecDmaCompletionObserved &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 20));
    check("successful cleanup permits the next serialized legacy generation",
        LecDmaCompletionPrepare(&tracker, 21) && activate(&tracker, 21));
    check("old completion cannot signal the next legacy generation",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 20) &&
        !LecDmaCompletionConsumeSignal(&tracker, 20) &&
        !LecDmaCompletionConsumeSignal(&tracker, 21));

    reset(&tracker, 22);
    check("successful wait followed by failed masking requires quarantine",
        activate(&tracker, 22) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 22) &&
        LecDmaCompletionConsumeSignal(&tracker, 22) &&
        LecDmaCompletionFinishCleanup(&tracker, 22, TRUE, FALSE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 22));
    check("mask failure blocks next acquisition and subsequent success",
        !LecDmaCompletionPrepare(&tracker, 23) &&
        LecDmaCompletionFinishCleanup(&tracker, 22, TRUE, TRUE) &&
        !LecDmaCompletionProveIdle(&tracker, 22));
    check("late or duplicate IRQ cannot rescue failed cleanup",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 22) &&
        !LecDmaCompletionConsumeSignal(&tracker, 22));
    check("quarantine after mask failure is irreversible and idempotent",
        LecDmaCompletionMarkQuarantined(&tracker, 22) &&
        LecDmaCompletionMarkQuarantined(&tracker, 22) &&
        LecDmaCompletionFinishCleanup(&tracker, 22, TRUE, TRUE) &&
        !LecDmaCompletionPrepare(&tracker, 23) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 22));

    reset(&tracker, 24);
    check("IRQ before timeout remains unknown after successful masking",
        activate(&tracker, 24) &&
        LecDmaCompletionObservePhysicalIrq(&tracker, 24) &&
        LecDmaCompletionFinishCleanup(&tracker, 24, FALSE, TRUE) &&
        !LecDmaCompletionConsumeSignal(&tracker, 24) &&
        !LecDmaCompletionPrepare(&tracker, 25));
    reset(&tracker, 26);
    check("IRQ after timeout cannot recover failed wait and mask cleanup",
        activate(&tracker, 26) &&
        LecDmaCompletionFinishCleanup(&tracker, 26, FALSE, FALSE) &&
        !LecDmaCompletionObservePhysicalIrq(&tracker, 26) &&
        !LecDmaCompletionMayReleaseMapping(&tracker, 26));
    reset(&tracker, 27);
    check("mask success plus synthetic event cannot establish completion",
        activate(&tracker, 27) &&
        LecDmaCompletionFinishCleanup(&tracker, 27, TRUE, TRUE) &&
        LecDmaCompletionReadPhase(&tracker) ==
            LecDmaCompletionUnknownActive);
    reset(&tracker, 28);
    check("cancelled pre-GO arm still permits NeverLaunched cleanup",
        LecDmaCompletionArm(&tracker, 28) &&
        LecDmaCompletionCancelArm(&tracker, 28) &&
        LecDmaCompletionMayReleaseMapping(&tracker, 28));
    check("invalid cleanup generation fails closed without changing owner",
        LecDmaCompletionFinishCleanup(&tracker, 29, TRUE, TRUE) &&
        LecDmaCompletionMayReleaseMapping(&tracker, 28) &&
        LecDmaCompletionFinishCleanup(NULL, 28, TRUE, TRUE));

    reset(&tracker, 30);
    if (!activate(&tracker, 30)) {
        return 2;
    }
    consumers[0].Tracker = &tracker;
    consumers[0].Generation = 30;
    consumers[0].Start = CreateEvent(NULL, TRUE, FALSE, NULL);
    consumers[0].Result = FALSE;
    consumers[1] = consumers[0];
    workers[0] = CreateThread(NULL, 0, observeWorker, &consumers[0], 0, NULL);
    workers[1] = CreateThread(NULL, 0, observeWorker, &consumers[1], 0, NULL);
    if (consumers[0].Start == NULL || workers[0] == NULL || workers[1] == NULL) {
        return 2;
    }
    SetEvent(consumers[0].Start);
    if (WaitForMultipleObjects(2, workers, TRUE, 5000) != WAIT_OBJECT_0) {
        return 2;
    }
    CloseHandle(workers[0]);
    CloseHandle(workers[1]);
    CloseHandle(consumers[0].Start);
    check("concurrent IRQ observations publish exactly one completion",
        consumers[0].Result != consumers[1].Result &&
        LecDmaCompletionConsumeSignal(&tracker, 30));
    check("concurrent duplicate publication cannot replenish consumed evidence",
        !LecDmaCompletionObservePhysicalIrq(&tracker, 30) &&
        !LecDmaCompletionConsumeSignal(&tracker, 30) &&
        !LecDmaCompletionFinishCleanup(&tracker, 30, TRUE, TRUE));

    printf("DMA COMPLETION: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
