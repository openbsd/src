/*
 * Copyright (C) 2014-2017  Internet Systems Consortium, Inc. ("ISC")
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
#include "dst_parse.h"
#include "dst_pkcs11.h"

#include <pk11/internal.h>

/*
 * FIPS 186-2 DSA keys:
 *  mechanisms:
 *    CKM_DSA_SHA1,
 *    CKM_DSA_KEY_PAIR_GEN,
 *    CKM_DSA_PARAMETER_GEN
 *  domain parameters:
 *    object class CKO_DOMAIN_PARAMETERS
 *    key type CKK_DSA
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_SUBPRIME (subprime q)
 *    attribute CKA_BASE (base g)
 *    optional attribute CKA_PRIME_BITS (p length in bits)
 *  public keys:
 *    object class CKO_PUBLIC_KEY
 *    key type CKK_DSA
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_SUBPRIME (subprime q)
 *    attribute CKA_BASE (base g)
 *    attribute CKA_VALUE (public value y)
 *  private keys:
 *    object class CKO_PRIVATE_KEY
 *    key type CKK_DSA
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_SUBPRIME (subprime q)
 *    attribute CKA_BASE (base g)
 *    attribute CKA_VALUE (private value x)
 *  reuse CKA_PRIVATE_EXPONENT for key pair private value
 */

#define CKA_VALUE2	CKA_PRIVATE_EXPONENT

#define DST_RET(a) {ret = a; goto err;}

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

static isc_result_t pkcs11dsa_todns(const dst_key_t *key, isc_buffer_t *data);
static void pkcs11dsa_destroy(dst_key_t *key);

