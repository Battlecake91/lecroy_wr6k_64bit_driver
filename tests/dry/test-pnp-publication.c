/* Actual live-PnP publication layer under host-only WDM primitives. */
#include <stdio.h>
#include "../../driver/DmaPnpPublication.h"

static unsigned passed, failed;
static volatile LONG poolOutstanding;
static volatile LONG allocationAttempt;
static volatile LONG failAllocationAttempt;
static volatile LONG quarantineMode;
static volatile LONG quarantineEntered;
static HANDLE quarantineContinue;

#define QUARANTINE_NORMAL 0L
#define QUARANTINE_FAIL   1L
#define QUARANTINE_BLOCK  2L

NTSTATUS FakeLecDmaPnpQuarantineUnknownActive(
    PLECS65_DMA_PNP_STAGE stage)
{
    LONG mode = InterlockedCompareExchange(
        &quarantineMode, QUARANTINE_NORMAL, QUARANTINE_NORMAL);

    if (mode == QUARANTINE_FAIL) {
        return STATUS_INTERNAL_ERROR;
    }
    if (mode == QUARANTINE_BLOCK) {
        InterlockedExchange(&quarantineEntered, 1);
        WaitForSingleObject(quarantineContinue, INFINITE);
    }
    return LecDmaPnpQuarantineUnknownActive(stage);
}

void* FakeExAllocatePool2(ULONG flags, size_t size, ULONG tag)
{
    LONG attempt;
    void* allocation;

    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(tag);
    attempt = InterlockedIncrement(&allocationAttempt);
    if (attempt == failAllocationAttempt) {
        return NULL;
    }
    allocation = malloc(size);
    if (allocation != NULL) {
        InterlockedIncrement(&poolOutstanding);
    }
    return allocation;
}

VOID FakeExFreePoolWithTag(void* allocation, ULONG tag)
{
    UNREFERENCED_PARAMETER(tag);
    if (allocation != NULL) {
        InterlockedDecrement(&poolOutstanding);
        free(allocation);
    }
}

PDMA_ADAPTER FakeIoGetDmaAdapter(
    PDEVICE_OBJECT deviceObject,
    PDEVICE_DESCRIPTION description,
    PULONG numberOfMapRegisters)
{
    UNREFERENCED_PARAMETER(deviceObject);
    UNREFERENCED_PARAMETER(description);
    UNREFERENCED_PARAMETER(numberOfMapRegisters);
    return NULL;
}

static void check(const char* label, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (ok) {
        ++passed;
    }
    else {
        ++failed;
    }
}

typedef struct _REMOVE_THREAD {
    PLECS65_DMA_PNP_PUBLICATION Publication;
    NTSTATUS Status;
    BOOLEAN Retained;
} REMOVE_THREAD;

typedef struct _RETAIN_THREAD {
    PLECS65_DMA_PNP_PUBLICATION Publication;
    PLIST_ENTRY TransferLink;
    volatile LONG* OwnershipState;
    NTSTATUS Status;
} RETAIN_THREAD;

static DWORD WINAPI concurrentRemove(void* argument)
{
    REMOVE_THREAD* remove = (REMOVE_THREAD*)argument;
    remove->Status = LecDmaPnpPublicationRemove(
        remove->Publication, &remove->Retained);
    return 0;
}

static DWORD WINAPI concurrentRetain(void* argument)
{
    RETAIN_THREAD* retain = (RETAIN_THREAD*)argument;
    retain->Status = LecDmaPnpPublicationRetainLegacyTransfer(
        retain->Publication,
        retain->TransferLink,
        retain->OwnershipState);
    return 0;
}

static int waitForPublicationState(
    PLECS65_DMA_PNP_PUBLICATION publication,
    LECS65_DMA_PNP_PUBLICATION_STATE expected)
{
    unsigned attempt;

    for (attempt = 0; attempt < 5000; ++attempt) {
        LECS65_DMA_PNP_PUBLICATION_SNAPSHOT snapshot;
        LecDmaPnpPublicationSnapshot(publication, &snapshot);
        if (snapshot.State == expected) {
            return 1;
        }
        Sleep(1);
    }
    return 0;
}

