param(
    [string]$CommitMessage = "analysis: export requested Ghidra targets"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$targetFile = Join-Path $repo "ghidra_scripts\targets.txt"
$localConfig = Join-Path $repo ".ghidra-local.ps1"

Set-Location $repo

if (Test-Path $localConfig) {
    . $localConfig
}

if (-not $GhidraHome) {
    $GhidraHome = $env:GHIDRA_HOME
}

if (-not $GhidraHome) {
    $GhidraHome = Read-Host "Path to Ghidra installation"
    if (-not $GhidraHome) {
        throw "No Ghidra installation path supplied."
    }

    $escaped = $GhidraHome.Replace("'","''")
    Set-Content -Path $localConfig -Encoding UTF8 -Value ("$GhidraHome = '" + $escaped + "'")
    Write-Host "Saved local Ghidra path to $localConfig"
}

$headless = Join-Path $GhidraHome "support\analyzeHeadless.bat"
if (-not (Test-Path $headless)) {
    throw "analyzeHeadless.bat not found at: $headless"
}

if (-not (Test-Path $targetFile)) {
    throw "Target file not found: $targetFile"
}

git pull --rebase
if ($LASTEXITCODE -ne 0) {
    throw "git pull --rebase failed."
}

$targets = Get-Content $targetFile |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith("#") }

if ($targets.Count -eq 0) {
    throw "No targets configured in $targetFile"
}

$args = @(
    (Join-Path $repo "ghidra_reverse_engineering_lecroy"),
    "LeCroy_Alladin_Driver",
    "-process", "LecS65AcqDrv.sys",
    "-scriptPath", (Join-Path $repo "ghidra_scripts"),
    "-postScript", "ExportSelected.java",
    (Join-Path $repo "ghidra_exports\selected")
)
$args += $targets
$args += "-noanalysis"

Write-Host ""
Write-Host "Exporting Ghidra targets:"
$targets | ForEach-Object { Write-Host "  $_" }
Write-Host ""

& $headless @args
if ($LASTEXITCODE -ne 0) {
    throw "Ghidra headless export failed with exit code $LASTEXITCODE."
}

git add ghidra_exports/selected

git diff --cached --quiet
$hasChanges = ($LASTEXITCODE -ne 0)

if ($hasChanges) {
    git commit -m $CommitMessage
    if ($LASTEXITCODE -ne 0) {
        throw "git commit failed."
    }
}
else {
    Write-Host "No new export changes to commit."
}

git push
if ($LASTEXITCODE -ne 0) {
    throw "git push failed."
}

Write-Host ""
Write-Host "Done."
git status --short
