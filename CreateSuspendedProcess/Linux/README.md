# CreateSuspendedProcess

### DISCLAIMER - Sales Engineering Management would like me to remind you that these utilities are not officially supported by CrowdStrike.  Please use discretion should you decide to share these with a prospect/customer.  I'm sharing the source code here so that you can reference it if you should want to provide some guidance to prospects/customers if they want to do something similar in a production environment but just keep in mind that any requests to our Support organization for assistance/troubleshooting with these tools will not be supported.

## Linux

On Linux we don't have the capability to start up a process in a suspended state.  There is no equivalent POSIX extension to start a process suspended for Linux like there is on the MacOS (and there is no other similar OS process creation mechanism that I'm aware of).  It would be risky to attempt the traditional fork/exec model and attempt to send the child process a STOP signal immediately after forking and executing as depending on the OS process scheduling the child process could potentially get a time slice to execute at least a few instructions before being paused (i.e. - STOPPED via signal).  So we'll need to get creative.  We'll use the GNU Debugger GDB to start a process and then break on its first instruction.  Fortunately, with the 8.1 release of gdb there was a new command introduced named "starti" which will automatically break on the first instruction.  You can read about it [here](https://lists.gnu.org/archive/html/info-gnu/2018-01/msg00016.html).

```
(gdb) help running
Running the program.
List of commands:
starti -- Start the debugged program stopping at the first instruction
```

This makes our life much easier since not every program (especially malware) has a main procedure entry point, thus rendering the traditional "start" command useless.  Previously, it could be difficult to find the correct entry point and instruction to break on (malware can often be compressed, obfuscated and/or encrypted making things even more difficult to decipher the first instruction address).  Fortunately, the "starti" command simplifies things.

I created a shell script to wrap and call the necessary components to start a child process under gdb.  You can simply run the script like so:

```
./CreateSuspendedProcess <PATH_TO_MALWARE>
```

The SHA256 hash for the underlying gdb executable (named createsuspendedprocess_gdb but you can obviously change the name) is: b2997c73551b4ec5ae6f8f7ddc4072452f38854ff92a9f28caf5fa9d756ee213  ML for Linux may trigger a detection for it but as it's a stock build of 8.2 gdb I don't think it should.  Keep that in mind as you might have to whitelist it or run it from an excluded file path.  To run a child process (i.e. - malware) use the script like so:

```
djstachniak@CS-SE-DJS-KALI:~$ ./CreateSuspendedProcess /usr/bin/vim
/home/djstachniak/csgdb --quiet --batch --command=/home/djstachniak/cs_starti.gdb /usr/bin/vim
Program stopped.
0x00007ffff7fd6210 in ?? () from /lib64/ld-linux-x86-64.so.2
 Num Description Executable 
* 1 process 4348 /usr/bin/vim 
Kill the program being debugged? (y or n) [answered Y; input not from terminal]
[Inferior 1 (process 4348) killed]
```

This starts the program (i.e. - malware) and breaks at the first instruction (thus simulating starting suspended).  It then terminates the process after breaking (presumably, when ML prevention arrives ML will prevent the executable from even starting).  You will see a ProcessRollup2 event with createsuspendedprocess_gdb being the parent process.  We'll need to see how machine learning is implemented into the Linux sensor to verify this is sufficient for testing purposes but in all likelihood this should work fine.  The usual Linux path/searching rules apply so be aware of absolute/relative paths when passing the name of the malware (i.e - child process).  You'll see output like so if the executable cannot be located:

```
djstachniak@CS-SE-DJS-KALI:~$ ./CreateSuspendedProcess /usr/bin/junk
/home/djstachniak/csgdb --quiet --batch --command=/home/djstachniak/cs_starti.gdb /usr/bin/junk
/usr/bin/junk: No such file or directory.
/home/djstachniak/cs_starti.gdb:2: Error in sourced command file:
No executable file specified.
Use the "file" or "exec-file" command.
```

Also, this build of gdb is dynamically linked, so you'll need to be sure you have the necessary library dependencies installed:

```
djstachniak@CS-SE-DJS-KALI:~$ ldd ./csgdb 
linux-vdso.so.1 (0x00007ffd4a9a7000)
 libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007faaa4dcf000)
 libstdc++.so.6 => /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007faaa4c4c000)
 libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007faaa4ab8000)
 libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007faaa4a9e000)
 libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007faaa48e1000)
 /lib64/ld-linux-x86-64.so.2 (0x0000563ff328a000)
```

I don't want to have to go through the effort to build and statically link all the necessary libraries but you should be fine as these libraries should already be present on your Linux system.  I've tested on Debian/Kali Linux, CentOS 7 and Ubuntu 18.  Your mileage may vary.  Please let me know if you encounter any issues.

You'll need the to download the following components.

First, the shell script to run gdb (note I renamed the script in my examples above as "CreateSuspendedProcess") is [here](createsuspendedprocess_linux).

The script contents are like so (note the paths to the createsuspendedprocess_gdb executable and createsuspendedprocess_starti.gdb command file that can be changed):

```
#! /bin/bash
if [ $# -ne 1 ]; then
 echo "This script requires only 1 argument: <name_of_executable_to_run>"
 exit 1
fi
echo "/home/djstachniak/createsuspendedprocess_gdb --quiet --batch --command=/home/djstachniak/createsuspendedprocess_starti.gdb $1"
/home/djstachniak/createsuspendedprocess_gdb --quiet --batch --command=/home/djstachniak/createsuspendedprocess_starti.gdb $1
```

The "–-quiet" switch simply suppresses the startup banner (i.e. - gdb version info, etc).  The "–-batch" switch tells gdb to execute all the commands in the file passed in with the "–-command" switch and then exit.  You'll want to modify it to appropriately set the paths for the createsuspendedprocess_gdb executable and the createsuspendedprocess_starti.gdb command file.  Of course, all the typical Linux path and security/permission rules apply.  The createsuspendedprocess_starti.gdb command file is [here](createsuspendedprocess_starti.gdb) (which can be renamed as well).

It contains the following gdb commands:

```
set startup-with-shell off
starti
info inferiors
kill
quit
```

These are the commands we're telling gdb to execute when it starts via the "--command" argument in the shell script.  The "set startup-with-shell off" command tells gdb to not start the child process via a shell (this way when we are searching for PR2s we can directly find gdb as the parent process versus having an intermediary shell which just complicates the process tree).  The "starti" instruction tells gdb to break at the first instruction address.  The "info inferiors" command just outputs info on the child process being debugged and tells us its PID.  The "kill" command terminates the child process and the "quit" command simply tells gdb to exit. 

The actual 8.2 build of gdb is [here](createsuspendedprocess_gdb) (named createsuspendedprocess_gdb but this can be changed as well).

Info on gdb is [here](https://www.gnu.org/software/gdb/).

This is a 64-bit build of gdb and thus will only run on 64-bit Linux versions:

```
djstachniak@CS-SE-DJS-KALI:~$ file csgdb 
csgdb: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, for GNU/Linux 2.6.32, BuildID[sha1]=f8057f25682dc45b372b74c1103620e0f80350de, with debug_info, not stripped
```

It can, however, be used to execute 32-bit applications (malware) on 64-bit Linux.  Please let me know if you encounter any issues using it.
