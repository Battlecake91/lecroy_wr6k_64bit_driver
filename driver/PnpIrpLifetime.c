#if defined(LECS65_PNP_IRP_HOST_TEST)
#include "../tests/dry/pnp-irp-mock.h"
#else
#include "LecS65Drv.h"
#endif

static NTSTATUS
LecCompletionSetEvent(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp,
    _In_ PVOID Context)
{
    PKEVENT event = (PKEVENT)Context;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    KeSetEvent(event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
LecForwardAndWait(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PIRP Irp)
{
    KEVENT event;
    NTSTATUS status;
#if LECS65_PNP_STATUS_DIAGNOSTICS
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    UCHAR majorFunction = stack->MajorFunction;
    UCHAR minorFunction = stack->MinorFunction;
    NTSTATUS dispatchStatus;
#endif

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(
        Irp, LecCompletionSetEvent, &event,
        TRUE, TRUE, TRUE);

    status = IoCallDriver(DevExt->LowerDeviceObject, Irp);
#if LECS65_PNP_STATUS_DIAGNOSTICS
    dispatchStatus = status;
#endif
    if (status == STATUS_PENDING) {
        (void)KeWaitForSingleObject(
            &event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }
#if LECS65_PNP_STATUS_DIAGNOSTICS
    if (majorFunction == IRP_MJ_PNP) {
        LecPnpStatusTrace(
            "PNP_STATUS: irp=%p minor=0x%02X stage=lower-wait dispatch=0x%08X final=0x%08X\n",
            Irp,
            minorFunction,
            (ULONG)dispatchStatus,
            (ULONG)status);
    }
#endif
    return status;
}

/*
 * A forwarded IRP can complete asynchronously. Release its remove-lock
 * reference from the completion routine, not from the dispatch return.
 */
static NTSTATUS
LecReleaseForwardedIrpLock(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp,
    _In_ PVOID Context)
{
    PLECS65_DEVICE_EXTENSION devExt = (PLECS65_DEVICE_EXTENSION)Context;

    UNREFERENCED_PARAMETER(DeviceObject);
#if LECS65_PNP_STATUS_DIAGNOSTICS
    {
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

        if (stack->MajorFunction == IRP_MJ_PNP) {
            LecPnpStatusTrace(
                "PNP_STATUS: irp=%p minor=0x%02X stage=lower-complete final=0x%08X pending=%u\n",
                Irp,
                stack->MinorFunction,
                (ULONG)Irp->IoStatus.Status,
                Irp->PendingReturned);
        }
    }
#endif
    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
LecForwardLockedIrp(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PIRP Irp,
    _In_ BOOLEAN IsPowerIrp)
{
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(
        Irp, LecReleaseForwardedIrpLock, DevExt,
        TRUE, TRUE, TRUE);

#if LECS65_PNP_STATUS_DIAGNOSTICS
    {
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
        UCHAR majorFunction = stack->MajorFunction;
        UCHAR minorFunction = stack->MinorFunction;
        NTSTATUS status = IsPowerIrp ?
            PoCallDriver(DevExt->LowerDeviceObject, Irp) :
            IoCallDriver(DevExt->LowerDeviceObject, Irp);

        if (majorFunction == IRP_MJ_PNP) {
            LecPnpStatusTrace(
                "PNP_STATUS: irp=%p minor=0x%02X stage=lower-dispatch returned=0x%08X\n",
                Irp,
                minorFunction,
                (ULONG)status);
        }
        return status;
    }
#else
    return IsPowerIrp ?
        PoCallDriver(DevExt->LowerDeviceObject, Irp) :
        IoCallDriver(DevExt->LowerDeviceObject, Irp);
#endif
}
