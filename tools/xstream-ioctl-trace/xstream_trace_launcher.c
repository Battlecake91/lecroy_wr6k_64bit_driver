#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#define TRACE_ENV_NAME L"LECS65_TRACE_PATH"

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

static int inject_dll(HANDLE process,const WCHAR* dllPath)
{
    SIZE_T bytes = ((SIZE_T)lstrlenW(dllPath)+1)*sizeof(WCHAR);
    LPVOID remote;
    HANDLE thread;
    HMODULE kernel32;
    FARPROC loadLibraryW;
    DWORD exitCode = 0;

    remote = VirtualAllocEx(process,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
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
        print_win32_error("CreateRemoteThread");
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        return 0;
    }

    WaitForSingleObject(thread,INFINITE);
    if (!GetExitCodeThread(thread,&exitCode))
        print_win32_error("GetExitCodeThread");

    CloseHandle(thread);
    VirtualFreeEx(process,remote,0,MEM_RELEASE);

    if (!exitCode) {
        fprintf(stderr,"LoadLibraryW in XStream returned NULL.\n");
        return 0;
    }
    return 1;
}

int wmain(int argc,WCHAR** argv)
{
    WCHAR hookPath[MAX_PATH*4];
    WCHAR tracePath[MAX_PATH*4];
    WCHAR commandLine[32768];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exitCode = 0;

    if (argc < 2 || argc > 3) {
        fwprintf(stderr,L"Usage:\n  %s <xstream.exe> [trace.jsonl]\n",argv[0]);
        return 2;
    }

    if (!make_hook_path(hookPath,(DWORD)(sizeof(hookPath)/sizeof(hookPath[0])))) {
        fprintf(stderr,"Could not resolve xstream_io_hook.dll path.\n");
        return 1;
    }

    if (GetFileAttributesW(hookPath) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr,L"Hook DLL not found: %s\n",hookPath);
        return 1;
    }

    if (argc == 3) {
        DWORD n = GetFullPathNameW(
            argv[2],(DWORD)(sizeof(tracePath)/sizeof(tracePath[0])),tracePath,NULL);
        if (!n || n >= (DWORD)(sizeof(tracePath)/sizeof(tracePath[0]))) {
            print_win32_error("GetFullPathName(trace)");
            return 1;
        }
    } else if (!make_default_trace_path(
                   tracePath,(DWORD)(sizeof(tracePath)/sizeof(tracePath[0])))) {
        fprintf(stderr,"Could not create default trace path.\n");
        return 1;
    }

    if (!SetEnvironmentVariableW(TRACE_ENV_NAME,tracePath)) {
        print_win32_error("SetEnvironmentVariable");
        return 1;
    }

    if (_snwprintf_s(
            commandLine,sizeof(commandLine)/sizeof(commandLine[0]),_TRUNCATE,
            L"\"%s\"",argv[1]) < 0) {
        fprintf(stderr,"XStream command line is too long.\n");
        return 1;
    }

    ZeroMemory(&si,sizeof(si));
    ZeroMemory(&pi,sizeof(pi));
    si.cb = sizeof(si);

    wprintf(L"XStream IOCTL trace launcher\n");
    wprintf(L"  Executable: %s\n",argv[1]);
    wprintf(L"  Hook DLL:   %s\n",hookPath);
    wprintf(L"  Trace:      %s\n",tracePath);

    if (!CreateProcessW(
            argv[1],commandLine,NULL,NULL,FALSE,CREATE_SUSPENDED,
            NULL,NULL,&si,&pi)) {
        print_win32_error("CreateProcessW");
        return 1;
    }

    if (!inject_dll(pi.hProcess,hookPath)) {
        TerminateProcess(pi.hProcess,1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    if (ResumeThread(pi.hThread) == (DWORD)-1) {
        print_win32_error("ResumeThread");
        TerminateProcess(pi.hProcess,1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    wprintf(L"Tracing PID %lu. Close XStream normally to finish capture.\n",
        (unsigned long)pi.dwProcessId);

    WaitForSingleObject(pi.hProcess,INFINITE);
    GetExitCodeProcess(pi.hProcess,&exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    wprintf(L"XStream exited with code %lu\n",(unsigned long)exitCode);
    wprintf(L"Trace saved to: %s\n",tracePath);
    return 0;
}
