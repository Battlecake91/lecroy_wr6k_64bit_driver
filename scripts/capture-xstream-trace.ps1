param(
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 90,
    [string]$OutputDirectory = "trace-captures"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$lecdiag = Join-Path $repo "tools\lecdiag\build\lecdiag.exe"
$lecdiagSource = Join-Path $repo "tools\lecdiag\lecdiag.c"
$buildHelper = Join-Path $PSScriptRoot "build-lecdiag.ps1"

$needsBuild = -not (Test-Path $lecdiag)

if (-not $needsBuild -and (Test-Path $lecdiagSource)) {
    $needsBuild = (Get-Item $lecdiagSource).LastWriteTimeUtc -gt (Get-Item $lecdiag).LastWriteTimeUtc
}

if ($needsBuild) {
    Write-Host "lecdiag is missing or older than its source. Rebuilding..."
    & $buildHelper
    if ($LASTEXITCODE -ne 0) {
        throw "lecdiag rebuild failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path $lecdiag)) {
    throw "lecdiag.exe not found at $lecdiag after rebuild."
}

# Guard against a stale binary copied from an older checkout.
$help = & $lecdiag 2>&1 | Out-String
if ($help -notmatch 'trace-capture') {
    Write-Host "lecdiag binary does not support trace-capture. Rebuilding..."
    & $buildHelper
    if ($LASTEXITCODE -ne 0) {
        throw "lecdiag rebuild failed with exit code $LASTEXITCODE."
    }
}

if (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repo $OutputDirectory
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path $OutputDirectory "xstream_trace_$stamp.jsonl"

Write-Host ""
Write-Host "LeCroy x64 IOCTL trace capture"
Write-Host "  Duration : $DurationSeconds s"
Write-Host "  Output   : $out"
Write-Host ""
Write-Host "Start XStream after capture begins."
Write-Host ""

& $lecdiag trace-capture $out $DurationSeconds
if ($LASTEXITCODE -ne 0) {
    throw "lecdiag trace-capture failed with exit code $LASTEXITCODE."
}

Write-Host ""
Write-Host "Trace saved:"
Write-Host "  $out"
