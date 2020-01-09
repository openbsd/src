/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
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

#if defined(OPENSSL) && \
    (defined(HAVE_OPENSSL_ED25519) || defined(HAVE_OPENSSL_ED448))


#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/sha2.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#ifndef NID_ED25519
#error "Ed25519 group is not known (NID_ED25519)"
#endif
#ifndef NID_ED448
#error "Ed448 group is not known (NID_ED448)"
#endif

#define DST_RET(a) {ret = a; goto err;}

/* OpenSSL doesn't provide direct access to key values */

#define PUBPREFIXLEN	12

static const unsigned char ed25519_pub_prefix[] = {
	0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65,
	0x70, 0x03, 0x21, 0x00
};

static EVP_PKEY *pub_ed25519_to_ossl(const unsigned char *key)
{
	unsigned char buf[PUBPREFIXLEN + DNS_KEY_ED25519SIZE];
	const unsigned char *p;

	memmove(buf, ed25519_pub_prefix, PUBPREFIXLEN);
	memmove(buf + PUBPREFIXLEN, key, DNS_KEY_ED25519SIZE);
	p = buf;
	return (d2i_PUBKEY(NULL, &p, PUBPREFIXLEN + DNS_KEY_ED25519SIZE));
}

static isc_result_t pub_ed25519_from_ossl(EVP_PKEY *pkey,
					  unsigned char *key)
{
	unsigned char buf[PUBPREFIXLEN + DNS_KEY_ED25519SIZE];
	unsigned char *p;
	int len;

	len = i2d_PUBKEY(pkey, NULL);
	if ((len <= DNS_KEY_ED25519SIZE) ||
	    (len > PUBPREFIXLEN + DNS_KEY_ED25519SIZE))
		return (DST_R_OPENSSLFAILURE);
	p = buf;
	len = i2d_PUBKEY(pkey, &p);
	if ((len <= DNS_KEY_ED25519SIZE) ||
	    (len > PUBPREFIXLEN + DNS_KEY_ED25519SIZE))
		return (DST_R_OPENSSLFAILURE);
	memmove(key, buf + len - DNS_KEY_ED25519SIZE, DNS_KEY_ED25519SIZE);
	return (ISC_R_SUCCESS);
}

static const unsigned char ed448_pub_prefix[] = {
	0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65,
	0x71, 0x03, 0x21, 0x00
};

static EVP_PKEY *pub_ed448_to_ossl(const unsigned char *key)
{
	unsigned char buf[PUBPREFIXLEN + DNS_KEY_ED448SIZE];
	const unsigned char *p;

	memmove(buf, ed448_pub_prefix, PUBPREFIXLEN);
	memmove(buf + PUBPREFIXLEN, key, DNS_KEY_ED448SIZE);
	p = buf;
	return (d2i_PUBKEY(NULL, &p, PUBPREFIXLEN + DNS_KEY_ED448SIZE));
}

static isc_result_t pub_ed448_from_ossl(EVP_PKEY *pkey,
					unsigned char *key)
{
	unsigned char buf[PUBPREFIXLEN + DNS_KEY_ED448SIZE];
	unsigned char *p;
	int len;

	len = i2d_PUBKEY(pkey, NULL);
	if ((len <= DNS_KEY_ED448SIZE) ||
	    (len > PUBPREFIXLEN + DNS_KEY_ED448SIZE))
		return (DST_R_OPENSSLFAILURE);
	p = buf;
	len = i2d_PUBKEY(pkey, &p);
	if ((len <= DNS_KEY_ED448SIZE) ||
	    (len > PUBPREFIXLEN + DNS_KEY_ED448SIZE))
		return (DST_R_OPENSSLFAILURE);
	memmove(key, buf + len - DNS_KEY_ED448SIZE, DNS_KEY_ED448SIZE);
	return (ISC_R_SUCCESS);
}

#define PRIVPREFIXLEN	16

static const unsigned char ed25519_priv_prefix[] = {
	0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06,
	0x03, 0x2b, 0x65, 0x70, 0x04, 0x22, 0x04, 0x20
};

