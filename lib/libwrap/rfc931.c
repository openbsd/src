/*	$OpenBSD: rfc931.c,v 1.12 2009/10/27 23:59:30 deraadt Exp $	*/

/* rfc1413 does an attempt at an ident query to a client. Originally written
 * by Wietse Venema, rewritten by Bob Beck <beck@openbsd.org> to avoid 
 * potential longjmp problems and get rid of stdio usage on sockets. 
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

/* Local stuff for tcpd. */

#include "tcpd.h"

#define	IDENT_PORT	113

static volatile sig_atomic_t ident_timeout;

int rfc931_timeout = RFC931_TIMEOUT; /* global, legacy from tcpwrapper stuff */

/*ARGSUSED*/
static void
timeout(s)
	int s;
{     
	ident_timeout = 1;
}

/*
 * The old rfc931 from original libwrap for compatibility. Now it calls
 * rfc1413 with the default global parameters, but puts in the string
 * "unknown" (from global unknown) on failure just like the original.
 */

void
rfc931(rmt_sin, our_sin, dest)
	struct sockaddr *rmt_sin;
	struct sockaddr *our_sin;
	char   *dest; 
{
	if (rfc1413(rmt_sin, our_sin, dest, STRING_LENGTH, rfc931_timeout) ==
	    -1)
		strlcpy(dest, unknown, STRING_LENGTH);
}


/*
 * rfc1413, an rfc1413 client request for user name given a socket
 * structure, with a timeout in seconds on the whole operation.  On
 * success returns 0 and saves dsize-1 characters of the username
 * provided by remote ident daemon into "dest", stripping off any
 * terminating CRLF, and terminating with a nul byte. Returns -1 on
 * failure (timeout, remote daemon didn't answer, etc). 
 */

int
rfc1413(rmt_sin, our_sin, dest, dsize, ident_timeout_time)
	struct sockaddr *rmt_sin;
	struct sockaddr *our_sin;
	char   *dest;
	size_t dsize;
	int ident_timeout_time;
{
	in_port_t rmt_port, our_port;
	int s, i, gotit, salen;
	char *cp;
	in_port_t *rmt_portp;
	in_port_t *our_portp;
	fd_set *readfds = NULL;
	fd_set *writefds = NULL;
	struct sockaddr_storage rmt_query_sin;
	struct sockaddr_storage our_query_sin;
	struct sigaction new_sa, old_sa;
	char user[256];	 
	char tbuf[1024];	 
	size_t rsize, wsize;
	
	gotit = 0;
	s = -1;
	
	/* address family must be the same */
	if (rmt_sin->sa_family != our_sin->sa_family)
		goto out1;
	switch (rmt_sin->sa_family) {
	case AF_INET:
		salen = sizeof(struct sockaddr_in);
		rmt_portp = &(((struct sockaddr_in *)rmt_sin)->sin_port);
		break;
#ifdef INET6
	case AF_INET6:
		salen = sizeof(struct sockaddr_in6);
		rmt_portp = &(((struct sockaddr_in6 *)rmt_sin)->sin6_port);
		break;
#endif
	default:
		goto out1;
	}
	switch (our_sin->sa_family) {
	case AF_INET:
		our_portp = &(((struct sockaddr_in *)our_sin)->sin_port);
		break;
#ifdef INET6
	case AF_INET6:
		our_portp = &(((struct sockaddr_in6 *)our_sin)->sin6_port);
		break;
#endif
	default:
		goto out1;
	}

	if ((s = socket(rmt_sin->sa_family, SOCK_STREAM, 0)) == -1)
		goto out1;

	/*
	 * Set up a timer so we won't get stuck while waiting for the server.
	 * timer sets volatile sig_atomic_t ident_timeout when it fires. 
	 * this has to be checked after system calls in case we timed out.
	 */
	
	ident_timeout = 0;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = timeout;
	new_sa.sa_flags = SA_RESTART;
	if (sigemptyset(&new_sa.sa_mask) == -1)
		goto out1;
	if (sigaction(SIGALRM, &new_sa, &old_sa) == -1)
		goto out1;
	alarm(ident_timeout_time);
	
	/*
	 * Bind the local and remote ends of the query socket to the same
	 * IP addresses as the connection under investigation. We go
	 * through all this trouble because the local or remote system
	 * might have more than one network address. The IDENT etc.
	 * client sends only port numbers; the server takes the IP
	 * addresses from the query socket.
	 */
	
	memcpy(&our_query_sin, our_sin, salen);
	switch (our_query_sin.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&our_query_sin)->sin_port = htons(0);
		break;
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)&our_query_sin)->sin6_port = htons(0);
		break;
