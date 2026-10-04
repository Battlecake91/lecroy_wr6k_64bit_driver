#pragma once
#if defined(LECS65_SG_HOST_TEST)
#include "../tests/dry/sg-stage-mock.h"
#else
#include "LecS65Drv.h"
#endif
#include "DmaMappingOwner.h"

/*
 * Unreachable staging-only WDM SG bridge. Caller MUST retain the DMA
 * adapter and locked MDL, plus the physical device and remove ownership,
 * until all callbacks are independently known to have retired.
 * FDO object references alone do NOT serialize PnP teardown.
 *
 * No release path may free the stage itself while other callers can
 * reference it: it remains a tombstone until a future owner-wide rundown
 * protocol has been implemented.
 */
typedef struct _LECS65_SG_STAGE {
    PDMA_ADAPTER Adapter;             /* Borrowed, not reference-counted. */
    PDEVICE_OBJECT DeviceObject;     /* Object-ref retained on submission. */
    PMDL SourceMdl;                  /* Borrowed; owner must keep pinned. */
    PSCATTER_GATHER_LIST List;
    KSPIN_LOCK Lock;
    BOOLEAN CallbackComplete;        /* Notification, not callback retirement. */
    BOOLEAN SubmissionReturned;
    BOOLEAN Unsafe;
    BOOLEAN PutStarted;
    BOOLEAN WriteToDevice;
    BOOLEAN Closing;
    ULONG CopiedSegments;
    LECS65_MAPPING_OWNER Owner;
} LECS65_SG_STAGE, *PLECS65_SG_STAGE;

/* Staging only. No caller from PnP, IOCTL or acquisition is allowed. */
NTSTATUS LecSgStageMap(
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMDL LockedMdl,
    _In_ ULONG Length,
    _Outptr_ PLECS65_SG_STAGE* Result);

/*
 * A bounded SNAPSHOT under the stage lock, never a borrowed SG pointer.
 * Copies real device-logical elements to caller storage. No target buffer
 * access after return without separately retaining its own allocation.
 */
NTSTATUS LecSgStageCopySegments(
    _Inout_ PLECS65_SG_STAGE Stage,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied);

/*
 * Currently fail-closed: no reliable callback-retirement proof or
 * PnP/MDL/adapter rundown contract exists. Even ProvenIdle=TRUE cannot
 * authorize PutScatterGatherList or free a stage. A FALSE argument
 * permanently quarantines the mapping.
 */
NTSTATUS LecSgStageRelease(
    _Inout_ PLECS65_SG_STAGE Stage,
    _In_ BOOLEAN ProvenIdle);

/* Staged for software-only ownership transitions; no MMIO. */
BOOLEAN LecSgStageMarkLaunched(_Inout_ PLECS65_SG_STAGE Stage);
BOOLEAN LecSgStageMarkIdleProved(_Inout_ PLECS65_SG_STAGE Stage);
