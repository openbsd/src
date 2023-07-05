/* $OpenBSD: ecdh.c,v 1.3 2023/07/05 17:10:10 tb Exp $ */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The ECDH software is originally written by Douglas Stebila of
 * Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "ec_local.h"

/*
 * Key derivation function from X9.63/SECG.
 */

/* Way more than we will ever need */
#define ECDH_KDF_MAX	(1 << 30)

int
ecdh_KDF_X9_63(unsigned char *out, size_t outlen, const unsigned char *Z,
    size_t Zlen, const unsigned char *sinfo, size_t sinfolen, const EVP_MD *md)
{
	EVP_MD_CTX *mctx = NULL;
	unsigned int i;
	size_t mdlen;
	unsigned char ctr[4];
	int rv = 0;

	if (sinfolen > ECDH_KDF_MAX || outlen > ECDH_KDF_MAX ||
	    Zlen > ECDH_KDF_MAX)
		return 0;
	mctx = EVP_MD_CTX_new();
	if (mctx == NULL)
		return 0;
	mdlen = EVP_MD_size(md);
	for (i = 1;; i++) {
		unsigned char mtmp[EVP_MAX_MD_SIZE];
		if (!EVP_DigestInit_ex(mctx, md, NULL))
			goto err;
		ctr[3] = i & 0xFF;
		ctr[2] = (i >> 8) & 0xFF;
		ctr[1] = (i >> 16) & 0xFF;
		ctr[0] = (i >> 24) & 0xFF;
		if (!EVP_DigestUpdate(mctx, Z, Zlen))
			goto err;
		if (!EVP_DigestUpdate(mctx, ctr, sizeof(ctr)))
			goto err;
		if (!EVP_DigestUpdate(mctx, sinfo, sinfolen))
			goto err;
		if (outlen >= mdlen) {
			if (!EVP_DigestFinal(mctx, out, NULL))
				goto err;
			outlen -= mdlen;
			if (outlen == 0)
				break;
			out += mdlen;
		} else {
			if (!EVP_DigestFinal(mctx, mtmp, NULL))
				goto err;
			memcpy(out, mtmp, outlen);
			explicit_bzero(mtmp, mdlen);
			break;
		}
	}
	rv = 1;

 err:
	EVP_MD_CTX_free(mctx);

	return rv;
}

/*
 * Based on the ECKAS-DH1 and ECSVDP-DH primitives in the IEEE 1363 standard.
 */
/* XXX - KDF handling moved to ECDH_compute_key().  See OpenSSL e2285d87. */
int
ecdh_compute_key(void *out, size_t outlen, const EC_POINT *pub_key, EC_KEY *ecdh,
    void *(*KDF)(const void *in, size_t inlen, void *out, size_t *outlen))
{
	BN_CTX *ctx;
	BIGNUM *cofactor, *x;
	const BIGNUM *priv_key;
	const EC_GROUP *group;
	EC_POINT *point = NULL;
	unsigned char *buf = NULL;
	int buflen;
	int ret = -1;

	if (outlen > INT_MAX) {
		/* Sort of, anyway. */
		ECerror(ERR_R_MALLOC_FAILURE);
		return -1;
	}

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	BN_CTX_start(ctx);

	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((cofactor = BN_CTX_get(ctx)) == NULL)
		goto err;

	if ((group = EC_KEY_get0_group(ecdh)) == NULL)
		goto err;

	if (!EC_POINT_is_on_curve(group, pub_key, ctx))
		goto err;

	if ((point = EC_POINT_new(group)) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if ((priv_key = EC_KEY_get0_private_key(ecdh)) == NULL) {
		ECerror(EC_R_MISSING_PRIVATE_KEY);
		goto err;
	}

	if ((EC_KEY_get_flags(ecdh) & EC_FLAG_COFACTOR_ECDH) != 0) {
		if (!EC_GROUP_get_cofactor(group, cofactor, NULL)) {
			ECerror(ERR_R_EC_LIB);
			goto err;
		}
		if (!BN_mul(cofactor, cofactor, priv_key, ctx)) {
			ECerror(ERR_R_BN_LIB);
			goto err;
		}
		priv_key = cofactor;
	}

	if (!EC_POINT_mul(group, point, NULL, pub_key, priv_key, ctx)) {
		ECerror(EC_R_POINT_ARITHMETIC_FAILURE);
		goto err;
	}

	if (!EC_POINT_get_affine_coordinates(group, point, x, NULL, ctx)) {
		ECerror(EC_R_POINT_ARITHMETIC_FAILURE);
		goto err;
	}

	if ((buflen = ECDH_size(ecdh)) < BN_num_bytes(x)) {
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}
	if (KDF == NULL && outlen < buflen) {
		/* The resulting key would be truncated. */
		ECerror(EC_R_KEY_TRUNCATION);
		goto err;
	}
	if ((buf = malloc(buflen)) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (BN_bn2binpad(x, buf, buflen) != buflen) {
		ECerror(ERR_R_BN_LIB);
		goto err;
	}

	if (KDF != NULL) {
		if (KDF(buf, buflen, out, &outlen) == NULL) {
			ECerror(EC_R_KDF_FAILED);
			goto err;
		}
	} else {
		memset(out, 0, outlen);
		if (outlen > buflen)
			outlen = buflen;
		memcpy(out, buf, outlen);
	}

	ret = outlen;
 err:
	EC_POINT_free(point);
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	free(buf);

	return ret;
}

int
ECDH_compute_key(void *out, size_t outlen, const EC_POINT *pub_key,
    EC_KEY *eckey,
    void *(*KDF)(const void *in, size_t inlen, void *out, size_t *outlen))
{
	if (eckey->meth->compute_key == NULL) {
		ECerror(EC_R_NOT_IMPLEMENTED);
		return 0;
	}
	return eckey->meth->compute_key(out, outlen, pub_key, eckey, KDF);
}

int
ECDH_size(const EC_KEY *d)
{
	return (EC_GROUP_get_degree(EC_KEY_get0_group(d)) + 7) / 8;
}
