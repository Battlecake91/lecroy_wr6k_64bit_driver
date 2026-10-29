#include "DmaPnpStage.h"

static VOID
LecDmaPnpPublicationBarrier(_In_opt_ PVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
#if defined(LECS65_SG_HOST_TEST)
    MemoryBarrier();
#else
    KeMemoryBarrier();
#endif
}

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
          Stage->State == LecDmaPnpStarted && Stage->AdmissionOpen &&
          Stage->PowerState == LecDmaPowerD0 && !Stage->QueryPending) ||
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
    LecDmaTransactionInitialize(&created->LegacyTransferTransaction);
    created->PhysicalDeviceObject = PhysicalDeviceObject;
    created->State = LecDmaPnpStopped;
    created->PowerState = LecDmaPowerUnknown;
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
    Stage->PowerState = LecDmaPowerResumePending;
    Stage->QueryPending = FALSE;
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
            Stage->PowerState = LecDmaPowerUnknown;
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
            Stage->PowerState = LecDmaPowerUnknown;
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
        ++Stage->PowerGeneration;
        Stage->State = LecDmaPnpStarted;
        Stage->PowerState = LecDmaPowerD0;
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

typedef struct _LECS65_DMA_LEGACY_TRANSFER_BRIDGE {
    LECS65_DMA_LEGACY_TRANSFER_EXECUTE Execute;
    PVOID ExecuteContext;
    PHYSICAL_ADDRESS DescriptorTable;
    ULONG DescriptorEntries;
    ULONG TransferBytes;
    PLECS65_SG_SYNC_OWNER MappingOwner;
    ULONGLONG MappingToken;
    NTSTATUS ExecuteStatus;
    LECS65_DMA_LEGACY_TRANSFER_OUTCOME Outcome;
} LECS65_DMA_LEGACY_TRANSFER_BRIDGE, *PLECS65_DMA_LEGACY_TRANSFER_BRIDGE;

static VOID
LecDmaPnpSetLegacyMappingToken(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ ULONGLONG Token)
{
    KIRQL irql;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->LegacyTransferMappingToken = Token;
    KeReleaseSpinLock(&Stage->Lock, irql);
}

static VOID
LecDmaPnpCompleteLegacyTransfer(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ BOOLEAN CountCompletion)
{
    KIRQL irql;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    Stage->LegacyTransferMappingToken = 0;
    Stage->LegacyTransferActive = FALSE;
    Stage->LegacyTransferOwnerId = 0;
    Stage->LegacyTransferFileId = 0;
    if (CountCompletion) ++Stage->CompletedTransferCount;
    KeReleaseSpinLock(&Stage->Lock, irql);
}

static unsigned
LecDmaPnpCommitLegacyTransfer(void* Context, uint64_t Generation)
{
    PLECS65_DMA_LEGACY_TRANSFER_BRIDGE bridge =
        (PLECS65_DMA_LEGACY_TRANSFER_BRIDGE)Context;

    RtlZeroMemory(&bridge->Outcome, sizeof(bridge->Outcome));
    bridge->Outcome.Status = STATUS_INTERNAL_ERROR;
    bridge->ExecuteStatus = LecSgSyncMarkPossibleGo(
        bridge->MappingOwner, bridge->MappingToken);
    if (!NT_SUCCESS(bridge->ExecuteStatus)) {
        bridge->Outcome.Status = bridge->ExecuteStatus;
        return 0;
    }
    bridge->ExecuteStatus = bridge->Execute(
        bridge->ExecuteContext,
        Generation,
        bridge->DescriptorTable,
        bridge->DescriptorEntries,
        bridge->TransferBytes,
        &bridge->Outcome);
    if (!NT_SUCCESS(bridge->ExecuteStatus) &&
        NT_SUCCESS(bridge->Outcome.Status)) {
        bridge->Outcome.Status = bridge->ExecuteStatus;
    }
    return bridge->Outcome.GoCommitted ? 1U : 0U;
}

