#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LECS65_IOCTL_GET_DRIVER_BUILD  ((DWORD)0xCFDC21C8)
#define LECS65_IOCTL_REGISTER_READ     ((DWORD)0xCFDC21C0)
#define LECS65_IOCTL_GET_DALLAS_ID     ((DWORD)0x00223080)
#define LECS65_IOCTL_READ_DALLAS_MEMORY ((DWORD)0x00223084)

#define LECS65_IOCTL_DEBUG_GET_STATS \
    ((DWORD)CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS))
#define LECS65_IOCTL_DEBUG_GET_BARS \
    ((DWORD)CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS))
#define LECS65_IOCTL_DEBUG_GET_TRACE \
    ((DWORD)CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS))
#define LECS65_IOCTL_DEBUG_CLEAR_TRACE \
    ((DWORD)CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS))
#define LECS65_IOCTL_DEBUG_GET_PCI_CONFIG \
    ((DWORD)CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_READ_ACCESS))

#define LECS65_TRACE_CAPACITY 256
#define LECS65_TRACE_PREVIEW_BYTES 256
#define LECS65_TRACE_OUTPUT_PREVIEW_BYTES 128

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


typedef struct LECS65_DEBUG_PCI_CONFIG {
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
} LECS65_DEBUG_PCI_CONFIG;

typedef struct LECS65_DEBUG_TRACE_ENTRY {
    uint64_t Sequence;
    uint64_t Timestamp100ns;
    uint64_t ProcessId;
    uint64_t Information;
    uint64_t Type3InputBuffer;
    uint64_t UserBuffer;
    uint32_t Ioctl;
    uint32_t InputLength;
    uint32_t OutputLength;
    uint32_t Status;
    uint32_t InputPreviewLength;
    uint32_t OutputPreviewLength;
    uint8_t Method;
    uint8_t Wow64;
    uint16_t Reserved;
    uint8_t InputPreview[LECS65_TRACE_PREVIEW_BYTES];
    uint8_t OutputPreview[LECS65_TRACE_OUTPUT_PREVIEW_BYTES];
} LECS65_DEBUG_TRACE_ENTRY;

typedef struct LECS65_DEBUG_TRACE {
    uint32_t Version;
    uint32_t Count;
    uint64_t TotalSeen;
    LECS65_DEBUG_TRACE_ENTRY Entry[LECS65_TRACE_CAPACITY];
} LECS65_DEBUG_TRACE;

static const char* ioctl_name(DWORD code);
static const char* method_name(unsigned method);
static void print_error(const char* what);

static int get_trace_snapshot(HANDLE h, LECS65_DEBUG_TRACE* trace)
{
    DWORD returned = 0;

    ZeroMemory(trace, sizeof(*trace));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_GET_TRACE,
            NULL,
            0,
            trace,
            (DWORD)sizeof(*trace),
            &returned,
            NULL)) {
        print_error("DEBUG_GET_TRACE");
        return 1;
    }

    return 0;
}

static void write_hex_json(FILE* out, const uint8_t* data, uint32_t length)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32_t i;

    fputc('"', out);
    for (i = 0; i < length; ++i) {
        fputc(hex[(data[i] >> 4) & 0x0F], out);
        fputc(hex[data[i] & 0x0F], out);
    }
    fputc('"', out);
}

