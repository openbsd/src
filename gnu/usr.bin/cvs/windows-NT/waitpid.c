/* waitpid.c --- waiting for process termination, under Windows NT
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

#include <assert.h>
#include <stdio.h>
#include <process.h>
#include <errno.h>

#include "config.h"

/* Wait for the process PID to exit.  Put the return status in *statusp.
   OPTIONS is not supported yet under Windows NT.  We hope it's always zero.  */
pid_t waitpid (pid, statusp, options)
     pid_t pid;
     int *statusp;
     int options;
{
    /* We don't know how to deal with any options yet.  */
    assert (options == 0);
    
    return _cwait (statusp, pid, _WAIT_CHILD);
}
