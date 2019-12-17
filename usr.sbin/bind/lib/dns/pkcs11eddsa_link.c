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

#if defined(PKCS11CRYPTO) && \
    defined(HAVE_PKCS11_ED25519) || defined(HAVE_PKCS11_ED448)

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
#include <pkcs11/eddsa.h>

/*
 * FIPS 186-3 EDDSA keys:
 *  mechanisms:
 *    CKM_EDDSA,
 *    CKM_EDDSA_KEY_PAIR_GEN
 *  domain parameters:
 *    CKA_EC_PARAMS (choice with OID namedCurve)
 *  public keys:
 *    object class CKO_PUBLIC_KEY
 *    key type CKK_EDDSA
 *    attribute CKA_EC_PARAMS (choice with OID namedCurve)
 *    attribute CKA_EC_POINT (big int A, CKA_VALUE on the token)
 *  private keys:
 *    object class CKO_PRIVATE_KEY
 *    key type CKK_EDDSA
 *    attribute CKA_EC_PARAMS (choice with OID namedCurve)
 *    attribute CKA_VALUE (big int k)
 */

#define DST_RET(a) {ret = a; goto err;}

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

static isc_result_t pkcs11eddsa_todns(const dst_key_t *key,
				      isc_buffer_t *data);
static void pkcs11eddsa_destroy(dst_key_t *key);
static isc_result_t pkcs11eddsa_fetch(dst_key_t *key, const char *engine,
				      const char *label, dst_key_t *pub);

static isc_result_t
pkcs11eddsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_buffer_t *buf = NULL;
	isc_result_t result;

	UNUSED(key);
	REQUIRE(dctx->key->key_alg == DST_ALG_ED25519 ||
		dctx->key->key_alg == DST_ALG_ED448);

	result = isc_buffer_allocate(dctx->mctx, &buf, 16);
	isc_buffer_setautorealloc(buf, ISC_TRUE);
	dctx->ctxdata.generic = buf;

	return (result);
}

static void
pkcs11eddsa_destroyctx(dst_context_t *dctx) {
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;

	REQUIRE(dctx->key->key_alg == DST_ALG_ED25519 ||
		dctx->key->key_alg == DST_ALG_ED448);
	if (buf != NULL)
		isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;
}

