<#
.SYNOPSIS
  Audit, plan, execute or verify the guarded WR6k system-image workflow.
.DESCRIPTION
  Audit and Plan are read-only. Execute requires an elevated 64-bit Windows
  PowerShell process, exact disk and volume identities, explicit acknowledgement
  and a new evidence directory. It invokes wbadmin exactly once and never
  formats, partitions, deletes or overwrites a destination. VerifyCatalog is
  read-only. MockInventoryPath is for hardware-independent regression only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Audit','Plan','Execute','VerifyCatalog')]
    [string]$Mode,

    [string]$TargetRoot,
    [string]$ExpectedDiskUniqueId,
    [string]$ExpectedVolumeUniqueId,
    [string]$OutputPath,
    [string]$EvidenceRoot,
    [string]$BackupVersion,
    [switch]$ExecuteAcknowledged,
    [ValidateScript({$_ -ge 137438953472})]
    [uint64]$MinimumFreeBytes = 137438953472,
    [string]$MockInventoryPath,
    [int]$MockWbadminExitCode = 0,
    [int]$MockCatalogExitCode = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'native-process.ps1')
$utf8 = New-Object System.Text.UTF8Encoding($false)
$wbadminPath = "$env:SystemRoot\System32\wbadmin.exe"

function Write-ExclusiveText {
    param([string]$Path, [string]$Text)
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
        try { $writer.Write($Text) } finally { $writer.Dispose() }
    }
    finally { if ($null -ne $stream) { $stream.Dispose() } }
}

function Write-ExclusiveJson {
    param([string]$Path, $Value)
    Write-ExclusiveText -Path $Path -Text ($Value | ConvertTo-Json -Depth 12)
}

