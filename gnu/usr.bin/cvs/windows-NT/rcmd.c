/* rcmd.c --- execute a command on a remote host from Windows NT
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <winsock.h>
#include <stdio.h>
#include <assert.h>

#include "cvs.h"
#include "rcmd.h"

void
init_winsock ()
{
    WSADATA data;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
      fprintf (stderr, "cvs: unable to initialize winsock\n");
      exit (1);
    }
}

/* The rest of this file contains the rcmd() code, which is used
   only by START_SERVER.  The idea for a long-term direction is
   that this code can be made portable (by using SOCK_ERRNO and
   so on), and then moved to client.c or someplace it can be
   shared with the VMS port and any other ports which may want it.  */


static int
resolve_address (const char **ahost, struct sockaddr_in *sai)
{
    {
	unsigned long addr = inet_addr (*ahost);

	if (addr != (unsigned long) -1)
	{
	    sai->sin_family = AF_INET;
	    sai->sin_addr.s_addr = addr;
	    return 0;
	}
    }

    {
        struct hostent *e = gethostbyname (*ahost);

	if (e)
	{
	    assert (e->h_addrtype == AF_INET);
	    assert (e->h_addr);
	    *ahost = e->h_name;
	    sai->sin_family = AF_INET;
	    memcpy (&sai->sin_addr, e->h_addr, sizeof (sai->sin_addr));
	    return 0;
	}
    }

    error (1, 0, "no such host %s", *ahost);
}

static SOCKET
bind_and_connect (struct sockaddr_in *server_sai)
{
    SOCKET s;
    struct sockaddr_in client_sai;
    u_short client_port;

    client_sai.sin_family = AF_INET;
    client_sai.sin_addr.s_addr = htonl (INADDR_ANY);

    for (client_port = IPPORT_RESERVED - 1;
         client_port >= IPPORT_RESERVED/2;
         client_port--)
    {
	int result, errcode;
	client_sai.sin_port = htons (client_port);

        if ((s = socket (PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	    error (1, 0, "cannot create socket: %s",
		   SOCK_STRERROR (SOCK_ERRNO));

	result = bind (s, (struct sockaddr *) &client_sai,
	               sizeof (client_sai));
	errcode = SOCK_ERRNO;
	if (result == SOCKET_ERROR)
	{
	    closesocket (s);
	    if (errcode == WSAEADDRINUSE)
		continue;
	    else
		error (1, 0, "cannot bind to socket: %s",
		       SOCK_STRERROR (errcode));
	}

	result = connect (s, (struct sockaddr *) server_sai,
	                  sizeof (*server_sai));
	errcode = SOCK_ERRNO;
	if (result == SOCKET_ERROR)
	{
	    closesocket (s);
	    if (errcode == WSAEADDRINUSE)
		continue;
	    else
		error (1, 0, "cannot connect to socket: %s",
		       SOCK_STRERROR (errcode));
	}

	return s;
    }

    error (1, 0, "cannot find free port");
}

static int
rcmd_authenticate (int fd, char *locuser, char *remuser, char *command)
{
    /* Send them a bunch of information, each terminated by '\0':
       - secondary stream port number (we don't use this)
       - username on local machine
       - username on server machine
       - command
       Now, the Ultrix man page says you transmit the username on the
       server first, but that doesn't seem to work.  Transmitting the
       client username first does.  Go figure.  The Linux man pages
       get it right --- hee hee.  */
    if ((send (fd, "0\0", 2, 0) == SOCKET_ERROR)
	|| (send (fd, locuser, strlen (locuser) + 1, 0) == SOCKET_ERROR)
	|| (send (fd, remuser, strlen (remuser) + 1, 0) == SOCKET_ERROR)
	|| (send (fd, command, strlen (command) + 1, 0) == SOCKET_ERROR))
	error (1, 0, "cannot send authentication info to rshd: %s",
	       SOCK_STRERROR (SOCK_ERRNO));

    /* They sniff our butt, and send us a '\0' character if they
       like us.  */
    {
        char c;
	if (recv (fd, &c, 1, 0) == SOCKET_ERROR)
	{
	    error (1, 0, "cannot receive authentication info from rshd: %s",
		   SOCK_STRERROR (SOCK_ERRNO));
	}
	if (c != '\0')
	{
	    error (1, 0, "Permission denied by rshd");
	}
    }

    return 0;
}

int
rcmd (const char **ahost,
      unsigned short inport,
      char *locuser,
      char *remuser,
      char *cmd,
      int *fd2p)
{
    struct sockaddr_in sai;
    SOCKET s;

    assert (fd2p == 0);

    if (resolve_address (ahost, &sai) < 0)
        error (1, 0, "internal error: resolve_address < 0");

    sai.sin_port = htons (inport);

    if ((s = bind_and_connect (&sai)) == INVALID_SOCKET)
	error (1, 0, "internal error: bind_and_connect < 0");

    if (rcmd_authenticate (s, locuser, remuser, cmd) < 0)
	error (1, 0, "internal error: rcmd_authenticate < 0");

    return s;
}
