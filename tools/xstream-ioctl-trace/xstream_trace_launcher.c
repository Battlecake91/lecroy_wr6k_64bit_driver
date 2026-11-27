#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_ENV_NAME L"LECS65_TRACE_PATH"

typedef struct CAPTURE_TIMELINE {
    char processCreateReturnedUtc[32];
    char resumeThreadCalledUtc[32];
    char resumeThreadReturnedUtc[32];
    char targetWaitStartedUtc[32];
    char targetDeadlineUtc[32];
    char targetWaitCompletedUtc[32];
    char terminationWaitCompletedUtc[32];
    char terminationRequestedUtc[32];
    char terminateProcessReturnedUtc[32];
    char targetExitObservedUtc[32];
    char waitResult[48];
    DWORD terminationError;
    int terminationSucceeded;
    int aliveAtTimeout;
} CAPTURE_TIMELINE;

static CAPTURE_TIMELINE g_timeline;

static void print_win32_error(const char* what);

static void format_utc_filetime(const FILETIME* value,char* output,size_t capacity)
{
    SYSTEMTIME systemTime;
    if (!value || !output || !capacity) return;
    if (!FileTimeToSystemTime(value,&systemTime)) {
        output[0] = '\0';
        return;
    }
    _snprintf_s(output,capacity,_TRUNCATE,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        (unsigned)systemTime.wYear,(unsigned)systemTime.wMonth,
        (unsigned)systemTime.wDay,(unsigned)systemTime.wHour,
        (unsigned)systemTime.wMinute,(unsigned)systemTime.wSecond,
        (unsigned)systemTime.wMilliseconds);
}

static void timestamp_now(char* output,size_t capacity)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    format_utc_filetime(&now,output,capacity);
}

static void timestamp_after_seconds(
    const char* start,DWORD seconds,char* output,size_t capacity)
{
    SYSTEMTIME systemTime;
    FILETIME fileTime;
    ULARGE_INTEGER ticks;
    if (!start || !*start || !output || !capacity) return;
    memset(&systemTime,0,sizeof(systemTime));
    if (sscanf_s(start,"%4hu-%2hu-%2huT%2hu:%2hu:%2hu.%3huZ",
            &systemTime.wYear,&systemTime.wMonth,&systemTime.wDay,
            &systemTime.wHour,&systemTime.wMinute,&systemTime.wSecond,
            &systemTime.wMilliseconds) != 7 ||
        !SystemTimeToFileTime(&systemTime,&fileTime)) {
        output[0] = '\0';
        return;
    }
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;
    ticks.QuadPart += (ULONGLONG)seconds * 10000000ULL;
    fileTime.dwLowDateTime = ticks.LowPart;
    fileTime.dwHighDateTime = ticks.HighPart;
    format_utc_filetime(&fileTime,output,capacity);
}

static DWORD terminate_target(
    HANDLE process,DWORD code,DWORD* exitCode)
{
    DWORD waitResult;
    timestamp_now(g_timeline.terminationRequestedUtc,
        sizeof(g_timeline.terminationRequestedUtc));
    g_timeline.terminationSucceeded =
        TerminateProcess(process,code) != 0;
    g_timeline.terminationError = g_timeline.terminationSucceeded ? 0 :
        GetLastError();
    timestamp_now(g_timeline.terminateProcessReturnedUtc,
        sizeof(g_timeline.terminateProcessReturnedUtc));
    waitResult = WaitForSingleObject(process,5000);
    timestamp_now(g_timeline.terminationWaitCompletedUtc,
        sizeof(g_timeline.terminationWaitCompletedUtc));
    if (waitResult == WAIT_OBJECT_0) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_OBJECT_0_AFTER_TERMINATION");
        timestamp_now(g_timeline.targetExitObservedUtc,
            sizeof(g_timeline.targetExitObservedUtc));
        if (exitCode) GetExitCodeProcess(process,exitCode);
    } else if (waitResult == WAIT_TIMEOUT) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_TIMEOUT_AFTER_TERMINATION");
    } else if (waitResult == WAIT_FAILED) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_FAILED_AFTER_TERMINATION");
    }
    return waitResult;
}

static int make_result_path(const WCHAR* tracePath,WCHAR* resultPath,DWORD capacity)
{
    int count = _snwprintf_s(
        resultPath,capacity,_TRUNCATE,L"%s.capture-result.json",tracePath);
    return count >= 0 && (DWORD)count < capacity;
}

