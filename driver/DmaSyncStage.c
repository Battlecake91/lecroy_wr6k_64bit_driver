#include "DmaSyncStage.h"

/* All functions below are unreachable from current live PCI paths. */
VOID
LecSgSyncOwnerInit(
    _Out_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject)
{
    RtlZeroMemory(Owner, sizeof(*Owner));
    KeInitializeSpinLock(&Owner->Lock);
    Owner->Adapter = Adapter;
    Owner->DeviceObject = DeviceObject;
}

NTSTATUS
LecSgSyncOwnerStop(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    ULONG pending;
    if (!Owner) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    Owner->Stopping = TRUE;
    pending = Owner->Outstanding;
    KeReleaseSpinLock(&Owner->Lock, irql);
    return pending ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
}

BOOLEAN
LecSgSyncOwnerCanTeardown(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    BOOLEAN ready;
    if (!Owner) return FALSE;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    ready = (BOOLEAN)(Owner->Stopping && Owner->Outstanding == 0 &&
                       Owner->Mappings == NULL);
    KeReleaseSpinLock(&Owner->Lock, irql);
    return ready;
}

static VOID
LecSgSyncOwnerDone(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    --Owner->Outstanding;
    KeReleaseSpinLock(&Owner->Lock, irql);
}

NTSTATUS
LecSgSyncMapNoLaunch(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Out_ ULONGLONG* Token)
{
    PLECS65_SG_SYNC_STAGE stage;
    PMDL mdl;
    ULONGLONG available = 0, id;
    NTSTATUS status;
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT device;
    KIRQL irql;

    if (!Token) return STATUS_INVALID_PARAMETER;
    *Token = 0;
    if (!Owner || !LockedMdlChain || !Length || (Length & 3U))
        return STATUS_INVALID_PARAMETER;

    /*
     * Reserve BEFORE inspecting MDLs. Once STOP is requested it must
     * not observe a quiescent parent while an in-flight request is
     * starting to access the adapter or pinned buffers.
     */
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (Owner->Stopping || Owner->Outstanding == MAXULONG ||
        Owner->NextToken == (ULONGLONG)-1) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    adapter = Owner->Adapter;
    device = Owner->DeviceObject;
    if (!adapter || !adapter->DmaOperations ||
        !adapter->DmaOperations->InitializeDmaTransferContext ||
        !adapter->DmaOperations->GetScatterGatherListEx ||
        !adapter->DmaOperations->FreeAdapterObject || !device) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_NOT_SUPPORTED;
    }
    ++Owner->Outstanding;
    id = ++Owner->NextToken; /* Never reused, including failed requests. */
    KeReleaseSpinLock(&Owner->Lock, irql);

    for (mdl = LockedMdlChain; mdl != NULL; mdl = mdl->Next) {
        if (!(mdl->MdlFlags & MDL_PAGES_LOCKED) ||
            MmGetMdlByteCount(mdl) == 0) {
            status = STATUS_INVALID_PARAMETER;
            goto FailReservation;
        }
        available += MmGetMdlByteCount(mdl);
        if (available >= Length) break;
    }
    if (available < Length) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto FailReservation;
    }

    stage = (PLECS65_SG_SYNC_STAGE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*stage), LECS65_TAG);
    if (!stage) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto FailReservation;
    }
    RtlZeroMemory(stage, sizeof(*stage));
    stage->Parent = Owner;
    stage->MdlChain = LockedMdlChain;
    stage->RequestedLength = Length;
    stage->Token = id;

    /*
     * Version 3 guarantees no delayed callback in this mode.
     * No code in this stage is allowed to launch DMA on this device.
     */
    ObReferenceObject(device);
    status = adapter->DmaOperations->InitializeDmaTransferContext(
        adapter, stage->TransferContext);
    if (NT_SUCCESS(status)) {
        status = adapter->DmaOperations->GetScatterGatherListEx(
            adapter, device, stage->TransferContext, LockedMdlChain,
            0, Length, DMA_SYNCHRONOUS_CALLBACK, NULL, NULL, FALSE,
            NULL, NULL, &stage->List);
    }
    if (!NT_SUCCESS(status)) {
        /* No queued callback or allocation on synchronous failure. */
        ObDereferenceObject(device);
        ExFreePoolWithTag(stage, LECS65_TAG);
        goto FailReservation;
    }

    if (!stage->List) {
        /*
         * Inconsistent success from DMA DDI: retain all resources,
         * stage and parent count. No valid list to free safely.
         */
        return STATUS_INTERNAL_ERROR;
    }

    KeAcquireSpinLock(&Owner->Lock, &irql);
    stage->Next = Owner->Mappings;
    Owner->Mappings = stage;
    KeReleaseSpinLock(&Owner->Lock, irql);
    *Token = id;
    return STATUS_SUCCESS;

