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

#if defined(PKCS11CRYPTO) && defined(HAVE_PKCS11_ECDSA)

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/safe.h>
#include <isc/sha2.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"
#include "dst_pkcs11.h"

#include <pk11/pk11.h>
#include <pk11/internal.h>
#define WANT_ECC_CURVES
#include <pk11/constants.h>

#include <pkcs11/pkcs11.h>

/*
 * FIPS 186-3 ECDSA keys:
 *  mechanisms:
 *    CKM_ECDSA,
 *    CKM_EC_KEY_PAIR_GEN
 *  domain parameters:
 *    CKA_EC_PARAMS (choice with OID namedCurve)
 *  public keys:
 *    object class CKO_PUBLIC_KEY
 *    key type CKK_EC
 *    attribute CKA_EC_PARAMS (choice with OID namedCurve)
 *    attribute CKA_EC_POINT (point Q)
 *  private keys:
 *    object class CKO_PRIVATE_KEY
 *    key type CKK_EC
 *    attribute CKA_EC_PARAMS (choice with OID namedCurve)
 *    attribute CKA_VALUE (big int d)
 *  point format: 0x04 (octet-string) <2*size+1> 0x4 (uncompressed) <x> <y>
 */

#define TAG_OCTECT_STRING	0x04
#define UNCOMPRESSED		0x04

#define DST_RET(a) {ret = a; goto err;}

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

static isc_result_t pkcs11ecdsa_todns(const dst_key_t *key,
				      isc_buffer_t *data);
static void pkcs11ecdsa_destroy(dst_key_t *key);
static isc_result_t pkcs11ecdsa_fetch(dst_key_t *key, const char *engine,
				      const char *label, dst_key_t *pub);

static isc_result_t
pkcs11ecdsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	CK_RV rv;
	CK_MECHANISM mech = {0, NULL, 0 };
	CK_SLOT_ID slotid;
	pk11_context_t *pk11_ctx;
	pk11_object_t *ec = key->keydata.pkey;
	isc_result_t ret;

	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);
	REQUIRE(ec != NULL);

	if (dctx->key->key_alg == DST_ALG_ECDSA256)
		mech.mechanism = CKM_SHA256;
	else
		mech.mechanism = CKM_SHA384;

	pk11_ctx = (pk11_context_t *) isc_mem_get(dctx->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	if (ec->ontoken && (dctx->use == DO_SIGN))
		slotid = ec->slot;
	else
		slotid = pk11_get_best_token(OP_EC);
	ret = pk11_get_session(pk11_ctx, OP_EC, ISC_TRUE, ISC_FALSE,
			       ec->reqlogon, NULL, slotid);
	if (ret != ISC_R_SUCCESS)
		goto err;

	PK11_RET(pkcs_C_DigestInit, (pk11_ctx->session, &mech), ISC_R_FAILURE);
	dctx->ctxdata.pk11_ctx = pk11_ctx;
	return (ISC_R_SUCCESS);

 err:
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));

	return (ret);
}

static void
pkcs11ecdsa_destroyctx(dst_context_t *dctx) {
	CK_BYTE garbage[ISC_SHA384_DIGESTLENGTH];
	CK_ULONG len = ISC_SHA384_DIGESTLENGTH;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;

	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);

	if (pk11_ctx != NULL) {
		(void) pkcs_C_DigestFinal(pk11_ctx->session, garbage, &len);
		memset(garbage, 0, sizeof(garbage));
		pk11_return_session(pk11_ctx);
		memset(pk11_ctx, 0, sizeof(*pk11_ctx));
		isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
		dctx->ctxdata.pk11_ctx = NULL;
	}
}

static isc_result_t
pkcs11ecdsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	CK_RV rv;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	isc_result_t ret = ISC_R_SUCCESS;

	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);

	PK11_CALL(pkcs_C_DigestUpdate,
		  (pk11_ctx->session,
		   (CK_BYTE_PTR) data->base,
		   (CK_ULONG) data->length),
		  ISC_R_FAILURE);

	return (ret);
}

