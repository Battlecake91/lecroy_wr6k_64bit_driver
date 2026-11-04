#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdint.h>

#define TARGET_IOCTL 0xCFDC2110U

typedef struct TRACE_THREAD_CONTEXT {
    HANDLE Device;
    const BYTE* Request;
    volatile LONG* Calls;
    volatile LONG* Failures;
} TRACE_THREAD_CONTEXT;

static void write_u16(BYTE* target,USHORT value)
{
    target[0] = (BYTE)value;
    target[1] = (BYTE)(value >> 8);
}

static void build_request(BYTE request[130])
{
    DWORD i;
    ZeroMemory(request,130);
    write_u16(request,6);
    write_u16(request+2,112);
    write_u16(request+4,3);
    write_u16(request+6,0xA5FB);
    request[8] = 0x40;
    request[9] = 2;
    request[10] = 3;
    for (i=11; i<120; ++i)
        request[i] = (i % 9U == 0U) ? 0U : (BYTE)(i ^ 0xA5U);
    write_u16(request+120,8);
    write_u16(request+122,2);
    write_u16(request+124,3);
    write_u16(request+126,0x85FB);
    request[128] = 0x40;
    request[129] = 0;
}

static DWORD WINAPI issue_requests(LPVOID parameter)
{
    TRACE_THREAD_CONTEXT* context = (TRACE_THREAD_CONTEXT*)parameter;
    DWORD index;
    for (index=0; index<3; ++index) {
        BYTE output[14];
        DWORD returned = 0;
        ZeroMemory(output,sizeof(output));
        SetLastError(ERROR_SUCCESS);
        BOOL ok = DeviceIoControl(
            context->Device,TARGET_IOCTL,(LPVOID)context->Request,130,
            output,sizeof(output),&returned,NULL);
        if (ok || GetLastError() == ERROR_SUCCESS || returned != 0)
            InterlockedIncrement(context->Failures);
        InterlockedIncrement(context->Calls);
    }
    return 0;
}

static int issue_successful_output_ioctl(void)
{
    WCHAR directory[MAX_PATH];
    WCHAR path[MAX_PATH];
    HANDLE file;
    USHORT compression = 0;
    DWORD returned = 0;
    if (!GetTempPathW(MAX_PATH,directory) ||
        !GetTempFileNameW(directory,L"LCT",0,path)) return 0;
    file = CreateFileW(
        path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(path);
        return 0;
    }
    if (!DeviceIoControl(
        file,FSCTL_GET_COMPRESSION,NULL,0,&compression,sizeof(compression),
        &returned,NULL) || returned != sizeof(compression)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    return 1;
}

static int write_dispatch_marker(void)
{
    WCHAR path[MAX_PATH*4];
    HANDLE marker;
    DWORD length = GetEnvironmentVariableW(
        L"LECS65_SYNTHETIC_DISPATCH_MARKER",path,
        (DWORD)(sizeof(path)/sizeof(path[0])));
    if (!length || length >= (DWORD)(sizeof(path)/sizeof(path[0]))) return 0;
    marker = CreateFileW(
        path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,NULL);
    if (marker == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(marker);
    return 1;
}

int wmain(int argc,WCHAR** argv)
{
    BYTE request[130];
    HANDLE devices[2];
    HANDLE threads[2];
    TRACE_THREAD_CONTEXT contexts[2];
    volatile LONG calls = 0;
    volatile LONG failures = 0;
    DWORD index;
    WCHAR expectEmbedding[2];
    DWORD expectLength;

    expectLength = GetEnvironmentVariableW(
        L"LECS65_EXPECT_EMBEDDING",expectEmbedding,
        (DWORD)(sizeof(expectEmbedding)/sizeof(expectEmbedding[0])));
    if (expectLength != 0 &&
        (argc != 2 || lstrcmpiW(argv[1],L"/Embedding") != 0)) return 13;

    build_request(request);
    for (index=0; index<2; ++index) {
        devices[index] = CreateFileW(
            L"NUL",GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
        if (devices[index] == INVALID_HANDLE_VALUE) return 10;
        contexts[index].Device = devices[index];
        contexts[index].Request = request;
        contexts[index].Calls = &calls;
        contexts[index].Failures = &failures;
        threads[index] = CreateThread(
            NULL,0,issue_requests,&contexts[index],0,NULL);
        if (!threads[index]) return 11;
    }
    WaitForMultipleObjects(2,threads,TRUE,10000);
    for (index=0; index<2; ++index) {
        CloseHandle(threads[index]);
        CloseHandle(devices[index]);
    }
    if (!issue_successful_output_ioctl()) return 12;
    if (calls != 6 || failures != 0 || !write_dispatch_marker()) return 14;
    return 0;
}
