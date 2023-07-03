/* $OpenBSD: ecs_ossl.c,v 1.49 2023/07/03 09:55:42 tb Exp $ */
/*
 * Written by Nils Larsch for the OpenSSL project
 */
/* ====================================================================
 * Copyright (c) 1998-2004 The OpenSSL Project.  All rights reserved.
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

#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

#include "bn_local.h"
#include "ec_local.h"
#include "ecs_local.h"

static int ecdsa_prepare_digest(const unsigned char *dgst, int dgst_len,
    const BIGNUM *order, BIGNUM *ret);

static int
ecdsa_prepare_digest(const unsigned char *dgst, int dgst_len,
    const BIGNUM *order, BIGNUM *ret)
{
	int dgst_bits, order_bits;

	if (!BN_bin2bn(dgst, dgst_len, ret)) {
		ECDSAerror(ERR_R_BN_LIB);
		return 0;
	}

	/* FIPS 186-3 6.4: Use order_bits leftmost bits if digest is too long */
	dgst_bits = 8 * dgst_len;
	order_bits = BN_num_bits(order);
	if (dgst_bits > order_bits) {
		if (!BN_rshift(ret, ret, dgst_bits - order_bits)) {
			ECDSAerror(ERR_R_BN_LIB);
			return 0;
		}
	}

	return 1;
}

int
ossl_ecdsa_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey)
{
	ECDSA_SIG *s;
	int outlen = 0;
	int ret = 0;

	if ((s = ECDSA_do_sign_ex(dgst, dlen, kinv, r, eckey)) == NULL)
		goto err;

	if ((outlen = i2d_ECDSA_SIG(s, &sig)) < 0) {
		outlen = 0;
		goto err;
	}

	ret = 1;

 err:
	*siglen = outlen;
	ECDSA_SIG_free(s);

	return ret;
}

int
ossl_ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *in_ctx, BIGNUM **out_kinv,
    BIGNUM **out_r)
{
	const EC_GROUP *group;
	EC_POINT *point = NULL;
	BN_CTX *ctx = NULL;
	BIGNUM *k = NULL, *r = NULL;
	const BIGNUM *order;
	BIGNUM *x;
	int order_bits;
	int ret = 0;

	BN_free(*out_kinv);
	*out_kinv = NULL;

	BN_free(*out_r);
	*out_r = NULL;

	if (eckey == NULL || (group = EC_KEY_get0_group(eckey)) == NULL) {
		ECDSAerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if ((k = BN_new()) == NULL)
		goto err;
	if ((r = BN_new()) == NULL)
		goto err;

	if ((ctx = in_ctx) == NULL)
		ctx = BN_CTX_new();
	if (ctx == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	BN_CTX_start(ctx);

	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;

	if ((point = EC_POINT_new(group)) == NULL) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}
	if ((order = EC_GROUP_get0_order(group)) == NULL) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}

	if (BN_cmp(order, BN_value_one()) <= 0) {
		ECDSAerror(EC_R_INVALID_GROUP_ORDER);
		goto err;
	}

	/* Reject curves with an order that is smaller than 80 bits. */
	if ((order_bits = BN_num_bits(order)) < 80) {
		ECDSAerror(EC_R_INVALID_GROUP_ORDER);
		goto err;
	}

	/* Preallocate space. */
	if (!BN_set_bit(k, order_bits) ||
	    !BN_set_bit(r, order_bits) ||
	    !BN_set_bit(x, order_bits))
		goto err;

	do {
		if (!bn_rand_interval(k, BN_value_one(), order)) {
			ECDSAerror(ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED);
			goto err;
		}

		/*
		 * We do not want timing information to leak the length of k,
		 * so we compute G * k using an equivalent scalar of fixed
		 * bit-length.
		 *
		 * We unconditionally perform both of these additions to prevent
		 * a small timing information leakage.  We then choose the sum
		 * that is one bit longer than the order.  This guarantees the
		 * code path used in the constant time implementations
		 * elsewhere.
		 *
		 * TODO: revisit the bn_copy aiming for a memory access agnostic
		 * conditional copy.
		 */
		if (!BN_add(r, k, order) ||
		    !BN_add(x, r, order) ||
		    !bn_copy(k, BN_num_bits(r) > order_bits ? r : x))
			goto err;

		BN_set_flags(k, BN_FLG_CONSTTIME);

		/* Compute r, the x-coordinate of G * k. */
		if (!EC_POINT_mul(group, point, k, NULL, NULL, ctx)) {
			ECDSAerror(ERR_R_EC_LIB);
			goto err;
		}
		if (!EC_POINT_get_affine_coordinates(group, point, x, NULL,
		    ctx)) {
			ECDSAerror(ERR_R_EC_LIB);
			goto err;
		}
		if (!BN_nnmod(r, x, order, ctx)) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
	} while (BN_is_zero(r));

	if (BN_mod_inverse_ct(k, k, order, ctx) == NULL) {
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}

	*out_kinv = k;
	k = NULL;

	*out_r = r;
	r = NULL;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	if (ctx != in_ctx)
		BN_CTX_free(ctx);
	BN_free(k);
	BN_free(r);
	EC_POINT_free(point);

	return ret;
}

