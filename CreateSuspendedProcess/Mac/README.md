# CreateSuspendedProcess

### DISCLAIMER - Sales Engineering Management would like me to remind you that these utilities are not officially supported by CrowdStrike.  Please use discretion should you decide to share these with a prospect/customer.  I'm sharing the source code here so that you can reference it if you should want to provide some guidance to prospects/customers if they want to do something similar in a production environment but just keep in mind that any requests to our Support organization for assistance/troubleshooting with these tools will not be supported.

## Mac

For the Mac OS the tool behaves in a similar manner.  We leverage the Apple POSIX extension to start a process in a suspended state.  More info about POSIX and Apple's extension can be found [here](https://en.wikipedia.org/wiki/POSIX) and [here](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/posix_spawnattr_getflags.3.html).

You can use the tool in a similar manner to the Windows version like so (of course you can adjust the paths appropriately, etc):

```
./CreateSuspendedProcess <PATH_TO_MALWARE>
```

The SHA256 hash for it is: 410ffac399487e6ca3ba5c34a66c24b8dabdc0336c20aa9de2129df2efdb4c2d

When malware is blocked by ML you'll see a message like so indicating the child (i.e. - malware) was terminated immediately:

```
CS-SE-DJS-MacVM:samples dj.stachniak$ ./CreateSuspendedProcess ./1db30d5b2bb24bcc4b68d647c6a2e96d984a13a28cc5f17596b3bfe316cca342 
PID of ./1db30d5b2bb24bcc4b68d647c6a2e96d984a13a28cc5f17596b3bfe316cca342 = 1123
Child PID 1123 was terminated
```

If ML does not block the child process (i.e. - malware) from executing it will simply terminate the process while it's in a suspended state thus never allowing it to run:

```
CS-SE-DJS-MacVM:samples dj.stachniak$ ./CreateSuspendedProcess /usr/bin/vim
PID of /usr/bin/vim = 1125
Child PID 1125 started in suspended state - killing it...PID 1125 killed
```

The typical operating system path rules apply so if the targeted executable to run can't be found you'll see an error like so:

```
CS-SE-DJS-MacVM:samples dj.stachniak$ ./CreateSuspendedProcess /usr/bin/junk
posix_spawn: No such file or directory
```

If the targeted executable is not an executable binary (or not executable on that machine) you'll see an error like so:

```
CS-SE-DJS-MacVM:samples dj.stachniak$ ./CreateSuspendedProcess ./42098076256facf228c27f796f05a5ad20d5436eb8cd60efbb8c0c2a07a5ab99 
posix_spawn: Bad CPU type in executable
```

I've tested the tool on MacOS High Sierra and Mojave.  Again, your mileage may vary.  Attempts to execute non-binaries (i.e. - documents, etc) may result in undefined behavior.  You also cannot pass command line arguments to the malware to be executed.  Be cognizant of file paths when passing the malware file name as it'll use the usual path searching to locate it.  You shouldn't encounter any library dependencies (I believe they should already be present on your Mac) but please let me know if you encounter any issues running it - these are the needed libs:

```
CS-SE-DJS-MacVM:samples dj.stachniak$ otool -L CreateSuspendedProcess 
CreateSuspendedProcess:
 /usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 400.9.4)
 /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1252.200.5)
```

The Mac version of the tool (binary) is available [here](createsuspendedprocess_mac) (and can be renamed of course).

The source code for the Mac version of the tool is [here](createsuspendedprocess_mac.cpp).

Again, please let me know if you discover any issues, bugs, race conditions, etc.  posix_spawn is a wrapper for the traditional Unix/Linux fork & exec paradigm and allows us to use the suspended flag.  You can learn more about it [here](http://man7.org/linux/man-pages/man3/posix_spawn.3.html).

The CrowdStrike Mac sensor behaves differently than on Windows, of course.  On the MacOS we use the Mandatory Access Control Framework to hook into the OS to be notified of new process creation.  You can learn more about it [here](http://www.trustedbsd.org/mac.html).

And some of the functions we use for new process notification are described [here](https://opensource.apple.com/source/xnu/xnu-4903.221.2/security/mac_policy.h.auto.html).

As with Windows, the new process creation is handled the same by ML regardless of whether or not the process is set to start in a suspended state or runnable like normal.  This may change however as Apple continues their efforts to kick 3rd party developers out of their kernel.
