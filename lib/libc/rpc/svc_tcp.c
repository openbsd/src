/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: svc_tcp.c,v 1.24 2005/01/08 19:17:39 krw Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * svc_tcp.c, Server side for TCP/IP based RPC. 
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listner and connection establisher)
 * and a record/tcp stream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <errno.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

/*
 * Ops vector for TCP/IP based rpc service handle
 */
static bool_t		svctcp_recv();
static enum xprt_stat	svctcp_stat();
static bool_t		svctcp_getargs();
static bool_t		svctcp_reply();
static bool_t		svctcp_freeargs();
static void		svctcp_destroy();

static struct xp_ops svctcp_op = {
	svctcp_recv,
	svctcp_stat,
	svctcp_getargs,
	svctcp_reply,
	svctcp_freeargs,
	svctcp_destroy
};

/*
 * Ops vector for TCP/IP rendezvous handler
 */
static bool_t		rendezvous_request();
static enum xprt_stat	rendezvous_stat();

static struct xp_ops svctcp_rendezvous_op = {
	rendezvous_request,
	rendezvous_stat,
	(bool_t (*)())abort,	/* XXX abort illegal in library */
	(bool_t (*)())abort,	/* XXX abort illegal in library */
	(bool_t (*)())abort,	/* XXX abort illegal in library */
	svctcp_destroy
};

static int readtcp(), writetcp();
static SVCXPRT *makefd_xprt();

struct tcp_rendezvous { /* kept in xprt->xp_p1 */
	u_int sendsize;
	u_int recvsize;
};

struct tcp_conn {  /* kept in xprt->xp_p1 */
	enum xprt_stat strm_stat;
	u_long x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
};

/*
 * Usage:
 *	xprt = svctcp_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) tcp based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svctcp_create
 * binds it to an arbitrary port.  The routine then starts a tcp
 * listener on the socket's associated port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 *
 * Since tcp streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svctcp_create(sock, sendsize, recvsize)
	int sock;
	u_int sendsize;
	u_int recvsize;
{
	bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct tcp_rendezvous *r;
	struct sockaddr_in addr;
	socklen_t len = sizeof(struct sockaddr_in);

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			perror("svctcp_.c - udp socket creation problem");
			return (NULL);
		}
		madesock = TRUE;
	}
	memset(&addr, 0, sizeof (addr));
	addr.sin_len = sizeof(struct sockaddr_in);
	addr.sin_family = AF_INET;
	if (bindresvport(sock, &addr)) {
		addr.sin_port = 0;
		(void)bind(sock, (struct sockaddr *)&addr, len);
	}
	if ((getsockname(sock, (struct sockaddr *)&addr, &len) != 0)  ||
	    (listen(sock, 2) != 0)) {
		perror("svctcp_.c - cannot getsockname or listen");
		if (madesock)
			(void)close(sock);
		return (NULL);
	}
	r = (struct tcp_rendezvous *)mem_alloc(sizeof(*r));
	if (r == NULL) {
		(void)fprintf(stderr, "svctcp_create: out of memory\n");
		if (madesock)
			(void)close(sock);
		return (NULL);
	}
	r->sendsize = sendsize;
	r->recvsize = recvsize;
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void)fprintf(stderr, "svctcp_create: out of memory\n");
		if (madesock)
			(void)close(sock);
		free(r);
		return (NULL);
	}
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)r;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svctcp_rendezvous_op;
	xprt->xp_port = ntohs(addr.sin_port);
	xprt->xp_sock = sock;
	if (__xprt_register(xprt) == 0) {
		if (madesock)
			(void)close(sock);
		free(r);
		free(xprt);
		return (NULL);
	}
	return (xprt);
}

/*
 * Like svtcp_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svcfd_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{

	return (makefd_xprt(fd, sendsize, recvsize));
}

static SVCXPRT *
makefd_xprt(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	SVCXPRT *xprt;
	struct tcp_conn *cd;
 
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: out of memory\n");
		goto done;
	}
	cd = (struct tcp_conn *)mem_alloc(sizeof(struct tcp_conn));
	if (cd == NULL) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: out of memory\n");
		mem_free((char *) xprt, sizeof(SVCXPRT));
		xprt = NULL;
		goto done;
	}
	cd->strm_stat = XPRT_IDLE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)xprt, readtcp, writetcp);
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)cd;
	xprt->xp_verf.oa_base = cd->verf_body;
	xprt->xp_addrlen = 0;
	xprt->xp_ops = &svctcp_op;  /* truely deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_sock = fd;
	if (__xprt_register(xprt) == 0) {
		free(xprt);
		free(cd);
		return (NULL);
	}
    done:
	return (xprt);
}

static bool_t
rendezvous_request(xprt)
	SVCXPRT *xprt;
{
	int sock;
	struct tcp_rendezvous *r;
	struct sockaddr_in addr;
	socklen_t len;

	r = (struct tcp_rendezvous *)xprt->xp_p1;
    again:
	len = sizeof(struct sockaddr_in);
	if ((sock = accept(xprt->xp_sock, (struct sockaddr *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}

#ifdef IP_OPTIONS
	{
		struct ipoption opts;
		int optsize = sizeof(opts), i;

		if (!getsockopt(sock, IPPROTO_IP, IP_OPTIONS, (char *)&opts,
		    &optsize) && optsize != 0) {
			for (i = 0; (char *)&opts.ipopt_list[i] - (char *)&opts <
			    optsize; ) {	
				u_char c = (u_char)opts.ipopt_list[i];
				if (c == IPOPT_LSRR || c == IPOPT_SSRR) {
					close(sock);
					return (FALSE);
				}
				if (c == IPOPT_EOL)
					break;
				i += (c == IPOPT_NOP) ? 1 :
				    (u_char)opts.ipopt_list[i+1];
			}
		}
	}
#endif

	/*
	 * XXX careful for ftp bounce attacks. If discovered, close the
	 * socket and look for another connection.
	 */
	if (addr.sin_port == htons(20)) {
		close(sock);
		return (FALSE);
	}

	/*
	 * make a new transporter (re-uses xprt)
	 */
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	xprt->xp_raddr = addr;
	xprt->xp_addrlen = len;
	return (FALSE); /* there is never an rpc msg to be processed */
}

