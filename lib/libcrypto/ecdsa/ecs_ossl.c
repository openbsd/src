/* $OpenBSD: ecs_ossl.c,v 1.24 2022/04/07 17:37:25 tb Exp $ */
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

#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>

#include "bn_lcl.h"
#include "ecs_locl.h"

static int ecdsa_prepare_digest(const unsigned char *dgst, int dgst_len,
    BIGNUM *order, BIGNUM *ret);
static ECDSA_SIG *ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
    const BIGNUM *, const BIGNUM *, EC_KEY *eckey);
static int ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
    BIGNUM **rp);
static int ecdsa_do_verify(const unsigned char *dgst, int dgst_len,
    const ECDSA_SIG *sig, EC_KEY *eckey);

static ECDSA_METHOD openssl_ecdsa_meth = {
	.name = "OpenSSL ECDSA method",
	.ecdsa_do_sign = ecdsa_do_sign,
	.ecdsa_sign_setup = ecdsa_sign_setup,
	.ecdsa_do_verify = ecdsa_do_verify
};

const ECDSA_METHOD *
ECDSA_OpenSSL(void)
{
	return &openssl_ecdsa_meth;
}

static int
ecdsa_prepare_digest(const unsigned char *dgst, int dgst_len, BIGNUM *order,
    BIGNUM *ret)
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

	if ((s = ECDSA_do_sign_ex(dgst, dlen, kinv, r, eckey)) == NULL) {
		*siglen = 0;
		return 0;
	}
	*siglen = i2d_ECDSA_SIG(s, &sig);
	ECDSA_SIG_free(s);
	return 1;
}

static int
ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	BN_CTX *ctx = ctx_in;
	BIGNUM *k = NULL, *r = NULL, *order = NULL, *X = NULL;
	EC_POINT *point = NULL;
	const EC_GROUP *group;
	int order_bits, ret = 0;

	if (eckey == NULL || (group = EC_KEY_get0_group(eckey)) == NULL) {
		ECDSAerror(ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	if (ctx == NULL) {
		if ((ctx = BN_CTX_new()) == NULL) {
			ECDSAerror(ERR_R_MALLOC_FAILURE);
			return 0;
		}
	}

	if ((k = BN_new()) == NULL || (r = BN_new()) == NULL ||
	    (order = BN_new()) == NULL || (X = BN_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if ((point = EC_POINT_new(group)) == NULL) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}
	if (!EC_GROUP_get_order(group, order, ctx)) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}

	if (BN_cmp(order, BN_value_one()) <= 0) {
		ECDSAerror(EC_R_INVALID_GROUP_ORDER);
		goto err;
	}

	/* Preallocate space. */
	order_bits = BN_num_bits(order);
	if (!BN_set_bit(k, order_bits) ||
	    !BN_set_bit(r, order_bits) ||
	    !BN_set_bit(X, order_bits))
		goto err;

	do {
		do {
			if (!BN_rand_range(k, order)) {
				ECDSAerror(
				    ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED);
				goto err;
			}
		} while (BN_is_zero(k));

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
		 * TODO: revisit the BN_copy aiming for a memory access agnostic
		 * conditional copy.
		 */
		if (!BN_add(r, k, order) ||
		    !BN_add(X, r, order) ||
		    !BN_copy(k, BN_num_bits(r) > order_bits ? r : X))
			goto err;

		BN_set_flags(k, BN_FLG_CONSTTIME);

		/* Compute r, the x-coordinate of G * k. */
		if (!EC_POINT_mul(group, point, k, NULL, NULL, ctx)) {
			ECDSAerror(ERR_R_EC_LIB);
			goto err;
		}
		if (!EC_POINT_get_affine_coordinates(group, point, X, NULL,
		    ctx)) {
			ECDSAerror(ERR_R_EC_LIB);
			goto err;
		}
		if (!BN_nnmod(r, X, order, ctx)) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
	} while (BN_is_zero(r));

	if (BN_mod_inverse_ct(k, k, order, ctx) == NULL) {
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}
	BN_clear_free(*rp);
	BN_clear_free(*kinvp);
	*rp = r;
	*kinvp = k;
	ret = 1;

 err:
	if (ret == 0) {
		BN_clear_free(k);
		BN_clear_free(r);
	}
	if (ctx_in == NULL)
		BN_CTX_free(ctx);
	BN_free(order);
	EC_POINT_free(point);
	BN_clear_free(X);
	return (ret);
}

/* replace w/ ecdsa_sign_setup() when ECDSA_METHOD gets removed */
int
ossl_ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	ECDSA_DATA *ecdsa;

	if ((ecdsa = ecdsa_check(eckey)) == NULL)
		return 0;
	return ecdsa->meth->ecdsa_sign_setup(eckey, ctx_in, kinvp, rp);
}

