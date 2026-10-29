# Original x86, historical x64 and current acquisition parity

## Batch 21 bounded first-transfer status

The exact-shape, exact-once candidate is installed as `oem106.inf`, version
`0.2026.1029.1`. One captured installation and the complete 9/9 R0 plus 2/2 R1
matrix passed. PCI identity remains `1570:0005`, and both qualified START reads
return `0x00000002`. No MAM GO has occurred, so this adds installed ABI and
START evidence but no physical IRQ, DPC, payload or mapping-retirement parity.
The next parity step remains one separately authorized 16-byte transfer after
positive physical isolation of the productive x86 disk.

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

The development MAM gate is signed and installed but has not been exercised.
The next bounded milestone is one no-XStream transfer using the exact 16-byte vector,
one DMA-logical segment and two descriptors, followed by passive device-health
checks. No retry is allowed. A separate authorization is required.
