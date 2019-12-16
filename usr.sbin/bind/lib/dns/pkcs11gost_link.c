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

#include <config.h>

#if defined(PKCS11CRYPTO) && defined(HAVE_PKCS11_GOST)

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/sha2.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"
#include "dst_pkcs11.h"
#include "dst_gost.h"

#include <pk11/pk11.h>
#include <pk11/internal.h>
#define WANT_GOST_PARAMS
#include <pk11/constants.h>

#include <pkcs11/pkcs11.h>

/*
 * RU CryptoPro GOST keys:
 *  mechanisms:
 *    CKM_GOSTR3411
 *    CKM_GOSTR3410_WITH_GOSTR3411
 *    CKM_GOSTR3410_KEY_PAIR_GEN
 *  domain parameters:
 *    CKA_GOSTR3410_PARAMS (fixed BER OID 1.2.643.2.2.35.1)
 *    CKA_GOSTR3411_PARAMS (fixed BER OID 1.2.643.2.2.30.1)
 *    CKA_GOST28147_PARAMS (optional, don't use)
 *  public keys:
 *    object class CKO_PUBLIC_KEY
 *    key type CKK_GOSTR3410
 *    attribute CKA_VALUE (point Q)
 *    attribute CKA_GOSTR3410_PARAMS
 *    attribute CKA_GOSTR3411_PARAMS
 *    attribute CKA_GOST28147_PARAMS
 *  private keys:
 *    object class CKO_PRIVATE_KEY
 *    key type CKK_GOSTR3410
 *    attribute CKA_VALUE (big int d)
 *    attribute CKA_GOSTR3410_PARAMS
 *    attribute CKA_GOSTR3411_PARAMS
 *    attribute CKA_GOST28147_PARAMS
 *  point format: <x> <y> (little endian)
 */

#define CKA_VALUE2			CKA_PRIVATE_EXPONENT

#define ISC_GOST_SIGNATURELENGTH	64
#define ISC_GOST_PUBKEYLENGTH		64
#define ISC_GOST_KEYSIZE		256

/* HASH methods */

isc_result_t
isc_gost_init(isc_gost_t *ctx) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_GOSTR3411, NULL, 0 };
	int ret = ISC_R_SUCCESS;

	ret = pk11_get_session(ctx, OP_GOST, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, 0);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	PK11_CALL(pkcs_C_DigestInit, (ctx->session, &mech), ISC_R_FAILURE);
	return (ret);
}

void
isc_gost_invalidate(isc_gost_t *ctx) {
	CK_BYTE garbage[ISC_GOST_DIGESTLENGTH];
	CK_ULONG len = ISC_GOST_DIGESTLENGTH;

	if (ctx->handle == NULL)
		return;
	(void) pkcs_C_DigestFinal(ctx->session, garbage, &len);
	memset(garbage, 0, sizeof(garbage));
	pk11_return_session(ctx);
}

isc_result_t
isc_gost_update(isc_gost_t *ctx, const unsigned char *buf, unsigned int len) {
	CK_RV rv;
	CK_BYTE_PTR pPart;
	int ret = ISC_R_SUCCESS;

	DE_CONST(buf, pPart);
	PK11_CALL(pkcs_C_DigestUpdate,
		  (ctx->session, pPart, (CK_ULONG) len),
		  ISC_R_FAILURE);
	return (ret);
}

isc_result_t
isc_gost_final(isc_gost_t *ctx, unsigned char *digest) {
	CK_RV rv;
	CK_ULONG len = ISC_GOST_DIGESTLENGTH;
	int ret = ISC_R_SUCCESS;

	PK11_CALL(pkcs_C_DigestFinal,
		  (ctx->session, (CK_BYTE_PTR) digest, &len),
		  ISC_R_FAILURE);
	pk11_return_session(ctx);
	return (ret);
}

/* DST methods */

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

#define DST_RET(a) {ret = a; goto err;}

static isc_result_t pkcs11gost_todns(const dst_key_t *key, isc_buffer_t *data);
static void pkcs11gost_destroy(dst_key_t *key);

