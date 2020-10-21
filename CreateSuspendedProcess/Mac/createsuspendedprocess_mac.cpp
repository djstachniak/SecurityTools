#include <spawn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define errExitEN(en, msg) do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

char **environ;

int main(int argc, char *argv[])
{
	pid_t child_pid = 0;
        int s = 0;
        posix_spawnattr_t attr;

        s = posix_spawnattr_init(&attr);
        if (s != 0)
	{
        	errExitEN(s, "posix_spawnattr_init");
        }

	// Set the flag to start the process suspended                   
        s = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
        if (s != 0)
	{
        	errExitEN(s, "posix_spawnattr_setflags");
        }

        // Spawn the child. The name of the program to execute and the
        // command-line arguments are taken from the command-line arguments
        // of this program. The environment of the program execed in the
        // child is made the same as the parent's environment.

        s = posix_spawnp(&child_pid, 
                         argv[1], 
                         NULL, 
                         &attr,
                         NULL, 
                         environ);
        
	// If the spawn failed...   
        if (s != 0)
	{
               errExitEN(s, "posix_spawn");
	}

	printf("PID of %s = %ld\n", argv[1], (long) child_pid);

	// Because posix_spawn wraps fork & exec*, the fork call returns us a child PID 
	// but the exec* may have failed, so we'll wait to see if the child process has terminated
	siginfo_t info;
	info.si_pid = 0; // Be sure to zero out the PID in case of a wait failure
	
	if ((0 == waitid(P_PID, 
			 child_pid, 
			 &info, 
			 WEXITED | WNOWAIT)) && // Let this short circuit... 
	    (info.si_pid == child_pid)) // si_pid should be set to the child PID if the wait call succeeds
	{
		// If this is a signal sent to the parent by child - it should always be set to this
		if (info.si_signo == SIGCHLD)
		{
			// If the process was killed by ML or exited normally then no need to stop it
			if (info.si_code == CLD_EXITED || info.si_code == CLD_DUMPED || info.si_code == CLD_KILLED)
			{
				// The child process is not running (even in a suspended state)
				printf("Child PID %ld was terminated\n", (long) child_pid);
			}
			else if (info.si_code == CLD_STOPPED) // The process was started in a suspended state
			{
				// If we got this far then process is sitting in a suspended state, so we'll kill it ourselves
        			printf("Child PID %ld started in suspended state - killing it...", (long) child_pid);

				if (kill(child_pid, SIGKILL))
        			{
                			// We failed to kill the suspended program
					errExit("kill failed");
				}
				else
				{
					printf("PID %ld killed\n", (long) child_pid);
				}
			}
		}
	}
    
	exit(EXIT_SUCCESS);
}
