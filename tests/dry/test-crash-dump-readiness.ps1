[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Join-Path $repo 'scripts\manage-crash-dump-readiness.ps1'
$scriptText = Get-Content -Raw -LiteralPath $script
$root = Join-Path ([System.IO.Path]::GetTempPath()) `
    ('lec-crash-readiness-' + [guid]::NewGuid().ToString('N'))
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

function Write-State([string]$Path, [bool]$Elevated = $true) {
    $state = [ordered]@{
        Schema=1; IsMock=$true; IsElevated=$Elevated; OsBuild='19045'
        RamBytes=8458805248; SystemDrive='C:'
        SystemDriveFreeBytes=17338515456; FixedDiskCount=1
        AutomaticManagedPagefile=$true
        PagingFiles=@('C:\pagefile.sys 0 0')
        PageFileUsage=@([ordered]@{Name='C:\pagefile.sys';AllocatedMiB=1280;CurrentMiB=175;PeakMiB=183})
        CrashControl=[ordered]@{
            CrashDumpEnabled=[ordered]@{Exists=$true;Value=7;Kind='DWord'}
            AutoReboot=[ordered]@{Exists=$true;Value=1;Kind='DWord'}
            DumpFile=[ordered]@{Exists=$true;Value='C:\Windows\MEMORY.DMP';Kind='ExpandString'}
            Overwrite=[ordered]@{Exists=$true;Value=1;Kind='DWord'}
            DedicatedDumpFile=[ordered]@{Exists=$false;Value=$null;Kind=$null}
        }
    }
    $state | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Path -Encoding UTF8
}

$statePath = Join-Path $root 'state.json'
Write-State $statePath
$before = (Get-FileHash -Algorithm SHA256 $statePath).Hash
$auditPath = Join-Path $root 'audit.json'
$audit = & $script -Mode Audit -MockStatePath $statePath -OutputPath $auditPath
Test-Condition 'audit is read-only' ((Get-FileHash -Algorithm SHA256 $statePath).Hash -eq $before)
Test-Condition 'real registry writes request an explicitly writable key' (
    $scriptText -match 'OpenSubKey\(\s*\$crashSubKeyPath,\s*\$true\)' -and
    $scriptText -notmatch '\$key\s*=\s*Get-Item\s+-LiteralPath\s+\$crashKeyPath\s*\r?\n\s*\$key\.SetValue')
Test-Condition 'audit reports current automatic dump and reboot state' (
    -not $audit.Assessment.KernelDumpSelected -and -not $audit.Assessment.AutoRebootDisabled)
Test-Condition 'audit preserves system-managed page-file evidence' (
    $audit.Assessment.SystemManagedPagefile -and $audit.Assessment.AllocatedPageFileMiB -eq 1280)
Test-Condition 'single internal disk rejects dedicated-dump advantage' (
    -not $audit.Assessment.DedicatedDumpFileFeasible)
Expect-Rejection 'existing output is rejected' {
    & $script -Mode Audit -MockStatePath $statePath -OutputPath $auditPath
}
Expect-Rejection 'apply requires explicit acknowledgement' {
    & $script -Mode Apply -MockStatePath $statePath -BackupPath (Join-Path $root 'noack.json')
}

$lowState = Join-Path $root 'low-state.json'
Write-State $lowState $false
Expect-Rejection 'non-elevated apply is rejected' {
    & $script -Mode Apply -MockStatePath $lowState -ApplyAcknowledged `
        -BackupPath (Join-Path $root 'low-backup.json')
}

$backup = Join-Path $root 'backup.json'
$applyResult = & $script -Mode Apply -MockStatePath $statePath `
    -ApplyAcknowledged -BackupPath $backup
$applied = Get-Content -Raw $statePath | ConvertFrom-Json
Test-Condition 'apply creates exact backup and bounded settings' (
    (Test-Path $backup) -and $applied.CrashControl.CrashDumpEnabled.Value -eq 2 -and
    $applied.CrashControl.AutoReboot.Value -eq 0)
Test-Condition 'apply preserves page file and overwrite policy' (
    $applied.AutomaticManagedPagefile -and $applied.PageFileUsage[0].AllocatedMiB -eq 1280 -and
    $applied.CrashControl.Overwrite.Value -eq 1)
$verify = & $script -Mode Verify -MockStatePath $statePath
Test-Condition 'verify accepts the proposed state' (
    $verify.Assessment.KernelDumpSelected -and $verify.Assessment.AutoRebootDisabled)
Expect-Rejection 'existing backup cannot be overwritten' {
    & $script -Mode Apply -MockStatePath $statePath -ApplyAcknowledged -BackupPath $backup
}

$restore = & $script -Mode Restore -MockStatePath $statePath `
    -ApplyAcknowledged -BackupPath $backup
$restored = Get-Content -Raw $statePath | ConvertFrom-Json
Test-Condition 'restore returns exact original CrashControl values' (
    $restored.CrashControl.CrashDumpEnabled.Value -eq 7 -and
    $restored.CrashControl.AutoReboot.Value -eq 1 -and
    $restored.CrashControl.Overwrite.Value -eq 1)

$badBackup = Join-Path $root 'bad-backup.json'
'{"Schema":2,"PagefileChanged":false}' | Set-Content $badBackup
Expect-Rejection 'malformed backup is rejected' {
    & $script -Mode Restore -MockStatePath $statePath -ApplyAcknowledged -BackupPath $badBackup
}

Remove-Item -LiteralPath $root -Recurse -Force
Write-Host ("CRASH DUMP READINESS: {0}/{1} passed; {2} failed." -f
    $script:passed,($script:passed+$script:failed),$script:failed)
if($script:failed){throw 'Crash-dump readiness tests failed.'}
