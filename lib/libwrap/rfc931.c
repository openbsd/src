/*	$OpenBSD: rfc931.c,v 1.2 2000/10/14 00:56:15 itojun Exp $	*/

 /*
  * rfc931() speaks a common subset of the RFC 931, AUTH, TAP, IDENT and RFC
  * 1413 protocols. It queries an RFC 931 etc. compatible daemon on a remote
  * host to look up the owner of a connection. The information should not be
  * used for authentication purposes. This routine intercepts alarm signals.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) rfc931.c 1.10 95/01/02 16:11:34";
#else
static char rcsid[] = "$OpenBSD: rfc931.c,v 1.2 2000/10/14 00:56:15 itojun Exp $";
#endif
#endif

/* System libraries. */

#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/* Local stuff. */

#include "tcpd.h"

#define	RFC931_PORT	113		/* Semi-well-known port */
#define	ANY_PORT	0		/* Any old port will do */

int     rfc931_timeout = RFC931_TIMEOUT;/* Global so it can be changed */

static jmp_buf timebuf;

/* fsocket - open stdio stream on top of socket */

static FILE *fsocket(domain, type, protocol)
int     domain;
int     type;
int     protocol;
{
    int     s;
    FILE   *fp;

    if ((s = socket(domain, type, protocol)) < 0) {
	tcpd_warn("socket: %m");
	return (0);
    } else {
	if ((fp = fdopen(s, "r+")) == 0) {
	    tcpd_warn("fdopen: %m");
	    close(s);
	}
	return (fp);
    }
}

/* timeout - handle timeouts */

static void timeout(sig)
int     sig;
{
    longjmp(timebuf, sig);
}

/* rfc931 - return remote user name, given socket structures */

void    rfc931(rmt_sin, our_sin, dest)
struct sockaddr *rmt_sin;
struct sockaddr *our_sin;
char   *dest;
{
    unsigned rmt_port;
    unsigned our_port;
    struct sockaddr_storage rmt_query_sin;
    struct sockaddr_storage our_query_sin;
    char    user[256];			/* XXX */
    char    buffer[512];		/* XXX */
    char   *cp;
    char   *result = unknown;
    FILE   *fp;
    int salen;
    u_short *rmt_portp;
    u_short *our_portp;

    /* address family must be the same */
    if (rmt_sin->sa_family != our_sin->sa_family) {
	STRN_CPY(dest, result, STRING_LENGTH);
	return;
    }
    switch (rmt_sin->sa_family) {
    case AF_INET:
	salen = sizeof(struct sockaddr_in);
	rmt_portp = &((struct sockaddr_in *)&rmt_sin)->sin_port;
	break;
#ifdef INET6
    case AF_INET6:
	salen = sizeof(struct sockaddr_in6);
	rmt_portp = &((struct sockaddr_in6 *)&rmt_sin)->sin6_port;
	break;
#endif
    default:
	STRN_CPY(dest, result, STRING_LENGTH);
	return;
    }
    switch (our_sin->sa_family) {
    case AF_INET:
	our_portp = &((struct sockaddr_in *)&our_sin)->sin_port;
	break;
#ifdef INET6
    case AF_INET6:
	our_portp = &((struct sockaddr_in6 *)&our_sin)->sin6_port;
	break;
#endif
    default:
	STRN_CPY(dest, result, STRING_LENGTH);
	return;
    }

#ifdef __GNUC__
    (void)&result;	/* Avoid longjmp clobbering */
    (void)&fp;		/* XXX gcc */
#endif

    /*
     * Use one unbuffered stdio stream for writing to and for reading from
     * the RFC931 etc. server. This is done because of a bug in the SunOS
     * 4.1.x stdio library. The bug may live in other stdio implementations,
     * too. When we use a single, buffered, bidirectional stdio stream ("r+"
     * or "w+" mode) we read our own output. Such behaviour would make sense
     * with resources that support random-access operations, but not with
     * sockets.
     */

    if ((fp = fsocket(rmt_sin->sa_family, SOCK_STREAM, 0)) != 0) {
	setbuf(fp, (char *) 0);

	/*
	 * Set up a timer so we won't get stuck while waiting for the server.
	 */

	if (setjmp(timebuf) == 0) {
	    signal(SIGALRM, timeout);
	    alarm(rfc931_timeout);

	    /*
	     * Bind the local and remote ends of the query socket to the same
	     * IP addresses as the connection under investigation. We go
	     * through all this trouble because the local or remote system
	     * might have more than one network address. The RFC931 etc.
	     * client sends only port numbers; the server takes the IP
	     * addresses from the query socket.
	     */

	    memcpy(&our_query_sin, our_sin, salen);
	    switch (our_query_sin.ss_family) {
	    case AF_INET:
		((struct sockaddr_in *)&our_query_sin)->sin_port =
			htons(ANY_PORT);
		break;
#ifdef INET6
	    case AF_INET6:
		((struct sockaddr_in6 *)&our_query_sin)->sin6_port =
			htons(ANY_PORT);
		break;
#endif
	    }
	    memcpy(&rmt_query_sin, rmt_sin, salen);
	    switch (rmt_query_sin.ss_family) {
	    case AF_INET:
		((struct sockaddr_in *)&rmt_query_sin)->sin_port =
			htons(RFC931_PORT);
		break;
#ifdef INET6
	    case AF_INET6:
		((struct sockaddr_in6 *)&rmt_query_sin)->sin6_port = 
			htons(RFC931_PORT);
		break;
#endif
	    }

	    if (bind(fileno(fp), (struct sockaddr *) & our_query_sin,
		     salen) >= 0 &&
		connect(fileno(fp), (struct sockaddr *) & rmt_query_sin,
			salen) >= 0) {

		/*
		 * Send query to server. Neglect the risk that a 13-byte
		 * write would have to be fragmented by the local system and
		 * cause trouble with buggy System V stdio libraries.
		 */

		fprintf(fp, "%u,%u\r\n",
			ntohs(*rmt_portp),
			ntohs(*our_portp));
		fflush(fp);

		/*
		 * Read response from server. Use fgets()/sscanf() so we can
		 * work around System V stdio libraries that incorrectly
		 * assume EOF when a read from a socket returns less than
		 * requested.
		 */

		if (fgets(buffer, sizeof(buffer), fp) != 0
		    && ferror(fp) == 0 && feof(fp) == 0
		    && sscanf(buffer, "%u , %u : USERID :%*[^:]:%255s",
			      &rmt_port, &our_port, user) == 3
		    && ntohs(*rmt_portp) == rmt_port
		    && ntohs(*our_portp) == our_port) {

		    /*
		     * Strip trailing carriage return. It is part of the
		     * protocol, not part of the data.
		     */

		    cp = strchr(user, '\r');
		    if (cp)
			*cp = 0;
		    result = user;
		}
	    }
	    alarm(0);
	}
	fclose(fp);
    }
    STRN_CPY(dest, result, STRING_LENGTH);
}
