#pragma once
#include "LecS65Drv.h"

/*
 * Inactive staging API for transitioning from PFN-derived CPU addresses to
 * Windows DMA-adapter logical addresses. Do not call from acquisition until
 * all transfer/map/callback lifetime requirements are met.
 *
 * Context allocation is independent of the FDO. On an unproven DMA idle
 * condition the context and any common-buffer mapping MUST survive REMOVE.
 */
typedef struct _LECS65_DMA_ADAPTER_CONTEXT {
    PDMA_ADAPTER Adapter;
    ULONG NumberOfMapRegisters;
    PVOID TableVirtual;
    PHYSICAL_ADDRESS TableLogical;
    ULONG TableLength;
    BOOLEAN Quarantined;
} LECS65_DMA_ADAPTER_CONTEXT, *PLECS65_DMA_ADAPTER_CONTEXT;

NTSTATUS LecDmaCreateAdapterContext(
    _In_ PDEVICE_OBJECT PhysicalDeviceObject,
    _In_ ULONG MaximumTransferBytes,
    _Outptr_ PLECS65_DMA_ADAPTER_CONTEXT* Result);

NTSTATUS LecDmaAllocateCommonTable(
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ ULONG Length);

/*
 * ProvenIdle may become TRUE only after a separately proven hardware and
 * mapping-domain quiesce. FALSE permanently retains the adapter + table.
 * No caller in the live path currently supplies that guarantee.
 */
VOID LecDmaReleaseAdapterContext(
    _In_opt_ PLECS65_DMA_ADAPTER_CONTEXT Context,
    _In_ BOOLEAN ProvenIdle);
