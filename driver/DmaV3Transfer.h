#pragma once

#include "DmaAdapterStage.h"
#include "DmaLayout.h"

/*
 * Inactive explicit WDM-v3 transfer-map owner.
 *
 * This is a no-launch API: it allocates a channel, maps one complete transfer
 * and releases it only through FlushAdapterBuffersEx + FreeAdapterObject.  It
 * never writes MMIO and never treats IRQ completion as physical idle.  A
 * future caller must transfer this owner into permanent quarantine after any
 * possible GO unless independent physical-idle evidence exists.
 */
typedef enum _LECS65_DMA_V3_MAP_STATE {
    LecDmaV3MapEmpty = 0,
    LecDmaV3MapPreparing,
    LecDmaV3MapReady,
    LecDmaV3MapReleaseCommitted,
    LecDmaV3MapReleased,
    LecDmaV3MapQuarantined
} LECS65_DMA_V3_MAP_STATE;

typedef struct _LECS65_DMA_V3_TRANSFER {
    PLECS65_DMA_ADAPTER_CONTEXT AdapterContext;
    PMDL MdlChain;                 /* borrowed locked pages */
    PSCATTER_GATHER_LIST ScatterGather;
    PVOID MapRegisterBase;
    ULONG RequestedLength;
    ULONG MappedLength;
    ULONG ScatterGatherBytes;
    ULONG MapRegisterCount;
    ULONG DescriptorSlotCapacity;
    ULONG_PTR TransferContext[
        (DMA_TRANSFER_CONTEXT_SIZE_V1 + sizeof(ULONG_PTR) - 1U) /
        sizeof(ULONG_PTR)];
    LECS65_DMA_V3_MAP_STATE State;
    BOOLEAN ChannelAllocated;
    BOOLEAN MappingOwned;
    BOOLEAN AdapterOwnerClaimed;
    BOOLEAN QuarantineLatched;
} LECS65_DMA_V3_TRANSFER, *PLECS65_DMA_V3_TRANSFER;

VOID LecDmaV3TransferInitialize(_Out_ PLECS65_DMA_V3_TRANSFER Transfer);

/* PASSIVE_LEVEL entry; no caller lock may be held across these DMA DDIs. */
NTSTATUS LecDmaV3PrepareNoLaunch(
    _Inout_ PLECS65_DMA_V3_TRANSFER Transfer,
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT AdapterContext,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length);

/*
 * Encode the validated DMA logical addresses into the adapter-owned common
 * descriptor table. This remains a no-launch operation and does not publish
 * or commit the table to hardware.
 */
NTSTATUS LecDmaV3BuildCommonDescriptorsNoLaunch(
    _In_ PLECS65_DMA_V3_TRANSFER Transfer,
    _Out_ PULONG UsedEntries);

/* Never valid after possible GO unless a separate idle proof is supplied. */
NTSTATUS LecDmaV3ReleaseNeverLaunched(
    _Inout_ PLECS65_DMA_V3_TRANSFER Transfer);

VOID LecDmaV3Quarantine(_Inout_ PLECS65_DMA_V3_TRANSFER Transfer);
