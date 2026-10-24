<#
.SYNOPSIS
  Execute the bounded CFDC2124/CFDC2138/CFDC2128 no-launch contract once.
.DESCRIPTION
  OfflineMock validates the exact command path without hardware. RunAuthorized
  requires an elevated 64-bit Windows PowerShell 5.1 token, exact installed
  package identity, a fresh evidence directory and explicit owner approval.
  The child process is invoked once with a five-second timeout and is never
  retried. The driver must report ERROR_NOT_SUPPORTED for CFDC2138 after
  releasing its never-launched WDM-v3 mapping.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('OfflineMock','RunAuthorized')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [Parameter(Mandatory=$true)][string]$ExecutablePath,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedExecutableHash,
    [Parameter(Mandatory=$true)][string]$ExpectedInf,
    [Parameter(Mandatory=$true)][string]$ExpectedVersion,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSysHash,
    [string]$DeviceInstanceId =
        'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
    [switch]$OwnerAuthorized,
    [string]$MockStatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object Text.UTF8Encoding($false, $true)

function Write-ExclusiveText([string]$Path, [string]$Text) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = $utf8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

function Write-ExclusiveJson([string]$Path, [object]$Value) {
    Write-ExclusiveText $Path (($Value | ConvertTo-Json -Depth 12) + "`r`n")
}

