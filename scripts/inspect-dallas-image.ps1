<#
.SYNOPSIS
Prints a REDACTED structural summary of a private 512-byte DS2433 image.

.DESCRIPTION
Never connects to hardware, never writes the supplied image(s), and
never prints EEPROM bytes, strings, license keys, or hash fingerprints.

Without -After: per 32-byte page shows counts of 0xFF, 0x00 and ASCII
printable bytes. A page lacking 32 x FF is not evidence that no free
application-level license record exists.

With -After: additionally identifies changed byte offsets, contiguous
changed ranges and affected pages WITHOUT exposing their values.
Useful around a legitimate XStream license-dialog transaction.

Both images must be exactly 512 bytes.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Before,

    [string]$After
)

$ErrorActionPreference = 'Stop'
$beforePath = (Resolve-Path -LiteralPath $Before -ErrorAction Stop).ProviderPath
$first = [System.IO.File]::ReadAllBytes($beforePath)
if ($first.Length -ne 512) {
    throw "Expected exactly 512 bytes in -Before, got $($first.Length)."
}

$second = $null
if ($PSBoundParameters.ContainsKey('After')) {
    $afterPath = (Resolve-Path -LiteralPath $After -ErrorAction Stop).ProviderPath
    $second = [System.IO.File]::ReadAllBytes($afterPath)
    if ($second.Length -ne 512) {
        throw "Expected exactly 512 bytes in -After, got $($second.Length)."
    }
}

Write-Host 'PRIVATE DS2433 structure (counts only; no EEPROM/license bytes printed):'
$perPage = @()
for ($page=0; $page -lt 16; $page++) {
    $begin=$page*32
    $ff=0; $zeros=0; $printable=0; $changes=0
    for ($j=0; $j -lt 32; $j++) {
        $value = [int]$first[$begin+$j]
        if ($value -eq 0xFF) { $ff++ }
        if ($value -eq 0) { $zeros++ }
        if ($value -ge 0x20 -and $value -le 0x7E) { $printable++ }
        if ($null -ne $second -and $first[$begin+$j] -ne $second[$begin+$j]) {
            $changes++
        }
    }
    $row=[ordered]@{
        Page=$page
        Offset=('0x{0:X3}' -f $begin)
        FF=$ff
        Zero=$zeros
        Printable=$printable
        Other=(32-$ff-$zeros-$printable)
    }
    if ($null -ne $second) { $row['Changed']=$changes }
    $perPage += [pscustomobject]$row
}
$perPage | Format-Table -AutoSize

if ($null -eq $second) {
    Write-Host 'This is structural occupancy only: no full-FF page does NOT prove no free license record.'
    return
}

$ranges=@()
$from=-1
for ($index=0; $index -le 512; $index++) {
    $different=($index -lt 512 -and $first[$index] -ne $second[$index])
    if ($different -and $from -lt 0) { $from=$index }
    if (-not $different -and $from -ge 0) {
        $last=$index-1
        $ranges += [pscustomobject]@{
            Start=('0x{0:X3}' -f $from)
            End=('0x{0:X3}' -f $last)
            Length=($index-$from)
            Pages=('{0}..{1}' -f [int][math]::Floor($from/32),[int][math]::Floor($last/32))
        }
        $from=-1
    }
}
$total=($perPage | Measure-Object -Property Changed -Sum).Sum
Write-Host ("Total changed bytes: {0} / 512" -f $total)
if ($ranges.Count -gt 0) {
    Write-Host 'Changed contiguous OFFSET RANGES (no contents):'
    $ranges | Format-Table -AutoSize
} else {
    Write-Host 'Images are byte-for-byte identical. No EEPROM image change detected.'
}
