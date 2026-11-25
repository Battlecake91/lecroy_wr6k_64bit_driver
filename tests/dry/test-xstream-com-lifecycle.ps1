$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'scripts\xstream-com-lifecycle.ps1')
$passed = 0
$failed = 0

function Check([string]$Name,[bool]$Condition) {
    if ($Condition) { $script:passed++; Write-Host "PASS: $Name" }
    else { $script:failed++; Write-Host "FAIL: $Name" }
}

function Invoke-ExpectedFailure([scriptblock]$Action) {
    try { & $Action; return $false }
    catch { return $true }
}

$child = [pscustomobject]@{ Name='Acquisition'; Ready=$true }
$root = [pscustomobject]@{ Child=$child }
$root | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    if ($Name -eq 'Acquisition') { return $this.Child }
    throw 'DISP_E_MEMBERNOTFOUND'
}
Check 'root automation object resolves Acquisition through Item' `
    ([object]::ReferenceEquals((Get-XStreamObject -Parent $root -Name 'Acquisition'),$child))

$delayed = [pscustomobject]@{ Calls=0; Child=$child }
$delayed | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    $this.Calls++
    if ($this.Calls -lt 3) { return $null }
    return $this.Child
}
$first = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
$second = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
$third = Get-XStreamObject -Parent $delayed -Name 'Acquisition'
Check 'delayed Acquisition availability is observable without extra COM access paths' `
    ($null -eq $first -and $null -eq $second -and
     [object]::ReferenceEquals($third,$child) -and $delayed.Calls -eq 3)

$fallback = [pscustomobject]@{ Objects=[pscustomobject]@{ Child=$child } }
$fallback.Objects | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name) return $this.Child
}
$fallback | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name) throw 'ordinary dispatch member mismatch'
}
Check 'ordinary Item failure retains legacy Objects.Item fallback' `
    ([object]::ReferenceEquals((Get-XStreamObject -Parent $fallback -Name 'Acquisition'),$child))

$rpcError = New-Object System.Runtime.InteropServices.COMException(
    'The RPC server is unavailable.',[int32]-2147023174)
$rpcRoot = [pscustomobject]@{ Calls=0; Objects=$null; Object=$null }
$rpcRoot | Add-Member -MemberType ScriptMethod -Name Item -Value {
    param($Name)
    $this.Calls++
    throw $script:rpcError
}
$script:rpcError = $rpcError
Check 'RPC server unavailable is classified from HRESULT 0x800706BA' `
    (Test-XStreamRpcServerUnavailable -Exception $rpcError)
Check 'RPC failure is rethrown without null fallback calls' `
    ((Invoke-ExpectedFailure {
        Get-XStreamObject -Parent $rpcRoot -Name 'Acquisition'
    }) -and $rpcRoot.Calls -eq 1)
Check 'inner RPC exception is classified' `
    (Test-XStreamRpcServerUnavailable -Exception (
        New-Object InvalidOperationException('wrapped',$rpcError)))

$ordinaryError = New-Object System.Exception('not an RPC failure')
Check 'ordinary failures are not misclassified as RPC loss' `
    (-not (Test-XStreamRpcServerUnavailable -Exception $ordinaryError))
Check 'PowerShell COM probe runs in STA for this test host' `
    ([Threading.Thread]::CurrentThread.GetApartmentState() -eq [Threading.ApartmentState]::STA)

Write-Host "XSTREAM COM LIFECYCLE: $passed/$($passed+$failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
