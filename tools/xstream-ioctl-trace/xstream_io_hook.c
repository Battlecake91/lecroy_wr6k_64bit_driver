#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>

#if defined(_M_IX86)
#pragma comment(linker, "/EXPORT:InitializeXStreamTrace=_InitializeXStreamTrace@4")
#endif

#define TRACE_ENV_NAME L"LECS65_TRACE_PATH"
#define TRACE_CAPTURE_LIMIT (1024U * 1024U)

typedef BOOL (WINAPI *PFN_DeviceIoControl)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
typedef HANDLE (WINAPI *PFN_CreateFileW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
typedef HANDLE (WINAPI *PFN_CreateFileA)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
typedef BOOL (WINAPI *PFN_CloseHandle)(HANDLE);
typedef HMODULE (WINAPI *PFN_LoadLibraryW)(LPCWSTR);
typedef HMODULE (WINAPI *PFN_LoadLibraryA)(LPCSTR);
typedef FARPROC (WINAPI *PFN_GetProcAddress)(HMODULE,LPCSTR);
typedef struct LECS65_IO_STATUS_BLOCK {
    union {
        LONG Status;
        PVOID Pointer;
    } u;
    ULONG_PTR Information;
} LECS65_IO_STATUS_BLOCK, *PLECS65_IO_STATUS_BLOCK;


typedef struct LECS65_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} LECS65_UNICODE_STRING, *PLECS65_UNICODE_STRING;

typedef struct LECS65_OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PLECS65_UNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} LECS65_OBJECT_ATTRIBUTES, *PLECS65_OBJECT_ATTRIBUTES;

typedef LONG (NTAPI *PFN_NtCreateFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    PLECS65_OBJECT_ATTRIBUTES ObjectAttributes,
    PLECS65_IO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength);

typedef LONG (NTAPI *PFN_NtDeviceIoControlFile)(
    HANDLE FileHandle,
    HANDLE Event,
    PVOID ApcRoutine,
    PVOID ApcContext,
    PLECS65_IO_STATUS_BLOCK IoStatusBlock,
    ULONG IoControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength);

static PFN_DeviceIoControl g_realDeviceIoControl;
static PFN_CreateFileW g_realCreateFileW;
static PFN_CreateFileA g_realCreateFileA;
static PFN_CloseHandle g_realCloseHandle;
static PFN_LoadLibraryW g_realLoadLibraryW;
static PFN_LoadLibraryA g_realLoadLibraryA;
static PFN_GetProcAddress g_realGetProcAddress;
static PFN_NtDeviceIoControlFile g_realNtDeviceIoControlFile;
static PFN_NtCreateFile g_realNtCreateFile;

static HMODULE g_self;
static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logLock;
static volatile LONG g_sequence;
static LARGE_INTEGER g_qpcFrequency;
static volatile LONG g_initialized;
static volatile LONG g_recordsSinceFlush;

