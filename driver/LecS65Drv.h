#pragma once

#include <ntddk.h>
#include <stdint.h>
#include "DmaCompletion.h"

#define LECS65_TAG '56SL'

#define LECS65_DEVICE_NAME      L"\\Device\\ALADDINAcqDriver0"
#define LECS65_DOS_DEVICE_NAME  L"\\DosDevices\\ALADDINAcqDriver0"

#define LECS65_BAR_COUNT 3
#define LECS65_REGISTER_BAR_LENGTH 0x200
#define LECS65_INTERFACE_COUNT 5
#define LECS65_TRACE_CAPACITY 256
#define LECS65_TRACE_PREVIEW_BYTES 256
#define LECS65_TRACE_OUTPUT_PREVIEW_BYTES 128
#define LECS65_DMA_TABLE_BYTES 0x33000
#define LECS65_DMA_MAX_TRANSFER_BYTES 0x06000000
#define LECS65_DMA_MDL_CHUNK_BYTES 0x02000000
#define LECS65_ONEWIRE_OFFSET 0x40
#define LECS65_ONEWIRE_BUSY   0x01
#define LECS65_ONEWIRE_DATA   0x02
#define LECS65_ONEWIRE_POLL_LIMIT 100000

#define LECS65_BAR0_START_OFFSET 0x00C
#define LECS65_BAR1_ITMODE_OFFSET 0x000
#define LECS65_BAR2_BUZZER_OFFSET 0x000

#define LECS65_BAR1_TX_CONTROL 0x400
#define LECS65_BAR1_RX_CONTROL 0x404
#define LECS65_BAR1_TX_COUNT   0x408
#define LECS65_BAR1_TX_DATA    0x420
#define LECS65_BAR1_RX_DATA    0x600
#define LECS65_BAR1_JTAG_NUM   0x020
#define LECS65_BAR1_JTAG_DATA  0x024
#define LECS65_BAR1_JTAG_IN    0x028
#define LECS65_TRANSFER_TIMEOUT_POLLS 5000

/* Recovered legacy IOCTLs used by the first bring-up build. */
#define LECS65_IOCTL_CFDC2110          ((ULONG)0xCFDC2110)
#define LECS65_IOCTL_REGISTER_TRANSFER ((ULONG)0xCFDC2124)
#define LECS65_IOCTL_UNREGISTER_TRANSFER ((ULONG)0xCFDC2128)
#define LECS65_IOCTL_ACQUIRE_BUFFERED   ((ULONG)0xCFDC2138)
#define LECS65_IOCTL_ACQUIRE_NEITHER    ((ULONG)0xCFDD219F)
#define LECS65_IOCTL_CFDC212C          ((ULONG)0xCFDC212C)
#define LECS65_IOCTL_CFDC2184          ((ULONG)0xCFDC2184)
#define LECS65_IOCTL_00222400            ((ULONG)0x00222400)
#define LECS65_IOCTL_DELAY_MILLISECONDS  ((ULONG)0x00222C00)
#define LECS65_IOCTL_SET_FLAG_BYTE       ((ULONG)0x00222C04)
#define LECS65_IOCTL_SET_TRACE_CONTROL   ((ULONG)0x00223000)
#define LECS65_IOCTL_QUERY_BUFFER_A      ((ULONG)0x00223004)
#define LECS65_IOCTL_SET_ONE_REGISTER    ((ULONG)0x0022303C)
#define LECS65_IOCTL_QUERY_BUFFER_B      ((ULONG)0x00223040)
#define LECS65_IOCTL_READ_START_REGISTER ((ULONG)0x00223044)
#define LECS65_IOCTL_GET_DALLAS_ID        ((ULONG)0x00223080)
#define LECS65_IOCTL_READ_DALLAS_MEMORY   ((ULONG)0x00223084)
#define LECS65_IOCTL_SET_THREE_EVENTS     ((ULONG)0x00223100)
#define LECS65_IOCTL_SET_EVENT_0          ((ULONG)0xCFDC2180)
#define LECS65_IOCTL_SET_EVENT_1          ((ULONG)0xCFDC218C)
#define LECS65_IOCTL_CFDC2190              ((ULONG)0xCFDC2190)
#define LECS65_IOCTL_CFDC2194              ((ULONG)0xCFDC2194)
#define LECS65_IOCTL_CFDC2400              ((ULONG)0xCFDC2400)
#define LECS65_IOCTL_REGISTER_READ     ((ULONG)0xCFDC21C0)
#define LECS65_IOCTL_REGISTER_WRITE    ((ULONG)0xCFDC21C4)
#define LECS65_IOCTL_GET_DRIVER_BUILD  ((ULONG)0xCFDC21C8)

#define LECS65_LEGACY_DRIVER_BUILD     ((ULONG)1002)

