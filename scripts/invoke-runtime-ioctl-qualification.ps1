<#
.SYNOPSIS
  Run the permanent WR6k R0/R1 qualification workflow.
.DESCRIPTION
  OfflineMock exercises the production control flow through a fake Win32
  boundary. Preflight performs passive validation only. RunAuthorized performs
  the exact nine approved R0 commands and, after their success, two read-only
  R1 commands. The script never installs, restarts or reconfigures a driver.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('OfflineMock','Preflight','RunAuthorized')]
    [string]$Mode,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [switch]$OwnerAuthorized,
    [ValidateSet('All','Batch3Remaining')]
    [string]$CommandSet = 'All',
    [string]$MockStatePath,
    [string]$MockExecutablePath,
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$MockExecutableSha256,
    [string]$MockScenarioPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$lecdiag = Join-Path $repo 'tools\lecdiag\build\lecdiag.exe'
$runnerScript = Join-Path $PSScriptRoot 'native-process.ps1'
$guardScript = Join-Path $PSScriptRoot 'deviceguard-preflight.ps1'
$expectedLecdiagHash = '0567A21992E95FFBB8E08B24ED10DA1CB4339682CB6DD31CD42F740F99E955D2'
$expectedSysHash = 'D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A'
$expectedInstance = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
$expectedHardware = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00'
$interfaceClasses = @(
    '{958695A4-693A-435E-8297-66F805D8E46A}',
    '{7AC34BE9-F766-4F15-9E88-854BA5E2146E}',
    '{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}',
    '{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}',
    '{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}'
)
$commands = @(
    [pscustomobject]@{ Name='r0-1-build'; Phase='R0'; Args=@('build');
        Pattern='(?m)^driver build:\s+\d+ \(returned=4\)\r?$';
        MockCalls=@('ioctl=0xCFDC21C8 input=0 output=4') },
    [pscustomobject]@{ Name='r0-2-stats'; Phase='R0'; Args=@('stats');
        Pattern='(?m)^stats version:\s+\d+\r?$';
        MockCalls=@('ioctl=0x80006000 input=0 output=48') },
    [pscustomobject]@{ Name='r0-3-bars'; Phase='R0'; Args=@('bars');
        Pattern='(?m)^bars version:\s+\d+, count:\s+\d+, returned=72\r?$';
        MockCalls=@('ioctl=0x80006008 input=0 output=72') },
    [pscustomobject]@{ Name='r0-4-trace'; Phase='R0'; Args=@('trace');
        Pattern='(?m)^trace version:\s+\d+, entries:\s+\d+, total seen:\s+\d+, returned=\d+\r?$';
        MockCalls=@('ioctl=0x8000600C input=0 output=118800') },
    [pscustomobject]@{ Name='r0-5-noop'; Phase='R0'; Args=@('raw-ioctl','0x00222400','','0');
        Pattern='(?m)^IOCTL 0x00222400 succeeded: input=0 output-capacity=0 returned=0\r?$';
        MockCalls=@('ioctl=0x00222400 input=0 output=0 input-hex=') },
    [pscustomobject]@{ Name='r0-6-noop'; Phase='R0'; Args=@('raw-ioctl','0xCFDC2184','','0');
        Pattern='(?m)^IOCTL 0xCFDC2184 succeeded: input=0 output-capacity=0 returned=0\r?$';
        MockCalls=@('ioctl=0xCFDC2184 input=0 output=0 input-hex=') },
    [pscustomobject]@{ Name='r0-7-trace-size'; Phase='R0'; Args=@('raw-ioctl','0x00223004','','4');
        Pattern='(?ms)^IOCTL 0x00223004 succeeded: input=0 output-capacity=4 returned=4\r?\nOutput:10010000\r?$';
        MockCalls=@('ioctl=0x00223004 input=0 output=4 input-hex=') },
    [pscustomobject]@{ Name='r0-8-register-size'; Phase='R0'; Args=@('raw-ioctl','0x00223040','','4');
        Pattern='(?ms)^IOCTL 0x00223040 succeeded: input=0 output-capacity=4 returned=4\r?\nOutput:AE2C0000\r?$';
        MockCalls=@('ioctl=0x00223040 input=0 output=4 input-hex=') },
    [pscustomobject]@{ Name='r0-9-delay-zero'; Phase='R0'; Args=@('raw-ioctl','0x00222C00','00000000','0');
        Pattern='(?m)^IOCTL 0x00222C00 succeeded: input=4 output-capacity=0 returned=0\r?$';
        MockCalls=@('ioctl=0x00222C00 input=4 output=0 input-hex=00000000') },
    [pscustomobject]@{ Name='r1-pci'; Phase='R1'; Args=@('pci');
        Pattern='(?im)^vendor/device:\s+1570:0005\b';
        MockCalls=@('ioctl=0x80006014 input=0 output=84 input-hex=') },
    [pscustomobject]@{ Name='r1-start-register'; Phase='R1'; Args=@('start-register');
        Pattern='(?m)^PASS: legacy 4-byte output matches physical register read\.\r?$';
        MockCalls=@('ioctl=0x00223044 input=0 output=4 input-hex=',
            'ioctl=0xCFDC21C0 input=5 output=4 input-hex=0000000000') }
)
$selectedCommands = if ($CommandSet -eq 'Batch3Remaining') {
    @($commands | Where-Object { $_.Name -in @(
        'r0-8-register-size','r0-9-delay-zero','r1-pci','r1-start-register') })
} else { @($commands) }
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Write-ExclusiveText([string]$Path, [AllowEmptyString()][string]$Text) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = $utf8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