static isc_result_t
pkcs11eddsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
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
pkcs11eddsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;
	CK_RV rv;
	CK_MECHANISM mech = { CKM_EDDSA, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_EDDSA;
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
	CK_ULONG siglen;
	CK_SLOT_ID slotid;
	pk11_context_t *pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *ec = key->keydata.pkey;
	isc_region_t t;
	isc_region_t r;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);
	REQUIRE(ec != NULL);

	if (key->key_alg == DST_ALG_ED25519)
		siglen = DNS_SIG_ED25519SIZE;
	else
		siglen = DNS_SIG_ED448SIZE;

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

	isc_buffer_usedregion(buf, &t);

	PK11_RET(pkcs_C_Sign,
		 (pk11_ctx->session,
		  (CK_BYTE_PTR) t.base, (CK_ULONG) t.length,
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
	isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;

	return (ret);
}

static isc_result_t
pkcs11eddsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_buffer_t *buf = (isc_buffer_t *) dctx->ctxdata.generic;
	CK_RV rv;
	CK_MECHANISM mech = { CKM_EDDSA, NULL, 0 };
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_EDDSA;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_EC_PARAMS, NULL, 0 },
		{ CKA_VALUE, NULL, 0 }
	};
	CK_ATTRIBUTE *attr;
	CK_SLOT_ID slotid;
	pk11_context_t *pk11_ctx;
	dst_key_t *key = dctx->key;
	pk11_object_t *ec = key->keydata.pkey;
	isc_region_t t;
	isc_result_t ret = ISC_R_SUCCESS;
	unsigned int i;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);
	REQUIRE(ec != NULL);

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
			/* keyTemplate[6].type is CKA_VALUE */
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

	isc_buffer_usedregion(buf, &t);

	PK11_RET(pkcs_C_Verify,
		 (pk11_ctx->session,
		  (CK_BYTE_PTR) t.base, (CK_ULONG) t.length,
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
	isc_buffer_free(&buf);
	dctx->ctxdata.generic = NULL;

	return (ret);
}

static isc_boolean_t
pkcs11eddsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
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
	if (key->key_alg == DST_ALG_ED25519) { \
		attr->pValue = isc_mem_get(key->mctx, \
					   sizeof(pk11_ecc_ed25519)); \
		if (attr->pValue == NULL) \
			DST_RET(ISC_R_NOMEMORY); \
		memmove(attr->pValue, \
			pk11_ecc_ed25519, sizeof(pk11_ecc_ed25519)); \
		attr->ulValueLen = sizeof(pk11_ecc_ed25519); \
	} else { \
		attr->pValue = isc_mem_get(key->mctx, \
					   sizeof(pk11_ecc_ed448)); \
		if (attr->pValue == NULL) \
			DST_RET(ISC_R_NOMEMORY); \
		memmove(attr->pValue, \
			pk11_ecc_ed448, sizeof(pk11_ecc_ed448)); \
		attr->ulValueLen = sizeof(pk11_ecc_ed448); \
	}

#define FREECURVE() \
	if (attr->pValue != NULL) { \
		memset(attr->pValue, 0, attr->ulValueLen); \
		isc_mem_put(key->mctx, attr->pValue, attr->ulValueLen); \
		attr->pValue = NULL; \
	}

static isc_result_t
pkcs11eddsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_EDDSA_KEY_PAIR_GEN, NULL, 0 };
	CK_OBJECT_HANDLE pub = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE  keyType = CKK_EDDSA;
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

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);
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
	attr[1].type = CKA_VALUE;
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
	attr->type = CKA_EC_POINT;

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

	if (key->key_alg == DST_ALG_ED25519)
		key->key_size = DNS_KEY_ED25519SIZE;
	else
		key->key_size = DNS_KEY_ED448SIZE;

	return (ISC_R_SUCCESS);

 err:
	pkcs11eddsa_destroy(key);
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
pkcs11eddsa_isprivate(const dst_key_t *key) {
	pk11_object_t *ec = key->keydata.pkey;
	CK_ATTRIBUTE *attr;

	if (ec == NULL)
		return (ISC_FALSE);
	attr = pk11_attribute_bytype(ec, CKA_VALUE);
	return (ISC_TF((attr != NULL) || ec->ontoken));
}

static void
pkcs11eddsa_destroy(dst_key_t *key) {
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
pkcs11eddsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *ec;
	isc_region_t r;
	unsigned int len;
	CK_ATTRIBUTE *attr;

	REQUIRE(key->keydata.pkey != NULL);

	if (key->key_alg == DST_ALG_ED25519)
		len = DNS_KEY_ED25519SIZE;
	else
		len = DNS_KEY_ED448SIZE;

	ec = key->keydata.pkey;
	attr = pk11_attribute_bytype(ec, CKA_EC_POINT);
	if ((attr == NULL) || (attr->ulValueLen != len))
		return (ISC_R_FAILURE);

	isc_buffer_availableregion(data, &r);
	if (r.length < len)
		return (ISC_R_NOSPACE);
	memmove(r.base, (CK_BYTE_PTR) attr->pValue, len);
	isc_buffer_add(data, len);

	return (ISC_R_SUCCESS);
}

