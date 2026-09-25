#include "LecS65Drv.h"

static
NTSTATUS
LecCompletionSetEvent(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_ PVOID Context
    )
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
    _Inout_ PIRP Irp
    )
{
    KEVENT event;
    NTSTATUS status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(
        Irp,
        LecCompletionSetEvent,
        &event,
        TRUE,
        TRUE,
        TRUE);

    status = IoCallDriver(DevExt->LowerDeviceObject, Irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(
            &event,
            Executive,
            KernelMode,
            FALSE,
            NULL);
        status = Irp->IoStatus.Status;
    }

    return status;
}

VOID
LecUnmapBars(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    ULONG i;

    for (i = 0; i < LECS65_BAR_COUNT; ++i) {
        if (DevExt->Bar[i] != NULL) {
            LecTrace("BAR%lu unmap VA=%p length=0x%lX\n",
                i, DevExt->Bar[i], DevExt->BarLength[i]);

            MmUnmapIoSpace(DevExt->Bar[i], DevExt->BarLength[i]);
            DevExt->Bar[i] = NULL;
            DevExt->BarLength[i] = 0;
            DevExt->BarPhysical[i].QuadPart = 0;
        }
    }
}

NTSTATUS
LecHandleStartDevice(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PCM_RESOURCE_LIST TranslatedResources
    )
{
    ULONG listIndex;
    ULONG barIndex = 0;

    LecUnmapBars(DevExt);

    if (TranslatedResources == NULL) {
        LecTrace("START_DEVICE: no translated resource list\n");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    LecTrace("START_DEVICE: translated resource lists=%lu\n",
        TranslatedResources->Count);

    for (listIndex = 0; listIndex < TranslatedResources->Count; ++listIndex) {
        PCM_FULL_RESOURCE_DESCRIPTOR full =
            &TranslatedResources->List[listIndex];
        PCM_PARTIAL_RESOURCE_LIST partial =
            &full->PartialResourceList;
        ULONG descriptorIndex;

        LecTrace(" resources[%lu]: interface=%u bus=%lu descriptors=%lu\n",
            listIndex,
            (ULONG)full->InterfaceType,
            full->BusNumber,
            partial->Count);

        for (descriptorIndex = 0;
             descriptorIndex < partial->Count;
             ++descriptorIndex) {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR d =
                &partial->PartialDescriptors[descriptorIndex];

            switch (d->Type) {
            case CmResourceTypeMemory:
                LecTrace(
                    "  MEM[%lu]: start=%I64X length=0x%lX flags=0x%X%s\n",
                    descriptorIndex,
                    d->u.Memory.Start.QuadPart,
                    d->u.Memory.Length,
                    d->Flags,
                    barIndex < LECS65_BAR_COUNT ? " -> BAR" : "");

                if (barIndex < LECS65_BAR_COUNT) {
                    PUCHAR mapped = (PUCHAR)MmMapIoSpace(
                        d->u.Memory.Start,
                        d->u.Memory.Length,
                        MmNonCached);

                    if (mapped == NULL) {
                        LecTrace("  BAR%lu map FAILED\n", barIndex);
                        LecUnmapBars(DevExt);
                        return STATUS_INSUFFICIENT_RESOURCES;
                    }

                    DevExt->Bar[barIndex] = mapped;
                    DevExt->BarLength[barIndex] = d->u.Memory.Length;
                    DevExt->BarPhysical[barIndex] = d->u.Memory.Start;

                    LecTrace(
                        "  BAR%lu: PA=%I64X VA=%p length=0x%lX\n",
                        barIndex,
                        d->u.Memory.Start.QuadPart,
                        mapped,
                        d->u.Memory.Length);

                    ++barIndex;
                }
                break;

            case CmResourceTypeInterrupt:
                LecTrace(
                    "  IRQ[%lu]: level=%lu vector=%lu affinity=%p flags=0x%X\n",
                    descriptorIndex,
                    d->u.Interrupt.Level,
                    d->u.Interrupt.Vector,
                    (PVOID)d->u.Interrupt.Affinity,
                    d->Flags);
                break;

            case CmResourceTypePort:
                LecTrace(
                    "  PORT[%lu]: start=%I64X length=0x%lX flags=0x%X\n",
                    descriptorIndex,
                    d->u.Port.Start.QuadPart,
                    d->u.Port.Length,
                    d->Flags);
                break;

            default:
                LecTrace("  RES[%lu]: type=%u share=%u flags=0x%X\n",
                    descriptorIndex,
                    d->Type,
                    d->ShareDisposition,
                    d->Flags);
                break;
            }
        }
    }

    if (barIndex < LECS65_BAR_COUNT) {
        LecTrace("START_DEVICE: expected 3 memory BAR resources, found %lu\n",
            barIndex);
        LecUnmapBars(DevExt);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return STATUS_SUCCESS;
}

static
VOID
LecEnableInterfaces(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    ULONG i;

    for (i = 0; i < LECS65_INTERFACE_COUNT; ++i) {
        NTSTATUS status;

        if (!DevExt->InterfaceRegistered[i]) {
            continue;
        }

        status = IoSetDeviceInterfaceState(&DevExt->InterfaceLink[i], TRUE);
        if (NT_SUCCESS(status)) {
            DevExt->InterfaceEnabled[i] = TRUE;
            LecTrace("interface[%lu] enabled: %wZ\n",
                i, &DevExt->InterfaceLink[i]);
        }
        else {
            LecTrace("interface[%lu] enable failed 0x%08X\n", i, status);
        }
    }
}

VOID
LecDisableInterfaces(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    ULONG i;

    for (i = 0; i < LECS65_INTERFACE_COUNT; ++i) {
        if (DevExt->InterfaceRegistered[i] && DevExt->InterfaceEnabled[i]) {
            NTSTATUS status =
                IoSetDeviceInterfaceState(&DevExt->InterfaceLink[i], FALSE);

            LecTrace("interface[%lu] disable -> 0x%08X\n", i, status);
            DevExt->InterfaceEnabled[i] = FALSE;
        }
    }
}

NTSTATUS
LecS65Pnp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    LecTrace("PNP: minor=0x%02X\n", stack->MinorFunction);

    switch (stack->MinorFunction) {
    case IRP_MN_START_DEVICE:
        status = LecForwardAndWait(devExt, Irp);
        if (NT_SUCCESS(status)) {
            status = LecHandleStartDevice(
                devExt,
                stack->Parameters.StartDevice.AllocatedResourcesTranslated);

            if (NT_SUCCESS(status)) {
                devExt->Started = TRUE;
                LecEnableInterfaces(devExt);
            }
        }

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;

    case IRP_MN_STOP_DEVICE:
        devExt->Started = FALSE;
        LecDisableInterfaces(devExt);
        LecUnmapBars(devExt);
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->LowerDeviceObject, Irp);

    case IRP_MN_SURPRISE_REMOVAL:
        devExt->Started = FALSE;
        LecDisableInterfaces(devExt);
        LecUnmapBars(devExt);
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->LowerDeviceObject, Irp);

    case IRP_MN_REMOVE_DEVICE:
        devExt->Removed = TRUE;
        devExt->Started = FALSE;

        LecDisableInterfaces(devExt);
        LecUnmapBars(devExt);

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->LowerDeviceObject, Irp);

        if (devExt->SymbolicLinkCreated) {
            UNICODE_STRING dosName;
            RtlInitUnicodeString(&dosName, LECS65_DOS_DEVICE_NAME);
            (VOID)IoDeleteSymbolicLink(&dosName);
            devExt->SymbolicLinkCreated = FALSE;
        }

        {
            ULONG i;
            for (i = 0; i < LECS65_INTERFACE_COUNT; ++i) {
                if (devExt->InterfaceRegistered[i]) {
                    RtlFreeUnicodeString(&devExt->InterfaceLink[i]);
                    devExt->InterfaceRegistered[i] = FALSE;
                }
            }
        }

        IoDetachDevice(devExt->LowerDeviceObject);
        IoDeleteDevice(DeviceObject);
        return status;

    default:
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->LowerDeviceObject, Irp);
    }
}

NTSTATUS
LecS65Power(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    LecTrace("POWER: minor=0x%02X type=%u state=%u\n",
        stack->MinorFunction,
        stack->Parameters.Power.Type,
        stack->Parameters.Power.State.SystemState);

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(devExt->LowerDeviceObject, Irp);
}
