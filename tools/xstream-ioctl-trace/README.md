# XStream user-mode IOCTL tracer

This tool captures the original 32-bit XStream application talking to the
legacy LeCroy driver without modifying the kernel driver.

Components:

- `xstream_trace_launcher.exe`: starts XStream suspended, injects the hook DLL, then resumes it.
- `xstream_io_hook.dll`: patches loaded-module imports for `DeviceIoControl`, `CreateFileA/W`, `CloseHandle`, `LoadLibraryA/W` and `GetProcAddress`.

The hook forwards calls unchanged and records JSONL with exact IOCTL input bytes
before the call and output bytes after synchronous completion. Format 4 also
records a UTC timestamp, sequence, thread, handle, family/opcode metadata and a
normal-completion footer with the record count and capture-write status.

The launcher keeps XStream suspended until the hook DLL has been loaded and its
exported `InitializeXStreamTrace` routine has completed successfully. Hook setup
therefore runs outside `DllMain` and outside the loader lock.

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

## Controlled COM-server capture

Do not start XStream manually. Run from an elevated PowerShell:

```powershell
.\xstream_trace_launcher.exe "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe"
```

For a COM local-server observation, provide the fixed `/Embedding` mode, an
exclusive private trace path and an exclusive readiness marker:

```powershell
.\xstream_trace_launcher.exe --embedding `
  "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe" `
  "C:\PrivateEvidence\xstream-ioctl.jsonl" `
  "C:\PrivateEvidence\xstream-ready.txt"
```

The launcher rejects existing trace/marker paths, starts the target suspended,
initializes the hook, writes the PID-bound readiness marker, and only then
resumes the target. It waits for orderly target exit so the trace footer is
flushed. `scripts/invoke-xstream-lossless-capture.ps1` combines this path with
the bounded single-waveform observer; its active mode still requires explicit
owner authorization and physical-disk isolation confirmation.

Validate a private capture without publishing opaque bytes:

```powershell
.\scripts\validate-xstream-ioctl-capture.ps1 `
  -TracePath "C:\PrivateEvidence\xstream-ioctl.jsonl" `
  -ResultPath "C:\PrivateEvidence\validation.json" `
  -PrivateEvidenceRoot "C:\PrivateEvidence" `
  -RepositoryRoot (Resolve-Path .)
```

## Trace format

The first JSONL record is a header. Subsequent records include:

- `create_file`: file/device opens, including path and returned handle;
- `ioctl`: code, input/output sizes, exact captured bytes, return value,
  `GetLastError`, bytes returned, duration, thread id and overlapped state;
- `close_handle`: handle closes.

Buffers are capped at 1 MiB per direction and records mark truncation. The
validator rejects a target 130-byte request unless all bytes and the complete
14-byte synchronous output contract are present. Raw JSONL is private evidence
and must never be committed to the public repository.
Asynchronous calls are marked `pending=true`; this first tracer version does not
yet hook completion APIs, so pending calls do not contain final completion output.

The capture is intentionally user-mode only. The original legacy kernel driver
is not patched or replaced.

## Offline qualification

Build on the 64-bit development system:

```powershell
cd C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver
git pull
.\scripts\build-xstream-ioctl-trace.ps1
```

The focused suite builds a deterministic x86 target and exercises the actual
launcher, hook, validator and orchestration path under Windows PowerShell 5.1:

```powershell
.\tests\dry\test-xstream-ioctl-trace.ps1
```

It covers exact 130/14-byte binary preservation including NULs, concurrency,
multiple handles, failures, output capture, write failure, collision handling,
`/Embedding`, private-evidence containment, footer integrity and the complete
mocked launch/validation chain. It performs no driver or hardware operation.