/*
 * It is too expensive to check curve parameters on every sign operation.
 * Instead, cap the number of retries. A single retry is very unlikely, so
 * allowing 32 retries is amply enough.
 */
#define ECDSA_MAX_SIGN_ITERATIONS		32

ECDSA_SIG *
ossl_ecdsa_sign_sig(const unsigned char *dgst, int dgst_len,
    const BIGNUM *in_kinv, const BIGNUM *in_r, EC_KEY *eckey)
{
	const EC_GROUP *group;
	BN_CTX *ctx = NULL;
	BIGNUM *kinv = NULL, *r = NULL, *s = NULL;
	BIGNUM *b, *binv, *bm, *bxr, *m;
	const BIGNUM *ckinv, *order, *priv_key;
	int attempts = 0;
	ECDSA_SIG *sig = NULL;

	group = EC_KEY_get0_group(eckey);
	priv_key = EC_KEY_get0_private_key(eckey);

	if (group == NULL || priv_key == NULL) {
		ECDSAerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if ((r = BN_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if ((s = BN_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if ((ctx = BN_CTX_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	BN_CTX_start(ctx);

	if ((b = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((binv = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((bm = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((bxr = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((m = BN_CTX_get(ctx)) == NULL)
		goto err;

	if ((order = EC_GROUP_get0_order(group)) == NULL) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}

	if (!ecdsa_prepare_digest(dgst, dgst_len, order, m))
		goto err;

	do {
		if (in_kinv == NULL || in_r == NULL) {
			if (!ECDSA_sign_setup(eckey, ctx, &kinv, &r)) {
				ECDSAerror(ERR_R_ECDSA_LIB);
				goto err;
			}
			ckinv = kinv;
		} else {
			ckinv = in_kinv;
			if (!bn_copy(r, in_r)) {
				ECDSAerror(ERR_R_MALLOC_FAILURE);
				goto err;
			}
		}

		/*
		 * Compute:
		 *
		 *  s = inv(k)(m + xr) mod order
		 *
		 * In order to reduce the possibility of a side-channel attack,
		 * the following is calculated using a blinding value:
		 *
		 *  s = inv(b)(bm + bxr)inv(k) mod order
		 *
		 * where b is a random value in the range [1, order).
		 */

		if (!bn_rand_interval(b, BN_value_one(), order)) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}

		if (BN_mod_inverse_ct(binv, b, order, ctx) == NULL) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}

		if (!BN_mod_mul(bxr, b, priv_key, order, ctx)) { /* bx */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_mod_mul(bxr, bxr, r, order, ctx)) { /* bxr */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_mod_mul(bm, b, m, order, ctx)) { /* bm */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_mod_add(s, bm, bxr, order, ctx)) { /* s = bm + bxr */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_mod_mul(s, s, ckinv, order, ctx)) { /* s = b(m + xr)k^-1 */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_mod_mul(s, s, binv, order, ctx)) { /* s = (m + xr)k^-1 */
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}

		if (BN_is_zero(s)) {
			/*
			 * If kinv and r have been supplied by the caller,
			 * don't generate new kinv and r values
			 */
			if (in_kinv != NULL && in_r != NULL) {
				ECDSAerror(ECDSA_R_NEED_NEW_SETUP_VALUES);
				goto err;
			}

			if (++attempts > ECDSA_MAX_SIGN_ITERATIONS) {
				ECDSAerror(EC_R_WRONG_CURVE_PARAMETERS);
				goto err;
			}
		} else
			/* s != 0 => we have a valid signature */
			break;
	} while (1);

	if ((sig = ECDSA_SIG_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!ECDSA_SIG_set0(sig, r, s)) {
		ECDSA_SIG_free(sig);
		goto err;
	}
	r = NULL;
	s = NULL;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	BN_free(kinv);
	BN_free(r);
	BN_free(s);

	return sig;
}

int
ossl_ecdsa_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int sig_len, EC_KEY *eckey)
{
	ECDSA_SIG *s;
	unsigned char *der = NULL;
	const unsigned char *p;
	int derlen = 0;
	int ret = -1;

	if ((s = ECDSA_SIG_new()) == NULL)
		goto err;

	p = sigbuf;
	if (d2i_ECDSA_SIG(&s, &p, sig_len) == NULL)
		goto err;

	/* Ensure signature uses DER and doesn't have trailing garbage */
	if ((derlen = i2d_ECDSA_SIG(s, &der)) != sig_len)
		goto err;
	if (memcmp(sigbuf, der, derlen))
		goto err;

	ret = ECDSA_do_verify(dgst, dgst_len, s, eckey);

 err:
	freezero(der, derlen);
	ECDSA_SIG_free(s);

	return ret;
}

int
ossl_ecdsa_verify_sig(const unsigned char *dgst, int dgst_len, const ECDSA_SIG *sig,
    EC_KEY *eckey)
{
	BN_CTX *ctx;
	BIGNUM *u1, *u2, *m, *x;
	EC_POINT *point = NULL;
	const EC_GROUP *group;
	const EC_POINT *pub_key;
	const BIGNUM *order;
	int ret = -1;

	if (eckey == NULL || (group = EC_KEY_get0_group(eckey)) == NULL ||
	    (pub_key = EC_KEY_get0_public_key(eckey)) == NULL || sig == NULL) {
		ECDSAerror(ECDSA_R_MISSING_PARAMETERS);
		return -1;
	}

	if ((ctx = BN_CTX_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		return -1;
	}
	BN_CTX_start(ctx);
	u1 = BN_CTX_get(ctx);
	u2 = BN_CTX_get(ctx);
	m = BN_CTX_get(ctx);
	x = BN_CTX_get(ctx);
	if (x == NULL) {
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}

	if ((order = EC_GROUP_get0_order(group)) == NULL) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}

	/* Verify that r and s are in the range [1, order-1]. */
	if (BN_is_zero(sig->r) || BN_is_negative(sig->r) ||
	    BN_ucmp(sig->r, order) >= 0 ||
	    BN_is_zero(sig->s) || BN_is_negative(sig->s) ||
	    BN_ucmp(sig->s, order) >= 0) {
		ECDSAerror(ECDSA_R_BAD_SIGNATURE);
		ret = 0;
		goto err;
	}

	if (!ecdsa_prepare_digest(dgst, dgst_len, order, m))
		goto err;

	if (BN_mod_inverse_ct(u2, sig->s, order, ctx) == NULL) { /* w = inv(s) */
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}
	if (!BN_mod_mul(u1, m, u2, order, ctx)) {		/* u1 = mw */
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}
	if (!BN_mod_mul(u2, sig->r, u2, order, ctx)) {		/* u2 = rw */
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}

	/* Compute the x-coordinate of G * u1 + pub_key * u2. */
	if ((point = EC_POINT_new(group)) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!EC_POINT_mul(group, point, u1, pub_key, u2, ctx)) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}
	if (!EC_POINT_get_affine_coordinates(group, point, x, NULL, ctx)) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}
	if (!BN_nnmod(u1, x, order, ctx)) {
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}

	/* If the signature is correct, the x-coordinate is equal to sig->r. */
	ret = (BN_ucmp(u1, sig->r) == 0);

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	EC_POINT_free(point);
	return ret;
}

ECDSA_SIG *
ECDSA_do_sign(const unsigned char *dgst, int dlen, EC_KEY *eckey)
{
	return ECDSA_do_sign_ex(dgst, dlen, NULL, NULL, eckey);
}

ECDSA_SIG *
ECDSA_do_sign_ex(const unsigned char *dgst, int dlen, const BIGNUM *kinv,
    const BIGNUM *out_r, EC_KEY *eckey)
{
	if (eckey->meth->sign_sig == NULL) {
		ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
		return 0;
	}
	return eckey->meth->sign_sig(dgst, dlen, kinv, out_r, eckey);
}

int
ECDSA_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, EC_KEY *eckey)
{
	return ECDSA_sign_ex(type, dgst, dlen, sig, siglen, NULL, NULL, eckey);
}

int
ECDSA_sign_ex(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey)
{
	if (eckey->meth->sign == NULL) {
		ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
		return 0;
	}
	return eckey->meth->sign(type, dgst, dlen, sig, siglen, kinv, r, eckey);
}

int
ECDSA_sign_setup(EC_KEY *eckey, BN_CTX *in_ctx, BIGNUM **out_kinv, BIGNUM **out_r)
{
	if (eckey->meth->sign_setup == NULL) {
		ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
		return 0;
	}
	return eckey->meth->sign_setup(eckey, in_ctx, out_kinv, out_r);
}

int
ECDSA_do_verify(const unsigned char *dgst, int dgst_len, const ECDSA_SIG *sig,
    EC_KEY *eckey)
{
	if (eckey->meth->verify_sig == NULL) {
		ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
		return 0;
	}
	return eckey->meth->verify_sig(dgst, dgst_len, sig, eckey);
}

int
ECDSA_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int sig_len, EC_KEY *eckey)
{
	if (eckey->meth->verify == NULL) {
		ECDSAerror(EVP_R_METHOD_NOT_SUPPORTED);
		return 0;
	}
	return eckey->meth->verify(type, dgst, dgst_len, sigbuf, sig_len, eckey);
}

int
ECDSA_size(const EC_KEY *r)
{
	const EC_GROUP *group;
	const BIGNUM *order = NULL;
	ECDSA_SIG signature;
	int ret = 0;

	if (r == NULL)
		goto err;

	if ((group = EC_KEY_get0_group(r)) == NULL)
		goto err;

	if ((order = EC_GROUP_get0_order(group)) == NULL)
		goto err;

	signature.r = (BIGNUM *)order;
	signature.s = (BIGNUM *)order;

	if ((ret = i2d_ECDSA_SIG(&signature, NULL)) < 0)
		ret = 0;

 err:
	return ret;
}
