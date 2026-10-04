#include "DmaSyncStage.h"

NTSTATUS
LecSgSyncMapNoLaunch(
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Outptr_ PLECS65_SG_SYNC_STAGE* Result)
{
    PLECS65_SG_SYNC_STAGE stage;
    PMDL mdl;
    ULONGLONG available = 0;
    NTSTATUS status;

    if (Result == NULL) return STATUS_INVALID_PARAMETER;
    *Result = NULL;
    if (Adapter == NULL || Adapter->DmaOperations == NULL ||
        Adapter->DmaOperations->GetScatterGatherListEx == NULL ||
        Adapter->DmaOperations->InitializeDmaTransferContext == NULL ||
        Adapter->DmaOperations->FreeAdapterObject == NULL ||
        DeviceObject == NULL || LockedMdlChain == NULL ||
        Length == 0 || (Length & 3U) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    for (mdl = LockedMdlChain; mdl != NULL; mdl = mdl->Next) {
        if ((mdl->MdlFlags & MDL_PAGES_LOCKED) == 0 ||
            MmGetMdlByteCount(mdl) == 0) {
            return STATUS_INVALID_PARAMETER;
        }
        available += MmGetMdlByteCount(mdl);
        if (available >= Length) break;
    }
    if (available < Length) return STATUS_INVALID_BUFFER_SIZE;

    stage = (PLECS65_SG_SYNC_STAGE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*stage), LECS65_TAG);
    if (!stage) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(stage, sizeof(*stage));
    stage->Adapter = Adapter;
    stage->DeviceObject = DeviceObject;
    stage->MdlChain = LockedMdlChain;
    stage->RequestedLength = Length;

    /*
     * No queued callback, no external device operation, no outstanding
     * async submission after return. References remain owned through
     * FreeAdapterObject. No bus-master launch API exists in this module.
     */
    ObReferenceObject(DeviceObject);
    status = Adapter->DmaOperations->InitializeDmaTransferContext(
        Adapter, stage->TransferContext);
    if (NT_SUCCESS(status)) {
        status = Adapter->DmaOperations->GetScatterGatherListEx(
            Adapter, DeviceObject, stage->TransferContext,
            LockedMdlChain, 0, Length,
            DMA_SYNCHRONOUS_CALLBACK, NULL, NULL, FALSE,
            NULL, NULL, &stage->List);
    }

    if (!NT_SUCCESS(status)) {
        /*
         * By the synchronous DDI contract, failure never queues a later
         * callback or adapter-resource allocation. No FreeAdapterObject
         * is required, and no hardware transaction has started.
         */
        ObDereferenceObject(DeviceObject);
        ExFreePoolWithTag(stage, LECS65_TAG);
        return status;
    }

    if (stage->List == NULL) {
        /*
         * A successful DDI must return the mapping. Treat a violated
         * contract as fatal/unsafe rather than guessing at cleanup.
         */
        return STATUS_INTERNAL_ERROR;
    }

    *Result = stage;
    return STATUS_SUCCESS;
}

NTSTATUS
LecSgSyncCopySegments(
    _In_ PLECS65_SG_SYNC_STAGE Stage,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied)
{
    ULONG i, n;
    ULONGLONG total = 0;

    if (Copied == NULL || Stage == NULL || Elements == NULL ||
        Capacity == 0 || Stage->List == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Copied = 0;
    n = Stage->List->NumberOfElements;
    if (n == 0) return STATUS_INVALID_BUFFER_SIZE;
    if (n > Capacity) return STATUS_BUFFER_TOO_SMALL;

    for (i = 0; i < n; ++i) {
        const SCATTER_GATHER_ELEMENT* e = &Stage->List->Elements[i];
        ULONGLONG address = (ULONGLONG)e->Address.QuadPart;
        if (e->Length == 0 || (e->Length & 3U) != 0 ||
            (address & 3U) != 0 ||
            address > MAXULONG ||
            (ULONGLONG)e->Length - 1U > MAXULONG - address) {
            return STATUS_INVALID_BUFFER_SIZE;
        }
        total += e->Length;
        if (total > Stage->RequestedLength) {
            return STATUS_INVALID_BUFFER_SIZE;
        }
    }
    if (total != Stage->RequestedLength) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlCopyMemory(Elements, Stage->List->Elements,
        (SIZE_T)n * sizeof(*Elements));
    *Copied = n;
    return STATUS_SUCCESS;
}

NTSTATUS
LecSgSyncReleaseNoLaunch(
    _Inout_ PLECS65_SG_SYNC_STAGE* Stage)
{
    PLECS65_SG_SYNC_STAGE current;
    if (Stage == NULL || *Stage == NULL) return STATUS_INVALID_PARAMETER;
    current = *Stage;
    /*
     * Safe only because this interface has no DMA launch capability,
     * GetScatterGatherListEx cannot queue with this flag, and the caller
     * holds sole ownership. Parent must retain adapter/MDLs/PnP gate.
     */
    current->Adapter->DmaOperations->FreeAdapterObject(
        current->Adapter, DeallocateObject);
    ObDereferenceObject(current->DeviceObject);
    *Stage = NULL;
    ExFreePoolWithTag(current, LECS65_TAG);
    return STATUS_SUCCESS;
}
