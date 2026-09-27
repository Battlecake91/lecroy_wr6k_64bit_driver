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
    case LECS65_IOCTL_DEBUG_GET_PCI_CONFIG:
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
    _In_opt_ PVOID Type3InputBuffer,
    _In_opt_ PVOID UserBuffer,
    _In_reads_bytes_opt_(PreviewLength) const UCHAR* Preview,
    _In_ ULONG PreviewLength,
    _In_reads_bytes_opt_(OutputPreviewLength) const UCHAR* OutputPreview,
    _In_ ULONG OutputPreviewLength
    )
{
    KIRQL oldIrql;
    ULONG index;
    PLECS65_DEBUG_TRACE_ENTRY entry;

    if (LecIsPrivateDebugIoctl(Code)) {
        return;
    }

    /*
     * Keep successful legacy register reads in the trace.
     *
     * Earlier bring-up builds suppressed CFDC21C0 because a divergent x64
     * startup path polled it heavily. The current interface-based path is now
     * close enough to the legacy flow that these reads are diagnostically
     * important: the original application performs register/status reads
     * immediately around the transition from the late JTAG status poll into
     * CFDC2124/CFDC2138 acquisition traffic.
     *
     * lecdiag trace-capture snapshots the ring every 250 ms, so retain the
     * reads rather than hiding the very control-flow discriminator needed for
     * the current comparison.
     */

    KeAcquireSpinLock(&DevExt->TraceLock, &oldIrql);

    index = DevExt->TraceWriteIndex;
    entry = &DevExt->Trace[index];
    RtlZeroMemory(entry, sizeof(*entry));

    entry->Sequence = ++DevExt->TraceNextSequence;
    entry->TimestampTicks =
        (ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart;
    entry->ProcessId = (ULONGLONG)(ULONG_PTR)PsGetCurrentProcessId();
    entry->Information = (ULONGLONG)Information;
    entry->Type3InputBuffer = (ULONGLONG)(ULONG_PTR)Type3InputBuffer;
    entry->UserBuffer = (ULONGLONG)(ULONG_PTR)UserBuffer;
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

    if (OutputPreview != NULL && OutputPreviewLength != 0) {
        entry->OutputPreviewLength = min(
            OutputPreviewLength,
            (ULONG)LECS65_TRACE_OUTPUT_PREVIEW_BYTES);

        RtlCopyMemory(
            entry->OutputPreview,
            OutputPreview,
            entry->OutputPreviewLength);
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
    Trace->Version = 3;

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
volatile ULONG*
LecOneWireRegister(
    _In_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    if (!DevExt->Started ||
        DevExt->Bar[2] == NULL ||
        DevExt->BarLength[2] <
            LECS65_ONEWIRE_OFFSET + sizeof(ULONG)) {
        return NULL;
    }

    return (volatile ULONG*)(
        DevExt->Bar[2] + LECS65_ONEWIRE_OFFSET);
}

static
NTSTATUS
LecOneWireIssue(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Command,
    _Out_opt_ PULONG FinalStatus
    )
{
    volatile ULONG* reg = LecOneWireRegister(DevExt);
    ULONG i;
    ULONG value = 0;

    if (reg == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    WRITE_REGISTER_ULONG(reg, Command);

    for (i = 0; i < LECS65_ONEWIRE_POLL_LIMIT; ++i) {
        value = READ_REGISTER_ULONG(reg);

        if ((value & LECS65_ONEWIRE_BUSY) == 0) {
            if (FinalStatus != NULL) {
                *FinalStatus = value;
            }
            return STATUS_SUCCESS;
        }
    }

    LecTrace(
        "ONEWIRE timeout: command=%lu last=0x%08lX\n",
        Command,
        value);

    return STATUS_IO_TIMEOUT;
}

static
NTSTATUS
LecOneWireReset(
    _In_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    ULONG status;
    NTSTATUS ntStatus;

    ntStatus = LecOneWireIssue(DevExt, 0, &status);
    if (!NT_SUCCESS(ntStatus)) {
        return ntStatus;
    }

    /*
     * Recovered original semantics:
     * bit0 = controller busy
     * bit1 = sampled 1-Wire line / presence result.
     * After reset, bit1 == 0 means a device answered the presence pulse.
     */
    status = READ_REGISTER_ULONG(LecOneWireRegister(DevExt));

    if ((status & LECS65_ONEWIRE_DATA) != 0) {
        LecTrace(
            "ONEWIRE reset: no presence, status=0x%08lX\n",
            status);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecOneWireWriteByte(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ UCHAR Value
    )
{
    ULONG bit;
    NTSTATUS status;

    for (bit = 0; bit < 8; ++bit) {
        ULONG command =
            (Value & 0x01) != 0 ? 2UL : 1UL;

        status = LecOneWireIssue(DevExt, command, NULL);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        Value >>= 1;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecOneWireReadByte(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_ PUCHAR Value
    )
{
    volatile ULONG* reg = LecOneWireRegister(DevExt);
    UCHAR result = 0;
    ULONG bit;
    NTSTATUS status;

    if (reg == NULL || Value == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    for (bit = 0; bit < 8; ++bit) {
        ULONG sampled;

        status = LecOneWireIssue(DevExt, 3, NULL);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        sampled = READ_REGISTER_ULONG(reg);

        if ((sampled & LECS65_ONEWIRE_DATA) != 0) {
            result |= (UCHAR)(1U << bit);
        }
    }

    *Value = result;
    return STATUS_SUCCESS;
}

static
UCHAR
LecDallasCrc8(
    _In_reads_bytes_(Length) const UCHAR* Data,
    _In_ ULONG Length
    )
{
    UCHAR crc = 0;
    ULONG i;

    for (i = 0; i < Length; ++i) {
        UCHAR in = Data[i];
        ULONG bit;

        for (bit = 0; bit < 8; ++bit) {
            UCHAR mix = (UCHAR)((crc ^ in) & 0x01);

            crc >>= 1;
            if (mix != 0) {
                crc ^= 0x8C;
            }

            in >>= 1;
        }
    }

    return crc;
}

static
NTSTATUS
LecDallasReadId(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_writes_bytes_(8) UCHAR Id[8]
    )
{
    ULONG attempt;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    KeWaitForSingleObject(
        &DevExt->DallasMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    for (attempt = 0; attempt < 10; ++attempt) {
        ULONG i;

        status = LecOneWireReset(DevExt);
        if (!NT_SUCCESS(status)) {
            continue;
        }

        status = LecOneWireWriteByte(DevExt, 0x33);
        if (!NT_SUCCESS(status)) {
            continue;
        }

        for (i = 0; i < 8; ++i) {
            status = LecOneWireReadByte(DevExt, &Id[i]);
            if (!NT_SUCCESS(status)) {
                break;
            }
        }

        if (!NT_SUCCESS(status)) {
            continue;
        }

        if (LecDallasCrc8(Id, 7) == Id[7]) {
            status = STATUS_SUCCESS;
            break;
        }

        LecTrace(
            "DALLAS ID CRC mismatch attempt=%lu id=%02X %02X %02X %02X %02X %02X %02X %02X\n",
            attempt + 1,
            Id[0], Id[1], Id[2], Id[3],
            Id[4], Id[5], Id[6], Id[7]);

        status = STATUS_CRC_ERROR;
    }

    KeReleaseMutex(&DevExt->DallasMutex, FALSE);
    return status;
}

static
NTSTATUS
LecDallasReadMemory(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_writes_bytes_(Length) PUCHAR Buffer,
    _In_ ULONG Length
    )
{
    ULONG i;
    NTSTATUS status;

    if (Buffer == NULL || Length == 0 || Length > 0x200) {
        return STATUS_INVALID_PARAMETER;
    }

    KeWaitForSingleObject(
        &DevExt->DallasMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    status = LecOneWireReset(DevExt);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    /*
     * Recovered from the 2008 binary:
     *   0xCC = SKIP ROM
     *   0xF0 = READ MEMORY
     *   0x00, 0x00 = start address 0x0000
     */
    status = LecOneWireWriteByte(DevExt, 0xCC);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = LecOneWireWriteByte(DevExt, 0xF0);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = LecOneWireWriteByte(DevExt, 0x00);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = LecOneWireWriteByte(DevExt, 0x00);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    for (i = 0; i < Length; ++i) {
        status = LecOneWireReadByte(DevExt, &Buffer[i]);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
    }

Exit:
    KeReleaseMutex(&DevExt->DallasMutex, FALSE);
    return status;
}

static
NTSTATUS
LecCaptureLegacyEvent(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PKEVENT* Slot,
    _In_ ULONG HandleValue,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PKEVENT eventObject = NULL;
    PKEVENT oldEvent;
    KIRQL oldIrql;
    NTSTATUS status;

    status = ObReferenceObjectByHandle(
        ULongToHandle(HandleValue),
        EVENT_MODIFY_STATE,
        *ExEventObjectType,
        AccessMode,
        (PVOID*)&eventObject,
        NULL);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    KeClearEvent(eventObject);

    KeAcquireSpinLock(&DevExt->LegacyEventLock, &oldIrql);
    oldEvent = *Slot;
    *Slot = eventObject;
    KeReleaseSpinLock(&DevExt->LegacyEventLock, oldIrql);

    if (oldEvent != NULL) {
        ObDereferenceObject(oldEvent);
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecIoctlSetSingleEvent(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PKEVENT* Slot,
    _In_reads_bytes_(InputLength) PVOID SystemBuffer,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    ULONG handleValue;

    if (SystemBuffer == NULL ||
        InputLength != sizeof(ULONG) ||
        OutputLength != 0) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    handleValue = *(PULONG)SystemBuffer;
    return LecCaptureLegacyEvent(
        DevExt,
        Slot,
        handleValue,
        AccessMode);
}

static
NTSTATUS
LecIoctlSetThreeEvents(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(InputLength) PVOID SystemBuffer,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PULONG handles;
    PKEVENT event2 = NULL;
    PKEVENT event3 = NULL;
    PKEVENT event4 = NULL;
    PKEVENT oldEvent2 = NULL;
    PKEVENT oldEvent3 = NULL;
    PKEVENT oldEvent4 = NULL;
    KIRQL oldIrql;
    NTSTATUS status;

    if (SystemBuffer == NULL ||
        InputLength != 3 * sizeof(ULONG) ||
        OutputLength != 0) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    handles = (PULONG)SystemBuffer;

    status = ObReferenceObjectByHandle(
        ULongToHandle(handles[0]),
        EVENT_MODIFY_STATE,
        *ExEventObjectType,
        AccessMode,
        (PVOID*)&event2,
        NULL);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = ObReferenceObjectByHandle(
        ULongToHandle(handles[1]),
        EVENT_MODIFY_STATE,
        *ExEventObjectType,
        AccessMode,
        (PVOID*)&event3,
        NULL);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = ObReferenceObjectByHandle(
        ULongToHandle(handles[2]),
        EVENT_MODIFY_STATE,
        *ExEventObjectType,
        AccessMode,
        (PVOID*)&event4,
        NULL);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    KeClearEvent(event2);
    KeClearEvent(event3);
    KeClearEvent(event4);

    KeAcquireSpinLock(&DevExt->LegacyEventLock, &oldIrql);

    oldEvent2 = DevExt->LegacyEvent2;
    oldEvent3 = DevExt->LegacyEvent3;
    oldEvent4 = DevExt->LegacyEvent4;

    DevExt->LegacyEvent2 = event2;
    DevExt->LegacyEvent3 = event3;
    DevExt->LegacyEvent4 = event4;

    KeReleaseSpinLock(&DevExt->LegacyEventLock, oldIrql);

    event2 = NULL;
    event3 = NULL;
    event4 = NULL;

    if (oldEvent2 != NULL) {
        ObDereferenceObject(oldEvent2);
    }
    if (oldEvent3 != NULL) {
        ObDereferenceObject(oldEvent3);
    }
    if (oldEvent4 != NULL) {
        ObDereferenceObject(oldEvent4);
    }

    status = STATUS_SUCCESS;

Exit:
    if (event2 != NULL) {
        ObDereferenceObject(event2);
    }
    if (event3 != NULL) {
        ObDereferenceObject(event3);
    }
    if (event4 != NULL) {
        ObDereferenceObject(event4);
    }

    return status;
}

static
NTSTATUS
LecGetBar1Register(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Offset,
    _Out_ volatile ULONG** Register
    )
{
    if (!DevExt->Started ||
        DevExt->Bar[1] == NULL ||
        DevExt->BarLength[1] < sizeof(ULONG) ||
        Offset > DevExt->BarLength[1] - sizeof(ULONG)) {
        return STATUS_DEVICE_NOT_READY;
    }

    *Register = (volatile ULONG*)(DevExt->Bar[1] + Offset);
    return STATUS_SUCCESS;
}

static
VOID
LecDelayOneMillisecond(VOID)
{
    LARGE_INTEGER interval;

    interval.QuadPart = -10000LL;
    (VOID)KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

static
NTSTATUS
LecTransportWaitTxIdle(
    _In_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    volatile ULONG* control;
    ULONG poll;
    NTSTATUS status;

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_TX_CONTROL,
        &control);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (poll = 0; poll < LECS65_TRANSFER_TIMEOUT_POLLS; ++poll) {
        ULONG value = READ_REGISTER_ULONG(control);

        if ((value & 0x8000UL) == 0 &&
            (value & 0x00FFUL) == 0) {
            return STATUS_SUCCESS;
        }

        LecDelayOneMillisecond();
    }

    return STATUS_IO_TIMEOUT;
}

static
NTSTATUS
LecTransportSend(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(Length) const UCHAR* Buffer,
    _In_ ULONG Length
    )
{
    volatile ULONG* control;
    volatile ULONG* totalCount;
    ULONG remainingWords;
    ULONG byteOffset = 0;
    NTSTATUS status;

    if (Buffer == NULL || Length == 0 || (Length & 1U) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_TX_CONTROL,
        &control);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_TX_COUNT,
        &totalCount);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    remainingWords = Length / 2;

    while (remainingWords != 0) {
        ULONG chunkWords = min(remainingWords, 0x78UL);
        ULONG command = chunkWords;
        ULONG i;

        status = LecTransportWaitTxIdle(DevExt);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        for (i = 0; i < chunkWords; ++i) {
            volatile ULONG* slot;
            USHORT word;
            ULONG value;

            status = LecGetBar1Register(
                DevExt,
                LECS65_BAR1_TX_DATA + i * sizeof(ULONG),
                &slot);
            if (!NT_SUCCESS(status)) {
                return status;
            }

            RtlCopyMemory(
                &word,
                Buffer + byteOffset + i * sizeof(USHORT),
                sizeof(word));
            value = word;
            WRITE_REGISTER_ULONG(slot, value);
        }

        WRITE_REGISTER_ULONG(totalCount, remainingWords);

        if (remainingWords > chunkWords) {
            command |= 0x4000UL;
        }

        command |= 0x8000UL;
        WRITE_REGISTER_ULONG(control, command);

        byteOffset += chunkWords * sizeof(USHORT);
        remainingWords -= chunkWords;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecTransportReceive(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_writes_bytes_(Capacity) UCHAR* Buffer,
    _In_ ULONG Capacity,
    _Out_ PULONG Received
    )
{
    volatile ULONG* control;
    ULONG offset = 0;
    NTSTATUS status;

    if (Buffer == NULL || Received == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    *Received = 0;

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_RX_CONTROL,
        &control);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (;;) {
        ULONG poll;
        ULONG value = 0;
        ULONG words;
        ULONG i;

        for (poll = 0; poll < LECS65_TRANSFER_TIMEOUT_POLLS; ++poll) {
            value = READ_REGISTER_ULONG(control);
            if ((value & 0x8000UL) != 0) {
                break;
            }
            LecDelayOneMillisecond();
        }

        if ((value & 0x8000UL) == 0) {
            return STATUS_IO_TIMEOUT;
        }

        words = value & 0x00FFUL;
        if (words > 0x78UL ||
            offset + words * sizeof(USHORT) > Capacity) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        for (i = 0; i < words; ++i) {
            volatile ULONG* slot;
            ULONG raw;
            USHORT word;

            status = LecGetBar1Register(
                DevExt,
                LECS65_BAR1_RX_DATA + i * sizeof(ULONG),
                &slot);
            if (!NT_SUCCESS(status)) {
                return status;
            }

            raw = READ_REGISTER_ULONG(slot);
            word = (USHORT)raw;
            RtlCopyMemory(
                Buffer + offset + i * sizeof(USHORT),
                &word,
                sizeof(word));
        }

        offset += words * sizeof(USHORT);

        WRITE_REGISTER_ULONG(
            control,
            value & ~0x80FFUL);

        if ((value & 0x4000UL) == 0) {
            break;
        }
    }

    *Received = offset;
    return STATUS_SUCCESS;
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
USHORT
LecReadU16(
    _In_reads_bytes_(sizeof(USHORT)) const UCHAR* Buffer
    )
{
    USHORT value;

    RtlCopyMemory(&value, Buffer, sizeof(value));
    return value;
}

static
ULONG
LecReadU32(
    _In_reads_bytes_(sizeof(ULONG)) const UCHAR* Buffer
    )
{
    ULONG value;

    RtlCopyMemory(&value, Buffer, sizeof(value));
    return value;
}

static
VOID
LecWriteU16(
    _Out_writes_bytes_(sizeof(USHORT)) UCHAR* Buffer,
    _In_ USHORT Value
    )
{
    RtlCopyMemory(Buffer, &Value, sizeof(Value));
}

static
VOID
LecWriteU32(
    _Out_writes_bytes_(sizeof(ULONG)) UCHAR* Buffer,
    _In_ ULONG Value
    )
{
    RtlCopyMemory(Buffer, &Value, sizeof(Value));
}

typedef struct _LECS65_LEGACY_REGISTER_LIST_ENTRY {
    PCSTR Name;
    UCHAR Bar;
    ULONG Offset;
    UCHAR Type;
} LECS65_LEGACY_REGISTER_LIST_ENTRY;

#define LECS65_LEGACY_TRACE_BLOCK_BYTES 0x110UL
#define LECS65_LEGACY_REGISTER_ENTRY_BYTES 0x10AUL
#define LECS65_LEGACY_REGISTER_COUNT 43UL
#define LECS65_LEGACY_REGISTER_LIST_BYTES \
    (LECS65_LEGACY_REGISTER_COUNT * LECS65_LEGACY_REGISTER_ENTRY_BYTES)

static const LECS65_LEGACY_REGISTER_LIST_ENTRY
g_LecLegacyRegisterList[LECS65_LEGACY_REGISTER_COUNT] = {
    /*
     * Live original-driver capture establishes this exact public ordering.
     * XStream requests indexed entry zero and expects TxControl there.
     */
    { "TxControl", 1, 0x400, 4 },
    { "RxControl", 1, 0x404, 4 },
    { "TxCount",   1, 0x408, 4 },
    { "RxCount",   1, 0x40C, 4 },
    { "SetIRQ",    1, 0x100, 2 },
    { "HWInt",     1, 0x410, 4 },
    { "FVER",      0, 0x000, 1 },
    { "ERRS",      0, 0x004, 4 },
    { "ERRM",      0, 0x008, 2 },
    { "INTST",     0, 0x080, 4 },
    { "IIMCL",     0, 0x048, 2 },
    { "CLRIRQ",    1, 0x008, 2 },
    { "CLRERR",    1, 0x004, 2 },
    { "INTEN",     0, 0x084, 2 },
    { "SGTA",      0, 0x040, 2 },
    { "IIMTC",     0, 0x044, 2 },
    { "IIMST",     0, 0x04C, 1 },
    { "BUZZER",    2, 0x000, 2 },
    { "ONEWIRE",   2, 0x040, 4 },
    { "START",     0, 0x00C, 4 },
    { "ITMODE",    1, 0x000, 2 },
    { "MAMDAT",    1, 0x040, 2 },
    { "MAMPGO",    1, 0x044, 2 },
    { "MAMSEQ",    1, 0x060, 2 },
    { "MAMRGO",    1, 0x064, 2 },
    { "SPICTL",    1, 0x0A0, 2 },
    { "SPIDAT",    1, 0x0A4, 2 },
    { "SPIDIN",    1, 0x0A8, 1 },
    { "JTAGNUM",   1, 0x020, 2 },
    { "JTAGDAT",   1, 0x024, 2 },
    { "JTAGDIN",   1, 0x028, 1 },
    { "MTTCTL",    1, 0x080, 2 },
    { "MTTRGO",    1, 0x084, 2 },
    { "MTTNUM",    1, 0x090, 1 },
    { "LEDCTL",    1, 0x0E0, 2 },
    { "ACQFVER",   1, 0x00C, 1 },
    { "RMIDIV",    1, 0x0E4, 2 },
    { "RMICUM",    1, 0x0E8, 1 },
    { "ACQDIV",    1, 0x0EC, 2 },
    { "ACQCUM",    1, 0x0F0, 1 },
    { "PFREG",     1, 0x0F4, 2 },
    { "GPIODIR",   1, 0x0C0, 2 },
    { "GPIODAT",   1, 0x0C4, 4 }
};

static
NTSTATUS
LecFillLegacyTraceBlock(
    _Out_writes_bytes_(OutputLength) UCHAR* Buffer,
    _In_ ULONG OutputLength
    )
{
    static const CHAR traceName[] = "CKeTraceControl: ";

    if (Buffer == NULL ||
        OutputLength != LECS65_LEGACY_TRACE_BLOCK_BYTES) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlZeroMemory(Buffer, OutputLength);
    LecWriteU32(Buffer + 0x000, LECS65_LEGACY_TRACE_BLOCK_BYTES);
    LecWriteU32(Buffer + 0x004, 1UL);
    RtlCopyMemory(
        Buffer + 0x008,
        traceName,
        min((ULONG)sizeof(traceName), 0x100UL));
    LecWriteU32(Buffer + 0x108, 2UL);
    LecWriteU32(Buffer + 0x10C, 0UL);
    return STATUS_SUCCESS;
}

static
NTSTATUS
LecFillLegacyRegisterEntry(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Index,
    _Out_writes_bytes_(LECS65_LEGACY_REGISTER_ENTRY_BYTES) UCHAR* Target
    )
{
    const LECS65_LEGACY_REGISTER_LIST_ENTRY* source;
    ULONG data = 0;
    SIZE_T nameLength = 0;
    volatile ULONG* reg;
    NTSTATUS status;

    if (Target == NULL ||
        Index >= LECS65_LEGACY_REGISTER_COUNT) {
        return STATUS_INVALID_PARAMETER;
    }

    source = &g_LecLegacyRegisterList[Index];
    RtlZeroMemory(Target, LECS65_LEGACY_REGISTER_ENTRY_BYTES);

    while (source->Name[nameLength] != '\0' &&
           nameLength < 0xFF) {
        ++nameLength;
    }

    if (nameLength != 0) {
        RtlCopyMemory(Target, source->Name, nameLength);
    }

    Target[0x100] = source->Bar;
    LecWriteU32(Target + 0x101, source->Offset);
    Target[0x105] = source->Type;

    /*
     * Legacy type 2 returns the wrapper's shadow DWORD rather than
     * touching MMIO. Those shadows are zero immediately after
     * construction unless updated through a wrapper write.
     *
     * Types 0, 1 and 4 are refreshed from hardware before serialization.
     */
    if (source->Type != 2) {
        status = LecResolveRegister(
            DevExt,
            source->Bar,
            source->Offset,
            &reg);

        if (NT_SUCCESS(status)) {
            data = READ_REGISTER_ULONG(reg);
        }
    }

    if (source->Bar == 0 &&
        source->Offset == 0x084) {
        data = DevExt->InterruptEnableShadow;
    }

    LecWriteU32(Target + 0x106, data);
    return STATUS_SUCCESS;
}

static
NTSTATUS
LecFillLegacyRegisterList(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_writes_bytes_(OutputLength) UCHAR* Buffer,
    _In_ ULONG OutputLength
    )
{
    ULONG i;

    if (Buffer == NULL ||
        OutputLength != LECS65_LEGACY_REGISTER_LIST_BYTES) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    for (i = 0; i < LECS65_LEGACY_REGISTER_COUNT; ++i) {
        NTSTATUS status = LecFillLegacyRegisterEntry(
            DevExt,
            i,
            Buffer + i * LECS65_LEGACY_REGISTER_ENTRY_BYTES);

        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecCommitLegacyInterruptMask(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG NewMask
    )
{
    volatile ULONG* inten;
    NTSTATUS status;

    status = LecResolveRegister(DevExt, 0, 0x084, &inten);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    InterlockedExchange(
        (volatile LONG*)&DevExt->InterruptEnableShadow,
        (LONG)NewMask);

    WRITE_REGISTER_ULONG(inten, NewMask);

    LecTrace(
        "CFDC2110 legacy INTEN shadow <- 0x%08lX\n",
        NewMask);

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecResetLegacyInterruptState(
    _In_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    volatile ULONG* intst;
    volatile ULONG* errs;
    volatile ULONG* iimcl;
    volatile ULONG* clrirq;
    volatile ULONG* clrerr;
    ULONG interruptState;
    NTSTATUS status;

    status = LecResolveRegister(DevExt, 0, 0x080, &intst);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 0, 0x004, &errs);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 0, 0x048, &iimcl);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 1, 0x008, &clrirq);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 1, 0x004, &clrerr);
    if (!NT_SUCCESS(status)) return status;

    interruptState = READ_REGISTER_ULONG(intst);

    if ((interruptState & 0x01UL) != 0) {
        WRITE_REGISTER_ULONG(iimcl, 0);
    }

    if ((interruptState & 0x02UL) != 0) {
        ULONG errorState = READ_REGISTER_ULONG(errs);
        ULONG clearMask = 0;

        if ((errorState & 0x0400UL) != 0) clearMask |= 0x01;
        if ((errorState & 0x0800UL) != 0) clearMask |= 0x02;
        if ((errorState & 0x1000UL) != 0) clearMask |= 0x04;
        if ((errorState & 0x2000UL) != 0) clearMask |= 0x08;
        if ((errorState & 0x4000UL) != 0) clearMask |= 0x10;

        if (clearMask != 0) {
            WRITE_REGISTER_ULONG(clrerr, clearMask);
        }

        WRITE_REGISTER_ULONG(errs, errorState);
    }

    if ((interruptState & 0x04UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 1);
    }

    if ((interruptState & 0x08UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 2);
    }

    WRITE_REGISTER_ULONG(intst, interruptState);

    WRITE_REGISTER_ULONG(clrirq, 3);
    WRITE_REGISTER_ULONG(intst, 0xFFFFFFFFUL);
    (VOID)READ_REGISTER_ULONG(intst);

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecJtagExecute(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(RequestLength) const UCHAR* Request,
    _In_ ULONG RequestLength,
    _Out_writes_bytes_(ResponseCapacity) UCHAR* Response,
    _In_ ULONG ResponseCapacity,
    _Out_ PULONG ResponseLength
    )
{
    volatile ULONG* jtagNum;
    volatile ULONG* jtagData;
    volatile ULONG* jtagIn;
    ULONG requestedDataBytes;
    ULONG bitCount;
    ULONG remainingBits;
    ULONG inputOffset = 9;
    ULONG outputOffset = 8;
    ULONG produced = 0;
    UCHAR mode;
    NTSTATUS status;

    if (Request == NULL ||
        Response == NULL ||
        ResponseLength == NULL ||
        RequestLength < 9) {
        return STATUS_INVALID_PARAMETER;
    }

    mode = Request[0];
    if (mode > 1) {
        return STATUS_INVALID_PARAMETER;
    }

    requestedDataBytes = LecReadU16(Request + 3);
    bitCount = LecReadU16(Request + 5);

    if (ResponseCapacity < 8 + requestedDataBytes) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_JTAG_NUM,
        &jtagNum);
    if (!NT_SUCCESS(status)) return status;
    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_JTAG_DATA,
        &jtagData);
    if (!NT_SUCCESS(status)) return status;
    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_JTAG_IN,
        &jtagIn);
    if (!NT_SUCCESS(status)) return status;

    RtlZeroMemory(Response, 8 + requestedDataBytes);
    remainingBits = bitCount;

    /*
     * Legacy FUN_00015C7E programs JTAGNUM once for the entire run of
     * full 16-bit chunks. It then streams JTAGDAT words without touching
     * JTAGNUM again. Only the final partial chunk reprograms JTAGNUM.
     * Rewriting JTAGNUM for every 16-bit word restarts the FPGA JTAG shift
     * operation and is not equivalent.
     */
    if (remainingBits >= 16UL) {
        WRITE_REGISTER_ULONG(
            jtagNum,
            ((mode != 0) ? 0x100UL : 0UL));

        while (remainingBits >= 16UL) {
            USHORT firstWord;
            USHORT secondWord;
            ULONG dataValue;
            ULONG inputValue;
            USHORT outputWord;

            if (inputOffset + 4 > RequestLength ||
                outputOffset + 2 > 8 + requestedDataBytes) {
                return STATUS_INVALID_BUFFER_SIZE;
            }

            firstWord = LecReadU16(Request + inputOffset);
            secondWord = LecReadU16(Request + inputOffset + 2);

            dataValue =
                ((ULONG)firstWord << 16) |
                (ULONG)secondWord;
            WRITE_REGISTER_ULONG(jtagData, dataValue);

            inputValue = READ_REGISTER_ULONG(jtagIn);
            outputWord = (USHORT)(inputValue >> 16);
            LecWriteU16(Response + outputOffset, outputWord);

            inputOffset += 4;
            outputOffset += 2;
            produced += 2;
            remainingBits -= 16UL;
        }
    }

    if (remainingBits != 0) {
        USHORT firstWord;
        USHORT secondWord;
        ULONG dataValue;
        ULONG inputValue;
        USHORT outputWord;

        if (inputOffset + 4 > RequestLength ||
            outputOffset + 2 > 8 + requestedDataBytes) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        firstWord = LecReadU16(Request + inputOffset);
        secondWord = LecReadU16(Request + inputOffset + 2);

        WRITE_REGISTER_ULONG(
            jtagNum,
            ((mode != 0) ? 0x100UL : 0UL) |
            (remainingBits & 0x0FUL));

        dataValue =
            ((ULONG)firstWord << 16) |
            (ULONG)secondWord;
        WRITE_REGISTER_ULONG(jtagData, dataValue);

        inputValue = READ_REGISTER_ULONG(jtagIn);
        outputWord = (USHORT)(
            inputValue >> (32 - remainingBits));
        LecWriteU16(Response + outputOffset, outputWord);

        outputOffset += 2;
        produced += 2;
    }

    LecWriteU32(Response, 0);
    LecWriteU16(Response + 4, (USHORT)(produced + 2));
    LecWriteU16(Response + 6, 0);

    *ResponseLength = 8 + produced;
    return STATUS_SUCCESS;
}


static
NTSTATUS
LecJtagWriteOnly(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(RequestLength) const UCHAR* Request,
    _In_ ULONG RequestLength
    )
{
    volatile ULONG* jtagNum;
    volatile ULONG* jtagData;
    ULONG remainingBits;
    ULONG inputOffset = 5;
    UCHAR mode;
    NTSTATUS status;

    if (Request == NULL || RequestLength < 5) {
        return STATUS_INVALID_PARAMETER;
    }

    mode = Request[0];
    remainingBits = Request[1];

    if (mode > 1) {
        return STATUS_INVALID_PARAMETER;
    }

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_JTAG_NUM,
        &jtagNum);
    if (!NT_SUCCESS(status)) return status;

    status = LecGetBar1Register(
        DevExt,
        LECS65_BAR1_JTAG_DATA,
        &jtagData);
    if (!NT_SUCCESS(status)) return status;

    /*
     * Match legacy FUN_00015ACC: program JTAGNUM once for all complete
     * 16-bit chunks, stream JTAGDAT, then reprogram JTAGNUM only for a
     * final partial chunk.
     */
    if (remainingBits >= 16UL) {
        WRITE_REGISTER_ULONG(
            jtagNum,
            ((mode != 0) ? 0x100UL : 0UL));

        while (remainingBits >= 16UL) {
            USHORT firstWord;
            USHORT secondWord;
            ULONG dataValue;

            if (inputOffset + 4 > RequestLength) {
                return STATUS_INVALID_BUFFER_SIZE;
            }

            firstWord = LecReadU16(Request + inputOffset);
            secondWord = LecReadU16(Request + inputOffset + 2);
            dataValue =
                ((ULONG)firstWord << 16) |
                (ULONG)secondWord;
            WRITE_REGISTER_ULONG(jtagData, dataValue);

            inputOffset += 4;
            remainingBits -= 16UL;
        }
    }

    if (remainingBits != 0) {
        USHORT firstWord;
        USHORT secondWord;
        ULONG dataValue;

        if (inputOffset + 4 > RequestLength) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        firstWord = LecReadU16(Request + inputOffset);
        secondWord = LecReadU16(Request + inputOffset + 2);

        WRITE_REGISTER_ULONG(
            jtagNum,
            ((mode != 0) ? 0x100UL : 0UL) |
            (remainingBits & 0x0FUL));

        dataValue =
            ((ULONG)firstWord << 16) |
            (ULONG)secondWord;
        WRITE_REGISTER_ULONG(jtagData, dataValue);
    }

    return STATUS_SUCCESS;
}

static
ULONG
LecReverseBits32(
    _In_ ULONG Value
    )
{
    ULONG result = 0;
    ULONG bit;

    for (bit = 0; bit < 32; ++bit) {
        if ((Value & 0x80000000UL) != 0) {
            result |= 1UL << bit;
        }
        Value <<= 1;
    }

    return result;
}

static
BOOLEAN
LecLegacySpiSelect(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ UCHAR Selector,
    _In_ BOOLEAN Asserted
    )
{
    ULONG shadow = DevExt->LegacySpiControlShadow;
    ULONG state = Asserted ? 1UL : 0UL;

    switch (Selector) {
    case 0:
        shadow = (state << 14) | (shadow & 0xFFFFBC1FUL);
        break;
    case 1:
        shadow = (state << 15) | (shadow & 0xFFFF7C3FUL) | 0x20UL;
        break;
    case 2:
        shadow = (state << 16) | (shadow & 0xFFFEFD5FUL) | 0x140UL;
        break;
    case 3:
        shadow = (state << 17) | (shadow & 0xFFFDFD7FUL) | 0x160UL;
        break;
    case 4:
        shadow = (state << 18) | (shadow & 0xFFFBFD9FUL) | 0x180UL;
        break;
    case 0x0C:
        shadow = (((state << 13) ^ shadow) & 0x2000UL) ^ shadow;
        shadow |= 0x300UL;
        break;
    case 0x0E:
        shadow = (state << 12) | (shadow & 0xFFFFEEBFUL) | 0x2A0UL;
        break;
    default:
        return FALSE;
    }

    DevExt->LegacySpiControlShadow = shadow;
    return TRUE;
}

static
NTSTATUS
LecLegacySpiWrite(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(RequestLength) const UCHAR* Request,
    _In_ ULONG RequestLength
    )
{
    volatile ULONG* spiCtl;
    volatile ULONG* spiData;
    ULONG bitCount;
    ULONG remainingBits;
    ULONG inputOffset = 5;
    NTSTATUS status;

    if (Request == NULL || RequestLength < 5) {
        return STATUS_INVALID_PARAMETER;
    }

    bitCount = LecReadU16(Request + 1);

    if (RequestLength < 5UL + ((bitCount + 15UL) / 16UL) * 2UL) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = LecGetBar1Register(DevExt, 0x0A0, &spiCtl);
    if (!NT_SUCCESS(status)) return status;
    status = LecGetBar1Register(DevExt, 0x0A4, &spiData);
    if (!NT_SUCCESS(status)) return status;

    if (!DevExt->LegacySpiInitialized) {
        /*
         * Legacy FUN_0001340C initializes the SPI helper shadow with 0x1FF000
         * and immediately commits it to SPICTL.
         */
        DevExt->LegacySpiControlShadow = 0x001FF000UL;
        WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);
        DevExt->LegacySpiInitialized = TRUE;
    }

    if (LecReadU16(Request + 3) != 0) {
        if (!LecLegacySpiSelect(DevExt, Request[0], FALSE)) {
            return STATUS_INVALID_PARAMETER;
        }
        WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);

        if (!LecLegacySpiSelect(DevExt, Request[0], TRUE)) {
            return STATUS_INVALID_PARAMETER;
        }
        WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);
    }

    if (!LecLegacySpiSelect(DevExt, Request[0], FALSE)) {
        return STATUS_INVALID_PARAMETER;
    }

    remainingBits = bitCount;

    if (remainingBits < 17UL) {
        DevExt->LegacySpiControlShadow =
            (DevExt->LegacySpiControlShadow & ~0x1FUL) |
            (remainingBits & 0x1FUL);
    }
    else {
        DevExt->LegacySpiControlShadow =
            (DevExt->LegacySpiControlShadow & 0xFFFFFFF0UL) |
            0x10UL;
    }

    WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);

    while (remainingBits > 15UL) {
        USHORT word = LecReadU16(Request + inputOffset);
        WRITE_REGISTER_ULONG(spiData, LecReverseBits32((ULONG)word));
        inputOffset += 2;
        remainingBits -= 16UL;
    }

    if (remainingBits != 0) {
        USHORT word = LecReadU16(Request + inputOffset);

        DevExt->LegacySpiControlShadow =
            (DevExt->LegacySpiControlShadow & ~0x1FUL) |
            (remainingBits & 0x1FUL);
        WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);
        WRITE_REGISTER_ULONG(spiData, LecReverseBits32((ULONG)word));
    }

    if (!LecLegacySpiSelect(DevExt, Request[0], TRUE)) {
        return STATUS_INVALID_PARAMETER;
    }

    DevExt->LegacySpiControlShadow &= ~0x1FUL;
    WRITE_REGISTER_ULONG(spiCtl, DevExt->LegacySpiControlShadow);

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecApplyMamConfigRecord(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_reads_bytes_(PayloadLength) const UCHAR* Payload,
    _In_ ULONG PayloadLength,
    _In_ UCHAR Mode
    )
{
    volatile ULONG* gpioData;
    volatile ULONG* mamData;
    volatile ULONG* mamPgo;
    ULONG count;
    ULONG i;
    ULONG gpioValue;
    NTSTATUS status;

    if (Payload == NULL || (PayloadLength & 1U) != 0) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    count = PayloadLength / 2UL;
    if (count > 0xFFUL) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = LecGetBar1Register(DevExt, 0x0C4, &gpioData);
    if (!NT_SUCCESS(status)) return status;
    status = LecGetBar1Register(DevExt, 0x040, &mamData);
    if (!NT_SUCCESS(status)) return status;
    status = LecGetBar1Register(DevExt, 0x044, &mamPgo);
    if (!NT_SUCCESS(status)) return status;

    gpioValue = READ_REGISTER_ULONG(gpioData);
    WRITE_REGISTER_ULONG(gpioData, gpioValue & 0xFFFEFFFFUL);

    /*
     * Legacy FUN_00011A00 constructs the 256 indexed MAMDAT shadows as
     * DWORD 0xFFFFFFFF. FUN_000179E2 compares the low 16-bit data value,
     * so the effective initial value for every index is 0xFFFF.
     */
    if (!DevExt->LegacyMamShadowInitialized) {
        RtlFillMemory(
            DevExt->LegacyMamShadow,
            sizeof(DevExt->LegacyMamShadow),
            0xFF);
        DevExt->LegacyMamShadowInitialized = TRUE;
    }

    for (i = 0; i < count; ++i) {
        USHORT data =
            LecReadU16(Payload + i * 2UL);

        /*
         * Legacy FUN_000179E2 keeps a 16-bit shadow per MAM index and
         * suppresses duplicate MAMDAT writes. MAMPGO is still issued for
         * every record, even when every indexed data value is unchanged.
         */
        if (DevExt->LegacyMamShadow[i] != data) {
            ULONG value =
                (i << 16) |
                (ULONG)data;

            DevExt->LegacyMamShadow[i] = data;
            WRITE_REGISTER_ULONG(mamData, value);
        }
    }

    WRITE_REGISTER_ULONG(
        mamPgo,
        (((ULONG)Mode & 3UL) << 8) | count);

    return STATUS_SUCCESS;
}

static
NTSTATUS
LecLegacyTimerArmOrExtend(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG RequestedMilliseconds
    )
{
    LARGE_INTEGER zeroTimeout;
    LARGE_INTEGER dueTime;
    LARGE_INTEGER counterFrequency;
    LARGE_INTEGER nowCounter;
    NTSTATUS waitStatus;
    ULONG requested = RequestedMilliseconds != 0 ?
        RequestedMilliseconds : 1UL;

    if (!DevExt->LegacyTimerInitialized) {
        KeInitializeTimerEx(
            &DevExt->LegacyTimer,
            NotificationTimer);
        DevExt->LegacyTimerStartTime.QuadPart = 0;
        DevExt->LegacyTimerDurationMs = 0;
        DevExt->LegacyTimerInitialized = TRUE;
    }

    zeroTimeout.QuadPart = 0;
    waitStatus = KeWaitForSingleObject(
        &DevExt->LegacyTimer,
        Executive,
        KernelMode,
        FALSE,
        &zeroTimeout);

    nowCounter = KeQueryPerformanceCounter(&counterFrequency);

    if (waitStatus == STATUS_SUCCESS) {
        dueTime.QuadPart =
            -((LONGLONG)requested * 10000LL);

        (VOID)KeSetTimer(
            &DevExt->LegacyTimer,
            dueTime,
            NULL);

        DevExt->LegacyTimerStartTime = nowCounter;
        DevExt->LegacyTimerDurationMs = requested;
        return STATUS_SUCCESS;
    }

    if (waitStatus == STATUS_TIMEOUT) {
        LONGLONG elapsedTicks =
            nowCounter.QuadPart -
            DevExt->LegacyTimerStartTime.QuadPart;
        ULONG elapsedMs = 0;

        if (elapsedTicks > 0 &&
            counterFrequency.QuadPart > 0) {
            elapsedMs = (ULONG)(
                (elapsedTicks * 1000LL) /
                counterFrequency.QuadPart);
        }
        LONG remainingMs =
            (LONG)DevExt->LegacyTimerDurationMs -
            (LONG)elapsedMs;

        if (remainingMs < (LONG)RequestedMilliseconds) {
            dueTime.QuadPart =
                -((LONGLONG)requested * 10000LL);

            (VOID)KeSetTimer(
                &DevExt->LegacyTimer,
                dueTime,
                NULL);

            DevExt->LegacyTimerStartTime = nowCounter;
            DevExt->LegacyTimerDurationMs = requested;
        }

        return STATUS_SUCCESS;
    }

    return waitStatus;
}

static
NTSTATUS
LecExecuteLegacyMttTransferLocked(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer,
    _In_ USHORT LaunchUnits
    )
{
    volatile ULONG* sgta;
    volatile ULONG* iimtc;
    volatile ULONG* iimcl;
    volatile ULONG* mttrgo;
    ULONG currentMask;
    ULONG cleanupMask;
    LARGE_INTEGER timeout;
    NTSTATUS status;
    NTSTATUS disableStatus;
    BOOLEAN transferSelected = FALSE;
    BOOLEAN transferInterruptEnabled = FALSE;

    if (Transfer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
     * Caller holds TransferMutex.  Legacy FUN_000160DC resolves the transfer
     * entry first, then FUN_00017478/FUN_000171DE uses that selected entry for
     * the synchronous MTTRGO launch.
     */
    if (DevExt->CurrentTransfer != NULL) {
        return STATUS_DEVICE_BUSY;
    }

    status = LecResolveRegister(DevExt, 0, 0x040, &sgta);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 0, 0x044, &iimtc);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 0, 0x048, &iimcl);
    if (!NT_SUCCESS(status)) return status;
    status = LecResolveRegister(DevExt, 1, 0x084, &mttrgo);
    if (!NT_SUCCESS(status)) return status;

    WRITE_REGISTER_ULONG(
        sgta,
        Transfer->DescriptorTablePhysical);
    WRITE_REGISTER_ULONG(
        iimtc,
        Transfer->TotalDwords);

    KeResetEvent(&Transfer->CompletionEvent);
    (VOID)InterlockedAnd(
        (volatile LONG*)&DevExt->InterruptPendingShadow,
        ~1L);

    (VOID)InterlockedExchangePointer(
        (PVOID volatile*)&DevExt->CurrentTransfer,
        Transfer);
    transferSelected = TRUE;

    currentMask = (ULONG)InterlockedCompareExchange(
        (volatile LONG*)&DevExt->InterruptEnableShadow,
        0,
        0);

    status = LecCommitLegacyInterruptMask(
        DevExt,
        currentMask | 0x01UL);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    transferInterruptEnabled = TRUE;

    WRITE_REGISTER_ULONG(iimcl, 1UL);
    WRITE_REGISTER_ULONG(mttrgo, (ULONG)LaunchUnits);

    timeout.QuadPart = -50000000LL;
    status = KeWaitForSingleObject(
        &Transfer->CompletionEvent,
        Executive,
        KernelMode,
        FALSE,
        &timeout);

    if (status == STATUS_TIMEOUT) {
        status = STATUS_IO_TIMEOUT;
    }

Cleanup:
    if (transferInterruptEnabled) {
        cleanupMask = (ULONG)InterlockedCompareExchange(
            (volatile LONG*)&DevExt->InterruptEnableShadow,
            0,
            0);

        disableStatus = LecCommitLegacyInterruptMask(
            DevExt,
            cleanupMask & ~0x01UL);

        if (NT_SUCCESS(status) &&
            !NT_SUCCESS(disableStatus)) {
            status = disableStatus;
        }
    }

    if (transferSelected) {
        (VOID)InterlockedExchangePointer(
            (PVOID volatile*)&DevExt->CurrentTransfer,
            NULL);
    }

    LecTrace(
        "CFDC2110 family1 MTT DMA: token=%lu bytes=%lu SGTA=0x%08lX IIMTC=%lu MTTRGO=%u status=0x%08X\n",
        Transfer->Token,
        Transfer->DataBytes,
        Transfer->DescriptorTablePhysical,
        Transfer->TotalDwords,
        (ULONG)LaunchUnits,
        status);

    return status;
}

static
BOOLEAN
LecIsStructurallySupportedCfDc2110(
    _In_reads_bytes_(InputLength) const UCHAR* Buffer,
    _In_ ULONG InputLength
    )
{
    ULONG offset = 0;
    BOOLEAN sawForwardCommand = FALSE;

    /*
     * Admit statically decoded command classes rather than individual captured
     * packet literals, while still validating the packed CFDC2110 framing
     * before hardware is touched.
     *
     * Most admitted family-1 classes are direct board-forwarding commands.
     * Opcodes 0x50/0x51 are different: FUN_000165A6 dispatches both to local
     * host handler FUN_000160DC, which resolves a registered transfer token and
     * launches the already-built descriptor chain through MTTRGO.
     *
     * 85FB fetch records are allowed only as companions to a supported request.
     */
    if (Buffer == NULL || InputLength < 8) {
        return FALSE;
    }

    while (offset < InputLength) {
        USHORT payloadLength;
        USHORT type;
        USHORT signature;
        ULONG span;
        const UCHAR* payload;

        if (InputLength - offset < 8) {
            return FALSE;
        }

        payloadLength = LecReadU16(Buffer + offset + 2);
        type = LecReadU16(Buffer + offset + 4);
        signature = LecReadU16(Buffer + offset + 6);
        span = 8UL + (ULONG)payloadLength;

        if (span < 8 || span > InputLength - offset) {
            return FALSE;
        }

        payload = Buffer + offset + 8;

        if (type == 1 || type == 2) {
            if ((payloadLength & 1U) != 0 || payloadLength / 2U > 0xFFU) {
                return FALSE;
            }
            sawForwardCommand = TRUE;
        }
        else if (type != 3) {
            return FALSE;
        }
        else if (signature == 0xA5FB) {
            UCHAR family;
            UCHAR opcode;

            if (payloadLength < 3 || payload[0] != 0x40) {
                return FALSE;
            }

            family = payload[1];
            opcode = payload[2];

            if ((family == 0 && opcode == 0x42)) {
                const UCHAR* request = payload + 3;
                ULONG requestLength = (ULONG)payloadLength - 3;
                ULONG bitCount;
                ULONG chunks;

                if (requestLength < 5 || request[0] > 1) {
                    return FALSE;
                }

                bitCount = request[1];
                chunks = (bitCount + 15UL) / 16UL;
                if (requestLength < 5UL + chunks * 4UL) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 0 && opcode == 0x92) {
                ULONG registerOffset;

                if (payloadLength != 10 || payload[3] != 1) {
                    return FALSE;
                }

                registerOffset = (ULONG)LecReadU16(payload + 4);
                if ((registerOffset & 3UL) != 0) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 2 && opcode == 0x01) {
                if (payloadLength < 8 || payload[3] != 1) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 2 && opcode == 0x02) {
                if (payloadLength < 4 || payload[3] > 1) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 2 && opcode == 0x05) {
                sawForwardCommand = TRUE;
            }
            else if (family == 2 && opcode == 0x10) {
                if (payloadLength < 5) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 0 && opcode == 0x85) {
                if (payloadLength < 6) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 0 && opcode == 0x88) {
                /*
                 * FUN_000169xx handles opcode 0x88 generically: the trailing
                 * WORD is consumed as a mask and the command is then forwarded
                 * to board firmware. Runtime traces show multiple legitimate
                 * mask values (for example 0xFFDF and 0x001F), so this must not
                 * be restricted to one captured packet literal.
                 */
                if (payloadLength < 6) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 0 && opcode == 0xA0) {
                if (payloadLength < 6) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 0 && opcode == 0x90) {
                const UCHAR* request = payload + 3;
                ULONG requestLength = (ULONG)payloadLength - 3;
                ULONG bitCount;

                if (requestLength < 5) {
                    return FALSE;
                }

                bitCount = LecReadU16(request + 1);
                if (requestLength < 5UL + ((bitCount + 15UL) / 16UL) * 2UL) {
                    return FALSE;
                }

                switch (request[0]) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 0x0C:
                case 0x0E:
                    break;
                default:
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 1 &&
                     (opcode == 0x50 || opcode == 0x51)) {
                /*
                 * FUN_000160DC consumes exactly seven bytes after the opcode:
                 * one ignored/control byte, DWORD transfer token and WORD
                 * MTTRGO launch value.
                 */
                if (payloadLength != 10) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if (family == 1 && opcode == 0x42) {
                const UCHAR* request = payload + 3;
                ULONG requestOffset =
                    offset + 8UL + 3UL;
                ULONG available;
                ULONG bitCount;
                ULONG required;

                if (payloadLength < 12 ||
                    request[0] > 1 ||
                    requestOffset > InputLength) {
                    return FALSE;
                }

                bitCount = LecReadU16(request + 5);
                required =
                    9UL + ((bitCount + 15UL) / 16UL) * 4UL;
                available = InputLength - requestOffset;

                if (required > available) {
                    return FALSE;
                }

                sawForwardCommand = TRUE;
            }
            else if ((family == 0 &&
                      (opcode == 0x4A ||
                       opcode == 0x84 ||
                       opcode == 0x86 ||
                       opcode == 0x87 ||
                       opcode == 0x96 ||
                       opcode == 0x97 ||
                       opcode == 0xA1 ||
                       opcode == 0xA2)) ||
                     (family == 1 &&
                      (opcode == 0x4A ||
                       opcode == 0x81 ||
                       opcode == 0x82 ||
                       opcode == 0x90 ||
                       opcode == 0x91 ||
                       opcode == 0x96 ||
                       opcode == 0x97 ||
                       opcode == 0x99))) {
                sawForwardCommand = TRUE;
            }
            else {
                return FALSE;
            }
        }
        else if (signature == 0x85FB) {
            if (payloadLength < 2 ||
                payload[0] != 0x40 ||
                payload[1] != 0x00) {
                return FALSE;
            }
        }
        else {
            return FALSE;
        }

        offset += span;
    }

    return sawForwardCommand && offset == InputLength;
}

static
BOOLEAN
LecIsAllowedCfDc2110(
    _In_reads_bytes_(InputLength) const UCHAR* Buffer,
    _In_ ULONG InputLength
    )
{
    static const UCHAR resetPacket[] = {
        0x06,0x00,0x0A,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x02,0x40,0x01,0x52,0x45,0x53,0x45,0x54,0x00
    };
    static const UCHAR opcode88Packet[] = {
        0x06,0x00,0x06,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x00,0x88,0x00,0xDF,0xFF,
        0x08,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR status85fbPacket[] = {
        0x0A,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x01
    };
    static const UCHAR jtag42Packet[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x00,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode1LongPacket[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x01,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode2LongPacket[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x02,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode3LongPacket[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x03,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode4LongPacket[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x04,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode5LongPacket[] = {
        0x06,0x00,0x4C,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x05,0x00,0x00,0x20,0x00,
        0x00,0x01,0x00,0x00,0xFF,0x0B,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x28,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42ShortPacket[] = {
        0x06,0x00,0x24,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x00,0x00,0x00,0x0C,0x00,
        0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,
        0x07,0x00,0x00,0x00,0x14,0x00,0x02,0x00,
        0x03,0x00,0xFB,0x85,0x40,0x00
    };
    static const UCHAR jtag42Mode1ShortPacket[] = {
        0x06,0x00,0x24,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x01,0x00,0x00,0x0C,0x00,
        0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,
        0x07,0x00,0x00,0x00,0x14,0x00,0x02,0x00,
        0x03,0x00,0xFB,0x85,0x40,0x00
    };

    static const UCHAR jtag42Mode1Packet[] = {
        0x06,0x00,0x18,0x00,0x03,0x00,0xFB,0xA5,
        0x40,0x01,0x42,0x01,0x00,0x00,0x08,0x00,
        0x3A,0x00,0x00,0x00,0xDF,0xC0,0x00,0x20,
        0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x10,0x00,0x02,0x00,0x03,0x00,0xFB,0x85,0x40,0x00
    };

    if (Buffer == NULL) {
        return FALSE;
    }

    if (LecIsStructurallySupportedCfDc2110(Buffer, InputLength)) {
        return TRUE;
    }

#define LECS65_MATCH_CAPTURED_PACKET(packet) \
    (InputLength == sizeof(packet) && \
     RtlCompareMemory(Buffer, (packet), sizeof(packet)) == sizeof(packet))

    if (LECS65_MATCH_CAPTURED_PACKET(resetPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(opcode88Packet) ||
        LECS65_MATCH_CAPTURED_PACKET(status85fbPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Packet) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode1LongPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode2LongPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode3LongPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode4LongPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode5LongPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42ShortPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode1ShortPacket) ||
        LECS65_MATCH_CAPTURED_PACKET(jtag42Mode1Packet)) {
        return TRUE;
    }

#undef LECS65_MATCH_CAPTURED_PACKET
    return FALSE;
}

static
NTSTATUS
LecIoctlCfDc2110(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_updates_bytes_(OutputLength) UCHAR* SystemBuffer,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _Out_ PULONG_PTR Information
    )
{
    UCHAR* inputCopy = NULL;
    UCHAR* pendingResponse = NULL;
    ULONG pendingResponseLength = 0;
    BOOLEAN pendingResponseReady = FALSE;
    BOOLEAN hardwareResponsePending = FALSE;
    BOOLEAN pendingResponseIsRawHardware = FALSE;
    USHORT pendingResponseLengthOverride = 0;
    ULONG inputOffset;
    ULONG outputOffset;
    ULONG totalOutput = 0;
    NTSTATUS status = STATUS_SUCCESS;

    if (SystemBuffer == NULL || InputLength == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
     * METHOD_BUFFERED uses the same SystemBuffer for input and output.
     * The legacy handler first captures/parses the complete record list before
     * writing responses. Keep a private copy so an early output record cannot
     * overwrite a later input record.
     */
    inputCopy = (UCHAR*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        InputLength,
        LECS65_TAG);
    if (inputCopy == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pendingResponse = (UCHAR*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        max(OutputLength, 8UL),
        LECS65_TAG);
    if (pendingResponse == NULL) {
        ExFreePool(inputCopy);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(inputCopy, SystemBuffer, InputLength);

    /* First pass: validate framing and the caller-provided output size. */
    inputOffset = 0;
    while (inputOffset < InputLength) {
        ULONG payloadLength;
        ULONG recordOutput;

        if (InputLength - inputOffset < 8) {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto Exit;
        }

        recordOutput = LecReadU16(inputCopy + inputOffset);
        payloadLength = LecReadU16(inputCopy + inputOffset + 2);

        if (payloadLength > InputLength - inputOffset - 8) {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto Exit;
        }

        if (recordOutput > OutputLength - totalOutput) {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        totalOutput += recordOutput;
        inputOffset += 8 + payloadLength;
    }

    if (inputOffset != InputLength) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto Exit;
    }

    RtlZeroMemory(SystemBuffer, OutputLength);

    KeWaitForSingleObject(
        &DevExt->TransferMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    inputOffset = 0;
    outputOffset = 0;

    while (inputOffset < InputLength) {
        const UCHAR* record = inputCopy + inputOffset;
        ULONG recordOutput = LecReadU16(record);
        ULONG payloadLength = LecReadU16(record + 2);
        USHORT type = LecReadU16(record + 4);
        USHORT signature = LecReadU16(record + 6);
        const UCHAR* payload = record + 8;
        UCHAR* recordResult = SystemBuffer + outputOffset;

        if (type == 1 || type == 2) {
            NTSTATUS hwStatus = LecApplyMamConfigRecord(
                DevExt,
                payload,
                payloadLength,
                1U);

            if (!NT_SUCCESS(hwStatus)) {
                status = hwStatus;
                break;
            }

            if (recordOutput >= 6) {
                LecWriteU32(recordResult, 0);
                LecWriteU16(recordResult + 4, 0);
            }

            outputOffset += recordOutput;
            inputOffset += 8 + payloadLength;
            continue;
        }

        if (type != 3) {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        if (signature == 0xA5FB) {
            USHORT protocolStatus = 2;

            if (recordOutput < 6 || payloadLength < 3 ||
                payload[0] != 0x40) {
                protocolStatus = 2;
            }
            else if (payload[1] == 2 && payload[2] == 0x40) {
                NTSTATUS hwStatus =
                    LecResetLegacyInterruptState(DevExt);
                protocolStatus =
                    NT_SUCCESS(hwStatus) ? 0 : 8;
            }
            else if (payload[1] == 1 &&
                     (payload[2] == 0x4A ||
                      payload[2] == 0x81 ||
                      payload[2] == 0x82 ||
                      payload[2] == 0x90 ||
                      payload[2] == 0x91 ||
                      payload[2] == 0x96 ||
                      payload[2] == 0x97 ||
                      payload[2] == 0x99)) {
                NTSTATUS hwStatus = LecTransportSend(
                    DevExt,
                    record + 6,
                    payloadLength + 2);

                if (NT_SUCCESS(hwStatus)) {
                    hardwareResponsePending = TRUE;
                    pendingResponseReady = FALSE;
                    pendingResponseIsRawHardware = FALSE;
                    pendingResponseLengthOverride =
                        (payload[2] == 0x99) ? 2U : 0U;
                    pendingResponseLength = 0;
                    protocolStatus = 0;
                }
                else {
                    protocolStatus = 8;
                }
            }
            else if (payload[1] == 2 && payload[2] == 0x01) {
                NTSTATUS timerStatus;
                ULONG requestedMilliseconds;

                if (payloadLength < 8 || payload[3] != 1) {
                    protocolStatus = 4;
                }
                else {
                    RtlCopyMemory(
                        &requestedMilliseconds,
                        payload + 4,
                        sizeof(requestedMilliseconds));

                    timerStatus = LecLegacyTimerArmOrExtend(
                        DevExt,
                        requestedMilliseconds);

                    protocolStatus =
                        NT_SUCCESS(timerStatus) ? 0 : 8;
                }

                LecWriteU32(pendingResponse, 0);
                LecWriteU16(pendingResponse + 4, 2);
                LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                pendingResponseLength = 8;
                pendingResponseReady = TRUE;
                pendingResponseIsRawHardware = FALSE;
                hardwareResponsePending = FALSE;
            }
            else if (payload[1] == 2 && payload[2] == 0x05) {
                volatile ULONG* itmode;
                NTSTATUS hwStatus = LecGetBar1Register(
                    DevExt,
                    0x000,
                    &itmode);

                if (NT_SUCCESS(hwStatus)) {
                    WRITE_REGISTER_ULONG(itmode, 7UL);
                    WRITE_REGISTER_ULONG(itmode, 3UL);
                    protocolStatus = 0;
                }
                else {
                    protocolStatus = 8;
                }

                LecWriteU32(pendingResponse, 0);
                LecWriteU16(pendingResponse + 4, 2);
                LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                pendingResponseLength = 8;
                pendingResponseReady = TRUE;
                pendingResponseIsRawHardware = FALSE;
                hardwareResponsePending = FALSE;
            }
            else if (payload[1] == 2 && payload[2] == 0x10) {
                volatile ULONG* ledCtl;
                ULONG value = 0;
                NTSTATUS hwStatus;

                if (payloadLength < 5) {
                    protocolStatus = 4;
                }
                else {
                    if (payload[3] != 0) {
                        value |= 2UL;
                    }
                    if (payload[4] != 0) {
                        value |= 1UL;
                    }

                    hwStatus = LecGetBar1Register(
                        DevExt,
                        0x0E0,
                        &ledCtl);

                    if (NT_SUCCESS(hwStatus)) {
                        WRITE_REGISTER_ULONG(ledCtl, value);
                        protocolStatus = 0;
                    }
                    else {
                        protocolStatus = 8;
                    }
                }

                LecWriteU32(pendingResponse, 0);
                LecWriteU16(pendingResponse + 4, 2);
                LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                pendingResponseLength = 8;
                pendingResponseReady = TRUE;
                pendingResponseIsRawHardware = FALSE;
                hardwareResponsePending = FALSE;
            }
            else if (payload[1] == 2 && payload[2] == 0x02) {
                NTSTATUS hwStatus = STATUS_SUCCESS;
                volatile ULONG* mttCtl = NULL;
                UCHAR enable = payload[3];

                /*
                 * Legacy FUN_000163B2 waits on the KTIMER owned by the
                 * command object before it writes MTTCTL. This timer is armed
                 * by family-2 opcode 0x01. The old x64 path incorrectly waited
                 * on a DMA completion event instead, which removes the required
                 * hardware settling delay before MTTCTL changes state.
                 */
                if (DevExt->LegacyTimerInitialized) {
                    hwStatus = KeWaitForSingleObject(
                        &DevExt->LegacyTimer,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL);
                }

                if (NT_SUCCESS(hwStatus)) {
                    hwStatus = LecGetBar1Register(
                        DevExt,
                        0x080,
                        &mttCtl);
                }

                if (NT_SUCCESS(hwStatus)) {
                    WRITE_REGISTER_ULONG(mttCtl, enable != 0 ? 1UL : 0UL);
                    protocolStatus = 0;
                }
                else {
                    protocolStatus = 8;
                }

                LecWriteU32(pendingResponse, 0);
                LecWriteU16(pendingResponse + 4, 2);
                LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                pendingResponseLength = 8;
                pendingResponseReady = TRUE;
                pendingResponseIsRawHardware = FALSE;
                hardwareResponsePending = FALSE;
            }
            else if (payload[1] == 0 && payload[2] == 0x42) {
                NTSTATUS hwStatus = LecJtagWriteOnly(
                    DevExt,
                    payload + 3,
                    payloadLength - 3);

                if (NT_SUCCESS(hwStatus)) {
                    protocolStatus = 0;
                    LecWriteU32(pendingResponse, 0);
                    LecWriteU16(pendingResponse + 4, 2);
                    LecWriteU16(pendingResponse + 6, 0);
                    pendingResponseLength = 8;
                    pendingResponseReady = TRUE;
                    hardwareResponsePending = FALSE;
                }
                else if (hwStatus == STATUS_INVALID_PARAMETER ||
                         hwStatus == STATUS_INVALID_BUFFER_SIZE) {
                    protocolStatus = 4;
                }
                else {
                    protocolStatus = 8;
                }
            }
            else if (payload[1] == 0 && payload[2] == 0x92) {
                NTSTATUS hwStatus;
                volatile ULONG* target;
                ULONG offset;
                ULONG value;

                if (payloadLength != 10 || payload[3] != 1) {
                    protocolStatus = 4;
                }
                else {
                    offset = (ULONG)LecReadU16(payload + 4);
                    RtlCopyMemory(&value, payload + 6, sizeof(value));

                    hwStatus = LecGetBar1Register(
                        DevExt,
                        offset,
                        &target);

                    if (NT_SUCCESS(hwStatus)) {
                        WRITE_REGISTER_ULONG(target, value);
                        protocolStatus = 0;
                    }
                    else {
                        protocolStatus = 8;
                    }

                    LecWriteU32(pendingResponse, 0);
                    LecWriteU16(pendingResponse + 4, 2);
                    LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                    pendingResponseLength = 8;
                    pendingResponseReady = TRUE;
                    hardwareResponsePending = FALSE;
                }
            }
            else if (payload[1] == 0 && payload[2] == 0x90) {
                NTSTATUS hwStatus = LecLegacySpiWrite(
                    DevExt,
                    payload + 3,
                    payloadLength - 3);

                if (NT_SUCCESS(hwStatus)) {
                    protocolStatus = 0;
                }
                else if (hwStatus == STATUS_INVALID_PARAMETER ||
                         hwStatus == STATUS_INVALID_BUFFER_SIZE) {
                    protocolStatus = 4;
                }
                else {
                    protocolStatus = 8;
                }

                LecWriteU32(pendingResponse, 0);
                LecWriteU16(pendingResponse + 4, 2);
                LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                pendingResponseLength = 8;
                pendingResponseReady = TRUE;
                pendingResponseIsRawHardware = FALSE;
                hardwareResponsePending = FALSE;
            }
            else if (payload[1] == 0 && payload[2] == 0xA0) {
                NTSTATUS hwStatus;
                volatile ULONG* pfreg;
                USHORT value;

                if (payloadLength < 6) {
                    protocolStatus = 8;
                }
                else {
                    value = LecReadU16(payload + 4);
                    hwStatus = LecGetBar1Register(
                        DevExt,
                        0x0F4,
                        &pfreg);

                    if (NT_SUCCESS(hwStatus)) {
                        WRITE_REGISTER_ULONG(pfreg, (ULONG)value);
                        protocolStatus = 0;
                    }
                    else {
                        protocolStatus = 8;
                    }

                    LecWriteU32(pendingResponse, 0);
                    LecWriteU16(pendingResponse + 4, 2);
                    LecWriteU16(pendingResponse + 6, (USHORT)protocolStatus);
                    pendingResponseLength = 8;
                    pendingResponseReady = TRUE;
                    hardwareResponsePending = FALSE;
                }
            }
            else if (payload[1] == 0 &&
                     (payload[2] == 0x4A ||
                      payload[2] == 0x84 ||
                      payload[2] == 0x86 ||
                      payload[2] == 0x87 ||
                      payload[2] == 0x96 ||
                      payload[2] == 0x97 ||
                      payload[2] == 0xA1 ||
                      payload[2] == 0xA2)) {
                NTSTATUS hwStatus = LecTransportSend(
                    DevExt,
                    record + 6,
                    payloadLength + 2);

                if (NT_SUCCESS(hwStatus)) {
                    hardwareResponsePending = TRUE;
                    pendingResponseReady = FALSE;
                    pendingResponseLength = 0;
                    protocolStatus = 0;
                }
                else {
                    protocolStatus = 8;
                }
            }
            else if (payload[1] == 0 && payload[2] == 0x88) {
                NTSTATUS hwStatus = STATUS_SUCCESS;
                USHORT mask = 0;

                if (payloadLength < 6) {
                    protocolStatus = 4;
                }
                else {
                    KIRQL oldIrql;

                    mask = LecReadU16(payload + 4);

                    /*
                     * Legacy FUN_00016A66 first clears sticky command-status
                     * bits for every opcode-0x88 request.
                     */
                    KeAcquireSpinLock(
                        &DevExt->LegacyEventLock,
                        &oldIrql);
                    DevExt->LegacyCommandPendingMask &=
                        (USHORT)~mask;
                    KeReleaseSpinLock(
                        &DevExt->LegacyEventLock,
                        oldIrql);

                    /*
                     * Critical legacy special case:
                     *
                     *   mask == 0x0080 or 0x0800
                     *
                     * is handled entirely in the host driver. FUN_00016A66
                     * calls FUN_00015A88(this, 0), which installs the local
                     * eight-byte response { DWORD 0, WORD 2, WORD 0 }, and
                     * does NOT forward the command to board firmware.
                     *
                     * Other masks keep the normal firmware-forwarded path.
                     */
                    if (mask == 0x0080U || mask == 0x0800U) {
                        LecWriteU32(pendingResponse, 0);
                        LecWriteU16(pendingResponse + 4, 2);
                        LecWriteU16(pendingResponse + 6, 0);
                        pendingResponseLength = 8;
                        pendingResponseReady = TRUE;
                        pendingResponseIsRawHardware = FALSE;
                        hardwareResponsePending = FALSE;
                        protocolStatus = 0;
                    }
                    else {
                        hwStatus = LecTransportSend(
                            DevExt,
                            record + 6,
                            payloadLength + 2);

                        if (NT_SUCCESS(hwStatus)) {
                            hardwareResponsePending = TRUE;
                            pendingResponseReady = FALSE;
                            pendingResponseLength = 0;
                            protocolStatus = 0;
                        }
                        else {
                            protocolStatus = 8;
                        }
                    }
                }

                LecTrace(
                    "CFDC2110 family0/0x88 mask=0x%04X -> %s protocol=%u hw=0x%08X\n",
                    (ULONG)mask,
                    (mask == 0x0080U || mask == 0x0800U) ?
                        "local" : "firmware",
                    (ULONG)protocolStatus,
                    hwStatus);
            }
            else if (payload[1] == 0 && payload[2] == 0x85) {
                NTSTATUS hwStatus;
                USHORT controlWord;

                if (payloadLength < 6) {
                    hwStatus = STATUS_INVALID_BUFFER_SIZE;
                }
                else {
                    ULONG newMask;

                    controlWord = LecReadU16(payload + 4);

                    /*
                     * FUN_00016962 first stores the complete 16-bit command
                     * enable mask. Standalone 85FB/0x01 later reports this
                     * value together with the DPC-latched pending mask.
                     */
                    {
                        KIRQL oldIrql;

                        KeAcquireSpinLock(
                            &DevExt->LegacyEventLock,
                            &oldIrql);
                        DevExt->LegacyCommandEnableMask =
                            controlWord;
                        KeReleaseSpinLock(
                            &DevExt->LegacyEventLock,
                            oldIrql);
                    }

                    /*
                     * Legacy FUN_00016962 interprets the captured control word
                     * 0x00A0 as:
                     *   bit 7 of low byte -> global INTEN bit 2 (0x04)
                     *   bit 3 of high byte -> global INTEN bit 4 (0x10)
                     *   bit 0 of high byte -> global INTEN bit 5 (0x20)
                     *
                     * Preserve all other mask bits exactly as the legacy
                     * helpers do, then commit the resulting value to BAR0
                     * INTEN before forwarding the command to board firmware.
                     */
                    newMask = DevExt->InterruptEnableShadow;

                    if ((controlWord & 0x0080U) != 0) {
                        newMask |= 0x04UL;
                    }
                    else {
                        newMask &= ~0x04UL;
                    }

                    if ((controlWord & 0x0800U) != 0) {
                        newMask |= 0x10UL;
                    }
                    else {
                        newMask &= ~0x10UL;
                    }

                    if ((controlWord & 0x0100U) != 0) {
                        newMask |= 0x20UL;
                    }
                    else {
                        newMask &= ~0x20UL;
                    }

                    hwStatus =
                        LecCommitLegacyInterruptMask(
                            DevExt,
                            newMask);
                }

                if (NT_SUCCESS(hwStatus)) {
                    hwStatus = LecTransportSend(
                        DevExt,
                        record + 6,
                        payloadLength + 2);
                }

                if (NT_SUCCESS(hwStatus)) {
                    hardwareResponsePending = TRUE;
                    pendingResponseReady = FALSE;
                    pendingResponseLength = 0;
                    protocolStatus = 0;
                }
                else {
                    protocolStatus = 8;
                }
            }
            else if (payload[1] == 1 &&
                     (payload[2] == 0x50 ||
                      payload[2] == 0x51)) {
                PLECS65_TRANSFER transfer = NULL;
                ULONG token = 0;
                USHORT launchUnits = 0;
                NTSTATUS hwStatus = STATUS_INVALID_PARAMETER;

                if (payloadLength != 10) {
                    protocolStatus = 4;
                }
                else {
                    token = LecReadU32(payload + 4);
                    launchUnits = LecReadU16(payload + 8);

                    transfer = LecFindTransferOwned(
                        DevExt,
                        token,
                        PsGetCurrentProcessId());

                    /*
                     * FUN_000160DC accepts the selected transfer only when
                     * (launchUnits << 3) covers at least the registered data
                     * byte count. Runtime uses 0x80 for the 0x400-byte helper
                     * buffer, with one observed 0x180 launch value also
                     * accepted for that same 0x400-byte registration.
                     */
                    if (transfer == NULL ||
                        (((ULONG)launchUnits << 3) <
                         transfer->DataBytes)) {
                        protocolStatus = 4;
                    }
                    else {
                        hwStatus = LecExecuteLegacyMttTransferLocked(
                            DevExt,
                            transfer,
                            launchUnits);

                        protocolStatus =
                            NT_SUCCESS(hwStatus) ? 0 : 8;

                        /*
                         * FUN_000160DC installs an eight-byte local response
                         * after attempting the synchronous transfer:
                         *   DWORD NTSTATUS
                         *   WORD  2
                         *   WORD  protocol status (0 or 8)
                         */
                        LecWriteU32(
                            pendingResponse,
                            (ULONG)hwStatus);
                        LecWriteU16(
                            pendingResponse + 4,
                            2);
                        LecWriteU16(
                            pendingResponse + 6,
                            protocolStatus);
                        pendingResponseLength = 8;
                        pendingResponseReady = TRUE;
                        pendingResponseIsRawHardware = FALSE;
                        hardwareResponsePending = FALSE;
                    }
                }

                LecTrace(
                    "CFDC2110 family1/0x%02X MTT token=%lu launch=%u -> protocol=%u hw=0x%08X\n",
                    (ULONG)payload[2],
                    token,
                    (ULONG)launchUnits,
                    (ULONG)protocolStatus,
                    hwStatus);
            }
            else if (payload[1] == 1 && payload[2] == 0x42) {
                NTSTATUS hwStatus;

                if (payloadLength < 3) {
                    hwStatus = STATUS_INVALID_BUFFER_SIZE;
                }
                else {
                    ULONG requestOffset =
                        inputOffset + 8UL + 3UL;
                    ULONG requestAvailable =
                        InputLength - requestOffset;

                    hwStatus = LecJtagExecute(
                        DevExt,
                        payload + 3,
                        requestAvailable,
                        pendingResponse,
                        max(OutputLength, 8UL),
                        &pendingResponseLength);
                }

                if (NT_SUCCESS(hwStatus)) {
                    pendingResponseReady = TRUE;
                    hardwareResponsePending = FALSE;
                    protocolStatus = 0;
                }
                else if (hwStatus == STATUS_INVALID_PARAMETER ||
                         hwStatus == STATUS_INVALID_BUFFER_SIZE) {
                    /*
                     * Legacy FUN_00015C7E treats JTAG modes >= 2 as a local
                     * protocol error. It does not touch JTAG hardware. For the
                     * captured mode-2 request it installs a response buffer of
                     * requestedDataBytes + 8 bytes with a defined header:
                     *   DWORD 0, WORD length=2, WORD status=4.
                     * The legacy allocator leaves the remaining bytes
                     * uninitialized. Zero them here rather than reproducing a
                     * kernel-pool information leak.
                     */
                    if (payloadLength >= 9 &&
                        payload[3] >= 2) {
                        ULONG requestedDataBytes = LecReadU16(payload + 6);
                        ULONG legacyResponseLength =
                            8UL + requestedDataBytes;

                        if (legacyResponseLength <= max(OutputLength, 8UL)) {
                            RtlZeroMemory(
                                pendingResponse,
                                legacyResponseLength);
                            LecWriteU16(pendingResponse + 4, 2);
                            LecWriteU16(pendingResponse + 6, 4);
                            pendingResponseLength =
                                legacyResponseLength;
                            pendingResponseReady = TRUE;
                            hardwareResponsePending = FALSE;
                        }
                    }

                    protocolStatus = 4;
                }
                else {
                    protocolStatus = 8;
                }
            }

            if (recordOutput >= 6) {
                LecWriteU32(recordResult, 0);
                LecWriteU16(
                    recordResult + 4,
                    protocolStatus);
            }
        }
        else if (signature == 0x85FB) {
            if (payloadLength >= 2 &&
                payload[0] == 0x40 &&
                payload[1] == 0x01) {
                USHORT enabledMask;
                USHORT pendingMask;
                KIRQL oldIrql;

                /*
                 * Legacy FUN_000169B4 subcommand 1 is a local status query.
                 * It returns exactly ten bytes:
                 *
                 *   DWORD 0
                 *   WORD  4
                 *   WORD  enabled command mask (object +0x0B)
                 *   WORD  sticky pending mask  (object +0x09)
                 *
                 * Both independent original runtime captures repeatedly show
                 * 000000000400BF028000 at the acquisition transition.
                 */
                if (recordOutput < 10) {
                    status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                KeAcquireSpinLock(
                    &DevExt->LegacyEventLock,
                    &oldIrql);
                enabledMask = DevExt->LegacyCommandEnableMask;
                pendingMask = DevExt->LegacyCommandPendingMask;
                KeReleaseSpinLock(
                    &DevExt->LegacyEventLock,
                    oldIrql);

                LecWriteU32(recordResult, 0);
                LecWriteU16(recordResult + 4, 4);
                LecWriteU16(recordResult + 6, enabledMask);
                LecWriteU16(recordResult + 8, pendingMask);
            }
            else if (payloadLength < 2 ||
                     payload[0] != 0x40 ||
                     payload[1] != 0) {
                if (recordOutput >= 6) {
                    LecWriteU32(recordResult, 0);
                    LecWriteU16(recordResult + 4, 2);
                }
            }
            else {
                if (!pendingResponseReady &&
                    hardwareResponsePending) {
                    static const UCHAR fetchPacket[4] = {
                        0xFB, 0x85, 0x40, 0x00
                    };
                    ULONG received = 0;

                    status = LecTransportSend(
                        DevExt,
                        fetchPacket,
                        sizeof(fetchPacket));
                    if (!NT_SUCCESS(status)) {
                        break;
                    }

                    status = LecTransportReceive(
                        DevExt,
                        pendingResponse,
                        max(OutputLength, 8UL),
                        &received);
                    if (!NT_SUCCESS(status)) {
                        break;
                    }

                    pendingResponseLength = received;
                    pendingResponseReady = TRUE;
                    pendingResponseIsRawHardware = TRUE;
                    hardwareResponsePending = FALSE;
                }

                if (!pendingResponseReady) {
                    if (max(OutputLength, 8UL) < 8) {
                        status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }

                    RtlZeroMemory(pendingResponse, 8);
                    LecWriteU16(pendingResponse + 4, 2);
                    LecWriteU16(pendingResponse + 6, 0x20);
                    pendingResponseLength = 8;
                    pendingResponseReady = TRUE;
                }

                if (pendingResponseIsRawHardware) {
                    ULONG payloadCapacity;
                    ULONG copyLength;

                    /*
                     * The legacy 85FB fetch path does not expose the raw BAR1
                     * receive buffer directly. It prepends the six-byte host
                     * response header used by type-3 records:
                     *
                     *   DWORD 0
                     *   WORD  2
                     *
                     * and then appends the firmware response bytes.
                     *
                     * Passive original-driver captures prove this framing for
                     * family-1 opcode 0x99 and family-0 opcodes 0x88/0x85.
                     */
                    if (recordOutput < 6) {
                        status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }

                    LecWriteU32(recordResult, 0);
                    LecWriteU16(
                        recordResult + 4,
                        pendingResponseLengthOverride != 0
                            ? pendingResponseLengthOverride
                            : (USHORT)(recordOutput - 6));

                    payloadCapacity = recordOutput - 6;
                    copyLength = min(
                        pendingResponseLength,
                        payloadCapacity);

                    if (copyLength != 0) {
                        RtlCopyMemory(
                            recordResult + 6,
                            pendingResponse,
                            copyLength);
                    }
                }
                else {
                    if (pendingResponseLength > recordOutput) {
                        RtlCopyMemory(
                            recordResult,
                            pendingResponse,
                            recordOutput);
                    }
                    else {
                        RtlCopyMemory(
                            recordResult,
                            pendingResponse,
                            pendingResponseLength);
                    }
                }

                pendingResponseReady = FALSE;
                pendingResponseIsRawHardware = FALSE;
                pendingResponseLengthOverride = 0;
                pendingResponseLength = 0;
            }
        }
        else if (signature == 0xC5FB) {
            /*
             * The third type-3 signature is present in the legacy binary but
             * has not appeared in the captured XStream startup sequence.
             */
            if (recordOutput >= 6) {
                LecWriteU32(recordResult, 0);
                LecWriteU16(recordResult + 4, 2);
            }
        }
        else {
            if (recordOutput >= 6) {
                LecWriteU32(recordResult, 0);
                LecWriteU16(recordResult + 4, 2);
            }
        }

        outputOffset += recordOutput;
        inputOffset += 8 + payloadLength;
    }

    KeReleaseMutex(&DevExt->TransferMutex, FALSE);

    if (NT_SUCCESS(status)) {
        *Information = totalOutput;
    }

Exit:
    if (pendingResponse != NULL) {
        ExFreePool(pendingResponse);
    }
    if (inputCopy != NULL) {
        ExFreePool(inputCopy);
    }

    return status;
}

static
NTSTATUS
LecIoctlAcquireBufferedOneChannel(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_updates_bytes_(OutputLength) UCHAR* SystemBuffer,
    _In_ ULONG InputLength,
    _In_ ULONG OutputLength,
    _Out_ PULONG_PTR Information
    )
{
    ULONG token;
    UCHAR channelCount;
    UCHAR pairMarker;
    UCHAR channel;
    ULONG config;
    ULONG requestedBytes;
    ULONG blockBytes;
    ULONG launchCount;
    ULONG currentMask;
    ULONG cleanupMask;
    ULONG gpioValue;
    ULONG iimStatus;
    ULONG mamValues[5];
    USHORT sequenceValue;
    PLECS65_TRANSFER transfer = NULL;
    volatile ULONG* gpioData;
    volatile ULONG* mamData;
    volatile ULONG* mamPgo;
    volatile ULONG* mamSeq;
    volatile ULONG* mamRgo;
    volatile ULONG* sgta;
    volatile ULONG* iimtc;
    volatile ULONG* iimcl;
    volatile ULONG* iimst;
    volatile ULONG* errs;
    LARGE_INTEGER timeout;
    NTSTATUS status;
    NTSTATUS disableStatus;
    BOOLEAN transferSelected = FALSE;
    BOOLEAN transferInterruptEnabled = FALSE;
    ULONG i;

    if (SystemBuffer == NULL ||
        Information == NULL ||
        InputLength != 15 ||
        OutputLength != sizeof(ULONG)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlCopyMemory(&token, SystemBuffer, sizeof(token));
    channelCount = SystemBuffer[4];
    pairMarker = SystemBuffer[5];
    channel = SystemBuffer[6];
    config = LecReadU32(SystemBuffer + 7);
    requestedBytes = LecReadU32(SystemBuffer + 11);

    /*
     * Stage only the exact CFDC2138 ABI shape seen in both complete legacy
     * runtime traces. All 1902 calls in the 19:56 reference capture use one
     * channel and pair-marker byte 1. The legacy parser consumes the second
     * byte of that pair as the six-bit channel identifier.
     */
    if (channelCount != 1 ||
        pairMarker != 1 ||
        channel > 0x3FU ||
        requestedBytes == 0 ||
        requestedBytes >= 0x01000000UL) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
     * FUN_00013C84 requires the one-channel byte count to be a 0x400-byte
     * multiple once it reaches 0x400 bytes. Smaller transfers are legal.
     */
    if (requestedBytes >= 0x400UL &&
        (requestedBytes & 0x3FFUL) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    KeWaitForSingleObject(
        &DevExt->TransferMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    transfer = LecFindTransferOwned(
        DevExt,
        token,
        PsGetCurrentProcessId());

    if (transfer == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (DevExt->CurrentTransfer != NULL) {
        status = STATUS_DEVICE_BUSY;
        goto Exit;
    }

    if (transfer->DataBytes != requestedBytes) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto Exit;
    }

    if (transfer->TotalDwords != requestedBytes / sizeof(ULONG)) {
        status = STATUS_INTERNAL_ERROR;
        goto Exit;
    }

    /*
     * CFDC2124 builds the board descriptor chain before publishing the token.
     * The x64 builder rejects every source or table physical address above
     * 4 GiB, so a registered transfer reaching this point is representable by
     * the board's legacy 32-bit descriptor ABI.
     */
    status = LecResolveRegister(DevExt, 1, 0x0C4, &gpioData);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 1, 0x040, &mamData);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 1, 0x044, &mamPgo);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 1, 0x060, &mamSeq);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 1, 0x064, &mamRgo);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 0, 0x040, &sgta);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 0, 0x044, &iimtc);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 0, 0x048, &iimcl);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 0, 0x04C, &iimst);
    if (!NT_SUCCESS(status)) goto Exit;
    status = LecResolveRegister(DevExt, 0, 0x004, &errs);
    if (!NT_SUCCESS(status)) goto Exit;

    /* FUN_000120DC clears GPIODAT bit 16 before acquisition setup. */
    gpioValue = READ_REGISTER_ULONG(gpioData);
    WRITE_REGISTER_ULONG(
        gpioData,
        gpioValue & 0xFFFEFFFFUL);

    blockBytes = min(requestedBytes, 0x400UL);

    /*
     * FUN_00017D20/FUN_00017C16 generate five indexed MAMDAT words:
     * channel selector, config low/high and block size low/high.
     */
    mamValues[0] = 0x0E00UL | (ULONG)channel;
    mamValues[1] = config & 0xFFFFUL;
    mamValues[2] = (config >> 16) & 0xFFFFUL;
    mamValues[3] = blockBytes & 0xFFFFUL;
    mamValues[4] = (blockBytes >> 16) & 0xFFFFUL;

    if (!DevExt->LegacyMamShadowInitialized) {
        RtlFillMemory(
            DevExt->LegacyMamShadow,
            sizeof(DevExt->LegacyMamShadow),
            0xFF);
        DevExt->LegacyMamShadowInitialized = TRUE;
    }

    for (i = 0; i < RTL_NUMBER_OF(mamValues); ++i) {
        USHORT data = (USHORT)mamValues[i];

        if (DevExt->LegacyMamShadow[i] != data) {
            DevExt->LegacyMamShadow[i] = data;
            WRITE_REGISTER_ULONG(
                mamData,
                (i << 16) | (ULONG)data);
        }
    }

    WRITE_REGISTER_ULONG(mamPgo, 0x105UL);

    /*
     * FUN_00017EE0 encodes sequence index 0, final-entry bit 6 and channel.
     * MAMSEQ owns a separate indexed shadow in the original driver.
     */
    sequenceValue = (USHORT)(0x40U | channel);

    if (!DevExt->LegacyMamSeqShadowInitialized) {
        RtlFillMemory(
            DevExt->LegacyMamSeqShadow,
            sizeof(DevExt->LegacyMamSeqShadow),
            0xFF);
        DevExt->LegacyMamSeqShadowInitialized = TRUE;
    }

    if (DevExt->LegacyMamSeqShadow[0] != sequenceValue) {
        DevExt->LegacyMamSeqShadow[0] = sequenceValue;
        WRITE_REGISTER_ULONG(mamSeq, (ULONG)sequenceValue);
    }

    /*
     * FUN_00012D6A derives MAMRGO as:
     * requested_bytes / min(requested_bytes, 0x400).
     */
    launchCount = requestedBytes / blockBytes;
    if (launchCount == 0) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    /*
     * FUN_000171DE programs the selected transfer and then waits for INTST
     * bit 0 to signal transfer->CompletionEvent.
     */
    WRITE_REGISTER_ULONG(
        sgta,
        transfer->DescriptorTablePhysical);
    WRITE_REGISTER_ULONG(
        iimtc,
        transfer->TotalDwords);

    KeResetEvent(&transfer->CompletionEvent);
    (VOID)InterlockedAnd(
        (volatile LONG*)&DevExt->InterruptPendingShadow,
        ~1L);

    (VOID)InterlockedExchangePointer(
        (PVOID volatile*)&DevExt->CurrentTransfer,
        transfer);
    transferSelected = TRUE;

    currentMask = (ULONG)InterlockedCompareExchange(
        (volatile LONG*)&DevExt->InterruptEnableShadow,
        0,
        0);

    status = LecCommitLegacyInterruptMask(
        DevExt,
        currentMask | 0x01UL);
    if (!NT_SUCCESS(status)) {
        goto CleanupTransfer;
    }
    transferInterruptEnabled = TRUE;

    WRITE_REGISTER_ULONG(iimcl, 1UL);
    WRITE_REGISTER_ULONG(mamRgo, launchCount);

    timeout.QuadPart = -50000000LL;
    status = KeWaitForSingleObject(
        &transfer->CompletionEvent,
        Executive,
        KernelMode,
        FALSE,
        &timeout);

    if (status == STATUS_TIMEOUT) {
        status = STATUS_IO_TIMEOUT;
    }

CleanupTransfer:
    if (transferInterruptEnabled) {
        cleanupMask = (ULONG)InterlockedCompareExchange(
            (volatile LONG*)&DevExt->InterruptEnableShadow,
            0,
            0);

        disableStatus = LecCommitLegacyInterruptMask(
            DevExt,
            cleanupMask & ~0x01UL);

        if (NT_SUCCESS(status) &&
            !NT_SUCCESS(disableStatus)) {
            status = disableStatus;
        }
    }

    if (transferSelected) {
        (VOID)InterlockedExchangePointer(
            (PVOID volatile*)&DevExt->CurrentTransfer,
            NULL);
    }

    /*
     * FUN_00012D6A performs this IIM status cleanup after the synchronous
     * transfer and then reads ERRS.
     */
    iimStatus = READ_REGISTER_ULONG(iimst);
    if ((iimStatus & 0x01UL) != 0) {
        WRITE_REGISTER_ULONG(iimcl, 0UL);
    }
    (VOID)READ_REGISTER_ULONG(errs);

    /*
     * Once legacy validation reaches FUN_00012D6A, the handler reports the
     * requested byte count through its four-byte output slot even when the
     * synchronous hardware operation itself later fails.
     */
    LecWriteU32(SystemBuffer, requestedBytes);
    *Information = sizeof(ULONG);

    LecTrace(
        "CFDC2138 one-channel DMA: token=%lu ch=%u config=0x%08lX bytes=%lu SGTA=0x%08lX IIMTC=%lu MAMRGO=%lu status=0x%08X\n",
        token,
        (ULONG)channel,
        config,
        requestedBytes,
        transfer->DescriptorTablePhysical,
        transfer->TotalDwords,
        launchCount,
        status);

Exit:
    KeReleaseMutex(&DevExt->TransferMutex, FALSE);
    return status;
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
    UCHAR outputPreview[LECS65_TRACE_OUTPUT_PREVIEW_BYTES];
    ULONG inputPreviewLength = 0;
    ULONG outputPreviewLength = 0;
    PVOID type3InputBuffer =
        stack->Parameters.DeviceIoControl.Type3InputBuffer;

    RtlZeroMemory(inputPreview, sizeof(inputPreview));
    RtlZeroMemory(outputPreview, sizeof(outputPreview));

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

        LecTrace("IOCTL input preview:\n");
        LecHexDump((const UCHAR*)systemBuffer, inputLength);
    }
    else if (method == METHOD_NEITHER &&
             type3InputBuffer != NULL &&
             inputLength != 0 &&
             Irp->RequestorMode == UserMode) {
        __try {
            inputPreviewLength = min(
                inputLength,
                (ULONG)sizeof(inputPreview));
            ProbeForRead(
                type3InputBuffer,
                inputPreviewLength,
                sizeof(UCHAR));
            RtlCopyMemory(
                inputPreview,
                type3InputBuffer,
                inputPreviewLength);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            inputPreviewLength = 0;
            LecTrace(
                "METHOD_NEITHER trace capture failed: 0x%08X\n",
                GetExceptionCode());
        }
    }

    switch (code) {
    case LECS65_IOCTL_00222400:
        /*
         * Live original-driver capture proves that XStream probes this
         * control during startup and the legacy driver returns success with
         * Information=0. The 4-byte output buffer is left untouched.
         */
        status = STATUS_SUCCESS;
        information = 0;
        LecTrace("legacy 0x00222400 -> STATUS_SUCCESS, info=0\n");
        break;

    case LECS65_IOCTL_CFDC2110:
        /*
         * Confirmed pure board-forwarding command classes are admitted after
         * structural CFDC2110 framing validation. Commands with host-side
         * semantics remain byte-exact until their behavior is decoded.
         */
        if (systemBuffer != NULL &&
            LecIsAllowedCfDc2110(
                (const UCHAR*)systemBuffer,
                inputLength)) {
            status = LecIoctlCfDc2110(
                devExt,
                (UCHAR*)systemBuffer,
                inputLength,
                outputLength,
                &information);
            LecTrace(
                "CFDC2110 validated gate -> 0x%08X info=%Iu\n",
                status,
                information);
        }
        else {
            status = STATUS_INVALID_DEVICE_REQUEST;
            information = 0;
            LecTrace(
                "CFDC2110 rejected outside validated gate: in=%lu out=%lu\n",
                inputLength,
                outputLength);
        }
        break;

    case LECS65_IOCTL_DELAY_MILLISECONDS:
        if (systemBuffer == NULL ||
            inputLength < sizeof(ULONG)) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }
        else {
            ULONG milliseconds = *(PULONG)systemBuffer;
            LARGE_INTEGER interval;

            /*
             * Original handler accepts >=4 bytes and optionally consumes a
             * fifth control byte.  Its externally visible behaviour includes
             * a relative millisecond delay.  Reproduce that timing here while
             * deliberately omitting the old auxiliary hardware toggle until
             * its necessity is proven.
             */
            if (milliseconds != 0) {
                interval.QuadPart = -((LONGLONG)milliseconds * 10000LL);
                status = KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &interval);
            }
            else {
                status = STATUS_SUCCESS;
            }

            information = 0;
            LecTrace(
                "legacy 0x00222C00 delay=%lu ms optional=%u -> 0x%08X\n",
                milliseconds,
                inputLength == 5 ? (ULONG)((PUCHAR)systemBuffer)[4] : 0UL,
                status);
        }
        break;

    case LECS65_IOCTL_SET_FLAG_BYTE:
        if (systemBuffer == NULL ||
            inputLength != sizeof(UCHAR) ||
            outputLength != 0) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }

        devExt->LegacyFlagByte = *(PUCHAR)systemBuffer;
        status = STATUS_SUCCESS;
        information = 0;
        LecTrace(
            "legacy 0x00222C04 flag <- %u\n",
            (ULONG)devExt->LegacyFlagByte);
        break;

    case LECS65_IOCTL_SET_TRACE_CONTROL:
        if (systemBuffer == NULL ||
            inputLength != 0x108 ||
            outputLength != 0) {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
            break;
        }
        else {
            ULONG level;
            ULONG index;

            RtlCopyMemory(
                &level,
                (PUCHAR)systemBuffer + 0x100,
                sizeof(level));
            RtlCopyMemory(
                &index,
                (PUCHAR)systemBuffer + 0x104,
                sizeof(index));

            /*
             * Legacy FUN_00012ADA passes the final two DWORDs to
             * FUN_00012290(traceControl, index, level). This only changes
             * software trace verbosity and has no board-side effect.
             */
            status = STATUS_SUCCESS;
            information = 0;
            LecTrace(
                "legacy 0x00223000 trace-control index=%lu level=%lu\n",
                index,
                level);
        }
        break;

    case LECS65_IOCTL_QUERY_BUFFER_A:
        if (systemBuffer == NULL) {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
            break;
        }

        if (outputLength == sizeof(ULONG)) {
            *(PULONG)systemBuffer = LECS65_LEGACY_TRACE_BLOCK_BYTES;
            information = sizeof(ULONG);
            status = STATUS_SUCCESS;
            LecTrace(
                "legacy 0x00223004 trace-block size -> 0x%08lX\n",
                *(PULONG)systemBuffer);
        }
        else if (outputLength == LECS65_LEGACY_TRACE_BLOCK_BYTES) {
            status = LecFillLegacyTraceBlock(
                (PUCHAR)systemBuffer,
                outputLength);
            information = NT_SUCCESS(status) ? outputLength : 0;
            LecTrace(
                "legacy 0x00223004 trace-block payload -> 0x%08X info=%Iu\n",
                status,
                information);
        }
        else {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
        }
        break;

    case LECS65_IOCTL_QUERY_BUFFER_B:
        if (systemBuffer == NULL) {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
            break;
        }

        if (outputLength == sizeof(ULONG)) {
            *(PULONG)systemBuffer = LECS65_LEGACY_REGISTER_LIST_BYTES;
            information = sizeof(ULONG);
            status = STATUS_SUCCESS;
            LecTrace(
                "legacy 0x00223040 register-list size -> 0x%08lX\n",
                *(PULONG)systemBuffer);
        }
        else if (inputLength == 0 &&
                 outputLength == LECS65_LEGACY_REGISTER_LIST_BYTES) {
            status = LecFillLegacyRegisterList(
                devExt,
                (PUCHAR)systemBuffer,
                outputLength);
            information = NT_SUCCESS(status) ? outputLength : 0;
            LecTrace(
                "legacy 0x00223040 register-list payload -> 0x%08X info=%Iu\n",
                status,
                information);
        }
        else if (inputLength == sizeof(ULONG) &&
                 outputLength == LECS65_LEGACY_REGISTER_ENTRY_BYTES) {
            ULONG index = *(PULONG)systemBuffer;

            status = LecFillLegacyRegisterEntry(
                devExt,
                index,
                (PUCHAR)systemBuffer);
            information = NT_SUCCESS(status) ?
                LECS65_LEGACY_REGISTER_ENTRY_BYTES : 0;

            LecTrace(
                "legacy 0x00223040 register entry index=%lu -> 0x%08X info=%Iu\n",
                index,
                status,
                information);
        }
        else {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
        }
        break;

    case LECS65_IOCTL_SET_EVENT_0:
        status = LecIoctlSetSingleEvent(
            devExt,
            &devExt->LegacyEvent0,
            systemBuffer,
            inputLength,
            outputLength,
            Irp->RequestorMode);
        information = 0;
        break;

    case LECS65_IOCTL_SET_EVENT_1:
        status = LecIoctlSetSingleEvent(
            devExt,
            &devExt->LegacyEvent1,
            systemBuffer,
            inputLength,
            outputLength,
            Irp->RequestorMode);
        information = 0;
        break;

    case LECS65_IOCTL_CFDC2190:
        /*
         * Legacy 0xCFDC2190 accepts exactly 29 input bytes and no output.
         * Static analysis of FUN_00013A40 plus the original runtime trace
         * establish the startup-relevant fields:
         *
         *   +0x04 DWORD: controls global interrupt-mask bit 1
         *   +0x08 DWORD: value written to BAR0 ERRM (offset 0x008)
         *
         * The remaining bytes belong to the paired 0xCFDC2194
         * status/readback structure and are preserved as ABI padding here.
         */
        if (systemBuffer == NULL ||
            inputLength != 29 ||
            outputLength != 0) {
            status = STATUS_INVALID_BUFFER_SIZE;
            information = 0;
            break;
        }
        else {
            ULONG control;
            ULONG errorMask;
            ULONG newInterruptMask;
            volatile ULONG* errm;

            RtlCopyMemory(
                &control,
                (PUCHAR)systemBuffer + 4,
                sizeof(control));
            RtlCopyMemory(
                &errorMask,
                (PUCHAR)systemBuffer + 8,
                sizeof(errorMask));

            status = LecResolveRegister(
                devExt,
                0,
                0x008,
                &errm);

            if (NT_SUCCESS(status)) {
                WRITE_REGISTER_ULONG(errm, errorMask);

                newInterruptMask =
                    (ULONG)InterlockedCompareExchange(
                        (volatile LONG*)&devExt->InterruptEnableShadow,
                        0,
                        0);

                if (control != 0) {
                    newInterruptMask |= 0x02UL;
                }
                else {
                    newInterruptMask &= ~0x02UL;
                }

                status = LecCommitLegacyInterruptMask(
                    devExt,
                    newInterruptMask);
            }

            information = 0;
            LecTrace(
                "CFDC2190 control=0x%08lX ERRM=0x%08lX -> 0x%08X\n",
                control,
                errorMask,
                status);
        }
        break;

    case LECS65_IOCTL_SET_THREE_EVENTS:
        status = LecIoctlSetThreeEvents(
            devExt,
            systemBuffer,
            inputLength,
            outputLength,
            Irp->RequestorMode);
        information = 0;
        break;

    case LECS65_IOCTL_GET_DALLAS_ID:
        if (systemBuffer == NULL || outputLength != 8) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }

        status = LecDallasReadId(
            devExt,
            (PUCHAR)systemBuffer);

        if (NT_SUCCESS(status)) {
            information = 8;
            LecTrace(
                "GET_DALLAS_ID -> %02X %02X %02X %02X %02X %02X %02X %02X\n",
                ((PUCHAR)systemBuffer)[0],
                ((PUCHAR)systemBuffer)[1],
                ((PUCHAR)systemBuffer)[2],
                ((PUCHAR)systemBuffer)[3],
                ((PUCHAR)systemBuffer)[4],
                ((PUCHAR)systemBuffer)[5],
                ((PUCHAR)systemBuffer)[6],
                ((PUCHAR)systemBuffer)[7]);
        }
        break;

    case LECS65_IOCTL_READ_DALLAS_MEMORY:
        if (systemBuffer == NULL ||
            outputLength == 0 ||
            outputLength > 0x200) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }

        status = LecDallasReadMemory(
            devExt,
            (PUCHAR)systemBuffer,
            outputLength);

        if (NT_SUCCESS(status)) {
            information = outputLength;
            LecTrace(
                "READ_DALLAS_MEMORY -> %lu bytes\n",
                outputLength);
        }
        break;

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

    case LECS65_IOCTL_REGISTER_TRANSFER:
        if (systemBuffer == NULL ||
            inputLength != 12 ||
            outputLength < sizeof(ULONG)) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }
        else {
            ULONG request[3];
            ULONG token = 0;

            RtlCopyMemory(request, systemBuffer, sizeof(request));

            status = LecRegisterTransfer(
                devExt,
                ULongToPtr(request[0]),
                request[1],
                Irp->RequestorMode,
                &token);

            if (NT_SUCCESS(status)) {
                *(PULONG)systemBuffer = token;
                information = sizeof(ULONG);
            }

            LecTrace(
                "CFDC2124 register: user32=0x%08lX bytes=%lu reserved=0x%08lX -> token=%lu status=0x%08X\n",
                request[0],
                request[1],
                request[2],
                token,
                status);
        }
        break;

    case LECS65_IOCTL_UNREGISTER_TRANSFER:
        if (systemBuffer == NULL ||
            inputLength != sizeof(ULONG)) {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }

        status = LecUnregisterTransfer(
            devExt,
            *(PULONG)systemBuffer,
            PsGetCurrentProcessId());
        information = 0;
        break;

    case LECS65_IOCTL_ACQUIRE_BUFFERED:
        status = LecIoctlAcquireBufferedOneChannel(
            devExt,
            (UCHAR*)systemBuffer,
            inputLength,
            outputLength,
            &information);
        break;

    case LECS65_IOCTL_ACQUIRE_NEITHER:
        /*
         * Type3InputBuffer is captured above for offline analysis. Do not
         * launch DMA yet; the x64 implementation must preserve the packed ABI
         * while fixing the legacy probing bugs.
         */
        status = STATUS_NOT_SUPPORTED;
        information = 0;
        LecTrace(
            "CFDD219F METHOD_NEITHER acquisition gated: trace captured\n");
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

    case LECS65_IOCTL_DEBUG_GET_PCI_CONFIG:
        if (systemBuffer == NULL ||
            outputLength < sizeof(LECS65_DEBUG_PCI_CONFIG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = LecReadPciConfig(
            devExt,
            (PLECS65_DEBUG_PCI_CONFIG)systemBuffer);
        if (NT_SUCCESS(status)) {
            information = sizeof(LECS65_DEBUG_PCI_CONFIG);
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

    if (method == METHOD_BUFFERED &&
        NT_SUCCESS(status) &&
        information != 0 &&
        systemBuffer != NULL) {
        outputPreviewLength = (ULONG)min(
            information,
            (ULONG_PTR)sizeof(outputPreview));
        RtlCopyMemory(
            outputPreview,
            systemBuffer,
            outputPreviewLength);
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
        type3InputBuffer,
        Irp->UserBuffer,
        inputPreviewLength != 0 ? inputPreview : NULL,
        inputPreviewLength,
        outputPreviewLength != 0 ? outputPreview : NULL,
        outputPreviewLength);

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
