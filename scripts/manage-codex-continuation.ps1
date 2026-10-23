<#
.SYNOPSIS
  Prepare and validate a fail-closed, one-time Codex continuation record.
.DESCRIPTION
  The script records repository and authorization state before an independently
  authorized reboot. Registration creates an interactive-logon Scheduled Task
  that runs only this script in Resume mode. Resume validates the manifest,
  host, worktree, branch and HEAD, then uses the supported noninteractive
  `codex exec resume` command for one exact session UUID. The bounded prompt is
  hashed in the manifest and cannot expand the recorded authorization scope.
  No arbitrary saved command is executed.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Prepare','Validate','Register','Resume','Cleanup')]
    [string]$Mode,
    [Parameter(Mandatory=$true)][string]$ManifestPath,
    [string]$WorktreePath,
    [string]$ExpectedBranch,
    [ValidatePattern('^$|^[0-9a-fA-F]{40}$')][string]$ExpectedHead,
    [string]$Phase,
    [string[]]$CompletedSteps = @(),
    [string[]]$ConsumedAuthorizations = @(),
    [string[]]$EvidencePaths = @(),
    [string]$NextPermittedAction,
    [ValidatePattern('^$|^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')]
    [string]$CodexThreadId,
    [string]$ContinuationPromptPath,
    [string]$CodexExecutablePath,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')][string]$ExpectedCodexSha256,
    [string]$ExpectedCodexVersion,
    [ValidateRange(60,3600)][int]$ResumeTimeoutSeconds = 900,
    [ValidatePattern('^LecS65-Codex-Continuation-[A-Za-z0-9_-]{1,48}$')]
    [string]$TaskName,
    [switch]$AcknowledgePreparation,
    [switch]$AcknowledgeRegistration,
    [switch]$TestMode,
    [string]$MockTaskRoot,
    [string]$MockHostPath,
    [int]$MockChildExitCode = 0,
    [switch]$MockChildTimeout,
    [string]$MockCodexVersion = 'codex-cli mock',
    [switch]$KeepTaskAfterResume
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$scriptPath = [IO.Path]::GetFullPath($MyInvocation.MyCommand.Path)

function Get-AbsolutePath([string]$Path, [string]$Label) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Path]::IsPathRooted($Path)) {
        throw "$Label must be an absolute path."
    }
    [IO.Path]::GetFullPath($Path)
}

function Write-ExclusiveUtf8([string]$Path, [string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Parent directory does not exist: $parent"
    }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = $utf8.GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

function Write-ExclusiveJson([string]$Path, [object]$Value) {
    Write-ExclusiveUtf8 $Path (($Value | ConvertTo-Json -Depth 16) + "`r`n")
}

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "File is missing: $Path"
    }
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Get-EvidenceIdentity([string]$Path) {
    $full = Get-AbsolutePath $Path 'EvidencePath'
    if (Test-Path -LiteralPath $full -PathType Leaf) {
        return [ordered]@{ Path=$full; Kind='File'; Length=(Get-Item $full).Length;
            SHA256=Get-Sha256 $full }
    }
    if (-not (Test-Path -LiteralPath $full -PathType Container)) {
        throw "Evidence path does not exist: $full"
    }
    $records = @(Get-ChildItem -LiteralPath $full -File -Recurse | Sort-Object FullName |
        ForEach-Object {
            [ordered]@{ RelativePath=$_.FullName.Substring($full.Length).TrimStart('\');
                Length=$_.Length; SHA256=Get-Sha256 $_.FullName }
        })
    $serialized = ($records | ConvertTo-Json -Depth 6 -Compress)
    $bytes = $utf8.GetBytes($serialized)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $treeHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','') }
    finally { $sha.Dispose() }
    [ordered]@{ Path=$full; Kind='Directory'; FileCount=$records.Count;
        TreeSHA256=$treeHash }
}

function Get-HostIdentity {
    if ($TestMode -and -not [string]::IsNullOrWhiteSpace($MockHostPath)) {
        return ([IO.File]::ReadAllText((Get-AbsolutePath $MockHostPath 'MockHostPath'),
            $utf8) | ConvertFrom-Json)
    }
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $machineGuid = [string](Get-ItemProperty `
        'HKLM:\SOFTWARE\Microsoft\Cryptography' -Name MachineGuid `
        -ErrorAction Stop).MachineGuid
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $guidHash = ([BitConverter]::ToString(
            $sha.ComputeHash($utf8.GetBytes($machineGuid)))).Replace('-','')
    } finally { $sha.Dispose() }
    [ordered]@{ BuildNumber=[string]$os.BuildNumber; OSArchitecture=[string]$os.OSArchitecture;
        WindowsDirectory=[Environment]::GetFolderPath('Windows');
        MachineGuidSHA256=$guidHash; User=[Security.Principal.WindowsIdentity]::GetCurrent().Name }
}

