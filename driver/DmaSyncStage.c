#include "DmaSyncStage.h"
#include "DmaLayout.h"

#define LECS65_DMA_DESCRIPTOR_SLOTS_PER_PAGE \
    LECS65_DMA_LAYOUT_SLOTS_PER_PAGE

static NTSTATUS
LecSgSyncValidateMdlRange(
    _In_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PMDL MdlChain,
    _In_ ULONG Length)
{
    PMDL slow = MdlChain;
    PMDL fast = MdlChain;
    PMDL mdl;
    ULONG remaining = Length;
    ULONGLONG mapRegisters = 0;

    /* Reject a corrupt/cyclic chain before its byte counts can be reused. */
    while (fast != NULL && fast->Next != NULL) {
        slow = slow->Next;
        fast = fast->Next->Next;
        if (slow == fast) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    for (mdl = MdlChain; mdl != NULL && remaining != 0; mdl = mdl->Next) {
        ULONG bytes;
        ULONG used;
        ULONG span;
        PVOID virtualAddress;

        if (!(mdl->MdlFlags & MDL_PAGES_LOCKED)) {
            return STATUS_INVALID_PARAMETER;
        }
        bytes = MmGetMdlByteCount(mdl);
        virtualAddress = MmGetMdlVirtualAddress(mdl);
        if (bytes == 0 || virtualAddress == NULL) {
            return STATUS_INVALID_PARAMETER;
        }
        used = bytes < remaining ? bytes : remaining;
        span = ADDRESS_AND_SIZE_TO_SPAN_PAGES(virtualAddress, used);
        mapRegisters += span;
        if (mapRegisters > Owner->AdapterContext->NumberOfMapRegisters) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        remaining -= used;
    }

    return remaining == 0 ? STATUS_SUCCESS : STATUS_INVALID_BUFFER_SIZE;
}

static NTSTATUS
LecSgSyncValidateMappedList(
    _In_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PLECS65_SG_SYNC_STAGE Stage)
{
    ULONG i;
    ULONG slot = 0;
    ULONGLONG total = 0;

    if (Stage->List == NULL || Stage->List->NumberOfElements == 0) {
        return STATUS_INVALID_BUFFER_SIZE;
    }
    if (Stage->List->NumberOfElements >= Owner->DescriptorSlotCapacity) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    for (i = 0; i < Stage->List->NumberOfElements; ++i) {
        const SCATTER_GATHER_ELEMENT* element = &Stage->List->Elements[i];
        ULONGLONG address = (ULONGLONG)element->Address.QuadPart;
        ULONG remaining = element->Length;

        if (remaining == 0 || (remaining & 3U) != 0 ||
            (address & 3U) != 0 || address > MAXULONG ||
            (ULONGLONG)remaining - 1U > MAXULONG - address) {
            return STATUS_INVALID_BUFFER_SIZE;
        }
        total += remaining;
        if (total > Stage->RequestedLength) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        while (remaining != 0) {
            ULONG pageRemaining = PAGE_SIZE -
                (ULONG)(address & (PAGE_SIZE - 1U));
            ULONG fragment = remaining < pageRemaining ?
                remaining : pageRemaining;

            if ((slot % LECS65_DMA_DESCRIPTOR_SLOTS_PER_PAGE) ==
                LECS65_DMA_DESCRIPTOR_SLOTS_PER_PAGE - 1U) {
                if (slot >= Owner->DescriptorSlotCapacity ||
                    slot + 1U >= Owner->DescriptorSlotCapacity) {
                    return STATUS_BUFFER_TOO_SMALL;
                }
                ++slot; /* Page-chain descriptor. */
            }
            if (slot >= Owner->DescriptorSlotCapacity) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            ++slot;
            address += fragment;
            remaining -= fragment;
        }
    }

    if (total != Stage->RequestedLength) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    /* The board requires a final zero descriptor in a normal data slot. */
    if ((slot % LECS65_DMA_DESCRIPTOR_SLOTS_PER_PAGE) ==
        LECS65_DMA_DESCRIPTOR_SLOTS_PER_PAGE - 1U) {
        if (slot >= Owner->DescriptorSlotCapacity ||
            slot + 1U >= Owner->DescriptorSlotCapacity) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        ++slot;
    }
    return slot < Owner->DescriptorSlotCapacity ?
        STATUS_SUCCESS : STATUS_BUFFER_TOO_SMALL;
}

static VOID
LecSgSyncFreeStage(_Inout_ PLECS65_SG_SYNC_STAGE Stage)
{
    PLECS65_DMA_ADAPTER_CONTEXT context = Stage->Parent->AdapterContext;

    context->Adapter->DmaOperations->FreeAdapterObject(
        context->Adapter, DeallocateObject);
    ObDereferenceObject(context->PhysicalDeviceObject);
    ExFreePoolWithTag(Stage, LECS65_TAG);
}

static BOOLEAN
LecSgSyncQuarantineRequested(_In_ PLECS65_SG_SYNC_OWNER Owner)
{
    return (BOOLEAN)(InterlockedCompareExchange(
        &Owner->QuarantineRequested, 0, 0) != 0);
}

/* Caller holds Owner->Lock. A TRUE result is an irreversible free commit. */
static BOOLEAN
LecSgSyncCommitReleaseLocked(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    if (LecSgSyncQuarantineRequested(Owner)) {
        Owner->State = LecSgSyncOwnerQuarantined;
        return FALSE;
    }
    ++Owner->ReleasesInFlight;
    return TRUE;
}

static VOID
LecSgSyncCompleteRelease(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;

    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (Owner->ReleasesInFlight != 0) {
        --Owner->ReleasesInFlight;
    }
    if (Owner->Outstanding != 0) {
        --Owner->Outstanding;
    }
    if (LecSgSyncQuarantineRequested(Owner)) {
        Owner->LateQuarantine = TRUE;
        Owner->State = LecSgSyncOwnerQuarantined;
    }
    KeReleaseSpinLock(&Owner->Lock, irql);
}

/* All functions below are unreachable from current live PCI paths. */
VOID
LecSgSyncOwnerConstruct(_Out_ PLECS65_SG_SYNC_OWNER Owner)
{
    if (Owner == NULL) {
        return;
    }

    /* One-time construction never reads possibly uninitialized storage. */
    RtlZeroMemory(Owner, sizeof(*Owner));
    KeInitializeSpinLock(&Owner->Lock);
    Owner->State = LecSgSyncOwnerConstructed;
}

NTSTATUS
LecSgSyncOwnerInit(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT AdapterContext)
{
    LECS65_SG_SYNC_OWNER_STATE priorState;
    ULONG descriptorSlotCapacity;
    KIRQL irql;
    NTSTATUS status;

    if (Owner == NULL || AdapterContext == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Owner->Lock, &irql);
    priorState = Owner->State;
    if ((priorState != LecSgSyncOwnerConstructed &&
         priorState != LecSgSyncOwnerDestroyed) ||
        Owner->AdapterContext != NULL || Owner->Outstanding != 0 ||
        Owner->Mappings != NULL || LecSgSyncQuarantineRequested(Owner)) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Owner->State = LecSgSyncOwnerInitializing;
    KeReleaseSpinLock(&Owner->Lock, irql);

    status = LecDmaClaimSynchronousOwner(
        AdapterContext, Owner, &descriptorSlotCapacity);

    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (!NT_SUCCESS(status)) {
        Owner->State = priorState;
    }
    else if (LecSgSyncQuarantineRequested(Owner)) {
        /* The successfully claimed adapter ownership is now retained. */
        Owner->AdapterContext = AdapterContext;
        Owner->DescriptorSlotCapacity = descriptorSlotCapacity;
        Owner->State = LecSgSyncOwnerQuarantined;
        status = STATUS_DEVICE_BUSY;
    }
    else {
        Owner->AdapterContext = AdapterContext;
        Owner->DescriptorSlotCapacity = descriptorSlotCapacity;
        Owner->State = LecSgSyncOwnerActive;
    }
    KeReleaseSpinLock(&Owner->Lock, irql);
    return status;
}

NTSTATUS
LecSgSyncOwnerStop(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    ULONG pending;
    if (!Owner) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner) ||
        Owner->State == LecSgSyncOwnerQuarantined ||
        Owner->State == LecSgSyncOwnerInitializing ||
        Owner->State == LecSgSyncOwnerDestroying) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (Owner->State != LecSgSyncOwnerActive &&
        Owner->State != LecSgSyncOwnerStopping) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    Owner->State = LecSgSyncOwnerStopping;
    pending = Owner->Outstanding;
    KeReleaseSpinLock(&Owner->Lock, irql);
    return pending ? STATUS_DEVICE_BUSY : STATUS_SUCCESS;
}