static EVP_PKEY *priv_ed25519_to_ossl(const unsigned char *key)
{
	unsigned char buf[PRIVPREFIXLEN + DNS_KEY_ED25519SIZE];
	const unsigned char *p;

	memmove(buf, ed25519_priv_prefix, PRIVPREFIXLEN);
	memmove(buf + PRIVPREFIXLEN, key, DNS_KEY_ED25519SIZE);
	p = buf;
	return (d2i_PrivateKey(NID_ED25519, NULL, &p,
			       PRIVPREFIXLEN + DNS_KEY_ED25519SIZE));
}

static isc_result_t priv_ed25519_from_ossl(EVP_PKEY *pkey,
					  unsigned char *key)
{
	unsigned char buf[PRIVPREFIXLEN + DNS_KEY_ED25519SIZE];
	unsigned char *p;
	int len;

	len = i2d_PrivateKey(pkey, NULL);
	if ((len <= DNS_KEY_ED25519SIZE) ||
	    (len > PRIVPREFIXLEN + DNS_KEY_ED25519SIZE))
		return (DST_R_OPENSSLFAILURE);
	p = buf;
	len = i2d_PrivateKey(pkey, &p);
	if ((len <= DNS_KEY_ED25519SIZE) ||
	    (len > PRIVPREFIXLEN + DNS_KEY_ED25519SIZE))
		return (DST_R_OPENSSLFAILURE);
	memmove(key, buf + len - DNS_KEY_ED25519SIZE, DNS_KEY_ED25519SIZE);
	return (ISC_R_SUCCESS);
}

static const unsigned char ed448_priv_prefix[] = {
	0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06,
	0x03, 0x2b, 0x65, 0x71, 0x04, 0x22, 0x04, 0x20
};

static EVP_PKEY *priv_ed448_to_ossl(const unsigned char *key)
{
	unsigned char buf[PRIVPREFIXLEN + DNS_KEY_ED448SIZE];
	const unsigned char *p;

	memmove(buf, ed448_priv_prefix, PRIVPREFIXLEN);
	memmove(buf + PRIVPREFIXLEN, key, DNS_KEY_ED448SIZE);
	p = buf;
	return (d2i_PrivateKey(NID_ED448, NULL, &p,
			       PRIVPREFIXLEN + DNS_KEY_ED448SIZE));
}

static isc_result_t priv_ed448_from_ossl(EVP_PKEY *pkey,
					unsigned char *key)
{
	unsigned char buf[PRIVPREFIXLEN + DNS_KEY_ED448SIZE];
	unsigned char *p;
	int len;

	len = i2d_PrivateKey(pkey, NULL);
	if ((len <= DNS_KEY_ED448SIZE) ||
	    (len > PRIVPREFIXLEN + DNS_KEY_ED448SIZE))
		return (DST_R_OPENSSLFAILURE);
	p = buf;
	len = i2d_PrivateKey(pkey, &p);
	if ((len <= DNS_KEY_ED448SIZE) ||
	    (len > PRIVPREFIXLEN + DNS_KEY_ED448SIZE))
		return (DST_R_OPENSSLFAILURE);
	memmove(key, buf + len - DNS_KEY_ED448SIZE, DNS_KEY_ED448SIZE);
	return (ISC_R_SUCCESS);
}

static isc_result_t openssleddsa_todns(const dst_key_t *key,
				       isc_buffer_t *data);

static isc_result_t
openssleddsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_buffer_t *buf = NULL;
	isc_result_t result;

	UNUSED(key);
	REQUIRE(dctx->key->key_alg == DST_ALG_ED25519 ||
		dctx->key->key_alg == DST_ALG_ED448);

	result = isc_buffer_allocate(dctx->mctx, &buf, 64);
	dctx->ctxdata.generic = buf;

	return (result);
}

static void
openssleddsa_destroyctx(dst_context_t *dctx) {
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;

	REQUIRE(dctx->key->key_alg == DST_ALG_ED25519 ||
		dctx->key->key_alg == DST_ALG_ED448);
	if (buf != NULL)
		isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;
}

