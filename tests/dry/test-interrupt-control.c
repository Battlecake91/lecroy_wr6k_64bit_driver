#include <stdio.h>
#include "interrupt-control-mock.h"

static LONG checks;
static LONG failures;
static LONG writes;
static HANDLE writeEntered;
static HANDLE writeResume;
static volatile LONG pauseWrite;
static BOOLEAN failSynchronization;

#define CHECK(condition, name) do { ++checks; if (!(condition)) { \
    ++failures; printf("FAIL: %s\n", name); } } while (0)

BOOLEAN KeSynchronizeExecution(CRITICAL_SECTION* Lock,
    BOOLEAN (*Callback)(PVOID), PVOID Context)
{
    BOOLEAN result;
    if (failSynchronization) return FALSE;
    EnterCriticalSection(Lock);
    result = Callback(Context);
    LeaveCriticalSection(Lock);
    return result;
}

ULONG LecTestReadRegister(volatile ULONG* Register) { return *Register; }
VOID LecTestWriteRegister(volatile ULONG* Register, ULONG Value)
{
    if (InterlockedExchange(&pauseWrite, 0)) {
        SetEvent(writeEntered);
        if (WaitForSingleObject(writeResume, 10000) != WAIT_OBJECT_0) {
            ExitProcess(3);
        }
    }
    *Register = Value;
    InterlockedIncrement(&writes);
}

typedef struct _WORK {
    PLECS65_DEVICE_EXTENSION DevExt;
    ULONG Bits;
    ULONG ClearBits;
    NTSTATUS Status;
    BOOLEAN Acquisition;
    BOOLEAN Absolute;
    BOOLEAN Close;
    BOOLEAN Quarantine;
    HANDLE Entered;
    HANDLE Done;
} WORK;

static DWORD WINAPI Worker(PVOID Context)
{
    WORK* work = (WORK*)Context;
    if (work->Entered) SetEvent(work->Entered);
    if (work->Quarantine) LecQuarantineInterruptAdmission(work->DevExt);
    else if (work->Close) LecSetInterruptAdmission(work->DevExt, FALSE);
    else if (work->Absolute) work->Status = LecWriteInterruptMask(work->DevExt, work->Bits);
    else work->Status = LecUpdateInterruptMask(work->DevExt,
        work->ClearBits, work->Bits, work->Acquisition);
    if (work->Done) SetEvent(work->Done);
    return 0;
}

static VOID Join(HANDLE Thread)
{
    if (WaitForSingleObject(Thread, 10000) != WAIT_OBJECT_0) ExitProcess(4);
    CloseHandle(Thread);
}