#endif
	}
	memcpy(&rmt_query_sin, rmt_sin, salen);
	switch (rmt_query_sin.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&rmt_query_sin)->sin_port =
		    htons(IDENT_PORT);
		break;
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)&rmt_query_sin)->sin6_port =
		    htons(IDENT_PORT);
		break;
#endif
	}
	
	if (bind(s, (struct sockaddr *) & our_query_sin, salen) == -1)
		goto out;
	if ((connect(s, (struct sockaddr *) & rmt_query_sin, salen) == -1) ||
	    ident_timeout)
		goto out;

	/* We are connected,  build an ident query and send it. */ 
	
	rsize = howmany(s+1, NFDBITS);
	readfds = calloc(rsize, sizeof(fd_mask));
	if (readfds == NULL) 
		goto out;

	wsize = howmany(s+1, NFDBITS);
	writefds = calloc(wsize, sizeof(fd_mask));
	if (writefds == NULL) 
		goto out;
	snprintf(tbuf, sizeof(tbuf), "%u,%u\r\n", ntohs(*rmt_portp),
	    ntohs(*our_portp));
	i = 0;
	while (i < strlen(tbuf)) { 
		int j;

		memset(writefds, 0, wsize * sizeof(fd_mask));
		FD_SET(s, writefds);
		do { 
			j = select(s + 1, NULL, writefds, NULL, NULL);
			if (ident_timeout)			    
				goto out;
		} while ( j == -1 && (errno == EAGAIN || errno == EINTR ));
		if (j == -1)
			goto out;
		if (FD_ISSET(s, writefds)) {
			j = write(s, tbuf + i, strlen(tbuf + i));
			if ((j == -1 && errno != EAGAIN && errno != EINTR) ||
			    ident_timeout) 
				goto out;
			if  (j != -1) 
				i += j;
		} else 
			goto out;
	} 

	
	/* Read the answer back. */
	i = 0;
	tbuf[0] = '\0';
	while ((cp = strchr(tbuf, '\n')) == NULL && i < sizeof(tbuf) - 1) {
		int j;

		memset(readfds, 0, rsize * sizeof(fd_mask));
		FD_SET(s, readfds);
		do { 
			j = select(s + 1, readfds, NULL, NULL, NULL);
			if (ident_timeout)
				goto out;
		} while ( j == -1 && (errno == EAGAIN || errno == EINTR ));
		if (j == -1)
			goto out;
		if (FD_ISSET(s, readfds)) {
			j = read(s, tbuf + i, sizeof(tbuf) - 1 - i);
			if ((j == -1 && errno != EAGAIN && errno != EINTR) ||
			    j == 0 || ident_timeout) 
				goto out;
			if  (j != -1) 
				i += j;
			tbuf[i] = '\0';
		} else
			goto out;
	}
	
	if ((sscanf(tbuf,"%hu , %hu : USERID :%*[^:]:%255s", &rmt_port,
	    &our_port, user) == 3) &&
	    (ntohs(*rmt_portp) == rmt_port) &&
	    (ntohs(*our_portp) == our_port)) {
		if ((cp = strchr(user, '\r')) != NULL)
			*cp = '\0';
		gotit = 1;
	}

out:
	alarm(0);
	sigaction(SIGALRM, &old_sa, NULL);
	if (readfds != NULL)
		free(readfds);
	if (writefds != NULL)
		free(writefds);
out1:
	if (s != -1) 
		close(s);
	if (gotit) {
		strlcpy(dest, user, dsize);
		return(0);
	}
	return(-1);
}
