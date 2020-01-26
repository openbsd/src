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

/*
 * Principal Author: Brian Wellington
 */




#include <isc/md5.h>

#include <isc/safe.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <string.h>
#include <isc/util.h>



#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

/*
 * Limit the size of public exponents.
 */
#ifndef RSA_MAX_PUBEXP_BITS
#define RSA_MAX_PUBEXP_BITS    35
#endif

/*
 * We don't use configure for windows so enforce the OpenSSL version
 * here.  Unlike with configure we don't support overriding this test.
 */


	/*
	 * XXXMPA  Temporarily disable RSA_BLINDING as it requires
	 * good quality random data that cannot currently be guaranteed.
	 * XXXMPA  Find which versions of openssl use pseudo random data
	 * and set RSA_FLAG_BLINDING for those.
	 */

#define SET_FLAGS(rsa) \
	do { \
		RSA_clear_flags(rsa, RSA_FLAG_BLINDING); \
		RSA_set_flags(rsa, RSA_FLAG_NO_BLINDING); \
	} while (0)

#define DST_RET(a) {ret = a; goto err;}

static isc_result_t opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data);

static isc_result_t
opensslrsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx;
	const EVP_MD *type = NULL;

	UNUSED(key);
	REQUIRE(dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	/*
	 * Reject incorrect RSA key lengths.
	 */
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		/* From RFC 3110 */
		if (dctx->key->key_size > 4096)
			return (ISC_R_FAILURE);
		break;
	case DST_ALG_RSASHA256:
		/* From RFC 5702 */
		if ((dctx->key->key_size < 512) ||
		    (dctx->key->key_size > 4096))
			return (ISC_R_FAILURE);
		break;
	case DST_ALG_RSASHA512:
		/* From RFC 5702 */
		if ((dctx->key->key_size < 1024) ||
		    (dctx->key->key_size > 4096))
			return (ISC_R_FAILURE);
		break;
	default:
		INSIST(0);
	}

	evp_md_ctx = EVP_MD_CTX_create();
	if (evp_md_ctx == NULL)
		return (ISC_R_NOMEMORY);

	switch (dctx->key->key_alg) {
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		type = EVP_sha1();	/* SHA1 + RSA */
		break;
	case DST_ALG_RSASHA256:
		type = EVP_sha256();	/* SHA256 + RSA */
		break;
	case DST_ALG_RSASHA512:
		type = EVP_sha512();
		break;
	default:
		INSIST(0);
	}

	if (!EVP_DigestInit_ex(evp_md_ctx, type, NULL)) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_DigestInit_ex",
					       ISC_R_FAILURE));
	}
	dctx->ctxdata.evp_md_ctx = evp_md_ctx;

	return (ISC_R_SUCCESS);
}

static void
opensslrsa_destroyctx(dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	REQUIRE(dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	if (evp_md_ctx != NULL) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		dctx->ctxdata.evp_md_ctx = NULL;
	}
}

