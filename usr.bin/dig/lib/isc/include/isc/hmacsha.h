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

/* $Id: hmacsha.h,v 1.6 2020/09/14 08:40:44 florian Exp $ */

/*! \file isc/hmacsha.h
 * This is the header file for the HMAC-SHA1, HMAC-SHA224, HMAC-SHA256,
 * HMAC-SHA334 and HMAC-SHA512 hash algorithm described in RFC 2104.
 */

#ifndef ISC_HMACSHA_H
#define ISC_HMACSHA_H 1

#include <isc/sha1.h>
#include <isc/sha2.h>

#define ISC_HMACSHA1_KEYLENGTH ISC_SHA1_BLOCK_LENGTH
#define ISC_HMACSHA224_KEYLENGTH ISC_SHA224_BLOCK_LENGTH
#define ISC_HMACSHA256_KEYLENGTH ISC_SHA256_BLOCK_LENGTH
#define ISC_HMACSHA384_KEYLENGTH ISC_SHA384_BLOCK_LENGTH
#define ISC_HMACSHA512_KEYLENGTH ISC_SHA512_BLOCK_LENGTH

#include <openssl/hmac.h>

typedef struct {
	HMAC_CTX *ctx;
} isc_hmacsha_t;

typedef isc_hmacsha_t isc_hmacsha1_t;
typedef isc_hmacsha_t isc_hmacsha224_t;
typedef isc_hmacsha_t isc_hmacsha256_t;
typedef isc_hmacsha_t isc_hmacsha384_t;
typedef isc_hmacsha_t isc_hmacsha512_t;

void
isc_hmacsha1_init(isc_hmacsha1_t *ctx, const unsigned char *key,
		  unsigned int len);

void
isc_hmacsha1_invalidate(isc_hmacsha1_t *ctx);

void
isc_hmacsha1_update(isc_hmacsha1_t *ctx, const unsigned char *buf,
		    unsigned int len);

void
isc_hmacsha1_sign(isc_hmacsha1_t *ctx, unsigned char *digest, size_t len);

int
isc_hmacsha1_verify(isc_hmacsha1_t *ctx, unsigned char *digest, size_t len);

void
isc_hmacsha224_init(isc_hmacsha224_t *ctx, const unsigned char *key,
		    unsigned int len);

void
isc_hmacsha224_invalidate(isc_hmacsha224_t *ctx);

void
isc_hmacsha224_update(isc_hmacsha224_t *ctx, const unsigned char *buf,
		      unsigned int len);

void
isc_hmacsha224_sign(isc_hmacsha224_t *ctx, unsigned char *digest, size_t len);

int
isc_hmacsha224_verify(isc_hmacsha224_t *ctx, unsigned char *digest, size_t len);

void
isc_hmacsha256_init(isc_hmacsha256_t *ctx, const unsigned char *key,
		    unsigned int len);

void
isc_hmacsha256_invalidate(isc_hmacsha256_t *ctx);

void
isc_hmacsha256_update(isc_hmacsha256_t *ctx, const unsigned char *buf,
		      unsigned int len);

void
isc_hmacsha256_sign(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len);

int
isc_hmacsha256_verify(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len);

void
isc_hmacsha384_init(isc_hmacsha384_t *ctx, const unsigned char *key,
		    unsigned int len);

void
isc_hmacsha384_invalidate(isc_hmacsha384_t *ctx);

void
isc_hmacsha384_update(isc_hmacsha384_t *ctx, const unsigned char *buf,
		      unsigned int len);

void
isc_hmacsha384_sign(isc_hmacsha384_t *ctx, unsigned char *digest, size_t len);

int
isc_hmacsha384_verify(isc_hmacsha384_t *ctx, unsigned char *digest, size_t len);

void
isc_hmacsha512_init(isc_hmacsha512_t *ctx, const unsigned char *key,
		    unsigned int len);

void
isc_hmacsha512_invalidate(isc_hmacsha512_t *ctx);

void
isc_hmacsha512_update(isc_hmacsha512_t *ctx, const unsigned char *buf,
		      unsigned int len);

void
isc_hmacsha512_sign(isc_hmacsha512_t *ctx, unsigned char *digest, size_t len);

int
isc_hmacsha512_verify(isc_hmacsha512_t *ctx, unsigned char *digest, size_t len);

#endif /* ISC_HMACSHA_H */
