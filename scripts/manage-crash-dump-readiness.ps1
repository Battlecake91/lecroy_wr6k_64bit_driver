<#
.SYNOPSIS
  Audit, plan, apply, verify or restore the bounded WR6k crash-dump policy.
.DESCRIPTION
  Audit and Plan are read-only. Apply changes only CrashDumpEnabled and
  AutoReboot after writing an exclusive backup. The system-managed page file
  is deliberately preserved and must be verified after the required reboot.
  Restore reinstates the exact backed-up CrashControl values. MockStatePath is
  for hardware-independent regression tests only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Audit','Plan','Apply','Verify','Restore')]
    [string]$Mode,

    [string]$OutputPath,
    [string]$BackupPath,
    [switch]$ApplyAcknowledged,
    [string]$MockStatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$crashKeyPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl'
$crashSubKeyPath = 'SYSTEM\CurrentControlSet\Control\CrashControl'
$memoryKeyPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Write-ExclusiveJson {
    param([Parameter(Mandatory=$true)][string]$Path,
          [Parameter(Mandatory=$true)]$Value)
    $full = [System.IO.Path]::GetFullPath($Path)
    $parent = Split-Path -Parent $full
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Output parent does not exist: $parent"
    }
    $json = $Value | ConvertTo-Json -Depth 12
    $stream = New-Object System.IO.FileStream($full,
        [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.StreamWriter($stream, $utf8)
        try { $writer.Write($json) } finally { $writer.Dispose() }
    }
    finally { if ($null -ne $stream) { $stream.Dispose() } }
}

function Get-RegistryValueRecord {
    param([Parameter(Mandatory=$true)]$Key,
          [Parameter(Mandatory=$true)][string]$Name)
    $exists = $Key.GetValueNames() -contains $Name
    if (-not $exists) {
        return [ordered]@{ Exists=$false; Value=$null; Kind=$null }
    }
    return [ordered]@{
        Exists = $true
        Value = $Key.GetValue($Name, $null,
            [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        Kind = $Key.GetValueKind($Name).ToString()
    }
}

function Get-RealState {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $elevated = $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    $crashKey = Get-Item -LiteralPath $crashKeyPath
    $memoryKey = Get-Item -LiteralPath $memoryKeyPath
    $os = Get-CimInstance Win32_OperatingSystem
    $computer = Get-CimInstance Win32_ComputerSystem
    $systemDrive = $os.SystemDrive
    $volume = Get-CimInstance Win32_LogicalDisk -Filter (
        "DeviceID='$systemDrive'")
    $usage = @(Get-CimInstance Win32_PageFileUsage)
    return [ordered]@{
        Schema = 1
        IsMock = $false
        IsElevated = $elevated
        OsBuild = $os.BuildNumber
        RamBytes = [uint64]$computer.TotalPhysicalMemory
        SystemDrive = $systemDrive
        SystemDriveFreeBytes = [uint64]$volume.FreeSpace
        FixedDiskCount = @(Get-Disk | Where-Object BusType -ne 'USB').Count
        AutomaticManagedPagefile = [bool]$computer.AutomaticManagedPagefile
        PagingFiles = @($memoryKey.GetValue('PagingFiles'))
        PageFileUsage = @($usage | ForEach-Object {
            [ordered]@{ Name=$_.Name; AllocatedMiB=[int]$_.AllocatedBaseSize;
                CurrentMiB=[int]$_.CurrentUsage; PeakMiB=[int]$_.PeakUsage }
        })
        CrashControl = [ordered]@{
            CrashDumpEnabled = Get-RegistryValueRecord $crashKey 'CrashDumpEnabled'
            AutoReboot = Get-RegistryValueRecord $crashKey 'AutoReboot'
            DumpFile = Get-RegistryValueRecord $crashKey 'DumpFile'
            Overwrite = Get-RegistryValueRecord $crashKey 'Overwrite'
            DedicatedDumpFile = Get-RegistryValueRecord $crashKey 'DedicatedDumpFile'
        }
    }
}

function Get-State {
    if ([string]::IsNullOrWhiteSpace($MockStatePath)) { return Get-RealState }
    if (-not (Test-Path -LiteralPath $MockStatePath -PathType Leaf)) {
        throw "Mock state is missing: $MockStatePath"
    }
    return Get-Content -Raw -LiteralPath $MockStatePath | ConvertFrom-Json
}

function Save-MockState {
    param([Parameter(Mandatory=$true)]$State)
    $json = $State | ConvertTo-Json -Depth 12
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($MockStatePath), $json, $utf8)
}

function Set-CrashValue {
    param([Parameter(Mandatory=$true)]$State,
          [Parameter(Mandatory=$true)][string]$Name,
          [Parameter(Mandatory=$true)][int]$Value)
    if (-not [string]::IsNullOrWhiteSpace($MockStatePath)) {
        $record = $State.CrashControl.$Name
        $record.Exists = $true
        $record.Value = $Value
        $record.Kind = 'DWord'
        Save-MockState $State
        return
    }
    $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
        $crashSubKeyPath, $true)
    if ($null -eq $key) {
        throw "Unable to open CrashControl for bounded write access."
    }
    try {
        $key.SetValue($Name, $Value,
            [Microsoft.Win32.RegistryValueKind]::DWord)
    }
    finally {
        $key.Dispose()
    }
}

