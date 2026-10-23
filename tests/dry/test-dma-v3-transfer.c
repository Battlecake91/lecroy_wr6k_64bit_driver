#include "../../driver/DmaV3Transfer.h"
#include <stdio.h>

enum { NORMAL, INFO_FAIL, INIT_FAIL, ALLOC_FAIL, ALLOC_NULL,
    MAP_FAIL, MAP_PARTIAL, MAP_BAD_LIST, MAP_HIGH_ADDRESS, FLUSH_FAIL };
static int mode;
static int fail_pool;
static unsigned checks, info_calls, init_calls, alloc_calls, map_calls;
static unsigned flush_calls, free_calls, pool_allocs, pool_frees, bad_args;
static DMA_OPERATIONS operations;
static DMA_ADAPTER adapter;
static FAKE_DEVICE device;
__declspec(align(4096)) static LECS65_DMA_LAYOUT_ENTRY commonTable[512];

#define CHECK(x) do { checks++; if (!(x)) { \
    printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

void* FakeExAllocatePool2(ULONG flags, size_t bytes, ULONG tag)
{
    UNREFERENCED_PARAMETER(flags); UNREFERENCED_PARAMETER(tag);
    if (fail_pool) { fail_pool = 0; return NULL; }
    pool_allocs++; return calloc(1, bytes);
}
VOID FakeExFreePoolWithTag(void* p, ULONG tag)
{
    UNREFERENCED_PARAMETER(tag); if (p) { pool_frees++; free(p); }
}
PDMA_ADAPTER FakeIoGetDmaAdapter(PDEVICE_OBJECT d, PDEVICE_DESCRIPTION x, PULONG n)
{
    UNREFERENCED_PARAMETER(d); UNREFERENCED_PARAMETER(x); *n = 8; return &adapter;
}
static VOID PutAdapter(PDMA_ADAPTER a) { UNREFERENCED_PARAMETER(a); }
static PVOID AllocCommon(PDMA_ADAPTER a, ULONG n, LARGE_INTEGER* l, BOOLEAN c)
{ UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(n); UNREFERENCED_PARAMETER(l); UNREFERENCED_PARAMETER(c); return NULL; }
static VOID FreeCommon(PDMA_ADAPTER a, ULONG n, LARGE_INTEGER l, PVOID p, BOOLEAN c)
{ UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(n); UNREFERENCED_PARAMETER(l); UNREFERENCED_PARAMETER(p); UNREFERENCED_PARAMETER(c); }
static NTSTATUS InitContext(PDMA_ADAPTER a, PVOID c)
{ UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(c); init_calls++; return mode == INIT_FAIL ? STATUS_INTERNAL_ERROR : STATUS_SUCCESS; }
static VOID FreeObject(PDMA_ADAPTER a, int action)
{ UNREFERENCED_PARAMETER(a); if (action != DeallocateObject) bad_args++; free_calls++; }
static NTSTATUS GetInfo(PDMA_ADAPTER a, PMDL m, ULONGLONG o, ULONG n,
    BOOLEAN w, PDMA_TRANSFER_INFO info)
{
    UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(m);
    info_calls++;
    if (mode == INFO_FAIL) return STATUS_INTERNAL_ERROR;
    CHECK(o == 0 && n == 4096 && !w && info->Version == DMA_TRANSFER_INFO_VERSION1);
    info->V1.MapRegisterCount = mode == ALLOC_FAIL ? 2 : 1;
    info->V1.ScatterGatherElementCount = 2;
    info->V1.ScatterGatherListSize = (ULONG)(FIELD_OFFSET(SCATTER_GATHER_LIST, Elements) +
        2 * sizeof(SCATTER_GATHER_ELEMENT));
    return STATUS_SUCCESS;
}
static NTSTATUS AllocateChannel(PDMA_ADAPTER a, PDEVICE_OBJECT d, PVOID c,
    ULONG r, ULONG flags, PVOID routine, PVOID context, PVOID* base)
{
    UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(c);
    UNREFERENCED_PARAMETER(routine); UNREFERENCED_PARAMETER(context);
    alloc_calls++;
    CHECK(d == &device && r > 0 && flags == DMA_SYNCHRONOUS_CALLBACK);
    if (mode == ALLOC_FAIL) return STATUS_INSUFFICIENT_RESOURCES;
    *base = mode == ALLOC_NULL ? NULL : (PVOID)(ULONG_PTR)0x1234;
    return STATUS_SUCCESS;
}
static NTSTATUS MapTransfer(PDMA_ADAPTER a, PMDL m, PVOID base, ULONGLONG o,
    ULONG deviceOffset, PULONG length, BOOLEAN write, PSCATTER_GATHER_LIST list,
    ULONG bytes, PVOID completion, PVOID completionContext)
{
    UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(m);
    UNREFERENCED_PARAMETER(completion); UNREFERENCED_PARAMETER(completionContext);
    map_calls++;
    CHECK(base == (PVOID)(ULONG_PTR)0x1234 && o == 0 && deviceOffset == 0 &&
        !write && bytes >= FIELD_OFFSET(SCATTER_GATHER_LIST, Elements) +
            2 * sizeof(SCATTER_GATHER_ELEMENT));
    if (mode == MAP_FAIL) return STATUS_INTERNAL_ERROR;
    list->NumberOfElements = 2;
    list->Elements[0].Address.QuadPart = mode == MAP_HIGH_ADDRESS ? 0x100000000ULL : 0x100000;
    list->Elements[0].Length = 2048;
    list->Elements[1].Address.QuadPart = 0x200000;
    list->Elements[1].Length = mode == MAP_BAD_LIST ? 1024 : 2048;
    if (mode == MAP_PARTIAL) *length = 2048;
    return STATUS_SUCCESS;
}
static NTSTATUS Flush(PDMA_ADAPTER a, PMDL m, PVOID base, ULONGLONG o,
    ULONG length, BOOLEAN write)
{
    UNREFERENCED_PARAMETER(a); UNREFERENCED_PARAMETER(m);
    flush_calls++;
    CHECK(base == (PVOID)(ULONG_PTR)0x1234 && o == 0 && length > 0 && !write);
    return mode == FLUSH_FAIL ? STATUS_INTERNAL_ERROR : STATUS_SUCCESS;
}

