/*	$OpenBSD: ec_kmeth.c,v 1.13 2023/11/19 15:46:09 tb Exp $	*/
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
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
 */

#include <openssl/ec.h>
#include <openssl/err.h>

#include "bn_local.h"
#include "ec_local.h"
#include "ecdsa_local.h"

static const EC_KEY_METHOD openssl_ec_key_method = {
	.name = "OpenSSL EC_KEY method",
	.flags = 0,

	.init = NULL,
	.finish = NULL,
	.copy = NULL,

	.set_group = NULL,
	.set_private = NULL,
	.set_public = NULL,

	.keygen = ec_key_gen,
	.compute_key = ecdh_compute_key,

	.sign = ecdsa_sign,
	.sign_setup = ecdsa_sign_setup,
	.sign_sig = ecdsa_sign_sig,

	.verify = ecdsa_verify,
	.verify_sig = ecdsa_verify_sig,
};

const EC_KEY_METHOD *default_ec_key_meth = &openssl_ec_key_method;

const EC_KEY_METHOD *
EC_KEY_OpenSSL(void)
{
	return &openssl_ec_key_method;
}
LCRYPTO_ALIAS(EC_KEY_OpenSSL);

const EC_KEY_METHOD *
EC_KEY_get_default_method(void)
{
	return default_ec_key_meth;
}
LCRYPTO_ALIAS(EC_KEY_get_default_method);

void
EC_KEY_set_default_method(const EC_KEY_METHOD *meth)
{
	if (meth == NULL)
		default_ec_key_meth = &openssl_ec_key_method;
	else
		default_ec_key_meth = meth;
}
LCRYPTO_ALIAS(EC_KEY_set_default_method);

const EC_KEY_METHOD *
EC_KEY_get_method(const EC_KEY *key)
{
	return key->meth;
}
LCRYPTO_ALIAS(EC_KEY_get_method);

int
EC_KEY_set_method(EC_KEY *key, const EC_KEY_METHOD *meth)
{
	void (*finish)(EC_KEY *key) = key->meth->finish;

	if (finish != NULL)
		finish(key);

	key->meth = meth;
	if (meth->init != NULL)
		return meth->init(key);
	return 1;
}
LCRYPTO_ALIAS(EC_KEY_set_method);

EC_KEY *
EC_KEY_new_method(ENGINE *engine)
{
	EC_KEY *ret;

	if ((ret = calloc(1, sizeof(EC_KEY))) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	ret->meth = EC_KEY_get_default_method();
	ret->version = 1;
	ret->flags = 0;
	ret->group = NULL;
	ret->pub_key = NULL;
	ret->priv_key = NULL;
	ret->enc_flag = 0;
	ret->conv_form = POINT_CONVERSION_UNCOMPRESSED;
	ret->references = 1;

	if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data))
		goto err;
	if (ret->meth->init != NULL && ret->meth->init(ret) == 0)
		goto err;

	return ret;

 err:
	EC_KEY_free(ret);
	return NULL;
}
LCRYPTO_ALIAS(EC_KEY_new_method);

EC_KEY_METHOD *
EC_KEY_METHOD_new(const EC_KEY_METHOD *meth)
{
	EC_KEY_METHOD *ret;

	if ((ret = calloc(1, sizeof(*meth))) == NULL)
		return NULL;
	if (meth != NULL)
		*ret = *meth;
	ret->flags |= EC_KEY_METHOD_DYNAMIC;
	return ret;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_new);

void
EC_KEY_METHOD_free(EC_KEY_METHOD *meth)
{
	if (meth == NULL)
		return;
	if (meth->flags & EC_KEY_METHOD_DYNAMIC)
		free(meth);
}
LCRYPTO_ALIAS(EC_KEY_METHOD_free);

void
EC_KEY_METHOD_set_init(EC_KEY_METHOD *meth,
    int (*init)(EC_KEY *key),
    void (*finish)(EC_KEY *key),
    int (*copy)(EC_KEY *dest, const EC_KEY *src),
    int (*set_group)(EC_KEY *key, const EC_GROUP *grp),
    int (*set_private)(EC_KEY *key, const BIGNUM *priv_key),
    int (*set_public)(EC_KEY *key, const EC_POINT *pub_key))
{
	meth->init = init;
	meth->finish = finish;
	meth->copy = copy;
	meth->set_group = set_group;
	meth->set_private = set_private;
	meth->set_public = set_public;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_set_init);

