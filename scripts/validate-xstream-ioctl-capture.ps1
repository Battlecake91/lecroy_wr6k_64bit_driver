<#
.SYNOPSIS
  Validate a private, lossless XStream user-mode IOCTL capture.
.DESCRIPTION
  Validates complete CFDC2110 family-2 opcode 0x03 records without printing or
  copying the opaque request or response bytes into the repository. The result
  contains hashes and protocol metadata only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$TracePath,
    [Parameter(Mandatory=$true)][string]$ResultPath,
    [Parameter(Mandatory=$true)][string]$PrivateEvidenceRoot,
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [ValidateRange(0,1000)][int]$ExpectedRequestCount = 0,
    [ValidatePattern('^$|^[0-9a-fA-F]{64}$')][string]$ExpectedRequestHash = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-AbsolutePath {
    param([string]$Path,[string]$Label)
    if (-not [IO.Path]::IsPathRooted($Path)) {
        throw "$Label must be absolute."
    }
    return [IO.Path]::GetFullPath($Path)
}

function Test-ContainedPath {
    param([string]$Path,[string]$Root)
    $rootWithSeparator = $Root.TrimEnd('\') + '\'
    return $Path.StartsWith(
        $rootWithSeparator,[StringComparison]::OrdinalIgnoreCase)
}

function Convert-HexBytes {
    param([string]$Hex,[int]$ExpectedBytes,[string]$Label)
    if ($null -eq $Hex -or $Hex.Length -ne $ExpectedBytes * 2 -or
        $Hex -notmatch '^[0-9a-fA-F]*$') {
        throw "$Label is not a complete $ExpectedBytes-byte hex value."
    }
    $bytes = New-Object byte[] $ExpectedBytes
    for ($i=0; $i -lt $ExpectedBytes; $i++) {
        $bytes[$i] = [Convert]::ToByte($Hex.Substring($i*2,2),16)
    }
    return ,$bytes
}

function Get-ByteHash {
    param([byte[]]$Bytes)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','')
    }
    finally { $sha.Dispose() }
}

function Assert-Opcode03Shape {
    param([byte[]]$Bytes)
    if ($Bytes.Length -ne 130 -or
        $Bytes[0] -ne 6 -or $Bytes[1] -ne 0 -or
        $Bytes[2] -ne 112 -or $Bytes[3] -ne 0 -or
        $Bytes[4] -ne 3 -or $Bytes[5] -ne 0 -or
        $Bytes[6] -ne 0xFB -or $Bytes[7] -ne 0xA5 -or
        $Bytes[8] -ne 0x40 -or $Bytes[9] -ne 2 -or
        $Bytes[10] -ne 3 -or
        $Bytes[120] -ne 8 -or $Bytes[121] -ne 0 -or
        $Bytes[122] -ne 2 -or $Bytes[123] -ne 0 -or
        $Bytes[124] -ne 3 -or $Bytes[125] -ne 0 -or
        $Bytes[126] -ne 0xFB -or $Bytes[127] -ne 0x85 -or
        $Bytes[128] -ne 0x40 -or $Bytes[129] -ne 0) {
        throw 'Captured request does not match the exact opcode 0x03 framing.'
    }
}

