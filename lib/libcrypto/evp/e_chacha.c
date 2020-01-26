/* $OpenBSD: e_chacha.c,v 1.6 2020/01/26 02:39:58 tb Exp $ */
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_CHACHA

#include <openssl/chacha.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

#include "evp_locl.h"

static int chacha_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
    const unsigned char *in, size_t len);
static int chacha_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
    const unsigned char *iv, int enc);

static const EVP_CIPHER chacha20_cipher = {
	.nid = NID_chacha20,
	.block_size = 1,
	.key_len = 32,
	.iv_len = 16, /* OpenSSL has 8 byte counter followed by 8 byte iv */
	.flags = EVP_CIPH_STREAM_CIPHER | EVP_CIPH_ALWAYS_CALL_INIT |
	    EVP_CIPH_CUSTOM_IV,
	.init = chacha_init,
	.do_cipher = chacha_cipher,
	.ctx_size = sizeof(ChaCha_ctx)
};

const EVP_CIPHER *
EVP_chacha20(void)
{
	return (&chacha20_cipher);
}

static int
chacha_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
    const unsigned char *iv, int enc)
{
	if (key != NULL)
		ChaCha_set_key((ChaCha_ctx *)ctx->cipher_data, key,
		    EVP_CIPHER_CTX_key_length(ctx) * 8);
	if (iv != NULL) {
		const unsigned char *openssl_iv = iv + 8;
		const unsigned char *counter = iv;

		ChaCha_set_iv((ChaCha_ctx *)ctx->cipher_data, openssl_iv,
		    counter);
	}
	return 1;
}

static int
chacha_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
    size_t len)
{
	ChaCha((ChaCha_ctx *)ctx->cipher_data, out, in, len);
	return 1;
}

#endif
