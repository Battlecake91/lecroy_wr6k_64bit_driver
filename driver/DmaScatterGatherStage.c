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
     * Both a synchronous callback (within GetScatterGatherList) and a
     * delayed callback use the same lock. Completion means the fields
     * have been stored, NOT that this callback has returned.
     */
    KeAcquireSpinLock(&stage->Lock, &irql);
    if (!stage->CallbackComplete && ScatterGather != NULL &&
        LecMapOwnerCallback(&stage->Owner)) {
        stage->List = ScatterGather;
        stage->CallbackComplete = TRUE;
    }
    else {
        stage->Unsafe = TRUE;
        LecMapOwnerUncertain(&stage->Owner);
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
    KIRQL irql;
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
    stage->SourceMdl = LockedMdl;
    stage->RequestedLength = Length;
    stage->WriteToDevice = FALSE;  /* Acquisition: device writes host. */
    ObReferenceObject(DeviceObject);

    /*
     * GetScatterGatherList expects DISPATCH_LEVEL. Callback can run
     * inline or later; no return path can free callback context, source
     * MDL, adapter, or FDO until an independent rundown is implemented.
     * Therefore even a failed submission is deliberately quarantined.
     */
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    status = Adapter->DmaOperations->GetScatterGatherList(
        Adapter, DeviceObject, LockedMdl,
        MmGetMdlVirtualAddress(LockedMdl), Length,
        LecSgStageReady, stage, stage->WriteToDevice);
    KeLowerIrql(oldIrql);

    KeAcquireSpinLock(&stage->Lock, &irql);
    stage->SubmissionReturned = TRUE;
    if (!NT_SUCCESS(status)) {
        stage->Unsafe = TRUE;
        LecMapOwnerUncertain(&stage->Owner);
    }
    KeReleaseSpinLock(&stage->Lock, irql);

    *Result = stage;
    return status;
}

NTSTATUS
LecSgStageCopySegments(
    _Inout_ PLECS65_SG_STAGE Stage,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied)
{
    KIRQL irql;
    ULONG count;
    NTSTATUS status = STATUS_DEVICE_NOT_READY;

    if (Copied == NULL || Elements == NULL || Capacity == 0 ||
        Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Copied = 0;
    KeAcquireSpinLock(&Stage->Lock, &irql);

    /*
     * Never hand out a raw pointer that could be invalidated by Put.
     * There is currently no Put path; a future one must use the same
     * reader/rundown barrier, not just this spin lock.
     */
    if (Stage->SubmissionReturned && Stage->CallbackComplete &&
        !Stage->Unsafe && !Stage->Closing && !Stage->PutStarted &&
        Stage->Owner.Phase == LecMapReady && Stage->List != NULL) {
        count = Stage->List->NumberOfElements;
        if (count == 0) {
            status = STATUS_INVALID_BUFFER_SIZE;
        }
        else if (count > Capacity) {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else {
            ULONG i;
            ULONGLONG total = 0;

            /*
             * Windows returns device-logical addresses. Even a valid
             * callback must not silently accept a truncated, malformed
             * or non-32-bit mapping. This does not prove bus idleness.
             */
            status = STATUS_SUCCESS;
            for (i = 0; i < count; ++i) {
                const SCATTER_GATHER_ELEMENT* element =
                    &Stage->List->Elements[i];
                ULONGLONG address =
                    (ULONGLONG)element->Address.QuadPart;
                ULONG length = element->Length;
                if (length == 0 || (length & 3U) != 0 ||
                    (address & 3U) != 0 ||
                    address > MAXULONG ||
                    (ULONGLONG)length - 1U >
                        MAXULONG - address) {
                    status = STATUS_INVALID_BUFFER_SIZE;
                    break;
                }
                total += length;
                if (total > Stage->RequestedLength) {
                    status = STATUS_INVALID_BUFFER_SIZE;
                    break;
                }
            }
            if (total != Stage->RequestedLength) {
                status = STATUS_INVALID_BUFFER_SIZE;
            }
            if (NT_SUCCESS(status)) {
                RtlCopyMemory(Elements, Stage->List->Elements,
                    (SIZE_T)count * sizeof(*Elements));
                *Copied = count;
                Stage->CopiedSegments = count;
            }
            else {
                Stage->Unsafe = TRUE;
                LecMapOwnerUncertain(&Stage->Owner);
            }
        }
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    return status;
}

NTSTATUS
LecSgStageRelease(
    _Inout_ PLECS65_SG_STAGE Stage,
    _In_ BOOLEAN ProvenIdle)
{
    KIRQL irql;

    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    /*
     * The caller's ProposedIdle does not certify callback retirement.
     * Until a joint rundown exists, EVERY release request quarantines.
     */
    UNREFERENCED_PARAMETER(ProvenIdle);
    Stage->Closing = TRUE;
    Stage->Unsafe = TRUE;
    LecMapOwnerUncertain(&Stage->Owner);

    /*
     * Even if device idle were proven, CallbackComplete only means
     * the callback published the list, not that WDM has returned from
     * that callback. A borrowed adapter/MDL and async FDO remove cannot
     * be released either. Deliberately do not call PutScatterGatherList,
     * ObDereferenceObject, or ExFreePoolWithTag here. This stage is
     * UNUSABLE for live DMA until an owner-wide rundown is implemented.
     */
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_DEVICE_BUSY;
}

BOOLEAN
LecSgStageMarkLaunched(_Inout_ PLECS65_SG_STAGE Stage)
{
    /*
     * The WDM callback may still be executing when it publishes its
     * result. The isolated stage does not yet have the complete
     * adapter/MDL/FDO/PNP rundown required to launch actual DMA.
     * Do not advertise READY as authorization to bus-master.
     */
    UNREFERENCED_PARAMETER(Stage);
    return FALSE;
}

BOOLEAN
LecSgStageMarkIdleProved(_Inout_ PLECS65_SG_STAGE Stage)
{
    KIRQL irql;
    BOOLEAN result;

    if (Stage == NULL) return FALSE;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    result = (BOOLEAN)(!Stage->Unsafe && !Stage->Closing &&
        LecMapOwnerIdleProved(&Stage->Owner));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return result;
}
