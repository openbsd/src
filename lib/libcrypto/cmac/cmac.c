/* $OpenBSD: cmac.c,v 1.16 2023/11/29 21:35:57 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2010 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/cmac.h>

#include "evp_local.h"

/*
 * This implementation follows https://doi.org/10.6028/NIST.SP.800-38B
 */

/*
 * CMAC context. k1 and k2 are the secret subkeys, computed as in section 6.1.
 * The temporary block tbl is a scratch buffer that holds intermediate secrets.
 */
struct CMAC_CTX_st {
	EVP_CIPHER_CTX cctx;
	unsigned char k1[EVP_MAX_BLOCK_LENGTH];
	unsigned char k2[EVP_MAX_BLOCK_LENGTH];
	unsigned char tbl[EVP_MAX_BLOCK_LENGTH];
	unsigned char last_block[EVP_MAX_BLOCK_LENGTH];
	/* Bytes in last block. -1 means not initialized. */
	int nlast_block;
};

/*
 * SP 800-38B, section 6.1, steps 2 and 3: given the input key l, calculate
 * the subkeys k1 and k2: shift l one bit to the left. If the most significant
 * bit of l was 1, additionally xor the result with Rb to get kn.
 *
 * Step 2: calculate k1 with l being the intermediate block CIPH_K(0),
 * Step 3: calculate k2 from l == k1.
 *
 * Per 5.3, Rb is the lexically first irreducible polynomial of degree b with
 * the minimum number of non-zero terms. This gives R128 = (1 << 128) | 0x87
 * and R64 = (1 << 64) | 0x1b for the only supported block sizes 128 and 64.
 */
static void
make_kn(unsigned char *kn, const unsigned char *l, int bl)
{
	unsigned char mask, Rb;
	int i;

	/* Choose Rb according to the block size in bytes. */
	Rb = bl == 16 ? 0x87 : 0x1b;

	/* Compute l << 1 up to last byte. */
	for (i = 0; i < bl - 1; i++)
		kn[i] = (l[i] << 1) | (l[i + 1] >> 7);

	/* Only xor with Rb if the MSB is one. */
	mask = 0 - (l[0] >> 7);
	kn[bl - 1] = (l[bl - 1] << 1) ^ (Rb & mask);
}

CMAC_CTX *
CMAC_CTX_new(void)
{
	CMAC_CTX *ctx;

	ctx = malloc(sizeof(CMAC_CTX));
	if (!ctx)
		return NULL;
	EVP_CIPHER_CTX_init(&ctx->cctx);
	ctx->nlast_block = -1;
	return ctx;
}
LCRYPTO_ALIAS(CMAC_CTX_new);

void
CMAC_CTX_cleanup(CMAC_CTX *ctx)
{
	EVP_CIPHER_CTX_cleanup(&ctx->cctx);
	explicit_bzero(ctx->tbl, EVP_MAX_BLOCK_LENGTH);
	explicit_bzero(ctx->k1, EVP_MAX_BLOCK_LENGTH);
	explicit_bzero(ctx->k2, EVP_MAX_BLOCK_LENGTH);
	explicit_bzero(ctx->last_block, EVP_MAX_BLOCK_LENGTH);
	ctx->nlast_block = -1;
}
LCRYPTO_ALIAS(CMAC_CTX_cleanup);

EVP_CIPHER_CTX *
CMAC_CTX_get0_cipher_ctx(CMAC_CTX *ctx)
{
	return &ctx->cctx;
}
LCRYPTO_ALIAS(CMAC_CTX_get0_cipher_ctx);

void
CMAC_CTX_free(CMAC_CTX *ctx)
{
	if (ctx == NULL)
		return;

	CMAC_CTX_cleanup(ctx);
	free(ctx);
}
LCRYPTO_ALIAS(CMAC_CTX_free);

int
CMAC_CTX_copy(CMAC_CTX *out, const CMAC_CTX *in)
{
	int bl;

	if (in->nlast_block == -1)
		return 0;
	if (!EVP_CIPHER_CTX_copy(&out->cctx, &in->cctx))
		return 0;
	bl = EVP_CIPHER_CTX_block_size(&in->cctx);
	memcpy(out->k1, in->k1, bl);
	memcpy(out->k2, in->k2, bl);
	memcpy(out->tbl, in->tbl, bl);
	memcpy(out->last_block, in->last_block, bl);
	out->nlast_block = in->nlast_block;
	return 1;
}
LCRYPTO_ALIAS(CMAC_CTX_copy);