/* Private diagnostics. These are not part of the LeCroy ABI. */
#define LECS65_IOCTL_DEBUG_GET_STATS \
    CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define LECS65_IOCTL_DEBUG_CLEAR_STATS \
    CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define LECS65_IOCTL_DEBUG_GET_BARS \
    CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define LECS65_IOCTL_DEBUG_GET_TRACE \
    CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
#define LECS65_IOCTL_DEBUG_CLEAR_TRACE \
    CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define LECS65_IOCTL_DEBUG_GET_PCI_CONFIG \
    CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_READ_ACCESS)

#pragma pack(push, 1)

typedef struct _LECS65_REG_READ_LEGACY {
    ULONG Offset;
} LECS65_REG_READ_LEGACY, *PLECS65_REG_READ_LEGACY;

typedef struct _LECS65_REG_READ_EXT {
    UCHAR Bar;
    ULONG Offset;
} LECS65_REG_READ_EXT, *PLECS65_REG_READ_EXT;

typedef struct _LECS65_REG_WRITE_LEGACY {
    ULONG Offset;
    ULONG Value;
} LECS65_REG_WRITE_LEGACY, *PLECS65_REG_WRITE_LEGACY;

typedef struct _LECS65_REG_WRITE_EXT {
    UCHAR Bar;
    ULONG Offset;
    ULONG Value;
} LECS65_REG_WRITE_EXT, *PLECS65_REG_WRITE_EXT;

#pragma pack(pop)

typedef struct _LECS65_DEBUG_STATS {
    ULONG Version;
    ULONG LegacyBuild;
    ULONGLONG CreateCount;
    ULONGLONG CloseCount;
    ULONGLONG IoctlCount;
    ULONGLONG UnknownIoctlCount;
    ULONG LastIoctl;
    ULONG Reserved;
} LECS65_DEBUG_STATS, *PLECS65_DEBUG_STATS;

typedef struct _LECS65_DEBUG_BAR_ENTRY {
    ULONGLONG PhysicalAddress;
    ULONG Length;
    ULONG Reserved;
} LECS65_DEBUG_BAR_ENTRY, *PLECS65_DEBUG_BAR_ENTRY;

typedef struct _LECS65_DEBUG_BARS {
    ULONG Version;
    ULONG Count;
    LECS65_DEBUG_BAR_ENTRY Entry[LECS65_BAR_COUNT];
    LECS65_DEBUG_BAR_ENTRY Bulk;
} LECS65_DEBUG_BARS, *PLECS65_DEBUG_BARS;


typedef struct _LECS65_DEBUG_PCI_CONFIG {
    ULONG Version;
    ULONG BytesRead;
    ULONG BusNumber;
    ULONG DeviceNumber;
    ULONG FunctionNumber;
    USHORT VendorId;
    USHORT DeviceId;
    USHORT Command;
    USHORT Status;
    UCHAR RevisionId;
    UCHAR ProgIf;
    UCHAR SubClass;
    UCHAR BaseClass;
    UCHAR CacheLineSize;
    UCHAR LatencyTimer;
    UCHAR HeaderType;
    UCHAR Bist;
    ULONG Bar[6];
    ULONG CardbusCisPointer;
    USHORT SubsystemVendorId;
    USHORT SubsystemId;
    ULONG ExpansionRomBase;
    UCHAR CapabilitiesPointer;
    UCHAR Reserved1[3];
    ULONG Reserved2;
    UCHAR InterruptLine;
    UCHAR InterruptPin;
    UCHAR MinimumGrant;
    UCHAR MaximumLatency;
} LECS65_DEBUG_PCI_CONFIG, *PLECS65_DEBUG_PCI_CONFIG;

typedef struct _LECS65_DEBUG_TRACE_ENTRY {
    ULONGLONG Sequence;
    ULONGLONG TimestampTicks;
    ULONGLONG ProcessId;
    ULONGLONG Information;
    ULONGLONG Type3InputBuffer;
    ULONGLONG UserBuffer;
    ULONG Ioctl;
    ULONG InputLength;
    ULONG OutputLength;
    ULONG Status;
    ULONG InputPreviewLength;
    ULONG OutputPreviewLength;
    UCHAR Method;
    UCHAR Wow64;
    USHORT Reserved;
    UCHAR InputPreview[LECS65_TRACE_PREVIEW_BYTES];
    UCHAR OutputPreview[LECS65_TRACE_OUTPUT_PREVIEW_BYTES];
} LECS65_DEBUG_TRACE_ENTRY, *PLECS65_DEBUG_TRACE_ENTRY;

typedef struct _LECS65_DEBUG_TRACE {
    ULONG Version;
    ULONG Count;
    ULONGLONG TotalSeen;
    LECS65_DEBUG_TRACE_ENTRY Entry[LECS65_TRACE_CAPACITY];
} LECS65_DEBUG_TRACE, *PLECS65_DEBUG_TRACE;