static enum xprt_stat
rendezvous_stat()
{

	return (XPRT_IDLE);
}

static void
svctcp_destroy(xprt)
	SVCXPRT *xprt;
{
	struct tcp_conn *cd = (struct tcp_conn *)xprt->xp_p1;

	xprt_unregister(xprt);
	if (xprt->xp_sock != -1)
		(void)close(xprt->xp_sock);
	xprt->xp_sock = -1;
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
	}
	mem_free((caddr_t)cd, sizeof(struct tcp_conn));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}

/*
 * All read operations timeout after 35 seconds.
 * A timeout is fatal for the connection.
 */
static struct timeval wait_per_try = { 35, 0 };

/*
 * reads data from the tcp conection.
 * any error is fatal and the connection is closed.
 * (And a read of zero bytes is a half closed stream => error.)
 */
static int
readtcp(xprt, buf, len)
	SVCXPRT *xprt;
	caddr_t buf;
	int len;
{
	int sock = xprt->xp_sock;
	int delta, nready;
	struct timeval start;
	struct timeval tmp1, tmp2;
	struct pollfd *pfd = NULL;
	int prevbytes = 0, bytes;

	pfd = (struct pollfd *)malloc(sizeof(*pfd) * (svc_max_pollfd + 1));
	if (pfd == NULL)
		goto fatal_err;
	pfd[0].fd = sock;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	memcpy(&pfd[1], svc_pollfd, (sizeof(*pfd) * svc_max_pollfd));

	/*
	 * All read operations timeout after 35 seconds.
	 * A timeout is fatal for the connection.
	 */
	delta = wait_per_try.tv_sec * 1000;
	gettimeofday(&start, NULL);
	do {
		nready = poll(pfd, svc_max_pollfd + 1, delta);
		switch (nready) {
		case -1:
			if (errno != EINTR)
				goto fatal_err;
			gettimeofday(&tmp1, NULL);
			timersub(&tmp1, &start, &tmp2);
			timersub(&wait_per_try, &tmp2, &tmp1);
			if (tmp1.tv_sec < 0 || !timerisset(&tmp1))
				goto fatal_err;
			delta = tmp1.tv_sec * 1000 + tmp1.tv_usec / 1000;
			continue;
		case 0:
			goto fatal_err;
		default:
			if (pfd[0].revents == 0) {
				svc_getreq_poll(&pfd[1], nready);
				gettimeofday(&tmp1, NULL);
				timersub(&tmp1, &start, &tmp2);
				timersub(&wait_per_try, &tmp2, &tmp1);
				if (tmp1.tv_sec < 0 || !timerisset(&tmp1))
					goto fatal_err;
				delta = tmp1.tv_sec * 1000 + tmp1.tv_usec / 1000;
				continue;
			}
		}
	} while (pfd[0].revents == 0);
	if ((len = read(sock, buf, len)) > 0) {
		if (pfd)
			free(pfd);
		return (len);
	}
fatal_err:
	((struct tcp_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	if (pfd)
		free(pfd);
	return (-1);
}

/*
 * writes data to the tcp connection.
 * Any error is fatal and the connection is closed.
 */
static int
writetcp(xprt, buf, len)
	SVCXPRT *xprt;
	caddr_t buf;
	int len;
{
	int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = write(xprt->xp_sock, buf, cnt)) < 0) {
			((struct tcp_conn *)(xprt->xp_p1))->strm_stat =
			    XPRT_DIED;
			return (-1);
		}
	}
	return (len);
}

static enum xprt_stat
svctcp_stat(xprt)
	SVCXPRT *xprt;
{
	struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svctcp_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);
	XDR *xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		return (TRUE);
	}
	cd->strm_stat = XPRT_DIED;	/* XXX */
	return (FALSE);
}

static bool_t
svctcp_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	return ((*xdr_args)(&(((struct tcp_conn *)(xprt->xp_p1))->xdrs), args_ptr));
}

static bool_t
svctcp_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	XDR *xdrs =
	    &(((struct tcp_conn *)(xprt->xp_p1))->xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static bool_t
svctcp_reply(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);
	XDR *xdrs = &(cd->xdrs);
	bool_t stat;

	xdrs->x_op = XDR_ENCODE;
	msg->rm_xid = cd->x_id;
	stat = xdr_replymsg(xdrs, msg);
	(void)xdrrec_endofrecord(xdrs, TRUE);
	return (stat);
}
