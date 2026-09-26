# Runtime trace capture

The native x64 driver contains a non-invasive IOCTL trace ring intended for
capturing the exact startup/runtime protocol emitted by the original 32-bit
XStream software.

The trace path is deliberately observational. It does not enable partially
understood `0xCFDC2110` hardware execution.

## Recommended capture

Build `tools/lecdiag` first, then run from the repository root:

```powershell
.\scripts\capture-xstream-trace.ps1
```

The default capture duration is 90 seconds. A different duration can be used:

```powershell
.\scripts\capture-xstream-trace.ps1 -DurationSeconds 180
```

The script writes a timestamped file under the ignored local directory:

```text
trace-captures/xstream_trace_YYYYMMDD_HHMMSS.jsonl
```

Start XStream after the capture command begins.

## JSONL format

The first line is capture metadata. Every following line is one IOCTL record.

Example shape:

```json
{"type":"ioctl","seq":12,"timestamp_100ns":123456789,"pid":1234,"wow64":1,"method":0,"method_name":"BUF","ioctl":"0xCFDC2110","name":"CFDC2110","input_length":24,"output_length":8,"information":0,"status":"0xC0000010","type3_input_buffer":"0x0000000000000000","user_buffer":"0x0000000000000000","input_hex":"...","output_hex":""}
```

Fields:

- `seq`: monotonically increasing trace sequence after the trace is cleared;
- `timestamp_100ns`: boot-relative interrupt time in 100 ns units;
- `pid`: caller process ID;
- `wow64`: whether the request came from a 32-bit process;
- `method`: IOCTL transfer method;
- `ioctl`: numeric IOCTL code;
- `name`: known symbolic name when available;
- `input_length` / `output_length`: requested buffer lengths;
- `information`: completed `IoStatus.Information`;
- `status`: completed NTSTATUS;
- `type3_input_buffer` and `user_buffer`: pointer values for METHOD_NEITHER
  correlation only;
- `input_hex`: up to 256 captured input bytes;
- `output_hex`: up to 128 captured METHOD_BUFFERED output bytes.

For METHOD_NEITHER requests the tracer probes and copies only the bounded input
preview. A probe failure is logged and leaves `input_hex` empty. The tracer
does not dereference arbitrary output user buffers.

## Live capture versus snapshot

`lecdiag trace-save file.jsonl` writes the current in-kernel ring once.

`lecdiag trace-capture file.jsonl 90` first clears the ring and then polls it
every 250 ms, appending newly observed sequence numbers to the JSONL file. This
is preferred for XStream because it avoids losing early startup traffic when
the ring wraps.

The kernel ring currently stores 256 records. Successful high-frequency generic
register reads are intentionally omitted because they would otherwise evict the
control protocol that is relevant to reverse engineering.

## Handling

Runtime captures are ignored by Git by default because they contain process
IDs and pointer values. Share a selected trace explicitly when analysis is
needed; do not commit raw captures to the public repository by accident.
