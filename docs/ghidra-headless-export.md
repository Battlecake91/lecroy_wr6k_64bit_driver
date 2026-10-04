# Compact Ghidra export workflow

This workflow keeps reverse-engineering review compact and repeatable. Instead of manually copying decompiler output into chat, export only selected functions and XREF summaries from the existing Ghidra project.

## Script

`ghidra_scripts/ExportSelected.java`

The Java script is used because standard `analyzeHeadless` does not load PyGhidra Python scripts by default. It accepts:

```text
<output-dir> <target> [<target> ...]
```

A target may be a function address such as `1619a` or `0x1619a`, or a symbol/import name such as `KeSetEvent`.

For address targets the script exports decompiled C plus compact incoming and outgoing function references. For symbol targets it exports references and containing caller functions. The special target `inventory` writes `FUNCTION_INVENTORY.txt`, listing every function Ghidra currently recognizes together with body size, whether selected pseudocode has already been exported, and compact incoming/outgoing reference counts. The new target `coverage` writes `CODE_COVERAGE.txt`: recognized vs unowned decoded instruction byte counts in executable memory blocks, plus each contiguous cluster of disassembled instructions **outside** Ghidra function bodies. These clusters frequently include compiler thunks and virtual call targets that ordinary function inventory misses. The companion `UNOWNED_CODE_REFS.txt` enumerates incoming references for *every* orphan decoded instruction, including entry points in the middle of a contiguous cluster. The updated script also writes `UNOWNED_CODE_ASM.txt` with **all** 90 unowned decoded-instruction clusters and `EXECUTABLE_BYTE_CLASSIFICATION.txt` showing instruction/defined-data/undefined byte counts for each executable memory block. The same run now emits `UNDEFINED_EXECUTABLE_RANGES.txt`, listing each still-undefined executable range with its exact address span, byte length, references to the start address and a 32-byte hex prefix. This report is for manual triage, not automatic disassembly. The latter is crucial because the difference between total executable-section bytes and disassembled instruction bytes is not necessarily undiscovered code.

## Explicit recovery of overlooked functions

After the complete code-coverage census, the original Ghidra inventory is
known to omit 90 decoded executable-code clusters. These clusters are **not
equivalent to 90 missing functions**, since multiple independent routines
may share a cluster and SEH landing pads need different treatment.

The `recover:<hex-address>` target is an **explicit opt-in** function
boundary reconstruction command. It uses Ghidra's
`CreateFunctionCmd(entryAddress)` on an already-disassembled executable
instruction and exports the new function's decompiled C and references.
It writes `RECOVER_<full-8-digit-address>.txt` with the result. If the
address is already owned by a different function, it fails without
reconstructing or overwriting that function. Failed or non-executable
entries are reported separately. It is **not** an automatic function
discovery mode.

**Unlike `coverage`/`inventory`, this changes the LOCAL GHIDRA PROJECT
DATABASE.** Shut down the interactive Ghidra GUI before running the
headless script. When `ghidra_scripts/targets.txt` contains any
`recover:` target, `scripts/run-ghidra-analysis.ps1` automatically
copies the entire Ghidra project to a timestamped sibling directory
(`LeCroy_Ghidra_backup_YYYYMMDD_HHMMSS`) before execution.
This backup is deliberately outside the Git working tree.

The first curated pass includes the proven 27-case IOCTL dispatch
entry `recover:11018` and selected independent DriverWorks,
interrupt, cancel and compiler-SEH function starts. It avoids blindly
promoting internal exception landing pads and the many adjacent
five-byte virtual thunks. The final `inventory` and `coverage`
targets measure the newly recognized functions and remaining orphan
regions after the recovery pass.

Recovery is purely static. It never loads the Windows kernel driver,
sends IOCTLs or accesses LeCroy hardware. Only report success after
the user's Ghidra export confirms individual `RECOVER_...` records
and new valid pseudocode files.

## Stage 2: referenced short functions and reviewed undefined code

The first explicit `recover:` stage (commit `9d3ac5b1`) successfully
created all **25** selected functions, bringing the recognized internal
inventory from **420 to 445** and reducing unowned code from 3,207
bytes/90 clusters to **1,280 bytes/69 clusters**. Each new entry has
decompiled-C and reference exports.

The next `targets.txt` selects **79 more independently referenced,
already decoded entrypoints**, primarily LeCroy virtual thunks,
DriverWorks PnP/power callback thunks and static initializer helpers.
The selection is grounded in incoming `DATA` references from
`UNOWNED_CODE_REFS.txt`, not speculative disassembly. Deliberately
excluded are exceptional cleanup/filter entrypoints inside x86 SEH
scope regions (`0x18067`, `0x1806B`, `0x180BD`,
`0x1814C`, `0x18150`).

A further **explicitly whitelisted** target form is
`decode:<hexaddress>`. Unlike `recover:` (which requires
pre-existing decoded instructions), `decode:` invokes Ghidra's
disassembler before attempting function creation. It is restricted
to three opcode-verified candidates: `0x18E58`, `0x18EDB`,
`0x1C280`. Unknown addresses, altered instruction prefixes and
existing defined data are rejected. Results appear in
`DECODE_<address>.txt`, and successful function creations in
`RECOVER_<address>.txt`.

Both `decode:` and `recover:` **change the local Ghidra analysis
database**. The runner copies the complete Ghidra project to a
timestamped sibling directory before invoking either target type.
Close interactive Ghidra before running. The first/second-stage
exports are static only; no original or x64 Windows driver is loaded
or exercised.

## Windows command template

Replace `<GHIDRA_HOME>` with the installed Ghidra directory.

```powershell
$repo = "C:\\Users\\LeCroyUser\\Git\\lecroy_wr6k_64bit_driver"
$ghidra = "<GHIDRA_HOME>"

& "$ghidra\\support\\analyzeHeadless.bat" `
  "$repo\\ghidra_reverse_engineering_lecroy" `
  "LeCroy_Alladin_Driver" `
  -process "LecS65AcqDrv.sys" `
  -scriptPath "$repo\\ghidra_scripts" `
  -postScript ExportSelected.java `
    "$repo\\ghidra_exports\\selected" `
    KeSetEvent `
    KeInsertQueueDpc `
    IoConnectInterrupt `
    1600e `
    163b2 `
    16490 `
    16414 `
    15dea `
    160dc `
  -noanalysis
```

`-noanalysis` is intentional because the project has already been analyzed.

If you see `Ghidra was not started with PyGhidra. Python is not available`, you are still invoking the obsolete Python script. Pull the latest `main` and use `ExportSelected.java`.

After export:

```powershell
cd $repo
git add ghidra_exports/selected ghidra_scripts docs
git commit -m "analysis: export selected legacy driver paths"
git push
```

## Review rule

The complete pseudocode snapshot of the current 420-function inventory was exported in October 2026. New focused exports should avoid re-exporting all functions. Use `inventory` for the Ghidra-recognized function census and `coverage` for an **independent** decoded-instruction check against missed executable thunks.

`coverage` is not a proof of complete binary reconstruction: executable blocks also contain padding, data, and potentially undecoded code. The report compares decoded instruction lengths, not all executable bytes labeled as genuine instructions. Short indirect virtual targets still require manual assembly and call-site review.

In the working Ghidra checkout, run `git pull --rebase` after script updates, set `ghidra_scripts/targets.txt` to `coverage` and `inventory`, then run `scripts/run-ghidra-analysis.ps1`.

Do not replay unknown CFDC2110 commands on real hardware. Static analysis and passive traces remain the preferred evidence sources.
