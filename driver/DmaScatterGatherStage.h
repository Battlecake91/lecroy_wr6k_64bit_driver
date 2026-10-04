#pragma once
#include "LecS65Drv.h"
#include "DmaMappingOwner.h"

/*
 * INACTIVE WDM SG prototype: never called by live acquisition.
 * Holds its own referenced DeviceObject across an asynchronous callback.
 * The parent DMA_ADAPTER must outlive the entire request.
 * The caller serializes all query/release operations and PnP ownership.
 */
typedef struct _LECS65_SG_STAGE {
    PDMA_ADAPTER Adapter;            /* Borrowed; parent must outlive us. */
    PDEVICE_OBJECT DeviceObject;    /* Object reference owned by stage. */
    PSCATTER_GATHER_LIST List;
    KSPIN_LOCK Lock;
    BOOLEAN CallbackComplete;
    BOOLEAN Submitted;
    BOOLEAN Unsafe;
    BOOLEAN PutStarted;
    BOOLEAN WriteToDevice;
    LECS65_MAPPING_OWNER Owner;
} LECS65_SG_STAGE, *PLECS65_SG_STAGE;

/*
 * Stage the DMA mapping for exactly one already-locked MDL at PASSIVE_LEVEL.
 * GetScatterGatherList may call back later at DISPATCH_LEVEL. No hardware
 * launch is allowed from this staging callback.
 */
NTSTATUS LecSgStageMap(
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMDL LockedMdl,
    _In_ ULONG Length,
    _Outptr_ PLECS65_SG_STAGE* Result);

/*
 * Returns a borrowed list, not proof of DMA idle or ownership transfer.
 * A caller must serialize the entire read/use interval with release.
 */
BOOLEAN LecSgStagePeek(
    _Inout_ PLECS65_SG_STAGE Stage,
    _Outptr_result_maybenull_ PSCATTER_GATHER_LIST* List);

/* On FALSE or absent callback, indefinitely retain the mapping. */
NTSTATUS LecSgStageRelease(
    _Inout_ PLECS65_SG_STAGE Stage,
    _In_ BOOLEAN ProvenIdle);

/* State transitions called by future serialized hardware-owner code only. */
BOOLEAN LecSgStageMarkLaunched(_Inout_ PLECS65_SG_STAGE Stage);
BOOLEAN LecSgStageMarkIdleProved(_Inout_ PLECS65_SG_STAGE Stage);