static BOOL WINAPI HookDeviceIoControl(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static HANDLE WINAPI HookCreateFileW(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static HANDLE WINAPI HookCreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static BOOL WINAPI HookCloseHandle(HANDLE);
static HMODULE WINAPI HookLoadLibraryW(LPCWSTR);
static HMODULE WINAPI HookLoadLibraryA(LPCSTR);
static FARPROC WINAPI HookGetProcAddress(HMODULE,LPCSTR);
static void write_text(const char* s);
static void maybe_flush_log(void);
static void write_u32(DWORD value);
static void write_u64(ULONGLONG value);
static void write_hex_u32(DWORD value);
static void write_hex_ptr(const void* value);
static void write_json_string(const char* s);
static char* copy_object_name_utf8(
    const LECS65_OBJECT_ATTRIBUTES* attributes);
static ULONG_PTR safe_read_information(
    const LECS65_IO_STATUS_BLOCK* p);
static LONG NTAPI HookNtDeviceIoControlFile(
    HANDLE,HANDLE,PVOID,PVOID,PLECS65_IO_STATUS_BLOCK,ULONG,
    PVOID,ULONG,PVOID,ULONG);
static LONG NTAPI HookNtCreateFile(
    PHANDLE fileHandle,
    ACCESS_MASK desiredAccess,
    PLECS65_OBJECT_ATTRIBUTES objectAttributes,
    PLECS65_IO_STATUS_BLOCK ioStatus,
    PLARGE_INTEGER allocationSize,
    ULONG fileAttributes,
    ULONG shareAccess,
    ULONG createDisposition,
    ULONG createOptions,
    PVOID eaBuffer,
    ULONG eaLength)
{
    char* objectName = copy_object_name_utf8(objectAttributes);
    LONG status;
    HANDLE result = NULL;
    ULONG_PTR information = 0;
    LONG seq;

    status = g_realNtCreateFile(
        fileHandle,desiredAccess,objectAttributes,ioStatus,allocationSize,
        fileAttributes,shareAccess,createDisposition,createOptions,
        eaBuffer,eaLength);

    __try {
        if (fileHandle) result = *fileHandle;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        result = NULL;
    }
    information = safe_read_information(ioStatus);

    seq = InterlockedIncrement(&g_sequence);
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"nt_create_file\",\"seq\":"); write_u32((DWORD)seq);
    write_text(",\"tid\":"); write_u32(GetCurrentThreadId());
    write_text(",\"path\":"); write_json_string(objectName ? objectName : "");
    write_text(",\"handle\":\""); write_hex_ptr(result);
    write_text("\",\"ntstatus\":\""); write_hex_u32((DWORD)status);
    write_text("\",\"information\":"); write_u64((ULONGLONG)information);
    write_text(",\"desired_access\":\""); write_hex_u32((DWORD)desiredAccess);
    write_text("\",\"share_access\":\""); write_hex_u32(shareAccess);
    write_text("\",\"create_disposition\":"); write_u32(createDisposition);
    write_text(",\"create_options\":\""); write_hex_u32(createOptions);
    write_text("\"}\r\n");
    maybe_flush_log();
    LeaveCriticalSection(&g_logLock);

    if (objectName) HeapFree(GetProcessHeap(),0,objectName);
    return status;
}

static DWORD text_len(const char* s)
{
    const char* p = s;
    if (!s) return 0;
    while (*p) ++p;
    return (DWORD)(p - s);
}

static void write_raw(const char* s, DWORD n)
{
    DWORD written;
    if (g_log == INVALID_HANDLE_VALUE || !s || !n) return;
    WriteFile(g_log, s, n, &written, NULL);
}

static void write_text(const char* s)
{
    write_raw(s, text_len(s));
}

static void maybe_flush_log(void)
{
    /*
     * Avoid flushing the filesystem cache after every traced call. That
     * perturbs timing-sensitive legacy software far more than the hook itself.
     * Periodic flushes still bound data loss if XStream crashes.
     */
    if (InterlockedIncrement(&g_recordsSinceFlush) >= 256) {
        InterlockedExchange(&g_recordsSinceFlush, 0);
        FlushFileBuffers(g_log);
    }
}

static void write_u32(DWORD value)
{
    char buf[16];
    char* p = buf + sizeof(buf);
    *--p = 0;
    do {
        *--p = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    write_text(p);
}

static void write_u64(ULONGLONG value)
{
    char buf[32];
    char* p = buf + sizeof(buf);
    *--p = 0;
    do {
        *--p = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    write_text(p);
}

static void write_hex_u32(DWORD value)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[10];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i)
        buf[2+i] = hex[(value >> ((7-i)*4)) & 0xF];
    write_raw(buf, sizeof(buf));
}

static void write_hex_ptr(const void* value)
{
    static const char hex[] = "0123456789ABCDEF";
    uintptr_t v = (uintptr_t)value;
    char buf[2 + sizeof(uintptr_t)*2];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < (int)(sizeof(uintptr_t)*2); ++i) {
        int shift = ((int)(sizeof(uintptr_t)*2)-1-i)*4;
        buf[2+i] = hex[(v >> shift) & 0xF];
    }
    write_raw(buf, sizeof(buf));
}

static void write_hex_bytes(const BYTE* data, DWORD length)
{
    static const char hex[] = "0123456789ABCDEF";
    char pair[2];
    DWORD i;
    write_raw("\"", 1);
    if (data) {
        for (i = 0; i < length; ++i) {
            pair[0] = hex[(data[i] >> 4) & 0xF];
            pair[1] = hex[data[i] & 0xF];
            write_raw(pair, 2);
        }
    }
    write_raw("\"", 1);
}

