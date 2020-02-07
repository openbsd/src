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

/* $Id: hmacsha.c,v 1.1 2020/02/07 09:58:53 florian Exp $ */

/*
 * This code implements the HMAC-SHA1, HMAC-SHA224, HMAC-SHA256, HMAC-SHA384
 * and HMAC-SHA512 keyed hash algorithm described in RFC 2104 and
 * draft-ietf-dnsext-tsig-sha-01.txt.
 */



#include <isc/assertions.h>
#include <isc/hmacsha.h>

#include <isc/safe.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <string.h>
#include <isc/types.h>
#include <isc/util.h>

void
isc_hmacsha1_init(isc_hmacsha1_t *ctx, const unsigned char *key,
		  unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_sha1(), NULL) == 1);
}

void
isc_hmacsha1_invalidate(isc_hmacsha1_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacsha1_update(isc_hmacsha1_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacsha1_sign(isc_hmacsha1_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA1_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA1_DIGESTLENGTH);

	RUNTIME_CHECK(HMAC_Final(ctx->ctx, newdigest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	memmove(digest, newdigest, len);
	isc_safe_memwipe(newdigest, sizeof(newdigest));
}

void
isc_hmacsha224_init(isc_hmacsha224_t *ctx, const unsigned char *key,
		    unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_sha224(), NULL) == 1);
}

void
isc_hmacsha224_invalidate(isc_hmacsha224_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacsha224_update(isc_hmacsha224_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacsha224_sign(isc_hmacsha224_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA224_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA224_DIGESTLENGTH);

	RUNTIME_CHECK(HMAC_Final(ctx->ctx, newdigest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	memmove(digest, newdigest, len);
	isc_safe_memwipe(newdigest, sizeof(newdigest));
}

void
isc_hmacsha256_init(isc_hmacsha256_t *ctx, const unsigned char *key,
		    unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_sha256(), NULL) == 1);
}

void
isc_hmacsha256_invalidate(isc_hmacsha256_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacsha256_update(isc_hmacsha256_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacsha256_sign(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA256_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA256_DIGESTLENGTH);

	RUNTIME_CHECK(HMAC_Final(ctx->ctx, newdigest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	memmove(digest, newdigest, len);
	isc_safe_memwipe(newdigest, sizeof(newdigest));
}

void
isc_hmacsha384_init(isc_hmacsha384_t *ctx, const unsigned char *key,
		    unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_sha384(), NULL) == 1);
}

void
isc_hmacsha384_invalidate(isc_hmacsha384_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacsha384_update(isc_hmacsha384_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacsha384_sign(isc_hmacsha384_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA384_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA384_DIGESTLENGTH);

	RUNTIME_CHECK(HMAC_Final(ctx->ctx, newdigest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	memmove(digest, newdigest, len);
	isc_safe_memwipe(newdigest, sizeof(newdigest));
}

void
isc_hmacsha512_init(isc_hmacsha512_t *ctx, const unsigned char *key,
		    unsigned int len)
{
	ctx->ctx = HMAC_CTX_new();
	RUNTIME_CHECK(ctx->ctx != NULL);
	RUNTIME_CHECK(HMAC_Init_ex(ctx->ctx, (const void *) key,
				   (int) len, EVP_sha512(), NULL) == 1);
}

void
isc_hmacsha512_invalidate(isc_hmacsha512_t *ctx) {
	if (ctx->ctx == NULL)
		return;
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
}

void
isc_hmacsha512_update(isc_hmacsha512_t *ctx, const unsigned char *buf,
		   unsigned int len)
{
	RUNTIME_CHECK(HMAC_Update(ctx->ctx, buf, (int) len) == 1);
}

void
isc_hmacsha512_sign(isc_hmacsha512_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA512_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA512_DIGESTLENGTH);

	RUNTIME_CHECK(HMAC_Final(ctx->ctx, newdigest, NULL) == 1);
	HMAC_CTX_free(ctx->ctx);
	ctx->ctx = NULL;
	memmove(digest, newdigest, len);
	isc_safe_memwipe(newdigest, sizeof(newdigest));
}

/*
 * Verify signature - finalize SHA1 operation and reapply SHA1, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacsha1_verify(isc_hmacsha1_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA1_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA1_DIGESTLENGTH);
	isc_hmacsha1_sign(ctx, newdigest, ISC_SHA1_DIGESTLENGTH);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Verify signature - finalize SHA224 operation and reapply SHA224, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacsha224_verify(isc_hmacsha224_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA224_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA224_DIGESTLENGTH);
	isc_hmacsha224_sign(ctx, newdigest, ISC_SHA224_DIGESTLENGTH);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Verify signature - finalize SHA256 operation and reapply SHA256, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacsha256_verify(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA256_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA256_DIGESTLENGTH);
	isc_hmacsha256_sign(ctx, newdigest, ISC_SHA256_DIGESTLENGTH);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Verify signature - finalize SHA384 operation and reapply SHA384, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacsha384_verify(isc_hmacsha384_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA384_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA384_DIGESTLENGTH);
	isc_hmacsha384_sign(ctx, newdigest, ISC_SHA384_DIGESTLENGTH);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Verify signature - finalize SHA512 operation and reapply SHA512, then
 * compare to the supplied digest.
 */
isc_boolean_t
isc_hmacsha512_verify(isc_hmacsha512_t *ctx, unsigned char *digest, size_t len) {
	unsigned char newdigest[ISC_SHA512_DIGESTLENGTH];

	REQUIRE(len <= ISC_SHA512_DIGESTLENGTH);
	isc_hmacsha512_sign(ctx, newdigest, ISC_SHA512_DIGESTLENGTH);
	return (isc_safe_memequal(digest, newdigest, len));
}

/*
 * Check for SHA-1 support; if it does not work, raise a fatal error.
 *
 * Use the first test vector from RFC 2104, with a second round using
 * a too-short key.
 *
 * Standard use is testing 0 and expecting result true.
 * Testing use is testing 1..4 and expecting result false.
 */
isc_boolean_t
isc_hmacsha1_check(int testing) {
	isc_hmacsha1_t ctx;
	unsigned char key[] = { /* 20*0x0b */
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b
	};
	unsigned char input[] = { /* "Hi There" */
		0x48, 0x69, 0x20, 0x54, 0x68, 0x65, 0x72, 0x65
	};
	unsigned char expected[] = {
		0xb6, 0x17, 0x31, 0x86, 0x55, 0x05, 0x72, 0x64,
		0xe2, 0x8b, 0xc0, 0xb6, 0xfb, 0x37, 0x8c, 0x8e,
		0xf1, 0x46, 0xbe, 0x00
	};
	unsigned char expected2[] = {
		0xa0, 0x75, 0xe0, 0x5f, 0x7f, 0x17, 0x9d, 0x34,
		0xb2, 0xab, 0xc5, 0x19, 0x8f, 0x38, 0x62, 0x36,
		0x42, 0xbd, 0xec, 0xde
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
	isc_hmacsha1_init(&ctx, key, 20U);
	isc_hmacsha1_update(&ctx, input, 8U);
	result = isc_hmacsha1_verify(&ctx, expected, sizeof(expected));
	if (!result) {
		return (result);
	}

	/* Second round using a byte key */
	isc_hmacsha1_init(&ctx, key, 1U);
	isc_hmacsha1_update(&ctx, input, 8U);
	return (isc_hmacsha1_verify(&ctx, expected2, sizeof(expected2)));
}
