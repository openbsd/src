/* startserver.c --- open a connection to the CVS server under Windows NT
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

#include "cvs.h"
#include "rcmd.h"

#include <stdlib.h>
#include <winsock.h>
#include <malloc.h>
#include <io.h>
#include <errno.h>


/* Apply the Winsock shutdown function to a CRT file descriptor.  */
static void
shutdown_fd (int fd, int how)
{
    SOCKET s;
    
    if ((s = _get_osfhandle (fd)) < 0)
        error (1, errno, "couldn't get socket handle from file descriptor");
    if (shutdown (s, how) == SOCKET_ERROR)
        error (1, 0, "couldn't shut down socket half");
}


void
wnt_start_server (int *tofd, int *fromfd,
		  char *client_user,
		  char *server_user,
		  char *server_host,
		  char *server_cvsroot)
{
    char *cvs_server;
    char *command;
    struct servent *s;
    unsigned short port;
    int read_fd, write_fd;

    if (! (cvs_server = getenv ("CVS_SERVER")))
        cvs_server = "cvs";
    command = alloca (strlen (cvs_server)
    		      + strlen (server_cvsroot)
		      + 50);
    sprintf (command, "%s -d %s server", cvs_server, server_cvsroot);

    if ((s = getservbyname("shell", "tcp")) == NULL)
	port = IPPORT_CMDSERVER;
    else
        port = ntohs (s->s_port);

    read_fd = rcmd (&server_host,
    	            port,
    	            client_user,
	            (server_user ? server_user : client_user),
	            command,
	            0);
    if (read_fd < 0)
	error (1, errno, "cannot start server via rcmd");
    
    /* Split the socket into a reading and a writing half.  */
    if ((write_fd = dup (read_fd)) < 0)
        error (1, errno, "duplicating server connection");
#if 0
    /* This ought to be legal, since I've duped it, but shutting
       down the writing end of read_fd seems to terminate the
       whole connection.  */
    shutdown_fd (read_fd, 1);
    shutdown_fd (write_fd, 0);
#endif
    
    *tofd = write_fd;
    *fromfd = read_fd;
}


void
wnt_shutdown_server (int fd)
{
    SOCKET s;
    
    if ((s = _get_osfhandle (fd)) < 0)
        error (1, errno, "couldn't get handle of server connection");
    if (shutdown (s, 2) == SOCKET_ERROR)
        error (1, 0, "couldn't shutdown server connection");
    if (closesocket (s) == SOCKET_ERROR)
        error (1, 0, "couldn't close server connection");
}
