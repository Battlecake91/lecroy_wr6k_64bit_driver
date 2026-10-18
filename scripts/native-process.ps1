<#
.SYNOPSIS
  Run a native process with bounded execution and independent output capture.
.DESCRIPTION
  This Windows-only helper uses System.Diagnostics.Process rather than the
  PowerShell native pipeline. Standard output and standard error are drained
  concurrently, and stderr is treated as diagnostic data. A Windows job object
  bounds the launched process tree and terminates only that tree on timeout or
  after the root process exits. The implementation is compatible with Windows
  PowerShell 5.1 and PowerShell 7.
#>

Set-StrictMode -Version Latest

if (-not ('LecS65.NativeProcessRunner' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace LecS65
{
    public sealed class NativeProcessResult
    {
        public int ProcessId { get; set; }
        public int ExitCode { get; set; }
        public bool TimedOut { get; set; }
        public string StdOut { get; set; }
        public string StdErr { get; set; }
        public bool JobAssigned { get; set; }
        public bool ProcessTreeClosed { get; set; }
    }

    public static class NativeProcessRunner
    {
        private const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000;
        private const int JobObjectExtendedLimitInformation = 9;

        [StructLayout(LayoutKind.Sequential)]
        private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
        {
            public long PerProcessUserTimeLimit;
            public long PerJobUserTimeLimit;
            public uint LimitFlags;
            public UIntPtr MinimumWorkingSetSize;
            public UIntPtr MaximumWorkingSetSize;
            public uint ActiveProcessLimit;
            public UIntPtr Affinity;
            public uint PriorityClass;
            public uint SchedulingClass;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct IO_COUNTERS
        {
            public ulong ReadOperationCount;
            public ulong WriteOperationCount;
            public ulong OtherOperationCount;
            public ulong ReadTransferCount;
            public ulong WriteTransferCount;
            public ulong OtherTransferCount;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
        {
            public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
            public IO_COUNTERS IoInfo;
            public UIntPtr ProcessMemoryLimit;
            public UIntPtr JobMemoryLimit;
            public UIntPtr PeakProcessMemoryUsed;
            public UIntPtr PeakJobMemoryUsed;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateJobObject(IntPtr securityAttributes, string name);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetInformationJobObject(
            IntPtr job, int informationClass,
            ref JOBOBJECT_EXTENDED_LIMIT_INFORMATION information,
            uint informationLength);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        private static string QuoteArgument(string value)
        {
            if (value == null)
                throw new ArgumentNullException("value");
            if (value.Length == 0)
                return "\"\"";
            if (value.IndexOfAny(new char[] { ' ', '\t', '\n', '\v', '"' }) < 0)
                return value;

            StringBuilder quoted = new StringBuilder();
            quoted.Append('"');
            int backslashes = 0;
            foreach (char character in value)
            {
                if (character == '\\')
                {
                    backslashes++;
                    continue;
                }
                if (character == '"')
                {
                    quoted.Append('\\', backslashes * 2 + 1);
                    quoted.Append('"');
                    backslashes = 0;
                    continue;
                }
                quoted.Append('\\', backslashes);
                backslashes = 0;
                quoted.Append(character);
            }
            quoted.Append('\\', backslashes * 2);
            quoted.Append('"');
            return quoted.ToString();
        }

        private static string BuildArguments(string[] arguments)
        {
            List<string> quoted = new List<string>();
            foreach (string argument in arguments)
                quoted.Add(QuoteArgument(argument));
            return String.Join(" ", quoted.ToArray());
        }

        public static NativeProcessResult Run(
            string fileName, string[] arguments, string workingDirectory,
            int timeoutMilliseconds, int streamDrainMilliseconds)
        {
            if (String.IsNullOrWhiteSpace(fileName))
                throw new ArgumentException("Native executable path is empty.", "fileName");
            if (arguments == null)
                throw new ArgumentNullException("arguments");
            if (String.IsNullOrWhiteSpace(workingDirectory))
                throw new ArgumentException("Working directory is empty.", "workingDirectory");
            if (timeoutMilliseconds <= 0 || streamDrainMilliseconds <= 0)
                throw new ArgumentOutOfRangeException("Timeouts must be positive.");

            IntPtr job = IntPtr.Zero;
            Process process = null;
            try
            {
                job = CreateJobObject(IntPtr.Zero, null);
                if (job == IntPtr.Zero)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateJobObject failed.");

                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits =
                    new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                        ref limits, (uint)Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION))))
                    throw new Win32Exception(Marshal.GetLastWin32Error(),
                        "SetInformationJobObject failed.");

                ProcessStartInfo start = new ProcessStartInfo();
                start.FileName = fileName;
                start.Arguments = BuildArguments(arguments);
                start.WorkingDirectory = workingDirectory;
                start.UseShellExecute = false;
                start.CreateNoWindow = true;
                start.RedirectStandardOutput = true;
                start.RedirectStandardError = true;

                process = new Process();
                process.StartInfo = start;
                if (!process.Start())
                    throw new InvalidOperationException("Process.Start returned false.");
                if (!AssignProcessToJobObject(job, process.Handle))
                {
                    int error = Marshal.GetLastWin32Error();
                    try { if (!process.HasExited) process.Kill(); } catch { }
                    throw new Win32Exception(error, "AssignProcessToJobObject failed.");
                }

                Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
                Task<string> stderrTask = process.StandardError.ReadToEndAsync();
                bool exited = process.WaitForExit(timeoutMilliseconds);
                int exitCode = exited ? process.ExitCode : 124;

                CloseHandle(job);
                job = IntPtr.Zero;

                if (!exited && !process.WaitForExit(streamDrainMilliseconds))
                    throw new TimeoutException("Timed-out native process did not terminate after job closure.");

                Task[] streamTasks = new Task[] { stdoutTask, stderrTask };
                if (!Task.WaitAll(streamTasks, streamDrainMilliseconds))
                    throw new TimeoutException("Native process output streams did not close after process-tree cleanup.");

                return new NativeProcessResult {
                    ProcessId = process.Id,
                    ExitCode = exitCode,
                    TimedOut = !exited,
                    StdOut = stdoutTask.Result,
                    StdErr = stderrTask.Result,
                    JobAssigned = true,
                    ProcessTreeClosed = true
                };
            }
            finally
            {
                if (job != IntPtr.Zero)
                    CloseHandle(job);
                if (process != null)
                    process.Dispose();
            }
        }
    }
}
'@
}