static isc_result_t
pkcs11ecdsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_ECDSA, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_EC;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_EC_PARAMS, NULL, 0 },
		{ CKA_VALUE, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	CK_BYTE digest[ISC_SHA384_DIGESTLENGTH];
	CK_ULONG dgstlen;
	CK_ULONG siglen;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *ec = key->keydata.pkey;
	isc_region_t r;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);
	REQUIRE(ec != NULL);

	if (key->key_alg == DST_ALG_ECDSA256) {
		dgstlen = ISC_SHA256_DIGESTLENGTH;
		siglen = DNS_SIG_ECDSA256SIZE;
	} else {
		siglen = DNS_SIG_ECDSA384SIZE;
		dgstlen = ISC_SHA384_DIGESTLENGTH;
	}

	PK11_RET(pkcs_C_DigestFinal,
		 (pk11_ctx->session, digest, &dgstlen),
		 ISC_R_FAILURE);

	isc_buffer_availableregion(sig, &r);
	if (r.length < siglen)
		DST_RET(ISC_R_NOSPACE);

	if (ec->ontoken && (ec->object != CK_INVALID_HANDLE)) {
		pk11_ctx->ontoken = ec->ontoken;
		pk11_ctx->object = ec->object;
		goto token_key;
	}

	for (attr = pk11_attribute_first(ec);
	     attr != NULL;
	     attr = pk11_attribute_next(ec, attr))
		switch (attr->type) {
		case CKA_EC_PARAMS:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		case CKA_VALUE:
			INSIST(keyTemplate[6].type == attr->type);
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
		  keyTemplate, (CK_ULONG) 7,
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
		  (CK_BYTE_PTR) r.base, &siglen),
		 DST_R_SIGNFAILURE);

	isc_buffer_add(sig, (unsigned int) siglen);

 err:

	if (hKey != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(pk11_ctx->session, hKey);
	for (i = 5; i <= 6; i++)
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
	dctx->ctxdata.pk11_ctx = NULL;

	return (ret);
}

static isc_result_t
pkcs11ecdsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_ECDSA, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_EC;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_EC_PARAMS, NULL, 0 },
		{ CKA_EC_POINT, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	CK_BYTE digest[ISC_SHA384_DIGESTLENGTH];
	CK_ULONG dgstlen;
	pk11_context_t *pk11_ctx = dctx->ctxdata.pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *ec = key->keydata.pkey;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);
	REQUIRE(ec != NULL);

	if (key->key_alg == DST_ALG_ECDSA256)
		dgstlen = ISC_SHA256_DIGESTLENGTH;
	else
		dgstlen = ISC_SHA384_DIGESTLENGTH;

	PK11_RET(pkcs_C_DigestFinal,
		 (pk11_ctx->session, digest, &dgstlen),
		 ISC_R_FAILURE);

	for (attr = pk11_attribute_first(ec);
	     attr != NULL;
	     attr = pk11_attribute_next(ec, attr))
		switch (attr->type) {
		case CKA_EC_PARAMS:
			INSIST(keyTemplate[5].type == attr->type);
			keyTemplate[5].pValue = isc_mem_get(dctx->mctx,
							    attr->ulValueLen);
			if (keyTemplate[5].pValue == NULL)
				DST_RET(ISC_R_NOMEMORY);
			memmove(keyTemplate[5].pValue, attr->pValue,
				attr->ulValueLen);
			keyTemplate[5].ulValueLen = attr->ulValueLen;
			break;
		case CKA_EC_POINT:
			INSIST(keyTemplate[6].type == attr->type);
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
			memset(keyTemplate[i].pValue, 0,
			       keyTemplate[i].ulValueLen);
			isc_mem_put(dctx->mctx,
				    keyTemplate[i].pValue,
				    keyTemplate[i].ulValueLen);
		}
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(dctx->mctx, pk11_ctx, sizeof(*pk11_ctx));
	dctx->ctxdata.pk11_ctx = NULL;

	return (ret);
}