static isc_result_t
pkcs11dsa_createctx_sign(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_DSA_SHA1, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_DSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		{ CKA_VALUE, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *dsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	REQUIRE(key != NULL);
	dsa = key->keydata.pkey;
	REQUIRE(dsa != NULL);

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_DSA, ISC_TRUE, ISC_FALSE,
			       dsa->reqlogon, NULL,
			       pk11_get_best_token(OP_DSA));
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (dsa->ontoken && (dsa->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = dsa->ontoken;
		pk11_ctx->object = dsa->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			break;
		case CKA_SUBPRIME:
			INSIST(keyTemplate[7].type == attr->type);
			keyTemplate[7].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[7].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[7].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[7].ulValueLen = attr->ulValueLen;
			break;
		case CKA_BASE:
			INSIST(keyTemplate[8].type == attr->type);
			keyTemplate[8].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[8].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[8].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[8].ulValueLen = attr->ulValueLen;
			break;
		case CKA_VALUE2:
			INSIST(keyTemplate[9].type == CKA_VALUE);
			keyTemplate[9].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[9].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[9].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[9].ulValueLen = attr->ulValueLen;
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 10,
		  &pk11_ctx->object),
		 ISC_R_FAILURE);

    token_key:

	PK11_RET(pkcs_C_SignInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 6; i <= 9; i++)
		if (keyTemplate[i].pValue != NULL) {
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}

	return (ISC_R_SUCCESS);

    err:
	if (!pk11_ctx->ontoken && (pk11_ctx->object != CK_INVALID_HANDLE))
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pk11_ctx->object);
	for (i = 6; i <= 9; i++)
		if (keyTemplate[i].pValue != NULL) {
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_result_t
pkcs11dsa_createctx_verify(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_DSA_SHA1, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_DSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		{ CKA_VALUE, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *dsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	dsa = key->keydata.pkey;
	REQUIRE(dsa != NULL);
	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_DSA, ISC_TRUE, ISC_FALSE,
			       dsa->reqlogon, NULL,
			       pk11_get_best_token(OP_DSA));
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (dsa->ontoken && (dsa->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = dsa->ontoken;
		pk11_ctx->object = dsa->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		case CKA_SUBPRIME:
			INSIST(keyTemplate[6].type == attr->type);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
			break;
		case CKA_BASE:
			INSIST(keyTemplate[7].type == attr->type);
			keyTemplate[7].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[7].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[7].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[7].ulValueLen = attr->ulValueLen;
			break;
		case CKA_VALUE:
			INSIST(keyTemplate[8].type == attr->type);
			keyTemplate[8].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[8].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[8].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[8].ulValueLen = attr->ulValueLen;
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 9,
		  &pk11_ctx->object),
		 ISC_R_FAILURE);

    token_key:

	PK11_RET(pkcs_C_VerifyInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 5; i <= 8; i++)
		if (keyTemplate[i].pValue != NULL) {
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}

	return (ISC_R_SUCCESS);

    err:
	if (!pk11_ctx->ontoken && (pk11_ctx->object != CK_INVALID_HANDLE))
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pk11_ctx->object);
	for (i = 5; i <= 8; i++)
		if (keyTemplate[i].pValue != NULL) {
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_result_t
pkcs11dsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	if (dctx->use == DO_SIGN)
		return (pkcs11dsa_createctx_sign(key, dctx));
	else
		return (pkcs11dsa_createctx_verify(key, dctx));
}

static void
pkcs11dsa_destroyctx(dst_context_t *dctx) {
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;

	if (pk11_ctx != NULL) {
		if (!pk11_ctx->ontoken &&
		    (pk11_ctx->object != CK_INVALID_HANDLE))
			(void) pkcs_C_DestroyObject(pk11_ctx->session,
					       pk11_ctx->object);
		pk11_return_session(pk11_ctx);
		memset(pk11_ctx, 0, sizeof(*pk11_ctx));
		isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
		dctx->ctxdata.pk11_ctx = NULL;
	}
}

static isc_result_t
pkcs11dsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
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
pkcs11dsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	CK_RV rv;
	CK_ULONG siglen = ISC_SHA1_DIGESTLENGTH * 2;
	isc_region_t r;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int klen;

	isc_buffer_availableregion(sig, &r);
	if (r.length < ISC_SHA1_DIGESTLENGTH * 2 + 1)
		return (ISC_R_NOSPACE);

	PK11_RET(pkcs_C_SignFinal,
		 (pk11_ctx->session, (CK_BYTE_PTR) r.base + 1, &siglen),
		 DST_R_SIGNFAILURE);
	if (siglen != ISC_SHA1_DIGESTLENGTH * 2)
		return (DST_R_SIGNFAILURE);

	klen = (dctx->key->key_size - 512)/64;
	if (klen > 255)
		return (ISC_R_FAILURE);
	*r.base = klen;
	isc_buffer_add(sig, ISC_SHA1_DIGESTLENGTH * 2 + 1);

    err:
	return (ret);
}

static isc_result_t
pkcs11dsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	CK_RV rv;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	PK11_CALL(pkcs_C_VerifyFinal,
		  (pk11_ctx->session,
		   (CK_BYTE_PTR) sig->base + 1,
		   (CK_ULONG) sig->length - 1),
		  DST_R_VERIFYFAILURE);
	return (ret);
}

static isc_boolean_t
pkcs11dsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *dsa1, *dsa2;
	CK_ATTRIBUTE *attr1, *attr2;

	dsa1 = key1->keydata.pkey;
	dsa2 = key2->keydata.pkey;

	if ((dsa1 == NULL) && (dsa2 == NULL))
		return (ISC_TRUE);
	else if ((dsa1 == NULL) || (dsa2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dsa1, CKA_PRIME);
	attr2 = pk11_attribute_bytype(dsa2, CKA_PRIME);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dsa1, CKA_SUBPRIME);
	attr2 = pk11_attribute_bytype(dsa2, CKA_SUBPRIME);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dsa1, CKA_BASE);
	attr2 = pk11_attribute_bytype(dsa2, CKA_BASE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dsa1, CKA_VALUE);
	attr2 = pk11_attribute_bytype(dsa2, CKA_VALUE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dsa1, CKA_VALUE2);
	attr2 = pk11_attribute_bytype(dsa2, CKA_VALUE2);
	if (((attr1 != NULL) || (attr2 != NULL)) &&
	    ((attr1 == NULL) || (attr2 == NULL) ||
	     (attr1->ulValueLen != attr2->ulValueLen) ||
	     !isc_safe_memequal(attr1->pValue, attr2->pValue,
				attr1->ulValueLen)))
		return (ISC_FALSE);

	if (!dsa1->ontoken && !dsa2->ontoken)
		return (ISC_TRUE);
	else if (dsa1->ontoken || dsa2->ontoken ||
		 (dsa1->object != dsa2->object))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

