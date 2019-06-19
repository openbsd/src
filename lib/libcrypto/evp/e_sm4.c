/*	$OpenBSD: e_sm4.c,v 1.1 2019/03/17 17:42:37 tb Exp $	*/
/*
 * Copyright (c) 2017, 2019 Ribose Inc
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#ifndef OPENSSL_NO_SM4
#include <openssl/evp.h>
#include <openssl/modes.h>
#include <openssl/sm4.h>

#include "evp_locl.h"

typedef struct {
	SM4_KEY ks;
} EVP_SM4_KEY;

static int
sm4_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
    const unsigned char *iv, int enc)
{
	SM4_set_key(key, ctx->cipher_data);
	return 1;
}

static void
sm4_cbc_encrypt(const unsigned char *in, unsigned char *out, size_t len,
    const SM4_KEY *key, unsigned char *ivec, const int enc)
{
	if (enc)
		CRYPTO_cbc128_encrypt(in, out, len, key, ivec,
		    (block128_f)SM4_encrypt);
	else
		CRYPTO_cbc128_decrypt(in, out, len, key, ivec,
		    (block128_f)SM4_decrypt);
}

static void
sm4_cfb128_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const SM4_KEY *key, unsigned char *ivec, int *num, const int enc)
{
	CRYPTO_cfb128_encrypt(in, out, length, key, ivec, num, enc,
	    (block128_f)SM4_encrypt);
}

static void
sm4_ecb_encrypt(const unsigned char *in, unsigned char *out, const SM4_KEY *key,
    const int enc)
{
	if (enc)
		SM4_encrypt(in, out, key);
	else
		SM4_decrypt(in, out, key);
}

static void
sm4_ofb128_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const SM4_KEY *key, unsigned char *ivec, int *num)
{
	CRYPTO_ofb128_encrypt(in, out, length, key, ivec, num,
	    (block128_f)SM4_encrypt);
}

IMPLEMENT_BLOCK_CIPHER(sm4, ks, sm4, EVP_SM4_KEY, NID_sm4, 16, 16, 16, 128,
    EVP_CIPH_FLAG_DEFAULT_ASN1, sm4_init_key, NULL, 0, 0, 0)

static int
sm4_ctr_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
    size_t len)
{
	EVP_SM4_KEY *key = EVP_C_DATA(EVP_SM4_KEY, ctx);

	CRYPTO_ctr128_encrypt(in, out, len, &key->ks, ctx->iv, ctx->buf,
	    &ctx->num, (block128_f)SM4_encrypt);
	return 1;
}

static const EVP_CIPHER sm4_ctr_mode = {
	.nid = NID_sm4_ctr,
	.block_size = 1,
	.key_len = 16,
	.iv_len = 16,
	.flags = EVP_CIPH_CTR_MODE,
	.init = sm4_init_key,
	.do_cipher = sm4_ctr_cipher,
	.cleanup = NULL,
	.ctx_size = sizeof(EVP_SM4_KEY),
	.set_asn1_parameters = NULL,
	.get_asn1_parameters = NULL,
	.ctrl = NULL,
	.app_data = NULL,
};

const EVP_CIPHER *
EVP_sm4_ctr(void)
{
	return &sm4_ctr_mode;
}

#endif
