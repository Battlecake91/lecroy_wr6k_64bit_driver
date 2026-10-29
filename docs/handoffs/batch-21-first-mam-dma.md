# Batch 21 first MAM DMA handoff

## Current state

Branch `codex/phase2c-runtime-batch21` started at
`074432447d980010082b02312d45df2f03fadced`. Commit
`dace6c022d29e1d436f9e430db9ef0edc975a865` fixes only the PnPUtil launcher
boundary: an omitted path is resolved from `SystemRoot` inside the elevated
Windows PowerShell 5.1 process; malformed literal environment-variable text,
relative paths and missing files remain fail-closed. The focused production-
path suite passed 41/41.

The frozen Batch 20 package was not rebuilt or re-signed. It was installed
exactly once and is now published as `oem106.inf`, version `0.2026.1029.1`.
Loaded SYS SHA-256 is
`647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71`.
Device status is `OK`, problem code is zero, service `LecS65AcqDrv` is running
and all five interfaces are active.

The permanent runtime runner completed R0 9/9 and R1 2/2 exactly once. Trace
size is `0x110`, register-list size is `0x2CAE`, PCI identity is `1570:0005`,
observed BDF is evidence-only `5:1.0`, and both START reads are `0x00000002`.
No target failure event was observed.

## Frozen package identity

- INF: `653622D71A7E4F62954A441947C691D090AC6ABF4B3FDC6BF342BEBF24F88B06`
- CAT: `E28B7EC2870454CFF6B88747D4496C67FCF3F1F8D0045FDB57E22E6D7752DD83`
- SYS: `647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71`
- PDB: `AF8FF333B31BCA2FEBD40ACE87BE741296D46F028861D48EF40A95CB093ADCB3`
- Manifest: `000055E084E4554669ED8D74D2EB747FCA5393DF19415AB14EA0B57E486BE653`

## External evidence

- Launcher tests: `Batch21-LauncherTests-20261029-attempt2`; result SHA-256
  `07BA5C572AC591C7115493B9D02528D52223EB8652F48721F5575BDF7AEE0915`.
- Captured installation: `Batch21-InstallWithCapture-20261029-attempt1`;
  kernel capture SHA-256
  `6F372E38291A8C57B88C117EB807B156C8C059BE5B383E96AC3B0F20DE975D63`.
- R0/R1: `Batch21-R0R1-20261029-attempt1`; evidence manifest SHA-256
  `467C5A3A51237F339F3B5B8DA370B1D40DA18566B5CE22FAD5993AB0B38DDF4F`.
- Disk inventory: `Batch21-PreGo-20261029-attempt1`; SHA-256
  `AFFA6D23C704085503EC348FD7235DE816E75446A91BC1F4D299B77E86A35550`.

These directories contain private local runtime evidence and must not be
committed to the public repository.

## Stop gate

No MAM GO has occurred. Software inventory shows only the healthy x64 NVMe
`HFM256GDHTNG-8510B`, serial `ACE4_2E00_9590_A2AA.`, but cannot prove that the
productive x86 disk is physically disconnected. Before dispatch, obtain an
explicit owner confirmation of physical disconnection and a fresh authorization
for exactly:

```text
lecdiag mam-dma-once I-AUTHORIZE-ONE-MAM-GO
```

The command must run once under the established kernel capture with a durable
pre-dispatch marker. A timeout or uncertain result is not retryable and must
retain `UnknownActive` resources. XStream remains prohibited in this batch.
