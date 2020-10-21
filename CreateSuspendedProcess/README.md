# CreateSuspendedProcess

## Background

Modern operating systems have the ability to suspend a process from running.  An operating system has a component called a scheduler which schedules the amount of time a thread in a process gets to execute on a processor (a timeslice).  You can learn more about this [here](https://en.wikipedia.org/wiki/Scheduling_(computing)).

We can request that the operating system suspend or resume scheduling a process/thread for execution via API and command line utilities.

For example, say we start a process on Windows:

```
PS C:\> $notepad = Start-Process "notepad.exe" -PassThru
PS C:\> $notepad

Handles  NPM(K)    PM(K)      WS(K)     CPU(s)     Id  SI ProcessName
-------  ------    -----      -----     ------     --  -- -----------
    231      14     3140      13616       0.11  18820   1 notepad
```

In this case we started notepad.exe and can see that it has a pid (process identifier) of 18820.  We can use the SysInternals utility [PsList](https://docs.microsoft.com/en-us/sysinternals/downloads/pslist) to view the threads inside the process and their current state:

```
PS C:\> PsList -x 18820

Process and thread information for COMPUTERNAME:

Name                Pid      VM      WS    Priv Priv Pk   Faults   NonP Page
notepad           18820 4194303   13616    3140    3140     3559     13  228
    Tid     Pri    Cswtch            State    User Time   Kernel Time   Elapsed Time
    10584    10       769     Wait:UserReq  0:00:00.000   0:00:00.109    0:00:26.506
    7204      8        15       Wait:Queue  0:00:00.000   0:00:00.000    0:00:26.484
    27176     8        13       Wait:Queue  0:00:00.000   0:00:00.000    0:00:26.484
    23720     8         9       Wait:Queue  0:00:00.000   0:00:00.000    0:00:26.484
    27436     8         3       Wait:Queue  0:00:00.000   0:00:00.000    0:00:26.452
    26204     8         1     Wait:UserReq  0:00:00.000   0:00:00.000    0:00:26.448
    17220     8         1       Wait:Queue  0:00:00.000   0:00:00.000    0:00:26.448
```

We can see the state of each thread within the process.  Next we can use the SysInternals utility [PsSuspend](https://docs.microsoft.com/en-us/sysinternals/downloads/pssuspend) to suspend all the threads in the process:

```
PS C:\> PsSuspend 18820

Process 18820 suspended.
```

Again if we use PsList to view the state of the threads within the notepad.exe process, we now see that they're all suspended:

```
PS C:\> PsList -x 18820

Process and thread information for COMPUTERNAME:

Name                Pid      VM      WS    Priv Priv Pk   Faults   NonP Page
notepad           18820 4194303   13628    3128    3140     3562     13  212
      Tid   Pri    Cswtch            State    User Time   Kernel Time   Elapsed Time
    10584    10      1635   Wait:Suspended  0:00:00.015   0:00:00.125    0:00:48.729
    7204      8        16   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.708
    27176     8        14   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.707
    23720     8        10   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.707
    27436     8         4   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.675
    26204     8         1   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.671
    17220     8         3   Wait:Suspended  0:00:00.000   0:00:00.000    0:00:48.671
```
    
At this point all the threads in notepad.exe are suspended and the process isn't doing anything because none of its threads are currently eligible to be scheduled for processor time.  We can then resume the process using PsSuspend once more (except this time we pass the -r switch to resume the process):

```
PS C:\> PsSuspend -r 18820

Process 18820 resumed.
```

Now notepad.exe is running again as normal.  We can verify this with PsList:

```
PS C:\> PsList -x 18820

Process and thread information for COMPUTERNAME:

Name                Pid      VM      WS    Priv Priv Pk   Faults   NonP Page
notepad           18820 4194303   13628    3128    3140     3562     13  212
      Tid Pri    Cswtch            State    User Time   Kernel Time   Elapsed Time
    10584  10      1696     Wait:UserReq  0:00:00.015   0:00:00.125    0:01:03.669
     7204   8        17       Wait:Queue  0:00:00.000   0:00:00.000    0:01:03.647
    27176   8        15       Wait:Queue  0:00:00.000   0:00:00.000    0:01:03.647
    23720   8        11       Wait:Queue  0:00:00.000   0:00:00.000    0:01:03.647
    27436   8         5       Wait:Queue  0:00:00.000   0:00:00.000    0:01:03.614
    26204   8         2     Wait:UserReq  0:00:00.000   0:00:00.000    0:01:03.611
    17220   8         4       Wait:Queue  0:00:00.000   0:00:00.000    0:01:03.611
```

Other operating systems such as MacOS and Linux behave in the same manner.

Linux example:

```
[root@COMPUTERNAME ~]# ping www.yahoo.com 2>&1 > /dev/null &
[1] 30396
[root@COMPUTERNAME ~]# ps -o pid,state,cmd -p 30396
  PID S CMD
30396 S ping www.yahoo.com 2
```

Note the state S for pid 30396 indicates it's currently sleeping & interruptable (meaning it's technically running but doing nothing - and another state value is R when it actually is running and doing something).  If we stop the process (i.e. - suspend it), we see the following change in state:

```
[root@COMPUTERNAME ~]# kill -STOP 30396
[root@COMPUTERNAME ~]# ps -o pid,state,cmd -p 30396
  PID S CMD
30396 T ping www.yahoo.com 2
```

You see the state for pid 30396 now shows T meaning it is stopped (suspended).  We can then continue (resume) it like so:

```
[root@COMPUTERNAME ~]# kill -CONT 30396
[root@COMPUTERNAME ~]# ps -o pid,state,cmd -p 30396
  PID S CMD
30396 S ping www.yahoo.com 2
```

And now we see the thread state go back to S for pid 30396.  MacOS behaves in a similar manner.

Thus, you see how we can take a running process and suspend it, or take a suspended process and resume it.

## Description

Since we just saw how we can take a running process and suspend it, we can actually use APIs for an operating system to create (start) a process in a suspended state upon startup of the process.  Both Windows and MacOS support this functionality (Linux currently does *not* but we have a workaround for that which we'll get to later).

Creating a process in a suspended state starts a process normally but stops before executing its first instruction.  This means the process will just be sitting idle (suspended) until you tell it to resume (and start executing instructions).

## Use Cases

### Safely Testing Malware

Being able to start a process in a suspended state is useful for many reasons but for our purpose we want to leverage that ability for malware testing.  If we can start a process in a suspended state, it will go through all the regular operating system mechanisms to create a process and in turn give any endpoint security solutions the opportunity to examine the process the OS is creating and determine whether or not it should be allowed to start.  For example, next gen machine learning engines can analyze the process attempting to be started and evaluate whether or not it is malicious.  If it is malicious, then it can tell the OS not to start the process.  If it is determined to be benign, then it can tell the OS to start the process.  However, in the latter case, the OS will just start the process in a suspended state.  So if we want to safely test malware, we can use this option and rest assured that if any AV/NGAV/ML solution "misses" the malware, it'll just be sitting in a suspended state and can be killed without doing any damage to the system.

### System Scans

Since CrowdStrike currently does *not* offer any system file scanning capability, we can use these tools to script scans of folders and files letting machine learning analyze each file as its attempted to be started in a suspended state.  Examples to follow in each subfolder for the different operating systems.

## CreateSuspendedProcess Tools

In this project I have built utilities to be able to start a process in a suspended state.  There's both an executable for Windows and a PowerShell script option.  For MacOS there's an executable option.  For Linux, there's no API to be able to start a process suspended but we can leverage the GNU GDB debugger to start a process and stop before its first instruction executes.

The individual tools are available here:

* [Windows](Windows)
* [PowerShell](Windows/PowerShell)
* [Mac](Mac)
* [Linux](Linux)