static isc_result_t
pkcs11eddsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	pk11_object_t *ec;
	isc_region_t r;
	unsigned int len;
	CK_ATTRIBUTE *attr;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	if (key->key_alg == DST_ALG_ED25519)
		len = DNS_KEY_ED25519SIZE;
	else
		len = DNS_KEY_ED448SIZE;

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
	if (key->key_alg == DST_ALG_ED25519) {
		attr->pValue =
			isc_mem_get(key->mctx, sizeof(pk11_ecc_ed25519));
		if (attr->pValue == NULL)
			goto nomemory;
		memmove(attr->pValue,
			pk11_ecc_ed25519, sizeof(pk11_ecc_ed25519));
		attr->ulValueLen = sizeof(pk11_ecc_ed25519);
	} else {
		attr->pValue =
			isc_mem_get(key->mctx, sizeof(pk11_ecc_ed448));
		if (attr->pValue == NULL)
			goto nomemory;
		memmove(attr->pValue,
			pk11_ecc_ed448, sizeof(pk11_ecc_ed448));
		attr->ulValueLen = sizeof(pk11_ecc_ed448);
	}

	attr++;
	attr->type = CKA_EC_POINT;
	attr->pValue = isc_mem_get(key->mctx, len);
	if (attr->pValue == NULL)
		goto nomemory;
	memmove((CK_BYTE_PTR) attr->pValue, r.base, len);
	attr->ulValueLen = len;

	isc_buffer_forward(data, len);
	key->keydata.pkey = ec;
	key->key_size = len;
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
pkcs11eddsa_tofile(const dst_key_t *key, const char *directory) {
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
		priv.elements[i].tag = TAG_EDDSA_PRIVATEKEY;
		priv.elements[i].length = (unsigned short) attr->ulValueLen;
		memmove(buf, attr->pValue, attr->ulValueLen);
		priv.elements[i].data = buf;
		i++;
	}

	if (key->engine != NULL) {
		priv.elements[i].tag = TAG_EDDSA_ENGINE;
		priv.elements[i].length = strlen(key->engine) + 1;
		priv.elements[i].data = (unsigned char *)key->engine;
		i++;
	}

	if (key->label != NULL) {
		priv.elements[i].tag = TAG_EDDSA_LABEL;
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
pkcs11eddsa_fetch(dst_key_t *key, const char *engine, const char *label,
		  dst_key_t *pub)
{
	CK_RV rv;
	CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
	CK_KEY_TYPE keyType = CKK_EDDSA;
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
pkcs11eddsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	pk11_object_t *ec = NULL;
	CK_ATTRIBUTE *attr, *pattr;
	isc_mem_t *mctx = key->mctx;
	unsigned int i;
	const char *engine = NULL, *label = NULL;

	REQUIRE(key->key_alg == DST_ALG_ED25519 ||
		key->key_alg == DST_ALG_ED448);

	if ((pub == NULL) || (pub->keydata.pkey == NULL))
		DST_RET(DST_R_INVALIDPRIVATEKEY);

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_ED25519, lexer, mctx, &priv);
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
		case TAG_EDDSA_ENGINE:
			engine = (char *)priv.elements[i].data;
			break;
		case TAG_EDDSA_LABEL:
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
		ret = pkcs11eddsa_fetch(key, engine, label, pub);
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
	if (key->key_alg == DST_ALG_ED25519)
		key->key_size = DNS_KEY_ED25519SIZE;
	else
		key->key_size = DNS_KEY_ED448SIZE;

	return (ISC_R_SUCCESS);

 err:
	pkcs11eddsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static isc_result_t
pkcs11eddsa_fromlabel(dst_key_t *key, const char *engine, const char *label,
		      const char *pin)
{
	CK_RV rv;
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE keyType = CKK_EDDSA;
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
	attr[1].type = CKA_VALUE;

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
	attr[1].type = CKA_EC_POINT;

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
	if (key->key_alg == DST_ALG_ED25519)
		key->key_size = DNS_KEY_ED25519SIZE;
	else
		key->key_size = DNS_KEY_ED448SIZE;

	pk11_return_session(pk11_ctx);
	memset(pk11_ctx, 0, sizeof(*pk11_ctx));
	isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	return (ISC_R_SUCCESS);

 err:
	pkcs11eddsa_destroy(key);
	if (pk11_ctx != NULL) {
		pk11_return_session(pk11_ctx);
		memset(pk11_ctx, 0, sizeof(*pk11_ctx));
		isc_mem_put(key->mctx, pk11_ctx, sizeof(*pk11_ctx));
	}
	return (ret);
}

static dst_func_t pkcs11eddsa_functions = {
	pkcs11eddsa_createctx,
	NULL, /*%< createctx2 */
	pkcs11eddsa_destroyctx,
	pkcs11eddsa_adddata,
	pkcs11eddsa_sign,
	pkcs11eddsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	pkcs11eddsa_compare,
	NULL, /*%< paramcompare */
	pkcs11eddsa_generate,
	pkcs11eddsa_isprivate,
	pkcs11eddsa_destroy,
	pkcs11eddsa_todns,
	pkcs11eddsa_fromdns,
	pkcs11eddsa_tofile,
	pkcs11eddsa_parse,
	NULL, /*%< cleanup */
	pkcs11eddsa_fromlabel,
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__pkcs11eddsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &pkcs11eddsa_functions;
	return (ISC_R_SUCCESS);
}

#else /* PKCS11CRYPTO && HAVE_PKCS11_EDxxx */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* PKCS11CRYPTO && HAVE_PKCS11_EDxxx */
/*! \file */
