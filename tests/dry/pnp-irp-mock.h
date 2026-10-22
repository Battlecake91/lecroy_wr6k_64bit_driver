#pragma once

#include <windows.h>
#include <stddef.h>

#ifndef _In_
#define _In_
#define _Inout_
#define _In_opt_
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif
#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0L)
#endif
#ifndef STATUS_PENDING
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif
#ifndef STATUS_MORE_PROCESSING_REQUIRED
#define STATUS_MORE_PROCESSING_REQUIRED ((NTSTATUS)0xC0000016L)
#endif
#ifndef STATUS_CONTINUE_COMPLETION
#define STATUS_CONTINUE_COMPLETION STATUS_SUCCESS
#endif
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#ifndef NotificationEvent
#define NotificationEvent 0
#define Executive 0
#define KernelMode 0
#define IO_NO_INCREMENT 0
#endif

typedef PVOID PDEVICE_OBJECT;
typedef struct _FAKE_KEVENT {
    volatile LONG Signaled;
} KEVENT, *PKEVENT;
typedef struct _FAKE_REMOVE_LOCK {
    volatile LONG Outstanding;
    volatile LONG ReleaseCalls;
} IO_REMOVE_LOCK, *PIO_REMOVE_LOCK;

struct _FAKE_IRP;
typedef NTSTATUS (*PIO_COMPLETION_ROUTINE)(
    PDEVICE_OBJECT DeviceObject,
    struct _FAKE_IRP* Irp,
    PVOID Context);
typedef struct _FAKE_IRP {
    struct {
        NTSTATUS Status;
        ULONG_PTR Information;
    } IoStatus;
    BOOLEAN PendingReturned;
    BOOLEAN MarkedPending;
    ULONG StackCopies;
    PIO_COMPLETION_ROUTINE CompletionRoutine;
    PVOID CompletionContext;
} IRP, *PIRP;
typedef struct _LECS65_DEVICE_EXTENSION {
    PDEVICE_OBJECT LowerDeviceObject;
    IO_REMOVE_LOCK RemoveLock;
} LECS65_DEVICE_EXTENSION, *PLECS65_DEVICE_EXTENSION;

NTSTATUS IoCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS PoCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static __inline VOID KeInitializeEvent(
    PKEVENT Event,
    int Type,
    BOOLEAN State)
{
    UNREFERENCED_PARAMETER(Type);
    Event->Signaled = State ? 1 : 0;
}

static __inline LONG KeSetEvent(
    PKEVENT Event,
    LONG Increment,
    BOOLEAN Wait)
{
    UNREFERENCED_PARAMETER(Increment);
    UNREFERENCED_PARAMETER(Wait);
    return InterlockedExchange(&Event->Signaled, 1);
}

static __inline NTSTATUS KeWaitForSingleObject(
    PVOID Object,
    int Reason,
    int Mode,
    BOOLEAN Alertable,
    PLARGE_INTEGER Timeout)
{
    PKEVENT event = (PKEVENT)Object;

    UNREFERENCED_PARAMETER(Reason);
    UNREFERENCED_PARAMETER(Mode);
    UNREFERENCED_PARAMETER(Alertable);
    UNREFERENCED_PARAMETER(Timeout);
    while (InterlockedCompareExchange(&event->Signaled, 0, 0) == 0) {
        Sleep(1);
    }
    return STATUS_SUCCESS;
}

static __inline VOID IoCopyCurrentIrpStackLocationToNext(PIRP Irp)
{
    ++Irp->StackCopies;
}

static __inline VOID IoSetCompletionRoutine(
    PIRP Irp,
    PIO_COMPLETION_ROUTINE CompletionRoutine,
    PVOID Context,
    BOOLEAN InvokeOnSuccess,
    BOOLEAN InvokeOnError,
    BOOLEAN InvokeOnCancel)
{
    UNREFERENCED_PARAMETER(InvokeOnSuccess);
    UNREFERENCED_PARAMETER(InvokeOnError);
    UNREFERENCED_PARAMETER(InvokeOnCancel);
    Irp->CompletionRoutine = CompletionRoutine;
    Irp->CompletionContext = Context;
}

static __inline VOID IoMarkIrpPending(PIRP Irp)
{
    Irp->MarkedPending = TRUE;
}

static __inline VOID IoReleaseRemoveLock(
    PIO_REMOVE_LOCK RemoveLock,
    PVOID Tag)
{
    UNREFERENCED_PARAMETER(Tag);
    InterlockedIncrement(&RemoveLock->ReleaseCalls);
    InterlockedDecrement(&RemoveLock->Outstanding);
}

NTSTATUS LecForwardAndWait(
    PLECS65_DEVICE_EXTENSION DevExt,
    PIRP Irp);
NTSTATUS LecForwardLockedIrp(
    PLECS65_DEVICE_EXTENSION DevExt,
    PIRP Irp,
    BOOLEAN IsPowerIrp);
