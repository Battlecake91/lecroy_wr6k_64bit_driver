#include "DmaPnpPublication.h"

#if defined(LECS65_PNP_PUBLICATION_TEST_HOOKS)
NTSTATUS FakeLecDmaPnpQuarantineUnknownActive(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage);
#define LecDmaPnpPublicationQuarantineParent \
    FakeLecDmaPnpQuarantineUnknownActive
#else
#define LecDmaPnpPublicationQuarantineParent \
    LecDmaPnpQuarantineUnknownActive
#endif

static BOOLEAN
LecDmaPnpPublicationIsQuarantined(
    _Inout_ PLECS65_DMA_PNP_STAGE Parent)
{
    LECS65_DMA_PNP_SNAPSHOT snapshot;

    RtlZeroMemory(&snapshot, sizeof(snapshot));
    LecDmaPnpSnapshot(Parent, &snapshot);
    return (BOOLEAN)(snapshot.UnknownActive ||
        snapshot.State == LecDmaPnpQuarantined);
}

NTSTATUS
LecDmaPnpPublicationCreate(
    _In_ PDEVICE_OBJECT PhysicalDeviceObject,
    _Inout_ PLECS65_DMA_PNP_PUBLICATION* Publication)
{
    PLECS65_DMA_PNP_PUBLICATION created;
    NTSTATUS status;

    if (PhysicalDeviceObject == NULL || Publication == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (*Publication != NULL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    created = (PLECS65_DMA_PNP_PUBLICATION)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*created), LECS65_TAG);
    if (created == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(created, sizeof(*created));
    KeInitializeSpinLock(&created->Lock);
    KeInitializeEvent(&created->IdleEvent, NotificationEvent, TRUE);
    InitializeListHead(&created->RetainedLegacyTransfers);
    created->State = LecDmaPnpPublicationPublished;

    status = LecDmaPnpStageCreate(PhysicalDeviceObject, &created->Parent);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(created, LECS65_TAG);
        return status;
    }

    ObReferenceObject(PhysicalDeviceObject);
    created->PhysicalDeviceObject = PhysicalDeviceObject;
    *Publication = created;
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaPnpPublicationAcquire(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLECS65_DMA_PNP_PUBLICATION_REFERENCE Reference)
{
    KIRQL irql;
    NTSTATUS status = STATUS_DELETE_PENDING;

    if (Publication == NULL || Reference == NULL ||
        InterlockedCompareExchange(&Reference->State, 1, 0) != 0) {
        return STATUS_INVALID_PARAMETER;
    }
    Reference->Publication = NULL;
    Reference->Parent = NULL;

    KeAcquireSpinLock(&Publication->Lock, &irql);
    if (Publication->State == LecDmaPnpPublicationPublished &&
        Publication->Parent != NULL &&
        Publication->ActiveUsers != MAXULONG) {
        if (Publication->ActiveUsers++ == 0) {
            KeClearEvent(&Publication->IdleEvent);
        }
        Reference->Publication = Publication;
        Reference->Parent = Publication->Parent;
        status = STATUS_SUCCESS;
    }
    KeReleaseSpinLock(&Publication->Lock, irql);
    if (!NT_SUCCESS(status)) {
        Reference->State = 0;
    }
    return status;
}

NTSTATUS
LecDmaPnpPublicationRelease(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLECS65_DMA_PNP_PUBLICATION_REFERENCE Reference)
{
    KIRQL irql;
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    if (Publication == NULL || Reference == NULL ||
        Reference->Publication != Publication || Reference->Parent == NULL ||
        InterlockedCompareExchange(&Reference->State, 2, 1) != 1) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Publication->Lock, &irql);
    if (Publication->Parent == Reference->Parent &&
        Publication->ActiveUsers != 0) {
        if (--Publication->ActiveUsers == 0) {
            KeSetEvent(&Publication->IdleEvent, IO_NO_INCREMENT, FALSE);
        }
        status = STATUS_SUCCESS;
    }
    KeReleaseSpinLock(&Publication->Lock, irql);
    return status;
}

NTSTATUS
LecDmaPnpPublicationQuarantine(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication)
{
    LECS65_DMA_PNP_PUBLICATION_REFERENCE reference = { 0 };
    NTSTATUS status;

    status = LecDmaPnpPublicationAcquire(Publication, &reference);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = LecDmaPnpPublicationQuarantineParent(reference.Parent);
    (void)LecDmaPnpPublicationRelease(Publication, &reference);
    return status;
}