NTSTATUS
LecDmaPnpExecuteLegacyTransfer(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _In_ ULONG_PTR OwnerId,
    _In_ ULONG_PTR FileId,
    _In_opt_ volatile BOOLEAN* CancelRequested,
    _In_ LECS65_DMA_LEGACY_TRANSFER_EXECUTE Execute,
    _Inout_ PVOID ExecuteContext,
    _Out_ PLECS65_DMA_PREPARE_RESULT Result)
{
    PSCATTER_GATHER_ELEMENT elements = NULL;
    LECS65_DMA_MAPPED_SEGMENT* segments = NULL;
    LECS65_DMA_TRANSACTION* transaction;
    LECS65_DMA_LEGACY_TRANSFER_BRIDGE bridge;
    ULONGLONG generation = 0;
    ULONGLONG token = 0;
    ULONG count = 0;
    ULONG capacity;
    ULONG i;
    SIZE_T elementBytes;
    SIZE_T segmentBytes;
    NTSTATUS status;
    LECS65_DMA_TRANSACTION_RESULT txResult = LecDmaTxInvalid;
    BOOLEAN mappingOwned = FALSE;
    KIRQL irql;

    if (Stage == NULL || Result == NULL || LockedMdlChain == NULL ||
        Length == 0 || Length >= 0x01000000UL ||
        (Length >= 0x400UL && (Length & 0x3FFUL) != 0) || OwnerId == 0 ||
        FileId == 0 || Execute == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    RtlZeroMemory(Result, sizeof(*Result));
    RtlZeroMemory(&bridge, sizeof(bridge));
    status = LecDmaPnpEnterCall(Stage, LecDmaPnpCallMap);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->LegacyTransferActive) {
        status = STATUS_DEVICE_BUSY;
    }
    else {
        Stage->LegacyTransferActive = TRUE;
        Stage->LegacyTransferOwnerId = OwnerId;
        Stage->LegacyTransferFileId = FileId;
        status = STATUS_SUCCESS;
    }
    KeReleaseSpinLock(&Stage->Lock, irql);
    if (!NT_SUCCESS(status)) goto Exit;

    transaction = &Stage->LegacyTransferTransaction;
    LecDmaTransactionInitialize(transaction);
    capacity = Stage->SyncOwner.DescriptorSlotCapacity;
    if (capacity < 2 ||
        capacity > ((SIZE_T)-1) / sizeof(*elements) ||
        capacity > ((SIZE_T)-1) / sizeof(*segments)) {
        status = STATUS_INTEGER_OVERFLOW;
        goto Exit;
    }
    elementBytes = (SIZE_T)capacity * sizeof(*elements);
    segmentBytes = (SIZE_T)capacity * sizeof(*segments);
    elements = (PSCATTER_GATHER_ELEMENT)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, elementBytes, LECS65_TAG);
    segments = (LECS65_DMA_MAPPED_SEGMENT*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, segmentBytes, LECS65_TAG);
    if (elements == NULL || segments == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    txResult = LecDmaTransactionSetLifecycle(transaction, 1, 1);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionBegin(
        transaction, OwnerId, FileId, Length, &generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionBindOwner(
        transaction, generation, OwnerId, FileId);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionAdmitAdapter(transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionAcquireBuffer(transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (CancelRequested != NULL && *CancelRequested) {
        (void)LecDmaTransactionCancel(transaction, generation, 0);
        status = STATUS_CANCELLED;
        goto Exit;
    }
    txResult = LecDmaTransactionBeginMapping(transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;

    status = LecSgSyncMapNoLaunch(
        &Stage->SyncOwner, LockedMdlChain, Length, &token);
    mappingOwned = (BOOLEAN)NT_SUCCESS(status);
    txResult = LecDmaTransactionMappingComplete(
        transaction, generation, token, NT_SUCCESS(status));
    if (!NT_SUCCESS(status) || txResult != LecDmaTxOk) {
        goto TransactionFailure;
    }
    LecDmaPnpSetLegacyMappingToken(Stage, token);
    Result->MappingToken = token;

    status = LecSgSyncCopySegments(
        &Stage->SyncOwner, token, elements, capacity, &count);
    if (!NT_SUCCESS(status) || count == 0 || count > capacity) {
        goto CancelAndRelease;
    }
    for (i = 0; i < count; ++i) {
        segments[i].DeviceAddress = elements[i].Address.QuadPart;
        segments[i].LengthBytes = elements[i].Length;
    }
    Result->SegmentCount = count;
    Result->FirstDeviceAddress = segments[0].DeviceAddress;
    Result->FirstSegmentBytes = segments[0].LengthBytes;
    txResult = LecDmaTransactionBuildDescriptors(
        transaction,
        generation,
        (LECS65_DMA_LAYOUT_ENTRY*)Stage->AdapterContext->TableVirtual,
        Stage->AdapterContext->TableLength,
        Stage->AdapterContext->TableLogical.QuadPart,
        segments,
        count);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    Result->DescriptorEntries = transaction->DescriptorEntries;
    txResult = LecDmaTransactionPublish(
        transaction, generation, LecDmaPnpPublicationBarrier, NULL);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (CancelRequested != NULL && *CancelRequested) {
        status = STATUS_CANCELLED;
        goto CancelAndRelease;
    }
    txResult = LecDmaTransactionBeginArm(transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;

    bridge.Execute = Execute;
    bridge.ExecuteContext = ExecuteContext;
    bridge.DescriptorTable = Stage->AdapterContext->TableLogical;
    bridge.DescriptorEntries = transaction->DescriptorEntries;
    bridge.TransferBytes = Length;
    bridge.MappingOwner = &Stage->SyncOwner;
    bridge.MappingToken = token;
    txResult = LecDmaTransactionCommitHardwareLaunch(
        transaction, generation, LecDmaPnpCommitLegacyTransfer, &bridge);
    Result->TransactionResult = txResult;
    Result->LaunchDenied = (BOOLEAN)(txResult == LecDmaTxLaunchDisabled);
    Result->GoPossible = (BOOLEAN)(transaction->EverPossiblyLaunched != 0);
    Result->CompletionObserved = bridge.Outcome.CompletionObserved;

    if (Result->LaunchDenied) {
        status = STATUS_NOT_SUPPORTED;
        goto CancelAndRelease;
    }

    if (bridge.Outcome.GoCommitted && bridge.Outcome.CompletionObserved &&
        NT_SUCCESS(bridge.Outcome.Status) &&
        LecDmaTransactionObserveCompletion(transaction, generation) ==
            LecDmaTxOk &&
        NT_SUCCESS(LecSgSyncObserveCompletion(
            &Stage->SyncOwner, token)) &&
        LecDmaTransactionRetireNormalCompletion(
            transaction, generation, 1) == LecDmaTxOk &&
        LecDmaTransactionReleaseMapping(
            transaction, generation) == LecDmaTxOk &&
        NT_SUCCESS(LecSgSyncReleaseCompleted(
            &Stage->SyncOwner, token))) {
        mappingOwned = FALSE;
        LecDmaPnpCompleteLegacyTransfer(Stage, TRUE);
        Result->CompletionObserved = TRUE;
        Result->MappingReleased = TRUE;
        status = STATUS_SUCCESS;
        goto Exit;
    }

    /* Every abnormal or uncertain possible-GO result is retained. */
    if (transaction->EverPossiblyLaunched) {
        (void)LecDmaTransactionCancel(
            transaction, generation,
            Result->CompletionObserved ? 0U : 1U);
        (void)LecDmaTransactionQuarantine(transaction, generation);
        (void)LecDmaPnpQuarantineUnknownActive(Stage);
        Result->Quarantined = TRUE;
        status = NT_SUCCESS(bridge.Outcome.Status) ?
            STATUS_IO_TIMEOUT : bridge.Outcome.Status;
        goto Exit;
    }
    goto TransactionFailure;

CancelAndRelease:
    txResult = LecDmaTransactionCancel(transaction, generation, 0);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionReleaseMapping(transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (mappingOwned) {
        status = LecSgSyncReleaseNoLaunch(&Stage->SyncOwner, token);
        if (!NT_SUCCESS(status)) {
            (void)LecDmaPnpQuarantineUnknownActive(Stage);
            goto Exit;
        }
        mappingOwned = FALSE;
        LecDmaPnpCompleteLegacyTransfer(Stage, FALSE);
        Result->MappingReleased = TRUE;
    }
    goto Exit;

TransactionFailure:
    Result->TransactionResult = txResult;
    status = STATUS_INTERNAL_ERROR;
    if (mappingOwned) {
        LECS65_DMA_TRANSACTION_RESULT cancelResult =
            LecDmaTransactionCancel(transaction, generation, 0);
        if (cancelResult == LecDmaTxOk &&
            LecDmaTransactionReleaseMapping(transaction, generation) ==
                LecDmaTxOk &&
            NT_SUCCESS(LecSgSyncReleaseNoLaunch(
                &Stage->SyncOwner, token))) {
            mappingOwned = FALSE;
            LecDmaPnpCompleteLegacyTransfer(Stage, FALSE);
            Result->MappingReleased = TRUE;
        }
        else {
            (void)LecDmaPnpQuarantineUnknownActive(Stage);
            Result->Quarantined = TRUE;
        }
    }

Exit:
    if (!mappingOwned && !Result->Quarantined) {
        LecDmaPnpCompleteLegacyTransfer(Stage, FALSE);
    }
    if (segments != NULL) ExFreePoolWithTag(segments, LECS65_TAG);
    if (elements != NULL) ExFreePoolWithTag(elements, LECS65_TAG);
    LecDmaPnpLeaveCall(Stage);
    return status;
}

NTSTATUS LecDmaPnpBeginQuery(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStarted || Stage->QueryPending ||
        Stage->ActiveCalls != 0 ||
        Stage->CleanupActive || Stage->UnknownActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->QueryPending = TRUE;
    Stage->AdmissionOpen = FALSE;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpCancelQuery(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (!Stage->QueryPending || Stage->State != LecDmaPnpStarted) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Stage->QueryPending = FALSE;
    Stage->AdmissionOpen = (BOOLEAN)(Stage->PowerState == LecDmaPowerD0 &&
        !Stage->UnknownActive && !LecDmaPnpQuarantineRequested(Stage));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpBeginPowerDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStarted || Stage->ActiveCalls != 0 ||
        Stage->PowerState != LecDmaPowerD0 || Stage->UnknownActive) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Stage->AdmissionOpen = FALSE;
    Stage->PowerState = LecDmaPowerDownPending;
    ++Stage->PowerGeneration;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpCancelPowerDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStarted ||
        Stage->PowerState != LecDmaPowerDownPending) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Stage->PowerState = LecDmaPowerD0;
    Stage->AdmissionOpen = (BOOLEAN)(!Stage->QueryPending &&
        !Stage->UnknownActive && !LecDmaPnpQuarantineRequested(Stage));
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpRecordPoweredDown(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->PowerState != LecDmaPowerDownPending || Stage->ActiveCalls != 0) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_BUSY;
    }
    Stage->PowerState = LecDmaPowerDx;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpBeginResumeD0(_Inout_ PLECS65_DMA_PNP_STAGE Stage)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->State != LecDmaPnpStarted || Stage->PowerState != LecDmaPowerDx ||
        Stage->UnknownActive || LecDmaPnpQuarantineRequested(Stage)) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    Stage->PowerState = LecDmaPowerResumePending;
    KeReleaseSpinLock(&Stage->Lock, irql);
    return STATUS_SUCCESS;
}

NTSTATUS LecDmaPnpCompleteResumeD0(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ BOOLEAN LowerAndRestoreSucceeded)
{
    KIRQL irql;
    if (Stage == NULL) return STATUS_INVALID_PARAMETER;
    KeAcquireSpinLock(&Stage->Lock, &irql);
    if (Stage->PowerState != LecDmaPowerResumePending) {
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_INVALID_DEVICE_STATE;
    }
    if (!LowerAndRestoreSucceeded) {
        Stage->PowerState = LecDmaPowerDx;
        Stage->AdmissionOpen = FALSE;
        KeReleaseSpinLock(&Stage->Lock, irql);
        return STATUS_DEVICE_NOT_READY;
    }
    Stage->PowerState = LecDmaPowerD0;
    Stage->AdmissionOpen = (BOOLEAN)(Stage->State == LecDmaPnpStarted &&
        !Stage->QueryPending && !Stage->UnknownActive &&
        !LecDmaPnpQuarantineRequested(Stage));
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
LecDmaPnpPrepareLegacyNoLaunch(
    _Inout_ PLECS65_DMA_PNP_STAGE Stage,
    _In_ PMDL LockedMdlChain,
    _In_ ULONG Length,
    _In_ ULONG_PTR OwnerId,
    _In_ ULONG_PTR FileId,
    _In_opt_ volatile BOOLEAN* CancelRequested,
    _Out_ PLECS65_DMA_PREPARE_RESULT Result)
{
    LECS65_DMA_TRANSACTION transaction;
    PSCATTER_GATHER_ELEMENT elements = NULL;
    LECS65_DMA_MAPPED_SEGMENT* segments = NULL;
    ULONGLONG generation = 0;
    ULONGLONG token = 0;
    ULONG count = 0;
    ULONG capacity;
    ULONG i;
    SIZE_T elementBytes;
    SIZE_T segmentBytes;
    NTSTATUS status;
    LECS65_DMA_TRANSACTION_RESULT txResult = LecDmaTxInvalid;
    BOOLEAN mappingOwned = FALSE;

    if (Result == NULL || LockedMdlChain == NULL || Length == 0 ||
        OwnerId == 0 || FileId == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    RtlZeroMemory(Result, sizeof(*Result));
    status = LecDmaPnpEnterCall(Stage, LecDmaPnpCallMap);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    capacity = Stage->SyncOwner.DescriptorSlotCapacity;
    if (capacity < 2 ||
        capacity > ((SIZE_T)-1) / sizeof(*elements) ||
        capacity > ((SIZE_T)-1) / sizeof(*segments)) {
        status = STATUS_INTEGER_OVERFLOW;
        goto Exit;
    }
    elementBytes = (SIZE_T)capacity * sizeof(*elements);
    segmentBytes = (SIZE_T)capacity * sizeof(*segments);
    elements = (PSCATTER_GATHER_ELEMENT)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, elementBytes, LECS65_TAG);
    segments = (LECS65_DMA_MAPPED_SEGMENT*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, segmentBytes, LECS65_TAG);
    if (elements == NULL || segments == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    LecDmaTransactionInitialize(&transaction);
    txResult = LecDmaTransactionSetLifecycle(&transaction, 1, 1);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionBegin(
        &transaction, OwnerId, FileId, Length, &generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionBindOwner(
        &transaction, generation, OwnerId, FileId);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionAdmitAdapter(&transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionAcquireBuffer(&transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (CancelRequested != NULL && *CancelRequested) {
        (void)LecDmaTransactionCancel(&transaction, generation, 0);
        status = STATUS_CANCELLED;
        goto Exit;
    }
    txResult = LecDmaTransactionBeginMapping(&transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;

    status = LecSgSyncMapNoLaunch(
        &Stage->SyncOwner, LockedMdlChain, Length, &token);
    mappingOwned = (BOOLEAN)NT_SUCCESS(status);
    txResult = LecDmaTransactionMappingComplete(
        &transaction, generation, token, NT_SUCCESS(status));
    if (!NT_SUCCESS(status) || txResult != LecDmaTxOk) {
        goto TransactionFailure;
    }
    Result->MappingToken = token;

    status = LecSgSyncCopySegments(
        &Stage->SyncOwner, token, elements, capacity, &count);
    if (!NT_SUCCESS(status) || count == 0 || count > capacity) {
        goto CancelAndRelease;
    }
    for (i = 0; i < count; ++i) {
        segments[i].DeviceAddress = elements[i].Address.QuadPart;
        segments[i].LengthBytes = elements[i].Length;
    }
    Result->SegmentCount = count;
    txResult = LecDmaTransactionBuildDescriptors(
        &transaction,
        generation,
        (LECS65_DMA_LAYOUT_ENTRY*)Stage->AdapterContext->TableVirtual,
        Stage->AdapterContext->TableLength,
        Stage->AdapterContext->TableLogical.QuadPart,
        segments,
        count);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    Result->DescriptorEntries = transaction.DescriptorEntries;
    txResult = LecDmaTransactionPublish(
        &transaction, generation, LecDmaPnpPublicationBarrier, NULL);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (CancelRequested != NULL && *CancelRequested) {
        status = STATUS_CANCELLED;
        goto CancelAndRelease;
    }
    txResult = LecDmaTransactionBeginArm(&transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionCommitHardwareLaunch(
        &transaction, generation, NULL, NULL);
    Result->TransactionResult = txResult;
    Result->LaunchDenied = (BOOLEAN)(txResult == LecDmaTxLaunchDisabled);
    if (!Result->LaunchDenied) goto TransactionFailure;
    status = STATUS_NOT_SUPPORTED;

CancelAndRelease:
    txResult = LecDmaTransactionCancel(&transaction, generation, 0);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    txResult = LecDmaTransactionReleaseMapping(&transaction, generation);
    if (txResult != LecDmaTxOk) goto TransactionFailure;
    if (mappingOwned) {
        NTSTATUS releaseStatus = LecSgSyncReleaseNoLaunch(
            &Stage->SyncOwner, token);
        if (!NT_SUCCESS(releaseStatus)) {
            (void)LecDmaPnpQuarantineUnknownActive(Stage);
            status = releaseStatus;
            goto Exit;
        }
        mappingOwned = FALSE;
        Result->MappingReleased = TRUE;
    }
    goto Exit;

TransactionFailure:
    Result->TransactionResult = txResult;
    status = STATUS_INTERNAL_ERROR;
    if (mappingOwned) {
        LECS65_DMA_TRANSACTION_RESULT cancelResult =
            LecDmaTransactionCancel(&transaction, generation, 0);
        if (cancelResult == LecDmaTxOk &&
            LecDmaTransactionReleaseMapping(&transaction, generation) ==
                LecDmaTxOk &&
            NT_SUCCESS(LecSgSyncReleaseNoLaunch(
                &Stage->SyncOwner, token))) {
            mappingOwned = FALSE;
            Result->MappingReleased = TRUE;
        }
        else {
            (void)LecDmaPnpQuarantineUnknownActive(Stage);
        }
    }

Exit:
    if (segments != NULL) ExFreePoolWithTag(segments, LECS65_TAG);
    if (elements != NULL) ExFreePoolWithTag(elements, LECS65_TAG);
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
    Snapshot->PowerGeneration = Stage->PowerGeneration;
    Snapshot->AdmissionOpen = Stage->AdmissionOpen;
    Snapshot->HasAdapterContext = (BOOLEAN)(Stage->AdapterContext != NULL);
    Snapshot->CleanupActive = Stage->CleanupActive;
    Snapshot->SyncOwnerDetached = Stage->SyncOwnerDetached;
    Snapshot->InterruptDisconnected = Stage->InterruptDisconnected;
    Snapshot->DpcDrained = Stage->DpcDrained;
    Snapshot->TimerStopped = Stage->TimerStopped;
    Snapshot->UnknownActive = Stage->UnknownActive;
    Snapshot->LateQuarantine = Stage->LateQuarantine;
    Snapshot->QueryPending = Stage->QueryPending;
    Snapshot->PowerState = Stage->PowerState;
    Snapshot->CleanupPhase = Stage->CleanupPhase;
    Snapshot->LegacyTransferActive = Stage->LegacyTransferActive;
    Snapshot->CompletedTransferCount = Stage->CompletedTransferCount;
    Snapshot->LegacyTransferPhase = Stage->LegacyTransferTransaction.Phase;
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
