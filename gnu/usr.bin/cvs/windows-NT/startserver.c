/* startserver.c --- open a connection to the CVS server under Windows NT
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

#include "cvs.h"
#include "rcmd.h"

#include <stdlib.h>
#include <winsock.h>
#include <malloc.h>
#include <io.h>
#include <errno.h>

void
wnt_start_server (int *tofd, int *fromfd,
		  char *client_user,
		  char *server_user,
		  char *server_host,
		  char *server_cvsroot)
{
    char *cvs_server;
    char *command;
    struct servent *sptr;
    unsigned short port;
    int read_fd;
    char *portenv;
    
    if (! (cvs_server = getenv ("CVS_SERVER")))
        cvs_server = "cvs";
    command = xmalloc (strlen (cvs_server)
		       + strlen (server_cvsroot)
		       + 50);
    sprintf (command, "%s -d %s server", cvs_server, server_cvsroot);

    portenv = getenv("CVS_RCMD_PORT");
    if (portenv)
	port = atoi(portenv);
    else if ((sptr = getservbyname("shell", "tcp")) != NULL)
	port = sptr->s_port;
    else
	port = IPPORT_CMDSERVER; /* shell/tcp */

    read_fd = rcmd (&server_host,
    	            port,
    	            client_user,
	            (server_user ? server_user : client_user),
	            command,
	            0);
    if (read_fd < 0)
	error (1, errno, "cannot start server via rcmd");

    *tofd = read_fd;
    *fromfd = read_fd;
    free (command);
}


void
wnt_shutdown_server (int fd)
{
    SOCKET s;

    s = fd;
    if (shutdown (s, 2) == SOCKET_ERROR)
        error (1, 0, "couldn't shutdown server connection");
    if (closesocket (s) == SOCKET_ERROR)
        error (1, 0, "couldn't close server connection");
}