NTSTATUS
LecSgSyncOwnerQuarantine(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _Out_opt_ PBOOLEAN ReleaseAlreadyCommitted)
{
    KIRQL irql;
    BOOLEAN late;

    if (ReleaseAlreadyCommitted != NULL) {
        *ReleaseAlreadyCommitted = FALSE;
    }
    if (Owner == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Latch before taking the lock so waiters cannot pass a later commit. */
    InterlockedExchange(&Owner->QuarantineRequested, 1);
    KeAcquireSpinLock(&Owner->Lock, &irql);
    late = (BOOLEAN)(Owner->ReleasesInFlight != 0);
    if (late) {
        Owner->LateQuarantine = TRUE;
    }
    Owner->State = LecSgSyncOwnerQuarantined;
    KeReleaseSpinLock(&Owner->Lock, irql);

    if (ReleaseAlreadyCommitted != NULL) {
        *ReleaseAlreadyCommitted = late;
    }
    return STATUS_SUCCESS;
}

BOOLEAN
LecSgSyncOwnerCanTeardown(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    BOOLEAN ready;
    if (!Owner) return FALSE;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    ready = (BOOLEAN)(!LecSgSyncQuarantineRequested(Owner) &&
                       Owner->State == LecSgSyncOwnerStopping &&
                       Owner->Outstanding == 0 &&
                       Owner->ReleasesInFlight == 0 &&
                       Owner->Mappings == NULL);
    KeReleaseSpinLock(&Owner->Lock, irql);
    return ready;
}

static VOID
LecSgSyncOwnerDone(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (Owner->Outstanding != 0) {
        --Owner->Outstanding;
    }
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
    ULONGLONG id;
    NTSTATUS status;
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT device;
    KIRQL irql;

    if (!Token) return STATUS_INVALID_PARAMETER;
    *Token = 0;
    if (!Owner || !LockedMdlChain ||
        !Length || (Length & 3U))
        return STATUS_INVALID_PARAMETER;

    /*
     * Reserve BEFORE inspecting MDLs. Once STOP is requested it must
     * not observe a quiescent parent while an in-flight request is
     * starting to access the adapter or pinned buffers.
     */
    KeAcquireSpinLock(&Owner->Lock, &irql);
    /*
     * A v3 channel is freed by adapter pointer, not a stage token.
     * Until channel-sharing semantics are proven, allow at most ONE
     * outstanding allocation for this DMA adapter.
     */
    if (LecSgSyncQuarantineRequested(Owner) ||
        Owner->State != LecSgSyncOwnerActive ||
        Owner->AdapterContext == NULL || Owner->Outstanding != 0 ||
        Owner->NextToken == (ULONGLONG)-1) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    adapter = Owner->AdapterContext->Adapter;
    device = Owner->AdapterContext->PhysicalDeviceObject;
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

    status = LecSgSyncValidateMdlRange(Owner, LockedMdlChain, Length);
    if (!NT_SUCCESS(status)) {
        goto FailReservation;
    }

    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner) ||
        Owner->State != LecSgSyncOwnerActive) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        status = STATUS_DELETE_PENDING;
        goto FailReservation;
    }
    KeReleaseSpinLock(&Owner->Lock, irql);

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

    status = LecSgSyncValidateMappedList(Owner, stage);
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner)) {
        /* GetEx succeeded: retain even a malformed allocation fail-closed. */
        stage->Next = Owner->Mappings;
        Owner->Mappings = stage;
        Owner->State = LecSgSyncOwnerQuarantined;
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (!NT_SUCCESS(status) || Owner->State != LecSgSyncOwnerActive) {
        NTSTATUS releaseStatus = status;

        if (NT_SUCCESS(releaseStatus)) {
            releaseStatus = STATUS_DELETE_PENDING;
        }
        if (!LecSgSyncCommitReleaseLocked(Owner)) {
            stage->Next = Owner->Mappings;
            Owner->Mappings = stage;
            KeReleaseSpinLock(&Owner->Lock, irql);
            return STATUS_DEVICE_BUSY;
        }
        KeReleaseSpinLock(&Owner->Lock, irql);
        LecSgSyncFreeStage(stage);
        LecSgSyncCompleteRelease(Owner);
        return releaseStatus;
    }
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
    ULONG count;
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    if (!Owner || !Token || !Copied ||
        !Elements || !Capacity)
        return STATUS_INVALID_PARAMETER;
    *Copied = 0;
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner) ||
        (Owner->State != LecSgSyncOwnerActive &&
         Owner->State != LecSgSyncOwnerStopping)) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    for (stage = Owner->Mappings; stage != NULL; stage = stage->Next)
        if (stage->Token == Token) break;

    if (!stage || !stage->List) goto Done;
    count = stage->List->NumberOfElements;
    if (count > Capacity) {
        status = STATUS_BUFFER_TOO_SMALL;
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
    if (!Owner || !Token)
        return STATUS_INVALID_PARAMETER;

    /*
     * Unlink under the SAME lock used by CopySegments. Concurrent
     * readers can never dereference a freed stage, and stale tokens
     * never resolve to a different mapping (tokens aren't reused).
     */
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner) ||
        (Owner->State != LecSgSyncOwnerActive &&
         Owner->State != LecSgSyncOwnerStopping)) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    for (link = &Owner->Mappings; *link; link = &(*link)->Next)
        if ((*link)->Token == Token) break;
    stage = *link;
    if (!stage) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    if (!LecSgSyncCommitReleaseLocked(Owner)) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    *link = stage->Next;
    KeReleaseSpinLock(&Owner->Lock, irql);

    /*
     * This stage has no device launch capability. Only its synchronous
     * v3 no-launch allocation may be released. Do not use for any
     * actual/unknown-active WR6k DMA transaction.
     */
    LecSgSyncFreeStage(stage);
    LecSgSyncCompleteRelease(Owner);
    return STATUS_SUCCESS;
}

