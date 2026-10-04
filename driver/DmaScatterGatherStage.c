#include "DmaScatterGatherStage.h"

static
VOID
LecSgStageReady(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_opt_ PIRP Irp,
    _In_ PSCATTER_GATHER_LIST ScatterGather,
    _In_ PVOID Context)
{
    PLECS65_SG_STAGE stage = (PLECS65_SG_STAGE)Context;
    KIRQL irql;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    /*
     * Callback can be delayed; all callback memory and the FDO reference
     * survive until a separately authorized release.
     */
    KeAcquireSpinLock(&stage->Lock, &irql);
    if (!stage->CallbackComplete) {
        stage->List = ScatterGather;
        stage->CallbackComplete = TRUE;
        if (!LecMapOwnerCallback(&stage->Owner)) stage->Unsafe = TRUE;
    }
    else {
        /* Unexpected duplicate callback: never free an uncertain map. */
        stage->Unsafe = TRUE;
    }
    KeReleaseSpinLock(&stage->Lock, irql);
}

NTSTATUS
LecSgStageMap(
    _In_ PDMA_ADAPTER Adapter,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMDL LockedMdl,
    _In_ ULONG Length,
    _Outptr_ PLECS65_SG_STAGE* Result)
{
    PLECS65_SG_STAGE stage;
    KIRQL oldIrql;
    NTSTATUS status;

    if (Result == NULL) return STATUS_INVALID_PARAMETER;
    *Result = NULL;
    if (Adapter == NULL || Adapter->DmaOperations == NULL ||
        Adapter->DmaOperations->GetScatterGatherList == NULL ||
        DeviceObject == NULL || LockedMdl == NULL ||
        Length == 0 || Length > MmGetMdlByteCount(LockedMdl) ||
        (LockedMdl->MdlFlags & MDL_PAGES_LOCKED) == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    stage = (PLECS65_SG_STAGE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*stage), LECS65_TAG);
    if (stage == NULL) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(stage, sizeof(*stage));
    KeInitializeSpinLock(&stage->Lock);
    LecMapOwnerInit(&stage->Owner);
    (VOID)LecMapOwnerRequest(&stage->Owner);
    stage->Adapter = Adapter;
    stage->DeviceObject = DeviceObject;
    stage->WriteToDevice = FALSE; /* WR6k acquisition writes into host RAM. */
    ObReferenceObject(DeviceObject);

    /*
     * WDM scatter/gather operations require DISPATCH_LEVEL. The callback
     * may run synchronously, before GetScatterGatherList returns.
     */
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    status = Adapter->DmaOperations->GetScatterGatherList(
        Adapter, DeviceObject, LockedMdl,
        MmGetMdlVirtualAddress(LockedMdl), Length,
        LecSgStageReady, stage, stage->WriteToDevice);
    KeLowerIrql(oldIrql);

    if (!NT_SUCCESS(status)) {
        /*
         * On a failed submission no callback ownership guarantee has been
         * established for this prototype. Keep the stage and its FDO ref
         * rather than risking a late callback into freed pool. This is an
         * intentional conservative leak, not a finished error path.
         */
        stage->Unsafe = TRUE;
        LecMapOwnerUncertain(&stage->Owner);
        *Result = stage;
        return status;
    }

    stage->Submitted = TRUE;
    *Result = stage;
    return status;
}

BOOLEAN
LecSgStagePeek(
    _Inout_ PLECS65_SG_STAGE Stage,
    _Outptr_result_maybenull_ PSCATTER_GATHER_LIST* List)
{
    KIRQL irql;
    BOOLEAN ready;

    if (List == NULL) return FALSE;
    *List = NULL;
    if (Stage == NULL) return FALSE;

    KeAcquireSpinLock(&Stage->Lock, &irql);
    ready = (BOOLEAN)(Stage->CallbackComplete &&
        !Stage->Unsafe && !Stage->PutStarted &&
        Stage->Owner.Phase == LecMapReady &&
        Stage->List != NULL);
    if (ready) *List = Stage->List;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return ready;
}

NTSTATUS
LecSgStageRelease(
    _Inout_ PLECS65_SG_STAGE Stage,
    _In_ BOOLEAN ProvenIdle)
{
    KIRQL irql;
    KIRQL oldIrql;
    PSCATTER_GATHER_LIST list;

    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);

    if (!ProvenIdle) {
        Stage->Unsafe = TRUE;
        LecMapOwnerUncertain(&Stage->Owner);
    }
    if (Stage->Unsafe || !Stage->CallbackComplete ||
        Stage->List == NULL || Stage->PutStarted ||
        !ProvenIdle || !LecMapOwnerMayRelease(&Stage->Owner)) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }

    Stage->PutStarted = TRUE;
    (VOID)LecMapOwnerReleased(&Stage->Owner);
    list = Stage->List;
    KeReleaseSpinLock(&Stage->Lock, irql);

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    Stage->Adapter->DmaOperations->PutScatterGatherList(
        Stage->Adapter, list, Stage->WriteToDevice);
    KeLowerIrql(oldIrql);

    ObDereferenceObject(Stage->DeviceObject);
    ExFreePoolWithTag(Stage, LECS65_TAG);
    return STATUS_SUCCESS;
}

BOOLEAN
LecSgStageMarkLaunched(_Inout_ PLECS65_SG_STAGE Stage)
{
    KIRQL irql;
    BOOLEAN result;

    if (Stage == NULL) return FALSE;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    result = (BOOLEAN)(!Stage->Unsafe && !Stage->PutStarted &&
        LecMapOwnerLaunch(&Stage->Owner));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return result;
}

BOOLEAN
LecSgStageMarkIdleProved(_Inout_ PLECS65_SG_STAGE Stage)
{
    KIRQL irql;
    BOOLEAN result;

    if (Stage == NULL) return FALSE;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    result = (BOOLEAN)(!Stage->Unsafe &&
        LecMapOwnerIdleProved(&Stage->Owner));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return result;
}
