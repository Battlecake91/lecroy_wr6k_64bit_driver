#pragma once
/*
 * Host-only WDM shim. Never included in the production x64 build.
 * It runs the actual DmaScatterGatherStage.c callback/guard logic.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
#define UNREFERENCED_PARAMETER(x) (void)(x)
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
typedef CRITICAL_SECTION KSPIN_LOCK;
typedef struct _FAKE_DEVICE { volatile LONG References; } FAKE_DEVICE, *PDEVICE_OBJECT;
typedef struct _FAKE_MDL { void* Va; ULONG Size; ULONG MdlFlags; } MDL, *PMDL;
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
    NTSTATUS (*GetScatterGatherList)(
        struct _DMA_ADAPTER*, PDEVICE_OBJECT, PMDL, void*, ULONG,
        PDRIVER_LIST_CONTROL, void*, BOOLEAN);
    VOID (*PutScatterGatherList)(
        struct _DMA_ADAPTER*, PSCATTER_GATHER_LIST, BOOLEAN);
} DMA_OPERATIONS, *PDMA_OPERATIONS;
typedef struct _DMA_ADAPTER {
    PDMA_OPERATIONS DmaOperations;
} DMA_ADAPTER, *PDMA_ADAPTER;

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
#ifndef NT_SUCCESS
#define NT_SUCCESS(x) (((NTSTATUS)(x)) >= 0)
#endif

static inline void* ExAllocatePool2(ULONG flags, size_t bytes, ULONG tag) {
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(tag);
    return malloc(bytes);
}
static inline VOID RtlZeroMemory(void* p, size_t n) { memset(p, 0, n); }
static inline VOID RtlCopyMemory(void* d, const void* s, size_t n) { memcpy(d,s,n); }
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