static void write_trace_entry_jsonl(
    FILE* out,
    const LECS65_DEBUG_TRACE_ENTRY* e)
{
    fprintf(
        out,
        "{\"type\":\"ioctl\","
        "\"seq\":%llu,"
        "\"timestamp_100ns\":%llu,"
        "\"pid\":%llu,"
        "\"wow64\":%u,"
        "\"method\":%u,"
        "\"method_name\":\"%s\","
        "\"ioctl\":\"0x%08lX\","
        "\"name\":\"%s\","
        "\"input_length\":%lu,"
        "\"output_length\":%lu,"
        "\"information\":%llu,"
        "\"status\":\"0x%08lX\","
        "\"type3_input_buffer\":\"0x%016llX\","
        "\"user_buffer\":\"0x%016llX\","
        "\"input_hex\":",
        (unsigned long long)e->Sequence,
        (unsigned long long)e->Timestamp100ns,
        (unsigned long long)e->ProcessId,
        (unsigned)e->Wow64,
        (unsigned)e->Method,
        method_name(e->Method),
        (unsigned long)e->Ioctl,
        ioctl_name(e->Ioctl),
        (unsigned long)e->InputLength,
        (unsigned long)e->OutputLength,
        (unsigned long long)e->Information,
        (unsigned long)e->Status,
        (unsigned long long)e->Type3InputBuffer,
        (unsigned long long)e->UserBuffer);

    write_hex_json(
        out,
        e->InputPreview,
        e->InputPreviewLength < LECS65_TRACE_PREVIEW_BYTES
            ? e->InputPreviewLength
            : LECS65_TRACE_PREVIEW_BYTES);

    fprintf(out, ",\"output_hex\":");
    write_hex_json(
        out,
        e->OutputPreview,
        e->OutputPreviewLength < LECS65_TRACE_OUTPUT_PREVIEW_BYTES
            ? e->OutputPreviewLength
            : LECS65_TRACE_OUTPUT_PREVIEW_BYTES);

    fprintf(out, "}\n");
}

static int trace_save(HANDLE h, const char* path)
{
    LECS65_DEBUG_TRACE* trace;
    FILE* out;
    uint32_t i;

    trace = (LECS65_DEBUG_TRACE*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*trace));
    if (trace == NULL) {
        fprintf(stderr, "HeapAlloc failed\n");
        return 1;
    }

    if (get_trace_snapshot(h, trace) != 0) {
        HeapFree(GetProcessHeap(), 0, trace);
        return 1;
    }

    out = fopen(path, "wb");
    if (out == NULL) {
        perror("fopen");
        HeapFree(GetProcessHeap(), 0, trace);
        return 1;
    }

    fprintf(
        out,
        "{\"type\":\"lecdiag_trace\","
        "\"format_version\":2,"
        "\"entries\":%lu,"
        "\"total_seen\":%llu}\n",
        (unsigned long)trace->Count,
        (unsigned long long)trace->TotalSeen);

    for (i = 0; i < trace->Count && i < LECS65_TRACE_CAPACITY; ++i) {
        write_trace_entry_jsonl(out, &trace->Entry[i]);
    }

    fclose(out);
    printf(
        "saved %lu trace entries to %s\n",
        (unsigned long)trace->Count,
        path);

    HeapFree(GetProcessHeap(), 0, trace);
    return 0;
}

static int trace_capture(HANDLE h, const char* path, unsigned seconds)
{
    LECS65_DEBUG_TRACE* trace;
    FILE* out;
    uint64_t lastSequence = 0;
    ULONGLONG deadline;
    DWORD returned = 0;
    unsigned long written = 0;

    trace = (LECS65_DEBUG_TRACE*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*trace));
    if (trace == NULL) {
        fprintf(stderr, "HeapAlloc failed\n");
        return 1;
    }

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_CLEAR_TRACE,
            NULL,
            0,
            NULL,
            0,
            &returned,
            NULL)) {
        print_error("DEBUG_CLEAR_TRACE");
        HeapFree(GetProcessHeap(), 0, trace);
        return 1;
    }

    out = fopen(path, "wb");
    if (out == NULL) {
        perror("fopen");
        HeapFree(GetProcessHeap(), 0, trace);
        return 1;
    }

    fprintf(
        out,
        "{\"type\":\"lecdiag_trace\","
        "\"format_version\":2,"
        "\"mode\":\"capture\","
        "\"duration_seconds\":%u}\n",
        seconds);
    fflush(out);

    printf(
        "capturing IOCTL trace for %u seconds -> %s\n"
        "Start XStream now. Press Ctrl+C to stop the process early.\n",
        seconds,
        path);

    deadline = GetTickCount64() + ((ULONGLONG)seconds * 1000ULL);

    while (GetTickCount64() < deadline) {
        uint32_t i;

        if (get_trace_snapshot(h, trace) != 0) {
            fclose(out);
            HeapFree(GetProcessHeap(), 0, trace);
            return 1;
        }

        for (i = 0; i < trace->Count && i < LECS65_TRACE_CAPACITY; ++i) {
            const LECS65_DEBUG_TRACE_ENTRY* e = &trace->Entry[i];

            if (e->Sequence <= lastSequence) {
                continue;
            }

            write_trace_entry_jsonl(out, e);
            lastSequence = e->Sequence;
            ++written;
        }

        fflush(out);
        Sleep(250);
    }

    fclose(out);
    HeapFree(GetProcessHeap(), 0, trace);

    printf(
        "capture complete: %lu entries written to %s\n",
        written,
        path);
    return 0;
}

