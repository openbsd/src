/* $NetBSD: pkcs5_pbkdf2.c,v 1.5 2004/03/17 01:29:13 dan Exp $ */

/*-
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code is an implementation of PKCS #5 PBKDF2 which is described
 * in:
 *
 * ``PKCS #5 v2.0: Password-Based Cryptography Standard'', RSA Laboratories,
 * March 25, 1999.
 *
 * and can be found at the following URL:
 *
 *	http://www.rsasecurity.com/rsalabs/pkcs/pkcs-5/
 *
 * It was also republished as RFC 2898.
 */


#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/hmac.h>

#include "pkcs5_pbkdf2.h"

static void	int_encode(u_int8_t *, int);
static void	prf_iterate(u_int8_t *, const u_int8_t *, int,
			    const u_int8_t *, int, int, int);

static void
memxor(void *res, const void *src, size_t len)
{
	size_t i;
	char *r;
	const char *s;

	r = res;
	s = src;
	for (i=0; i < len; i++)
		r[i] ^= s[i];
}

#define PRF_BLOCKLEN	20

/*
 * int_encode encodes i as a four octet integer, most significant
 * octet first.  (from the end of Step 3).
 */

static void
int_encode(u_int8_t *res, int i)
{

	*res++ = (i >> 24) & 0xff;
	*res++ = (i >> 16) & 0xff;
	*res++ = (i >>  8) & 0xff;
	*res   = (i      ) & 0xff;
}

static void
prf_iterate(u_int8_t *r, const u_int8_t *P, int Plen,
	    const u_int8_t *S, int Slen, int c, int ind)
{
	int		 first_time = 1;
	int		 i;
	int		 datalen;
	int		 tmplen;
	u_int8_t	*data;
	u_int8_t	 tmp[EVP_MAX_MD_SIZE];

	data = malloc(Slen + 4);
	if (!data)
		err(1, "prf_iterate");
	memcpy(data, S, Slen);
	int_encode(data + Slen, ind);
	datalen = Slen + 4;

	for (i=0; i < c; i++) {
		HMAC(EVP_sha1(), P, Plen, data, datalen, tmp, &tmplen);

		assert(tmplen == PRF_BLOCKLEN);

		if (first_time) {
			memcpy(r, tmp, PRF_BLOCKLEN);
			first_time = 0;
		} else 
			memxor(r, tmp, PRF_BLOCKLEN);
		memcpy(data, tmp, PRF_BLOCKLEN);
		datalen = PRF_BLOCKLEN;
	}
	free(data);
}

/*
 * pkcs5_pbkdf2 takes all of its lengths in bytes.
 */

int
pkcs5_pbkdf2(u_int8_t **r, int dkLen, const u_int8_t *P, int Plen,
	     const u_int8_t *S, int Slen, int c, int compat)
{
	int	i;
	int	l;

	/* sanity */
	if (!r)
		return -1;
	if (dkLen <= 0)
		return -1;
	if (c < 1)
		return -1;

	/* Step 2 */
	l = (dkLen + PRF_BLOCKLEN - 1) / PRF_BLOCKLEN;

	/* allocate the output */
	*r = calloc(l, PRF_BLOCKLEN);
	if (!*r)
		return -1;

	/* Step 3 */
	for (i=0; i < l; i++)
		prf_iterate(*r + (PRF_BLOCKLEN * i), P, Plen, S, Slen, c, 
			(compat?i:i+1));

	/* Step 4 and 5
	 *  by the structure of the code, we do not need to concatenate
	 *  the blocks, they're already concatenated.  We do not extract
	 *  the first dkLen octets, since we [naturally] assume that the
	 *  calling function will use only the octets that it needs and
	 *  the free(3) will free all of the allocated memory.
	 */
	return 0;
}
