/*	$OpenBSD: send.c,v 1.4 2002/09/06 22:00:03 deraadt Exp $	*/

/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char Version[] = "@(#)send.c	e07@nikhef.nl (Eric Wassenaar) 961013";
#endif

#if defined(apollo) && defined(lint)
#define __attribute(x)
#endif

#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>

#include <sys/types.h>		/* not always automatically included */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>

#undef NOERROR			/* in <sys/streams.h> on solaris 2.x */
#include <arpa/nameser.h>
#include <resolv.h>

#include "port.h"		/* various portability definitions */
#include "conf.h"		/* various configuration definitions */

#define input			/* read-only input parameter */
#define output			/* modified output parameter */

#define bitset(a,b)	(((a) & (b)) != 0)
#define setalarm(n)	(void) alarm((unsigned int)(n))

extern res_state_t _res;	/* defined in res_init.c */

char *dbprefix = DBPREFIX;	/* prefix for debug messages to stdout */

static int timeout;		/* connection read timeout */
static struct sockaddr_in from;	/* address of inbound packet */
static struct sockaddr *from_sa = (struct sockaddr *)&from;

char *inet_ntoa		PROTO((struct in_addr));
unsigned int alarm	PROTO((unsigned int));

#ifdef HOST_RES_SEND
int res_send		PROTO((CONST qbuf_t *, int, qbuf_t *, int));
void _res_close		PROTO((void));
static int check_from	PROTO((void));
static int send_stream	PROTO((struct sockaddr_in *, qbuf_t *, int, qbuf_t *, int));
static int send_dgram	PROTO((struct sockaddr_in *, qbuf_t *, int, qbuf_t *, int));
#endif /*HOST_RES_SEND*/
static sigtype_t timer	PROTO((int));
int _res_connect	PROTO((int, struct sockaddr_in *, int));
int _res_write		PROTO((int, struct sockaddr_in *, char *, char *, int));
int _res_read		PROTO((int, struct sockaddr_in *, char *, char *, int));
static int recv_sock	PROTO((int, char *, int));
void _res_perror	PROTO((struct sockaddr_in *, char *, char *));

#ifdef HOST_RES_SEND

/*
** RES_SEND -- Send nameserver query and retrieve answer
** -----------------------------------------------------
**
**	Returns:
**		Length of (untruncated) nameserver answer, if obtained.
**		-1 if an error occurred (errno set appropriately).
**
**	This is a simplified version of the BIND 4.8.3 res_send().
**	- Always use connected datagrams to get proper error messages.
**	- Do not only return ETIMEDOUT or ECONNREFUSED in datagram mode,
**	  but also host or network unreachable status if appropriate.
**	- Never leave a connection open after we got an answer.
**	- No special ECONNRESET handling when using virtual circuits.
**
**	Note that this private version of res_send() is not only called
**	directly by 'host' but also indirectly by gethostbyname() or by
**	gethostbyaddr() via their resolver interface routines.
*/

