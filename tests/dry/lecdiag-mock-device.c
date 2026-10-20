#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IOCTL_GET_BUILD       ((DWORD)0xCFDC21C8)
#define IOCTL_REGISTER_READ   ((DWORD)0xCFDC21C0)
#define IOCTL_START_REGISTER  ((DWORD)0x00223044)
#define IOCTL_DEBUG_STATS     ((DWORD)0x80006000)
#define IOCTL_DEBUG_BARS      ((DWORD)0x80006008)
#define IOCTL_DEBUG_TRACE     ((DWORD)0x8000600C)
#define IOCTL_DEBUG_PCI       ((DWORD)0x80006014)

typedef struct MOCK_STATS {
    uint32_t Version;
    uint32_t LegacyBuild;
    uint64_t CreateCount;
    uint64_t CloseCount;
    uint64_t IoctlCount;
    uint64_t UnknownIoctlCount;
    uint32_t LastIoctl;
    uint32_t Reserved;
} MOCK_STATS;

typedef struct MOCK_BAR_ENTRY {
    uint64_t PhysicalAddress;
    uint32_t Length;
    uint32_t Reserved;
} MOCK_BAR_ENTRY;

typedef struct MOCK_BARS {
    uint32_t Version;
    uint32_t Count;
    MOCK_BAR_ENTRY Entry[3];
    MOCK_BAR_ENTRY Bulk;
} MOCK_BARS;

typedef struct MOCK_PCI {
    uint32_t Version;
    uint32_t BytesRead;
    uint32_t BusNumber;
    uint32_t DeviceNumber;
    uint32_t FunctionNumber;
    uint16_t VendorId;
    uint16_t DeviceId;
    uint16_t Command;
    uint16_t Status;
    uint8_t RevisionId;
    uint8_t ProgIf;
    uint8_t SubClass;
    uint8_t BaseClass;
    uint8_t CacheLineSize;
    uint8_t LatencyTimer;
    uint8_t HeaderType;
    uint8_t Bist;
    uint32_t Bar[6];
    uint32_t CardbusCisPointer;
    uint16_t SubsystemVendorId;
    uint16_t SubsystemId;
    uint32_t ExpansionRomBase;
    uint8_t CapabilitiesPointer;
    uint8_t Reserved1[3];
    uint32_t Reserved2;
    uint8_t InterruptLine;
    uint8_t InterruptPin;
    uint8_t MinimumGrant;
    uint8_t MaximumLatency;
} MOCK_PCI;

C_ASSERT(sizeof(MOCK_STATS) == 48);
C_ASSERT(sizeof(MOCK_BARS) == 72);
C_ASSERT(sizeof(MOCK_PCI) == 84);

static BOOL fail_request(DWORD error)
{
    SetLastError(error);
    return FALSE;
}

HANDLE WINAPI LecdiagMockCreateFileW(
    LPCWSTR FileName,
    DWORD DesiredAccess,
    DWORD ShareMode,
    LPSECURITY_ATTRIBUTES SecurityAttributes,
    DWORD CreationDisposition,
    DWORD FlagsAndAttributes,
    HANDLE TemplateFile)
{
    UNREFERENCED_PARAMETER(DesiredAccess);
    UNREFERENCED_PARAMETER(ShareMode);
    UNREFERENCED_PARAMETER(SecurityAttributes);
    UNREFERENCED_PARAMETER(CreationDisposition);
    UNREFERENCED_PARAMETER(FlagsAndAttributes);
    UNREFERENCED_PARAMETER(TemplateFile);
    wprintf(L"MOCK_CREATE_FILE path=%ls\n", FileName);
    return CreateEventW(NULL, TRUE, FALSE, NULL);
}