static void write_json_string(const char* s)
{
    const unsigned char* p = (const unsigned char*)s;
    write_raw("\"", 1);
    if (p) {
        while (*p) {
            unsigned char c = *p++;
            if (c == '"' || c == '\\') {
                char pair[2] = {'\\',(char)c};
                write_raw(pair, 2);
            } else if (c == '\r') {
                write_raw("\\r", 2);
            } else if (c == '\n') {
                write_raw("\\n", 2);
            } else if (c == '\t') {
                write_raw("\\t", 2);
            } else if (c >= 0x20) {
                write_raw((const char*)&c, 1);
            }
        }
    }
    write_raw("\"", 1);
}

static char* wide_to_utf8(LPCWSTR value)
{
    int count;
    char* out;
    if (!value) return NULL;
    count = WideCharToMultiByte(CP_UTF8,0,value,-1,NULL,0,NULL,NULL);
    if (count <= 0) return NULL;
    out = (char*)HeapAlloc(GetProcessHeap(),0,(SIZE_T)count);
    if (!out) return NULL;
    if (WideCharToMultiByte(CP_UTF8,0,value,-1,out,count,NULL,NULL) <= 0) {
        HeapFree(GetProcessHeap(),0,out);
        return NULL;
    }
    return out;
}

static char* copy_object_name_utf8(const LECS65_OBJECT_ATTRIBUTES* attributes)
{
    LECS65_UNICODE_STRING name;
    WCHAR* copy = NULL;
    char* utf8 = NULL;
    SIZE_T chars;

    if (!attributes) return NULL;

    __try {
        if (!attributes->ObjectName) return NULL;
        name = *attributes->ObjectName;
        if (!name.Buffer || name.Length == 0) return NULL;
        chars = name.Length / sizeof(WCHAR);
        copy = (WCHAR*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY,
            (chars + 1) * sizeof(WCHAR));
        if (!copy) return NULL;
        CopyMemory(copy, name.Buffer, name.Length);
        copy[chars] = L'\0';
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (copy) HeapFree(GetProcessHeap(), 0, copy);
        return NULL;
    }

    utf8 = wide_to_utf8(copy);
    HeapFree(GetProcessHeap(), 0, copy);
    return utf8;
}


static BYTE* safe_copy(const void* src, DWORD requested, DWORD* copied)
{
    BYTE* out;
    DWORD amount = requested;
    *copied = 0;
    if (!src || !requested) return NULL;
    if (amount > TRACE_CAPTURE_LIMIT) amount = TRACE_CAPTURE_LIMIT;
    out = (BYTE*)HeapAlloc(GetProcessHeap(),0,amount);
    if (!out) return NULL;
    __try {
        CopyMemory(out,src,amount);
        *copied = amount;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        HeapFree(GetProcessHeap(),0,out);
        return NULL;
    }
    return out;
}

static DWORD safe_read_dword(const DWORD* p)
{
    DWORD value = 0;
    if (!p) return 0;
    __try { value = *p; }
    __except(EXCEPTION_EXECUTE_HANDLER) { value = 0; }
    return value;
}

static ULONG_PTR safe_read_information(const LECS65_IO_STATUS_BLOCK* p)
{
    ULONG_PTR value = 0;
    if (!p) return 0;
    __try { value = p->Information; }
    __except(EXCEPTION_EXECUTE_HANDLER) { value = 0; }
    return value;
}

static void log_header(void)
{
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"legacy_xstream_user_trace\",\"format_version\":3,\"pid\":");
    write_u32(GetCurrentProcessId());
    write_text(",\"capture_limit\":");
    write_u32(TRACE_CAPTURE_LIMIT);
    write_text("}\r\n");
    FlushFileBuffers(g_log);
    LeaveCriticalSection(&g_logLock);
}