NTSTATUS
LecSgSyncOwnerDrainNoLaunch(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    KIRQL irql;
    ULONGLONG token;
    NTSTATUS status;

    if (Owner == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    for (;;) {
        KeAcquireSpinLock(&Owner->Lock, &irql);
        if (LecSgSyncQuarantineRequested(Owner) ||
            Owner->State != LecSgSyncOwnerStopping) {
            KeReleaseSpinLock(&Owner->Lock, irql);
            return STATUS_INVALID_DEVICE_STATE;
        }
        if (Owner->Mappings == NULL) {
            status = Owner->Outstanding == 0 ?
                STATUS_SUCCESS : STATUS_DEVICE_BUSY;
            KeReleaseSpinLock(&Owner->Lock, irql);
            return status;
        }
        token = Owner->Mappings->Token;
        KeReleaseSpinLock(&Owner->Lock, irql);

        status = LecSgSyncReleaseNoLaunch(Owner, token);
        if (!NT_SUCCESS(status) && status != STATUS_INVALID_PARAMETER) {
            return status;
        }
    }
}

NTSTATUS
LecSgSyncOwnerDestroy(_Inout_ PLECS65_SG_SYNC_OWNER Owner)
{
    PLECS65_DMA_ADAPTER_CONTEXT context;
    KIRQL irql;
    NTSTATUS status;

    if (Owner == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (LecSgSyncQuarantineRequested(Owner) ||
        Owner->State == LecSgSyncOwnerQuarantined) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (Owner->State == LecSgSyncOwnerDestroyed) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    if (Owner->State != LecSgSyncOwnerStopping ||
        Owner->AdapterContext == NULL || Owner->Outstanding != 0 ||
        Owner->Mappings != NULL || Owner->ReleasesInFlight != 0) {
        KeReleaseSpinLock(&Owner->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Owner->State = LecSgSyncOwnerDestroying;
    context = Owner->AdapterContext;
    KeReleaseSpinLock(&Owner->Lock, irql);

    status = LecDmaReleaseSynchronousOwner(context, Owner);
    KeAcquireSpinLock(&Owner->Lock, &irql);
    if (NT_SUCCESS(status)) {
        Owner->AdapterContext = NULL;
        Owner->DescriptorSlotCapacity = 0;
        Owner->State = LecSgSyncOwnerDestroyed;
    }
    else {
        Owner->State = LecSgSyncOwnerStopping;
    }
    KeReleaseSpinLock(&Owner->Lock, irql);
    return status;
}
