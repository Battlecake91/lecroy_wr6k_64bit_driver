# Compact Ghidra export workflow

This workflow keeps reverse-engineering review compact and repeatable. Instead of manually copying decompiler output into chat, export only selected functions and XREF summaries from the existing Ghidra project.

## Script

`ghidra_scripts/ExportSelected.java`

The Java script is used because standard `analyzeHeadless` does not load PyGhidra Python scripts by default. It accepts:

```text
<output-dir> <target> [<target> ...]
```

A target may be a function address such as `1619a` or `0x1619a`, or a symbol/import name such as `KeSetEvent`.

For address targets the script exports decompiled C plus compact incoming and outgoing function references. For symbol targets it exports references and containing caller functions. The special target `inventory` writes `FUNCTION_INVENTORY.txt`, listing every function Ghidra currently recognizes together with body size, whether selected pseudocode has already been exported, and compact incoming/outgoing reference counts. The new target `coverage` writes `CODE_COVERAGE.txt`: recognized vs unowned decoded instruction byte counts in executable memory blocks, plus each contiguous cluster of disassembled instructions **outside** Ghidra function bodies. These clusters frequently include compiler thunks and virtual call targets that ordinary function inventory misses. The companion `UNOWNED_CODE_REFS.txt` enumerates incoming references for *every* orphan decoded instruction, including entry points in the middle of a contiguous cluster.

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
