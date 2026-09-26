#include "LecS65Drv.h"

static
USHORT
LecPciReadU16(
    _In_reads_bytes_(2) const UCHAR* Buffer
    )
{
    USHORT value;
    RtlCopyMemory(&value, Buffer, sizeof(value));
    return value;
}

static
ULONG
LecPciReadU32(
    _In_reads_bytes_(4) const UCHAR* Buffer
    )
{
    ULONG value;
    RtlCopyMemory(&value, Buffer, sizeof(value));
    return value;
}

NTSTATUS
LecReadPciConfig(
    _Inout_ PLECS65_DEVICE_EXTENSION DevExt,
    _Out_ PLECS65_DEBUG_PCI_CONFIG Config
    )
{
    ULONG busNumber = 0;
    ULONG address = 0;
    ULONG returned = 0;
    ULONG deviceNumber;
    ULONG functionNumber;
    PCI_SLOT_NUMBER slot;
    UCHAR raw[64];
    ULONG bytesRead;
    NTSTATUS status;

    if (DevExt == NULL ||
        DevExt->PhysicalDeviceObject == NULL ||
        Config == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Config, sizeof(*Config));
    RtlZeroMemory(raw, sizeof(raw));

    status = IoGetDeviceProperty(
        DevExt->PhysicalDeviceObject,
        DevicePropertyBusNumber,
        sizeof(busNumber),
        &busNumber,
        &returned);
    if (!NT_SUCCESS(status)) {
        LecTrace(
            "PCI config: DevicePropertyBusNumber failed 0x%08X\n",
            status);
        return status;
    }

    status = IoGetDeviceProperty(
        DevExt->PhysicalDeviceObject,
        DevicePropertyAddress,
        sizeof(address),
        &address,
        &returned);
    if (!NT_SUCCESS(status)) {
        LecTrace(
            "PCI config: DevicePropertyAddress failed 0x%08X\n",
            status);
        return status;
    }

    /*
     * For PCI PDOs DevicePropertyAddress is encoded as device << 16 |
     * function. This is only used to construct a passive config-space read.
     */
    deviceNumber = (address >> 16) & 0xFFFFUL;
    functionNumber = address & 0xFFFFUL;

    if (deviceNumber > 31 || functionNumber > 7) {
        LecTrace(
            "PCI config: invalid BDF from address property: bus=%lu address=0x%08lX\n",
            busNumber,
            address);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    slot.u.AsULONG = 0;
    slot.u.bits.DeviceNumber = deviceNumber;
    slot.u.bits.FunctionNumber = functionNumber;

    bytesRead = HalGetBusDataByOffset(
        PCIConfiguration,
        busNumber,
        slot.u.AsULONG,
        raw,
        0,
        sizeof(raw));

    Config->Version = 1;
    Config->BytesRead = bytesRead;
    Config->BusNumber = busNumber;
    Config->DeviceNumber = deviceNumber;
    Config->FunctionNumber = functionNumber;

    if (bytesRead < sizeof(raw)) {
        LecTrace(
            "PCI config: short read bus=%lu dev=%lu fn=%lu bytes=%lu\n",
            busNumber,
            deviceNumber,
            functionNumber,
            bytesRead);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Config->VendorId = LecPciReadU16(raw + 0x00);
    Config->DeviceId = LecPciReadU16(raw + 0x02);
    Config->Command = LecPciReadU16(raw + 0x04);
    Config->Status = LecPciReadU16(raw + 0x06);
    Config->RevisionId = raw[0x08];
    Config->ProgIf = raw[0x09];
    Config->SubClass = raw[0x0A];
    Config->BaseClass = raw[0x0B];
    Config->CacheLineSize = raw[0x0C];
    Config->LatencyTimer = raw[0x0D];
    Config->HeaderType = raw[0x0E];
    Config->Bist = raw[0x0F];

    Config->Bar[0] = LecPciReadU32(raw + 0x10);
    Config->Bar[1] = LecPciReadU32(raw + 0x14);
    Config->Bar[2] = LecPciReadU32(raw + 0x18);
    Config->Bar[3] = LecPciReadU32(raw + 0x1C);
    Config->Bar[4] = LecPciReadU32(raw + 0x20);
    Config->Bar[5] = LecPciReadU32(raw + 0x24);
    Config->CardbusCisPointer = LecPciReadU32(raw + 0x28);
    Config->SubsystemVendorId = LecPciReadU16(raw + 0x2C);
    Config->SubsystemId = LecPciReadU16(raw + 0x2E);
    Config->ExpansionRomBase = LecPciReadU32(raw + 0x30);
    Config->CapabilitiesPointer = raw[0x34];
    Config->Reserved2 = LecPciReadU32(raw + 0x38);
    Config->InterruptLine = raw[0x3C];
    Config->InterruptPin = raw[0x3D];
    Config->MinimumGrant = raw[0x3E];
    Config->MaximumLatency = raw[0x3F];

    LecTrace(
        "PCI config: %04X:%04X bus=%lu dev=%lu fn=%lu command=0x%04X status=0x%04X BAR0=%08lX BAR1=%08lX BAR2=%08lX IRQ=%u pin=%u\n",
        Config->VendorId,
        Config->DeviceId,
        Config->BusNumber,
        Config->DeviceNumber,
        Config->FunctionNumber,
        Config->Command,
        Config->Status,
        Config->Bar[0],
        Config->Bar[1],
        Config->Bar[2],
        (ULONG)Config->InterruptLine,
        (ULONG)Config->InterruptPin);

    return STATUS_SUCCESS;
}
