/*
 * server_if.c
 * Open connection to the CVS server under MacOS
 *
 * Michael Ladwig <mike@twinpeaks.prc.com> --- November 1995
 */

#include "mac_config.h"
#include "cvs.h"

#include <GUSI.h>
#include <sys/socket.h>

static int read_fd, write_fd;
    
void
macos_start_server (int *tofd, int *fromfd,
		  char *client_user,
		  char *server_user,
		  char *server_host,
		  char *server_cvsroot)
{
    char *cvs_server;
    char *command;
    char *portenv;
    struct servent *sptr;
    unsigned short port;

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
	/* This is the normal case.  Macs will generally lack a /etc/services
	   file (causing getservbyname to fail), and getenv is only something
	   that we provide via our own AppleEvents stuff, not a standard
	   Mac feature.  */
	port = 514;

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
    
    *tofd = write_fd;
    *fromfd = read_fd;
    free (command);
}


void
macos_shutdown_server (int to_server)
{
	if( close (read_fd) != 0 ) perror( "close on read_fd");
	if( close (write_fd) != 0 ) perror( "close on write_fd");
}
