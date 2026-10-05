#include "DmaAdapterStage.h"
#include "DmaLayout.h"

static BOOLEAN
LecDmaHasOperation(
    _In_opt_ PDMA_OPERATIONS Operations,
    _In_ SIZE_T RequiredSize)
{
    return (BOOLEAN)(Operations != NULL &&
        Operations->Size >= RequiredSize);
}

static VOID
LecDmaPutAdapterIfSupported(_In_opt_ PDMA_ADAPTER Adapter)
{
    PDMA_OPERATIONS operations;

    if (Adapter == NULL) {
        return;
    }
    operations = Adapter->DmaOperations;
    if (LecDmaHasOperation(operations,
            RTL_SIZEOF_THROUGH_FIELD(DMA_OPERATIONS, PutDmaAdapter)) &&
        operations->PutDmaAdapter != NULL) {
        operations->PutDmaAdapter(Adapter);
    }
}

/*
 * WDM DMA-adapter acquisition and device-visible common buffer.
 * Intentionally not wired to PnP or DMA requests yet.
 */
NTSTATUS
LecDmaCreateAdapterContext(
    _In_ PDEVICE_OBJECT PhysicalDeviceObject,
    _In_ ULONG MaximumTransferBytes,
    _Outptr_ PLECS65_DMA_ADAPTER_CONTEXT* Result)
{
    DEVICE_DESCRIPTION description;
    PLECS65_DMA_ADAPTER_CONTEXT context;

    if (Result == NULL || PhysicalDeviceObject == NULL ||
        MaximumTransferBytes == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    *Result = NULL;
    context = (PLECS65_DMA_ADAPTER_CONTEXT)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*context), LECS65_TAG);
    if (context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(context, sizeof(*context));
    RtlZeroMemory(&description, sizeof(description));
    KeInitializeSpinLock(&context->Lock);

    description.Version = DEVICE_DESCRIPTION_VERSION3;
    description.Master = TRUE;
    description.ScatterGather = TRUE;
    description.DmaAddressWidth = 32;
    description.InterfaceType = PCIBus;
    description.MaximumLength = MaximumTransferBytes;

    context->Adapter = IoGetDmaAdapter(
        PhysicalDeviceObject,
        &description,
        &context->NumberOfMapRegisters);
    if (context->Adapter == NULL) {
        ExFreePoolWithTag(context, LECS65_TAG);
        return STATUS_NOT_SUPPORTED;
    }

    /*
     * The only new no-launch mapping stage requires the v3 synchronous
     * contract. Refuse an adapter missing any mandatory v3 operation;
     * never silently fall back to the callback-driven v2 mechanism.
     */
    if (!LecDmaHasOperation(context->Adapter->DmaOperations,
            RTL_SIZEOF_THROUGH_FIELD(DMA_OPERATIONS, FreeAdapterObject)) ||
        context->Adapter->DmaOperations->PutDmaAdapter == NULL ||
        context->Adapter->DmaOperations->AllocateCommonBuffer == NULL ||
        context->Adapter->DmaOperations->FreeCommonBuffer == NULL ||
        context->Adapter->DmaOperations->GetScatterGatherListEx == NULL ||
        context->Adapter->DmaOperations->InitializeDmaTransferContext == NULL ||
        context->Adapter->DmaOperations->FreeAdapterObject == NULL ||
        context->NumberOfMapRegisters == 0) {
        LecDmaPutAdapterIfSupported(context->Adapter);
        ExFreePoolWithTag(context, LECS65_TAG);
        return STATUS_NOT_SUPPORTED;
    }

    /*
     * An adapter may provide fewer map registers than the request would
     * need. The later mapping layer must fragment transfers as required,
     * not assume MaximumLength is a hard allocation guarantee.
     */
    ObReferenceObject(PhysicalDeviceObject);
    context->PhysicalDeviceObject = PhysicalDeviceObject;
    *Result = context;
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaAllocateCommonTable(
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ ULONG Length)
{
    PVOID buffer;
    PHYSICAL_ADDRESS logical;
    KIRQL irql;

    if (Context == NULL || Context->Adapter == NULL ||
        Context->Adapter->DmaOperations == NULL ||
        Length == 0 || (Length & (PAGE_SIZE - 1U)) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Context->Lock, &irql);
    if (Context->Releasing || Context->Quarantined ||
        Context->SynchronousOwner != NULL ||
        Context->TableAllocationPending) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (Context->TableVirtual != NULL) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    Context->TableAllocationPending = TRUE;
    KeReleaseSpinLock(&Context->Lock, irql);

    logical.QuadPart = 0;
    buffer = Context->Adapter->DmaOperations->AllocateCommonBuffer(
        Context->Adapter, Length, &logical, FALSE);
    if (buffer == NULL) {
        KeAcquireSpinLock(&Context->Lock, &irql);
        Context->TableAllocationPending = FALSE;
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* SGTA and every page-chain target are legacy 32-bit dword addresses. */
    if ((((ULONG_PTR)buffer) & (PAGE_SIZE - 1U)) != 0 ||
        (logical.QuadPart & (PAGE_SIZE - 1U)) != 0 ||
        logical.QuadPart < 0 ||
        (ULONGLONG)logical.QuadPart > MAXULONG ||
        (ULONGLONG)Length - 1U >
            MAXULONG - (ULONGLONG)logical.QuadPart) {
        Context->Adapter->DmaOperations->FreeCommonBuffer(
            Context->Adapter, Length, logical, buffer, FALSE);
        KeAcquireSpinLock(&Context->Lock, &irql);
        Context->TableAllocationPending = FALSE;
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_NOT_SUPPORTED;
    }

    RtlZeroMemory(buffer, Length);
    KeAcquireSpinLock(&Context->Lock, &irql);
    Context->TableVirtual = buffer;
    Context->TableLogical = logical;
    Context->TableLength = Length;
    Context->TableAllocationPending = FALSE;
    KeReleaseSpinLock(&Context->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaReleaseAdapterContext(
    _In_opt_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ BOOLEAN ProvenIdle)
{
    KIRQL irql;

    if (Context == NULL) {
        return STATUS_SUCCESS;
    }

    KeAcquireSpinLock(&Context->Lock, &irql);
    if (Context->Releasing || Context->TableAllocationPending ||
        Context->SynchronousOwner != NULL) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (!ProvenIdle || Context->Quarantined) {
        /*
         * DMA may still read the descriptor table. Dropping a common
         * buffer's IOMMU mapping would be unsafe even if CPU pages remain
         * pinned. Quarantine context independent of the device extension.
         */
        Context->Quarantined = TRUE;
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Context->Releasing = TRUE;
    KeReleaseSpinLock(&Context->Lock, irql);

    if (Context->TableVirtual != NULL) {
        Context->Adapter->DmaOperations->FreeCommonBuffer(
            Context->Adapter,
            Context->TableLength,
            Context->TableLogical,
            Context->TableVirtual,
            FALSE);
    }

    LecDmaPutAdapterIfSupported(Context->Adapter);
    if (Context->PhysicalDeviceObject != NULL) {
        ObDereferenceObject(Context->PhysicalDeviceObject);
    }

    ExFreePoolWithTag(Context, LECS65_TAG);
    return STATUS_SUCCESS;
}

VOID
LecDmaQuarantineAdapterContext(
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT Context)
{
    KIRQL irql;

    if (Context == NULL) {
        return;
    }
    KeAcquireSpinLock(&Context->Lock, &irql);
    Context->Quarantined = TRUE;
    KeReleaseSpinLock(&Context->Lock, irql);
}

NTSTATUS
LecDmaClaimSynchronousOwner(
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ PVOID Owner,
    _Out_ PULONG DescriptorSlotCapacity)
{
    KIRQL irql;
    ULONG capacity;

    if (Context == NULL || Owner == NULL || DescriptorSlotCapacity == NULL ||
        Context->Adapter == NULL || Context->PhysicalDeviceObject == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *DescriptorSlotCapacity = 0;

    KeAcquireSpinLock(&Context->Lock, &irql);
    if (Context->Quarantined || Context->Releasing ||
        Context->TableAllocationPending ||
        Context->SynchronousOwner != NULL) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }

    if (Context->TableVirtual == NULL || Context->TableLength == 0) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    if ((((ULONG_PTR)Context->TableVirtual) &
            (LECS65_DMA_LAYOUT_PAGE_BYTES - 1U)) != 0 ||
        (Context->TableLogical.QuadPart &
            (LECS65_DMA_LAYOUT_PAGE_BYTES - 1U)) != 0 ||
        Context->TableLogical.QuadPart < 0 ||
        (ULONGLONG)Context->TableLogical.QuadPart > MAXULONG ||
        Context->TableLength < LECS65_DMA_LAYOUT_PAGE_BYTES ||
        (Context->TableLength % LECS65_DMA_LAYOUT_PAGE_BYTES) != 0 ||
        (ULONGLONG)Context->TableLength - 1U >
            MAXULONG - (ULONGLONG)Context->TableLogical.QuadPart) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    capacity = Context->TableLength /
        (ULONG)sizeof(LECS65_DMA_LAYOUT_ENTRY);
    if (capacity < LECS65_DMA_LAYOUT_SLOTS_PER_PAGE) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    Context->SynchronousOwner = Owner;
    *DescriptorSlotCapacity = capacity;
    KeReleaseSpinLock(&Context->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaReleaseSynchronousOwner(
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ PVOID Owner)
{
    KIRQL irql;

    if (Context == NULL || Owner == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Context->Lock, &irql);
    if (Context->SynchronousOwner != Owner) {
        KeReleaseSpinLock(&Context->Lock, irql);
        return STATUS_INVALID_PARAMETER;
    }
    Context->SynchronousOwner = NULL;
    KeReleaseSpinLock(&Context->Lock, irql);
    return STATUS_SUCCESS;
}