static int write_capture_result(
    const WCHAR* tracePath,const WCHAR* resultPath,DWORD pid,DWORD exitCode,
    const WCHAR* termination)
{
    WCHAR temporary[MAX_PATH*4];
    HANDLE trace = INVALID_HANDLE_VALUE;
    HANDLE result = INVALID_HANDLE_VALUE;
    LARGE_INTEGER size;
    LARGE_INTEGER offset;
    char tail[2049];
    DWORD readBytes = 0;
    DWORD written = 0;
    char* lastLine;
    char* end;
    int footerPresent = 0;
    int footerWriteFailed = 0;
    int flushOk = 0;
    int launcherTermination = termination &&
        wcsncmp(termination,L"launcher_",8) == 0 &&
        wcsstr(termination,L"_terminated") != NULL;
    char json[4096];
    int jsonLength;

    trace = CreateFileW(
        tracePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (trace != INVALID_HANDLE_VALUE && GetFileSizeEx(trace,&size) &&
        size.QuadPart > 0) {
        ULONGLONG available = (ULONGLONG)size.QuadPart;
        DWORD wanted = available >= sizeof(tail) ?
            (DWORD)sizeof(tail)-1U : (DWORD)available;
        offset.QuadPart = size.QuadPart - wanted;
        if (SetFilePointerEx(trace,offset,NULL,FILE_BEGIN) &&
            ReadFile(trace,tail,wanted,&readBytes,NULL) && readBytes == wanted) {
            tail[readBytes] = '\0';
            end = tail + readBytes;
            while (end > tail && (end[-1] == '\r' || end[-1] == '\n' ||
                   end[-1] == ' ' || end[-1] == '\t')) --end;
            *end = '\0';
            lastLine = strrchr(tail,'\n');
            lastLine = lastLine ? lastLine + 1 : tail;
            footerPresent = strstr(lastLine,"\"type\":\"trace_footer\"") != NULL;
            footerWriteFailed = footerPresent &&
                strstr(lastLine,"\"write_failed\":true") != NULL;
        }
        flushOk = FlushFileBuffers(trace) != 0;
    }
#if defined(LECS65_TRACE_TESTING)
    {
        WCHAR forced[8];
        if (GetEnvironmentVariableW(
                L"LECS65_TEST_PARENT_FLUSH_FAILURE",forced,8) == 1 &&
            forced[0] == L'1') flushOk = 0;
    }
#endif
    if (trace != INVALID_HANDLE_VALUE) CloseHandle(trace);

    if (_snwprintf_s(
            temporary,sizeof(temporary)/sizeof(temporary[0]),_TRUNCATE,
            L"%s.tmp.%lu",resultPath,(unsigned long)GetCurrentProcessId()) < 0)
        return 0;
    if (GetFileAttributesW(resultPath) != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(temporary) != INVALID_FILE_ATTRIBUTES)
        return 0;

    jsonLength = _snprintf_s(
        json,sizeof(json),_TRUNCATE,
        "{\"schema\":1,\"target_pid\":%lu,\"target_exit_code\":%lu,"
        "\"termination\":\"%ls\",\"footer_present\":%s,"
        "\"footer_write_failed\":%s,\"parent_flush_succeeded\":%s,"
        "\"capture_classification\":\"%s\","
        "\"process_create_returned_utc\":\"%s\","
        "\"resume_thread_called_utc\":\"%s\","
        "\"resume_thread_returned_utc\":\"%s\","
        "\"target_wait_started_utc\":\"%s\","
        "\"target_deadline_utc\":\"%s\","
        "\"target_wait_completed_utc\":\"%s\","
        "\"termination_wait_completed_utc\":\"%s\","
        "\"wait_result\":\"%s\","
        "\"target_was_alive_at_timeout\":%s,"
        "\"termination_requested_utc\":\"%s\","
        "\"terminate_process_returned_utc\":\"%s\","
        "\"terminate_process_succeeded\":%s,"
        "\"termination_error\":%lu,"
        "\"target_exit_observed_utc\":\"%s\"}\r\n",
        (unsigned long)pid,(unsigned long)exitCode,termination,
        footerPresent ? "true" : "false",
        footerWriteFailed ? "true" : "false",
        flushOk ? "true" : "false",
        footerPresent && !footerWriteFailed && flushOk ? "complete_normal" :
            (launcherTermination ?
                "partial_launcher_terminated" : "unknown_termination"),
        g_timeline.processCreateReturnedUtc,
        g_timeline.resumeThreadCalledUtc,g_timeline.resumeThreadReturnedUtc,
        g_timeline.targetWaitStartedUtc,g_timeline.targetDeadlineUtc,
        g_timeline.targetWaitCompletedUtc,
        g_timeline.terminationWaitCompletedUtc,g_timeline.waitResult,
        g_timeline.aliveAtTimeout ? "true" : "false",
        g_timeline.terminationRequestedUtc,
        g_timeline.terminateProcessReturnedUtc,
        g_timeline.terminationSucceeded ? "true" : "false",
        (unsigned long)g_timeline.terminationError,
        g_timeline.targetExitObservedUtc);
    if (jsonLength < 0 || (size_t)jsonLength >= sizeof(json)) return 0;

    result = CreateFileW(
        temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if (result == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(result,json,(DWORD)jsonLength,&written,NULL) ||
        written != (DWORD)jsonLength || !FlushFileBuffers(result)) {
        CloseHandle(result);
        return 0;
    }
    CloseHandle(result);
    if (!MoveFileExW(temporary,resultPath,MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary);
        return 0;
    }
    return footerPresent && !footerWriteFailed && flushOk;
}

static int file_exists(const WCHAR* path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static int write_ready_marker(const WCHAR* path,DWORD pid,int embedding)
{
    HANDLE file;
    char text[256];
    int count;
    DWORD written = 0;

    if (!path || !*path) return 1;
    file = CreateFileW(
        path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) {
        print_win32_error("CreateFile(ready marker)");
        return 0;
    }
    count = _snprintf_s(
        text,sizeof(text),_TRUNCATE,
        "CaptureReady=true\r\nPid=%lu\r\nEmbedding=%s\r\n",
        (unsigned long)pid,embedding ? "true" : "false");
    if (count < 0 || !WriteFile(file,text,(DWORD)count,&written,NULL) ||
        written != (DWORD)count || !FlushFileBuffers(file)) {
        print_win32_error("WriteFile(ready marker)");
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    return 1;
}

static void print_win32_error(const char* what)
{
    DWORD error = GetLastError();
    char* message = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,error,0,(LPSTR)&message,0,NULL);
    fprintf(stderr,"%s failed: %lu (%s)\n",
        what,(unsigned long)error,message ? message : "unknown error");
    if (message) LocalFree(message);
}

static int make_hook_path(WCHAR* output,DWORD capacity)
{
    DWORD n = GetModuleFileNameW(NULL,output,capacity);
    WCHAR* slash;
    if (!n || n >= capacity) return 0;
    slash = output + n;
    while (slash > output && slash[-1] != L'\\' && slash[-1] != L'/') --slash;
    *slash = L'\0';
    if ((DWORD)(lstrlenW(output)+lstrlenW(L"xstream_io_hook.dll")+1) >= capacity)
        return 0;
    lstrcatW(output,L"xstream_io_hook.dll");
    return 1;
}

static int make_default_trace_path(WCHAR* output,DWORD capacity)
{
    SYSTEMTIME st;
    WCHAR cwd[MAX_PATH*4];
    if (!GetCurrentDirectoryW((DWORD)(sizeof(cwd)/sizeof(cwd[0])),cwd))
        return 0;
    GetLocalTime(&st);
    if (_snwprintf_s(
            output,capacity,_TRUNCATE,
            L"%s\\legacy_xstream_trace_%04u%02u%02u_%02u%02u%02u.jsonl",
            cwd,st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond) < 0)
        return 0;
    return 1;
}

static int inject_dll_and_initialize(
    HANDLE process,
    const WCHAR* dllPath)
{
    SIZE_T bytes = ((SIZE_T)lstrlenW(dllPath)+1)*sizeof(WCHAR);
    LPVOID remote;
    HANDLE thread;
    HMODULE kernel32;
    FARPROC loadLibraryW;
    DWORD remoteBase = 0;
    HMODULE localImage = NULL;
    FARPROC localInit;
    SIZE_T initOffset;
    LPTHREAD_START_ROUTINE remoteInit;
    DWORD initResult = 0;

    remote = VirtualAllocEx(
        process,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!remote) {
        print_win32_error("VirtualAllocEx");
        return 0;
    }

    if (!WriteProcessMemory(process,remote,dllPath,bytes,NULL)) {
        print_win32_error("WriteProcessMemory");
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        return 0;
    }

    kernel32 = GetModuleHandleW(L"kernel32.dll");
    loadLibraryW = kernel32 ? GetProcAddress(kernel32,"LoadLibraryW") : NULL;
    if (!loadLibraryW) {
        print_win32_error("GetProcAddress(LoadLibraryW)");
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        return 0;
    }

    thread = CreateRemoteThread(
        process,NULL,0,(LPTHREAD_START_ROUTINE)loadLibraryW,remote,0,NULL);
    if (!thread) {
        print_win32_error("CreateRemoteThread(LoadLibraryW)");
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        return 0;
    }

    WaitForSingleObject(thread,INFINITE);
    if (!GetExitCodeThread(thread,&remoteBase)) {
        print_win32_error("GetExitCodeThread(LoadLibraryW)");
        remoteBase = 0;
    }

    CloseHandle(thread);
    VirtualFreeEx(process,remote,0,MEM_RELEASE);

    if (!remoteBase) {
        fprintf(stderr,"LoadLibraryW in XStream returned NULL.\n");
        return 0;
    }

    /*
     * Resolve the exported initializer RVA locally without running the DLL's
     * normal dependency initialization. The same RVA is valid in the remote
     * mapping even when ASLR chose a different image base.
     */
    localImage = LoadLibraryExW(
        dllPath,NULL,DONT_RESOLVE_DLL_REFERENCES);
    if (!localImage) {
        print_win32_error("LoadLibraryExW(hook)");
        return 0;
    }

    localInit = GetProcAddress(localImage,"InitializeXStreamTrace");
#if defined(_M_IX86)
    if (!localInit)
        localInit = GetProcAddress(localImage,"_InitializeXStreamTrace@4");
#endif
    if (!localInit) {
        print_win32_error("GetProcAddress(InitializeXStreamTrace)");
        FreeLibrary(localImage);
        return 0;
    }

    initOffset = (SIZE_T)((BYTE*)localInit - (BYTE*)localImage);
    remoteInit = (LPTHREAD_START_ROUTINE)(
        (BYTE*)(ULONG_PTR)remoteBase + initOffset);

    thread = CreateRemoteThread(
        process,NULL,0,remoteInit,NULL,0,NULL);
    if (!thread) {
        print_win32_error("CreateRemoteThread(InitializeXStreamTrace)");
        FreeLibrary(localImage);
        return 0;
    }

    WaitForSingleObject(thread,INFINITE);
    if (!GetExitCodeThread(thread,&initResult)) {
        print_win32_error("GetExitCodeThread(InitializeXStreamTrace)");
        initResult = 0;
    }

    CloseHandle(thread);
    FreeLibrary(localImage);

    if (!initResult) {
        fprintf(stderr,"InitializeXStreamTrace in XStream failed.\n");
        return 0;
    }

    return 1;
}

int wmain(int argc,WCHAR** argv)
{
    WCHAR hookPath[MAX_PATH*4];
    WCHAR tracePath[MAX_PATH*4];
    WCHAR resultPath[MAX_PATH*4];
    WCHAR commandLine[32768];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exitCode = 0;
    DWORD timeoutSeconds = 0;
    DWORD waitResult;
    DWORD terminationCode;
    int embedding = 0;
    int launcherTerminated = 0;
    int timeoutTermination = 0;
    int argumentIndex = 1;
    const WCHAR* executable;
    const WCHAR* readyPath = NULL;

    while (argumentIndex < argc) {
        if (lstrcmpiW(argv[argumentIndex],L"--embedding") == 0) {
            embedding = 1;
            argumentIndex++;
            continue;
        }
        if (lstrcmpiW(argv[argumentIndex],L"--timeout-seconds") == 0 &&
            argumentIndex + 1 < argc) {
            WCHAR* end = NULL;
            unsigned long parsed;
            parsed = wcstoul(argv[argumentIndex+1],&end,10);
            if (!end || *end || parsed == 0 || parsed > 3600) {
                fwprintf(stderr,L"Invalid --timeout-seconds value.\n");
                return 2;
            }
            timeoutSeconds = (DWORD)parsed;
            argumentIndex += 2;
            continue;
        }
        break;
    }
    if (argc - argumentIndex < 1 || argc - argumentIndex > 3) {
        fwprintf(stderr,
            L"Usage:\n  %s [--embedding] [--timeout-seconds N] <target.exe> [trace.jsonl] [ready.marker]\n",
            argv[0]);
        return 2;
    }
    executable = argv[argumentIndex];

    if (!make_hook_path(hookPath,(DWORD)(sizeof(hookPath)/sizeof(hookPath[0])))) {
        fprintf(stderr,"Could not resolve xstream_io_hook.dll path.\n");
        return 1;
    }

    if (GetFileAttributesW(hookPath) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr,L"Hook DLL not found: %s\n",hookPath);
        return 1;
    }

    if (argc - argumentIndex >= 2) {
        DWORD n = GetFullPathNameW(
            argv[argumentIndex+1],
            (DWORD)(sizeof(tracePath)/sizeof(tracePath[0])),tracePath,NULL);
        if (!n || n >= (DWORD)(sizeof(tracePath)/sizeof(tracePath[0]))) {
            print_win32_error("GetFullPathName(trace)");
            return 1;
        }
    } else if (!make_default_trace_path(
                   tracePath,(DWORD)(sizeof(tracePath)/sizeof(tracePath[0])))) {
        fprintf(stderr,"Could not create default trace path.\n");
        return 1;
    }
    if (file_exists(tracePath)) {
        fwprintf(stderr,L"Trace path already exists: %s\n",tracePath);
        return 1;
    }
    if (!make_result_path(
            tracePath,resultPath,
            (DWORD)(sizeof(resultPath)/sizeof(resultPath[0])))) {
        fprintf(stderr,"Could not resolve capture result path.\n");
        return 1;
    }
    if (file_exists(resultPath)) {
        fwprintf(stderr,L"Capture result path already exists: %s\n",resultPath);
        return 1;
    }
    if (argc - argumentIndex == 3) {
        static WCHAR readyFullPath[MAX_PATH*4];
        DWORD n = GetFullPathNameW(
            argv[argumentIndex+2],
            (DWORD)(sizeof(readyFullPath)/sizeof(readyFullPath[0])),
            readyFullPath,NULL);
        if (!n || n >= (DWORD)(sizeof(readyFullPath)/sizeof(readyFullPath[0]))) {
            print_win32_error("GetFullPathName(ready marker)");
            return 1;
        }
        if (file_exists(readyFullPath)) {
            fwprintf(stderr,L"Ready marker already exists: %s\n",readyFullPath);
            return 1;
        }
        readyPath = readyFullPath;
    }

    if (!SetEnvironmentVariableW(TRACE_ENV_NAME,tracePath)) {
        print_win32_error("SetEnvironmentVariable");
        return 1;
    }

    if (_snwprintf_s(
            commandLine,sizeof(commandLine)/sizeof(commandLine[0]),_TRUNCATE,
            embedding ? L"\"%s\" /Embedding" : L"\"%s\"",executable) < 0) {
        fprintf(stderr,"XStream command line is too long.\n");
        return 1;
    }

    ZeroMemory(&si,sizeof(si));
    ZeroMemory(&pi,sizeof(pi));
    si.cb = sizeof(si);

    wprintf(L"XStream IOCTL trace launcher\n");
    wprintf(L"  Executable: %s\n",executable);
    wprintf(L"  Hook DLL:   %s\n",hookPath);
    wprintf(L"  Trace:      %s\n",tracePath);
    wprintf(L"  Embedding:  %s\n",embedding ? L"yes" : L"no");
    if (readyPath) wprintf(L"  Ready:      %s\n",readyPath);

    if (!CreateProcessW(
            executable,commandLine,NULL,NULL,FALSE,CREATE_SUSPENDED,
            NULL,NULL,&si,&pi)) {
        print_win32_error("CreateProcessW");
        return 1;
    }
    timestamp_now(g_timeline.processCreateReturnedUtc,
        sizeof(g_timeline.processCreateReturnedUtc));

    if (!inject_dll_and_initialize(pi.hProcess,hookPath)) {
        (void)terminate_target(pi.hProcess,1,&exitCode);
        write_capture_result(
            tracePath,resultPath,pi.dwProcessId,exitCode,
            L"launcher_injection_failure_terminated");
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    if (!write_ready_marker(readyPath,pi.dwProcessId,embedding)) {
        (void)terminate_target(pi.hProcess,1,&exitCode);
        write_capture_result(
            tracePath,resultPath,pi.dwProcessId,exitCode,
            L"launcher_readiness_failure_terminated");
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    timestamp_now(g_timeline.resumeThreadCalledUtc,
        sizeof(g_timeline.resumeThreadCalledUtc));
    if (ResumeThread(pi.hThread) == (DWORD)-1) {
        timestamp_now(g_timeline.resumeThreadReturnedUtc,
            sizeof(g_timeline.resumeThreadReturnedUtc));
        print_win32_error("ResumeThread");
        (void)terminate_target(pi.hProcess,1,&exitCode);
        write_capture_result(
            tracePath,resultPath,pi.dwProcessId,exitCode,
            L"launcher_resume_failure_terminated");
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }
    timestamp_now(g_timeline.resumeThreadReturnedUtc,
        sizeof(g_timeline.resumeThreadReturnedUtc));

    wprintf(L"Tracing PID %lu. Close XStream normally to finish capture.\n",
        (unsigned long)pi.dwProcessId);

    timestamp_now(g_timeline.targetWaitStartedUtc,
        sizeof(g_timeline.targetWaitStartedUtc));
    if (timeoutSeconds)
        timestamp_after_seconds(g_timeline.targetWaitStartedUtc,
            timeoutSeconds,g_timeline.targetDeadlineUtc,
            sizeof(g_timeline.targetDeadlineUtc));
    waitResult = WaitForSingleObject(pi.hProcess,
        timeoutSeconds ? timeoutSeconds*1000UL : INFINITE);
    timestamp_now(g_timeline.targetWaitCompletedUtc,
        sizeof(g_timeline.targetWaitCompletedUtc));
    if (waitResult == WAIT_OBJECT_0) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_OBJECT_0");
        timestamp_now(g_timeline.targetExitObservedUtc,
            sizeof(g_timeline.targetExitObservedUtc));
    } else if (waitResult == WAIT_TIMEOUT) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_TIMEOUT");
        g_timeline.aliveAtTimeout = 1;
    } else if (waitResult == WAIT_FAILED) {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "WAIT_FAILED");
    } else {
        strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
            "OTHER");
    }
    if (waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED) {
        timeoutTermination = waitResult == WAIT_TIMEOUT;
        terminationCode = waitResult == WAIT_TIMEOUT ?
            ERROR_TIMEOUT : ERROR_INVALID_DATA;
        launcherTerminated = 1;
        waitResult = terminate_target(pi.hProcess,terminationCode,&exitCode);
        if (!g_timeline.terminationSucceeded) launcherTerminated = 0;
        if (waitResult == WAIT_OBJECT_0)
            strcpy_s(g_timeline.waitResult,sizeof(g_timeline.waitResult),
                "WAIT_OBJECT_0_AFTER_TERMINATION");
    }
    if (!GetExitCodeProcess(pi.hProcess,&exitCode)) {
        print_win32_error("GetExitCodeProcess");
        exitCode = ERROR_INVALID_DATA;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (!write_capture_result(
            tracePath,resultPath,pi.dwProcessId,exitCode,
            launcherTerminated ? (waitResult == WAIT_OBJECT_0 ?
                (timeoutTermination ? L"launcher_timeout_terminated" :
                    L"launcher_wait_failure_terminated") :
                L"launcher_termination_state_unknown") :
                (waitResult == WAIT_OBJECT_0 ? L"process_exited" :
                    L"unknown_wait_state"))) {
        fprintf(stderr,"Capture finalization did not prove a flushed normal footer.\n");
        wprintf(L"Capture result: %s\n",resultPath);
        return ERROR_INVALID_DATA;
    }

    wprintf(L"XStream exited with code %lu\n",(unsigned long)exitCode);
    wprintf(L"Trace saved to: %s\n",tracePath);
    wprintf(L"Capture result: %s\n",resultPath);
    return exitCode == 0 ? 0 : (int)exitCode;
}
