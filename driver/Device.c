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
LecReleaseLegacyEvents(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    PKEVENT released[5];
    KIRQL oldIrql;
    ULONG i;

    /*
     * A queued DPC may still be inspecting the registered event pointers
     * while a handle is closed or the device is stopped. Detach all pointers
     * under the same spin lock used by the DPC, then drop object references
     * after leaving the lock.
     */
    KeAcquireSpinLock(&DevExt->LegacyEventLock, &oldIrql);

    released[0] = DevExt->LegacyEvent0;
    released[1] = DevExt->LegacyEvent1;
    released[2] = DevExt->LegacyEvent2;
    released[3] = DevExt->LegacyEvent3;
    released[4] = DevExt->LegacyEvent4;

    DevExt->LegacyEvent0 = NULL;
    DevExt->LegacyEvent1 = NULL;
    DevExt->LegacyEvent2 = NULL;
    DevExt->LegacyEvent3 = NULL;
    DevExt->LegacyEvent4 = NULL;

    KeReleaseSpinLock(&DevExt->LegacyEventLock, oldIrql);

    for (i = 0; i < RTL_NUMBER_OF(released); ++i) {
        if (released[i] != NULL) {
            ObDereferenceObject(released[i]);
        }
    }
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

    if (DevExt->BulkMmio != NULL) {
        LecTrace("BULK unmap VA=%p length=0x%lX\n",
            DevExt->BulkMmio, DevExt->BulkMmioLength);

        MmUnmapIoSpace(DevExt->BulkMmio, DevExt->BulkMmioLength);
        DevExt->BulkMmio = NULL;
    }

    DevExt->BulkMmioLength = 0;
    DevExt->BulkMmioPhysical.QuadPart = 0;
}


static
VOID
LecLegacyBuzzerPulse(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG DurationMs
    )
{
    volatile ULONG* buzzer;
    LARGE_INTEGER interval;

    if (DevExt->Bar[2] == NULL ||
        DevExt->BarLength[2] <
            LECS65_BAR2_BUZZER_OFFSET + sizeof(ULONG)) {
        return;
    }

    buzzer = (volatile ULONG*)(
        DevExt->Bar[2] + LECS65_BAR2_BUZZER_OFFSET);

    WRITE_REGISTER_ULONG(buzzer, 1);

    interval.QuadPart = -((LONGLONG)DurationMs * 10000LL);
    (VOID)KeDelayExecutionThread(
        KernelMode,
        FALSE,
        &interval);

    WRITE_REGISTER_ULONG(buzzer, 0);
}

static
NTSTATUS
LecRunLegacyStartupProbe(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    volatile ULONG* start;
    volatile ULONG* itmode;
    LARGE_INTEGER interval;
    ULONG raw;
    ULONG bit0;

    if (DevExt->Bar[0] == NULL ||
        DevExt->Bar[1] == NULL ||
        DevExt->Bar[2] == NULL ||
        DevExt->BarLength[0] <
            LECS65_BAR0_START_OFFSET + sizeof(ULONG) ||
        DevExt->BarLength[1] <
            LECS65_BAR1_ITMODE_OFFSET + sizeof(ULONG) ||
        DevExt->BarLength[2] <
            LECS65_BAR2_BUZZER_OFFSET + sizeof(ULONG)) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    start = (volatile ULONG*)(
        DevExt->Bar[0] + LECS65_BAR0_START_OFFSET);
    itmode = (volatile ULONG*)(
        DevExt->Bar[1] + LECS65_BAR1_ITMODE_OFFSET);

    /*
     * Exact host-side startup sequence recovered from legacy FUN_00012FDE:
     *   START <- 1
     *   wait 100 us
     *   read START bit 0
     *
     * A clear bit is legacy failure and produces one 1200 ms buzzer pulse.
     * A set bit is legacy success and produces two 300 ms pulses around
     * ITMODE 7 -> 3 with a 500 us delay.
     */
    WRITE_REGISTER_ULONG(start, 1);

    interval.QuadPart = -1000LL; /* 100 us */
    (VOID)KeDelayExecutionThread(
        KernelMode,
        FALSE,
        &interval);

    raw = READ_REGISTER_ULONG(start);
    bit0 = raw & 1UL;

    LecTrace(
        "LEGACY_START: START readback=0x%08lX bit0=%lu\n",
        raw,
        bit0);

    if (bit0 == 0) {
        LecLegacyBuzzerPulse(DevExt, 1200);
        LecTrace(
            "LEGACY_START: legacy probe failed (START bit0 clear)\n");
        return STATUS_UNSUCCESSFUL;
    }

    LecLegacyBuzzerPulse(DevExt, 300);

    WRITE_REGISTER_ULONG(itmode, 7);
    LecTrace("LEGACY_START: ITMODE <- 7\n");

    interval.QuadPart = -5000LL; /* 500 us */
    (VOID)KeDelayExecutionThread(
        KernelMode,
        FALSE,
        &interval);

    LecLegacyBuzzerPulse(DevExt, 300);

    WRITE_REGISTER_ULONG(itmode, 3);
    LecTrace("LEGACY_START: ITMODE <- 3\n");

    return STATUS_SUCCESS;
}

