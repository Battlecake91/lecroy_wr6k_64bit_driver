#pragma once
/*
 * Host-only WDM shim. Never included in the production x64 build.
 * It runs the actual DmaScatterGatherStage.c callback/guard logic.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Outptr_
#define _Outptr_
#endif
#ifndef _Out_writes_to_
#define _Out_writes_to_(x,y)
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif
#ifndef POOL_FLAG_NON_PAGED
#define POOL_FLAG_NON_PAGED 0
#endif
#ifndef LECS65_TAG
#define LECS65_TAG 0x4C534447
#endif
#ifndef MDL_PAGES_LOCKED
#define MDL_PAGES_LOCKED 1U
#endif
#ifndef DISPATCH_LEVEL
#define DISPATCH_LEVEL 2
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef unsigned long KIRQL;
/* The Windows user header already typedefs KSPIN_LOCK. Substitute a
 * real host-side mutex only within this fake-WDM translation unit. */
#define KSPIN_LOCK CRITICAL_SECTION
typedef struct _FAKE_DEVICE { volatile LONG References; } FAKE_DEVICE, *PDEVICE_OBJECT;
typedef struct _FAKE_MDL { void* Va; ULONG Size; ULONG MdlFlags; struct _FAKE_MDL* Next; } MDL, *PMDL;
typedef void* PIRP;
typedef struct _SCATTER_GATHER_ELEMENT {
    LARGE_INTEGER Address;
    ULONG Length;
    ULONG Reserved;
} SCATTER_GATHER_ELEMENT, *PSCATTER_GATHER_ELEMENT;
typedef struct _SCATTER_GATHER_LIST {
    ULONG NumberOfElements;
    ULONG Reserved;
    SCATTER_GATHER_ELEMENT Elements[3];
} SCATTER_GATHER_LIST, *PSCATTER_GATHER_LIST;
typedef void (*PDRIVER_LIST_CONTROL)(
    PDEVICE_OBJECT, PIRP, PSCATTER_GATHER_LIST, void*);
struct _DMA_ADAPTER;
typedef struct _DMA_OPERATIONS {
    ULONG Size;
    VOID (*PutDmaAdapter)(struct _DMA_ADAPTER*);
    PVOID (*AllocateCommonBuffer)(
        struct _DMA_ADAPTER*, ULONG, LARGE_INTEGER*, BOOLEAN);
    VOID (*FreeCommonBuffer)(
        struct _DMA_ADAPTER*, ULONG, LARGE_INTEGER, PVOID, BOOLEAN);
    NTSTATUS (*GetScatterGatherList)(
        struct _DMA_ADAPTER*, PDEVICE_OBJECT, PMDL, void*, ULONG,
        PDRIVER_LIST_CONTROL, void*, BOOLEAN);
    VOID (*PutScatterGatherList)(
        struct _DMA_ADAPTER*, PSCATTER_GATHER_LIST, BOOLEAN);
    NTSTATUS (*InitializeDmaTransferContext)(
        struct _DMA_ADAPTER*, void*);
    NTSTATUS (*GetScatterGatherListEx)(
        struct _DMA_ADAPTER*, PDEVICE_OBJECT, void*, PMDL,
        ULONGLONG, ULONG, ULONG, PDRIVER_LIST_CONTROL, void*,
        BOOLEAN, void*, void*, PSCATTER_GATHER_LIST*);
    VOID (*FreeAdapterObject)(
        struct _DMA_ADAPTER*, int);
} DMA_OPERATIONS, *PDMA_OPERATIONS;
typedef struct _DMA_ADAPTER {
    USHORT Version;
    USHORT Size;
    PDMA_OPERATIONS DmaOperations;
} DMA_ADAPTER, *PDMA_ADAPTER;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;
typedef enum _INTERFACE_TYPE { InterfaceTypeUndefined = -1, PCIBus = 5 } INTERFACE_TYPE;
typedef struct _DEVICE_DESCRIPTION {
    ULONG Version;
    BOOLEAN Master;
    BOOLEAN ScatterGather;
    BOOLEAN DemandMode;
    BOOLEAN AutoInitialize;
    BOOLEAN Dma32BitAddresses;
    BOOLEAN IgnoreCount;
    BOOLEAN Reserved1;
    BOOLEAN Dma64BitAddresses;
    ULONG BusNumber;
    ULONG DmaChannel;
    INTERFACE_TYPE InterfaceType;
    ULONG DmaWidth;
    ULONG DmaSpeed;
    ULONG MaximumLength;
    ULONG DmaPort;
    ULONG DmaAddressWidth;
    ULONG DmaControllerInstance;
    ULONG DmaRequestLine;
    PHYSICAL_ADDRESS DeviceAddress;
} DEVICE_DESCRIPTION, *PDEVICE_DESCRIPTION;