static const char* ioctl_name(DWORD code)
{
    switch (code) {
    case 0x00222C00: return "DELAY_MS";
    case 0x00222C04: return "SET_FLAG_BYTE";
    case 0x00223004: return "QUERY_BUFFER_A";
    case 0x00223040: return "QUERY_BUFFER_B";
    case 0x00223100: return "SET_THREE_EVENTS";
    case 0x00223080: return "GET_DALLAS_ID";
    case 0x00223084: return "READ_DALLAS_MEMORY";
    case 0x00223088: return "WRITE_DALLAS_MEMORY";
    case 0x00223044: return "DIRECT_REGISTER_READ";
    case 0xCFDC2110: return "CFDC2110";
    case 0xCFDC2114: return "CFDC2114";
    case 0xCFDC2124: return "CFDC2124";
    case 0xCFDC2128: return "CFDC2128";
    case 0xCFDC212C: return "CFDC212C_NOT_IMPLEMENTED";
    case 0xCFDC2130: return "PROG_SERTRIG_FPGA";
    case 0xCFDC2138: return "CFDC2138";
    case 0xCFDC2180: return "CFDC2180";
    case 0xCFDC2184: return "CFDC2184_NOOP";
    case 0xCFDC218C: return "CFDC218C";
    case 0xCFDC2190: return "CFDC2190";
    case 0xCFDC2194: return "CFDC2194";
    case 0xCFDC21C0: return "REGISTER_READ";
    case 0xCFDC21C4: return "REGISTER_WRITE";
    case 0xCFDC21C8: return "GET_DRIVER_BUILD";
    case 0xCFDC21CC: return "CFDC21CC";
    case 0xCFDC2400: return "CFDC2400";
    case 0xCFDD219F: return "CFDD219F_METHOD_NEITHER";
    default: return "";
    }
}

static const char* method_name(unsigned method)
{
    switch (method & 3U) {
    case METHOD_BUFFERED: return "BUF";
    case METHOD_IN_DIRECT: return "IN";
    case METHOD_OUT_DIRECT: return "OUT";
    case METHOD_NEITHER: return "NEITHER";
    default: return "?";
    }
}

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


static int query_pci(HANDLE h)
{
    LECS65_DEBUG_PCI_CONFIG p;
    DWORD returned = 0;
    unsigned i;

    ZeroMemory(&p, sizeof(p));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_GET_PCI_CONFIG,
            NULL,
            0,
            &p,
            sizeof(p),
            &returned,
            NULL)) {
        print_error("DEBUG_GET_PCI_CONFIG");
        return 1;
    }

    printf("PCI config version: %lu, bytes read: %lu, returned=%lu\n",
        (unsigned long)p.Version,
        (unsigned long)p.BytesRead,
        (unsigned long)returned);
    printf("BDF: %lu:%lu.%lu\n",
        (unsigned long)p.BusNumber,
        (unsigned long)p.DeviceNumber,
        (unsigned long)p.FunctionNumber);
    printf("vendor/device: %04X:%04X  subsystem: %04X:%04X  rev=%02X\n",
        p.VendorId,
        p.DeviceId,
        p.SubsystemVendorId,
        p.SubsystemId,
        p.RevisionId);
    printf("command=0x%04X status=0x%04X\n",
        p.Command,
        p.Status);
    printf("  IO space:      %s\n", (p.Command & 0x0001) ? "enabled" : "disabled");
    printf("  Memory space:  %s\n", (p.Command & 0x0002) ? "enabled" : "disabled");
    printf("  Bus master:    %s\n", (p.Command & 0x0004) ? "enabled" : "disabled");
    printf("  SERR:          %s\n", (p.Command & 0x0100) ? "enabled" : "disabled");
    printf("  INTx disable:  %s\n", (p.Command & 0x0400) ? "yes" : "no");

    for (i = 0; i < 6; ++i) {
        printf("BAR%u cfg: 0x%08lX\n", i, (unsigned long)p.Bar[i]);
    }

    printf("IRQ line=%u pin=%u  header=0x%02X class=%02X:%02X:%02X\n",
        p.InterruptLine,
        p.InterruptPin,
        p.HeaderType,
        p.BaseClass,
        p.SubClass,
        p.ProgIf);

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

