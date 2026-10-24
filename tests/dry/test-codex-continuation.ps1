<# Hardware-independent tests for the one-time Codex continuation workflow. #>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$scriptPath = Join-Path $repo 'scripts\manage-codex-continuation.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('lec-continuation-' + [guid]::NewGuid().ToString('N'))
$passed = 0
$failed = 0

function Test-Case([string]$Name, [scriptblock]$Body) {
    try { & $Body; Write-Host "[PASS] $Name"; $script:passed++ }
    catch { Write-Host "[FAIL] $Name`: $($_.Exception.Message)"; $script:failed++ }
}

function Invoke-Workflow([string[]]$Arguments, [int]$ExpectedExit) {
    $savedPreference = $ErrorActionPreference
    try {
        # Expected negative cases write diagnostics to stderr. Windows
        # PowerShell surfaces redirected native stderr as ErrorRecord objects;
        # collect them without letting the harness terminate prematurely.
        $ErrorActionPreference = 'Continue'
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $scriptPath @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($exitCode -ne $ExpectedExit) {
        throw "exit=$exitCode expected=$ExpectedExit output=$($output -join ' ')"
    }
    $output
}

function New-Fixture([string]$Name) {
    $case = Join-Path $root $Name
    $worktree = Join-Path $case 'repo'
    $tasks = Join-Path $case 'tasks'
    New-Item -ItemType Directory -Path $worktree,$tasks | Out-Null
    & git -C $worktree init -b batch10 *> $null
    & git -C $worktree config user.email 'offline@example.invalid'
    & git -C $worktree config user.name 'Offline Test'
    [IO.File]::WriteAllText((Join-Path $worktree 'state.txt'), 'initial')
    & git -C $worktree add state.txt
    & git -C $worktree commit -m initial *> $null
    $head = (& git -C $worktree rev-parse HEAD).Trim()
    $hostPath = Join-Path $case 'host.json'
    [IO.File]::WriteAllText($hostPath, (@{
        BuildNumber='19045';OSArchitecture='64-bit';WindowsDirectory='C:\Windows';
        MachineGuidSHA256=('A'*64);User='TEST\User'
    } | ConvertTo-Json))
    $evidence = Join-Path $case 'evidence.txt'
    [IO.File]::WriteAllText($evidence, 'evidence')
    $codex = Join-Path $case 'Codex.exe'
    [IO.File]::WriteAllText($codex, 'mock')
    $prompt = Join-Path $case 'prompt.txt'
    [IO.File]::WriteAllText($prompt, 'Continue only the recorded offline work. Do not perform hardware operations.')
    [pscustomobject]@{ Root=$case;Worktree=$worktree;Tasks=$tasks;Head=$head;
        Host=$hostPath;Evidence=$evidence;Codex=$codex;Prompt=$prompt;
        Manifest=(Join-Path $case 'continue.json') }
}

function Prepare([object]$Fixture) {
    Invoke-Workflow @('-Mode','Prepare','-ManifestPath',$Fixture.Manifest,
        '-WorktreePath',$Fixture.Worktree,'-ExpectedBranch','batch10',
        '-ExpectedHead',$Fixture.Head,'-Phase','OfflineAnalysis',
        '-CompletedSteps','analysis','-ConsumedAuthorizations','none',
        '-EvidencePaths',$Fixture.Evidence,
        '-NextPermittedAction','Continue offline analysis only',
        '-CodexThreadId','01234567-89ab-cdef-0123-456789abcdef',
        '-ContinuationPromptPath',$Fixture.Prompt,
        '-CodexExecutablePath',$Fixture.Codex,
        '-ExpectedCodexSha256',(Get-FileHash $Fixture.Codex -Algorithm SHA256).Hash,
        '-ExpectedCodexVersion','codex-cli mock',
        '-TaskName','LecS65-Codex-Continuation-Test','-AcknowledgePreparation',
        '-TestMode','-MockTaskRoot',$Fixture.Tasks,'-MockHostPath',$Fixture.Host) 0 | Out-Null
}

New-Item -ItemType Directory -Path $root | Out-Null
try {
    Test-Case 'prepare records repository authorization and evidence state' {
        $f=New-Fixture 'prepare'; Prepare $f
        $m=Get-Content $f.Manifest -Raw|ConvertFrom-Json
        if ($m.Repository.Head -ne $f.Head -or $m.ResumeMayPerformHardwareOperations -or
            @($m.CompletedSteps).Count -ne 1 -or @($m.Evidence).Count -ne 1) { throw 'manifest mismatch' }
    }
    Test-Case 'validate accepts exact clean state' {
        $f=New-Fixture 'validate'; Prepare $f
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0 | Out-Null
    }
    Test-Case 'mock registration is exact and non-overwriting' {
        $f=New-Fixture 'register'; Prepare $f
        $args=@('-Mode','Register','-ManifestPath',$f.Manifest,'-AcknowledgeRegistration',
            '-TestMode','-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host)
        Invoke-Workflow $args 0|Out-Null; Invoke-Workflow $args 1|Out-Null
    }
    Test-Case 'resume records no hardware action and removes task' {
        $f=New-Fixture 'resume'; Prepare $f
        Invoke-Workflow @('-Mode','Register','-ManifestPath',$f.Manifest,'-AcknowledgeRegistration',
            '-TestMode','-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        $r=Get-Content ($f.Manifest+'.resume-result.json') -Raw|ConvertFrom-Json
        if ($r.HardwareOperationExecuted -or @(Get-ChildItem $f.Tasks).Count -ne 0) { throw 'unsafe resume' }
    }
    Test-Case 'resume is one-shot after partial completion' {
        $f=New-Fixture 'oneshot'; Prepare $f
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'changed repository HEAD fails closed' {
        $f=New-Fixture 'head'; Prepare $f
        [IO.File]::AppendAllText((Join-Path $f.Worktree 'state.txt'),'changed')
        & git -C $f.Worktree add state.txt; & git -C $f.Worktree commit -m changed *> $null
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'dirty worktree fails closed' {
        $f=New-Fixture 'dirty'; Prepare $f
        [IO.File]::AppendAllText((Join-Path $f.Worktree 'state.txt'),'dirty')
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'missing worktree fails closed' {
        $f=New-Fixture 'missing'; Prepare $f
        [IO.Directory]::Move($f.Worktree,$f.Worktree+'-gone')
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'corrupted manifest fails closed' {
        $f=New-Fixture 'corrupt'; Prepare $f
        [IO.File]::AppendAllText($f.Manifest,'x')
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'changed evidence fails closed' {
        $f=New-Fixture 'evidence-change'; Prepare $f
        [IO.File]::AppendAllText($f.Evidence,'changed')
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'host mismatch fails closed' {
        $f=New-Fixture 'host'; Prepare $f
        $h=Get-Content $f.Host -Raw|ConvertFrom-Json; $h.BuildNumber='99999'
        [IO.File]::WriteAllText($f.Host,($h|ConvertTo-Json))
        Invoke-Workflow @('-Mode','Validate','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'existing Codex desktop processes do not select or block the exact UUID' {
        $f=New-Fixture 'desktop-processes'; Prepare $f
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        $line=Get-Content ($f.Manifest+'.stdout.jsonl') -Raw|ConvertFrom-Json
        if (@($line.arguments) -contains '--last' -or
            @($line.arguments) -notcontains '01234567-89ab-cdef-0123-456789abcdef') {
            throw 'unsafe session selection'
        }
    }
    Test-Case 'nonzero child is one-shot and preserves disabled task plus exception evidence' {
        $f=New-Fixture 'child-failure'; Prepare $f
        Invoke-Workflow @('-Mode','Register','-ManifestPath',$f.Manifest,'-AcknowledgeRegistration',
            '-TestMode','-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host,'-MockChildExitCode','17') 1|Out-Null
        $task=Get-Content (Join-Path $f.Tasks 'LecS65-Codex-Continuation-Test.json') -Raw|ConvertFrom-Json
        if (-not $task.Disabled -or -not (Test-Path ($f.Manifest+'.exception.log')) -or
            -not (Test-Path ($f.Manifest+'.attempt-started.json'))) { throw 'failure evidence incomplete' }
    }
    Test-Case 'timeout is one-shot and preserves failure evidence' {
        $f=New-Fixture 'timeout'; Prepare $f
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host,'-MockChildTimeout') 1|Out-Null
        if (-not (Test-Path ($f.Manifest+'.exception.log'))) { throw 'timeout evidence missing' }
    }
    Test-Case 'changed prompt fails closed before attempt' {
        $f=New-Fixture 'prompt-change'; Prepare $f
        [IO.File]::AppendAllText($f.Prompt,' changed')
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
        if (Test-Path ($f.Manifest+'.attempt-started.json')) { throw 'attempt claimed after validation failure' }
    }
    Test-Case 'missing Codex executable fails closed' {
        $f=New-Fixture 'missing-codex'; Prepare $f
        [IO.File]::Delete($f.Codex)
        Invoke-Workflow @('-Mode','Resume','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 1|Out-Null
    }
    Test-Case 'cleanup removes registered task idempotently' {
        $f=New-Fixture 'cleanup'; Prepare $f
        Invoke-Workflow @('-Mode','Register','-ManifestPath',$f.Manifest,'-AcknowledgeRegistration',
            '-TestMode','-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host) 0|Out-Null
        $args=@('-Mode','Cleanup','-ManifestPath',$f.Manifest,'-TestMode',
            '-MockTaskRoot',$f.Tasks,'-MockHostPath',$f.Host)
        Invoke-Workflow $args 0|Out-Null; Invoke-Workflow $args 0|Out-Null
        if (@(Get-ChildItem $f.Tasks).Count -ne 0) { throw 'task remains' }
    }
}
finally {
    if (Test-Path $root) { Remove-Item $root -Recurse -Force }
}

Write-Host "CODEX CONTINUATION: $passed/$($passed+$failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
