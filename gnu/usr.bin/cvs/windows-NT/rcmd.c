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
    int optionValue = SO_SYNCHRONOUS_NONALERT;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
      fprintf (stderr, "cvs: unable to initialize winsock\n");
      exit (1);
    }

     if (setsockopt(INVALID_SOCKET, SOL_SOCKET,
                    SO_OPENTYPE, (char *)&optionValue, sizeof(optionValue))
         == SOCKET_ERROR)
     {
         fprintf (stderr, "cvs: unable to setup winsock\n");
         exit (1);
     }
}

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

    return -1;
}

#if 0
static int
bind_local_end (SOCKET s)
{
    struct sockaddr_in sai;
    int result;
    u_short port;

    sai.sin_family = AF_INET;
    sai.sin_addr.s_addr = htonl (INADDR_ANY);

    for (port = IPPORT_RESERVED - 2; port >= IPPORT_RESERVED/2; port--)
    {
    	int error;
        sai.sin_port = htons (port);
	result = bind (s, (struct sockaddr *) &sai, sizeof (sai));
	error = GetLastError ();
	if (result != SOCKET_ERROR || error != WSAEADDRINUSE)
	    break;
    }

    return result;
}
#endif

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
        int result, error;
	client_sai.sin_port = htons (client_port);

        if ((s = socket (PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
            return INVALID_SOCKET;

	result = bind (s, (struct sockaddr *) &client_sai,
	               sizeof (client_sai));
	error = GetLastError ();
	if (result == SOCKET_ERROR)
	{
	    closesocket (s);
	    if (error == WSAEADDRINUSE)
		continue;
	    else
	        return INVALID_SOCKET;
	}

	result = connect (s, (struct sockaddr *) server_sai,
	                  sizeof (*server_sai));
	error = GetLastError ();
	if (result == SOCKET_ERROR)
	{
	    closesocket (s);
	    if (error == WSAEADDRINUSE)
		continue;
	    else
	        return INVALID_SOCKET;
	}

	return s;
    }

    /* We couldn't find a free port.  */
    return INVALID_SOCKET;
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
	return -1;

    /* They sniff our butt, and send us a '\0' character if they
       like us.  */
    {
        char c;
	if (recv (fd, &c, 1, 0) == SOCKET_ERROR
	    || c != '\0')
	{
	    errno = EPERM;
	    return -1;
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
        return -1;

    sai.sin_port = htons (inport);

#if 0
    if ((s = socket (PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return -1;

    if (bind_local_end (s) < 0)
        return -1;

    if (connect (s, (struct sockaddr *) &sai, sizeof (sai))
        == SOCKET_ERROR)
        return -1;
#else
    if ((s = bind_and_connect (&sai)) == INVALID_SOCKET)
        return -1;
#endif

    if (rcmd_authenticate (s, locuser, remuser, cmd) < 0)
        return -1;

    return s;
}