BOOL WINAPI LecdiagMockDeviceIoControl(
    HANDLE Device,
    DWORD IoControlCode,
    LPVOID InputBuffer,
    DWORD InputLength,
    LPVOID OutputBuffer,
    DWORD OutputLength,
    LPDWORD BytesReturned,
    LPOVERLAPPED Overlapped)
{
    DWORD i;
    const DWORD startValue = 0x12345678UL;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Overlapped);
    if (BytesReturned == NULL) {
        return fail_request(ERROR_INVALID_PARAMETER);
    }
    *BytesReturned = 0;
    printf("MOCK_DEVICE_IO_CONTROL ioctl=0x%08lX input=%lu output=%lu input-hex=",
        (unsigned long)IoControlCode,
        (unsigned long)InputLength,
        (unsigned long)OutputLength);
    for (i = 0; i < InputLength; ++i) {
        printf("%02X", ((const BYTE*)InputBuffer)[i]);
    }
    printf("\n");

    if (OutputLength != 0 && OutputBuffer == NULL) {
        return fail_request(ERROR_INVALID_PARAMETER);
    }
    if (OutputBuffer != NULL) {
        ZeroMemory(OutputBuffer, OutputLength);
    }

    switch (IoControlCode) {
    case IOCTL_GET_BUILD:
        if (InputLength != 0 || OutputLength != sizeof(DWORD)) return fail_request(ERROR_INVALID_PARAMETER);
        *(DWORD*)OutputBuffer = 1002;
        *BytesReturned = sizeof(DWORD);
        return TRUE;
    case IOCTL_DEBUG_STATS:
        if (InputLength != 0 || OutputLength != sizeof(MOCK_STATS)) return fail_request(ERROR_INVALID_PARAMETER);
        ((MOCK_STATS*)OutputBuffer)->Version = 1;
        ((MOCK_STATS*)OutputBuffer)->LegacyBuild = 1002;
        *BytesReturned = sizeof(MOCK_STATS);
        return TRUE;
    case IOCTL_DEBUG_BARS:
        if (InputLength != 0 || OutputLength != sizeof(MOCK_BARS)) return fail_request(ERROR_INVALID_PARAMETER);
        ((MOCK_BARS*)OutputBuffer)->Version = 1;
        ((MOCK_BARS*)OutputBuffer)->Count = 3;
        ((MOCK_BARS*)OutputBuffer)->Entry[0].Length = 0x200;
        ((MOCK_BARS*)OutputBuffer)->Entry[1].Length = 0x40000;
        ((MOCK_BARS*)OutputBuffer)->Entry[2].Length = 0x200;
        ((MOCK_BARS*)OutputBuffer)->Bulk.Length = 0x40000;
        *BytesReturned = sizeof(MOCK_BARS);
        return TRUE;
    case IOCTL_DEBUG_TRACE:
        if (InputLength != 0 || OutputLength < 16) return fail_request(ERROR_INVALID_PARAMETER);
        ((uint32_t*)OutputBuffer)[0] = 3;
        ((uint32_t*)OutputBuffer)[1] = 0;
        *BytesReturned = OutputLength;
        return TRUE;
    case IOCTL_DEBUG_PCI:
        if (InputLength != 0 || OutputLength != sizeof(MOCK_PCI)) return fail_request(ERROR_INVALID_PARAMETER);
        ((MOCK_PCI*)OutputBuffer)->Version = 1;
        ((MOCK_PCI*)OutputBuffer)->BytesRead = 256;
        ((MOCK_PCI*)OutputBuffer)->BusNumber = 4;
        ((MOCK_PCI*)OutputBuffer)->DeviceNumber = 1;
        ((MOCK_PCI*)OutputBuffer)->FunctionNumber = 0;
        ((MOCK_PCI*)OutputBuffer)->VendorId = 0x1570;
        ((MOCK_PCI*)OutputBuffer)->DeviceId = 0x0005;
        *BytesReturned = sizeof(MOCK_PCI);
        return TRUE;
    case 0x00222400:
    case 0xCFDC2184:
        return (InputLength == 0 && OutputLength == 0) ? TRUE : fail_request(ERROR_INVALID_PARAMETER);
    case 0x00223004:
        if (InputLength != 0 || OutputLength != sizeof(DWORD)) return fail_request(ERROR_INVALID_PARAMETER);
        *(DWORD*)OutputBuffer = 0x1A08;
        *BytesReturned = sizeof(DWORD);
        return TRUE;
    case 0x00223040:
        if (InputLength != 0 || OutputLength != sizeof(DWORD)) return fail_request(ERROR_INVALID_PARAMETER);
        *(DWORD*)OutputBuffer = 0x2CAE;
        *BytesReturned = sizeof(DWORD);
        return TRUE;
    case 0x00222C00:
        if (InputLength != sizeof(DWORD) || OutputLength != 0) return fail_request(ERROR_INVALID_PARAMETER);
        for (i = 0; i < InputLength; ++i) {
            if (((const BYTE*)InputBuffer)[i] != 0) return fail_request(ERROR_INVALID_DATA);
        }
        return TRUE;
    case IOCTL_START_REGISTER:
        if (InputLength != 0 || OutputLength != sizeof(DWORD)) return fail_request(ERROR_INVALID_PARAMETER);
        *(DWORD*)OutputBuffer = startValue;
        *BytesReturned = sizeof(DWORD);
        return TRUE;
    case IOCTL_REGISTER_READ:
        if (InputLength != 5 || OutputLength != sizeof(DWORD)) return fail_request(ERROR_INVALID_PARAMETER);
        for (i = 0; i < InputLength; ++i) {
            if (((const BYTE*)InputBuffer)[i] != 0) return fail_request(ERROR_INVALID_DATA);
        }
        *(DWORD*)OutputBuffer = startValue;
        *BytesReturned = sizeof(DWORD);
        return TRUE;
    default:
        return fail_request(ERROR_INVALID_FUNCTION);
    }
}