function Restore-CrashValue {
    param([Parameter(Mandatory=$true)]$State,
          [Parameter(Mandatory=$true)][string]$Name,
          [Parameter(Mandatory=$true)]$Record)
    if (-not [string]::IsNullOrWhiteSpace($MockStatePath)) {
        $target = $State.CrashControl.$Name
        $target.Exists = [bool]$Record.Exists
        $target.Value = $Record.Value
        $target.Kind = $Record.Kind
        Save-MockState $State
        return
    }
    $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
        $crashSubKeyPath, $true)
    if ($null -eq $key) {
        throw "Unable to open CrashControl for bounded restore access."
    }
    try {
        if (-not [bool]$Record.Exists) {
            $key.DeleteValue($Name, $false)
            return
        }
        $kind = [Enum]::Parse([Microsoft.Win32.RegistryValueKind],
            [string]$Record.Kind)
        $key.SetValue($Name, $Record.Value, $kind)
    }
    finally {
        $key.Dispose()
    }
}

function Assert-WriteAuthorized {
    param([Parameter(Mandatory=$true)]$State)
    if (-not $ApplyAcknowledged) {
        throw 'Apply or Restore requires -ApplyAcknowledged.'
    }
    if (-not [bool]$State.IsElevated) {
        throw 'Apply or Restore requires an elevated 64-bit Windows PowerShell process.'
    }
}

function New-Assessment {
    param([Parameter(Mandatory=$true)]$State)
    $crash = $State.CrashControl
    $allocated = @($State.PageFileUsage | ForEach-Object { [int]$_.AllocatedMiB })
    return [ordered]@{
        Schema = 1
        CapturedUtc = [datetime]::UtcNow.ToString('o')
        Mode = $Mode
        Current = $State
        Proposed = [ordered]@{
            CrashDumpEnabled = 2
            AutoReboot = 0
            DumpFile = '%SystemRoot%\MEMORY.DMP'
            PreserveOverwrite = $true
            PreserveSystemManagedPagefile = $true
            DedicatedDumpFile = $null
        }
        Assessment = [ordered]@{
            KernelDumpSelected = ([bool]$crash.CrashDumpEnabled.Exists -and
                [int]$crash.CrashDumpEnabled.Value -eq 2)
            AutoRebootDisabled = ([bool]$crash.AutoReboot.Exists -and
                [int]$crash.AutoReboot.Value -eq 0)
            SystemManagedPagefile = [bool]$State.AutomaticManagedPagefile
            AllocatedPageFileMiB = if ($allocated.Count) {
                ($allocated | Measure-Object -Sum).Sum } else { 0 }
            PostRebootSizingVerificationRequired = $true
            DedicatedDumpFileFeasible = ([int]$State.FixedDiskCount -gt 1)
            ChangesRequireReboot = $true
        }
        Rationale = @(
            'Kernel dumps require backing for kernel virtual-memory usage, not RAM plus 257 MiB.',
            'The RAM-plus-header rule applies to complete dumps, not the selected kernel dump.',
            'Keeping the page file system-managed is the smallest change and lets Windows size it for the selected dump policy.',
            'The allocated page-file size and free space must be rechecked after the required reboot before Driver Verifier.',
            'A dedicated dump file on the same only internal volume provides no capacity or failure-domain advantage.'
        )
    }
}

$state = Get-State
$result = New-Assessment $state

switch ($Mode) {
    'Audit' { }
    'Plan' { }
    'Verify' {
        if (-not $result.Assessment.KernelDumpSelected -or
            -not $result.Assessment.AutoRebootDisabled -or
            -not $result.Assessment.SystemManagedPagefile) {
            throw 'Crash-dump readiness verification failed.'
        }
    }
    'Apply' {
        Assert-WriteAuthorized $state
        if ([string]::IsNullOrWhiteSpace($BackupPath)) {
            throw 'Apply requires a new -BackupPath.'
        }
        $backup = [ordered]@{
            Schema=1; CapturedUtc=[datetime]::UtcNow.ToString('o')
            CrashDumpEnabled=$state.CrashControl.CrashDumpEnabled
            AutoReboot=$state.CrashControl.AutoReboot
            PagefileChanged=$false
        }
        Write-ExclusiveJson -Path $BackupPath -Value $backup
        Set-CrashValue $state 'CrashDumpEnabled' 2
        $state = Get-State
        Set-CrashValue $state 'AutoReboot' 0
        $result = New-Assessment (Get-State)
    }
    'Restore' {
        Assert-WriteAuthorized $state
        if ([string]::IsNullOrWhiteSpace($BackupPath) -or
            -not (Test-Path -LiteralPath $BackupPath -PathType Leaf)) {
            throw 'Restore requires an existing -BackupPath.'
        }
        $backup = Get-Content -Raw -LiteralPath $BackupPath | ConvertFrom-Json
        if ($backup.Schema -ne 1 -or $backup.PagefileChanged -ne $false) {
            throw 'Crash-dump backup schema or page-file contract is invalid.'
        }
        Restore-CrashValue $state 'CrashDumpEnabled' $backup.CrashDumpEnabled
        $state = Get-State
        Restore-CrashValue $state 'AutoReboot' $backup.AutoReboot
        $result = New-Assessment (Get-State)
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    Write-ExclusiveJson -Path $OutputPath -Value $result
}
$result
