#pragma once

#include "DmaPnpStage.h"

/*
 * Live-PnP lifetime anchor for the otherwise inactive DMA staging parent.
 *
 * Creation allocates only nonpaged software objects. It never calls START,
 * requests a DMA adapter, creates a mapping or touches hardware. The FDO owns
 * the published pointer while its remove lock admits callers. REMOVE must
 * unpublish the pointer and complete remove-lock rundown before calling
 * LecDmaPnpPublicationRemove.
 */
typedef enum _LECS65_DMA_PNP_PUBLICATION_STATE {
    LecDmaPnpPublicationPublished = 0,
    LecDmaPnpPublicationUnpublishing,
    LecDmaPnpPublicationRetained
} LECS65_DMA_PNP_PUBLICATION_STATE;

typedef struct _LECS65_DMA_PNP_PUBLICATION {
    KSPIN_LOCK Lock;
    KEVENT IdleEvent;
    PDEVICE_OBJECT PhysicalDeviceObject;
    PLECS65_DMA_PNP_STAGE Parent;
    LIST_ENTRY RetainedLegacyTransfers;
    ULONG ActiveUsers;
    ULONG RetainedLegacyTransferCount;
    LECS65_DMA_PNP_PUBLICATION_STATE State;
} LECS65_DMA_PNP_PUBLICATION, *PLECS65_DMA_PNP_PUBLICATION;

typedef struct _LECS65_DMA_PNP_PUBLICATION_SNAPSHOT {
    LECS65_DMA_PNP_PUBLICATION_STATE State;
    ULONG ActiveUsers;
    ULONG RetainedLegacyTransferCount;
    BOOLEAN HasParent;
} LECS65_DMA_PNP_PUBLICATION_SNAPSHOT,
    *PLECS65_DMA_PNP_PUBLICATION_SNAPSHOT;

/* Caller-owned, single-use and non-copyable while active. */
typedef struct _LECS65_DMA_PNP_PUBLICATION_REFERENCE {
    PLECS65_DMA_PNP_PUBLICATION Publication;
    PLECS65_DMA_PNP_STAGE Parent;
    volatile LONG State;
} LECS65_DMA_PNP_PUBLICATION_REFERENCE,
    *PLECS65_DMA_PNP_PUBLICATION_REFERENCE;

NTSTATUS LecDmaPnpPublicationCreate(
    _In_ PDEVICE_OBJECT PhysicalDeviceObject,
    _Outptr_ PLECS65_DMA_PNP_PUBLICATION* Publication);

/* Caller must already own the FDO remove-lock reference. */
NTSTATUS LecDmaPnpPublicationAcquire(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLECS65_DMA_PNP_PUBLICATION_REFERENCE Reference);

NTSTATUS LecDmaPnpPublicationRelease(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLECS65_DMA_PNP_PUBLICATION_REFERENCE Reference);

/* Permanent notification; safe at DISPATCH_LEVEL and never waits. */
NTSTATUS LecDmaPnpPublicationQuarantine(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication);

/*
 * Transfers passed here are already detached from the FDO transfer list.
 * Their Link becomes owned by Publication and is deliberately never freed.
 */
NTSTATUS LecDmaPnpPublicationRetainLegacyTransfer(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLIST_ENTRY TransferLink);

/* Software-only mirroring after real STOP/SURPRISE rundown has completed. */
NTSTATUS LecDmaPnpPublicationNotifyTeardown(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _In_ LECS65_DMA_PNP_TEARDOWN_REASON Reason);

/*
 * PASSIVE_LEVEL, after the pointer is unpublished and remove-lock rundown is
 * complete. Retained=TRUE means unknown ownership survives FDO deletion and
 * the publication object plus parent must intentionally remain allocated.
 */
NTSTATUS LecDmaPnpPublicationRemove(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Out_ PBOOLEAN Retained);

VOID LecDmaPnpPublicationSnapshot(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Out_ PLECS65_DMA_PNP_PUBLICATION_SNAPSHOT Snapshot);
