#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <setupapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include <strsafe.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

#define LECS65_IOCTL_DEBUG_GET_TRACE \
    ((DWORD)CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS))

#define LECS65_TRACE_CAPACITY 256
#define LECS65_TRACE_PREVIEW_BYTES 256
#define LECS65_TRACE_OUTPUT_PREVIEW_BYTES 128

#define WM_APP_TRACE_BATCH (WM_APP + 1)
#define WM_APP_TRACE_STATE (WM_APP + 2)

#define ID_TIMER_UI 1
#define ID_ACTIVITY 1001
#define ID_LOG 1002
#define ID_FILTER 1003
#define ID_HIDE_KNOWN 1004
#define ID_HIDE_BASELINE 1005
#define ID_ERRORS_ONLY 1006
#define ID_PAUSE 1007
#define ID_BASELINE 1008
#define ID_MARKER_EDIT 1009
#define ID_ADD_MARKER 1010
#define ID_ACTION_START 1011
#define ID_ACTION_END 1012
#define ID_SAVE 1013
#define ID_CLEAR 1014
#define ID_STATUS 1015
#define ID_BASELINE_STATUS 1016

#define MAX_STATS 96
#define MAX_EVENTS 5000
#define MAX_LOG_ROWS 2000
#define MAX_BASELINE_HASHES 32
#define ACTIVITY_HOLD_SECONDS 0.75
#define NOVEL_HOLD_SECONDS 5.0
#define BASELINE_SECONDS 5.0

