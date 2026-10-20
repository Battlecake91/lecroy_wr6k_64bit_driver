#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

HANDLE WINAPI LecdiagMockCreateFileW(
    LPCWSTR FileName,
    DWORD DesiredAccess,
    DWORD ShareMode,
    LPSECURITY_ATTRIBUTES SecurityAttributes,
    DWORD CreationDisposition,
    DWORD FlagsAndAttributes,
    HANDLE TemplateFile);

BOOL WINAPI LecdiagMockDeviceIoControl(
    HANDLE Device,
    DWORD IoControlCode,
    LPVOID InputBuffer,
    DWORD InputLength,
    LPVOID OutputBuffer,
    DWORD OutputLength,
    LPDWORD BytesReturned,
    LPOVERLAPPED Overlapped);

#define CreateFileW LecdiagMockCreateFileW
#define DeviceIoControl LecdiagMockDeviceIoControl
