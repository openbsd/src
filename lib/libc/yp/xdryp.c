/*	$NetBSD: xdryp.c,v 1.9 1995/07/14 21:04:17 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
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
 *	This product includes software developed by Theo de Raadt.
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
static char *rcsid = "$NetBSD: xdryp.c,v 1.9 1995/07/14 21:04:17 christos Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

extern int (*ypresp_allfn) __P((u_long, char *, int, char *, int, void *));
extern void *ypresp_data;

bool_t
xdr_domainname(xdrs, objp)
XDR *xdrs;
char *objp;
{
	return xdr_string(xdrs, &objp, YPMAXDOMAIN);
}

bool_t
xdr_peername(xdrs, objp)
XDR *xdrs;
char *objp;
{
	return xdr_string(xdrs, &objp, YPMAXPEER);
}

bool_t
xdr_datum(xdrs, objp)
XDR *xdrs;
datum *objp;
{
	return xdr_bytes(xdrs, (char **)&objp->dptr,
			 (u_int *)&objp->dsize, YPMAXRECORD);
}

bool_t
xdr_mapname(xdrs, objp)
XDR *xdrs;
char *objp;
{
	return xdr_string(xdrs, &objp, YPMAXMAP);
}

bool_t
xdr_ypreq_key(xdrs, objp)
XDR *xdrs;
struct ypreq_key *objp;
{
	if (!xdr_domainname(xdrs, (char *)objp->domain)) {
		return FALSE;
	}
	if (!xdr_mapname(xdrs, (char *)objp->map)) {
		return FALSE;
	}
	return xdr_datum(xdrs, &objp->keydat);
}

bool_t
xdr_ypreq_nokey(xdrs, objp)
XDR *xdrs;
struct ypreq_nokey *objp;
{
	if (!xdr_domainname(xdrs, (char *)objp->domain)) {
		return FALSE;
	}
	return xdr_mapname(xdrs, (char *)objp->map);
}

bool_t
xdr_yp_inaddr(xdrs, objp)
XDR *xdrs;
struct in_addr *objp;
{
	return xdr_opaque(xdrs, (caddr_t)&objp->s_addr, sizeof objp->s_addr);
}

bool_t
xdr_ypbind_binding(xdrs, objp)
XDR *xdrs;
struct ypbind_binding *objp;
{
	if (!xdr_yp_inaddr(xdrs, &objp->ypbind_binding_addr)) {
		return FALSE;
	}
	return xdr_opaque(xdrs, (void *)&objp->ypbind_binding_port,
	    sizeof objp->ypbind_binding_port);
}

bool_t
xdr_ypbind_resptype(xdrs, objp)
XDR *xdrs;
enum ypbind_resptype *objp;
{
	return xdr_enum(xdrs, (enum_t *)objp);
}

bool_t
xdr_ypstat(xdrs, objp)
XDR *xdrs;
enum ypbind_resptype *objp;
{
	return xdr_enum(xdrs, (enum_t *)objp);
}

bool_t
xdr_ypbind_resp(xdrs, objp)
XDR *xdrs;
struct ypbind_resp *objp;
{
	if (!xdr_ypbind_resptype(xdrs, &objp->ypbind_status)) {
		return FALSE;
	}

	switch (objp->ypbind_status) {
	case YPBIND_FAIL_VAL:
		return xdr_u_int(xdrs,
				 (u_int *)&objp->ypbind_respbody.ypbind_error);
	case YPBIND_SUCC_VAL:
		return xdr_ypbind_binding(xdrs, 
				 &objp->ypbind_respbody.ypbind_bindinfo);
	default:
		return FALSE;
	}
	/* NOTREACHED */
}

bool_t
xdr_ypresp_val(xdrs, objp)
XDR *xdrs;
struct ypresp_val *objp;
{
	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)&objp->status)) {
		return FALSE;
	}
	return xdr_datum(xdrs, &objp->valdat);
}

bool_t
xdr_ypbind_setdom(xdrs, objp)
XDR *xdrs;
struct ypbind_setdom *objp;
{
	if (!xdr_domainname(xdrs, objp->ypsetdom_domain)) {
		return FALSE;
	}
	if (!xdr_ypbind_binding(xdrs, &objp->ypsetdom_binding)) {
		return FALSE;
	}
	return xdr_u_short(xdrs, &objp->ypsetdom_vers);
}