function New-LecS65ExclusiveFile {
    param([Parameter(Mandatory=$true)][string]$Path)

    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Native process log parent does not exist: $parent"
    }
    $stream = [System.IO.File]::Open(
        $Path, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    $stream.Dispose()
}

function Write-LecS65NativeLogs {
    param(
        [Parameter(Mandatory=$true)][string]$LogPath,
        [AllowEmptyString()][string]$StdOut,
        [AllowEmptyString()][string]$StdErr,
        [Parameter(Mandatory=$true)][string]$Summary
    )

    $encoding = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText($LogPath + '.stdout.txt', $StdOut, $encoding)
    [System.IO.File]::WriteAllText($LogPath + '.stderr.txt', $StdErr, $encoding)
    $combined = @(
        '=== CHILD STDOUT ==='
        $StdOut
        '=== CHILD STDERR ==='
        $StdErr
        $Summary
    ) -join [Environment]::NewLine
    [System.IO.File]::WriteAllText($LogPath, $combined + [Environment]::NewLine,
        $encoding)
}

function Invoke-LecS65NativeProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][AllowEmptyCollection()][string[]]$Arguments,
        [string]$WorkingDirectory = (Get-Location).Path,
        [string]$LogPath,
        [ValidateRange(1, 86400)][int]$TimeoutSeconds = 900,
        [ValidateRange(1, 300)][int]$StreamDrainTimeoutSeconds = 15,
        [switch]$AllowPositiveExitCode,
        [switch]$ReturnOnNonZeroExit
    )

    $executable = [System.IO.Path]::GetFullPath($FilePath)
    $working = [System.IO.Path]::GetFullPath($WorkingDirectory)
    if (-not [System.IO.Path]::IsPathRooted($FilePath)) {
        throw "Native executable path is not absolute: $FilePath"
    }
    if (-not [System.IO.Path]::IsPathRooted($WorkingDirectory) -or
        -not (Test-Path -LiteralPath $working -PathType Container)) {
        throw "Native working directory is missing or not absolute: $WorkingDirectory"
    }

    $resolvedLog = $null
    if (-not [string]::IsNullOrWhiteSpace($LogPath)) {
        if (-not [System.IO.Path]::IsPathRooted($LogPath)) {
            throw "Native process log path must be absolute: $LogPath"
        }
        $resolvedLog = [System.IO.Path]::GetFullPath($LogPath)
        $logPaths = @(
            $resolvedLog
            ($resolvedLog + '.stdout.txt')
            ($resolvedLog + '.stderr.txt')
        )
        foreach ($path in $logPaths) {
            if (Test-Path -LiteralPath $path) {
                throw "Native process log path already exists: $path"
            }
        }
        foreach ($path in $logPaths) {
            New-LecS65ExclusiveFile -Path $path
        }
    }

    Write-Host ''
    Write-Host ("=== {0} ===" -f $Name) -ForegroundColor Cyan
    try {
        $result = [LecS65.NativeProcessRunner]::Run(
            $executable, [string[]]$Arguments, $working,
            [int]($TimeoutSeconds * 1000),
            [int]($StreamDrainTimeoutSeconds * 1000))
    }
    catch {
        $message = "$Name failed to start or supervise: $($_.Exception.Message)"
        if ($null -ne $resolvedLog) {
            Write-LecS65NativeLogs -LogPath $resolvedLog -StdOut '' `
                -StdErr ($message + [Environment]::NewLine) `
                -Summary '=== PROCESS START/SUPERVISION FAILURE ==='
        }
        throw $message
    }

    if ($null -ne $resolvedLog) {
        $summary = if ($result.TimedOut) {
            "=== PROCESS TIMEOUT; EXIT CODE: $($result.ExitCode) ==="
        }
        else {
            "=== PROCESS EXIT CODE: $($result.ExitCode) ==="
        }
        Write-LecS65NativeLogs -LogPath $resolvedLog -StdOut $result.StdOut `
            -StdErr $result.StdErr -Summary $summary
    }
    if (-not [string]::IsNullOrEmpty($result.StdOut)) {
        Write-Host $result.StdOut.TrimEnd([char[]]@("`r", "`n"))
    }
    if (-not [string]::IsNullOrEmpty($result.StdErr)) {
        [Console]::Error.Write($result.StdErr)
    }
    if ($result.TimedOut) {
        throw "$Name timed out after $TimeoutSeconds seconds; its assigned process tree was terminated."
    }
    if ($result.ExitCode -ne 0 -and
        -not ($AllowPositiveExitCode -and $result.ExitCode -gt 0) -and
        -not $ReturnOnNonZeroExit) {
        throw "$Name failed with exit code $($result.ExitCode)."
    }
    return $result
}
