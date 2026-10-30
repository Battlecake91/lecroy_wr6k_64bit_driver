# Original x86, historical x64 and current acquisition parity

## Batch 25 first XStream execution result

The historically used XStream installation is LeCroy XStream DSO 8.5.1.1.
Its registered 32-bit COM local server is
`C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe`, SHA-256
`27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F`.
Creating `LeCroy.XStreamDSO` starts the application and may automatically begin
acquisition, so COM activation itself is the hardware-attempt boundary.

The Batch 25 observer deliberately does not call `Acquisition.Acquire`, change
trigger/timebase/vertical CVARs or start a second application process. After
the pinned capture is ready, it creates the COM server once and polls the
existing `Acquisition/C1/Out/Result` hierarchy for a non-empty finite
`DataArray`. It records channel configuration, sample counts, compact sample
statistics and a SHA-256 over the returned double values. Raw waveform values
remain private. A second XStream request may be generated automatically, but
the installed kernel limit rejects it before acquisition MMIO.

This mode passed 9/9 focused mock cases and the complete capture-launcher suite
passed 44/44. The owner then confirmed physical disk isolation and authorized
one live activation. The COM server process started once but failed to register,
returning `CO_E_SERVER_EXEC_FAILURE`; no retry occurred.

The kernel trace proves that startup progressed through four device handles and
511 initialization IOCTLs. It proves equally clearly that acquisition did not
begin: there is no `CFDC2124`, `CFDC2138`, mapping, descriptor, GO, IRQ or DPC
record. Six identical family-2 opcode-`0x03` `CFDC2110` chains were the only
requests to fail at NTSTATUS level. The native validated gate does not yet admit
that opcode, while original-driver analysis assigns it common-transport send
and extended response-timeout semantics. This is the leading compatibility gap
to resolve before another separately authorized waveform attempt. No waveform
data or completion length exists for Batch 25.

## Batch 24 automatic-rearm conclusion

The reference waveform trace reaches `CFDC2138` about 18.31 seconds after
startup and records 3,134 successful acquisitions; a later controls run records
43,157. Live monitoring shows that acquisition traffic stops and resumes with
the application state. XStream therefore automatically rearms and cannot be
treated as a single-GO launch. Batch 24 retains the regular WDM-v3 sequence and
adds only a separate one-admission kernel limit for the first waveform test.

## Batch 23 first physical parity evidence

The current WDM-v3 implementation completed one real 16-byte MAM transfer with
the historically observed channel `0x30` and config `0x00000C00`. Evidence
shows one logical SG segment, the exact two-entry descriptor shape, admitted
GO, physical-IRQ/DPC completion, a four-byte completed count of 16, normal
mapping retirement and successful unregister. Device health remained intact.

This closes the first-transfer transport/lifetime gap against the original x86
sequence. It does not yet prove that the 16 destination bytes represent a valid
waveform because the one-shot diagnostic did not preserve or print them. It
also does not qualify XStream's persistent registration reuse, rearming,
trigger configuration, repeated acquisition or Stop/restart behavior. The next
milestone is one separately authorized XStream waveform acquisition using a
reviewed candidate that does not bypass or reset the consumed one-shot gate.

## Batch 21 bounded first-transfer status

The exact-shape, exact-once candidate is installed as `oem106.inf`, version
`0.2026.1029.1`. One captured installation and the complete 9/9 R0 plus 2/2 R1
matrix passed. PCI identity remains `1570:0005`, and both qualified START reads
return `0x00000002`. At that stage no MAM GO had occurred, so it added installed
ABI and START evidence but no physical IRQ, DPC, payload or mapping-retirement
parity. Batch 23 later completed the separately authorized transfer.

## Batch 19 bounded first-transfer conclusion

The historical parity review supports a 16-byte, one-channel MAM request with
channel `0x30`, config `0x00000C00`, the original descriptor encoding and
terminator, the established MAM/SGTA/IIMTC/INTEN/IIMCL/MAMRGO order, physical
IRQ/DPC completion and a four-byte completed-count response. Historical x86 and
x64 captures also show that registrations were reused across many transfers.

Batch 19 did not test that path on the current WDM-v3 implementation. The
installed candidate lacked an in-kernel exact-shape and exact-once boundary, so
the authorized GO was skipped. The offline correction deliberately targets only
the smallest reviewed single-channel experiment. Multi-channel requests,
repeated transfers and XStream acquisition remain outside that gate and require
later, separately authorized lifecycle qualification.

Batch 20 produced the signed exact-shape candidate but did not install it. The
captured installation launcher failed before PnPUtil dispatch, so the installed
binary, R0/R1 status and absence of a real transfer remain those of Batch 19.
This outcome adds no IRQ, DPC, payload or normal-retirement parity evidence.

## Established compatibility reference

The original x86 driver registers a process-owned user buffer through
`CFDC2124`, keeps that registration across acquisitions, accepts a 15-byte
single-channel `CFDC2138` request, programs MAM and DMA registers, waits for
interrupt/DPC completion and returns the requested byte count in a four-byte
output. `CFDC2128` or final process close releases the registration.

Its descriptor entries are `{ count_dwords, address }`, split at 4-KiB
boundaries, page-chained through slot 511 and terminated by `{0,0}`. The x86
driver derives addresses directly from MDL PFNs. That describes the legacy
format but is not a correct x64 DMA-remapping contract.

## Historically working x64 evidence

Commit `6cc5a238684c217f1238aae2069950ec99a39672` is the first precisely
identified waveform-capable implementation. The immediately following
evidence records 3,134 successful acquisitions with visible waveforms. Later
evidence records 43,157 successful acquisitions and normal scope controls; the
last precisely identified extensively exercised code line is
`34907090820a582ac360d02120316c8792d7c888`.

That line preserved the original MAM/IRQ sequence, but also preserved the PFN
address assumption. Commit `4682976add715b642339405f847c6d58861bf709`
disabled that path by default. Historical success is therefore strong evidence
for register and XStream semantics, not approval to restore PFN addressing.

## Current WDM-v3 implementation

The current path preserves the public IOCTL shapes, process/FileObject
ownership, five-interface discovery, MAM register order, interrupt mask,
generation-specific physical IRQ/DPC completion, four-byte result and reusable
registration. It replaces source/table PFNs with Windows DMA logical addresses
and a WDM common-buffer descriptor table.

Normal completion retires the per-call mapping and allows a repeat. A timeout
or other uncertain post-GO result retains the mapping and locked pages and
closes admission. The old x86 behavior did not provide this safe abnormal
boundary.

## Minimal XStream-compatible sequence

XStream discovers the main PCI interface class, registers legacy event handles,
uses `CFDC2124` to obtain a persistent token, and issues synchronous
`CFDC2138` requests. The request is:

```text
DWORD token, BYTE 1, BYTE 1, BYTE channel,
DWORD config, DWORD requested_bytes
```

The smallest historically observed success used 16 data bytes, channel
`0x30`, config `0x00000C00` and a `CFDC2124` total size of 20 bytes. This is a
first-test vector, not proof of a universal minimum or a complete XStream
front-end/trigger setup. Visible waveform qualification still requires a later
separately authorized XStream run.

## Remaining runtime gap

The development MAM gate is signed, installed and has completed its one allowed
bounded transfer. It must not be reset or reused for another diagnostic GO.
The next milestone is a separately reviewed single XStream waveform stage.