typedef struct _LECS65_DMA_DESCRIPTOR {
    ULONG CountDwords;
    ULONG PhysicalAddress;
} LECS65_DMA_DESCRIPTOR, *PLECS65_DMA_DESCRIPTOR;

typedef struct _LECS65_TRANSFER {
    LIST_ENTRY Link;
    ULONG Token;
    HANDLE OwnerProcessId;
    PVOID UserBuffer;
    ULONG TotalBytes;
    ULONG DataBytes;
    PMDL SourceMdlChain;
    PVOID DescriptorBuffer;
    PMDL DescriptorMdl;
    ULONG DescriptorTablePhysical;
    ULONG TotalDwords;
    ULONGLONG DmaGeneration;
    /* Remains permanently pinned if DMA idle cannot be proven. */
    BOOLEAN DmaUnsafeToFree;
    /* Atomic LECS65_TRANSFER_QUARANTINE_* ownership state. */
    volatile LONG QuarantineOwnership;
    KEVENT CompletionEvent;
} LECS65_TRANSFER, *PLECS65_TRANSFER;

struct _LECS65_DMA_PNP_PUBLICATION;
typedef struct _LECS65_DMA_PNP_PUBLICATION
    LECS65_DMA_PNP_PUBLICATION, *PLECS65_DMA_PNP_PUBLICATION;

typedef struct _LECS65_DEVICE_EXTENSION {
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT PhysicalDeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;
    IO_REMOVE_LOCK RemoveLock;
    PLECS65_DMA_PNP_PUBLICATION DmaPnpPublication;
    KSPIN_LOCK IoAdmissionLock;
    KEVENT IoIdleEvent;
    ULONG ActiveIoctls;
    BOOLEAN AcceptIoctls;

    BOOLEAN Started;
    BOOLEAN Removed;
    BOOLEAN SymbolicLinkCreated;
    UCHAR Reserved0;

    PUCHAR Bar[LECS65_BAR_COUNT];
    ULONG BarLength[LECS65_BAR_COUNT];
    PHYSICAL_ADDRESS BarPhysical[LECS65_BAR_COUNT];

    PUCHAR BulkMmio;
    ULONG BulkMmioLength;
    PHYSICAL_ADDRESS BulkMmioPhysical;

    UNICODE_STRING InterfaceLink[LECS65_INTERFACE_COUNT];
    BOOLEAN InterfaceRegistered[LECS65_INTERFACE_COUNT];
    BOOLEAN InterfaceEnabled[LECS65_INTERFACE_COUNT];

    volatile LONG64 CreateCount;
    volatile LONG64 CloseCount;
    volatile LONG64 IoctlCount;
    volatile LONG64 UnknownIoctlCount;
    volatile LONG LastIoctl;

    KMUTEX DallasMutex;
    KMUTEX TransferMutex;
    LIST_ENTRY TransferList;
    ULONG NextTransferToken;
    volatile PLECS65_TRANSFER CurrentTransfer;
    KSPIN_LOCK DmaCompletionLock;
    LECS65_DMA_COMPLETION_TRACKER DmaCompletion;
    volatile LONG64 DmaGenerationCounter;
    volatile LONG64 DmaActiveGeneration;
    /*
     * An unknown DMA finish is terminal for this FDO: software cannot
     * prove that old bus-master reads/writes have stopped.
     */
    volatile LONG DmaUnknownActive;

    PKINTERRUPT InterruptObject;
    ULONG InterruptVector;
    KIRQL InterruptIrql;
    KIRQL InterruptSynchronizeIrql;
    KAFFINITY InterruptAffinity;
    KINTERRUPT_MODE InterruptMode;
    BOOLEAN InterruptShareVector;
    BOOLEAN InterruptConnected;
    USHORT ReservedInterrupt;
    volatile ULONG InterruptEnableShadow;
    volatile ULONG InterruptPendingShadow;
    /* Original FUN_000108D6 accumulates ERRS at main+0x134A. */
    volatile LONG LegacyErrorStatusLatch;
    /* BAR0 ERRM cached wrapper value (original main+0x38C). */
    volatile LONG LegacyErrmShadow;
    KDPC InterruptDpc;

    KSPIN_LOCK LegacyEventLock;
    PKEVENT LegacyEvent0;
    PKEVENT LegacyEvent1;
    PKEVENT LegacyEvent2;
    PKEVENT LegacyEvent3;
    PKEVENT LegacyEvent4;
    UCHAR LegacyFlagByte;
    UCHAR ReservedLegacy;
    USHORT LegacyCommandPendingMask;
    USHORT LegacyCommandEnableMask;
    ULONG LegacySpiControlShadow;
    BOOLEAN LegacySpiInitialized;
    BOOLEAN LegacyTimerInitialized;
    UCHAR ReservedSpi[2];
    KTIMER LegacyTimer;
    LARGE_INTEGER LegacyTimerStartTime;
    ULONG LegacyTimerDurationMs;

    USHORT LegacyMamShadow[256];
    BOOLEAN LegacyMamShadowInitialized;
    UCHAR ReservedMamShadow[3];

    USHORT LegacyMamSeqShadow[256];
    BOOLEAN LegacyMamSeqShadowInitialized;
    UCHAR ReservedMamSeqShadow[3];

    KSPIN_LOCK TraceLock;
    ULONGLONG TraceNextSequence;
    ULONG TraceWriteIndex;
    ULONG TraceCount;
    LECS65_DEBUG_TRACE_ENTRY Trace[LECS65_TRACE_CAPACITY];
} LECS65_DEVICE_EXTENSION, *PLECS65_DEVICE_EXTENSION;