function Write-JsonExclusive {
    param([string]$Path,[object]$Value)
    $stream = [IO.File]::Open(
        $Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try {
        $json = ($Value | ConvertTo-Json -Depth 8) + [Environment]::NewLine
        $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($json)
        $stream.Write($bytes,0,$bytes.Length)
    }
    finally { $stream.Dispose() }
}

$trace = Get-AbsolutePath $TracePath 'TracePath'
$result = Get-AbsolutePath $ResultPath 'ResultPath'
$privateRoot = Get-AbsolutePath $PrivateEvidenceRoot 'PrivateEvidenceRoot'
$repository = Get-AbsolutePath $RepositoryRoot 'RepositoryRoot'

if (-not (Test-Path -LiteralPath $privateRoot -PathType Container)) {
    throw 'PrivateEvidenceRoot does not exist.'
}
if ((Test-ContainedPath -Path $privateRoot -Root $repository) -or
    $privateRoot.Equals($repository,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Private evidence must remain outside the public repository.'
}
if ((-not (Test-ContainedPath -Path $trace -Root $privateRoot)) -or
    -not (Test-ContainedPath -Path $result -Root $privateRoot)) {
    throw 'TracePath and ResultPath must be contained by PrivateEvidenceRoot.'
}
if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) {
    throw 'TracePath does not exist.'
}
if (Test-Path -LiteralPath $result) {
    throw 'ResultPath already exists.'
}

$records = New-Object System.Collections.Generic.List[object]
$lineNumber = 0
$header = $null
$footer = $null
$reader = New-Object IO.StreamReader($trace,[Text.Encoding]::UTF8,$true)
try {
    while (($line = $reader.ReadLine()) -ne $null) {
        $lineNumber++
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        try { $record = $line | ConvertFrom-Json }
        catch { throw "Trace JSON is malformed at line $lineNumber." }
        if ($record.type -eq 'legacy_xstream_user_trace') { $header = $record }
        elseif ($record.type -eq 'trace_footer') { $footer = $record }
        elseif (($record.type -eq 'ioctl' -or $record.type -eq 'nt_ioctl') -and
            [string]$record.ioctl -eq '0xCFDC2110' -and
            [int]$record.input_length -eq 130 -and
            [int]$record.output_length -eq 14) {
            [void]$records.Add($record)
        }
    }
}
finally { $reader.Dispose() }

if ($null -eq $header -or [int]$header.format_version -lt 4 -or
    [int]$header.capture_limit -lt 130 -or
    [uint64]$header.qpc_frequency -eq 0) {
    throw 'Trace header does not prove lossless format version 4 readiness.'
}
if ($null -eq $footer -or [bool]$footer.write_failed) {
    throw 'Trace footer is missing or reports a capture write failure.'
}
if ($records.Count -eq 0) { throw 'No target opcode 0x03 records were captured.' }
if ($ExpectedRequestCount -ne 0 -and $records.Count -ne $ExpectedRequestCount) {
    throw "Expected $ExpectedRequestCount target requests; captured $($records.Count)."
}

$seenSequence = @{}
$requestHashes = New-Object System.Collections.Generic.List[string]
$summaries = New-Object System.Collections.Generic.List[object]
foreach ($record in @($records | Sort-Object {[int64]$_.seq})) {
    $sequence = [int64]$record.seq
    if ($seenSequence.ContainsKey($sequence)) { throw 'Duplicate trace sequence value.' }
    $seenSequence[$sequence] = $true
    if ([int]$record.input_captured -ne 130 -or [bool]$record.input_truncated -or
        [int]$record.family -ne 2 -or [int]$record.opcode -ne 3 -or
        [uint64]$record.timestamp_filetime_utc -eq 0 -or
        [string]::IsNullOrWhiteSpace([string]$record.handle)) {
        throw "Target request sequence $sequence is incomplete or uncorrelated."
    }
    $requestBytes = Convert-HexBytes -Hex ([string]$record.input_hex) `
        -ExpectedBytes 130 -Label "Request sequence $sequence"
    Assert-Opcode03Shape -Bytes $requestBytes
    $requestHash = Get-ByteHash -Bytes $requestBytes
    [void]$requestHashes.Add($requestHash)
    if (-not [string]::IsNullOrWhiteSpace($ExpectedRequestHash) -and
        $requestHash -ne $ExpectedRequestHash.ToUpperInvariant()) {
        throw "Request sequence $sequence does not match ExpectedRequestHash."
    }

    $returned = if ($record.type -eq 'ioctl') {
        [int64]$record.bytes_returned
    } else { [int64]$record.information }
    if ([bool]$record.pending) {
        throw "Target request sequence $sequence is asynchronous; completion output is unavailable."
    }
    if ($returned -lt 0 -or $returned -gt 14 -or
        [bool]$record.output_truncated -or
        [int]$record.output_captured -ne [int]$returned) {
        throw "Target response sequence $sequence is incomplete."
    }
    $responseBytes = Convert-HexBytes -Hex ([string]$record.output_hex) `
        -ExpectedBytes ([int]$returned) -Label "Response sequence $sequence"
    $timestamp = [DateTime]::FromFileTimeUtc([int64]$record.timestamp_filetime_utc)
    [void]$summaries.Add([ordered]@{
        Sequence = $sequence
        ThreadId = [int64]$record.tid
        Handle = [string]$record.handle
        TimestampUtc = $timestamp.ToString('o')
        DurationMicroseconds = [uint64]$record.duration_us
        Api = [string]$record.type
        RequestSha256 = $requestHash
        ResponseBytes = $returned
        ResponseSha256 = Get-ByteHash -Bytes $responseBytes
        NtStatus = if ($record.type -eq 'nt_ioctl') {[string]$record.ntstatus} else {$null}
        Success = if ($record.type -eq 'ioctl') {[bool]$record.success} else {$null}
        LastError = if ($record.type -eq 'ioctl') {[int64]$record.last_error} else {$null}
    })
}

$uniqueHashes = @($requestHashes | Sort-Object -Unique)
$resultValue = [ordered]@{
    Schema = 1
    Result = 'PASS'
    ValidatedUtc = [DateTime]::UtcNow.ToString('o')
    TraceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $trace).Hash
    TraceFormatVersion = [int]$header.format_version
    RequestCount = $records.Count
    UniqueRequestHashes = $uniqueHashes
    AllRequestsIdentical = ($uniqueHashes.Count -eq 1)
    Requests = $summaries.ToArray()
    OpaqueBytesIncluded = $false
}
Write-JsonExclusive -Path $result -Value $resultValue
Write-Host "XStream IOCTL capture validation PASS: $($records.Count) complete request(s)."
Write-Host "Opaque request and response bytes remain only in the private trace."
