/* $OpenBSD: sha256.c,v 1.38 2026/05/09 07:14:42 jsing Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2011 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 */

#include <endian.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/crypto.h>
#include <openssl/sha.h>

#include "crypto_internal.h"

#if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_SHA256)

/* Ensure that SHA_LONG and uint32_t are equivalent. */
CTASSERT(sizeof(SHA_LONG) == sizeof(uint32_t));

void sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num);
void sha256_block_generic(SHA256_CTX *ctx, const void *_in, size_t num);

#ifndef HAVE_SHA256_BLOCK_GENERIC
/*
 * SHA-256 constants - see FIPS 180-4 section 4.2.2.
 */
static const uint32_t K256[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

static inline uint32_t
Sigma0(uint32_t x)
{
	return crypto_ror_u32(x, 2) ^ crypto_ror_u32(x, 13) ^
	    crypto_ror_u32(x, 22);
}

static inline uint32_t
Sigma1(uint32_t x)
{
	return crypto_ror_u32(x, 6) ^ crypto_ror_u32(x, 11) ^
	    crypto_ror_u32(x, 25);
}

static inline uint32_t
sigma0(uint32_t x)
{
	return crypto_ror_u32(x, 7) ^ crypto_ror_u32(x, 18) ^ (x >> 3);
}

static inline uint32_t
sigma1(uint32_t x)
{
	return crypto_ror_u32(x, 17) ^ crypto_ror_u32(x, 19) ^ (x >> 10);
}

static inline uint32_t
Ch(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}

static inline uint32_t
Maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline void
sha256_msg_schedule_update(uint32_t *W0, uint32_t W1, uint32_t W9, uint32_t W14)
{
	*W0 = sigma1(W14) + W9 + sigma0(W1) + *W0;
}

static inline void
sha256_round(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e,
    uint32_t *f, uint32_t *g, uint32_t *h, uint32_t Kt, uint32_t Wt)
{
	uint32_t T1, T2;

	T1 = *h + Sigma1(*e) + Ch(*e, *f, *g) + Kt + Wt;
	T2 = Sigma0(*a) + Maj(*a, *b, *c);

	*h = *g;
	*g = *f;
	*f = *e;
	*e = *d + T1;
	*d = *c;
	*c = *b;
	*b = *a;
	*a = T1 + T2;
}

void
sha256_block_generic(SHA256_CTX *ctx, const void *_in, size_t num)
{
	const uint8_t *in = _in;
	const uint32_t *in32;
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t W[16];
	int i;

	while (num--) {
		a = ctx->h[0];
		b = ctx->h[1];
		c = ctx->h[2];
		d = ctx->h[3];
		e = ctx->h[4];
		f = ctx->h[5];
		g = ctx->h[6];
		h = ctx->h[7];

		if ((size_t)in % 4 == 0) {
			/* Input is 32 bit aligned. */
			in32 = (const uint32_t *)in;
			W[0] = be32toh(in32[0]);
			W[1] = be32toh(in32[1]);
			W[2] = be32toh(in32[2]);
			W[3] = be32toh(in32[3]);
			W[4] = be32toh(in32[4]);
			W[5] = be32toh(in32[5]);
			W[6] = be32toh(in32[6]);
			W[7] = be32toh(in32[7]);
			W[8] = be32toh(in32[8]);
			W[9] = be32toh(in32[9]);
			W[10] = be32toh(in32[10]);
			W[11] = be32toh(in32[11]);
			W[12] = be32toh(in32[12]);
			W[13] = be32toh(in32[13]);
			W[14] = be32toh(in32[14]);
			W[15] = be32toh(in32[15]);
		} else {
			/* Input is not 32 bit aligned. */
			W[0] = crypto_load_be32toh(&in[0 * 4]);
			W[1] = crypto_load_be32toh(&in[1 * 4]);
			W[2] = crypto_load_be32toh(&in[2 * 4]);
			W[3] = crypto_load_be32toh(&in[3 * 4]);
			W[4] = crypto_load_be32toh(&in[4 * 4]);
			W[5] = crypto_load_be32toh(&in[5 * 4]);
			W[6] = crypto_load_be32toh(&in[6 * 4]);
			W[7] = crypto_load_be32toh(&in[7 * 4]);
			W[8] = crypto_load_be32toh(&in[8 * 4]);
			W[9] = crypto_load_be32toh(&in[9 * 4]);
			W[10] = crypto_load_be32toh(&in[10 * 4]);
			W[11] = crypto_load_be32toh(&in[11 * 4]);
			W[12] = crypto_load_be32toh(&in[12 * 4]);
			W[13] = crypto_load_be32toh(&in[13 * 4]);
			W[14] = crypto_load_be32toh(&in[14 * 4]);
			W[15] = crypto_load_be32toh(&in[15 * 4]);
		}
		in += SHA256_CBLOCK;

		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[0], W[0]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[1], W[1]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[2], W[2]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[3], W[3]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[4], W[4]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[5], W[5]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[6], W[6]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[7], W[7]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[8], W[8]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[9], W[9]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[10], W[10]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[11], W[11]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[12], W[12]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[13], W[13]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[14], W[14]);
		sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[15], W[15]);

		for (i = 16; i < 64; i += 16) {
			sha256_msg_schedule_update(&W[0], W[1], W[9], W[14]);
			sha256_msg_schedule_update(&W[1], W[2], W[10], W[15]);
			sha256_msg_schedule_update(&W[2], W[3], W[11], W[0]);
			sha256_msg_schedule_update(&W[3], W[4], W[12], W[1]);
			sha256_msg_schedule_update(&W[4], W[5], W[13], W[2]);
			sha256_msg_schedule_update(&W[5], W[6], W[14], W[3]);
			sha256_msg_schedule_update(&W[6], W[7], W[15], W[4]);
			sha256_msg_schedule_update(&W[7], W[8], W[0], W[5]);
			sha256_msg_schedule_update(&W[8], W[9], W[1], W[6]);
			sha256_msg_schedule_update(&W[9], W[10], W[2], W[7]);
			sha256_msg_schedule_update(&W[10], W[11], W[3], W[8]);
			sha256_msg_schedule_update(&W[11], W[12], W[4], W[9]);
			sha256_msg_schedule_update(&W[12], W[13], W[5], W[10]);
			sha256_msg_schedule_update(&W[13], W[14], W[6], W[11]);
			sha256_msg_schedule_update(&W[14], W[15], W[7], W[12]);
			sha256_msg_schedule_update(&W[15], W[0], W[8], W[13]);

			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 0], W[0]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 1], W[1]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 2], W[2]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 3], W[3]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 4], W[4]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 5], W[5]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 6], W[6]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 7], W[7]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 8], W[8]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 9], W[9]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 10], W[10]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 11], W[11]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 12], W[12]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 13], W[13]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 14], W[14]);
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i + 15], W[15]);
		}

		ctx->h[0] += a;
		ctx->h[1] += b;
		ctx->h[2] += c;
		ctx->h[3] += d;
		ctx->h[4] += e;
		ctx->h[5] += f;
		ctx->h[6] += g;
		ctx->h[7] += h;
	}
}
#endif

