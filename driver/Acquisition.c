#include "LecS65Drv.h"
#include "DmaPnpPublication.h"

#define LECS65_BAR0_SGTA   0x040
#define LECS65_BAR0_IIMTC  0x044
#define LECS65_BAR0_IIMCL  0x048
#define LECS65_BAR0_ERRS   0x004
#define LECS65_BAR0_ERRM   0x008
#define LECS65_BAR0_INTST  0x080
#define LECS65_BAR0_INTEN  0x084
#define LECS65_BAR1_CLRERR  0x004
#define LECS65_BAR1_CLRIRQ  0x008
#define LECS65_BAR1_HWINT   0x410

static
VOID
LecFreeMdlChain(
    _In_opt_ PMDL Mdl
    )
{
    while (Mdl != NULL) {
        PMDL next = Mdl->Next;

        if ((Mdl->MdlFlags & MDL_PAGES_LOCKED) != 0) {
            MmUnlockPages(Mdl);
        }

        IoFreeMdl(Mdl);
        Mdl = next;
    }
}

static
VOID
LecFreeTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PLECS65_TRANSFER Transfer
    )
{
    /*
     * Deliberate fail-closed quarantine. Without a proven DMA abort/idle
     * protocol, unlocking/reusing these pages could corrupt unrelated
     * kernel or process memory. The pinned pages and descriptor storage
     * must survive FDO destruction; recovery is a controlled system restart,
     * not a driver restart. This leaks kernel resources by design and is
     * NOT production-ready DMA lifecycle management.
     */
    if (Transfer->DmaUnsafeToFree) {
        LONG ownershipState;
        NTSTATUS retainStatus = STATUS_INVALID_DEVICE_STATE;

        ownershipState = InterlockedCompareExchange(
            &Transfer->QuarantineOwnership,
            LECS65_TRANSFER_QUARANTINE_FDO,
            LECS65_TRANSFER_QUARANTINE_FDO);
        if (ownershipState == LECS65_TRANSFER_QUARANTINE_PUBLICATION) {
            retainStatus = STATUS_SUCCESS;
        }
        else if (DevExt->DmaPnpPublication != NULL) {
            retainStatus = LecDmaPnpPublicationRetainLegacyTransfer(
                DevExt->DmaPnpPublication,
                &Transfer->Link,
                &Transfer->QuarantineOwnership);
        }
        ownershipState = InterlockedCompareExchange(
            &Transfer->QuarantineOwnership,
            LECS65_TRANSFER_QUARANTINE_FDO,
            LECS65_TRANSFER_QUARANTINE_FDO);
        (void)LecDmaCompletionMarkQuarantined(
            &DevExt->DmaCompletion,
            Transfer->DmaGeneration);
        LecTrace(
            "DMA quarantine: token=%lu retained across teardown status=0x%08X owner=%ld\n",
            Transfer->Token,
            retainStatus,
            ownershipState);
        return;
    }

    if (Transfer->DescriptorMdl != NULL) {
        IoFreeMdl(Transfer->DescriptorMdl);
        Transfer->DescriptorMdl = NULL;
    }

    if (Transfer->DescriptorBuffer != NULL) {
        ExFreePoolWithTag(Transfer->DescriptorBuffer, LECS65_TAG);
        Transfer->DescriptorBuffer = NULL;
    }

    LecFreeMdlChain(Transfer->SourceMdlChain);
    Transfer->SourceMdlChain = NULL;

    ExFreePoolWithTag(Transfer, LECS65_TAG);
}

