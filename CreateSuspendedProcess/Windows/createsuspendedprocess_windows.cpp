// CreateSuspendedProcess.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <tchar.h>

DWORD getppid()
{
    HANDLE hSnapshot;
    PROCESSENTRY32 pe32;
    DWORD ppid = 0, pid = GetCurrentProcessId();

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    __try 
    {
        if (hSnapshot == INVALID_HANDLE_VALUE) __leave;

        ZeroMemory(&pe32, sizeof(pe32));
        pe32.dwSize = sizeof(pe32);
        if (!Process32First(hSnapshot, &pe32)) __leave;

        do 
        {
            if (pe32.th32ProcessID == pid) 
            {
                ppid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));

    }
    __finally 
    {
        if (hSnapshot != INVALID_HANDLE_VALUE) CloseHandle(hSnapshot);
    }
    return ppid;
}

int _tmain(int argc, TCHAR *argv[])
{
    STARTUPINFOEX sie;
    PROCESS_INFORMATION pi;
    SIZE_T cbAttributeListSize = 0;
    PPROC_THREAD_ATTRIBUTE_LIST pAttributeList = NULL;
    HANDLE hParentProcess = NULL;
    DWORD ppid = 0;

    ZeroMemory(&sie, sizeof(sie));
    sie.StartupInfo.cb = sizeof(sie);
    ZeroMemory(&pi, sizeof(pi));

    if (argc != 2)
    {
        _tprintf(_T("Usage: CreateSuspendedProcess.exe <NAME_OF_EXECUTABLE_TO_START_SUSPENDED>\n"));

	return -1;
    }

    // First get the size of the attribute list so we know how much space to allocate.
    // We're not checking for a return value here because we know the first time we call it will fail.
    InitializeProcThreadAttributeList(NULL, 
                                      1, 
                                      0, 
                                      &cbAttributeListSize);

    // Allocate the space needed for the attribute list
    pAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc(GetProcessHeap(), 
                                                             0, 
                                                             cbAttributeListSize);
    
    // Check if we failed to allocate space for the attribute list
    if (NULL == pAttributeList)
    {
        _tprintf(_T("HeapAlloc error.  GetLastError() = %d.\n"), GetLastError());
        
        return 0;
    }

    // Initialize the attribute list
    if (!InitializeProcThreadAttributeList(pAttributeList, 
                                           1, 
                                           0, 
                                           &cbAttributeListSize))
    {
        _tprintf(_T("InitializeProcThreadAttributeList error.  GetLastError() = %d.\n"), GetLastError());
        
        return 0;
    }

    // Get the PID of our parent process
    ppid = getppid();

    // Check to make sure we got a valid PID
    if (ppid == 0)
    {
        // We failed to find our parent process' PID, so log error and exit
        _tprintf(_T("Failed to locate parent PID.  GetLastError() = %d.\n"), GetLastError());
        return 0;
    }
    else
    {
        _tprintf(_T("Parent process ID = %d.\n"), ppid);
    }

    // Get a handle to our parent process
    hParentProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ppid);

    if (NULL == hParentProcess)
    {
        _tprintf(_T("Couldn't open our parent process.  GetLastError() = %d.\n"), GetLastError());
        return 0;
    }

    // Update our attribute list with those of our parent process
    if (!UpdateProcThreadAttribute(pAttributeList, 
                                   0, 
                                   PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, 
                                   &hParentProcess, 
                                   sizeof(HANDLE), 
                                   NULL, 
                                   NULL))
    {
        _tprintf(_T("UpdateProcThreadAttribute failed.  GetLastError() = %d.\n"), GetLastError());

        return 0;
    }

    // Set the attribute list of our process to be created to those of its new parent (i.e. - not this process)
    sie.lpAttributeList = pAttributeList;

    // Start the child process. 
    if (!CreateProcess(NULL,                                            // No module name (use command line)
                       argv[1],                                         // Command line
                       NULL,                                            // Process handle not inheritable
                       NULL,                                            // Thread handle not inheritable
                       FALSE,                                           // Set handle inheritance to FALSE
                       CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,   // Create the process suspended and inform the OS we've got an extended startupinfo struct present (to set the parent PID)
                       NULL,                                            // Use parent's environment block
                       NULL,                                            // Use parent's starting directory 
                       &sie.StartupInfo,                                // Pointer to STARTUPINFO structure
                       &pi))                                            // Pointer to PROCESS_INFORMATION structure
    {
        _tprintf(_T("CreateProcess failed. GetLastError() = %d.\n"), GetLastError());

        return -1;
    }

    // Assuming we got this far the create process succeeded and the process is in a suspended state.
    // Since for our purpose of using this is to trigger an ML scan if ML did NOT kill the process then we should kill it ourselves so it never runs.
    if (!TerminateProcess(pi.hProcess, 0))
    {
        _tprintf(_T("Failed to terminate process.  GetLastError() = %d.\n"), GetLastError());
        
        return -1;
    }

    // Wait until child process exits/terminates.
    DWORD dwWaitResult = WaitForSingleObject(pi.hProcess, INFINITE);

    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0:
            break;
        // Should never get here since we are waiting infinitely
        case WAIT_ABANDONED:
        case WAIT_TIMEOUT:
            return -1;
        // The wait failed for some reason
        case WAIT_FAILED:
        {
            _tprintf(_T("Failed to wait on process to exit.  GetLastError() = %d.\n"), GetLastError());

            return -1;
        }
    }

    // Print out the process handle value
    _tprintf(_T("Process ID = %d terminated.\n"), (int) pi.dwProcessId);

    // Close process and thread handles. 
    if (!CloseHandle(pi.hProcess))
    {
        _tprintf(_T("Failed to close process handle.  GetLastError() = %d.\n"), GetLastError());

        return -1;
    }

    if (!CloseHandle(pi.hThread))
    {
        _tprintf(_T("Failed to close thread handle.  GetLastError() = %d.\n"), GetLastError());

        return -1;
    }

    return 0;
}