#ifndef HAVE_SHA256_BLOCK_DATA_ORDER
void
sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num)
{
	sha256_block_generic(ctx, _in, num);
}
#endif

int
SHA224_Init(SHA256_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.2. */
	ctx->h[0] = 0xc1059ed8UL;
	ctx->h[1] = 0x367cd507UL;
	ctx->h[2] = 0x3070dd17UL;
	ctx->h[3] = 0xf70e5939UL;
	ctx->h[4] = 0xffc00b31UL;
	ctx->h[5] = 0x68581511UL;
	ctx->h[6] = 0x64f98fa7UL;
	ctx->h[7] = 0xbefa4fa4UL;

	ctx->md_len = SHA224_DIGEST_LENGTH;

	return 1;
}
LCRYPTO_ALIAS(SHA224_Init);

int
SHA224_Update(SHA256_CTX *ctx, const void *data, size_t len)
{
	return SHA256_Update(ctx, data, len);
}
LCRYPTO_ALIAS(SHA224_Update);

int
SHA224_Final(unsigned char *md, SHA256_CTX *ctx)
{
	return SHA256_Final(md, ctx);
}
LCRYPTO_ALIAS(SHA224_Final);

unsigned char *
SHA224(const unsigned char *data, size_t len, unsigned char *md)
{
	SHA256_CTX ctx;

	SHA224_Init(&ctx);
	SHA256_Update(&ctx, data, len);
	SHA256_Final(md, &ctx);

	explicit_bzero(&ctx, sizeof(ctx));

	return (md);
}
LCRYPTO_ALIAS(SHA224);