static isc_boolean_t
pkcs11ecdsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	pk11_object_t *ec1, *ec2;
	CK_ATTRIBUTE *attr1, *attr2;

	ec1 = key1->keydata.pkey;
	ec2 = key2->keydata.pkey;

	if ((ec1 == NULL) && (ec2 == NULL))
		return (ISC_TRUE);
	else if ((ec1 == NULL) || (ec2 == NULL))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(ec1, CKA_EC_PARAMS);
	attr2 = pk11_attribute_bytype(ec2, CKA_EC_PARAMS);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(ec1, CKA_EC_POINT);
	attr2 = pk11_attribute_bytype(ec2, CKA_EC_POINT);
	if ((attr1 == NULL) && (attr2 == NULL))
		return (ISC_TRUE);
	else if ((attr1 == NULL) || (attr2 == NULL) ||
		 (attr1->ulValueLen != attr2->ulValueLen) ||
		 !isc_safe_memequal(attr1->pValue, attr2->pValue,
				    attr1->ulValueLen))
		return (ISC_FALSE);

	attr1 = pk11_attribute_bytype(ec1, CKA_VALUE);
	attr2 = pk11_attribute_bytype(ec2, CKA_VALUE);
	if (((attr1 != NULL) || (attr2 != NULL)) &&
	    ((attr1 == NULL) || (attr2 == NULL) ||
	     (attr1->ulValueLen != attr2->ulValueLen) ||
	     !isc_safe_memequal(attr1->pValue, attr2->pValue,
				attr1->ulValueLen)))
		return (ISC_FALSE);

	if (!ec1->ontoken && !ec2->ontoken)
		return (ISC_TRUE);
	else if (ec1->ontoken || ec2->ontoken ||
		 (ec1->object != ec2->object))
		return (ISC_FALSE);

	return (ISC_TRUE);
}

#define SETCURVE() \
	if (key->key_alg == DST_ALG_ECDSA256) { \
		attr->pValue = isc_mem_get(key->mctx, \
					   sizeof(pk11_ecc_prime256v1)); \
		if (attr->pValue == NULL) \
			DST_RET(ISC_R_NOMEMORY); \
		memmove(attr->pValue, \
			pk11_ecc_prime256v1, sizeof(pk11_ecc_prime256v1)); \
		attr->ulValueLen = sizeof(pk11_ecc_prime256v1); \
	} else { \
		attr->pValue = isc_mem_get(key->mctx, \
					   sizeof(pk11_ecc_secp384r1)); \
		if (attr->pValue == NULL) \
			DST_RET(ISC_R_NOMEMORY); \
		memmove(attr->pValue, \
			pk11_ecc_secp384r1, sizeof(pk11_ecc_secp384r1)); \
		attr->ulValueLen = sizeof(pk11_ecc_secp384r1); \
	}

#define FREECURVE() \
	if (attr->pValue != NULL) { \
		memset(attr->pValue, 0, attr->ulValueLen); \
		isc_mem_put(key->mctx, attr->pValue, attr->ulValueLen); \
		attr->pValue = NULL; \
	}