static void log_create(const char* api,const char* path,HANDLE result,
    DWORD access,DWORD share,DWORD creation,DWORD flags,DWORD error)
{
    LONG seq = InterlockedIncrement(&g_sequence);
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"create_file\",\"seq\":"); write_u32((DWORD)seq);
    write_text(",\"tid\":"); write_u32(GetCurrentThreadId());
    write_text(",\"api\":"); write_json_string(api);
    write_text(",\"path\":"); write_json_string(path ? path : "");
    write_text(",\"handle\":\""); write_hex_ptr(result);
    write_text("\",\"desired_access\":\""); write_hex_u32(access);
    write_text("\",\"share_mode\":\""); write_hex_u32(share);
    write_text("\",\"creation_disposition\":"); write_u32(creation);
    write_text(",\"flags\":\""); write_hex_u32(flags);
    write_text("\",\"last_error\":"); write_u32(error);
    write_text("}\r\n");
    maybe_flush_log();
    LeaveCriticalSection(&g_logLock);
}

static void log_close(HANDLE handle, BOOL result, DWORD error)
{
    LONG seq = InterlockedIncrement(&g_sequence);
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"close_handle\",\"seq\":"); write_u32((DWORD)seq);
    write_text(",\"tid\":"); write_u32(GetCurrentThreadId());
    write_text(",\"handle\":\""); write_hex_ptr(handle);
    write_text("\",\"success\":"); write_text(result ? "true" : "false");
    write_text(",\"last_error\":"); write_u32(error);
    write_text("}\r\n");
    maybe_flush_log();
    LeaveCriticalSection(&g_logLock);
}

static BOOL patch_pointer(void** slot, void* replacement)
{
    DWORD oldProtect;
    if (*slot == replacement) return TRUE;
    if (!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&oldProtect)) return FALSE;
    *slot = replacement;
    FlushInstructionCache(GetCurrentProcess(),slot,sizeof(void*));
    VirtualProtect(slot,sizeof(void*),oldProtect,&oldProtect);
    return TRUE;
}

