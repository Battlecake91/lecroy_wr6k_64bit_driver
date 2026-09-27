# XStream user-mode IOCTL tracer

This tool captures the original 32-bit XStream application talking to the
legacy LeCroy driver without modifying the kernel driver.

Components:

- `xstream_trace_launcher.exe`: starts XStream suspended, injects the hook DLL, then resumes it.
- `xstream_io_hook.dll`: patches loaded-module imports for `DeviceIoControl`, `CreateFileA/W`, `CloseHandle`, `LoadLibraryA/W` and `GetProcAddress`.

The hook forwards calls unchanged and records JSONL with exact IOCTL input bytes
before the call and output bytes after synchronous completion.

## Build

From the repository root on a machine with MSVC x86 tools:

```powershell
.\scripts\build-xstream-ioctl-trace.ps1
```

Outputs:

```text
tools\xstream-ioctl-trace\build\x86\xstream_trace_launcher.exe
tools\xstream-ioctl-trace\build\x86\xstream_io_hook.dll
```

Both files must stay in the same directory.

## Capture on the original 32-bit system

Do not start XStream manually. Run from an elevated PowerShell:

```powershell
.\xstream_trace_launcher.exe "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe"
```

Or specify the output path explicitly:

```powershell
.\xstream_trace_launcher.exe "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe" "C:\Temp\legacy_xstream_trace.jsonl"
```

Close XStream normally after startup reaches the state to compare. The launcher
waits for XStream to exit so the trace is flushed.

## Trace format

The first JSONL record is a header. Subsequent records include:

- `create_file`: file/device opens, including path and returned handle;
- `ioctl`: code, input/output sizes, exact captured bytes, return value,
  `GetLastError`, bytes returned, duration, thread id and overlapped state;
- `close_handle`: handle closes.

Buffers are capped at 1 MiB per direction and records mark truncation.
Asynchronous calls are marked `pending=true`; this first tracer version does not
yet hook completion APIs, so pending calls do not contain final completion output.

The capture is intentionally user-mode only. The original legacy kernel driver
is not patched or replaced.
