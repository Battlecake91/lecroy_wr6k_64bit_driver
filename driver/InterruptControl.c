#if defined(LECS65_INTEN_HOST_TEST)
#include "../tests/dry/interrupt-control-mock.h"
#else
#include "LecS65Drv.h"
#endif

/* Lock order: InterruptControlLock -> interrupt synchronization lock.
 * The ISR never takes InterruptControlLock. No waits or allocations here.
 */
typedef struct _LEC_INTEN_OPERATION {
    PLECS65_DEVICE_EXTENSION DevExt;
    ULONG ClearBits;
    ULONG SetBits;
    ULONG Value;
    BOOLEAN Absolute;
    BOOLEAN Read;
    BOOLEAN Acquisition;
    BOOLEAN Teardown;
    BOOLEAN OnlyIfChanged;
    NTSTATUS Status;
} LEC_INTEN_OPERATION;

static BOOLEAN
LecInterruptOperationSynchronized(PVOID Context)
{
    LEC_INTEN_OPERATION* op = (LEC_INTEN_OPERATION*)Context;
    PLECS65_DEVICE_EXTENSION devExt = op->DevExt;
    volatile ULONG* reg;
    ULONG oldMask = devExt->InterruptEnableShadow;
    ULONG newMask;
    BOOLEAN cleanup = op->Acquisition &&
        op->ClearBits == 1 && op->SetBits == 0;

    if (devExt->InterruptHardwareUnavailable ||
        (!devExt->Started && !op->Teardown) ||
        devExt->Bar[0] == NULL ||
        devExt->BarLength[0] < 0x084 + sizeof(ULONG)) {
        op->Status = STATUS_INVALID_DEVICE_STATE;
        return TRUE;
    }
    reg = (volatile ULONG*)(devExt->Bar[0] + 0x084);
    if (op->Read) {
        op->Value = READ_REGISTER_ULONG(reg);
        op->Status = STATUS_SUCCESS;
        return TRUE;
    }
    if (!op->Teardown && !cleanup &&
        (!devExt->InterruptWriteAdmission || devExt->DmaUnknownActive)) {
        op->Status = STATUS_INVALID_DEVICE_STATE;
        return TRUE;
    }
    newMask = op->Absolute ? op->Value :
        ((oldMask & ~op->ClearBits) | op->SetBits);
    if (!op->Acquisition && !op->Teardown &&
        ((newMask ^ oldMask) & 1) != 0) {
        /* Generic ABI may change other bits, but cannot steal DMA ownership. */
        op->Status = STATUS_DEVICE_BUSY;
        return TRUE;
    }
    if (!op->OnlyIfChanged || newMask != oldMask) {
        WRITE_REGISTER_ULONG(reg, newMask);
        /* WRITE_REGISTER is void: this means issued, not hardware accepted. */
        InterlockedExchange(
            (volatile LONG*)&devExt->InterruptEnableShadow, (LONG)newMask);
    }
    op->Status = STATUS_SUCCESS;
    return TRUE;
}

static NTSTATUS
LecRunInterruptOperation(LEC_INTEN_OPERATION* Op)
{
    KIRQL irql;
    PLECS65_DEVICE_EXTENSION devExt = Op->DevExt;
    Op->Status = STATUS_UNSUCCESSFUL;
    KeAcquireSpinLock(&devExt->InterruptControlLock, &irql);
    if (devExt->InterruptConnected && devExt->InterruptObject != NULL) {
        if (!KeSynchronizeExecution(devExt->InterruptObject,
            LecInterruptOperationSynchronized, Op)) {
            Op->Status = STATUS_UNSUCCESSFUL;
        }
    }
    else {
        /* No ISR exists; PnP has drained callers before disconnect/unmap. */
        (VOID)LecInterruptOperationSynchronized(Op);
    }
    KeReleaseSpinLock(&devExt->InterruptControlLock, irql);
    return Op->Status;
}

