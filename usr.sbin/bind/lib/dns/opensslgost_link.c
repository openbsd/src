/*
 * Copyright (C) 2010-2016  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#if defined(OPENSSL) && defined(HAVE_OPENSSL_GOST)

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"
#include "dst_gost.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#define EVP_MD_CTX_new() &(ctx->_ctx), EVP_MD_CTX_init(&(ctx->_ctx))
#define EVP_MD_CTX_free(ptr) EVP_MD_CTX_cleanup(ptr)
#endif

static ENGINE *e = NULL;
static const EVP_MD *opensslgost_digest;
extern const EVP_MD *EVP_gost(void);

const EVP_MD *EVP_gost(void) {
	return (opensslgost_digest);
}

/* ISC methods */

isc_result_t
isc_gost_init(isc_gost_t *ctx) {
	const EVP_MD *md;
	int ret;

	INSIST(ctx != NULL);

	md = EVP_gost();
	if (md == NULL)
		return (DST_R_CRYPTOFAILURE);
	ctx->ctx = EVP_MD_CTX_new();
	if (ctx->ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = EVP_DigestInit(ctx->ctx, md);
	if (ret != 1)
		return (DST_R_CRYPTOFAILURE);
	return (ISC_R_SUCCESS);
}

void
isc_gost_invalidate(isc_gost_t *ctx) {
	EVP_MD_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

isc_result_t
isc_gost_update(isc_gost_t *ctx, const unsigned char *data,
		unsigned int len)
{
	int ret;

	INSIST(ctx != NULL);
	INSIST(ctx->ctx != NULL);
	INSIST(data != NULL);

	ret = EVP_DigestUpdate(ctx->ctx, (const void *) data, (size_t) len);
	if (ret != 1)
		return (DST_R_CRYPTOFAILURE);
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_gost_final(isc_gost_t *ctx, unsigned char *digest) {
	int ret;

	INSIST(ctx != NULL);
	INSIST(ctx->ctx != NULL);
	INSIST(digest != NULL);

	ret = EVP_DigestFinal(ctx->ctx, digest, NULL);
	EVP_MD_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	if (ret != 1)
		return (DST_R_CRYPTOFAILURE);
	return (ISC_R_SUCCESS);
}

/* DST methods */

#define DST_RET(a) {ret = a; goto err;}

static isc_result_t opensslgost_todns(const dst_key_t *key,
				      isc_buffer_t *data);

static isc_result_t
opensslgost_createctx(dst_key_t *key, dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx;
	const EVP_MD *md = EVP_gost();

	UNUSED(key);

	if (md == NULL)
		return (DST_R_OPENSSLFAILURE);

	evp_md_ctx = EVP_MD_CTX_create();
	if (evp_md_ctx == NULL)
		return (ISC_R_NOMEMORY);

	if (!EVP_DigestInit_ex(evp_md_ctx, md, NULL)) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		return (ISC_R_FAILURE);
	}
	dctx->ctxdata.evp_md_ctx = evp_md_ctx;

	return (ISC_R_SUCCESS);
}

static void
opensslgost_destroyctx(dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	if (evp_md_ctx != NULL) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		dctx->ctxdata.evp_md_ctx = NULL;
	}
}

static isc_result_t
opensslgost_adddata(dst_context_t *dctx, const isc_region_t *data) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	if (!EVP_DigestUpdate(evp_md_ctx, data->base, data->length))
		return (ISC_R_FAILURE);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslgost_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key = dctx->key;
	isc_region_t r;
	unsigned int siglen = 0;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;

	isc_buffer_availableregion(sig, &r);

	if (r.length < (unsigned int) EVP_PKEY_size(pkey))
		return (ISC_R_NOSPACE);

	if (!EVP_SignFinal(evp_md_ctx, r.base, &siglen, pkey))
		return (ISC_R_FAILURE);

	isc_buffer_add(sig, siglen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslgost_verify(dst_context_t *dctx, const isc_region_t *sig) {
	dst_key_t *key = dctx->key;
	int status = 0;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;

	status = EVP_VerifyFinal(evp_md_ctx, sig->base, sig->length, pkey);
	switch (status) {
	case 1:
		return (ISC_R_SUCCESS);
	case 0:
		return (dst__openssl_toresult(DST_R_VERIFYFAILURE));
	default:
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_VerifyFinal",
					       DST_R_VERIFYFAILURE));
	}
}

