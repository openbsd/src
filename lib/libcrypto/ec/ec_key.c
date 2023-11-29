/* $OpenBSD: ec_key.c,v 1.39 2023/11/29 21:35:57 tb Exp $ */
/*
 * Written by Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * Portions originally developed by SUN MICROSYSTEMS, INC., and
 * contributed to the OpenSSL project.
 */

#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/err.h>

#include "bn_local.h"
#include "ec_local.h"

EC_KEY *
EC_KEY_new(void)
{
	return EC_KEY_new_method(NULL);
}
LCRYPTO_ALIAS(EC_KEY_new);

EC_KEY *
EC_KEY_new_by_curve_name(int nid)
{
	EC_KEY *ret = EC_KEY_new();
	if (ret == NULL)
		return NULL;
	ret->group = EC_GROUP_new_by_curve_name(nid);
	if (ret->group == NULL) {
		EC_KEY_free(ret);
		return NULL;
	}
	if (ret->meth->set_group != NULL &&
	    ret->meth->set_group(ret, ret->group) == 0) {
		EC_KEY_free(ret);
		return NULL;
	}
	return ret;
}
LCRYPTO_ALIAS(EC_KEY_new_by_curve_name);

void
EC_KEY_free(EC_KEY *r)
{
	int i;

	if (r == NULL)
		return;

	i = CRYPTO_add(&r->references, -1, CRYPTO_LOCK_EC);
	if (i > 0)
		return;

	if (r->meth != NULL && r->meth->finish != NULL)
		r->meth->finish(r);

	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_EC_KEY, r, &r->ex_data);

	EC_GROUP_free(r->group);
	EC_POINT_free(r->pub_key);
	BN_free(r->priv_key);

	freezero(r, sizeof(EC_KEY));
}
LCRYPTO_ALIAS(EC_KEY_free);

EC_KEY *
EC_KEY_copy(EC_KEY *dest, const EC_KEY *src)
{
	if (dest == NULL || src == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
	}
	if (src->meth != dest->meth) {
		if (dest->meth != NULL && dest->meth->finish != NULL)
			dest->meth->finish(dest);
	}
	/* copy the parameters */
	if (src->group) {
		const EC_METHOD *meth = EC_GROUP_method_of(src->group);
		/* clear the old group */
		EC_GROUP_free(dest->group);
		dest->group = EC_GROUP_new(meth);
		if (dest->group == NULL)
			return NULL;
		if (!EC_GROUP_copy(dest->group, src->group))
			return NULL;
	}
	/* copy the public key */
	if (src->pub_key && src->group) {
		EC_POINT_free(dest->pub_key);
		dest->pub_key = EC_POINT_new(src->group);
		if (dest->pub_key == NULL)
			return NULL;
		if (!EC_POINT_copy(dest->pub_key, src->pub_key))
			return NULL;
	}
	/* copy the private key */
	if (src->priv_key) {
		if (dest->priv_key == NULL) {
			dest->priv_key = BN_new();
			if (dest->priv_key == NULL)
				return NULL;
		}
		if (!bn_copy(dest->priv_key, src->priv_key))
			return NULL;
	}

	/* copy the rest */
	dest->enc_flag = src->enc_flag;
	dest->conv_form = src->conv_form;
	dest->version = src->version;
	dest->flags = src->flags;

	if (!CRYPTO_dup_ex_data(CRYPTO_EX_INDEX_EC_KEY, &dest->ex_data,
	    &((EC_KEY *)src)->ex_data))	/* XXX const */
		return NULL;

	if (src->meth != dest->meth) {
		dest->meth = src->meth;
	}

	if (src->meth != NULL && src->meth->copy != NULL &&
	    src->meth->copy(dest, src) == 0)
		return 0;

	return dest;
}
LCRYPTO_ALIAS(EC_KEY_copy);

EC_KEY *
EC_KEY_dup(const EC_KEY *ec_key)
{
	EC_KEY *ret;

	if ((ret = EC_KEY_new_method(NULL)) == NULL)
		return NULL;
	if (EC_KEY_copy(ret, ec_key) == NULL) {
		EC_KEY_free(ret);
		return NULL;
	}
	return ret;
}
LCRYPTO_ALIAS(EC_KEY_dup);

int
EC_KEY_up_ref(EC_KEY *r)
{
	int i = CRYPTO_add(&r->references, 1, CRYPTO_LOCK_EC);
	return ((i > 1) ? 1 : 0);
}
LCRYPTO_ALIAS(EC_KEY_up_ref);