static int clear_trace(HANDLE h)
{
    DWORD returned = 0;

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_CLEAR_TRACE,
            NULL,
            0,
            NULL,
            0,
            &returned,
            NULL)) {
        print_error("DEBUG_CLEAR_TRACE");
        return 1;
    }

    printf("trace cleared\n");
    return 0;
}

static int query_trace(HANDLE h)
{
    LECS65_DEBUG_TRACE* trace;
    DWORD returned = 0;
    uint32_t i;

    trace = (LECS65_DEBUG_TRACE*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*trace));

    if (trace == NULL) {
        fprintf(stderr, "HeapAlloc failed\n");
        return 1;
    }

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_DEBUG_GET_TRACE,
            NULL,
            0,
            trace,
            (DWORD)sizeof(*trace),
            &returned,
            NULL)) {
        print_error("DEBUG_GET_TRACE");
        HeapFree(GetProcessHeap(), 0, trace);
        return 1;
    }

    printf("trace version: %lu, entries: %lu, total seen: %llu, returned=%lu\n",
        (unsigned long)trace->Version,
        (unsigned long)trace->Count,
        (unsigned long long)trace->TotalSeen,
        (unsigned long)returned);

    if (trace->Count == 0) {
        printf("trace is empty\n");
        HeapFree(GetProcessHeap(), 0, trace);
        return 0;
    }

    printf("\n");
    printf("SEQ   PID     W64 METHOD   IOCTL       IN     OUT    INFO   STATUS      NAME / INPUT PREVIEW\n");
    printf("----  ------  --- -------- ---------- ------ ------ ------ ----------  --------------------\n");

    for (i = 0; i < trace->Count && i < LECS65_TRACE_CAPACITY; ++i) {
        const LECS65_DEBUG_TRACE_ENTRY* e = &trace->Entry[i];
        uint32_t j;

        printf("%4llu  %6llu  %3s %-8s 0x%08lX %6lu %6lu %6llu 0x%08lX  %s",
            (unsigned long long)e->Sequence,
            (unsigned long long)e->ProcessId,
            e->Wow64 ? "yes" : "no",
            method_name(e->Method),
            (unsigned long)e->Ioctl,
            (unsigned long)e->InputLength,
            (unsigned long)e->OutputLength,
            (unsigned long long)e->Information,
            (unsigned long)e->Status,
            ioctl_name(e->Ioctl));

        if (e->InputPreviewLength != 0) {
            printf("  input:");
            for (j = 0;
                 j < e->InputPreviewLength &&
                 j < LECS65_TRACE_PREVIEW_BYTES;
                 ++j) {
                printf(" %02X", e->InputPreview[j]);
            }
        }

        if (e->OutputPreviewLength != 0) {
            printf("  output:");
            for (j = 0;
                 j < e->OutputPreviewLength &&
                 j < LECS65_TRACE_OUTPUT_PREVIEW_BYTES;
                 ++j) {
                printf(" %02X", e->OutputPreview[j]);
            }
        }

        printf("\n");
    }

    if (trace->TotalSeen > trace->Count) {
        printf("\nNote: ring buffer contains the newest %lu of %llu traced IOCTLs.\n",
            (unsigned long)trace->Count,
            (unsigned long long)trace->TotalSeen);
    }

    HeapFree(GetProcessHeap(), 0, trace);
    return 0;
}