static void Reset(void)
{
    memset(&operations, 0, sizeof(operations));
    operations.Size = sizeof(operations);
    operations.PutDmaAdapter = PutAdapter;
    operations.AllocateCommonBuffer = AllocCommon;
    operations.FreeCommonBuffer = FreeCommon;
    operations.InitializeDmaTransferContext = InitContext;
    operations.FreeAdapterObject = FreeObject;
    operations.GetDmaTransferInfo = GetInfo;
    operations.AllocateAdapterChannelEx = AllocateChannel;
    operations.MapTransferEx = MapTransfer;
    operations.FlushAdapterBuffersEx = Flush;
    adapter.DmaOperations = &operations;
    mode = NORMAL; info_calls = init_calls = alloc_calls = map_calls = 0;
    flush_calls = free_calls = pool_allocs = pool_frees = 0;
    fail_pool = 0; bad_args = 0;
}

static void InitAdapterContext(LECS65_DMA_ADAPTER_CONTEXT* c)
{
    memset(c, 0, sizeof(*c)); c->Adapter = &adapter;
    c->PhysicalDeviceObject = &device; c->NumberOfMapRegisters = 8;
    c->TableVirtual = commonTable; c->TableLength = sizeof(commonTable);
    c->TableLogical.QuadPart = 0x300000;
    KeInitializeSpinLock(&c->Lock);
}

