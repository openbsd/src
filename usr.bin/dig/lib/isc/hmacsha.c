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

/* $Id: hmacsha.c,v 1.6 2020/02/25 18:10:17 florian Exp $ */

/*
 * This code implements the HMAC-SHA1, HMAC-SHA224, HMAC-SHA256, HMAC-SHA384
 * and HMAC-SHA512 keyed hash algorithm described in RFC 2104 and
 * draft-ietf-dnsext-tsig-sha-01.txt.
 */

#include <string.h>

#include <isc/hmacsha.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
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
	explicit_bzero(newdigest, sizeof(newdigest));
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
	explicit_bzero(newdigest, sizeof(newdigest));
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
	explicit_bzero(newdigest, sizeof(newdigest));
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
	explicit_bzero(newdigest, sizeof(newdigest));
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
	explicit_bzero(newdigest, sizeof(newdigest));
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
	return (ISC_TF(timingsafe_bcmp(digest, newdigest, len) == 0));
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
	return (ISC_TF(timingsafe_bcmp(digest, newdigest, len) == 0));
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
	return (ISC_TF(timingsafe_bcmp(digest, newdigest, len) == 0));
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
	return (ISC_TF(timingsafe_bcmp(digest, newdigest, len) == 0));
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
	return (ISC_TF(timingsafe_bcmp(digest, newdigest, len) == 0));
}
