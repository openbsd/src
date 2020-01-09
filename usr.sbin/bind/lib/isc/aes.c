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

/* $Id: aes.c,v 1.4 2020/01/09 14:18:30 florian Exp $ */

/*! \file isc/aes.c */

#include "config.h"

#include <isc/assertions.h>
#include <isc/aes.h>
#include <isc/platform.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/util.h>

#if HAVE_OPENSSL_EVP_AES

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#define EVP_CIPHER_CTX_new() &(_context), EVP_CIPHER_CTX_init(&_context)
#define EVP_CIPHER_CTX_free(c) RUNTIME_CHECK(EVP_CIPHER_CTX_cleanup(c) == 1)
#endif

void
isc_aes128_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	EVP_CIPHER_CTX _context;
#endif
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_128_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

void
isc_aes192_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	EVP_CIPHER_CTX _context;
#endif
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_192_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

void
isc_aes256_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	EVP_CIPHER_CTX _context;
#endif
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_256_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

#elif HAVE_OPENSSL_AES

#include <openssl/aes.h>

void
isc_aes128_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	AES_KEY k;

	RUNTIME_CHECK(AES_set_encrypt_key(key, 128, &k) == 0);
	AES_encrypt(in, out, &k);
}

void
isc_aes192_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	AES_KEY k;

	RUNTIME_CHECK(AES_set_encrypt_key(key, 192, &k) == 0);
	AES_encrypt(in, out, &k);
}

void
isc_aes256_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	AES_KEY k;

	RUNTIME_CHECK(AES_set_encrypt_key(key, 256, &k) == 0);
	AES_encrypt(in, out, &k);
}

#endif