function Protect-ContinuationFile([string]$Path) {
    if ($TestMode) { return }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $security = New-Object Security.AccessControl.FileSecurity
    $security.SetAccessRuleProtection($true, $false)
    foreach ($sid in @($identity.User,
            (New-Object Security.Principal.SecurityIdentifier('S-1-5-18')),
            (New-Object Security.Principal.SecurityIdentifier('S-1-5-32-544')))) {
        $rule = New-Object Security.AccessControl.FileSystemAccessRule(
            $sid, [Security.AccessControl.FileSystemRights]::FullControl,
            [Security.AccessControl.AccessControlType]::Allow)
        [void]$security.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $Path -AclObject $security
}

function Read-And-ValidateManifest([string]$Path) {
    $full = Get-AbsolutePath $Path 'ManifestPath'
    $hashPath = $full + '.sha256'
    if (-not (Test-Path -LiteralPath $full -PathType Leaf) -or
        -not (Test-Path -LiteralPath $hashPath -PathType Leaf)) {
        throw 'Continuation manifest or hash companion is missing.'
    }
    $expectedHash = ([IO.File]::ReadAllText($hashPath, $utf8)).Trim().ToUpperInvariant()
    if ($expectedHash -notmatch '^[0-9A-F]{64}$' -or (Get-Sha256 $full) -ne $expectedHash) {
        throw 'Continuation manifest hash mismatch.'
    }
    $manifest = [IO.File]::ReadAllText($full, $utf8) | ConvertFrom-Json
    if ([int]$manifest.Schema -ne 2 -or -not [bool]$manifest.FailClosed -or
        [bool]$manifest.ResumeMayPerformHardwareOperations) {
        throw 'Continuation manifest safety contract is invalid.'
    }
    if ([string]$manifest.Script.Path -ne $scriptPath -or
        [string]$manifest.Script.SHA256 -ne (Get-Sha256 $scriptPath)) {
        throw 'Continuation script identity mismatch.'
    }
    $currentHost = Get-HostIdentity
    foreach ($name in @('BuildNumber','OSArchitecture','WindowsDirectory',
            'MachineGuidSHA256','User')) {
        if ([string]$manifest.Host.$name -ne [string]$currentHost.$name) {
            throw "Continuation host identity mismatch: $name"
        }
    }
    $worktree = Get-AbsolutePath ([string]$manifest.Repository.Worktree) 'Recorded worktree'
    if (-not (Test-Path -LiteralPath $worktree -PathType Container)) {
        throw 'Recorded worktree is missing.'
    }
    $head = (& git -C $worktree rev-parse HEAD 2>$null).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne [string]$manifest.Repository.Head) {
        throw 'Recorded repository HEAD does not match.'
    }
    $branch = (& git -C $worktree branch --show-current 2>$null).Trim()
    if ($LASTEXITCODE -ne 0 -or $branch -ne [string]$manifest.Repository.Branch) {
        throw 'Recorded repository branch does not match.'
    }
    if (@(& git -C $worktree status --porcelain).Count -ne 0) {
        throw 'Recorded worktree is not clean.'
    }
    if ([string]$manifest.Codex.ThreadId -notmatch
        '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$') {
        throw 'Recorded Codex thread UUID is invalid.'
    }
    $codexExecutable = Get-AbsolutePath ([string]$manifest.Codex.Executable) 'Recorded Codex executable'
    if ((Get-Sha256 $codexExecutable) -ne [string]$manifest.Codex.SHA256) {
        throw 'Recorded Codex executable hash mismatch.'
    }
    $promptPath = Get-AbsolutePath ([string]$manifest.Codex.PromptPath) 'Recorded prompt'
    if ((Get-Sha256 $promptPath) -ne [string]$manifest.Codex.PromptSHA256) {
        throw 'Recorded continuation prompt hash mismatch.'
    }
    if ([string]::IsNullOrWhiteSpace([string]$manifest.NextPermittedAction)) {
        throw 'Next permitted action is missing.'
    }
    foreach ($record in @($manifest.Evidence)) {
        $current = Get-EvidenceIdentity ([string]$record.Path)
        if ([string]$record.Kind -ne [string]$current.Kind) {
            throw "Continuation evidence kind mismatch: $($record.Path)"
        }
        if ([string]$record.Kind -eq 'File') {
            if ([long]$record.Length -ne [long]$current.Length -or
                [string]$record.SHA256 -ne [string]$current.SHA256) {
                throw "Continuation evidence file mismatch: $($record.Path)"
            }
        }
        elseif ([int]$record.FileCount -ne [int]$current.FileCount -or
            [string]$record.TreeSHA256 -ne [string]$current.TreeSHA256) {
            throw "Continuation evidence directory mismatch: $($record.Path)"
        }
    }
    return $manifest
}