NTSTATUS
LecHandleStartDevice(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PCM_RESOURCE_LIST TranslatedResources
    )
{
    ULONG listIndex;
    ULONG memoryIndex = 0;

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
            PCM_PARTIAL_RESOURCE_DESCRIPTOR resource =
                &partial->PartialDescriptors[descriptorIndex];

            switch (resource->Type) {
            case CmResourceTypeMemory:
            {
                PUCHAR mapped;

                LecTrace(
                    "  MEM[%lu]: start=%I64X length=0x%lX flags=0x%X\n",
                    descriptorIndex,
                    resource->u.Memory.Start.QuadPart,
                    resource->u.Memory.Length,
                    resource->Flags);

                mapped = (PUCHAR)MmMapIoSpace(
                    resource->u.Memory.Start,
                    resource->u.Memory.Length,
                    MmNonCached);

                if (mapped == NULL) {
                    LecTrace("  MEM[%lu] map FAILED\n", descriptorIndex);
                    LecUnmapBars(DevExt);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                /*
                 * The original 2008 driver asks DriverWorks for memory
                 * resources 0, 1 and 2 in that exact order and binds them to
                 * BAR0, BAR1 and BAR2 respectively.  Do not classify these by
                 * size: on the reference hardware BAR1 is the large window
                 * while BAR0/BAR2 are 0x200-byte register windows.
                 */
                if (memoryIndex < LECS65_BAR_COUNT) {
                    DevExt->Bar[memoryIndex] = mapped;
                    DevExt->BarLength[memoryIndex] =
                        resource->u.Memory.Length;
                    DevExt->BarPhysical[memoryIndex] =
                        resource->u.Memory.Start;

                    LecTrace(
                        "  MEM[%lu] -> logical BAR%lu: PA=%I64X VA=%p length=0x%lX\n",
                        descriptorIndex,
                        memoryIndex,
                        resource->u.Memory.Start.QuadPart,
                        mapped,
                        resource->u.Memory.Length);

                    if (resource->u.Memory.Length >
                        LECS65_REGISTER_BAR_LENGTH) {
                        /*
                         * Keep the old BULK diagnostic as metadata only.
                         * Bar[memoryIndex] owns the actual mapping.
                         */
                        DevExt->BulkMmioLength =
                            resource->u.Memory.Length;
                        DevExt->BulkMmioPhysical =
                            resource->u.Memory.Start;
                    }

                    ++memoryIndex;
                }
                else {
                    LecTrace(
                        "  MEM[%lu] extra resource; unmapping PA=%I64X length=0x%lX\n",
                        descriptorIndex,
                        resource->u.Memory.Start.QuadPart,
                        resource->u.Memory.Length);

                    MmUnmapIoSpace(mapped, resource->u.Memory.Length);
                }
                break;
            }

            case CmResourceTypeInterrupt:
                LecTrace(
                    "  IRQ[%lu]: level=%lu vector=%lu affinity=%p flags=0x%X share=%u\n",
                    descriptorIndex,
                    resource->u.Interrupt.Level,
                    resource->u.Interrupt.Vector,
                    (PVOID)resource->u.Interrupt.Affinity,
                    resource->Flags,
                    resource->ShareDisposition);

                DevExt->InterruptVector =
                    resource->u.Interrupt.Vector;
                DevExt->InterruptIrql =
                    (KIRQL)resource->u.Interrupt.Level;
                DevExt->InterruptSynchronizeIrql =
                    (KIRQL)resource->u.Interrupt.Level;
                DevExt->InterruptAffinity =
                    resource->u.Interrupt.Affinity;
                DevExt->InterruptMode =
                    (resource->Flags & CM_RESOURCE_INTERRUPT_LATCHED) != 0
                        ? Latched
                        : LevelSensitive;
                DevExt->InterruptShareVector =
                    resource->ShareDisposition == CmResourceShareShared;
                break;

            case CmResourceTypePort:
                LecTrace(
                    "  PORT[%lu]: start=%I64X length=0x%lX flags=0x%X\n",
                    descriptorIndex,
                    resource->u.Port.Start.QuadPart,
                    resource->u.Port.Length,
                    resource->Flags);
                break;

            default:
                LecTrace("  RES[%lu]: type=%u share=%u flags=0x%X\n",
                    descriptorIndex,
                    resource->Type,
                    resource->ShareDisposition,
                    resource->Flags);
                break;
            }
        }
    }

    if (memoryIndex < LECS65_BAR_COUNT) {
        LecTrace(
            "START_DEVICE: expected three memory BARs, found %lu\n",
            memoryIndex);
        LecUnmapBars(DevExt);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    if (DevExt->BarLength[2] < LECS65_ONEWIRE_OFFSET + sizeof(ULONG)) {
        LecTrace(
            "START_DEVICE: BAR2 too small for ONEWIRE register: length=0x%lX\n",
            DevExt->BarLength[2]);
        LecUnmapBars(DevExt);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    LecTrace(
        "START_DEVICE: logical BAR0=%I64X/0x%lX BAR1=%I64X/0x%lX BAR2=%I64X/0x%lX BULK=%I64X/0x%lX\n",
        DevExt->BarPhysical[0].QuadPart,
        DevExt->BarLength[0],
        DevExt->BarPhysical[1].QuadPart,
        DevExt->BarLength[1],
        DevExt->BarPhysical[2].QuadPart,
        DevExt->BarLength[2],
        DevExt->BulkMmioPhysical.QuadPart,
        DevExt->BulkMmioLength);

    {
        NTSTATUS startupStatus =
            LecRunLegacyStartupProbe(DevExt);

        if (!NT_SUCCESS(startupStatus)) {
            LecTrace(
                "START_DEVICE: legacy startup probe failed 0x%08X\n",
                startupStatus);
            LecUnmapBars(DevExt);
            return startupStatus;
        }
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
                NTSTATUS irqStatus;

                /*
                 * Acquisition/DMA requires an ISR to complete transfers.
                 * Never publish a started device if IRQ registration fails:
                 * the former diagnostic-only fallback also admitted active
                 * hardware operations through the normal interface.
                 */
                irqStatus = LecConnectInterrupt(devExt);
                if (!NT_SUCCESS(irqStatus)) {
                    LecTrace(
                        "START_DEVICE: IRQ unavailable, rejecting start: 0x%08X\n",
                        irqStatus);
                    devExt->Started = FALSE;
                    devExt->LegacyMamShadowInitialized = FALSE;
                    devExt->LegacyMamSeqShadowInitialized = FALSE;
                    LecUnmapBars(devExt);
                    status = irqStatus;
                }
                else {
                    devExt->Started = TRUE;
                    LecEnableInterfaces(devExt);
                }
            }
        }

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;

    case IRP_MN_STOP_DEVICE:
        devExt->Started = FALSE;
        LecDisableInterfaces(devExt);
        LecDisconnectInterrupt(devExt);
        LecReleaseLegacyEvents(devExt);
        LecReleaseAllTransfers(devExt);
        LecUnmapBars(devExt);
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->LowerDeviceObject, Irp);

    case IRP_MN_SURPRISE_REMOVAL:
        devExt->Started = FALSE;
        LecDisableInterfaces(devExt);
        LecDisconnectInterrupt(devExt);
        LecReleaseLegacyEvents(devExt);
        LecReleaseAllTransfers(devExt);
        LecUnmapBars(devExt);
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->LowerDeviceObject, Irp);

    case IRP_MN_REMOVE_DEVICE:
        devExt->Removed = TRUE;
        devExt->Started = FALSE;

        LecDisableInterfaces(devExt);
        LecDisconnectInterrupt(devExt);
        LecReleaseLegacyEvents(devExt);
        LecReleaseAllTransfers(devExt);
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
