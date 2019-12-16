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

#ifndef PK11_DH_DISABLE

#include <ctype.h>

#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"
#include "dst_pkcs11.h"

#include <pk11/pk11.h>
#include <pk11/internal.h>
#define WANT_DH_PRIMES
#include <pk11/constants.h>

#include <pkcs11/pkcs11.h>

/*
 * PKCS#3 DH keys:
 *  mechanisms:
 *    CKM_DH_PKCS_PARAMETER_GEN,
 *    CKM_DH_PKCS_KEY_PAIR_GEN,
 *    CKM_DH_PKCS_DERIVE
 *  domain parameters:
 *    object class CKO_DOMAIN_PARAMETERS
 *    key type CKK_DH
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_BASE (base g)
 *    optional attribute CKA_PRIME_BITS (p length in bits)
 *  public key:
 *    object class CKO_PUBLIC_KEY
 *    key type CKK_DH
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_BASE (base g)
 *    attribute CKA_VALUE (public value y)
 *  private key:
 *    object class CKO_PRIVATE_KEY
 *    key type CKK_DH
 *    attribute CKA_PRIME (prime p)
 *    attribute CKA_BASE (base g)
 *    attribute CKA_VALUE (private value x)
 *    optional attribute CKA_VALUE_BITS (x length in bits)
 *  reuse CKA_PRIVATE_EXPONENT for key pair private value
 */

#define CKA_VALUE2	CKA_PRIVATE_EXPONENT

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

#define DST_RET(a) {ret = a; goto err;}

static void pkcs11dh_destroy(dst_key_t *key);
static isc_result_t pkcs11dh_todns(const dst_key_t *key, isc_buffer_t *data);