int
res_send(query, querylen, answer, anslen)
input CONST qbuf_t *query;		/* location of formatted query buffer */
input int querylen;			/* length of query buffer */
output qbuf_t *answer;			/* location of buffer to store answer */
input int anslen;			/* maximum size of answer buffer */
{
	HEADER *bp = (HEADER *)answer;
	struct sockaddr_in *addr;	/* the server address to connect to */
	int v_circuit;			/* virtual circuit or datagram switch */
	int servfail[MAXNS];		/* saved failure codes per nameserver */
	register int try, ns;
	register int n;

	/* make sure resolver has been initialized */
	if (!bitset(RES_INIT, _res.options) && res_init() == -1)
		return(-1);

	if (bitset(RES_DEBUG, _res.options))
	{
		printf("%sres_send()\n", dbprefix);
		pr_query(query, querylen, stdout);
	}

	/* use virtual circuit if requested or if necessary */
	v_circuit = bitset(RES_USEVC, _res.options) || (querylen > PACKETSZ);

	/* reset server failure codes */
	for (ns = 0; ns < MAXNS; ns++)
		servfail[ns] = 0;

/*
 * Do _res.retry attempts for each of the _res.nscount addresses.
 * Upon failure, the current server will be marked bad if we got
 * an error condition which makes it unlikely that we will succeed
 * the next time we try this server.
 * Internal operating system failures, such as temporary lack of
 * resources, do not fall in that category.
 */
	for (try = 0; try < _res.retry; try++)
	{
	    for (ns = 0; ns < _res.nscount; ns++)
	    {
		/* skip retry if server failed permanently */
		if (servfail[ns])
			continue;

		/* fetch server address */
		addr = &nslist(ns);
retry:
		if (bitset(RES_DEBUG, _res.options))
			printf("%sQuerying server (# %d) %s address = %s\n", dbprefix,
			    ns+1, v_circuit ? "tcp" : "udp", inet_ntoa(addr->sin_addr));

		if (v_circuit)
		{
			/* at most one attempt per server */
			try = _res.retry;

			/* connect via virtual circuit */
			n = send_stream(addr, query, querylen, answer, anslen);
		}
		else
		{
			/* set datagram read timeout for recv_sock() */
			timeout = (_res.retrans << try);
			if (try > 0)
				timeout /= _res.nscount;
			if (timeout <= 0)
				timeout = 1;

			/* connect via datagram */
			n = send_dgram(addr, query, querylen, answer, anslen);

			/* check truncation; use v_circuit with same server */
			if ((n > 0) && bp->tc)
			{
				if (bitset(RES_DEBUG, _res.options))
					printf("%struncated answer, %d bytes\n", dbprefix, n);

				if (!bitset(RES_IGNTC, _res.options))
				{
					v_circuit = 1;
					goto retry;
				}
			}
		}

		if (n <= 0)
		{
			switch (errno)
			{
			    case ECONNREFUSED:
			    case ENETDOWN:
			    case ENETUNREACH:
			    case EHOSTDOWN:
			    case EHOSTUNREACH:
				servfail[ns] = errno;
				break;
			}

			/* try next server */
			continue;
		}

		if (bitset(RES_DEBUG, _res.options))
		{
			printf("%sgot answer, %d bytes:\n", dbprefix, n);
			pr_query(answer, (n > anslen) ? anslen : n, stdout);
		}

		/* we have an answer; clear possible error condition */
		errno = 0;
		return(n);
	    }
	}

	/* no answer obtained; return error condition */
	return(-1);
}

/*
** _RES_CLOSE -- Close an open stream or dgram connection
** ------------------------------------------------------
**
**	Returns:
**		None.
*/

static int srvsock = -1;	/* socket descriptor */

void
_res_close()
{
	int save_errno = errno;		/* preserve state */

	/* close the connection if open */
	if (srvsock >= 0)
	{
		(void) close(srvsock);
		srvsock = -1;
	}

	/* restore state */
	errno = save_errno;
}

/*
** CHECK_FROM -- Make sure the response comes from a known server
** --------------------------------------------------------------
**
**	Returns:
**		Nonzero if the source address is known.
**		Zero otherwise.
*/

static int
check_from()
{
	struct sockaddr_in *addr;
	register int ns;

	for (ns = 0; ns < _res.nscount; ns++)
	{
		/* fetch server address */
		addr = &nslist(ns);

		if (from.sin_family != addr->sin_family)
			continue;

		if (from.sin_port != addr->sin_port)
			continue;

		/* this allows a reply from any responding server */
		if (addr->sin_addr.s_addr == INADDR_ANY)
			return(1);

		if (from.sin_addr.s_addr == addr->sin_addr.s_addr)
			return(1);
	}

	/* matches none of the known addresses */
	return(0);
}

/*
** SEND_STREAM -- Query nameserver via virtual circuit
** ---------------------------------------------------
**
**	Returns:
**		Length of (untruncated) nameserver answer, if obtained.
**		-1 if an error occurred.
**
**	A new socket is allocated for each call, and it is never
**	left open. Checking the packet id is rather redundant.
**
**	Note that connect() is the call that is allowed to fail
**	under normal circumstances. All other failures generate
**	an unconditional error message.
**	Note that truncation is handled within _res_read().
*/

