function CreateSuspendedProcess
{
<#
.SYNOPSIS

Starts a process (typically malware) in a suspended state via a call to CreateProcess with the CREATE_SUSPENDED flag set.  This will give any AV or NextGen AV the chance to examine
the file (process) and determine if it wants to block it.  If the process is not block by any security vendor, then the process is killed while sitting in a suspended state, thus
guaranteeing that none of its (potentially) malicious code is allowed to execute.

.DESCRIPTION

CreateSuspendedProcess starts a process (typically malware) in a suspended state via a call to CreateProcess with the CREATE_SUSPENDED flag set.  This will give any AV or NextGen AV (machine learning, etc)
the chance to examine the file (process) and determine if it wants to block it.  If the process is not block by any security vendor, then the process is killed while sitting in a suspended state, thus
guaranteeing that none of its (potentially) malicious code is allowed to execute.

.PARAMETER filename
A string that contains the name of the file (process) to be executed.

.OUTPUTS

None

.EXAMPLE
Process that is prevented by CrowdStrike Falcon Sensor Machine Learning

C:\PS> CreateSuspendedProcess C:\CrowdStrike\Malware\A4C1C37DF78B626DD40AB4BA83D243690111E6F0FBE9CB8EC804E74EF406B9FD.exe -Verbose
VERBOSE: Attempting to execute C:\CrowdStrike\Malware\A4C1C37DF78B626DD40AB4BA83D243690111E6F0FBE9CB8EC804E74EF406B9FD.exe
VERBOSE: Failed to start C:\CrowdStrike\Malware\A4C1C37DF78B626DD40AB4BA83D243690111E6F0FBE9CB8EC804E74EF406B9FD.exe LastError = System.ComponentModel.Win32Exception (0x80004005): Access is denied

.EXAMPLE
Process that is allowed by CrowdStrike Falcon Sensor Machine Learning

C:\PS>CreateSuspendedProcess C:\Windows\System32\notepad.exe -Verbose
VERBOSE: Attempting to execute C:\Windows\System32\notepad.exe
VERBOSE: Successfully started C:\Windows\System32\notepad.exe PID = 12112
VERBOSE: Thread info for PID 12112 @{Id=2920; ThreadState=Wait; WaitReason=Suspended}
VERBOSE: Successfully terminated C:\Windows\System32\notepad.exe PID = 12112
#>
     param([parameter(Mandatory=$true)]
          [ValidateNotNullOrEmpty()]
          [ValidateScript({
            if(-Not ($_ | Test-Path) ){
                throw "File or folder does not exist" 
            }
            if(-Not ($_ | Test-Path -PathType Leaf) ){
                throw "The Path argument must be a file. Folder paths are not allowed."
            }
            return $true
        })]
        [System.IO.FileInfo] $path)

    function Invoke-CreateProcess 
    {
        param ([Parameter(Mandatory = $True)]
               [string] $Binary,
               [Parameter(Mandatory = $False)]
               [string] $Args=$null,
               [Parameter(Mandatory = $True)]
               [string] $CreationFlags,
               [Parameter(Mandatory = $True)]
               [string] $ShowWindow,
               [Parameter(Mandatory = $True)]
               [string] $StartF)  
    
# Define all the structures for CreateProcess
Add-Type -TypeDefinition @"
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)]
public struct PROCESS_INFORMATION
{
    public IntPtr hProcess; public IntPtr hThread; public uint dwProcessId; public uint dwThreadId;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct STARTUPINFO
{
    public uint cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public uint dwX; public uint dwY; public uint dwXSize; public uint dwYSize; public uint dwXCountChars;
    public uint dwYCountChars; public uint dwFillAttribute; public uint dwFlags; public short wShowWindow;
    public short cbReserved2; public IntPtr lpReserved2; public IntPtr hStdInput; public IntPtr hStdOutput;
    public IntPtr hStdError;
}

[StructLayout(LayoutKind.Sequential)]
public struct SECURITY_ATTRIBUTES
{
    public int length; public IntPtr lpSecurityDescriptor; public bool bInheritHandle;
}

public static class Kernel32
{
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CreateProcess(
        string lpApplicationName, string lpCommandLine, ref SECURITY_ATTRIBUTES lpProcessAttributes, 
        ref SECURITY_ATTRIBUTES lpThreadAttributes, bool bInheritHandles, uint dwCreationFlags, 
        IntPtr lpEnvironment, string lpCurrentDirectory, ref STARTUPINFO lpStartupInfo, 
        out PROCESS_INFORMATION lpProcessInformation);
}
"@
        
        # StartupInfo Struct
        $StartupInfo = New-Object STARTUPINFO
        $StartupInfo.dwFlags = $StartF # StartupInfo.dwFlag
        $StartupInfo.wShowWindow = $ShowWindow # StartupInfo.ShowWindow
        $StartupInfo.cb = [System.Runtime.InteropServices.Marshal]::SizeOf($StartupInfo) # Struct Size
        
        # ProcessInfo Struct
        $ProcessInfo = New-Object PROCESS_INFORMATION
        
        # SECURITY_ATTRIBUTES Struct (Process & Thread)
        $SecAttr = New-Object SECURITY_ATTRIBUTES
        $SecAttr.Length = [System.Runtime.InteropServices.Marshal]::SizeOf($SecAttr)
        $SecAttr.bInheritHandle = $false
        $SecAttr.lpSecurityDescriptor = [IntPtr]::Zero
        
        # lpCurrentDirectory
        $CurrentDir = (Get-Item -Path ".\" -Verbose).FullName
        
        $process_id = 0
    
        # Call CreateProcess
        $retval = [Kernel32]::CreateProcess($Binary, $Args, [ref] $SecAttr, [ref] $SecAttr, $false, $CreationFlags, [IntPtr]::Zero, $CurrentDir, [ref] $StartupInfo, [ref] $ProcessInfo)
    
        if ($retval)
        {
            $output = "Successfully started $binary PID = " + $ProcessInfo.dwProcessId
            Write-Verbose $output
            $ProcessInfo.dwProcessId
        }
        else
        {
            $output = "Failed to start $binary LastError = " + [ComponentModel.Win32Exception][Runtime.InteropServices.Marshal]::GetLastWin32Error()
            Write-Verbose $output
            $process_id
        }
    }

    $fullpath = ""
	
    if ([System.IO.Path]::IsPathRooted($path))
    {
        $fullpath = $path
    }
    else
    {
        $fullpath = Convert-Path $path
    }

    $output = "Attempting to execute $fullpath"
    Write-Verbose $output

    $process_id = Invoke-CreateProcess -Binary $fullpath -CreationFlags 0x4 -ShowWindow 0x1 -StartF 0x1

    if ($process_id -ne 0)
    {
        $process = Get-Process -Id $process_id
        $threads = $process.Threads
        $thread_info = $threads | Select-Object Id,ThreadState,WaitReason
        $output = "Thread info for PID " + $process_id + " " + $thread_info
        Write-Verbose $output

        # The process was successfully started, so now terminate it while still suspended
        Stop-Process -Force -Id $process_id

        $output = "Successfully terminated $fullpath PID = " + $process_id
        Write-Verbose $output
    }
}
