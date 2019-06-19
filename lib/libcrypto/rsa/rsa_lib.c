/* $OpenBSD: rsa_lib.c,v 1.37 2018/04/14 07:09:21 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>

#include <openssl/opensslconf.h>

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/lhash.h>
#include <openssl/rsa.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

static const RSA_METHOD *default_RSA_meth = NULL;

RSA *
RSA_new(void)
{
	RSA *r = RSA_new_method(NULL);

	return r;
}

void
RSA_set_default_method(const RSA_METHOD *meth)
{
	default_RSA_meth = meth;
}

const RSA_METHOD *
RSA_get_default_method(void)
{
	if (default_RSA_meth == NULL)
		default_RSA_meth = RSA_PKCS1_SSLeay();

	return default_RSA_meth;
}

const RSA_METHOD *
RSA_get_method(const RSA *rsa)
{
	return rsa->meth;
}

int
RSA_set_method(RSA *rsa, const RSA_METHOD *meth)
{
	/*
	 * NB: The caller is specifically setting a method, so it's not up to us
	 * to deal with which ENGINE it comes from.
	 */
	const RSA_METHOD *mtmp;

	mtmp = rsa->meth;
	if (mtmp->finish)
		mtmp->finish(rsa);
#ifndef OPENSSL_NO_ENGINE
	ENGINE_finish(rsa->engine);
	rsa->engine = NULL;
#endif
	rsa->meth = meth;
	if (meth->init)
		meth->init(rsa);
	return 1;
}

RSA *
RSA_new_method(ENGINE *engine)
{
	RSA *ret;

	ret = malloc(sizeof(RSA));
	if (ret == NULL) {
		RSAerror(ERR_R_MALLOC_FAILURE);
		return NULL;
	}

	ret->meth = RSA_get_default_method();
#ifndef OPENSSL_NO_ENGINE
	if (engine) {
		if (!ENGINE_init(engine)) {
			RSAerror(ERR_R_ENGINE_LIB);
			free(ret);
			return NULL;
		}
		ret->engine = engine;
	} else
		ret->engine = ENGINE_get_default_RSA();
	if (ret->engine) {
		ret->meth = ENGINE_get_RSA(ret->engine);
		if (ret->meth == NULL) {
			RSAerror(ERR_R_ENGINE_LIB);
			ENGINE_finish(ret->engine);
			free(ret);
			return NULL;
		}
	}
#endif

	ret->pad = 0;
	ret->version = 0;
	ret->n = NULL;
	ret->e = NULL;
	ret->d = NULL;
	ret->p = NULL;
	ret->q = NULL;
	ret->dmp1 = NULL;
	ret->dmq1 = NULL;
	ret->iqmp = NULL;
	ret->references = 1;
	ret->_method_mod_n = NULL;
	ret->_method_mod_p = NULL;
	ret->_method_mod_q = NULL;
	ret->blinding = NULL;
	ret->mt_blinding = NULL;
	ret->flags = ret->meth->flags & ~RSA_FLAG_NON_FIPS_ALLOW;
	if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data)) {
#ifndef OPENSSL_NO_ENGINE
		ENGINE_finish(ret->engine);
#endif
		free(ret);
		return NULL;
	}

	if (ret->meth->init != NULL && !ret->meth->init(ret)) {
#ifndef OPENSSL_NO_ENGINE
		ENGINE_finish(ret->engine);
#endif
		CRYPTO_free_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data);
		free(ret);
		ret = NULL;
	}
	return ret;
}

void
RSA_free(RSA *r)
{
	int i;

	if (r == NULL)
		return;

	i = CRYPTO_add(&r->references, -1, CRYPTO_LOCK_RSA);
	if (i > 0)
		return;

	if (r->meth->finish)
		r->meth->finish(r);
#ifndef OPENSSL_NO_ENGINE
	ENGINE_finish(r->engine);
#endif

	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_RSA, r, &r->ex_data);

	BN_clear_free(r->n);
	BN_clear_free(r->e);
	BN_clear_free(r->d);
	BN_clear_free(r->p);
	BN_clear_free(r->q);
	BN_clear_free(r->dmp1);
	BN_clear_free(r->dmq1);
	BN_clear_free(r->iqmp);
	BN_BLINDING_free(r->blinding);
	BN_BLINDING_free(r->mt_blinding);
	free(r);
}

int
RSA_up_ref(RSA *r)
{
	int i = CRYPTO_add(&r->references, 1, CRYPTO_LOCK_RSA);
	return i > 1 ? 1 : 0;
}

int
RSA_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
    CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
{
	return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_RSA, argl, argp,
	    new_func, dup_func, free_func);
}

int
RSA_set_ex_data(RSA *r, int idx, void *arg)
{
	return CRYPTO_set_ex_data(&r->ex_data, idx, arg);
}

void *
RSA_get_ex_data(const RSA *r, int idx)
{
	return CRYPTO_get_ex_data(&r->ex_data, idx);
}

void
RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
	if (n != NULL)
		*n = r->n;
	if (e != NULL)
		*e = r->e;
	if (d != NULL)
		*d = r->d;
}

int
RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d)
{
	if ((r->n == NULL && n == NULL) || (r->e == NULL && e == NULL))
		return 0;

	if (n != NULL) {
		BN_free(r->n);
		r->n = n;
	}
	if (e != NULL) {
		BN_free(r->e);
		r->e = e;
	}
	if (d != NULL) {
		BN_free(r->d);
		r->d = d;
	}

	return 1;
}

void
RSA_get0_crt_params(const RSA *r, const BIGNUM **dmp1, const BIGNUM **dmq1,
    const BIGNUM **iqmp)
{
	if (dmp1 != NULL)
		*dmp1 = r->dmp1;
	if (dmq1 != NULL)
		*dmq1 = r->dmq1;
	if (iqmp != NULL)
		*iqmp = r->iqmp;
}

int
RSA_set0_crt_params(RSA *r, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp)
{
	if ((r->dmp1 == NULL && dmp1 == NULL) ||
	    (r->dmq1 == NULL && dmq1 == NULL) ||
	    (r->iqmp == NULL && iqmp == NULL))
	       	return 0;

	if (dmp1 != NULL) {
		BN_free(r->dmp1);
		r->dmp1 = dmp1;
	}
	if (dmq1 != NULL) {
		BN_free(r->dmq1);
		r->dmq1 = dmq1;
	}
	if (iqmp != NULL) {
		BN_free(r->iqmp);
		r->iqmp = iqmp;
	}

	return 1;
}

void
RSA_get0_factors(const RSA *r, const BIGNUM **p, const BIGNUM **q)
{
	if (p != NULL)
		*p = r->p;
	if (q != NULL)
		*q = r->q;
}

int
RSA_set0_factors(RSA *r, BIGNUM *p, BIGNUM *q)
{
	if ((r->p == NULL && p == NULL) || (r->q == NULL && q == NULL))
		return 0;

	if (p != NULL) {
		BN_free(r->p);
		r->p = p;
	}
	if (q != NULL) {
		BN_free(r->q);
		r->q = q;
	}

	return 1;
}

void
RSA_clear_flags(RSA *r, int flags)
{
	r->flags &= ~flags;
}

int
RSA_test_flags(const RSA *r, int flags)
{
	return r->flags & flags;
}

void
RSA_set_flags(RSA *r, int flags)
{
	r->flags |= flags;
}
