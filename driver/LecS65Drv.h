#pragma once

#include <ntddk.h>
#include <stdint.h>

#define LECS65_TAG '56SL'

#define LECS65_DEVICE_NAME      L"\\Device\\ALADDINAcqDriver0"
#define LECS65_DOS_DEVICE_NAME  L"\\DosDevices\\ALADDINAcqDriver0"

#define LECS65_BAR_COUNT 3
#define LECS65_REGISTER_BAR_LENGTH 0x200
#define LECS65_INTERFACE_COUNT 4
#define LECS65_TRACE_CAPACITY 128
#define LECS65_TRACE_PREVIEW_BYTES 16
#define LECS65_ONEWIRE_OFFSET 0x40
#define LECS65_ONEWIRE_BUSY   0x01
#define LECS65_ONEWIRE_DATA   0x02
#define LECS65_ONEWIRE_POLL_LIMIT 100000

/* Recovered legacy IOCTLs used by the first bring-up build. */
#define LECS65_IOCTL_CFDC212C          ((ULONG)0xCFDC212C)
#define LECS65_IOCTL_CFDC2184          ((ULONG)0xCFDC2184)
#define LECS65_IOCTL_DELAY_MILLISECONDS  ((ULONG)0x00222C00)
#define LECS65_IOCTL_SET_FLAG_BYTE       ((ULONG)0x00222C04)
#define LECS65_IOCTL_GET_DALLAS_ID        ((ULONG)0x00223080)
#define LECS65_IOCTL_READ_DALLAS_MEMORY   ((ULONG)0x00223084)
#define LECS65_IOCTL_SET_THREE_EVENTS     ((ULONG)0x00223100)
#define LECS65_IOCTL_SET_EVENT_0          ((ULONG)0xCFDC2180)
#define LECS65_IOCTL_SET_EVENT_1          ((ULONG)0xCFDC218C)
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

typedef struct _LECS65_DEBUG_TRACE_ENTRY {
    ULONGLONG Sequence;
    ULONGLONG ProcessId;
    ULONGLONG Information;
    ULONG Ioctl;
    ULONG InputLength;
    ULONG OutputLength;
    ULONG Status;
    ULONG InputPreviewLength;
    UCHAR Method;
    UCHAR Wow64;
    USHORT Reserved;
    UCHAR InputPreview[LECS65_TRACE_PREVIEW_BYTES];
} LECS65_DEBUG_TRACE_ENTRY, *PLECS65_DEBUG_TRACE_ENTRY;

typedef struct _LECS65_DEBUG_TRACE {
    ULONG Version;
    ULONG Count;
    ULONGLONG TotalSeen;
    LECS65_DEBUG_TRACE_ENTRY Entry[LECS65_TRACE_CAPACITY];
} LECS65_DEBUG_TRACE, *PLECS65_DEBUG_TRACE;

typedef struct _LECS65_DEVICE_EXTENSION {
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT PhysicalDeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;

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

    PKEVENT LegacyEvent0;
    PKEVENT LegacyEvent1;
    PKEVENT LegacyEvent2;
    PKEVENT LegacyEvent3;
    PKEVENT LegacyEvent4;
    UCHAR LegacyFlagByte;
    UCHAR ReservedLegacy[3];

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