bool_t
xdr_ypresp_key_val(xdrs, objp)
XDR *xdrs;
struct ypresp_key_val *objp;
{
	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)&objp->status)) {
		return FALSE;
	}
	if (!xdr_datum(xdrs, &objp->valdat)) {
		return FALSE;
	}
	return xdr_datum(xdrs, &objp->keydat);
}

bool_t
xdr_ypresp_all(xdrs, objp)
XDR *xdrs;
struct ypresp_all *objp;
{
	if (!xdr_bool(xdrs, &objp->more)) {
		return FALSE;
	}
	switch (objp->more) {
	case TRUE:
		return xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val);
	case FALSE:
		return (TRUE);
	default:
		return FALSE;
	}
	/* NOTREACHED */
}

bool_t
xdr_ypresp_all_seq(xdrs, objp)
XDR *xdrs;
u_long *objp;
{
	struct ypresp_all out;
	u_long status;
	char *key, *val;
	int size;
	int r;

	memset(&out, 0, sizeof out);
	while(1) {
		if( !xdr_ypresp_all(xdrs, &out)) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = YP_YPERR;
			return FALSE;
		}
		if(out.more == 0) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			return FALSE;
		}
		status = out.ypresp_all_u.val.status;
		switch(status) {
		case YP_TRUE:
			size = out.ypresp_all_u.val.keydat.dsize;
			if ((key = malloc(size + 1)) != NULL) {
				(void)memcpy(key,
					     out.ypresp_all_u.val.keydat.dptr,
					     size);
				key[size] = '\0';
			}
			size = out.ypresp_all_u.val.valdat.dsize;
			if ((val = malloc(size + 1)) != NULL) {
				(void)memcpy(val,
					     out.ypresp_all_u.val.valdat.dptr,
					     size);
				val[size] = '\0';
			}
			else {
				free(key);
				key = NULL;
			}
			xdr_free(xdr_ypresp_all, (char *)&out);

			if (key == NULL || val == NULL)
				return FALSE;

			r = (*ypresp_allfn)(status,
				key, out.ypresp_all_u.val.keydat.dsize,
				val, out.ypresp_all_u.val.valdat.dsize,
				ypresp_data);
			*objp = status;
			free(key);
			free(val);
			if(r)
				return TRUE;
			break;
		case YP_NOMORE:
			xdr_free(xdr_ypresp_all, (char *)&out);
			return TRUE;
		default:
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = status;
			return TRUE;
		}
	}
}

bool_t
xdr_ypresp_master(xdrs, objp)
XDR *xdrs;
struct ypresp_master *objp;
{
	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)&objp->status)) {
		return FALSE;
	}
	return xdr_string(xdrs, &objp->master, YPMAXPEER);
}

bool_t
xdr_ypmaplist_str(xdrs, objp)
XDR *xdrs;
char *objp;
{
	return xdr_string(xdrs, &objp, YPMAXMAP+1);
}

bool_t
xdr_ypmaplist(xdrs, objp)
XDR *xdrs;
struct ypmaplist *objp;
{
	if (!xdr_ypmaplist_str(xdrs, objp->ypml_name)) {
		return FALSE;
	}
	return xdr_pointer(xdrs, (caddr_t *)&objp->ypml_next,
	    sizeof(struct ypmaplist), xdr_ypmaplist);
}

bool_t
xdr_ypresp_maplist(xdrs, objp)
XDR *xdrs;
struct ypresp_maplist *objp;
{
	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)&objp->status)) {
		return FALSE;
	}
	return xdr_pointer(xdrs, (caddr_t *)&objp->list,
	    sizeof(struct ypmaplist), xdr_ypmaplist);
}

bool_t
xdr_ypresp_order(xdrs, objp)
XDR *xdrs;
struct ypresp_order *objp;
{
	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)&objp->status)) {
		return FALSE;
	}
	return xdr_u_long(xdrs, &objp->ordernum);
}