int main(int argc, char** argv)
{
    LECS65_DEVICE_EXTENSION dev = {0};
    ULONG registers[64] = {0};
    CRITICAL_SECTION irq;
    ULONG value = 0;
    LONG before;
    ULONG iterations = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000;
    ULONG i;
    InitializeCriticalSection(&dev.InterruptControlLock);
    InitializeCriticalSection(&dev.IoAdmissionLock);
    InitializeCriticalSection(&irq);
    dev.InterruptObject = &irq;
    dev.InterruptConnected = TRUE;
    dev.Started = TRUE;
    dev.Bar[0] = (PUCHAR)registers;
    dev.BarLength[0] = sizeof(registers);
    writeEntered = CreateEvent(NULL, TRUE, FALSE, NULL);
    writeResume = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!writeEntered || !writeResume) return 2;
    LecSetInterruptAdmission(&dev, TRUE);
    CHECK(dev.AcceptIoctls && dev.InterruptWriteAdmission, "start admission");
    CHECK(LecWriteInterruptMask(&dev, 8) == STATUS_SUCCESS,
        "legacy absolute non-DMA write");
    CHECK(registers[0x084 / 4] == 8 && dev.InterruptEnableShadow == 8,
        "generic write updates physical double and shadow");
    CHECK(LecReadInterruptMask(&dev, &value) == STATUS_SUCCESS && value == 8,
        "generic read remains physical");
    before = writes;
    CHECK(LecEnsureReceiveInterrupt(&dev) == STATUS_SUCCESS && writes == before,
        "receive enable retains legacy idempotent MMIO behavior");
    CHECK(LecUpdateInterruptMask(&dev, 0, 1, TRUE) == STATUS_SUCCESS,
        "acquisition enable");
    CHECK(LecWriteInterruptMask(&dev, 0) == STATUS_DEVICE_BUSY,
        "generic writer cannot disable owned bit");
    CHECK(LecWriteInterruptMask(&dev, 0x21) == STATUS_SUCCESS,
        "generic writer may preserve owned bit");
    CHECK(LecUpdateInterruptMask(&dev, 1, 0, TRUE) == STATUS_SUCCESS,
        "success cleanup");
    CHECK(dev.InterruptEnableShadow == 0x20 && registers[0x084 / 4] == 0x20,
        "cleanup preserves unrelated consumer");
    CHECK(LecWriteInterruptMask(&dev, 0x21) == STATUS_DEVICE_BUSY,
        "stale generic mask cannot re-enable after cleanup");
    CHECK((dev.InterruptEnableShadow & 1) == 0, "late IRQ filtered by shadow");
    CHECK(LecUpdateInterruptMask(&dev, 0, 1, TRUE) == STATUS_SUCCESS &&
        LecUpdateInterruptMask(&dev, 1, 0, TRUE) == STATUS_SUCCESS,
        "repeated acquisition");
    CHECK(LecUpdateInterruptMask(&dev, 0, 1, TRUE) == STATUS_SUCCESS,
        "arm mask failure fixture");
    before = writes;
    dev.BarLength[0] = 0x084;
    CHECK(!NT_SUCCESS(LecUpdateInterruptMask(&dev, 1, 0, TRUE)),
        "failed mask resolution propagates failure");
    CHECK(writes == before && dev.InterruptEnableShadow == 0x21,
        "failed mask does not publish successful shadow");
    dev.BarLength[0] = sizeof(registers);
    CHECK(LecUpdateInterruptMask(&dev, 1, 0, TRUE) == STATUS_SUCCESS,
        "owner can finish mask after mapping fixture restored");
    failSynchronization = TRUE;
    before = writes;
    CHECK(!NT_SUCCESS(LecUpdateInterruptMask(&dev, 1, 0, TRUE)) &&
        writes == before && dev.InterruptEnableShadow == 0x20,
        "synchronization failure cannot report successful mask");
    failSynchronization = FALSE;
    CHECK(LecUpdateInterruptMask(&dev, 0, 2, TRUE) == STATUS_INVALID_PARAMETER,
        "acquisition role limited to established DMA bit");

    /* Generic absolute writes cannot steal bit 0 on either side of cleanup. */
    for (i = 0; i < 2; ++i) {
        WORK first = {0}, second = {0};
        HANDLE a, b;
        first.DevExt = second.DevExt = &dev;
        first.Acquisition = TRUE;
        first.Bits = i == 0 ? 1 : 0;
        first.ClearBits = i == 0 ? 0 : 1;
        second.Absolute = TRUE;
        second.Bits = i == 0 ? 8 : 1;
        second.Entered = CreateEvent(NULL, TRUE, FALSE, NULL);
        second.Done = CreateEvent(NULL, TRUE, FALSE, NULL);
        ResetEvent(writeEntered); ResetEvent(writeResume);
        InterlockedExchange(&pauseWrite, 1);
        a = CreateThread(NULL, 0, Worker, &first, 0, NULL);
        if (!a || WaitForSingleObject(writeEntered, 10000) != WAIT_OBJECT_0) return 9;
        b = CreateThread(NULL, 0, Worker, &second, 0, NULL);
        if (!b || WaitForSingleObject(second.Entered, 10000) != WAIT_OBJECT_0) return 10;
        CHECK(WaitForSingleObject(second.Done, 20) == WAIT_TIMEOUT,
            "generic absolute write waits behind acquisition transaction");
        SetEvent(writeResume); Join(a); Join(b);
        CHECK(first.Status == STATUS_SUCCESS && second.Status == STATUS_DEVICE_BUSY,
            "concurrent generic bit theft rejected");
        CHECK((dev.InterruptEnableShadow & 1) == (i == 0 ? 1UL : 0UL) &&
            dev.InterruptEnableShadow == registers[0x084 / 4],
            "acquisition and shadow stay coherent under conflicting generic write");
        CloseHandle(second.Entered); CloseHandle(second.Done);
    }

    /* Pause inside the actual production critical section, before MMIO. */
    for (i = 0; i < 3; ++i) {
        WORK first = {0}, second = {0};
        HANDLE a, b;
        LecSetInterruptAdmission(&dev, TRUE);
        CHECK(LecWriteInterruptMask(&dev, 0) == STATUS_SUCCESS, "race reset");
        ResetEvent(writeEntered);
        ResetEvent(writeResume);
        InterlockedExchange(&pauseWrite, 1);
        first.DevExt = &dev; first.Bits = 2;
        a = CreateThread(NULL, 0, Worker, &first, 0, NULL);
        if (!a || WaitForSingleObject(writeEntered, 10000) != WAIT_OBJECT_0) return 5;
        second.DevExt = &dev; second.Bits = 8;
        second.Close = i == 1; second.Quarantine = i == 2;
        second.Entered = CreateEvent(NULL, TRUE, FALSE, NULL);
        second.Done = CreateEvent(NULL, TRUE, FALSE, NULL);
        b = CreateThread(NULL, 0, Worker, &second, 0, NULL);
        if (!b || WaitForSingleObject(second.Entered, 10000) != WAIT_OBJECT_0) return 6;
        CHECK(WaitForSingleObject(second.Done, 20) == WAIT_TIMEOUT,
            "RMW/admission/quarantine cannot pass in-flight writer");
        SetEvent(writeResume);
        Join(a); Join(b);
        CHECK(first.Status == STATUS_SUCCESS, "linearized first writer completes");
        if (i == 0) CHECK(dev.InterruptEnableShadow == 10 && registers[0x084 / 4] == 10,
            "concurrent RMW retains both consumers");
        else CHECK(!NT_SUCCESS(LecUpdateInterruptMask(&dev, 0, 8, FALSE)),
            "already admitted writer rejected after closure");
        CloseHandle(second.Entered); CloseHandle(second.Done);
    }
    before = writes;
    LecSetInterruptAdmission(&dev, TRUE);
    CHECK(!dev.AcceptIoctls && !dev.InterruptWriteAdmission,
        "quarantine cannot reopen on restart");
    CHECK(!NT_SUCCESS(LecWriteInterruptMask(&dev, 0)) &&
        !NT_SUCCESS(LecUpdateInterruptMask(&dev, 0, 1, TRUE)),
        "quarantine rejects generic changes and acquisition enable");
    CHECK(writes == before, "no post-quarantine unsafe MMIO");
    CHECK(LecUpdateInterruptMask(&dev, 1, 0, TRUE) == STATUS_SUCCESS,
        "narrow mask-only owner cleanup allowed during quarantine");
    CHECK(LecMaskInterruptsForPnp(&dev) == STATUS_SUCCESS &&
        dev.InterruptEnableShadow == 0, "PnP mask-only exception");
    dev.Bar[0] = NULL;
    before = writes;
    CHECK(!NT_SUCCESS(LecMaskInterruptsForPnp(&dev)) &&
        !NT_SUCCESS(LecReadInterruptMask(&dev, &value)) && writes == before,
        "no access beyond BAR lifetime");
    dev.Bar[0] = (PUCHAR)registers;
    LecInvalidateInterruptHardware(&dev);
    before = writes;
    CHECK(!NT_SUCCESS(LecUpdateInterruptMask(&dev, 1, 0, TRUE)) &&
        !NT_SUCCESS(LecMaskInterruptsForPnp(&dev)) &&
        !NT_SUCCESS(LecReadInterruptMask(&dev, &value)) && writes == before,
        "surprise removal blocks even cleanup and read MMIO");

    /* A fresh FDO, not a recovery of the quarantined FDO. */
    DeleteCriticalSection(&dev.InterruptControlLock);
    DeleteCriticalSection(&dev.IoAdmissionLock);
    ZeroMemory(&dev, sizeof(dev));
    InitializeCriticalSection(&dev.InterruptControlLock);
    InitializeCriticalSection(&dev.IoAdmissionLock);
    dev.InterruptObject = &irq;
    dev.InterruptConnected = TRUE;
    dev.Started = TRUE;
    dev.BarLength[0] = sizeof(registers);
    dev.Bar[0] = (PUCHAR)registers;
    LecSetInterruptAdmission(&dev, TRUE);
    for (i = 0; i < iterations; ++i) {
        WORK a = {0}, b = {0};
        HANDLE ta, tb;
        if (LecWriteInterruptMask(&dev, 0) != STATUS_SUCCESS) return 7;
        a.DevExt = b.DevExt = &dev;
        a.Bits = 2; b.Bits = 8;
        ta = CreateThread(NULL, 0, Worker, &a, 0, NULL);
        tb = CreateThread(NULL, 0, Worker, &b, 0, NULL);
        if (!ta || !tb) return 8;
        Join(ta); Join(tb);
        if (a.Status != STATUS_SUCCESS || b.Status != STATUS_SUCCESS ||
            dev.InterruptEnableShadow != 10 || registers[0x084 / 4] != 10) {
            ++failures; break;
        }
    }
    printf("INTEN functional: %ld PASS, %ld FAIL, 0 SKIP\n", checks - failures, failures);
    printf("INTEN RMW stress: %lu/%lu iterations completed\n", i, iterations);
    CloseHandle(writeEntered); CloseHandle(writeResume);
    DeleteCriticalSection(&irq);
    DeleteCriticalSection(&dev.InterruptControlLock);
    DeleteCriticalSection(&dev.IoAdmissionLock);
    return failures ? 1 : 0;
}
