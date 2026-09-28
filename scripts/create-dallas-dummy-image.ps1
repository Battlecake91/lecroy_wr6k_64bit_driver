<#
.SYNOPSIS
Creates a PRIVATE 512-byte DS2433 test image without accessing hardware.

.DESCRIPTION
Checks two independent source backups byte-for-byte and requires that both
are exactly 512 bytes. Finds the highest entirely 0xFF-filled 32-byte page
and writes an unmistakably invalid ASCII test marker into a COPY only.

The marker is NOT an XStream license record. It tests image preparation,
not license parsing or installed-chip write/erase functionality.

Refuses existing output paths. No Dallas DeviceIoControl is used.
The source backup files are never modified. Keep all image files private.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ImageA,

    [Parameter(Mandatory = $true)]
    [string]$ImageB,

    [Parameter(Mandatory = $true)]
    [string]$OutputImage
)

$ErrorActionPreference = 'Stop'

$sourceA = (Resolve-Path -LiteralPath $ImageA -ErrorAction Stop).ProviderPath
$sourceB = (Resolve-Path -LiteralPath $ImageB -ErrorAction Stop).ProviderPath
$target = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputImage)

if ([string]::Equals($sourceA, $sourceB, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Supply two independent backup files, not the same path twice.'
}
if ([string]::Equals($target, $sourceA, [System.StringComparison]::OrdinalIgnoreCase) -or
    [string]::Equals($target, $sourceB, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output image must not be one of the original backup paths.'
}
if ([System.IO.File]::Exists($target)) {
    throw "Refusing to overwrite existing file: $target"
}
if (-not $target.EndsWith('.ds2433.bin', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use a .ds2433.bin output filename so the private image is Git-ignored.'
}

$original = [System.IO.File]::ReadAllBytes($sourceA)
$independent = [System.IO.File]::ReadAllBytes($sourceB)

if ($original.Length -ne 512 -or $independent.Length -ne 512) {
    throw 'A DS2433 image must contain exactly 512 bytes.'
}

for ($i = 0; $i -lt 512; $i++) {
    if ($original[$i] -ne $independent[$i]) {
        throw ("Independent backups disagree at EEPROM offset 0x{0:X3}; no test image created." -f $i)
    }
}

$marker = [System.Text.Encoding]::ASCII.GetBytes('FAKE-XSTREAM-LICENSE-TEST-ONLY')
if ($marker.Length -gt 32) {
    throw 'Internal test marker unexpectedly exceeds one 32-byte page.'
}

# Only modify a full 0xFF page, searched from the end. No attempt is made
# to guess license slot locations from readable-looking content.
$selectedPage = -1
for ($page = 15; $page -ge 0; $page--) {
    $pageIsBlank = $true
    $start = $page * 32
    for ($position = 0; $position -lt 32; $position++) {
        if ($original[$start + $position] -ne 0xFF) {
            $pageIsBlank = $false
            break
        }
    }
    if ($pageIsBlank) {
        $selectedPage = $page
        break
    }
}

if ($selectedPage -lt 0) {
    throw 'No entirely 0xFF-filled 32-byte page found. No image created; license layout analysis required.'
}

$image = New-Object byte[] 512
[System.Array]::Copy($original, $image, 512)

$targetOffset = $selectedPage * 32
[System.Array]::Copy($marker, 0, $image, $targetOffset, $marker.Length)

# Refuse accidental differences anywhere except the known marker range.
for ($i = 0; $i -lt 512; $i++) {
    $expected = $original[$i]
    if ($i -ge $targetOffset -and $i -lt ($targetOffset + $marker.Length)) {
        $expected = $marker[$i - $targetOffset]
    }
    if ($image[$i] -ne $expected) {
        throw ("Unexpected test-image difference at 0x{0:X3}." -f $i)
    }
}

$stream = [System.IO.FileStream]::new(
    $target,
    [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None
)
try {
    $stream.Write($image, 0, $image.Length)
    $stream.Flush($true)
}
finally {
    $stream.Dispose()
}

$saved = [System.IO.File]::ReadAllBytes($target)
$verified = $saved.Length -eq 512
if ($verified) {
    for ($i = 0; $i -lt 512; $i++) {
        if ($saved[$i] -ne $image[$i]) {
            $verified = $false
            break
        }
    }
}

if (-not $verified) {
    Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue
    throw 'Saved test-image verification failed; output removed if possible.'
}

Write-Host 'Private invalid-license TEST IMAGE created (no hardware access).'
Write-Host ("Unmodified inputs: two matching 512-byte backups.")
Write-Host ("Changed page: {0} (offset 0x{1:X3}, {2} ASCII bytes; all other bytes identical)." -f
    $selectedPage, $targetOffset, $marker.Length)
Write-Host ("Output: {0}" -f $target)
Write-Host ("Original SHA256: {0}" -f (Get-FileHash -LiteralPath $sourceA -Algorithm SHA256).Hash)
Write-Host ("Test image SHA256: {0}" -f (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)
Write-Warning 'A fully 0xFF page is only a candidate test area, not verified application-free EEPROM space.'
Write-Warning 'DO NOT write this image to the installed licensing chip until a tested recovery/write procedure exists.'
