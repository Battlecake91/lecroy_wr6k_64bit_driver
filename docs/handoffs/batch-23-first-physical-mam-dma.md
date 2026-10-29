# Batch 23 first physical MAM DMA handoff

## Outcome

Classification: **successful transfer and normal completion**.

The owner explicitly confirmed that the productive Windows x86 disk was
physically disconnected and authorized one 16-byte MAM GO using installed
`oem106.inf`. The immutable loaded SYS remained
`647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71`.

One pre-dispatch orchestration invocation failed on a PowerShell boolean literal
before it wrote authorization evidence, created an attempt marker or started
`lecdiag`. It was retained as evidence and caused zero device commands. The
corrected orchestration then wrote the owner record and exclusive marker before
the only hardware dispatch.

The exact command was dispatched once:

```text
lecdiag mam-dma-once I-AUTHORIZE-ONE-MAM-GO
```

It exited zero in about 145 ms without timeout. User-mode output reported a
16-byte request, four returned bytes, completed count 16 and successful token
unregister. No second transfer was attempted.

## Kernel evidence

The 180-second qualified DebugView capture completed normally. It contains
exactly one each of:

- `CFDC2124` registration;
- exact-shape atomic experiment-gate consumption;
- `CFDC2138` acquisition;
- WDM-v3 transaction summary;
- `CFDC2128` unregister.

The transaction summary records one 16-byte logical SG segment, two descriptors,
`launchDenied=0`, `completed=1`, `released=1`, `quarantined=0` and
`status=0x00000000`. Gate consumption proves the descriptor payload/address
match, 32-bit range/alignment and `{0,0}` terminator validation completed before
the atomic latch changed.

`completed=1` is correlated with production source, not inferred merely from
the process exit code. It requires `LecDmaCompletionObservePhysicalIrq` in the
ISR, generation-matched `LecDmaCompletionConsumeSignal` in the DPC, event wake,
successful INTEN cleanup and normal transaction retirement. The mapping was
then released and the persistent registration was explicitly unregistered.

The capture does not contain a separate line for every MMIO write. The exact
MAM/SGTA/IIMTC/INTEN/IIMCL/MAMRGO order remains source-proven and was exercised
as the successful path. The diagnostic did not print or retain destination
contents, so no waveform-content assertion is made.

## Evidence hashes

Private local root:
`Batch23-MamDmaCapture-20261029-attempt1`.

- raw kernel capture:
  `FF9176764CF54E3EA003CF9075FDC6F1C67F144A8BB82ACAE5F3DB735A60F398`
- live stdout:
  `088B09DF7B7892B574B10E0A155AD133B045E87D5C83B8DF57388C4EB1084E14`
- capture result:
  `D9F582DD3D80668DE7760CB262D5DEFBF072F312F878E073106B63579AD73324`
- MAM attempt marker:
  `A82D9A0001ABEEA3BB402A13D1140DE620BDEC398AFD75D7B71DAE02D2140EEE`
- command result:
  `3D44CEB70B0C398278D31864DD7CC8D79795AE60F40F6C6CE5E6F443597E60F4`
- post-test runtime baseline:
  `C90746C8214325ECE3FA60E20CC50552125DB8C1D586F13568CAE1DD2860CDE0`
- analysis summary:
  `1AAF05CB21A5CB575CB8A42C14947FF0251B1B9F38CEAA263CC46B3160F9D795`

Raw traces and addresses remain private local evidence and must not be committed.

## Final state and next milestone

Post-test device status is `OK`, problem code zero, service running, five
interfaces and exact SYS identity. No new target Kernel-PnP, Code Integrity or
WHEA error was observed. DebugView and Dbgv are gone. Mapping retirement was
normal; no `UnknownActive` quarantine exists.

The one-shot latch is consumed for this loaded FDO. Do not reload merely to
obtain another diagnostic GO. The next stage should prepare one reviewed,
separately authorized XStream waveform acquisition, including payload capture,
persistent registration ownership, original-compatible completion, and a clear
Stop boundary. Repeated acquisition, trigger control and restart follow only
after that single-waveform result.
