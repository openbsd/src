/*	$OpenBSD: sha256.c,v 1.1 2026/07/15 13:39:15 jsing Exp $	*/
/*
 * Copyright (c) 2023, 2026 Joel Sing <jsing@openbsd.org>
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

#include <sys/types.h>

#include <endian.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sha2.h>

static inline void
crypto_store_htobe32(uint8_t *dst, uint32_t v)
{
	v = htobe32(v);
	memcpy(dst, &v, sizeof(v));
}

static inline void
crypto_store_htobe64(uint8_t *dst, uint64_t v)
{
	v = htobe64(v);
	memcpy(dst, &v, sizeof(v));
}

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
crypto_load_be32toh(const uint8_t *src)
{
	uint32_t v;

	memcpy(&v, src, sizeof(v));

	return be32toh(v);
}

static inline uint32_t
crypto_ror_u32(uint32_t v, size_t shift)
{
	return (v << (32 - shift)) | (v >> shift);
}

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

#ifndef SHA256_SMALL
void
__sha256_block_generic(uint32_t state[8], const uint8_t *in, size_t num)
{
	const uint32_t *in32;
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t W[16];
	int i;

	while (num-- > 0) {
		a = state[0];
		b = state[1];
		c = state[2];
		d = state[3];
		e = state[4];
		f = state[5];
		g = state[6];
		h = state[7];

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
		in += SHA256_BLOCK_LENGTH;

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

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		state[5] += f;
		state[6] += g;
		state[7] += h;
	}
}
#endif
#endif

#ifdef SHA256_SMALL
void
__sha256_block(uint32_t state[8], const uint8_t *in, size_t num)
{
	const uint32_t *in32;
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t W[16];
	int i, j;

	while (num-- > 0) {
		a = state[0];
		b = state[1];
		c = state[2];
		d = state[3];
		e = state[4];
		f = state[5];
		g = state[6];
		h = state[7];

		for (i = 0; i < 64; i++) {
			if (i < 16) {
				W[i] = crypto_load_be32toh(&in[i * 4]);
			} else {
				sha256_msg_schedule_update(&W[i % 16], W[(i + 1) % 16],
				    W[(i + 9) % 16], W[(i + 14) % 16]);
			}
			sha256_round(&a, &b, &c, &d, &e, &f, &g, &h, K256[i], W[i % 16]);
		}

		in += SHA256_BLOCK_LENGTH;

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		state[5] += f;
		state[6] += g;
		state[7] += h;
	}
}
#else

#ifndef HAVE_SHA256_BLOCK
void
__sha256_block(uint32_t state[8], const uint8_t *in, size_t num)
{
	__sha256_block_generic(state, in, num);
}
#endif
#endif

#ifndef SHA256_SMALL
void
SHA224Init(SHA2_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.2. */
	ctx->state.st32[0] = 0xc1059ed8UL;
	ctx->state.st32[1] = 0x367cd507UL;
	ctx->state.st32[2] = 0x3070dd17UL;
	ctx->state.st32[3] = 0xf70e5939UL;
	ctx->state.st32[4] = 0xffc00b31UL;
	ctx->state.st32[5] = 0x68581511UL;
	ctx->state.st32[6] = 0x64f98fa7UL;
	ctx->state.st32[7] = 0xbefa4fa4UL;
}
DEF_WEAK(SHA224Init);

MAKE_CLONE(SHA224Transform, SHA256Transform);
MAKE_CLONE(SHA224Update, SHA256Update);
MAKE_CLONE(SHA224Pad, SHA256Pad);
DEF_WEAK(SHA224Transform);
DEF_WEAK(SHA224Update);
DEF_WEAK(SHA224Pad);

void
SHA224Final(uint8_t digest[SHA224_DIGEST_LENGTH], SHA2_CTX *ctx)
{
	int i;

	SHA224Pad(ctx);

	for (i = 0; i < SHA224_DIGEST_LENGTH / 4; i++)
		crypto_store_htobe32(&digest[i * 4], ctx->state.st32[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA224Final);
#endif

void
SHA256Init(SHA2_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.3. */
	ctx->state.st32[0] = 0x6a09e667UL;
	ctx->state.st32[1] = 0xbb67ae85UL;
	ctx->state.st32[2] = 0x3c6ef372UL;
	ctx->state.st32[3] = 0xa54ff53aUL;
	ctx->state.st32[4] = 0x510e527fUL;
	ctx->state.st32[5] = 0x9b05688cUL;
	ctx->state.st32[6] = 0x1f83d9abUL;
	ctx->state.st32[7] = 0x5be0cd19UL;
}
DEF_WEAK(SHA256Init);

void
SHA256Transform(uint32_t state[8], const uint8_t data[SHA256_BLOCK_LENGTH])
{
	__sha256_block(state, data, 1);
}
DEF_WEAK(SHA256Transform);

void
SHA256Update(SHA2_CTX *ctx, const uint8_t *data, size_t len)
{
	size_t blocks, m, n;

	if (len == 0)
		return;

	n = (ctx->bitcount[0] >> 3) % SHA256_BLOCK_LENGTH;
	ctx->bitcount[0] += (uint64_t)len << 3;

	if (n > 0) {
		if ((m = SHA256_BLOCK_LENGTH - n) > len)
			m = len;

		memcpy(&ctx->buffer[n], data, m);
		data += m;
		len -= m;

		if (n + m == SHA256_BLOCK_LENGTH) {
			__sha256_block(ctx->state.st32, ctx->buffer, 1);
			memset(ctx->buffer, 0, sizeof(ctx->buffer));
		}
	}

	if (len >= SHA256_BLOCK_LENGTH) {
		blocks = len / SHA256_BLOCK_LENGTH;
		__sha256_block(ctx->state.st32, data, blocks);
		data += blocks * SHA256_BLOCK_LENGTH;
		len -= blocks * SHA256_BLOCK_LENGTH;
	}

	if (len > 0)
		memcpy(ctx->buffer, data, len);
}
DEF_WEAK(SHA256Update);

void
SHA256Pad(SHA2_CTX *ctx)
{
	size_t n;

	n = (ctx->bitcount[0] >> 3) % SHA256_BLOCK_LENGTH;
	ctx->buffer[n++] = 0x80;

	if ((SHA256_BLOCK_LENGTH - n) < 8) {
		__sha256_block(ctx->state.st32, ctx->buffer, 1);
		memset(ctx->buffer, 0, sizeof(ctx->buffer));
	}

	crypto_store_htobe64(&ctx->buffer[SHA256_BLOCK_LENGTH - 8],
	    ctx->bitcount[0]);

	__sha256_block(ctx->state.st32, ctx->buffer, 1);
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
	ctx->bitcount[0] = 0;
}
DEF_WEAK(SHA256Pad);

void
SHA256Final(uint8_t digest[SHA256_DIGEST_LENGTH], SHA2_CTX *ctx)
{
	int i;

	SHA256Pad(ctx);

	for (i = 0; i < SHA256_DIGEST_LENGTH / 4; i++)
		crypto_store_htobe32(&digest[i * 4], ctx->state.st32[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA256Final);