FailReservation:
    LecSgSyncOwnerDone(Owner);
    return status;
}

NTSTATUS
LecSgSyncCopySegments(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ ULONGLONG Token,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied)
{
    PLECS65_SG_SYNC_STAGE stage;
    KIRQL irql;
    ULONG i, count;
    ULONGLONG total = 0;
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    if (!Owner || !Token || !Copied || !Elements || !Capacity)
        return STATUS_INVALID_PARAMETER;
    *Copied = 0;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    for (stage = Owner->Mappings; stage != NULL; stage = stage->Next)
        if (stage->Token == Token) break;

    if (!stage || !stage->List) goto Done;
    count = stage->List->NumberOfElements;
    if (!count) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto Done;
    }
    if (count > Capacity) {
        status = STATUS_BUFFER_TOO_SMALL;
        goto Done;
    }

    for (i = 0; i < count; ++i) {
        const SCATTER_GATHER_ELEMENT* e = &stage->List->Elements[i];
        ULONGLONG address = (ULONGLONG)e->Address.QuadPart;
        if (e->Length == 0 || (e->Length & 3U) ||
            (address & 3U) || address > MAXULONG ||
            (ULONGLONG)e->Length - 1U > MAXULONG - address) {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto Done;
        }
        total += e->Length;
        if (total > stage->RequestedLength) {
            status = STATUS_INVALID_BUFFER_SIZE;
            goto Done;
        }
    }
    if (total != stage->RequestedLength) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto Done;
    }

    RtlCopyMemory(Elements, stage->List->Elements,
        (SIZE_T)count * sizeof(*Elements));
    *Copied = count;
    status = STATUS_SUCCESS;
Done:
    KeReleaseSpinLock(&Owner->Lock, irql);
    return status;
}

NTSTATUS
LecSgSyncReleaseNoLaunch(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ ULONGLONG Token)
{
    PLECS65_SG_SYNC_STAGE stage, *link;
    KIRQL irql;
    if (!Owner || !Token) return STATUS_INVALID_PARAMETER;

    /*
     * Unlink under the SAME lock used by CopySegments. Concurrent
     * readers can never dereference a freed stage, and stale tokens
     * never resolve to a different mapping (tokens aren't reused).
     */
    KeAcquireSpinLock(&Owner->Lock, &irql);
    for (link = &Owner->Mappings; *link; link = &(*link)->Next)
        if ((*link)->Token == Token) break;
    stage = *link;
    if (!stage) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    *link = stage->Next;
    KeReleaseSpinLock(&Owner->Lock, irql);

    /*
     * This stage has no device launch capability. Only its synchronous
     * v3 no-launch allocation may be released. Do not use for any
     * actual/unknown-active WR6k DMA transaction.
     */
    Owner->Adapter->DmaOperations->FreeAdapterObject(
        Owner->Adapter, DeallocateObject);
    ObDereferenceObject(Owner->DeviceObject);
    LecSgSyncOwnerDone(Owner);
    ExFreePoolWithTag(stage, LECS65_TAG);
    return STATUS_SUCCESS;
}