static isc_boolean_t
opensslgost_compare(const dst_key_t *key1, const dst_key_t *key2) {
	EVP_PKEY *pkey1, *pkey2;

	pkey1 = key1->keydata.pkey;
	pkey2 = key2->keydata.pkey;

	if (pkey1 == NULL && pkey2 == NULL)
		return (ISC_TRUE);
	else if (pkey1 == NULL || pkey2 == NULL)
		return (ISC_FALSE);

	if (EVP_PKEY_cmp(pkey1, pkey2) != 1)
		return (ISC_FALSE);
	return (ISC_TRUE);
}

static int
progress_cb(EVP_PKEY_CTX *ctx)
{
	union {
		void *dptr;
		void (*fptr)(int);
	} u;
	int p;

	u.dptr = EVP_PKEY_CTX_get_app_data(ctx);
	p = EVP_PKEY_CTX_get_keygen_info(ctx, 0);
	if (u.fptr != NULL)
		u.fptr(p);
	return (1);
}

static isc_result_t
opensslgost_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	EVP_PKEY_CTX *ctx;
	union {
		void *dptr;
		void (*fptr)(int);
	} u;
	EVP_PKEY *pkey = NULL;
	isc_result_t ret;

	UNUSED(unused);
	ctx = EVP_PKEY_CTX_new_id(NID_id_GostR3410_2001, NULL);
	if (ctx == NULL)
		DST_RET(dst__openssl_toresult2("EVP_PKEY_CTX_new_id",
					       DST_R_OPENSSLFAILURE));
	if (callback != NULL) {
		u.fptr = callback;
		EVP_PKEY_CTX_set_app_data(ctx, u.dptr);
		EVP_PKEY_CTX_set_cb(ctx, &progress_cb);
	}
	if (EVP_PKEY_keygen_init(ctx) <= 0)
		DST_RET(dst__openssl_toresult2("EVP_PKEY_keygen_init",
					       DST_R_OPENSSLFAILURE));
	if (EVP_PKEY_CTX_ctrl_str(ctx, "paramset", "A") <= 0)
		DST_RET(dst__openssl_toresult2("EVP_PKEY_CTX_ctrl_str",
					       DST_R_OPENSSLFAILURE));
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
		DST_RET(dst__openssl_toresult2("EVP_PKEY_keygen",
					       DST_R_OPENSSLFAILURE));
	key->keydata.pkey = pkey;
	key->key_size = EVP_PKEY_bits(pkey);
	EVP_PKEY_CTX_free(ctx);
	return (ISC_R_SUCCESS);

err:
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	if (ctx != NULL)
		EVP_PKEY_CTX_free(ctx);
	return (ret);
}

static isc_boolean_t
opensslgost_isprivate(const dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;
	EC_KEY *ec;

	INSIST(pkey != NULL);

	ec = EVP_PKEY_get0(pkey);
	return (ISC_TF(ec != NULL && EC_KEY_get0_private_key(ec) != NULL));
}

static void
opensslgost_destroy(dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;

	EVP_PKEY_free(pkey);
	key->keydata.pkey = NULL;
}

static const unsigned char gost_prefix[37] = {
	0x30, 0x63, 0x30, 0x1c, 0x06, 0x06, 0x2a, 0x85,
	0x03, 0x02, 0x02, 0x13, 0x30, 0x12, 0x06, 0x07,
	0x2a, 0x85, 0x03, 0x02, 0x02, 0x23, 0x01, 0x06,
	0x07, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x1e, 0x01,
	0x03, 0x43, 0x00, 0x04, 0x40
};

