/* $OpenBSD: ap_ctx.h,v 1.6 2005/03/28 23:26:51 niallo Exp $ */

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

#ifndef AP_CTX_H
#define AP_CTX_H

#ifndef FALSE
#define FALSE 0
#define TRUE  !FALSE
#endif

/*
 * Internal Context Record Definition
 */

#define AP_CTX_MAX_ENTRIES 1024

typedef struct {
	char *ce_key;
	void *ce_val;
} ap_ctx_entry;

typedef struct {
	pool          *cr_pool;
	ap_ctx_entry **cr_entry;
} ap_ctx_rec;

typedef ap_ctx_rec ap_ctx;

/*
 * Some convinience macros for storing _numbers_ 0...n in contexts, i.e.
 * treating numbers as pointers but keeping track of the NULL return code of
 * ap_ctx_get.
 */
#define AP_CTX_NUM2PTR(n) (void *)(((unsigned long)(n))+1)
#define AP_CTX_PTR2NUM(p) (unsigned long)(((char *)(p))-1)

/*
 * Prototypes for Context Handling Functions
 */

API_EXPORT(ap_ctx *)ap_ctx_new(pool *p);
API_EXPORT(void)    ap_ctx_set(ap_ctx *ctx, char *key, void *val);
API_EXPORT(void *)  ap_ctx_get(ap_ctx *ctx, char *key);
API_EXPORT(ap_ctx *)ap_ctx_overlay(pool *p, ap_ctx *over, ap_ctx *base);

#endif /* AP_CTX_H */
