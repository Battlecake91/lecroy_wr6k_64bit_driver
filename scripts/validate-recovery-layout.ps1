<#
.SYNOPSIS
  Validate a read-only Windows/EFI/recovery-media layout before a recovery drill.
.DESCRIPTION
  The caller supplies already identified roots. The script reads required
  markers and writes only an optional new result file. It never mounts volumes,
  changes BCD, edits registry hives, invokes DISM/PnPUtil or reboots.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$WindowsRoot,
    [Parameter(Mandatory=$true)][string]$EfiRoot,
    [Parameter(Mandatory=$true)][string]$RecoveryMediaRoot,
    [string]$RollbackRoot,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Resolve-ExistingRoot {
    param([string]$Path, [string]$Name)
    if (-not [System.IO.Path]::IsPathRooted($Path)) {
        throw "$Name must be absolute: $Path"
    }
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $full -PathType Container)) {
        throw "$Name is missing: $full"
    }
    return $full
}

function Require-File {
    param([string]$Root, [string]$Relative, [string]$Label)
    $path = Join-Path $Root $Relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$Label is missing: $path"
    }
    return [ordered]@{
        RelativePath = $Relative
        Length = [uint64](Get-Item -LiteralPath $path).Length
        SHA256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
}

function Write-ExclusiveJson {
    param([string]$Path, $Value)
    $full = [System.IO.Path]::GetFullPath($Path)
    $parent = Split-Path -Parent $full
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Output parent does not exist: $parent"
    }
    $stream = New-Object System.IO.FileStream($full,
        [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.StreamWriter($stream, $utf8)
        try { $writer.Write(($Value | ConvertTo-Json -Depth 8)) }
        finally { $writer.Dispose() }
    }
    finally { if ($null -ne $stream) { $stream.Dispose() } }
}

$windows = Resolve-ExistingRoot $WindowsRoot 'WindowsRoot'
$efi = Resolve-ExistingRoot $EfiRoot 'EfiRoot'
$media = Resolve-ExistingRoot $RecoveryMediaRoot 'RecoveryMediaRoot'
if ($windows -eq $efi -or $windows -eq $media -or $efi -eq $media) {
    throw 'Windows, EFI and recovery-media roots must be distinct.'
}

$windowsMarkers = @(
    Require-File $windows 'Windows\System32\Config\SYSTEM' 'Offline SYSTEM hive'
    Require-File $windows 'Windows\System32\ntoskrnl.exe' 'Offline kernel'
)
$efiMarkers = @(
    Require-File $efi 'EFI\Microsoft\Boot\BCD' 'Offline BCD store'
)
$mediaMarkers = @(
    Require-File $media 'EFI\Boot\bootx64.efi' 'x64 UEFI boot file'
    Require-File $media 'sources\boot.wim' 'Windows recovery boot image'
)

$rollback = $null
if (-not [string]::IsNullOrWhiteSpace($RollbackRoot)) {
    $rollbackPath = Resolve-ExistingRoot $RollbackRoot 'RollbackRoot'
    $rollback = [ordered]@{
        Root = $rollbackPath
        Manifest = Require-File $rollbackPath 'recovery-manifest.json' `
            'Rollback recovery manifest'
    }
}

$result = [ordered]@{
    Schema = 1
    CapturedUtc = [datetime]::UtcNow.ToString('o')
    Outcome = 'PASS'
    WindowsRoot = $windows
    EfiRoot = $efi
    RecoveryMediaRoot = $media
    WindowsMarkers = $windowsMarkers
    EfiMarkers = $efiMarkers
    RecoveryMediaMarkers = $mediaMarkers
    Rollback = $rollback
    Safety = [ordered]@{
        ReadOnlyInputs = $true
        BcdModified = $false
        RegistryHiveLoaded = $false
        DriverStoreModified = $false
        RebootInitiated = $false
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    Write-ExclusiveJson -Path $OutputPath -Value $result
}
$result