int
SHA256_Init(SHA256_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.3. */
	ctx->h[0] = 0x6a09e667UL;
	ctx->h[1] = 0xbb67ae85UL;
	ctx->h[2] = 0x3c6ef372UL;
	ctx->h[3] = 0xa54ff53aUL;
	ctx->h[4] = 0x510e527fUL;
	ctx->h[5] = 0x9b05688cUL;
	ctx->h[6] = 0x1f83d9abUL;
	ctx->h[7] = 0x5be0cd19UL;

	ctx->md_len = SHA256_DIGEST_LENGTH;

	return 1;
}
LCRYPTO_ALIAS(SHA256_Init);

int
SHA256_Update(SHA256_CTX *ctx, const void *data_, size_t len)
{
	const unsigned char *data = data_;
	unsigned char *p;
	size_t n;

	if (len == 0)
		return 1;

	/* Update message bit counter. */
        crypto_add_u32dw_u64(&ctx->Nh, &ctx->Nl, (uint64_t)len << 3);

	n = ctx->num;
	if (n != 0) {
		p = (unsigned char *)ctx->data;

		if (len >= SHA_CBLOCK || len + n >= SHA_CBLOCK) {
			memcpy(p + n, data, SHA_CBLOCK - n);
			sha256_block_data_order(ctx, p, 1);
			n = SHA_CBLOCK - n;
			data += n;
			len -= n;
			ctx->num = 0;
			memset(p, 0, SHA_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy(p + n, data, len);
			ctx->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/SHA_CBLOCK;
	if (n > 0) {
		sha256_block_data_order(ctx, data, n);
		n *= SHA_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		p = (unsigned char *)ctx->data;
		ctx->num = (unsigned int)len;
		memcpy(p, data, len);
	}
	return 1;
}
LCRYPTO_ALIAS(SHA256_Update);

void
SHA256_Transform(SHA256_CTX *ctx, const unsigned char *data)
{
	sha256_block_data_order(ctx, data, 1);
}
LCRYPTO_ALIAS(SHA256_Transform);

int
SHA256_Final(unsigned char *md, SHA256_CTX *ctx)
{
	unsigned char *p = (unsigned char *)ctx->data;
	size_t n = ctx->num;
	unsigned int nn;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (SHA_CBLOCK - 8)) {
		memset(p + n, 0, SHA_CBLOCK - n);
		n = 0;
		sha256_block_data_order(ctx, p, 1);
	}

	memset(p + n, 0, SHA_CBLOCK - 8 - n);
	ctx->data[SHA_LBLOCK - 2] = htobe32(ctx->Nh);
	ctx->data[SHA_LBLOCK - 1] = htobe32(ctx->Nl);

	sha256_block_data_order(ctx, p, 1);
	ctx->num = 0;
	memset(p, 0, SHA_CBLOCK);

	/*
	 * Note that FIPS180-2 discusses "Truncation of the Hash Function Output."
	 * default: case below covers for it. It's not clear however if it's
	 * permitted to truncate to amount of bytes not divisible by 4. I bet not,
	 * but if it is, then default: case shall be extended. For reference.
	 * Idea behind separate cases for pre-defined lengths is to let the
	 * compiler decide if it's appropriate to unroll small loops.
	 */
	switch (ctx->md_len) {
	case SHA224_DIGEST_LENGTH:
		for (nn = 0; nn < SHA224_DIGEST_LENGTH / 4; nn++) {
			crypto_store_htobe32(md, ctx->h[nn]);
			md += 4;
		}
		break;

	case SHA256_DIGEST_LENGTH:
		for (nn = 0; nn < SHA256_DIGEST_LENGTH / 4; nn++) {
			crypto_store_htobe32(md, ctx->h[nn]);
			md += 4;
		}
		break;

	default:
		if (ctx->md_len > SHA256_DIGEST_LENGTH)
			return 0;
		for (nn = 0; nn < ctx->md_len / 4; nn++) {
			crypto_store_htobe32(md, ctx->h[nn]);
			md += 4;
		}
		break;
	}

	return 1;
}
LCRYPTO_ALIAS(SHA256_Final);

unsigned char *
SHA256(const unsigned char *data, size_t len, unsigned char *md)
{
	SHA256_CTX ctx;

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, data, len);
	SHA256_Final(md, &ctx);

	explicit_bzero(&ctx, sizeof(ctx));

	return (md);
}
LCRYPTO_ALIAS(SHA256);

#endif /* OPENSSL_NO_SHA256 */