int main(void)
{
    FAKE_DEVICE device;
    PLECS65_DMA_PNP_PUBLICATION publication = NULL;
    PLECS65_DMA_PNP_STAGE parent = NULL;
    LECS65_DMA_PNP_PUBLICATION_REFERENCE reference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE peerReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE rejectedReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE staleReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE stopReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE surpriseReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_REFERENCE quarantineReference = { 0 };
    LECS65_DMA_PNP_PUBLICATION_SNAPSHOT publicationSnapshot;
    LECS65_DMA_PNP_SNAPSHOT parentSnapshot;
    LIST_ENTRY retainedTransfer;
    LIST_ENTRY rejectedTransfer;
    LIST_ENTRY failedNotificationTransfer;
    LIST_ENTRY parallelTransfer;
    volatile LONG retainedOwnership = LECS65_TRANSFER_QUARANTINE_FDO;
    volatile LONG rejectedOwnership = LECS65_TRANSFER_QUARANTINE_FDO;
    volatile LONG failedNotificationOwnership =
        LECS65_TRANSFER_QUARANTINE_FDO;
    volatile LONG parallelOwnership = LECS65_TRANSFER_QUARANTINE_FDO;
    REMOVE_THREAD remove;
    RETAIN_THREAD retain;
    HANDLE worker;
    HANDLE retainWorker;
    NTSTATUS status;

    (void)setvbuf(stdout, NULL, _IONBF, 0);
    memset(&device, 0, sizeof(device));

    failAllocationAttempt = 1;
    check("publication factory reports wrapper allocation failure",
        LecDmaPnpPublicationCreate(&device, &publication) ==
            STATUS_INSUFFICIENT_RESOURCES &&
        publication == NULL && poolOutstanding == 0);

    allocationAttempt = 0;
    failAllocationAttempt = 2;
    check("publication factory rolls back parent allocation failure",
        LecDmaPnpPublicationCreate(&device, &publication) ==
            STATUS_INSUFFICIENT_RESOURCES &&
        publication == NULL && poolOutstanding == 0);

    allocationAttempt = 0;
    failAllocationAttempt = 0;
    check("publication creates one software-only parent",
        LecDmaPnpPublicationCreate(&device, &publication) ==
            STATUS_SUCCESS && publication != NULL &&
        poolOutstanding == 2 && device.References == 1);
    check("duplicate factory call preserves the published owner",
        LecDmaPnpPublicationCreate(&device, &publication) ==
            STATUS_INVALID_DEVICE_STATE &&
        publication != NULL && poolOutstanding == 2 &&
        device.References == 1);
    LecDmaPnpPublicationSnapshot(publication, &publicationSnapshot);
    check("fresh publication exposes no adapter or mapping",
        publicationSnapshot.State == LecDmaPnpPublicationPublished &&
        publicationSnapshot.ActiveUsers == 0 &&
        publicationSnapshot.RetainedLegacyTransferCount == 0 &&
        publicationSnapshot.HasParent);

    check("request acquires published parent lifetime",
        LecDmaPnpPublicationAcquire(publication, &reference) ==
            STATUS_SUCCESS && reference.Parent != NULL);
    parent = reference.Parent;
    check("second request owns an independent release reference",
        LecDmaPnpPublicationAcquire(publication, &peerReference) ==
            STATUS_SUCCESS && peerReference.Parent == parent);
    LecDmaPnpSnapshot(parent, &parentSnapshot);
    check("published parent remains inactive and stopped",
        parentSnapshot.State == LecDmaPnpStopped &&
        !parentSnapshot.HasAdapterContext &&
        !parentSnapshot.AdmissionOpen);

    remove.Publication = publication;
    remove.Status = STATUS_INTERNAL_ERROR;
    remove.Retained = TRUE;
    worker = CreateThread(NULL, 0, concurrentRemove, &remove, 0, NULL);
    check("REMOVE unpublishes before waiting for an active request",
        worker != NULL && waitForPublicationState(
            publication, LecDmaPnpPublicationUnpublishing));
    check("new request cannot acquire an unpublished parent",
        LecDmaPnpPublicationAcquire(publication, &rejectedReference) ==
            STATUS_DELETE_PENDING);
    InitializeListHead(&rejectedTransfer);
    check("retention racing unpublish fails without losing caller ownership",
        LecDmaPnpPublicationRetainLegacyTransfer(
            publication,
            &rejectedTransfer,
            &rejectedOwnership) == STATUS_DELETE_PENDING &&
        rejectedOwnership == LECS65_TRANSFER_QUARANTINE_FDO);
    staleReference.Publication = publication;
    staleReference.Parent = (PLECS65_DMA_PNP_STAGE)(ULONG_PTR)1;
    staleReference.State = 1;
    check("stale or duplicate release cannot decrement rundown",
        LecDmaPnpPublicationRelease(publication, &staleReference) ==
                STATUS_INVALID_PARAMETER);
    check("active request releases the exact published parent",
        LecDmaPnpPublicationRelease(publication, &reference) ==
            STATUS_SUCCESS &&
        LecDmaPnpPublicationRelease(publication, &reference) ==
            STATUS_INVALID_PARAMETER);
    LecDmaPnpPublicationSnapshot(publication, &publicationSnapshot);
    check("duplicate release cannot steal a concurrent request reference",
        publicationSnapshot.ActiveUsers == 1 &&
        LecDmaPnpPublicationRelease(publication, &peerReference) ==
            STATUS_SUCCESS);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    check("REMOVE destroys clean publication after rundown",
        remove.Status == STATUS_SUCCESS && !remove.Retained &&
        poolOutstanding == 0 && device.References == 0);
    publication = NULL;

    allocationAttempt = 0;
    check("STOP reuse fixture creates publication",
        LecDmaPnpPublicationCreate(&device, &publication) == STATUS_SUCCESS);
    check("repeated STOP keeps inactive parent reusable",
        LecDmaPnpPublicationNotifyTeardown(
            publication, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        LecDmaPnpPublicationNotifyTeardown(
            publication, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        LecDmaPnpPublicationAcquire(publication, &stopReference) ==
            STATUS_SUCCESS);
    parent = stopReference.Parent;
    LecDmaPnpSnapshot(parent, &parentSnapshot);
    check("STOP does not allocate or mutate staged ownership",
        parentSnapshot.State == LecDmaPnpStopped &&
        !parentSnapshot.HasAdapterContext && poolOutstanding == 2);
    (void)LecDmaPnpPublicationRelease(publication, &stopReference);
    check("surprise removal closes staged lifecycle without hardware work",
        LecDmaPnpPublicationNotifyTeardown(
            publication, LecDmaPnpTeardownSurprise) == STATUS_SUCCESS &&
        LecDmaPnpPublicationAcquire(publication, &surpriseReference) ==
            STATUS_SUCCESS);
    parent = surpriseReference.Parent;
    LecDmaPnpSnapshot(parent, &parentSnapshot);
    check("surprise state owns no adapter mapping and keeps PDO lifetime",
        parentSnapshot.State == LecDmaPnpSurpriseRemoved &&
        !parentSnapshot.HasAdapterContext && device.References == 1);
    (void)LecDmaPnpPublicationRelease(publication, &surpriseReference);
    check("duplicate surprise removal fails closed without state loss",
        LecDmaPnpPublicationNotifyTeardown(
            publication, LecDmaPnpTeardownSurprise) == STATUS_DEVICE_BUSY);
    check("STOP after surprise is idempotent and cannot reopen admission",
        LecDmaPnpPublicationNotifyTeardown(
            publication, LecDmaPnpTeardownStop) == STATUS_SUCCESS);
    remove.Publication = publication;
    remove.Retained = TRUE;
    remove.Status = LecDmaPnpPublicationRemove(
        publication, &remove.Retained);
    check("REMOVE after surprise frees publication exactly once",
        remove.Status == STATUS_SUCCESS && !remove.Retained &&
        poolOutstanding == 0 && device.References == 0);
    publication = NULL;

    allocationAttempt = 0;
    check("quarantine retention fixture creates publication",
        LecDmaPnpPublicationCreate(&device, &publication) == STATUS_SUCCESS &&
        LecDmaPnpPublicationAcquire(publication, &quarantineReference) ==
            STATUS_SUCCESS);
    parent = quarantineReference.Parent;
    InitializeListHead(&retainedTransfer);
    check("legacy transfer moves to independent quarantine owner",
        LecDmaPnpPublicationRetainLegacyTransfer(
            publication,
            &retainedTransfer,
            &retainedOwnership) == STATUS_SUCCESS &&
        retainedOwnership == LECS65_TRANSFER_QUARANTINE_PUBLICATION);
    check("duplicate legacy retention cannot corrupt the owner list",
        LecDmaPnpPublicationRetainLegacyTransfer(
            publication,
            &retainedTransfer,
            &retainedOwnership) == STATUS_INVALID_DEVICE_STATE);
    LecDmaPnpPublicationSnapshot(publication, &publicationSnapshot);
    check("retention records parent request and legacy ownership",
        publicationSnapshot.ActiveUsers == 1 &&
        publicationSnapshot.RetainedLegacyTransferCount == 1 &&
        poolOutstanding == 2);

    remove.Publication = publication;
    remove.Status = STATUS_INTERNAL_ERROR;
    remove.Retained = FALSE;
    worker = CreateThread(NULL, 0, concurrentRemove, &remove, 0, NULL);
    check("quarantined REMOVE still waits only for software users",
        worker != NULL && waitForPublicationState(
            publication, LecDmaPnpPublicationUnpublishing));
    check("unpublished quarantine rejects stale new acquisition",
        LecDmaPnpPublicationAcquire(publication, &rejectedReference) ==
            STATUS_DELETE_PENDING);
    check("outstanding request can retire without freeing quarantine",
        LecDmaPnpPublicationRelease(
            publication, &quarantineReference) == STATUS_SUCCESS);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    LecDmaPnpPublicationSnapshot(publication, &publicationSnapshot);
    LecDmaPnpSnapshot(publication->Parent, &parentSnapshot);
    check("unknown-active parent and MDL owner survive FDO removal",
        remove.Status == STATUS_DEVICE_BUSY && remove.Retained &&
        publicationSnapshot.State == LecDmaPnpPublicationRetained &&
        publicationSnapshot.ActiveUsers == 0 &&
        publicationSnapshot.RetainedLegacyTransferCount == 1 &&
        parentSnapshot.State == LecDmaPnpQuarantined &&
        parentSnapshot.UnknownActive && poolOutstanding == 2 &&
        device.References == 1);
    check("retained publication cannot be removed or acquired again",
        LecDmaPnpPublicationRemove(publication, &remove.Retained) ==
            STATUS_INVALID_DEVICE_STATE &&
        LecDmaPnpPublicationAcquire(publication, &rejectedReference) ==
            STATUS_DELETE_PENDING);

    publication = NULL;
    allocationAttempt = 0;
    check("notification-failure fixture creates publication",
        LecDmaPnpPublicationCreate(&device, &publication) == STATUS_SUCCESS);
    quarantineMode = QUARANTINE_FAIL;
    InitializeListHead(&failedNotificationTransfer);
    check("failed parent notification cannot undo transfer ownership",
        LecDmaPnpPublicationRetainLegacyTransfer(
            publication,
            &failedNotificationTransfer,
            &failedNotificationOwnership) == STATUS_INTERNAL_ERROR &&
        failedNotificationOwnership ==
            LECS65_TRANSFER_QUARANTINE_PUBLICATION);
    LecDmaPnpPublicationSnapshot(publication, &publicationSnapshot);
    remove.Retained = FALSE;
    remove.Status = LecDmaPnpPublicationRemove(publication, &remove.Retained);
    check("retained transfer survives failed parent notification and REMOVE",
        publicationSnapshot.RetainedLegacyTransferCount == 1 &&
        remove.Status == STATUS_DEVICE_BUSY && remove.Retained);

    publication = NULL;
    allocationAttempt = 0;
    quarantineMode = QUARANTINE_BLOCK;
    quarantineEntered = 0;
    quarantineContinue = CreateEvent(NULL, TRUE, FALSE, NULL);
    check("parallel-retention fixture creates publication and gate",
        quarantineContinue != NULL &&
        LecDmaPnpPublicationCreate(&device, &publication) == STATUS_SUCCESS);
    InitializeListHead(&parallelTransfer);
    retain.Publication = publication;
    retain.TransferLink = &parallelTransfer;
    retain.OwnershipState = &parallelOwnership;
    retain.Status = STATUS_INTERNAL_ERROR;
    retainWorker = CreateThread(NULL, 0, concurrentRetain, &retain, 0, NULL);
    if (retainWorker == NULL) {
        CloseHandle(quarantineContinue);
        fprintf(stderr, "Failed to create retention worker.\n");
        return 1;
    }
    {
        unsigned attempt;
        for (attempt = 0; attempt < 5000; ++attempt) {
            if (InterlockedCompareExchange(
                    &quarantineEntered, 0, 0) != 0) {
                break;
            }
            Sleep(1);
        }
        if (attempt == 5000) {
            SetEvent(quarantineContinue);
            WaitForSingleObject(retainWorker, INFINITE);
            CloseHandle(retainWorker);
            CloseHandle(quarantineContinue);
            fprintf(stderr, "Retention worker did not reach notification.\n");
            return 1;
        }
    }
    remove.Publication = publication;
    remove.Status = STATUS_INTERNAL_ERROR;
    remove.Retained = FALSE;
    worker = CreateThread(NULL, 0, concurrentRemove, &remove, 0, NULL);
    if (worker == NULL) {
        SetEvent(quarantineContinue);
        WaitForSingleObject(retainWorker, INFINITE);
        CloseHandle(retainWorker);
        CloseHandle(quarantineContinue);
        fprintf(stderr, "Failed to create REMOVE worker.\n");
        return 1;
    }
    check("REMOVE waits while committed retention notification is blocked",
        retainWorker != NULL && worker != NULL &&
        parallelOwnership == LECS65_TRANSFER_QUARANTINE_PUBLICATION &&
        waitForPublicationState(
            publication, LecDmaPnpPublicationUnpublishing));
    SetEvent(quarantineContinue);
    WaitForSingleObject(retainWorker, INFINITE);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(retainWorker);
    CloseHandle(worker);
    CloseHandle(quarantineContinue);
    check("parallel notification and REMOVE retain exact ownership",
        retain.Status == STATUS_SUCCESS &&
        remove.Status == STATUS_DEVICE_BUSY && remove.Retained &&
        parallelOwnership == LECS65_TRANSFER_QUARANTINE_PUBLICATION);

    status = failed == 0 ? STATUS_SUCCESS : STATUS_INTERNAL_ERROR;
    printf("PNP PUBLICATION: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return NT_SUCCESS(status) ? 0 : 1;
}