static isc_result_t
opensslrsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	REQUIRE(dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	if (!EVP_DigestUpdate(evp_md_ctx, data->base, data->length)) {
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_DigestUpdate",
					       ISC_R_FAILURE));
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key = dctx->key;
	isc_region_t r;
	unsigned int siglen = 0;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;

	REQUIRE(dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	isc_buffer_availableregion(sig, &r);

	if (r.length < (unsigned int) EVP_PKEY_size(pkey))
		return (ISC_R_NOSPACE);

	if (!EVP_SignFinal(evp_md_ctx, r.base, &siglen, pkey)) {
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_SignFinal",
					       ISC_R_FAILURE));
	}

	isc_buffer_add(sig, siglen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_verify2(dst_context_t *dctx, int maxbits, const isc_region_t *sig) {
	dst_key_t *key = dctx->key;
	int status = 0;
	const BIGNUM *e = NULL;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;
	RSA *rsa;
	int bits;

	REQUIRE(dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	RSA_get0_key(rsa, NULL, &e, NULL);
	bits = BN_num_bits(e);
	RSA_free(rsa);
	if (bits > maxbits && maxbits != 0)
		return (DST_R_VERIFYFAILURE);

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

static isc_result_t
opensslrsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	return (opensslrsa_verify2(dctx, 0, sig));
}

static isc_boolean_t
opensslrsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	int status;
	RSA *rsa1 = NULL, *rsa2 = NULL;
	const BIGNUM *n1 = NULL, *n2 = NULL;
	const BIGNUM *e1 = NULL, *e2 = NULL;
	const BIGNUM *d1 = NULL, *d2 = NULL;
	const BIGNUM *p1 = NULL, *p2 = NULL;
	const BIGNUM *q1 = NULL, *q2 = NULL;
	EVP_PKEY *pkey1, *pkey2;

	pkey1 = key1->keydata.pkey;
	pkey2 = key2->keydata.pkey;
	/*
	 * The pkey reference will keep these around after
	 * the RSA_free() call.
	 */
	if (pkey1 != NULL) {
		rsa1 = EVP_PKEY_get1_RSA(pkey1);
		RSA_free(rsa1);
	}
	if (pkey2 != NULL) {
		rsa2 = EVP_PKEY_get1_RSA(pkey2);
		RSA_free(rsa2);
	}

	if (rsa1 == NULL && rsa2 == NULL)
		return (ISC_TRUE);
	else if (rsa1 == NULL || rsa2 == NULL)
		return (ISC_FALSE);

	RSA_get0_key(rsa1, &n1, &e1, &d1);
	RSA_get0_key(rsa2, &n2, &e2, &d2);
	status = BN_cmp(n1, n2) || BN_cmp(e1, e2);

	if (status != 0)
		return (ISC_FALSE);

	if (RSA_test_flags(rsa1, RSA_FLAG_EXT_PKEY) != 0 ||
	    RSA_test_flags(rsa2, RSA_FLAG_EXT_PKEY) != 0) {
		if (RSA_test_flags(rsa1, RSA_FLAG_EXT_PKEY) == 0 ||
		    RSA_test_flags(rsa2, RSA_FLAG_EXT_PKEY) == 0)
			return (ISC_FALSE);
		/*
		 * Can't compare private parameters, BTW does it make sense?
		 */
		return (ISC_TRUE);
	}

	if (d1 != NULL || d2 != NULL) {
		if (d1 == NULL || d2 == NULL)
			return (ISC_FALSE);
		RSA_get0_factors(rsa1, &p1, &q1);
		RSA_get0_factors(rsa2, &p2, &q2);
		status = BN_cmp(d1, d2) || BN_cmp(p1, p1) || BN_cmp(q1, q2);

		if (status != 0)
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

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

static isc_result_t
opensslrsa_generate(dst_key_t *key, int exp, void (*callback)(int)) {
	isc_result_t ret = DST_R_OPENSSLFAILURE;
	union {
		void *dptr;
		void (*fptr)(int);
	} u;
	RSA *rsa = RSA_new();
	BIGNUM *e = BN_new();
	BN_GENCB *cb = BN_GENCB_new();
	EVP_PKEY *pkey = EVP_PKEY_new();

	/*
	 * Reject incorrect RSA key lengths.
	 */
	switch (key->key_alg) {
	case DST_ALG_RSAMD5:
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		/* From RFC 3110 */
		if (key->key_size > 4096)
			goto err;
		break;
	case DST_ALG_RSASHA256:
		/* From RFC 5702 */
		if ((key->key_size < 512) ||
		    (key->key_size > 4096))
			goto err;
		break;
	case DST_ALG_RSASHA512:
		/* From RFC 5702 */
		if ((key->key_size < 1024) ||
		    (key->key_size > 4096))
			goto err;
		break;
	default:
		INSIST(0);
	}

	if (rsa == NULL || e == NULL || cb == NULL)
		goto err;
	if (pkey == NULL)
		goto err;
	if (!EVP_PKEY_set1_RSA(pkey, rsa))
		goto err;

	if (exp == 0) {
		/* RSA_F4 0x10001 */
		BN_set_bit(e, 0);
		BN_set_bit(e, 16);
	} else {
		/* (phased-out) F5 0x100000001 */
		BN_set_bit(e, 0);
		BN_set_bit(e, 32);
	}

	if (callback == NULL) {
		BN_GENCB_set_old(cb, NULL, NULL);
	} else {
		u.fptr = callback;
		BN_GENCB_set(cb, &progress_cb, u.dptr);
	}

	if (RSA_generate_key_ex(rsa, key->key_size, e, cb)) {
		BN_free(e);
		BN_GENCB_free(cb);
		cb = NULL;
		SET_FLAGS(rsa);
		key->keydata.pkey = pkey;

		RSA_free(rsa);
		return (ISC_R_SUCCESS);
	}
	ret = dst__openssl_toresult2("RSA_generate_key_ex",
				     DST_R_OPENSSLFAILURE);

 err:
	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
		pkey = NULL;
	}
	if (e != NULL) {
		BN_free(e);
		e = NULL;
	}
	if (rsa != NULL) {
		RSA_free(rsa);
		rsa = NULL;
	}
	if (cb != NULL) {
		BN_GENCB_free(cb);
		cb = NULL;
	}
	return (dst__openssl_toresult(ret));
}

static isc_boolean_t
opensslrsa_isprivate(const dst_key_t *key) {
	const BIGNUM *d = NULL;
	RSA *rsa = EVP_PKEY_get1_RSA(key->keydata.pkey);
	INSIST(rsa != NULL);
	RSA_free(rsa);
	/* key->keydata.pkey still has a reference so rsa is still valid. */
	if (rsa != NULL && RSA_test_flags(rsa, RSA_FLAG_EXT_PKEY) != 0)
		return (ISC_TRUE);
	RSA_get0_key(rsa, NULL, NULL, &d);
	return (ISC_TF(rsa != NULL && d != NULL));
}

static void
opensslrsa_destroy(dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;
	EVP_PKEY_free(pkey);
	key->keydata.pkey = NULL;
}

static isc_result_t
opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	isc_region_t r;
	unsigned int e_bytes;
	unsigned int mod_bytes;
	isc_result_t ret;
	RSA *rsa;
	EVP_PKEY *pkey;
	const BIGNUM *e = NULL, *n = NULL;

	REQUIRE(key->keydata.pkey != NULL);

	pkey = key->keydata.pkey;
	rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));

	isc_buffer_availableregion(data, &r);

	RSA_get0_key(rsa, &n, &e, NULL);
	mod_bytes = BN_num_bytes(n);
	e_bytes = BN_num_bytes(e);

	if (e_bytes < 256) {	/*%< key exponent is <= 2040 bits */
		if (r.length < 1)
			DST_RET(ISC_R_NOSPACE);
		isc_buffer_putuint8(data, (uint8_t) e_bytes);
		isc_region_consume(&r, 1);
	} else {
		if (r.length < 3)
			DST_RET(ISC_R_NOSPACE);
		isc_buffer_putuint8(data, 0);
		isc_buffer_putuint16(data, (uint16_t) e_bytes);
		isc_region_consume(&r, 3);
	}

	if (r.length < e_bytes + mod_bytes)
		DST_RET(ISC_R_NOSPACE);

	RSA_get0_key(rsa, &n, &e, NULL);
	BN_bn2bin(e, r.base);
	isc_region_consume(&r, e_bytes);
	BN_bn2bin(n, r.base);

	isc_buffer_add(data, e_bytes + mod_bytes);

	ret = ISC_R_SUCCESS;
 err:
	if (rsa != NULL)
		RSA_free(rsa);
	return (ret);
}

