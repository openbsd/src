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

#ifdef OPENSSL
#ifndef USE_EVP
#define USE_EVP 1
#endif

#include <config.h>

#include <pk11/site.h>

#ifndef PK11_DSA_DISABLE

#include <string.h>

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/sha1.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"

#include <openssl/dsa.h>

static isc_result_t openssldsa_todns(const dst_key_t *key, isc_buffer_t *data);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static void
DSA_get0_pqg(const DSA *d, const BIGNUM **p, const BIGNUM **q,
	     const BIGNUM **g)
{
	if (p != NULL)
		*p = d->p;
	if (q != NULL)
		*q = d->q;
	if (g != NULL)
		*g = d->g;
}

static int
DSA_set0_pqg(DSA *d, BIGNUM *p, BIGNUM *q, BIGNUM *g) {
	if (p == NULL || q == NULL || g == NULL)
		return 0;
	BN_free(d->p);
	BN_free(d->q);
	BN_free(d->g);
	d->p = p;
	d->q = q;
	d->g = g;

	return 1;
}

static void
DSA_get0_key(const DSA *d, const BIGNUM **pub_key, const BIGNUM **priv_key) {
	if (pub_key != NULL)
		*pub_key = d->pub_key;
	if (priv_key != NULL)
		*priv_key = d->priv_key;
}

static int
DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key) {
	/* Note that it is valid for priv_key to be NULL */
	if (pub_key == NULL)
		return 0;

	BN_free(d->pub_key);
	BN_free(d->priv_key);
	d->pub_key = pub_key;
	d->priv_key = priv_key;

	return 1;
}

static void
DSA_SIG_get0(const DSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps) {
	*pr = sig->r;
	*ps = sig->s;
}

static int
DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s) {
	if (r == NULL || s == NULL)
		return 0;

	BN_clear_free(sig->r);
	BN_clear_free(sig->s);
	sig->r = r;
	sig->s = s;

	return 1;
}


#define DSA_clear_flags(d, x) (d)->flags &= ~(x)

#endif

static isc_result_t
openssldsa_createctx(dst_key_t *key, dst_context_t *dctx) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx;

	UNUSED(key);

	evp_md_ctx = EVP_MD_CTX_create();
	if (evp_md_ctx == NULL)
		return (ISC_R_NOMEMORY);

	if (!EVP_DigestInit_ex(evp_md_ctx, EVP_dss1(), NULL)) {
		EVP_MD_CTX_destroy(evp_md_ctx);
			return (ISC_R_FAILURE);
	}

	dctx->ctxdata.evp_md_ctx = evp_md_ctx;

	return (ISC_R_SUCCESS);
#else
	isc_sha1_t *sha1ctx;

	UNUSED(key);

	sha1ctx = isc_mem_get(dctx->mctx, sizeof(isc_sha1_t));
	if (sha1ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_sha1_init(sha1ctx);
	dctx->ctxdata.sha1ctx = sha1ctx;
	return (ISC_R_SUCCESS);
#endif
}

static void
openssldsa_destroyctx(dst_context_t *dctx) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	if (evp_md_ctx != NULL) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		dctx->ctxdata.evp_md_ctx = NULL;
	}
#else
	isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

	if (sha1ctx != NULL) {
		isc_sha1_invalidate(sha1ctx);
		isc_mem_put(dctx->mctx, sha1ctx, sizeof(isc_sha1_t));
		dctx->ctxdata.sha1ctx = NULL;
	}
#endif
}

static isc_result_t
openssldsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	if (!EVP_DigestUpdate(evp_md_ctx, data->base, data->length)) {
		return (ISC_R_FAILURE);
	}
#else
	isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

	isc_sha1_update(sha1ctx, data->base, data->length);
#endif
	return (ISC_R_SUCCESS);
}

static int
BN_bn2bin_fixed(const BIGNUM *bn, unsigned char *buf, int size) {
	int bytes = size - BN_num_bytes(bn);
	while (bytes-- > 0)
		*buf++ = 0;
	BN_bn2bin(bn, buf);
	return (size);
}

static isc_result_t
openssldsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key = dctx->key;
	DSA *dsa = key->keydata.dsa;
	isc_region_t region;
	DSA_SIG *dsasig;
	const BIGNUM *r = 0, *s = NULL;
	unsigned int klen;
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey;
	unsigned char *sigbuf;
	const unsigned char *sb;
	unsigned int siglen;