typedef struct LECS65_DEBUG_TRACE_ENTRY {
    uint64_t Sequence;
    uint64_t TimestampTicks;
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

typedef enum SEMANTIC_CONFIDENCE {
    CONF_CONFIRMED = 0,
    CONF_FUNCTIONAL = 1,
    CONF_PARTIAL = 2,
    CONF_UNKNOWN = 3
} SEMANTIC_CONFIDENCE;

typedef struct IOCTL_DESC {
    DWORD Code;
    const wchar_t* Name;
    SEMANTIC_CONFIDENCE Confidence;
} IOCTL_DESC;

typedef struct IOCTL_STAT {
    DWORD Code;
    uint64_t Count;
    uint64_t BaselineStartCount;
    uint64_t RateSampleCount;
    LONGLONG LastSeenQpc;
    LONGLONG RateSampleQpc;
    LONGLONG LastNovelQpc;
    DWORD LastStatus;
    double CurrentRate;
    double BaselineRate;
    uint64_t BaselineHashes[MAX_BASELINE_HASHES];
    unsigned BaselineHashCount;
    BOOL BaselineHashOverflow;
    wchar_t LastDetail[96];
} IOCTL_STAT;

typedef enum LOG_KIND {
    LOG_TRACE = 0,
    LOG_MARKER = 1
} LOG_KIND;

typedef struct LOG_ROW {
    LOG_KIND Kind;
    LONGLONG Qpc;
    LECS65_DEBUG_TRACE_ENTRY Trace;
    SEMANTIC_CONFIDENCE EffectiveConfidence;
    wchar_t Detail[160];
} LOG_ROW;

typedef struct TRACE_BATCH {
    DWORD Count;
    DWORD Dropped;
    LECS65_DEBUG_TRACE_ENTRY Entry[1];
} TRACE_BATCH;

static const GUID g_interface_guids[] = {
    {0x7AC34BE9,0xF766,0x4F15,{0x9E,0x88,0x85,0x4B,0xA5,0xE2,0x14,0x6E}},
    {0x8D1103B8,0x5BF4,0x4B5C,{0xB2,0x1E,0xEE,0xAA,0xCE,0x97,0xD4,0x18}},
    {0x9007C2BC,0xEDFD,0x4F2F,{0xA0,0x59,0xDF,0x11,0x31,0xCB,0x1A,0xE5}},
    {0xFC5DF040,0xD6CD,0x4BA0,{0xB5,0xE0,0x25,0x61,0x97,0x29,0x63,0xA2}}
};

static const IOCTL_DESC g_ioctl_desc[] = {
    {0x00222400, L"LEGACY_START_PROBE", CONF_PARTIAL},
    {0x00222C00, L"DELAY_MS", CONF_CONFIRMED},
    {0x00222C04, L"SET_FLAG_BYTE", CONF_FUNCTIONAL},
    {0x00223000, L"SET_TRACE_CONTROL", CONF_FUNCTIONAL},
    {0x00223004, L"QUERY_BUFFER_A", CONF_PARTIAL},
    {0x0022303C, L"SET_REGISTER_BY_INDEX", CONF_FUNCTIONAL},
    {0x00223040, L"QUERY_BUFFER_B", CONF_PARTIAL},
    {0x00223044, L"READ_START_FVER", CONF_CONFIRMED},
    {0x00223080, L"GET_DALLAS_ID", CONF_CONFIRMED},
    {0x00223084, L"READ_DALLAS_MEMORY", CONF_CONFIRMED},
    {0x00223088, L"WRITE_DALLAS_MEMORY", CONF_CONFIRMED},
    {0x00223100, L"SET_THREE_EVENTS", CONF_FUNCTIONAL},
    {0xCFDC2110, L"COMMAND_TRANSPORT", CONF_PARTIAL},
    {0xCFDC2114, L"CFDC2114_NONINVENTORY", CONF_UNKNOWN},
    {0xCFDC2124, L"REGISTER_TRANSFER", CONF_FUNCTIONAL},
    {0xCFDC2128, L"UNREGISTER_TRANSFER", CONF_FUNCTIONAL},
    {0xCFDC212C, L"NOT_IMPLEMENTED", CONF_CONFIRMED},
    {0xCFDC2130, L"PROG_SERTRIG_FPGA", CONF_CONFIRMED},
    {0xCFDC2138, L"ACQUIRE_BUFFERED", CONF_FUNCTIONAL},
    {0xCFDC2180, L"SET_EVENT_0", CONF_FUNCTIONAL},
    {0xCFDC2184, L"NOOP", CONF_CONFIRMED},
    {0xCFDC218C, L"SET_EVENT_1", CONF_FUNCTIONAL},
    {0xCFDC2190, L"ERROR_MASK_CONTROL", CONF_FUNCTIONAL},
    {0xCFDC2194, L"READ_ERROR_STATUS", CONF_FUNCTIONAL},
    {0xCFDC21C0, L"REGISTER_READ", CONF_CONFIRMED},
    {0xCFDC21C4, L"REGISTER_WRITE", CONF_CONFIRMED},
    {0xCFDC21C8, L"GET_DRIVER_BUILD", CONF_CONFIRMED},
    {0xCFDC21CC, L"CFDC21CC_NONINVENTORY", CONF_UNKNOWN},
    {0xCFDC2400, L"SOFTWARE_PENDING_DISPATCH", CONF_FUNCTIONAL},
    {0xCFDD219F, L"ACQUIRE_NEITHER", CONF_PARTIAL}
};

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_activity;
static HWND g_log;
static HWND g_filter;
static HWND g_hideKnown;
static HWND g_hideBaseline;
static HWND g_errorsOnly;
static HWND g_pause;
static HWND g_baselineButton;
static HWND g_markerEdit;
static HWND g_actionStart;
static HWND g_actionEnd;
static HWND g_status;
static HWND g_baselineStatus;
static HFONT g_font;
static HANDLE g_worker;
static HANDLE g_stopEvent;
static LARGE_INTEGER g_qpcFrequency;
static LONGLONG g_sessionStartQpc;
static BOOL g_connected;
static DWORD g_connectionError;
static uint64_t g_dropped;
static IOCTL_STAT g_stats[MAX_STATS];
static unsigned g_statCount;
static LOG_ROW g_events[MAX_EVENTS];
static unsigned g_eventHead;
static unsigned g_eventCount;
static BOOL g_baselineActive;
static BOOL g_baselineReady;
static LONGLONG g_baselineStartQpc;
static BOOL g_actionActive;
static LONGLONG g_actionStartQpc;
static uint64_t g_actionStartCount[MAX_STATS];
static wchar_t g_actionLabel[128];

static BOOL nt_success(DWORD status)
{
    return (status & 0x80000000UL) == 0;
}

static BOOL is_sensitive_ioctl(DWORD code)
{
    return code == 0x00223080 ||
           code == 0x00223084 ||
           code == 0x00223088;
}

static const IOCTL_DESC* find_desc(DWORD code)
{
    size_t i;
    for (i = 0; i < sizeof(g_ioctl_desc) / sizeof(g_ioctl_desc[0]); ++i) {
        if (g_ioctl_desc[i].Code == code) {
            return &g_ioctl_desc[i];
        }
    }
    return NULL;
}

static const wchar_t* confidence_name(SEMANTIC_CONFIDENCE confidence)
{
    switch (confidence) {
    case CONF_CONFIRMED: return L"confirmed";
    case CONF_FUNCTIONAL: return L"functional";
    case CONF_PARTIAL: return L"partial";
    default: return L"unknown";
    }
}

static void format_ioctl_name(DWORD code, wchar_t* buffer, size_t cch)
{
    const IOCTL_DESC* desc = find_desc(code);
    if (desc != NULL) {
        StringCchCopyW(buffer, cch, desc->Name);
    } else {
        StringCchPrintfW(buffer, cch, L"UNKNOWN_0x%08lX", (unsigned long)code);
    }
}

static SEMANTIC_CONFIDENCE decode_nested(
    const LECS65_DEBUG_TRACE_ENTRY* entry,
    wchar_t* buffer,
    size_t cch)
{
    uint32_t length;
    uint32_t i;
    unsigned family;
    unsigned opcode;
    unsigned arg = 0;
    const wchar_t* name = L"";
    SEMANTIC_CONFIDENCE confidence = CONF_UNKNOWN;

    buffer[0] = L'\0';
    if (entry->Ioctl != 0xCFDC2110) {
        const IOCTL_DESC* desc = find_desc(entry->Ioctl);
        return desc != NULL ? desc->Confidence : CONF_UNKNOWN;
    }

    length = entry->InputPreviewLength;
    if (length > LECS65_TRACE_PREVIEW_BYTES) {
        length = LECS65_TRACE_PREVIEW_BYTES;
    }

    for (i = 0; i + 5 < length; ++i) {
        if (entry->InputPreview[i] == 0xFB &&
            entry->InputPreview[i + 1] == 0xA5) {
            family = entry->InputPreview[i + 3];
            opcode = entry->InputPreview[i + 4];
            arg = entry->InputPreview[i + 5];

            if (family == 2 && opcode == 0x40) {
                name = L"RESET";
                confidence = CONF_FUNCTIONAL;
            } else if (family == 1 && opcode == 0x42) {
                name = L"JTAG";
                confidence = CONF_FUNCTIONAL;
            } else if (family == 0 && opcode == 0x88) {
                name = L"PENDING_ACK";
                confidence = CONF_FUNCTIONAL;
            } else if (family == 1 && opcode == 0x81) {
                name = L"FW_MESSAGE_81";
                confidence = CONF_PARTIAL;
            } else if (family == 1 && opcode == 0x82) {
                name = L"PROBE_STATE";
                confidence = CONF_PARTIAL;
            } else if (family == 0 && opcode == 0x85) {
                name = L"HOST_STATE_85";
                confidence = CONF_PARTIAL;
            } else if (family == 1 && opcode == 0x90) {
                name = L"FW_MESSAGE_90";
                confidence = CONF_PARTIAL;
            } else if (family == 1 && opcode == 0x99) {
                name = L"FW_MESSAGE_99";
                confidence = CONF_PARTIAL;
            } else if (family == 1 && opcode == 0x4A) {
                name = L"PROBE_METADATA";
                confidence = CONF_PARTIAL;
            } else if (family == 0 && opcode == 0x4A) {
                name = L"PROBE_CONTROL";
                confidence = CONF_PARTIAL;
            } else {
                name = L"UNCLASSIFIED";
                confidence = CONF_UNKNOWN;
            }

            if (opcode == 0x42) {
                StringCchPrintfW(
                    buffer,
                    cch,
                    L"F%u/0x%02X %s mode=%u",
                    family,
                    opcode,
                    name,
                    arg);
            } else {
                StringCchPrintfW(
                    buffer,
                    cch,
                    L"F%u/0x%02X %s",
                    family,
                    opcode,
                    name);
            }
            return confidence;
        }
    }

    StringCchCopyW(buffer, cch, L"CFDC2110 undecoded");
    return CONF_UNKNOWN;
}

static uint64_t fnv1a64(const void* data, size_t length, uint64_t hash)
{
    const unsigned char* p = (const unsigned char*)data;
    size_t i;
    for (i = 0; i < length; ++i) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t trace_variant_hash(const LECS65_DEBUG_TRACE_ENTRY* entry)
{
    uint64_t h = 1469598103934665603ULL;
    h = fnv1a64(&entry->InputLength, sizeof(entry->InputLength), h);
    h = fnv1a64(&entry->OutputLength, sizeof(entry->OutputLength), h);
    if (!is_sensitive_ioctl(entry->Ioctl) &&
        entry->InputPreviewLength != 0) {
        uint32_t n = entry->InputPreviewLength;
        if (n > 64) {
            n = 64;
        }
        h = fnv1a64(entry->InputPreview, n, h);
    }
    return h;
}

static IOCTL_STAT* find_stat(DWORD code, BOOL create)
{
    unsigned i;
    LARGE_INTEGER now;

    for (i = 0; i < g_statCount; ++i) {
        if (g_stats[i].Code == code) {
            return &g_stats[i];
        }
    }

    if (!create || g_statCount >= MAX_STATS) {
        return NULL;
    }

    i = g_statCount++;
    ZeroMemory(&g_stats[i], sizeof(g_stats[i]));
    g_stats[i].Code = code;
    QueryPerformanceCounter(&now);
    g_stats[i].RateSampleQpc = now.QuadPart;
    return &g_stats[i];
}

static void initialize_stats(void)
{
    size_t i;
    g_statCount = 0;
    for (i = 0; i < sizeof(g_ioctl_desc) / sizeof(g_ioctl_desc[0]); ++i) {
        find_stat(g_ioctl_desc[i].Code, TRUE);
    }
}

static BOOL baseline_contains_hash(const IOCTL_STAT* stat, uint64_t hash)
{
    unsigned i;
    for (i = 0; i < stat->BaselineHashCount; ++i) {
        if (stat->BaselineHashes[i] == hash) {
            return TRUE;
        }
    }
    return FALSE;
}

static void baseline_add_hash(IOCTL_STAT* stat, uint64_t hash)
{
    if (baseline_contains_hash(stat, hash)) {
        return;
    }
    if (stat->BaselineHashCount < MAX_BASELINE_HASHES) {
        stat->BaselineHashes[stat->BaselineHashCount++] = hash;
    } else {
        stat->BaselineHashOverflow = TRUE;
    }
}

static void push_log_row(const LOG_ROW* row)
{
    g_events[g_eventHead] = *row;
    g_eventHead = (g_eventHead + 1) % MAX_EVENTS;
    if (g_eventCount < MAX_EVENTS) {
        ++g_eventCount;
    }
}

static const LOG_ROW* event_by_ordinal(unsigned ordinal)
{
    unsigned start;
    if (ordinal >= g_eventCount) {
        return NULL;
    }
    start = (g_eventHead + MAX_EVENTS - g_eventCount) % MAX_EVENTS;
    return &g_events[(start + ordinal) % MAX_EVENTS];
}

static void add_marker_at(LONGLONG qpc, const wchar_t* text)
{
    LOG_ROW row;
    ZeroMemory(&row, sizeof(row));
    row.Kind = LOG_MARKER;
    row.Qpc = qpc;
    row.EffectiveConfidence = CONF_CONFIRMED;
    StringCchCopyW(row.Detail, _countof(row.Detail), text);
    push_log_row(&row);
}

static double qpc_seconds(LONGLONG delta)
{
    if (g_qpcFrequency.QuadPart == 0) {
        return 0.0;
    }
    return (double)delta / (double)g_qpcFrequency.QuadPart;
}

static BOOL checked(HWND control)
{
    return Button_GetCheck(control) == BST_CHECKED;
}

static BOOL contains_case_insensitive(const wchar_t* haystack, const wchar_t* needle)
{
    wchar_t h[256];
    wchar_t n[128];
    size_t i;

    if (needle == NULL || needle[0] == L'\0') {
        return TRUE;
    }

    StringCchCopyW(h, _countof(h), haystack != NULL ? haystack : L"");
    StringCchCopyW(n, _countof(n), needle);
    for (i = 0; h[i] != L'\0'; ++i) {
        h[i] = (wchar_t)towlower(h[i]);
    }
    for (i = 0; n[i] != L'\0'; ++i) {
        n[i] = (wchar_t)towlower(n[i]);
    }
    return wcsstr(h, n) != NULL;
}

static BOOL stat_is_baseline_noise(const IOCTL_STAT* stat, LONGLONG now)
{
    if (!g_baselineReady || stat == NULL) {
        return FALSE;
    }
    if (stat->LastNovelQpc != 0 &&
        qpc_seconds(now - stat->LastNovelQpc) <= NOVEL_HOLD_SECONDS) {
        return FALSE;
    }
    if (stat->BaselineRate < 0.5) {
        return FALSE;
    }
    return stat->CurrentRate <= (stat->BaselineRate * 1.5 + 1.0);
}

static BOOL should_show_trace(
    const LECS65_DEBUG_TRACE_ENTRY* entry,
    SEMANTIC_CONFIDENCE confidence,
    const wchar_t* detail,
    LONGLONG now)
{
    wchar_t filter[128];
    wchar_t name[96];
    wchar_t code[32];
    IOCTL_STAT* stat;

    if (checked(g_errorsOnly) && nt_success(entry->Status)) {
        return FALSE;
    }

    if (checked(g_hideKnown) &&
        (confidence == CONF_CONFIRMED || confidence == CONF_FUNCTIONAL)) {
        return FALSE;
    }

    stat = find_stat(entry->Ioctl, FALSE);
    if (checked(g_hideBaseline) && stat_is_baseline_noise(stat, now)) {
        return FALSE;
    }

    GetWindowTextW(g_filter, filter, _countof(filter));
    if (filter[0] != L'\0') {
        format_ioctl_name(entry->Ioctl, name, _countof(name));
        StringCchPrintfW(code, _countof(code), L"0x%08lX", (unsigned long)entry->Ioctl);
        if (!contains_case_insensitive(name, filter) &&
            !contains_case_insensitive(code, filter) &&
            !contains_case_insensitive(detail, filter)) {
            return FALSE;
        }
    }

    return TRUE;
}

static void set_list_text(HWND list, int row, int column, const wchar_t* text)
{
    ListView_SetItemText(list, row, column, (LPWSTR)text);
}

static void insert_log_list_row(const LOG_ROW* row)
{
    LVITEMW item;
    wchar_t timeText[48];
    wchar_t seqText[32];
    wchar_t codeText[32];
    wchar_t name[96];
    wchar_t sizes[48];
    wchar_t status[48];
    int index;
    LARGE_INTEGER now;

    if (checked(g_pause)) {
        return;
    }

    QueryPerformanceCounter(&now);

    if (row->Kind == LOG_MARKER) {
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        StringCchPrintfW(
            timeText,
            _countof(timeText),
            L"%+.3f",
            qpc_seconds(row->Qpc - g_sessionStartQpc));
        item.pszText = timeText;
        item.iItem = ListView_GetItemCount(g_log);
        item.lParam = 0;
        index = ListView_InsertItem(g_log, &item);
        set_list_text(g_log, index, 1, L"-");
        set_list_text(g_log, index, 2, L"MARK");
        set_list_text(g_log, index, 3, row->Detail);
        set_list_text(g_log, index, 4, L"");
        set_list_text(g_log, index, 5, L"");
    } else {
        if (!should_show_trace(
                &row->Trace,
                row->EffectiveConfidence,
                row->Detail,
                now.QuadPart)) {
            return;
        }

        format_ioctl_name(row->Trace.Ioctl, name, _countof(name));
        if (row->Detail[0] != L'\0') {
            StringCchCatW(name, _countof(name), L" | ");
            StringCchCatW(name, _countof(name), row->Detail);
        }

        StringCchPrintfW(
            timeText,
            _countof(timeText),
            L"%+.3f",
            qpc_seconds(row->Trace.TimestampTicks - g_sessionStartQpc));
        StringCchPrintfW(
            seqText,
            _countof(seqText),
            L"%llu",
            (unsigned long long)row->Trace.Sequence);
        StringCchPrintfW(
            codeText,
            _countof(codeText),
            L"0x%08lX",
            (unsigned long)row->Trace.Ioctl);
        StringCchPrintfW(
            sizes,
            _countof(sizes),
            L"%lu / %lu",
            (unsigned long)row->Trace.InputLength,
            (unsigned long)row->Trace.OutputLength);
        StringCchPrintfW(
            status,
            _countof(status),
            L"0x%08lX %s",
            (unsigned long)row->Trace.Status,
            nt_success(row->Trace.Status) ? L"OK" : L"ERR");

        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(g_log);
        item.pszText = timeText;
        item.lParam = (LPARAM)row->Trace.Sequence;
        index = ListView_InsertItem(g_log, &item);
        set_list_text(g_log, index, 1, seqText);
        set_list_text(g_log, index, 2, codeText);
        set_list_text(g_log, index, 3, name);
        set_list_text(g_log, index, 4, sizes);
        set_list_text(g_log, index, 5, status);
    }

    while (ListView_GetItemCount(g_log) > MAX_LOG_ROWS) {
        ListView_DeleteItem(g_log, 0);
    }

    if (ListView_GetItemCount(g_log) > 0) {
        ListView_EnsureVisible(g_log, ListView_GetItemCount(g_log) - 1, FALSE);
    }
}

static void rebuild_log(void)
{
    unsigned i;
    const LOG_ROW* row;

    ListView_DeleteAllItems(g_log);
    if (checked(g_pause)) {
        return;
    }

    for (i = 0; i < g_eventCount; ++i) {
        row = event_by_ordinal(i);
        if (row != NULL) {
            insert_log_list_row(row);
        }
    }
}

static void refresh_activity(void)
{
    unsigned i;
    LARGE_INTEGER now;
    wchar_t filter[128];

    QueryPerformanceCounter(&now);
    GetWindowTextW(g_filter, filter, _countof(filter));
    ListView_DeleteAllItems(g_activity);

    for (i = 0; i < g_statCount; ++i) {
        IOCTL_STAT* stat = &g_stats[i];
        const IOCTL_DESC* desc = find_desc(stat->Code);
        SEMANTIC_CONFIDENCE confidence =
            desc != NULL ? desc->Confidence : CONF_UNKNOWN;
        wchar_t name[160];
        wchar_t code[32];
        wchar_t rate[64];
        wchar_t count[40];
        wchar_t status[48];
        const wchar_t* led;
        BOOL active =
            stat->LastSeenQpc != 0 &&
            qpc_seconds(now.QuadPart - stat->LastSeenQpc) <= ACTIVITY_HOLD_SECONDS;
        int row;
        LVITEMW item;

        if (stat->Code == 0xCFDC2110 && stat->LastDetail[0] != L'\0') {
            confidence = CONF_PARTIAL;
        }

        format_ioctl_name(stat->Code, name, _countof(name));
        if (stat->LastDetail[0] != L'\0') {
            StringCchCatW(name, _countof(name), L" | ");
            StringCchCatW(name, _countof(name), stat->LastDetail);
        }

        if (checked(g_hideKnown) &&
            (confidence == CONF_CONFIRMED || confidence == CONF_FUNCTIONAL) &&
            stat->Code != 0xCFDC2110) {
            continue;
        }

        if (checked(g_hideBaseline) && stat_is_baseline_noise(stat, now.QuadPart)) {
            continue;
        }

        if (checked(g_errorsOnly) &&
            (stat->Count == 0 || nt_success(stat->LastStatus))) {
            continue;
        }

        StringCchPrintfW(code, _countof(code), L"0x%08lX", (unsigned long)stat->Code);
        if (filter[0] != L'\0' &&
            !contains_case_insensitive(name, filter) &&
            !contains_case_insensitive(code, filter)) {
            continue;
        }

        led = active ? L"\x25CF" : L"\x25CB";
        StringCchPrintfW(
            rate,
            _countof(rate),
            L"%.1f/s%s",
            stat->CurrentRate,
            (g_baselineReady && stat->BaselineRate > 0.0) ? L" *" : L"");
        StringCchPrintfW(
            count,
            _countof(count),
            L"%llu",
            (unsigned long long)stat->Count);
        if (stat->Count != 0) {
            StringCchPrintfW(
                status,
                _countof(status),
                L"0x%08lX %s",
                (unsigned long)stat->LastStatus,
                nt_success(stat->LastStatus) ? L"OK" : L"ERR");
        } else {
            StringCchCopyW(status, _countof(status), L"-");
        }

        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(g_activity);
        item.pszText = (LPWSTR)led;
        item.lParam = (LPARAM)i;
        row = ListView_InsertItem(g_activity, &item);
        set_list_text(g_activity, row, 1, code);
        set_list_text(g_activity, row, 2, name);
        set_list_text(g_activity, row, 3, rate);
        set_list_text(g_activity, row, 4, count);
        set_list_text(g_activity, row, 5, status);
        set_list_text(g_activity, row, 6, confidence_name(confidence));
    }
}

static void update_rates(LONGLONG now)
{
    unsigned i;
    for (i = 0; i < g_statCount; ++i) {
        IOCTL_STAT* stat = &g_stats[i];
        double seconds = qpc_seconds(now - stat->RateSampleQpc);
        if (seconds >= 0.75) {
            stat->CurrentRate =
                (double)(stat->Count - stat->RateSampleCount) / seconds;
            stat->RateSampleCount = stat->Count;
            stat->RateSampleQpc = now;
        }
    }
}

static void begin_baseline(void)
{
    LARGE_INTEGER now;
    unsigned i;

    QueryPerformanceCounter(&now);
    g_baselineActive = TRUE;
    g_baselineReady = FALSE;
    g_baselineStartQpc = now.QuadPart;

    for (i = 0; i < g_statCount; ++i) {
        g_stats[i].BaselineStartCount = g_stats[i].Count;
        g_stats[i].BaselineRate = 0.0;
        g_stats[i].BaselineHashCount = 0;
        g_stats[i].BaselineHashOverflow = FALSE;
        g_stats[i].LastNovelQpc = 0;
    }

    SetWindowTextW(g_baselineStatus, L"Learning idle baseline: 5.0 s");
    EnableWindow(g_baselineButton, FALSE);
    add_marker_at(now.QuadPart, L"IDLE BASELINE START");
    rebuild_log();
}

static void finish_baseline(LONGLONG now)
{
    unsigned i;
    double seconds = qpc_seconds(now - g_baselineStartQpc);
    wchar_t text[160];

    if (seconds <= 0.0) {
        seconds = BASELINE_SECONDS;
    }

    for (i = 0; i < g_statCount; ++i) {
        g_stats[i].BaselineRate =
            (double)(g_stats[i].Count - g_stats[i].BaselineStartCount) / seconds;
    }

    g_baselineActive = FALSE;
    g_baselineReady = TRUE;
    EnableWindow(g_baselineButton, TRUE);
    StringCchPrintfW(
        text,
        _countof(text),
        L"Idle baseline ready (%.1f s). '*' marks IOCTLs with baseline traffic.",
        seconds);
    SetWindowTextW(g_baselineStatus, text);
    add_marker_at(now, L"IDLE BASELINE READY");
    rebuild_log();
}

static void record_trace(const LECS65_DEBUG_TRACE_ENTRY* entry)
{
    LOG_ROW row;
    IOCTL_STAT* stat;
    uint64_t hash;
    BOOL novel = FALSE;

    stat = find_stat(entry->Ioctl, TRUE);
    if (stat != NULL) {
        ++stat->Count;
        stat->LastSeenQpc = entry->TimestampTicks;
        stat->LastStatus = entry->Status;
    }

    ZeroMemory(&row, sizeof(row));
    row.Kind = LOG_TRACE;
    row.Qpc = entry->TimestampTicks;
    row.Trace = *entry;
    row.EffectiveConfidence =
        decode_nested(entry, row.Detail, _countof(row.Detail));

    if (stat != NULL) {
        if (entry->Ioctl == 0xCFDC2110) {
            StringCchCopyW(
                stat->LastDetail,
                _countof(stat->LastDetail),
                row.Detail);
        }

        hash = trace_variant_hash(entry);
        if (g_baselineActive) {
            baseline_add_hash(stat, hash);
        } else if (g_baselineReady) {
            if (!baseline_contains_hash(stat, hash) ||
                stat->BaselineHashOverflow) {
                novel = TRUE;
            }
        }

        if (novel) {
            stat->LastNovelQpc = entry->TimestampTicks;
        }
    }

    push_log_row(&row);
    insert_log_list_row(&row);
}

static void add_user_marker(void)
{
    wchar_t text[128];
    wchar_t marker[180];
    LARGE_INTEGER now;

    GetWindowTextW(g_markerEdit, text, _countof(text));
    if (text[0] == L'\0') {
        StringCchCopyW(text, _countof(text), L"Marker");
    }

    QueryPerformanceCounter(&now);
    StringCchPrintfW(marker, _countof(marker), L"USER: %s", text);
    add_marker_at(now.QuadPart, marker);
    SetWindowTextW(g_markerEdit, L"");
    rebuild_log();
}

static void start_action(void)
{
    unsigned i;
    LARGE_INTEGER now;

    if (g_actionActive) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    GetWindowTextW(g_markerEdit, g_actionLabel, _countof(g_actionLabel));
    if (g_actionLabel[0] == L'\0') {
        StringCchCopyW(g_actionLabel, _countof(g_actionLabel), L"Action");
    }

    QueryPerformanceCounter(&now);
    g_actionStartQpc = now.QuadPart;
    g_actionActive = TRUE;

    for (i = 0; i < MAX_STATS; ++i) {
        g_actionStartCount[i] = (i < g_statCount) ? g_stats[i].Count : 0;
    }

    {
        wchar_t text[180];
        StringCchPrintfW(text, _countof(text), L"ACTION START: %s", g_actionLabel);
        add_marker_at(now.QuadPart, text);
    }

    EnableWindow(g_actionStart, FALSE);
    EnableWindow(g_actionEnd, TRUE);
    rebuild_log();
}

static void end_action(void)
{
    LARGE_INTEGER now;
    unsigned i;
    unsigned changed = 0;
    wchar_t text[200];

    if (!g_actionActive) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    QueryPerformanceCounter(&now);
    StringCchPrintfW(
        text,
        _countof(text),
        L"ACTION END: %s (%.3f s)",
        g_actionLabel,
        qpc_seconds(now.QuadPart - g_actionStartQpc));
    add_marker_at(now.QuadPart, text);

    for (i = 0; i < g_statCount; ++i) {
        uint64_t before = i < MAX_STATS ? g_actionStartCount[i] : 0;
        uint64_t delta = g_stats[i].Count - before;
        if (delta != 0) {
            wchar_t name[96];
            format_ioctl_name(g_stats[i].Code, name, _countof(name));
            StringCchPrintfW(
                text,
                _countof(text),
                L"ACTION IOCTL: 0x%08lX %s x%llu",
                (unsigned long)g_stats[i].Code,
                name,
                (unsigned long long)delta);
            add_marker_at(now.QuadPart, text);
            ++changed;
        }
    }

    if (changed == 0) {
        add_marker_at(now.QuadPart, L"ACTION IOCTL: no traced IOCTLs observed");
    }

    g_actionActive = FALSE;
    EnableWindow(g_actionStart, TRUE);
    EnableWindow(g_actionEnd, FALSE);
    rebuild_log();
}

static void write_json_wide(FILE* f, const wchar_t* text)
{
    const wchar_t* p = text;
    fputwc(L'"', f);
    while (*p != L'\0') {
        switch (*p) {
        case L'\\': fputws(L"\\\\", f); break;
        case L'"': fputws(L"\\\"", f); break;
        case L'\n': fputws(L"\\n", f); break;
        case L'\r': fputws(L"\\r", f); break;
        case L'\t': fputws(L"\\t", f); break;
        default:
            if (*p < 0x20) {
                fwprintf(f, L"\\u%04X", (unsigned)*p);
            } else {
                fputwc(*p, f);
            }
            break;
        }
        ++p;
    }
    fputwc(L'"', f);
}

static void write_hex_json(FILE* f, const uint8_t* bytes, uint32_t length)
{
    uint32_t i;
    fputwc(L'"', f);
    for (i = 0; i < length; ++i) {
        fwprintf(f, L"%02X", bytes[i]);
    }
    fputwc(L'"', f);
}

static void save_session(void)
{
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH] = L"lecwatch_session.jsonl";
    FILE* f;
    unsigned i;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"JSON Lines (*.jsonl)\0*.jsonl\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = _countof(path);
    ofn.lpstrDefExt = L"jsonl";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) {
        return;
    }

    f = _wfopen(path, L"wt, ccs=UTF-8");
    if (f == NULL) {
        MessageBoxW(g_main, L"Could not create session file.", L"lecwatch", MB_ICONERROR);
        return;
    }

    fwprintf(
        f,
        L"{\"type\":\"lecwatch_session\",\"format_version\":1,"
        L"\"dropped\":%llu,\"baseline_ready\":%s}\n",
        (unsigned long long)g_dropped,
        g_baselineReady ? L"true" : L"false");

    for (i = 0; i < g_eventCount; ++i) {
        const LOG_ROW* row = event_by_ordinal(i);
        if (row == NULL) {
            continue;
        }

        if (row->Kind == LOG_MARKER) {
            fwprintf(
                f,
                L"{\"type\":\"marker\",\"qpc\":%lld,\"text\":",
                (long long)row->Qpc);
            write_json_wide(f, row->Detail);
            fputws(L"}\n", f);
        } else {
            const LECS65_DEBUG_TRACE_ENTRY* e = &row->Trace;
            uint32_t inLen = e->InputPreviewLength;
            uint32_t outLen = e->OutputPreviewLength;
            BOOL redact = is_sensitive_ioctl(e->Ioctl);
            if (inLen > LECS65_TRACE_PREVIEW_BYTES) inLen = LECS65_TRACE_PREVIEW_BYTES;
            if (outLen > LECS65_TRACE_OUTPUT_PREVIEW_BYTES) outLen = LECS65_TRACE_OUTPUT_PREVIEW_BYTES;

            fwprintf(
                f,
                L"{\"type\":\"ioctl\",\"seq\":%llu,\"timestamp_ticks\":%llu,"
                L"\"pid\":%llu,\"wow64\":%u,\"ioctl\":\"0x%08lX\","
                L"\"input_length\":%lu,\"output_length\":%lu,"
                L"\"information\":%llu,\"status\":\"0x%08lX\","
                L"\"confidence\":",
                (unsigned long long)e->Sequence,
                (unsigned long long)e->TimestampTicks,
                (unsigned long long)e->ProcessId,
                (unsigned)e->Wow64,
                (unsigned long)e->Ioctl,
                (unsigned long)e->InputLength,
                (unsigned long)e->OutputLength,
                (unsigned long long)e->Information,
                (unsigned long)e->Status);
            write_json_wide(f, confidence_name(row->EffectiveConfidence));
            fputws(L",\"detail\":", f);
            write_json_wide(f, row->Detail);
            fputws(L",\"input_hex\":", f);
            write_hex_json(f, e->InputPreview, redact ? 0 : inLen);
            fputws(L",\"output_hex\":", f);
            write_hex_json(f, e->OutputPreview, redact ? 0 : outLen);
            fwprintf(
                f,
                L",\"sensitive_payload_redacted\":%s}\n",
                redact ? L"true" : L"false");
        }
    }

    fclose(f);
    MessageBoxW(g_main, L"Session saved.", L"lecwatch", MB_OK | MB_ICONINFORMATION);
}