function Get-ElevationState {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-RealInventory {
    $os = Get-CimInstance Win32_OperatingSystem
    $systemDrive = [string]$os.SystemDrive
    $disks = @(Get-Disk | ForEach-Object {
        [ordered]@{
            Number = [int]$_.Number
            FriendlyName = [string]$_.FriendlyName
            SerialNumber = [string]$_.SerialNumber
            UniqueId = [string]$_.UniqueId
            BusType = [string]$_.BusType
            PartitionStyle = [string]$_.PartitionStyle
            OperationalStatus = [string]$_.OperationalStatus
            HealthStatus = [string]$_.HealthStatus
            IsBoot = [bool]$_.IsBoot
            IsSystem = [bool]$_.IsSystem
            IsOffline = [bool]$_.IsOffline
            IsReadOnly = [bool]$_.IsReadOnly
            Size = [uint64]$_.Size
        }
    })
    $volumes = @()
    foreach ($volume in @(Get-Volume | Where-Object DriveLetter)) {
        $partition = Get-Partition -DriveLetter $volume.DriveLetter `
            -ErrorAction SilentlyContinue | Select-Object -First 1
        $diskNumber = -1
        $partitionNumber = -1
        if ($null -ne $partition) {
            $diskNumber = [int]$partition.DiskNumber
            $partitionNumber = [int]$partition.PartitionNumber
        }
        $volumes += [ordered]@{
            Root = ([string]$volume.DriveLetter + ':\')
            DiskNumber = $diskNumber
            PartitionNumber = $partitionNumber
            FileSystemLabel = [string]$volume.FileSystemLabel
            FileSystem = [string]$volume.FileSystem
            DriveType = [string]$volume.DriveType
            HealthStatus = [string]$volume.HealthStatus
            Size = [uint64]$volume.Size
            SizeRemaining = [uint64]$volume.SizeRemaining
            UniqueId = [string]$volume.UniqueId
            ExistingWindowsImageBackup = Test-Path -LiteralPath (
                ([string]$volume.DriveLetter + ':\WindowsImageBackup'))
        }
    }
    return [ordered]@{
        Schema = 1
        IsMock = $false
        IsElevated = Get-ElevationState
        SystemDrive = $systemDrive
        Disks = $disks
        Volumes = $volumes
    }
}

function Get-Inventory {
    if ([string]::IsNullOrWhiteSpace($MockInventoryPath)) {
        return Get-RealInventory
    }
    if (-not (Test-Path -LiteralPath $MockInventoryPath -PathType Leaf)) {
        throw "Mock inventory is missing: $MockInventoryPath"
    }
    return Get-Content -Raw -LiteralPath $MockInventoryPath | ConvertFrom-Json
}

function Normalize-Root {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path) -or $Path -notmatch '^[A-Za-z]:\\?$') {
        throw "TargetRoot must be an absolute local volume root such as E:\: $Path"
    }
    return ($Path.Substring(0,1).ToUpperInvariant() + ':\')
}

function Find-Target {
    param($Inventory, [string]$Root)
    $normalized = Normalize-Root $Root
    $volume = @($Inventory.Volumes | Where-Object {
        ([string]$_.Root).ToUpperInvariant() -eq $normalized.ToUpperInvariant()
    })
    if ($volume.Count -ne 1) {
        throw "Target volume was not uniquely identified: $normalized"
    }
    $disk = @($Inventory.Disks | Where-Object {
        [int]$_.Number -eq [int]$volume[0].DiskNumber
    })
    if ($disk.Count -ne 1) {
        throw "Target disk was not uniquely identified for $normalized"
    }
    return [ordered]@{ Root=$normalized; Volume=$volume[0]; Disk=$disk[0] }
}

function Assert-TargetIdentity {
    param($Inventory, $Target)
    if ([string]::IsNullOrWhiteSpace($ExpectedDiskUniqueId) -or
        [string]::IsNullOrWhiteSpace($ExpectedVolumeUniqueId)) {
        throw 'Exact expected disk and volume unique IDs are required.'
    }
    if (-not ([string]$Target.Disk.UniqueId).Equals($ExpectedDiskUniqueId,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Target disk unique ID does not match the approved identity.'
    }
    if (-not ([string]$Target.Volume.UniqueId).Equals($ExpectedVolumeUniqueId,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Target volume unique ID does not match the approved identity.'
    }
    if ([bool]$Target.Disk.IsBoot -or [bool]$Target.Disk.IsSystem) {
        throw 'The system/boot disk cannot be used as the backup target.'
    }
    if ([bool]$Target.Disk.IsOffline -or [bool]$Target.Disk.IsReadOnly -or
        [string]$Target.Disk.HealthStatus -ne 'Healthy' -or
        [string]$Target.Volume.HealthStatus -ne 'Healthy') {
        throw 'The target is not online, writable and healthy.'
    }
    if ([string]$Target.Volume.FileSystem -ne 'NTFS') {
        throw 'The guarded workflow requires an existing NTFS target volume.'
    }
    if ([uint64]$Target.Volume.SizeRemaining -lt $MinimumFreeBytes) {
        throw 'The target does not meet the configured free-space requirement.'
    }
    if ($null -ne $Target.Volume.PSObject.Properties['ExistingWindowsImageBackup'] -and
        [bool]$Target.Volume.ExistingWindowsImageBackup) {
        throw 'The target already contains WindowsImageBackup; overwrite or catalog mutation is refused.'
    }
}

function New-Assessment {
    param($Inventory, $Target)
    $targetRecord = $null
    if ($null -ne $Target) {
        $targetRecord = $Target
    }
    return [ordered]@{
        Schema = 1
        CapturedUtc = [datetime]::UtcNow.ToString('o')
        Mode = $Mode
        Inventory = $Inventory
        Target = $targetRecord
        MinimumFreeBytes = $MinimumFreeBytes
        ProposedCommand = if ($null -eq $Target) { $null } else {
            'wbadmin start backup -backupTarget:{0} -include:{1} -allCritical -quiet' -f
                $Target.Root.TrimEnd('\'), $Inventory.SystemDrive
        }
        Safety = [ordered]@{
            FormatsOrPartitions = $false
            DeletesExistingFiles = $false
            RequiresElevation = $true
            RequiresExplicitExecutionAcknowledgement = $true
            RequiresNewEvidenceDirectory = $true
            RequiresIndependentRestoreValidation = $true
        }
    }
}

$inventory = Get-Inventory
$target = $null
if (-not [string]::IsNullOrWhiteSpace($TargetRoot)) {
    $target = Find-Target -Inventory $inventory -Root $TargetRoot
}

if ($Mode -ne 'Audit') {
    if ($null -eq $target) { throw "$Mode requires -TargetRoot." }
    Assert-TargetIdentity -Inventory $inventory -Target $target
}

$result = New-Assessment -Inventory $inventory -Target $target

if ($Mode -eq 'Execute' -or $Mode -eq 'VerifyCatalog') {
    if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
        throw "$Mode requires -EvidenceRoot."
    }
    $evidence = [System.IO.Path]::GetFullPath($EvidenceRoot)
    if (Test-Path -LiteralPath $evidence) {
        throw "EvidenceRoot already exists: $evidence"
    }
    $parent = Split-Path -Parent $evidence
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "EvidenceRoot parent does not exist: $parent"
    }
    if (-not [bool]$inventory.IsMock -and -not [bool]$inventory.IsElevated) {
        throw "$Mode requires an elevated 64-bit Windows PowerShell process."
    }
    if (-not [bool]$inventory.IsMock -and -not [Environment]::Is64BitProcess) {
        throw "$Mode requires a 64-bit Windows PowerShell process."
    }
    if ($Mode -eq 'Execute' -and -not $ExecuteAcknowledged) {
        throw 'Execute requires -ExecuteAcknowledged.'
    }
    if ($evidence.StartsWith($target.Root,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'EvidenceRoot must not be created on the backup target.'
    }
    New-Item -ItemType Directory -Path $evidence | Out-Null
    Write-ExclusiveJson -Path (Join-Path $evidence 'preflight.json') -Value $result

    $arguments = if ($Mode -eq 'Execute') {
        @('start','backup',('-backupTarget:' + $target.Root.TrimEnd('\')),
            ('-include:' + $inventory.SystemDrive),'-allCritical','-quiet')
    } else {
        @('get','versions',('-backupTarget:' + $target.Root.TrimEnd('\')))
    }
    if ($Mode -eq 'VerifyCatalog' -and
        -not [string]::IsNullOrWhiteSpace($BackupVersion)) {
        $arguments = @('get','items',('-version:' + $BackupVersion),
            ('-backupTarget:' + $target.Root.TrimEnd('\')))
    }

    if ([bool]$inventory.IsMock) {
        Write-ExclusiveText -Path (Join-Path $evidence 'wbadmin.log') -Text (
            "MOCK wbadmin " + ($arguments -join ' ') +
            [Environment]::NewLine + "ExitCode=$MockWbadminExitCode" +
            [Environment]::NewLine)
        $exitCode = $MockWbadminExitCode
    } else {
        if (-not [System.IO.Path]::IsPathRooted($wbadminPath) -or
            -not (Test-Path -LiteralPath $wbadminPath -PathType Leaf)) {
            throw "System WBAdmin is missing or not absolute: $wbadminPath"
        }
        $native = Invoke-LecS65NativeProcess -Name ('WBAdmin ' + $Mode) `
            -FilePath $wbadminPath -Arguments $arguments `
            -WorkingDirectory $repo -LogPath (Join-Path $evidence 'wbadmin.log') `
            -LiveStdOutPath (Join-Path $evidence 'wbadmin.live.stdout.txt') `
            -LiveStdErrPath (Join-Path $evidence 'wbadmin.live.stderr.txt') `
            -ProcessInfoPath (Join-Path $evidence 'wbadmin.process.json') `
            -TimeoutSeconds 43200 -ReturnOnNonZeroExit
        $exitCode = [int]$native.ExitCode
    }

    $catalogExitCode = $null
    if ($Mode -eq 'Execute' -and $exitCode -eq 0) {
        $catalogArguments = @('get','versions',
            ('-backupTarget:' + $target.Root.TrimEnd('\')))
        if ([bool]$inventory.IsMock) {
            Write-ExclusiveText -Path (Join-Path $evidence 'wbadmin-catalog.log') `
                -Text ("MOCK wbadmin " + ($catalogArguments -join ' ') +
                [Environment]::NewLine + "ExitCode=$MockCatalogExitCode" +
                [Environment]::NewLine)
            $catalogExitCode = $MockCatalogExitCode
        } else {
            $catalogNative = Invoke-LecS65NativeProcess `
                -Name 'WBAdmin post-backup catalog verification' `
                -FilePath $wbadminPath -Arguments $catalogArguments `
                -WorkingDirectory $repo `
                -LogPath (Join-Path $evidence 'wbadmin-catalog.log') `
                -TimeoutSeconds 300 -ReturnOnNonZeroExit
            $catalogExitCode = [int]$catalogNative.ExitCode
        }
    }

    $result.Execution = [ordered]@{
        Arguments = $arguments
        ExitCode = $exitCode
        Mock = [bool]$inventory.IsMock
        CatalogQueryOnly = ($Mode -eq 'VerifyCatalog')
        CatalogExitCode = $catalogExitCode
        CatalogEnumerated = ($Mode -eq 'Execute' -and
            $exitCode -eq 0 -and $catalogExitCode -eq 0)
        RestoreValidated = $false
    }
    Write-ExclusiveJson -Path (Join-Path $evidence 'result.json') -Value $result
    if ($exitCode -ne 0) { throw "WBAdmin $Mode failed with exit code $exitCode." }
    if ($Mode -eq 'Execute' -and $catalogExitCode -ne 0) {
        throw "WBAdmin catalog verification failed with exit code $catalogExitCode."
    }
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    Write-ExclusiveJson -Path $OutputPath -Value $result
}
$result
