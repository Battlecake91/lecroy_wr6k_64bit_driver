#pragma once
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

typedef ULONG KIRQL;
#define KSPIN_LOCK CRITICAL_SECTION
typedef struct _LECS65_DEVICE_EXTENSION {
    KSPIN_LOCK InterruptControlLock;
    KSPIN_LOCK IoAdmissionLock;
    BOOLEAN Started;
    BOOLEAN AcceptIoctls;
    BOOLEAN InterruptWriteAdmission;
    BOOLEAN InterruptHardwareUnavailable;
    BOOLEAN InterruptConnected;
    CRITICAL_SECTION* InterruptObject;
    volatile LONG DmaUnknownActive;
    volatile ULONG InterruptEnableShadow;
    PUCHAR Bar[1];
    ULONG BarLength[1];
} LECS65_DEVICE_EXTENSION, *PLECS65_DEVICE_EXTENSION;

#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#define KeAcquireSpinLock(lock, irql) do { *(irql) = 0; EnterCriticalSection(lock); } while (0)
#define KeReleaseSpinLock(lock, irql) do { (void)(irql); LeaveCriticalSection(lock); } while (0)
BOOLEAN KeSynchronizeExecution(CRITICAL_SECTION* Lock,
    BOOLEAN (*Callback)(PVOID), PVOID Context);
ULONG LecTestReadRegister(volatile ULONG* Register);
VOID LecTestWriteRegister(volatile ULONG* Register, ULONG Value);
#define READ_REGISTER_ULONG LecTestReadRegister
#define WRITE_REGISTER_ULONG LecTestWriteRegister

NTSTATUS LecWriteInterruptMask(PLECS65_DEVICE_EXTENSION DevExt, ULONG Value);
NTSTATUS LecUpdateInterruptMask(PLECS65_DEVICE_EXTENSION DevExt,
    ULONG ClearBits, ULONG SetBits, BOOLEAN Acquisition);
NTSTATUS LecReadInterruptMask(PLECS65_DEVICE_EXTENSION DevExt, PULONG Value);
NTSTATUS LecEnsureReceiveInterrupt(PLECS65_DEVICE_EXTENSION DevExt);
NTSTATUS LecMaskInterruptsForPnp(PLECS65_DEVICE_EXTENSION DevExt);
VOID LecSetInterruptAdmission(PLECS65_DEVICE_EXTENSION DevExt, BOOLEAN Enable);
VOID LecQuarantineInterruptAdmission(PLECS65_DEVICE_EXTENSION DevExt);
VOID LecInvalidateInterruptHardware(PLECS65_DEVICE_EXTENSION DevExt);