int main(void)
{
    LECS65_DMA_V3_TRANSFER tx;
    LECS65_DMA_ADAPTER_CONTEXT context;
    MDL mdl = { (void*)0x10000, 4096, MDL_PAGES_LOCKED, NULL };
    NTSTATUS status;
    ULONG usedEntries;

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    CHECK(LecDmaV3PrepareNoLaunch(NULL, &context, &mdl, 4096) == STATUS_INVALID_PARAMETER);
    operations.Size = RTL_SIZEOF_THROUGH_FIELD(DMA_OPERATIONS, FreeAdapterObject);
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_PARAMETER);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    context.TableVirtual = NULL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_DEVICE_STATE);
    CHECK(context.SynchronousOwner == NULL && pool_allocs == 0);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = INFO_FAIL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INTERNAL_ERROR);
    CHECK(pool_allocs == 0 && free_calls == 0);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    fail_pool = 1;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INSUFFICIENT_RESOURCES);
    CHECK(tx.State == LecDmaV3MapEmpty && free_calls == 0);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = INIT_FAIL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INTERNAL_ERROR);
    CHECK(pool_allocs == pool_frees && free_calls == 0);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = ALLOC_FAIL;
    status = LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096);
    CHECK(status == STATUS_INSUFFICIENT_RESOURCES && free_calls == 0 && pool_allocs == pool_frees);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = ALLOC_NULL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_DEVICE_STATE);
    CHECK(free_calls == 0 && pool_allocs == pool_frees);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = MAP_FAIL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INTERNAL_ERROR);
    CHECK(free_calls == 1 && flush_calls == 0 && pool_allocs == pool_frees);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = MAP_PARTIAL;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_BUFFER_SIZE);
    CHECK(flush_calls == 1 && free_calls == 1 && pool_allocs == pool_frees);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = MAP_BAD_LIST;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_BUFFER_SIZE);
    CHECK(flush_calls == 1 && free_calls == 1);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    mode = MAP_HIGH_ADDRESS;
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_INVALID_BUFFER_SIZE);
    CHECK(flush_calls == 1 && free_calls == 1);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_SUCCESS);
    CHECK(tx.State == LecDmaV3MapReady && tx.MappingOwned && tx.ChannelAllocated);
    CHECK(tx.AdapterOwnerClaimed && context.SynchronousOwner == &tx &&
        tx.DescriptorSlotCapacity == 512);
    CHECK(info_calls == 1 && init_calls == 1 && alloc_calls == 1 && map_calls == 1);
    CHECK(LecDmaV3BuildCommonDescriptorsNoLaunch(&tx, &usedEntries) == STATUS_SUCCESS);
    CHECK(usedEntries == 3 && commonTable[0].DeviceAddress == 0x100000 &&
        commonTable[0].CountDwords == 512 &&
        commonTable[1].DeviceAddress == 0x200000 &&
        commonTable[1].CountDwords == 512 &&
        commonTable[2].DeviceAddress == 0 &&
        commonTable[2].CountDwords == 0);
    {
        LECS65_DMA_V3_TRANSFER second;
        LecDmaV3TransferInitialize(&second);
        CHECK(LecDmaV3PrepareNoLaunch(&second, &context, &mdl, 4096) ==
            STATUS_DEVICE_BUSY);
        CHECK(context.SynchronousOwner == &tx && second.State == LecDmaV3MapEmpty);
    }
    CHECK(LecDmaV3ReleaseNeverLaunched(&tx) == STATUS_SUCCESS);
    CHECK(tx.State == LecDmaV3MapReleased && !tx.AdapterOwnerClaimed &&
        context.SynchronousOwner == NULL && flush_calls == 1 && free_calls == 1 && bad_args == 0);
    CHECK(LecDmaV3ReleaseNeverLaunched(&tx) == STATUS_INVALID_DEVICE_STATE);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_SUCCESS);
    mode = FLUSH_FAIL;
    CHECK(LecDmaV3ReleaseNeverLaunched(&tx) == STATUS_INTERNAL_ERROR);
    CHECK(tx.State == LecDmaV3MapQuarantined && tx.MappingOwned &&
        tx.AdapterOwnerClaimed && free_calls == 0);
    CHECK(context.Quarantined && pool_allocs != pool_frees);

    Reset(); InitAdapterContext(&context); LecDmaV3TransferInitialize(&tx);
    CHECK(LecDmaV3PrepareNoLaunch(&tx, &context, &mdl, 4096) == STATUS_SUCCESS);
    LecDmaV3Quarantine(&tx);
    CHECK(tx.State == LecDmaV3MapQuarantined && context.Quarantined);
    CHECK(LecDmaV3ReleaseNeverLaunched(&tx) == STATUS_INVALID_DEVICE_STATE);
    CHECK(flush_calls == 0 && free_calls == 0);

    printf("DMA V3 EXPLICIT TRANSFER: %u/%u passed; hardware launch absent\n", checks, checks);
    return 0;
}