static ECDSA_SIG *
ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
    const BIGNUM *in_kinv, const BIGNUM *in_r, EC_KEY *eckey)
{
	BIGNUM *b = NULL, *binv = NULL, *bm = NULL, *bxr = NULL;
	BIGNUM *kinv = NULL, *m = NULL, *order = NULL, *range = NULL, *s;
	const BIGNUM *ckinv, *priv_key;
	BN_CTX *ctx = NULL;
	const EC_GROUP *group;
	ECDSA_SIG  *ret;
	ECDSA_DATA *ecdsa;
	int ok = 0;

	ecdsa = ecdsa_check(eckey);
	group = EC_KEY_get0_group(eckey);
	priv_key = EC_KEY_get0_private_key(eckey);

	if (group == NULL || priv_key == NULL || ecdsa == NULL) {
		ECDSAerror(ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
	}

	if ((ret = ECDSA_SIG_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	s = ret->s;

	if ((ctx = BN_CTX_new()) == NULL || (order = BN_new()) == NULL ||
	    (range = BN_new()) == NULL || (b = BN_new()) == NULL ||
	    (binv = BN_new()) == NULL || (bm = BN_new()) == NULL ||
	    (bxr = BN_new()) == NULL || (m = BN_new()) == NULL) {
		ECDSAerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (!EC_GROUP_get_order(group, order, ctx)) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}

	if (!ecdsa_prepare_digest(dgst, dgst_len, order, m))
		goto err;

	do {
		if (in_kinv == NULL || in_r == NULL) {
			if (!ECDSA_sign_setup(eckey, ctx, &kinv, &ret->r)) {
				ECDSAerror(ERR_R_ECDSA_LIB);
				goto err;
			}
			ckinv = kinv;
		} else {
			ckinv = in_kinv;
			if (BN_copy(ret->r, in_r) == NULL) {
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
		 * where b is a random value in the range [1, order-1].
		 */

		/* Generate b in range [1, order-1]. */
		if (!BN_sub(range, order, BN_value_one())) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_rand_range(b, range)) {
			ECDSAerror(ERR_R_BN_LIB);
			goto err;
		}
		if (!BN_add(b, b, BN_value_one())) {
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
		if (!BN_mod_mul(bxr, bxr, ret->r, order, ctx)) { /* bxr */
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
		} else
			/* s != 0 => we have a valid signature */
			break;
	} while (1);

	ok = 1;

 err:
	if (ok == 0) {
		ECDSA_SIG_free(ret);
		ret = NULL;
	}
	BN_CTX_free(ctx);
	BN_clear_free(b);
	BN_clear_free(binv);
	BN_clear_free(bm);
	BN_clear_free(bxr);
	BN_clear_free(kinv);
	BN_clear_free(m);
	BN_free(order);
	BN_free(range);
	return ret;
}

/* replace w/ ecdsa_do_sign() when ECDSA_METHOD gets removed */
ECDSA_SIG *
ossl_ecdsa_sign_sig(const unsigned char *dgst, int dgst_len,
    const BIGNUM *in_kinv, const BIGNUM *in_r, EC_KEY *eckey)
{
	ECDSA_DATA *ecdsa;

	if ((ecdsa = ecdsa_check(eckey)) == NULL)
		return NULL;
	return ecdsa->meth->ecdsa_do_sign(dgst, dgst_len, in_kinv, in_r, eckey);
}

int
ossl_ecdsa_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int sig_len, EC_KEY *eckey)
{
	ECDSA_SIG *s;
	unsigned char *der = NULL;
	const unsigned char *p = sigbuf;
	int derlen = -1;
	int ret = -1;

	if ((s = ECDSA_SIG_new()) == NULL)
		return (ret);
	if (d2i_ECDSA_SIG(&s, &p, sig_len) == NULL)
		goto err;
	/* Ensure signature uses DER and doesn't have trailing garbage */
	derlen = i2d_ECDSA_SIG(s, &der);
	if (derlen != sig_len || memcmp(sigbuf, der, derlen))
		goto err;
	ret = ECDSA_do_verify(dgst, dgst_len, s, eckey);

 err:
	freezero(der, derlen);
	ECDSA_SIG_free(s);
	return (ret);
}

static int
ecdsa_do_verify(const unsigned char *dgst, int dgst_len, const ECDSA_SIG *sig,
    EC_KEY *eckey)
{
	BN_CTX *ctx;
	BIGNUM *order, *u1, *u2, *m, *X;
	EC_POINT *point = NULL;
	const EC_GROUP *group;
	const EC_POINT *pub_key;
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
	order = BN_CTX_get(ctx);
	u1 = BN_CTX_get(ctx);
	u2 = BN_CTX_get(ctx);
	m = BN_CTX_get(ctx);
	X = BN_CTX_get(ctx);
	if (X == NULL) {
		ECDSAerror(ERR_R_BN_LIB);
		goto err;
	}

	if (!EC_GROUP_get_order(group, order, ctx)) {
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
	if (!EC_POINT_get_affine_coordinates(group, point, X, NULL, ctx)) {
		ECDSAerror(ERR_R_EC_LIB);
		goto err;
	}
	if (!BN_nnmod(u1, X, order, ctx)) {
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

/* replace w/ ecdsa_do_verify() when ECDSA_METHOD gets removed */
int
ossl_ecdsa_verify_sig(const unsigned char *dgst, int dgst_len,
    const ECDSA_SIG *sig, EC_KEY *eckey)
{
	ECDSA_DATA *ecdsa;

	if ((ecdsa = ecdsa_check(eckey)) == NULL)
		return 0;
	return ecdsa->meth->ecdsa_do_verify(dgst, dgst_len, sig, eckey);
}
