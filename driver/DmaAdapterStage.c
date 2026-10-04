#include "DmaAdapterStage.h"

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

    description.Version = DEVICE_DESCRIPTION_VERSION2;
    description.Master = TRUE;
    description.ScatterGather = TRUE;
    description.Dma32BitAddresses = TRUE;
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
     * An adapter may provide fewer map registers than the request would
     * need. The later mapping layer must fragment transfers as required,
     * not assume MaximumLength is a hard allocation guarantee.
     */
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

    if (Context == NULL || Context->Adapter == NULL ||
        Context->Adapter->DmaOperations == NULL ||
        Context->TableVirtual != NULL ||
        Length == 0 || (Length & (PAGE_SIZE - 1U)) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    logical.QuadPart = 0;
    buffer = Context->Adapter->DmaOperations->AllocateCommonBuffer(
        Context->Adapter, Length, &logical, FALSE);
    if (buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* SGTA and every page-chain target are legacy 32-bit dword addresses. */
    if ((logical.QuadPart & (PAGE_SIZE - 1U)) != 0 ||
        logical.QuadPart < 0 ||
        (ULONGLONG)logical.QuadPart > MAXULONG ||
        (ULONGLONG)Length - 1U >
            MAXULONG - (ULONGLONG)logical.QuadPart) {
        Context->Adapter->DmaOperations->FreeCommonBuffer(
            Context->Adapter, Length, logical, buffer, FALSE);
        return STATUS_NOT_SUPPORTED;
    }

    RtlZeroMemory(buffer, Length);
    Context->TableVirtual = buffer;
    Context->TableLogical = logical;
    Context->TableLength = Length;
    return STATUS_SUCCESS;
}

VOID
LecDmaReleaseAdapterContext(
    _In_opt_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ BOOLEAN ProvenIdle)
{
    if (Context == NULL) {
        return;
    }

    if (!ProvenIdle || Context->Quarantined) {
        /*
         * DMA may still read the descriptor table. Dropping a common
         * buffer's IOMMU mapping would be unsafe even if CPU pages remain
         * pinned. Quarantine context independent of the device extension.
         */
        Context->Quarantined = TRUE;
        return;
    }

    if (Context->TableVirtual != NULL) {
        Context->Adapter->DmaOperations->FreeCommonBuffer(
            Context->Adapter,
            Context->TableLength,
            Context->TableLogical,
            Context->TableVirtual,
            FALSE);
    }

    if (Context->Adapter != NULL) {
        Context->Adapter->DmaOperations->PutDmaAdapter(Context->Adapter);
    }

    ExFreePoolWithTag(Context, LECS65_TAG);
}
