[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$allowlistPath = Join-Path $repo 'docs\dma-pre-go-side-effect-allowlist.json'
$ioctlPath = Join-Path $repo 'driver\Ioctl.c'
$pnpPath = Join-Path $repo 'driver\DmaPnpStage.c'
$syncPath = Join-Path $repo 'driver\DmaSyncStage.c'
$transactionPath = Join-Path $repo 'driver\DmaTransaction.c'
$headerPath = Join-Path $repo 'driver\LecS65Drv.h'
$acquisitionPath = Join-Path $repo 'driver\Acquisition.c'
$script:passed = 0
$script:failed = 0

function Test-Case([string]$Name, [scriptblock]$Body) {
    try {
        if (-not (& $Body)) { throw 'predicate returned false' }
        Write-Host "[PASS] $Name"
        $script:passed++
    } catch {
        Write-Host "[FAIL] $Name`: $($_.Exception.Message)" -ForegroundColor Red
        $script:failed++
    }
}

function Get-FunctionText([string]$Text, [string]$Name) {
    $match = [regex]::Match($Text,
        ('(?ms)^\s*(?:static\s+)?(?:NTSTATUS|VOID|BOOLEAN|LECS65_DMA_TRANSACTION_RESULT)\s+{0}\s*\(.*?^\}}' -f [regex]::Escape($Name)))
    if (-not $match.Success) { throw "Function not found: $Name" }
    $match.Value
}

$allowlist = Get-Content -LiteralPath $allowlistPath -Raw | ConvertFrom-Json
$ioctl = Get-Content -LiteralPath $ioctlPath -Raw
$pnp = Get-Content -LiteralPath $pnpPath -Raw
$sync = Get-Content -LiteralPath $syncPath -Raw
$transaction = Get-Content -LiteralPath $transactionPath -Raw
$header = Get-Content -LiteralPath $headerPath -Raw
$acquisition = Get-Content -LiteralPath $acquisitionPath -Raw
$acquire = Get-FunctionText $ioctl 'LecIoctlAcquireBufferedOneChannel'
$prepare = Get-FunctionText $pnp 'LecDmaPnpPrepareLegacyNoLaunch'
$map = Get-FunctionText $sync 'LecSgSyncMapNoLaunch'
$release = Get-FunctionText $sync 'LecSgSyncReleaseNoLaunch'
$commit = Get-FunctionText $transaction 'LecDmaTransactionCommitHardwareLaunch'
$register = Get-FunctionText $acquisition 'LecRegisterTransfer'
$unregister = Get-FunctionText $acquisition 'LecUnregisterTransfer'
$safeAcquire = [regex]::Match($acquire,
    '(?ms)#if\s+!LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA(?<safe>.*?)#else').Groups['safe'].Value

Test-Case 'allowlist is the exact reviewed Batch 12 production scope' {
    $allowlist.schema -eq 1 -and
    $allowlist.reviewedDriverCommit -eq 'fba434344d82817f9f517f4ddcc97a0b60310571' -and
    $allowlist.compileGate.symbol -eq 'LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA' -and
    [int]$allowlist.compileGate.requiredValue -eq 0 -and
    -not [bool]$allowlist.compileGate.unsafeBranchLiveAllowed
}
Test-Case 'every allowlisted operation has resolved side effects and no hardware access' {
    @($allowlist.operations).Count -ge 10 -and
    @($allowlist.operations | Where-Object {
        $_.hardwareRead -ne $false -or $_.hardwareWrite -ne $false -or $_.uncertain -ne $false
    }).Count -eq 0
}
Test-Case 'unsafe PFN DMA compile gate is disabled by default' {
    $header -match '(?m)^#define\s+LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA\s+0\s*$'
}
Test-Case 'production CFDC2138 branch has no register or port access primitive' {
    -not [string]::IsNullOrWhiteSpace($safeAcquire) -and
    $safeAcquire -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_' -and
    $safeAcquire -notmatch 'Lec(?:Select|Arm|Launch)Dma|LecUpdateInterruptMask' -and
    $safeAcquire -notmatch 'TotalDwords|DescriptorTablePhysical|DescriptorBuffer' -and
    $safeAcquire -match 'LecDmaPnpPrepareLegacyNoLaunch'
}
Test-Case 'CFDC2124 production registration locks memory without building PFN descriptors' {
    $register -match 'LecBuildSourceMdlChain' -and
    $register -match 'LecBuildDescriptorTable' -and
    $register -match '(?s)#if\s+LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA.*?LecBuildDescriptorTable' -and
    $register -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_' -and
    $register -notmatch 'LecLaunchSelectedDma|LecUpdateInterruptMask'
}
Test-Case 'no-launch preparation contains no hardware access or launch helper' {
    $prepare -match 'LecSgSyncMapNoLaunch' -and
    $prepare -match 'LecDmaTransactionBuildDescriptors' -and
    $prepare -match 'LecDmaTransactionCommitHardwareLaunch' -and
    $prepare -match 'LecSgSyncReleaseNoLaunch' -and
    $prepare -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_' -and
    $prepare -notmatch 'LecLaunchSelectedDma|LecUpdateInterruptMask'
}
Test-Case 'mapping uses only synchronous WDM v3 SG allocation' {
    $map -match 'GetScatterGatherListEx' -and
    $map -match 'DMA_SYNCHRONOUS_CALLBACK' -and
    $map -match 'InitializeDmaTransferContext' -and
    $map -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_'
}
Test-Case 'hardware launch commit is an unconditional software rejection' {
    $commit -match 'tx->Phase\s*=\s*LecDmaTxPublished' -and
    $commit -match 'LaunchRejected\+\+' -and
    $commit -match 'return\s+LecDmaTxLaunchDisabled' -and
    $commit -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_|EverPossiblyLaunched\s*=\s*1'
}
Test-Case 'cleanup can only release never-launched mapping and locked memory' {
    $release -match 'LecSgSyncFreeStage' -and
    $release -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_' -and
    $unregister -match 'LecFreeTransfer' -and
    $unregister -notmatch '(?:READ|WRITE)_(?:REGISTER|PORT)_'
}
Test-Case 'unsafe branch remains visibly hardware-active and excluded from live allowlist' {
    $acquire -match '(?s)#else.*?WRITE_REGISTER_ULONG.*?LecLaunchSelectedDma.*?#endif' -and
    @($allowlist.forbiddenReachableEffects) -contains 'MAMRGO'
}

Write-Host ("PRE-GO SIDE EFFECTS: {0}/{1} passed; {2} failed." -f
    $script:passed, ($script:passed + $script:failed), $script:failed)
if ($script:failed -ne 0) { exit 1 }