static isc_result_t
openssleddsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;
	isc_buffer_t *nbuf = NULL;
	isc_region_t r;
	unsigned int length;
	isc_result_t result;

	REQUIRE(dctx->key->key_alg == DST_ALG_ED25519 ||
		dctx->key->key_alg == DST_ALG_ED448);

	result = isc_buffer_copyregion(buf, data);
	if (result == ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	length = isc_buffer_length(buf) + data->length + 64;
	result = isc_buffer_allocate(dctx->mctx, &nbuf, length);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(buf, &r);
	(void) isc_buffer_copyregion(nbuf, &r);
	(void) isc_buffer_copyregion(nbuf, data);
	isc_buffer_free(&buf);
	dctx->ctxdata.generic = nbuf;

	return (ISC_R_SUCCESS);
}

static isc_result_t
openssleddsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_result_t ret;
	dst_key_t *key = dctx->key;
	isc_region_t tbsreg;
	isc_region_t sigreg;
	EVP_PKEY *pkey = key->keydata.pkey;
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;
	size_t siglen;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	if (ctx == NULL)
		return (ISC_R_NOMEMORY);

	if (key->key_alg == DST_ALG_ED25519)
		siglen = DNS_SIG_ED25519SIZE;
	else
		siglen = DNS_SIG_ED448SIZE;

	isc_buffer_availableregion(sig, &sigreg);
	if (sigreg.length < (unsigned int) siglen)
		DST_RET(ISC_R_NOSPACE);

	isc_buffer_usedregion(buf, &tbsreg);

	if (!EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey))
		DST_RET(dst__openssl_toresult3(dctx->category,
					       "EVP_DigestSignInit",
					       ISC_R_FAILURE));
	if (!EVP_DigestSign(ctx, sigreg.base, &siglen,
			    tbsreg.base, tbsreg.length))
		DST_RET(dst__openssl_toresult3(dctx->category,
					       "EVP_DigestSign",
					       DST_R_SIGNFAILURE));
	isc_buffer_add(sig, (unsigned int) siglen);
	ret = ISC_R_SUCCESS;

 err:
	if (ctx != NULL)
		EVP_MD_CTX_free(ctx);
	isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;

	return (ret);
}

static isc_result_t
openssleddsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_result_t ret;
	dst_key_t *key = dctx->key;
	int status;
	isc_region_t tbsreg;
	EVP_PKEY *pkey = key->keydata.pkey;
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;
	unsigned int siglen;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	if (ctx == NULL)
		return (ISC_R_NOMEMORY);

	if (key->key_alg == DST_ALG_ED25519)
		siglen = DNS_SIG_ED25519SIZE;
	else
		siglen = DNS_SIG_ED448SIZE;

	if (sig->length != siglen)
		return (DST_R_VERIFYFAILURE);

	isc_buffer_usedregion(buf, &tbsreg);

	if (!EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey))
		DST_RET(dst__openssl_toresult3(dctx->category,
					       "EVP_DigestVerifyInit",
					       ISC_R_FAILURE));

	status = EVP_DigestVerify(ctx, sig->base, siglen,
				  tbsreg.base, tbsreg.length);

	switch (status) {
	case 1:
		ret = ISC_R_SUCCESS;
		break;
	case 0:
		ret = dst__openssl_toresult(DST_R_VERIFYFAILURE);
		break;
	default:
		ret = dst__openssl_toresult3(dctx->category,
					     "EVP_DigestVerify",
					     DST_R_VERIFYFAILURE);
		break;
	}

 err:
	if (ctx != NULL)
		EVP_MD_CTX_free(ctx);
	isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;

	return (ret);
}

static isc_boolean_t
openssleddsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	int status;
	EVP_PKEY *pkey1 = key1->keydata.pkey;
	EVP_PKEY *pkey2 = key2->keydata.pkey;

	if (pkey1 == NULL && pkey2 == NULL)
		return (ISC_TRUE);
	else if (pkey1 == NULL || pkey2 == NULL)
		return (ISC_FALSE);

	status = EVP_PKEY_cmp(pkey1, pkey2);
	if (status == 1)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static isc_result_t
openssleddsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	isc_result_t ret;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	int nid, status;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);
	UNUSED(unused);
	UNUSED(callback);

	if (key->key_alg == DST_ALG_ED25519) {
		nid = NID_ED25519;
		key->key_size = DNS_KEY_ED25519SIZE;
	} else {
		nid = NID_ED448;
		key->key_size = DNS_KEY_ED448SIZE;
	}

	ctx = EVP_PKEY_CTX_new_id(nid, NULL);
	if (ctx == NULL)
		return (dst__openssl_toresult2("EVP_PKEY_CTX_new_id",
					       DST_R_OPENSSLFAILURE));

	status = EVP_PKEY_keygen_init(ctx);
	if (status != 1)
		DST_RET (dst__openssl_toresult2("EVP_PKEY_keygen_init",
						DST_R_OPENSSLFAILURE));

	status = EVP_PKEY_keygen(ctx, &pkey);
	if (status != 1)
		DST_RET (dst__openssl_toresult2("EVP_PKEY_keygen",
						DST_R_OPENSSLFAILURE));

	key->keydata.pkey = pkey;
	ret = ISC_R_SUCCESS;

 err:
	if (ctx != NULL)
		EVP_PKEY_CTX_free(ctx);
	return (ret);
}

static isc_boolean_t
openssleddsa_isprivate(const dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;
	int len;
	unsigned long err;

	if (pkey == NULL)
		return (ISC_FALSE);

	len = i2d_PrivateKey(pkey, NULL);
	if (len > 0)
		return (ISC_TRUE);
	/* can check if first error is EC_R_INVALID_PRIVATE_KEY */
	while ((err = ERR_get_error()) != 0)
		/**/;

	return (ISC_FALSE);
}

static void
openssleddsa_destroy(dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;

	EVP_PKEY_free(pkey);
	key->keydata.pkey = NULL;
}

static isc_result_t
openssleddsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	EVP_PKEY *pkey = key->keydata.pkey;
	isc_region_t r;
	isc_result_t result;

	REQUIRE(pkey != NULL);

	pkey = key->keydata.pkey;
	switch (key->key_alg) {
	case DST_ALG_ED25519:
		isc_buffer_availableregion(data, &r);
		if (r.length < DNS_KEY_ED25519SIZE)
			return (ISC_R_NOSPACE);
		result = pub_ed25519_from_ossl(pkey, r.base);
		if (result == ISC_R_SUCCESS)
			isc_buffer_add(data, DNS_KEY_ED25519SIZE);
		return (result);
	case DST_ALG_ED448:
		isc_buffer_availableregion(data, &r);
		if (r.length < DNS_KEY_ED448SIZE)
			return (ISC_R_NOSPACE);
		result = pub_ed448_from_ossl(pkey, r.base);
		if (result == ISC_R_SUCCESS)
			isc_buffer_add(data, DNS_KEY_ED448SIZE);
		return (result);
	default:
		INSIST(0);
	}
}

static isc_result_t
openssleddsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	EVP_PKEY *pkey;
	isc_region_t r;
	unsigned int len;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	if (key->key_alg == DST_ALG_ED25519) {
		len = DNS_KEY_ED25519SIZE;
		if (r.length < len)
			return (DST_R_INVALIDPUBLICKEY);
		pkey = pub_ed25519_to_ossl(r.base);
	} else {
		len = DNS_KEY_ED448SIZE;
		if (r.length < len)
			return (DST_R_INVALIDPUBLICKEY);
		pkey = pub_ed448_to_ossl(r.base);
	}
	if (pkey == NULL)
		return (dst__openssl_toresult(ISC_R_FAILURE));
	isc_buffer_forward(data, len);
	key->keydata.pkey = pkey;
	key->key_size = len;
	return (ISC_R_SUCCESS);
}

