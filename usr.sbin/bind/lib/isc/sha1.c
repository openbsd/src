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

/* $Id: sha1.c,v 1.12 2020/01/22 13:02:10 florian Exp $ */

/*	$NetBSD: sha1.c,v 1.5 2000/01/22 22:19:14 mycroft Exp $	*/
/*	$OpenBSD: sha1.c,v 1.12 2020/01/22 13:02:10 florian Exp $	*/

/*! \file
 * SHA-1 in C
 * \author By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 * \verbatim
 * Test Vectors (from FIPS PUB 180-1)
 * "abc"
 *   A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *   84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 * A million repetitions of "a"
 *   34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
 * \endverbatim
 */



#include <isc/assertions.h>

#include <isc/safe.h>
#include <isc/sha1.h>
#include <string.h>
#include <isc/types.h>
#include <isc/util.h>

void
isc_sha1_init(isc_sha1_t *context)
{
	INSIST(context != NULL);

	context->ctx = EVP_MD_CTX_new();
	RUNTIME_CHECK(context->ctx != NULL);
	if (EVP_DigestInit(context->ctx, EVP_sha1()) != 1) {
		FATAL_ERROR(__FILE__, __LINE__, "Cannot initialize SHA1.");
	}
}

void
isc_sha1_invalidate(isc_sha1_t *context) {
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}

void
isc_sha1_update(isc_sha1_t *context, const unsigned char *data,
		unsigned int len)
{
	INSIST(context != 0);
	INSIST(context->ctx != 0);
	INSIST(data != 0);

	RUNTIME_CHECK(EVP_DigestUpdate(context->ctx,
				       (const void *) data,
				       (size_t) len) == 1);
}

void
isc_sha1_final(isc_sha1_t *context, unsigned char *digest) {
	INSIST(digest != 0);
	INSIST(context != 0);
	INSIST(context->ctx != 0);

	RUNTIME_CHECK(EVP_DigestFinal(context->ctx, digest, NULL) == 1);
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}

/*
 * Check for SHA-1 support; if it does not work, raise a fatal error.
 *
 * Use "a" as the test vector.
 *
 * Standard use is testing false and result true.
 * Testing use is testing true and result false;
 */
isc_boolean_t
isc_sha1_check(isc_boolean_t testing) {
	isc_sha1_t ctx;
	unsigned char input = 'a';
	unsigned char digest[ISC_SHA1_DIGESTLENGTH];
	unsigned char expected[] = {
		0x86, 0xf7, 0xe4, 0x37, 0xfa, 0xa5, 0xa7, 0xfc,
		0xe1, 0x5d, 0x1d, 0xdc, 0xb9, 0xea, 0xea, 0xea,
		0x37, 0x76, 0x67, 0xb8
	};

	INSIST(sizeof(expected) == ISC_SHA1_DIGESTLENGTH);

	/*
	 * Introduce a fault for testing.
	 */
	if (testing) {
		input ^= 0x01;
	}

	/*
	 * These functions do not return anything; any failure will be fatal.
	 */
	isc_sha1_init(&ctx);
	isc_sha1_update(&ctx, &input, 1U);
	isc_sha1_final(&ctx, digest);

	/*
	 * Must return true in standard case, should return false for testing.
	 */
	return (ISC_TF(memcmp(digest, expected, ISC_SHA1_DIGESTLENGTH) == 0));
}