function Write-ExclusiveJson([string]$Path, [object]$Value) {
    Write-ExclusiveText $Path (($Value | ConvertTo-Json -Depth 14) + "`r`n")
}

function Read-JsonUtf8([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "JSON file is missing: $Path" }
    [IO.File]::ReadAllText($Path, $utf8) | ConvertFrom-Json
}

function Get-FileHashValue([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "File is missing: $Path" }
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Get-RealInterfaceState([string]$InstanceId) {
    foreach ($class in $interfaceClasses) {
        $output = & "$env:SystemRoot\System32\pnputil.exe" /enum-interfaces /class $class 2>&1
        $exitCode = $LASTEXITCODE
        $text = @($output) -join [Environment]::NewLine
        $instanceCount = [regex]::Matches($text, [regex]::Escape($InstanceId),
            [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
        $pathCount = [regex]::Matches($text,
            '(?im)^\s*(Interface path|Schnittstellenpfad)\s*:').Count
        $enabled = $text -match
            '(?im)^\s*(Interface status|Schnittstellestatus)\s*:\s*(Enabled|Aktiviert)\s*$'
        [pscustomobject]@{
            ClassGuid=$class; Count=$pathCount; InstanceMatches=$instanceCount
            Published=($exitCode -eq 0 -and $pathCount -eq 1 -and
                $instanceCount -ge 1 -and $enabled)
        }
    }
}

function Get-RealState {
    $matches = @()
    foreach ($candidate in @(Get-PnpDevice -PresentOnly -ErrorAction Stop)) {
        $idProperties = @(Get-PnpDeviceProperty -InstanceId $candidate.InstanceId `
            -KeyName 'DEVPKEY_Device_HardwareIds' -ErrorAction SilentlyContinue |
            Where-Object { $_.PSObject.Properties.Name -contains 'Data' })
        $ids = @($idProperties | ForEach-Object { @($_.Data) })
        if ($ids -contains $expectedHardware) { $matches += $candidate }
    }
    $devices = foreach ($device in $matches) {
        $keys = @('DEVPKEY_Device_HardwareIds','DEVPKEY_Device_DriverInfPath',
            'DEVPKEY_Device_DriverVersion','DEVPKEY_Device_ProblemCode',
            'DEVPKEY_Device_Service','DEVPKEY_Device_LocationInfo')
        $properties = @(Get-PnpDeviceProperty -InstanceId $device.InstanceId `
            -KeyName $keys -ErrorAction Stop)
        $map = @{}
        foreach ($property in $properties) {
            if ($property.PSObject.Properties.Name -contains 'Data') {
                $map[$property.KeyName] = $property.Data
            }
        }
        $serviceName = [string]$map['DEVPKEY_Device_Service']
        [pscustomobject]@{
            InstanceId=$device.InstanceId
            HardwareIds=@($map['DEVPKEY_Device_HardwareIds'])
            Inf=[string]$map['DEVPKEY_Device_DriverInfPath']
            Version=if ($null -eq $map['DEVPKEY_Device_DriverVersion']) { $null } else {
                $map['DEVPKEY_Device_DriverVersion'].ToString()
            }
            ProblemCode=$map['DEVPKEY_Device_ProblemCode']
            DeviceStatus=$device.Status
            ServiceName=$serviceName
            ServiceStatus=if ([string]::IsNullOrWhiteSpace($serviceName)) { $null } else {
                (Get-Service -Name $serviceName -ErrorAction Stop).Status.ToString()
            }
            SysHash=Get-FileHashValue "$env:SystemRoot\System32\drivers\LecS65AcqDrv.sys"
            Location=[string]$map['DEVPKEY_Device_LocationInfo']
            Interfaces=@(Get-RealInterfaceState $device.InstanceId)
        }
    }
    $deviceGuard = Get-CimInstance -Namespace 'root\Microsoft\Windows\DeviceGuard' `
        -ClassName Win32_DeviceGuard -ErrorAction Stop
    $activePolicies = @(
        "$env:SystemRoot\System32\CodeIntegrity\SIPolicy.p7b",
        "$env:SystemRoot\System32\CodeIntegrity\CiPolicies\Active\*.cip"
    ) | ForEach-Object { Get-Item -Path $_ -ErrorAction SilentlyContinue } |
        Select-Object -ExpandProperty FullName
    $blocked = @(Get-Process -ErrorAction Stop | Where-Object {
        if ($_.ProcessName -match '(?i)^(XStream|lecdiag|lecwatch)$') { return $true }
        try { return $_.Path -like 'C:\Program Files\LeCroy\*' } catch { return $false }
    } | ForEach-Object { [pscustomobject]@{ Id=$_.Id; Name=$_.ProcessName; Path=$_.Path } })
    $rollback = [ordered]@{}
    foreach ($item in @(
        @('CAT','C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017\LecS65AcqDrv.cat','DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18'),
        @('INF','C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017\LecS65AcqDrv.inf','C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16'),
        @('SYS','C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017\LecS65AcqDrv.sys','4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687')
    )) {
        $actual = Get-FileHashValue $item[1]
        $rollback[$item[0]] = [ordered]@{ Path=$item[1]; Expected=$item[2]; Actual=$actual;
            Matches=($actual -eq $item[2]) }
    }
    $bcd = @(& "$env:SystemRoot\System32\bcdedit.exe" /enum '{current}') -join "`r`n"
    if ($LASTEXITCODE -ne 0) { throw 'BCDEdit current-entry query failed.' }
    $secureBoot = Confirm-SecureBootUEFI -ErrorAction Stop
    $os = Get-CimInstance Win32_OperatingSystem
    [pscustomobject]@{
        SchemaVersion=1; MockMode=$false
        Host=[pscustomobject]@{ BuildNumber=$os.BuildNumber;
            Is64BitOperatingSystem=[Environment]::Is64BitOperatingSystem }
        Security=[pscustomobject]@{ BcdCurrent=$bcd;
            TestSigning=($bcd -match '(?im)^testsigning\s+Yes\s*$')
            SecureBootEnabled=[bool]$secureBoot; DeviceGuard=$deviceGuard;
            ActiveCiPolicyFiles=@($activePolicies) }
        Devices=@($devices); BlockedClients=@($blocked); Rollback=[pscustomobject]$rollback
    }
}

function Get-CurrentState {
    if ($Mode -eq 'OfflineMock') { return (Read-JsonUtf8 $MockStatePath) }
    Get-RealState
}

function Test-QualificationState([object]$State, [string]$ExecutableHash) {
    $errors = @()
    if ($Mode -eq 'OfflineMock' -and -not [bool]$State.MockMode) {
        $errors += 'OfflineMock state lacks its mock-only marker.'
    }
    if ([int]$State.Host.BuildNumber -ne 19045 -or -not [bool]$State.Host.Is64BitOperatingSystem) {
        $errors += 'Windows identity mismatch.'
    }
    if (-not [bool]$State.Security.TestSigning) { $errors += 'Test signing is not enabled.' }
    if ([bool]$State.Security.SecureBootEnabled) { $errors += 'Secure Boot is enabled.' }
    $guard = Get-LecS65DeviceGuardEvaluation $State.Security.DeviceGuard `
        @($State.Security.ActiveCiPolicyFiles)
    if (-not $guard.Allowed) { $errors += "Device Guard state is $($guard.State)." }
    $matching = @($State.Devices | Where-Object {
        $null -ne $_.PSObject.Properties['HardwareIds'] -and
        @($_.HardwareIds) -contains $expectedHardware
    })
    if ($matching.Count -ne 1) {
        $errors += "Expected one matching PCI function; found $($matching.Count)."
    }
    $device = if ($matching.Count -eq 1) { $matching[0] } else { $null }
    if ($null -ne $device) {
        if ($device.InstanceId -ne $expectedInstance) { $errors += 'PCI instance ID mismatch.' }
        if ($device.Inf -ne 'oem102.inf') { $errors += 'Published INF mismatch.' }
        if ($device.Version -ne '0.2026.1022.1') { $errors += 'Driver version mismatch.' }
        if ($null -eq $device.ProblemCode -or [int]$device.ProblemCode -ne 0 -or
            $device.DeviceStatus -ne 'OK') { $errors += 'Device is not problem-free.' }
        if ($device.ServiceName -ne 'LecS65AcqDrv' -or $device.ServiceStatus -ne 'Running') {
            $errors += 'Driver service is not running.'
        }
        if ($device.SysHash -ne $expectedSysHash) { $errors += 'Loaded SYS hash mismatch.' }
        $interfaces = @($device.Interfaces)
        foreach ($class in $interfaceClasses) {
            $items = @($interfaces | Where-Object { $_.ClassGuid -eq $class })
            if ($items.Count -ne 1 -or [int]$items[0].Count -ne 1 -or
                -not [bool]$items[0].Published) {
                $errors += "Interface class $class is missing, duplicated or disabled."
            }
        }
        if ($interfaces.Count -ne 5) { $errors += 'Unexpected interface class count.' }
    }
    if (@($State.BlockedClients).Count -ne 0) { $errors += 'An acquisition client is running.' }
    foreach ($name in @('CAT','INF','SYS')) {
        if ($null -eq $State.Rollback.PSObject.Properties[$name] -or
            -not [bool]$State.Rollback.$name.Matches) { $errors += "Rollback $name evidence mismatch." }
    }
    $expectedExecutableHash = if ($Mode -eq 'OfflineMock') {
        $MockExecutableSha256.ToUpperInvariant()
    } else { $expectedLecdiagHash }
    if ($ExecutableHash -ne $expectedExecutableHash) { $errors += 'Diagnostic executable hash mismatch.' }
    [pscustomobject]@{ Passed=($errors.Count -eq 0); Errors=@($errors); Device=$device;
        DeviceGuard=$guard; ObservedLocation=if ($device) { $device.Location } else { $null } }
}

function Get-TargetEvents([datetime]$StartUtc) {
    if ($Mode -eq 'OfflineMock') { return @() }
    $result = @()
    foreach ($log in @('System','Microsoft-Windows-CodeIntegrity/Operational',
        'Microsoft-Windows-Kernel-PnP/Configuration','Microsoft-Windows-UserPnp/DeviceInstall')) {
        foreach ($event in @(Get-WinEvent -FilterHashtable @{
            LogName=$log; StartTime=$StartUtc.ToLocalTime()
        } -ErrorAction SilentlyContinue)) {
            $xml = $event.ToXml()
            if ($xml -match '(?i)(VEN_1570&amp;DEV_0005|VEN_1570&DEV_0005|LecS65AcqDrv|oem102\.inf)') {
                $result += [pscustomobject]@{ LogName=$log; RecordId=$event.RecordId;
                    Id=$event.Id; Level=$event.Level;
                    TimeCreatedUtc=$event.TimeCreated.ToUniversalTime().ToString('o');
                    Provider=$event.ProviderName; Message=$event.Message; Xml=$xml }
            }
        }
    }
    @($result | Sort-Object TimeCreatedUtc,LogName,RecordId)
}

function Write-StateCheck([string]$Name, [string]$ExecutableHash) {
    $state = Get-CurrentState
    $evaluation = Test-QualificationState $state $ExecutableHash
    Write-ExclusiveJson (Join-Path $EvidenceRoot ($Name + '.json')) ([ordered]@{
        CapturedUtc=[datetime]::UtcNow.ToString('o'); State=$state; Evaluation=$evaluation
    })
    if (-not $evaluation.Passed) { throw ($evaluation.Errors -join ' ') }
    $events = @(Get-TargetEvents $script:startedUtc)
    Write-ExclusiveJson (Join-Path $EvidenceRoot ($Name + '-target-events.json')) $events
    if (@($events | Where-Object { $_.Level -in @(1,2) -or $_.Id -in @(219,411) }).Count -ne 0) {
        throw "Target kernel/PnP/CI failure detected at $Name."
    }
    $evaluation
}

function Get-MockBehavior([string]$Name) {
    if ($Mode -ne 'OfflineMock' -or $null -eq $script:mockScenario) { return $null }
    $matches = @($script:mockScenario.Behaviors | Where-Object { $_.Name -eq $Name })
    if ($matches.Count -eq 0) { return $null }
    return $matches[0]
}

function Set-MockEnvironment([object]$Behavior) {
    foreach ($name in @('LECS65_MOCK_FAIL_IOCTL','LECS65_MOCK_DELAY_IOCTL',
        'LECS65_MOCK_DELAY_MS','LECS65_MOCK_MISSING_OUTPUT_IOCTL')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    if ($null -eq $Behavior) { return }
    if ($Behavior.Type -eq 'Fail') {
        [Environment]::SetEnvironmentVariable('LECS65_MOCK_FAIL_IOCTL', [string]$Behavior.Ioctl, 'Process')
    } elseif ($Behavior.Type -eq 'Timeout') {
        [Environment]::SetEnvironmentVariable('LECS65_MOCK_DELAY_IOCTL', [string]$Behavior.Ioctl, 'Process')
        [Environment]::SetEnvironmentVariable('LECS65_MOCK_DELAY_MS', '7000', 'Process')
    } elseif ($Behavior.Type -eq 'MissingOutput') {
        [Environment]::SetEnvironmentVariable('LECS65_MOCK_MISSING_OUTPUT_IOCTL', [string]$Behavior.Ioctl, 'Process')
    } else { throw "Unknown mock behavior: $($Behavior.Type)" }
}

function Invoke-ApprovedCommand([object]$Command, [string]$Executable,
    [string]$ExecutableHash) {
    Write-StateCheck ('before-' + $Command.Name) $ExecutableHash | Out-Null
    Write-ExclusiveJson (Join-Path $EvidenceRoot ($Command.Name + '.attempted')) ([ordered]@{
        Name=$Command.Name; Phase=$Command.Phase; Arguments=@($Command.Args)
        AttemptedUtc=[datetime]::UtcNow.ToString('o')
    })
    $behavior = Get-MockBehavior $Command.Name
    Set-MockEnvironment $behavior
    $started = [datetime]::UtcNow
    try {
        $result = Invoke-LecS65NativeProcess -Name $Command.Name -FilePath $Executable `
            -Arguments ([string[]]$Command.Args) -WorkingDirectory (Split-Path -Parent $Executable) `
            -LogPath (Join-Path $EvidenceRoot ($Command.Name + '.log')) `
            -TimeoutSeconds 5 -ReturnOnNonZeroExit
    } catch {
        Write-ExclusiveText (Join-Path $EvidenceRoot ($Command.Name + '.exception.txt')) `
            (($_ | Out-String) + "`r`n")
        throw
    } finally { Set-MockEnvironment $null }
    Write-ExclusiveJson (Join-Path $EvidenceRoot ($Command.Name + '.result.json')) ([ordered]@{
        Name=$Command.Name; Arguments=@($Command.Args); StartedUtc=$started.ToString('o')
        CompletedUtc=[datetime]::UtcNow.ToString('o'); ExitCode=$result.ExitCode
        TimedOut=$result.TimedOut; StdOut=$result.StdOut; StdErr=$result.StdErr
        JobAssigned=$result.JobAssigned; ProcessTreeClosed=$result.ProcessTreeClosed
    })
    if ($result.TimedOut -or $result.ExitCode -ne 0) { throw "$($Command.Name) process failed." }
    if (-not [string]::IsNullOrEmpty($result.StdErr)) { throw "$($Command.Name) wrote stderr." }
    if ($result.StdOut -notmatch $Command.Pattern) { throw "$($Command.Name) output contract failed." }
    if ($Mode -eq 'OfflineMock') {
        $calls = @($result.StdOut -split "`r?`n" | Where-Object { $_ -like 'MOCK_DEVICE_IO_CONTROL *' })
        if ($calls.Count -ne $Command.MockCalls.Count) { throw "$($Command.Name) mock call count mismatch." }
        for ($i=0; $i -lt $calls.Count; ++$i) {
            if ($calls[$i] -notlike ('*' + $Command.MockCalls[$i] + '*')) {
                throw "$($Command.Name) mock IOCTL mismatch."
            }
        }
    }
    Write-StateCheck ('after-' + $Command.Name) $ExecutableHash | Out-Null
    $result.StdOut
}

if (-not [IO.Path]::IsPathRooted($EvidenceRoot)) { throw 'EvidenceRoot must be absolute.' }
if (Test-Path -LiteralPath $EvidenceRoot) { throw "EvidenceRoot already exists: $EvidenceRoot" }
if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5 -or
    -not [Environment]::Is64BitProcess) { throw '64-bit Windows PowerShell 5.1 is required.' }
if ($Mode -ne 'OfflineMock') {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'An elevated token is required.'
    }
}
if ($Mode -eq 'RunAuthorized' -and -not $OwnerAuthorized) {
    throw 'RunAuthorized requires OwnerAuthorized.'
}
if ($CommandSet -eq 'Batch3Remaining' -and $Mode -eq 'Preflight') {
    throw 'Preflight does not accept an active command selection.'
}
if ($Mode -eq 'OfflineMock') {
    foreach ($value in @($MockStatePath,$MockExecutablePath,$MockExecutableSha256)) {
        if ([string]::IsNullOrWhiteSpace($value)) { throw 'OfflineMock requires state, executable and hash.' }
    }
    if ([IO.Path]::GetFullPath($MockExecutablePath) -eq [IO.Path]::GetFullPath($lecdiag)) {
        throw 'OfflineMock refuses the real diagnostic executable.'
    }
}

[void][IO.Directory]::CreateDirectory($EvidenceRoot)
$script:startedUtc = [datetime]::UtcNow
$script:mockScenario = if ($MockScenarioPath) { Read-JsonUtf8 $MockScenarioPath } else { $null }
$outcome = 'FAILED'
$failure = $null
try {
    . $runnerScript
    . $guardScript
    $executable = if ($Mode -eq 'OfflineMock') { [IO.Path]::GetFullPath($MockExecutablePath) } else { $lecdiag }
    $executableHash = Get-FileHashValue $executable
    if ($Mode -eq 'OfflineMock' -and $executableHash -ne $MockExecutableSha256.ToUpperInvariant()) {
        throw 'Mock executable hash mismatch.'
    }
    $head = (& git -C $repo rev-parse HEAD).Trim()
    $dirty = @(& git -C $repo status --porcelain)
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'context.json') ([ordered]@{
        Mode=$Mode; StartedUtc=$script:startedUtc.ToString('o'); GitHead=$head
        GitDirty=@($dirty); PowerShell=$PSVersionTable.PSVersion.ToString()
        Executable=$executable; ExecutableSha256=$executableHash
        CommandSet=$CommandSet
        ApprovedCommands=@($selectedCommands | ForEach-Object {
            [ordered]@{ Name=$_.Name; Phase=$_.Phase; Arguments=@($_.Args) }
        })
    })
    $preflight = Write-StateCheck 'preflight' $executableHash
    if ($Mode -eq 'Preflight') {
        $outcome = 'PASS'
    } else {
        $outputs = [ordered]@{}
        foreach ($command in @($selectedCommands | Where-Object Phase -eq 'R0')) {
            $outputs[$command.Name] = Invoke-ApprovedCommand $command $executable $executableHash
        }
        Write-StateCheck 'post-r0' $executableHash | Out-Null
        foreach ($command in @($selectedCommands | Where-Object Phase -eq 'R1')) {
            $outputs[$command.Name] = Invoke-ApprovedCommand $command $executable $executableHash
        }
        $pci = [regex]::Match($outputs['r1-pci'], '(?im)^BDF:\s+([0-9]+:[0-9]+\.[0-9]+)\s*$')
        $legacy = [regex]::Match($outputs['r1-start-register'],
            '(?im)^0x00223044 START/FVER:\s+(0x[0-9A-F]+)\s*$')
        $physical = [regex]::Match($outputs['r1-start-register'],
            '(?im)^BAR0\+0x000 reference:\s+(0x[0-9A-F]+)\s*$')
        if (-not $pci.Success -or -not $legacy.Success -or -not $physical.Success -or
            $legacy.Groups[1].Value -ne $physical.Groups[1].Value) {
            throw 'R1 structured output validation failed.'
        }
        Write-ExclusiveJson (Join-Path $EvidenceRoot 'r1-values.json') ([ordered]@{
            ObservedBdf=$pci.Groups[1].Value; VendorDevice='1570:0005'
            LegacyStart=$legacy.Groups[1].Value; PhysicalStart=$physical.Groups[1].Value
        })
        Write-StateCheck 'post-r1' $executableHash | Out-Null
        $attempts = @(Get-ChildItem $EvidenceRoot -Filter '*.attempted')
        if ($attempts.Count -ne $selectedCommands.Count) {
            throw "Expected $($selectedCommands.Count) attempt markers; found $($attempts.Count)."
        }
        $outcome = 'PASS'
    }
} catch {
    $failure = ($_ | Out-String)
    if (-not (Test-Path (Join-Path $EvidenceRoot 'failure.txt'))) {
        Write-ExclusiveText (Join-Path $EvidenceRoot 'failure.txt') ($failure + "`r`n")
    }
} finally {
    Set-MockEnvironment $null
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'outcome.json') ([ordered]@{
        Mode=$Mode; Outcome=$outcome; StartedUtc=$script:startedUtc.ToString('o')
        CompletedUtc=[datetime]::UtcNow.ToString('o'); Failure=$failure
    })
    $entries = @(Get-ChildItem $EvidenceRoot -File -Recurse | Sort FullName | ForEach-Object {
        [pscustomobject]@{ Path=$_.FullName.Substring($EvidenceRoot.Length).TrimStart('\');
            Length=$_.Length; SHA256=(Get-FileHash -Algorithm SHA256 $_.FullName).Hash }
    })
    Write-ExclusiveJson (Join-Path $EvidenceRoot 'manifest.json') $entries
}
if ($outcome -ne 'PASS') { exit 1 }
exit 0
