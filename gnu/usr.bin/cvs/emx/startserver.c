/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <assert.h>
#include "cvs.h"
#include "rcmd.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "options.h"

static char *cvs_server;
static char *command;

extern int trace;

void
os2_start_server (int *tofd, int *fromfd,
		  char *client_user,
		  char *server_user,
		  char *server_host,
		  char *server_cvsroot)
{
    int fd, port;
    char *portenv;
    struct servent *sptr;
	const char *rcmd_host = (const char *) server_host;

    if (! (cvs_server = getenv ("CVS_SERVER")))
	cvs_server = "cvs";
	command = xmalloc (strlen (cvs_server)
			 + strlen (server_cvsroot)
			 + 50);
    sprintf (command, "%s server", cvs_server);

    portenv = getenv ("CVS_RCMD_PORT");
    if (portenv)
	port = atoi (portenv);
    else if ((sptr = getservbyname ("shell", "tcp")) != NULL)
	port = sptr->s_port;
    else
	port = 514; /* shell/tcp */

    if (trace)
    {
	fprintf (stderr, "os2_start_server(): connecting to %s:%d\n",
	    server_host, port);
	fprintf (stderr, "local_user = %s, remote_user = %s, CVSROOT = %s\n",
	    client_user, (server_user ? server_user : client_user),
	    server_cvsroot);
    }

    fd = rcmd (&rcmd_host, port,
	       client_user,
	       (server_user ? server_user : client_user),
	       command, 0);

    if (fd < 0)
	error (1, errno, "cannot start server via rcmd()");

    *tofd = fd;
    *fromfd = fd;
    free (command);
}

void
os2_shutdown_server (int fd)
{
    /* FIXME: shutdown on files seems to have no bad effects */
    if (shutdown (fd, 2) < 0 && errno != ENOTSOCK)
        error (1, 0, "couldn't shutdown server connection");
    if (close (fd) < 0)
        error (1, 0, "couldn't close server connection");
}