int
EC_KEY_set_ex_data(EC_KEY *r, int idx, void *arg)
{
	return CRYPTO_set_ex_data(&r->ex_data, idx, arg);
}
LCRYPTO_ALIAS(EC_KEY_set_ex_data);

void *
EC_KEY_get_ex_data(const EC_KEY *r, int idx)
{
	return CRYPTO_get_ex_data(&r->ex_data, idx);
}
LCRYPTO_ALIAS(EC_KEY_get_ex_data);

int
EC_KEY_generate_key(EC_KEY *eckey)
{
	if (eckey->meth->keygen != NULL)
		return eckey->meth->keygen(eckey);
	ECerror(EC_R_NOT_IMPLEMENTED);
	return 0;
}
LCRYPTO_ALIAS(EC_KEY_generate_key);

int
ec_key_gen(EC_KEY *eckey)
{
	BIGNUM *priv_key = NULL;
	EC_POINT *pub_key = NULL;
	const BIGNUM *order;
	int ret = 0;

	if (eckey == NULL || eckey->group == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if ((priv_key = BN_new()) == NULL)
		goto err;
	if ((pub_key = EC_POINT_new(eckey->group)) == NULL)
		goto err;

	if ((order = EC_GROUP_get0_order(eckey->group)) == NULL)
		goto err;
	if (!bn_rand_interval(priv_key, 1, order))
		goto err;
	if (!EC_POINT_mul(eckey->group, pub_key, priv_key, NULL, NULL, NULL))
		goto err;

	BN_free(eckey->priv_key);
	eckey->priv_key = priv_key;
	priv_key = NULL;

	EC_POINT_free(eckey->pub_key);
	eckey->pub_key = pub_key;
	pub_key = NULL;

	ret = 1;

 err:
	EC_POINT_free(pub_key);
	BN_free(priv_key);

	return ret;
}

int
EC_KEY_check_key(const EC_KEY *eckey)
{
	BN_CTX *ctx = NULL;
	EC_POINT *point = NULL;
	const BIGNUM *order;
	int ret = 0;

	if (eckey == NULL || eckey->group == NULL || eckey->pub_key == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if (EC_POINT_is_at_infinity(eckey->group, eckey->pub_key) > 0) {
		ECerror(EC_R_POINT_AT_INFINITY);
		goto err;
	}

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	if ((point = EC_POINT_new(eckey->group)) == NULL)
		goto err;

	/* Ensure public key is on the elliptic curve. */
	if (EC_POINT_is_on_curve(eckey->group, eckey->pub_key, ctx) <= 0) {
		ECerror(EC_R_POINT_IS_NOT_ON_CURVE);
		goto err;
	}

	/* Ensure public key multiplied by the order is the point at infinity. */
	if ((order = EC_GROUP_get0_order(eckey->group)) == NULL) {
		ECerror(EC_R_INVALID_GROUP_ORDER);
		goto err;
	}
	if (!EC_POINT_mul(eckey->group, point, NULL, eckey->pub_key, order, ctx)) {
		ECerror(ERR_R_EC_LIB);
		goto err;
	}
	if (EC_POINT_is_at_infinity(eckey->group, point) <= 0) {
		ECerror(EC_R_WRONG_ORDER);
		goto err;
	}

	/*
	 * If the private key is present, ensure that the private key multiplied
	 * by the generator matches the public key.
	 */
	if (eckey->priv_key != NULL) {
		if (BN_cmp(eckey->priv_key, order) >= 0) {
			ECerror(EC_R_WRONG_ORDER);
			goto err;
		}
		if (!EC_POINT_mul(eckey->group, point, eckey->priv_key, NULL,
		    NULL, ctx)) {
			ECerror(ERR_R_EC_LIB);
			goto err;
		}
		if (EC_POINT_cmp(eckey->group, point, eckey->pub_key,
		    ctx) != 0) {
			ECerror(EC_R_INVALID_PRIVATE_KEY);
			goto err;
		}
	}

	ret = 1;

 err:
	BN_CTX_free(ctx);
	EC_POINT_free(point);

	return ret;
}
LCRYPTO_ALIAS(EC_KEY_check_key);

int
EC_KEY_set_public_key_affine_coordinates(EC_KEY *key, BIGNUM *x, BIGNUM *y)
{
	BN_CTX *ctx = NULL;
	EC_POINT *point = NULL;
	BIGNUM *tx, *ty;
	int ret = 0;

	if (key == NULL || key->group == NULL || x == NULL || y == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	BN_CTX_start(ctx);

	if ((tx = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((ty = BN_CTX_get(ctx)) == NULL)
		goto err;

	if ((point = EC_POINT_new(key->group)) == NULL)
		goto err;

	if (!EC_POINT_set_affine_coordinates(key->group, point, x, y, ctx))
		goto err;
	if (!EC_POINT_get_affine_coordinates(key->group, point, tx, ty, ctx))
		goto err;

	/*
	 * Check if retrieved coordinates match originals: if not values are
	 * out of range.
	 */
	if (BN_cmp(x, tx) != 0 || BN_cmp(y, ty) != 0) {
		ECerror(EC_R_COORDINATES_OUT_OF_RANGE);
		goto err;
	}
	if (!EC_KEY_set_public_key(key, point))
		goto err;
	if (EC_KEY_check_key(key) == 0)
		goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	EC_POINT_free(point);

	return ret;
}
LCRYPTO_ALIAS(EC_KEY_set_public_key_affine_coordinates);

const EC_GROUP *
EC_KEY_get0_group(const EC_KEY *key)
{
	return key->group;
}
LCRYPTO_ALIAS(EC_KEY_get0_group);

int
EC_KEY_set_group(EC_KEY *key, const EC_GROUP *group)
{
	if (key->meth->set_group != NULL &&
	    key->meth->set_group(key, group) == 0)
		return 0;
	EC_GROUP_free(key->group);
	key->group = EC_GROUP_dup(group);
	return (key->group == NULL) ? 0 : 1;
}
LCRYPTO_ALIAS(EC_KEY_set_group);

const BIGNUM *
EC_KEY_get0_private_key(const EC_KEY *key)
{
	return key->priv_key;
}
LCRYPTO_ALIAS(EC_KEY_get0_private_key);

int
EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *priv_key)
{
	if (key->meth->set_private != NULL &&
	    key->meth->set_private(key, priv_key) == 0)
		return 0;

	BN_free(key->priv_key);
	if ((key->priv_key = BN_dup(priv_key)) == NULL)
		return 0;

	return 1;
}
LCRYPTO_ALIAS(EC_KEY_set_private_key);

const EC_POINT *
EC_KEY_get0_public_key(const EC_KEY *key)
{
	return key->pub_key;
}
LCRYPTO_ALIAS(EC_KEY_get0_public_key);

int
EC_KEY_set_public_key(EC_KEY *key, const EC_POINT *pub_key)
{
	if (key->meth->set_public != NULL &&
	    key->meth->set_public(key, pub_key) == 0)
		return 0;

	EC_POINT_free(key->pub_key);
	if ((key->pub_key = EC_POINT_dup(pub_key, key->group)) == NULL)
		return 0;

	return 1;
}
LCRYPTO_ALIAS(EC_KEY_set_public_key);

unsigned int
EC_KEY_get_enc_flags(const EC_KEY *key)
{
	return key->enc_flag;
}
LCRYPTO_ALIAS(EC_KEY_get_enc_flags);

void
EC_KEY_set_enc_flags(EC_KEY *key, unsigned int flags)
{
	key->enc_flag = flags;
}
LCRYPTO_ALIAS(EC_KEY_set_enc_flags);

point_conversion_form_t
EC_KEY_get_conv_form(const EC_KEY *key)
{
	return key->conv_form;
}
LCRYPTO_ALIAS(EC_KEY_get_conv_form);

void
EC_KEY_set_conv_form(EC_KEY *key, point_conversion_form_t cform)
{
	key->conv_form = cform;
	if (key->group != NULL)
		EC_GROUP_set_point_conversion_form(key->group, cform);
}
LCRYPTO_ALIAS(EC_KEY_set_conv_form);

void
EC_KEY_set_asn1_flag(EC_KEY *key, int flag)
{
	if (key->group != NULL)
		EC_GROUP_set_asn1_flag(key->group, flag);
}
LCRYPTO_ALIAS(EC_KEY_set_asn1_flag);

int
EC_KEY_precompute_mult(EC_KEY *key, BN_CTX *ctx)
{
	if (key->group == NULL)
		return 0;
	return EC_GROUP_precompute_mult(key->group, ctx);
}
LCRYPTO_ALIAS(EC_KEY_precompute_mult);

int
EC_KEY_get_flags(const EC_KEY *key)
{
	return key->flags;
}
LCRYPTO_ALIAS(EC_KEY_get_flags);

void
EC_KEY_set_flags(EC_KEY *key, int flags)
{
	key->flags |= flags;
}
LCRYPTO_ALIAS(EC_KEY_set_flags);

void
EC_KEY_clear_flags(EC_KEY *key, int flags)
{
	key->flags &= ~flags;
}
LCRYPTO_ALIAS(EC_KEY_clear_flags);
