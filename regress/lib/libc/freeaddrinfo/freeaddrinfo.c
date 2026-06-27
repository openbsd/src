/* $OpenBSD: freeaddrinfo.c,v 1.1 2026/06/27 17:52:29 jca Exp $ */

/* Public Domain */

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>

#include <stddef.h>

int
main(void)
{
	/*
	 * The behavior of freeaddrinfo(NULL) isn't specified,
	 * but we want to gracefully handle it (ie avoid a crash).
	 */
	freeaddrinfo(NULL);

	return 0;
}