extern const GUID g_LecS65InterfaceGuids[LECS65_INTERFACE_COUNT];

DRIVER_INITIALIZE DriverEntry;
DRIVER_ADD_DEVICE LecS65AddDevice;

_Dispatch_type_(IRP_MJ_CREATE)
DRIVER_DISPATCH LecS65Create;
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH LecS65Close;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH LecS65DeviceControl;
_Dispatch_type_(IRP_MJ_PNP)
DRIVER_DISPATCH LecS65Pnp;
_Dispatch_type_(IRP_MJ_POWER)
DRIVER_DISPATCH LecS65Power;
DRIVER_DISPATCH LecS65PassThrough;

VOID LecS65Unload(_In_ PDRIVER_OBJECT DriverObject);

VOID LecTrace(_In_z_ _Printf_format_string_ PCSTR Format, ...);
VOID LecHexDump(_In_reads_bytes_opt_(Length) const UCHAR* Buffer, _In_ ULONG Length);
VOID LecUnmapBars(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
BOOLEAN LecEnterIoctl(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
VOID LecLeaveIoctl(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
VOID LecSetIoctlAdmission(_Inout_ PLECS65_DEVICE_EXTENSION DevExt, _In_ BOOLEAN Enable);
VOID LecDrainIoctls(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);

NTSTATUS LecForwardLockedIrp(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PIRP Irp,
    _In_ BOOLEAN IsPowerIrp);


NTSTATUS LecForwardAndWait(
    _In_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PIRP Irp
    );

NTSTATUS LecHandleStartDevice(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PCM_RESOURCE_LIST TranslatedResources
    );

VOID LecDisableInterfaces(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
VOID LecReleaseLegacyEvents(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);

NTSTATUS LecReadPciConfig(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_ PLECS65_DEBUG_PCI_CONFIG Config);

NTSTATUS LecConnectInterrupt(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
VOID LecDisconnectInterrupt(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
VOID LecQuiesceDeferredWork(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ BOOLEAN HardwareAccessible);

BOOLEAN LecInterruptService(_In_ PKINTERRUPT Interrupt, _In_ PVOID Context);
VOID LecInterruptDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2);
/*
 * CFDC2400's derived hardware-subobject vtable +0x24 calls original
 * DPC dispatcher synchronously after a software pending-bit OR.
 * Mirror that operation at safe DPC IRQL, using the existing core.
 */
NTSTATUS LecInjectLegacyPendingAndDispatch(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG PendingMask);

NTSTATUS LecRegisterTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ PVOID UserBuffer,
    _In_ ULONG TotalBytes,
    _In_ KPROCESSOR_MODE AccessMode,
    _Out_ PULONG Token);
NTSTATUS LecUnregisterTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Token,
    _In_ HANDLE OwnerProcessId);
VOID LecReleaseTransfersForProcess(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ HANDLE OwnerProcessId);
VOID LecReleaseAllTransfers(_Inout_ PLECS65_DEVICE_EXTENSION DevExt);
NTSTATUS LecSelectDmaTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer,
    _Out_ PULONGLONG Generation);
BOOLEAN LecArmSelectedDma(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation);
BOOLEAN LecCancelSelectedDmaArm(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation);
NTSTATUS LecLaunchSelectedDma(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONGLONG Generation,
    _In_ volatile ULONG* CompletionControl,
    _In_ volatile ULONG* GoRegister,
    _In_ ULONG GoValue,
    _Out_ PBOOLEAN DmaLaunched);
VOID LecDeselectDmaTransfer(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer,
    _In_ ULONGLONG Generation);
VOID LecMarkDmaUnknownActive(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Inout_ PLECS65_TRANSFER Transfer);
PLECS65_TRANSFER LecFindTransferOwned(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _In_ ULONG Token,
    _In_ HANDLE OwnerProcessId);
