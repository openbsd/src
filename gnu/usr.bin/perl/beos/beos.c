#include "beos/beosish.h"

#undef waitpid
#undef kill
#undef sigaction

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <OS.h>

/* In BeOS 5.0 the waitpid() seems to misbehave in that the status
 * has the upper and lower bytes swapped compared with the usual
 * POSIX/UNIX implementations.  To undo the surpise effect to the
 * rest of Perl we need this wrapper.  (The rest of BeOS might be
 * surprised because of this, though.) */

pid_t beos_waitpid(pid_t process_id, int *status_location, int options) {
    pid_t got = waitpid(process_id, status_location, options);
    if (status_location)
      *status_location =
	(*status_location & 0x00FF) << 8 |
	(*status_location & 0xFF00) >> 8;
    return got;
}


/* BeOS kill() doesn't like the combination of the pseudo-signal 0 and
 * specifying a process group (i.e. pid < -1 || pid == 0). We work around
 * by changing pid to the respective process group leader. That should work
 * well enough in most cases. */

int beos_kill(pid_t pid, int sig)
{
    if (sig == 0) {
        if (pid == 0) {
            /* it's our process group */
            pid = getpgrp();
        } else if (pid < -1) {
            /* just address the process group leader */
            pid = -pid;
        }
    }

    return kill(pid, sig);
}

/* sigaction() should fail, if trying to ignore or install a signal handler
 * for a signal that cannot be caught or ignored. The BeOS R5 sigaction()
 * doesn't return an error, though. */
int beos_sigaction(int sig, const struct sigaction *act,
                   struct sigaction *oact)
{
    int result = sigaction(sig, act, oact);

    if (result == 0 && act && act->sa_handler != SIG_DFL
        && act->sa_handler != SIG_ERR && (sig == SIGKILL || sig == SIGSTOP)) {
        result = -1;
        errno = EINVAL;
    }

    return result;
}
