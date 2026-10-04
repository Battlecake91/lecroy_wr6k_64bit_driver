/* Actual inactive synchronous DMA v3 bridge under fake WDM DDIs. */
#include <stdio.h>
#include "../../driver/DmaSyncStage.h"
#include "../../driver/DmaPnpStage.h"
#include "../../driver/DmaLayout.h"

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
static BOOLEAN commonMisaligned;
static PHYSICAL_ADDRESS commonLogical;
static __declspec(align(4096)) char commonBuffer[4096];
static __declspec(align(4096)) char mainDescriptorTable[40U * 4096U];
static __declspec(align(4096)) char otherDescriptorTable[8192];
static __declspec(align(4096)) char drainDescriptorTable[4096];
static __declspec(align(4096)) char repeatDescriptorTable[4096];
static __declspec(align(4096)) char capacityDescriptorTable[4096];
static __declspec(align(4096)) char sameDeviceDescriptorTable[4096];
static __declspec(align(4096)) char initRaceDescriptorTable[4096];
static __declspec(align(4096)) char initBlockDescriptorTable[4096];
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
    return commonBuffer + (commonMisaligned ? 8 : 0);
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
    PDEVICE_OBJECT deviceObject,
    PVOID table,
    ULONG tableLength,
    ULONGLONG tableLogical)
{
    memset(context, 0, sizeof(*context));
    context->Adapter = dmaAdapter;
    context->PhysicalDeviceObject = deviceObject;
    context->NumberOfMapRegisters = 20000;
    context->TableVirtual = table;
    context->TableLength = tableLength;
    context->TableLogical.QuadPart = tableLogical;
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
    initAdapterContext(&adapterContext, &adapter, &device,
        mainDescriptorTable, sizeof(mainDescriptorTable), 0x30000);
    initAdapterContext(&otherAdapterContext, &otherAdapter, &otherDevice,
        otherDescriptorTable, sizeof(otherDescriptorTable), 0x40000);
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
    commonMisaligned = FALSE;
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

typedef struct _INIT_THREAD {
    PLECS65_SG_SYNC_OWNER Owner;
    PLECS65_DMA_ADAPTER_CONTEXT Context;
    NTSTATUS Status;
} INIT_THREAD;

typedef struct _OWNER_THREAD {
    PLECS65_SG_SYNC_OWNER Owner;
    NTSTATUS Status;
} OWNER_THREAD;

typedef struct _PNP_MAP_THREAD {
    PLECS65_DMA_PNP_STAGE Stage;
    PMDL Mdl;
    ULONG Length;
    ULONGLONG Token;
    NTSTATUS Status;
} PNP_MAP_THREAD;

typedef struct _PNP_RELEASE_THREAD {
    PLECS65_DMA_PNP_STAGE Stage;
    ULONGLONG Token;
    NTSTATUS Status;
} PNP_RELEASE_THREAD;

static DWORD WINAPI concurrentMap(void* argument)
{
    MAP_THREAD* map = (MAP_THREAD*)argument;
    map->Status = LecSgSyncMapNoLaunch(
        map->Owner, map->Mdl, map->Length, &map->Token);
    return 0;
}

static DWORD WINAPI concurrentInit(void* argument)
{
    INIT_THREAD* init = (INIT_THREAD*)argument;
    init->Status = LecSgSyncOwnerInit(init->Owner, init->Context);
    return 0;
}

static DWORD WINAPI concurrentDestroy(void* argument)
{
    OWNER_THREAD* destroy = (OWNER_THREAD*)argument;
    destroy->Status = LecSgSyncOwnerDestroy(destroy->Owner);
    return 0;
}

static DWORD WINAPI concurrentPnpMap(void* argument)
{
    PNP_MAP_THREAD* map = (PNP_MAP_THREAD*)argument;
    map->Status = LecDmaPnpMapNoLaunch(
        map->Stage, map->Mdl, map->Length, &map->Token);
    return 0;
}

static DWORD WINAPI concurrentPnpRelease(void* argument)
{
    PNP_RELEASE_THREAD* release = (PNP_RELEASE_THREAD*)argument;
    release->Status = LecDmaPnpReleaseNoLaunch(
        release->Stage, release->Token);
    return 0;
}

static NTSTATUS quiescePnpStage(PLECS65_DMA_PNP_STAGE stage)
{
    NTSTATUS status;
    status = LecDmaPnpRecordQuiescence(
        stage, LecDmaPnpInterruptDisconnected);
    if (NT_SUCCESS(status)) {
        status = LecDmaPnpRecordQuiescence(stage, LecDmaPnpDpcDrained);
    }
    if (NT_SUCCESS(status)) {
        status = LecDmaPnpRecordQuiescence(stage, LecDmaPnpTimerStopped);
    }
    return status;
}

static int waitForOwnerState(
    PLECS65_SG_SYNC_OWNER target,
    LECS65_SG_SYNC_OWNER_STATE expected)
{
    unsigned attempt;
    for (attempt = 0; attempt < 5000; ++attempt) {
        KIRQL irql;
        LECS65_SG_SYNC_OWNER_STATE current;
        KeAcquireSpinLock(&target->Lock, &irql);
        current = target->State;
        KeReleaseSpinLock(&target->Lock, irql);
        if (current == expected) {
            return 1;
        }
        Sleep(1);
    }
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
    LECS65_SG_SYNC_OWNER invalidOwner;
    LECS65_SG_SYNC_OWNER repeatOwner;
    LECS65_SG_SYNC_OWNER capacityOwner;
    LECS65_SG_SYNC_OWNER sameDeviceOwner;
    LECS65_SG_SYNC_OWNER initRaceOwner;
    LECS65_SG_SYNC_OWNER initBlockOwner;
    LECS65_DMA_ADAPTER_CONTEXT drainContext;
    LECS65_DMA_ADAPTER_CONTEXT invalidContext;
    LECS65_DMA_ADAPTER_CONTEXT repeatContext;
    LECS65_DMA_ADAPTER_CONTEXT capacityContext;
    LECS65_DMA_ADAPTER_CONTEXT sameDeviceContext;
    LECS65_DMA_ADAPTER_CONTEXT initRaceContext;
    LECS65_DMA_ADAPTER_CONTEXT initBlockContext;
    DMA_ADAPTER drainAdapter;
    DMA_OPERATIONS drainOps;
    FAKE_DEVICE drainDevice;
    LECS65_DMA_ADAPTER_CONTEXT* createdContext = NULL;
    LECS65_DMA_ADAPTER_CONTEXT quarantineContext;
    FAKE_DEVICE createDevice;
    ULONG fullOperationsSize;
    LONG putsBefore;
    LONG commonFreesBefore;
    ULONG ignoredCapacity = 0;
    ULONG savedOutstanding;
    ULONGLONG savedToken;
    PLECS65_SG_SYNC_STAGE savedMappings;
    INIT_THREAD initA, initB;
    INIT_THREAD blockedInit;
    OWNER_THREAD blockedDestroy;
    KIRQL heldIrql;
    LECS65_DMA_PNP_STAGE pnpStage;
    LECS65_DMA_PNP_STAGE failedPnpStage;
    LECS65_DMA_PNP_STAGE surprisePnpStage;
    LECS65_DMA_PNP_STAGE mapRacePnpStage;
    LECS65_DMA_PNP_STAGE releaseRacePnpStage;
    LECS65_DMA_PNP_STAGE removePnpStage;
    LECS65_DMA_PNP_STAGE teardownRetryPnpStage;
    LECS65_DMA_PNP_STAGE quarantinePnpStage;
    LECS65_DMA_PNP_SNAPSHOT pnpSnapshot;
    FAKE_DEVICE pnpDevice;
    FAKE_DEVICE failedPnpDevice;
    FAKE_DEVICE surprisePnpDevice;
    FAKE_DEVICE mapRacePnpDevice;
    FAKE_DEVICE releaseRacePnpDevice;
    FAKE_DEVICE removePnpDevice;
    FAKE_DEVICE teardownRetryPnpDevice;
    FAKE_DEVICE quarantinePnpDevice;
    PNP_MAP_THREAD pnpMapRace;
    PNP_RELEASE_THREAD pnpReleaseRace;
    ULONGLONG firstGenerationToken;
    LONG commonFreesAtQuarantine;
    LONG putsAtQuarantine;

    setup();
    LecSgSyncOwnerConstruct(&owner);
    LecSgSyncOwnerConstruct(&otherOwner);
    LecSgSyncOwnerConstruct(&rejectedOwner);
    LecSgSyncOwnerConstruct(&drainOwner);
    LecSgSyncOwnerConstruct(&invalidOwner);
    LecSgSyncOwnerConstruct(&repeatOwner);
    LecSgSyncOwnerConstruct(&capacityOwner);
    LecSgSyncOwnerConstruct(&sameDeviceOwner);
    LecSgSyncOwnerConstruct(&initRaceOwner);
    LecSgSyncOwnerConstruct(&initBlockOwner);
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
    commonMisaligned = TRUE;
    commonFreesBefore = commonFreeCalls;
    check("misaligned common buffer is returned immediately",
        LecDmaAllocateCommonTable(createdContext, 4096) ==
            STATUS_NOT_SUPPORTED &&
        commonFreeCalls == commonFreesBefore + 1 &&
        createdContext->TableVirtual == NULL);
    commonMisaligned = FALSE;
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

    initAdapterContext(&quarantineContext, &adapter, &createDevice,
        mainDescriptorTable, sizeof(mainDescriptorTable), 0x30000);
    check("unproven adapter teardown quarantines permanently",
        LecDmaReleaseAdapterContext(&quarantineContext, FALSE) ==
            STATUS_DEVICE_BUSY && quarantineContext.Quarantined &&
        LecDmaClaimSynchronousOwner(
            &quarantineContext, &owner, &ignoredCapacity) ==
            STATUS_DEVICE_BUSY);

    initAdapterContext(&invalidContext, &adapter, &device,
        NULL, 0, 0);
    check("owner rejects a missing descriptor table",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_DEVICE_STATE &&
        invalidOwner.State == LecSgSyncOwnerConstructed &&
        invalidContext.SynchronousOwner == NULL);
    invalidContext.TableVirtual = mainDescriptorTable;
    invalidContext.TableLength = 8;
    invalidContext.TableLogical.QuadPart = 0x30000;
    check("owner rejects an undersized descriptor table",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_BUFFER_SIZE &&
        invalidOwner.State == LecSgSyncOwnerConstructed);
    invalidContext.TableLength = 4097;
    check("owner rejects a non-page descriptor length",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_BUFFER_SIZE);
    invalidContext.TableLength = 0xFFFFF000U;
    check("owner rejects capacity inconsistent with 32-bit backing",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_BUFFER_SIZE);
    invalidContext.TableLength = 4096;
    invalidContext.TableVirtual = mainDescriptorTable + 8;
    check("owner rejects a misaligned descriptor virtual base",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_BUFFER_SIZE);
    invalidContext.TableVirtual = mainDescriptorTable;
    invalidContext.TableLogical.QuadPart = 0x30008;
    check("owner rejects a misaligned descriptor logical base",
        LecSgSyncOwnerInit(&invalidOwner, &invalidContext) ==
            STATUS_INVALID_BUFFER_SIZE);

    check("owner claims central adapter context",
        LecSgSyncOwnerInit(&owner, &adapterContext) ==
            STATUS_SUCCESS &&
        adapterContext.SynchronousOwner == &owner &&
        owner.DescriptorSlotCapacity ==
            sizeof(mainDescriptorTable) / sizeof(LECS65_DMA_LAYOUT_ENTRY));
    check("second owner cannot claim same adapter context",
        LecSgSyncOwnerInit(&rejectedOwner, &adapterContext) ==
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

    initAdapterContext(&repeatContext, &otherAdapter, &otherDevice,
        repeatDescriptorTable, sizeof(repeatDescriptorTable), 0x50000);
    check("repeat-init owner starts with table-derived capacity",
        LecSgSyncOwnerInit(&repeatOwner, &repeatContext) == STATUS_SUCCESS &&
        repeatOwner.DescriptorSlotCapacity == 512);
    status = LecSgSyncMapNoLaunch(&repeatOwner, &mdls[0], 128, &savedToken);
    savedOutstanding = repeatOwner.Outstanding;
    savedMappings = repeatOwner.Mappings;
    check("repeated init with mapping fails without mutation",
        NT_SUCCESS(status) && savedToken != 0 &&
        LecSgSyncOwnerInit(&repeatOwner, &repeatContext) ==
            STATUS_DEVICE_BUSY &&
        repeatOwner.Outstanding == savedOutstanding &&
        repeatOwner.Mappings == savedMappings &&
        repeatOwner.Mappings->Token == savedToken &&
        repeatOwner.AdapterContext == &repeatContext &&
        repeatContext.SynchronousOwner == &repeatOwner);
    check("copy survives rejected repeated init",
        LecSgSyncCopySegments(
            &repeatOwner, savedToken, elements, 3, &count) ==
            STATUS_SUCCESS && count == 2);
    check("STOP survives rejected repeated init",
        LecSgSyncOwnerStop(&repeatOwner) == STATUS_DEVICE_BUSY);
    check("release drain and destroy survive rejected repeated init",
        LecSgSyncReleaseNoLaunch(&repeatOwner, savedToken) == STATUS_SUCCESS &&
        LecSgSyncOwnerDrainNoLaunch(&repeatOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&repeatOwner) == STATUS_SUCCESS &&
        repeatContext.SynchronousOwner == NULL);

    copyRace.Owner = &owner;
    copyRace.Token = id;
    copyRace.Errors = 0;
    worker = CreateThread(NULL, 0, concurrentCopy, &copyRace, 0, NULL);
    freesBefore = freeCalls;
    check("first release unmaps exactly once",
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_SUCCESS &&
        freeCalls == freesBefore + 1);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    check("copy versus release cannot see freed mapping",
        copyRace.Errors == 0);
    check("sequential double release rejects stale token",
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_INVALID_PARAMETER &&
        freeCalls == freesBefore + 1);
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

    initAdapterContext(&capacityContext, &adapter, &device,
        capacityDescriptorTable, sizeof(capacityDescriptorTable), 0x60000);
    check("one-page owner derives exactly 512 descriptor slots",
        LecSgSyncOwnerInit(&capacityOwner, &capacityContext) ==
            STATUS_SUCCESS &&
        capacityOwner.DescriptorSlotCapacity == 512);
    mdls[0].Size = 511U * PAGE_SIZE;
    mdls[0].Next = NULL;
    sg.NumberOfElements = 1;
    sg.Elements[0].Address.QuadPart = 0x10000;
    sg.Elements[0].Length = 511U * PAGE_SIZE;
    status = LecSgSyncMapNoLaunch(
        &capacityOwner, mdls, 511U * PAGE_SIZE, &id);
    check("descriptor capacity reserves chain slot before terminator",
        status == STATUS_BUFFER_TOO_SMALL && id == 0);
    (void)LecSgSyncOwnerStop(&capacityOwner);
    check("capacity failure leaves one-page owner destroyable",
        LecSgSyncOwnerDrainNoLaunch(&capacityOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&capacityOwner) == STATUS_SUCCESS);

    status = LecSgSyncMapNoLaunch(
        &owner, mdls, 511U * PAGE_SIZE, &id);
    check("two-page backing accepts chained terminator page",
        NT_SUCCESS(status) && id != 0 &&
        LecSgSyncReleaseNoLaunch(&owner, id) == STATUS_SUCCESS);

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
        LecSgSyncOwnerInit(&otherOwner, &otherAdapterContext) ==
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
        mainFreeCalls > 0 && otherFreeCalls > 0 &&
        device.References == 0 && otherDevice.References == 0);
    (void)LecSgSyncOwnerStop(&otherOwner);
    check("independent owner destroys after STOP and drain",
        LecSgSyncOwnerDrainNoLaunch(&otherOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&otherOwner) == STATUS_SUCCESS &&
        otherAdapterContext.SynchronousOwner == NULL);

    initAdapterContext(&sameDeviceContext, &adapter, &device,
        sameDeviceDescriptorTable, sizeof(sameDeviceDescriptorTable),
        0x80000);
    check("separate context claim is only context-local",
        LecSgSyncOwnerInit(&sameDeviceOwner, &sameDeviceContext) ==
            STATUS_SUCCESS &&
        adapterContext.SynchronousOwner == &owner &&
        sameDeviceContext.SynchronousOwner == &sameDeviceOwner);
    status = LecSgSyncMapNoLaunch(&owner, mdls, 128, &id);
    status = NT_SUCCESS(status) ?
        LecSgSyncMapNoLaunch(&sameDeviceOwner, mdls, 128, &otherId) : status;
    check("same physical adapter contexts are not device-wide serialized",
        NT_SUCCESS(status) && id != 0 && otherId != 0 &&
        owner.Outstanding == 1 && sameDeviceOwner.Outstanding == 1);
    (void)LecSgSyncReleaseNoLaunch(&owner, id);
    (void)LecSgSyncReleaseNoLaunch(&sameDeviceOwner, otherId);
    (void)LecSgSyncOwnerStop(&sameDeviceOwner);
    check("context-local scope remains explicitly drainable",
        LecSgSyncOwnerDrainNoLaunch(&sameDeviceOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&sameDeviceOwner) == STATUS_SUCCESS);

    initAdapterContext(&initRaceContext, &otherAdapter, &otherDevice,
        initRaceDescriptorTable, sizeof(initRaceDescriptorTable), 0x90000);
    initA.Owner = initB.Owner = &initRaceOwner;
    initA.Context = initB.Context = &initRaceContext;
    initA.Status = initB.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(NULL, 0, concurrentInit, &initA, 0, NULL);
    worker2 = CreateThread(NULL, 0, concurrentInit, &initB, 0, NULL);
    WaitForSingleObject(worker, INFINITE);
    WaitForSingleObject(worker2, INFINITE);
    CloseHandle(worker);
    CloseHandle(worker2);
    check("concurrent owner initialization has exactly one winner",
        ((initA.Status == STATUS_SUCCESS &&
          initB.Status == STATUS_DEVICE_BUSY) ||
         (initB.Status == STATUS_SUCCESS &&
          initA.Status == STATUS_DEVICE_BUSY)) &&
        initRaceOwner.State == LecSgSyncOwnerActive &&
        initRaceContext.SynchronousOwner == &initRaceOwner);
    (void)LecSgSyncOwnerStop(&initRaceOwner);
    check("concurrently initialized owner remains destroyable",
        LecSgSyncOwnerDrainNoLaunch(&initRaceOwner) == STATUS_SUCCESS &&
        LecSgSyncOwnerDestroy(&initRaceOwner) == STATUS_SUCCESS);

    initAdapterContext(&initBlockContext, &otherAdapter, &otherDevice,
        initBlockDescriptorTable, sizeof(initBlockDescriptorTable), 0xA0000);
    blockedInit.Owner = &initBlockOwner;
    blockedInit.Context = &initBlockContext;
    blockedInit.Status = STATUS_INTERNAL_ERROR;
    KeAcquireSpinLock(&initBlockContext.Lock, &heldIrql);
    worker = CreateThread(NULL, 0, concurrentInit, &blockedInit, 0, NULL);
    check("owner exposes a stable initializing state",
        waitForOwnerState(
            &initBlockOwner, LecSgSyncOwnerInitializing));
    check("STOP map and destroy safely reject concurrent initialization",
        LecSgSyncOwnerStop(&initBlockOwner) == STATUS_DEVICE_BUSY &&
        LecSgSyncMapNoLaunch(
            &initBlockOwner, mdls, 128, &second) == STATUS_DEVICE_BUSY &&
        second == 0 &&
        LecSgSyncOwnerDestroy(&initBlockOwner) == STATUS_DEVICE_BUSY);
    KeReleaseSpinLock(&initBlockContext.Lock, heldIrql);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    check("blocked initialization completes without lost ownership",
        blockedInit.Status == STATUS_SUCCESS &&
        initBlockOwner.State == LecSgSyncOwnerActive &&
        initBlockContext.SynchronousOwner == &initBlockOwner);

    check("blocked-destroy owner enters STOP cleanly",
        LecSgSyncOwnerStop(&initBlockOwner) == STATUS_SUCCESS);
    blockedDestroy.Owner = &initBlockOwner;
    blockedDestroy.Status = STATUS_INTERNAL_ERROR;
    KeAcquireSpinLock(&initBlockContext.Lock, &heldIrql);
    worker = CreateThread(
        NULL, 0, concurrentDestroy, &blockedDestroy, 0, NULL);
    check("owner exposes a stable destroying state",
        waitForOwnerState(&initBlockOwner, LecSgSyncOwnerDestroying));
    check("APIs safely reject concurrent owner destruction",
        LecSgSyncOwnerStop(&initBlockOwner) == STATUS_DEVICE_BUSY &&
        LecSgSyncMapNoLaunch(
            &initBlockOwner, mdls, 128, &second) == STATUS_DEVICE_BUSY &&
        LecSgSyncCopySegments(
            &initBlockOwner, 1, elements, 3, &count) ==
                STATUS_INVALID_PARAMETER &&
        LecSgSyncReleaseNoLaunch(&initBlockOwner, 1) ==
            STATUS_INVALID_PARAMETER);
    KeReleaseSpinLock(&initBlockContext.Lock, heldIrql);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    check("blocked destruction relinquishes the claim once",
        blockedDestroy.Status == STATUS_SUCCESS &&
        initBlockOwner.State == LecSgSyncOwnerDestroyed &&
        initBlockContext.SynchronousOwner == NULL);

    memset(&drainAdapter, 0, sizeof(drainAdapter));
    memset(&drainDevice, 0, sizeof(drainDevice));
    initOperations(&drainOps);
    drainAdapter.DmaOperations = &drainOps;
    initAdapterContext(&drainContext, &drainAdapter, &drainDevice,
        drainDescriptorTable, sizeof(drainDescriptorTable), 0x70000);
    check("teardown owner initializes",
        LecSgSyncOwnerInit(&drainOwner, &drainContext) ==
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
    drainContext.SynchronousOwner = &rejectedOwner;
    check("failed central release restores stopping owner state",
        LecSgSyncOwnerDestroy(&drainOwner) == STATUS_INVALID_PARAMETER &&
        drainOwner.State == LecSgSyncOwnerStopping &&
        drainOwner.AdapterContext == &drainContext);
    drainContext.SynchronousOwner = &drainOwner;
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

    memset(&pnpDevice, 0, sizeof(pnpDevice));
    LecDmaPnpStageConstruct(&pnpStage, &pnpDevice);
    LecDmaPnpSnapshot(&pnpStage, &pnpSnapshot);
    check("PnP parent construction performs no DMA allocation",
        pnpSnapshot.State == LecDmaPnpStopped &&
        !pnpSnapshot.HasAdapterContext && !pnpSnapshot.AdmissionOpen &&
        pnpDevice.References == 0);
    check("PnP START creates one owned adapter context",
        LecDmaPnpStartNoLaunch(&pnpStage, 0x100000, 4096) ==
            STATUS_SUCCESS);
    LecDmaPnpSnapshot(&pnpStage, &pnpSnapshot);
    check("PnP START publishes admission only after full initialization",
        pnpSnapshot.State == LecDmaPnpStarted &&
        pnpSnapshot.HasAdapterContext && pnpSnapshot.AdmissionOpen &&
        pnpSnapshot.Generation == 1 && pnpDevice.References == 1);
    commonFreesBefore = commonAllocateCalls;
    check("one parent cannot create a second adapter context",
        LecDmaPnpStartNoLaunch(&pnpStage, 0x100000, 4096) ==
            STATUS_DEVICE_BUSY && commonAllocateCalls == commonFreesBefore);
    status = LecDmaPnpMapNoLaunch(
        &pnpStage, mdls, 128, &firstGenerationToken);
    check("PnP parent maps through the actual no-launch owner",
        NT_SUCCESS(status) && firstGenerationToken != 0 &&
        pnpDevice.References == 2);
    check("PnP parent copies mapping data under parent rundown",
        LecDmaPnpCopySegments(
            &pnpStage, firstGenerationToken, elements, 3, &count) ==
            STATUS_SUCCESS && count == 2 && elements[0].Length == 64);
    check("STOP closes admission before cleanup",
        LecDmaPnpBeginTeardown(
            &pnpStage, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        LecDmaPnpMapNoLaunch(&pnpStage, mdls, 128, &second) ==
            STATUS_DELETE_PENDING && second == 0);
    check("teardown refuses missing software quiescence",
        LecDmaPnpFinishTeardownNoLaunch(&pnpStage) == STATUS_DEVICE_BUSY);
    check("DPC and timer rundown cannot precede interrupt disconnect",
        LecDmaPnpRecordQuiescence(
            &pnpStage, LecDmaPnpDpcDrained) ==
                STATUS_INVALID_DEVICE_STATE &&
        LecDmaPnpRecordQuiescence(
            &pnpStage, LecDmaPnpTimerStopped) ==
                STATUS_INVALID_DEVICE_STATE);
    check("IRQ DPC timer quiescence ordering is accepted",
        quiescePnpStage(&pnpStage) == STATUS_SUCCESS);
    check("STOP drains only no-launch mappings and releases ownership",
        LecDmaPnpFinishTeardownNoLaunch(&pnpStage) == STATUS_SUCCESS &&
        pnpDevice.References == 0);
    check("START after STOP creates a new generation",
        LecDmaPnpStartNoLaunch(&pnpStage, 0x100000, 4096) ==
            STATUS_SUCCESS);
    LecDmaPnpSnapshot(&pnpStage, &pnpSnapshot);
    check("stale prior-generation token cannot resolve",
        pnpSnapshot.Generation == 2 &&
        LecDmaPnpReleaseNoLaunch(
            &pnpStage, firstGenerationToken) == STATUS_INVALID_PARAMETER);
    status = LecDmaPnpMapNoLaunch(&pnpStage, mdls, 128, &id);
    check("mapping tokens remain monotonic across START cycles",
        NT_SUCCESS(status) && id > firstGenerationToken);
    check("parent rejects duplicate release without double free",
        LecDmaPnpReleaseNoLaunch(&pnpStage, id) == STATUS_SUCCESS &&
        LecDmaPnpReleaseNoLaunch(&pnpStage, id) ==
            STATUS_INVALID_PARAMETER);
    check("second STOP and REMOVE are idempotent",
        LecDmaPnpBeginTeardown(
            &pnpStage, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        quiescePnpStage(&pnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&pnpStage) == STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &pnpStage, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &pnpStage, LecDmaPnpTeardownRemove) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&pnpStage) == STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &pnpStage, LecDmaPnpTeardownRemove) == STATUS_SUCCESS &&
        LecDmaPnpCanDestroy(&pnpStage));

    memset(&failedPnpDevice, 0, sizeof(failedPnpDevice));
    LecDmaPnpStageConstruct(&failedPnpStage, &failedPnpDevice);
    commonAvailable = FALSE;
    status = LecDmaPnpStartNoLaunch(
        &failedPnpStage, 0x100000, 4096);
    commonAvailable = TRUE;
    LecDmaPnpSnapshot(&failedPnpStage, &pnpSnapshot);
    check("failed START rolls back partial adapter ownership",
        status == STATUS_INSUFFICIENT_RESOURCES &&
        pnpSnapshot.State == LecDmaPnpStopped &&
        !pnpSnapshot.HasAdapterContext && failedPnpDevice.References == 0);
    check("failed START owner remains reusable",
        LecDmaPnpStartNoLaunch(
            &failedPnpStage, 0x100000, 4096) == STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &failedPnpStage, LecDmaPnpTeardownRemove) == STATUS_SUCCESS &&
        quiescePnpStage(&failedPnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&failedPnpStage) == STATUS_SUCCESS &&
        LecDmaPnpCanDestroy(&failedPnpStage));

    memset(&surprisePnpDevice, 0, sizeof(surprisePnpDevice));
    LecDmaPnpStageConstruct(&surprisePnpStage, &surprisePnpDevice);
    check("surprise-removal fixture starts",
        LecDmaPnpStartNoLaunch(
            &surprisePnpStage, 0x100000, 4096) == STATUS_SUCCESS);
    check("surprise removal drains without any hardware-idle claim",
        LecDmaPnpBeginTeardown(
            &surprisePnpStage, LecDmaPnpTeardownSurprise) ==
                STATUS_SUCCESS &&
        quiescePnpStage(&surprisePnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&surprisePnpStage) ==
            STATUS_SUCCESS);
    LecDmaPnpSnapshot(&surprisePnpStage, &pnpSnapshot);
    check("surprise-removed owner cannot restart",
        pnpSnapshot.State == LecDmaPnpSurpriseRemoved &&
        LecDmaPnpStartNoLaunch(
            &surprisePnpStage, 0x100000, 4096) == STATUS_DEVICE_BUSY);
    check("REMOVE after surprise makes parent destroyable",
        LecDmaPnpBeginTeardown(
            &surprisePnpStage, LecDmaPnpTeardownRemove) ==
                STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&surprisePnpStage) ==
            STATUS_SUCCESS && LecDmaPnpCanDestroy(&surprisePnpStage));

    memset(&mapRacePnpDevice, 0, sizeof(mapRacePnpDevice));
    LecDmaPnpStageConstruct(&mapRacePnpStage, &mapRacePnpDevice);
    check("mapping-race fixture starts",
        LecDmaPnpStartNoLaunch(
            &mapRacePnpStage, 0x100000, 4096) == STATUS_SUCCESS);
    mode = MODE_BLOCK_GET;
    ResetEvent(getEntered);
    ResetEvent(allowGet);
    pnpMapRace.Stage = &mapRacePnpStage;
    pnpMapRace.Mdl = mdls;
    pnpMapRace.Length = 128;
    pnpMapRace.Token = 0;
    pnpMapRace.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(NULL, 0, concurrentPnpMap, &pnpMapRace, 0, NULL);
    WaitForSingleObject(getEntered, INFINITE);
    check("STOP observes parent reference during mapping submission",
        LecDmaPnpBeginTeardown(
            &mapRacePnpStage, LecDmaPnpTeardownStop) ==
                STATUS_DEVICE_BUSY);
    check("owner destruction waits for mapping submission reference",
        quiescePnpStage(&mapRacePnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&mapRacePnpStage) ==
            STATUS_DEVICE_BUSY);
    SetEvent(allowGet);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    mode = MODE_NORMAL;
    check("STOP race releases never-launched parent mapping",
        pnpMapRace.Status == STATUS_DELETE_PENDING &&
        pnpMapRace.Token == 0 && mapRacePnpDevice.References == 1);
    check("mapping-race STOP completes after reference rundown",
        LecDmaPnpBeginTeardown(
            &mapRacePnpStage, LecDmaPnpTeardownStop) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&mapRacePnpStage) ==
            STATUS_SUCCESS && mapRacePnpDevice.References == 0);
    check("mapping-race parent removes cleanly",
        LecDmaPnpBeginTeardown(
            &mapRacePnpStage, LecDmaPnpTeardownRemove) ==
                STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&mapRacePnpStage) ==
            STATUS_SUCCESS && LecDmaPnpCanDestroy(&mapRacePnpStage));

    memset(&releaseRacePnpDevice, 0, sizeof(releaseRacePnpDevice));
    LecDmaPnpStageConstruct(&releaseRacePnpStage, &releaseRacePnpDevice);
    check("release-race fixture maps",
        LecDmaPnpStartNoLaunch(
            &releaseRacePnpStage, 0x100000, 4096) == STATUS_SUCCESS &&
        LecDmaPnpMapNoLaunch(
            &releaseRacePnpStage, mdls, 128, &id) == STATUS_SUCCESS);
    mode = MODE_BLOCK_FREE;
    ResetEvent(freeEntered);
    ResetEvent(allowFree);
    pnpReleaseRace.Stage = &releaseRacePnpStage;
    pnpReleaseRace.Token = id;
    pnpReleaseRace.Status = STATUS_INTERNAL_ERROR;
    worker = CreateThread(
        NULL, 0, concurrentPnpRelease, &pnpReleaseRace, 0, NULL);
    WaitForSingleObject(freeEntered, INFINITE);
    check("STOP and cleanup wait for concurrent mapping release",
        LecDmaPnpBeginTeardown(
            &releaseRacePnpStage, LecDmaPnpTeardownStop) ==
                STATUS_DEVICE_BUSY &&
        quiescePnpStage(&releaseRacePnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&releaseRacePnpStage) ==
            STATUS_DEVICE_BUSY);
    SetEvent(allowFree);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    mode = MODE_NORMAL;
    check("concurrent release finishes exactly once",
        pnpReleaseRace.Status == STATUS_SUCCESS);
    check("release-race owner tears down after reference rundown",
        LecDmaPnpBeginTeardown(
            &releaseRacePnpStage, LecDmaPnpTeardownRemove) ==
                STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&releaseRacePnpStage) ==
            STATUS_SUCCESS && LecDmaPnpCanDestroy(&releaseRacePnpStage));

    memset(&removePnpDevice, 0, sizeof(removePnpDevice));
    LecDmaPnpStageConstruct(&removePnpStage, &removePnpDevice);
    check("REMOVE fixture owns an outstanding no-launch mapping",
        LecDmaPnpStartNoLaunch(
            &removePnpStage, 0x100000, 4096) == STATUS_SUCCESS &&
        LecDmaPnpMapNoLaunch(
            &removePnpStage, mdls, 128, &id) == STATUS_SUCCESS);
    check("REMOVE drains outstanding no-launch mapping after rundown",
        LecDmaPnpBeginTeardown(
            &removePnpStage, LecDmaPnpTeardownRemove) == STATUS_SUCCESS &&
        quiescePnpStage(&removePnpStage) == STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&removePnpStage) ==
            STATUS_SUCCESS && removePnpDevice.References == 0 &&
        LecDmaPnpCanDestroy(&removePnpStage));

    memset(&teardownRetryPnpDevice, 0, sizeof(teardownRetryPnpDevice));
    LecDmaPnpStageConstruct(
        &teardownRetryPnpStage, &teardownRetryPnpDevice);
    check("teardown-retry fixture reaches quiesced STOP",
        LecDmaPnpStartNoLaunch(
            &teardownRetryPnpStage, 0x100000, 4096) == STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &teardownRetryPnpStage, LecDmaPnpTeardownStop) ==
                STATUS_SUCCESS &&
        quiescePnpStage(&teardownRetryPnpStage) == STATUS_SUCCESS);
    teardownRetryPnpStage.AdapterContext->TableAllocationPending = TRUE;
    check("adapter-release failure retains detached parent for retry",
        LecDmaPnpFinishTeardownNoLaunch(&teardownRetryPnpStage) ==
            STATUS_DEVICE_BUSY);
    LecDmaPnpSnapshot(&teardownRetryPnpStage, &pnpSnapshot);
    teardownRetryPnpStage.AdapterContext->TableAllocationPending = FALSE;
    check("teardown retry does not destroy the child owner twice",
        pnpSnapshot.SyncOwnerDetached && pnpSnapshot.HasAdapterContext &&
        LecDmaPnpFinishTeardownNoLaunch(&teardownRetryPnpStage) ==
            STATUS_SUCCESS &&
        LecDmaPnpBeginTeardown(
            &teardownRetryPnpStage, LecDmaPnpTeardownRemove) ==
                STATUS_SUCCESS &&
        LecDmaPnpFinishTeardownNoLaunch(&teardownRetryPnpStage) ==
            STATUS_SUCCESS && LecDmaPnpCanDestroy(&teardownRetryPnpStage));
    check("all releasable PnP-stage resources are balanced",
        poolOutstanding == 0 && pnpDevice.References == 0 &&
        failedPnpDevice.References == 0 &&
        surprisePnpDevice.References == 0 &&
        mapRacePnpDevice.References == 0 &&
        releaseRacePnpDevice.References == 0 &&
        removePnpDevice.References == 0 &&
        teardownRetryPnpDevice.References == 0);

    memset(&quarantinePnpDevice, 0, sizeof(quarantinePnpDevice));
    LecDmaPnpStageConstruct(&quarantinePnpStage, &quarantinePnpDevice);
    check("quarantine fixture owns adapter descriptor and mapping",
        LecDmaPnpStartNoLaunch(
            &quarantinePnpStage, 0x100000, 4096) == STATUS_SUCCESS &&
        LecDmaPnpMapNoLaunch(
            &quarantinePnpStage, mdls, 128, &id) == STATUS_SUCCESS);
    freesBefore = freeCalls;
    commonFreesAtQuarantine = commonFreeCalls;
    putsAtQuarantine = putCalls;
    check("unknown-active transition latches permanent quarantine",
        LecDmaPnpQuarantineUnknownActive(&quarantinePnpStage) ==
            STATUS_SUCCESS);
    LecDmaPnpSnapshot(&quarantinePnpStage, &pnpSnapshot);
    check("quarantine rejects release remove and destruction",
        pnpSnapshot.State == LecDmaPnpQuarantined &&
        pnpSnapshot.UnknownActive && !pnpSnapshot.AdmissionOpen &&
        LecDmaPnpReleaseNoLaunch(&quarantinePnpStage, id) ==
            STATUS_DELETE_PENDING &&
        LecDmaPnpBeginTeardown(
            &quarantinePnpStage, LecDmaPnpTeardownRemove) ==
                STATUS_DEVICE_BUSY &&
        LecDmaPnpFinishTeardownNoLaunch(&quarantinePnpStage) ==
            STATUS_DEVICE_BUSY &&
        !LecDmaPnpCanDestroy(&quarantinePnpStage));
    check("quarantine retains mapping adapter table PDO and MDL owner",
        freeCalls == freesBefore &&
        commonFreeCalls == commonFreesAtQuarantine &&
        putCalls == putsAtQuarantine && poolOutstanding == 2 &&
        quarantinePnpDevice.References == 2);

    CloseHandle(getEntered);
    CloseHandle(allowGet);
    CloseHandle(freeEntered);
    CloseHandle(allowFree);

    printf("SG SYNC V3: %u/%u passed; %u failed.\n",
        passed, passed + failed, failed);
    return failed ? 1 : 0;
}
