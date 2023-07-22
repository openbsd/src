/* $OpenBSD: obj_xref.c,v 1.10 2023/07/22 18:12:09 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
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
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/objects.h>
#include "obj_xref.h"

DECLARE_STACK_OF(nid_triple)

static int
sig_cmp(const nid_triple *a, const nid_triple *b)
{
	return a->sign_id - b->sign_id;
}

static int
sig_cmp_BSEARCH_CMP_FN(const void *a_, const void *b_)
{
	nid_triple const *a = a_;
	nid_triple const *b = b_;
	return sig_cmp(a, b);
}

static const nid_triple *
OBJ_bsearch_sig(nid_triple *key, nid_triple const *base, int num)
{
	return OBJ_bsearch_(key, base, num, sizeof(nid_triple),
	    sig_cmp_BSEARCH_CMP_FN);
}

static int
sigx_cmp(const nid_triple * const *a, const nid_triple * const *b)
{
	int ret;

	ret = (*a)->hash_id - (*b)->hash_id;
	if (ret)
		return ret;
	return (*a)->pkey_id - (*b)->pkey_id;
}

static int
sigx_cmp_BSEARCH_CMP_FN(const void *a_, const void *b_)
{
	const nid_triple * const *a = a_;
	const nid_triple * const *b = b_;
	return sigx_cmp(a, b);
}

static const nid_triple * const*
OBJ_bsearch_sigx(const nid_triple * *key, const nid_triple * const *base, int num)
{
	return OBJ_bsearch_(key, base, num, sizeof(const nid_triple *),
	    sigx_cmp_BSEARCH_CMP_FN);
}

int
OBJ_find_sigid_algs(int signid, int *pdig_nid, int *ppkey_nid)
{
	nid_triple tmp;
	const nid_triple *rv = NULL;
	tmp.sign_id = signid;

	if ((rv = OBJ_bsearch_sig(&tmp, sigoid_srt,
	    sizeof(sigoid_srt) / sizeof(nid_triple))) == NULL)
		return 0;
	if (pdig_nid)
		*pdig_nid = rv->hash_id;
	if (ppkey_nid)
		*ppkey_nid = rv->pkey_id;
	return 1;
}
LCRYPTO_ALIAS(OBJ_find_sigid_algs);

int
OBJ_find_sigid_by_algs(int *psignid, int dig_nid, int pkey_nid)
{
	nid_triple tmp;
	const nid_triple *t = &tmp;
	const nid_triple *const *rv;

	tmp.hash_id = dig_nid;
	tmp.pkey_id = pkey_nid;

	if ((rv = OBJ_bsearch_sigx(&t, sigoid_srt_xref,
	    sizeof(sigoid_srt_xref) / sizeof(nid_triple *))) == NULL)
		return 0;
	if (psignid)
		*psignid = (*rv)->sign_id;
	return 1;
}
LCRYPTO_ALIAS(OBJ_find_sigid_by_algs);

int
OBJ_add_sigid(int signid, int dig_id, int pkey_id)
{
	return 0;
}
LCRYPTO_ALIAS(OBJ_add_sigid);

void
OBJ_sigid_free(void)
{
}
LCRYPTO_ALIAS(OBJ_sigid_free);
