#include "DmaV3Transfer.h"

static BOOLEAN LecDmaV3OperationsAvailable(_In_ PDMA_ADAPTER Adapter)
{
    return (BOOLEAN)(Adapter != NULL && Adapter->DmaOperations != NULL &&
        Adapter->DmaOperations->Size >= RTL_SIZEOF_THROUGH_FIELD(
            DMA_OPERATIONS, FlushAdapterBuffersEx) &&
        Adapter->DmaOperations->GetDmaTransferInfo != NULL &&
        Adapter->DmaOperations->InitializeDmaTransferContext != NULL &&
        Adapter->DmaOperations->AllocateAdapterChannelEx != NULL &&
        Adapter->DmaOperations->MapTransferEx != NULL &&
        Adapter->DmaOperations->FlushAdapterBuffersEx != NULL &&
        Adapter->DmaOperations->FreeAdapterObject != NULL);
}

static NTSTATUS LecDmaV3ValidateList(
    _In_ PSCATTER_GATHER_LIST List,
    _In_ ULONG BufferBytes,
    _In_ ULONG ExpectedBytes)
{
    ULONG i;
    ULONGLONG total = 0;
    ULONGLONG required;
    if (List == NULL || List->NumberOfElements == 0) return STATUS_INVALID_BUFFER_SIZE;
    required = FIELD_OFFSET(SCATTER_GATHER_LIST, Elements) +
        (ULONGLONG)List->NumberOfElements * sizeof(SCATTER_GATHER_ELEMENT);
    if (required > BufferBytes) return STATUS_INVALID_BUFFER_SIZE;
    for (i = 0; i < List->NumberOfElements; ++i) {
        ULONGLONG address = (ULONGLONG)List->Elements[i].Address.QuadPart;
        ULONG length = List->Elements[i].Length;
        if (length == 0 || (length & 3U) != 0 || (address & 3U) != 0 ||
            address > MAXULONG || (ULONGLONG)length - 1U > MAXULONG - address)
            return STATUS_INVALID_BUFFER_SIZE;
        total += length;
        if (total > ExpectedBytes) return STATUS_INVALID_BUFFER_SIZE;
    }
    return total == ExpectedBytes ? STATUS_SUCCESS : STATUS_INVALID_BUFFER_SIZE;
}

VOID LecDmaV3TransferInitialize(_Out_ PLECS65_DMA_V3_TRANSFER Transfer)
{
    if (Transfer != NULL) {
        RtlZeroMemory(Transfer, sizeof(*Transfer));
        Transfer->State = LecDmaV3MapEmpty;
    }
}

static NTSTATUS LecDmaV3FreeNeverLaunched(
    _Inout_ PLECS65_DMA_V3_TRANSFER Transfer,
    _In_ BOOLEAN FlushMapping)
{
    NTSTATUS ownerStatus = STATUS_SUCCESS;
    if (FlushMapping) {
        NTSTATUS flushStatus = Transfer->AdapterContext->Adapter->DmaOperations->FlushAdapterBuffersEx(
            Transfer->AdapterContext->Adapter, Transfer->MdlChain,
            Transfer->MapRegisterBase, 0, Transfer->MappedLength, FALSE);
        if (!NT_SUCCESS(flushStatus)) {
            Transfer->QuarantineLatched = TRUE;
            Transfer->State = LecDmaV3MapQuarantined;
            LecDmaQuarantineAdapterContext(Transfer->AdapterContext);
            return flushStatus;
        }
    }
    if (Transfer->ChannelAllocated) {
        Transfer->AdapterContext->Adapter->DmaOperations->FreeAdapterObject(
            Transfer->AdapterContext->Adapter, DeallocateObject);
    }
    if (Transfer->ScatterGather != NULL) {
        ExFreePoolWithTag(Transfer->ScatterGather, LECS65_TAG);
    }
    Transfer->ScatterGather = NULL;
    Transfer->MapRegisterBase = NULL;
    Transfer->ChannelAllocated = FALSE;
    Transfer->MappingOwned = FALSE;
    if (Transfer->AdapterOwnerClaimed) {
        ownerStatus = LecDmaReleaseSynchronousOwner(
            Transfer->AdapterContext, Transfer);
        if (NT_SUCCESS(ownerStatus)) {
            Transfer->AdapterOwnerClaimed = FALSE;
        } else {
            Transfer->QuarantineLatched = TRUE;
            Transfer->State = LecDmaV3MapQuarantined;
            LecDmaQuarantineAdapterContext(Transfer->AdapterContext);
        }
    }
    return ownerStatus;
}