static isc_result_t
opensslrsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	RSA *rsa;
	isc_region_t r;
	unsigned int e_bytes;
	unsigned int length;
	EVP_PKEY *pkey;
	BIGNUM *e = NULL, *n = NULL;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	length = r.length;

	rsa = RSA_new();
	if (rsa == NULL)
		return (dst__openssl_toresult(ISC_R_NOMEMORY));
	SET_FLAGS(rsa);

	if (r.length < 1) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	e_bytes = *r.base;
	isc_region_consume(&r, 1);

	if (e_bytes == 0) {
		if (r.length < 2) {
			RSA_free(rsa);
			return (DST_R_INVALIDPUBLICKEY);
		}
		e_bytes = (*r.base) << 8;
		isc_region_consume(&r, 1);
		e_bytes += *r.base;
		isc_region_consume(&r, 1);
	}

	if (r.length < e_bytes) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	e = BN_bin2bn(r.base, e_bytes, NULL);
	isc_region_consume(&r, e_bytes);
	n = BN_bin2bn(r.base, r.length, NULL);
	if (RSA_set0_key(rsa, n, e, NULL) == 0) {
		if (n != NULL) BN_free(n);
		if (e != NULL) BN_free(e);
		RSA_free(rsa);
		return (ISC_R_NOMEMORY);
	}
	key->key_size = BN_num_bits(n);

	isc_buffer_forward(data, length);

	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		RSA_free(rsa);
		return (ISC_R_NOMEMORY);
	}
	if (!EVP_PKEY_set1_RSA(pkey, rsa)) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	key->keydata.pkey = pkey;
	RSA_free(rsa);

	return (ISC_R_SUCCESS);
}