static isc_result_t
pkcs11gost_createctx_sign(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_GOSTR3410_WITH_GOSTR3411, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_GOSTR3410;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_VALUE, NULL, 0 },
		{ CKA_GOSTR3410_PARAMS, pk11_gost_a_paramset,
		  (CK_ULONG) sizeof(pk11_gost_a_paramset) },
		{ CKA_GOSTR3411_PARAMS, pk11_gost_paramset,
		  (CK_ULONG) sizeof(pk11_gost_paramset) }
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *gost;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	REQUIRE(key != NULL);
	gost = key->keydata.pkey;
	REQUIRE(gost != NULL);

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_GOST, ISC_TRUE, ISC_FALSE,
			       gost->reqlogon, NULL,
			       pk11_get_best_token(OP_GOST));
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (gost->ontoken && (gost->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = gost->ontoken;
		pk11_ctx->object = gost->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(gost);
	     attr != NULL;
	     attr = pk11_attribute_next(gost, attr))
		switch (attr->type) {
		case CKA_VALUE2:
			INSIST(keyTemplate[6].type == CKA_VALUE);
			keyTemplate[6].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[6].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[6].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[6].ulValueLen = attr->ulValueLen;
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

	PK11_RET(pkcs_C_SignInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 6; i <= 6; i++)
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
	for (i = 6; i <= 6; i++)
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
pkcs11gost_createctx_verify(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_GOSTR3410_WITH_GOSTR3411, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_GOSTR3410;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_VALUE, NULL, 0 },
		{ CKA_GOSTR3410_PARAMS, pk11_gost_a_paramset,
		  (CK_ULONG) sizeof(pk11_gost_a_paramset) },
		{ CKA_GOSTR3411_PARAMS, pk11_gost_paramset,
		  (CK_ULONG) sizeof(pk11_gost_paramset) }
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *gost;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;
	unsigned int i;

	REQUIRE(key != NULL);
	gost = key->keydata.pkey;
	REQUIRE(gost != NULL);

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_GOST, ISC_TRUE, ISC_FALSE,
			       gost->reqlogon, NULL,
			       pk11_get_best_token(OP_GOST));
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (gost->ontoken && (gost->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = gost->ontoken;
		pk11_ctx->object = gost->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(gost);
	     attr != NULL;
	     attr = pk11_attribute_next(gost, attr))
		switch (attr->type) {
		case CKA_VALUE:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		}
	pk11_ctx->object = CK_INVALID_HANDLE;
	pk11_ctx->ontoken = ISC_FALSE;
	PK11_RET(pkcs_C_CreateObject,
		 (pk11_ctx->session,
		  keyTemplate, (CK_ULONG) 8,
		  &pk11_ctx->object),
		 ISC_R_FAILURE);

    token_key:

	PK11_RET(pkcs_C_VerifyInit,
		 (pk11_ctx->session, &mech, pk11_ctx->object),
		 ISC_R_FAILURE);

	dctx->ctxdata.pk11_ctx = pk11_ctx;

	for (i = 5; i <= 5; i++)
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
	for (i = 5; i <= 5; i++)
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
pkcs11gost_createctx(dst_key_t *key, dst_context_t *dctx) {
	if (dctx->use == DO_SIGN)
		return (pkcs11gost_createctx_sign(key, dctx));
	else
		return (pkcs11gost_createctx_verify(key, dctx));
}

static void
pkcs11gost_destroyctx(dst_context_t *dctx) {
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
pkcs11gost_adddata(dst_context_t *dctx, const isc_region_t *data) {
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
pkcs11gost_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	CK_RV rv;
	CK_ULONG siglen = ISC_GOST_SIGNATURELENGTH;
	isc_region_t r;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	isc_buffer_availableregion(sig, &r);
	if (r.length < ISC_GOST_SIGNATURELENGTH)
		return (ISC_R_NOSPACE);

	PK11_RET(pkcs_C_SignFinal,
		 (pk11_ctx->session, (CK_BYTE_PTR) r.base, &siglen),
		 DST_R_SIGNFAILURE);
	if (siglen != ISC_GOST_SIGNATURELENGTH)
		return (DST_R_SIGNFAILURE);

	isc_buffer_add(sig, ISC_GOST_SIGNATURELENGTH);

    err:
	return (ret);
}

static isc_result_t
pkcs11gost_verify(dst_context_t *dctx, const isc_region_t *sig) {
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

static isc_boolean_t
pkcs11gost_compare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *gost1, *gost2;
	CK_ATTRIBUTE *attr1, *attr2;

	gost1 = key1->keydata.pkey;
	gost2 = key2->keydata.pkey;

	if ((gost1 == NULL) && (gost2 == NULL))
		return (ISC_TRUE);
	else if ((gost1 == NULL) || (gost2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(gost1, CKA_VALUE);
	attr2 = pk11_attribute_bytype(gost2, CKA_VALUE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(gost1, CKA_VALUE2);
	attr2 = pk11_attribute_bytype(gost2, CKA_VALUE2);
	if (((attr1 != NULL) || (attr2 != NULL)) &&
	    ((attr1 == NULL) || (attr2 == NULL) ||
	     (attr1->ulValueLen != attr2->ulValueLen) ||
	     !isc_safe_memequal(attr1->pValue, attr2->pValue,
				attr1->ulValueLen)))
		return (ISC_FALSE);

	if (!gost1->ontoken && !gost2->ontoken)
		return (ISC_TRUE);
	else if (gost1->ontoken || gost2->ontoken ||
		 (gost1->object != gost2->object))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

static isc_result_t
pkcs11gost_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_GOSTR3410_KEY_PAIR_GEN, NULL, 0 };
	CK_KEY_TYPE  keyType = CKK_GOSTR3410;
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_ATTRIBUTE pubTemplate[] =
	{
		{ CKA_CLASS, &pubClass, (CK_ULONG) sizeof(pubClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_GOSTR3410_PARAMS, pk11_gost_a_paramset,
		  (CK_ULONG) sizeof(pk11_gost_a_paramset) },
		{ CKA_GOSTR3411_PARAMS, pk11_gost_paramset,
		  (CK_ULONG) sizeof(pk11_gost_paramset) }
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
	pk11_object_t *gost;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;

	UNUSED(unused);
	UNUSED(callback);

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_GOST, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, pk11_get_best_token(OP_GOST));
	if (ret != ISC_R_SUCCESS)
		goto err;

	PK11_RET(pkcs_C_GenerateKeyPair,
		 (pk11_ctx->session, &mech,
		  pubTemplate, (CK_ULONG) 7,
		  privTemplate, (CK_ULONG) 7,
		  &pub, &priv),
		 DST_R_CRYPTOFAILURE);

	gost = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*gost));
	if (gost == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(gost, 0, sizeof(*gost));
	key->keydata.pkey = gost;
	key->key_size = ISC_GOST_KEYSIZE;
	gost->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx,
						  sizeof(*attr) * 2);
	if (gost->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(gost->repr, 0, sizeof(*attr) * 2);
	gost->attrcnt = 2;

	attr = gost->repr;
	attr[0].type = CKA_VALUE;
	attr[1].type = CKA_VALUE2;

	attr = gost->repr;
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
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ISC_R_SUCCESS);

    err:
	pkcs11gost_destroy(key);
	if (priv != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	if (pub != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_boolean_t
pkcs11gost_isprivate(const dst_key_t *key) {
	pk11_object_t *gost = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (gost == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(gost, CKA_VALUE2);
	return (ISC_TF((attr != NULL) || gost->ontoken));
}

static void
pkcs11gost_destroy(dst_key_t *key) {
	pk11_object_t *gost = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (gost == NULL)
		return;

	INSIST((gost->object == CK_INVALID_HANDLE) || gost->ontoken);

	for (attr = pk11_attribute_first(gost);
	     attr != NULL;
	     attr = pk11_attribute_next(gost, attr))
		switch (attr->type) {
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
	if (gost->repr != NULL) {
		memset(gost->repr, 0, gost->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    gost->repr,
			    gost->attrcnt * sizeof(*attr));
	}
	memset(gost, 0, sizeof(*gost));
	isc_mem_put(key->mctx, gost, sizeof(*gost));
	key->keydata.pkey = NULL;
}

static isc_result_t
pkcs11gost_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *gost;
	isc_region_t r;
	CK_ATTRIBUTE *attr;

	REQUIRE(key->keydata.pkey != NULL);

	gost = key->keydata.pkey;
	attr = pk11_attribute_bytype(gost, CKA_VALUE);
	if ((attr == NULL) || (attr->ulValueLen != ISC_GOST_PUBKEYLENGTH))
		return (ISC_R_FAILURE);

	isc_buffer_availableregion(data, &r);
	if (r.length < ISC_GOST_PUBKEYLENGTH)
		return (ISC_R_NOSPACE);
	memmove(r.base, (CK_BYTE_PTR) attr->pValue, ISC_GOST_PUBKEYLENGTH);
	isc_buffer_add(data, ISC_GOST_PUBKEYLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11gost_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *gost;
	isc_region_t r;
	CK_ATTRIBUTE *attr;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	if (r.length != ISC_GOST_PUBKEYLENGTH)
		return (DST_R_INVALIDPUBLICKEY);

	gost = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*gost));
	if (gost == NULL)
		return (ISC_R_NOMEMORY);
	memset(gost, 0, sizeof(*gost));
	gost->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr));
	if (gost->repr == NULL)
		goto nomemory;
	gost->attrcnt = 1;

	attr = gost->repr;
	attr->type = CKA_VALUE;
	attr->pValue = isc_mem_get(key->mctx, ISC_GOST_PUBKEYLENGTH);
	if (attr->pValue == NULL)
		goto nomemory;
	memmove((CK_BYTE_PTR) attr->pValue, r.base, ISC_GOST_PUBKEYLENGTH);
	attr->ulValueLen = ISC_GOST_PUBKEYLENGTH;

	isc_buffer_forward(data, ISC_GOST_PUBKEYLENGTH);
	key->keydata.pkey = gost;
	key->key_size = ISC_GOST_KEYSIZE;
	return (ISC_R_SUCCESS);

 nomemory:
	for (attr = pk11_attribute_first(gost);
	     attr != NULL;
	     attr = pk11_attribute_next(gost, attr))
		switch (attr->type) {
		case CKA_VALUE:
			if (attr->pValue != NULL) {
				memset(attr->pValue, 0, attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (gost->repr != NULL) {
		memset(gost->repr, 0, gost->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    gost->repr,
			    gost->attrcnt * sizeof(*attr));
	}
	memset(gost, 0, sizeof(*gost));
	isc_mem_put(key->mctx, gost, sizeof(*gost));
	return (ISC_R_NOMEMORY);
}

static unsigned char gost_private_der[39] = {
	0x30, 0x45, 0x02, 0x01, 0x00, 0x30, 0x1c, 0x06,
	0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x13, 0x30,
	0x12, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02,
	0x23, 0x01, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02,
	0x02, 0x1e, 0x01, 0x04, 0x22, 0x02, 0x20
};

#ifdef PREFER_GOSTASN1

static isc_result_t
pkcs11gost_tofile(const dst_key_t *key, const char *directory) {
	isc_result_t ret;
	pk11_object_t *gost;
	dst_private_t priv;
	unsigned char *buf = NULL;
	unsigned int i = 0;
	CK_ATTRIBUTE *attr;
	int adj;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	gost = key->keydata.pkey;
	attr = pk11_attribute_bytype(gost, CKA_VALUE2);
	if (attr != NULL) {
		buf = isc_mem_get(key->mctx, attr->ulValueLen + 39);
		if (buf == NULL)
			return (ISC_R_NOMEMORY);
		priv.elements[i].tag = TAG_GOST_PRIVASN1;
		priv.elements[i].length =
			(unsigned short) attr->ulValueLen + 39;
		memmove(buf, gost_private_der, 39);
		memmove(buf + 39, attr->pValue, attr->ulValueLen);
		adj = (int) attr->ulValueLen - 32;
		if (adj != 0) {
			buf[1] += adj;
			buf[36] += adj;
			buf[38] += adj;
		}
		priv.elements[i].data = buf;
		i++;
	} else
		return (DST_R_CRYPTOFAILURE);

	priv.nelements = i;
	ret = dst__privstruct_writefile(key, &priv, directory);

	if (buf != NULL) {
		memset(buf, 0, attr->ulValueLen);
		isc_mem_put(key->mctx, buf, attr->ulValueLen);
	}
	return (ret);
}

#else

static isc_result_t
pkcs11gost_tofile(const dst_key_t *key, const char *directory) {
	isc_result_t ret;
	pk11_object_t *gost;
	dst_private_t priv;
	unsigned char *buf = NULL;
	unsigned int i = 0;
	CK_ATTRIBUTE *attr;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	gost = key->keydata.pkey;
	attr = pk11_attribute_bytype(gost, CKA_VALUE2);
	if (attr != NULL) {
		buf = isc_mem_get(key->mctx, attr->ulValueLen);
		if (buf == NULL)
			return (ISC_R_NOMEMORY);
		priv.elements[i].tag = TAG_GOST_PRIVRAW;
		priv.elements[i].length = (unsigned short) attr->ulValueLen;
		memmove(buf, attr->pValue, attr->ulValueLen);
		priv.elements[i].data = buf;
		i++;
	} else
		return (DST_R_CRYPTOFAILURE);

	priv.nelements = i;
	ret = dst__privstruct_writefile(key, &priv, directory);

	if (buf != NULL) {
		memset(buf, 0, attr->ulValueLen);
		isc_mem_put(key->mctx, buf, attr->ulValueLen);
	}
	return (ret);
}
#endif

static isc_result_t
pkcs11gost_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	pk11_object_t *gost = NULL;
	CK_ATTRIBUTE *attr, *pattr;
	isc_mem_t *mctx = key->mctx;

	if ((pub == NULL) || (pub->keydata.pkey == NULL))
		DST_RET(DST_R_INVALIDPRIVATEKEY);

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_ECDSA256, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	if (key->external) {
		if (priv.nelements != 0)
			DST_RET(DST_R_INVALIDPRIVATEKEY);

		key->keydata.pkey = pub->keydata.pkey;
		pub->keydata.pkey = NULL;
		key->key_size = pub->key_size;

		dst__privstruct_free(&priv, mctx);
		memset(&priv, 0, sizeof(priv));

		return (ISC_R_SUCCESS);
	}

	if (priv.elements[0].tag == TAG_GOST_PRIVASN1) {
		int adj = (int) priv.elements[0].length - (39 + 32);
		unsigned char buf[39];

		if ((adj > 0) || (adj < -31))
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		memmove(buf, gost_private_der, 39);
		if (adj != 0) {
			buf[1] += adj;
			buf[36] += adj;
			buf[38] += adj;
		}
		if (!isc_safe_memequal(priv.elements[0].data, buf, 39))
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		priv.elements[0].tag = TAG_GOST_PRIVRAW;
		priv.elements[0].length -= 39;
		memmove(priv.elements[0].data,
			priv.elements[0].data + 39,
			32 + adj);
	}

	gost = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*gost));
	if (gost == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(gost, 0, sizeof(*gost));
	key->keydata.pkey = gost;
	key->key_size = ISC_GOST_KEYSIZE;

	gost->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx,
						  sizeof(*attr) * 2);
	if (gost->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(gost->repr, 0, sizeof(*attr) * 2);
	gost->attrcnt = 2;

	attr = gost->repr;
	attr->type = CKA_VALUE;
	pattr = pk11_attribute_bytype(pub->keydata.pkey, CKA_VALUE);
	INSIST(pattr != NULL);
	attr->pValue = isc_mem_get(key->mctx, pattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pattr->pValue, pattr->ulValueLen);
	attr->ulValueLen = pattr->ulValueLen;

	attr++;
	attr->type = CKA_VALUE2;
	attr->pValue = isc_mem_get(key->mctx, priv.elements[0].length);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, priv.elements[0].data, priv.elements[0].length);
	attr->ulValueLen = priv.elements[0].length;

	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));

	return (ISC_R_SUCCESS);

 err:
	pkcs11gost_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t pkcs11gost_functions = {
	pkcs11gost_createctx,
	NULL, /*%< createctx2 */
	pkcs11gost_destroyctx,
	pkcs11gost_adddata,
	pkcs11gost_sign,
	pkcs11gost_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	pkcs11gost_compare,
	NULL, /*%< paramcompare */
	pkcs11gost_generate,
	pkcs11gost_isprivate,
	pkcs11gost_destroy,
	pkcs11gost_todns,
	pkcs11gost_fromdns,
	pkcs11gost_tofile,
	pkcs11gost_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11gost_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &pkcs11gost_functions;
	return (ISC_R_SUCCESS);
}

#else /* PKCS11CRYPTO && HAVE_PKCS11_GOST */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO && HAVE_PKCS11_GOST */
/*! \file */
