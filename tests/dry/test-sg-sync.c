/* Actual inactive synchronous DMA v3 bridge under fake WDM DDIs. */
#include <stdio.h>
#include "../../driver/DmaSyncStage.h"

enum {
    MODE_NORMAL,
    MODE_GET_RESOURCE_FAILURE,
    MODE_INIT_FAILURE,
    MODE_SUCCESS_WITH_NULL_LIST,
    MODE_BLOCK_GET,
    MODE_BLOCK_FREE
};

static unsigned passed, failed;
static volatile LONG initCalls, getCalls, freeCalls;
static volatile LONG mainFreeCalls, otherFreeCalls;
static volatile LONG poolOutstanding, failNextPoolAllocation;
static volatile LONG badDdiArguments;
static volatile LONG putCalls, commonAllocateCalls, commonFreeCalls;
static volatile LONG mode;
static PDMA_ADAPTER ioAdapterResult;
static ULONG ioMapRegisters;
static DEVICE_DESCRIPTION lastDescription;
static BOOLEAN commonAvailable;
static PHYSICAL_ADDRESS commonLogical;
static char commonBuffer[4096];
static HANDLE getEntered, allowGet, freeEntered, allowFree;
static DMA_ADAPTER adapter, otherAdapter;
static DMA_OPERATIONS ops, otherOps;
static FAKE_DEVICE device, otherDevice;
static LECS65_DMA_ADAPTER_CONTEXT adapterContext, otherAdapterContext;
static LECS65_SG_SYNC_OWNER owner, otherOwner;
static MDL mdls[2];
static SCATTER_GATHER_LIST sg;
static char bytes[128];

