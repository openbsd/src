/*	$OpenBSD: ypxfr_xdr.c,v 1.3 1996/06/26 21:26:41 maja Exp $ */

/*
 * Copyright (c) 1995 Mats O Jansson <moj@stacken.kth.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: ypxfr_xdr.c,v 1.3 1996/06/26 21:26:41 maja Exp $";
#endif



#include <rpc/rpc.h>
#include <rpcsvc/yp.h>

bool_t
xdr_ypxfrstat(xdrs, objp)
	XDR *xdrs;
	ypxfrstat *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

#ifdef notdef
bool_t
xdr_ypreq_xfr(xdrs, objp)
	XDR *xdrs;
	ypreq_xfr *objp;
{
	if (!xdr_ypmap_parms(xdrs, &objp->map_parms)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->prog)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->port)) {
		return (FALSE);
	}
	return (TRUE);
}
#endif

bool_t
xdr_ypresp_xfr(xdrs, objp)
	XDR *xdrs;
	ypresp_xfr *objp;
{
	if (!xdr_u_int(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_ypxfrstat(xdrs, &objp->xfrstat)) {
		return (FALSE);
	}
	return (TRUE);
}




