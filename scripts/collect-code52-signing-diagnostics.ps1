<#
.SYNOPSIS
  Collect read-only Code 52 and test-signing policy evidence.
.DESCRIPTION
  Requires elevated 64-bit Windows PowerShell 5.1. Creates a new evidence
  directory, reads device/boot/policy/signature metadata, and never changes
  BCD, certificate stores, Driver Store, PnP state, services, or devices.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][string]$CandidatePackage,
    [Parameter(Mandatory=$true)][string]$DriverStorePackage,
    [Parameter(Mandatory=$true)][string]$DeviceInstanceId,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9A-Fa-f]{40}$')]
    [string]$CertificateThumbprint,
    [string]$SignTool =
        'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'native-process.ps1')

function Assert-AbsoluteExistingDirectory {
    param([string]$Path, [string]$Name)
    if (-not [System.IO.Path]::IsPathRooted($Path) -or
        -not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Name must be an existing absolute directory: $Path"
    }
}

function Write-JsonFile {
    param([object]$Value, [string]$Path, [int]$Depth = 6)
    $Value | ConvertTo-Json -Depth $Depth |
        Set-Content -LiteralPath $Path -Encoding UTF8
}

function Invoke-ReadOnlyNative {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments,
        [switch]$AllowFailure
    )
    $safeName = $Name -replace '[^A-Za-z0-9_.-]', '-'
    $result = Invoke-LecS65NativeProcess -Name $Name -FilePath $FilePath `
        -Arguments $Arguments -WorkingDirectory $repo `
        -LogPath (Join-Path $script:NativeRoot ($safeName + '.log')) `
        -TimeoutSeconds 120 -ReturnOnNonZeroExit
    if (-not $AllowFailure -and ($result.TimedOut -or $result.ExitCode -ne 0)) {
        throw "$Name failed with exit $($result.ExitCode), timeout=$($result.TimedOut)."
    }
    return $result
}

function Get-CertificateMetadata {
    param([System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate)
    if ($null -eq $Certificate) { return $null }
    [ordered]@{
        Subject = $Certificate.Subject
        Issuer = $Certificate.Issuer
        Thumbprint = $Certificate.Thumbprint
        SerialNumber = $Certificate.SerialNumber
        NotBefore = $Certificate.NotBefore.ToUniversalTime().ToString('o')
        NotAfter = $Certificate.NotAfter.ToUniversalTime().ToString('o')
        HasPrivateKey = $Certificate.HasPrivateKey
        PublicKeyAlgorithm = $Certificate.PublicKey.Oid.Value
        EnhancedKeyUsages = @($Certificate.Extensions | Where-Object {
            $_ -is [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]
        } | ForEach-Object { $_.EnhancedKeyUsages | ForEach-Object Value })
    }
}

function Get-ChainMetadata {
    param([System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate)
    $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
    $chain.ChainPolicy.RevocationMode =
        [System.Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
    $chain.ChainPolicy.VerificationFlags =
        [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::NoFlag
    $built = $chain.Build($Certificate)
    [ordered]@{
        Built = $built
        RevocationMode = 'NoCheck'
        Status = @($chain.ChainStatus | ForEach-Object {
            [ordered]@{ Status = $_.Status.ToString(); Information = $_.StatusInformation.Trim() }
        })
        Elements = @($chain.ChainElements | ForEach-Object {
            [ordered]@{ Subject = $_.Certificate.Subject; Thumbprint = $_.Certificate.Thumbprint }
        })
    }
}

if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5) {
    throw 'This collector requires Windows PowerShell 5.1.'
}
if (-not [Environment]::Is64BitProcess) {
    throw 'This collector requires a 64-bit process.'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This collector requires an elevated administrator token.'
}
if (-not [System.IO.Path]::IsPathRooted($OutputRoot)) {
    throw 'OutputRoot must be absolute.'
}
if (Test-Path -LiteralPath $OutputRoot) {
    throw "OutputRoot already exists: $OutputRoot"
}
Assert-AbsoluteExistingDirectory $CandidatePackage 'CandidatePackage'
Assert-AbsoluteExistingDirectory $DriverStorePackage 'DriverStorePackage'
if (-not (Test-Path -LiteralPath $SignTool -PathType Leaf)) {
    throw "SignTool is missing: $SignTool"
}

New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$script:NativeRoot = Join-Path $OutputRoot 'native'
New-Item -ItemType Directory -Path $script:NativeRoot | Out-Null
$startedUtc = [datetime]::UtcNow
$outcome = 'INCOMPLETE'

try {
    Write-JsonFile ([ordered]@{
        StartedUtc = $startedUtc.ToString('o')
        User = $identity.Name
        Elevated = $true
        Is64Bit = [Environment]::Is64BitProcess
        PowerShell = $PSVersionTable.PSVersion.ToString()
        RepositoryHead = (& git -C $repo rev-parse HEAD).Trim()
        Parameters = [ordered]@{
            CandidatePackage = $CandidatePackage
            DriverStorePackage = $DriverStorePackage
            DeviceInstanceId = $DeviceInstanceId
            CertificateThumbprint = $CertificateThumbprint.ToUpperInvariant()
            SignTool = $SignTool
        }
    }) (Join-Path $OutputRoot 'collector-context.json')

    $device = Get-PnpDevice -InstanceId $DeviceInstanceId -ErrorAction Stop
    $propertyKeys = @(
        'DEVPKEY_Device_ProblemCode',
        'DEVPKEY_Device_DriverInfPath',
        'DEVPKEY_Device_DriverVersion',
        'DEVPKEY_Device_DriverDate',
        'DEVPKEY_Device_Service',
        'DEVPKEY_Device_LocationInfo',
        'DEVPKEY_Device_HardwareIds',
        'DEVPKEY_Device_CompatibleIds'
    )
    $properties = foreach ($key in $propertyKeys) {
        $value = Get-PnpDeviceProperty -InstanceId $DeviceInstanceId `
            -KeyName $key -ErrorAction SilentlyContinue
        [ordered]@{ Key = $key; Data = if ($value) { $value.Data } else { $null } }
    }
    $signedDriver = Get-CimInstance Win32_PnPSignedDriver |
        Where-Object DeviceID -eq $DeviceInstanceId | Select-Object -First 1
    $systemDriver = Get-CimInstance Win32_SystemDriver -Filter "Name='LecS65AcqDrv'"
    Write-JsonFile ([ordered]@{
        CapturedUtc = [datetime]::UtcNow.ToString('o')
        Device = [ordered]@{
            InstanceId = $device.InstanceId
            Class = $device.Class
            FriendlyName = $device.FriendlyName
            Status = $device.Status
            Problem = [int]$device.Problem
        }
        Properties = @($properties)
        SignedDriver = $signedDriver | Select-Object DeviceName,DeviceID,InfName,
            DriverVersion,DriverDate,DriverProviderName,Signer,IsSigned
        Service = $systemDriver | Select-Object Name,State,Status,StartMode,
            PathName,ServiceType,ExitCode,Started
    }) (Join-Path $OutputRoot 'device-service-snapshot.json') 8

    Invoke-ReadOnlyNative 'pnputil-device-drivers' "$env:SystemRoot\System32\pnputil.exe" `
        @('/enum-devices','/instanceid',$DeviceInstanceId,'/drivers') | Out-Null
    Invoke-ReadOnlyNative 'pnputil-driver-packages' "$env:SystemRoot\System32\pnputil.exe" `
        @('/enum-drivers') | Out-Null
    Invoke-ReadOnlyNative 'sc-query' "$env:SystemRoot\System32\sc.exe" `
        @('query','LecS65AcqDrv') -AllowFailure | Out-Null
    Invoke-ReadOnlyNative 'sc-qc' "$env:SystemRoot\System32\sc.exe" `
        @('qc','LecS65AcqDrv') | Out-Null

    $system219 = Get-WinEvent -FilterHashtable @{LogName='System'; Id=219} |
        Where-Object Message -match 'LecS65AcqDrv' | Select-Object -First 1
    $ci3004 = Get-WinEvent -FilterHashtable @{
        LogName='Microsoft-Windows-CodeIntegrity/Operational'; Id=3004
    } | Where-Object Message -match 'LecS65AcqDrv\.sys' | Select-Object -First 1
    if ($null -eq $system219 -or $null -eq $ci3004) {
        throw 'Required Kernel-PnP 219 or Code Integrity 3004 evidence is missing.'
    }
    $system219.ToXml() | Set-Content -LiteralPath `
        (Join-Path $OutputRoot 'kernel-pnp-219.xml') -Encoding UTF8
    $ci3004.ToXml() | Set-Content -LiteralPath `
        (Join-Path $OutputRoot 'code-integrity-3004.xml') -Encoding UTF8

    $eventStart = (Get-Date).AddDays(-7)
    $logs = @(
        'System',
        'Microsoft-Windows-CodeIntegrity/Operational',
        'Microsoft-Windows-Kernel-PnP/Configuration',
        'Microsoft-Windows-UserPnp/DeviceInstall'
    )
    foreach ($log in $logs) {
        $safe = $log -replace '[^A-Za-z0-9_.-]', '-'
        $events = Get-WinEvent -FilterHashtable @{LogName=$log; StartTime=$eventStart} `
            -ErrorAction SilentlyContinue | Where-Object {
                $_.Message -match 'LecS65|VEN_1570|Code Integrity will|boot session|start type'
            } | Select-Object TimeCreated,RecordId,ProviderName,Id,LevelDisplayName,Message
        Write-JsonFile @($events) (Join-Path $OutputRoot ("events-$safe.json")) 5
    }
    $bootEvents = Get-WinEvent -FilterHashtable @{
        LogName='System'; Id=@(12,13,18,20,25,27,32,153,6005,6006,6008,1074); StartTime=$eventStart
    } -ErrorAction SilentlyContinue | Select-Object TimeCreated,RecordId,ProviderName,Id,Message
    Write-JsonFile @($bootEvents) (Join-Path $OutputRoot 'boot-history.json') 5

    $setupApi = Join-Path $env:SystemRoot 'INF\setupapi.dev.log'
    Copy-Item -LiteralPath $setupApi -Destination (Join-Path $OutputRoot 'setupapi.dev.log')
    Select-String -LiteralPath $setupApi -Pattern 'VEN_1570&DEV_0005' -Context 12,12 |
        Out-File -LiteralPath (Join-Path $OutputRoot 'setupapi-lecroy-excerpts.txt') -Encoding utf8

    $bcd = Join-Path $env:SystemRoot 'System32\bcdedit.exe'
    Invoke-ReadOnlyNative 'bcd-current' $bcd @('/enum','{current}') | Out-Null
    Invoke-ReadOnlyNative 'bcd-bootmgr' $bcd @('/enum','{bootmgr}') | Out-Null
    Invoke-ReadOnlyNative 'bcd-all' $bcd @('/enum','all') | Out-Null
    Invoke-ReadOnlyNative 'reagentc-info' "$env:SystemRoot\System32\reagentc.exe" `
        @('/info') -AllowFailure | Out-Null
    Invoke-ReadOnlyNative 'mountvol-list' "$env:SystemRoot\System32\mountvol.exe" `
        @() | Out-Null

    $disks = Get-Disk | Select-Object Number,FriendlyName,SerialNumber,
        PartitionStyle,Size,IsBoot,IsSystem,OperationalStatus
    $partitions = Get-Partition | Select-Object DiskNumber,PartitionNumber,
        DriveLetter,Guid,GptType,Type,Size,Offset,IsBoot,IsSystem,IsHidden,
        AccessPaths
    $volumes = Get-Volume | Select-Object DriveLetter,FileSystemLabel,
        FileSystem,Path,UniqueId,Size,SizeRemaining,HealthStatus
    Write-JsonFile ([ordered]@{
        Disks=@($disks); Partitions=@($partitions); Volumes=@($volumes)
    }) (Join-Path $OutputRoot 'storage-topology.json') 8

    $secureBoot = try {
        [ordered]@{ Supported=$true; Enabled=[bool](Confirm-SecureBootUEFI); Error=$null }
    } catch {
        [ordered]@{ Supported=$false; Enabled=$null; Error=$_.Exception.Message }
    }
    $deviceGuard = Get-CimInstance -Namespace 'root\Microsoft\Windows\DeviceGuard' `
        -ClassName Win32_DeviceGuard -ErrorAction SilentlyContinue
    $registryPolicy = [ordered]@{}
    foreach ($path in @(
        'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard',
        'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity',
        'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy',
        'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DeviceGuard'
    )) {
        $registryPolicy[$path] = if (Test-Path -LiteralPath $path) {
            Get-ItemProperty -LiteralPath $path | Select-Object * -ExcludeProperty PSPath,
                PSParentPath,PSChildName,PSDrive,PSProvider
        } else { $null }
    }
    $ciPolicyFiles = @(
        "$env:SystemRoot\System32\CodeIntegrity\SIPolicy.p7b",
        "$env:SystemRoot\System32\CodeIntegrity\CiPolicies\Active\*.cip"
    ) | ForEach-Object { Get-Item -Path $_ -ErrorAction SilentlyContinue } |
        ForEach-Object {
            [ordered]@{ Path=$_.FullName; Length=$_.Length;
                SHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash }
        }
    Write-JsonFile ([ordered]@{
        SecureBoot = $secureBoot
        DeviceGuard = $deviceGuard
        Registry = $registryPolicy
        ActiveCiPolicyFiles = @($ciPolicyFiles)
    }) (Join-Path $OutputRoot 'security-policy.json') 10

    $computer = Get-CimInstance Win32_OperatingSystem
    $computerSystem = Get-CimInstance Win32_ComputerSystem
    $hotfixes = Get-HotFix | Sort-Object InstalledOn -Descending |
        Select-Object -First 50 HotFixID,Description,InstalledOn,InstalledBy
    $packages = Get-WindowsPackage -Online | Where-Object PackageState -eq 'Installed' |
        Sort-Object InstallTime -Descending | Select-Object -First 50 PackageName,
            PackageState,ReleaseType,InstallTime
    Write-JsonFile ([ordered]@{
        CapturedUtc = [datetime]::UtcNow.ToString('o')
        OperatingSystem = $computer | Select-Object Caption,Version,BuildNumber,
            OSArchitecture,LastBootUpTime,InstallDate
        ComputerSystem = $computerSystem | Select-Object Manufacturer,Model,
            HypervisorPresent
        HotFixes = @($hotfixes)
        Packages = @($packages)
    }) (Join-Path $OutputRoot 'os-updates.json') 8

    $expected = [ordered]@{
        'LecS65AcqDrv.inf'='ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C'
        'LecS65AcqDrv.cat'='2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF'
        'LecS65AcqDrv.sys'='8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A'
    }
    $artifactRecords = @()
    foreach ($set in @(
        [ordered]@{ Name='FrozenCandidate'; Root=$CandidatePackage },
        [ordered]@{ Name='DriverStore'; Root=$DriverStorePackage }
    )) {
        foreach ($name in $expected.Keys) {
            $path = Join-Path $set.Root $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "$($set.Name) artifact missing: $path"
            }
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
            $artifactRecords += [ordered]@{
                Set=$set.Name; Name=$name; Path=$path; SHA256=$hash;
                Expected=$expected[$name]; Matches=($hash -eq $expected[$name])
            }
            if ($hash -ne $expected[$name]) {
                throw "$($set.Name) hash mismatch: $name"
            }
        }
    }
    Write-JsonFile @($artifactRecords) (Join-Path $OutputRoot 'artifact-hashes.json') 5

    $thumb = $CertificateThumbprint.ToUpperInvariant()
    $storeRecords = @()
    $certByStore = @{}
    foreach ($store in @('My','Root','TrustedPublisher')) {
        $cert = Get-ChildItem -LiteralPath ("Cert:\LocalMachine\$store") |
            Where-Object Thumbprint -eq $thumb | Select-Object -First 1
        $certByStore[$store] = $cert
        $storeRecords += [ordered]@{ Store="LocalMachine\$store";
            Present=($null -ne $cert); Certificate=(Get-CertificateMetadata $cert) }
    }
    if ($null -eq $certByStore['My'] -or $null -eq $certByStore['Root'] -or
        $null -eq $certByStore['TrustedPublisher']) {
        throw 'The approved certificate is missing from a required LocalMachine store.'
    }
    $chain = Get-ChainMetadata $certByStore['My']
    Write-JsonFile ([ordered]@{ Stores=$storeRecords; Chain=$chain }) `
        (Join-Path $OutputRoot 'certificate-trust.json') 10

    $signatureRecords = foreach ($set in @(
        [ordered]@{ Name='FrozenCandidate'; Root=$CandidatePackage },
        [ordered]@{ Name='DriverStore'; Root=$DriverStorePackage }
    )) {
        foreach ($name in @('LecS65AcqDrv.sys','LecS65AcqDrv.cat')) {
            $signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $set.Root $name)
            [ordered]@{
                Set=$set.Name; Name=$name; Status=$signature.Status.ToString();
                StatusMessage=$signature.StatusMessage;
                Signer=(Get-CertificateMetadata $signature.SignerCertificate)
            }
        }
    }
    Write-JsonFile @($signatureRecords) (Join-Path $OutputRoot 'authenticode.json') 8

    foreach ($set in @(
        [ordered]@{ Name='candidate'; Root=$CandidatePackage },
        [ordered]@{ Name='driverstore'; Root=$DriverStorePackage }
    )) {
        $sys = Join-Path $set.Root 'LecS65AcqDrv.sys'
        $cat = Join-Path $set.Root 'LecS65AcqDrv.cat'
        Invoke-ReadOnlyNative "signtool-$($set.Name)-sys-pa" $SignTool `
            @('verify','/v','/pa',$sys) | Out-Null
        Invoke-ReadOnlyNative "signtool-$($set.Name)-cat-pa" $SignTool `
            @('verify','/v','/pa',$cat) | Out-Null
        Invoke-ReadOnlyNative "signtool-$($set.Name)-membership" $SignTool `
            @('verify','/v','/pa','/c',$cat,$sys) | Out-Null
        Invoke-ReadOnlyNative "signtool-$($set.Name)-sys-kp" $SignTool `
            @('verify','/v','/kp',$sys) -AllowFailure | Out-Null
    }

    $outcome = 'PASS'
}
catch {
    $outcome = 'FAILED'
    $_ | Format-List * -Force | Out-File -LiteralPath `
        (Join-Path $OutputRoot 'collector-exception.txt') -Encoding utf8
    throw
}
finally {
    if (Test-Path -LiteralPath $OutputRoot -PathType Container) {
        $manifestPath = Join-Path $OutputRoot 'evidence-manifest.json'
        $files = Get-ChildItem -LiteralPath $OutputRoot -File -Recurse |
            Where-Object FullName -ne $manifestPath | Sort-Object FullName |
            ForEach-Object {
                [ordered]@{
                    Path=$_.FullName.Substring($OutputRoot.Length + 1).Replace('\','/')
                    Length=$_.Length
                    SHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
                }
            }
        Write-JsonFile ([ordered]@{
            SchemaVersion=1; StartedUtc=$startedUtc.ToString('o');
            CompletedUtc=[datetime]::UtcNow.ToString('o'); Outcome=$outcome;
            Files=@($files)
        }) $manifestPath 6
    }
}

Write-Host "CODE52 SIGNING DIAGNOSTICS: $outcome"
