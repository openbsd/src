/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
static char rcsid[] = "$Id: ypserv_xdr.c,v 1.1 1995/10/23 07:46:47 deraadt Exp $";
#endif



#include <rpc/rpc.h>
#include <rpcsvc/yp.h>

bool_t
xdr_ypstat(xdrs, objp)
	XDR *xdrs;
	ypstat *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




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




bool_t
xdr_domainname(xdrs, objp)
	XDR *xdrs;
	domainname *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXDOMAIN)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_mapname(xdrs, objp)
	XDR *xdrs;
	mapname *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXMAP)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_peername(xdrs, objp)
	XDR *xdrs;
	peername *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXPEER)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_keydat(xdrs, objp)
	XDR *xdrs;
	keydat *objp;
{
	if (!xdr_bytes(xdrs, (char **)&objp->keydat_val, (u_int *)&objp->keydat_len, YPMAXRECORD)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_valdat(xdrs, objp)
	XDR *xdrs;
	valdat *objp;
{
	if (!xdr_bytes(xdrs, (char **)&objp->valdat_val, (u_int *)&objp->valdat_len, YPMAXRECORD)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypmap_parms(xdrs, objp)
	XDR *xdrs;
	ypmap_parms *objp;
{
	if (!xdr_domainname(xdrs, &objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, &objp->map)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->ordernum)) {
		return (FALSE);
	}
	if (!xdr_peername(xdrs, &objp->peer)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypreq_key(xdrs, objp)
	XDR *xdrs;
	ypreq_key *objp;
{
	if (!xdr_domainname(xdrs, &objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, &objp->map)) {
		return (FALSE);
	}
	if (!xdr_keydat(xdrs, &objp->key)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypreq_nokey(xdrs, objp)
	XDR *xdrs;
	ypreq_nokey *objp;
{
	if (!xdr_domainname(xdrs, &objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, &objp->map)) {
		return (FALSE);
	}
	return (TRUE);
}




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




bool_t
xdr_ypresp_val(xdrs, objp)
	XDR *xdrs;
	ypresp_val *objp;
{
	if (!xdr_ypstat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_valdat(xdrs, &objp->val)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypresp_key_val(xdrs, objp)
	XDR *xdrs;
	ypresp_key_val *objp;
{
	if (!xdr_ypstat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_valdat(xdrs, &objp->val)) {
		return (FALSE);
	}
	if (!xdr_keydat(xdrs, &objp->key)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypresp_master(xdrs, objp)
	XDR *xdrs;
	ypresp_master *objp;
{
	if (!xdr_ypstat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_peername(xdrs, &objp->peer)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypresp_order(xdrs, objp)
	XDR *xdrs;
	ypresp_order *objp;
{
	if (!xdr_ypstat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->ordernum)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypresp_all(xdrs, objp)
	XDR *xdrs;
	ypresp_all *objp;
{
	if (!xdr_bool(xdrs, &objp->more)) {
		return (FALSE);
	}
	switch (objp->more) {
	case TRUE:
		if (!xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val)) {
			return (FALSE);
		}
		break;
	case FALSE:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}




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




bool_t
xdr_ypmaplist(xdrs, objp)
	XDR *xdrs;
	ypmaplist *objp;
{
	if (!xdr_mapname(xdrs, &objp->map)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->next, sizeof(ypmaplist), xdr_ypmaplist)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_ypresp_maplist(xdrs, objp)
	XDR *xdrs;
	ypresp_maplist *objp;
{
	if (!xdr_ypstat(xdrs, &objp->stat)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->maps, sizeof(ypmaplist), xdr_ypmaplist)) {
		return (FALSE);
	}
	return (TRUE);
}


