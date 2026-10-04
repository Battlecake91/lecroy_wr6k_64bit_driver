#pragma once

#if defined(LECS65_SG_HOST_TEST)
#include "../tests/dry/sg-stage-mock.h"
#else
#include "LecS65Drv.h"
#endif

/*
 * INACTIVE bus-master DMA v3 no-callback staging.
 *
 * This is deliberately a NO-LAUNCH lifetime: no function can start
 * the device. Only no-launch mappings can be released. The owner of the
 * DMA adapter, MDL chain and PnP resources must keep them valid across
 * Map and Release; live PnP is NOT integrated.
 *
 * Unlike GetScatterGatherList, GetScatterGatherListEx with
 * DMA_SYNCHRONOUS_CALLBACK and NULL ExecutionRoutine never queues a
 * delayed callback. Failure cannot leave a pending mapping request.
 */
typedef struct _LECS65_SG_SYNC_STAGE {
    PDMA_ADAPTER Adapter;                /* borrowed from parent */
    PDEVICE_OBJECT DeviceObject;         /* referenced until released */
    PMDL MdlChain;                       /* pinned, borrowed from parent */
    PSCATTER_GATHER_LIST List;           /* owns v3 allocation */
    ULONG RequestedLength;
    ULONG_PTR TransferContext[
        (DMA_TRANSFER_CONTEXT_SIZE_V1 + sizeof(ULONG_PTR) - 1U) /
        sizeof(ULONG_PTR)];
} LECS65_SG_SYNC_STAGE, *PLECS65_SG_SYNC_STAGE;

/* PASSIVE_LEVEL; WDM v3 adapter and pinned MDL chain required. */
NTSTATUS LecSgSyncMapNoLaunch(
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Outptr_ PLECS65_SG_SYNC_STAGE* Result);

/*
 * Copy and validate whole SG mapping. Caller buffer must be nonpaged.
 * On any invalid layout no release is implied; no hardware can launch.
 */
NTSTATUS LecSgSyncCopySegments(
    _In_ PLECS65_SG_SYNC_STAGE Stage,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied);

/*
 * ONLY releases a proven no-launch v3 allocation using FreeAdapterObject.
 * Caller must serialize accesses and own stage pointer exclusively.
 * The pointer is set to NULL to prevent sequential double release.
 */
NTSTATUS LecSgSyncReleaseNoLaunch(
    _Inout_ PLECS65_SG_SYNC_STAGE* Stage);