function Get-FileHashValue([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Get-RealState {
    $keys = @('DEVPKEY_Device_DriverInfPath','DEVPKEY_Device_DriverVersion',
        'DEVPKEY_Device_ProblemCode','DEVPKEY_Device_Service')
    $properties = Get-PnpDeviceProperty -InstanceId $DeviceInstanceId `
        -KeyName $keys -ErrorAction Stop
    $map = @{}
    foreach ($property in $properties) { $map[$property.KeyName] = $property.Data }
    $service = [string]$map['DEVPKEY_Device_Service']
    [pscustomobject]@{
        InstanceId = $DeviceInstanceId
        Inf = [string]$map['DEVPKEY_Device_DriverInfPath']
        Version = $map['DEVPKEY_Device_DriverVersion'].ToString()
        ProblemCode = [int]$map['DEVPKEY_Device_ProblemCode']
        DeviceStatus = (Get-PnpDevice -InstanceId $DeviceInstanceId -ErrorAction Stop).Status
        ServiceName = $service
        ServiceStatus = (Get-Service -Name $service -ErrorAction Stop).Status.ToString()
        SysHash = Get-FileHashValue "$env:SystemRoot\System32\drivers\LecS65AcqDrv.sys"
    }
}

function Get-State {
    if ($Mode -eq 'OfflineMock') {
        if (-not (Test-Path -LiteralPath $MockStatePath -PathType Leaf)) {
            throw 'OfflineMock requires an existing MockStatePath.'
        }
        return Get-Content -LiteralPath $MockStatePath -Raw | ConvertFrom-Json
    }
    Get-RealState
}

function Assert-State([object]$State) {
    if ($State.InstanceId -ne $DeviceInstanceId -or
        $State.Inf -ne $ExpectedInf -or
        $State.Version -ne $ExpectedVersion -or
        [int]$State.ProblemCode -ne 0 -or
        $State.DeviceStatus -ne 'OK' -or
        $State.ServiceName -ne 'LecS65AcqDrv' -or
        $State.ServiceStatus -ne 'Running' -or
        $State.SysHash -ne $ExpectedSysHash.ToUpperInvariant()) {
        throw 'Device state does not match the approved no-launch candidate.'
    }
}

if (-not [IO.Path]::IsPathRooted($EvidenceRoot)) {
    throw 'EvidenceRoot must be absolute.'
}
if (-not [IO.Path]::IsPathRooted($ExecutablePath)) {
    throw 'ExecutablePath must be absolute.'
}
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "EvidenceRoot already exists: $EvidenceRoot"
}
if ($PSVersionTable.PSEdition -ne 'Desktop' -or
    $PSVersionTable.PSVersion.Major -ne 5 -or
    -not [Environment]::Is64BitProcess) {
    throw '64-bit Windows PowerShell 5.1 is required.'
}
if ($Mode -eq 'RunAuthorized') {
    if (-not $OwnerAuthorized) { throw 'RunAuthorized requires OwnerAuthorized.' }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'RunAuthorized requires an elevated token.'
    }
    $blocked = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(XStream|Acquisition|WaveStudio|ScopeExplorer|lecdiag|lecwatch)\.exe$'
    })
    if ($blocked.Count -ne 0) {
        throw "Blocked acquisition client is running: $($blocked.Name -join ', ')"
    }
}

$executable = [IO.Path]::GetFullPath($ExecutablePath)
$actualExecutableHash = Get-FileHashValue $executable
if ($actualExecutableHash -ne $ExpectedExecutableHash.ToUpperInvariant()) {
    throw 'No-launch executable hash mismatch.'
}
[void][IO.Directory]::CreateDirectory($EvidenceRoot)
$startedUtc = [datetime]::UtcNow
$resultState = 'FAIL'
$failure = $null
try {
    . (Join-Path $PSScriptRoot 'native-process.ps1')
    $before = Get-State
    Assert-State $before
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'before.json') $before
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'attempt.marker') ([ordered]@{
        CreatedUtc=$startedUtc.ToString('o'); Mode=$Mode
        Command=@($executable,'no-launch-prepare')
        ExecutableSha256=$actualExecutableHash; ExpectedInf=$ExpectedInf
        ExpectedVersion=$ExpectedVersion; ExpectedSysHash=$ExpectedSysHash.ToUpperInvariant()
        OwnerAuthorized=($Mode -eq 'RunAuthorized' -and [bool]$OwnerAuthorized)
    })
    $native = Invoke-LecS65NativeProcess -Name 'Bounded no-launch preparation' `
        -FilePath $executable -Arguments @('no-launch-prepare') `
        -WorkingDirectory (Split-Path -Parent $executable) `
        -LogPath (Join-Path $EvidenceRoot 'no-launch-child.log') `
        -TimeoutSeconds 5 -ReturnOnNonZeroExit
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'child-result.json') ([ordered]@{
        ExitCode=$native.ExitCode; TimedOut=$native.TimedOut
        JobAssigned=$native.JobAssigned; ProcessTreeClosed=$native.ProcessTreeClosed
        StdOut=$native.StdOut; StdErr=$native.StdErr
    })
    if ($native.TimedOut -or $native.ExitCode -ne 0 -or
        -not [string]::IsNullOrEmpty($native.StdErr) -or
        $native.StdOut -notmatch '(?m)^NO_LAUNCH prepare: success=0 error=50 returned=0 output=0\r?$' -or
        $native.StdOut -notmatch '(?m)^NO_LAUNCH cleanup: token=[1-9][0-9]* returned=0\r?$' -or
        $native.StdOut -notmatch '(?m)^NO_LAUNCH PASS: mapping prepared and released; hardware launch disabled\.\r?$') {
        throw 'No-launch child did not satisfy the exact launch-disabled contract.'
    }
    $after = Get-State
    Assert-State $after
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'after.json') $after
    $resultState = 'PASS'
}
catch {
    $failure = $_.Exception.Message
    Write-ExclusiveText (Join-Path $EvidenceRoot 'exception.txt') (($_ | Out-String) + "`r`n")
}
finally {
    $records = @(Get-ChildItem -LiteralPath $EvidenceRoot -File | Sort-Object Name |
        ForEach-Object { [ordered]@{ Name=$_.Name; Bytes=$_.Length;
            SHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash } })
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'result.json') ([ordered]@{
        Result=$resultState; Failure=$failure; StartedUtc=$startedUtc.ToString('o')
        CompletedUtc=[datetime]::UtcNow.ToString('o'); InvocationCount=1
        HardwareLaunchAuthorized=$false; HardwareLaunchObserved=$false
        Evidence=$records
    })
}
if ($resultState -ne 'PASS') { throw $failure }
Write-Host 'NO-LAUNCH PREPARATION RUNNER PASS'

