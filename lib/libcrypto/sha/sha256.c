/* $OpenBSD: sha256.c,v 1.27 2023/07/08 12:24:10 beck Exp $ */
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

#define	DATA_ORDER_IS_BIG_ENDIAN

#define	HASH_LONG		SHA_LONG
#define	HASH_CTX		SHA256_CTX
#define	HASH_CBLOCK		SHA_CBLOCK

#define	HASH_BLOCK_DATA_ORDER	sha256_block_data_order

#ifdef SHA256_ASM
void sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num);
#endif

#define HASH_NO_UPDATE
#define HASH_NO_TRANSFORM
#define HASH_NO_FINAL

#include "md32_common.h"

#ifndef SHA256_ASM
static const SHA_LONG K256[64] = {
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

/*
 * FIPS specification refers to right rotations, while our ROTATE macro
 * is left one. This is why you might notice that rotation coefficients
 * differ from those observed in FIPS document by 32-N...
 */
#define Sigma0(x)	(ROTATE((x),30) ^ ROTATE((x),19) ^ ROTATE((x),10))
#define Sigma1(x)	(ROTATE((x),26) ^ ROTATE((x),21) ^ ROTATE((x),7))
#define sigma0(x)	(ROTATE((x),25) ^ ROTATE((x),14) ^ ((x)>>3))
#define sigma1(x)	(ROTATE((x),15) ^ ROTATE((x),13) ^ ((x)>>10))

#define Ch(x, y, z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x, y, z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define	ROUND_00_15(x, i, a, b, c, d, e, f, g, h)	do {	\
	T1 = x + h + Sigma1(e) + Ch(e, f, g) + K256[i];	\
	h = Sigma0(a) + Maj(a, b, c);				\
	d += T1;	h += T1;		} while (0)

#define	ROUND_16_63(i, a, b, c, d, e, f, g, h, X)	do {	\
	s0 = X[(i+1)&0x0f];	s0 = sigma0(s0);		\
	s1 = X[(i+14)&0x0f];	s1 = sigma1(s1);		\
	T1 = X[(i)&0x0f] += s0 + s1 + X[(i+9)&0x0f];		\
	ROUND_00_15(T1, i, a, b, c, d, e, f, g, h);	} while (0)

