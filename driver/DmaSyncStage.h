#pragma once

#if defined(LECS65_SG_HOST_TEST)
#include "../tests/dry/sg-stage-mock.h"
#else
#include "LecS65Drv.h"
#endif

/*
 * INACTIVE v3 synchronous no-callback staging only. No hardware launch
 * function exists here. The parent owns its DMA adapter, the PDO and the
 * pinned MDL buffers for as long as outstanding mappings remain.
 *
 * Stage allocations are never exposed as raw pointers. A monotonically
 * increasing token plus the parent lock protects copy versus release.
 * The parent is nonpaged and must live until STOP + quiescence.
 */
struct _LECS65_SG_SYNC_STAGE;
typedef struct _LECS65_SG_SYNC_OWNER {
    PDMA_ADAPTER Adapter;
    PDEVICE_OBJECT DeviceObject;
    KSPIN_LOCK Lock;
    struct _LECS65_SG_SYNC_STAGE* Mappings;
    ULONGLONG NextToken;
    ULONG Outstanding;
    BOOLEAN Stopping;
} LECS65_SG_SYNC_OWNER, *PLECS65_SG_SYNC_OWNER;

typedef struct _LECS65_SG_SYNC_STAGE {
    struct _LECS65_SG_SYNC_STAGE* Next;
    PLECS65_SG_SYNC_OWNER Parent;
    PMDL MdlChain;                   /* borrowed and pinned by parent */
    PSCATTER_GATHER_LIST List;       /* v3 allocation, no device launch */
    ULONG RequestedLength;
    ULONGLONG Token;
    ULONG_PTR TransferContext[
        (DMA_TRANSFER_CONTEXT_SIZE_V1 + sizeof(ULONG_PTR) - 1U) /
        sizeof(ULONG_PTR)];
} LECS65_SG_SYNC_STAGE, *PLECS65_SG_SYNC_STAGE;

VOID LecSgSyncOwnerInit(
    _Out_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject);

/* STOP is nonblocking. BUSY means retain parent, adapter and MDLs. */
NTSTATUS LecSgSyncOwnerStop(_Inout_ PLECS65_SG_SYNC_OWNER Owner);
BOOLEAN LecSgSyncOwnerCanTeardown(_Inout_ PLECS65_SG_SYNC_OWNER Owner);

/* PASSIVE_LEVEL; v3 adapter and pinned MDL chain, may span 32-MiB MDLs. */
NTSTATUS LecSgSyncMapNoLaunch(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Out_ ULONGLONG* Token);

/*
 * Copies under the parent lock into caller-owned NONPAGED memory.
 * A concurrent release can never invalidate the SG list while copying.
 */
NTSTATUS LecSgSyncCopySegments(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ ULONGLONG Token,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied);

/*
 * Removes the token before unmapping, so repeated or concurrent releases
 * cannot dereference a freed stage. A token does NOT authorize DMA launch.
 * Releases only mappings from this strictly no-launch interface.
 */
NTSTATUS LecSgSyncReleaseNoLaunch(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _In_ ULONGLONG Token);
