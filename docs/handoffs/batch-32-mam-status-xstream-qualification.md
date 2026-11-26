# Batch 32 — Corrected MAM Status During XStream Startup

## Scope and result

Batch 32 completed offline preparation and passive host inspection only. No
XStream process was started, no COM activation or device IOCTL occurred, no
kernel capture was started, and no MAM GO was dispatched. The Batch 32 GO
count is zero. The existing Batch 28/29 evidence remains historical and is
not reused as authorization or as a result for the installed Batch 31 image.

The source branch was created from the required Batch 31 HEAD
`2282727655258d0661bd61324c1dafd17d0b7eb2` as
`codex/phase2c-runtime-batch32`.

## Passive installed baseline

Observed on `DESKTOP-0Q0RKAQ`, Windows 10 Pro x64, build 19045, under
`DESKTOP-0Q0RKAQ\LeCroyUser`:

| Check | Observation |
|---|---|
| PCI device | `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, present and `OK` |
| Problem code | `0` |
| Bound package | `oem109.inf`, version `0.2026.1126.1` |
| Service | `LecS65AcqDrv`, running; image `C:\windows\system32\drivers\LecS65AcqDrv.sys` |
| Loaded image file hash | `6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75` |
| Device interfaces | Five enabled instances on the stable PCI instance: `{fc5df040-d6cd-4ba0-b5e0-2561972963a2}`, `{8d1103b8-5bf4-4b5c-b21e-eeaace97d418}`, `{7ac34be9-f766-4f15-9e88-854ba5e2146e}`, `{958695a4-693a-435e-8297-66f805d8e46a}`, `{9007c2bc-edfd-4f2f-a059-df1131cb1ae5}` |
| XStream/acquisition/diagnostic processes | None matched the inspected process-name set |
| DebugView process/service | No process or `Dbgv` system-driver service found |
| Registered XStream COM server | 32-bit COM server path resolves to `C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe`; version `0,23,95,31`, SHA-256 `27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F` |

The frozen Batch 31 directory was checked read-only. INF, CAT and SYS hashes
match `AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF`,
`BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789`, and
`6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`.
The retained evidence PDB and manifest match
`020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484` and
`3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647`.
The PDB and manifest are in the package's `evidence` directory, not its
three-file installable `package` directory. Manifest metadata records
`MamLaunchLimit=1`, WDM-v3 MAM enabled, legacy PFN DMA disabled and MTT launch
disabled. This metadata does not establish that the current FDO launch latch
is unused.

The read-only BCD query could not open the boot configuration store because
the current shell is not elevated (`Access denied`). Therefore this run does
not independently verify current test-signing state. No privilege elevation
or BCD change was attempted.

## Observer and capture preparation

Focused offline tests passed under Windows PowerShell 5.1:

| Suite | Result |
|---|---:|
| Diagnostic capture/install launcher mock suite | 51/51 |
| XStream COM lifecycle | 8/8 |
| XStream IOCTL trace | 28/28 |
| Single-waveform observer | 13/13 |

The launcher fixture was corrected to copy its direct `xstream-com-lifecycle.ps1`
dependency into its isolated temporary repository. The previous test-only
failure was a missing fixture file, not a production observer failure. The
trace test generated synthetic traces and test-only x86 binaries; these are not
production assets and must not be used for a live capture.

Current script SHA-256 identities:

| Script | SHA-256 |
|---|---|
| `scripts/invoke-diagnostic-capture-install.ps1` | `8A7D86E5E6AFD1D4322CB204DDEBC42896919E537AAB762B7994CEE9F27891D2` |
| `scripts/invoke-xstream-lossless-capture.ps1` | `6634F51CA16B9EA38FA61D09F43415B5CEEDFFBA066B157D7A028133C19BBA6D` |
| `scripts/invoke-xstream-single-waveform.ps1` | `8B4C87F0EC15FED1DC6ADC2EF27B60A32099133F23564F4A4F8E19E4DC6546F3` |
| `scripts/validate-xstream-ioctl-capture.ps1` | `745CB6BAB8A05768CDFA2E9518D579B1F00FB9B01695E5452B2669C780CB68C9` |
| `scripts/xstream-com-lifecycle.ps1` | `E948F2E4451D26FA2B392B996F8F76AFEC28E34277DEFCF90695F4B9EBC943A1` |

DebugView 5.02 is present with the documented SHA-256
`7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B` and a
valid Microsoft Authenticode signature. Its kernel component was not loaded.
The production `xstream_trace_launcher.exe` and `xstream_io_hook.dll` were not
found in the scoped `.codex` file search; only ignored `x86-test` outputs were
present in the repository. Their historic hashes in the Batch 29 handoff are
not proof that matching production files are currently available. Production
capture identity and parent-owned finalization must be re-established before
live execution.

## Qualification meaning and blockers

Batch 30's source fix preserves `STATUS_DEVICE_BUSY` after successful cleanup
for launch-limit rejections. The installed Batch 31 candidate includes that
fix, but neither R0/R1 nor this passive inspection exercised an exhausted
MAM request. Batch 32 therefore has no real-hardware result for denied-request
status, `IO_STATUS_BLOCK.Information`, completion bytes, GO count during
XStream, COM Acquisition resolution, opcode `0x03`, or waveform delivery.

Before live execution, all of the following remain required:

1. Locate and hash-verify the intended production trace launcher and hook; do
   not substitute the synthetic test binaries.
2. Run the elevated read-only preflight to verify BCD test-signing state and
   the exact driver/package state immediately before the attempt.
3. Obtain fresh owner confirmation that the productive Windows x86 disk is
   physically disconnected.
4. Obtain explicit authorization for exactly one XStream COM startup, its
   automatic initialization IOCTLs, private full IOCTL capture, and at most one
   MAM hardware GO. No explicit `Acquire()`, CVar changes, retry, driver
   restart, or launch-limit reset is permitted.
5. Establish genuine kernel capture readiness before dispatch. DebugView
   binary identity alone is not capture readiness.

The exact next live stage is one fresh `XStreamWithCapture` attempt using the
verified `oem109.inf` image and the corrected Batch 29 observer, after these
gates. Stop after that attempt; preserve incomplete traces without claiming
transport, DMA, or waveform success.