static isc_result_t
pkcs11ecdsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_EC_KEY_PAIR_GEN, NULL, 0 };
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE  keyType = CKK_EC;
	CK_ATTRIBUTE pubTemplate[] =
	{
		{ CKA_CLASS, &pubClass, (CK_ULONG) sizeof(pubClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_EC_PARAMS, NULL, 0 }
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
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) }
	};
	CK_ATTRIBUTE *attr;
	pk11_object_t *ec;
	pk11_context_t *pk11_ctx;
	isc_result_t ret;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);
	UNUSED(unused);
	UNUSED(callback);

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		return (ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_EC, ISC_TRUE, ISC_FALSE,
			       ISC_FALSE, NULL, pk11_get_best_token(OP_EC));
	if (ret != ISC_R_SUCCESS)
		goto err;

	ec = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*ec));
	if (ec == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(ec, 0, sizeof(*ec));
	key->keydata.pkey = ec;
	ec->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 3);
	if (ec->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(ec->repr, 0, sizeof(*attr) * 3);
	ec->attrcnt = 3;

	attr = ec->repr;
	attr[0].type = CKA_EC_PARAMS;
	attr[1].type = CKA_EC_POINT;
	attr[2].type = CKA_VALUE;

	attr = &pubTemplate[5];
	SETCURVE();

	PK11_RET(pkcs_C_GenerateKeyPair,
		 (pk11_ctx->session, &mech,
		  pubTemplate, (CK_ULONG) 6,
		  privTemplate, (CK_ULONG) 7,
		  &pub, &priv),
		 DST_R_CRYPTOFAILURE);

	attr = &pubTemplate[5];
	FREECURVE();

	attr = ec->repr;
	SETCURVE();

	attr++;
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

	(void) pkcs_C_DestroyObject(pk11_ctx->session, priv);
	(void) pkcs_C_DestroyObject(pk11_ctx->session, pub);
	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));

	if (key->key_alg == DST_ALG_ECDSA256)
		key->key_size = DNS_KEY_ECDSA256SIZE * 4;
	else
		key->key_size = DNS_KEY_ECDSA384SIZE * 4;

	return (ISC_R_SUCCESS);

 err:
	pkcs11ecdsa_destroy(key);
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
pkcs11ecdsa_isprivate(const dst_key_t *key) {
	pk11_object_t *ec = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (ec == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(ec, CKA_VALUE);
	return (ISC_TF((attr != NULL) || ec->ontoken));
}

static void
pkcs11ecdsa_destroy(dst_key_t *key) {
	pk11_object_t *ec = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (ec == NULL)
		return;

	INSIST((ec->object == CK_INVALID_HANDLE) || ec->ontoken);

	for (attr = pk11_attribute_first(ec);
	     attr != NULL;
	     attr = pk11_attribute_next(ec, attr))
		switch (attr->type) {
		case CKA_LABEL:
		case CKA_ID:
		case CKA_EC_PARAMS:
		case CKA_EC_POINT:
		case CKA_VALUE:
			FREECURVE();
			break;
		}
	if (ec->repr != NULL) {
		memset(ec->repr, 0, ec->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    ec->repr,
			    ec->attrcnt * sizeof(*attr));
	}
	memset(ec, 0, sizeof(*ec));
	isc_mem_put(key->mctx, ec, sizeof(*ec));
	key->keydata.pkey = NULL;
}

static isc_result_t
pkcs11ecdsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *ec;
	isc_region_t r;
	unsigned int len;
	CK_ATTRIBUTE *attr;

	REQUIRE(key->keydata.pkey != NULL);

	if (key->key_alg == DST_ALG_ECDSA256)
		len = DNS_KEY_ECDSA256SIZE;
	else
		len = DNS_KEY_ECDSA384SIZE;

	ec = key->keydata.pkey;
	attr = pk11_attribute_bytype(ec, CKA_EC_POINT);
	if ((attr == NULL) ||
	    (attr->ulValueLen != len + 3) ||
	    (((CK_BYTE_PTR) attr->pValue)[0] != TAG_OCTECT_STRING) ||
	    (((CK_BYTE_PTR) attr->pValue)[1] != len + 1) ||
	    (((CK_BYTE_PTR) attr->pValue)[2] != UNCOMPRESSED))
		return (ISC_R_FAILURE);

	isc_buffer_availableregion(data, &r);
	if (r.length < len)
		return (ISC_R_NOSPACE);
	memmove(r.base, (CK_BYTE_PTR) attr->pValue + 3, len);
	isc_buffer_add(data, len);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11ecdsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *ec;
	isc_region_t r;
	unsigned int len;
	CK_ATTRIBUTE *attr;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

	if (key->key_alg == DST_ALG_ECDSA256)
		len = DNS_KEY_ECDSA256SIZE;
	else
		len = DNS_KEY_ECDSA384SIZE;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	if (r.length != len)
		return (DST_R_INVALIDPUBLICKEY);

	ec = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*ec));
	if (ec == NULL)
		return (ISC_R_NOMEMORY);
	memset(ec, 0, sizeof(*ec));
	ec->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (ec->repr == NULL)
		goto nomemory;
	ec->attrcnt = 2;

	attr = ec->repr;
	attr->type = CKA_EC_PARAMS;
	if (key->key_alg == DST_ALG_ECDSA256) {
		attr->pValue =
			isc_mem_get(key->mctx, sizeof(pk11_ecc_prime256v1));
		if (attr->pValue == NULL)
			goto nomemory;
		memmove(attr->pValue,
			pk11_ecc_prime256v1, sizeof(pk11_ecc_prime256v1));
		attr->ulValueLen = sizeof(pk11_ecc_prime256v1);
	} else {
		attr->pValue =
			isc_mem_get(key->mctx, sizeof(pk11_ecc_secp384r1));
		if (attr->pValue == NULL)
			goto nomemory;
		memmove(attr->pValue,
			pk11_ecc_secp384r1, sizeof(pk11_ecc_secp384r1));
		attr->ulValueLen = sizeof(pk11_ecc_secp384r1);
	}

	attr++;
	attr->type = CKA_EC_POINT;
	attr->pValue = isc_mem_get(key->mctx, len + 3);
	if (attr->pValue == NULL)
		goto nomemory;
	((CK_BYTE_PTR) attr->pValue)[0] = TAG_OCTECT_STRING;
	((CK_BYTE_PTR) attr->pValue)[1] = len + 1;
	((CK_BYTE_PTR) attr->pValue)[2] = UNCOMPRESSED;
	memmove((CK_BYTE_PTR) attr->pValue + 3, r.base, len);
	attr->ulValueLen = len + 3;

	isc_buffer_forward(data, len);
	key->keydata.pkey = ec;
	key->key_size = len * 4;
	return (ISC_R_SUCCESS);

 nomemory:
	for (attr = pk11_attribute_first(ec);
	     attr != NULL;
	     attr = pk11_attribute_next(ec, attr))
		switch (attr->type) {
		case CKA_EC_PARAMS:
		case CKA_EC_POINT:
			FREECURVE();
			break;
		}
	if (ec->repr != NULL) {
		memset(ec->repr, 0, ec->attrcnt * sizeof(*attr));
		isc_mem_put(key->mctx,
			    ec->repr,
			    ec->attrcnt * sizeof(*attr));
	}
	memset(ec, 0, sizeof(*ec));
	isc_mem_put(key->mctx, ec, sizeof(*ec));
	return (ISC_R_NOMEMORY);
}