static isc_result_t
openssleddsa_tofile(const dst_key_t *key, const char *directory) {
	isc_result_t ret;
	EVP_PKEY *pkey;
	dst_private_t priv;
	unsigned char *buf = NULL;
	unsigned int len;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	pkey = key->keydata.pkey;
	if (key->key_alg == DST_ALG_ED25519) {
		len = DNS_KEY_ED25519SIZE;
		buf = isc_mem_get(key->mctx, len);
		if (buf == NULL)
			return (ISC_R_NOMEMORY);
		priv.elements[0].tag = TAG_EDDSA_PRIVATEKEY;
		priv.elements[0].length = len;
		ret = priv_ed25519_from_ossl(pkey, buf);
		if (ret != ISC_R_SUCCESS)
			DST_RET (dst__openssl_toresult(ret));
		priv.elements[0].data = buf;
		priv.nelements = 1;
		ret = dst__privstruct_writefile(key, &priv, directory);
	} else {
		len = DNS_KEY_ED448SIZE;
		buf = isc_mem_get(key->mctx, len);
		if (buf == NULL)
			return (ISC_R_NOMEMORY);
		priv.elements[0].tag = TAG_EDDSA_PRIVATEKEY;
		priv.elements[0].length = len;
		ret = priv_ed448_from_ossl(pkey, buf);
		if (ret != ISC_R_SUCCESS)
			DST_RET (dst__openssl_toresult(ret));
		priv.elements[0].data = buf;
		priv.nelements = 1;
		ret = dst__privstruct_writefile(key, &priv, directory);
	}

 err:
	if (buf != NULL)
		isc_mem_put(key->mctx, buf, len);
	return (ret);
}

static isc_result_t
eddsa_check(EVP_PKEY *privkey, dst_key_t *pub)
{
	EVP_PKEY *pkey;

	if (pub == NULL)
		return (ISC_R_SUCCESS);
	pkey = pub->keydata.pkey;
	if (pkey == NULL)
		return (ISC_R_SUCCESS);
	if (EVP_PKEY_cmp(privkey, pkey) == 1)
		return (ISC_R_SUCCESS);
	return (ISC_R_FAILURE);
}

static isc_result_t
openssleddsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	EVP_PKEY *pkey = NULL;
	unsigned int len;
	isc_mem_t *mctx = key->mctx;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_ED25519, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (key->external) {
		if (priv.nelements != 0)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (pub == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		key->keydata.pkey = pub->keydata.pkey;
		pub->keydata.pkey = NULL;
		dst__privstruct_free(&priv, mctx);
		isc_safe_memwipe(&priv, sizeof(priv));
		return (ISC_R_SUCCESS);
	}

	if (key->key_alg == DST_ALG_ED25519) {
		len = DNS_KEY_ED25519SIZE;
		if (priv.elements[0].length < len)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		pkey = priv_ed25519_to_ossl(priv.elements[0].data);
	} else {
		len = DNS_KEY_ED448SIZE;
		if (priv.elements[0].length < len)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		pkey = priv_ed448_to_ossl(priv.elements[0].data);
	}
	if (pkey == NULL)
		DST_RET (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	if (eddsa_check(pkey, pub) != ISC_R_SUCCESS) {
		EVP_PKEY_free(pkey);
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	}
	key->keydata.pkey = pkey;
	key->key_size = len;
	ret = ISC_R_SUCCESS;

 err:
	dst__privstruct_free(&priv, mctx);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (ret);
}

static dst_func_t openssleddsa_functions = {
	openssleddsa_createctx,
	NULL, /*%< createctx2 */
	openssleddsa_destroyctx,
	openssleddsa_adddata,
	openssleddsa_sign,
	openssleddsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	openssleddsa_compare,
	NULL, /*%< paramcompare */
	openssleddsa_generate,
	openssleddsa_isprivate,
	openssleddsa_destroy,
	openssleddsa_todns,
	openssleddsa_fromdns,
	openssleddsa_tofile,
	openssleddsa_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__openssleddsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &openssleddsa_functions;
	return (ISC_R_SUCCESS);
}

#else /* HAVE_OPENSSL_EDxxx */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* HAVE_OPENSSL_EDxxx */
/*! \file */
