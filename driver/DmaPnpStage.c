#include "DmaPnpStage.h"

typedef enum _LECS65_DMA_PNP_CALL_KIND {
    LecDmaPnpCallMap = 0,
    LecDmaPnpCallExistingMapping
} LECS65_DMA_PNP_CALL_KIND;

static BOOLEAN
LecDmaPnpIsTeardownState(_In_ LECS65_DMA_PNP_STATE State)
{
    return (BOOLEAN)(State == LecDmaPnpStopping ||
        State == LecDmaPnpSurprisePending ||
        State == LecDmaPnpRemoving);
}

static NTSTATUS
LecDmaPnpEnterCall(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_CALL_KIND Kind)
{
    KIRQL irql;
    BOOLEAN allowed;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    allowed = (BOOLEAN)(!Stage->CleanupActive && !Stage->UnknownActive &&
        Stage->ActiveCalls != MAXULONG &&
        Stage->AdapterContext != NULL &&
        ((Kind == LecDmaPnpCallMap &&
          Stage->State == LecDmaPnpStarted && Stage->AdmissionOpen) ||
         (Kind == LecDmaPnpCallExistingMapping &&
          (Stage->State == LecDmaPnpStarted ||
           LecDmaPnpIsTeardownState(Stage->State)))));
    if (allowed) {
        ++Stage->ActiveCalls;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    return allowed ? STATUS_SUCCESS : STATUS_DELETE_PENDING;
}

static VOID
LecDmaPnpLeaveCall(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->ActiveCalls != 0) {
        --Stage->ActiveCalls;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
}

static VOID
LecDmaPnpSetTeardownState(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_TEARDOWN_REASON Reason)
{
    if (Reason == LecDmaPnpTeardownRemove) {
        Stage->State = LecDmaPnpRemoving;
    }
    else if (Reason == LecDmaPnpTeardownSurprise &&
             Stage->State != LecDmaPnpRemoving) {
        Stage->State = LecDmaPnpSurprisePending;
    }
    else if (Stage->State != LecDmaPnpRemoving &&
             Stage->State != LecDmaPnpSurprisePending) {
        Stage->State = LecDmaPnpStopping;
    }
}

VOID
LecDmaPnpStageConstruct(
    _Out_ PLECS65_DMA_PNP_STAGE Stage,
    _In_opt_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    if (Stage == NULL) {
        return;
    }

    RtlZeroMemory(Stage, sizeof(*Stage));
    KeInitializeSpinLock(&Stage->Lock);
    LecSgSyncOwnerConstruct(&Stage->SyncOwner);
    Stage->PhysicalDeviceObject = PhysicalDeviceObject;
    Stage->State = LecDmaPnpStopped;
    Stage->InterruptDisconnected = TRUE;
    Stage->DpcDrained = TRUE;
    Stage->TimerStopped = TRUE;
}

NTSTATUS
LecDmaPnpStartNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONG MaximumTransferBytes,
    _In_ ULONG DescriptorTableBytes)
{
    PLECS65_DMA_ADAPTER_CONTEXT context = NULL;
    KIRQL irql;
    NTSTATUS status;
    NTSTATUS cleanupStatus;

    if (Stage == NULL || Stage->PhysicalDeviceObject == NULL ||
        MaximumTransferBytes == 0 || DescriptorTableBytes == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStopped || Stage->AdapterContext != NULL ||
        Stage->ActiveCalls != 0 || Stage->CleanupActive ||
        Stage->UnknownActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->State = LecDmaPnpStarting;
    Stage->AdmissionOpen = FALSE;
    Stage->SyncOwnerDetached = FALSE;
    Stage->InterruptDisconnected = FALSE;
    Stage->DpcDrained = FALSE;
    Stage->TimerStopped = FALSE;
    KeReleaseSpinLock(&Stage->Lock, irql);

    /* Every DDI below may block or invoke arbitrary adapter code. */
    status = LecDmaCreateAdapterContext(
        Stage->PhysicalDeviceObject, MaximumTransferBytes, &context);
    if (NT_SUCCESS(status)) {
        status = LecDmaAllocateCommonTable(context, DescriptorTableBytes);
    }
    if (NT_SUCCESS(status)) {
        status = LecSgSyncOwnerInit(&Stage->SyncOwner, context);
    }

    if (!NT_SUCCESS(status)) {
        cleanupStatus = LecDmaReleaseAdapterContext(context, TRUE);
        KeAcquireSpinLock(&Stage->Lock, &irql);
        if (!NT_SUCCESS(cleanupStatus)) {
            /* Preserve an unexpectedly unreleasable partial owner. */
            Stage->AdapterContext = context;
            Stage->UnknownActive = TRUE;
            Stage->State = LecDmaPnpQuarantined;
        }
        else {
            Stage->State = LecDmaPnpStopped;
            Stage->InterruptDisconnected = TRUE;
            Stage->DpcDrained = TRUE;
            Stage->TimerStopped = TRUE;
        }
        KeReleaseSpinLock(&Stage->Lock, irql);
        return !NT_SUCCESS(cleanupStatus) ? cleanupStatus : status;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->AdapterContext = context;
    ++Stage->Generation;
    Stage->State = LecDmaPnpStarted;
    Stage->AdmissionOpen = TRUE;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaPnpBeginTeardown(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_TEARDOWN_REASON Reason)
{
    PLECS65_DMA_ADAPTER_CONTEXT context;
    ULONG activeCalls;
    BOOLEAN ownerDetached;
    KIRQL irql;
    NTSTATUS status;

    if (Stage == NULL || Reason < LecDmaPnpTeardownStop ||
        Reason > LecDmaPnpTeardownRemove) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State == LecDmaPnpStarting || Stage->CleanupActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (Stage->State == LecDmaPnpQuarantined || Stage->UnknownActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    if (Stage->State == LecDmaPnpRemoved) {
        status = Reason == LecDmaPnpTeardownRemove ?
            STATUS_SUCCESS : STATUS_INVALID_DEVICE_STATE;
        KeReleaseSpinLock(&Stage->Lock, irql);
        return status;
    }
    if (Stage->State == LecDmaPnpSurpriseRemoved &&
        Reason != LecDmaPnpTeardownRemove) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_SUCCESS;
    }
    if (Stage->State == LecDmaPnpStopped &&
        Reason == LecDmaPnpTeardownStop) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_SUCCESS;
    }

    Stage->AdmissionOpen = FALSE;
    LecDmaPnpSetTeardownState(Stage, Reason);
    context = Stage->AdapterContext;
    activeCalls = Stage->ActiveCalls;
    ownerDetached = Stage->SyncOwnerDetached;
    KeReleaseSpinLock(&Stage->Lock, irql);

    if (context == NULL) {
        return STATUS_SUCCESS;
    }

    /* Child STOP is nonblocking and runs outside the parent lock. */
    status = ownerDetached ? STATUS_SUCCESS :
        LecSgSyncOwnerStop(&Stage->SyncOwner);
    if (activeCalls != 0) {
        return STATUS_DEVICE_BUSY;
    }
    if (status == STATUS_DEVICE_BUSY) {
        /* Published no-launch mappings are drained only by Finish. */
        return STATUS_SUCCESS;
    }
    return status;
}

NTSTATUS
LecDmaPnpRecordQuiescence(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ LECS65_DMA_PNP_QUIESCE_STEP Step)
{
    KIRQL irql;
    NTSTATUS status = STATUS_SUCCESS;

    if (Stage == NULL || Step < LecDmaPnpInterruptDisconnected ||
        Step > LecDmaPnpTimerStopped) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (!LecDmaPnpIsTeardownState(Stage->State) || Stage->CleanupActive ||
        Stage->UnknownActive) {
        status = STATUS_INVALID_DEVICE_STATE;
    }
    else if (Step == LecDmaPnpInterruptDisconnected) {
        Stage->InterruptDisconnected = TRUE;
    }
    else if (Step == LecDmaPnpDpcDrained) {
        if (!Stage->InterruptDisconnected) {
            status = STATUS_INVALID_DEVICE_STATE;
        }
        else {
            Stage->DpcDrained = TRUE;
        }
    }
    else if (!Stage->DpcDrained) {
        status = STATUS_INVALID_DEVICE_STATE;
    }
    else {
        Stage->TimerStopped = TRUE;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    return status;
}

NTSTATUS
LecDmaPnpFinishTeardownNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    PLECS65_DMA_ADAPTER_CONTEXT context;
    LECS65_DMA_PNP_STATE teardownState;
    BOOLEAN ownerDetached;
    KIRQL irql;
    NTSTATUS status;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (!LecDmaPnpIsTeardownState(Stage->State) ||
        Stage->UnknownActive || Stage->CleanupActive ||
        Stage->ActiveCalls != 0 || !Stage->InterruptDisconnected ||
        !Stage->DpcDrained || !Stage->TimerStopped) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->CleanupActive = TRUE;
    context = Stage->AdapterContext;
    teardownState = Stage->State;
    ownerDetached = Stage->SyncOwnerDetached;
    KeReleaseSpinLock(&Stage->Lock, irql);

    if (context != NULL) {
        status = STATUS_SUCCESS;
        if (!ownerDetached) {
            status = LecSgSyncOwnerDrainNoLaunch(&Stage->SyncOwner);
            if (NT_SUCCESS(status)) {
                status = LecSgSyncOwnerDestroy(&Stage->SyncOwner);
            }
            if (NT_SUCCESS(status)) {
                ownerDetached = TRUE;
            }
        }
        if (NT_SUCCESS(status)) {
            /* Safe only because this parent has no launch capability. */
            status = LecDmaReleaseAdapterContext(context, TRUE);
        }
    }
    else {
        status = STATUS_SUCCESS;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->CleanupActive = FALSE;
    Stage->SyncOwnerDetached = ownerDetached;
    if (NT_SUCCESS(status)) {
        Stage->AdapterContext = NULL;
        Stage->SyncOwnerDetached = FALSE;
        if (teardownState == LecDmaPnpRemoving) {
            Stage->State = LecDmaPnpRemoved;
        }
        else if (teardownState == LecDmaPnpSurprisePending) {
            Stage->State = LecDmaPnpSurpriseRemoved;
        }
        else {
            Stage->State = LecDmaPnpStopped;
        }
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    return status;
}

NTSTATUS
LecDmaPnpMapNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _Out_ ULONGLONG* Token)
{
    NTSTATUS status;

    if (Token == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Token = 0;
    status = LecDmaPnpEnterCall(Stage, LecDmaPnpCallMap);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = LecSgSyncMapNoLaunch(
        &Stage->SyncOwner, LockedMdlChain, Length, Token);
    LecDmaPnpLeaveCall(Stage);
    return status;
}

NTSTATUS
LecDmaPnpCopySegments(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONGLONG Token,
    _Out_writes_to_(Capacity, *Copied) PSCATTER_GATHER_ELEMENT Elements,
    _In_ ULONG Capacity,
    _Out_ PULONG Copied)
{
    NTSTATUS status;

    if (Copied == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Copied = 0;
    status = LecDmaPnpEnterCall(Stage, LecDmaPnpCallExistingMapping);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = LecSgSyncCopySegments(
        &Stage->SyncOwner, Token, Elements, Capacity, Copied);
    LecDmaPnpLeaveCall(Stage);
    return status;
}

NTSTATUS
LecDmaPnpReleaseNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONGLONG Token)
{
    NTSTATUS status = LecDmaPnpEnterCall(
        Stage, LecDmaPnpCallExistingMapping);

    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = LecSgSyncReleaseNoLaunch(&Stage->SyncOwner, Token);
    LecDmaPnpLeaveCall(Stage);
    return status;
}

NTSTATUS
LecDmaPnpQuarantineUnknownActive(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    PLECS65_DMA_ADAPTER_CONTEXT context;
    KIRQL irql;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->AdapterContext == NULL || Stage->CleanupActive ||
        Stage->ActiveCalls != 0 ||
        (Stage->State != LecDmaPnpStarted &&
         !LecDmaPnpIsTeardownState(Stage->State))) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Stage->AdmissionOpen = FALSE;
    Stage->UnknownActive = TRUE;
    Stage->State = LecDmaPnpQuarantined;
    context = Stage->AdapterContext;
    KeReleaseSpinLock(&Stage->Lock, irql);

    LecDmaQuarantineAdapterContext(context);
    return STATUS_SUCCESS;
}

VOID
LecDmaPnpSnapshot(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _Out_ PLECS65_DMA_PNP_SNAPSHOT Snapshot)
{
    KIRQL irql;

    if (Stage == NULL || Snapshot == NULL) {
        return;
    }
    KeAcquireSpinLock(&Stage->Lock, &irql);
    Snapshot->State = Stage->State;
    Snapshot->ActiveCalls = Stage->ActiveCalls;
    Snapshot->Generation = Stage->Generation;
    Snapshot->AdmissionOpen = Stage->AdmissionOpen;
    Snapshot->HasAdapterContext = (BOOLEAN)(Stage->AdapterContext != NULL);
    Snapshot->CleanupActive = Stage->CleanupActive;
    Snapshot->SyncOwnerDetached = Stage->SyncOwnerDetached;
    Snapshot->InterruptDisconnected = Stage->InterruptDisconnected;
    Snapshot->DpcDrained = Stage->DpcDrained;
    Snapshot->TimerStopped = Stage->TimerStopped;
    Snapshot->UnknownActive = Stage->UnknownActive;
    KeReleaseSpinLock(&Stage->Lock, irql);
}

BOOLEAN
LecDmaPnpCanDestroy(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    BOOLEAN canDestroy;

    if (Stage == NULL) {
        return FALSE;
    }
    KeAcquireSpinLock(&Stage->Lock, &irql);
    canDestroy = (BOOLEAN)(Stage->State == LecDmaPnpRemoved &&
        Stage->AdapterContext == NULL && Stage->ActiveCalls == 0 &&
        !Stage->CleanupActive && !Stage->UnknownActive);
    KeReleaseSpinLock(&Stage->Lock, irql);
    return canDestroy;
}
