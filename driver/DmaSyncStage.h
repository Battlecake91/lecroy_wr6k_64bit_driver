#pragma once

#include "DmaAdapterStage.h"

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
typedef enum _LECS65_SG_SYNC_OWNER_STATE {
    LecSgSyncOwnerUnconstructed = 0,
    LecSgSyncOwnerConstructed,
    LecSgSyncOwnerInitializing,
    LecSgSyncOwnerActive,
    LecSgSyncOwnerStopping,
    LecSgSyncOwnerQuarantined,
    LecSgSyncOwnerDestroying,
    LecSgSyncOwnerDestroyed
} LECS65_SG_SYNC_OWNER_STATE;

typedef struct _LECS65_SG_SYNC_OWNER {
    PLECS65_DMA_ADAPTER_CONTEXT AdapterContext;
    KSPIN_LOCK Lock;
    struct _LECS65_SG_SYNC_STAGE* Mappings;
    ULONGLONG NextToken;
    ULONG Outstanding;
    ULONG ReleasesInFlight;
    ULONG DescriptorSlotCapacity;
    volatile LONG QuarantineRequested;
    BOOLEAN LateQuarantine;
    LECS65_SG_SYNC_OWNER_STATE State;
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

/*
 * Construct exactly once before publishing the caller-owned storage or
 * calling any other owner API. Construct does not inspect the prior bytes.
 * The storage and its lock must remain resident until all callers have
 * completed, including after LecSgSyncOwnerDestroy returns.
 */
VOID LecSgSyncOwnerConstruct(_Out_ PLECS65_SG_SYNC_OWNER Owner);

/* Attaches a constructed/destroyed owner; capacity comes from the table. */
NTSTATUS LecSgSyncOwnerInit(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _Inout_ PLECS65_DMA_ADAPTER_CONTEXT AdapterContext);

/* STOP is nonblocking. BUSY means retain parent, adapter and MDLs. */
NTSTATUS LecSgSyncOwnerStop(_Inout_ PLECS65_SG_SYNC_OWNER Owner);
BOOLEAN LecSgSyncOwnerCanTeardown(_Inout_ PLECS65_SG_SYNC_OWNER Owner);

/* Permanent fail-closed latch. Existing or future allocations are retained. */
NTSTATUS LecSgSyncOwnerQuarantine(
    _Inout_ PLECS65_SG_SYNC_OWNER Owner,
    _Out_opt_ PBOOLEAN ReleaseAlreadyCommitted);

/* Releases every published mapping because this API can never launch DMA. */
NTSTATUS LecSgSyncOwnerDrainNoLaunch(_Inout_ PLECS65_SG_SYNC_OWNER Owner);

/* Requires STOP, no in-flight submission/copy/release, and a drained owner. */
NTSTATUS LecSgSyncOwnerDestroy(_Inout_ PLECS65_SG_SYNC_OWNER Owner);

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