static int query_dallas_id(HANDLE h)
{
    BYTE id[8];
    DWORD returned = 0;
    unsigned i;

    ZeroMemory(id, sizeof(id));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_GET_DALLAS_ID,
            NULL,
            0,
            id,
            sizeof(id),
            &returned,
            NULL)) {
        print_error("GET_DALLAS_ID");
        return 1;
    }

    printf("Dallas ID:");
    for (i = 0; i < sizeof(id); ++i) {
        printf(" %02X", id[i]);
    }
    printf(" (returned=%lu)\n", (unsigned long)returned);
    return 0;
}

static int read_dallas_memory(HANDLE h, unsigned long length)
{
    BYTE buffer[0x200];
    DWORD returned = 0;
    unsigned long offset;

    if (length == 0 || length > sizeof(buffer)) {
        fprintf(stderr, "length must be 1..512\n");
        return 2;
    }

    ZeroMemory(buffer, sizeof(buffer));

    if (!DeviceIoControl(
            h,
            LECS65_IOCTL_READ_DALLAS_MEMORY,
            NULL,
            0,
            buffer,
            (DWORD)length,
            &returned,
            NULL)) {
        print_error("READ_DALLAS_MEMORY");
        return 1;
    }

    printf("Dallas memory: returned=%lu\n", (unsigned long)returned);

    for (offset = 0; offset < returned; offset += 16) {
        unsigned long i;

        printf("%04lX :", offset);
        for (i = 0; i < 16 && offset + i < returned; ++i) {
            printf(" %02X", buffer[offset + i]);
        }
        printf("\n");
    }

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
    printf("  %s pci\n", exe);
    printf("  %s trace\n", exe);
    printf("  %s trace-save <file.jsonl>\n", exe);
    printf("  %s trace-capture <file.jsonl> [seconds, default 60]\n", exe);
    printf("  %s trace-clear\n", exe);
    printf("  %s dallas-id\n", exe);
    printf("  %s dallas-read [length 1..512]\n", exe);
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
    else if (_stricmp(argv[1], "pci") == 0) {
        result = query_pci(h);
    }
    else if (_stricmp(argv[1], "trace") == 0) {
        result = query_trace(h);
    }
    else if (_stricmp(argv[1], "trace-save") == 0 && argc == 3) {
        result = trace_save(h, argv[2]);
    }
    else if (_stricmp(argv[1], "trace-capture") == 0 &&
             (argc == 3 || argc == 4)) {
        unsigned seconds = 60;

        if (argc == 4) {
            char* end = NULL;
            unsigned long parsed = strtoul(argv[3], &end, 0);
            if (end == argv[3] || *end != '\0' ||
                parsed == 0 || parsed > 3600) {
                fprintf(stderr, "seconds must be 1..3600\n");
                CloseHandle(h);
                return 2;
            }
            seconds = (unsigned)parsed;
        }

        result = trace_capture(h, argv[2], seconds);
    }
    else if (_stricmp(argv[1], "trace-clear") == 0) {
        result = clear_trace(h);
    }
    else if (_stricmp(argv[1], "dallas-id") == 0) {
        result = query_dallas_id(h);
    }
    else if (_stricmp(argv[1], "dallas-read") == 0) {
        unsigned long length = 512;

        if (argc == 3) {
            char* end = NULL;
            length = strtoul(argv[2], &end, 0);
            if (end == argv[2] || *end != '\0') {
                fprintf(stderr, "invalid length\n");
                result = 2;
                CloseHandle(h);
                return result;
            }
        }
        else if (argc != 2) {
            usage(argv[0]);
            result = 2;
            CloseHandle(h);
            return result;
        }

        result = read_dallas_memory(h, length);
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