static void patch_module(HMODULE module)
{
    BYTE* base = (BYTE*)module;
    IMAGE_DOS_HEADER* dos;
    IMAGE_NT_HEADERS32* nt;
    IMAGE_DATA_DIRECTORY dir;
    IMAGE_IMPORT_DESCRIPTOR* imp;

    if (!module || module == g_self) return;

    __try {
        dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        nt = (IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return;
        dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress || !dir.Size) return;
        imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress);

        while (imp->Name) {
            IMAGE_THUNK_DATA32* thunk =
                (IMAGE_THUNK_DATA32*)(base + imp->FirstThunk);
            while (thunk->u1.Function) {
                void** slot = (void**)&thunk->u1.Function;
                void* value = *slot;
                if (value == (void*)g_realDeviceIoControl) patch_pointer(slot,HookDeviceIoControl);
                else if (value == (void*)g_realCreateFileW) patch_pointer(slot,HookCreateFileW);
                else if (value == (void*)g_realCreateFileA) patch_pointer(slot,HookCreateFileA);
                else if (value == (void*)g_realCloseHandle) patch_pointer(slot,HookCloseHandle);
                else if (value == (void*)g_realLoadLibraryW) patch_pointer(slot,HookLoadLibraryW);
                else if (value == (void*)g_realLoadLibraryA) patch_pointer(slot,HookLoadLibraryA);
                else if (value == (void*)g_realGetProcAddress) patch_pointer(slot,HookGetProcAddress);
                else if (g_realNtDeviceIoControlFile != NULL &&
                         value == (void*)g_realNtDeviceIoControlFile)
                    patch_pointer(slot,HookNtDeviceIoControlFile);
                else if (g_realNtCreateFile != NULL &&
                         value == (void*)g_realNtCreateFile)
                    patch_pointer(slot,HookNtCreateFile);
                ++thunk;
            }
            ++imp;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

static void patch_all_modules(void)
{
    HANDLE snapshot;
    MODULEENTRY32W entry;
    snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return;
    ZeroMemory(&entry,sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot,&entry)) {
        do { patch_module(entry.hModule); }
        while (Module32NextW(snapshot,&entry));
    }
    g_realCloseHandle(snapshot);
}

static BOOL WINAPI HookDeviceIoControl(
    HANDLE hDevice,DWORD code,LPVOID inBuffer,DWORD inSize,
    LPVOID outBuffer,DWORD outSize,LPDWORD bytesOut,LPOVERLAPPED overlapped)
{
    BYTE* inCopy;
    BYTE* outCopy = NULL;
    DWORD inCopied = 0, outCopied = 0;
    DWORD returned, error;
    BOOL result;
    LARGE_INTEGER start,end;
    ULONGLONG durationUs = 0;
    LONG seq;

    inCopy = safe_copy(inBuffer,inSize,&inCopied);
    QueryPerformanceCounter(&start);
    result = g_realDeviceIoControl(
        hDevice,code,inBuffer,inSize,outBuffer,outSize,bytesOut,overlapped);
    error = GetLastError();
    QueryPerformanceCounter(&end);
    returned = safe_read_dword(bytesOut);

    if (g_qpcFrequency.QuadPart)
        durationUs = (ULONGLONG)(((end.QuadPart-start.QuadPart)*1000000LL)/
            g_qpcFrequency.QuadPart);

    if (outBuffer && returned) {
        DWORD wanted = returned < outSize ? returned : outSize;
        outCopy = safe_copy(outBuffer,wanted,&outCopied);
    }

    seq = InterlockedIncrement(&g_sequence);
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"ioctl\",\"seq\":"); write_u32((DWORD)seq);
    write_text(",\"tid\":"); write_u32(GetCurrentThreadId());
    write_text(",\"qpc\":"); write_u64((ULONGLONG)start.QuadPart);
    write_text(",\"duration_us\":"); write_u64(durationUs);
    write_text(",\"handle\":\""); write_hex_ptr(hDevice);
    write_text("\",\"ioctl\":\""); write_hex_u32(code);
    write_text("\",\"method\":"); write_u32(code & 3U);
    write_text(",\"input_length\":"); write_u32(inSize);
    write_text(",\"output_length\":"); write_u32(outSize);
    write_text(",\"success\":"); write_text(result ? "true" : "false");
    write_text(",\"last_error\":"); write_u32(error);
    write_text(",\"bytes_returned\":"); write_u32(returned);
    write_text(",\"overlapped\":\""); write_hex_ptr(overlapped);
    write_text("\",\"pending\":");
    write_text((!result && error == ERROR_IO_PENDING) ? "true" : "false");
    write_text(",\"input_captured\":"); write_u32(inCopied);
    write_text(",\"input_truncated\":"); write_text(inSize > inCopied ? "true" : "false");
    write_text(",\"input_hex\":"); write_hex_bytes(inCopy,inCopied);
    write_text(",\"output_captured\":"); write_u32(outCopied);
    write_text(",\"output_truncated\":"); write_text(returned > outCopied ? "true" : "false");
    write_text(",\"output_hex\":"); write_hex_bytes(outCopy,outCopied);
    write_text("}\r\n");
    maybe_flush_log();
    LeaveCriticalSection(&g_logLock);

    if (inCopy) HeapFree(GetProcessHeap(),0,inCopy);
    if (outCopy) HeapFree(GetProcessHeap(),0,outCopy);
    SetLastError(error);
    return result;
}