PDMA_ADAPTER FakeIoGetDmaAdapter(
    PDEVICE_OBJECT DeviceObject,
    PDEVICE_DESCRIPTION Description,
    PULONG NumberOfMapRegisters);
#define IoGetDmaAdapter FakeIoGetDmaAdapter

#ifndef DMA_TRANSFER_CONTEXT_SIZE_V1
#define DMA_TRANSFER_CONTEXT_SIZE_V1 128U
#endif
#ifndef DEVICE_DESCRIPTION_VERSION3
#define DEVICE_DESCRIPTION_VERSION3 3U
#endif
#ifndef DMA_SYNCHRONOUS_CALLBACK
#define DMA_SYNCHRONOUS_CALLBACK 1U
#endif
#ifndef DeallocateObject
#define DeallocateObject 2
#endif
#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBL)
#endif
#ifndef STATUS_INTERNAL_ERROR
#define STATUS_INTERNAL_ERROR ((NTSTATUS)0xC00000E5L)
#endif
#ifndef MAXULONG
#define MAXULONG 0xFFFFFFFFUL
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096U
#endif
#ifndef RTL_SIZEOF_THROUGH_FIELD
#define RTL_SIZEOF_THROUGH_FIELD(type, field) \
    (offsetof(type, field) + sizeof(((type*)0)->field))
#endif
#ifndef ADDRESS_AND_SIZE_TO_SPAN_PAGES
#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(va, size) \
    ((ULONG)(((((ULONG_PTR)(va)) & (PAGE_SIZE - 1U)) + \
        (ULONG_PTR)(size) + PAGE_SIZE - 1U) / PAGE_SIZE))
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0L)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif
#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009AL)
#endif
#ifndef STATUS_DEVICE_BUSY
#define STATUS_DEVICE_BUSY ((NTSTATUS)0xC000009EL)
#endif
#ifndef STATUS_DEVICE_NOT_READY
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xC00000A3L)
#endif
#ifndef STATUS_INVALID_BUFFER_SIZE
#define STATUS_INVALID_BUFFER_SIZE ((NTSTATUS)0xC0000206L)
#endif
#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif
#ifndef STATUS_DELETE_PENDING
#define STATUS_DELETE_PENDING ((NTSTATUS)0xC0000056L)
#endif
#ifndef STATUS_INVALID_DEVICE_STATE
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xC0000184L)
#endif
#ifndef NT_SUCCESS
#define NT_SUCCESS(x) (((NTSTATUS)(x)) >= 0)
#endif

#if defined(LECS65_SG_TEST_ALLOCATION_HOOKS)
void* FakeExAllocatePool2(ULONG flags, size_t bytes, ULONG tag);
VOID FakeExFreePoolWithTag(void* p, ULONG tag);
#define ExAllocatePool2 FakeExAllocatePool2
#define ExFreePoolWithTag FakeExFreePoolWithTag
#else
static inline void* ExAllocatePool2(ULONG flags, size_t bytes, ULONG tag) {
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(tag);
    return malloc(bytes);
}
static inline VOID ExFreePoolWithTag(void* p, ULONG tag) {
    UNREFERENCED_PARAMETER(tag);
    free(p);
}
#endif
#ifndef RtlZeroMemory
#define RtlZeroMemory(p,n) memset((p),0,(n))
#endif
#ifndef RtlCopyMemory
#define RtlCopyMemory(d,s,n) memcpy((d),(s),(n))
#endif
static inline VOID KeInitializeSpinLock(KSPIN_LOCK* p) { InitializeCriticalSection(p); }
static inline VOID KeAcquireSpinLock(KSPIN_LOCK* p, KIRQL* irql) {
    *irql = 0; EnterCriticalSection(p);
}
static inline VOID KeReleaseSpinLock(KSPIN_LOCK* p, KIRQL irql) {
    UNREFERENCED_PARAMETER(irql); LeaveCriticalSection(p);
}
static inline VOID KeRaiseIrql(KIRQL target, KIRQL* prev) {
    UNREFERENCED_PARAMETER(target); *prev = 0;
}
static inline VOID KeLowerIrql(KIRQL prev) { UNREFERENCED_PARAMETER(prev); }
static inline VOID ObReferenceObject(PDEVICE_OBJECT d) { InterlockedIncrement(&d->References); }
static inline ULONG MmGetMdlByteCount(PMDL m) { return m->Size; }
static inline void* MmGetMdlVirtualAddress(PMDL m) { return m->Va; }

static inline VOID ObDereferenceObject(PDEVICE_OBJECT d) {
    InterlockedDecrement(&d->References);
}