static isc_result_t
pkcs11ecdsa_tofile(const dst_key_t *key, const char *directory) {
	isc_result_t ret;
	pk11_object_t *ec;
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

	ec = key->keydata.pkey;
	attr = pk11_attribute_bytype(ec, CKA_VALUE);
	if (attr != NULL) {
		buf = isc_mem_get(key->mctx, attr->ulValueLen);
		if (buf == NULL)
			return (ISC_R_NOMEMORY);
		priv.elements[i].tag = TAG_ECDSA_PRIVATEKEY;
		priv.elements[i].length = (unsigned short) attr->ulValueLen;
		memmove(buf, attr->pValue, attr->ulValueLen);
		priv.elements[i].data = buf;
		i++;
	}

	if (key->engine != NULL) {
		priv.elements[i].tag = TAG_ECDSA_ENGINE;
		priv.elements[i].length = strlen(key->engine) + 1;
		priv.elements[i].data = (unsigned char *)key->engine;
		i++;
	}

	if (key->label != NULL) {
		priv.elements[i].tag = TAG_ECDSA_LABEL;
		priv.elements[i].length = strlen(key->label) + 1;
		priv.elements[i].data = (unsigned char *)key->label;
		i++;
	}

	priv.nelements = i;
	ret = dst__privstruct_writefile(key, &priv, directory);

	if (buf != NULL) {
		memset(buf, 0, attr->ulValueLen);
		isc_mem_put(key->mctx, buf, attr->ulValueLen);
	}
	return (ret);
}

