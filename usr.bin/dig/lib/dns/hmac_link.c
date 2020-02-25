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
 * $Id: hmac_link.c,v 1.9 2020/02/25 18:10:17 florian Exp $
 */

#include <string.h>

#include <isc/buffer.h>
#include <isc/hmacsha.h>
#include <isc/sha1.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"

static isc_result_t hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha1_key {
	unsigned char key[ISC_SHA1_BLOCK_LENGTH];
};

static isc_result_t
hmacsha1_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx;
	dst_hmacsha1_key_t *hkey = key->keydata.hmacsha1;

	hmacsha1ctx = malloc(sizeof(isc_hmacsha1_t));
	if (hmacsha1ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha1_init(hmacsha1ctx, hkey->key, ISC_SHA1_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha1ctx = hmacsha1ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha1_destroyctx(dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	if (hmacsha1ctx != NULL) {
		isc_hmacsha1_invalidate(hmacsha1ctx);
		free(hmacsha1ctx);
		dctx->ctxdata.hmacsha1ctx = NULL;
	}
}

static isc_result_t
hmacsha1_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	isc_hmacsha1_update(hmacsha1ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA1_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha1_sign(hmacsha1ctx, digest, ISC_SHA1_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA1_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	if (sig->length > ISC_SHA1_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha1_verify(hmacsha1ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static void
hmacsha1_destroy(dst_key_t *key) {
	dst_hmacsha1_key_t *hkey = key->keydata.hmacsha1;

	freezero(hkey, sizeof(*hkey));
	key->keydata.hmacsha1 = NULL;
}

static isc_result_t
hmacsha1_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha1_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha1 != NULL);

	hkey = key->keydata.hmacsha1;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha1_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha1_t sha1ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha1_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA1_BLOCK_LENGTH) {
		isc_sha1_init(&sha1ctx);
		isc_sha1_update(&sha1ctx, r.base, r.length);
		isc_sha1_final(&sha1ctx, hkey->key);
		keylen = ISC_SHA1_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha1 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static dst_func_t hmacsha1_functions = {
	hmacsha1_createctx,
	hmacsha1_destroyctx,
	hmacsha1_adddata,
	hmacsha1_sign,
	hmacsha1_verify,
	hmacsha1_destroy,
	hmacsha1_todns,
	hmacsha1_fromdns,
};

isc_result_t
dst__hmacsha1_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha1_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha224_key {
	unsigned char key[ISC_SHA224_BLOCK_LENGTH];
};

static isc_result_t
hmacsha224_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx;
	dst_hmacsha224_key_t *hkey = key->keydata.hmacsha224;

	hmacsha224ctx = malloc(sizeof(isc_hmacsha224_t));
	if (hmacsha224ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha224_init(hmacsha224ctx, hkey->key, ISC_SHA224_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha224ctx = hmacsha224ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha224_destroyctx(dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	if (hmacsha224ctx != NULL) {
		isc_hmacsha224_invalidate(hmacsha224ctx);
		free(hmacsha224ctx);
		dctx->ctxdata.hmacsha224ctx = NULL;
	}
}

static isc_result_t
hmacsha224_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	isc_hmacsha224_update(hmacsha224ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA224_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha224_sign(hmacsha224ctx, digest, ISC_SHA224_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA224_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	if (sig->length > ISC_SHA224_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha224_verify(hmacsha224ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static void
hmacsha224_destroy(dst_key_t *key) {
	dst_hmacsha224_key_t *hkey = key->keydata.hmacsha224;

	freezero(hkey, sizeof(*hkey));
	key->keydata.hmacsha224 = NULL;
}

static isc_result_t
hmacsha224_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha224_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha224 != NULL);

	hkey = key->keydata.hmacsha224;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha224_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha224_t sha224ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha224_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA224_BLOCK_LENGTH) {
		isc_sha224_init(&sha224ctx);
		isc_sha224_update(&sha224ctx, r.base, r.length);
		isc_sha224_final(hkey->key, &sha224ctx);
		keylen = ISC_SHA224_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha224 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static dst_func_t hmacsha224_functions = {
	hmacsha224_createctx,
	hmacsha224_destroyctx,
	hmacsha224_adddata,
	hmacsha224_sign,
	hmacsha224_verify,
	hmacsha224_destroy,
	hmacsha224_todns,
	hmacsha224_fromdns,
};

isc_result_t
dst__hmacsha224_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha224_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha256_key {
	unsigned char key[ISC_SHA256_BLOCK_LENGTH];
};

static isc_result_t
hmacsha256_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx;
	dst_hmacsha256_key_t *hkey = key->keydata.hmacsha256;

	hmacsha256ctx = malloc(sizeof(isc_hmacsha256_t));
	if (hmacsha256ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha256_init(hmacsha256ctx, hkey->key, ISC_SHA256_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha256ctx = hmacsha256ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha256_destroyctx(dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	if (hmacsha256ctx != NULL) {
		isc_hmacsha256_invalidate(hmacsha256ctx);
		free(hmacsha256ctx);
		dctx->ctxdata.hmacsha256ctx = NULL;
	}
}

static isc_result_t
hmacsha256_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	isc_hmacsha256_update(hmacsha256ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA256_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha256_sign(hmacsha256ctx, digest, ISC_SHA256_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA256_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	if (sig->length > ISC_SHA256_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha256_verify(hmacsha256ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static void
hmacsha256_destroy(dst_key_t *key) {
	dst_hmacsha256_key_t *hkey = key->keydata.hmacsha256;

	freezero(hkey, sizeof(*hkey));
	key->keydata.hmacsha256 = NULL;
}

static isc_result_t
hmacsha256_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha256_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha256 != NULL);

	hkey = key->keydata.hmacsha256;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha256_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha256_t sha256ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha256_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA256_BLOCK_LENGTH) {
		isc_sha256_init(&sha256ctx);
		isc_sha256_update(&sha256ctx, r.base, r.length);
		isc_sha256_final(hkey->key, &sha256ctx);
		keylen = ISC_SHA256_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha256 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static dst_func_t hmacsha256_functions = {
	hmacsha256_createctx,
	hmacsha256_destroyctx,
	hmacsha256_adddata,
	hmacsha256_sign,
	hmacsha256_verify,
	hmacsha256_destroy,
	hmacsha256_todns,
	hmacsha256_fromdns,
};

isc_result_t
dst__hmacsha256_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha256_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha384_key {
	unsigned char key[ISC_SHA384_BLOCK_LENGTH];
};

static isc_result_t
hmacsha384_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx;
	dst_hmacsha384_key_t *hkey = key->keydata.hmacsha384;

	hmacsha384ctx = malloc(sizeof(isc_hmacsha384_t));
	if (hmacsha384ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha384_init(hmacsha384ctx, hkey->key, ISC_SHA384_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha384ctx = hmacsha384ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha384_destroyctx(dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	if (hmacsha384ctx != NULL) {
		isc_hmacsha384_invalidate(hmacsha384ctx);
		free(hmacsha384ctx);
		dctx->ctxdata.hmacsha384ctx = NULL;
	}
}

static isc_result_t
hmacsha384_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	isc_hmacsha384_update(hmacsha384ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA384_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha384_sign(hmacsha384ctx, digest, ISC_SHA384_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA384_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	if (sig->length > ISC_SHA384_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha384_verify(hmacsha384ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static void
hmacsha384_destroy(dst_key_t *key) {
	dst_hmacsha384_key_t *hkey = key->keydata.hmacsha384;

	freezero(hkey, sizeof(*hkey));
	key->keydata.hmacsha384 = NULL;
}

static isc_result_t
hmacsha384_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha384_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha384 != NULL);

	hkey = key->keydata.hmacsha384;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha384_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha384_t sha384ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha384_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA384_BLOCK_LENGTH) {
		isc_sha384_init(&sha384ctx);
		isc_sha384_update(&sha384ctx, r.base, r.length);
		isc_sha384_final(hkey->key, &sha384ctx);
		keylen = ISC_SHA384_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha384 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static dst_func_t hmacsha384_functions = {
	hmacsha384_createctx,
	hmacsha384_destroyctx,
	hmacsha384_adddata,
	hmacsha384_sign,
	hmacsha384_verify,
	hmacsha384_destroy,
	hmacsha384_todns,
	hmacsha384_fromdns,
};

isc_result_t
dst__hmacsha384_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha384_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha512_key {
	unsigned char key[ISC_SHA512_BLOCK_LENGTH];
};

static isc_result_t
hmacsha512_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx;
	dst_hmacsha512_key_t *hkey = key->keydata.hmacsha512;

	hmacsha512ctx = malloc(sizeof(isc_hmacsha512_t));
	if (hmacsha512ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha512_init(hmacsha512ctx, hkey->key, ISC_SHA512_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha512ctx = hmacsha512ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha512_destroyctx(dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	if (hmacsha512ctx != NULL) {
		isc_hmacsha512_invalidate(hmacsha512ctx);
		free(hmacsha512ctx);
		dctx->ctxdata.hmacsha512ctx = NULL;
	}
}

static isc_result_t
hmacsha512_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	isc_hmacsha512_update(hmacsha512ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA512_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha512_sign(hmacsha512ctx, digest, ISC_SHA512_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA512_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	if (sig->length > ISC_SHA512_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha512_verify(hmacsha512ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static void
hmacsha512_destroy(dst_key_t *key) {
	dst_hmacsha512_key_t *hkey = key->keydata.hmacsha512;

	freezero(hkey, sizeof(*hkey));
	key->keydata.hmacsha512 = NULL;
}

static isc_result_t
hmacsha512_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha512_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha512 != NULL);

	hkey = key->keydata.hmacsha512;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha512_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha512_t sha512ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha512_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA512_BLOCK_LENGTH) {
		isc_sha512_init(&sha512ctx);
		isc_sha512_update(&sha512ctx, r.base, r.length);
		isc_sha512_final(hkey->key, &sha512ctx);
		keylen = ISC_SHA512_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha512 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static dst_func_t hmacsha512_functions = {
	hmacsha512_createctx,
	hmacsha512_destroyctx,
	hmacsha512_adddata,
	hmacsha512_sign,
	hmacsha512_verify,
	hmacsha512_destroy,
	hmacsha512_todns,
	hmacsha512_fromdns,
};

isc_result_t
dst__hmacsha512_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha512_functions;
	return (ISC_R_SUCCESS);
}

/*! \file */
