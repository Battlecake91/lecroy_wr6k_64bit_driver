# Compact Ghidra export workflow

This workflow keeps reverse-engineering review compact and repeatable. Instead of manually copying decompiler output into chat, export only selected functions and XREF summaries from the existing Ghidra project.

## Script

`ghidra_scripts/ExportSelected.py`

The script accepts:

```text
<output-dir> <target> [<target> ...]
```

A target may be a function address such as `1619a` or `0x1619a`, or a symbol/import name such as `KeSetEvent`.

For address targets the script exports decompiled C plus compact incoming and outgoing function references. For symbol targets it exports references and containing caller functions.

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
  -postScript ExportSelected.py `
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

After export:

```powershell
cd $repo
git add ghidra_exports/selected ghidra_scripts docs
git commit -m "analysis: export selected legacy driver paths"
git push
```

## Review rule

Keep exports narrow. Prefer a small set of functions and symbol XREFs per iteration rather than exporting the whole binary. This reduces repository churn and keeps AI review/token usage focused.

Do not replay unknown CFDC2110 commands on real hardware. Static analysis and passive traces remain the preferred evidence sources.