static LONG NTAPI HookNtDeviceIoControlFile(
    HANDLE fileHandle,
    HANDLE eventHandle,
    PVOID apcRoutine,
    PVOID apcContext,
    PLECS65_IO_STATUS_BLOCK ioStatus,
    ULONG code,
    PVOID inBuffer,
    ULONG inSize,
    PVOID outBuffer,
    ULONG outSize)
{
    const LONG STATUS_PENDING_VALUE = 0x00000103L;
    BYTE* inCopy;
    BYTE* outCopy = NULL;
    DWORD inCopied = 0;
    DWORD outCopied = 0;
    ULONG_PTR information;
    LONG status;
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    ULONGLONG durationUs = 0;
    LONG seq;

    inCopy = safe_copy(inBuffer,inSize,&inCopied);

    QueryPerformanceCounter(&start);
    status = g_realNtDeviceIoControlFile(
        fileHandle,eventHandle,apcRoutine,apcContext,ioStatus,code,
        inBuffer,inSize,outBuffer,outSize);
    QueryPerformanceCounter(&end);

    information = safe_read_information(ioStatus);

    if (g_qpcFrequency.QuadPart)
        durationUs = (ULONGLONG)(
            ((end.QuadPart-start.QuadPart)*1000000LL) /
            g_qpcFrequency.QuadPart);

    if (status != STATUS_PENDING_VALUE && outBuffer && information) {
        ULONG_PTR wanted = information;
        if (wanted > outSize) wanted = outSize;
        if (wanted > 0xFFFFFFFFUL) wanted = 0xFFFFFFFFUL;
        outCopy = safe_copy(outBuffer,(DWORD)wanted,&outCopied);
    }

    seq = InterlockedIncrement(&g_sequence);
    EnterCriticalSection(&g_logLock);
    write_text("{\"type\":\"nt_ioctl\",\"seq\":"); write_u32((DWORD)seq);
    write_text(",\"tid\":"); write_u32(GetCurrentThreadId());
    write_text(",\"qpc\":"); write_u64((ULONGLONG)start.QuadPart);
    write_text(",\"duration_us\":"); write_u64(durationUs);
    write_text(",\"handle\":\""); write_hex_ptr(fileHandle);
    write_text("\",\"ioctl\":\""); write_hex_u32(code);
    write_text("\",\"method\":"); write_u32(code & 3U);
    write_text(",\"input_length\":"); write_u32(inSize);
    write_text(",\"output_length\":"); write_u32(outSize);
    write_text(",\"ntstatus\":\""); write_hex_u32((DWORD)status);
    write_text("\",\"information\":"); write_u64((ULONGLONG)information);
    write_text(",\"event\":\""); write_hex_ptr(eventHandle);
    write_text("\",\"apc_routine\":\""); write_hex_ptr(apcRoutine);
    write_text("\",\"pending\":");
    write_text(status == STATUS_PENDING_VALUE ? "true" : "false");
    write_text(",\"input_captured\":"); write_u32(inCopied);
    write_text(",\"input_truncated\":");
    write_text(inSize > inCopied ? "true" : "false");
    write_text(",\"input_hex\":"); write_hex_bytes(inCopy,inCopied);
    write_text(",\"output_captured\":"); write_u32(outCopied);
    write_text(",\"output_truncated\":");
    write_text(information > outCopied ? "true" : "false");
    write_text(",\"output_hex\":"); write_hex_bytes(outCopy,outCopied);
    write_text("}\r\n");
    maybe_flush_log();
    LeaveCriticalSection(&g_logLock);

    if (inCopy) HeapFree(GetProcessHeap(),0,inCopy);
    if (outCopy) HeapFree(GetProcessHeap(),0,outCopy);

    return status;
}

static HANDLE WINAPI HookCreateFileW(
    LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,
    DWORD creation,DWORD flags,HANDLE templateFile)
{
    HANDLE result;
    DWORD error;
    char* utf8;
    result = g_realCreateFileW(path,access,share,sa,creation,flags,templateFile);
    error = GetLastError();
    utf8 = wide_to_utf8(path);
    log_create("CreateFileW",utf8 ? utf8 : "",result,access,share,creation,flags,error);
    if (utf8) HeapFree(GetProcessHeap(),0,utf8);
    SetLastError(error);
    return result;
}

static HANDLE WINAPI HookCreateFileA(
    LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,
    DWORD creation,DWORD flags,HANDLE templateFile)
{
    HANDLE result;
    DWORD error;
    result = g_realCreateFileA(path,access,share,sa,creation,flags,templateFile);
    error = GetLastError();
    log_create("CreateFileA",path ? path : "",result,access,share,creation,flags,error);
    SetLastError(error);
    return result;
}

static BOOL WINAPI HookCloseHandle(HANDLE handle)
{
    BOOL result;
    DWORD error;
    result = g_realCloseHandle(handle);
    error = GetLastError();
    log_close(handle,result,error);
    SetLastError(error);
    return result;
}

static HMODULE WINAPI HookLoadLibraryW(LPCWSTR path)
{
    HMODULE module = g_realLoadLibraryW(path);
    if (module) patch_module(module);
    return module;
}

static HMODULE WINAPI HookLoadLibraryA(LPCSTR path)
{
    HMODULE module = g_realLoadLibraryA(path);
    if (module) patch_module(module);
    return module;
}

