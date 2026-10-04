/*
 * Synchronous v3 WDM DMA stage: fake adapter only, never real hardware.
 * Checks that failed resource allocation cannot queue callbacks and that
 * no-launch cleanup returns the adapter resources exactly once.
 */
#include <stdio.h>
#include "../../driver/DmaSyncStage.h"

static unsigned passed, failed;
static unsigned initCalls, getCalls, freeCalls, deferredCallbacks;
static int mode;
static DMA_ADAPTER adapter;
static DMA_OPERATIONS ops;
static FAKE_DEVICE device;
static LECS65_SG_SYNC_OWNER owner;
static unsigned stopBusy;
static MDL mdls[2];
static SCATTER_GATHER_LIST sg;
static char buf[128];

static void check(const char* label, int result) {
    printf("[%s] %s\n", result ? "PASS" : "FAIL", label);
    if (result) ++passed; else ++failed;
}
static NTSTATUS fakeInitialize(PDMA_ADAPTER a, void* ctx) {
    (void)a; ++initCalls;
    if (mode==2) return STATUS_INSUFFICIENT_RESOURCES;
    memset(ctx,0,DMA_TRANSFER_CONTEXT_SIZE_V1);
    return STATUS_SUCCESS;
}
static NTSTATUS fakeEx(PDMA_ADAPTER a, PDEVICE_OBJECT d,
                       void* ctx, PMDL m, ULONGLONG offset, ULONG len,
                       ULONG flags, PDRIVER_LIST_CONTROL cb, void* arg,
                       BOOLEAN toDevice, void* completion, void* cc,
                       PSCATTER_GATHER_LIST* list) {
    (void)a;(void)d;(void)ctx;(void)m;(void)offset;
    (void)len;(void)arg;(void)toDevice;(void)completion;(void)cc;
    ++getCalls;
    if (mode==3) {
        if (LecSgSyncOwnerStop(&owner)==STATUS_DEVICE_BUSY)
            ++stopBusy;
    }
    if (flags!=DMA_SYNCHRONOUS_CALLBACK || cb!=NULL)
        ++deferredCallbacks;
    if (mode==1) return STATUS_INSUFFICIENT_RESOURCES;
    *list=&sg;
    return STATUS_SUCCESS;
}
static VOID fakeFree(PDMA_ADAPTER a, int action) {
    (void)a;
    if(action==DeallocateObject) ++freeCalls;
}
static void setup(void) {
    memset(&ops,0,sizeof(ops));
    memset(&adapter,0,sizeof(adapter));
    memset(&device,0,sizeof(device));
    memset(&mdls,0,sizeof(mdls));
    memset(&sg,0,sizeof(sg));
    ops.InitializeDmaTransferContext=fakeInitialize;
    ops.GetScatterGatherListEx=fakeEx;
    ops.FreeAdapterObject=fakeFree;
    adapter.DmaOperations=&ops;
    LecSgSyncOwnerInit(&owner, &adapter, &device);
    stopBusy=0;
    mdls[0].Va=buf;mdls[0].Size=128;
    mdls[0].MdlFlags=MDL_PAGES_LOCKED;
    sg.NumberOfElements=2;
    sg.Elements[0].Address.QuadPart=0x10000;
    sg.Elements[0].Length=64;
    sg.Elements[1].Address.QuadPart=0x20000;
    sg.Elements[1].Length=64;
    initCalls=getCalls=freeCalls=deferredCallbacks=0;
    mode=0;
}
int main(void) {
    PLECS65_SG_SYNC_STAGE stage=NULL;
    PLECS65_SG_SYNC_STAGE extra=NULL;
    SCATTER_GATHER_ELEMENT elements[3];
    ULONG n=9;
    NTSTATUS st;

    setup();
    st=LecSgSyncMapNoLaunch(&owner,&mdls[0],128,&stage);
    check("mapping uses synchronous callback-free v3 DDI",
          NT_SUCCESS(st)&&stage!=NULL&&initCalls==1&&getCalls==1&&
          deferredCallbacks==0);
    check("mapping holds device reference",device.References==1);
    check("bounded SG snapshot",LecSgSyncCopySegments(stage,elements,3,&n)
          ==STATUS_SUCCESS&&n==2&&elements[0].Length==64);
    check("insufficient SG copy capacity",
          LecSgSyncCopySegments(stage,elements,1,&n)
          ==STATUS_BUFFER_TOO_SMALL&&n==0);
    check("no-launch map releases adapter exactly once",
          LecSgSyncReleaseNoLaunch(&stage)==STATUS_SUCCESS &&
          stage==NULL&&freeCalls==1&&device.References==0);
    check("sequential double release refused",
          LecSgSyncReleaseNoLaunch(&stage)==STATUS_INVALID_PARAMETER &&
          freeCalls==1);

    mode=1;
    st=LecSgSyncMapNoLaunch(&owner,&mdls[0],128,&stage);
    check("no-resource failure never creates outstanding stage",
          st==STATUS_INSUFFICIENT_RESOURCES&&stage==NULL&&
          device.References==0&&freeCalls==1);
    mode=2;
    st=LecSgSyncMapNoLaunch(&owner,&mdls[0],128,&stage);
    check("context initialization failure releases FDO ref",
          st==STATUS_INSUFFICIENT_RESOURCES&&stage==NULL&&
          device.References==0);
    mode=0;

    mdls[0].Size=0x02000000U; mdls[1].Size=0x01000000U;
    mdls[0].Next=&mdls[1];mdls[1].MdlFlags=MDL_PAGES_LOCKED;
    sg.Elements[0].Length=0x02000000U;
    sg.Elements[1].Length=0x01000000U;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&stage);
    check("48-MiB two-MDL mapping accepted", NT_SUCCESS(st)&&stage!=NULL);
    check("multiple MDLs total segment coverage",
          LecSgSyncCopySegments(stage,elements,3,&n)==STATUS_SUCCESS &&
          n==2);
    (void)LecSgSyncReleaseNoLaunch(&stage);
    check("multi-MDL no-launch cleanup balanced",device.References==0);

    sg.Elements[1].Length=0x01000004U;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&stage);
    check("reject SG mapping beyond requested bytes",
          NT_SUCCESS(st)&&LecSgSyncCopySegments(stage,elements,3,&n)
          ==STATUS_INVALID_BUFFER_SIZE);
    (void)LecSgSyncReleaseNoLaunch(&stage);
    sg.Elements[1].Length=0x01000000U;
    sg.Elements[1].Address.QuadPart=0x100000000ULL;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&stage);
    check("reject 64-bit device logical address",
          NT_SUCCESS(st)&&LecSgSyncCopySegments(stage,elements,3,&n)
          ==STATUS_INVALID_BUFFER_SIZE);
    (void)LecSgSyncReleaseNoLaunch(&stage);
    sg.Elements[1].Address.QuadPart=0x20000;
    mdls[1].MdlFlags=0;
    check("reject unpinned second MDL",
          LecSgSyncMapNoLaunch(&owner,mdls,
          0x03000000U,&stage)==STATUS_INVALID_PARAMETER);
    check("balanced references with faults",device.References==0);
    check("STOP gate not yet engaged",
          !LecSgSyncOwnerCanTeardown(&owner));
    mode=3;
    mdls[1].MdlFlags=MDL_PAGES_LOCKED;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&stage);
    check("STOP while GetEx in-flight reports busy",
          NT_SUCCESS(st)&&stopBusy==1&&stage!=NULL);
    check("STOP prevents new submissions",
          LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,
                              &extra)==
              STATUS_DEVICE_BUSY);
    check("STOP cannot release active mapping owner",
          !LecSgSyncOwnerCanTeardown(&owner));
    check("no-launch release drains owner",
          LecSgSyncReleaseNoLaunch(&stage)==STATUS_SUCCESS&&
          LecSgSyncOwnerCanTeardown(&owner)&&device.References==0);
    check("STOP stays closed after drain",
          LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&extra)
              ==STATUS_DEVICE_BUSY && extra==NULL);


    printf("SG SYNC V3: %u/%u passed; %u failed.\n",
           passed,passed+failed,failed);
    return failed ? 1:0;
}
