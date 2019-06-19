/*	$OpenBSD: m_sm3.c,v 1.1 2018/11/11 06:53:31 tb Exp $	*/
/*
 * Copyright (c) 2018, Ribose Inc
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

#ifndef OPENSSL_NO_SM3
#include <openssl/evp.h>
#include <openssl/sm3.h>

#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif

static int
sm3_init(EVP_MD_CTX *ctx)
{
	return SM3_Init(ctx->md_data);
}

static int
sm3_update(EVP_MD_CTX *ctx, const void *data, size_t count)
{
	return SM3_Update(ctx->md_data, data, count);
}

static int
sm3_final(EVP_MD_CTX *ctx, unsigned char *md)
{
	return SM3_Final(md, ctx->md_data);
}

static const EVP_MD sm3_md = {
	.type = NID_sm3,
	.pkey_type = NID_sm3WithRSAEncryption,
	.md_size = SM3_DIGEST_LENGTH,
	.flags = EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT,
	.init = sm3_init,
	.update = sm3_update,
	.final = sm3_final,
	.copy = NULL,
	.cleanup = NULL,
#ifndef OPENSSL_NO_RSA
	.sign = (evp_sign_method *)RSA_sign,
	.verify = (evp_verify_method *)RSA_verify,
	.required_pkey_type = {
		EVP_PKEY_RSA, EVP_PKEY_RSA2, 0, 0,
	},
#endif
	.block_size = SM3_CBLOCK,
	.ctx_size = sizeof(EVP_MD *) + sizeof(SM3_CTX),
};

const EVP_MD *
EVP_sm3(void)
{
	return &sm3_md;
}

#endif /* OPENSSL_NO_SM3 */
