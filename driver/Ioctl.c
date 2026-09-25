#include "LecS65Drv.h"

static
BOOLEAN
LecIsPrivateDebugIoctl(
    _In_ ULONG Code
    )
{
    switch (Code) {
    case LECS65_IOCTL_DEBUG_GET_STATS:
    case LECS65_IOCTL_DEBUG_CLEAR_STATS:
    case LECS65_IOCTL_DEBUG_GET_BARS:
    case LECS65_IOCTL_DEBUG_GET_TRACE:
    case LECS65_IOCTL_DEBUG_CLEAR_TRACE:
        return TRUE;

    default:
        return FALSE;
    }
}

static
VOID
LecRecordIoctlTrace(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Code,
    _In_ ULONG Method,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _In_ BOOLEAN Wow64,
    _In_ NTSTATUS Status,
    _In_ ULONG_PTR Information,
    _In_reads_bytes_opt_(PreviewLength) const UCHAR* Preview,
    _In_ ULONG PreviewLength
    )
{
    KIRQL oldIrql;
    ULONG index;
    LARGE_INTEGER systemTime;
    PLECS65_DEBUG_TRACE_ENTRY entry;

    if (LecIsPrivateDebugIoctl(Code)) {
        return;
    }

    KeAcquireSpinLock(&DevExt->TraceLock, &oldIrql);

    index = DevExt->TraceWriteIndex;
    entry = &DevExt->Trace[index];
    RtlZeroMemory(entry, sizeof(*entry));

    entry->Sequence = ++DevExt->TraceNextSequence;
    KeQuerySystemTime(&systemTime);
    entry->Time100ns = (ULONGLONG)systemTime.QuadPart;
    entry->ProcessId = (ULONGLONG)(ULONG_PTR)PsGetCurrentProcessId();
    entry->Information = (ULONGLONG)Information;
    entry->Ioctl = Code;
    entry->InputLength = InputLength;
    entry->OutputLength = OutputLength;
    entry->Status = (ULONG)Status;
    entry->Method = (UCHAR)Method;
    entry->Wow64 = Wow64 ? 1 : 0;

    if (Preview != NULL && PreviewLength != 0) {
        entry->InputPreviewLength = min(
            PreviewLength,
            (ULONG)LECS65_TRACE_PREVIEW_BYTES);

        RtlCopyMemory(
            entry->InputPreview,
            Preview,
            entry->InputPreviewLength);
    }

    DevExt->TraceWriteIndex =
        (DevExt->TraceWriteIndex + 1) % LECS65_TRACE_CAPACITY;

    if (DevExt->TraceCount < LECS65_TRACE_CAPACITY) {
        ++DevExt->TraceCount;
    }

    KeReleaseSpinLock(&DevExt->TraceLock, oldIrql);
}

static
VOID
LecFillTrace(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_ PLECS65_DEBUG_TRACE Trace
    )
{
    KIRQL oldIrql;
    ULONG count;
    ULONG start;
    ULONG i;

    RtlZeroMemory(Trace, sizeof(*Trace));
    Trace->Version = 1;

    KeAcquireSpinLock(&DevExt->TraceLock, &oldIrql);

    count = DevExt->TraceCount;
    Trace->Count = count;
    Trace->TotalSeen = DevExt->TraceNextSequence;

    start = (DevExt->TraceWriteIndex +
             LECS65_TRACE_CAPACITY -
             count) % LECS65_TRACE_CAPACITY;

    for (i = 0; i < count; ++i) {
        ULONG source =
            (start + i) % LECS65_TRACE_CAPACITY;
        Trace->Entry[i] = DevExt->Trace[source];
    }

    KeReleaseSpinLock(&DevExt->TraceLock, oldIrql);
}

static
VOID
LecClearTrace(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&DevExt->TraceLock, &oldIrql);

    RtlZeroMemory(DevExt->Trace, sizeof(DevExt->Trace));
    DevExt->TraceNextSequence = 0;
    DevExt->TraceWriteIndex = 0;
    DevExt->TraceCount = 0;

    KeReleaseSpinLock(&DevExt->TraceLock, oldIrql);
}

static
NTSTATUS
LecResolveRegister(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ UCHAR Bar,
    _In_ ULONG Offset,
    _Out_ volatile ULONG** Register
    )
{
    if (!DevExt->Started) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (Bar >= LECS65_BAR_COUNT ||
        DevExt->Bar[Bar] == NULL ||
        DevExt->BarLength[Bar] < sizeof(ULONG)) {
        return STATUS_INVALID_PARAMETER;
    }

    if ((Offset & (sizeof(ULONG) - 1)) != 0) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    if (Offset > DevExt->BarLength[Bar] - sizeof(ULONG)) {
        return STATUS_INVALID_PARAMETER;
    }

    *Register = (volatile ULONG*)(DevExt->Bar[Bar] + Offset);
    return STATUS_SUCCESS;
}

