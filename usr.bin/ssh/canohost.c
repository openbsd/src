/* $OpenBSD: canohost.c,v 1.74 2019/06/28 13:35:04 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for returning the canonical host name of the remote site.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "xmalloc.h"
#include "packet.h"
#include "log.h"
#include "canohost.h"
#include "misc.h"

/*
 * Returns the local/remote IP-address/hostname of socket as a string.
 * The returned string must be freed.
 */
static char *
get_socket_address(int sock, int remote, int flags)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	char ntop[NI_MAXHOST];
	int r;

	/* Get IP address of client. */
	addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));

	if (remote) {
		if (getpeername(sock, (struct sockaddr *)&addr, &addrlen) != 0)
			return NULL;
	} else {
		if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) != 0)
			return NULL;
	}

	switch (addr.ss_family) {
	case AF_INET:
	case AF_INET6:
		/* Get the address in ascii. */
		if ((r = getnameinfo((struct sockaddr *)&addr, addrlen, ntop,
		    sizeof(ntop), NULL, 0, flags)) != 0) {
			error("%s: getnameinfo %d failed: %s", __func__,
			    flags, ssh_gai_strerror(r));
			return NULL;
		}
		return xstrdup(ntop);
	case AF_UNIX:
		/* Get the Unix domain socket path. */
		return xstrdup(((struct sockaddr_un *)&addr)->sun_path);
	default:
		/* We can't look up remote Unix domain sockets. */
		return NULL;
	}
}

char *
get_peer_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 1, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 0, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_name(int fd)
{
	char *host, myname[NI_MAXHOST];

	/* Assume we were passed a socket */
	if ((host = get_socket_address(fd, 0, NI_NAMEREQD)) != NULL)
		return host;

	/* Handle the case where we were passed a pipe */
	if (gethostname(myname, sizeof(myname)) == -1) {
		verbose("%s: gethostname: %s", __func__, strerror(errno));
		host = xstrdup("UNKNOWN");
	} else {
		host = xstrdup(myname);
	}

	return host;
}

/* Returns the local/remote port for the socket. */

static int
get_sock_port(int sock, int local)
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	char strport[NI_MAXSERV];
	int r;

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (local) {
		if (getsockname(sock, (struct sockaddr *)&from, &fromlen) == -1) {
			error("getsockname failed: %.100s", strerror(errno));
			return 0;
		}
	} else {
		if (getpeername(sock, (struct sockaddr *)&from, &fromlen) == -1) {
			debug("getpeername failed: %.100s", strerror(errno));
			return -1;
		}
	}

	/* Non-inet sockets don't have a port number. */
	if (from.ss_family != AF_INET && from.ss_family != AF_INET6)
		return 0;

	/* Return port number. */
	if ((r = getnameinfo((struct sockaddr *)&from, fromlen, NULL, 0,
	    strport, sizeof(strport), NI_NUMERICSERV)) != 0)
		fatal("%s: getnameinfo NI_NUMERICSERV failed: %s", __func__,
		    ssh_gai_strerror(r));
	return atoi(strport);
}

int
get_peer_port(int sock)
{
	return get_sock_port(sock, 0);
}

int
get_local_port(int sock)
{
	return get_sock_port(sock, 1);
}