static int
send_stream(addr, query, querylen, answer, anslen)
input struct sockaddr_in *addr;		/* the server address to connect to */
input qbuf_t *query;			/* location of formatted query buffer */
input int querylen;			/* length of query buffer */
output qbuf_t *answer;			/* location of buffer to store answer */
input int anslen;			/* maximum size of answer buffer */
{
	char *host = NULL;		/* name of server is unknown */
	HEADER *qp = (HEADER *)query;
	HEADER *bp = (HEADER *)answer;
	register int n;

/*
 * Setup a virtual circuit connection.
 */
	srvsock = socket(AF_INET, SOCK_STREAM, 0);
	if (srvsock < 0)
	{
		_res_perror(addr, host, "socket");
		return(-1);
	}

	if (_res_connect(srvsock, addr, sizeof(*addr)) < 0)
	{
		if (bitset(RES_DEBUG, _res.options))
			_res_perror(addr, host, "connect");
		_res_close();
		return(-1);
	}

	if (bitset(RES_DEBUG, _res.options))
		printf("%sconnected to %s\n", dbprefix, inet_ntoa(addr->sin_addr));

/*
 * Send the query buffer.
 */
	if (_res_write(srvsock, addr, host, (char *)query, querylen) < 0)
	{
		_res_close();
		return(-1);
	}

/*
 * Read the answer buffer.
 */
wait:
	n = _res_read(srvsock, addr, host, (char *)answer, anslen);
	if (n <= 0)
	{
		_res_close();
		return(-1);
	}

/*
 * Make sure it is the proper response by checking the packet id.
 */
	if (qp->id != bp->id)
	{
		if (bitset(RES_DEBUG, _res.options))
		{
			printf("%sunexpected answer:\n", dbprefix);
			pr_query(answer, (n > anslen) ? anslen : n, stdout);
		}
		goto wait;
	}

/*
 * Never leave the socket open.
 */
	_res_close();
	return(n);
}

/*
** SEND_DGRAM -- Query nameserver via datagram
** -------------------------------------------
**
**	Returns:
**		Length of nameserver answer, if obtained.
**		-1 if an error occurred.
**
**	Inputs:
**		The global variable ``timeout'' should have been
**		set with the desired timeout value in seconds.
**
**	Sending to a nameserver datagram port with no nameserver running
**	will cause an ICMP port unreachable message to be returned. If the
**	socket is connected, we get an ECONNREFUSED error on the next socket
**	operation, and select returns if the error message is received.
**	Also, we get ENETUNREACH or EHOSTUNREACH errors if appropriate.
**	We thus get a proper error status before timing out.
**	This method usually works only if BSD >= 43.
**
**	Note that send() and recvfrom() are now the calls that are allowed
**	to fail under normal circumstances. All other failures generate
**	an unconditional error message.
*/