static isc_result_t
rsa_check(RSA *rsa, RSA *pub) {
	const BIGNUM *n1 = NULL, *n2 = NULL;
	const BIGNUM *e1 = NULL, *e2 = NULL;
	BIGNUM *n = NULL, *e = NULL;

	/*
	 * Public parameters should be the same but if they are not set
	 * copy them from the public key.
	 */
	RSA_get0_key(rsa, &n1, &e1, NULL);
	if (pub != NULL) {
		RSA_get0_key(pub, &n2, &e2, NULL);
		if (n1 != NULL) {
			if (BN_cmp(n1, n2) != 0)
				return (DST_R_INVALIDPRIVATEKEY);
		} else {
			n = BN_dup(n2);
		}
		if (e1 != NULL) {
			if (BN_cmp(e1, e2) != 0)
				return (DST_R_INVALIDPRIVATEKEY);
		} else {
			e = BN_dup(e2);
		}
		if (RSA_set0_key(rsa, n, e, NULL) == 0) {
			if (n != NULL)
				BN_free(n);
			if (e != NULL)
				BN_free(e);
		}
	}
	RSA_get0_key(rsa, &n1, &e1, NULL);
	if (n1 == NULL || e1 == NULL)
		return (DST_R_INVALIDPRIVATEKEY);
	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	RSA *rsa = NULL, *pubrsa = NULL;
	const char *engine = NULL, *label = NULL;
	EVP_PKEY *pkey = NULL;
	BIGNUM *n = NULL, *e = NULL, *d = NULL;
	BIGNUM *p = NULL, *q = NULL;
	BIGNUM *dmp1 = NULL, *dmq1 = NULL, *iqmp = NULL;

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_RSA, lexer, &priv);
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (key->external) {
		if (priv.nelements != 0)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (pub == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		key->keydata.pkey = pub->keydata.pkey;
		pub->keydata.pkey = NULL;
		key->key_size = pub->key_size;
		dst__privstruct_free(&priv);
		isc_safe_memwipe(&priv, sizeof(priv));
		return (ISC_R_SUCCESS);
	}

	if (pub != NULL && pub->keydata.pkey != NULL)
		pubrsa = EVP_PKEY_get1_RSA(pub->keydata.pkey);

	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_RSA_ENGINE:
			engine = (char *)priv.elements[i].data;
			break;
		case TAG_RSA_LABEL:
			label = (char *)priv.elements[i].data;
			break;
		default:
			break;
		}
	}

	/*
	 * Is this key is stored in a HSM?
	 * See if we can fetch it.
	 */
	if (label != NULL) {
		DST_RET(DST_R_NOENGINE);
	}

	rsa = RSA_new();
	if (rsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	SET_FLAGS(rsa);

	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		DST_RET(ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_RSA(pkey, rsa))
		DST_RET(ISC_R_FAILURE);
	key->keydata.pkey = pkey;

	for (i = 0; i < priv.nelements; i++) {
		BIGNUM *bn;
		switch (priv.elements[i].tag) {
		case TAG_RSA_ENGINE:
			continue;
		case TAG_RSA_LABEL:
			continue;
		default:
			bn = BN_bin2bn(priv.elements[i].data,
				       priv.elements[i].length, NULL);
			if (bn == NULL)
				DST_RET(ISC_R_NOMEMORY);
			switch (priv.elements[i].tag) {
			case TAG_RSA_MODULUS:
				n = bn;
				break;
			case TAG_RSA_PUBLICEXPONENT:
				e = bn;
				break;
			case TAG_RSA_PRIVATEEXPONENT:
				d = bn;
				break;
			case TAG_RSA_PRIME1:
				p = bn;
				break;
			case TAG_RSA_PRIME2:
				q = bn;
				break;
			case TAG_RSA_EXPONENT1:
				dmp1 = bn;
				break;
			case TAG_RSA_EXPONENT2:
				dmq1 = bn;
				break;
			case TAG_RSA_COEFFICIENT:
				iqmp = bn;
				break;
			}
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));

	if (RSA_set0_key(rsa, n, e, d) == 0) {
		if (n != NULL) BN_free(n);
		if (e != NULL) BN_free(e);
		if (d != NULL) BN_free(d);
	}
	if (RSA_set0_factors(rsa, p, q) == 0) {
		if (p != NULL) BN_free(p);
		if (q != NULL) BN_free(q);
	}
	if (RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp) == 0) {
		if (dmp1 != NULL) BN_free(dmp1);
		if (dmq1 != NULL) BN_free(dmq1);
		if (iqmp != NULL) BN_free(iqmp);
	}

	if (rsa_check(rsa, pubrsa) != ISC_R_SUCCESS)
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	if (BN_num_bits(e) > RSA_MAX_PUBEXP_BITS)
		DST_RET(ISC_R_RANGE);
	key->key_size = BN_num_bits(n);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
	RSA_free(rsa);

	return (ISC_R_SUCCESS);

 err:
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	if (rsa != NULL)
		RSA_free(rsa);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
	key->keydata.generic = NULL;
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (ret);
}

