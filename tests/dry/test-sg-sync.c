/* Actual inactive synchronous DMA v3 bridge under fake WDM DDIs. */
#include <stdio.h>
#include "../../driver/DmaSyncStage.h"

static unsigned passed, failed, initCalls, getCalls, freeCalls;
static unsigned deferredCallbacks, stopBusy;
static int mode;
static DMA_ADAPTER adapter;
static DMA_OPERATIONS ops;
static FAKE_DEVICE device;
static LECS65_SG_SYNC_OWNER owner;
static MDL mdls[2];
static SCATTER_GATHER_LIST sg;
static char bytes[128];

static void check(const char* label, int ok) {
    printf("[%s] %s\n",ok?"PASS":"FAIL",label);
    if(ok) ++passed; else ++failed;
}
static NTSTATUS fakeInit(PDMA_ADAPTER a, void* ctx) {
    (void)a; ++initCalls;
    if (mode==2) return STATUS_INSUFFICIENT_RESOURCES;
    memset(ctx,0,DMA_TRANSFER_CONTEXT_SIZE_V1);
    return STATUS_SUCCESS;
}
static NTSTATUS fakeGet(PDMA_ADAPTER a, PDEVICE_OBJECT d, void* ctx,
                        PMDL m, ULONGLONG off, ULONG size, ULONG flags,
                        PDRIVER_LIST_CONTROL cb, void* arg,
                        BOOLEAN write, void* comp, void* compCtx,
                        PSCATTER_GATHER_LIST* list) {
    (void)a;(void)d;(void)ctx;(void)m;(void)off;(void)size;
    (void)arg;(void)write;(void)comp;(void)compCtx;
    ++getCalls;
    if (mode==3 && LecSgSyncOwnerStop(&owner)==STATUS_DEVICE_BUSY)
        ++stopBusy;
    if (flags!=DMA_SYNCHRONOUS_CALLBACK || cb!=NULL)
        ++deferredCallbacks;
    if (mode==1) return STATUS_INSUFFICIENT_RESOURCES;
    *list=&sg;
    return STATUS_SUCCESS;
}
static VOID fakeFree(PDMA_ADAPTER a,int action) {
    (void)a;
    if(action==DeallocateObject) ++freeCalls;
}
static void setup(void) {
    memset(&ops,0,sizeof(ops));
    memset(&adapter,0,sizeof(adapter));
    memset(&device,0,sizeof(device));
    memset(mdls,0,sizeof(mdls));
    memset(&sg,0,sizeof(sg));
    ops.InitializeDmaTransferContext=fakeInit;
    ops.GetScatterGatherListEx=fakeGet;
    ops.FreeAdapterObject=fakeFree;
    adapter.DmaOperations=&ops;
    LecSgSyncOwnerInit(&owner,&adapter,&device);
    mdls[0].Va=bytes; mdls[0].Size=128;
    mdls[0].MdlFlags=MDL_PAGES_LOCKED;
    sg.NumberOfElements=2;
    sg.Elements[0].Address.QuadPart=0x10000;
    sg.Elements[0].Length=64;
    sg.Elements[1].Address.QuadPart=0x20000;
    sg.Elements[1].Length=64;
    passed=failed=initCalls=getCalls=freeCalls=0;
    deferredCallbacks=stopBusy=0; mode=0;
}
typedef struct _COPY_RACE {
    ULONGLONG Token;
    volatile LONG Errors;
} COPY_RACE;
static DWORD WINAPI concurrentCopy(void* ctx) {
    COPY_RACE* race=(COPY_RACE*)ctx;
    SCATTER_GATHER_ELEMENT els[3];
    ULONG n;
    unsigned i;
    for(i=0;i<3000;i++) {
        NTSTATUS s=LecSgSyncCopySegments(
            &owner,race->Token,els,3,&n);
        if(!(s==STATUS_INVALID_PARAMETER ||
            (s==STATUS_SUCCESS && n==2 &&
             els[0].Address.QuadPart==0x10000))) {
            InterlockedIncrement(&race->Errors);
        }
    }
    return 0;
}
int main(void) {
    ULONGLONG id=0, second=0;
    SCATTER_GATHER_ELEMENT els[3];
    ULONG count=0;
    NTSTATUS st;
    COPY_RACE race;
    HANDLE worker;

    setup();
    st=LecSgSyncMapNoLaunch(&owner,&mdls[0],128,&id);
    check("GetEx uses inline no-callback DMA v3",
          NT_SUCCESS(st)&&id!=0&&deferredCallbacks==0&&
          initCalls==1&&getCalls==1);
    check("mapped stage holds PDO",device.References==1);
    check("copied SG values match",LecSgSyncCopySegments(
          &owner,id,els,3,&count)==STATUS_SUCCESS &&
          count==2&&els[0].Length==64);
    check("bounded copy checks size",LecSgSyncCopySegments(
          &owner,id,els,1,&count)==STATUS_BUFFER_TOO_SMALL && count==0);
    check("parent cannot teardown before STOP",
          !LecSgSyncOwnerCanTeardown(&owner));
    st=LecSgSyncMapNoLaunch(&owner,&mdls[0],128,&second);
    check("distinct mappings receive distinct IDs",
          NT_SUCCESS(st)&&second!=id&&second!=0&&
          owner.Outstanding==2);

    race.Token=id;race.Errors=0;
    worker=CreateThread(NULL,0,concurrentCopy,&race,0,NULL);
    check("first release unmaps exactly once",
          LecSgSyncReleaseNoLaunch(&owner,id)==STATUS_SUCCESS &&
          freeCalls==1);
    WaitForSingleObject(worker,INFINITE);
    CloseHandle(worker);
    check("copy versus release cannot see freed mapping",
          race.Errors==0);
    check("stale token cannot double release",
          LecSgSyncReleaseNoLaunch(&owner,id)==STATUS_INVALID_PARAMETER &&
          freeCalls==1);
    check("other mapping stays accessible",LecSgSyncCopySegments(
          &owner,second,els,3,&count)==STATUS_SUCCESS);
    (void)LecSgSyncReleaseNoLaunch(&owner,second);
    check("successful no-launch cleanup balances refs",
          device.References==0&&owner.Outstanding==0&&freeCalls==2);

    mode=1;
    st=LecSgSyncMapNoLaunch(&owner,mdls,128,&id);
    check("resource shortage never queues callback",
          st==STATUS_INSUFFICIENT_RESOURCES&&id==0&&
          device.References==0&&owner.Outstanding==0);
    mode=2;
    st=LecSgSyncMapNoLaunch(&owner,mdls,128,&id);
    check("transfer-context failure drains references",
          st==STATUS_INSUFFICIENT_RESOURCES&&id==0&&
          device.References==0&&owner.Outstanding==0);
    mode=0;

    mdls[0].Size=0x02000000U;mdls[0].Next=&mdls[1];
    mdls[1].MdlFlags=MDL_PAGES_LOCKED;
    mdls[1].Size=0x01000000U;
    sg.Elements[0].Length=0x02000000U;
    sg.Elements[1].Length=0x01000000U;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&id);
    check("48-MiB two-MDL mapping",NT_SUCCESS(st)&&id!=0);
    check("SG byte coverage across MDLs",LecSgSyncCopySegments(
          &owner,id,els,3,&count)==STATUS_SUCCESS);
    (void)LecSgSyncReleaseNoLaunch(&owner,id);

    sg.Elements[1].Length=0x01000004U;
    (void)LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&id);
    check("truncated or excessive SG map rejected",
          LecSgSyncCopySegments(&owner,id,els,3,&count)
              ==STATUS_INVALID_BUFFER_SIZE);
    (void)LecSgSyncReleaseNoLaunch(&owner,id);
    sg.Elements[1].Length=0x01000000U;
    sg.Elements[1].Address.QuadPart=0x100000000ULL;
    (void)LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&id);
    check("out-of-range 64-bit device address rejected",
          LecSgSyncCopySegments(&owner,id,els,3,&count)
              ==STATUS_INVALID_BUFFER_SIZE);
    (void)LecSgSyncReleaseNoLaunch(&owner,id);
    sg.Elements[1].Address.QuadPart=0x20000;

    mdls[1].MdlFlags=0;
    check("reject unpinned second MDL",
          LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&id)
              ==STATUS_INVALID_PARAMETER && id==0);
    mdls[1].MdlFlags=MDL_PAGES_LOCKED;

    mode=3;
    st=LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&id);
    check("STOP while allocating observes outstanding slot",
          NT_SUCCESS(st)&&id!=0&&stopBusy==1);
    check("STOP blocks new mapping",LecSgSyncMapNoLaunch(
          &owner,mdls,0x03000000U,&second)==STATUS_DEVICE_BUSY &&
          second==0);
    check("STOP cannot destroy live mapping",
          !LecSgSyncOwnerCanTeardown(&owner));
    check("release drains owner after STOP",
          LecSgSyncReleaseNoLaunch(&owner,id)==STATUS_SUCCESS &&
          LecSgSyncOwnerCanTeardown(&owner)&&device.References==0);
    check("STOP permanently closes admission",
          LecSgSyncMapNoLaunch(&owner,mdls,0x03000000U,&second)
              ==STATUS_DEVICE_BUSY);
    check("all successful maps freed once",
          freeCalls==6 && deferredCallbacks==0);

    printf("SG SYNC V3: %u/%u passed; %u failed.\n",
           passed,passed+failed,failed);
    return failed?1:0;
}
