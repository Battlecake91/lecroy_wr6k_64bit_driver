param(
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 90,
    [string]$OutputDirectory = "trace-captures"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$lecdiag = Join-Path $repo "tools\lecdiag\build\lecdiag.exe"

if (-not (Test-Path $lecdiag)) {
    throw "lecdiag.exe not found at $lecdiag. Build tools\lecdiag first."
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