#else
	isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;
	unsigned char digest[ISC_SHA1_DIGESTLENGTH];
#endif

	isc_buffer_availableregion(sig, &region);
	if (region.length < ISC_SHA1_DIGESTLENGTH * 2 + 1)
		return (ISC_R_NOSPACE);

#if USE_EVP
	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		return (ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_DSA(pkey, dsa)) {
		EVP_PKEY_free(pkey);
		return (ISC_R_FAILURE);
	}
	sigbuf = malloc(EVP_PKEY_size(pkey));
	if (sigbuf == NULL) {
		EVP_PKEY_free(pkey);
		return (ISC_R_NOMEMORY);
	}
	if (!EVP_SignFinal(evp_md_ctx, sigbuf, &siglen, pkey)) {
		EVP_PKEY_free(pkey);
		free(sigbuf);
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_SignFinal",
					       ISC_R_FAILURE));
	}
	INSIST(EVP_PKEY_size(pkey) >= (int) siglen);
	EVP_PKEY_free(pkey);
	/* Convert from Dss-Sig-Value (RFC2459). */
	dsasig = DSA_SIG_new();
	if (dsasig == NULL) {
		free(sigbuf);
		return (ISC_R_NOMEMORY);
	}
	sb = sigbuf;
	if (d2i_DSA_SIG(&dsasig, &sb, (long) siglen) == NULL) {
		free(sigbuf);
		return (dst__openssl_toresult3(dctx->category,
					       "d2i_DSA_SIG",
					       ISC_R_FAILURE));
	}
	free(sigbuf);

#elif 0
	/* Only use EVP for the Digest */
	if (!EVP_DigestFinal_ex(evp_md_ctx, digest, &siglen)) {
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_DigestFinal_ex",
					       ISC_R_FAILURE));
	}
	dsasig = DSA_do_sign(digest, ISC_SHA1_DIGESTLENGTH, dsa);
	if (dsasig == NULL)
		return (dst__openssl_toresult3(dctx->category,
					       "DSA_do_sign",
					       DST_R_SIGNFAILURE));
#else
	isc_sha1_final(sha1ctx, digest);

	dsasig = DSA_do_sign(digest, ISC_SHA1_DIGESTLENGTH, dsa);
	if (dsasig == NULL)
		return (dst__openssl_toresult3(dctx->category,
					       "DSA_do_sign",
					       DST_R_SIGNFAILURE));
#endif

	klen = (key->key_size - 512)/64;
	if (klen > 255)
		return (ISC_R_FAILURE);
	*region.base = klen;
	isc_region_consume(&region, 1);

	DSA_SIG_get0(dsasig, &r, &s);
	BN_bn2bin_fixed(r, region.base, ISC_SHA1_DIGESTLENGTH);
	isc_region_consume(&region, ISC_SHA1_DIGESTLENGTH);
	BN_bn2bin_fixed(s, region.base, ISC_SHA1_DIGESTLENGTH);
	isc_region_consume(&region, ISC_SHA1_DIGESTLENGTH);
	DSA_SIG_free(dsasig);
	isc_buffer_add(sig, ISC_SHA1_DIGESTLENGTH * 2 + 1);

	return (ISC_R_SUCCESS);
}

static isc_result_t
openssldsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	dst_key_t *key = dctx->key;
	DSA *dsa = key->keydata.dsa;
	BIGNUM *r = NULL, *s = NULL;
	int status = 0;
	unsigned char *cp = sig->base;
	DSA_SIG *dsasig;
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
#if 0
	EVP_PKEY *pkey;
	unsigned char *sigbuf;
#endif
	unsigned int siglen;
#else
	isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;
#endif
	unsigned char digest[ISC_SHA1_DIGESTLENGTH];


#if USE_EVP
#if 1
	/* Only use EVP for the digest */
	if (!EVP_DigestFinal_ex(evp_md_ctx, digest, &siglen)) {
		return (ISC_R_FAILURE);
	}
#endif
#else
	isc_sha1_final(sha1ctx, digest);
#endif

	if (sig->length != 2 * ISC_SHA1_DIGESTLENGTH + 1) {
		return (DST_R_VERIFYFAILURE);
	}

	cp++;	/*%< Skip T */
	dsasig = DSA_SIG_new();
	if (dsasig == NULL)
		return (ISC_R_NOMEMORY);
	r = BN_bin2bn(cp, ISC_SHA1_DIGESTLENGTH, NULL);
	cp += ISC_SHA1_DIGESTLENGTH;
	s = BN_bin2bn(cp, ISC_SHA1_DIGESTLENGTH, NULL);
	DSA_SIG_set0(dsasig, r, s);