void* FakeExAllocatePool2(ULONG flags, size_t size, ULONG tag)
{
    void* allocation;
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(tag);
    if (InterlockedExchange(&failNextPoolAllocation, 0) != 0) {
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

PDMA_ADAPTER
FakeIoGetDmaAdapter(
    PDEVICE_OBJECT deviceObject,
    PDEVICE_DESCRIPTION description,
    PULONG numberOfMapRegisters)
{
    UNREFERENCED_PARAMETER(deviceObject);
    lastDescription = *description;
    *numberOfMapRegisters = ioMapRegisters;
    return ioAdapterResult;
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

static NTSTATUS fakeInit(PDMA_ADAPTER dmaAdapter, void* context)
{
    UNREFERENCED_PARAMETER(dmaAdapter);
    InterlockedIncrement(&initCalls);
    if (mode == MODE_INIT_FAILURE) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memset(context, 0, DMA_TRANSFER_CONTEXT_SIZE_V1);
    return STATUS_SUCCESS;
}

static NTSTATUS
fakeGet(
    PDMA_ADAPTER dmaAdapter,
    PDEVICE_OBJECT deviceObject,
    void* transferContext,
    PMDL mdl,
    ULONGLONG offset,
    ULONG size,
    ULONG flags,
    PDRIVER_LIST_CONTROL callback,
    void* callbackContext,
    BOOLEAN writeToDevice,
    void* completion,
    void* completionContext,
    PSCATTER_GATHER_LIST* list)
{
    UNREFERENCED_PARAMETER(dmaAdapter);
    UNREFERENCED_PARAMETER(deviceObject);
    UNREFERENCED_PARAMETER(transferContext);
    UNREFERENCED_PARAMETER(mdl);
    UNREFERENCED_PARAMETER(callbackContext);
    UNREFERENCED_PARAMETER(completionContext);
    InterlockedIncrement(&getCalls);
    if (offset != 0 || size == 0 ||
        flags != DMA_SYNCHRONOUS_CALLBACK || callback != NULL ||
        writeToDevice != FALSE || completion != NULL || list == NULL) {
        InterlockedIncrement(&badDdiArguments);
    }
    if (mode == MODE_BLOCK_GET) {
        SetEvent(getEntered);
        WaitForSingleObject(allowGet, INFINITE);
    }
    if (mode == MODE_GET_RESOURCE_FAILURE) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    if (mode == MODE_SUCCESS_WITH_NULL_LIST) {
        *list = NULL;
        return STATUS_SUCCESS;
    }
    *list = &sg;
    return STATUS_SUCCESS;
}

static VOID fakeFree(PDMA_ADAPTER dmaAdapter, int action)
{
    if (action != DeallocateObject) {
        InterlockedIncrement(&badDdiArguments);
    }
    InterlockedIncrement(&freeCalls);
    if (dmaAdapter == &adapter) {
        InterlockedIncrement(&mainFreeCalls);
    }
    else if (dmaAdapter == &otherAdapter) {
        InterlockedIncrement(&otherFreeCalls);
    }
    if (mode == MODE_BLOCK_FREE) {
        SetEvent(freeEntered);
        WaitForSingleObject(allowFree, INFINITE);
    }
}

static VOID fakePut(PDMA_ADAPTER dmaAdapter)
{
    UNREFERENCED_PARAMETER(dmaAdapter);
    InterlockedIncrement(&putCalls);
}

static PVOID
fakeAllocateCommon(
    PDMA_ADAPTER dmaAdapter,
    ULONG length,
    LARGE_INTEGER* logical,
    BOOLEAN cacheEnabled)
{
    UNREFERENCED_PARAMETER(dmaAdapter);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(cacheEnabled);
    InterlockedIncrement(&commonAllocateCalls);
    if (!commonAvailable) {
        return NULL;
    }
    *logical = commonLogical;
    return commonBuffer;
}

static VOID
fakeFreeCommon(
    PDMA_ADAPTER dmaAdapter,
    ULONG length,
    LARGE_INTEGER logical,
    PVOID buffer,
    BOOLEAN cacheEnabled)
{
    UNREFERENCED_PARAMETER(dmaAdapter);
    UNREFERENCED_PARAMETER(length);
    UNREFERENCED_PARAMETER(logical);
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(cacheEnabled);
    InterlockedIncrement(&commonFreeCalls);
}

static void initOperations(PDMA_OPERATIONS operations)
{
    memset(operations, 0, sizeof(*operations));
    operations->Size = sizeof(*operations);
    operations->PutDmaAdapter = fakePut;
    operations->AllocateCommonBuffer = fakeAllocateCommon;
    operations->FreeCommonBuffer = fakeFreeCommon;
    operations->InitializeDmaTransferContext = fakeInit;
    operations->GetScatterGatherListEx = fakeGet;
    operations->FreeAdapterObject = fakeFree;
}

static void initAdapterContext(
    PLECS65_DMA_ADAPTER_CONTEXT context,
    PDMA_ADAPTER dmaAdapter,
    PDEVICE_OBJECT deviceObject)
{
    memset(context, 0, sizeof(*context));
    context->Adapter = dmaAdapter;
    context->PhysicalDeviceObject = deviceObject;
    context->NumberOfMapRegisters = 20000;
    KeInitializeSpinLock(&context->Lock);
}

static void setup(void)
{
    memset(&adapter, 0, sizeof(adapter));
    memset(&otherAdapter, 0, sizeof(otherAdapter));
    memset(&device, 0, sizeof(device));
    memset(&otherDevice, 0, sizeof(otherDevice));
    memset(mdls, 0, sizeof(mdls));
    memset(&sg, 0, sizeof(sg));
    initOperations(&ops);
    initOperations(&otherOps);
    adapter.DmaOperations = &ops;
    otherAdapter.DmaOperations = &otherOps;
    initAdapterContext(&adapterContext, &adapter, &device);
    initAdapterContext(&otherAdapterContext, &otherAdapter, &otherDevice);
    mdls[0].Va = bytes;
    mdls[0].Size = 128;
    mdls[0].MdlFlags = MDL_PAGES_LOCKED;
    sg.NumberOfElements = 2;
    sg.Elements[0].Address.QuadPart = 0x10000;
    sg.Elements[0].Length = 64;
    sg.Elements[1].Address.QuadPart = 0x20000;
    sg.Elements[1].Length = 64;
    getEntered = CreateEvent(NULL, TRUE, FALSE, NULL);
    allowGet = CreateEvent(NULL, TRUE, FALSE, NULL);
    freeEntered = CreateEvent(NULL, TRUE, FALSE, NULL);
    allowFree = CreateEvent(NULL, TRUE, FALSE, NULL);
    passed = failed = 0;
    initCalls = getCalls = freeCalls = 0;
    mainFreeCalls = otherFreeCalls = 0;
    poolOutstanding = failNextPoolAllocation = 0;
    badDdiArguments = 0;
    putCalls = commonAllocateCalls = commonFreeCalls = 0;
    ioAdapterResult = NULL;
    ioMapRegisters = 0;
    memset(&lastDescription, 0, sizeof(lastDescription));
    commonAvailable = FALSE;
    commonLogical.QuadPart = 0;
    mode = MODE_NORMAL;
}

typedef struct _COPY_RACE {
    PLECS65_SG_SYNC_OWNER Owner;
    ULONGLONG Token;
    volatile LONG Errors;
} COPY_RACE;

static DWORD WINAPI concurrentCopy(void* argument)
{
    COPY_RACE* race = (COPY_RACE*)argument;
    SCATTER_GATHER_ELEMENT elements[3];
    ULONG copied;
    unsigned i;

    for (i = 0; i < 3000; ++i) {
        NTSTATUS status = LecSgSyncCopySegments(
            race->Owner, race->Token, elements, 3, &copied);
        if (!(status == STATUS_INVALID_PARAMETER ||
            (status == STATUS_SUCCESS && copied == 2 &&
             elements[0].Address.QuadPart == 0x10000))) {
            InterlockedIncrement(&race->Errors);
        }
    }
    return 0;
}

typedef struct _RELEASE_THREAD {
    PLECS65_SG_SYNC_OWNER Owner;
    ULONGLONG Token;
    NTSTATUS Status;
} RELEASE_THREAD;

static DWORD WINAPI concurrentRelease(void* argument)
{
    RELEASE_THREAD* release = (RELEASE_THREAD*)argument;
    release->Status = LecSgSyncReleaseNoLaunch(
        release->Owner, release->Token);
    return 0;
}

typedef struct _MAP_THREAD {
    PLECS65_SG_SYNC_OWNER Owner;
    PMDL Mdl;
    ULONG Length;
    ULONGLONG Token;
    NTSTATUS Status;
} MAP_THREAD;

static DWORD WINAPI concurrentMap(void* argument)
{
    MAP_THREAD* map = (MAP_THREAD*)argument;
    map->Status = LecSgSyncMapNoLaunch(
        map->Owner, map->Mdl, map->Length, &map->Token);
    return 0;
}

int main(void)
{
    ULONGLONG id = 0, second = 0, otherId = 0;
    SCATTER_GATHER_ELEMENT elements[3];
    ULONG count = 0;
    NTSTATUS status;
    COPY_RACE copyRace;
    RELEASE_THREAD releaseA, releaseB;
    MAP_THREAD mapRace;
    HANDLE worker, worker2;
    LONG freesBefore;
    LECS65_SG_SYNC_OWNER rejectedOwner;
    LECS65_SG_SYNC_OWNER drainOwner;
    LECS65_DMA_ADAPTER_CONTEXT drainContext;
    DMA_ADAPTER drainAdapter;
    DMA_OPERATIONS drainOps;
    FAKE_DEVICE drainDevice;
    LECS65_DMA_ADAPTER_CONTEXT* createdContext = NULL;
    LECS65_DMA_ADAPTER_CONTEXT quarantineContext;
    FAKE_DEVICE createDevice;
    ULONG fullOperationsSize;
    LONG putsBefore;
    LONG commonFreesBefore;

    setup();
    memset(&createDevice, 0, sizeof(createDevice));
    check("adapter creation rejects unavailable v3 adapter",
        LecDmaCreateAdapterContext(&createDevice, 0x100000,
            &createdContext) == STATUS_NOT_SUPPORTED &&
        createdContext == NULL && poolOutstanding == 0);

    ioAdapterResult = &adapter;
    ioMapRegisters = 16;
    fullOperationsSize = ops.Size;
    ops.Size = (ULONG)offsetof(DMA_OPERATIONS, FreeAdapterObject);
    putsBefore = putCalls;
    check("short DMA_OPERATIONS table is rejected and put",
        LecDmaCreateAdapterContext(&createDevice, 0x100000,
            &createdContext) == STATUS_NOT_SUPPORTED &&
        createdContext == NULL && putCalls == putsBefore + 1 &&
        poolOutstanding == 0);
    ops.Size = fullOperationsSize;

    ioMapRegisters = 0;
    putsBefore = putCalls;
    check("zero map-register adapter capacity is rejected",
        LecDmaCreateAdapterContext(&createDevice, 0x100000,
            &createdContext) == STATUS_NOT_SUPPORTED &&
        putCalls == putsBefore + 1 && poolOutstanding == 0);

    ioMapRegisters = 16;
    check("valid adapter context owns adapter and PDO",
        LecDmaCreateAdapterContext(&createDevice, 0x100000,
            &createdContext) == STATUS_SUCCESS &&
        createdContext != NULL && createDevice.References == 1 &&
        createdContext->NumberOfMapRegisters == 16 &&
        lastDescription.Version == DEVICE_DESCRIPTION_VERSION3 &&
        lastDescription.Master && lastDescription.ScatterGather &&
        lastDescription.DmaAddressWidth == 32);
    check("common-buffer shortage leaves context reusable",
        LecDmaAllocateCommonTable(createdContext, 4096) ==
            STATUS_INSUFFICIENT_RESOURCES &&
        createdContext->TableVirtual == NULL);
    commonAvailable = TRUE;
    commonLogical.QuadPart = 0x100000000ULL;
    commonFreesBefore = commonFreeCalls;
    check("out-of-range common buffer is returned immediately",
        LecDmaAllocateCommonTable(createdContext, 4096) ==
            STATUS_NOT_SUPPORTED &&
        commonFreeCalls == commonFreesBefore + 1 &&
        createdContext->TableVirtual == NULL);
    commonLogical.QuadPart = 0x30000;
    check("valid common buffer is retained by adapter context",
        LecDmaAllocateCommonTable(createdContext, 4096) ==
            STATUS_SUCCESS &&
        createdContext->TableVirtual == commonBuffer &&
        createdContext->TableLength == 4096);
    putsBefore = putCalls;
    commonFreesBefore = commonFreeCalls;
    check("proven-idle adapter teardown frees table adapter and PDO",
        LecDmaReleaseAdapterContext(createdContext, TRUE) ==
            STATUS_SUCCESS &&
        commonFreeCalls == commonFreesBefore + 1 &&
        putCalls == putsBefore + 1 && createDevice.References == 0 &&
        poolOutstanding == 0);
    createdContext = NULL;

    initAdapterContext(&quarantineContext, &adapter, &createDevice);
    check("unproven adapter teardown quarantines permanently",
        LecDmaReleaseAdapterContext(&quarantineContext, FALSE) ==
            STATUS_DEVICE_BUSY && quarantineContext.Quarantined &&
        LecDmaClaimSynchronousOwner(&quarantineContext, &owner) ==
            STATUS_DEVICE_BUSY);

    check("owner claims central adapter context",
        LecSgSyncOwnerInit(&owner, &adapterContext, 20000) ==
            STATUS_SUCCESS &&
        adapterContext.SynchronousOwner == &owner);
    check("second owner cannot claim same adapter context",
        LecSgSyncOwnerInit(&rejectedOwner, &adapterContext, 20000) ==
            STATUS_DEVICE_BUSY);
    check("adapter teardown blocked while owner is claimed",
        LecDmaReleaseAdapterContext(&adapterContext, TRUE) ==
            STATUS_DEVICE_BUSY);

    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    check("GetEx uses synchronous no-callback DMA v3",
        NT_SUCCESS(status) && id != 0 && initCalls == 1 &&
        getCalls == 1 && badDdiArguments == 0);
    check("mapped stage holds PDO", device.References == 1);
    check("copied SG values match",
        LecSgSyncCopySegments(&owner, id, elements, 3, &count) ==
            STATUS_SUCCESS && count == 2 && elements[0].Length == 64);
    check("bounded copy checks caller capacity",
        LecSgSyncCopySegments(&owner, id, elements, 1, &count) ==
            STATUS_BUFFER_TOO_SMALL && count == 0);
    check("adapter refuses overlapping channel allocation",
        LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &second) ==
            STATUS_DEVICE_BUSY && second == 0);

    copyRace.Owner = &owner;
    copyRace.Token = id;
    copyRace.Errors = 0;
    worker = CreateThread(NULL, 0, concurrentCopy, &copyRace, 0, NULL);
    check("first release unmaps exactly once",
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_SUCCESS &&
        freeCalls == 1);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    check("copy versus release cannot see freed mapping",
        copyRace.Errors == 0);
    check("sequential double release rejects stale token",
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_INVALID_PARAMETER &&
        freeCalls == 1);
    check("arbitrary invalid token is rejected",
        LecSgSyncReleaseNoLaunch(&owner, 0xFEDCBA9876543210ULL) ==
            STATUS_INVALID_PARAMETER);

    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    freesBefore = freeCalls;
    releaseA.Owner = releaseB.Owner = &owner;
    releaseA.Token = releaseB.Token = id;
    releaseA.Status = releaseB.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(NULL, 0, concurrentRelease, &releaseA, 0, NULL);
    worker2 = CreateThread(NULL, 0, concurrentRelease, &releaseB, 0, NULL);
    WaitForSingleObject(worker, INFINITE);
    WaitForSingleObject(worker2, INFINITE);
    CloseHandle(worker);
    CloseHandle(worker2);
    check("concurrent double release has one winner",
        NT_SUCCESS(status) &&
        ((releaseA.Status == STATUS_SUCCESS &&
          releaseB.Status == STATUS_INVALID_PARAMETER) ||
         (releaseB.Status == STATUS_SUCCESS &&
          releaseA.Status == STATUS_INVALID_PARAMETER)) &&
        freeCalls == freesBefore + 1);

    failNextPoolAllocation = 1;
    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    check("stage allocation failure drains reservation",
        status == STATUS_INSUFFICIENT_RESOURCES && id == 0 &&
        owner.Outstanding == 0 && device.References == 0);
    mode = MODE_INIT_FAILURE;
    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    check("transfer-context failure drains references",
        status == STATUS_INSUFFICIENT_RESOURCES && id == 0 &&
        owner.Outstanding == 0 && device.References == 0);
    mode = MODE_GET_RESOURCE_FAILURE;
    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    check("resource shortage never publishes a mapping",
        status == STATUS_INSUFFICIENT_RESOURCES && id == 0 &&
        owner.Outstanding == 0 && device.References == 0);
    mode = MODE_SUCCESS_WITH_NULL_LIST;
    freesBefore = freeCalls;
    status = LecSgSyncMapNoLaunch(&owner, &mdls[0], 128, &id);
    check("successful GetEx with NULL list is explicitly freed",
        status == STATUS_INVALID_BUFFER_SIZE && id == 0 &&
        freeCalls == freesBefore + 1 && owner.Outstanding == 0 &&
        device.References == 0);
    mode = MODE_NORMAL;

    mdls[0].Size = 0x02000000U;
    mdls[0].Next = &mdls[1];
    mdls[1].Va = bytes;
    mdls[1].MdlFlags = MDL_PAGES_LOCKED;
    mdls[1].Size = 0x01000000U;
    sg.Elements[0].Length = 0x02000000U;
    sg.Elements[1].Length = 0x01000000U;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 0x03000000U, &id);
    check("48-MiB two-MDL mapping", NT_SUCCESS(status) && id != 0);
    check("SG byte coverage across MDLs",
        LecSgSyncCopySegments(&owner, id, elements, 3, &count) ==
            STATUS_SUCCESS);
    (void)LecSgSyncReleaseNoLaunch(&owner, id);

    adapterContext.NumberOfMapRegisters = 1;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 0x03000000U, &id);
    check("map-register capacity rejects oversized MDL chain",
        status == STATUS_INSUFFICIENT_RESOURCES && id == 0 &&
        owner.Outstanding == 0);
    adapterContext.NumberOfMapRegisters = 20000;

    mdls[1].Next = &mdls[0];
    status = LecSgSyncMapNoLaunch(&owner, mdls, 0x03000000U, &id);
    check("cyclic MDL chain rejected before DMA DDI",
        status == STATUS_INVALID_PARAMETER && id == 0);
    mdls[1].Next = NULL;

    sg.Elements[1].Length = 0x01000004U;
    freesBefore = freeCalls;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 0x03000000U, &id);
    check("invalid SG byte coverage is freed before publication",
        status == STATUS_INVALID_BUFFER_SIZE && id == 0 &&
        freeCalls == freesBefore + 1 && owner.Outstanding == 0);
    sg.Elements[1].Length = 0x01000000U;
    sg.Elements[1].Address.QuadPart = 0x100000000ULL;
    freesBefore = freeCalls;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 0x03000000U, &id);
    check("64-bit device address is freed before publication",
        status == STATUS_INVALID_BUFFER_SIZE && id == 0 &&
        freeCalls == freesBefore + 1);
    sg.Elements[1].Address.QuadPart = 0x20000;

    owner.DescriptorSlotCapacity = 2;
    mdls[0].Size = 8192;
    mdls[0].Next = NULL;
    sg.NumberOfElements = 1;
    sg.Elements[0].Address.QuadPart = 0x10000;
    sg.Elements[0].Length = 8192;
    freesBefore = freeCalls;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 8192, &id);
    check("descriptor capacity includes split entries and terminator",
        status == STATUS_BUFFER_TOO_SMALL && id == 0 &&
        freeCalls == freesBefore + 1 && owner.Outstanding == 0);

    mdls[0].Size = 511U * PAGE_SIZE;
    sg.Elements[0].Length = 511U * PAGE_SIZE;
    owner.DescriptorSlotCapacity = 512;
    status = LecSgSyncMapNoLaunch(
        &owner, mdls, 511U * PAGE_SIZE, &id);
    check("descriptor capacity reserves chain slot before terminator",
        status == STATUS_BUFFER_TOO_SMALL && id == 0);
    owner.DescriptorSlotCapacity = 513;
    status = LecSgSyncMapNoLaunch(
        &owner, mdls, 511U * PAGE_SIZE, &id);
    check("descriptor capacity accepts chained terminator page",
        NT_SUCCESS(status) && id != 0 &&
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_SUCCESS);
    owner.DescriptorSlotCapacity = 20000;

    mdls[0].Size = 128;
    sg.NumberOfElements = 2;
    sg.Elements[0].Length = 64;
    sg.Elements[1].Address.QuadPart = 0x20000;
    sg.Elements[1].Length = 64;
    status = LecSgSyncMapNoLaunch(&owner, mdls, 128, &id);
    check("mapping prepared for release/submission race",
        NT_SUCCESS(status) && id != 0);
    mode = MODE_BLOCK_FREE;
    ResetEvent(freeEntered);
    ResetEvent(allowFree);
    releaseA.Owner = &owner;
    releaseA.Token = id;
    releaseA.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(NULL, 0, concurrentRelease, &releaseA, 0, NULL);
    WaitForSingleObject(freeEntered, INFINITE);
    check("mapping submission stays blocked until release completes",
        LecSgSyncMapNoLaunch(&owner, mdls, 128, &second) ==
            STATUS_DEVICE_BUSY && second == 0);
    SetEvent(allowFree);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    mode = MODE_NORMAL;
    check("release/submission race drains exactly once",
        releaseA.Status == STATUS_SUCCESS && owner.Outstanding == 0);

    check("second adapter context has independent ownership",
        LecSgSyncOwnerInit(&otherOwner, &otherAdapterContext, 20000) ==
            STATUS_SUCCESS);
    status = LecSgSyncMapNoLaunch(&owner, mdls, 128, &id);
    check("first adapter can hold its own mapping",
        NT_SUCCESS(status) && id != 0);
    status = LecSgSyncMapNoLaunch(&otherOwner, mdls, 128, &otherId);
    check("distinct adapter can map concurrently",
        NT_SUCCESS(status) && otherId != 0 &&
        owner.Outstanding == 1 && otherOwner.Outstanding == 1);
    (void)LecSgSyncReleaseNoLaunch(&owner, id);
    (void)LecSgSyncReleaseNoLaunch(&otherOwner, otherId);
    check("each adapter mapping frees through its own adapter",
        mainFreeCalls > 0 && otherFreeCalls == 1 &&
        device.References == 0 && otherDevice.References == 0);
    (void)LecSgSyncOwnerStop(&otherOwner);
    check("independent owner destroys after STOP and drain",
        LecSgSyncOwnerDrainNoLaunch(&otherOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&otherOwner) == STATUS_SUCCESS &&
        otherAdapterContext.SynchronousOwner == NULL);

    memset(&drainAdapter, 0, sizeof(drainAdapter));
    memset(&drainDevice, 0, sizeof(drainDevice));
    initOperations(&drainOps);
    drainAdapter.DmaOperations = &drainOps;
    initAdapterContext(&drainContext, &drainAdapter, &drainDevice);
    check("teardown owner initializes",
        LecSgSyncOwnerInit(&drainOwner, &drainContext, 20000) ==
            STATUS_SUCCESS);
    status = LecSgSyncMapNoLaunch(&drainOwner, mdls, 128, &id);
    check("destroy refuses a live mapping",
        NT_SUCCESS(status) &&
        LecSgSyncOwnerDestroy(&drainOwner) == STATUS_DEVICE_BUSY);
    check("STOP reports outstanding no-launch resource",
        LecSgSyncOwnerStop(&drainOwner) == STATUS_DEVICE_BUSY);
    check("PnP drain releases published no-launch mapping",
        LecSgSyncOwnerDrainNoLaunch(&drainOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerCanTeardown(&drainOwner) &&
        drainDevice.References == 0);
    check("destroy relinquishes central adapter claim",
        LecSgSyncOwnerDestroy(&drainOwner) == STATUS_SUCCESS &&
        drainContext.SynchronousOwner == NULL);

    mode = MODE_BLOCK_GET;
    ResetEvent(getEntered);
    ResetEvent(allowGet);
    mapRace.Owner = &owner;
    mapRace.Mdl = mdls;
    mapRace.Length = 128;
    mapRace.Token = 0;
    mapRace.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(NULL, 0, concurrentMap, &mapRace, 0, NULL);
    WaitForSingleObject(getEntered, INFINITE);
    check("concurrent STOP observes in-flight GetEx allocation",
        LecSgSyncOwnerStop(&owner) == STATUS_DEVICE_BUSY &&
        !LecSgSyncOwnerCanTeardown(&owner));
    SetEvent(allowGet);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    mode = MODE_NORMAL;
    check("STOP race auto-releases never-launched mapping",
        mapRace.Status == STATUS_DELETE_PENDING && mapRace.Token == 0 &&
        owner.Outstanding == 0 && device.References == 0 &&
        LecSgSyncOwnerCanTeardown(&owner));
    check("STOP permanently closes new submissions",
        LecSgSyncMapNoLaunch(&owner, mdls, 128, &second) ==
            STATUS_DEVICE_BUSY && second == 0);
    check("drain is idempotent after STOP",
        LecSgSyncOwnerDrainNoLaunch(&owner) == STATUS_SUCCESS);
    check("owner destroy relinquishes adapter exactly once",
        LecSgSyncOwnerDestroy(&owner) == STATUS_SUCCESS &&
        adapterContext.SynchronousOwner == NULL &&
        LecSgSyncOwnerDestroy(&owner) == STATUS_INVALID_PARAMETER);
    check("all stage allocations and PDO references are balanced",
        poolOutstanding == 0 && device.References == 0 &&
        otherDevice.References == 0 && drainDevice.References == 0);
    check("all DDI calls used the required synchronous contract",
        badDdiArguments == 0);

    CloseHandle(getEntered);
    CloseHandle(allowGet);
    CloseHandle(freeEntered);
    CloseHandle(allowFree);

    printf("SG SYNC V3: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed ? 1 : 0;
}
