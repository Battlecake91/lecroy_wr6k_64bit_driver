#include "LecS65Drv.h"
#include "DmaPnpPublication.h"
#include <stdarg.h>

const GUID g_LecS65InterfaceGuids[LECS65_INTERFACE_COUNT] = {
    /*
     * Passive original-XStream tracing established five distinct interface
     * GUIDs. The 958695A4 interface is the main acquisition/control endpoint;
     * omitting it caused XStream to select a different startup path.
     */
    { 0x958695a4, 0x693a, 0x435e, { 0x82, 0x97, 0x66, 0xf8, 0x05, 0xd8, 0xe4, 0x6a } },
    { 0x7ac34be9, 0xf766, 0x4f15, { 0x9e, 0x88, 0x85, 0x4b, 0xa5, 0xe2, 0x14, 0x6e } },
    { 0x8d1103b8, 0x5bf4, 0x4b5c, { 0xb2, 0x1e, 0xee, 0xaa, 0xce, 0x97, 0xd4, 0x18 } },
    { 0x9007c2bc, 0xedfd, 0x4f2f, { 0xa0, 0x59, 0xdf, 0x11, 0x31, 0xcb, 0x1a, 0xe5 } },
    { 0xfc5df040, 0xd6cd, 0x4ba0, { 0xb5, 0xe0, 0x25, 0x61, 0x97, 0x29, 0x63, 0xa2 } }
};

VOID
LecTrace(
    _In_z_ _Printf_format_string_ PCSTR Format,
    ...
    )
{
    va_list args;

    va_start(args, Format);
    vDbgPrintExWithPrefix(
        "[LecS65x64] ",
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_ERROR_LEVEL,
        Format,
        args);
    va_end(args);
}

VOID
LecHexDump(
    _In_reads_bytes_opt_(Length) const UCHAR* Buffer,
    _In_ ULONG Length
    )
{
    static const CHAR Hex[] = "0123456789ABCDEF";
    ULONG offset;
    ULONG limit;

    if (Buffer == NULL || Length == 0) {
        return;
    }

    limit = min(Length, 64UL);

    for (offset = 0; offset < limit; offset += 16) {
        CHAR line[80];
        ULONG pos = 0;
        ULONG i;

        line[pos++] = ' ';
        line[pos++] = ' ';
        line[pos++] = Hex[(offset >> 12) & 0x0F];
        line[pos++] = Hex[(offset >> 8) & 0x0F];
        line[pos++] = Hex[(offset >> 4) & 0x0F];
        line[pos++] = Hex[offset & 0x0F];
        line[pos++] = ' ';
        line[pos++] = ':';

        for (i = 0; i < 16 && (offset + i) < limit; ++i) {
            UCHAR value = Buffer[offset + i];
            line[pos++] = ' ';
            line[pos++] = Hex[(value >> 4) & 0x0F];
            line[pos++] = Hex[value & 0x0F];
        }

        line[pos] = '\0';
        LecTrace("%s\n", line);
    }

    if (Length > limit) {
        LecTrace("  ... %lu more bytes\n", Length - limit);
    }
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    ULONG i;

    UNREFERENCED_PARAMETER(RegistryPath);

    LecTrace("DriverEntry: native x64 bring-up driver, legacy build emulation=%lu\n",
        LECS65_LEGACY_DRIVER_BUILD);

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) {
        DriverObject->MajorFunction[i] = LecS65PassThrough;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = LecS65Create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = LecS65Close;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = LecS65DeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = LecS65Pnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = LecS65Power;

    DriverObject->DriverExtension->AddDevice = LecS65AddDevice;
    DriverObject->DriverUnload = LecS65Unload;

    return STATUS_SUCCESS;
}

