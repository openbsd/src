/*	$OpenBSD: svc_raw.c,v 1.9 2005/08/08 08:05:35 espie Exp $ */
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

/*
 * svc_raw.c,   This a toy for simple testing and timing.
 * Interface to create an rpc client and server in the same UNIX process.
 * This lets us similate rpc and get rpc (round trip) overhead, without
 * any interference from the kernel.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <stdlib.h>
#include <rpc/rpc.h>


/*
 * This is the "network" that we will be moving data over
 */
static struct svcraw_private {
	char	_raw_buf[UDPMSGSIZE];
	SVCXPRT	server;
	XDR	xdr_stream;
	char	verf_body[MAX_AUTH_BYTES];
} *svcraw_private;

static bool_t		svcraw_recv(SVCXPRT *xprt, struct rpc_msg *msg);
static enum xprt_stat 	svcraw_stat(SVCXPRT *xprt);
static bool_t		svcraw_getargs(SVCXPRT *xprt, xdrproc_t xdr_args,
			    caddr_t args_ptr);
static bool_t		svcraw_reply(SVCXPRT *xprt, struct rpc_msg *msg);
static bool_t		svcraw_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args,
			    caddr_t args_ptr);
static void		svcraw_destroy(SVCXPRT *xprt);

static struct xp_ops server_ops = {
	svcraw_recv,
	svcraw_stat,
	svcraw_getargs,
	svcraw_reply,
	svcraw_freeargs,
	svcraw_destroy
};

SVCXPRT *
svcraw_create(void)
{
	struct svcraw_private *srp = svcraw_private;

	if (srp == NULL) {
		srp = (struct svcraw_private *)calloc(1, sizeof (*srp));
		if (srp == NULL)
			return (NULL);
	}
	srp->server.xp_sock = 0;
	srp->server.xp_port = 0;
	srp->server.xp_ops = &server_ops;
	srp->server.xp_verf.oa_base = srp->verf_body;
	xdrmem_create(&srp->xdr_stream, srp->_raw_buf, UDPMSGSIZE, XDR_FREE);
	return (&srp->server);
}

/* ARGSUSED */
static enum xprt_stat
svcraw_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
}

/* ARGSUSED */
static bool_t
svcraw_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct svcraw_private *srp = svcraw_private;
	XDR *xdrs;

	if (srp == NULL)
		return (0);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg))
	       return (FALSE);
	return (TRUE);
}

/* ARGSUSED */
static bool_t
svcraw_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct svcraw_private *srp = svcraw_private;
	XDR *xdrs;

	if (srp == NULL)
		return (FALSE);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_replymsg(xdrs, msg))
	       return (FALSE);
	(void)XDR_GETPOS(xdrs);  /* called just for overhead */
	return (TRUE);
}

/* ARGSUSED */
static bool_t
svcraw_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{
	struct svcraw_private *srp = svcraw_private;

	if (srp == NULL)
		return (FALSE);
	return ((*xdr_args)(&srp->xdr_stream, args_ptr));
}

/* ARGSUSED */
static bool_t
svcraw_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{ 
	struct svcraw_private *srp = svcraw_private;
	XDR *xdrs;

	if (srp == NULL)
		return (FALSE);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
} 

/* ARGSUSED */
static void
svcraw_destroy(SVCXPRT *xprt)
{
}
