/* rcmd.c --- execute a command on a remote host from OS/2
   Karl Fogel <kfogel@cyclic.com> --- November 1995  */

#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
/* <sys/socket.h> wants `off_t': */
#include <sys/types.h>
/* This should get us ibmtcpip\include\sys\socket.h: */
#include <sys/socket.h>
#include <assert.h>

#include "rcmd.h"

void
init_sockets ()
{
	int rc;

	rc = sock_init ();
    if (rc != 0)
    {
      fprintf (stderr, "sock_init() failed -- returned %d!\n", rc);
      exit (1);
    }
}


static int
resolve_address (const char **ahost, struct sockaddr_in *sai)
{
    fprintf (stderr,
             "Error: resolve_address() doesn't work.\n");
    exit (1);
}

static int
bind_and_connect (struct sockaddr_in *server_sai)
{
    fprintf (stderr,
             "Error: bind_and_connect() doesn't work.\n");
    exit (1);
}

static int
rcmd_authenticate (int fd, char *locuser, char *remuser, char *command)
{
    fprintf (stderr,
             "Error: rcmd_authenticate() doesn't work.\n");
    exit (1);
}

int
rcmd (const char **ahost,
      unsigned short inport,
      char *locuser,
      char *remuser,
      char *cmd,
      int *fd2p)
{
    fprintf (stderr,
             "Error: rcmd() doesn't work.\n");
    exit (1);
}
