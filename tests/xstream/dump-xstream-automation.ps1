<#
.SYNOPSIS
  Read-only inspection of the local LeCroy XStream COM automation object model.
.DESCRIPTION
  Connects to LeCroy.XStreamDSO and records COM-visible member names and the
  shape of Object/Objects/Item collections. It does not set CVARs, invoke
  actions, acquire waveforms, or access the replacement driver's debug IOCTLs.

  The purpose is to characterize old XStream/PowerShell COM interop on the
  WaveRunner 6000 instead of assuming that VBScript aliases are exposed through
  PowerShell's COM adapter.
#>
[CmdletBinding()]
param(
    [string]$OutputPath = ".\xstream-automation-dump.txt",

    [ValidateRange(1, 20)]
    [int]$MaxCollectionItems = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:Lines = New-Object System.Collections.Generic.List[string]

function Add-Line {
    param([string]$Text = "")
    $script:Lines.Add($Text)
    Write-Host $Text
}

function Get-ComTypeText {
    param($Object)

    if ($null -eq $Object) {
        return "<null>"
    }

    $parts = New-Object System.Collections.Generic.List[string]
    try {
        $parts.Add(("PS type: {0}" -f $Object.GetType().FullName))
    }
    catch {
        $parts.Add(("PS type error: {0}" -f $_.Exception.Message))
    }

    try {
        $parts.Add(("COM object: {0}" -f [System.Runtime.InteropServices.Marshal]::IsComObject($Object)))
    }
    catch {
    }

    return ($parts -join "; ")
}

function Show-Members {
    param(
        [Parameter(Mandatory=$true)]$Object,
        [Parameter(Mandatory=$true)][string]$Label
    )

    Add-Line ""
    Add-Line ("=== {0} ===" -f $Label)
    Add-Line (Get-ComTypeText -Object $Object)

    try {
        $members = @($Object | Get-Member -Force | Sort-Object MemberType, Name)
        Add-Line ("Get-Member count: {0}" -f $members.Count)
        foreach ($member in $members) {
            Add-Line ("  {0,-14} {1}" -f $member.MemberType, $member.Name)
        }
    }
    catch {
        Add-Line ("Get-Member failed: {0}" -f $_.Exception.Message)
    }
}

function Try-ReadMember {
    param(
        [Parameter(Mandatory=$true)]$Object,
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Label
    )

    try {
        $value = $Object.$Name
        Add-Line ("[OK] {0}.{1}: {2}" -f $Label, $Name, (Get-ComTypeText -Object $value))
        return $value
    }
    catch {
        Add-Line ("[NO] {0}.{1}: {2}" -f $Label, $Name, $_.Exception.Message)
        return $null
    }
}

function Try-Item {
    param(
        [Parameter(Mandatory=$true)]$Object,
        [Parameter(Mandatory=$true)]$Key,
        [Parameter(Mandatory=$true)][string]$Label
    )

    try {
        $value = $Object.Item($Key)
        Add-Line ("[OK] {0}.Item({1}): {2}" -f $Label, $Key, (Get-ComTypeText -Object $value))
        return $value
    }
    catch {
        Add-Line ("[NO] {0}.Item({1}): {2}" -f $Label, $Key, $_.Exception.Message)
        return $null
    }
}

function Show-CollectionShape {
    param(
        [Parameter(Mandatory=$true)]$Collection,
        [Parameter(Mandatory=$true)][string]$Label
    )

    Show-Members -Object $Collection -Label $Label

    $count = $null
    try {
        $count = [int]$Collection.Count
        Add-Line ("[OK] {0}.Count = {1}" -f $Label, $count)
    }
    catch {
        Add-Line ("[NO] {0}.Count: {1}" -f $Label, $_.Exception.Message)
    }

    if ($null -ne $count) {
        $limit = [Math]::Min($count, $MaxCollectionItems)
        foreach ($base in @(0, 1)) {
            Add-Line ("Trying numeric Item() enumeration with base {0}" -f $base)
            for ($offset = 0; $offset -lt $limit; $offset++) {
                $index = $base + $offset
                try {
                    $item = $Collection.Item($index)
                    $name = ""
                    foreach ($candidate in @("Name", "Path", "FullName")) {
                        try {
                            $n = [string]$item.$candidate
                            if (-not [string]::IsNullOrWhiteSpace($n)) {
                                $name = ("; {0}={1}" -f $candidate, $n)
                                break
                            }
                        }
                        catch {
                        }
                    }
                    Add-Line ("  [{0}] {1}{2}" -f $index, (Get-ComTypeText -Object $item), $name)
                }
                catch {
                    Add-Line ("  [{0}] failed: {1}" -f $index, $_.Exception.Message)
                    break
                }
            }
        }
    }

    try {
        $i = 0
        Add-Line "Trying foreach enumeration:"
        foreach ($item in $Collection) {
            Add-Line ("  foreach[{0}] {1}" -f $i, (Get-ComTypeText -Object $item))
            $i++
            if ($i -ge $MaxCollectionItems) {
                break
            }
        }
        Add-Line ("foreach yielded {0} item(s) shown." -f $i)
    }
    catch {
        Add-Line ("foreach failed: {0}" -f $_.Exception.Message)
    }
}

$app = $null

try {
    Add-Line "LeCroy XStream COM automation read-only dump"
    Add-Line ("Timestamp: {0:o}" -f [DateTime]::Now)

    $errors = @()
    foreach ($progId in @("LeCroy.XStreamDSO", "LeCroy.XStreamDSO.1")) {
        try {
            $app = New-Object -ComObject $progId
            Add-Line ("Connected via {0}" -f $progId)
            break
        }
        catch {
            $errors += ("{0}: {1}" -f $progId, $_.Exception.Message)
        }
    }

    if ($null -eq $app) {
        throw ("Could not create XStream COM server. " + ($errors -join " | "))
    }

    Show-Members -Object $app -Label "app"

    $rootObjects = Try-ReadMember -Object $app -Name "Objects" -Label "app"
    if ($null -ne $rootObjects) {
        Show-CollectionShape -Collection $rootObjects -Label "app.Objects"
    }

    $rootObject = Try-ReadMember -Object $app -Name "Object" -Label "app"
    if ($null -ne $rootObject) {
        Show-CollectionShape -Collection $rootObject -Label "app.Object"
    }

    $acq = $null

    if ($null -ne $rootObjects) {
        $acq = Try-Item -Object $rootObjects -Key "Acquisition" -Label "app.Objects"
    }
    if ($null -eq $acq -and $null -ne $rootObject) {
        $acq = Try-Item -Object $rootObject -Key "Acquisition" -Label "app.Object"
    }
    if ($null -eq $acq) {
        $acq = Try-Item -Object $app -Key "Acquisition" -Label "app"
    }

    if ($null -ne $acq) {
        Show-Members -Object $acq -Label "Acquisition"

        $acqObjects = Try-ReadMember -Object $acq -Name "Objects" -Label "Acquisition"
        if ($null -ne $acqObjects) {
            Show-CollectionShape -Collection $acqObjects -Label "Acquisition.Objects"
        }

        $acqObject = Try-ReadMember -Object $acq -Name "Object" -Label "Acquisition"
        if ($null -ne $acqObject) {
            Show-CollectionShape -Collection $acqObject -Label "Acquisition.Object"
        }

        foreach ($name in @("C1", "C2", "Horizontal", "Trigger", "AuxOutput", "AuxiliaryOutput")) {
            $child = Try-Item -Object $acq -Key $name -Label "Acquisition"
            if ($null -eq $child -and $null -ne $acqObjects) {
                $child = Try-Item -Object $acqObjects -Key $name -Label "Acquisition.Objects"
            }
            if ($null -eq $child -and $null -ne $acqObject) {
                $child = Try-Item -Object $acqObject -Key $name -Label "Acquisition.Object"
            }

            if ($null -ne $child) {
                Show-Members -Object $child -Label ("Acquisition/{0}" -f $name)

                if ($name -eq "C1") {
                    foreach ($cvar in @("VerScale", "Coupling", "BandwidthLimit", "ProbeName")) {
                        [void](Try-Item -Object $child -Key $cvar -Label "Acquisition/C1")
                    }
                }
                elseif ($name -eq "Horizontal") {
                    [void](Try-Item -Object $child -Key "HorScale" -Label "Acquisition/Horizontal")
                }
            }
        }
    }
}
finally {
    $fullPath = [System.IO.Path]::GetFullPath($OutputPath)
    [System.IO.File]::WriteAllLines($fullPath, $script:Lines, [System.Text.Encoding]::UTF8)
    Write-Host ""
    Write-Host ("Saved read-only COM dump: {0}" -f $fullPath) -ForegroundColor Cyan

    if ($null -ne $app -and [System.Runtime.InteropServices.Marshal]::IsComObject($app)) {
        try {
            [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($app)
        }
        catch {
        }
    }
}
