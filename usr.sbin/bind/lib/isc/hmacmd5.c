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

/* $Id: hmacmd5.c,v 1.3 2019/12/17 01:46:34 sthen Exp $ */

/*! \file
 * This code implements the HMAC-MD5 keyed hash algorithm
 * described in RFC2104.
 */

#include "config.h"

#include <pk11/site.h>

#ifndef PK11_MD5_DISABLE

#include <isc/assertions.h>
#include <isc/hmacmd5.h>
#include <isc/md5.h>
#include <isc/platform.h>
#include <isc/safe.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/util.h>

#if PKCS11CRYPTO
#include <pk11/internal.h>
#include <pk11/pk11.h>
#endif

#ifdef ISC_PLATFORM_OPENSSLHASH
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#define HMAC_CTX_new() &(ctx->_ctx), HMAC_CTX_init(&(ctx->_ctx))
#define HMAC_CTX_free(ptr) HMAC_CTX_cleanup(ptr)
#endif

void
isc_hmacmd5_init(isc_hmacmd5_t *ctx, const unsigned char *key,
		 unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_md5(), NULL) == 1);
}

void
isc_hmacmd5_invalidate(isc_hmacmd5_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacmd5_update(isc_hmacmd5_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacmd5_sign(isc_hmacmd5_t *ctx, unsigned char *digest) {
	RUNTIME_CHECK(HMAC_Final(ctx->ctx, digest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

#elif PKCS11CRYPTO

#ifndef PK11_MD5_HMAC_REPLACE

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

void
isc_hmacmd5_init(isc_hmacmd5_t *ctx, const unsigned char *key,
		 unsigned int len)
{
	CK_RV rv;
	CK_MECHANISM mech = { CKM_MD5_HMAC, NULL, 0 };
	CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
	CK_KEY_TYPE keyType = CKK_MD5_HMAC;
	CK_ATTRIBUTE keyTemplate[] =
	{
		{ CKA_CLASS, &keyClass, (CK_ULONG) sizeof(keyClass) },
		{ CKA_KEY_TYPE, &keyType, (CK_ULONG) sizeof(keyType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_SIGN, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_VALUE, NULL, (CK_ULONG) len }
	};
#ifdef PK11_PAD_HMAC_KEYS
	CK_BYTE keypad[ISC_MD5_DIGESTLENGTH];

	if (len < ISC_MD5_DIGESTLENGTH) {
		memset(keypad, 0, ISC_MD5_DIGESTLENGTH);
		memmove(keypad, key, len);
		keyTemplate[5].pValue = keypad;
		keyTemplate[5].ulValueLen = ISC_MD5_DIGESTLENGTH;
	} else
		DE_CONST(key, keyTemplate[5].pValue);
#else
	DE_CONST(key, keyTemplate[5].pValue);
#endif
	RUNTIME_CHECK(pk11_get_session(ctx, OP_DIGEST, ISC_TRUE, ISC_FALSE,
				       ISC_FALSE, NULL, 0) == ISC_R_SUCCESS);
	ctx->object = CK_INVALID_HANDLE;
	PK11_FATALCHECK(pkcs_C_CreateObject,
			(ctx->session, keyTemplate,
			 (CK_ULONG) 6, &ctx->object));
	INSIST(ctx->object != CK_INVALID_HANDLE);
	PK11_FATALCHECK(pkcs_C_SignInit, (ctx->session, &mech, ctx->object));
}

void
isc_hmacmd5_invalidate(isc_hmacmd5_t *ctx) {
	CK_BYTE garbage[ISC_MD5_DIGESTLENGTH];
	CK_ULONG len = ISC_MD5_DIGESTLENGTH;

	if (ctx->handle == NULL)
		return;
	(void) pkcs_C_SignFinal(ctx->session, garbage, &len);
	isc_safe_memwipe(garbage, sizeof(garbage));
	if (ctx->object != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(ctx->session, ctx->object);
	ctx->object = CK_INVALID_HANDLE;
	pk11_return_session(ctx);
}

void
isc_hmacmd5_update(isc_hmacmd5_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	CK_RV rv;
	CK_BYTE_PTR pPart;

	DE_CONST(buf, pPart);
	PK11_FATALCHECK(pkcs_C_SignUpdate,
			(ctx->session, pPart, (CK_ULONG) len));
}

void
isc_hmacmd5_sign(isc_hmacmd5_t *ctx, unsigned char *digest) {
	CK_RV rv;
	CK_ULONG len = ISC_MD5_DIGESTLENGTH;

	PK11_FATALCHECK(pkcs_C_SignFinal,
			(ctx->session, (CK_BYTE_PTR) digest, &len));
	if (ctx->object != CK_INVALID_HANDLE)
		(void) pkcs_C_DestroyObject(ctx->session, ctx->object);
	ctx->object = CK_INVALID_HANDLE;
	pk11_return_session(ctx);
}
#else
/* Replace missing CKM_MD5_HMAC PKCS#11 mechanism */

#define PADLEN 64
#define IPAD 0x36
#define OPAD 0x5C

void
isc_hmacmd5_init(isc_hmacmd5_t *ctx, const unsigned char *key,
		 unsigned int len)
{
	CK_RV rv;
	CK_MECHANISM mech = { CKM_MD5, NULL, 0 };
	unsigned char ipad[PADLEN];
	unsigned int i;

	RUNTIME_CHECK(pk11_get_session(ctx, OP_DIGEST, ISC_TRUE, ISC_FALSE,
				       ISC_FALSE, NULL, 0) == ISC_R_SUCCESS);
	RUNTIME_CHECK((ctx->key = pk11_mem_get(PADLEN)) != NULL);
	if (len > PADLEN) {
		CK_BYTE_PTR kPart;
		CK_ULONG kl;

		PK11_FATALCHECK(pkcs_C_DigestInit, (ctx->session, &mech));
		DE_CONST(key, kPart);
		PK11_FATALCHECK(pkcs_C_DigestUpdate,
				(ctx->session, kPart, (CK_ULONG) len));
		kl = ISC_MD5_DIGESTLENGTH;
		PK11_FATALCHECK(pkcs_C_DigestFinal,
				(ctx->session, (CK_BYTE_PTR) ctx->key, &kl));
	} else
		memmove(ctx->key, key, len);
	PK11_FATALCHECK(pkcs_C_DigestInit, (ctx->session, &mech));
	memset(ipad, IPAD, PADLEN);
	for (i = 0; i < PADLEN; i++)
		ipad[i] ^= ctx->key[i];
	PK11_FATALCHECK(pkcs_C_DigestUpdate,
			(ctx->session, ipad, (CK_ULONG) PADLEN));
}

void
isc_hmacmd5_invalidate(isc_hmacmd5_t *ctx) {
	if (ctx->key != NULL)
		pk11_mem_put(ctx->key, PADLEN);
	ctx->key = NULL;
	isc_md5_invalidate(ctx);
}

void
isc_hmacmd5_update(isc_hmacmd5_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	CK_RV rv;
	CK_BYTE_PTR pPart;

	DE_CONST(buf, pPart);
	PK11_FATALCHECK(pkcs_C_DigestUpdate,
			(ctx->session, pPart, (CK_ULONG) len));
}

void
isc_hmacmd5_sign(isc_hmacmd5_t *ctx, unsigned char *digest) {
	CK_RV rv;
	CK_MECHANISM mech = { CKM_MD5, NULL, 0 };
	CK_ULONG len = ISC_MD5_DIGESTLENGTH;
	CK_BYTE opad[PADLEN];
	unsigned int i;

	PK11_FATALCHECK(pkcs_C_DigestFinal,
			(ctx->session, (CK_BYTE_PTR) digest,
			 (CK_ULONG_PTR) &len));
	memset(opad, OPAD, PADLEN);
	for (i = 0; i < PADLEN; i++)
		opad[i] ^= ctx->key[i];
	pk11_mem_put(ctx->key, PADLEN);
	ctx->key = NULL;
	PK11_FATALCHECK(pkcs_C_DigestInit, (ctx->session, &mech));
	PK11_FATALCHECK(pkcs_C_DigestUpdate,
			(ctx->session, opad, (CK_ULONG) PADLEN));
	PK11_FATALCHECK(pkcs_C_DigestUpdate,
			(ctx->session, (CK_BYTE_PTR) digest, len));
	PK11_FATALCHECK(pkcs_C_DigestFinal,
			(ctx->session,
			 (CK_BYTE_PTR) digest,
			 (CK_ULONG_PTR) &len));
	pk11_return_session(ctx);
}
#endif

#else

#define PADLEN 64
#define IPAD 0x36
#define OPAD 0x5C

/*!
 * Start HMAC-MD5 process.  Initialize an md5 context and digest the key.
 */
void
isc_hmacmd5_init(isc_hmacmd5_t *ctx, const unsigned char *key,
		 unsigned int len)
{
	unsigned char ipad[PADLEN];
	int i;

	memset(ctx->key, 0, sizeof(ctx->key));
	if (len > sizeof(ctx->key)) {
		isc_md5_t md5ctx;
		isc_md5_init(&md5ctx);
		isc_md5_update(&md5ctx, key, len);
		isc_md5_final(&md5ctx, ctx->key);
	} else
		memmove(ctx->key, key, len);

	isc_md5_init(&ctx->md5ctx);
	memset(ipad, IPAD, sizeof(ipad));
	for (i = 0; i < PADLEN; i++)
		ipad[i] ^= ctx->key[i];
	isc_md5_update(&ctx->md5ctx, ipad, sizeof(ipad));
}

void
isc_hmacmd5_invalidate(isc_hmacmd5_t *ctx) {
	isc_md5_invalidate(&ctx->md5ctx);
	isc_safe_memwipe(ctx->key, sizeof(ctx->key));
}

/*!
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void
isc_hmacmd5_update(isc_hmacmd5_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	isc_md5_update(&ctx->md5ctx, buf, len);
}

/*!
 * Compute signature - finalize MD5 operation and reapply MD5.
 */
void
isc_hmacmd5_sign(isc_hmacmd5_t *ctx, unsigned char *digest) {
	unsigned char opad[PADLEN];
	int i;

	isc_md5_final(&ctx->md5ctx, digest);

	memset(opad, OPAD, sizeof(opad));
	for (i = 0; i < PADLEN; i++)
		opad[i] ^= ctx->key[i];

	isc_md5_init(&ctx->md5ctx);
	isc_md5_update(&ctx->md5ctx, opad, sizeof(opad));
	isc_md5_update(&ctx->md5ctx, digest, ISC_MD5_DIGESTLENGTH);
	isc_md5_final(&ctx->md5ctx, digest);
	isc_hmacmd5_invalidate(ctx);
}

#endif /* !ISC_PLATFORM_OPENSSLHASH */

/*!
 * Verify signature - finalize MD5 operation and reapply MD5, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacmd5_verify(isc_hmacmd5_t *ctx, unsigned char *digest) {
	return (isc_hmacmd5_verify2(ctx, digest, ISC_MD5_DIGESTLENGTH));
}

isc_boolean_t
isc_hmacmd5_verify2(isc_hmacmd5_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_MD5_DIGESTLENGTH];

	REQUIRE(len <= ISC_MD5_DIGESTLENGTH);
	isc_hmacmd5_sign(ctx, newdigest);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Check for MD5 support; if it does not work, raise a fatal error.
 *
 * Use the first test vector from RFC 2104, with a second round using
 * a too-short key.
 *
 * Standard use is testing 0 and expecting result true.
 * Testing use is testing 1..4 and expecting result false.
 */
isc_boolean_t
isc_hmacmd5_check(int testing) {
	isc_hmacmd5_t ctx;
	unsigned char key[] = { /* 0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b */
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
	};
	unsigned char input[] = { /* "Hi There" */
		0x48, 0x69, 0x20, 0x54, 0x68, 0x65, 0x72, 0x65
	};
	unsigned char expected[] = {
		0x92, 0x94, 0x72, 0x7a, 0x36, 0x38, 0xbb, 0x1c,
		0x13, 0xf4, 0x8e, 0xf8, 0x15, 0x8b, 0xfc, 0x9d
	};
	unsigned char expected2[] = {
		0xad, 0xb8, 0x48, 0x05, 0xb8, 0x8d, 0x03, 0xe5,
		0x90, 0x1e, 0x4b, 0x05, 0x69, 0xce, 0x35, 0xea
	};
	isc_boolean_t result;

	/*
	 * Introduce a fault for testing.
	 */
	switch (testing) {
	case 0:
	default:
		break;
	case 1:
		key[0] ^= 0x01;
		break;
	case 2:
		input[0] ^= 0x01;
		break;
	case 3:
		expected[0] ^= 0x01;
		break;
	case 4:
		expected2[0] ^= 0x01;
		break;
	}

	/*
	 * These functions do not return anything; any failure will be fatal.
	 */
	isc_hmacmd5_init(&ctx, key, 16U);
	isc_hmacmd5_update(&ctx, input, 8U);
	result = isc_hmacmd5_verify2(&ctx, expected, sizeof(expected));
	if (!result) {
		return (result);
	}

	/* Second round using a byte key */
	isc_hmacmd5_init(&ctx, key, 1U);
	isc_hmacmd5_update(&ctx, input, 8U);
	return (isc_hmacmd5_verify2(&ctx, expected2, sizeof(expected2)));
}

#else /* !PK11_MD5_DISABLE */
#ifdef WIN32
/* Make the Visual Studio linker happy */
#include <isc/util.h>

void isc_hmacmd5_init() { INSIST(0); }
void isc_hmacmd5_invalidate() { INSIST(0); }
void isc_hmacmd5_sign() { INSIST(0); }
void isc_hmacmd5_update() { INSIST(0); }
void isc_hmacmd5_verify() { INSIST(0); }
void isc_hmacmd5_verify2() { INSIST(0); }
void isc_hmacmd5_check() { INSIST(0); }
#endif
#endif /* PK11_MD5_DISABLE */
