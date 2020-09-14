/*
 * Portions Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * Portions Copyright (C) Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Principal Author: Brian Wellington
 * $Id: dst_api.c,v 1.16 2020/09/14 08:40:43 florian Exp $
 */

/*! \file */
#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/refcount.h>
#include <isc/util.h>

#include <dns/keyvalues.h>

#include <dst/result.h>

#include "dst_internal.h"

static dst_func_t *dst_t_func[DST_MAX_ALGS];
static int dst_initialized = 0;

/*
 * Static functions.
 */
static dst_key_t *	get_key_struct(unsigned int alg,
				       unsigned int flags,
				       unsigned int protocol,
				       unsigned int bits);
static isc_result_t	computeid(dst_key_t *key);
static isc_result_t	frombuffer(unsigned int alg,
				   unsigned int flags,
				   unsigned int protocol,
				   isc_buffer_t *source,
				   dst_key_t **keyp);

static isc_result_t	algorithm_status(unsigned int alg);

#define RETERR(x)				\
	do {					\
		result = (x);			\
		if (result != ISC_R_SUCCESS)	\
			goto out;		\
	} while (0)

#define CHECKALG(alg)				\
	do {					\
		isc_result_t _r;		\
		_r = algorithm_status(alg);	\
		if (_r != ISC_R_SUCCESS)	\
			return (_r);		\
	} while (0);				\

isc_result_t
dst_lib_init(void) {
	isc_result_t result;

	REQUIRE(!dst_initialized);

	dst_result_register();

	memset(dst_t_func, 0, sizeof(dst_t_func));
	RETERR(dst__hmacsha1_init(&dst_t_func[DST_ALG_HMACSHA1]));
	RETERR(dst__hmacsha224_init(&dst_t_func[DST_ALG_HMACSHA224]));
	RETERR(dst__hmacsha256_init(&dst_t_func[DST_ALG_HMACSHA256]));
	RETERR(dst__hmacsha384_init(&dst_t_func[DST_ALG_HMACSHA384]));
	RETERR(dst__hmacsha512_init(&dst_t_func[DST_ALG_HMACSHA512]));
	RETERR(dst__openssl_init());
	dst_initialized = 1;
	return (ISC_R_SUCCESS);

 out:
	/* avoid immediate crash! */
	dst_initialized = 1;
	dst_lib_destroy();
	return (result);
}

void
dst_lib_destroy(void) {
	RUNTIME_CHECK(dst_initialized);
	dst_initialized = 0;

	dst__openssl_destroy();
}

int
dst_algorithm_supported(unsigned int alg) {
	REQUIRE(dst_initialized);

	if (alg >= DST_MAX_ALGS || dst_t_func[alg] == NULL)
		return (0);
	return (1);
}

isc_result_t
dst_context_create3(dst_key_t *key,
		    isc_logcategory_t *category, int useforsigning,
		    dst_context_t **dctxp)
{
	dst_context_t *dctx;
	isc_result_t result;

	REQUIRE(dst_initialized);
	REQUIRE(dctxp != NULL && *dctxp == NULL);

	dctx = malloc(sizeof(dst_context_t));
	if (dctx == NULL)
		return (ISC_R_NOMEMORY);
	memset(dctx, 0, sizeof(*dctx));
	dst_key_attach(key, &dctx->key);
	dctx->category = category;
	if (useforsigning)
		dctx->use = DO_SIGN;
	else
		dctx->use = DO_VERIFY;
	result = key->func->createctx(key, dctx);
	if (result != ISC_R_SUCCESS) {
		if (dctx->key != NULL)
			dst_key_free(&dctx->key);
		free(dctx);
		return (result);
	}
	*dctxp = dctx;
	return (ISC_R_SUCCESS);
}

void
dst_context_destroy(dst_context_t **dctxp) {
	dst_context_t *dctx;

	REQUIRE(dctxp != NULL);

	dctx = *dctxp;
	dctx->key->func->destroyctx(dctx);
	if (dctx->key != NULL)
		dst_key_free(&dctx->key);
	free(dctx);
	*dctxp = NULL;
}

isc_result_t
dst_context_adddata(dst_context_t *dctx, const isc_region_t *data) {
	REQUIRE(data != NULL);
	return (dctx->key->func->adddata(dctx, data));
}

isc_result_t
dst_context_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key;

	REQUIRE(sig != NULL);

	key = dctx->key;
	CHECKALG(key->key_alg);

	return (key->func->sign(dctx, sig));
}

isc_result_t
dst_context_verify(dst_context_t *dctx, isc_region_t *sig) {
	REQUIRE(sig != NULL);

	CHECKALG(dctx->key->key_alg);

	return (dctx->key->func->verify(dctx, sig));
}

