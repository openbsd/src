/* $OpenBSD: ap_ctx.c,v 1.6 2005/06/20 12:23:22 robert Exp $ */

/* ====================================================================
 * Copyright (c) 1998-2000 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
**  Generic Context Interface for Apache
**  Written by Ralf S. Engelschall <rse@engelschall.com> 
*/

#include "httpd.h"
#include "ap_config.h"
#include "ap_ctx.h"

API_EXPORT(ap_ctx *)
ap_ctx_new(pool *p)
{
	ap_ctx *ctx;
	int i;

	if (p != NULL) {
		ctx = (ap_ctx *)ap_palloc(p, sizeof(ap_ctx_rec));
		ctx->cr_pool = p;
		ctx->cr_entry = (ap_ctx_entry **)
		    ap_palloc(p, sizeof(ap_ctx_entry *)*(AP_CTX_MAX_ENTRIES+1));
	}
	else {
		ctx = (ap_ctx *)malloc(sizeof(ap_ctx_rec));
		ctx->cr_pool = NULL;
		ctx->cr_entry = (ap_ctx_entry **)
		    malloc(sizeof(ap_ctx_entry *)*(AP_CTX_MAX_ENTRIES+1));
	}
	for (i = 0; i < AP_CTX_MAX_ENTRIES+1; i++)
		ctx->cr_entry[i] = NULL;
	return ctx;
}

API_EXPORT(void)
ap_ctx_set(ap_ctx *ctx, char *key, void *val)
{
	int i;
	ap_ctx_entry *ce;

	ce = NULL;
	for (i = 0; ctx->cr_entry[i] != NULL; i++) {
		if (strcmp(ctx->cr_entry[i]->ce_key, key) == 0) {
			ce = ctx->cr_entry[i];
			break;
		}
	}
	if (ce == NULL) {
		if (i == AP_CTX_MAX_ENTRIES)
			return;
		if (ctx->cr_pool != NULL) {
			ce = (ap_ctx_entry *)ap_palloc(ctx->cr_pool,
			    sizeof(ap_ctx_entry));
			ce->ce_key = ap_pstrdup(ctx->cr_pool, key);
		}
		else {
			ce = (ap_ctx_entry *)malloc(sizeof(ap_ctx_entry));
			ce->ce_key = strdup(key);
		}
		ctx->cr_entry[i] = ce;
		ctx->cr_entry[i+1] = NULL;
	}
	ce->ce_val = val;
	return;
}

API_EXPORT(void *)
ap_ctx_get(ap_ctx *ctx, char *key)
{
	int i;

	for (i = 0; ctx->cr_entry[i] != NULL; i++)
		if (strcmp(ctx->cr_entry[i]->ce_key, key) == 0)
			return ctx->cr_entry[i]->ce_val;
	return NULL;
}

API_EXPORT(ap_ctx *)
ap_ctx_overlay(pool *p, ap_ctx *over, ap_ctx *base)
{
	ap_ctx *new;
	int i;

	#ifdef POOL_DEBUG
	if (p != NULL) {
		if (!ap_pool_is_ancestor(over->cr_pool, p))
		    ap_log_assert("ap_ctx_overlay: overlay's pool is not an"
			" ancestor of p", __FILE__, __LINE__);
		if (!ap_pool_is_ancestor(base->cr_pool, p))
		    ap_log_assert("ap_ctx_overlay: base's pool is not an"
			" ancestor of p", __FILE__, __LINE__);
	}
	#endif
	if ((new = ap_ctx_new(p)) == NULL)
		return NULL;
	memcpy(new->cr_entry, base->cr_entry,
	    sizeof(ap_ctx_entry *)*(AP_CTX_MAX_ENTRIES+1));
	for (i = 0; over->cr_entry[i] != NULL; i++)
		ap_ctx_set(new, over->cr_entry[i]->ce_key,
		    over->cr_entry[i]->ce_val);
	return new;
}
