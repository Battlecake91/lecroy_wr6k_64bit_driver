/*
 * Runs the REAL inactive SG bridge against an in-memory WDM mock.
 * All addresses/buffers are synthetic; never opens a device or loads SYS.
 */
#include <stdio.h>
#include "../../driver/DmaScatterGatherStage.h"

static unsigned passed, failed, puts;
static int mode;
static PDRIVER_LIST_CONTROL pending;
static void* pendingContext;
static FAKE_DEVICE dev;
static DMA_ADAPTER adapter;
static DMA_OPERATIONS ops;
static MDL mdl;
static char bytes[128];
static SCATTER_GATHER_LIST sg;

static void check(const char* label, int ok) {
    if (ok) { ++passed; printf("[PASS] %s\n",label); }
    else { ++failed; printf("[FAIL] %s\n",label); }
}

static NTSTATUS fakeGet(PDMA_ADAPTER a, PDEVICE_OBJECT d, PMDL m,
                        void* address, ULONG size, PDRIVER_LIST_CONTROL cb,
                        void* context, BOOLEAN writeToDevice) {
    (void)a; (void)m; (void)address; (void)size; (void)writeToDevice;
    pending=cb; pendingContext=context;
    if (mode == 1) cb(d, NULL, &sg, context);  /* reentrant */
    if (mode == 3) return STATUS_INSUFFICIENT_RESOURCES;
    return STATUS_SUCCESS;
}
static void fakePut(PDMA_ADAPTER a, PSCATTER_GATHER_LIST list,
                    BOOLEAN writeToDevice) {
    (void)a; (void)list; (void)writeToDevice; ++puts;
}
static DWORD WINAPI delayedCallback(void* unused) {
    (void)unused;
    pending(&dev, NULL, &sg, pendingContext);
    return 0;
}
static void init(void) {
    memset(&dev,0,sizeof(dev));
    memset(&mdl,0,sizeof(mdl));
    memset(&sg,0,sizeof(sg));
    puts=0;
    ops.GetScatterGatherList=fakeGet;
    ops.PutScatterGatherList=fakePut;
    adapter.DmaOperations=&ops;
    mdl.Va=bytes; mdl.Size=(ULONG)sizeof(bytes); mdl.MdlFlags=MDL_PAGES_LOCKED;
    sg.NumberOfElements=2;
    sg.Elements[0].Address.QuadPart=0x10000;
    sg.Elements[0].Length=64;
    sg.Elements[1].Address.QuadPart=0x20000;
    sg.Elements[1].Length=64;
}
int main(void) {
    PLECS65_SG_STAGE stage;
    SCATTER_GATHER_ELEMENT copy[3];
    ULONG count=77;
    NTSTATUS st;
    HANDLE worker;

    init();
    mode=1;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    check("inline callback and submit return",NT_SUCCESS(st) &&
          stage->CallbackComplete && stage->SubmissionReturned);
    check("copies exact SG elements, no borrowed pointer",
          LecSgStageCopySegments(stage,copy,3,&count)==STATUS_SUCCESS &&
          count==2 && copy[0].Address.QuadPart==0x10000 &&
          copy[1].Length==64);
    check("small copy buffer rejected",
          LecSgStageCopySegments(stage,copy,1,&count)==STATUS_BUFFER_TOO_SMALL &&
          count==0);
    check("stage launch requires completed mapping",
          LecSgStageMarkLaunched(stage) &&
          LecSgStageCopySegments(stage,copy,3,&count)!=STATUS_SUCCESS);
    check("active mapping refuses release",
          LecSgStageRelease(stage,TRUE)==STATUS_DEVICE_BUSY && puts==0);
    check("closed stage cannot relaunch",!LecSgStageMarkLaunched(stage));
    check("duplicate release cannot free stage",
          LecSgStageRelease(stage,TRUE)==STATUS_DEVICE_BUSY && puts==0);

    mode=0;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    check("delayed callback cannot be assumed ready",
          NT_SUCCESS(st) && !stage->CallbackComplete &&
          LecSgStageCopySegments(stage,copy,3,&count)==STATUS_DEVICE_NOT_READY);
    worker=CreateThread(NULL,0,delayedCallback,NULL,0,NULL);
    WaitForSingleObject(worker,INFINITE);
    CloseHandle(worker);
    check("delayed callback data available after return",
          LecSgStageCopySegments(stage,copy,3,&count)==STATUS_SUCCESS &&
          count==2);
    check("release with callback notification still blocked",
          LecSgStageRelease(stage,TRUE)==STATUS_DEVICE_BUSY && puts==0);

    mode=0;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    check("STOP with callback pending quarantines",
          NT_SUCCESS(st) &&
          LecSgStageRelease(stage,FALSE)==STATUS_DEVICE_BUSY &&
          stage->Unsafe);
    pending(&dev,NULL,&sg,pendingContext);
    check("late callback cannot revive quarantined mapping",
          !LecSgStageMarkLaunched(stage) &&
          LecSgStageCopySegments(stage,copy,3,&count)!=STATUS_SUCCESS &&
          puts==0);
    check("FDO references deliberately persist for late callbacks",
          dev.References==3);

    mode=0;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    worker=CreateThread(NULL,0,delayedCallback,NULL,0,NULL);
    /* This races release against callback notification under Stage->Lock. */
    check("concurrent REMOVE and delayed callback never unmap",
          NT_SUCCESS(st) &&
          LecSgStageRelease(stage,FALSE)==STATUS_DEVICE_BUSY);
    WaitForSingleObject(worker,INFINITE);
    CloseHandle(worker);
    check("concurrent callback cannot revive REMOVE stage",
          stage->Unsafe &&
          LecSgStageCopySegments(stage,copy,3,&count)!=STATUS_SUCCESS &&
          puts==0);

    mode=3;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    check("failed Get submission retains callback stage",
          !NT_SUCCESS(st) && stage->Unsafe && stage->SubmissionReturned);
    pending(&dev,NULL,&sg,pendingContext);
    check("failure with late callback still cannot release",
          LecSgStageRelease(stage,TRUE)==STATUS_DEVICE_BUSY &&
          LecSgStageCopySegments(stage,copy,3,&count)!=STATUS_SUCCESS &&
          puts==0);

    mode=1;
    st=LecSgStageMap(&adapter,&dev,&mdl,128,&stage);
    pending(&dev,NULL,&sg,pendingContext);
    check("duplicate callbacks quarantine rather than double Put",
          NT_SUCCESS(st) && stage->Unsafe && puts==0);

    check("reject locked-MDL contract violation", (mdl.MdlFlags=0,
          LecSgStageMap(&adapter,&dev,&mdl,128,&stage))==
          STATUS_INVALID_PARAMETER);
    printf("SG BRIDGE MOCK: %u/%u passed; %u failed.\n",
           passed,passed+failed,failed);
    return failed ? 1 : 0;
}
