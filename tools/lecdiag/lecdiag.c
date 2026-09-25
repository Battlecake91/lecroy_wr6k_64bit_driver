#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LECS65_IOCTL_GET_DRIVER_BUILD ((DWORD)0xCFDC21C8)
#define LECS65_IOCTL_REGISTER_READ    ((DWORD)0xCFDC21C0)

#define LECS65_IOCTL_DEBUG_GET_STATS \
    CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define LECS65_IOCTL_DEBUG_GET_BARS \
    CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)

#pragma pack(push, 1)
typedef struct LECS65_REG_READ_EXT {
    uint8_t  bar;
    uint32_t offset;
} LECS65_REG_READ_EXT;
#pragma pack(pop)

typedef struct LECS65_DEBUG_STATS {
    uint32_t Version;
    uint32_t LegacyBuild;
    uint64_t CreateCount;
    uint64_t CloseCount;
    uint64_t IoctlCount;
    uint64_t UnknownIoctlCount;
    uint32_t LastIoctl;
    uint32_t Reserved;
} LECS65_DEBUG_STATS;

typedef struct LECS65_DEBUG_BAR_ENTRY {
    uint64_t PhysicalAddress;
    uint32_t Length;
    uint32_t Reserved;
} LECS65_DEBUG_BAR_ENTRY;

typedef struct LECS65_DEBUG_BARS {
    uint32_t Version;
    uint32_t Count;
    LECS65_DEBUG_BAR_ENTRY Entry[3];
    LECS65_DEBUG_BAR_ENTRY Bulk;
} LECS65_DEBUG_BARS;

static void print_error(const char* what)
{
    DWORD e = GetLastError();
    char* message = NULL;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        e,
        0,
        (LPSTR)&message,
        0,
        NULL);

    fprintf(stderr, "%s failed: %lu (%s)\n",
        what,
        (unsigned long)e,
        message != NULL ? message : "unknown error");

    if (message != NULL) {
        LocalFree(message);
    }
}

static HANDLE open_driver(void)
{
    HANDLE h = CreateFileW(
        L"\\\\.\\ALADDINAcqDriver0",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (h == INVALID_HANDLE_VALUE) {
        print_error("CreateFile(\\\\.\\ALADDINAcqDriver0)");
    }

    return h;
}

static int query_build(HANDLE h)
{
    DWORD build = 0;
    DWORD returned = 0;

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_GET_DRIVER_BUILD,
            NULL,
            0,
            &build,
            sizeof(build),
            &returned,
            NULL)) {
        print_error("GET_DRIVER_BUILD");
        return 1;
    }

    printf("driver build: %lu (returned=%lu)\n",
        (unsigned long)build,
        (unsigned long)returned);
    return 0;
}

static int query_stats(HANDLE h)
{
    LECS65_DEBUG_STATS s;
    DWORD returned = 0;

    ZeroMemory(&s, sizeof(s));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_GET_STATS,
            NULL,
            0,
            &s,
            sizeof(s),
            &returned,
            NULL)) {
        print_error("DEBUG_GET_STATS");
        return 1;
    }

    printf("stats version:    %lu\n", (unsigned long)s.Version);
    printf("legacy build:     %lu\n", (unsigned long)s.LegacyBuild);
    printf("create count:     %llu\n", (unsigned long long)s.CreateCount);
    printf("close count:      %llu\n", (unsigned long long)s.CloseCount);
    printf("ioctl count:      %llu\n", (unsigned long long)s.IoctlCount);
    printf("unknown ioctls:   %llu\n", (unsigned long long)s.UnknownIoctlCount);
    printf("last ioctl:       0x%08lX\n", (unsigned long)s.LastIoctl);

    return 0;
}

static int query_bars(HANDLE h)
{
    LECS65_DEBUG_BARS bars;
    DWORD returned = 0;
    unsigned i;

    ZeroMemory(&bars, sizeof(bars));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_GET_BARS,
            NULL,
            0,
            &bars,
            sizeof(bars),
            &returned,
            NULL)) {
        print_error("DEBUG_GET_BARS");
        return 1;
    }

    printf("bars version: %lu, count: %lu, returned=%lu\n",
        (unsigned long)bars.Version,
        (unsigned long)bars.Count,
        (unsigned long)returned);

    for (i = 0; i < bars.Count && i < 3; ++i) {
        printf("logical BAR%u: PA=0x%016llX length=0x%08lX (%lu bytes)%s\n",
            i,
            (unsigned long long)bars.Entry[i].PhysicalAddress,
            (unsigned long)bars.Entry[i].Length,
            (unsigned long)bars.Entry[i].Length,
            bars.Entry[i].Length == 0 ? " [unmapped]" : "");
    }

    printf("bulk MMIO:    PA=0x%016llX length=0x%08lX (%lu bytes)\n",
        (unsigned long long)bars.Bulk.PhysicalAddress,
        (unsigned long)bars.Bulk.Length,
        (unsigned long)bars.Bulk.Length);

    return 0;
}

static int read_register(HANDLE h, unsigned bar, unsigned long offset)
{
    LECS65_REG_READ_EXT req;
    DWORD value = 0;
    DWORD returned = 0;

    if (bar > 2) {
        fprintf(stderr, "BAR must be 0, 1 or 2\n");
        return 1;
    }

    req.bar = (uint8_t)bar;
    req.offset = (uint32_t)offset;

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_REGISTER_READ,
            &req,
            sizeof(req),
            &value,
            sizeof(value),
            &returned,
            NULL)) {
        print_error("REGISTER_READ");
        return 1;
    }

    printf("BAR%u + 0x%08lX = 0x%08lX\n",
        bar,
        offset,
        (unsigned long)value);
    return 0;
}

static void usage(const char* exe)
{
    printf("Usage:\n");
    printf("  %s build\n", exe);
    printf("  %s stats\n", exe);
    printf("  %s bars\n", exe);
    printf("  %s read <bar 0..2> <offset>\n", exe);
    printf("\nExamples:\n");
    printf("  %s build\n", exe);
    printf("  %s read 0 0x0\n", exe);
    printf("  %s read 1 0xA0\n", exe);
}

int main(int argc, char** argv)
{
    HANDLE h;
    int result = 1;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    h = open_driver();
    if (h == INVALID_HANDLE_VALUE) {
        return 1;
    }

    if (_stricmp(argv[1], "build") == 0) {
        result = query_build(h);
    }
    else if (_stricmp(argv[1], "stats") == 0) {
        result = query_stats(h);
    }
    else if (_stricmp(argv[1], "bars") == 0) {
        result = query_bars(h);
    }
    else if (_stricmp(argv[1], "read") == 0 && argc == 4) {
        char* end1 = NULL;
        char* end2 = NULL;
        unsigned long bar = strtoul(argv[2], &end1, 0);
        unsigned long offset = strtoul(argv[3], &end2, 0);

        if (end1 == argv[2] || *end1 != '\0' ||
            end2 == argv[3] || *end2 != '\0') {
            fprintf(stderr, "invalid numeric argument\n");
            result = 2;
        }
        else {
            result = read_register(h, (unsigned)bar, offset);
        }
    }
    else {
        usage(argv[0]);
        result = 2;
    }

    CloseHandle(h);
    return result;
}
