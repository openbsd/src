/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@theos.com>
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

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: yp_all.c,v 1.7 2002/07/20 01:35:34 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"

bool_t
xdr_ypresp_all_seq(XDR *xdrs, u_long *objp)
{
	struct ypresp_all out;
	u_long status;
	char *key, *val;
	int size;
	int r;

	memset(&out, 0, sizeof out);
	while (1) {
		if (!xdr_ypresp_all(xdrs, &out)) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = (u_long)YP_YPERR;
			return FALSE;
		}
		if (out.more == 0) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			return FALSE;
		}
		status = out.ypresp_all_u.val.stat;
		switch (status) {
		case YP_TRUE:
			size = out.ypresp_all_u.val.key.keydat_len;
			if ((key = malloc(size + 1)) != NULL) {
				(void)memcpy(key,
				    out.ypresp_all_u.val.key.keydat_val,
				    size);
				key[size] = '\0';
			}
			size = out.ypresp_all_u.val.val.valdat_len;
			if ((val = malloc(size + 1)) != NULL) {
				(void)memcpy(val,
				    out.ypresp_all_u.val.val.valdat_val,
				    size);
				val[size] = '\0';
			} else {
				free(key);
				key = NULL;
			}
			xdr_free(xdr_ypresp_all, (char *)&out);

			if (key == NULL || val == NULL)
				return FALSE;

			r = (*ypresp_allfn)(status, key,
			    out.ypresp_all_u.val.key.keydat_len, val,
			    out.ypresp_all_u.val.val.valdat_len, ypresp_data);
			*objp = status;
			free(key);
			free(val);
			if (r)
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

int
yp_all(const char *indomain, const char *inmap, struct ypall_callback *incallback)
{
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct timeval  tv;
	struct sockaddr_in clnt_sin;
	CLIENT         *clnt;
	u_long		status;
	int             clnt_sock;
	int		r = 0;

	if (indomain == NULL || *indomain == '\0' ||
	    strlen(indomain) > YPMAXDOMAIN || inmap == NULL ||
	    *inmap == '\0' || strlen(inmap) > YPMAXMAP || incallback == NULL)
		return YPERR_BADARGS;

	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;
	clnt_sock = RPC_ANYSOCK;
	clnt_sin = ysd->dom_server_addr;
	clnt_sin.sin_port = 0;
	clnt = clnttcp_create(&clnt_sin, YPPROG, YPVERS, &clnt_sock, 0, 0);
	if (clnt == NULL) {
		printf("clnttcp_create failed\n");
		r = YPERR_PMAP;
		goto out;
	}
	yprnk.domain = (char *)indomain;
	yprnk.map = (char *)inmap;
	ypresp_allfn = incallback->foreach;
	ypresp_data = (void *) incallback->data;

	(void) clnt_call(clnt, YPPROC_ALL,
	    xdr_ypreq_nokey, &yprnk, xdr_ypresp_all_seq, &status, tv);
	clnt_destroy(clnt);
	if (status != YP_FALSE)
		r = ypprot_err(status);
out:
	_yp_unbind(ysd);
	return r;
}