NTSTATUS
LecWriteInterruptMask(PLECS65_DEVICE_EXTENSION DevExt, ULONG Value)
{
    LEC_INTEN_OPERATION op = {0};
    op.DevExt = DevExt;
    op.Absolute = TRUE;
    op.Value = Value;
    return LecRunInterruptOperation(&op);
}

NTSTATUS
LecUpdateInterruptMask(PLECS65_DEVICE_EXTENSION DevExt,
    ULONG ClearBits, ULONG SetBits, BOOLEAN Acquisition)
{
    LEC_INTEN_OPERATION op = {0};
    if (Acquisition && ((ClearBits | SetBits) & ~1UL) != 0) {
        return STATUS_INVALID_PARAMETER;
    }
    op.DevExt = DevExt;
    op.ClearBits = ClearBits;
    op.SetBits = SetBits;
    op.Acquisition = Acquisition;
    return LecRunInterruptOperation(&op);
}

NTSTATUS
LecEnsureReceiveInterrupt(PLECS65_DEVICE_EXTENSION DevExt)
{
    LEC_INTEN_OPERATION op = {0};
    op.DevExt = DevExt;
    op.SetBits = 0x08UL;
    op.OnlyIfChanged = TRUE;
    return LecRunInterruptOperation(&op);
}

NTSTATUS
LecReadInterruptMask(PLECS65_DEVICE_EXTENSION DevExt, PULONG Value)
{
    LEC_INTEN_OPERATION op = {0};
    NTSTATUS status;
    op.DevExt = DevExt;
    op.Read = TRUE;
    status = LecRunInterruptOperation(&op);
    if (NT_SUCCESS(status)) {
        *Value = op.Value;
    }
    return status;
}

NTSTATUS
LecMaskInterruptsForPnp(PLECS65_DEVICE_EXTENSION DevExt)
{
    LEC_INTEN_OPERATION op = {0};
    op.DevExt = DevExt;
    op.Absolute = TRUE;
    op.Teardown = TRUE;
    return LecRunInterruptOperation(&op);
}

VOID
LecInvalidateInterruptHardware(PLECS65_DEVICE_EXTENSION DevExt)
{
    KIRQL irql;
    KeAcquireSpinLock(&DevExt->InterruptControlLock, &irql);
    DevExt->InterruptHardwareUnavailable = TRUE;
    DevExt->InterruptWriteAdmission = FALSE;
    KeReleaseSpinLock(&DevExt->InterruptControlLock, irql);
}

VOID
LecSetInterruptAdmission(PLECS65_DEVICE_EXTENSION DevExt, BOOLEAN Enable)
{
    KIRQL irql;
    KIRQL admissionIrql;
    /* No interrupt callback takes either lock. Never called under them. */
    KeAcquireSpinLock(&DevExt->InterruptControlLock, &irql);
    KeAcquireSpinLock(&DevExt->IoAdmissionLock, &admissionIrql);
    Enable = Enable && !DevExt->DmaUnknownActive &&
        !DevExt->InterruptHardwareUnavailable;
    DevExt->InterruptWriteAdmission = Enable;
    DevExt->AcceptIoctls = Enable;
    KeReleaseSpinLock(&DevExt->IoAdmissionLock, admissionIrql);
    KeReleaseSpinLock(&DevExt->InterruptControlLock, irql);
}

VOID
LecQuarantineInterruptAdmission(PLECS65_DEVICE_EXTENSION DevExt)
{
    KIRQL irql;
    /* Linearize terminal uncertainty against already admitted writers. */
    KeAcquireSpinLock(&DevExt->InterruptControlLock, &irql);
    (VOID)InterlockedExchange(&DevExt->DmaUnknownActive, 1);
    DevExt->InterruptWriteAdmission = FALSE;
    KeReleaseSpinLock(&DevExt->InterruptControlLock, irql);
}