void
EC_KEY_METHOD_set_keygen(EC_KEY_METHOD *meth, int (*keygen)(EC_KEY *key))
{
	meth->keygen = keygen;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_set_keygen);

void
EC_KEY_METHOD_set_compute_key(EC_KEY_METHOD *meth,
    int (*ckey)(unsigned char **out, size_t *out_len, const EC_POINT *pub_key,
        const EC_KEY *ecdh))
{
	meth->compute_key = ckey;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_set_compute_key);

void
EC_KEY_METHOD_set_sign(EC_KEY_METHOD *meth,
    int (*sign)(int type, const unsigned char *dgst,
	int dlen, unsigned char *sig, unsigned int *siglen,
	const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey),
    int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
	BIGNUM **kinvp, BIGNUM **rp),
    ECDSA_SIG *(*sign_sig)(const unsigned char *dgst,
	int dgst_len, const BIGNUM *in_kinv,
	const BIGNUM *in_r, EC_KEY *eckey))
{
	meth->sign = sign;
	meth->sign_setup = sign_setup;
	meth->sign_sig = sign_sig;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_set_sign);

void
EC_KEY_METHOD_set_verify(EC_KEY_METHOD *meth,
    int (*verify)(int type, const unsigned char *dgst, int dgst_len,
	const unsigned char *sigbuf, int sig_len, EC_KEY *eckey),
    int (*verify_sig)(const unsigned char *dgst, int dgst_len,
	const ECDSA_SIG *sig, EC_KEY *eckey))
{
	meth->verify = verify;
	meth->verify_sig = verify_sig;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_set_verify);


void
EC_KEY_METHOD_get_init(const EC_KEY_METHOD *meth,
    int (**pinit)(EC_KEY *key),
    void (**pfinish)(EC_KEY *key),
    int (**pcopy)(EC_KEY *dest, const EC_KEY *src),
    int (**pset_group)(EC_KEY *key, const EC_GROUP *grp),
    int (**pset_private)(EC_KEY *key, const BIGNUM *priv_key),
    int (**pset_public)(EC_KEY *key, const EC_POINT *pub_key))
{
	if (pinit != NULL)
		*pinit = meth->init;
	if (pfinish != NULL)
		*pfinish = meth->finish;
	if (pcopy != NULL)
		*pcopy = meth->copy;
	if (pset_group != NULL)
		*pset_group = meth->set_group;
	if (pset_private != NULL)
		*pset_private = meth->set_private;
	if (pset_public != NULL)
		*pset_public = meth->set_public;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_get_init);

void
EC_KEY_METHOD_get_keygen(const EC_KEY_METHOD *meth,
    int (**pkeygen)(EC_KEY *key))
{
	if (pkeygen != NULL)
		*pkeygen = meth->keygen;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_get_keygen);

void
EC_KEY_METHOD_get_compute_key(const EC_KEY_METHOD *meth,
    int (**pck)(unsigned char **out, size_t *out_len, const EC_POINT *pub_key,
        const EC_KEY *ecdh))
{
	if (pck != NULL)
		*pck = meth->compute_key;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_get_compute_key);

void
EC_KEY_METHOD_get_sign(const EC_KEY_METHOD *meth,
    int (**psign)(int type, const unsigned char *dgst,
	int dlen, unsigned char *sig, unsigned int *siglen,
	const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey),
    int (**psign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
	BIGNUM **kinvp, BIGNUM **rp),
    ECDSA_SIG *(**psign_sig)(const unsigned char *dgst,
	int dgst_len, const BIGNUM *in_kinv, const BIGNUM *in_r,
	EC_KEY *eckey))
{
	if (psign != NULL)
		*psign = meth->sign;
	if (psign_setup != NULL)
		*psign_setup = meth->sign_setup;
	if (psign_sig != NULL)
		*psign_sig = meth->sign_sig;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_get_sign);

void
EC_KEY_METHOD_get_verify(const EC_KEY_METHOD *meth,
    int (**pverify)(int type, const unsigned char *dgst, int dgst_len,
	const unsigned char *sigbuf, int sig_len, EC_KEY *eckey),
    int (**pverify_sig)(const unsigned char *dgst, int dgst_len,
	const ECDSA_SIG *sig, EC_KEY *eckey))
{
	if (pverify != NULL)
		*pverify = meth->verify;
	if (pverify_sig != NULL)
		*pverify_sig = meth->verify_sig;
}
LCRYPTO_ALIAS(EC_KEY_METHOD_get_verify);
