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

/* $Id: sha2.c,v 1.4 2020/02/24 13:49:38 jsg Exp $ */

/*	$FreeBSD: src/sys/crypto/sha2/sha2.c,v 1.2.2.2 2002/03/05 08:36:47 ume Exp $	*/
/*	$KAME: sha2.c,v 1.8 2001/11/08 01:07:52 itojun Exp $	*/

/*
 * sha2.c
 *
 * Version 1.0.0beta1
 *
 * Written by Aaron D. Gifford <me@aarongifford.com>
 *
 * Copyright 2000 Aaron D. Gifford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <isc/sha2.h>
#include <string.h>
#include <isc/util.h>

void
isc_sha224_init(isc_sha224_t *context) {
	if (context == (isc_sha224_t *)0) {
		return;
	}
	context->ctx = EVP_MD_CTX_new();
	RUNTIME_CHECK(context->ctx != NULL);
	if (EVP_DigestInit(context->ctx, EVP_sha224()) != 1) {
		FATAL_ERROR(__FILE__, __LINE__, "Cannot initialize SHA224.");
	}
}

void
isc_sha224_update(isc_sha224_t *context, const uint8_t* data, size_t len) {
	if (len == 0U) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	REQUIRE(context != (isc_sha224_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);
	REQUIRE(data != (uint8_t*)0);

	RUNTIME_CHECK(EVP_DigestUpdate(context->ctx,
				       (const void *) data, len) == 1);
}

void
isc_sha224_final(uint8_t digest[], isc_sha224_t *context) {
	/* Sanity check: */
	REQUIRE(context != (isc_sha224_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0)
		RUNTIME_CHECK(EVP_DigestFinal(context->ctx,
					      digest, NULL) == 1);
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}

void
isc_sha256_init(isc_sha256_t *context) {
	if (context == (isc_sha256_t *)0) {
		return;
	}
	context->ctx = EVP_MD_CTX_new();
	RUNTIME_CHECK(context->ctx != NULL);
	if (EVP_DigestInit(context->ctx, EVP_sha256()) != 1) {
		FATAL_ERROR(__FILE__, __LINE__, "Cannot initialize SHA256.");
	}
}

void
isc_sha256_update(isc_sha256_t *context, const uint8_t *data, size_t len) {
	if (len == 0U) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	REQUIRE(context != (isc_sha256_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);
	REQUIRE(data != (uint8_t*)0);

	RUNTIME_CHECK(EVP_DigestUpdate(context->ctx,
				       (const void *) data, len) == 1);
}

void
isc_sha256_final(uint8_t digest[], isc_sha256_t *context) {
	/* Sanity check: */
	REQUIRE(context != (isc_sha256_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0)
		RUNTIME_CHECK(EVP_DigestFinal(context->ctx,
					      digest, NULL) == 1);
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}

void
isc_sha512_init(isc_sha512_t *context) {
	if (context == (isc_sha512_t *)0) {
		return;
	}
	context->ctx = EVP_MD_CTX_new();
	RUNTIME_CHECK(context->ctx != NULL);
	if (EVP_DigestInit(context->ctx, EVP_sha512()) != 1) {
		FATAL_ERROR(__FILE__, __LINE__, "Cannot initialize SHA512.");
	}
}

void isc_sha512_update(isc_sha512_t *context, const uint8_t *data, size_t len) {
	if (len == 0U) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	REQUIRE(context != (isc_sha512_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);
	REQUIRE(data != (uint8_t*)0);

	RUNTIME_CHECK(EVP_DigestUpdate(context->ctx,
				       (const void *) data, len) == 1);
}

void isc_sha512_final(uint8_t digest[], isc_sha512_t *context) {
	/* Sanity check: */
	REQUIRE(context != (isc_sha512_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0)
		RUNTIME_CHECK(EVP_DigestFinal(context->ctx,
					      digest, NULL) == 1);
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}

void
isc_sha384_init(isc_sha384_t *context) {
	if (context == (isc_sha384_t *)0) {
		return;
	}
	context->ctx = EVP_MD_CTX_new();
	RUNTIME_CHECK(context->ctx != NULL);
	if (EVP_DigestInit(context->ctx, EVP_sha384()) != 1) {
		FATAL_ERROR(__FILE__, __LINE__, "Cannot initialize SHA384.");
	}
}

void
isc_sha384_update(isc_sha384_t *context, const uint8_t* data, size_t len) {
	if (len == 0U) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	REQUIRE(context != (isc_sha512_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);
	REQUIRE(data != (uint8_t*)0);

	RUNTIME_CHECK(EVP_DigestUpdate(context->ctx,
				       (const void *) data, len) == 1);
}

void
isc_sha384_final(uint8_t digest[], isc_sha384_t *context) {
	/* Sanity check: */
	REQUIRE(context != (isc_sha384_t *)0);
	REQUIRE(context->ctx != (EVP_MD_CTX *)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0)
		RUNTIME_CHECK(EVP_DigestFinal(context->ctx,
					      digest, NULL) == 1);
	EVP_MD_CTX_free(context->ctx);
	context->ctx = NULL;
}