isc_result_t
dst_key_todns(const dst_key_t *key, isc_buffer_t *target) {
	REQUIRE(dst_initialized);
	REQUIRE(target != NULL);

	CHECKALG(key->key_alg);

	if (isc_buffer_availablelength(target) < 4)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint16(target, (uint16_t)(key->key_flags & 0xffff));
	isc_buffer_putuint8(target, (uint8_t)key->key_proto);
	isc_buffer_putuint8(target, (uint8_t)key->key_alg);

	if (key->key_flags & DNS_KEYFLAG_EXTENDED) {
		if (isc_buffer_availablelength(target) < 2)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint16(target,
				     (uint16_t)((key->key_flags >> 16)
						    & 0xffff));
	}

	return (key->func->todns(key, target));
}

isc_result_t
dst_key_frombuffer(unsigned int alg, unsigned int flags, unsigned int protocol,
		   isc_buffer_t *source, dst_key_t **keyp)
{
	dst_key_t *key = NULL;
	isc_result_t result;

	REQUIRE(dst_initialized);

	result = frombuffer(alg, flags, protocol, source, &key);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = computeid(key);
	if (result != ISC_R_SUCCESS) {
		dst_key_free(&key);
		return (result);
	}

	*keyp = key;
	return (ISC_R_SUCCESS);
}

void
dst_key_attach(dst_key_t *source, dst_key_t **target) {

	REQUIRE(dst_initialized);
	REQUIRE(target != NULL && *target == NULL);

	isc_refcount_increment(&source->refs, NULL);
	*target = source;
}

void
dst_key_free(dst_key_t **keyp) {
	dst_key_t *key;
	unsigned int refs;

	REQUIRE(dst_initialized);
	REQUIRE(keyp != NULL);

	key = *keyp;

	isc_refcount_decrement(&key->refs, &refs);
	if (refs != 0)
		return;

	isc_refcount_destroy(&key->refs);
	key->func->destroy(key);
	freezero(key, sizeof(*key));
	*keyp = NULL;
}

isc_result_t
dst_key_sigsize(const dst_key_t *key, unsigned int *n) {
	REQUIRE(dst_initialized);
	REQUIRE(n != NULL);

	/* XXXVIX this switch statement is too sparse to gen a jump table. */
	switch (key->key_alg) {
	case DST_ALG_HMACSHA1:
		*n = ISC_SHA1_DIGESTLENGTH;
		break;
	case DST_ALG_HMACSHA224:
		*n = ISC_SHA224_DIGESTLENGTH;
		break;
	case DST_ALG_HMACSHA256:
		*n = ISC_SHA256_DIGESTLENGTH;
		break;
	case DST_ALG_HMACSHA384:
		*n = ISC_SHA384_DIGESTLENGTH;
		break;
	case DST_ALG_HMACSHA512:
		*n = ISC_SHA512_DIGESTLENGTH;
		break;
	default:
		return (DST_R_UNSUPPORTEDALG);
	}
	return (ISC_R_SUCCESS);
}

/***
 *** Static methods
 ***/

/*%
 * Allocates a key structure and fills in some of the fields.
 */
static dst_key_t *
get_key_struct(unsigned int alg,
	       unsigned int flags, unsigned int protocol,
	       unsigned int bits)
{
	dst_key_t *key;
	isc_result_t result;

	key = (dst_key_t *) malloc(sizeof(dst_key_t));
	if (key == NULL)
		return (NULL);

	memset(key, 0, sizeof(dst_key_t));

	result = isc_refcount_init(&key->refs, 1);
	if (result != ISC_R_SUCCESS) {
		free(key);
		return (NULL);
	}
	key->key_alg = alg;
	key->key_flags = flags;
	key->key_proto = protocol;
	key->key_size = bits;
	key->func = dst_t_func[alg];
	return (key);
}

static isc_result_t
computeid(dst_key_t *key) {
	isc_buffer_t dnsbuf;
	unsigned char dns_array[DST_KEY_MAXSIZE];
	isc_region_t r;
	isc_result_t ret;

	isc_buffer_init(&dnsbuf, dns_array, sizeof(dns_array));
	ret = dst_key_todns(key, &dnsbuf);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_usedregion(&dnsbuf, &r);
	return (ISC_R_SUCCESS);
}

static isc_result_t
frombuffer(unsigned int alg, unsigned int flags,
	   unsigned int protocol, isc_buffer_t *source, dst_key_t **keyp)
{
	dst_key_t *key;
	isc_result_t ret;

	REQUIRE(source != NULL);
	REQUIRE(keyp != NULL && *keyp == NULL);

	key = get_key_struct(alg, flags, protocol, 0);
	if (key == NULL)
		return (ISC_R_NOMEMORY);

	if (isc_buffer_remaininglength(source) > 0) {
		ret = algorithm_status(alg);
		if (ret != ISC_R_SUCCESS) {
			dst_key_free(&key);
			return (ret);
		}

		ret = key->func->fromdns(key, source);
		if (ret != ISC_R_SUCCESS) {
			dst_key_free(&key);
			return (ret);
		}
	}

	*keyp = key;
	return (ISC_R_SUCCESS);
}

static isc_result_t
algorithm_status(unsigned int alg) {
	REQUIRE(dst_initialized);

	if (dst_algorithm_supported(alg))
		return (ISC_R_SUCCESS);
	return (DST_R_UNSUPPORTEDALG);
}
