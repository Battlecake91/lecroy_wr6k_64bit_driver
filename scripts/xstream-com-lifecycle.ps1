function Test-XStreamRpcServerUnavailable {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][Exception]$Exception)

    $current = $Exception
    while ($null -ne $current) {
        if (([int64]$current.HResult -band 0xFFFFFFFFL) -eq 0x800706BAL) {
            return $true
        }
        $current = $current.InnerException
    }
    return $false
}

function Get-XStreamObject {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][object]$Parent,
          [Parameter(Mandatory=$true)][string]$Name)

    $errors = New-Object 'System.Collections.Generic.List[string]'
    try { return $Parent.Item($Name) }
    catch {
        if (Test-XStreamRpcServerUnavailable -Exception $_.Exception) { throw }
        [void]$errors.Add("Item: $($_.Exception.Message)")
    }
    try { return $Parent.Objects.Item($Name) }
    catch {
        if (Test-XStreamRpcServerUnavailable -Exception $_.Exception) { throw }
        [void]$errors.Add("Objects.Item: $($_.Exception.Message)")
    }
    try { return $Parent.Object.Item($Name) }
    catch {
        if (Test-XStreamRpcServerUnavailable -Exception $_.Exception) { throw }
        [void]$errors.Add("Object.Item: $($_.Exception.Message)")
    }
    throw "Could not resolve XStream object '$Name': $($errors -join ' | ')"
}