static int
send_dgram(addr, query, querylen, answer, anslen)
input struct sockaddr_in *addr;		/* the server address to connect to */
input qbuf_t *query;			/* location of formatted query buffer */
input int querylen;			/* length of query buffer */
output qbuf_t *answer;			/* location of buffer to store answer */
input int anslen;			/* maximum size of answer buffer */
{
	char *host = NULL;		/* name of server is unknown */
	HEADER *qp = (HEADER *)query;
	HEADER *bp = (HEADER *)answer;
	register int n;

/*
 * Setup a connected datagram socket.
 */
	srvsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (srvsock < 0)
	{
		_res_perror(addr, host, "socket");
		return(-1);
	}

	if (connect(srvsock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
	{
		_res_perror(addr, host, "connect");
		_res_close();
		return(-1);
	}

/*
 * Send the query buffer.
 */
	if (send(srvsock, (char *)query, querylen, 0) != querylen)
	{
		if (bitset(RES_DEBUG, _res.options))
			_res_perror(addr, host, "send");
		_res_close();
		return(-1);
	}

/*
 * Wait for the arrival of a reply, timeout, or error message.
 */
wait:
	n = recv_sock(srvsock, (char *)answer, anslen);
	if (n <= 0)
	{
		if (bitset(RES_DEBUG, _res.options))
			_res_perror(addr, host, "recvfrom");
		_res_close();
		return(-1);
	}

/*
 * Make sure it is the proper response by checking the packet id.
 */
	if (qp->id != bp->id)
	{
		if (bitset(RES_DEBUG, _res.options))
		{
			printf("%sold answer:\n", dbprefix);
			pr_query(answer, (n > anslen) ? anslen : n, stdout);
		}
		goto wait;
	}

/*
 * Make sure it comes from a known server.
 */
	if (!check_from())
	{
		if (bitset(RES_DEBUG, _res.options))
		{
			printf("%sunknown server %s:\n", dbprefix, inet_ntoa(from.sin_addr));
			pr_query(answer, (n > anslen) ? anslen : n, stdout);
		}
		goto wait;
	}

/*
 * Never leave the socket open.
 */
	_res_close();
	return(n);
}

#endif /*HOST_RES_SEND*/

/*
** _RES_CONNECT -- Connect to a stream (virtual circuit) socket
** ------------------------------------------------------------
**
**	Returns:
**		0 if successfully connected.
**		-1 in case of failure or timeout.
**
**	Note that we use _res.retrans to override the default
**	connect timeout value.
*/

static jmp_buf timer_buf;

static sigtype_t
/*ARGSUSED*/
timer(sig)
int sig;
{
	longjmp(timer_buf, 1);
	/*NOTREACHED*/
}


int
_res_connect(sock, addr, addrlen)
input int sock;
input struct sockaddr_in *addr;		/* the server address to connect to */
input int addrlen;
{
	if (setjmp(timer_buf) != 0)
	{
		errno = ETIMEDOUT;
		setalarm(0);
		return(-1);
	}

	(void) signal(SIGALRM, timer);
	setalarm(_res.retrans);

	if (connect(sock, (struct sockaddr *)addr, addrlen) < 0)
	{
		if (errno == EINTR)
			errno = ETIMEDOUT;
		setalarm(0);
		return(-1);
	}

	setalarm(0);
	return(0);
}

/*
** _RES_WRITE -- Write the query buffer via a stream socket
** --------------------------------------------------------
**
**	Returns:
**		Length of buffer if successfully transmitted.
**		-1 in case of failure (error message is issued).
**
**	The query is sent in two steps: first a single word with
**	the length of the buffer, followed by the buffer itself.
*/

int
_res_write(sock, addr, host, buf, bufsize)
input int sock;
input struct sockaddr_in *addr;		/* the server address to connect to */
input char *host;			/* name of server to connect to */
input char *buf;			/* location of formatted query buffer */
input int bufsize;			/* length of query buffer */
{
	u_short len;

/*
 * Write the length of the query buffer.
 */
	/* len = htons(bufsize); */
	putshort((u_short)bufsize, (u_char *)&len);

	if (write(sock, (char *)&len, INT16SZ) != INT16SZ)
	{
		_res_perror(addr, host, "write query length");
		return(-1);
	}

/*
 * Write the query buffer itself.
 */
	if (write(sock, buf, bufsize) != bufsize)
	{
		_res_perror(addr, host, "write query");
		return(-1);
	}

	return(bufsize);
}

/*
** _RES_READ -- Read the answer buffer via a stream socket
** -------------------------------------------------------
**
**	Returns:
**		Length of (untruncated) answer if successfully received.
**		-1 in case of failure (error message is issued).
**
**	The answer is read in two steps: first a single word which
**	gives the length of the buffer, followed by the buffer itself.
**	If the answer is too long to fit into the supplied buffer,
**	only the portion that fits will be stored, the residu will be
**	flushed, and the truncation flag will be set.
**
**	Note. The returned length is that of the *un*truncated answer,
**	however, and not the amount of data that is actually available.
**	This may give the caller a hint about new buffer reallocation.
*/

int
_res_read(sock, addr, host, buf, bufsize)
input int sock;
input struct sockaddr_in *addr;		/* the server address to connect to */
input char *host;			/* name of server to connect to */
output char *buf;			/* location of buffer to store answer */
input int bufsize;			/* maximum size of answer buffer */
{
	u_short len;
	char *buffer;
	int buflen;
	int reslen;
	register int n;

	/* set stream timeout for recv_sock() */
	timeout = READTIMEOUT;

/*
 * Read the length of answer buffer.
 */
	buffer = (char *)&len;
	buflen = INT16SZ;

	while (buflen > 0 && (n = recv_sock(sock, buffer, buflen)) > 0)
	{
		buffer += n;
		buflen -= n;
	}

	if (buflen != 0)
	{
		_res_perror(addr, host, "read answer length");
		return(-1);
	}

/*
 * Terminate if length is zero.
 */
	/* len = ntohs(len); */
	len = _getshort((u_char *)&len);
	if (len == 0)
		return(0);

/*
 * Check for truncation.
 * Do not chop the returned length in case of buffer overflow.
 */
	reslen = 0;
	if ((int)len > bufsize)
	{
		reslen = len - bufsize;
		/* len = bufsize; */
	}

/*
 * Read the answer buffer itself.
 * Truncate the answer is the supplied buffer is not big enough.
 */
	buffer = buf;
	buflen = (reslen > 0) ? bufsize : len;

	while (buflen > 0 && (n = recv_sock(sock, buffer, buflen)) > 0)
	{
		buffer += n;
		buflen -= n;
	}

	if (buflen != 0)
	{
		_res_perror(addr, host, "read answer");
		return(-1);
	}

/*
 * Discard the residu to keep connection in sync.
 */
	if (reslen > 0)
	{
		HEADER *bp = (HEADER *)buf;
		char resbuf[PACKETSZ];

		buffer = resbuf;
		buflen = (reslen < sizeof(resbuf)) ? reslen : sizeof(resbuf);

		while (reslen > 0 && (n = recv_sock(sock, buffer, buflen)) > 0)
		{
			reslen -= n;
			buflen = (reslen < sizeof(resbuf)) ? reslen : sizeof(resbuf);
		}

		if (reslen != 0)
		{
			_res_perror(addr, host, "read residu");
			return(-1);
		}

		if (bitset(RES_DEBUG, _res.options))
			printf("%sresponse truncated to %d bytes\n", dbprefix, bufsize);

		/* set truncation flag */
		bp->tc = 1;
	}

	return(len);
}

/*
** RECV_SOCK -- Read from stream or datagram socket with timeout
** -------------------------------------------------------------
**
**	Returns:
**		Length of buffer if successfully received.
**		-1 in case of failure or timeout.
**	Inputs:
**		The global variable ``timeout'' should have been
**		set with the desired timeout value in seconds.
**	Outputs:
**		Sets ``from'' to the address of the packet sender.
*/

static int
recv_sock(sock, buffer, buflen)
input int sock;
output char *buffer;			/* current buffer address */
input int buflen;			/* remaining buffer size */
{
	fd_set fds;
	struct timeval wait;
	socklen_t fromlen;
	register int n;

	wait.tv_sec = timeout;
	wait.tv_usec = 0;
rewait:
	/* FD_ZERO(&fds); */
	bzero((char *)&fds, sizeof(fds));
	FD_SET(sock, &fds);

	/* wait for the arrival of data, or timeout */
	n = select(FD_SETSIZE, &fds, (fd_set *)NULL, (fd_set *)NULL, &wait);
	if (n <= 0)
	{
		if (n < 0 && errno == EINTR)
			goto rewait;
		if (n == 0)
			errno = ETIMEDOUT;
		return(-1);
	}
reread:
	/* fake an error if nothing was actually read */
	fromlen = sizeof(from);
	n = recvfrom(sock, buffer, buflen, 0, from_sa, &fromlen);
	if (n < 0 && errno == EINTR)
		goto reread;
	if (n == 0)
		errno = ECONNRESET;
	return(n);
}

/*
 * Alternative version for systems with broken networking code.
 *
 * The select() system call may fail on the solaris 2.4 platform
 * without appropriate patches. However, these patches are reported
 * to break client NFS.
 *
 * This version uses an alarm() instead of select(). This introduces
 * additional system call overhead.
 */

#ifdef BROKEN_SELECT

static int
recv_sock(sock, buffer, buflen)
input int sock;
output char *buffer;			/* current buffer address */
input int buflen;			/* remaining buffer size */
{
	int fromlen;
	register int n;

	if (setjmp(timer_buf) != 0)
	{
		errno = ETIMEDOUT;
		setalarm(0);
		return(-1);
	}

	(void) signal(SIGALRM, timer);
	setalarm(timeout);
reread:
	/* fake an error if nothing was actually read */
	fromlen = sizeof(from);
	n = recvfrom(sock, buffer, buflen, 0, from_sa, &fromlen);
	if (n < 0 && errno == EINTR)
		goto reread;
	if (n == 0)
		errno = ECONNRESET;
	setalarm(0);
	return(n);
}

#endif /*BROKEN_SELECT*/

/*
** _RES_PERROR -- Issue perror message including host info
** -------------------------------------------------------
**
**	Returns:
**		None.
*/

void
_res_perror(addr, host, message)
input struct sockaddr_in *addr;		/* the server address to connect to */
input char *host;			/* name of server to connect to */
input char *message;			/* perror message string */
{
	int save_errno = errno;		/* preserve state */

	/* prepend server address and name */
	if (addr != NULL)
		(void) fprintf(stderr, "%s ", inet_ntoa(addr->sin_addr));
	if (host != NULL)
		(void) fprintf(stderr, "(%s) ", host);

	/* issue actual message */
	errno = save_errno;
	perror(message);

	/* restore state */
	errno = save_errno;
}