NTSTATUS
LecS65AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    UNICODE_STRING deviceName;
    PDEVICE_OBJECT deviceObject = NULL;
    PLECS65_DEVICE_EXTENSION devExt;
    NTSTATUS status;
    ULONG i;

    RtlInitUnicodeString(&deviceName, LECS65_DEVICE_NAME);

    status = IoCreateDevice(
        DriverObject,
        sizeof(LECS65_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {
        LecTrace("AddDevice: IoCreateDevice failed 0x%08X\n", status);
        return status;
    }

    devExt = (PLECS65_DEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(devExt, sizeof(*devExt));
    IoInitializeRemoveLock(&devExt->RemoveLock, LECS65_TAG, 0, 0);
    KeInitializeSpinLock(&devExt->IoAdmissionLock);
    KeInitializeEvent(&devExt->IoIdleEvent, NotificationEvent, TRUE);
    devExt->ActiveIoctls = 0;
    devExt->AcceptIoctls = FALSE;
    /* Original BAR0 ERRM register wrapper starts with cached 0xFFFFFFFF. */
    devExt->LegacyErrmShadow = (LONG)0xFFFFFFFFUL;
    KeInitializeMutex(&devExt->DallasMutex, 0);
    KeInitializeMutex(&devExt->TransferMutex, 0);
    InitializeListHead(&devExt->TransferList);
    devExt->NextTransferToken = 0;
    devExt->CurrentTransfer = NULL;
    KeInitializeDpc(&devExt->InterruptDpc, LecInterruptDpc, devExt);
    KeInitializeSpinLock(&devExt->LegacyEventLock);
    KeInitializeSpinLock(&devExt->TraceLock);

    devExt->Self = deviceObject;
    devExt->PhysicalDeviceObject = PhysicalDeviceObject;

    devExt->LowerDeviceObject = IoAttachDeviceToDeviceStack(deviceObject, PhysicalDeviceObject);
    if (devExt->LowerDeviceObject == NULL) {
        LecTrace("AddDevice: IoAttachDeviceToDeviceStack failed\n");
        IoDeleteDevice(deviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    /*
     * Publish exactly one software-only lifetime anchor for this FDO/PDO.
     * This allocates no DMA adapter, mapping or hardware resource.
     */
    status = LecDmaPnpPublicationCreate(
        PhysicalDeviceObject, &devExt->DmaPnpPublication);
    if (!NT_SUCCESS(status)) {
        LecTrace("AddDevice: DMA/PnP publication failed 0x%08X\n", status);
        IoDetachDevice(devExt->LowerDeviceObject);
        devExt->LowerDeviceObject = NULL;
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceObject->Flags |= DO_POWER_PAGABLE;

    for (i = 0; i < LECS65_INTERFACE_COUNT; ++i) {
        status = IoRegisterDeviceInterface(
            PhysicalDeviceObject,
            &g_LecS65InterfaceGuids[i],
            NULL,
            &devExt->InterfaceLink[i]);

        if (NT_SUCCESS(status)) {
            devExt->InterfaceRegistered[i] = TRUE;
            LecTrace("AddDevice: interface[%lu] registered: %wZ\n",
                i, &devExt->InterfaceLink[i]);
        }
        else {
            LecTrace("AddDevice: interface[%lu] registration failed 0x%08X\n",
                i, status);
        }
    }

    /*
     * Do NOT publish \\.\ALADDINAcqDriver0.
     *
     * Passive tracing of the original 32-bit system proves that XStream first
     * probes this legacy DOS name and expects the open to fail with
     * STATUS_OBJECT_NAME_NOT_FOUND. It then falls back to the
     * {958695A4-693A-435E-8297-66F805D8E46A} PnP interface, which is where the
     * original driver receives CFDC2110/acquisition traffic.
     *
     * Publishing the DOS alias on x64 changed XStream's control path and sent
     * the main IOCTL stream through an endpoint that does not exist on the
     * reference installation.
     */
    devExt->SymbolicLinkCreated = FALSE;
    LecTrace("AddDevice: legacy DOS alias intentionally not published\n");

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    LecTrace("AddDevice: PDO=%p FDO=%p lower=%p\n",
        PhysicalDeviceObject, deviceObject, devExt->LowerDeviceObject);

    return STATUS_SUCCESS;
}

VOID
LecS65Unload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    LecTrace("Unload\n");
}

/*
 * The admission lock gives STOP an atomic boundary: once disabled,
 * no new IOCTL may enter the hardware/transfer/event paths. STOP waits
 * for already admitted synchronous IOCTLs before freeing their resources.
 * This is distinct from the remove lock, which protects the device object.
 */
BOOLEAN
LecEnterIoctl(_Inout_ PLECS65_DEVICE_EXTENSION DevExt)
{
    KIRQL irql;
    BOOLEAN accepted;
    KeAcquireSpinLock(&DevExt->IoAdmissionLock, &irql);
    accepted = DevExt->AcceptIoctls;
    if (accepted) {
        if (DevExt->ActiveIoctls++ == 0) {
            KeClearEvent(&DevExt->IoIdleEvent);
        }
    }
    KeReleaseSpinLock(&DevExt->IoAdmissionLock, irql);
    return accepted;
}

VOID
LecLeaveIoctl(_Inout_ PLECS65_DEVICE_EXTENSION DevExt)
{
    KIRQL irql;
    KeAcquireSpinLock(&DevExt->IoAdmissionLock, &irql);
    NT_ASSERT(DevExt->ActiveIoctls != 0);
    if (--DevExt->ActiveIoctls == 0) {
        KeSetEvent(&DevExt->IoIdleEvent, IO_NO_INCREMENT, FALSE);
    }
    KeReleaseSpinLock(&DevExt->IoAdmissionLock, irql);
}

VOID
LecSetIoctlAdmission(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ BOOLEAN Enable)
{
    KIRQL irql;
    KeAcquireSpinLock(&DevExt->IoAdmissionLock, &irql);
    DevExt->AcceptIoctls = Enable;
    KeReleaseSpinLock(&DevExt->IoAdmissionLock, irql);
}

VOID
LecDrainIoctls(_Inout_ PLECS65_DEVICE_EXTENSION DevExt)
{
    /* Called at PASSIVE_LEVEL only after admission has been disabled. */
    (VOID)KeWaitForSingleObject(
        &DevExt->IoIdleEvent, Executive, KernelMode, FALSE, NULL);
}

NTSTATUS
LecS65PassThrough(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt;

    devExt = (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    NTSTATUS status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    if (devExt->LowerDeviceObject == NULL) {
        IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }

    return LecForwardLockedIrp(devExt, Irp, FALSE);
}

NTSTATUS
LecS65Create(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    LONGLONG count;
    NTSTATUS status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    count = InterlockedIncrement64(&devExt->CreateCount);

    LecTrace("CREATE: pid=%p requestor=%s started=%u count=%lld\n",
        PsGetCurrentProcessId(),
        Irp->RequestorMode == UserMode ? "user" : "kernel",
        devExt->Started,
        count);

    status = devExt->Removed ? STATUS_DELETE_PENDING : STATUS_SUCCESS;
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
LecS65Close(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    LONGLONG count;
    NTSTATUS status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    count = InterlockedIncrement64(&devExt->CloseCount);

    LecTrace("CLOSE: pid=%p count=%lld\n", PsGetCurrentProcessId(), count);

    /*
     * The x86 driver releases process-owned registered transfers on the last
     * process close. The x64 replacement uses opaque tokens rather than
     * leaking kernel pointers, and always enforces owner identity.
     */
    /*
     * STOP waits for process-owned transfer cleanup as well as IOCTLs.
     * A close racing with STOP leaves transfer release to the PnP teardown.
     */
    if (LecEnterIoctl(devExt)) {
        LecReleaseTransfersForProcess(devExt, PsGetCurrentProcessId());
        LecLeaveIoctl(devExt);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