static
NTSTATUS
LecIoctlRegisterRead(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_updates_bytes_(OutputLength) PVOID SystemBuffer,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _Out_ PULONG_PTR Information
    )
{
    UCHAR bar = 0;
    ULONG offset;
    volatile ULONG* reg;
    ULONG value;
    NTSTATUS status;

    if (SystemBuffer == NULL || OutputLength < sizeof(ULONG)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (InputLength == sizeof(LECS65_REG_READ_LEGACY)) {
        offset = ((PLECS65_REG_READ_LEGACY)SystemBuffer)->Offset;
    }
    else if (InputLength == sizeof(LECS65_REG_READ_EXT)) {
        PLECS65_REG_READ_EXT request =
            (PLECS65_REG_READ_EXT)SystemBuffer;
        bar = request->Bar;
        offset = request->Offset;
    }
    else {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = LecResolveRegister(DevExt, bar, offset, &reg);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    value = READ_REGISTER_ULONG(reg);
    *(PULONG)SystemBuffer = value;
    *Information = sizeof(ULONG);

    LecTrace("REG-RD: BAR%u + 0x%08lX -> 0x%08lX\n",
        bar, offset, value);

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecIoctlRegisterWrite(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(InputLength) PVOID SystemBuffer,
    _In_ ULONG InputLength,
    _Out_ PULONG_PTR Information
    )
{
    UCHAR bar = 0;
    ULONG offset;
    ULONG value;
    volatile ULONG* reg;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Information);

    if (SystemBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (InputLength == sizeof(LECS65_REG_WRITE_LEGACY)) {
        PLECS65_REG_WRITE_LEGACY request =
            (PLECS65_REG_WRITE_LEGACY)SystemBuffer;
        offset = request->Offset;
        value = request->Value;
    }
    else if (InputLength == sizeof(LECS65_REG_WRITE_EXT)) {
        PLECS65_REG_WRITE_EXT request =
            (PLECS65_REG_WRITE_EXT)SystemBuffer;
        bar = request->Bar;
        offset = request->Offset;
        value = request->Value;
    }
    else {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = LecResolveRegister(DevExt, bar, offset, &reg);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    LecTrace("REG-WR: BAR%u + 0x%08lX <- 0x%08lX\n",
        bar, offset, value);

    WRITE_REGISTER_ULONG(reg, value);
    return STATUS_SUCCESS;
}

static
VOID
LecFillStats(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_ PLECS65_DEBUG_STATS Stats
    )
{
    RtlZeroMemory(Stats, sizeof(*Stats));

    Stats->Version = 1;
    Stats->LegacyBuild = LECS65_LEGACY_DRIVER_BUILD;
    Stats->CreateCount = (ULONGLONG)InterlockedCompareExchange64(
        &DevExt->CreateCount, 0, 0);
    Stats->CloseCount = (ULONGLONG)InterlockedCompareExchange64(
        &DevExt->CloseCount, 0, 0);
    Stats->IoctlCount = (ULONGLONG)InterlockedCompareExchange64(
        &DevExt->IoctlCount, 0, 0);
    Stats->UnknownIoctlCount = (ULONGLONG)InterlockedCompareExchange64(
        &DevExt->UnknownIoctlCount, 0, 0);
    Stats->LastIoctl = (ULONG)InterlockedCompareExchange(
        &DevExt->LastIoctl, 0, 0);
}

NTSTATUS
LecS65DeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG method = code & 3;
    PVOID systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR information = 0;
    BOOLEAN wow64 = FALSE;
    UCHAR inputPreview[LECS65_TRACE_PREVIEW_BYTES];
    ULONG inputPreviewLength = 0;

    RtlZeroMemory(inputPreview, sizeof(inputPreview));

    InterlockedIncrement64(&devExt->IoctlCount);
    InterlockedExchange(&devExt->LastIoctl, (LONG)code);

    if (Irp->RequestorMode == UserMode) {
        wow64 = IoIs32bitProcess(Irp);
    }

    LecTrace(
        "IOCTL: pid=%p wow64=%u code=0x%08lX method=%lu in=%lu out=%lu "
        "sys=%p type3=%p user=%p\n",
        PsGetCurrentProcessId(),
        wow64,
        code,
        method,
        inputLength,
        outputLength,
        systemBuffer,
        stack->Parameters.DeviceIoControl.Type3InputBuffer,
        Irp->UserBuffer);

    if (method == METHOD_BUFFERED &&
        systemBuffer != NULL &&
        inputLength != 0) {
        inputPreviewLength = min(
            inputLength,
            (ULONG)sizeof(inputPreview));

        RtlCopyMemory(
            inputPreview,
            systemBuffer,
            inputPreviewLength);

        LecTrace("IOCTL input (first <=64 bytes):\n");
        LecHexDump((const UCHAR*)systemBuffer, inputLength);
    }

    switch (code) {
    case LECS65_IOCTL_GET_DRIVER_BUILD:
        if (systemBuffer == NULL || outputLength < sizeof(ULONG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)systemBuffer = LECS65_LEGACY_DRIVER_BUILD;
        information = sizeof(ULONG);
        status = STATUS_SUCCESS;

        LecTrace("GET_DRIVER_BUILD -> %lu\n", LECS65_LEGACY_DRIVER_BUILD);
        break;

    case LECS65_IOCTL_REGISTER_READ:
        status = LecIoctlRegisterRead(
            devExt,
            systemBuffer,
            inputLength,
            outputLength,
            &information);
        break;

    case LECS65_IOCTL_REGISTER_WRITE:
        status = LecIoctlRegisterWrite(
            devExt,
            systemBuffer,
            inputLength,
            &information);
        break;

    case LECS65_IOCTL_CFDC2184:
        /* Captured 2008 driver completes this control successfully with no data. */
        status = STATUS_SUCCESS;
        information = 0;
        LecTrace("legacy no-op 0xCFDC2184 -> STATUS_SUCCESS\n");
        break;

    case LECS65_IOCTL_CFDC212C:
        /* Captured 2008 driver always reports this as not implemented. */
        status = STATUS_NOT_IMPLEMENTED;
        information = 0;
        LecTrace("legacy 0xCFDC212C -> STATUS_NOT_IMPLEMENTED\n");
        break;

    case LECS65_IOCTL_DEBUG_GET_STATS:
        if (systemBuffer == NULL ||
            outputLength < sizeof(LECS65_DEBUG_STATS)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        LecFillStats(devExt, (PLECS65_DEBUG_STATS)systemBuffer);
        information = sizeof(LECS65_DEBUG_STATS);
        status = STATUS_SUCCESS;
        break;

    case LECS65_IOCTL_DEBUG_GET_BARS:
        if (systemBuffer == NULL ||
            outputLength < sizeof(LECS65_DEBUG_BARS)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        else {
            PLECS65_DEBUG_BARS bars = (PLECS65_DEBUG_BARS)systemBuffer;
            ULONG i;

            RtlZeroMemory(bars, sizeof(*bars));
            bars->Version = 1;
            bars->Count = LECS65_BAR_COUNT;

            for (i = 0; i < LECS65_BAR_COUNT; ++i) {
                bars->Entry[i].PhysicalAddress =
                    (ULONGLONG)devExt->BarPhysical[i].QuadPart;
                bars->Entry[i].Length = devExt->BarLength[i];
            }

            bars->Bulk.PhysicalAddress =
                (ULONGLONG)devExt->BulkMmioPhysical.QuadPart;
            bars->Bulk.Length = devExt->BulkMmioLength;

            information = sizeof(*bars);
            status = STATUS_SUCCESS;
        }
        break;

    case LECS65_IOCTL_DEBUG_GET_TRACE:
        if (systemBuffer == NULL ||
            outputLength < sizeof(LECS65_DEBUG_TRACE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        LecFillTrace(devExt, (PLECS65_DEBUG_TRACE)systemBuffer);
        information = sizeof(LECS65_DEBUG_TRACE);
        status = STATUS_SUCCESS;
        break;

    case LECS65_IOCTL_DEBUG_CLEAR_TRACE:
        LecClearTrace(devExt);
        status = STATUS_SUCCESS;
        information = 0;
        break;

    case LECS65_IOCTL_DEBUG_CLEAR_STATS:
        InterlockedExchange64(&devExt->CreateCount, 0);
        InterlockedExchange64(&devExt->CloseCount, 0);
        InterlockedExchange64(&devExt->IoctlCount, 0);
        InterlockedExchange64(&devExt->UnknownIoctlCount, 0);
        InterlockedExchange(&devExt->LastIoctl, 0);
        status = STATUS_SUCCESS;
        break;

    default:
        InterlockedIncrement64(&devExt->UnknownIoctlCount);

        if (method == METHOD_NEITHER) {
            LecTrace(
                "UNKNOWN METHOD_NEITHER: intentionally NOT dereferencing "
                "Type3InputBuffer/UserBuffer in bring-up build\n");
        }
        else {
            LecTrace("UNKNOWN IOCTL 0x%08lX -> STATUS_INVALID_DEVICE_REQUEST\n",
                code);
        }
        break;
    }

    if (method == METHOD_BUFFERED &&
        NT_SUCCESS(status) &&
        information != 0 &&
        systemBuffer != NULL) {
        LecTrace("IOCTL output (%Iu bytes, first <=64):\n", information);
        LecHexDump((const UCHAR*)systemBuffer, (ULONG)min(information, MAXULONG));
    }

    LecTrace("IOCTL done: code=0x%08lX status=0x%08X info=%Iu\n",
        code, status, information);

    LecRecordIoctlTrace(
        devExt,
        code,
        method,
        inputLength,
        outputLength,
        wow64,
        status,
        information,
        inputPreviewLength != 0 ? inputPreview : NULL,
        inputPreviewLength);

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