NTSTATUS LecDmaV3PrepareNoLaunch(
    _Inout_ PLECS65_DMA_V3_TRANSFER Transfer,
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT AdapterContext,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length)
{
    DMA_TRANSFER_INFO info;
    KIRQL oldIrql;
    ULONG mappedLength;
    NTSTATUS status;
    if (Transfer == NULL || AdapterContext == NULL || LockedMdlChain == NULL ||
        Length == 0 || Transfer->State != LecDmaV3MapEmpty ||
        AdapterContext->Quarantined || !LecDmaV3OperationsAvailable(AdapterContext->Adapter))
        return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(&info, sizeof(info));
    info.Version = DMA_TRANSFER_INFO_VERSION1;
    status = AdapterContext->Adapter->DmaOperations->GetDmaTransferInfo(
        AdapterContext->Adapter, LockedMdlChain, 0, Length, FALSE, &info);
    if (!NT_SUCCESS(status)) return status;
    if (info.V1.MapRegisterCount == 0 ||
        info.V1.MapRegisterCount > AdapterContext->NumberOfMapRegisters ||
        info.V1.ScatterGatherElementCount == 0 ||
        info.V1.ScatterGatherListSize < FIELD_OFFSET(SCATTER_GATHER_LIST, Elements) +
            sizeof(SCATTER_GATHER_ELEMENT)) return STATUS_INVALID_BUFFER_SIZE;

    status = LecDmaClaimSynchronousOwner(
        AdapterContext, Transfer, &Transfer->DescriptorSlotCapacity);
    if (!NT_SUCCESS(status)) return status;
    Transfer->AdapterOwnerClaimed = TRUE;

    Transfer->State = LecDmaV3MapPreparing;
    Transfer->AdapterContext = AdapterContext;
    Transfer->MdlChain = LockedMdlChain;
    Transfer->RequestedLength = Length;
    Transfer->ScatterGatherBytes = info.V1.ScatterGatherListSize;
    Transfer->MapRegisterCount = info.V1.MapRegisterCount;
    Transfer->ScatterGather = (PSCATTER_GATHER_LIST)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, Transfer->ScatterGatherBytes, LECS65_TAG);
    if (Transfer->ScatterGather == NULL) {
        status = LecDmaV3FreeNeverLaunched(Transfer, FALSE);
        if (!NT_SUCCESS(status)) return status;
        Transfer->State = LecDmaV3MapEmpty;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(Transfer->ScatterGather, Transfer->ScatterGatherBytes);

    status = AdapterContext->Adapter->DmaOperations->InitializeDmaTransferContext(
        AdapterContext->Adapter, Transfer->TransferContext);
    if (!NT_SUCCESS(status)) { (void)LecDmaV3FreeNeverLaunched(Transfer, FALSE); Transfer->State = LecDmaV3MapEmpty; return status; }

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    status = AdapterContext->Adapter->DmaOperations->AllocateAdapterChannelEx(
        AdapterContext->Adapter, AdapterContext->PhysicalDeviceObject,
        Transfer->TransferContext, Transfer->MapRegisterCount,
        DMA_SYNCHRONOUS_CALLBACK, NULL, NULL, &Transfer->MapRegisterBase);
    KeLowerIrql(oldIrql);
    if (!NT_SUCCESS(status) || Transfer->MapRegisterBase == NULL) {
        (void)LecDmaV3FreeNeverLaunched(Transfer, FALSE);
        Transfer->State = LecDmaV3MapEmpty;
        return NT_SUCCESS(status) ? STATUS_INVALID_DEVICE_STATE : status;
    }
    Transfer->ChannelAllocated = TRUE;
    mappedLength = Length;
    status = AdapterContext->Adapter->DmaOperations->MapTransferEx(
        AdapterContext->Adapter, LockedMdlChain, Transfer->MapRegisterBase,
        0, 0, &mappedLength, FALSE, Transfer->ScatterGather,
        Transfer->ScatterGatherBytes, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        (void)LecDmaV3FreeNeverLaunched(Transfer, FALSE);
        Transfer->State = LecDmaV3MapEmpty;
        return status;
    }
    Transfer->MappingOwned = TRUE;
    Transfer->MappedLength = mappedLength;
    if (mappedLength != Length) {
        status = LecDmaV3FreeNeverLaunched(Transfer, TRUE);
        if (!NT_SUCCESS(status)) return status;
        Transfer->State = LecDmaV3MapEmpty;
        return STATUS_INVALID_BUFFER_SIZE;
    }
    status = LecDmaV3ValidateList(
        Transfer->ScatterGather, Transfer->ScatterGatherBytes, Length);
    if (!NT_SUCCESS(status)) {
        NTSTATUS cleanupStatus = LecDmaV3FreeNeverLaunched(Transfer, TRUE);
        if (!NT_SUCCESS(cleanupStatus)) return cleanupStatus;
        Transfer->State = LecDmaV3MapEmpty;
        return status;
    }
    Transfer->State = LecDmaV3MapReady;
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaV3ReleaseNeverLaunched(_Inout_ PLECS65_DMA_V3_TRANSFER Transfer)
{
    NTSTATUS status;
    if (Transfer == NULL || Transfer->State != LecDmaV3MapReady ||
        !Transfer->MappingOwned || Transfer->QuarantineLatched)
        return STATUS_INVALID_DEVICE_STATE;
    Transfer->State = LecDmaV3MapReleaseCommitted;
    status = Transfer->AdapterContext->Adapter->DmaOperations->FlushAdapterBuffersEx(
        Transfer->AdapterContext->Adapter, Transfer->MdlChain,
        Transfer->MapRegisterBase, 0, Transfer->MappedLength, FALSE);
    if (!NT_SUCCESS(status)) {
        Transfer->QuarantineLatched = TRUE;
        Transfer->State = LecDmaV3MapQuarantined;
        LecDmaQuarantineAdapterContext(Transfer->AdapterContext);
        return status;
    }
    Transfer->MappingOwned = FALSE;
    Transfer->AdapterContext->Adapter->DmaOperations->FreeAdapterObject(
        Transfer->AdapterContext->Adapter, DeallocateObject);
    Transfer->ChannelAllocated = FALSE;
    ExFreePoolWithTag(Transfer->ScatterGather, LECS65_TAG);
    Transfer->ScatterGather = NULL;
    Transfer->MapRegisterBase = NULL;
    status = LecDmaReleaseSynchronousOwner(
        Transfer->AdapterContext, Transfer);
    if (!NT_SUCCESS(status)) {
        Transfer->QuarantineLatched = TRUE;
        Transfer->State = LecDmaV3MapQuarantined;
        LecDmaQuarantineAdapterContext(Transfer->AdapterContext);
        return status;
    }
    Transfer->AdapterOwnerClaimed = FALSE;
    Transfer->State = LecDmaV3MapReleased;
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaV3BuildCommonDescriptorsNoLaunch(
    _In_ PLECS65_DMA_V3_TRANSFER Transfer,
    _Out_ PULONG UsedEntries)
{
    LECS65_DMA_MAPPED_SEGMENT* segments;
    LECS65_DMA_LAYOUT_RESULT result;
    ULONG i;
    uint32_t encodedUsed;
    SIZE_T segmentBytes;

    if (Transfer == NULL || UsedEntries == NULL ||
        Transfer->State != LecDmaV3MapReady || !Transfer->MappingOwned ||
        Transfer->AdapterContext == NULL ||
        Transfer->AdapterContext->TableVirtual == NULL ||
        Transfer->AdapterContext->TableLength == 0 ||
        Transfer->AdapterContext->TableLogical.QuadPart < 0 ||
        (ULONGLONG)Transfer->AdapterContext->TableLogical.QuadPart > MAXULONG ||
        Transfer->ScatterGather == NULL ||
        Transfer->ScatterGather->NumberOfElements == 0)
        return STATUS_INVALID_DEVICE_STATE;

    *UsedEntries = 0;
    if ((SIZE_T)Transfer->ScatterGather->NumberOfElements >
        MAXULONG_PTR / sizeof(*segments))
        return STATUS_INTEGER_OVERFLOW;
    segmentBytes = (SIZE_T)Transfer->ScatterGather->NumberOfElements *
        sizeof(*segments);
    segments = (LECS65_DMA_MAPPED_SEGMENT*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, segmentBytes, LECS65_TAG);
    if (segments == NULL) return STATUS_INSUFFICIENT_RESOURCES;

    for (i = 0; i < Transfer->ScatterGather->NumberOfElements; ++i) {
        segments[i].DeviceAddress =
            (uint64_t)Transfer->ScatterGather->Elements[i].Address.QuadPart;
        segments[i].LengthBytes = Transfer->ScatterGather->Elements[i].Length;
    }
    RtlZeroMemory(Transfer->AdapterContext->TableVirtual,
        Transfer->AdapterContext->TableLength);
    result = LecDmaEncodeMappedSegments(
        (LECS65_DMA_LAYOUT_ENTRY*)Transfer->AdapterContext->TableVirtual,
        Transfer->AdapterContext->TableLength,
        (uint64_t)Transfer->AdapterContext->TableLogical.QuadPart,
        segments, Transfer->ScatterGather->NumberOfElements,
        Transfer->MappedLength, &encodedUsed);
    ExFreePoolWithTag(segments, LECS65_TAG);
    if (result != LecDmaLayoutOk) return STATUS_INVALID_BUFFER_SIZE;
    *UsedEntries = (ULONG)encodedUsed;
    return STATUS_SUCCESS;
}

VOID LecDmaV3Quarantine(_Inout_ PLECS65_DMA_V3_TRANSFER Transfer)
{
    if (Transfer != NULL) {
        Transfer->QuarantineLatched = TRUE;
        Transfer->State = LecDmaV3MapQuarantined;
        if (Transfer->AdapterContext != NULL)
            LecDmaQuarantineAdapterContext(Transfer->AdapterContext);
    }
}
