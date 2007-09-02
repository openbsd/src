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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <stdlib.h>
#include <string.h>

#include <openssl/hmac.h>

#include "pkcs5_pbkdf2.h"

static void	int_encode(u_int8_t *, int);
static void	prf_iterate(u_int8_t *, const u_int8_t *, int,
			    const u_int8_t *, int, int, int);
static int	pkcs5_pbkdf2_time(int, int);

void
memxor(void *res, const void *src, size_t len)
{
	int i;
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

/*
 * We use predefined lengths for the password and salt to ensure that
 * no analysis can be done on the output of the calibration based on
 * those parameters.  We do not do the same for dkLen because:
 *	1.  dkLen is known to the attacker if they know the iteration
 *	    count, and
 *	2.  using the wrong dkLen will skew the calibration by an
 *	    integral factor n = (dkLen / 160).
 */

#define CAL_PASSLEN	   64
#define CAL_SALTLEN	   64
#define CAL_TIME	30000		/* Minimum number of microseconds that
					 * are considered significant.
					 */

/*
 * We return the user time in milliseconds that c iterations
 * of the algorithm take.
 */

static int
pkcs5_pbkdf2_time(int dkLen, int c)
{
	struct rusage	 start;
	struct rusage	 end;
	int		 ret;
	u_int8_t	*r = NULL;
	u_int8_t	 P[CAL_PASSLEN];
	u_int8_t	 S[CAL_SALTLEN];

	getrusage(RUSAGE_SELF, &start);
	/* XXX compat flag at end to be removed when _OLD keygen method is */
	ret = pkcs5_pbkdf2(&r, dkLen, P, sizeof(P), S, sizeof(S), c, 0);
	if (ret)
		return ret;
	getrusage(RUSAGE_SELF, &end);
	free(r);

	return (end.ru_utime.tv_sec - start.ru_utime.tv_sec) * 1000000
	     + (end.ru_utime.tv_usec - start.ru_utime.tv_usec);
}

int
pkcs5_pbkdf2_calibrate(int dkLen, int milliseconds)
{
	int	c;
	int	t = 0;
	int	ret;

	/*
	 * First we get a meaningfully long time by doubling the
	 * iteration count until it takes longer than CAL_TIME.  This
	 * should take approximately 2 * CAL_TIME.
	 */
	for (c=1;; c *= 2) {
		t = pkcs5_pbkdf2_time(dkLen, c);
		if (t > CAL_TIME)
			break;
	}

	/* Now that we know that, we scale it. */
	ret = (int) ((u_int64_t) c * milliseconds / t);

	/*
	 * Since it is quite important to not get this wrong,
	 * we test the result.
	 */

	t = pkcs5_pbkdf2_time(dkLen, 10000);

	/* if we are over 5% off, return an error */
	if (abs(milliseconds - t) > (milliseconds / 20))
		return -1;

	return ret;
}
