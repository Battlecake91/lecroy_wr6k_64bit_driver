#include <stdio.h>
#include <string.h>
#include "pnp-irp-mock.h"

typedef enum _LOWER_MODE {
    LowerSynchronous = 0,
    LowerPending
} LOWER_MODE;

static unsigned passed;
static unsigned failed;
static LOWER_MODE lowerMode;
static NTSTATUS lowerStatus;
static HANDLE lowerEntered;
static HANDLE lowerContinue;
static HANDLE lowerWorker;
static volatile LONG ioCalls;
static volatile LONG powerCalls;
static volatile LONG completionCalls;
static volatile LONG completionResult;

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

static int waitForHandle(HANDLE handle, const char* label)
{
    if (handle == NULL ||
        WaitForSingleObject(handle, 5000) != WAIT_OBJECT_0) {
        fprintf(stderr, "Timed out waiting for %s.\n", label);
        return 0;
    }
    return 1;
}

static DWORD WINAPI completePendingIrp(void* argument)
{
    PIRP irp = (PIRP)argument;

    SetEvent(lowerEntered);
    WaitForSingleObject(lowerContinue, INFINITE);
    irp->PendingReturned = TRUE;
    irp->IoStatus.Status = lowerStatus;
    InterlockedIncrement(&completionCalls);
    InterlockedExchange(
        &completionResult,
        irp->CompletionRoutine(NULL, irp, irp->CompletionContext));
    return 0;
}

static NTSTATUS callLower(PIRP irp)
{
    if (lowerMode == LowerSynchronous) {
        irp->IoStatus.Status = lowerStatus;
        InterlockedIncrement(&completionCalls);
        InterlockedExchange(
            &completionResult,
            irp->CompletionRoutine(NULL, irp, irp->CompletionContext));
        return lowerStatus;
    }

    lowerWorker = CreateThread(
        NULL, 0, completePendingIrp, irp, 0, NULL);
    return lowerWorker != NULL ? STATUS_PENDING : STATUS_UNSUCCESSFUL;
}

NTSTATUS IoCallDriver(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);
    InterlockedIncrement(&ioCalls);
    return callLower(irp);
}

NTSTATUS PoCallDriver(PDEVICE_OBJECT deviceObject, PIRP irp)
{
    UNREFERENCED_PARAMETER(deviceObject);
    InterlockedIncrement(&powerCalls);
    return callLower(irp);
}

typedef struct _WAIT_CALL {
    PLECS65_DEVICE_EXTENSION DevExt;
    PIRP Irp;
    NTSTATUS Status;
} WAIT_CALL;

static DWORD WINAPI forwardAndWaitWorker(void* argument)
{
    WAIT_CALL* call = (WAIT_CALL*)argument;
    call->Status = LecForwardAndWait(call->DevExt, call->Irp);
    return 0;
}

static void resetFixture(
    PLECS65_DEVICE_EXTENSION devExt,
    PIRP irp,
    NTSTATUS status,
    LOWER_MODE mode)
{
    memset(devExt, 0, sizeof(*devExt));
    memset(irp, 0, sizeof(*irp));
    devExt->LowerDeviceObject = devExt;
    devExt->RemoveLock.Outstanding = 1;
    lowerStatus = status;
    lowerMode = mode;
    ResetEvent(lowerEntered);
    ResetEvent(lowerContinue);
    lowerWorker = NULL;
    ioCalls = 0;
    powerCalls = 0;
    completionCalls = 0;
    completionResult = STATUS_UNSUCCESSFUL;
}

