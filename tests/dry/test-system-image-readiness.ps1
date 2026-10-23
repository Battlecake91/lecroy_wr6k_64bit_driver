[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$scriptPath = Join-Path $repo 'scripts\manage-system-image-readiness.ps1'
$root = Join-Path ([System.IO.Path]::GetTempPath()) `
    ('lec-image-readiness-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$script:passed = 0
$script:failed = 0

function Test-Condition([string]$Name, [bool]$Condition) {
    Write-Host ('[{0}] {1}' -f $(if($Condition){'PASS'}else{'FAIL'}),$Name)
    if($Condition){++$script:passed}else{++$script:failed}
}

function Expect-Rejection([string]$Name, [scriptblock]$Body) {
    $rejected = $false
    try { & $Body | Out-Null } catch { $rejected = $true }
    Test-Condition $Name $rejected
}

function Write-Inventory([string]$Path, [string]$FileSystem='NTFS',
        [uint64]$FreeBytes=200000000000,
        [bool]$ExistingWindowsImageBackup=$false) {
    $inventory = [ordered]@{
        Schema=1; IsMock=$true; IsElevated=$true; SystemDrive='C:'
        Disks=@(
            [ordered]@{Number=0;FriendlyName='System NVMe';SerialNumber='SYS';
                UniqueId='disk-system';BusType='NVMe';PartitionStyle='GPT';
                OperationalStatus='Online';HealthStatus='Healthy';IsBoot=$true;
                IsSystem=$true;IsOffline=$false;IsReadOnly=$false;Size=256000000000},
            [ordered]@{Number=1;FriendlyName='External Backup';SerialNumber='EXT123';
                UniqueId='disk-external-123';BusType='USB';PartitionStyle='GPT';
                OperationalStatus='Online';HealthStatus='Healthy';IsBoot=$false;
                IsSystem=$false;IsOffline=$false;IsReadOnly=$false;Size=512000000000}
        )
        Volumes=@(
            [ordered]@{Root='C:\';DiskNumber=0;PartitionNumber=2;
                FileSystemLabel='System';FileSystem='NTFS';DriveType='Fixed';
                HealthStatus='Healthy';Size=118000000000;SizeRemaining=17000000000;
                UniqueId='volume-system';ExistingWindowsImageBackup=$false},
            [ordered]@{Root='E:\';DiskNumber=1;PartitionNumber=1;
                FileSystemLabel='WR6KBACKUP';FileSystem=$FileSystem;DriveType='Removable';
                HealthStatus='Healthy';Size=500000000000;SizeRemaining=$FreeBytes;
                UniqueId='volume-external-456';
                ExistingWindowsImageBackup=$ExistingWindowsImageBackup}
        )
    }
    $inventory | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Path -Encoding UTF8
}

try {
    $inventory = Join-Path $root 'inventory.json'
    Write-Inventory $inventory
    $before = (Get-FileHash $inventory -Algorithm SHA256).Hash
    $audit = & $scriptPath -Mode Audit -MockInventoryPath $inventory
    Test-Condition 'audit is read-only and inventories both disks' (
        (Get-FileHash $inventory -Algorithm SHA256).Hash -eq $before -and
        @($audit.Inventory.Disks).Count -eq 2)

    $plan = & $scriptPath -Mode Plan -MockInventoryPath $inventory `
        -TargetRoot 'e:\' -ExpectedDiskUniqueId 'DISK-EXTERNAL-123' `
        -ExpectedVolumeUniqueId 'VOLUME-EXTERNAL-456'
    Test-Condition 'plan binds exact disk and volume identities' (
        $plan.Target.Disk.Number -eq 1 -and $plan.Target.Root -eq 'E:\')
    Test-Condition 'plan includes C and all critical volumes' (
        $plan.ProposedCommand -eq
        'wbadmin start backup -backupTarget:E: -include:C: -allCritical -quiet')
    Test-Condition 'plan records non-destructive and restore-test boundaries' (
        -not $plan.Safety.FormatsOrPartitions -and
        -not $plan.Safety.DeletesExistingFiles -and
        $plan.Safety.RequiresIndependentRestoreValidation)

    Expect-Rejection 'plan rejects missing approved identities' {
        & $scriptPath -Mode Plan -MockInventoryPath $inventory -TargetRoot 'E:\'
    }
    Expect-Rejection 'plan rejects disk identity mismatch' {
        & $scriptPath -Mode Plan -MockInventoryPath $inventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'wrong' -ExpectedVolumeUniqueId 'volume-external-456'
    }
    Expect-Rejection 'plan rejects the system volume' {
        & $scriptPath -Mode Plan -MockInventoryPath $inventory -TargetRoot 'C:\' `
            -ExpectedDiskUniqueId 'disk-system' -ExpectedVolumeUniqueId 'volume-system'
    }

    $smallInventory = Join-Path $root 'small.json'
    Write-Inventory $smallInventory 'NTFS' 1000000000
    Expect-Rejection 'plan rejects insufficient free space' {
        & $scriptPath -Mode Plan -MockInventoryPath $smallInventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456'
    }
    $fatInventory = Join-Path $root 'fat.json'
    Write-Inventory $fatInventory 'exFAT'
    Expect-Rejection 'plan rejects a non-NTFS target' {
        & $scriptPath -Mode Plan -MockInventoryPath $fatInventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456'
    }
    $existingBackupInventory = Join-Path $root 'existing-backup.json'
    Write-Inventory $existingBackupInventory 'NTFS' 200000000000 $true
    Expect-Rejection 'plan refuses an existing WindowsImageBackup tree' {
        & $scriptPath -Mode Plan -MockInventoryPath $existingBackupInventory `
            -TargetRoot 'E:\' -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456'
    }

    $noAck = Join-Path $root 'no-ack'
    Expect-Rejection 'execute requires explicit acknowledgement' {
        & $scriptPath -Mode Execute -MockInventoryPath $inventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456' -EvidenceRoot $noAck
    }
    Test-Condition 'rejected execute creates no evidence directory' (-not (Test-Path $noAck))

    $success = Join-Path $root 'success'
    $executed = & $scriptPath -Mode Execute -MockInventoryPath $inventory `
        -TargetRoot 'E:\' -ExpectedDiskUniqueId 'disk-external-123' `
        -ExpectedVolumeUniqueId 'volume-external-456' -EvidenceRoot $success `
        -ExecuteAcknowledged
    Test-Condition 'mock execute records one successful exact command' (
        $executed.Execution.ExitCode -eq 0 -and
        $executed.Execution.CatalogEnumerated -and
        ($executed.Execution.Arguments -join ' ') -eq
        'start backup -backupTarget:E: -include:C: -allCritical -quiet')
    Test-Condition 'mock execute preserves preflight log and result' (
        (Test-Path (Join-Path $success 'preflight.json')) -and
        (Test-Path (Join-Path $success 'wbadmin.log')) -and
        (Test-Path (Join-Path $success 'wbadmin-catalog.log')) -and
        (Test-Path (Join-Path $success 'result.json')))
    Expect-Rejection 'existing evidence directory cannot be reused' {
        & $scriptPath -Mode Execute -MockInventoryPath $inventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456' -EvidenceRoot $success `
            -ExecuteAcknowledged
    }

    $failure = Join-Path $root 'failure'
    Expect-Rejection 'nonzero wbadmin exit stops and preserves evidence' {
        & $scriptPath -Mode Execute -MockInventoryPath $inventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456' -EvidenceRoot $failure `
            -ExecuteAcknowledged -MockWbadminExitCode 5
    }
    Test-Condition 'failed attempt retains result and process log' (
        (Test-Path (Join-Path $failure 'result.json')) -and
        (Test-Path (Join-Path $failure 'wbadmin.log')))

    $catalogFailure = Join-Path $root 'catalog-failure'
    Expect-Rejection 'catalog failure stops after one successful backup attempt' {
        & $scriptPath -Mode Execute -MockInventoryPath $inventory -TargetRoot 'E:\' `
            -ExpectedDiskUniqueId 'disk-external-123' `
            -ExpectedVolumeUniqueId 'volume-external-456' `
            -EvidenceRoot $catalogFailure -ExecuteAcknowledged `
            -MockCatalogExitCode 9
    }
    Test-Condition 'catalog failure preserves both operation logs and result' (
        (Test-Path (Join-Path $catalogFailure 'wbadmin.log')) -and
        (Test-Path (Join-Path $catalogFailure 'wbadmin-catalog.log')) -and
        (Test-Path (Join-Path $catalogFailure 'result.json')))

    $catalog = Join-Path $root 'catalog'
    $verified = & $scriptPath -Mode VerifyCatalog -MockInventoryPath $inventory `
        -TargetRoot 'E:\' -ExpectedDiskUniqueId 'disk-external-123' `
        -ExpectedVolumeUniqueId 'volume-external-456' -EvidenceRoot $catalog `
        -BackupVersion '10/23/2026-12:00'
    Test-Condition 'catalog verification is read-only and version-bound' (
        $verified.Execution.CatalogQueryOnly -and
        ($verified.Execution.Arguments -join ' ') -eq
        'get items -version:10/23/2026-12:00 -backupTarget:E:')
    Test-Condition 'catalog success does not claim restore validation' (
        -not $verified.Execution.RestoreValidated)
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}

Write-Host ("SYSTEM IMAGE READINESS: {0}/{1} passed; {2} failed." -f
    $script:passed,($script:passed+$script:failed),$script:failed)
if($script:failed){throw 'System-image readiness tests failed.'}