static FARPROC WINAPI HookGetProcAddress(HMODULE module,LPCSTR name)
{
    FARPROC p = g_realGetProcAddress(module,name);
    if (!name || ((uintptr_t)name >> 16) == 0) return p;
    if (lstrcmpA(name,"DeviceIoControl") == 0) return (FARPROC)HookDeviceIoControl;
    if (lstrcmpA(name,"CreateFileW") == 0) return (FARPROC)HookCreateFileW;
    if (lstrcmpA(name,"CreateFileA") == 0) return (FARPROC)HookCreateFileA;
    if (lstrcmpA(name,"CloseHandle") == 0) return (FARPROC)HookCloseHandle;
    if (lstrcmpA(name,"LoadLibraryW") == 0) return (FARPROC)HookLoadLibraryW;
    if (lstrcmpA(name,"LoadLibraryA") == 0) return (FARPROC)HookLoadLibraryA;
    if (lstrcmpA(name,"GetProcAddress") == 0) return (FARPROC)HookGetProcAddress;
    if (lstrcmpA(name,"NtDeviceIoControlFile") == 0)
        return (FARPROC)HookNtDeviceIoControlFile;
    if (lstrcmpA(name,"NtCreateFile") == 0)
        return (FARPROC)HookNtCreateFile;
    return p;
}

__declspec(dllexport) DWORD WINAPI InitializeXStreamTrace(LPVOID unused)
{
    WCHAR path[MAX_PATH*4];
    UNREFERENCED_PARAMETER(unused);

    if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0)
        return 1;
    HMODULE kernel32;
    DWORD n;

    InitializeCriticalSection(&g_logLock);
    QueryPerformanceFrequency(&g_qpcFrequency);

    kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return 0;
    g_realGetProcAddress = (PFN_GetProcAddress)GetProcAddress(kernel32,"GetProcAddress");
    if (!g_realGetProcAddress) return 0;

    g_realDeviceIoControl = (PFN_DeviceIoControl)g_realGetProcAddress(kernel32,"DeviceIoControl");
    g_realCreateFileW = (PFN_CreateFileW)g_realGetProcAddress(kernel32,"CreateFileW");
    g_realCreateFileA = (PFN_CreateFileA)g_realGetProcAddress(kernel32,"CreateFileA");
    g_realCloseHandle = (PFN_CloseHandle)g_realGetProcAddress(kernel32,"CloseHandle");
    g_realLoadLibraryW = (PFN_LoadLibraryW)g_realGetProcAddress(kernel32,"LoadLibraryW");
    g_realLoadLibraryA = (PFN_LoadLibraryA)g_realGetProcAddress(kernel32,"LoadLibraryA");

    {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll != NULL) {
            g_realNtDeviceIoControlFile =
                (PFN_NtDeviceIoControlFile)g_realGetProcAddress(
                    ntdll,"NtDeviceIoControlFile");
            g_realNtCreateFile =
                (PFN_NtCreateFile)g_realGetProcAddress(
                    ntdll,"NtCreateFile");
        }
    }

    if (!g_realDeviceIoControl || !g_realCreateFileW || !g_realCreateFileA ||
        !g_realCloseHandle || !g_realLoadLibraryW || !g_realLoadLibraryA ||
        !g_realNtDeviceIoControlFile || !g_realNtCreateFile)
        return 0;

    n = GetEnvironmentVariableW(TRACE_ENV_NAME,path,(DWORD)(sizeof(path)/sizeof(path[0])));
    if (!n || n >= (DWORD)(sizeof(path)/sizeof(path[0])))
        lstrcpyW(path,L"legacy_xstream_trace.jsonl");

    g_log = g_realCreateFileW(
        path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,NULL);
    if (g_log == INVALID_HANDLE_VALUE) return 0;

    log_header();
    patch_all_modules();
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_ATTACH) {
        /*
         * Keep DllMain loader-lock safe. The launcher explicitly invokes
         * InitializeXStreamTrace in a second remote thread after LoadLibraryW
         * has returned, before XStream's primary thread is resumed.
         */
        g_self = instance;
        DisableThreadLibraryCalls(instance);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_initialized != 0) {
            if (g_log != INVALID_HANDLE_VALUE) {
                FlushFileBuffers(g_log);
                if (g_realCloseHandle) g_realCloseHandle(g_log);
                g_log = INVALID_HANDLE_VALUE;
            }
            DeleteCriticalSection(&g_logLock);
        }
    }

    return TRUE;
}