static isc_result_t
pkcs11dh_loadpriv(const dst_key_t *key,
		  CK_SESSION_HANDLE session,
		  CK_OBJECT_HANDLE *hKey)
{
	CK_RV rv;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_DH;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_DERIVE, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		{ CKA_VALUE, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	const pk11_object_t *priv;
	isc_result_t ret;
	unsigned int i;

	priv = key->keydata.pkey;
	if ((priv->object != CK_INVALID_HANDLE) && priv->ontoken) {
		*hKey = priv->object;
		return (ISC_R_SUCCESS);
	}

	attr = pk11_attribute_bytype(priv, CKA_PRIME);
	if (attr == NULL)
		return (DST_R_INVALIDPRIVATEKEY);
	keyTemplate[6].pValue = isc_mem_get(key->mctx, attr->ulValueLen);
	if (keyTemplate[6].pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(keyTemplate[6].pValue, attr->pValue, attr->ulValueLen);
	keyTemplate[6].ulValueLen = attr->ulValueLen;

	attr = pk11_attribute_bytype(priv, CKA_BASE);
	if (attr == NULL)
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	keyTemplate[7].pValue = isc_mem_get(key->mctx, attr->ulValueLen);
	if (keyTemplate[7].pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(keyTemplate[7].pValue, attr->pValue, attr->ulValueLen);
	keyTemplate[7].ulValueLen = attr->ulValueLen;

	attr = pk11_attribute_bytype(priv, CKA_VALUE2);
	if (attr == NULL)
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	keyTemplate[8].pValue = isc_mem_get(key->mctx, attr->ulValueLen);
	if (keyTemplate[8].pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(keyTemplate[8].pValue, attr->pValue, attr->ulValueLen);
	keyTemplate[8].ulValueLen = attr->ulValueLen;

	PK11_CALL(pkcs_C_CreateObject,
		  (session, keyTemplate, (CK_ULONG) 9, hKey),
		  DST_R_COMPUTESECRETFAILURE);
	if (rv == CKR_OK)
		ret = ISC_R_SUCCESS;

    err:
	for (i = 6; i <= 8; i++)
		if (keyTemplate[i].pValue != NULL) {
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(key->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	return (ret);
}

static isc_result_t
pkcs11dh_computesecret(const dst_key_t *pub, const dst_key_t *priv,
		       isc_buffer_t *secret)
{
	CK_RV rv;
	CK_MECHANISM mech = { CKM_DH_PKCS_DERIVE, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
	CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
	CK_OBJECT_HANDLE hDerived = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_ATTRIBUTE *attr;
	CK_ULONG secLen;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SENSITIVE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_EXTRACTABLE, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_VALUE_LEN, &secLen, (CK_ULONG) sizeof(secLen) }
	};
	CK_ATTRIBUTE valTemplate[] =
	{
		{ CKA_VALUE, NULL, 0 }
	};
	CK_BYTE *secValue;
	pk11_context_t ctx;
	isc_result_t ret;
	unsigned int i;
	isc_region_t r;

	REQUIRE(pub->keydata.pkey != NULL);
	REQUIRE(priv->keydata.pkey != NULL);
	REQUIRE(priv->keydata.pkey->repr != NULL);
	attr = pk11_attribute_bytype(pub->keydata.pkey, CKA_PRIME);
	if (attr == NULL)
		return (DST_R_INVALIDPUBLICKEY);
	REQUIRE(attr != NULL);
	secLen = attr->ulValueLen;
	attr = pk11_attribute_bytype(pub->keydata.pkey, CKA_VALUE);
	if (attr == NULL)
		return (DST_R_INVALIDPUBLICKEY);

	ret = pk11_get_session(&ctx, OP_DH, ISC_TRUE, ISC_FALSE,
			       priv->keydata.pkey->reqlogon, NULL,
			       pk11_get_best_token(OP_DH));
	if (ret != ISC_R_SUCCESS)
		return (ret);

	mech.ulParameterLen = attr->ulValueLen;
	mech.pParameter = isc_mem_get(pub->mctx, mech.ulParameterLen);
	if (mech.pParameter == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(mech.pParameter, attr->pValue, mech.ulParameterLen);

	ret = pkcs11dh_loadpriv(priv, ctx.session, &hKey);
	if (ret != ISC_R_SUCCESS)
		goto err;

	PK11_RET(pkcs_C_DeriveKey,
		 (ctx.session, &mech, hKey,
		  keyTemplate, (CK_ULONG) 6, &hDerived),
		 DST_R_COMPUTESECRETFAILURE);

	attr = valTemplate;
	PK11_RET(pkcs_C_GetAttributeValue,
		 (ctx.session, hDerived, attr, (CK_ULONG) 1),
		 DST_R_CRYPTOFAILURE);
	attr->pValue = isc_mem_get(pub->mctx, attr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(attr->pValue, 0, attr->ulValueLen);
	PK11_RET(pkcs_C_GetAttributeValue,
		 (ctx.session, hDerived, attr, (CK_ULONG) 1),
		 DST_R_CRYPTOFAILURE);

	/* strip leading zeros */
	secValue = (CK_BYTE_PTR) attr->pValue;
	for (i = 0; i < attr->ulValueLen; i++)
		if (secValue[i] != 0)
			break;
	isc_buffer_availableregion(secret, &r);
	if (r.length < attr->ulValueLen - i)
		DST_RET(ISC_R_NOSPACE);
	memmove(r.base, secValue + i, attr->ulValueLen - i);
	isc_buffer_add(secret, attr->ulValueLen - i);
	ret = ISC_R_SUCCESS;

    err:
	if (hDerived != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(ctx.session, hDerived);
	if (valTemplate[0].pValue != NULL) {
		memset(valTemplate[0].pValue, 0, valTemplate[0].ulValueLen);
		isc_mem_put(pub->mctx,
			    valTemplate[0].pValue,
			    valTemplate[0].ulValueLen);
	}
	if ((hKey != CK_INVALID_HANDLE) && !priv->keydata.pkey->ontoken)
		(void) pkcs_C_DestroyObject(ctx.session, hKey);
	if (mech.pParameter != NULL) {
		memset(mech.pParameter, 0, mech.ulParameterLen);
		isc_mem_put(pub->mctx, mech.pParameter, mech.ulParameterLen);
	}
	pk11_return_session(&ctx);
	return (ret);
}

static isc_boolean_t
pkcs11dh_compare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *dh1, *dh2;
	CK_ATTRIBUTE *attr1, *attr2;

	dh1 = key1->keydata.pkey;
	dh2 = key2->keydata.pkey;

	if ((dh1 == NULL) && (dh2 == NULL))
		return (ISC_TRUE);
	else if ((dh1 == NULL) || (dh2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_PRIME);
	attr2 = pk11_attribute_bytype(dh2, CKA_PRIME);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_BASE);
	attr2 = pk11_attribute_bytype(dh2, CKA_BASE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_VALUE);
	attr2 = pk11_attribute_bytype(dh2, CKA_VALUE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_VALUE2);
	attr2 = pk11_attribute_bytype(dh2, CKA_VALUE2);
	if (((attr1 != NULL) || (attr2 != NULL)) &&
	    ((attr1 == NULL) || (attr2 == NULL) ||
	     (attr1->ulValueLen != attr2->ulValueLen) ||
	     !isc_safe_memequal(attr1->pValue, attr2->pValue,
				attr1->ulValueLen)))
		return (ISC_FALSE);

	if (!dh1->ontoken && !dh2->ontoken)
		return (ISC_TRUE);
	else if (dh1->ontoken || dh2->ontoken ||
		 (dh1->object != dh2->object))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

static isc_boolean_t
pkcs11dh_paramcompare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *dh1, *dh2;
	CK_ATTRIBUTE *attr1, *attr2;

	dh1 = key1->keydata.pkey;
	dh2 = key2->keydata.pkey;

	if ((dh1 == NULL) && (dh2 == NULL))
		return (ISC_TRUE);
	else if ((dh1 == NULL) || (dh2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_PRIME);
	attr2 = pk11_attribute_bytype(dh2, CKA_PRIME);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(dh1, CKA_BASE);
	attr2 = pk11_attribute_bytype(dh2, CKA_BASE);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

static isc_result_t
pkcs11dh_generate(dst_key_t *key, int generator, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_DH_PKCS_PARAMETER_GEN, NULL, 0 };
	CK_OBJECT_HANDLE domainparams = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS dClass = CKO_DOMAIN_PARAMETERS;
	CK_KEY_TYPE keyType = CKK_DH;
	CK_ULONG bits = 0;
	CK_ATTRIBUTE dTemplate[] =
	{
		{ CKA_CLASS, &dClass, (CK_ULONG) sizeof(dClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIME_BITS, &bits, (CK_ULONG) sizeof(bits) }
	};
	CK_ATTRIBUTE pTemplate[] =
	{
		{ CKA_PRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 }
	};
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_ATTRIBUTE pubTemplate[] =
	{
		{ CKA_CLASS, &pubClass, (CK_ULONG) sizeof(pubClass) },
		{ CKA_KEY_TYPE,&keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
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
		{ CKA_DERIVE, &truevalue, (CK_ULONG) sizeof(truevalue) },
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *dh = NULL;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;

	UNUSED(callback);

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_DH, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, pk11_get_best_token(OP_DH));
	if (ret != ISC_R_SUCCESS)
		goto err;

	bits = key->key_size;
	if ((generator == 0) &&
	    ((bits == 768) || (bits == 1024) || (bits == 1536))) {
		if (bits == 768) {
			pubTemplate[4].pValue =
				isc_mem_get(key->mctx, sizeof(pk11_dh_bn768));
			if (pubTemplate[4].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(pubTemplate[4].pValue,
				pk11_dh_bn768, sizeof(pk11_dh_bn768));
			pubTemplate[4].ulValueLen = sizeof(pk11_dh_bn768);
		} else if (bits == 1024) {
			pubTemplate[4].pValue =
				isc_mem_get(key->mctx, sizeof(pk11_dh_bn1024));
			if (pubTemplate[4].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(pubTemplate[4].pValue,
				pk11_dh_bn1024, sizeof(pk11_dh_bn1024));
			pubTemplate[4].ulValueLen = sizeof(pk11_dh_bn1024);
		} else {
			pubTemplate[4].pValue =
				isc_mem_get(key->mctx, sizeof(pk11_dh_bn1536));
			if (pubTemplate[4].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(pubTemplate[4].pValue,
				pk11_dh_bn1536, sizeof(pk11_dh_bn1536));
			pubTemplate[4].ulValueLen = sizeof(pk11_dh_bn1536);
		}
		pubTemplate[5].pValue = isc_mem_get(key->mctx,
						    sizeof(pk11_dh_bn2));
		if (pubTemplate[5].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memmove(pubTemplate[5].pValue, pk11_dh_bn2,
			sizeof(pk11_dh_bn2));
		pubTemplate[5].ulValueLen = sizeof(pk11_dh_bn2);
	} else {
		PK11_RET(pkcs_C_GenerateKey,
			 (pk11_ctx->session, &mech,
			  dTemplate, (CK_ULONG) 5, &domainparams),
			 DST_R_CRYPTOFAILURE);
		PK11_RET(pkcs_C_GetAttributeValue,
			 (pk11_ctx->session, domainparams,
			  pTemplate, (CK_ULONG) 2),
			 DST_R_CRYPTOFAILURE);
		pTemplate[0].pValue = isc_mem_get(key->mctx,
						  pTemplate[0].ulValueLen);
		if (pTemplate[0].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(pTemplate[0].pValue, 0, pTemplate[0].ulValueLen);
		pTemplate[1].pValue = isc_mem_get(key->mctx,
						  pTemplate[1].ulValueLen);
		if (pTemplate[1].pValue == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memset(pTemplate[1].pValue, 0, pTemplate[1].ulValueLen);
		PK11_RET(pkcs_C_GetAttributeValue,
			 (pk11_ctx->session, domainparams,
			  pTemplate, (CK_ULONG) 2),
			 DST_R_CRYPTOFAILURE);

		pubTemplate[4].pValue = pTemplate[0].pValue;
		pubTemplate[4].ulValueLen = pTemplate[0].ulValueLen;
		pTemplate[0].pValue = NULL;
		pubTemplate[5].pValue = pTemplate[1].pValue;
		pubTemplate[5].ulValueLen = pTemplate[1].ulValueLen;
		pTemplate[1].pValue = NULL;
	}

	mech.mechanism = CKM_DH_PKCS_KEY_PAIR_GEN;
	PK11_RET(pkcs_C_GenerateKeyPair,
		 (pk11_ctx->session, &mech,
		  pubTemplate, (CK_ULONG) 6,
		  privTemplate, (CK_ULONG) 7,
		  &pub, &priv),
		 DST_R_CRYPTOFAILURE);

	dh = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dh));
	if (dh == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dh, 0,  sizeof(*dh));
	key->keydata.pkey = dh;
	dh->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 4);
	if (dh->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dh->repr, 0, sizeof(*attr) * 4);
	dh->attrcnt = 4;

	attr = dh->repr;
	attr[0].type = CKA_PRIME;
	attr[0].pValue = pubTemplate[4].pValue;
	attr[0].ulValueLen = pubTemplate[4].ulValueLen;
	pubTemplate[4].pValue = NULL;

	attr[1].type = CKA_BASE;
	attr[1].pValue = pubTemplate[5].pValue;
	attr[1].ulValueLen = pubTemplate[5].ulValueLen;
	pubTemplate[5].pValue =NULL;

	attr += 2;
	attr->type = CKA_VALUE;
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
	(void) pkcs_C_DestroyObject(pk11_ctx->session, domainparams);
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ISC_R_SUCCESS);

    err:
	pkcs11dh_destroy(key);
	if (priv != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	if (pub != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	if (domainparams != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, domainparams);

	if (pubTemplate[4].pValue != NULL) {
		memset(pubTemplate[4].pValue, 0, pubTemplate[4].ulValueLen);
		isc_mem_put(key->mctx,
			    pubTemplate[4].pValue,
			    pubTemplate[4].ulValueLen);
	}
	if (pubTemplate[5].pValue != NULL) {
		memset(pubTemplate[5].pValue, 0, pubTemplate[5].ulValueLen);
		isc_mem_put(key->mctx,
			    pubTemplate[5].pValue,
			    pubTemplate[5].ulValueLen);
	}
	if (pTemplate[0].pValue != NULL) {
		memset(pTemplate[0].pValue, 0, pTemplate[0].ulValueLen);
		isc_mem_put(key->mctx,
			    pTemplate[0].pValue,
			    pTemplate[0].ulValueLen);
	}
	if (pTemplate[1].pValue != NULL) {
		memset(pTemplate[1].pValue, 0, pTemplate[1].ulValueLen);
		isc_mem_put(key->mctx,
			    pTemplate[1].pValue,
			    pTemplate[1].ulValueLen);
	}

	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static isc_boolean_t
pkcs11dh_isprivate(const dst_key_t *key) {
	pk11_object_t *dh = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (dh == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(dh, CKA_VALUE2);
	return (ISC_TF((attr != NULL) || dh->ontoken));
}

static void
pkcs11dh_destroy(dst_key_t *key) {
	pk11_object_t *dh = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (dh == NULL)
		return;

	INSIST((dh->object == CK_INVALID_HANDLE) || dh->ontoken);

	for (attr = pk11_attribute_first(dh);
	     attr != NULL;
	     attr = pk11_attribute_next(dh, attr))
		switch (attr->type) {
		case CKA_VALUE:
		case CKA_VALUE2:
		case CKA_PRIME:
		case CKA_BASE:
			if (attr->pValue != NULL) {
				memset(attr->pValue, 0, attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (dh->repr != NULL) {
		memset(dh->repr, 0, dh->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx, dh->repr, dh->attrcnt * sizeof(*attr));
	}
	memset(dh, 0, sizeof(*dh));
	isc_mem_put(key->mctx, dh, sizeof(*dh));
	key->keydata.pkey = NULL;
}

static void
uint16_toregion(isc_uint16_t val, isc_region_t *region) {
	*region->base = (val & 0xff00) >> 8;
	isc_region_consume(region, 1);
	*region->base = (val & 0x00ff);
	isc_region_consume(region, 1);
}

static isc_uint16_t
uint16_fromregion(isc_region_t *region) {
	isc_uint16_t val;
	unsigned char *cp = region->base;

	val = ((unsigned int)(cp[0])) << 8;
	val |= ((unsigned int)(cp[1]));

	isc_region_consume(region, 2);

	return (val);
}

static isc_result_t
pkcs11dh_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *dh;
	CK_ATTRIBUTE *attr;
	isc_region_t r;
	isc_uint16_t dnslen, plen = 0, glen = 0, publen = 0;
	CK_BYTE *prime = NULL, *base = NULL, *pub = NULL;

	REQUIRE(key->keydata.pkey != NULL);

	dh = key->keydata.pkey;

	for (attr = pk11_attribute_first(dh);
	     attr != NULL;
	     attr = pk11_attribute_next(dh, attr))
		switch (attr->type) {
		case CKA_VALUE:
			pub = (CK_BYTE *) attr->pValue;
			publen = (isc_uint16_t) attr->ulValueLen;
			break;
		case CKA_PRIME:
			prime = (CK_BYTE *) attr->pValue;
			plen = (isc_uint16_t) attr->ulValueLen;
			break;
		case CKA_BASE:
			base = (CK_BYTE *) attr->pValue;
			glen = (isc_uint16_t) attr->ulValueLen;
			break;
		}
	REQUIRE((prime != NULL) && (base != NULL) && (pub != NULL));

	isc_buffer_availableregion(data, &r);

	if ((glen == 1) && isc_safe_memequal(pk11_dh_bn2, base, glen) &&
	    (((plen == sizeof(pk11_dh_bn768)) &&
	      isc_safe_memequal(pk11_dh_bn768, prime, plen)) ||
	     ((plen == sizeof(pk11_dh_bn1024)) &&
	      isc_safe_memequal(pk11_dh_bn1024, prime, plen)) ||
	     ((plen == sizeof(pk11_dh_bn1536)) &&
	      isc_safe_memequal(pk11_dh_bn1536, prime, plen)))) {
		plen = 1;
		glen = 0;
	}

	dnslen = plen + glen + publen + 6;
	if (r.length < (unsigned int) dnslen)
		return (ISC_R_NOSPACE);

	uint16_toregion(plen, &r);
	if (plen == 1) {
		if (isc_safe_memequal(pk11_dh_bn768, prime,
				      sizeof(pk11_dh_bn768)))
			*r.base = 1;
		else if (isc_safe_memequal(pk11_dh_bn1024, prime,
					   sizeof(pk11_dh_bn1024)))
			*r.base = 2;
		else
			*r.base = 3;
	}
	else
		memmove(r.base, prime, plen);
	isc_region_consume(&r, plen);

	uint16_toregion(glen, &r);
	if (glen > 0)
		memmove(r.base, base, glen);
	isc_region_consume(&r, glen);

	uint16_toregion(publen, &r);
	memmove(r.base, pub, publen);
	isc_region_consume(&r, publen);

	isc_buffer_add(data, dnslen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11dh_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *dh;
	isc_region_t r;
	isc_uint16_t plen, glen, plen_, glen_, publen;
	CK_BYTE *prime = NULL, *base = NULL, *pub = NULL;
	CK_ATTRIBUTE *attr;
	int special = 0;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	dh = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dh));
	if (dh == NULL)
		return (ISC_R_NOMEMORY);
	memset(dh, 0, sizeof(*dh));

	/*
	 * Read the prime length.  1 & 2 are table entries, > 16 means a
	 * prime follows, otherwise an error.
	 */
	if (r.length < 2) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	plen = uint16_fromregion(&r);
	if (plen < 16 && plen != 1 && plen != 2) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	if (r.length < plen) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	plen_ = plen;
	if (plen == 1 || plen == 2) {
		if (plen == 1) {
			special = *r.base;
			isc_region_consume(&r, 1);
		} else {
			special = uint16_fromregion(&r);
		}
		switch (special) {
			case 1:
				prime = pk11_dh_bn768;
				plen_ = sizeof(pk11_dh_bn768);
				break;
			case 2:
				prime = pk11_dh_bn1024;
				plen_ = sizeof(pk11_dh_bn1024);
				break;
			case 3:
				prime = pk11_dh_bn1536;
				plen_ = sizeof(pk11_dh_bn1536);
				break;
			default:
				memset(dh, 0, sizeof(*dh));
				isc_mem_put(key->mctx, dh, sizeof(*dh));
				return (DST_R_INVALIDPUBLICKEY);
		}
	}
	else {
		prime = r.base;
		isc_region_consume(&r, plen);
	}

	/*
	 * Read the generator length.  This should be 0 if the prime was
	 * special, but it might not be.  If it's 0 and the prime is not
	 * special, we have a problem.
	 */
	if (r.length < 2) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	glen = uint16_fromregion(&r);
	if (r.length < glen) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	glen_ = glen;
	if (special != 0) {
		if (glen == 0) {
			base = pk11_dh_bn2;
			glen_ = sizeof(pk11_dh_bn2);
		}
		else {
			base = r.base;
			if (isc_safe_memequal(base, pk11_dh_bn2, glen)) {
				base = pk11_dh_bn2;
				glen_ = sizeof(pk11_dh_bn2);
			}
			else {
				memset(dh, 0, sizeof(*dh));
				isc_mem_put(key->mctx, dh, sizeof(*dh));
				return (DST_R_INVALIDPUBLICKEY);
			}
		}
	}
	else {
		if (glen == 0) {
			memset(dh, 0, sizeof(*dh));
			isc_mem_put(key->mctx, dh, sizeof(*dh));
			return (DST_R_INVALIDPUBLICKEY);
		}
		base = r.base;
	}
	isc_region_consume(&r, glen);

	if (r.length < 2) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	publen = uint16_fromregion(&r);
	if (r.length < publen) {
		memset(dh, 0, sizeof(*dh));
		isc_mem_put(key->mctx, dh, sizeof(*dh));
		return (DST_R_INVALIDPUBLICKEY);
	}
	pub = r.base;
	isc_region_consume(&r, publen);

	key->key_size = pk11_numbits(prime, plen_);

	dh->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 3);
	if (dh->repr == NULL)
		goto nomemory;
	memset(dh->repr, 0, sizeof(*attr) * 3);
	dh->attrcnt = 3;

	attr = dh->repr;
	attr[0].type = CKA_PRIME;
	attr[0].pValue = isc_mem_get(key->mctx, plen_);
	if (attr[0].pValue == NULL)
		goto nomemory;
	memmove(attr[0].pValue, prime, plen_);
	attr[0].ulValueLen = (CK_ULONG) plen_;

	attr[1].type = CKA_BASE;
	attr[1].pValue = isc_mem_get(key->mctx, glen_);
	if (attr[1].pValue == NULL)
		goto nomemory;
	memmove(attr[1].pValue, base, glen_);
	attr[1].ulValueLen = (CK_ULONG) glen_;

	attr[2].type = CKA_VALUE;
	attr[2].pValue = isc_mem_get(key->mctx, publen);
	if (attr[2].pValue == NULL)
		goto nomemory;
	memmove(attr[2].pValue, pub, publen);
	attr[2].ulValueLen = (CK_ULONG) publen;

	isc_buffer_forward(data, plen + glen + publen + 6);

	key->keydata.pkey = dh;

	return (ISC_R_SUCCESS);

    nomemory:
	for (attr = pk11_attribute_first(dh);
	     attr != NULL;
	     attr = pk11_attribute_next(dh, attr))
		switch (attr->type) {
		case CKA_VALUE:
		case CKA_PRIME:
		case CKA_BASE:
			if (attr->pValue != NULL) {
				memset(attr->pValue, 0, attr->ulValueLen);
				isc_mem_put(key->mctx,
					    attr->pValue,
					    attr->ulValueLen);
			}
			break;
		}
	if (dh->repr != NULL) {
		memset(dh->repr, 0, dh->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx, dh->repr, dh->attrcnt * sizeof(*attr));
	}
	memset(dh, 0, sizeof(*dh));
	isc_mem_put(key->mctx, dh, sizeof(*dh));
	return (ISC_R_NOMEMORY);
}

static isc_result_t
pkcs11dh_tofile(const dst_key_t *key, const char *directory) {
	int i;
	pk11_object_t *dh;
	CK_ATTRIBUTE *attr;
	CK_ATTRIBUTE *prime = NULL, *base = NULL, *pub = NULL, *prv = NULL;
	dst_private_t priv;
	unsigned char *bufs[4];
	isc_result_t result;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external)
		return (DST_R_EXTERNALKEY);

	dh = key->keydata.pkey;

	for (attr = pk11_attribute_first(dh);
	     attr != NULL;
	     attr = pk11_attribute_next(dh, attr))
		switch (attr->type) {
		case CKA_VALUE:
			pub = attr;
			break;
		case CKA_VALUE2:
			prv = attr;
			break;
		case CKA_PRIME:
			prime = attr;
			break;
		case CKA_BASE:
			base = attr;
			break;
		}
	if ((prime == NULL) || (base == NULL) ||
	    (pub == NULL) || (prv == NULL))
		return (DST_R_NULLKEY);

	memset(bufs, 0, sizeof(bufs));
	for (i = 0; i < 4; i++) {
		bufs[i] = isc_mem_get(key->mctx, prime->ulValueLen);
		if (bufs[i] == NULL) {
			result = ISC_R_NOMEMORY;
			goto fail;
		}
		memset(bufs[i], 0, prime->ulValueLen);
	}

	i = 0;

	priv.elements[i].tag = TAG_DH_PRIME;
	priv.elements[i].length = (unsigned short) prime->ulValueLen;
	memmove(bufs[i], prime->pValue, prime->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_DH_GENERATOR;
	priv.elements[i].length = (unsigned short) base->ulValueLen;
	memmove(bufs[i], base->pValue, base->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_DH_PRIVATE;
	priv.elements[i].length = (unsigned short) prv->ulValueLen;
	memmove(bufs[i], prv->pValue, prv->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_DH_PUBLIC;
	priv.elements[i].length = (unsigned short) pub->ulValueLen;
	memmove(bufs[i], pub->pValue, pub->ulValueLen);
	priv.elements[i].data = bufs[i];
	i++;

	priv.nelements = i;
	result = dst__privstruct_writefile(key, &priv, directory);
 fail:
	for (i = 0; i < 4; i++) {
		if (bufs[i] == NULL)
			break;
		memset(bufs[i], 0, prime->ulValueLen);
		isc_mem_put(key->mctx, bufs[i], prime->ulValueLen);
	}
	return (result);
}

static isc_result_t
pkcs11dh_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	pk11_object_t *dh = NULL;
	CK_ATTRIBUTE *attr;
	isc_mem_t *mctx;

	UNUSED(pub);
	mctx = key->mctx;

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_DH, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	if (key->external)
		DST_RET(DST_R_EXTERNALKEY);

	dh = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*dh));
	if (dh == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dh, 0, sizeof(*dh));
	key->keydata.pkey = dh;
	dh->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 4);
	if (dh->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(dh->repr, 0, sizeof(*attr) * 4);
	dh->attrcnt = 4;
	attr = dh->repr;
	attr[0].type = CKA_PRIME;
	attr[1].type = CKA_BASE;
	attr[2].type = CKA_VALUE;
	attr[3].type = CKA_VALUE2;

	for (i = 0; i < priv.nelements; i++) {
		CK_BYTE *bn;

		bn = isc_mem_get(key->mctx, priv.elements[i].length);
		if (bn == NULL)
			DST_RET(ISC_R_NOMEMORY);
		memmove(bn, priv.elements[i].data, priv.elements[i].length);

		switch (priv.elements[i].tag) {
			case TAG_DH_PRIME:
				attr = pk11_attribute_bytype(dh, CKA_PRIME);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DH_GENERATOR:
				attr = pk11_attribute_bytype(dh, CKA_BASE);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DH_PRIVATE:
				attr = pk11_attribute_bytype(dh, CKA_VALUE2);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
			case TAG_DH_PUBLIC:
				attr = pk11_attribute_bytype(dh, CKA_VALUE);
				INSIST(attr != NULL);
				attr->pValue = bn;
				attr->ulValueLen = priv.elements[i].length;
				break;
		}
	}
	dst__privstruct_free(&priv, mctx);

	attr = pk11_attribute_bytype(dh, CKA_PRIME);
	INSIST(attr != NULL);
	key->key_size = pk11_numbits(attr->pValue, attr->ulValueLen);

	return (ISC_R_SUCCESS);

 err:
	pkcs11dh_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t pkcs11dh_functions = {
	NULL, /*%< createctx */
	NULL, /*%< createctx2 */
	NULL, /*%< destroyctx */
	NULL, /*%< adddata */
	NULL, /*%< sign */
	NULL, /*%< verify */
	NULL, /*%< verify2 */
	pkcs11dh_computesecret,
	pkcs11dh_compare,
	pkcs11dh_paramcompare,
	pkcs11dh_generate,
	pkcs11dh_isprivate,
	pkcs11dh_destroy,
	pkcs11dh_todns,
	pkcs11dh_fromdns,
	pkcs11dh_tofile,
	pkcs11dh_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11dh_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &pkcs11dh_functions;
	return (ISC_R_SUCCESS);
}
#endif /* !PK11_DH_DISABLE */

#else /* PKCS11CRYPTO */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO */
/*! \file */