int main(void)
{
    LECS65_DEVICE_EXTENSION devExt;
    IRP irp;
    WAIT_CALL call;
    HANDLE caller;
    NTSTATUS status;

    (void)setvbuf(stdout, NULL, _IONBF, 0);
    lowerEntered = CreateEvent(NULL, TRUE, FALSE, NULL);
    lowerContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (lowerEntered == NULL || lowerContinue == NULL) {
        return 1;
    }

    resetFixture(&devExt, &irp, STATUS_SUCCESS, LowerSynchronous);
    status = LecForwardLockedIrp(&devExt, &irp, FALSE);
    check("synchronous forwarding releases remove lock exactly once",
        status == STATUS_SUCCESS && ioCalls == 1 && powerCalls == 0 &&
        completionCalls == 1 && completionResult == STATUS_SUCCESS &&
        devExt.RemoveLock.Outstanding == 0 &&
        devExt.RemoveLock.ReleaseCalls == 1 && irp.StackCopies == 1 &&
        !irp.MarkedPending);

    resetFixture(&devExt, &irp, STATUS_UNSUCCESSFUL, LowerSynchronous);
    status = LecForwardLockedIrp(&devExt, &irp, FALSE);
    check("error completion still balances the remove lock",
        status == STATUS_UNSUCCESSFUL && completionCalls == 1 &&
        devExt.RemoveLock.Outstanding == 0 &&
        devExt.RemoveLock.ReleaseCalls == 1);

    resetFixture(&devExt, &irp, STATUS_SUCCESS, LowerSynchronous);
    status = LecForwardLockedIrp(&devExt, &irp, TRUE);
    check("power forwarding uses PoCallDriver and the same rundown",
        status == STATUS_SUCCESS && ioCalls == 0 && powerCalls == 1 &&
        devExt.RemoveLock.Outstanding == 0 &&
        devExt.RemoveLock.ReleaseCalls == 1);

    resetFixture(&devExt, &irp, STATUS_SUCCESS, LowerPending);
    status = LecForwardLockedIrp(&devExt, &irp, FALSE);
    if (!waitForHandle(lowerEntered, "pending lower dispatch")) return 1;
    check("pending forwarding retains remove lock until completion",
        status == STATUS_PENDING && devExt.RemoveLock.Outstanding == 1 &&
        devExt.RemoveLock.ReleaseCalls == 0);
    SetEvent(lowerContinue);
    if (!waitForHandle(lowerWorker, "pending lower completion")) return 1;
    CloseHandle(lowerWorker);
    check("pending completion marks IRP and releases exactly once",
        irp.MarkedPending && completionCalls == 1 &&
        completionResult == STATUS_SUCCESS &&
        devExt.RemoveLock.Outstanding == 0 &&
        devExt.RemoveLock.ReleaseCalls == 1);

    resetFixture(&devExt, &irp, STATUS_UNSUCCESSFUL, LowerSynchronous);
    status = LecForwardAndWait(&devExt, &irp);
    check("synchronous forward-and-wait retains IRP ownership",
        status == STATUS_UNSUCCESSFUL && completionCalls == 1 &&
        completionResult == STATUS_MORE_PROCESSING_REQUIRED &&
        devExt.RemoveLock.Outstanding == 1 &&
        devExt.RemoveLock.ReleaseCalls == 0 && irp.StackCopies == 1);

    resetFixture(&devExt, &irp, STATUS_SUCCESS, LowerPending);
    call.DevExt = &devExt;
    call.Irp = &irp;
    call.Status = STATUS_UNSUCCESSFUL;
    caller = CreateThread(NULL, 0, forwardAndWaitWorker, &call, 0, NULL);
    if (!waitForHandle(lowerEntered, "forward-and-wait lower dispatch")) {
        return 1;
    }
    check("pending forward-and-wait blocks without releasing remove lock",
        caller != NULL && call.Status == STATUS_UNSUCCESSFUL &&
        devExt.RemoveLock.Outstanding == 1 &&
        devExt.RemoveLock.ReleaseCalls == 0);
    SetEvent(lowerContinue);
    if (!waitForHandle(caller, "forward-and-wait caller") ||
        !waitForHandle(lowerWorker, "forward-and-wait completion")) {
        return 1;
    }
    CloseHandle(caller);
    CloseHandle(lowerWorker);
    check("pending forward-and-wait returns final status and held IRP",
        call.Status == STATUS_SUCCESS && completionCalls == 1 &&
        completionResult == STATUS_MORE_PROCESSING_REQUIRED &&
        devExt.RemoveLock.Outstanding == 1 &&
        devExt.RemoveLock.ReleaseCalls == 0);

    CloseHandle(lowerEntered);
    CloseHandle(lowerContinue);
    printf("PNP IRP LIFETIME: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed == 0 ? 0 : 1;
}