static isc_result_t
opensslgost_todns(const dst_key_t *key, isc_buffer_t *data) {
	EVP_PKEY *pkey;
	isc_region_t r;
	unsigned char der[37 + 64], *p;
	int len;

	REQUIRE(key->keydata.pkey != NULL);

	pkey = key->keydata.pkey;

	isc_buffer_availableregion(data, &r);
	if (r.length < 64)
		return (ISC_R_NOSPACE);

	p = der;
	len = i2d_PUBKEY(pkey, &p);
	INSIST(len == sizeof(der));
	INSIST(isc_safe_memequal(gost_prefix, der, 37));
	memmove(r.base, der + 37, 64);
	isc_buffer_add(data, 64);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslgost_fromdns(dst_key_t *key, isc_buffer_t *data) {
	isc_region_t r;
	EVP_PKEY *pkey = NULL;
	unsigned char der[37 + 64];
	const unsigned char *p;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	if (r.length != 64)
		return (DST_R_INVALIDPUBLICKEY);
	memmove(der, gost_prefix, 37);
	memmove(der + 37, r.base, 64);
	isc_buffer_forward(data, 64);

	p = der;
	if (d2i_PUBKEY(&pkey, &p, (long) sizeof(der)) == NULL)
		return (dst__openssl_toresult2("d2i_PUBKEY",
					       DST_R_OPENSSLFAILURE));
	key->keydata.pkey = pkey;
	key->key_size = EVP_PKEY_bits(pkey);

	return (ISC_R_SUCCESS);
}

#ifdef PREFER_GOSTASN1

static isc_result_t
opensslgost_tofile(const dst_key_t *key, const char *directory) {
	EVP_PKEY *pkey;
	dst_private_t priv;
	isc_result_t result;
	unsigned char *der, *p;
	int len;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	pkey = key->keydata.pkey;

	len = i2d_PrivateKey(pkey, NULL);
	der = isc_mem_get(key->mctx, (size_t) len);
	if (der == NULL)
		return (ISC_R_NOMEMORY);

	p = der;
	if (i2d_PrivateKey(pkey, &p) != len) {
		result = dst__openssl_toresult2("i2d_PrivateKey",
						DST_R_OPENSSLFAILURE);
		goto fail;
	}

	priv.elements[0].tag = TAG_GOST_PRIVASN1;
	priv.elements[0].length = len;
	priv.elements[0].data = der;
	priv.nelements = 1;

	result = dst__privstruct_writefile(key, &priv, directory);
 fail:
	if (der != NULL)
		isc_mem_put(key->mctx, der, (size_t) len);
	return (result);
}

#else

static isc_result_t
opensslgost_tofile(const dst_key_t *key, const char *directory) {
	EVP_PKEY *pkey;
	EC_KEY *eckey;
	const BIGNUM *privkey;
	dst_private_t priv;
	isc_result_t ret;
	unsigned char *buf = NULL;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	pkey = key->keydata.pkey;
	eckey = EVP_PKEY_get0(pkey);
	if (eckey == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	privkey = EC_KEY_get0_private_key(eckey);
	if (privkey == NULL)
		return (ISC_R_FAILURE);

	buf = isc_mem_get(key->mctx, BN_num_bytes(privkey));
	if (buf == NULL)
		return (ISC_R_NOMEMORY);

	priv.elements[0].tag = TAG_GOST_PRIVRAW;
	priv.elements[0].length = BN_num_bytes(privkey);
	BN_bn2bin(privkey, buf);
	priv.elements[0].data = buf;
	priv.nelements = 1;

	ret = dst__privstruct_writefile(key, &priv, directory);

	if (buf != NULL)
		isc_mem_put(key->mctx, buf, BN_num_bytes(privkey));
	return (ret);
}
#endif

static unsigned char gost_dummy_key[71] = {
	0x30, 0x45, 0x02, 0x01, 0x00, 0x30, 0x1c, 0x06,
	0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x13, 0x30,
	0x12, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02,
	0x23, 0x01, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02,
	0x02, 0x1e, 0x01, 0x04, 0x22, 0x02, 0x20, 0x1b,
	0x3f, 0x94, 0xf7, 0x1a, 0x5f, 0x2f, 0xe7, 0xe5,
	0x74, 0x0b, 0x8c, 0xd4, 0xb7, 0x18, 0xdd, 0x65,
	0x68, 0x26, 0xd1, 0x54, 0xfb, 0x77, 0xba, 0x63,
	0x72, 0xd9, 0xf0, 0x63, 0x87, 0xe0, 0xd6
};

static isc_result_t
opensslgost_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	isc_mem_t *mctx = key->mctx;
	EVP_PKEY *pkey = NULL;
	EC_KEY *eckey;
	const EC_POINT *pubkey = NULL;
	BIGNUM *privkey = NULL;
	const unsigned char *p;

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_ECCGOST, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	if (key->external) {
		if (priv.nelements != 0)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (pub == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		key->keydata.pkey = pub->keydata.pkey;
		pub->keydata.pkey = NULL;
		key->key_size = pub->key_size;
		dst__privstruct_free(&priv, mctx);
		memset(&priv, 0, sizeof(priv));
		return (ISC_R_SUCCESS);
	}

	INSIST((priv.elements[0].tag == TAG_GOST_PRIVASN1) ||
	       (priv.elements[0].tag == TAG_GOST_PRIVRAW));

	if (priv.elements[0].tag == TAG_GOST_PRIVASN1) {
		p = priv.elements[0].data;
		if (d2i_PrivateKey(NID_id_GostR3410_2001, &pkey, &p,
				   (long) priv.elements[0].length) == NULL)
			DST_RET(dst__openssl_toresult2(
					    "d2i_PrivateKey",
					    DST_R_INVALIDPRIVATEKEY));
	} else {
		if ((pub != NULL) && (pub->keydata.pkey != NULL)) {
			eckey = EVP_PKEY_get0(pub->keydata.pkey);
			pubkey = EC_KEY_get0_public_key(eckey);
		}

		privkey = BN_bin2bn(priv.elements[0].data,
				    priv.elements[0].length, NULL);
		if (privkey == NULL)
			DST_RET(ISC_R_NOMEMORY);

		/* can't create directly the whole key */
		p = gost_dummy_key;
		if (d2i_PrivateKey(NID_id_GostR3410_2001, &pkey, &p,
				   (long) sizeof(gost_dummy_key)) == NULL)
			DST_RET(dst__openssl_toresult2(
					    "d2i_PrivateKey",
					    DST_R_INVALIDPRIVATEKEY));

		eckey = EVP_PKEY_get0(pkey);
		if (eckey == NULL)
			return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
		if (!EC_KEY_set_private_key(eckey, privkey))
			DST_RET(ISC_R_NOMEMORY);

		/* have to (re)set the public key */
#ifdef notyet
		(void) gost2001_compute_public(eckey);
#else
		if ((pubkey != NULL) && !EC_KEY_set_public_key(eckey, pubkey))
			DST_RET(ISC_R_NOMEMORY);
#endif
		BN_clear_free(privkey);
		privkey = NULL;
	}
	key->keydata.pkey = pkey;
	key->key_size = EVP_PKEY_bits(pkey);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ISC_R_SUCCESS);

 err:
	if (privkey != NULL)
		BN_clear_free(privkey);
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	opensslgost_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static void
opensslgost_cleanup(void) {
	if (e != NULL) {
		ENGINE_finish(e);
		ENGINE_free(e);
		e = NULL;
	}
}

static dst_func_t opensslgost_functions = {
	opensslgost_createctx,
	NULL, /*%< createctx2 */
	opensslgost_destroyctx,
	opensslgost_adddata,
	opensslgost_sign,
	opensslgost_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	opensslgost_compare,
	NULL, /*%< paramcompare */
	opensslgost_generate,
	opensslgost_isprivate,
	opensslgost_destroy,
	opensslgost_todns,
	opensslgost_fromdns,
	opensslgost_tofile,
	opensslgost_parse,
	opensslgost_cleanup,
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL  /*%< restore */
};

isc_result_t
dst__opensslgost_init(dst_func_t **funcp) {
	isc_result_t ret;

	REQUIRE(funcp != NULL);

	/* check if the gost engine works properly */
	e = ENGINE_by_id("gost");
	if (e == NULL)
		return (dst__openssl_toresult2("ENGINE_by_id",
					       DST_R_OPENSSLFAILURE));
	if (ENGINE_init(e) <= 0) {
		ENGINE_free(e);
		e = NULL;
		return (dst__openssl_toresult2("ENGINE_init",
					       DST_R_OPENSSLFAILURE));
	}
	/* better than to rely on digest_gost symbol */
	opensslgost_digest = ENGINE_get_digest(e, NID_id_GostR3411_94);
	if (opensslgost_digest == NULL)
		DST_RET(dst__openssl_toresult2("ENGINE_get_digest",
					       DST_R_OPENSSLFAILURE));
	/* from openssl.cnf */
	if (ENGINE_register_pkey_asn1_meths(e) <= 0)
		DST_RET(dst__openssl_toresult2(
				"ENGINE_register_pkey_asn1_meths",
				DST_R_OPENSSLFAILURE));
	if (ENGINE_ctrl_cmd_string(e,
				   "CRYPT_PARAMS",
				   "id-Gost28147-89-CryptoPro-A-ParamSet",
				   0) <= 0)
		DST_RET(dst__openssl_toresult2("ENGINE_ctrl_cmd_string",
					       DST_R_OPENSSLFAILURE));

	if (*funcp == NULL)
		*funcp = &opensslgost_functions;
	return (ISC_R_SUCCESS);

 err:
	ENGINE_finish(e);
	ENGINE_free(e);
	e = NULL;
	return (ret);
}

#else /* HAVE_OPENSSL_GOST */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* HAVE_OPENSSL_GOST */
/*! \file */