static isc_result_t
pkcs11ecdsa_fetch(dst_key_t *key, const char *engine, const char *label,
		  dst_key_t *pub)
{
	CK_RV rv;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_EC;
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
	pk11_object_t *ec;
	pk11_object_t *pubec;
	pk11_context_t *pk11_ctx = NULL;
	isc_result_t ret;

	if (label == NULL)
		return (DST_R_NOENGINE);

	ec = key->keydata.pkey;
	pubec = pub->keydata.pkey;

	ec->object = CK_INVALID_HANDLE;
	ec->ontoken = ISC_TRUE;
	ec->reqlogon = ISC_TRUE;
	ec->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (ec->repr == NULL)
		return (ISC_R_NOMEMORY);
	memset(ec->repr, 0, sizeof(*attr) * 2);
	ec->attrcnt = 2;
	attr = ec->repr;

	attr->type = CKA_EC_PARAMS;
	pubattr = pk11_attribute_bytype(pubec, CKA_EC_PARAMS);
	attr->pValue = isc_mem_get(key->mctx, pubattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pubattr->pValue, pubattr->ulValueLen);
	attr->ulValueLen = pubattr->ulValueLen;
	attr++;

	attr->type = CKA_EC_POINT;
	pubattr = pk11_attribute_bytype(pubec, CKA_EC_POINT);
	attr->pValue = isc_mem_get(key->mctx, pubattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pubattr->pValue, pubattr->ulValueLen);
	attr->ulValueLen = pubattr->ulValueLen;

	ret = pk11_parse_uri(ec, label, key->mctx, OP_EC);
	if (ret != ISC_R_SUCCESS)
		goto err;

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		DST_RET(ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_EC, ISC_TRUE, ISC_FALSE,
			       ec->reqlogon, NULL, ec->slot);
	if (ret != ISC_R_SUCCESS)
		goto err;

	attr = pk11_attribute_bytype(ec, CKA_LABEL);
	if (attr == NULL) {
		attr = pk11_attribute_bytype(ec, CKA_ID);
		INSIST(attr != NULL);
		searchTemplate[3].type = CKA_ID;
	}
	searchTemplate[3].pValue = attr->pValue;
	searchTemplate[3].ulValueLen = attr->ulValueLen;

	PK11_RET(pkcs_C_FindObjectsInit,
		 (pk11_ctx->session, searchTemplate, (CK_ULONG) 4),
		 DST_R_CRYPTOFAILURE);
	PK11_RET(pkcs_C_FindObjects,
		 (pk11_ctx->session, &ec->object, (CK_ULONG) 1, &cnt),
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
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	return (ISC_R_SUCCESS);

 err:
	if (pk11_ctx != NULL) {
		pk11_return_session(pk11_ctx);
		memset(pk11_ctx, 0, sizeof(*pk11_ctx));
		isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	}
	return (ret);
}

static isc_result_t
pkcs11ecdsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	pk11_object_t *ec = NULL;
	CK_ATTRIBUTE *attr, *pattr;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;
	const char *engine = NULL, *label = NULL;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

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

	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_ECDSA_ENGINE:
			engine = (char *)priv.elements[i].data;
			break;
		case TAG_ECDSA_LABEL:
			label = (char *)priv.elements[i].data;
			break;
		default:
			break;
		}
	}
	ec = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*ec));
	if (ec == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(ec, 0, sizeof(*ec));
	key->keydata.pkey = ec;

	/* Is this key is stored in a HSM? See if we can fetch it. */
	if ((label != NULL) || (engine != NULL)) {
		ret = pkcs11ecdsa_fetch(key, engine, label, pub);
		if (ret != ISC_R_SUCCESS)
			goto err;
		dst__privstruct_free(&priv, mctx);
		memset(&priv, 0, sizeof(priv));
		return (ret);
	}

	ec->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 3);
	if (ec->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(ec->repr, 0, sizeof(*attr) * 3);
	ec->attrcnt = 3;

	attr = ec->repr;
	attr->type = CKA_EC_PARAMS;
	pattr = pk11_attribute_bytype(pub->keydata.pkey, CKA_EC_PARAMS);
	INSIST(pattr != NULL);
	attr->pValue = isc_mem_get(key->mctx, pattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pattr->pValue, pattr->ulValueLen);
	attr->ulValueLen = pattr->ulValueLen;

	attr++;
	attr->type = CKA_EC_POINT;
	pattr = pk11_attribute_bytype(pub->keydata.pkey, CKA_EC_POINT);
	INSIST(pattr != NULL);
	attr->pValue = isc_mem_get(key->mctx, pattr->ulValueLen);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, pattr->pValue, pattr->ulValueLen);
	attr->ulValueLen = pattr->ulValueLen;

	attr++;
	attr->type = CKA_VALUE;
	attr->pValue = isc_mem_get(key->mctx, priv.elements[0].length);
	if (attr->pValue == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memmove(attr->pValue, priv.elements[0].data, priv.elements[0].length);
	attr->ulValueLen = priv.elements[0].length;

	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	if (key->key_alg == DST_ALG_ECDSA256)
		key->key_size = DNS_KEY_ECDSA256SIZE * 4;
	else
		key->key_size = DNS_KEY_ECDSA384SIZE * 4;

	return (ISC_R_SUCCESS);

 err:
	pkcs11ecdsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static isc_result_t
pkcs11ecdsa_fromlabel(dst_key_t *key, const char *engine, const char *label,
		      const char *pin)
{
	CK_RV rv;
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_EC;
	CK_ATTRIBUTE searchTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_LABEL, NULL, 0 }
	};
	CK_ULONG cnt;
	CK_ATTRIBUTE *attr;
	pk11_object_t *ec;
	pk11_context_t *pk11_ctx = NULL;
	isc_result_t ret;
	unsigned int i;

	UNUSED(pin);

	ec = (pk11_object_t *) isc_mem_get(key->mctx, sizeof(*ec));
	if (ec == NULL)
		return (ISC_R_NOMEMORY);
	memset(ec, 0, sizeof(*ec));
	ec->object = CK_INVALID_HANDLE;
	ec->ontoken = ISC_TRUE;
	ec->reqlogon = ISC_TRUE;
	key->keydata.pkey = ec;

	ec->repr = (CK_ATTRIBUTE *) isc_mem_get(key->mctx, sizeof(*attr) * 2);
	if (ec->repr == NULL)
		DST_RET(ISC_R_NOMEMORY);
	memset(ec->repr, 0, sizeof(*attr) * 2);
	ec->attrcnt = 2;
	attr = ec->repr;
	attr[0].type = CKA_EC_PARAMS;
	attr[1].type = CKA_EC_POINT;

	ret = pk11_parse_uri(ec, label, key->mctx, OP_EC);
	if (ret != ISC_R_SUCCESS)
		goto err;

	pk11_ctx = (pk11_context_t *) isc_mem_get(key->mctx,
						  sizeof(*pk11_ctx));
	if (pk11_ctx == NULL)
		DST_RET(ISC_R_NOMEMORY);
	ret = pk11_get_session(pk11_ctx, OP_EC, ISC_TRUE, ISC_FALSE,
			       ec->reqlogon, NULL, ec->slot);
	if (ret != ISC_R_SUCCESS)
		goto err;

	attr = pk11_attribute_bytype(ec, CKA_LABEL);
	if (attr == NULL) {
		attr = pk11_attribute_bytype(ec, CKA_ID);
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

	attr = ec->repr;
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
		 (pk11_ctx->session, &ec->object, (CK_ULONG) 1, &cnt),
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
	if (key->key_alg == DST_ALG_ECDSA256)
		key->key_size = DNS_KEY_ECDSA256SIZE * 4;
	else
		key->key_size = DNS_KEY_ECDSA384SIZE * 4;

	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	return (ISC_R_SUCCESS);

 err:
	pkcs11ecdsa_destroy(key);
	if (pk11_ctx != NULL) {
		pk11_return_session(pk11_ctx);
		memset(pk11_ctx, 0, sizeof(*pk11_ctx));
		isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	}
	return (ret);
}

static dst_func_t pkcs11ecdsa_functions = {
	pkcs11ecdsa_createctx,
	NULL, /*%< createctx2 */
	pkcs11ecdsa_destroyctx,
	pkcs11ecdsa_adddata,
	pkcs11ecdsa_sign,
	pkcs11ecdsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	pkcs11ecdsa_compare,
	NULL, /*%< paramcompare */
	pkcs11ecdsa_generate,
	pkcs11ecdsa_isprivate,
	pkcs11ecdsa_destroy,
	pkcs11ecdsa_todns,
	pkcs11ecdsa_fromdns,
	pkcs11ecdsa_tofile,
	pkcs11ecdsa_parse,
	NULL, /*%< cleanup */
	pkcs11ecdsa_fromlabel,
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11ecdsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &pkcs11ecdsa_functions;
	return (ISC_R_SUCCESS);
}

#else /* PKCS11CRYPTO && HAVE_PKCS11_ECDSA */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO && HAVE_PKCS11_ECDSA */
/*! \file */