NTSTATUS
LecSelectDmaTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer,
    _Out_ PULONGLONG Generation)
{
    KIRQL irql;
    ULONGLONG generation;
    NTSTATUS status = STATUS_SUCCESS;

    if (DevExt == NULL || Transfer == NULL || Generation == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    generation = (ULONGLONG)InterlockedIncrement64(
        &DevExt->DmaGenerationCounter);
    if (generation == 0) {
        generation = (ULONGLONG)InterlockedIncrement64(
            &DevExt->DmaGenerationCounter);
    }
    if (!LecDmaCompletionPrepare(&DevExt->DmaCompletion, generation)) {
        return STATUS_DEVICE_NOT_READY;
    }

    KeAcquireSpinLock(&DevExt->DmaCompletionLock, &irql);
    if (DevExt->CurrentTransfer != NULL ||
        DevExt->DmaActiveGeneration != 0) {
        status = STATUS_DEVICE_BUSY;
    }
    else {
        Transfer->DmaGeneration = generation;
        DevExt->DmaActiveGeneration = (LONG64)generation;
        DevExt->CurrentTransfer = Transfer;
        *Generation = generation;
    }
    KeReleaseSpinLock(&DevExt->DmaCompletionLock, irql);
    return status;
}

BOOLEAN
LecArmSelectedDma(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation)
{
    return LecDmaCompletionArm(
        &DevExt->DmaCompletion,
        Generation);
}

BOOLEAN
LecCancelSelectedDmaArm(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation)
{
    return LecDmaCompletionCancelArm(
        &DevExt->DmaCompletion,
        Generation);
}

typedef struct _LECS65_DMA_LAUNCH_CONTEXT {
    PLECS65_DEVICE_EXTENSION DevExt;
    ULONGLONG Generation;
    volatile ULONG* CompletionControl;
    volatile ULONG* GoRegister;
    ULONG GoValue;
    BOOLEAN GoWritten;
} LECS65_DMA_LAUNCH_CONTEXT, *PLECS65_DMA_LAUNCH_CONTEXT;

static
BOOLEAN
LecCommitDmaLaunchSynchronized(_In_ PVOID Context)
{
    PLECS65_DMA_LAUNCH_CONTEXT launch =
        (PLECS65_DMA_LAUNCH_CONTEXT)Context;

    /*
     * KeSynchronizeExecution excludes the ISR across both the GO write and
     * publication of DeviceActive.  Therefore an IRQ before this callback is
     * rejected by Arming, while a fast IRQ after GO cannot run until the
     * active generation has been published.
     */
    if (!LecDmaCompletionIsArmed(
            &launch->DevExt->DmaCompletion,
            launch->Generation)) {
        return FALSE;
    }

    WRITE_REGISTER_ULONG(launch->CompletionControl, 1UL);
    WRITE_REGISTER_ULONG(launch->GoRegister, launch->GoValue);
    launch->GoWritten = TRUE;

    return LecDmaCompletionPublishDeviceActive(
        &launch->DevExt->DmaCompletion,
        launch->Generation);
}

NTSTATUS
LecLaunchSelectedDma(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation,
    _In_ volatile ULONG* CompletionControl,
    _In_ volatile ULONG* GoRegister,
    _In_ ULONG GoValue,
    _Out_ PBOOLEAN DmaLaunched)
{
    LECS65_DMA_LAUNCH_CONTEXT launch;
    BOOLEAN committed;

    if (DevExt == NULL || CompletionControl == NULL ||
        GoRegister == NULL || DmaLaunched == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    *DmaLaunched = FALSE;
    if (!DevExt->InterruptConnected || DevExt->InterruptObject == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    RtlZeroMemory(&launch, sizeof(launch));
    launch.DevExt = DevExt;
    launch.Generation = Generation;
    launch.CompletionControl = CompletionControl;
    launch.GoRegister = GoRegister;
    launch.GoValue = GoValue;

    committed = KeSynchronizeExecution(
        DevExt->InterruptObject,
        LecCommitDmaLaunchSynchronized,
        &launch);
    *DmaLaunched = launch.GoWritten;

    return committed ? STATUS_SUCCESS : STATUS_IO_DEVICE_ERROR;
}

VOID
LecDeselectDmaTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer,
    _In_ ULONGLONG Generation)
{
    KIRQL irql;

    KeAcquireSpinLock(&DevExt->DmaCompletionLock, &irql);
    if (DevExt->CurrentTransfer == Transfer &&
        (ULONGLONG)DevExt->DmaActiveGeneration == Generation) {
        DevExt->CurrentTransfer = NULL;
        DevExt->DmaActiveGeneration = 0;
        (void)LecDmaCompletionConsumeSignal(
            &DevExt->DmaCompletion,
            Generation);
    }
    KeReleaseSpinLock(&DevExt->DmaCompletionLock, irql);
}

VOID
LecMarkDmaUnknownActive(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer)
{
    /* Poison memory ownership before publishing device-wide uncertainty. */
    Transfer->DmaUnsafeToFree = TRUE;
    (void)InterlockedExchange(&DevExt->DmaUnknownActive, 1);
    /* Block new hardware work before any secondary notification can fail. */
    LecSetIoctlAdmission(DevExt, FALSE);
    if (DevExt->DmaPnpPublication != NULL) {
        (void)LecDmaPnpPublicationQuarantine(
            DevExt->DmaPnpPublication);
    }
}

static
NTSTATUS
LecBuildSourceMdlChain(
    _In_ PVOID UserBuffer,
    _In_ ULONG Length,
    _In_ KPROCESSOR_MODE AccessMode,
    _Out_ PMDL* FirstMdl
    )
{
    PMDL first = NULL;
    PMDL tail = NULL;
    ULONG offset = 0;
    NTSTATUS status = STATUS_SUCCESS;

    *FirstMdl = NULL;

    while (offset < Length) {
        ULONG chunk = min(Length - offset, (ULONG)LECS65_DMA_MDL_CHUNK_BYTES);
        PMDL mdl = IoAllocateMdl(
            (PUCHAR)UserBuffer + offset,
            chunk,
            FALSE,
            FALSE,
            NULL);

        if (mdl == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        __try {
            MmProbeAndLockPages(mdl, AccessMode, IoWriteAccess);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
            IoFreeMdl(mdl);
            break;
        }

        mdl->Next = NULL;

        if (first == NULL) {
            first = mdl;
        }
        else {
            tail->Next = mdl;
        }
        tail = mdl;
        offset += chunk;
    }

    if (!NT_SUCCESS(status)) {
        LecFreeMdlChain(first);
        return status;
    }

    *FirstMdl = first;
    return STATUS_SUCCESS;
}

static
NTSTATUS
LecBuildDescriptorTable(
    _Inout_ PLECS65_TRANSFER Transfer
    )
{
    PLECS65_DMA_DESCRIPTOR table;
    PPFN_NUMBER tablePfns;
    ULONG tablePages;
    ULONG tableSlot = 0;
    ULONG totalDwords = 0;
    PMDL sourceMdl;

    Transfer->DescriptorBuffer = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        LECS65_DMA_TABLE_BYTES,
        LECS65_TAG);
    if (Transfer->DescriptorBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(
        Transfer->DescriptorBuffer,
        LECS65_DMA_TABLE_BYTES);

    Transfer->DescriptorMdl = IoAllocateMdl(
        Transfer->DescriptorBuffer,
        LECS65_DMA_TABLE_BYTES,
        FALSE,
        FALSE,
        NULL);
    if (Transfer->DescriptorMdl == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MmBuildMdlForNonPagedPool(Transfer->DescriptorMdl);

    tablePfns = MmGetMdlPfnArray(Transfer->DescriptorMdl);
    tablePages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
        Transfer->DescriptorBuffer,
        LECS65_DMA_TABLE_BYTES);

    if (tablePages < 1) {
        return STATUS_INTERNAL_ERROR;
    }

    if ((((ULONGLONG)tablePfns[0]) << PAGE_SHIFT) > MAXULONG) {
        LecTrace(
            "DMA table first page above 4GiB: PFN=%I64u\n",
            (ULONGLONG)tablePfns[0]);
        return STATUS_NOT_SUPPORTED;
    }

    Transfer->DescriptorTablePhysical =
        (ULONG)(((ULONGLONG)tablePfns[0]) << PAGE_SHIFT);

    table = (PLECS65_DMA_DESCRIPTOR)Transfer->DescriptorBuffer;

    for (sourceMdl = Transfer->SourceMdlChain;
         sourceMdl != NULL;
         sourceMdl = sourceMdl->Next) {
        PPFN_NUMBER sourcePfns = MmGetMdlPfnArray(sourceMdl);
        ULONG remaining = MmGetMdlByteCount(sourceMdl);
        ULONG pageOffset = MmGetMdlByteOffset(sourceMdl);
        ULONG pageIndex = 0;

        while (remaining != 0) {
            ULONG bytesThisPage = min(
                remaining,
                PAGE_SIZE - pageOffset);
            ULONGLONG physical =
                (((ULONGLONG)sourcePfns[pageIndex]) << PAGE_SHIFT) +
                pageOffset;
            ULONG pageNumber = tableSlot / 512;
            ULONG slotInPage = tableSlot % 512;

            if (slotInPage == 511) {
                ULONGLONG nextTablePage;

                if (pageNumber + 1 >= tablePages) {
                    return STATUS_BUFFER_OVERFLOW;
                }

                nextTablePage =
                    ((ULONGLONG)tablePfns[pageNumber + 1]) << PAGE_SHIFT;
                if (nextTablePage > MAXULONG) {
                    LecTrace(
                        "DMA table chain page above 4GiB: page=%lu PA=%I64X\n",
                        pageNumber + 1,
                        nextTablePage);
                    return STATUS_NOT_SUPPORTED;
                }

                table[tableSlot].CountDwords = 0;
                table[tableSlot].PhysicalAddress = (ULONG)nextTablePage;
                ++tableSlot;
                ++pageNumber;
                slotInPage = 0;
            }

            if (physical > MAXULONG ||
                physical + bytesThisPage - 1 > MAXULONG) {
                LecTrace(
                    "DMA source page above 4GiB: PA=%I64X bytes=%lu\n",
                    physical,
                    bytesThisPage);
                return STATUS_NOT_SUPPORTED;
            }

            if ((bytesThisPage & 3U) != 0) {
                return STATUS_DATATYPE_MISALIGNMENT;
            }

            table[tableSlot].CountDwords = bytesThisPage >> 2;
            table[tableSlot].PhysicalAddress = (ULONG)physical;
            totalDwords += bytesThisPage >> 2;
            ++tableSlot;

            remaining -= bytesThisPage;
            ++pageIndex;
            pageOffset = 0;
        }
    }

    if ((tableSlot / 512) >= tablePages) {
        return STATUS_BUFFER_OVERFLOW;
    }

    table[tableSlot].CountDwords = 0;
    table[tableSlot].PhysicalAddress = 0;

    Transfer->TotalDwords = totalDwords;

    LecTrace(
        "DMA descriptor table: token=%lu SGTA=0x%08lX dwords=%lu slots=%lu\n",
        Transfer->Token,
        Transfer->DescriptorTablePhysical,
        Transfer->TotalDwords,
        tableSlot + 1);

    return STATUS_SUCCESS;
}

NTSTATUS
LecRegisterTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PVOID UserBuffer,
    _In_ ULONG TotalBytes,
    _In_ KPROCESSOR_MODE AccessMode,
    _Out_ PULONG Token
    )
{
    PLECS65_TRANSFER transfer;
    NTSTATUS status;
    ULONG dataBytes;

    if (Token == NULL ||
        UserBuffer == NULL ||
        TotalBytes < sizeof(ULONG) ||
        TotalBytes > LECS65_DMA_MAX_TRANSFER_BYTES + sizeof(ULONG)) {
        return STATUS_INVALID_PARAMETER;
    }

    dataBytes = TotalBytes - sizeof(ULONG);
    if (dataBytes == 0 || (dataBytes & 3U) != 0) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    transfer = (PLECS65_TRANSFER)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(*transfer),
        LECS65_TAG);
    if (transfer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(transfer, sizeof(*transfer));
    transfer->OwnerProcessId = PsGetCurrentProcessId();
    transfer->UserBuffer = UserBuffer;
    transfer->TotalBytes = TotalBytes;
    transfer->DataBytes = dataBytes;
    KeInitializeEvent(
        &transfer->CompletionEvent,
        SynchronizationEvent,
        FALSE);

    KeWaitForSingleObject(
        &DevExt->TransferMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    do {
        ++DevExt->NextTransferToken;
        if (DevExt->NextTransferToken == 0) {
            ++DevExt->NextTransferToken;
        }
        transfer->Token = DevExt->NextTransferToken;
    } while (LecFindTransferOwned(
        DevExt,
        transfer->Token,
        transfer->OwnerProcessId) != NULL);

    KeReleaseMutex(&DevExt->TransferMutex, FALSE);

    status = LecBuildSourceMdlChain(
        UserBuffer,
        dataBytes,
        AccessMode,
        &transfer->SourceMdlChain);
    if (!NT_SUCCESS(status)) {
        LecFreeTransfer(DevExt, transfer);
        return status;
    }

    status = LecBuildDescriptorTable(transfer);
    if (!NT_SUCCESS(status)) {
        LecFreeTransfer(DevExt, transfer);
        return status;
    }

    KeWaitForSingleObject(
        &DevExt->TransferMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);
    InsertTailList(&DevExt->TransferList, &transfer->Link);
    KeReleaseMutex(&DevExt->TransferMutex, FALSE);

    *Token = transfer->Token;

    LecTrace(
        "TRANSFER register: token=%lu pid=%p user=%p total=%lu data=%lu SGTA=0x%08lX IIMTC=%lu\n",
        transfer->Token,
        transfer->OwnerProcessId,
        transfer->UserBuffer,
        transfer->TotalBytes,
        transfer->DataBytes,
        transfer->DescriptorTablePhysical,
        transfer->TotalDwords);

    return STATUS_SUCCESS;
}

PLECS65_TRANSFER
LecFindTransferOwned(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Token,
    _In_ HANDLE OwnerProcessId
    )
{
    PLIST_ENTRY link;

    for (link = DevExt->TransferList.Flink;
         link != &DevExt->TransferList;
         link = link->Flink) {
        PLECS65_TRANSFER transfer =
            CONTAINING_RECORD(link, LECS65_TRANSFER, Link);

        if (transfer->Token == Token &&
            transfer->OwnerProcessId == OwnerProcessId) {
            return transfer;
        }
    }

    return NULL;
}

NTSTATUS
LecUnregisterTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Token,
    _In_ HANDLE OwnerProcessId
    )
{
    PLECS65_TRANSFER transfer;

    KeWaitForSingleObject(
        &DevExt->TransferMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    transfer = LecFindTransferOwned(
        DevExt,
        Token,
        OwnerProcessId);

    if (transfer == NULL) {
        KeReleaseMutex(&DevExt->TransferMutex, FALSE);
        return STATUS_NOT_FOUND;
    }

    if (DevExt->CurrentTransfer == transfer ||
        transfer->DmaUnsafeToFree) {
        KeReleaseMutex(&DevExt->TransferMutex, FALSE);
        return STATUS_DEVICE_BUSY;
    }

    RemoveEntryList(&transfer->Link);
    KeReleaseMutex(&DevExt->TransferMutex, FALSE);

    LecTrace(
        "TRANSFER unregister: token=%lu pid=%p\n",
        Token,
        OwnerProcessId);

    LecFreeTransfer(DevExt, transfer);
    return STATUS_SUCCESS;
}

VOID
LecReleaseTransfersForProcess(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ HANDLE OwnerProcessId
    )
{
    for (;;) {
        PLECS65_TRANSFER transfer = NULL;
        PLIST_ENTRY link;

        KeWaitForSingleObject(
            &DevExt->TransferMutex,
            Executive,
            KernelMode,
            FALSE,
            NULL);

        for (link = DevExt->TransferList.Flink;
             link != &DevExt->TransferList;
             link = link->Flink) {
            PLECS65_TRANSFER candidate =
                CONTAINING_RECORD(link, LECS65_TRANSFER, Link);

            if (candidate->OwnerProcessId == OwnerProcessId &&
                DevExt->CurrentTransfer != candidate &&
                !candidate->DmaUnsafeToFree) {
                RemoveEntryList(link);
                transfer = candidate;
                break;
            }
        }

        KeReleaseMutex(&DevExt->TransferMutex, FALSE);

        if (transfer == NULL) {
            break;
        }

        LecFreeTransfer(DevExt, transfer);
    }
}

VOID
LecReleaseAllTransfers(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    KIRQL irql;

    KeAcquireSpinLock(&DevExt->DmaCompletionLock, &irql);
    DevExt->CurrentTransfer = NULL;
    DevExt->DmaActiveGeneration = 0;
    KeReleaseSpinLock(&DevExt->DmaCompletionLock, irql);

    for (;;) {
        PLECS65_TRANSFER transfer = NULL;

        KeWaitForSingleObject(
            &DevExt->TransferMutex,
            Executive,
            KernelMode,
            FALSE,
            NULL);

        if (!IsListEmpty(&DevExt->TransferList)) {
            PLIST_ENTRY link = RemoveHeadList(&DevExt->TransferList);
            transfer = CONTAINING_RECORD(
                link,
                LECS65_TRANSFER,
                Link);
        }

        KeReleaseMutex(&DevExt->TransferMutex, FALSE);

        if (transfer == NULL) {
            break;
        }

        /*
         * LecFreeTransfer retains poisoned allocations permanently.
         * They are not recoverable without a verified hardware reset.
         */
        LecFreeTransfer(DevExt, transfer);
    }
}

BOOLEAN
LecInterruptService(
    _In_ PKINTERRUPT Interrupt,
    _In_ PVOID Context
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)Context;
    volatile ULONG* intst;
    volatile ULONG* iimcl;
    volatile ULONG* clrirq;
    volatile ULONG* errs = NULL;
    ULONG status;
    ULONG errorState = 0;

    UNREFERENCED_PARAMETER(Interrupt);

    if (!devExt->Started ||
        devExt->Bar[0] == NULL ||
        devExt->BarLength[0] < LECS65_BAR0_INTST + sizeof(ULONG) ||
        devExt->Bar[1] == NULL ||
        devExt->BarLength[1] < LECS65_BAR1_CLRIRQ + sizeof(ULONG) ||
        devExt->InterruptEnableShadow == 0) {
        return FALSE;
    }

    intst = (volatile ULONG*)(
        devExt->Bar[0] + LECS65_BAR0_INTST);
    status = READ_REGISTER_ULONG(intst);

    status &= devExt->InterruptEnableShadow;
    if (status == 0) {
        return FALSE;
    }

    /*
     * The previously unresolved CFDC2194 status producer is the original
     * ISR itself (FUN_000108D6 at 0x10958). For enabled INTST bit 1 it
     * reads BAR0 ERRS (+0x004) and ORs the raw DWORD into the sticky
     * software latch: main+0x134A == hardware-subobject+0x116A.
     *
     * Preserve the original order: accumulate first, translate ERRS bits
     * 10..14 to BAR1 CLRERR bits 0..4, and acknowledge ERRS with the raw
     * DWORD before the shared INTST acknowledgement below. The mask check
     * above remains the established x64 IRQ-ownership gate.
     */
    if ((status & 0x02UL) != 0) {
        ULONG clearMask = 0;
        volatile ULONG* clrerr = (volatile ULONG*)(
            devExt->Bar[1] + LECS65_BAR1_CLRERR);

        errs = (volatile ULONG*)(devExt->Bar[0] + LECS65_BAR0_ERRS);
        errorState = READ_REGISTER_ULONG(errs);
        InterlockedOr(
            &devExt->LegacyErrorStatusLatch,
            (LONG)errorState);

        if ((errorState & 0x0400UL) != 0) clearMask |= 0x01UL;
        if ((errorState & 0x0800UL) != 0) clearMask |= 0x02UL;
        if ((errorState & 0x1000UL) != 0) clearMask |= 0x04UL;
        if ((errorState & 0x2000UL) != 0) clearMask |= 0x08UL;
        if ((errorState & 0x4000UL) != 0) clearMask |= 0x10UL;

        if (clearMask != 0) {
            WRITE_REGISTER_ULONG(clrerr, clearMask);
        }
        WRITE_REGISTER_ULONG(errs, errorState);
    }

    /*
     * Legacy FUN_000108D6 performs an immediate source-specific acknowledge
     * for transfer-completion bit 0 before it queues deferred processing.
     *
     * FUN_00014847 is constructed at main+0x1E0 and stores BAR0 IIMCL
     * (offset 0x048) at subobject+0x1D8, i.e. main+0x3B8.  The ISR's bit-0
     * branch writes zero through exactly main+0x3B8.  Mirror that write here
     * before recording/acknowledging INTST, rather than waiting for the
     * synchronous acquisition thread's later cleanup.
     */
    if ((status & 0x01UL) != 0) {
        ULONGLONG generation = (ULONGLONG)InterlockedCompareExchange64(
            &devExt->DmaActiveGeneration, 0, 0);

        if (generation != 0) {
            (void)LecDmaCompletionObservePhysicalIrq(
                &devExt->DmaCompletion,
                generation);
        }
        iimcl = (volatile ULONG*)(
            devExt->Bar[0] + LECS65_BAR0_IIMCL);
        WRITE_REGISTER_ULONG(iimcl, 0UL);
    }

    /*
     * The same legacy ISR performs a second, source-specific acknowledge for
     * board-side interrupt sources 2..5 through BAR1 CLRIRQ.
     *
     * FUN_00014847 maps acquisition-subobject +0x200 to
     * BAR1 +0x008 ("CLRIRQ"). Because the acquisition subobject lives at
     * main+0x1E0, original ISR writes through main+0x3E0:
     *
     *   INTST 0x04 -> CLRIRQ 1
     *   INTST 0x08 -> CLRIRQ 2
     *   INTST 0x10 -> CLRIRQ 4
     *   INTST 0x20 -> CLRIRQ 8
     *
     * These writes happen before the common INTST write-back acknowledge.
     * Omitting them leaves the underlying board source asserted and causes
     * the CFDC2180 command-status event to retrigger continuously.
     */
    clrirq = (volatile ULONG*)(
        devExt->Bar[1] + LECS65_BAR1_CLRIRQ);

    if ((status & 0x04UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 1UL);
    }
    if ((status & 0x08UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 2UL);
    }
    if ((status & 0x10UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 4UL);
    }
    if ((status & 0x20UL) != 0) {
        WRITE_REGISTER_ULONG(clrirq, 8UL);
    }

    /*
     * Legacy FUN_000108D6 accumulates every enabled interrupt source in a
     * pending bitmap before it acknowledges the hardware and queues the DPC.
     * Coalesce multiple interrupts safely when one DPC is already queued.
     */
    InterlockedOr(
        (volatile LONG*)&devExt->InterruptPendingShadow,
        (LONG)status);

    /*
     * Legacy hardware uses write-back-to-acknowledge semantics for INTST.
     * Only acknowledge sources that this replacement driver explicitly owns.
     */
    WRITE_REGISTER_ULONG(intst, status);

    /*
     * Original FUN_000108D6 waits 1 us after acknowledging an ERRS IRQ.
     * If INTST bit 1 is still set and ERRS is unchanged, it widens the
     * cached ERRM mask with those error bits and marks bit 31 of the
     * sticky software latch. This is a genuine second status producer
     * (original 0x10A67), not a fabricated constant/error-code mapping.
     */
    if ((status & 0x02UL) != 0) {
        KeStallExecutionProcessor(1);
        if ((READ_REGISTER_ULONG(intst) & 0x02UL) != 0 &&
            READ_REGISTER_ULONG(errs) == errorState) {
            ULONG previousMask = (ULONG)InterlockedOr(
                &devExt->LegacyErrmShadow,
                (LONG)errorState);
            volatile ULONG* errm = (volatile ULONG*)(
                devExt->Bar[0] + LECS65_BAR0_ERRM);

            WRITE_REGISTER_ULONG(errm, previousMask | errorState);
            InterlockedOr(
                &devExt->LegacyErrorStatusLatch,
                (LONG)0x80000000UL);
        }
    }

    /*
     * The original ISR queues deferred processing for every accepted source,
     * not only acquisition-completion bit 0. This is required for the
     * CFDC2180/CFDC218C user events that wake XStream's acquisition control
     * threads before any CFDC2124/CFDC2138 request exists.
     */
    KeInsertQueueDpc(
        &devExt->InterruptDpc,
        NULL,
        NULL);

    return TRUE;
}

VOID
LecInterruptDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    )
{
    PLECS65_DEVICE_EXTENSION devExt =
        (PLECS65_DEVICE_EXTENSION)DeferredContext;
    ULONG pending;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    /*
     * Recovered legacy DPC mapping:
     *
     *   INTST 0x01 -> selected acquisition transfer completion event
     *   INTST 0x02 -> CFDC218C event
     *   INTST 0x04,
     *         0x10,
     *         0x20 -> CFDC2180 event
     *   INTST 0x08 -> read/clear BAR1 HWInt, latch its 16-bit enabled
     *                 command mask, wake CFDC2180 for nonzero HWInt.
     *
     * FUN_00011390 calls FUN_000176A2(transport, &hwIntWord).
     * Ghidra's C decompiler misses the stack out-parameter and misleadingly
     * displays the following FUN_000157A6(this, hwIntWord) as a zero input.
     * Raw assembly at 0x114A2..0x114C8 proves the actual value is passed.
     *
     * The separate internal RX_CONTROL-ready event is not required by the
     * replacement's existing bounded synchronous receive polling.
     */
    pending = (ULONG)InterlockedExchange(
        (volatile LONG*)&devExt->InterruptPendingShadow,
        0);

    if ((pending & 0x01UL) != 0) {
        PLECS65_TRANSFER transfer;
        ULONGLONG generation;

        KeAcquireSpinLockAtDpcLevel(&devExt->DmaCompletionLock);
        transfer = (PLECS65_TRANSFER)devExt->CurrentTransfer;
        generation = (ULONGLONG)devExt->DmaActiveGeneration;
        if (transfer != NULL &&
            transfer->DmaGeneration == generation &&
            LecDmaCompletionConsumeSignal(
                &devExt->DmaCompletion,
                generation)) {
            KeSetEvent(
                &transfer->CompletionEvent,
                IO_NO_INCREMENT,
                FALSE);
        }
        KeReleaseSpinLockFromDpcLevel(&devExt->DmaCompletionLock);
    }

    if ((pending & (0x02UL | 0x04UL | 0x10UL | 0x20UL)) != 0) {
        KeAcquireSpinLockAtDpcLevel(&devExt->LegacyEventLock);

        /*
         * The legacy command-status object has a 16-bit enabled mask and a
         * sticky 16-bit pending mask. FUN_00011390 latches these command bits
         * from the corresponding hardware interrupt sources before waking the
         * CFDC2180 event:
         *
         *   INTST 0x04 -> command pending 0x0080
         *   INTST 0x10 -> command pending 0x0800
         *   INTST 0x20 -> command pending 0x0100
         *
         * Only bits enabled through family-0 opcode 0x85 become pending.
         */
        if ((pending & 0x04UL) != 0) {
            devExt->LegacyCommandPendingMask |=
                (USHORT)(devExt->LegacyCommandEnableMask & 0x0080U);
        }
        if ((pending & 0x10UL) != 0) {
            devExt->LegacyCommandPendingMask |=
                (USHORT)(devExt->LegacyCommandEnableMask & 0x0800U);
        }
        if ((pending & 0x20UL) != 0) {
            devExt->LegacyCommandPendingMask |=
                (USHORT)(devExt->LegacyCommandEnableMask & 0x0100U);
        }

        if ((pending & 0x02UL) != 0 &&
            devExt->LegacyEvent1 != NULL) {
            KeSetEvent(
                devExt->LegacyEvent1,
                IO_NO_INCREMENT,
                FALSE);
        }

        if ((pending & (0x04UL | 0x10UL | 0x20UL)) != 0 &&
            devExt->LegacyEvent0 != NULL) {
            KeSetEvent(
                devExt->LegacyEvent0,
                IO_NO_INCREMENT,
                FALSE);
        }

        KeReleaseSpinLockFromDpcLevel(&devExt->LegacyEventLock);
    }

    /*
     * Legacy INTST source 0x08 is also an asynchronous command-status path.
     * FUN_000176A2 reads BAR1 HWInt (0x410), returns its low 16 bits through
     * a stack out-parameter, and clears the register by writing zero only
     * when nonzero. FUN_00011390 then latches (HWInt & enabledMask) and wakes
     * CFDC2180 (FUN_00010816), independent of the separate RX-ready event.
     *
     * In particular, the original ProBus insertion trace receives HWInt
     * 0x0200 here. Do not synthesize probe presence without real hardware
     * input, and do not repeatedly poll HWInt from this DPC.
     */
    if ((pending & 0x08UL) != 0 &&
        devExt->Bar[1] != NULL &&
        devExt->BarLength[1] >= LECS65_BAR1_HWINT + sizeof(ULONG)) {
        volatile ULONG* hwInt = (volatile ULONG*)(
            devExt->Bar[1] + LECS65_BAR1_HWINT);
        USHORT commandBits = (USHORT)READ_REGISTER_ULONG(hwInt);

        if (commandBits != 0) {
            WRITE_REGISTER_ULONG(hwInt, 0UL);

            KeAcquireSpinLockAtDpcLevel(&devExt->LegacyEventLock);
            devExt->LegacyCommandPendingMask |=
                (USHORT)(commandBits & devExt->LegacyCommandEnableMask);

            if (devExt->LegacyEvent0 != NULL) {
                KeSetEvent(
                    devExt->LegacyEvent0,
                    IO_NO_INCREMENT,
                    FALSE);
            }
            KeReleaseSpinLockFromDpcLevel(&devExt->LegacyEventLock);
        }
    }
}

/*
 * Original CFDC2400 (FUN_00012EDE) copies a caller DWORD into
 * DAT_0001CE1C, ORs it into DAT_0001CE10 under interrupt
 * synchronization (LAB_00012EC2), then calls the *derived hardware
 * subobject* virtual slot +0x24 (table 0x1C62C, entry 0x1C650).
 * The actual slot points to thunk 0x114F2, which adjusts this by
 * -0x1E0 and directly invokes original DPC dispatcher FUN_00011390.
 *
 * Invoke the existing replacement DPC processing immediately, not
 * KeInsertQueueDpc: that would change the observable timing. Its event
 * spin-lock operations require DISPATCH_LEVEL, so temporarily raise
 * IRQL if DeviceControl was entered below that level. Never invent
 * BAR writes or a hardware interrupt to implement this request.
 *
 * A zero mask still runs the pending dispatcher, exactly as the
 * original unconditional post-callback virtual call does. A nonzero
 * mask is software interrupt injection, not a passive query.
 */
NTSTATUS
LecInjectLegacyPendingAndDispatch(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG PendingMask
    )
{
    KIRQL callerIrql = KeGetCurrentIrql();
    KIRQL previousIrql = callerIrql;
    BOOLEAN raisedIrql = FALSE;

    if (callerIrql > DISPATCH_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    /*
     * CFDC2400 can force software-pending bit 0 and wake a DMA waiter
     * without any hardware completion. Never accept synthetic acquisition
     * completion, even when no transfer is currently selected.
     */
    if ((PendingMask & 0x01UL) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    (VOID)InterlockedOr(
        (volatile LONG*)&DevExt->InterruptPendingShadow,
        (LONG)PendingMask);

    if (callerIrql < DISPATCH_LEVEL) {
        previousIrql = KeRaiseIrqlToDpcLevel();
        raisedIrql = TRUE;
    }

    LecInterruptDpc(
        &DevExt->InterruptDpc,
        DevExt,
        NULL,
        NULL);

    if (raisedIrql) {
        KeLowerIrql(previousIrql);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
LecConnectInterrupt(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    NTSTATUS status;

    if (DevExt->InterruptConnected) {
        return STATUS_SUCCESS;
    }

    if (DevExt->InterruptVector == 0) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    status = IoConnectInterrupt(
        &DevExt->InterruptObject,
        LecInterruptService,
        DevExt,
        NULL,
        DevExt->InterruptVector,
        DevExt->InterruptIrql,
        DevExt->InterruptSynchronizeIrql,
        DevExt->InterruptMode,
        DevExt->InterruptShareVector,
        DevExt->InterruptAffinity,
        FALSE);

    if (NT_SUCCESS(status)) {
        DevExt->InterruptConnected = TRUE;
        LecTrace(
            "IRQ connected: vector=%lu irql=%u sync=%u affinity=%p mode=%u shared=%u\n",
            DevExt->InterruptVector,
            DevExt->InterruptIrql,
            DevExt->InterruptSynchronizeIrql,
            (PVOID)DevExt->InterruptAffinity,
            (ULONG)DevExt->InterruptMode,
            DevExt->InterruptShareVector);
    }
    else {
        LecTrace(
            "IRQ connect failed: 0x%08X\n",
            status);
    }

    return status;
}

static BOOLEAN
LecMaskHardwareInterruptsSynchronized(_In_ PVOID Context)
{
    PLECS65_DEVICE_EXTENSION devExt = (PLECS65_DEVICE_EXTENSION)Context;

    /*
     * INTEN is BAR0+0x084. Serialize the final mask write with the ISR
     * while the interrupt connection and BAR mapping are still valid.
     */
    InterlockedExchange(
        (volatile LONG*)&devExt->InterruptEnableShadow, 0);

    if (devExt->Bar[0] != NULL &&
        devExt->BarLength[0] >= 0x084 + sizeof(ULONG)) {
        WRITE_REGISTER_ULONG(
            (volatile ULONG*)(devExt->Bar[0] + 0x084), 0);
    }

    return TRUE;
}

VOID
LecQuiesceDeferredWork(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ BOOLEAN HardwareAccessible)
{
    /*
     * Caller has disabled admission and drained synchronous IOCTLs.
     * SURPRISE_REMOVAL must never touch potentially absent PCI MMIO.
     */
    if (HardwareAccessible &&
        DevExt->Bar[0] != NULL &&
        DevExt->BarLength[0] >= 0x084 + sizeof(ULONG)) {
        if (DevExt->InterruptConnected &&
            DevExt->InterruptObject != NULL) {
            (VOID)KeSynchronizeExecution(
                DevExt->InterruptObject,
                LecMaskHardwareInterruptsSynchronized,
                DevExt);
        }
        else {
            (VOID)LecMaskHardwareInterruptsSynchronized(DevExt);
        }
    }

    LecDisconnectInterrupt(DevExt);

    /*
     * No more ISRs can queue work after disconnect. Remove any pending
     * instance, then wait for an instance already running on another CPU.
     * The flush is kernel-wide, therefore use only for PnP teardown.
     */
    (VOID)KeRemoveQueueDpc(&DevExt->InterruptDpc);
    KeFlushQueuedDpcs();

    if (DevExt->LegacyTimerInitialized) {
        (VOID)KeCancelTimer(&DevExt->LegacyTimer);
        DevExt->LegacyTimerInitialized = FALSE;
        DevExt->LegacyTimerStartTime.QuadPart = 0;
        DevExt->LegacyTimerDurationMs = 0;
    }

    /*
     * The next START must repopulate hardware register shadows; the device
     * may have lost these values during the PnP transition.
     */
    DevExt->LegacyMamShadowInitialized = FALSE;
    DevExt->LegacyMamSeqShadowInitialized = FALSE;
    DevExt->LegacySpiInitialized = FALSE;
}

VOID
LecDisconnectInterrupt(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
    if (DevExt->InterruptConnected &&
        DevExt->InterruptObject != NULL) {
        IoDisconnectInterrupt(DevExt->InterruptObject);
        DevExt->InterruptObject = NULL;
        DevExt->InterruptConnected = FALSE;
        DevExt->InterruptEnableShadow = 0;
        InterlockedExchange(
            (volatile LONG*)&DevExt->InterruptPendingShadow,
            0);
        InterlockedExchange(&DevExt->LegacyErrorStatusLatch, 0);
        InterlockedExchange(&DevExt->LegacyErrmShadow, (LONG)0xFFFFFFFFUL);
        LecTrace("IRQ disconnected\n");
    }
}
