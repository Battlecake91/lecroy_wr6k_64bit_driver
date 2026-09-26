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
     * XStream polls the generic register-read IOCTL hundreds of times during
     * startup. Those successful reads contain no useful protocol payload in
     * the current trace format and would evict the interesting control calls.
     */
    if (Code == LECS65_IOCTL_REGISTER_READ &&
        NT_SUCCESS(Status)) {
        return;
    }

    KeAcquireSpinLock(&DevExt->TraceLock, &oldIrql);

    index = DevExt->TraceWriteIndex;
    entry = &DevExt->Trace[index];
    RtlZeroMemory(entry, sizeof(*entry));

    entry->Sequence = ++DevExt->TraceNextSequence;
    entry->Timestamp100ns = KeQueryInterruptTime();
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
    Trace->Version = 2;

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
    _Inout_ PKEVENT* Slot,
    _In_ ULONG HandleValue,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PKEVENT eventObject = NULL;
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

    if (*Slot != NULL) {
        ObDereferenceObject(*Slot);
    }

    *Slot = eventObject;
    KeClearEvent(eventObject);
    return STATUS_SUCCESS;
}

static
NTSTATUS
LecIoctlSetSingleEvent(
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

    if (DevExt->LegacyEvent2 != NULL) {
        ObDereferenceObject(DevExt->LegacyEvent2);
    }
    if (DevExt->LegacyEvent3 != NULL) {
        ObDereferenceObject(DevExt->LegacyEvent3);
    }
    if (DevExt->LegacyEvent4 != NULL) {
        ObDereferenceObject(DevExt->LegacyEvent4);
    }

    DevExt->LegacyEvent2 = event2;
    DevExt->LegacyEvent3 = event3;
    DevExt->LegacyEvent4 = event4;
    event2 = NULL;
    event3 = NULL;
    event4 = NULL;

    KeClearEvent(DevExt->LegacyEvent2);
    KeClearEvent(DevExt->LegacyEvent3);
    KeClearEvent(DevExt->LegacyEvent4);

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

    while (remainingBits != 0) {
        ULONG thisBits = min(remainingBits, 16UL);
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
            (thisBits & 0x0FUL));

        dataValue =
            ((ULONG)firstWord << 16) |
            (ULONG)secondWord;
        WRITE_REGISTER_ULONG(jtagData, dataValue);

        inputValue = READ_REGISTER_ULONG(jtagIn);

        if (thisBits == 16) {
            outputWord = (USHORT)(inputValue >> 16);
        }
        else {
            outputWord = (USHORT)(
                inputValue >> (32 - thisBits));
        }

        LecWriteU16(Response + outputOffset, outputWord);

        inputOffset += 4;
        outputOffset += 2;
        produced += 2;
        remainingBits -= thisBits;
    }

    LecWriteU32(Response, 0);
    LecWriteU16(Response + 4, (USHORT)(produced + 2));
    LecWriteU16(Response + 6, 0);

    *ResponseLength = 8 + produced;
    return STATUS_SUCCESS;
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
            else if (payload[1] == 1 && payload[2] == 0x99) {
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
                NTSTATUS hwStatus;

                if (payloadLength >= 6) {
                    USHORT mask = LecReadU16(payload + 4);
                    DevExt->LegacyTransferMask &=
                        (USHORT)~mask;
                }

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
            else if (payload[1] == 1 && payload[2] == 0x42) {
                NTSTATUS hwStatus;

                if (payloadLength < 3) {
                    hwStatus = STATUS_INVALID_BUFFER_SIZE;
                }
                else {
                    hwStatus = LecJtagExecute(
                        DevExt,
                        payload + 3,
                        payloadLength - 3,
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
            if (payloadLength < 2 ||
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

                pendingResponseReady = FALSE;
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
    case LECS65_IOCTL_CFDC2110:
        /*
         * Runtime testing showed that the current partial CFDC2110
         * implementation accepts many still-unknown A5FB subcommands and
         * returns structurally valid but semantically bogus response buffers.
         * XStream then proceeds with invalid board state and reports multiple
         * acquisition/UI errors. Keep the recovered parser/transport code in
         * tree for continued reverse engineering, but do not execute hardware
         * side effects until every startup subcommand used by XStream has a
         * confirmed implementation.
         */
        status = STATUS_INVALID_DEVICE_REQUEST;
        information = 0;
        LecTrace(
            "CFDC2110 temporarily trace-only: in=%lu out=%lu\n",
            inputLength,
            outputLength);
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

    case LECS65_IOCTL_SET_EVENT_0:
        status = LecIoctlSetSingleEvent(
            &devExt->LegacyEvent0,
            systemBuffer,
            inputLength,
            outputLength,
            Irp->RequestorMode);
        information = 0;
        break;

    case LECS65_IOCTL_SET_EVENT_1:
        status = LecIoctlSetSingleEvent(
            &devExt->LegacyEvent1,
            systemBuffer,
            inputLength,
            outputLength,
            Irp->RequestorMode);
        information = 0;
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
        /*
         * The host-side ABI and MAM descriptor programming are decoded, but
         * active acquisition stays gated until x64 DMA address-width and
         * launch-count units are verified on the reference hardware.
         */
        status = STATUS_NOT_SUPPORTED;
        information = 0;
        LecTrace(
            "CFDC2138 acquisition gated: trace captured, hardware launch disabled\n");
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