int
CMAC_Init(CMAC_CTX *ctx, const void *key, size_t keylen,
    const EVP_CIPHER *cipher, ENGINE *impl)
{
	static unsigned char zero_iv[EVP_MAX_BLOCK_LENGTH];
	int bl;

	/* All zeros means restart */
	if (key == NULL && cipher == NULL && keylen == 0) {
		/* Not initialised */
		if (ctx->nlast_block == -1)
			return 0;
		if (!EVP_EncryptInit_ex(&ctx->cctx, NULL, NULL, NULL, zero_iv))
			return 0;
		explicit_bzero(ctx->tbl, sizeof(ctx->tbl));
		ctx->nlast_block = 0;
		return 1;
	}

	/* Initialise context. */
	if (cipher != NULL) {
		if (!EVP_EncryptInit_ex(&ctx->cctx, cipher, NULL, NULL, NULL))
			return 0;
	}

	/* Non-NULL key means initialisation is complete. */
	if (key != NULL) {
		if (EVP_CIPHER_CTX_cipher(&ctx->cctx) == NULL)
			return 0;

		/* make_kn() only supports block sizes of 8 and 16 bytes. */
		bl = EVP_CIPHER_CTX_block_size(&ctx->cctx);
		if (bl != 8 && bl != 16)
			return 0;

		/*
		 * Section 6.1, step 1: store the intermediate secret CIPH_K(0)
		 * in ctx->tbl.
		 */
		if (!EVP_CIPHER_CTX_set_key_length(&ctx->cctx, keylen))
			return 0;
		if (!EVP_EncryptInit_ex(&ctx->cctx, NULL, NULL, key, zero_iv))
			return 0;
		if (!EVP_Cipher(&ctx->cctx, ctx->tbl, zero_iv, bl))
			return 0;

		/* Section 6.1, step 2: compute k1 from intermediate secret. */
		make_kn(ctx->k1, ctx->tbl, bl);
		/* Section 6.1, step 3: compute k2 from k1. */
		make_kn(ctx->k2, ctx->k1, bl);

		/* Destroy intermediate secret and reset last block count. */
		explicit_bzero(ctx->tbl, sizeof(ctx->tbl));
		ctx->nlast_block = 0;

		/* Reset context again to get ready for the first data block. */
		if (!EVP_EncryptInit_ex(&ctx->cctx, NULL, NULL, NULL, zero_iv))
			return 0;
	}

	return 1;
}
LCRYPTO_ALIAS(CMAC_Init);

int
CMAC_Update(CMAC_CTX *ctx, const void *in, size_t dlen)
{
	const unsigned char *data = in;
	size_t bl;

	if (ctx->nlast_block == -1)
		return 0;
	if (dlen == 0)
		return 1;
	bl = EVP_CIPHER_CTX_block_size(&ctx->cctx);
	/* Copy into partial block if we need to */
	if (ctx->nlast_block > 0) {
		size_t nleft;

		nleft = bl - ctx->nlast_block;
		if (dlen < nleft)
			nleft = dlen;
		memcpy(ctx->last_block + ctx->nlast_block, data, nleft);
		dlen -= nleft;
		ctx->nlast_block += nleft;
		/* If no more to process return */
		if (dlen == 0)
			return 1;
		data += nleft;
		/* Else not final block so encrypt it */
		if (!EVP_Cipher(&ctx->cctx, ctx->tbl, ctx->last_block, bl))
			return 0;
	}
	/* Encrypt all but one of the complete blocks left */
	while (dlen > bl) {
		if (!EVP_Cipher(&ctx->cctx, ctx->tbl, data, bl))
			return 0;
		dlen -= bl;
		data += bl;
	}
	/* Copy any data left to last block buffer */
	memcpy(ctx->last_block, data, dlen);
	ctx->nlast_block = dlen;
	return 1;
}
LCRYPTO_ALIAS(CMAC_Update);

int
CMAC_Final(CMAC_CTX *ctx, unsigned char *out, size_t *poutlen)
{
	int i, bl, lb;

	if (ctx->nlast_block == -1)
		return 0;
	bl = EVP_CIPHER_CTX_block_size(&ctx->cctx);
	*poutlen = (size_t)bl;
	if (!out)
		return 1;
	lb = ctx->nlast_block;
	/* Is last block complete? */
	if (lb == bl) {
		for (i = 0; i < bl; i++)
			out[i] = ctx->last_block[i] ^ ctx->k1[i];
	} else {
		ctx->last_block[lb] = 0x80;
		if (bl - lb > 1)
			memset(ctx->last_block + lb + 1, 0, bl - lb - 1);
		for (i = 0; i < bl; i++)
			out[i] = ctx->last_block[i] ^ ctx->k2[i];
	}
	if (!EVP_Cipher(&ctx->cctx, out, out, bl)) {
		explicit_bzero(out, bl);
		return 0;
	}
	return 1;
}
LCRYPTO_ALIAS(CMAC_Final);

int
CMAC_resume(CMAC_CTX *ctx)
{
	if (ctx->nlast_block == -1)
		return 0;
	/* The buffer "tbl" containes the last fully encrypted block
	 * which is the last IV (or all zeroes if no last encrypted block).
	 * The last block has not been modified since CMAC_final().
	 * So reinitialising using the last decrypted block will allow
	 * CMAC to continue after calling CMAC_Final().
	 */
	return EVP_EncryptInit_ex(&ctx->cctx, NULL, NULL, NULL, ctx->tbl);
}
LCRYPTO_ALIAS(CMAC_resume);