static isc_result_t
opensslrsa_fromlabel(dst_key_t *key, const char *engine, const char *label,
		     const char *pin)
{
	UNUSED(key);
	UNUSED(engine);
	UNUSED(label);
	UNUSED(pin);
	return(DST_R_NOENGINE);
}

static dst_func_t opensslrsa_functions = {
	opensslrsa_createctx,
	NULL, /*%< createctx2 */
	opensslrsa_destroyctx,
	opensslrsa_adddata,
	opensslrsa_sign,
	opensslrsa_verify,
	opensslrsa_verify2,
	NULL, /*%< computesecret */
	opensslrsa_compare,
	NULL, /*%< paramcompare */
	opensslrsa_generate,
	opensslrsa_isprivate,
	opensslrsa_destroy,
	opensslrsa_todns,
	opensslrsa_fromdns,
	NULL, /* opensslrsa_tofile */
	opensslrsa_parse,
	NULL, /*%< cleanup */
	opensslrsa_fromlabel,
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__opensslrsa_init(dst_func_t **funcp, unsigned char algorithm) {
	REQUIRE(funcp != NULL);

	if (*funcp == NULL) {
		switch (algorithm) {
		case DST_ALG_RSASHA256:
			*funcp = &opensslrsa_functions;
			break;
		case DST_ALG_RSASHA512:
			*funcp = &opensslrsa_functions;
			break;
		default:
			*funcp = &opensslrsa_functions;
			break;
		}
	}
	return (ISC_R_SUCCESS);
}

/*! \file */
