# Active handoff: ProBus hotplug / HWInt, 2026-09-28

**Read this file and `AGENTS.md` before changing code.** Chat in German,
write all repository documents and code comments in English.

Repository: https://github.com/Battlecake91/lecroy_wr6k_64bit_driver
Branch: `main`.

## Exact status at handoff

A targeted code patch was pushed in commit
`70716bace9ec874cf9b2f123a7946288215e1810`
(`Restore legacy HWInt receive interrupt and ProBus hotplug latch`).
It changes `driver/Ioctl.c` and `driver/Acquisition.c`, plus explanatory
documentation. **This patch has not yet been compiled or hardware-tested
by the user.** Any commits after it that only update documentation do not
constitute a tested build. The preceding driver acquisition baseline showed
correct-looking live waveforms, timebase/vertical scale, coupling, bandwidth,
triggers, 2-channel/10-GS/s and normal acquisition DMA.

The immediate next action is a build and ONE focused hardware regression of
the new HWInt interrupt path, not another speculative implementation.

## User's last physical test (pre-patch)

AP015 attached before x64 XStream starts:
- identified correctly via family-0/1 opcode `0x4A`;
- previous x64 preconnected trace proved the captured first 128 metadata
  bytes match the legacy AP015 response.
- On unplug while XStream was running, XStream did **not** notice removal.
- On reinsertion, the displayed waveform disappeared.

Trace `xstream_trace_20260928_213834.jsonl` was captured before the
`70716ba` patch. Parsed facts:

| Metric | Value |
|---|---:|
| Duration | 73.143 s |
| Captured IOCTLs | 36,700 |
| Non-success NTSTATUS | 0 |
| CFDC2138 DMA | 6,987 |
| DMA returned-byte-count mismatch | 0 |
| Standalone 85FB/0x01 status | 1,613 |
| Enabled mask | 0x02BF |
| Pending mask in all captured status reads | 0x0080 |
| Pending 0x0200 | 0 |
| Family-1/0x82 | 0 |
| Family-0/0x4A | 1 (startup seq 509) |
| Family-1/0x4A | 1 (startup seq 511) |

At approx. t=27.1 s the acquisition pattern changes from including
167,936-byte and other larger transfers to repeated 1,024-/2,048-byte
transfers. DMA does not stop and remains NTSTATUS-successful through the end.
No hand-action timestamps were recorded, so do not assert whether this
transition was caused by unplugging versus reinsertion, or equate DMA
success to a still-visible waveform.

## Four files to transfer to a new conversation

These runtime trace files are *not* committed to the public repository.
Upload them individually or in a single ZIP:

1. `legacy_xstream_trace_20260928_183409_probus_original.jsonl`
   - original x86 driver: XStream starts, AP015 hot-plugged, sensitivity
     changed, Degauss and Auto Zero invoked; contains the three pending
     0x0200 notifications and subsequent 0x82/0x4A exchange.
2. `xstream_trace_20260928_183807_probus_x64.jsonl`
   - previous x64 implementation: AP015 inserted into running XStream;
     no recognition and no pending 0x0200.
3. `xstream_trace_20260928_193741.jsonl`
   - previous x64 implementation: AP015 connected *before* XStream;
     startup metadata/recognition succeeds; Degauss/Auto Zero requested.
4. `xstream_trace_20260928_213834.jsonl`
   - previous x64 implementation: preconnected, then removed, then reinserted;
     removal not noticed, waveform disappears, DMA calls still succeed.

The four-trace A/B analysis lives in
`docs/probus-calibration-ab-comparison.md`. Calibration traces
`legacy_xstream_trace_20260928_183122_x86_Caibration_original.jsonl`
and `xstream_trace_20260928_183640_x64_calibration.jsonl` are **optional**:
the user's apparent excess calibration was corrected to once per previously
uncalibrated V/div step, with a working cache. It is no longer an active issue.

All relevant selectively recovered original C/ASM, scripts, and x64 source
are already on GitHub. No need to reupload those exported files. The original
proprietary binary must not be committed to the public repository.

## Root cause recovered statically and implemented in 70716ba

These paths in `ghidra_exports/selected/` are essential:

- `000160a8_FUN_000160a8.c`: enables legacy INTEN global bit `0x08`.
- `0001619a_FUN_0001619a.c`: calls the above before solicited 85FB
  firmware reply fetch; prior x64 synchronous polling failed to enable bit 3.