NTSTATUS
LecDmaPnpPublicationRetainLegacyTransfer(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Inout_ PLIST_ENTRY TransferLink,
    _Inout_ volatile LONG* OwnershipState)
{
    LECS65_DMA_PNP_PUBLICATION_REFERENCE reference = { 0 };
    KIRQL irql;
    NTSTATUS status;

    if (Publication == NULL || TransferLink == NULL ||
        OwnershipState == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (InterlockedCompareExchange(
            OwnershipState,
            LECS65_TRANSFER_QUARANTINE_TRANSFERRING,
            LECS65_TRANSFER_QUARANTINE_FDO) !=
        LECS65_TRANSFER_QUARANTINE_FDO) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    status = LecDmaPnpPublicationAcquire(Publication, &reference);
    if (!NT_SUCCESS(status)) {
        (void)InterlockedExchange(
            OwnershipState, LECS65_TRANSFER_QUARANTINE_FDO);
        return status;
    }

    /* Commit independent memory ownership before parent notification. */
    InitializeListHead(TransferLink);
    KeAcquireSpinLock(&Publication->Lock, &irql);
    InsertTailList(&Publication->RetainedLegacyTransfers, TransferLink);
    ++Publication->RetainedLegacyTransferCount;
    (void)InterlockedExchange(
        OwnershipState, LECS65_TRANSFER_QUARANTINE_PUBLICATION);
    KeReleaseSpinLock(&Publication->Lock, irql);

    status = LecDmaPnpPublicationQuarantineParent(reference.Parent);
    (void)LecDmaPnpPublicationRelease(Publication, &reference);
    return status;
}

NTSTATUS
LecDmaPnpPublicationNotifyTeardown(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _In_ LECS65_DMA_PNP_TEARDOWN_REASON Reason)
{
    LECS65_DMA_PNP_PUBLICATION_REFERENCE reference = { 0 };
    NTSTATUS status;

    if (Reason != LecDmaPnpTeardownStop &&
        Reason != LecDmaPnpTeardownSurprise) {
        return STATUS_INVALID_PARAMETER;
    }

    status = LecDmaPnpPublicationAcquire(Publication, &reference);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = LecDmaPnpBeginTeardown(reference.Parent, Reason);
    if (NT_SUCCESS(status) && Reason == LecDmaPnpTeardownSurprise) {
        status = LecDmaPnpFinishTeardownNoLaunch(reference.Parent);
    }
    (void)LecDmaPnpPublicationRelease(Publication, &reference);
    return status;
}

NTSTATUS
LecDmaPnpPublicationRemove(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Out_ PBOOLEAN Retained)
{
    PLECS65_DMA_PNP_STAGE parent;
    KIRQL irql;
    ULONG retainedTransfers;
    NTSTATUS status;

    if (Publication == NULL || Retained == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Retained = FALSE;

    KeAcquireSpinLock(&Publication->Lock, &irql);
    if (Publication->State != LecDmaPnpPublicationPublished ||
        Publication->Parent == NULL) {
        KeReleaseSpinLock(&Publication->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Publication->State = LecDmaPnpPublicationUnpublishing;
    parent = Publication->Parent;
    KeReleaseSpinLock(&Publication->Lock, irql);

    (void)KeWaitForSingleObject(
        &Publication->IdleEvent, Executive, KernelMode, FALSE, NULL);

    KeAcquireSpinLock(&Publication->Lock, &irql);
    retainedTransfers = Publication->RetainedLegacyTransferCount;
    KeReleaseSpinLock(&Publication->Lock, irql);

    if (retainedTransfers != 0 ||
        LecDmaPnpPublicationIsQuarantined(parent)) {
        KeAcquireSpinLock(&Publication->Lock, &irql);
        Publication->State = LecDmaPnpPublicationRetained;
        KeReleaseSpinLock(&Publication->Lock, irql);
        *Retained = TRUE;
        return STATUS_DEVICE_BUSY;
    }

    status = LecDmaPnpBeginTeardown(parent, LecDmaPnpTeardownRemove);
    if (NT_SUCCESS(status)) {
        status = LecDmaPnpFinishTeardownNoLaunch(parent);
    }
    if (NT_SUCCESS(status)) {
        status = LecDmaPnpStageDestroy(parent);
    }
    if (!NT_SUCCESS(status)) {
        (void)LecDmaPnpQuarantineUnknownActive(parent);
        KeAcquireSpinLock(&Publication->Lock, &irql);
        Publication->State = LecDmaPnpPublicationRetained;
        KeReleaseSpinLock(&Publication->Lock, irql);
        *Retained = TRUE;
        return status;
    }

    KeAcquireSpinLock(&Publication->Lock, &irql);
    Publication->Parent = NULL;
    KeReleaseSpinLock(&Publication->Lock, irql);
    ObDereferenceObject(Publication->PhysicalDeviceObject);
    Publication->PhysicalDeviceObject = NULL;
    ExFreePoolWithTag(Publication, LECS65_TAG);
    return STATUS_SUCCESS;
}

VOID
LecDmaPnpPublicationSnapshot(
    _Inout_ PLECS65_DMA_PNP_PUBLICATION Publication,
    _Out_ PLECS65_DMA_PNP_PUBLICATION_SNAPSHOT Snapshot)
{
    KIRQL irql;

    if (Publication == NULL || Snapshot == NULL) {
        return;
    }
    KeAcquireSpinLock(&Publication->Lock, &irql);
    Snapshot->State = Publication->State;
    Snapshot->ActiveUsers = Publication->ActiveUsers;
    Snapshot->RetainedLegacyTransferCount =
        Publication->RetainedLegacyTransferCount;
    Snapshot->HasParent = (BOOLEAN)(Publication->Parent != NULL);
    KeReleaseSpinLock(&Publication->Lock, irql);
}