static void
sha256_block_data_order(SHA256_CTX *ctx, const void *_in, size_t num)
{
	const uint8_t *in = _in;
	const SHA_LONG *in32;
	unsigned MD32_REG_T a, b, c, d, e, f, g, h, s0, s1, T1;
	SHA_LONG X[16];
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
			in32 = (const SHA_LONG *)in;
			X[0] = be32toh(in32[0]);
			X[1] = be32toh(in32[1]);
			X[2] = be32toh(in32[2]);
			X[3] = be32toh(in32[3]);
			X[4] = be32toh(in32[4]);
			X[5] = be32toh(in32[5]);
			X[6] = be32toh(in32[6]);
			X[7] = be32toh(in32[7]);
			X[8] = be32toh(in32[8]);
			X[9] = be32toh(in32[9]);
			X[10] = be32toh(in32[10]);
			X[11] = be32toh(in32[11]);
			X[12] = be32toh(in32[12]);
			X[13] = be32toh(in32[13]);
			X[14] = be32toh(in32[14]);
			X[15] = be32toh(in32[15]);
		} else {
			/* Input is not 32 bit aligned. */
			X[0] = crypto_load_be32toh(&in[0 * 4]);
			X[1] = crypto_load_be32toh(&in[1 * 4]);
			X[2] = crypto_load_be32toh(&in[2 * 4]);
			X[3] = crypto_load_be32toh(&in[3 * 4]);
			X[4] = crypto_load_be32toh(&in[4 * 4]);
			X[5] = crypto_load_be32toh(&in[5 * 4]);
			X[6] = crypto_load_be32toh(&in[6 * 4]);
			X[7] = crypto_load_be32toh(&in[7 * 4]);
			X[8] = crypto_load_be32toh(&in[8 * 4]);
			X[9] = crypto_load_be32toh(&in[9 * 4]);
			X[10] = crypto_load_be32toh(&in[10 * 4]);
			X[11] = crypto_load_be32toh(&in[11 * 4]);
			X[12] = crypto_load_be32toh(&in[12 * 4]);
			X[13] = crypto_load_be32toh(&in[13 * 4]);
			X[14] = crypto_load_be32toh(&in[14 * 4]);
			X[15] = crypto_load_be32toh(&in[15 * 4]);
		}
		in += SHA256_CBLOCK;

		ROUND_00_15(X[0], 0, a, b, c, d, e, f, g, h);
		ROUND_00_15(X[1], 1, h, a, b, c, d, e, f, g);
		ROUND_00_15(X[2], 2, g, h, a, b, c, d, e, f);
		ROUND_00_15(X[3], 3, f, g, h, a, b, c, d, e);
		ROUND_00_15(X[4], 4, e, f, g, h, a, b, c, d);
		ROUND_00_15(X[5], 5, d, e, f, g, h, a, b, c);
		ROUND_00_15(X[6], 6, c, d, e, f, g, h, a, b);
		ROUND_00_15(X[7], 7, b, c, d, e, f, g, h, a);

		ROUND_00_15(X[8], 8, a, b, c, d, e, f, g, h);
		ROUND_00_15(X[9], 9, h, a, b, c, d, e, f, g);
		ROUND_00_15(X[10], 10, g, h, a, b, c, d, e, f);
		ROUND_00_15(X[11], 11, f, g, h, a, b, c, d, e);
		ROUND_00_15(X[12], 12, e, f, g, h, a, b, c, d);
		ROUND_00_15(X[13], 13, d, e, f, g, h, a, b, c);
		ROUND_00_15(X[14], 14, c, d, e, f, g, h, a, b);
		ROUND_00_15(X[15], 15, b, c, d, e, f, g, h, a);

		for (i = 16; i < 64; i += 8) {
			ROUND_16_63(i + 0, a, b, c, d, e, f, g, h, X);
			ROUND_16_63(i + 1, h, a, b, c, d, e, f, g, X);
			ROUND_16_63(i + 2, g, h, a, b, c, d, e, f, X);
			ROUND_16_63(i + 3, f, g, h, a, b, c, d, e, X);
			ROUND_16_63(i + 4, e, f, g, h, a, b, c, d, X);
			ROUND_16_63(i + 5, d, e, f, g, h, a, b, c, X);
			ROUND_16_63(i + 6, c, d, e, f, g, h, a, b, X);
			ROUND_16_63(i + 7, b, c, d, e, f, g, h, a, X);
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
#endif /* SHA256_ASM */

int
SHA224_Init(SHA256_CTX *c)
{
	memset(c, 0, sizeof(*c));

	c->h[0] = 0xc1059ed8UL;
	c->h[1] = 0x367cd507UL;
	c->h[2] = 0x3070dd17UL;
	c->h[3] = 0xf70e5939UL;
	c->h[4] = 0xffc00b31UL;
	c->h[5] = 0x68581511UL;
	c->h[6] = 0x64f98fa7UL;
	c->h[7] = 0xbefa4fa4UL;

	c->md_len = SHA224_DIGEST_LENGTH;

	return 1;
}
LCRYPTO_ALIAS(SHA224_Init);

int
SHA224_Update(SHA256_CTX *c, const void *data, size_t len)
{
	return SHA256_Update(c, data, len);
}
LCRYPTO_ALIAS(SHA224_Update);

int
SHA224_Final(unsigned char *md, SHA256_CTX *c)
{
	return SHA256_Final(md, c);
}
LCRYPTO_ALIAS(SHA224_Final);

unsigned char *
SHA224(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA256_CTX c;
	static unsigned char m[SHA224_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	SHA224_Init(&c);
	SHA256_Update(&c, d, n);
	SHA256_Final(md, &c);

	explicit_bzero(&c, sizeof(c));

	return (md);
}
LCRYPTO_ALIAS(SHA224);

int
SHA256_Init(SHA256_CTX *c)
{
	memset(c, 0, sizeof(*c));

	c->h[0] = 0x6a09e667UL;
	c->h[1] = 0xbb67ae85UL;
	c->h[2] = 0x3c6ef372UL;
	c->h[3] = 0xa54ff53aUL;
	c->h[4] = 0x510e527fUL;
	c->h[5] = 0x9b05688cUL;
	c->h[6] = 0x1f83d9abUL;
	c->h[7] = 0x5be0cd19UL;

	c->md_len = SHA256_DIGEST_LENGTH;

	return 1;
}
LCRYPTO_ALIAS(SHA256_Init);

int
SHA256_Update(SHA256_CTX *c, const void *data_, size_t len)
{
	const unsigned char *data = data_;
	unsigned char *p;
	SHA_LONG l;
	size_t n;

	if (len == 0)
		return 1;

	l = (c->Nl + (((SHA_LONG)len) << 3)) & 0xffffffffUL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh += (SHA_LONG)(len >> 29);	/* might cause compiler warning on 16-bit */
	c->Nl = l;

	n = c->num;
	if (n != 0) {
		p = (unsigned char *)c->data;

		if (len >= SHA_CBLOCK || len + n >= SHA_CBLOCK) {
			memcpy(p + n, data, SHA_CBLOCK - n);
			sha256_block_data_order(c, p, 1);
			n = SHA_CBLOCK - n;
			data += n;
			len -= n;
			c->num = 0;
			memset(p, 0, SHA_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy(p + n, data, len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/SHA_CBLOCK;
	if (n > 0) {
		sha256_block_data_order(c, data, n);
		n *= SHA_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		p = (unsigned char *)c->data;
		c->num = (unsigned int)len;
		memcpy(p, data, len);
	}
	return 1;
}
LCRYPTO_ALIAS(SHA256_Update);

void
SHA256_Transform(SHA256_CTX *c, const unsigned char *data)
{
	sha256_block_data_order(c, data, 1);
}
LCRYPTO_ALIAS(SHA256_Transform);

int
SHA256_Final(unsigned char *md, SHA256_CTX *c)
{
	unsigned char *p = (unsigned char *)c->data;
	size_t n = c->num;
	unsigned int nn;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (SHA_CBLOCK - 8)) {
		memset(p + n, 0, SHA_CBLOCK - n);
		n = 0;
		sha256_block_data_order(c, p, 1);
	}

	memset(p + n, 0, SHA_CBLOCK - 8 - n);
	c->data[SHA_LBLOCK - 2] = htobe32(c->Nh);
	c->data[SHA_LBLOCK - 1] = htobe32(c->Nl);

	sha256_block_data_order(c, p, 1);
	c->num = 0;
	memset(p, 0, SHA_CBLOCK);

	/*
	 * Note that FIPS180-2 discusses "Truncation of the Hash Function Output."
	 * default: case below covers for it. It's not clear however if it's
	 * permitted to truncate to amount of bytes not divisible by 4. I bet not,
	 * but if it is, then default: case shall be extended. For reference.
	 * Idea behind separate cases for pre-defined lengths is to let the
	 * compiler decide if it's appropriate to unroll small loops.
	 */
	switch (c->md_len) {
	case SHA224_DIGEST_LENGTH:
		for (nn = 0; nn < SHA224_DIGEST_LENGTH / 4; nn++) {
			crypto_store_htobe32(md, c->h[nn]);
			md += 4;
		}
		break;

	case SHA256_DIGEST_LENGTH:
		for (nn = 0; nn < SHA256_DIGEST_LENGTH / 4; nn++) {
			crypto_store_htobe32(md, c->h[nn]);
			md += 4;
		}
		break;

	default:
		if (c->md_len > SHA256_DIGEST_LENGTH)
			return 0;
		for (nn = 0; nn < c->md_len / 4; nn++) {
			crypto_store_htobe32(md, c->h[nn]);
			md += 4;
		}
		break;
	}

	return 1;
}
LCRYPTO_ALIAS(SHA256_Final);

unsigned char *
SHA256(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA256_CTX c;
	static unsigned char m[SHA256_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	SHA256_Init(&c);
	SHA256_Update(&c, d, n);
	SHA256_Final(md, &c);

	explicit_bzero(&c, sizeof(c));

	return (md);
}
LCRYPTO_ALIAS(SHA256);

#endif /* OPENSSL_NO_SHA256 */