static void clear_session(void)
{
    LARGE_INTEGER now;
    unsigned i;

    QueryPerformanceCounter(&now);
    g_sessionStartQpc = now.QuadPart;
    g_eventHead = 0;
    g_eventCount = 0;
    g_dropped = 0;
    g_baselineActive = FALSE;
    g_baselineReady = FALSE;
    g_actionActive = FALSE;
    SetWindowTextW(g_baselineStatus, L"No idle baseline learned.");
    EnableWindow(g_baselineButton, TRUE);
    EnableWindow(g_actionStart, TRUE);
    EnableWindow(g_actionEnd, FALSE);

    for (i = 0; i < g_statCount; ++i) {
        DWORD code = g_stats[i].Code;
        ZeroMemory(&g_stats[i], sizeof(g_stats[i]));
        g_stats[i].Code = code;
        g_stats[i].RateSampleQpc = now.QuadPart;
    }

    ListView_DeleteAllItems(g_log);
    refresh_activity();
}

static HANDLE open_driver(void)
{
    size_t guidIndex;

    for (guidIndex = 0;
         guidIndex < sizeof(g_interface_guids) / sizeof(g_interface_guids[0]);
         ++guidIndex) {
        HDEVINFO info = SetupDiGetClassDevsW(
            &g_interface_guids[guidIndex],
            NULL,
            NULL,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (info != INVALID_HANDLE_VALUE) {
            SP_DEVICE_INTERFACE_DATA iface;
            DWORD index;

            ZeroMemory(&iface, sizeof(iface));
            iface.cbSize = sizeof(iface);

            for (index = 0;
                 SetupDiEnumDeviceInterfaces(
                     info,
                     NULL,
                     &g_interface_guids[guidIndex],
                     index,
                     &iface);
                 ++index) {
                DWORD required = 0;
                PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail;
                HANDLE h;

                SetupDiGetDeviceInterfaceDetailW(
                    info, &iface, NULL, 0, &required, NULL);
                if (required == 0) {
                    continue;
                }

                detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(
                    GetProcessHeap(), HEAP_ZERO_MEMORY, required);
                if (detail == NULL) {
                    continue;
                }

                detail->cbSize = sizeof(*detail);
                if (!SetupDiGetDeviceInterfaceDetailW(
                        info, &iface, detail, required, NULL, NULL)) {
                    HeapFree(GetProcessHeap(), 0, detail);
                    continue;
                }

                h = CreateFileW(
                    detail->DevicePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL);

                HeapFree(GetProcessHeap(), 0, detail);
                if (h != INVALID_HANDLE_VALUE) {
                    SetupDiDestroyDeviceInfoList(info);
                    return h;
                }
            }

            SetupDiDestroyDeviceInfoList(info);
        }
    }

    return CreateFileW(
        L"\\\\.\\ALADDINAcqDriver0",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
}

static BOOL get_trace_snapshot(HANDLE device, LECS65_DEBUG_TRACE* trace)
{
    DWORD returned = 0;
    ZeroMemory(trace, sizeof(*trace));
    return DeviceIoControl(
        device,
        LECS65_IOCTL_DEBUG_GET_TRACE,
        NULL,
        0,
        trace,
        (DWORD)sizeof(*trace),
        &returned,
        NULL);
}

static DWORD WINAPI trace_worker(LPVOID context)
{
    HWND hwnd = (HWND)context;
    HANDLE device = INVALID_HANDLE_VALUE;
    LECS65_DEBUG_TRACE* trace = NULL;
    uint64_t lastSequence = 0;
    BOOL initialized = FALSE;

    trace = (LECS65_DEBUG_TRACE*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*trace));
    if (trace == NULL) {
        PostMessageW(hwnd, WM_APP_TRACE_STATE, FALSE, ERROR_OUTOFMEMORY);
        return 1;
    }

    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        if (device == INVALID_HANDLE_VALUE) {
            device = open_driver();
            if (device == INVALID_HANDLE_VALUE) {
                PostMessageW(hwnd, WM_APP_TRACE_STATE, FALSE, GetLastError());
                if (WaitForSingleObject(g_stopEvent, 1000) != WAIT_TIMEOUT) {
                    break;
                }
                continue;
            }
            lastSequence = 0;
            initialized = FALSE;
            PostMessageW(hwnd, WM_APP_TRACE_STATE, TRUE, 0);
        }

        if (!get_trace_snapshot(device, trace)) {
            DWORD error = GetLastError();
            CloseHandle(device);
            device = INVALID_HANDLE_VALUE;
            PostMessageW(hwnd, WM_APP_TRACE_STATE, FALSE, error);
            if (WaitForSingleObject(g_stopEvent, 1000) != WAIT_TIMEOUT) {
                break;
            }
            continue;
        }

        if (!initialized) {
            lastSequence = trace->TotalSeen;
            initialized = TRUE;
        } else if (trace->TotalSeen < lastSequence) {
            lastSequence = trace->TotalSeen;
        } else {
            DWORD i;
            DWORD newCount = 0;
            DWORD dropped = 0;
            size_t bytes;
            TRACE_BATCH* batch;

            for (i = 0; i < trace->Count && i < LECS65_TRACE_CAPACITY; ++i) {
                if (trace->Entry[i].Sequence > lastSequence) {
                    ++newCount;
                }
            }

            if (newCount != 0) {
                bytes = sizeof(TRACE_BATCH) +
                    (size_t)(newCount - 1) * sizeof(LECS65_DEBUG_TRACE_ENTRY);
                batch = (TRACE_BATCH*)HeapAlloc(
                    GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
                if (batch != NULL) {
                    DWORD out = 0;
                    uint64_t expected = lastSequence + 1;
                    for (i = 0; i < trace->Count && i < LECS65_TRACE_CAPACITY; ++i) {
                        const LECS65_DEBUG_TRACE_ENTRY* e = &trace->Entry[i];
                        if (e->Sequence <= lastSequence) {
                            continue;
                        }
                        if (e->Sequence > expected) {
                            dropped += (DWORD)(e->Sequence - expected);
                        }
                        batch->Entry[out++] = *e;
                        expected = e->Sequence + 1;
                    }
                    batch->Count = out;
                    batch->Dropped = dropped;
                    lastSequence = batch->Entry[out - 1].Sequence;

                    if (!PostMessageW(
                            hwnd, WM_APP_TRACE_BATCH, 0, (LPARAM)batch)) {
                        HeapFree(GetProcessHeap(), 0, batch);
                    }
                } else {
                    lastSequence = trace->TotalSeen;
                }
            }
        }

        if (WaitForSingleObject(g_stopEvent, 100) != WAIT_TIMEOUT) {
            break;
        }
    }

    if (device != INVALID_HANDLE_VALUE) {
        CloseHandle(device);
    }
    HeapFree(GetProcessHeap(), 0, trace);
    return 0;
}

static void update_status_text(void)
{
    wchar_t text[256];
    if (g_connected) {
        StringCchPrintfW(
            text,
            _countof(text),
            L"Connected | dropped trace records: %llu | %u tracked IOCTL codes",
            (unsigned long long)g_dropped,
            g_statCount);
    } else {
        StringCchPrintfW(
            text,
            _countof(text),
            L"Disconnected | Win32 error %lu | retrying...",
            (unsigned long)g_connectionError);
    }
    SetWindowTextW(g_status, text);
}

static void show_trace_details(uint64_t sequence)
{
    unsigned i;
    wchar_t text[8192];

    if (sequence == 0) {
        return;
    }

    for (i = 0; i < g_eventCount; ++i) {
        const LOG_ROW* row = event_by_ordinal(i);
        if (row != NULL &&
            row->Kind == LOG_TRACE &&
            row->Trace.Sequence == sequence) {
            const LECS65_DEBUG_TRACE_ENTRY* e = &row->Trace;
            wchar_t name[96];
            size_t pos = 0;
            uint32_t j;
            uint32_t inLen = e->InputPreviewLength;
            uint32_t outLen = e->OutputPreviewLength;

            format_ioctl_name(e->Ioctl, name, _countof(name));
            if (inLen > LECS65_TRACE_PREVIEW_BYTES) inLen = LECS65_TRACE_PREVIEW_BYTES;
            if (outLen > LECS65_TRACE_OUTPUT_PREVIEW_BYTES) outLen = LECS65_TRACE_OUTPUT_PREVIEW_BYTES;

            pos += swprintf(
                text + pos,
                _countof(text) - pos,
                L"%s\n0x%08lX\n%s\n\nseq=%llu  pid=%llu  wow64=%u\n"
                L"in=%lu  out=%lu  information=%llu\nstatus=0x%08lX\n\n",
                name,
                (unsigned long)e->Ioctl,
                row->Detail,
                (unsigned long long)e->Sequence,
                (unsigned long long)e->ProcessId,
                (unsigned)e->Wow64,
                (unsigned long)e->InputLength,
                (unsigned long)e->OutputLength,
                (unsigned long long)e->Information,
                (unsigned long)e->Status);

            if (is_sensitive_ioctl(e->Ioctl)) {
                StringCchCatW(
                    text,
                    _countof(text),
                    L"Payload preview redacted: Dallas data may contain XStream license material.");
            } else {
                StringCchCatW(text, _countof(text), L"Input preview:\n");
                pos = wcslen(text);
                for (j = 0; j < inLen && pos + 4 < _countof(text); ++j) {
                    pos += swprintf(text + pos, _countof(text) - pos, L"%02X", e->InputPreview[j]);
                    if ((j & 15) == 15) {
                        StringCchCatW(text, _countof(text), L"\n");
                        pos = wcslen(text);
                    } else {
                        StringCchCatW(text, _countof(text), L" ");
                        pos = wcslen(text);
                    }
                }
                StringCchCatW(text, _countof(text), L"\n\nOutput preview:\n");
                pos = wcslen(text);
                for (j = 0; j < outLen && pos + 4 < _countof(text); ++j) {
                    pos += swprintf(text + pos, _countof(text) - pos, L"%02X", e->OutputPreview[j]);
                    if ((j & 15) == 15) {
                        StringCchCatW(text, _countof(text), L"\n");
                        pos = wcslen(text);
                    } else {
                        StringCchCatW(text, _countof(text), L" ");
                        pos = wcslen(text);
                    }
                }
            }

            MessageBoxW(g_main, text, L"IOCTL details", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
}

static void set_control_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
}

static HWND make_button(
    HWND parent,
    int id,
    const wchar_t* text,
    DWORD style)
{
    HWND control = CreateWindowExW(
        0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        0, 0, 100, 24,
        parent, (HMENU)(INT_PTR)id, g_instance, NULL);
    set_control_font(control);
    return control;
}

static void add_list_column(
    HWND list,
    int index,
    int width,
    const wchar_t* text)
{
    LVCOLUMNW column;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = index;
    column.cx = width;
    column.pszText = (LPWSTR)text;
    ListView_InsertColumn(list, index, &column);
}

static void create_ui(HWND hwnd)
{
    DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL;

    g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    g_status = CreateWindowExW(
        0, L"STATIC", L"Disconnected",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 100, 22, hwnd, (HMENU)ID_STATUS, g_instance, NULL);
    set_control_font(g_status);

    g_baselineStatus = CreateWindowExW(
        0, L"STATIC", L"No idle baseline learned.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 100, 22, hwnd, (HMENU)ID_BASELINE_STATUS, g_instance, NULL);
    set_control_font(g_baselineStatus);

    CreateWindowExW(
        0, L"STATIC", L"Filter:",
        WS_CHILD | WS_VISIBLE,
        0, 0, 50, 22, hwnd, NULL, g_instance, NULL);

    g_filter = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 180, 24, hwnd, (HMENU)ID_FILTER, g_instance, NULL);
    set_control_font(g_filter);

    g_hideKnown = make_button(
        hwnd, ID_HIDE_KNOWN, L"Hide known",
        BS_AUTOCHECKBOX);
    g_hideBaseline = make_button(
        hwnd, ID_HIDE_BASELINE, L"Hide idle baseline",
        BS_AUTOCHECKBOX);
    g_errorsOnly = make_button(
        hwnd, ID_ERRORS_ONLY, L"Errors only",
        BS_AUTOCHECKBOX);
    g_pause = make_button(
        hwnd, ID_PAUSE, L"Pause UI",
        BS_AUTOCHECKBOX);

    g_baselineButton = make_button(
        hwnd, ID_BASELINE, L"Learn Idle (5 s)", BS_PUSHBUTTON);

    g_markerEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 240, 24, hwnd, (HMENU)ID_MARKER_EDIT, g_instance, NULL);
    set_control_font(g_markerEdit);

    make_button(hwnd, ID_ADD_MARKER, L"Add Marker", BS_PUSHBUTTON);
    g_actionStart = make_button(
        hwnd, ID_ACTION_START, L"Start Action", BS_PUSHBUTTON);
    g_actionEnd = make_button(
        hwnd, ID_ACTION_END, L"End Action", BS_PUSHBUTTON);
    EnableWindow(g_actionEnd, FALSE);
    make_button(hwnd, ID_SAVE, L"Save Session", BS_PUSHBUTTON);
    make_button(hwnd, ID_CLEAR, L"Clear Session", BS_PUSHBUTTON);

    g_activity = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        listStyle,
        0, 0, 100, 100,
        hwnd, (HMENU)ID_ACTIVITY, g_instance, NULL);
    set_control_font(g_activity);
    ListView_SetExtendedListViewStyle(
        g_activity,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    add_list_column(g_activity, 0, 42, L"LED");
    add_list_column(g_activity, 1, 100, L"IOCTL");
    add_list_column(g_activity, 2, 300, L"Name / last nested command");
    add_list_column(g_activity, 3, 82, L"Rate");
    add_list_column(g_activity, 4, 90, L"Count");
    add_list_column(g_activity, 5, 135, L"Last status");
    add_list_column(g_activity, 6, 90, L"Confidence");

    g_log = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        listStyle,
        0, 0, 100, 100,
        hwnd, (HMENU)ID_LOG, g_instance, NULL);
    set_control_font(g_log);
    ListView_SetExtendedListViewStyle(
        g_log,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    add_list_column(g_log, 0, 75, L"t [s]");
    add_list_column(g_log, 1, 80, L"Seq");
    add_list_column(g_log, 2, 100, L"IOCTL");
    add_list_column(g_log, 3, 390, L"Name / nested command / marker");
    add_list_column(g_log, 4, 90, L"In / Out");
    add_list_column(g_log, 5, 145, L"Status");
}

static void layout_ui(HWND hwnd)
{
    RECT rc;
    int width;
    int height;
    int margin = 8;
    int y = 8;
    int topControlsHeight = 90;
    int activityHeight;
    int logY;
    int logHeight;
    int x;

    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    MoveWindow(g_status, margin, y, width - 2 * margin, 20, TRUE);
    y += 22;
    MoveWindow(g_baselineStatus, margin, y, width - 2 * margin, 20, TRUE);
    y += 24;

    x = margin;
    {
        HWND filterLabel = FindWindowExW(hwnd, NULL, L"STATIC", L"Filter:");
        if (filterLabel != NULL) {
            MoveWindow(filterLabel, x, y + 3, 42, 20, TRUE);
        }
    }
    x += 44;
    MoveWindow(g_filter, x, y, 170, 24, TRUE);
    x += 178;
    MoveWindow(g_hideKnown, x, y, 100, 24, TRUE);
    x += 104;
    MoveWindow(g_hideBaseline, x, y, 132, 24, TRUE);
    x += 136;
    MoveWindow(g_errorsOnly, x, y, 90, 24, TRUE);
    x += 94;
    MoveWindow(g_pause, x, y, 82, 24, TRUE);
    x += 86;
    MoveWindow(g_baselineButton, x, y, 120, 24, TRUE);

    y += 30;
    x = margin;
    MoveWindow(g_markerEdit, x, y, 260, 24, TRUE);
    x += 268;
    MoveWindow(GetDlgItem(hwnd, ID_ADD_MARKER), x, y, 92, 24, TRUE);
    x += 100;
    MoveWindow(g_actionStart, x, y, 92, 24, TRUE);
    x += 100;
    MoveWindow(g_actionEnd, x, y, 92, 24, TRUE);
    x += 100;
    MoveWindow(GetDlgItem(hwnd, ID_SAVE), x, y, 100, 24, TRUE);
    x += 108;
    MoveWindow(GetDlgItem(hwnd, ID_CLEAR), x, y, 104, 24, TRUE);

    y = topControlsHeight + margin;
    activityHeight = (height - y - margin) * 42 / 100;
    if (activityHeight < 160) activityHeight = 160;
    logY = y + activityHeight + 6;
    logHeight = height - logY - margin;
    if (logHeight < 120) logHeight = 120;

    MoveWindow(g_activity, margin, y, width - 2 * margin, activityHeight, TRUE);
    MoveWindow(g_log, margin, logY, width - 2 * margin, logHeight, TRUE);
}

static void handle_trace_batch(TRACE_BATCH* batch)
{
    DWORD i;
    if (batch == NULL) {
        return;
    }
    g_dropped += batch->Dropped;
    for (i = 0; i < batch->Count; ++i) {
        record_trace(&batch->Entry[i]);
    }
    HeapFree(GetProcessHeap(), 0, batch);
    update_status_text();
}

static LRESULT handle_activity_custom_draw(NMLVCUSTOMDRAW* draw)
{
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }
    if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        unsigned index = (unsigned)draw->nmcd.lItemlParam;
        LARGE_INTEGER now;
        if (index < g_statCount) {
            IOCTL_STAT* stat = &g_stats[index];
            QueryPerformanceCounter(&now);
            if (stat->LastSeenQpc != 0 &&
                qpc_seconds(now.QuadPart - stat->LastSeenQpc) <= ACTIVITY_HOLD_SECONDS) {
                draw->clrText = RGB(0, 130, 0);
            } else if (stat->Count != 0 && !nt_success(stat->LastStatus)) {
                draw->clrText = RGB(180, 0, 0);
            }
        }
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

static void stop_worker(void)
{
    if (g_stopEvent != NULL) {
        SetEvent(g_stopEvent);
    }
    if (g_worker != NULL) {
        WaitForSingleObject(g_worker, 3000);
        CloseHandle(g_worker);
        g_worker = NULL;
    }
    if (g_stopEvent != NULL) {
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
    }
}

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        create_ui(hwnd);
        layout_ui(hwnd);
        initialize_stats();
        {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            g_sessionStartQpc = now.QuadPart;
        }
        refresh_activity();
        SetTimer(hwnd, ID_TIMER_UI, 250, NULL);
        g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (g_stopEvent != NULL) {
            g_worker = CreateThread(NULL, 0, trace_worker, hwnd, 0, NULL);
        }
        return 0;

    case WM_SIZE:
        layout_ui(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER_UI) {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            update_rates(now.QuadPart);
            if (g_baselineActive &&
                qpc_seconds(now.QuadPart - g_baselineStartQpc) >= BASELINE_SECONDS) {
                finish_baseline(now.QuadPart);
            }
            refresh_activity();
            update_status_text();
        }
        return 0;

    case WM_APP_TRACE_BATCH:
        handle_trace_batch((TRACE_BATCH*)lParam);
        return 0;

    case WM_APP_TRACE_STATE:
        g_connected = (BOOL)wParam;
        g_connectionError = (DWORD)lParam;
        update_status_text();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_FILTER:
            if (HIWORD(wParam) == EN_CHANGE) {
                refresh_activity();
                rebuild_log();
            }
            return 0;
        case ID_HIDE_KNOWN:
        case ID_HIDE_BASELINE:
        case ID_ERRORS_ONLY:
            refresh_activity();
            rebuild_log();
            return 0;
        case ID_PAUSE:
            if (!checked(g_pause)) {
                rebuild_log();
            }
            return 0;
        case ID_BASELINE:
            begin_baseline();
            return 0;
        case ID_ADD_MARKER:
            add_user_marker();
            return 0;
        case ID_ACTION_START:
            start_action();
            return 0;
        case ID_ACTION_END:
            end_action();
            return 0;
        case ID_SAVE:
            save_session();
            return 0;
        case ID_CLEAR:
            clear_session();
            return 0;
        }
        break;

    case WM_NOTIFY:
        {
            NMHDR* hdr = (NMHDR*)lParam;
            if (hdr->hwndFrom == g_activity && hdr->code == NM_CUSTOMDRAW) {
                return handle_activity_custom_draw((NMLVCUSTOMDRAW*)lParam);
            }
            if (hdr->hwndFrom == g_log && hdr->code == NM_DBLCLK) {
                LPNMITEMACTIVATE activate = (LPNMITEMACTIVATE)lParam;
                if (activate->iItem >= 0) {
                    LVITEMW item;
                    ZeroMemory(&item, sizeof(item));
                    item.mask = LVIF_PARAM;
                    item.iItem = activate->iItem;
                    if (ListView_GetItem(g_log, &item)) {
                        show_trace_details((uint64_t)item.lParam);
                    }
                }
                return 0;
            }
        }
        break;

    case WM_CLOSE:
        stop_worker();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_UI);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous,
    PWSTR commandLine,
    int showCommand)
{
    WNDCLASSEXW wc;
    HWND hwnd;
    MSG msg;
    INITCOMMONCONTROLSEX icc;

    UNREFERENCED_PARAMETER(previous);
    UNREFERENCED_PARAMETER(commandLine);

    g_instance = instance;
    QueryPerformanceFrequency(&g_qpcFrequency);

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    SetProcessDPIAware();

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LecWatchMainWindow";
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"LeCroy WR6k Live IOCTL Monitor - lecwatch",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1180,
        800,
        NULL,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL) {
        return 1;
    }

    g_main = hwnd;
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN &&
            msg.wParam == VK_RETURN &&
            GetFocus() == g_markerEdit) {
            add_user_marker();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    stop_worker();
    return (int)msg.wParam;
}