- `000108d6_FUN_000108d6.c`: ISR handles `INTST 0x08` and acknowledges
  the source via BAR1 `CLRIRQ = 2` before common INTST acknowledge.
- `raw_114f2.asm.txt` (especially VA `0x114A2..0x114C8`) and
  `00011390_FUN_00011390.c`: actual original DPC call to
  `FUN_000176A2(transport, &hwIntWord)` followed by
  `FUN_000157A6(commandStatus, hwIntWord)`. The Ghidra decompiled C
  *incorrectly* shows `FUN_000157A6(..., 0)` because it misses a stack
  out-parameter. **Use the assembly, not that misleading decompilation.**
- `000176a2_FUN_000176a2.c`: reads BAR1 `HWInt` at offset `0x410`
  into a low-16-bit result and writes `0` to acknowledge nonzero HWInt.
- `0001785b_FUN_0001785b.c`: establishes the HWInt register wrapper
  as BAR1 `0x410`.
- `000157a6_FUN_000157a6.c`: `pending |= enabled & hwIntWord`.

The original uses INTST bit 0x08 for the HWInt command-status notification,
including the authentic ProBus bit `0x0200`. The previous x64 replacement
never enabled INTEN bit `0x08` and never executed this HWInt DPC branch.
Patch 70716ba now:

1. Enables `INTEN |= 0x08` before the first actual firmware response fetch.
2. On an actual INTST 0x08 DPC, reads BAR1 HWInt 0x410, acknowledges it
   by writing 0 when nonzero, latches only `hwIntWord & enabledMask`,
   and wakes registered CFDC2180 event.
3. Preserves synchronous RX_CONTROL polling for solicited reply data.
4. Does **not** synthesize pending 0x0200, does not inject a probe-detection
   command, and does not add a background polling loop.
5. Preserves the working immediate IIMCL completion acknowledgement,
   CLRIRQ writes, single-channel CFDC2138 DMA and family-1 MTTRGO path.

## Exact next scope test

Scope repository: `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
Use an elevated PowerShell window on the scope:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& "C:\Users\LeCroyUser\Desktop\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

The helper pulls `main`, builds/reloads/signs the Debug driver and captures
an XStream IOCTL trace. **First address any WDK compile/install failure.**
If the patch builds, connect AP015 before starting XStream, verify normal
waveform and AP015 metadata. Then perform only ONE controlled unplug and
replug during the running session, waiting several seconds between actions.
If the probe UI or waveform behaves abnormally, stop the test and close
XStream normally. Do not repeat probing on an unstable acquisition board.
Maintain the established <=5% CPU limit.

Collect the newly generated JSONL and the user's observed behavior/timing.
Specifically check:

- No new WDK build errors, DMA timeouts, NTSTATUS failures or interrupt storm;
- whether pending `0x0200` now appears in standalone 85FB/0x01;
- corresponding local/forwarded opcode-0x88 acknowledge with mask 0x0200;
- follow-up family-1/0x82, family-0/1 opcode-0x4A AP015 metadata;
- removal *and* insertion detection; and waveform remaining visible;
- whether the large-to-small transfer-pattern transition still occurs and
  whether its timing aligns with a recorded physical action.

Do not alter unrelated PCI/DMA code to chase the probe event. If the new
patch fails, compare directly against the preceding known-good driver
baseline and the four old ProBus traces.

## Static preflight review and first-build gate (2026-09-28)

The first review in the successor chat checked `main` at `ef8761f`
(documentation-only descendant of `70716ba`) against the checked-in
original decompilations and raw DPC assembly. The changes in
`driver/Ioctl.c` and `driver/Acquisition.c` are internally consistent:

- the receive enable is immediately before a real firmware 85FB fetch;
- ISR source `INTST 0x08` already uses the original `CLRIRQ = 2` and
  records the enabled interrupt for the DPC;
- DPC reads DWORD BAR1 HWInt `0x410`, uses only the low WORD, clears
  HWInt only when that WORD is nonzero, latches under `LegacyEventLock`
  and signals `LegacyEvent0` for any nonzero source word;
- standalone 85FB/0x01 reads the same pending/enable words under that
  lock; the existing family-0/0x88 path clears pending bits and forwards
  mask `0x0200` to the board instead of treating it as a host-only ack;
- BAR1 `0x410` falls within the established `0x40000`-byte resource;
  no newly referenced symbol or obvious C syntax/type mismatch was found.

This was **source review, NOT a WDK build or hardware verification**. No
change was made to the driver or known-good acquisition path. One existing
concurrency consideration for the first retest is that
`LecCommitLegacyInterruptMask` takes a caller-computed full mask; the
initial bit-`0x08` enable and a concurrent transfer bit-`0x01` toggle could
interleave. This is not an observed failure; do not proactively refactor DMA
or PCI register access without evidence. Watch for transfer timeouts or a
missing persistent bit after starting normal acquisition.

The four uploaded ZIP traces are pre-patch historical baselines, not test
results for this code. The independent check and exact matching seq numbers
are retained in `docs/probus-calibration-ab-comparison.md`.

### Build and hardware run: gated steps

On the scope, elevate PowerShell. First perform a build-only check (the
regular desktop helper also rebuilds during sign/install, but this exposes
compile problems before touching the installed driver):

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
Set-Location "C:\\Users\\LeCroyUser\\Git\\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git update failed; stop here." }
git log -1 --oneline
& ".\\scripts\\build-driver.ps1" -Configuration Debug
```