static isc_result_t
pkcs11dsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_DSA_PARAMETER_GEN, NULL, 0 };
	CK_OBJECT_HANDLE dp = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS dpClass = CKO_DOMAIN_PARAMETERS;
	CK_KEY_TYPE  keyType = CKK_DSA;
	CK_ULONG bits = 0;
	CK_ATTRIBUTE dpTemplate[] =
	{
		{ CKA_CLASS, &dpClass, (CK_ULONG) sizeof(dpClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIME_BITS, &bits, (CK_ULONG) sizeof(bits) },
	};
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_ATTRIBUTE pubTemplate[] =
	{
		{ CKA_CLASS, &pubClass, (CK_ULONG) sizeof(pubClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 }
	};
	CK_OBJECT_HANDLE priv = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE privClass = CKO_PRIVATE_KEY;
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
	pk11_object_t *dsa;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	UNUSED(unused);
	UNUSED(callback);

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_DSA, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, pk11_get_best_token(OP_DSA));
	if (ret != ISC_R_SUCCESS)
		goto err;

	bits = key->key_size;
	PK11_RET(pkcs_C_GenerateKey,
		 (pk11_ctx->session, &mech, dpTemplate, (CK_ULONG) 5, &dp),
		 DST_R_CRYPTOFAILURE);

	dsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dsa));
	if (dsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dsa, 0, sizeof(*dsa));
	key->keydata.pkey = dsa;
	dsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 5);
	if (dsa->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dsa->repr, 0, sizeof(*attr) * 5);
	dsa->attrcnt = 5;

	attr = dsa->repr;
	attr[0].type = CKA_PRIME;
	attr[1].type = CKA_SUBPRIME;
	attr[2].type = CKA_BASE;
	attr[3].type = CKA_VALUE;
	attr[4].type = CKA_VALUE2;

	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, dp, attr, 3),
		 DST_R_CRYPTOFAILURE);

	for (i = 0; i <= 2; i++) {
		attr[i].pValue = isc_mem_get(key->mctx, attr[i].ulValueLen);
		if (attr[i].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(attr[i].pValue, 0, attr[i].ulValueLen);
	}
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, dp, attr, 3),
		 DST_R_CRYPTOFAILURE);
	pubTemplate[5].pValue = attr[0].pValue;
	pubTemplate[5].ulValueLen = attr[0].ulValueLen;
	pubTemplate[6].pValue = attr[1].pValue;
	pubTemplate[6].ulValueLen = attr[1].ulValueLen;
	pubTemplate[7].pValue = attr[2].pValue;
	pubTemplate[7].ulValueLen = attr[2].ulValueLen;

	mech.mechanism = CKM_DSA_KEY_PAIR_GEN;
	PK11_RET(pkcs_C_GenerateKeyPair,
		 (pk11_ctx->session, &mech,
		  pubTemplate, (CK_ULONG) 8,
		  privTemplate, (CK_ULONG) 7,
		  &pub, &priv),
		 DST_R_CRYPTOFAILURE);

	attr = dsa->repr;
	attr += 3;
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, pub, attr, 1),
		 DST_R_CRYPTOFAILURE);
	attr->pValue = isc_mem_get(key->mctx, attr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(attr->pValue, 0, attr->ulValueLen);
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, pub, attr, 1),
		 DST_R_CRYPTOFAILURE);

	attr++;
	attr->type = CKA_VALUE;
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, priv, attr, 1),
		 DST_R_CRYPTOFAILURE);
	attr->pValue = isc_mem_get(key->mctx, attr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(attr->pValue, 0, attr->ulValueLen);
	PK11_RET(pkcs_C_GetAttributeValue,
		 (pk11_ctx->session, priv, attr, 1),
		 DST_R_CRYPTOFAILURE);
	attr->type = CKA_VALUE2;

	(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	(void) pkcs_C_DestroyObject(pk11_ctx->session, dp);
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ISC_R_SUCCESS);

    err:
	pkcs11dsa_destroy(key);
	if (priv != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	if (pub != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	if (dp != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, dp);
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_boolean_t
pkcs11dsa_isprivate(const dst_key_t *key) {
	pk11_object_t *dsa = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (dsa == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(dsa, CKA_VALUE2);
	return (ISC_TF((attr != NULL) || dsa->ontoken));
}

static void
pkcs11dsa_destroy(dst_key_t *key) {
	pk11_object_t *dsa = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (dsa == NULL)
		return;

	INSIST((dsa->object == CK_INVALID_HANDLE) || dsa->ontoken);

	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
		case CKA_SUBPRIME:
		case CKA_BASE:
		case CKA_VALUE:
		case CKA_VALUE2:
			if (attr->pValue != NULL) {
				memset(attr->pValue, 0, attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (dsa->repr != NULL) {
		memset(dsa->repr, 0, dsa->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    dsa->repr,
			    dsa->attrcnt * sizeof(*attr));
	}
	memset(dsa, 0, sizeof(*dsa));
	isc_mem_put(key->mctx, dsa, sizeof(*dsa));
	key->keydata.pkey = NULL;
}


static isc_result_t
pkcs11dsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *dsa;
	CK_ATTRIBUTE *attr;
	isc_region_t r;
	int dnslen;
	unsigned int t, p_bytes;
	CK_ATTRIBUTE *prime = NULL, *subprime = NULL;
	CK_ATTRIBUTE *base = NULL, *pub_key = NULL;
	CK_BYTE *cp;

	REQUIRE(key->keydata.pkey != NULL);

	dsa = key->keydata.pkey;

	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
			prime = attr;
			break;
		case CKA_SUBPRIME:
			subprime = attr;
			break;
		case CKA_BASE:
			base = attr;
			break;
		case CKA_VALUE:
			pub_key = attr;
			break;
		}
	REQUIRE((prime != NULL) && (subprime != NULL) &&
		(base != NULL) && (pub_key != NULL));

	isc_buffer_availableregion(data, &r);

	t = (prime->ulValueLen - 64) / 8;
	if (t > 8)
		return (DST_R_INVALIDPUBLICKEY);
	p_bytes = 64 + 8 * t;

	dnslen = 1 + (key->key_size * 3)/8 + ISC_SHA1_DIGESTLENGTH;
	if (r.length < (unsigned int) dnslen)
		return (ISC_R_NOSPACE);

	memset(r.base, 0, dnslen);
	*r.base = t;
	isc_region_consume(&r, 1);

	cp = (CK_BYTE *) subprime->pValue;
	memmove(r.base + ISC_SHA1_DIGESTLENGTH - subprime->ulValueLen,
		cp, subprime->ulValueLen);
	isc_region_consume(&r, ISC_SHA1_DIGESTLENGTH);
	cp = (CK_BYTE *) prime->pValue;
	memmove(r.base + key->key_size/8 - prime->ulValueLen,
		cp, prime->ulValueLen);
	isc_region_consume(&r, p_bytes);
	cp = (CK_BYTE *) base->pValue;
	memmove(r.base + key->key_size/8 - base->ulValueLen,
		cp, base->ulValueLen);
	isc_region_consume(&r, p_bytes);
	cp = (CK_BYTE *) pub_key->pValue;
	memmove(r.base + key->key_size/8 - pub_key->ulValueLen,
		cp, pub_key->ulValueLen);
	isc_region_consume(&r, p_bytes);

	isc_buffer_add(data, dnslen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11dsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *dsa;
	isc_region_t r;
	unsigned int t, p_bytes;
	CK_BYTE *prime, *subprime, *base, *pub_key;
	CK_ATTRIBUTE *attr;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	dsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dsa));
	if (dsa == NULL)
		return (ISC_R_NOMEMORY);
	memset(dsa, 0, sizeof(*dsa));

	t = (unsigned int) *r.base;
	isc_region_consume(&r, 1);
	if (t > 8) {
		memset(dsa, 0, sizeof(*dsa));
		isc_mem_put(key->mctx, dsa, sizeof(*dsa));
		return (DST_R_INVALIDPUBLICKEY);
	}
	p_bytes = 64 + 8 * t;

	if (r.length < ISC_SHA1_DIGESTLENGTH + 3 * p_bytes) {
		memset(dsa, 0, sizeof(*dsa));
		isc_mem_put(key->mctx, dsa, sizeof(*dsa));
		return (DST_R_INVALIDPUBLICKEY);
	}

	subprime = r.base;
	isc_region_consume(&r, ISC_SHA1_DIGESTLENGTH);

	prime = r.base;
	isc_region_consume(&r, p_bytes);

	base = r.base;
	isc_region_consume(&r, p_bytes);

	pub_key = r.base;
	isc_region_consume(&r, p_bytes);

	key->key_size = p_bytes * 8;

	isc_buffer_forward(data, 1 + ISC_SHA1_DIGESTLENGTH + 3 * p_bytes);

	dsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 4);
	if (dsa->repr == NULL)
		goto nomemory;
	memset(dsa->repr, 0, sizeof(*attr) * 4);
	dsa->attrcnt = 4;

	attr = dsa->repr;
	attr[0].type = CKA_PRIME;
	attr[0].pValue = isc_mem_get(key->mctx, p_bytes);
	if (attr[0].pValue == NULL)
		goto nomemory;
	memmove(attr[0].pValue, prime, p_bytes);
	attr[0].ulValueLen = p_bytes;

	attr[1].type = CKA_SUBPRIME;
	attr[1].pValue = isc_mem_get(key->mctx, ISC_SHA1_DIGESTLENGTH);
	if (attr[1].pValue == NULL)
		goto nomemory;
	memmove(attr[1].pValue, subprime, ISC_SHA1_DIGESTLENGTH);
	attr[1].ulValueLen = ISC_SHA1_DIGESTLENGTH;

	attr[2].type = CKA_BASE;
	attr[2].pValue = isc_mem_get(key->mctx, p_bytes);
	if (attr[2].pValue == NULL)
		goto nomemory;
	memmove(attr[2].pValue, base, p_bytes);
	attr[2].ulValueLen = p_bytes;

	attr[3].type = CKA_VALUE;
	attr[3].pValue = isc_mem_get(key->mctx, p_bytes);
	if (attr[3].pValue == NULL)
		goto nomemory;
	memmove(attr[3].pValue, pub_key, p_bytes);
	attr[3].ulValueLen = p_bytes;

	key->keydata.pkey = dsa;

	return (ISC_R_SUCCESS);

    nomemory:
	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
		case CKA_SUBPRIME:
		case CKA_BASE:
		case CKA_VALUE:
			if (attr->pValue != NULL) {
				memset(attr->pValue, 0, attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (dsa->repr != NULL) {
		memset(dsa->repr, 0, dsa->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    dsa->repr,
			    dsa->attrcnt * sizeof(*attr));
	}
	memset(dsa, 0, sizeof(*dsa));
	isc_mem_put(key->mctx, dsa, sizeof(*dsa));
	return (ISC_R_NOMEMORY);
}

static isc_result_t
pkcs11dsa_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	pk11_object_t *dsa;
	CK_ATTRIBUTE *attr;
	CK_ATTRIBUTE *prime = NULL, *subprime = NULL, *base = NULL;
	CK_ATTRIBUTE *pub_key = NULL, *priv_key = NULL;
	dst_private_t priv;
	unsigned char bufs[5][128];

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	dsa = key->keydata.pkey;

	for (attr = pk11_attribute_first(dsa);
	     attr != NULL;
	     attr = pk11_attribute_next(dsa, attr))
		switch (attr->type) {
		case CKA_PRIME:
			prime = attr;
			break;
		case CKA_SUBPRIME:
			subprime = attr;
			break;
		case CKA_BASE:
			base = attr;
			break;
		case CKA_VALUE:
			pub_key = attr;
			break;
		case CKA_VALUE2:
			priv_key = attr;
			break;
		}
	if ((prime == NULL) || (subprime == NULL) || (base == NULL) ||
	    (pub_key == NULL) || (priv_key ==NULL))
		return (DST_R_NULLKEY);

	priv.elements[cnt].tag = TAG_DSA_PRIME;
	priv.elements[cnt].length = (unsigned short) prime->ulValueLen;
	memmove(bufs[cnt], prime->pValue, prime->ulValueLen);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_SUBPRIME;
	priv.elements[cnt].length = (unsigned short) subprime->ulValueLen;
	memmove(bufs[cnt], subprime->pValue, subprime->ulValueLen);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_BASE;
	priv.elements[cnt].length = (unsigned short) base->ulValueLen;
	memmove(bufs[cnt], base->pValue, base->ulValueLen);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_PRIVATE;
	priv.elements[cnt].length = (unsigned short) priv_key->ulValueLen;
	memmove(bufs[cnt], priv_key->pValue, priv_key->ulValueLen);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.elements[cnt].tag = TAG_DSA_PUBLIC;
	priv.elements[cnt].length = (unsigned short) pub_key->ulValueLen;
	memmove(bufs[cnt], pub_key->pValue, pub_key->ulValueLen);
	priv.elements[cnt].data = bufs[cnt];
	cnt++;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
pkcs11dsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	pk11_object_t *dsa = NULL;
	CK_ATTRIBUTE *attr;
	isc_mem_t *mctx = key->mctx;

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
		memset(&priv, 0, sizeof(priv));

		return (ISC_R_SUCCESS);
	}

	dsa = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dsa));
	if (dsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dsa, 0, sizeof(*dsa));
	key->keydata.pkey = dsa;

	dsa->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 5);
	if (dsa->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dsa->repr, 0, sizeof(*attr) * 5);
	dsa->attrcnt = 5;
	attr = dsa->repr;
	attr[0].type = CKA_PRIME;
	attr[1].type = CKA_SUBPRIME;
	attr[2].type = CKA_BASE;
	attr[3].type = CKA_VALUE;
	attr[4].type = CKA_VALUE2;

	for (i = 0; i < priv.nelements; i++) {
		CK_BYTE *bn;

		bn = isc_mem_get(key->mctx, priv.elements[i].length);
		if (bn == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memmove(bn, priv.elements[i].data, priv.elements[i].length);

		switch (priv.elements[i].tag) {
			case TAG_DSA_PRIME:
				attr = pk11_attribute_bytype(dsa, CKA_PRIME);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DSA_SUBPRIME:
				attr = pk11_attribute_bytype(dsa,
							     CKA_SUBPRIME);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DSA_BASE:
				attr = pk11_attribute_bytype(dsa, CKA_BASE);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DSA_PRIVATE:
				attr = pk11_attribute_bytype(dsa, CKA_VALUE2);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DSA_PUBLIC:
				attr = pk11_attribute_bytype(dsa, CKA_VALUE);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
		}
	}
	dst__privstruct_free(&priv, mctx);

	attr = pk11_attribute_bytype(dsa, CKA_PRIME);
	INSIST(attr != NULL);
	key->key_size = pk11_numbits(attr->pValue, attr->ulValueLen);

	return (ISC_R_SUCCESS);

 err:
	pkcs11dsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t pkcs11dsa_functions = {
	pkcs11dsa_createctx,
	NULL, /*%< createctx2 */
	pkcs11dsa_destroyctx,
	pkcs11dsa_adddata,
	pkcs11dsa_sign,
	pkcs11dsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	pkcs11dsa_compare,
	NULL, /*%< paramcompare */
	pkcs11dsa_generate,
	pkcs11dsa_isprivate,
	pkcs11dsa_destroy,
	pkcs11dsa_todns,
	pkcs11dsa_fromdns,
	pkcs11dsa_tofile,
	pkcs11dsa_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11dsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &pkcs11dsa_functions;
	return (ISC_R_SUCCESS);
}
#endif /* !PK11_DSA_DISABLE */

#else /* PKCS11CRYPTO */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO */
/*! \file */