function Get-MockTaskPath([string]$Name) {
    if (-not $TestMode -or [string]::IsNullOrWhiteSpace($MockTaskRoot)) {
        throw 'MockTaskRoot is required for task operations in TestMode.'
    }
    $root = Get-AbsolutePath $MockTaskRoot 'MockTaskRoot'
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw 'MockTaskRoot does not exist.'
    }
    Join-Path $root ($Name + '.json')
}

function Register-OneTimeTask([object]$Manifest) {
    $name = [string]$Manifest.Task.Name
    if ($TestMode) {
        $path = Get-MockTaskPath $name
        Write-ExclusiveJson $path ([ordered]@{ Name=$name; User=$Manifest.Host.User;
            Mode='Resume'; ManifestPath=$ManifestPath; Registered=$true })
        return
    }
    if ($null -ne (Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue)) {
        throw "Scheduled Task already exists: $name"
    }
    $ps51 = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
    if (-not (Test-Path -LiteralPath $ps51 -PathType Leaf)) {
        throw '64-bit Windows PowerShell 5.1 is missing.'
    }
    $argument = "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`" -Mode Resume -ManifestPath `"$ManifestPath`""
    $action = New-ScheduledTaskAction -Execute $ps51 -Argument $argument `
        -WorkingDirectory (Split-Path -Parent $scriptPath)
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User ([string]$Manifest.Host.User)
    $principal = New-ScheduledTaskPrincipal -UserId ([string]$Manifest.Host.User) `
        -LogonType Interactive -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries `
        -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 5) `
        -MultipleInstances IgnoreNew
    $task = New-ScheduledTask -Action $action -Trigger $trigger `
        -Principal $principal -Settings $settings
    Register-ScheduledTask -TaskName $name -InputObject $task -Force:$false | Out-Null
    $actual = Get-ScheduledTask -TaskName $name -ErrorAction Stop
    if ($actual.Principal.UserId -ne [string]$Manifest.Host.User -or
        $actual.Principal.LogonType -ne 'InteractiveToken' -or
        @($actual.Actions).Count -ne 1 -or $actual.Actions[0].Execute -ne $ps51 -or
        $actual.Actions[0].Arguments -ne $argument) {
        Unregister-ScheduledTask -TaskName $name -Confirm:$false -ErrorAction SilentlyContinue
        throw 'Registered task failed exact identity validation.'
    }
}

function Remove-OneTimeTask([string]$Name) {
    if ($TestMode) {
        $path = Get-MockTaskPath $Name
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            [IO.File]::Delete($path)
        }
        return
    }
    if ($null -ne (Get-ScheduledTask -TaskName $Name -ErrorAction SilentlyContinue)) {
        Unregister-ScheduledTask -TaskName $Name -Confirm:$false
    }
}

function Disable-OneTimeTask([string]$Name) {
    if ($TestMode) {
        $path = Get-MockTaskPath $Name
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $task = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
            $task | Add-Member -NotePropertyName Disabled -NotePropertyValue $true -Force
            [IO.File]::WriteAllText($path, (($task | ConvertTo-Json -Depth 6) + "`r`n"), $utf8)
        }
        return
    }
    Disable-ScheduledTask -TaskName $Name -ErrorAction Stop | Out-Null
}

function Get-CodexVersion([string]$Executable) {
    if ($TestMode) { return $MockCodexVersion }
    $value = @(& $Executable --version 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1) { throw 'Unable to read Codex CLI version.' }
    [string]$value[0]
}

function Assert-CodexSignature([string]$Executable) {
    if ($TestMode) { return }
    $signature = Get-AuthenticodeSignature -LiteralPath $Executable
    if ($signature.Status -ne 'Valid' -or
        [string]$signature.SignerCertificate.Subject -notmatch 'OpenAI') {
        throw 'Codex CLI Authenticode identity is not valid OpenAI software.'
    }
}

$ManifestPath = Get-AbsolutePath $ManifestPath 'ManifestPath'
if ($Mode -eq 'Prepare') {
    if (-not $AcknowledgePreparation) { throw 'Prepare requires AcknowledgePreparation.' }
    foreach ($required in @($WorktreePath,$ExpectedBranch,$ExpectedHead,$Phase,
            $NextPermittedAction,$CodexThreadId,$ContinuationPromptPath,
            $CodexExecutablePath,$ExpectedCodexSha256,$ExpectedCodexVersion,$TaskName)) {
        if ([string]::IsNullOrWhiteSpace($required)) { throw 'Prepare parameters are incomplete.' }
    }
    if ((Test-Path -LiteralPath $ManifestPath) -or
        (Test-Path -LiteralPath ($ManifestPath + '.sha256'))) {
        throw 'Continuation manifest output already exists.'
    }
    $worktree = Get-AbsolutePath $WorktreePath 'WorktreePath'
    if (-not (Test-Path -LiteralPath $worktree -PathType Container)) { throw 'Worktree is missing.' }
    $head = (& git -C $worktree rev-parse HEAD).Trim()
    $branch = (& git -C $worktree branch --show-current).Trim()
    if ($head -ne $ExpectedHead.ToLowerInvariant() -or $branch -ne $ExpectedBranch) {
        throw 'Current repository identity differs from the requested continuation identity.'
    }
    if (-not $TestMode -and @(& git -C $worktree status --porcelain).Count -ne 0) {
        throw 'Prepare requires a clean worktree.'
    }
    $codexExecutable = Get-AbsolutePath $CodexExecutablePath 'CodexExecutablePath'
    if (-not (Test-Path -LiteralPath $codexExecutable -PathType Leaf)) {
        throw 'Codex CLI executable is missing.'
    }
    if ((Get-Sha256 $codexExecutable) -ne $ExpectedCodexSha256.ToUpperInvariant()) {
        throw 'Codex CLI hash differs from the approved identity.'
    }
    Assert-CodexSignature $codexExecutable
    $codexVersion = Get-CodexVersion $codexExecutable
    if ($codexVersion -ne $ExpectedCodexVersion) { throw 'Codex CLI version mismatch.' }
    $prompt = Get-AbsolutePath $ContinuationPromptPath 'ContinuationPromptPath'
    if (-not (Test-Path -LiteralPath $prompt -PathType Leaf) -or
        (Get-Item -LiteralPath $prompt).Length -gt 65536) {
        throw 'Continuation prompt is missing or exceeds 64 KiB.'
    }
    $evidence = @($EvidencePaths | ForEach-Object { Get-EvidenceIdentity $_ })
    $manifest = [ordered]@{
        Schema=2; FailClosed=$true; ResumeMayPerformHardwareOperations=$false;
        CreatedUtc=[datetime]::UtcNow.ToString('o'); Host=Get-HostIdentity;
        Repository=[ordered]@{ Worktree=$worktree; Branch=$branch; Head=$head };
        Task=[ordered]@{ Name=$TaskName; OneTime=$true; Registered=$false };
        Codex=[ordered]@{ ThreadId=$CodexThreadId.ToLowerInvariant(); Executable=$codexExecutable;
            SHA256=(Get-Sha256 $codexExecutable); Version=$codexVersion;
            PromptPath=$prompt; PromptSHA256=(Get-Sha256 $prompt);
            Sandbox='workspace-write'; TimeoutSeconds=$ResumeTimeoutSeconds;
            Invocation='exec --sandbox workspace-write --cd <worktree> --json --output-last-message <path> resume --strict-config <exact-uuid> -' };
        Script=[ordered]@{ Path=$scriptPath; SHA256=Get-Sha256 $scriptPath };
        Phase=$Phase; CompletedSteps=@($CompletedSteps);
        ConsumedAuthorizations=@($ConsumedAuthorizations);
        Evidence=@($evidence); NextPermittedAction=$NextPermittedAction
    }
    Write-ExclusiveJson $ManifestPath $manifest
    Write-ExclusiveUtf8 ($ManifestPath + '.sha256') ((Get-Sha256 $ManifestPath) + "`r`n")
    Protect-ContinuationFile $ManifestPath
    Protect-ContinuationFile ($ManifestPath + '.sha256')
    Write-Host "CONTINUATION PREPARE PASS"
    exit 0
}

$manifest = Read-And-ValidateManifest $ManifestPath
if ($Mode -eq 'Validate') {
    Write-Host "CONTINUATION VALIDATE PASS"
    exit 0
}
if ($Mode -eq 'Register') {
    if (-not $AcknowledgeRegistration) { throw 'Register requires AcknowledgeRegistration.' }
    Register-OneTimeTask $manifest
    Write-Host "CONTINUATION REGISTER PASS"
    exit 0
}
if ($Mode -eq 'Cleanup') {
    Remove-OneTimeTask ([string]$manifest.Task.Name)
    Write-Host "CONTINUATION CLEANUP PASS"
    exit 0
}

$mutexName = 'Local\LecS65CodexContinuation-' + (Get-Sha256 $ManifestPath).Substring(0,24)
$mutex = New-Object Threading.Mutex($false, $mutexName)
$acquired = $false
try {
    $acquired = $mutex.WaitOne(0)
    if (-not $acquired) { throw 'Another continuation instance is already active.' }
    $resultPath = $ManifestPath + '.resume-result.json'
    $attemptPath = $ManifestPath + '.attempt-started.json'
    $stdoutPath = $ManifestPath + '.stdout.jsonl'
    $stderrPath = $ManifestPath + '.stderr.log'
    $lastMessagePath = $ManifestPath + '.last-message.txt'
    $exceptionPath = $ManifestPath + '.exception.log'
    foreach ($path in @($resultPath,$attemptPath,$stdoutPath,$stderrPath,
            $lastMessagePath,$exceptionPath)) {
        if (Test-Path -LiteralPath $path) { throw "Continuation evidence already exists: $path" }
    }
    Write-ExclusiveJson $attemptPath ([ordered]@{ StartedUtc=[datetime]::UtcNow.ToString('o');
        ThreadId=$manifest.Codex.ThreadId; Head=$manifest.Repository.Head })
    Disable-OneTimeTask ([string]$manifest.Task.Name)
    $codexExecutable = Get-AbsolutePath ([string]$manifest.Codex.Executable) `
        'Recorded Codex executable'
    Assert-CodexSignature $codexExecutable
    if ((Get-CodexVersion $codexExecutable) -ne [string]$manifest.Codex.Version) {
        throw 'Codex CLI version changed after preparation.'
    }
    $arguments = @('exec','--sandbox','workspace-write','--cd',
        [string]$manifest.Repository.Worktree,'--json','--output-last-message',
        $lastMessagePath,'resume','--strict-config',[string]$manifest.Codex.ThreadId,'-')
    $exitCode = 0
    if ($TestMode) {
        Write-ExclusiveUtf8 $stdoutPath (([ordered]@{ type='mock'; arguments=$arguments } |
            ConvertTo-Json -Compress) + "`r`n")
        Write-ExclusiveUtf8 $stderrPath ''
        if ($MockChildTimeout) { throw 'Mock Codex child timed out.' }
        $exitCode = $MockChildExitCode
        if ($exitCode -eq 0) { Write-ExclusiveUtf8 $lastMessagePath "Mock continuation complete.`r`n" }
    }
    else {
        $quote = { param([string]$v) if ($v -notmatch '[\s"]') { $v } else {
            '"' + ($v -replace '(\\*)"','$1$1\"' -replace '(\\+)$','$1$1') + '"' } }
        $psi = New-Object Diagnostics.ProcessStartInfo
        $psi.FileName = $codexExecutable
        $psi.Arguments = (($arguments | ForEach-Object { & $quote $_ }) -join ' ')
        $psi.WorkingDirectory = [string]$manifest.Repository.Worktree
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardInput = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $process = New-Object Diagnostics.Process
        $process.StartInfo = $psi
        if (-not $process.Start()) { throw 'Codex CLI process did not start.' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $promptText = [IO.File]::ReadAllText([string]$manifest.Codex.PromptPath, $utf8)
        $process.StandardInput.Write($promptText)
        $process.StandardInput.Close()
        if (-not $process.WaitForExit([int]$manifest.Codex.TimeoutSeconds * 1000)) {
            $taskkill = Join-Path ([Environment]::GetFolderPath('Windows')) 'System32\taskkill.exe'
            if (Test-Path -LiteralPath $taskkill -PathType Leaf) {
                & $taskkill /PID ([string]$process.Id) /T /F *> $null
            }
            else { try { $process.Kill() } catch {} }
            try { $process.WaitForExit(15000) | Out-Null } catch {}
            try { [Threading.Tasks.Task]::WaitAll(@($stdoutTask,$stderrTask), 15000) | Out-Null } catch {}
            Write-ExclusiveUtf8 $stdoutPath $(if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { '' })
            Write-ExclusiveUtf8 $stderrPath $(if ($stderrTask.IsCompleted) { $stderrTask.Result } else { '' })
            $process.Dispose()
            throw 'Codex CLI continuation timed out.'
        }
        [Threading.Tasks.Task]::WaitAll(@($stdoutTask,$stderrTask))
        Write-ExclusiveUtf8 $stdoutPath $stdoutTask.Result
        Write-ExclusiveUtf8 $stderrPath $stderrTask.Result
        $exitCode = $process.ExitCode
        $process.Dispose()
    }
    if ($exitCode -ne 0) { throw "Codex CLI continuation failed with exit code $exitCode." }
    if (-not (Test-Path -LiteralPath $lastMessagePath -PathType Leaf)) {
        throw 'Codex CLI produced no final-message evidence.'
    }
    Write-ExclusiveJson $resultPath ([ordered]@{
        ResumedUtc=[datetime]::UtcNow.ToString('o'); HostValidated=$true;
        RepositoryValidated=$true; HardwareOperationExecuted=$false;
        ThreadId=$manifest.Codex.ThreadId; ExitCode=$exitCode;
        StdoutSHA256=(Get-Sha256 $stdoutPath); StderrSHA256=(Get-Sha256 $stderrPath);
        LastMessageSHA256=(Get-Sha256 $lastMessagePath);
        NextPermittedAction=$manifest.NextPermittedAction;
        Status=if ($TestMode) { 'MockResumeCompleted' } else { 'ResumeCompleted' }
    })
    Protect-ContinuationFile $resultPath
    if (-not $KeepTaskAfterResume) { Remove-OneTimeTask ([string]$manifest.Task.Name) }
    Write-Host "CONTINUATION RESUME PASS"
}
catch {
    if ($Mode -eq 'Resume' -and -not (Test-Path -LiteralPath ($ManifestPath + '.exception.log'))) {
        try { Write-ExclusiveUtf8 ($ManifestPath + '.exception.log') ($_.Exception.ToString() + "`r`n") } catch {}
    }
    throw
}
finally {
    if ($acquired) { [void]$mutex.ReleaseMutex() }
    $mutex.Dispose()
}
