/* $OpenBSD: aes_i386.c,v 1.1 2025/06/15 15:11:50 jsing Exp $ */
/*
 * Copyright (c) 2025 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <openssl/aes.h>

#include "crypto_arch.h"

int aes_set_encrypt_key_generic(const unsigned char *userKey, const int bits,
    AES_KEY *key);
int aes_set_decrypt_key_generic(const unsigned char *userKey, const int bits,
    AES_KEY *key);

void aes_encrypt_generic(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);
void aes_decrypt_generic(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);

void aes_cbc_encrypt_generic(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc);

int aesni_set_encrypt_key(const unsigned char *userKey, int bits,
    AES_KEY *key);
int aesni_set_decrypt_key(const unsigned char *userKey, int bits,
    AES_KEY *key);

void aesni_encrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);
void aesni_decrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key);

void aesni_cbc_encrypt(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc);

int
aes_set_encrypt_key_internal(const unsigned char *userKey, const int bits,
    AES_KEY *key)
{
	if ((crypto_cpu_caps_i386 & CRYPTO_CPU_CAPS_I386_AES) != 0)
		return aesni_set_encrypt_key(userKey, bits, key);

	return aes_set_encrypt_key_generic(userKey, bits, key);
}

int
aes_set_decrypt_key_internal(const unsigned char *userKey, const int bits,
    AES_KEY *key)
{
	if ((crypto_cpu_caps_i386 & CRYPTO_CPU_CAPS_I386_AES) != 0)
		return aesni_set_decrypt_key(userKey, bits, key);

	return aes_set_decrypt_key_generic(userKey, bits, key);
}

void
aes_encrypt_internal(const unsigned char *in, unsigned char *out,
    const AES_KEY *key)
{
	if ((crypto_cpu_caps_i386 & CRYPTO_CPU_CAPS_I386_AES) != 0) {
		aesni_encrypt(in, out, key);
		return;
	}

	aes_encrypt_generic(in, out, key);
}

void
aes_decrypt_internal(const unsigned char *in, unsigned char *out,
    const AES_KEY *key)
{
	if ((crypto_cpu_caps_i386 & CRYPTO_CPU_CAPS_I386_AES) != 0) {
		aesni_decrypt(in, out, key);
		return;
	}

	aes_decrypt_generic(in, out, key);
}

void
aes_cbc_encrypt_internal(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc)
{
	if ((crypto_cpu_caps_i386 & CRYPTO_CPU_CAPS_I386_AES) != 0) {
		aesni_cbc_encrypt(in, out, len, key, ivec, enc);
		return;
	}

	aes_cbc_encrypt_generic(in, out, len, key, ivec, enc);
}
