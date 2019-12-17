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

#ifdef PKCS11CRYPTO

#include <config.h>

#include <isc/entropy.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"
#include "dst_pkcs11.h"

#include <pk11/internal.h>
#include <pk11/site.h>

/*
 * Limit the size of public exponents.
 */
#ifndef RSA_MAX_PUBEXP_BITS
#define RSA_MAX_PUBEXP_BITS    35
#endif

#define DST_RET(a) {ret = a; goto err;}

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

static isc_result_t pkcs11rsa_todns(const dst_key_t *key, isc_buffer_t *data);
static void pkcs11rsa_destroy(dst_key_t *key);
static isc_result_t pkcs11rsa_fetch(dst_key_t *key, const char *engine,
				    const char *label, dst_key_t *pub);

#ifndef PK11_RSA_PKCS_REPLACE

static isc_result_t
pkcs11rsa_createctx_sign(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { 0, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS, NULL, 0 },
		{ CKA_PUBLIC_EXPONENT, NULL, 0 },
		{ CKA_PRIVATE_EXPONENT, NULL, 0 },
		{ CKA_PRIME_1, NULL, 0 },
		{ CKA_PRIME_2, NULL, 0 },
		{ CKA_EXPONENT_1, NULL, 0 },
		{ CKA_EXPONENT_2, NULL, 0 },
		{ CKA_COEFFICIENT, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	CK_SLOT_ID slotid;
	pk11_object_t *rsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

#ifndef PK11_MD5_DISABLE
	REQUIRE(key->key_alg == DST_ALG_RSAMD5 ||
		key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#else
	REQUIRE(key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#endif

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

	rsa = key->keydata.pkey;

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	if (rsa->ontoken)
		slotid = rsa->slot;
	else
		slotid = pk11_get_best_token(OP_RSA);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       rsa->reqlogon, NULL, slotid);
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (rsa->ontoken && (rsa->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = rsa->ontoken;
		pk11_ctx->object = rsa->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PUBLIC_EXPONENT:
			INSIST(keyTemplate[7].type == attr->type);
			keyTemplate[7].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[7].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[7].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[7].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIVATE_EXPONENT:
			INSIST(keyTemplate[8].type == attr->type);
			keyTemplate[8].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[8].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[8].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[8].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIME_1:
			INSIST(keyTemplate[9].type == attr->type);
			keyTemplate[9].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[9].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[9].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[9].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIME_2:
			INSIST(keyTemplate[10].type == attr->type);
			keyTemplate[10].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[10].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[10].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[10].ulValueLen = attr->ulValueLen;
			break;
		case CKA_EXPONENT_1:
			INSIST(keyTemplate[11].type == attr->type);
			keyTemplate[11].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[11].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[11].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[11].ulValueLen = attr->ulValueLen;
			break;
		case CKA_EXPONENT_2:
			INSIST(keyTemplate[12].type == attr->type);
			keyTemplate[12].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[12].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[12].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[12].ulValueLen = attr->ulValueLen;
			break;
		case CKA_COEFFICIENT:
			INSIST(keyTemplate[13].type == attr->type);
			keyTemplate[13].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[13].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[13].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[13].ulValueLen = attr->ulValueLen;
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 14,
		  &pk11_ctx->object),
		 ISC_R_FAILURE);

    token_key:

	switch (dctx->key->key_alg) {
#ifndef PK11_MD5_DISABLE
	case DST_ALG_RSAMD5:
		mech.mechanism = CKM_MD5_RSA_PKCS;
		break;
#endif
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		mech.mechanism = CKM_SHA1_RSA_PKCS;
		break;
	case DST_ALG_RSASHA256:
		mech.mechanism = CKM_SHA256_RSA_PKCS;
		break;
	case DST_ALG_RSASHA512:
		mech.mechanism = CKM_SHA512_RSA_PKCS;
		break;
	default:
		INSIST(0);
	}

	PK11_RET(pkcs_C_SignInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 6; i <= 13; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}

	return (ISC_R_SUCCESS);

    err:
	if (!pk11_ctx->ontoken && (pk11_ctx->object != CK_INVALID_HANDLE))
		(void) pkcs_C_DestroyObject(pk11_ctx->session,
					    pk11_ctx->object);
	for (i = 6; i <= 13; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_result_t
pkcs11rsa_createctx_verify(dst_key_t *key, unsigned int maxbits,
			   dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { 0, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS, NULL, 0 },
		{ CKA_PUBLIC_EXPONENT, NULL, 0 },
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *rsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

#ifndef PK11_MD5_DISABLE
	REQUIRE(key->key_alg == DST_ALG_RSAMD5 ||
		key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#else
	REQUIRE(key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#endif

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

	rsa = key->keydata.pkey;

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       rsa->reqlogon, NULL,
			       pk11_get_best_token(OP_RSA));
	if (ret != ISC_R_SUCCESS)
		goto err;

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PUBLIC_EXPONENT:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			if (pk11_numbits(attr->pValue,
					 attr->ulValueLen) > maxbits &&
			    maxbits != 0)
				DST_RET(DST_R_VERIFYFAILURE);
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 7,
		  &pk11_ctx->object),
		 ISC_R_FAILURE);

	switch (dctx->key->key_alg) {
#ifndef PK11_MD5_DISABLE
	case DST_ALG_RSAMD5:
		mech.mechanism = CKM_MD5_RSA_PKCS;
		break;
#endif
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		mech.mechanism = CKM_SHA1_RSA_PKCS;
		break;
	case DST_ALG_RSASHA256:
		mech.mechanism = CKM_SHA256_RSA_PKCS;
		break;
	case DST_ALG_RSASHA512:
		mech.mechanism = CKM_SHA512_RSA_PKCS;
		break;
	default:
		INSIST(0);
	}

	PK11_RET(pkcs_C_VerifyInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 5; i <= 6; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}

	return (ISC_R_SUCCESS);

    err:
	if (!pk11_ctx->ontoken && (pk11_ctx->object != CK_INVALID_HANDLE))
		(void) pkcs_C_DestroyObject(pk11_ctx->session,
					    pk11_ctx->object);
	for (i = 5; i <= 6; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_result_t
pkcs11rsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	if (dctx->use == DO_SIGN)
		return (pkcs11rsa_createctx_sign(key, dctx));
	else
		return (pkcs11rsa_createctx_verify(key, 0U, dctx));
}

static isc_result_t
pkcs11rsa_createctx2(dst_key_t *key, int maxbits, dst_context_t *dctx) {
	if (dctx->use == DO_SIGN)
		return (pkcs11rsa_createctx_sign(key, dctx));
	else
		return (pkcs11rsa_createctx_verify(key,
						   (unsigned) maxbits, dctx));
}

static void
pkcs11rsa_destroyctx(dst_context_t *dctx) {
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;

	if (pk11_ctx != NULL) {
		if (!pk11_ctx->ontoken &&
		    (pk11_ctx->object != CK_INVALID_HANDLE))
			(void) pkcs_C_DestroyObject(pk11_ctx->session,
						    pk11_ctx->object);
		pk11_return_session(pk11_ctx);
		isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
		isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
		dctx->ctxdata.pk11_ctx = NULL;
	}
}

static isc_result_t
pkcs11rsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	CK_RV rv;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	if (dctx->use == DO_SIGN)
		PK11_CALL(pkcs_C_SignUpdate,
			  (pk11_ctx->session,
			   (CK_BYTE_PTR) data->base,
			   (CK_ULONG) data->length),
			  ISC_R_FAILURE);
	else
		PK11_CALL(pkcs_C_VerifyUpdate,
			  (pk11_ctx->session,
			   (CK_BYTE_PTR) data->base,
			   (CK_ULONG) data->length),
			  ISC_R_FAILURE);
	return (ret);
}

static isc_result_t
pkcs11rsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	CK_RV rv;
	CK_ULONG siglen = 0;
	isc_region_t r;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	PK11_RET(pkcs_C_SignFinal,
		 (pk11_ctx->session, NULL, &siglen),
		 DST_R_SIGNFAILURE);

	isc_buffer_availableregion(sig, &r);

	if (r.length < (unsigned int) siglen)
		return (ISC_R_NOSPACE);

	PK11_RET(pkcs_C_SignFinal,
		 (pk11_ctx->session, (CK_BYTE_PTR) r.base, &siglen),
		 DST_R_SIGNFAILURE);

	isc_buffer_add(sig, (unsigned int) siglen);

    err:
	return (ret);
}

static isc_result_t
pkcs11rsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	CK_RV rv;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	PK11_CALL(pkcs_C_VerifyFinal,
		  (pk11_ctx->session,
		   (CK_BYTE_PTR) sig->base,
		   (CK_ULONG) sig->length),
		  DST_R_VERIFYFAILURE);
	return (ret);
}

#else

/*
 * CKM_<hash>_RSA_PKCS mechanisms are not available so fall back
 * to CKM_RSA_PKCS and do the EMSA-PKCS#1-v1.5 encapsulation by hand.
 */

CK_BYTE md5_der[] =
	{ 0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86,
	  0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05, 0x05, 0x00,
	  0x04, 0x10 };
CK_BYTE sha1_der[] =
	{ 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	  0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };
CK_BYTE sha256_der[] =
	{ 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	  0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	  0x00, 0x04, 0x20 };
CK_BYTE sha512_der[] =
	{ 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	  0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
	  0x00, 0x04, 0x40 };
#define MAX_DER_SIZE 19
#define MIN_PKCS1_PADLEN 11

static isc_result_t
pkcs11rsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { 0, NULL, 0 };
	CK_SLOT_ID slotid;
	pk11_object_t *rsa = key->keydata.pkey;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;

#ifndef PK11_MD5_DISABLE
	REQUIRE(key->key_alg == DST_ALG_RSAMD5 ||
		key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#else
	REQUIRE(key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#endif
	REQUIRE(rsa != NULL);

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

	switch (key->key_alg) {
#ifndef PK11_MD5_DISABLE
	case DST_ALG_RSAMD5:
		mech.mechanism = CKM_MD5;
		break;
#endif
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		mech.mechanism = CKM_SHA_1;
		break;
	case DST_ALG_RSASHA256:
		mech.mechanism = CKM_SHA256;
		break;
	case DST_ALG_RSASHA512:
		mech.mechanism = CKM_SHA512;
		break;
	default:
		INSIST(0);
	}

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	if (rsa->ontoken)
		slotid = rsa->slot;
	else
		slotid = pk11_get_best_token(OP_RSA);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       rsa->reqlogon, NULL, slotid);
	if (ret != ISC_R_SUCCESS)
		goto err;

	PK11_RET(pkcs_C_DigestInit, (pk11_ctx->session, &mech), ISC_R_FAILURE);
	dctx->ctxdata.pk11_ctx = pk11_ctx;
	return (ISC_R_SUCCESS);

    err:
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static void
pkcs11rsa_destroyctx(dst_context_t *dctx) {
	CK_BYTE garbage[ISC_SHA512_DIGESTLENGTH];
	CK_ULONG len = ISC_SHA512_DIGESTLENGTH;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;

	if (pk11_ctx != NULL) {
		(void) pkcs_C_DigestFinal(pk11_ctx->session, garbage, &len);
		isc_safe_memwipe(garbage, sizeof(garbage));
		pk11_return_session(pk11_ctx);
		isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
		isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
		dctx->ctxdata.pk11_ctx = NULL;
	}
}

static isc_result_t
pkcs11rsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	CK_RV rv;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	PK11_CALL(pkcs_C_DigestUpdate,
		  (pk11_ctx->session,
		   (CK_BYTE_PTR) data->base,
		   (CK_ULONG) data->length),
		  ISC_R_FAILURE);

	return (ret);
}

static isc_result_t
pkcs11rsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_RSA_PKCS, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS, NULL, 0 },
		{ CKA_PUBLIC_EXPONENT, NULL, 0 },
		{ CKA_PRIVATE_EXPONENT, NULL, 0 },
		{ CKA_PRIME_1, NULL, 0 },
		{ CKA_PRIME_2, NULL, 0 },
		{ CKA_EXPONENT_1, NULL, 0 },
		{ CKA_EXPONENT_2, NULL, 0 },
		{ CKA_COEFFICIENT, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	CK_BYTE digest[MAX_DER_SIZE + ISC_SHA512_DIGESTLENGTH];
	CK_BYTE *der;
	CK_ULONG derlen;
	CK_ULONG hashlen;
	CK_ULONG dgstlen;
	CK_ULONG siglen = 0;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *rsa = key->keydata.pkey;
	isc_region_t r;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

#ifndef PK11_MD5_DISABLE
	REQUIRE(key->key_alg == DST_ALG_RSAMD5 ||
		key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#else
	REQUIRE(key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#endif
	REQUIRE(rsa != NULL);

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

	switch (key->key_alg) {
#ifndef PK11_MD5_DISABLE
	case DST_ALG_RSAMD5:
		der = md5_der;
		derlen = sizeof(md5_der);
		hashlen = ISC_MD5_DIGESTLENGTH;
		break;
#endif
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		der = sha1_der;
		derlen = sizeof(sha1_der);
		hashlen = ISC_SHA1_DIGESTLENGTH;
		break;
	case DST_ALG_RSASHA256:
		der = sha256_der;
		derlen = sizeof(sha256_der);
		hashlen = ISC_SHA256_DIGESTLENGTH;
		break;
	case DST_ALG_RSASHA512:
		der = sha512_der;
		derlen = sizeof(sha512_der);
		hashlen = ISC_SHA512_DIGESTLENGTH;
		break;
	default:
		INSIST(0);
	}
	dgstlen = derlen + hashlen;
	INSIST(dgstlen <= sizeof(digest));
	memmove(digest, der, derlen);

	PK11_RET(pkcs_C_DigestFinal,
		 (pk11_ctx->session, digest + derlen, &hashlen),
		 DST_R_SIGNFAILURE);

	isc_buffer_availableregion(sig, &r);
	if (r.length < (unsigned int) dgstlen + MIN_PKCS1_PADLEN)
		return (ISC_R_NOSPACE);

	if (rsa->ontoken && (rsa->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = rsa->ontoken;
		pk11_ctx->object = rsa->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PUBLIC_EXPONENT:
			INSIST(keyTemplate[7].type == attr->type);
			keyTemplate[7].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[7].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[7].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[7].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIVATE_EXPONENT:
			INSIST(keyTemplate[8].type == attr->type);
			keyTemplate[8].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[8].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[8].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[8].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIME_1:
			INSIST(keyTemplate[9].type == attr->type);
			keyTemplate[9].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[9].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[9].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[9].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PRIME_2:
			INSIST(keyTemplate[10].type == attr->type);
			keyTemplate[10].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[10].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[10].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[10].ulValueLen = attr->ulValueLen;
			break;
		case CKA_EXPONENT_1:
			INSIST(keyTemplate[11].type == attr->type);
			keyTemplate[11].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[11].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[11].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[11].ulValueLen = attr->ulValueLen;
			break;
		case CKA_EXPONENT_2:
			INSIST(keyTemplate[12].type == attr->type);
			keyTemplate[12].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[12].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[12].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[12].ulValueLen = attr->ulValueLen;
			break;
		case CKA_COEFFICIENT:
			INSIST(keyTemplate[13].type == attr->type);
			keyTemplate[13].pValue = isc_mem_get(dctx->mctx,
							     attr->ulValueLen);
			if (keyTemplate[13].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[13].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[13].ulValueLen = attr->ulValueLen;
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 14,
		  &hKey),
		 ISC_R_FAILURE);

    token_key:

	PK11_RET(pkcs_C_SignInit,
		 (pk11_ctx->session, &mech,
		  pk11_ctx->ontoken ? pk11_ctx->object : hKey),
		 ISC_R_FAILURE);

	PK11_RET(pkcs_C_Sign,
		 (pk11_ctx->session,
		  digest, dgstlen,
		  NULL, &siglen),
		 DST_R_SIGNFAILURE);

	if (r.length < (unsigned int) siglen)
		return (ISC_R_NOSPACE);

	PK11_RET(pkcs_C_Sign,
		 (pk11_ctx->session,
		  digest, dgstlen,
		  (CK_BYTE_PTR) r.base, &siglen),
		 DST_R_SIGNFAILURE);

	isc_buffer_add(sig, (unsigned int) siglen);

    err:
	if (hKey != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, hKey);
	for (i = 6; i <= 13; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
	dctx->ctxdata.pk11_ctx = NULL;

	return (ret);
}

static isc_result_t
pkcs11rsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_RSA_PKCS, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS, NULL, 0 },
		{ CKA_PUBLIC_EXPONENT, NULL, 0 },
	};
	CK_ATTRIBUTE *attr;
	CK_BYTE digest[MAX_DER_SIZE + ISC_SHA512_DIGESTLENGTH];
	CK_BYTE *der;
	CK_ULONG derlen;
	CK_ULONG hashlen;
	CK_ULONG dgstlen;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *rsa = key->keydata.pkey;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

#ifndef PK11_MD5_DISABLE
	REQUIRE(key->key_alg == DST_ALG_RSAMD5 ||
		key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#else
	REQUIRE(key->key_alg == DST_ALG_RSASHA1 ||
		key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		key->key_alg == DST_ALG_RSASHA256 ||
		key->key_alg == DST_ALG_RSASHA512);
#endif
	REQUIRE(rsa != NULL);

	switch (key->key_alg) {
#ifndef PK11_MD5_DISABLE
	case DST_ALG_RSAMD5:
		der = md5_der;
		derlen = sizeof(md5_der);
		hashlen = ISC_MD5_DIGESTLENGTH;
		break;
#endif
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		der = sha1_der;
		derlen = sizeof(sha1_der);
		hashlen = ISC_SHA1_DIGESTLENGTH;
		break;
	case DST_ALG_RSASHA256:
		der = sha256_der;
		derlen = sizeof(sha256_der);
		hashlen = ISC_SHA256_DIGESTLENGTH;
		break;
	case DST_ALG_RSASHA512:
		der = sha512_der;
		derlen = sizeof(sha512_der);
		hashlen = ISC_SHA512_DIGESTLENGTH;
		break;
	default:
		INSIST(0);
	}
	dgstlen = derlen + hashlen;
	INSIST(dgstlen <= sizeof(digest));
	memmove(digest, der, derlen);

	PK11_RET(pkcs_C_DigestFinal,
		 (pk11_ctx->session, digest + derlen, &hashlen),
		 DST_R_SIGNFAILURE);

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		case CKA_PUBLIC_EXPONENT:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			if (pk11_numbits(attr->pValue,
					 attr->ulValueLen)
				> RSA_MAX_PUBEXP_BITS)
				DST_RET(DST_R_VERIFYFAILURE);
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 7,
		  &hKey),
		 ISC_R_FAILURE);

	PK11_RET(pkcs_C_VerifyInit,
		 (pk11_ctx->session, &mech, hKey),
		 ISC_R_FAILURE);

	PK11_RET(pkcs_C_Verify,
		 (pk11_ctx->session,
		  digest, dgstlen,
		  (CK_BYTE_PTR) sig->base, (CK_ULONG) sig->length),
		 DST_R_VERIFYFAILURE);

    err:
	if (hKey != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, hKey);
	for (i = 5; i <= 6; i++)
		if (keyTemplate[i].pValue != NULL) {
			isc_safe_memwipe(keyTemplate[i].pValue,
					 keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
	dctx->ctxdata.pk11_ctx = NULL;

	return (ret);
}
#endif

static isc_boolean_t
pkcs11rsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *rsa1, *rsa2;
	CK_ATTRIBUTE *attr1, *attr2;

	rsa1 = key1->keydata.pkey;
	rsa2 = key2->keydata.pkey;

	if ((rsa1 == NULL) && (rsa2 == NULL))
		return (ISC_TRUE);
	else if ((rsa1 == NULL) || (rsa2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(rsa1, CKA_MODULUS);
	attr2 = pk11_attribute_bytype(rsa2, CKA_MODULUS);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(rsa1, CKA_PUBLIC_EXPONENT);
	attr2 = pk11_attribute_bytype(rsa2, CKA_PUBLIC_EXPONENT);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(rsa1, CKA_PRIVATE_EXPONENT);
	attr2 = pk11_attribute_bytype(rsa2, CKA_PRIVATE_EXPONENT);
	if (((attr1 != NULL) || (attr2 != NULL)) &&
	    ((attr1 == NULL) || (attr2 == NULL) ||
	     (attr1->ulValueLen != attr2->ulValueLen) ||
	     !isc_safe_memequal(attr1->pValue, attr2->pValue,
				attr1->ulValueLen)))
		return (ISC_FALSE);

	if (!rsa1->ontoken && !rsa2->ontoken)
		return (ISC_TRUE);
	else if (rsa1->ontoken || rsa2->ontoken ||
		 (rsa1->object != rsa2->object))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

static isc_result_t
pkcs11rsa_generate(dst_key_t *key, int exp, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_RSA_PKCS_KEY_PAIR_GEN, NULL, 0 };
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_ULONG bits = 0;
	CK_BYTE pubexp[5];
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE  keyType = CKK_RSA;
	CK_ATTRIBUTE pubTemplate[] =
	{
		{ CKA_CLASS, &pubClass, (CK_ULONG) sizeof(pubClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS_BITS, &bits, (CK_ULONG) sizeof(bits) },
		{ CKA_PUBLIC_EXPONENT, &pubexp, (CK_ULONG) sizeof(pubexp) }
	};
	CK_OBJECT_HANDLE priv = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS privClass = CKO_PRIVATE_KEY;
	CK_ATTRIBUTE privTemplate[] =
	{
		{ CKA_CLASS, &privClass, (CK_ULONG) sizeof(privClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_EXTRACTABLE, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *rsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	UNUSED(callback);

	/*
	 * Reject incorrect RSA key lengths.
	 */
	switch (key->key_alg) {
	case DST_ALG_RSAMD5:
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		/* From RFC 3110 */
		if (key->key_size > 4096)
			return (ISC_R_FAILURE);
		break;
	case DST_ALG_RSASHA256:
		/* From RFC 5702 */
		if ((key->key_size < 512) ||
		    (key->key_size > 4096))
			return (ISC_R_FAILURE);
		break;
	case DST_ALG_RSASHA512:
		/* From RFC 5702 */
		if ((key->key_size < 1024) ||
		    (key->key_size > 4096))
			return (ISC_R_FAILURE);
		break;
	default:
		INSIST(0);
	}

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, pk11_get_best_token(OP_RSA));
	if (ret != ISC_R_SUCCESS)
		goto err;

	bits = key->key_size;
	if (exp == 0) {
		/* RSA_F4 0x10001 */
		pubexp[0] = 1;
		pubexp[1] = 0;
		pubexp[2] = 1;
		pubTemplate[6].ulValueLen = 3;
	} else {
		/* F5 0x100000001 */
		pubexp[0] = 1;
		pubexp[1] = 0;
		pubexp[2] = 0;
		pubexp[3] = 0;
		pubexp[4] = 1;
		pubTemplate[6].ulValueLen = 5;
	}

	PK11_RET(pkcs_C_GenerateKeyPair,
		 (pk11_ctx->session, &mech,
		  pubTemplate, (CK_ULONG) 7,
		  privTemplate, (CK_ULONG) 7,
		  &pub, &priv),
		 DST_R_CRYPTOFAILURE);

	rsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*rsa));
	if (rsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(rsa, 0, sizeof(*rsa));
	key->keydata.pkey = rsa;
	rsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 8);
	if (rsa->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(rsa->repr, 0, sizeof(*attr) * 8);
	rsa->attrcnt = 8;

	attr = rsa->repr;
	attr[0].type = CKA_MODULUS;
	attr[1].type = CKA_PUBLIC_EXPONENT;
	attr[2].type = CKA_PRIVATE_EXPONENT;
	attr[3].type = CKA_PRIME_1;
	attr[4].type = CKA_PRIME_2;
	attr[5].type = CKA_EXPONENT_1;
	attr[6].type = CKA_EXPONENT_2;
	attr[7].type = CKA_COEFFICIENT;

	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, pub, attr, 2),
		 DST_R_CRYPTOFAILURE);
	for (i = 0; i <= 1; i++) {
		attr[i].pValue = isc_mem_get(key->mctx, attr[i].ulValueLen);
		if (attr[i].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(attr[i].pValue, 0, attr[i].ulValueLen);
	}
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, pub, attr, 2),
		 DST_R_CRYPTOFAILURE);

	attr += 2;
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, priv, attr, 6),
		 DST_R_CRYPTOFAILURE);
	for (i = 0; i <= 5; i++) {
		attr[i].pValue = isc_mem_get(key->mctx, attr[i].ulValueLen);
		if (attr[i].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(attr[i].pValue, 0, attr[i].ulValueLen);
	}
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, priv, attr, 6),
		 DST_R_CRYPTOFAILURE);

	(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ISC_R_SUCCESS);

    err:
	pkcs11rsa_destroy(key);
	if (priv != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	if (pub != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_boolean_t
pkcs11rsa_isprivate(const dst_key_t *key) {
	pk11_object_t *rsa = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (rsa == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(rsa, CKA_PRIVATE_EXPONENT);
	return (ISC_TF((attr != NULL) || rsa->ontoken));
}

static void
pkcs11rsa_destroy(dst_key_t *key) {
	pk11_object_t *rsa = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (rsa == NULL)
		return;

	INSIST((rsa->object == CK_INVALID_HANDLE) || rsa->ontoken);

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_LABEL:
		case CKA_ID:
		case CKA_MODULUS:
		case CKA_PUBLIC_EXPONENT:
		case CKA_PRIVATE_EXPONENT:
		case CKA_PRIME_1:
		case CKA_PRIME_2:
		case CKA_EXPONENT_1:
		case CKA_EXPONENT_2:
		case CKA_COEFFICIENT:
			if (attr->pValue != NULL) {
				isc_safe_memwipe(attr->pValue,
						 attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (rsa->repr != NULL) {
		isc_safe_memwipe(rsa->repr, rsa->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    rsa->repr,
			    rsa->attrcnt * sizeof(*attr));
	}
	isc_safe_memwipe(rsa, sizeof(*rsa));
	isc_mem_put(key->mctx, rsa, sizeof(*rsa));
	key->keydata.pkey = NULL;
}

static isc_result_t
pkcs11rsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *rsa;
	CK_ATTRIBUTE *attr;
	isc_region_t r;
	unsigned int e_bytes = 0, mod_bytes = 0;
	CK_BYTE *exponent = NULL, *modulus = NULL;

	REQUIRE(key->keydata.pkey != NULL);

	rsa = key->keydata.pkey;

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_PUBLIC_EXPONENT:
			exponent = (CK_BYTE *) attr->pValue;
			e_bytes = (unsigned int) attr->ulValueLen;
			break;
		case CKA_MODULUS:
			modulus = (CK_BYTE *) attr->pValue;
			mod_bytes = (unsigned int) attr->ulValueLen;
			break;
		}
	REQUIRE((exponent != NULL) && (modulus != NULL));

	isc_buffer_availableregion(data, &r);

	if (e_bytes < 256) {	/*%< key exponent is <= 2040 bits */
		if (r.length < 1)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint8(data, (isc_uint8_t) e_bytes);
		isc_region_consume(&r, 1);
	} else {
		if (r.length < 3)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint8(data, 0);
		isc_buffer_putuint16(data, (isc_uint16_t) e_bytes);
		isc_region_consume(&r, 3);
	}

	if (r.length < e_bytes + mod_bytes)
		return (ISC_R_NOSPACE);

	memmove(r.base, exponent, e_bytes);
	isc_region_consume(&r, e_bytes);
	memmove(r.base, modulus, mod_bytes);

	isc_buffer_add(data, e_bytes + mod_bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11rsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *rsa;
	isc_region_t r;
	unsigned int e_bytes, mod_bytes;
	CK_BYTE *exponent = NULL, *modulus = NULL;
	CK_ATTRIBUTE *attr;
	unsigned int length;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	length = r.length;

	rsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*rsa));
	if (rsa == NULL)
		return (ISC_R_NOMEMORY);

	memset(rsa, 0, sizeof(*rsa));

	e_bytes = *r.base;
	isc_region_consume(&r, 1);

	if (e_bytes == 0) {
		if (r.length < 2) {
			isc_safe_memwipe(rsa, sizeof(*rsa));
			isc_mem_put(key->mctx, rsa, sizeof(*rsa));
			return (DST_R_INVALIDPUBLICKEY);
		}
		e_bytes = (*r.base) << 8;
		isc_region_consume(&r, 1);
		e_bytes += *r.base;
		isc_region_consume(&r, 1);
	}

	if (r.length < e_bytes) {
		isc_safe_memwipe(rsa, sizeof(*rsa));
		isc_mem_put(key->mctx, rsa, sizeof(*rsa));
		return (DST_R_INVALIDPUBLICKEY);
	}
	exponent = r.base;
	isc_region_consume(&r, e_bytes);
	modulus = r.base;
	mod_bytes = r.length;

	key->key_size = pk11_numbits(modulus, mod_bytes);

	isc_buffer_forward(data, length);

	rsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (rsa->repr == NULL)
		goto nomemory;
	memset(rsa->repr, 0, sizeof(*attr) * 2);
	rsa->attrcnt = 2;
	attr = rsa->repr;
	attr[0].type = CKA_MODULUS;
	attr[0].pValue = isc_mem_get(key->mctx, mod_bytes);
	if (attr[0].pValue == NULL)
		goto nomemory;
	memmove(attr[0].pValue, modulus, mod_bytes);
	attr[0].ulValueLen = (CK_ULONG) mod_bytes;
	attr[1].type = CKA_PUBLIC_EXPONENT;
	attr[1].pValue = isc_mem_get(key->mctx, e_bytes);
	if (attr[1].pValue == NULL)
		goto nomemory;
	memmove(attr[1].pValue, exponent, e_bytes);
	attr[1].ulValueLen = (CK_ULONG) e_bytes;

	key->keydata.pkey = rsa;

	return (ISC_R_SUCCESS);

    nomemory:
	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
		case CKA_PUBLIC_EXPONENT:
			if (attr->pValue != NULL) {
				isc_safe_memwipe(attr->pValue,
						 attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (rsa->repr != NULL) {
		isc_safe_memwipe(rsa->repr,
				 rsa->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    rsa->repr,
			    rsa->attrcnt * sizeof(*attr));
	}
	isc_safe_memwipe(rsa, sizeof(*rsa));
	isc_mem_put(key->mctx, rsa, sizeof(*rsa));
	return (ISC_R_NOMEMORY);
}

static isc_result_t
pkcs11rsa_tofile(const dst_key_t *key, const char *directory) {
	int i;
	pk11_object_t *rsa;
	CK_ATTRIBUTE *attr;
	CK_ATTRIBUTE *modulus = NULL, *exponent = NULL;
	CK_ATTRIBUTE  *d = NULL, *p = NULL, *q = NULL;
	CK_ATTRIBUTE *dmp1 = NULL, *dmq1 = NULL, *iqmp = NULL;
	dst_private_t priv;
	unsigned char *bufs[10];
	isc_result_t result;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	rsa = key->keydata.pkey;

	for (attr = pk11_attribute_first(rsa);
	     attr != NULL;
	     attr = pk11_attribute_next(rsa, attr))
		switch (attr->type) {
		case CKA_MODULUS:
			modulus = attr;
			break;
		case CKA_PUBLIC_EXPONENT:
			exponent = attr;
			break;
		case CKA_PRIVATE_EXPONENT:
			d = attr;
			break;
		case CKA_PRIME_1:
			p = attr;
			break;
		case CKA_PRIME_2:
			q = attr;
			break;
		case CKA_EXPONENT_1:
			dmp1 = attr;
			break;
		case CKA_EXPONENT_2:
			dmq1 = attr;
			break;
		case CKA_COEFFICIENT:
			iqmp = attr;
			break;
		}
	if ((modulus == NULL) || (exponent == NULL))
		return (DST_R_NULLKEY);

	memset(bufs, 0, sizeof(bufs));

	for (i = 0; i < 10; i++) {
		bufs[i] = isc_mem_get(key->mctx, modulus->ulValueLen);
		if (bufs[i] == NULL) {
			result = ISC_R_NOMEMORY;
			goto fail;
		}
		memset(bufs[i], 0, modulus->ulValueLen);
	}

	i = 0;

	priv.elements[i].tag = TAG_RSA_MODULUS;
	priv.elements[i].length = (unsigned short) modulus->ulValueLen;
	memmove(bufs[i], modulus->pValue, modulus->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PUBLICEXPONENT;
	priv.elements[i].length = (unsigned short) exponent->ulValueLen;
	memmove(bufs[i], exponent->pValue, exponent->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	if (d != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIVATEEXPONENT;
		priv.elements[i].length = (unsigned short) d->ulValueLen;
		memmove(bufs[i], d->pValue, d->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (p != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIME1;
		priv.elements[i].length = (unsigned short) p->ulValueLen;
		memmove(bufs[i], p->pValue, p->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (q != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIME2;
		priv.elements[i].length = (unsigned short) q->ulValueLen;
		memmove(bufs[i], q->pValue, q->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (dmp1 != NULL) {
		priv.elements[i].tag = TAG_RSA_EXPONENT1;
		priv.elements[i].length = (unsigned short) dmp1->ulValueLen;
		memmove(bufs[i], dmp1->pValue, dmp1->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (dmq1 != NULL) {
		priv.elements[i].tag = TAG_RSA_EXPONENT2;
		priv.elements[i].length = (unsigned short) dmq1->ulValueLen;
		memmove(bufs[i], dmq1->pValue, dmq1->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (iqmp != NULL) {
		priv.elements[i].tag = TAG_RSA_COEFFICIENT;
		priv.elements[i].length = (unsigned short) iqmp->ulValueLen;
		memmove(bufs[i], iqmp->pValue, iqmp->ulValueLen);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (key->engine != NULL) {
		priv.elements[i].tag = TAG_RSA_ENGINE;
		priv.elements[i].length = strlen(key->engine) + 1;
		priv.elements[i].data = (unsigned char *)key->engine;
		i++;
	}

	if (key->label != NULL) {
		priv.elements[i].tag = TAG_RSA_LABEL;
		priv.elements[i].length = strlen(key->label) + 1;
		priv.elements[i].data = (unsigned char *)key->label;
		i++;
	}

	priv.nelements = i;
	result = dst__privstruct_writefile(key, &priv, directory);
 fail:
	for (i = 0; i < 10; i++) {
		if (bufs[i] == NULL)
			break;
		isc_safe_memwipe(bufs[i], modulus->ulValueLen);
		isc_mem_put(key->mctx, bufs[i], modulus->ulValueLen);
	}
	return (result);
}

static isc_result_t
pkcs11rsa_fetch(dst_key_t *key, const char *engine, const char *label,
		dst_key_t *pub)
{
	CK_RV rv;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE searchTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_LABEL, NULL, 0 }
	};
	CK_ULONG cnt;
	CK_ATTRIBUTE *attr;
	CK_ATTRIBUTE *pubattr;
	pk11_object_t *rsa;
	pk11_object_t *pubrsa;
	pk11_context_t *pk11_ctx = NULL;
	isc_result_t ret;

	if (label == NULL)
		return (DST_R_NOENGINE);

	rsa = key->keydata.pkey;
	pubrsa = pub->keydata.pkey;

	rsa->object = CK_INVALID_HANDLE;
	rsa->ontoken = ISC_TRUE;
	rsa->reqlogon = ISC_TRUE;
	rsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (rsa->repr == NULL)
		return (ISC_R_NOMEMORY);
	memset(rsa->repr, 0, sizeof(*attr) * 2);
	rsa->attrcnt = 2;
	attr = rsa->repr;

	attr->type = CKA_MODULUS;
	pubattr = pk11_attribute_bytype(pubrsa, CKA_MODULUS);
	attr->pValue = isc_mem_get(key->mctx, pubattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pubattr->pValue, pubattr->ulValueLen);
	attr->ulValueLen = pubattr->ulValueLen;
	attr++;

	attr->type = CKA_PUBLIC_EXPONENT;
	pubattr = pk11_attribute_bytype(pubrsa, CKA_PUBLIC_EXPONENT);
	attr->pValue = isc_mem_get(key->mctx, pubattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pubattr->pValue, pubattr->ulValueLen);
	attr->ulValueLen = pubattr->ulValueLen;

	ret = pk11_parse_uri(rsa, label, key->mctx, OP_RSA);
	if (ret != ISC_R_SUCCESS)
		goto err;

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		DST_RET(ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       rsa->reqlogon, NULL, rsa->slot);
	if (ret != ISC_R_SUCCESS)
		goto err;

	attr = pk11_attribute_bytype(rsa, CKA_LABEL);
	if (attr == NULL) {
		attr = pk11_attribute_bytype(rsa, CKA_ID);
		INSIST(attr != NULL);
		searchTemplate[3].type = CKA_ID;
	}
	searchTemplate[3].pValue = attr->pValue;
	searchTemplate[3].ulValueLen = attr->ulValueLen;

	PK11_RET(pkcs_C_FindObjectsInit,
		 (pk11_ctx->session, searchTemplate, (CK_ULONG) 4),
		 DST_R_CRYPTOFAILURE);
	PK11_RET(pkcs_C_FindObjects,
		 (pk11_ctx->session, &rsa->object, (CK_ULONG) 1, &cnt),
		 DST_R_CRYPTOFAILURE);
	(void) pkcs_C_FindObjectsFinal(pk11_ctx->session);
	if (cnt == 0)
		DST_RET(ISC_R_NOTFOUND);
	if (cnt > 1)
		DST_RET(ISC_R_EXISTS);

	if (engine != NULL) {
		key->engine = isc_mem_strdup(key->mctx, engine);
		if (key->engine == NULL)
			DST_RET(ISC_R_NOMEMORY);
	}

	key->label = isc_mem_strdup(key->mctx, label);
	if (key->label == NULL)
		DST_RET(ISC_R_NOMEMORY);

	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	attr = pk11_attribute_bytype(rsa, CKA_MODULUS);
	INSIST(attr != NULL);
	key->key_size = pk11_numbits(attr->pValue, attr->ulValueLen);

	return (ISC_R_SUCCESS);

    err:
	if (pk11_ctx != NULL) {
		pk11_return_session(pk11_ctx);
		isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
		isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	}

	return (ret);
}

static isc_result_t
rsa_check(pk11_object_t *rsa, pk11_object_t *pubrsa) {
	CK_ATTRIBUTE *pubattr, *privattr;
	CK_BYTE *priv_exp = NULL, *priv_mod = NULL;
	CK_BYTE *pub_exp = NULL, *pub_mod = NULL;
	unsigned int priv_explen = 0, priv_modlen = 0;
	unsigned int pub_explen = 0, pub_modlen = 0;

	REQUIRE(rsa != NULL && pubrsa != NULL);

	privattr = pk11_attribute_bytype(rsa, CKA_PUBLIC_EXPONENT);
	INSIST(privattr != NULL);
	priv_exp = privattr->pValue;
	priv_explen = privattr->ulValueLen;

	pubattr = pk11_attribute_bytype(pubrsa, CKA_PUBLIC_EXPONENT);
	INSIST(pubattr != NULL);
	pub_exp = pubattr->pValue;
	pub_explen = pubattr->ulValueLen;

	if (priv_exp != NULL) {
		if (priv_explen != pub_explen)
			return (DST_R_INVALIDPRIVATEKEY);
		if (!isc_safe_memequal(priv_exp, pub_exp, pub_explen))
			return (DST_R_INVALIDPRIVATEKEY);
	} else {
		privattr->pValue = pub_exp;
		privattr->ulValueLen = pub_explen;
		pubattr->pValue = NULL;
		pubattr->ulValueLen = 0;
	}

	if (privattr->pValue == NULL)
		return (DST_R_INVALIDPRIVATEKEY);

	privattr = pk11_attribute_bytype(rsa, CKA_MODULUS);
	INSIST(privattr != NULL);
	priv_mod = privattr->pValue;
	priv_modlen = privattr->ulValueLen;

	pubattr = pk11_attribute_bytype(pubrsa, CKA_MODULUS);
	INSIST(pubattr != NULL);
	pub_mod = pubattr->pValue;
	pub_modlen = pubattr->ulValueLen;

	if (priv_mod != NULL) {
		if (priv_modlen != pub_modlen)
			return (DST_R_INVALIDPRIVATEKEY);
		if (!isc_safe_memequal(priv_mod, pub_mod, pub_modlen))
			return (DST_R_INVALIDPRIVATEKEY);
	} else {
		privattr->pValue = pub_mod;
		privattr->ulValueLen = pub_modlen;
		pubattr->pValue = NULL;
		pubattr->ulValueLen = 0;
	}

	if (privattr->pValue == NULL)
		return (DST_R_INVALIDPRIVATEKEY);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11rsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	pk11_object_t *rsa;
	CK_ATTRIBUTE *attr;
	isc_mem_t *mctx = key->mctx;
	const char *engine = NULL, *label = NULL;

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_RSA, lexer, mctx, &priv);
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
	rsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*rsa));
	if (rsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(rsa, 0, sizeof(*rsa));
	key->keydata.pkey = rsa;

	/* Is this key is stored in a HSM? See if we can fetch it. */
	if ((label != NULL) || (engine != NULL)) {
		ret = pkcs11rsa_fetch(key, engine, label, pub);
		if (ret != ISC_R_SUCCESS)
			goto err;
		dst__privstruct_free(&priv, mctx);
		isc_safe_memwipe(&priv, sizeof(priv));
		return (ret);
	}

	rsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 8);
	if (rsa->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(rsa->repr, 0, sizeof(*attr) * 8);
	rsa->attrcnt = 8;
	attr = rsa->repr;
	attr[0].type = CKA_MODULUS;
	attr[1].type = CKA_PUBLIC_EXPONENT;
	attr[2].type = CKA_PRIVATE_EXPONENT;
	attr[3].type = CKA_PRIME_1;
	attr[4].type = CKA_PRIME_2;
	attr[5].type = CKA_EXPONENT_1;
	attr[6].type = CKA_EXPONENT_2;
	attr[7].type = CKA_COEFFICIENT;

	for (i = 0; i < priv.nelements; i++) {
		CK_BYTE *bn;

		switch (priv.elements[i].tag) {
		case TAG_RSA_ENGINE:
			continue;
		case TAG_RSA_LABEL:
			continue;
		default:
			bn = isc_mem_get(key->mctx, priv.elements[i].length);
			if (bn == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(bn, priv.elements[i].data,
				priv.elements[i].length);
		}

		switch (priv.elements[i].tag) {
			case TAG_RSA_MODULUS:
				attr = pk11_attribute_bytype(rsa, CKA_MODULUS);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_PUBLICEXPONENT:
				attr = pk11_attribute_bytype(rsa,
						CKA_PUBLIC_EXPONENT);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_PRIVATEEXPONENT:
				attr = pk11_attribute_bytype(rsa,
						CKA_PRIVATE_EXPONENT);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_PRIME1:
				attr = pk11_attribute_bytype(rsa, CKA_PRIME_1);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_PRIME2:
				attr = pk11_attribute_bytype(rsa, CKA_PRIME_2);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_EXPONENT1:
				attr = pk11_attribute_bytype(rsa,
							     CKA_EXPONENT_1);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_EXPONENT2:
				attr = pk11_attribute_bytype(rsa,
							     CKA_EXPONENT_2);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_RSA_COEFFICIENT:
				attr = pk11_attribute_bytype(rsa,
							     CKA_COEFFICIENT);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
		}
	}

	if (rsa_check(rsa, pub->keydata.pkey) != ISC_R_SUCCESS)
		DST_RET(DST_R_INVALIDPRIVATEKEY);

	attr = pk11_attribute_bytype(rsa, CKA_MODULUS);
	INSIST(attr != NULL);
	key->key_size = pk11_numbits(attr->pValue, attr->ulValueLen);

	attr = pk11_attribute_bytype(rsa, CKA_PUBLIC_EXPONENT);
	INSIST(attr != NULL);
	if (pk11_numbits(attr->pValue, attr->ulValueLen) > RSA_MAX_PUBEXP_BITS)
		DST_RET(ISC_R_RANGE);

	dst__privstruct_free(&priv, mctx);
	isc_safe_memwipe(&priv, sizeof(priv));

	return (ISC_R_SUCCESS);

 err:
	pkcs11rsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (ret);
}

static isc_result_t
pkcs11rsa_fromlabel(dst_key_t *key, const char *engine, const char *label,
		    const char *pin)
{
	CK_RV rv;
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE searchTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_LABEL, NULL, 0 }
	};
	CK_ULONG cnt;
	CK_ATTRIBUTE *attr;
	pk11_object_t *rsa;
	pk11_context_t *pk11_ctx = NULL;
	isc_result_t ret;
	unsigned int i;

	UNUSED(pin);

	rsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*rsa));
	if (rsa == NULL)
		return (ISC_R_NOMEMORY);
	memset(rsa, 0, sizeof(*rsa));
	rsa->object = CK_INVALID_HANDLE;
	rsa->ontoken = ISC_TRUE;
	rsa->reqlogon = ISC_TRUE;
	key->keydata.pkey = rsa;

	rsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (rsa->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(rsa->repr, 0, sizeof(*attr) * 2);
	rsa->attrcnt = 2;
	attr = rsa->repr;
	attr[0].type = CKA_MODULUS;
	attr[1].type = CKA_PUBLIC_EXPONENT;

	ret = pk11_parse_uri(rsa, label, key->mctx, OP_RSA);
	if (ret != ISC_R_SUCCESS)
		goto err;

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		DST_RET(ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_RSA, ISC_TRUE, ISC_FALSE,
			       rsa->reqlogon, NULL, rsa->slot);
	if (ret != ISC_R_SUCCESS)
		goto err;

	attr = pk11_attribute_bytype(rsa, CKA_LABEL);
	if (attr == NULL) {
		attr = pk11_attribute_bytype(rsa, CKA_ID);
		INSIST(attr != NULL);
		searchTemplate[3].type = CKA_ID;
	}
	searchTemplate[3].pValue = attr->pValue;
	searchTemplate[3].ulValueLen = attr->ulValueLen;

	PK11_RET(pkcs_C_FindObjectsInit,
		 (pk11_ctx->session, searchTemplate, (CK_ULONG) 4),
		 DST_R_CRYPTOFAILURE);
	PK11_RET(pkcs_C_FindObjects,
		 (pk11_ctx->session, &hKey, (CK_ULONG) 1, &cnt),
		 DST_R_CRYPTOFAILURE);
	(void) pkcs_C_FindObjectsFinal(pk11_ctx->session);
	if (cnt == 0)
		DST_RET(ISC_R_NOTFOUND);
	if (cnt > 1)
		DST_RET(ISC_R_EXISTS);

	attr = rsa->repr;
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, hKey, attr, 2),
		 DST_R_CRYPTOFAILURE);
	for (i = 0; i <= 1; i++) {
		attr[i].pValue = isc_mem_get(key->mctx, attr[i].ulValueLen);
		if (attr[i].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(attr[i].pValue, 0, attr[i].ulValueLen);
	}
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, hKey, attr, 2),
		 DST_R_CRYPTOFAILURE);

	keyClass = CKO_PRIVATE_KEY;
	PK11_RET(pkcs_C_FindObjectsInit,
		 (pk11_ctx->session, searchTemplate, (CK_ULONG) 4),
		 DST_R_CRYPTOFAILURE);
	PK11_RET(pkcs_C_FindObjects,
		 (pk11_ctx->session, &rsa->object, (CK_ULONG) 1, &cnt),
		 DST_R_CRYPTOFAILURE);
	(void) pkcs_C_FindObjectsFinal(pk11_ctx->session);
	if (cnt == 0)
		DST_RET(ISC_R_NOTFOUND);
	if (cnt > 1)
		DST_RET(ISC_R_EXISTS);

	if (engine != NULL) {
		key->engine = isc_mem_strdup(key->mctx, engine);
		if (key->engine == NULL)
			DST_RET(ISC_R_NOMEMORY);
	}

	key->label = isc_mem_strdup(key->mctx, label);
	if (key->label == NULL)
		DST_RET(ISC_R_NOMEMORY);

	attr = pk11_attribute_bytype(rsa, CKA_PUBLIC_EXPONENT);
	INSIST(attr != NULL);
	if (pk11_numbits(attr->pValue, attr->ulValueLen) > RSA_MAX_PUBEXP_BITS)
		DST_RET(ISC_R_RANGE);

	attr = pk11_attribute_bytype(rsa, CKA_MODULUS);
	INSIST(attr != NULL);
	key->key_size = pk11_numbits(attr->pValue, attr->ulValueLen);

	pk11_return_session(pk11_ctx);
	isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ISC_R_SUCCESS);

    err:
	pkcs11rsa_destroy(key);
	if (pk11_ctx != NULL) {
		pk11_return_session(pk11_ctx);
		isc_safe_memwipe(pk11_ctx, sizeof(*pk11_ctx));
		isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	}

	return (ret);
}

static dst_func_t pkcs11rsa_functions = {
	pkcs11rsa_createctx,
#ifndef PK11_RSA_PKCS_REPLACE
	pkcs11rsa_createctx2,
#else
	NULL, /*%< createctx2 */
#endif
	pkcs11rsa_destroyctx,
	pkcs11rsa_adddata,
	pkcs11rsa_sign,
	pkcs11rsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	pkcs11rsa_compare,
	NULL, /*%< paramcompare */
	pkcs11rsa_generate,
	pkcs11rsa_isprivate,
	pkcs11rsa_destroy,
	pkcs11rsa_todns,
	pkcs11rsa_fromdns,
	pkcs11rsa_tofile,
	pkcs11rsa_parse,
	NULL, /*%< cleanup */
	pkcs11rsa_fromlabel,
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11rsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);

	if (*funcp == NULL)
		*funcp = &pkcs11rsa_functions;
	return (ISC_R_SUCCESS);
}

#else /* PKCS11CRYPTO */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO */
/*! \file */
