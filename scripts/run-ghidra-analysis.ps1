param(
    [string]$CommitMessage = "analysis: export requested Ghidra targets"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$targetFile = Join-Path $repo "ghidra_scripts\targets.txt"
$exportDir = Join-Path $repo "ghidra_exports\selected"
$manifestFile = Join-Path $exportDir "EXPORT_MANIFEST.txt"
$localConfig = Join-Path $repo ".ghidra-local.ps1"

Set-Location $repo

# Ghidra rotates its internal database snapshots. They are required by the
# checked-in project, but local headless runs must not make Git treat those
# rotations as source changes.
$trackedGhidraDb = git ls-files "ghidra_reverse_engineering_lecroy/**/~*.db/*"
foreach ($path in $trackedGhidraDb) {
    if ($path) {
        git update-index --skip-worktree -- $path
    }
}

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
    Set-Content -Path $localConfig -Encoding UTF8 -Value ("`$GhidraHome = '" + $escaped + "'")
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

# Explicit recover: targets modify the *local* analyzed Ghidra project
# (unlike normal read-only exports). Create an automatic sibling backup.
# Close the Ghidra GUI before running recovery so the copy is consistent.
$recoverTargets = @($targets | Where-Object { $_ -like "recover:*" })
if ($recoverTargets.Count -gt 0) {
    $ghidraProject = Join-Path $repo "ghidra_reverse_engineering_lecroy"
    if (-not (Test-Path -LiteralPath $ghidraProject)) {
        throw "Ghidra project directory not found: $ghidraProject"
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupRoot = Split-Path -Parent $repo
    $backupPath = Join-Path $backupRoot ("LeCroy_Ghidra_backup_" + $stamp)
    if (Test-Path -LiteralPath $backupPath) {
        throw "Ghidra backup path already exists: $backupPath"
    }

    Write-Host "Function recovery modifies the local Ghidra database."
    Write-Host "Backing up complete Ghidra project to $backupPath"
    Copy-Item -LiteralPath $ghidraProject -Destination $backupPath -Recurse -ErrorAction Stop
    Write-Host "Ghidra project backup created."
}

$baseCommit = (git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $baseCommit) {
    throw "Unable to determine current Git commit."
}

$args = @(
    (Join-Path $repo "ghidra_reverse_engineering_lecroy"),
    "LeCroy_Alladin_Driver",
    "-process", "LecS65AcqDrv.sys",
    "-scriptPath", (Join-Path $repo "ghidra_scripts"),
    "-postScript", "ExportSelected.java",
    $exportDir
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

if (-not (Test-Path $exportDir)) {
    throw "Expected export directory was not created: $exportDir"
}

$exportedFiles = Get-ChildItem -Path $exportDir -File |
    Where-Object { $_.Name -ne "EXPORT_MANIFEST.txt" } |
    Sort-Object Name |
    ForEach-Object { $_.Name }

$manifest = @(
    "LeCroy Ghidra selected export manifest"
    "Source commit: $baseCommit"
    "Program: LecS65AcqDrv.sys"
    ""
    "Targets:"
)
$manifest += $targets | ForEach-Object { "  $_" }
$manifest += @(
    ""
    "Exported files:"
)
$manifest += $exportedFiles | ForEach-Object { "  $_" }

Set-Content -Path $manifestFile -Encoding UTF8 -Value $manifest

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

$shareCommit = (git rev-parse HEAD).Trim()
$remoteUrl = (git remote get-url origin).Trim()
$shareUrl = $null
$repoPath = $null

if ($remoteUrl.StartsWith("https://github.com/")) {
    $repoPath = $remoteUrl.Substring("https://github.com/".Length)
}
elseif ($remoteUrl.StartsWith("git@github.com:")) {
    $repoPath = $remoteUrl.Substring("git@github.com:".Length)
}

if ($repoPath) {
    if ($repoPath.EndsWith(".git")) {
        $repoPath = $repoPath.Substring(0, $repoPath.Length - 4)
    }

    $shareUrl = "https://github.com/$repoPath/commit/$shareCommit"
}

Write-Host ""
Write-Host "Done."
Write-Host "Ghidra export commit: $shareCommit"
if ($shareUrl) {
    Write-Host "Share this commit:"
    Write-Host "  $shareUrl"
}
else {
    Write-Host "Share the commit SHA above. Remote URL could not be converted automatically."
}
Write-Host ""
Write-Host "Manifest:"
Write-Host "  $manifestFile"
Write-Host ""
git status --short
