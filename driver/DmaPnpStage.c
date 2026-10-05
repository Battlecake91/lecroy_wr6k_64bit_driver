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

static BOOLEAN
LecDmaPnpQuarantineRequested(_In_ PLECS65_DMA_PNP_STAGE Stage)
{
    return (BOOLEAN)(InterlockedCompareExchange(
        &Stage->QuarantineRequested, 0, 0) != 0);
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
    allowed = (BOOLEAN)(!LecDmaPnpQuarantineRequested(Stage) &&
        !Stage->CleanupActive && !Stage->UnknownActive &&
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

NTSTATUS
LecDmaPnpStageCreate(
    _In_opt_ PDEVICE_OBJECT PhysicalDeviceObject,
    _Outptr_ PLECS65_DMA_PNP_STAGE* Stage)
{
    PLECS65_DMA_PNP_STAGE created;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *Stage = NULL;
    created = (PLECS65_DMA_PNP_STAGE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(*created), LECS65_TAG);
    if (created == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(created, sizeof(*created));
    KeInitializeSpinLock(&created->Lock);
    LecSgSyncOwnerConstruct(&created->SyncOwner);
    created->PhysicalDeviceObject = PhysicalDeviceObject;
    created->State = LecDmaPnpStopped;
    created->InterruptDisconnected = TRUE;
    created->DpcDrained = TRUE;
    created->TimerStopped = TRUE;
    *Stage = created;
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaPnpStageDestroy(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if ((Stage->State != LecDmaPnpRemoved &&
         Stage->State != LecDmaPnpStopped) ||
        Stage->AdapterContext != NULL ||
        Stage->ActiveCalls != 0 || Stage->CleanupActive ||
        LecDmaPnpQuarantineRequested(Stage) || Stage->UnknownActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->State = LecDmaPnpDestroying;
    KeReleaseSpinLock(&Stage->Lock, irql);
    ExFreePoolWithTag(Stage, LECS65_TAG);
    return STATUS_SUCCESS;
}

NTSTATUS
LecDmaPnpStartNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONG MaximumTransferBytes,
    _In_ ULONG DescriptorTableBytes)
{
    PLECS65_DMA_ADAPTER_CONTEXT context = NULL;
    BOOLEAN releaseCommitted = FALSE;
    BOOLEAN late = FALSE;
    BOOLEAN quarantineRetainedContext = FALSE;
    KIRQL irql;
    NTSTATUS status;
    NTSTATUS cleanupStatus = STATUS_SUCCESS;

    if (Stage == NULL || Stage->PhysicalDeviceObject == NULL ||
        MaximumTransferBytes == 0 || DescriptorTableBytes == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStopped || Stage->AdapterContext != NULL ||
        Stage->ActiveCalls != 0 || Stage->CleanupActive ||
        Stage->UnknownActive || LecDmaPnpQuarantineRequested(Stage)) {
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
        /* Publish partial ownership before another blocking DDI begins. */
        KeAcquireSpinLock(&Stage->Lock, &irql);
        Stage->AdapterContext = context;
        KeReleaseSpinLock(&Stage->Lock, irql);
    }
    if (NT_SUCCESS(status) && !LecDmaPnpQuarantineRequested(Stage)) {
        status = LecDmaAllocateCommonTable(context, DescriptorTableBytes);
    }
    else if (NT_SUCCESS(status)) {
        status = STATUS_DEVICE_BUSY;
    }
    if (NT_SUCCESS(status) && !LecDmaPnpQuarantineRequested(Stage)) {
        status = LecSgSyncOwnerInit(&Stage->SyncOwner, context);
    }
    else if (NT_SUCCESS(status)) {
        status = STATUS_DEVICE_BUSY;
    }

    if (!NT_SUCCESS(status)) {
        KeAcquireSpinLock(&Stage->Lock, &irql);
        if (LecDmaPnpQuarantineRequested(Stage)) {
            Stage->UnknownActive = TRUE;
            Stage->State = LecDmaPnpQuarantined;
            Stage->AdmissionOpen = FALSE;
        }
        else if (context != NULL) {
            Stage->CleanupActive = TRUE;
            Stage->CleanupPhase = LecDmaPnpCleanupAdapterPending;
            releaseCommitted = TRUE;
        }
        else {
            Stage->State = LecDmaPnpStopped;
            Stage->InterruptDisconnected = TRUE;
            Stage->DpcDrained = TRUE;
            Stage->TimerStopped = TRUE;
        }
        KeReleaseSpinLock(&Stage->Lock, irql);

        if (LecDmaPnpQuarantineRequested(Stage)) {
            (void)LecSgSyncOwnerQuarantine(&Stage->SyncOwner, &late);
            if (context != NULL) {
                LecDmaQuarantineAdapterContext(context);
            }
            return STATUS_DEVICE_BUSY;
        }
        if (releaseCommitted) {
            KeAcquireSpinLock(&Stage->Lock, &irql);
            if (LecDmaPnpQuarantineRequested(Stage)) {
                cleanupStatus = STATUS_DEVICE_BUSY;
                releaseCommitted = FALSE;
            }
            else {
                Stage->CleanupPhase = LecDmaPnpCleanupAdapterCommitted;
            }
            KeReleaseSpinLock(&Stage->Lock, irql);
            if (releaseCommitted) {
                cleanupStatus = LecDmaReleaseAdapterContext(context, TRUE);
            }
        }

        KeAcquireSpinLock(&Stage->Lock, &irql);
        Stage->CleanupActive = FALSE;
        Stage->CleanupPhase = LecDmaPnpCleanupIdle;
        if (LecDmaPnpQuarantineRequested(Stage)) {
            Stage->LateQuarantine = TRUE;
            Stage->UnknownActive = TRUE;
            Stage->State = LecDmaPnpQuarantined;
            if (NT_SUCCESS(cleanupStatus)) {
                Stage->AdapterContext = NULL;
            }
            else if (context != NULL) {
                quarantineRetainedContext = TRUE;
            }
        }
        else if (!NT_SUCCESS(cleanupStatus)) {
            Stage->UnknownActive = TRUE;
            Stage->State = LecDmaPnpQuarantined;
        }
        else {
            Stage->AdapterContext = NULL;
            Stage->State = LecDmaPnpStopped;
            Stage->InterruptDisconnected = TRUE;
            Stage->DpcDrained = TRUE;
            Stage->TimerStopped = TRUE;
        }
        KeReleaseSpinLock(&Stage->Lock, irql);
        if (quarantineRetainedContext) {
            LecDmaQuarantineAdapterContext(context);
        }
        return !NT_SUCCESS(cleanupStatus) ? cleanupStatus : status;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (LecDmaPnpQuarantineRequested(Stage)) {
        Stage->UnknownActive = TRUE;
        Stage->State = LecDmaPnpQuarantined;
        Stage->AdmissionOpen = FALSE;
        status = STATUS_DEVICE_BUSY;
    }
    else {
        ++Stage->Generation;
        Stage->State = LecDmaPnpStarted;
        Stage->AdmissionOpen = TRUE;
        status = STATUS_SUCCESS;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    if (!NT_SUCCESS(status)) {
        (void)LecSgSyncOwnerQuarantine(&Stage->SyncOwner, &late);
        LecDmaQuarantineAdapterContext(context);
    }
    return status;
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
    if (LecDmaPnpQuarantineRequested(Stage) ||
        Stage->State == LecDmaPnpQuarantined || Stage->UnknownActive) {
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
        Stage->UnknownActive || LecDmaPnpQuarantineRequested(Stage)) {
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
    BOOLEAN quarantineRetainedContext = FALSE;
    KIRQL irql;
    NTSTATUS status;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (!LecDmaPnpIsTeardownState(Stage->State) ||
        LecDmaPnpQuarantineRequested(Stage) || Stage->UnknownActive ||
        Stage->CleanupActive ||
        Stage->ActiveCalls != 0 || !Stage->InterruptDisconnected ||
        !Stage->DpcDrained || !Stage->TimerStopped) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->CleanupActive = TRUE;
    Stage->CleanupPhase = LecDmaPnpCleanupMappings;
    context = Stage->AdapterContext;
    teardownState = Stage->State;
    ownerDetached = Stage->SyncOwnerDetached;
    KeReleaseSpinLock(&Stage->Lock, irql);

    status = STATUS_SUCCESS;
    if (context != NULL && !ownerDetached) {
        status = LecSgSyncOwnerDrainNoLaunch(&Stage->SyncOwner);
        if (NT_SUCCESS(status)) {
            KeAcquireSpinLock(&Stage->Lock, &irql);
            if (LecDmaPnpQuarantineRequested(Stage)) {
                status = STATUS_DEVICE_BUSY;
            }
            else {
                Stage->CleanupPhase = LecDmaPnpCleanupOwner;
            }
            KeReleaseSpinLock(&Stage->Lock, irql);
        }
        if (NT_SUCCESS(status)) {
            status = LecSgSyncOwnerDestroy(&Stage->SyncOwner);
            if (NT_SUCCESS(status)) {
                ownerDetached = TRUE;
            }
        }
    }

    if (NT_SUCCESS(status) && context != NULL) {
        KeAcquireSpinLock(&Stage->Lock, &irql);
        if (LecDmaPnpQuarantineRequested(Stage)) {
            status = STATUS_DEVICE_BUSY;
        }
        else {
            /* Irreversible release commit; quarantine reports a late race. */
            Stage->CleanupPhase = LecDmaPnpCleanupAdapterCommitted;
        }
        KeReleaseSpinLock(&Stage->Lock, irql);
        if (NT_SUCCESS(status)) {
            status = LecDmaReleaseAdapterContext(context, TRUE);
        }
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->CleanupActive = FALSE;
    Stage->CleanupPhase = LecDmaPnpCleanupIdle;
    Stage->SyncOwnerDetached = ownerDetached;
    if (LecDmaPnpQuarantineRequested(Stage)) {
        if (NT_SUCCESS(status)) {
            Stage->AdapterContext = NULL;
        }
        else if (context != NULL) {
            quarantineRetainedContext = TRUE;
        }
        Stage->UnknownActive = TRUE;
        Stage->State = LecDmaPnpQuarantined;
        status = STATUS_DEVICE_BUSY;
    }
    else if (NT_SUCCESS(status)) {
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
    if (quarantineRetainedContext) {
        LecDmaQuarantineAdapterContext(context);
    }
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
    BOOLEAN lateChild = FALSE;
    BOOLEAN quarantineAdapter;
    KIRQL irql;

    if (Stage == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    /* Latch parent and child before waiting for the parent cleanup lock. */
    InterlockedExchange(&Stage->QuarantineRequested, 1);
    (void)LecSgSyncOwnerQuarantine(&Stage->SyncOwner, &lateChild);
    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->AdmissionOpen = FALSE;
    Stage->UnknownActive = TRUE;
    Stage->State = LecDmaPnpQuarantined;
    context = Stage->AdapterContext;
    quarantineAdapter = (BOOLEAN)(context != NULL &&
        Stage->CleanupPhase != LecDmaPnpCleanupAdapterCommitted);
    if (Stage->CleanupPhase == LecDmaPnpCleanupAdapterCommitted ||
        lateChild) {
        Stage->LateQuarantine = TRUE;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);

    if (quarantineAdapter) {
        LecDmaQuarantineAdapterContext(context);
    }
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
    Snapshot->LateQuarantine = Stage->LateQuarantine;
    Snapshot->CleanupPhase = Stage->CleanupPhase;
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
        !Stage->CleanupActive && !Stage->UnknownActive &&
        !LecDmaPnpQuarantineRequested(Stage));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return canDestroy;
}
