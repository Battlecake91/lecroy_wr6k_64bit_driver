#include "LecS65Drv.h"

#define LECS65_BAR0_SGTA   0x040
#define LECS65_BAR0_IIMTC  0x044
#define LECS65_BAR0_IIMCL  0x048
#define LECS65_BAR0_INTST  0x080
#define LECS65_BAR0_INTEN  0x084

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
    _In_ PLECS65_TRANSFER Transfer
    )
{
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
        LecFreeTransfer(transfer);
        return status;
    }

    status = LecBuildDescriptorTable(transfer);
    if (!NT_SUCCESS(status)) {
        LecFreeTransfer(transfer);
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

    if (DevExt->CurrentTransfer == transfer) {
        KeReleaseMutex(&DevExt->TransferMutex, FALSE);
        return STATUS_DEVICE_BUSY;
    }

    RemoveEntryList(&transfer->Link);
    KeReleaseMutex(&DevExt->TransferMutex, FALSE);

    LecTrace(
        "TRANSFER unregister: token=%lu pid=%p\n",
        Token,
        OwnerProcessId);

    LecFreeTransfer(transfer);
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
                DevExt->CurrentTransfer != candidate) {
                RemoveEntryList(link);
                transfer = candidate;
                break;
            }
        }

        KeReleaseMutex(&DevExt->TransferMutex, FALSE);

        if (transfer == NULL) {
            break;
        }

        LecFreeTransfer(transfer);
    }
}

VOID
LecReleaseAllTransfers(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt
    )
{
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

        DevExt->CurrentTransfer = NULL;
        KeReleaseMutex(&DevExt->TransferMutex, FALSE);

        if (transfer == NULL) {
            break;
        }

        LecFreeTransfer(transfer);
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
    ULONG status;

    UNREFERENCED_PARAMETER(Interrupt);

    if (!devExt->Started ||
        devExt->Bar[0] == NULL ||
        devExt->BarLength[0] < LECS65_BAR0_INTST + sizeof(ULONG) ||
        devExt->InterruptEnableShadow == 0) {
        return FALSE;
    }

    intst = (volatile ULONG*)(
        devExt->Bar[0] + LECS65_BAR0_INTST);
    status = READ_REGISTER_ULONG(intst);

    if ((status & devExt->InterruptEnableShadow) == 0) {
        return FALSE;
    }

    /*
     * Legacy hardware uses write-back-to-acknowledge semantics for INTST.
     * Only acknowledge sources that this replacement driver explicitly owns.
     */
    status &= devExt->InterruptEnableShadow;
    WRITE_REGISTER_ULONG(intst, status);

    if ((status & 0x01UL) != 0) {
        KeInsertQueueDpc(
            &devExt->InterruptDpc,
            ULongToPtr(status),
            NULL);
    }

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
    PLECS65_TRANSFER transfer;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    transfer = (PLECS65_TRANSFER)devExt->CurrentTransfer;
    if (transfer != NULL) {
        KeSetEvent(
            &transfer->CompletionEvent,
            IO_NO_INCREMENT,
            FALSE);
    }
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
        LecTrace("IRQ disconnected\n");
    }
}