If a compiler/linker error occurs, **stop before sign/load** and retain its
full diagnostic output. There is no valid post-patch observation until a
working driver is compiled, installed and reloaded. Once the build passes,
run the known scope-local capture helper, still from elevated PowerShell:

```powershell
& "C:\\Users\\LeCroyUser\\Desktop\\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

Before the helper starts XStream, attach AP015. During capture use only the
following controlled phases, noting elapsed seconds and visible behavior:

1. Startup: verify AP015 label/metadata, live waveform, normal trigger,
   and no sustained CPU use above 5%. Abort if baseline acquisition is wrong.
2. Baseline: hold steady at least 5 seconds, without adjusting calibration,
   timebase, channels, Degauss or Auto Zero.
3. Disconnect AP015 **once**, note the approximate elapsed time, wait at
   least 5 seconds, and note whether XStream removes probe recognition and
   whether the waveform continues.
4. Reconnect AP015 **once**, note the approximate elapsed time, wait at
   least 5 seconds, and note metadata/recognition and visible acquisition.
5. Close XStream normally to complete the capture. Abort promptly for a lost
   waveform, DMA timeout, persistent high CPU, recurrent event storm or
   obvious abnormal behavior; do not repeat hotplug on an unstable board.

For the newly generated post-patch JSONL, parse standalone 85FB/0x01
`pending & 0x0200`, follow-up family-0/0x88 mask `0x0200`, family-1/0x82,
family-0/1 opcode-0x4A/AP015 data, CFDC2138 count/success/returned lengths,
and whether successful DMA still supplies a visible waveform. Compare with
the legacy event sequence (seq 14552/14553/14560, and subsequent events)
and the old x64 run 213834. The first test does not require identical event
counts. Include physical-action timing separately; the historical t=27.1 s
transfer-pattern change cannot be assigned to an unplug or replug without it.

## Other unfinished tasks (not the next priority)

- Identical probe family-0/0x4A `...47 00` operation returns different
  payloads: legacy `...02000000FFFF` versus pre-patch x64 repeated
  `...040000000000`; Degauss/Auto Zero protocol outcome not proven
  equivalent just because the user invoked them.
- METHOD_NEITHER `CFDD219F` stays deliberately gated; do not accidentally
  enable x86 pointer/DMA assumptions on x64.
- Unobserved multi-channel CFDC2138 remains gated; keep 32-bit PCI DMA
  descriptor physical-address safety (reject addresses above 4 GiB).
- Driver coverage audit: 27 recovered original top-level IOCTLs, 20
  represented legacy cases, six lacking a top-level x64 case and one
  deliberately gated. See `docs/ioctl-map.md`.
- Developer `Run Link Tests` is *vendor-disabled* for S65/WaveRunner in
  proprietary `lecaladdinhwaccesspcisvr.dll`, not a missing replacement
  kernel feature. The Service Revision error was already fixed by local
  family-1 `0xA1/0xA2` BAR register reads.

## Maintaining the public repository

Chat German; code and docs English. Update `AGENTS.md`, this handoff file,
`docs/runtime-trace.md`, relevant ProBus docs and README after new verified
findings. Avoid proprietary EXE/DLL/SYS uploads to GitHub. Do not regress the
known-good acquisition code. Prove changed driver behavior on hardware before
marking the new patch as working.