#if 0
	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		return (ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_DSA(pkey, dsa)) {
		EVP_PKEY_free(pkey);
		return (ISC_R_FAILURE);
	}
	/* Convert to Dss-Sig-Value (RFC2459). */
	sigbuf = malloc(EVP_PKEY_size(pkey) + 50);
	if (sigbuf == NULL) {
		EVP_PKEY_free(pkey);
		return (ISC_R_NOMEMORY);
	}
	siglen = (unsigned) i2d_DSA_SIG(dsasig, &sigbuf);
	INSIST(EVP_PKEY_size(pkey) >= (int) siglen);
	status = EVP_VerifyFinal(evp_md_ctx, sigbuf, siglen, pkey);
	EVP_PKEY_free(pkey);
	free(sigbuf);
#else
	status = DSA_do_verify(digest, ISC_SHA1_DIGESTLENGTH, dsasig, dsa);
#endif
	DSA_SIG_free(dsasig);
	switch (status) {
	case 1:
		return (ISC_R_SUCCESS);
	case 0:
		return (dst__openssl_toresult(DST_R_VERIFYFAILURE));
	default:
		return (dst__openssl_toresult3(dctx->category,
					       "DSA_do_verify",
					       DST_R_VERIFYFAILURE));
	}
}

static isc_boolean_t
openssldsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	DSA *dsa1, *dsa2;
	const BIGNUM *pub_key1 = NULL, *priv_key1 = NULL;
	const BIGNUM *pub_key2 = NULL, *priv_key2 = NULL;
	const BIGNUM *p1 = NULL, *q1 = NULL, *g1 = NULL;
	const BIGNUM *p2 = NULL, *q2 = NULL, *g2 = NULL;

	dsa1 = key1->keydata.dsa;
	dsa2 = key2->keydata.dsa;

	if (dsa1 == NULL && dsa2 == NULL)
		return (ISC_TRUE);
	else if (dsa1 == NULL || dsa2 == NULL)
		return (ISC_FALSE);

	DSA_get0_key(dsa1, &pub_key1, &priv_key1);
	DSA_get0_key(dsa2, &pub_key2, &priv_key2);
	DSA_get0_pqg(dsa1, &p1, &q1, &g1);
	DSA_get0_pqg(dsa2, &p2, &q2, &g2);

	if (BN_cmp(p1, p2) != 0 || BN_cmp(q1, q2) != 0 ||
	    BN_cmp(g1, g2) != 0 || BN_cmp(pub_key1, pub_key2) != 0)
		return (ISC_FALSE);

	if (priv_key1 != NULL || priv_key2 != NULL) {
		if (priv_key1 == NULL || priv_key2 == NULL)
			return (ISC_FALSE);
		if (BN_cmp(priv_key1, priv_key2))
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

#if OPENSSL_VERSION_NUMBER > 0x00908000L
static int
progress_cb(int p, int n, BN_GENCB *cb) {
	union {
		void *dptr;
		void (*fptr)(int);
	} u;

	UNUSED(n);

	u.dptr = BN_GENCB_get_arg(cb);
	if (u.fptr != NULL)
		u.fptr(p);
	return (1);
}
#endif

static isc_result_t
openssldsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	DSA *dsa;
	unsigned char rand_array[ISC_SHA1_DIGESTLENGTH];
	isc_result_t result;
#if OPENSSL_VERSION_NUMBER > 0x00908000L
	BN_GENCB *cb;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	BN_GENCB _cb;
#endif
	union {
		void *dptr;
		void (*fptr)(int);
	} u;

#else

	UNUSED(callback);
#endif
	UNUSED(unused);

	result = dst__entropy_getdata(rand_array, sizeof(rand_array),
				      ISC_FALSE);
	if (result != ISC_R_SUCCESS)
		return (result);

#if OPENSSL_VERSION_NUMBER > 0x00908000L
	dsa = DSA_new();
	if (dsa == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	cb = BN_GENCB_new();
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
	if (cb == NULL) {
		DSA_free(dsa);
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
#endif
	if (callback == NULL) {
		BN_GENCB_set_old(cb, NULL, NULL);
	} else {
		u.fptr = callback;
		BN_GENCB_set(cb, &progress_cb, u.dptr);
	}

	if (!DSA_generate_parameters_ex(dsa, key->key_size, rand_array,
					ISC_SHA1_DIGESTLENGTH,  NULL, NULL,
					cb))
	{
		DSA_free(dsa);
		BN_GENCB_free(cb);
		return (dst__openssl_toresult2("DSA_generate_parameters_ex",
					       DST_R_OPENSSLFAILURE));
	}
	BN_GENCB_free(cb);
	cb = NULL;
#else
	dsa = DSA_generate_parameters(key->key_size, rand_array,
				      ISC_SHA1_DIGESTLENGTH, NULL, NULL,
				      NULL, NULL);
	if (dsa == NULL)
		return (dst__openssl_toresult2("DSA_generate_parameters",
					       DST_R_OPENSSLFAILURE));
#endif

	if (DSA_generate_key(dsa) == 0) {
		DSA_free(dsa);
		return (dst__openssl_toresult2("DSA_generate_key",
					       DST_R_OPENSSLFAILURE));
	}

	DSA_clear_flags(dsa, DSA_FLAG_CACHE_MONT_P);

	key->keydata.dsa = dsa;

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
openssldsa_isprivate(const dst_key_t *key) {
	DSA *dsa = key->keydata.dsa;
	const BIGNUM *priv_key = NULL;

	DSA_get0_key(dsa, NULL, &priv_key);
	return (ISC_TF(dsa != NULL && priv_key != NULL));
}

static void
openssldsa_destroy(dst_key_t *key) {
	DSA *dsa = key->keydata.dsa;
	DSA_free(dsa);
	key->keydata.dsa = NULL;
}


static isc_result_t
openssldsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	DSA *dsa;
	const BIGNUM *pub_key, *p = NULL, *q = NULL, *g = NULL;
	isc_region_t r;
	int dnslen;
	unsigned int t, p_bytes;

	REQUIRE(key->keydata.dsa != NULL);

	dsa = key->keydata.dsa;

	isc_buffer_availableregion(data, &r);

	DSA_get0_key(dsa, &pub_key, NULL);
	DSA_get0_pqg(dsa, &p, &q, &g);

	t = (BN_num_bytes(p) - 64) / 8;
	if (t > 8)
		return (DST_R_INVALIDPUBLICKEY);
	p_bytes = 64 + 8 * t;

	dnslen = 1 + (key->key_size * 3)/8 + ISC_SHA1_DIGESTLENGTH;
	if (r.length < (unsigned int) dnslen)
		return (ISC_R_NOSPACE);

	*r.base = t;
	isc_region_consume(&r, 1);

	BN_bn2bin_fixed(q, r.base, ISC_SHA1_DIGESTLENGTH);
	isc_region_consume(&r, ISC_SHA1_DIGESTLENGTH);
	BN_bn2bin_fixed(p, r.base, key->key_size/8);
	isc_region_consume(&r, p_bytes);
	BN_bn2bin_fixed(g, r.base, key->key_size/8);
	isc_region_consume(&r, p_bytes);
	BN_bn2bin_fixed(pub_key, r.base, key->key_size/8);
	isc_region_consume(&r, p_bytes);

	isc_buffer_add(data, dnslen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
openssldsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	DSA *dsa;
	BIGNUM *pub_key, *p, *q, *g;
	isc_region_t r;
	unsigned int t, p_bytes;
	isc_mem_t *mctx = key->mctx;

	UNUSED(mctx);

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	dsa = DSA_new();
	if (dsa == NULL)
		return (ISC_R_NOMEMORY);
	DSA_clear_flags(dsa, DSA_FLAG_CACHE_MONT_P);

	t = (unsigned int) *r.base;
	isc_region_consume(&r, 1);
	if (t > 8) {
		DSA_free(dsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	p_bytes = 64 + 8 * t;

	if (r.length < ISC_SHA1_DIGESTLENGTH + 3 * p_bytes) {
		DSA_free(dsa);
		return (DST_R_INVALIDPUBLICKEY);
	}

	q = BN_bin2bn(r.base, ISC_SHA1_DIGESTLENGTH, NULL);
	isc_region_consume(&r, ISC_SHA1_DIGESTLENGTH);

	p = BN_bin2bn(r.base, p_bytes, NULL);
	isc_region_consume(&r, p_bytes);

	g = BN_bin2bn(r.base, p_bytes, NULL);
	isc_region_consume(&r, p_bytes);

	pub_key = BN_bin2bn(r.base, p_bytes, NULL);
	isc_region_consume(&r, p_bytes);

	if (pub_key == NULL || p == NULL || q == NULL || g == NULL) {
		DSA_free(dsa);
		if (p != NULL) BN_free(p);
		if (q != NULL) BN_free(q);
		if (g != NULL) BN_free(g);
		return (ISC_R_NOMEMORY);
	}

	DSA_set0_key(dsa, pub_key, NULL);
	DSA_set0_pqg(dsa, p, q, g);

	key->key_size = p_bytes * 8;

	isc_buffer_forward(data, 1 + ISC_SHA1_DIGESTLENGTH + 3 * p_bytes);

	key->keydata.dsa = dsa;

	return (ISC_R_SUCCESS);
}


static isc_result_t
openssldsa_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	DSA *dsa;
	const BIGNUM *pub_key = NULL, *priv_key = NULL;
	const BIGNUM *p = NULL, *q = NULL, *g = NULL;
	dst_private_t priv;
	unsigned char bufs[5][128];

	if (key->keydata.dsa == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	dsa = key->keydata.dsa;

	DSA_get0_key(dsa, &pub_key, &priv_key);
	DSA_get0_pqg(dsa, &p, &q, &g);

	priv.elements[cnt].tag = TAG_DSA_PRIME;
	priv.elements[cnt].length = BN_num_bytes(p);
	BN_bn2bin(p, bufs[cnt]);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_SUBPRIME;
	priv.elements[cnt].length = BN_num_bytes(q);
	BN_bn2bin(q, bufs[cnt]);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_BASE;
	priv.elements[cnt].length = BN_num_bytes(g);
	BN_bn2bin(g, bufs[cnt]);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_PRIVATE;
	priv.elements[cnt].length = BN_num_bytes(priv_key);
	BN_bn2bin(priv_key, bufs[cnt]);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_PUBLIC;
	priv.elements[cnt].length = BN_num_bytes(pub_key);
	BN_bn2bin(pub_key, bufs[cnt]);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
openssldsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	DSA *dsa = NULL;
	BIGNUM *pub_key = NULL, *priv_key = NULL;
	BIGNUM *p = NULL, *q = NULL, *g = NULL;
	isc_mem_t *mctx = key->mctx;
#define DST_RET(a) {ret = a; goto err;}

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_DSA, lexer, mctx, &priv);
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
		isc_safe_memwipe(&priv, sizeof(priv));
		return (ISC_R_SUCCESS);
	}

	dsa = DSA_new();
	if (dsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	DSA_clear_flags(dsa, DSA_FLAG_CACHE_MONT_P);
	key->keydata.dsa = dsa;

	for (i = 0; i < priv.nelements; i++) {
		BIGNUM *bn;
		bn = BN_bin2bn(priv.elements[i].data,
			       priv.elements[i].length, NULL);
		if (bn == NULL)
			DST_RET(ISC_R_NOMEMORY);

		switch (priv.elements[i].tag) {
			case TAG_DSA_PRIME:
				p = bn;
				break;
			case TAG_DSA_SUBPRIME:
				q = bn;
				break;
			case TAG_DSA_BASE:
				g = bn;
				break;
			case TAG_DSA_PRIVATE:
				priv_key = bn;
				break;
			case TAG_DSA_PUBLIC:
				pub_key = bn;
				break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	isc_safe_memwipe(&priv, sizeof(priv));
	DSA_set0_key(dsa, pub_key, priv_key);
	DSA_set0_pqg(dsa, p, q, g);
	key->key_size = BN_num_bits(p);
	return (ISC_R_SUCCESS);

 err:
	if (p != NULL)
		BN_free(p);
	if (q != NULL)
		BN_free(q);
	if (g != NULL)
		BN_free(g);
	openssldsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (ret);
}

static dst_func_t openssldsa_functions = {
	openssldsa_createctx,
	NULL, /*%< createctx2 */
	openssldsa_destroyctx,
	openssldsa_adddata,
	openssldsa_sign,
	openssldsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	openssldsa_compare,
	NULL, /*%< paramcompare */
	openssldsa_generate,
	openssldsa_isprivate,
	openssldsa_destroy,
	openssldsa_todns,
	openssldsa_fromdns,
	openssldsa_tofile,
	openssldsa_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__openssldsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &openssldsa_functions;
	return (ISC_R_SUCCESS);
}
#endif /* !PK11_DSA_DISABLE */

#else /* OPENSSL */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* OPENSSL */
/*! \file */
