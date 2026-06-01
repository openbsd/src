/*	$OpenBSD: sha1.c,v 1.29 2026/06/01 13:27:24 jsing Exp $	*/
/*
 * Copyright (c) 2024, 2026 Joel Sing <jsing@openbsd.org>
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

#include <sha1.h>

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

#ifndef HAVE_SHA1_BLOCK_GENERIC
static inline uint32_t
crypto_load_be32toh(const uint8_t *src)
{
	uint32_t v;

	memcpy(&v, src, sizeof(v));

	return be32toh(v);
}

static inline uint32_t
crypto_rol_u32(uint32_t v, size_t shift)
{
	return (v << shift) | (v >> (32 - shift));
}

static inline uint32_t
Ch(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}

static inline uint32_t
Parity(uint32_t x, uint32_t y, uint32_t z)
{
	return x ^ y ^ z;
}

static inline uint32_t
Maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline void
sha1_msg_schedule_update(uint32_t *W0, uint32_t W2, uint32_t W8, uint32_t W13)
{
	*W0 = crypto_rol_u32(W13 ^ W8 ^ W2 ^ *W0, 1);
}

static inline void
sha1_round1(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e,
    uint32_t Wt)
{
	uint32_t Kt, T;

	Kt = 0x5a827999UL;
	T = crypto_rol_u32(*a, 5) + Ch(*b, *c, *d) + *e + Kt + Wt;

	*e = *d;
	*d = *c;
	*c = crypto_rol_u32(*b, 30);
	*b = *a;
	*a = T;
}

static inline void
sha1_round2(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e,
    uint32_t Wt)
{
	uint32_t Kt, T;

	Kt = 0x6ed9eba1UL;
	T = crypto_rol_u32(*a, 5) + Parity(*b, *c, *d) + *e + Kt + Wt;

	*e = *d;
	*d = *c;
	*c = crypto_rol_u32(*b, 30);
	*b = *a;
	*a = T;
}

static inline void
sha1_round3(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e,
    uint32_t Wt)
{
	uint32_t Kt, T;

	Kt = 0x8f1bbcdcUL;
	T = crypto_rol_u32(*a, 5) + Maj(*b, *c, *d) + *e + Kt + Wt;

	*e = *d;
	*d = *c;
	*c = crypto_rol_u32(*b, 30);
	*b = *a;
	*a = T;
}

static inline void
sha1_round4(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d, uint32_t *e,
    uint32_t Wt)
{
	uint32_t Kt, T;

	Kt = 0xca62c1d6UL;
	T = crypto_rol_u32(*a, 5) + Parity(*b, *c, *d) + *e + Kt + Wt;

	*e = *d;
	*d = *c;
	*c = crypto_rol_u32(*b, 30);
	*b = *a;
	*a = T;
}

void
__sha1_block_generic(uint32_t state[5], const uint8_t *in, size_t num)
{
	const uint32_t *in32;
	uint32_t a, b, c, d, e;
	uint32_t W[16];

	while (num-- > 0) {
		a = state[0];
		b = state[1];
		c = state[2];
		d = state[3];
		e = state[4];

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
		in += SHA1_BLOCK_LENGTH;

		sha1_round1(&a, &b, &c, &d, &e, W[0]);
		sha1_round1(&a, &b, &c, &d, &e, W[1]);
		sha1_round1(&a, &b, &c, &d, &e, W[2]);
		sha1_round1(&a, &b, &c, &d, &e, W[3]);
		sha1_round1(&a, &b, &c, &d, &e, W[4]);
		sha1_round1(&a, &b, &c, &d, &e, W[5]);
		sha1_round1(&a, &b, &c, &d, &e, W[6]);
		sha1_round1(&a, &b, &c, &d, &e, W[7]);
		sha1_round1(&a, &b, &c, &d, &e, W[8]);
		sha1_round1(&a, &b, &c, &d, &e, W[9]);
		sha1_round1(&a, &b, &c, &d, &e, W[10]);
		sha1_round1(&a, &b, &c, &d, &e, W[11]);
		sha1_round1(&a, &b, &c, &d, &e, W[12]);
		sha1_round1(&a, &b, &c, &d, &e, W[13]);
		sha1_round1(&a, &b, &c, &d, &e, W[14]);
		sha1_round1(&a, &b, &c, &d, &e, W[15]);

		sha1_msg_schedule_update(&W[0], W[2], W[8], W[13]);
		sha1_msg_schedule_update(&W[1], W[3], W[9], W[14]);
		sha1_msg_schedule_update(&W[2], W[4], W[10], W[15]);
		sha1_msg_schedule_update(&W[3], W[5], W[11], W[0]);
		sha1_msg_schedule_update(&W[4], W[6], W[12], W[1]);
		sha1_msg_schedule_update(&W[5], W[7], W[13], W[2]);
		sha1_msg_schedule_update(&W[6], W[8], W[14], W[3]);
		sha1_msg_schedule_update(&W[7], W[9], W[15], W[4]);
		sha1_msg_schedule_update(&W[8], W[10], W[0], W[5]);
		sha1_msg_schedule_update(&W[9], W[11], W[1], W[6]);
		sha1_msg_schedule_update(&W[10], W[12], W[2], W[7]);
		sha1_msg_schedule_update(&W[11], W[13], W[3], W[8]);
		sha1_msg_schedule_update(&W[12], W[14], W[4], W[9]);
		sha1_msg_schedule_update(&W[13], W[15], W[5], W[10]);
		sha1_msg_schedule_update(&W[14], W[0], W[6], W[11]);
		sha1_msg_schedule_update(&W[15], W[1], W[7], W[12]);

		sha1_round1(&a, &b, &c, &d, &e, W[0]);
		sha1_round1(&a, &b, &c, &d, &e, W[1]);
		sha1_round1(&a, &b, &c, &d, &e, W[2]);
		sha1_round1(&a, &b, &c, &d, &e, W[3]);
		sha1_round2(&a, &b, &c, &d, &e, W[4]);
		sha1_round2(&a, &b, &c, &d, &e, W[5]);
		sha1_round2(&a, &b, &c, &d, &e, W[6]);
		sha1_round2(&a, &b, &c, &d, &e, W[7]);
		sha1_round2(&a, &b, &c, &d, &e, W[8]);
		sha1_round2(&a, &b, &c, &d, &e, W[9]);
		sha1_round2(&a, &b, &c, &d, &e, W[10]);
		sha1_round2(&a, &b, &c, &d, &e, W[11]);
		sha1_round2(&a, &b, &c, &d, &e, W[12]);
		sha1_round2(&a, &b, &c, &d, &e, W[13]);
		sha1_round2(&a, &b, &c, &d, &e, W[14]);
		sha1_round2(&a, &b, &c, &d, &e, W[15]);

		sha1_msg_schedule_update(&W[0], W[2], W[8], W[13]);
		sha1_msg_schedule_update(&W[1], W[3], W[9], W[14]);
		sha1_msg_schedule_update(&W[2], W[4], W[10], W[15]);
		sha1_msg_schedule_update(&W[3], W[5], W[11], W[0]);
		sha1_msg_schedule_update(&W[4], W[6], W[12], W[1]);
		sha1_msg_schedule_update(&W[5], W[7], W[13], W[2]);
		sha1_msg_schedule_update(&W[6], W[8], W[14], W[3]);
		sha1_msg_schedule_update(&W[7], W[9], W[15], W[4]);
		sha1_msg_schedule_update(&W[8], W[10], W[0], W[5]);
		sha1_msg_schedule_update(&W[9], W[11], W[1], W[6]);
		sha1_msg_schedule_update(&W[10], W[12], W[2], W[7]);
		sha1_msg_schedule_update(&W[11], W[13], W[3], W[8]);
		sha1_msg_schedule_update(&W[12], W[14], W[4], W[9]);
		sha1_msg_schedule_update(&W[13], W[15], W[5], W[10]);
		sha1_msg_schedule_update(&W[14], W[0], W[6], W[11]);
		sha1_msg_schedule_update(&W[15], W[1], W[7], W[12]);

		sha1_round2(&a, &b, &c, &d, &e, W[0]);
		sha1_round2(&a, &b, &c, &d, &e, W[1]);
		sha1_round2(&a, &b, &c, &d, &e, W[2]);
		sha1_round2(&a, &b, &c, &d, &e, W[3]);
		sha1_round2(&a, &b, &c, &d, &e, W[4]);
		sha1_round2(&a, &b, &c, &d, &e, W[5]);
		sha1_round2(&a, &b, &c, &d, &e, W[6]);
		sha1_round2(&a, &b, &c, &d, &e, W[7]);
		sha1_round3(&a, &b, &c, &d, &e, W[8]);
		sha1_round3(&a, &b, &c, &d, &e, W[9]);
		sha1_round3(&a, &b, &c, &d, &e, W[10]);
		sha1_round3(&a, &b, &c, &d, &e, W[11]);
		sha1_round3(&a, &b, &c, &d, &e, W[12]);
		sha1_round3(&a, &b, &c, &d, &e, W[13]);
		sha1_round3(&a, &b, &c, &d, &e, W[14]);
		sha1_round3(&a, &b, &c, &d, &e, W[15]);

		sha1_msg_schedule_update(&W[0], W[2], W[8], W[13]);
		sha1_msg_schedule_update(&W[1], W[3], W[9], W[14]);
		sha1_msg_schedule_update(&W[2], W[4], W[10], W[15]);
		sha1_msg_schedule_update(&W[3], W[5], W[11], W[0]);
		sha1_msg_schedule_update(&W[4], W[6], W[12], W[1]);
		sha1_msg_schedule_update(&W[5], W[7], W[13], W[2]);
		sha1_msg_schedule_update(&W[6], W[8], W[14], W[3]);
		sha1_msg_schedule_update(&W[7], W[9], W[15], W[4]);
		sha1_msg_schedule_update(&W[8], W[10], W[0], W[5]);
		sha1_msg_schedule_update(&W[9], W[11], W[1], W[6]);
		sha1_msg_schedule_update(&W[10], W[12], W[2], W[7]);
		sha1_msg_schedule_update(&W[11], W[13], W[3], W[8]);
		sha1_msg_schedule_update(&W[12], W[14], W[4], W[9]);
		sha1_msg_schedule_update(&W[13], W[15], W[5], W[10]);
		sha1_msg_schedule_update(&W[14], W[0], W[6], W[11]);
		sha1_msg_schedule_update(&W[15], W[1], W[7], W[12]);

		sha1_round3(&a, &b, &c, &d, &e, W[0]);
		sha1_round3(&a, &b, &c, &d, &e, W[1]);
		sha1_round3(&a, &b, &c, &d, &e, W[2]);
		sha1_round3(&a, &b, &c, &d, &e, W[3]);
		sha1_round3(&a, &b, &c, &d, &e, W[4]);
		sha1_round3(&a, &b, &c, &d, &e, W[5]);
		sha1_round3(&a, &b, &c, &d, &e, W[6]);
		sha1_round3(&a, &b, &c, &d, &e, W[7]);
		sha1_round3(&a, &b, &c, &d, &e, W[8]);
		sha1_round3(&a, &b, &c, &d, &e, W[9]);
		sha1_round3(&a, &b, &c, &d, &e, W[10]);
		sha1_round3(&a, &b, &c, &d, &e, W[11]);
		sha1_round4(&a, &b, &c, &d, &e, W[12]);
		sha1_round4(&a, &b, &c, &d, &e, W[13]);
		sha1_round4(&a, &b, &c, &d, &e, W[14]);
		sha1_round4(&a, &b, &c, &d, &e, W[15]);

		sha1_msg_schedule_update(&W[0], W[2], W[8], W[13]);
		sha1_msg_schedule_update(&W[1], W[3], W[9], W[14]);
		sha1_msg_schedule_update(&W[2], W[4], W[10], W[15]);
		sha1_msg_schedule_update(&W[3], W[5], W[11], W[0]);
		sha1_msg_schedule_update(&W[4], W[6], W[12], W[1]);
		sha1_msg_schedule_update(&W[5], W[7], W[13], W[2]);
		sha1_msg_schedule_update(&W[6], W[8], W[14], W[3]);
		sha1_msg_schedule_update(&W[7], W[9], W[15], W[4]);
		sha1_msg_schedule_update(&W[8], W[10], W[0], W[5]);
		sha1_msg_schedule_update(&W[9], W[11], W[1], W[6]);
		sha1_msg_schedule_update(&W[10], W[12], W[2], W[7]);
		sha1_msg_schedule_update(&W[11], W[13], W[3], W[8]);
		sha1_msg_schedule_update(&W[12], W[14], W[4], W[9]);
		sha1_msg_schedule_update(&W[13], W[15], W[5], W[10]);
		sha1_msg_schedule_update(&W[14], W[0], W[6], W[11]);
		sha1_msg_schedule_update(&W[15], W[1], W[7], W[12]);

		sha1_round4(&a, &b, &c, &d, &e, W[0]);
		sha1_round4(&a, &b, &c, &d, &e, W[1]);
		sha1_round4(&a, &b, &c, &d, &e, W[2]);
		sha1_round4(&a, &b, &c, &d, &e, W[3]);
		sha1_round4(&a, &b, &c, &d, &e, W[4]);
		sha1_round4(&a, &b, &c, &d, &e, W[5]);
		sha1_round4(&a, &b, &c, &d, &e, W[6]);
		sha1_round4(&a, &b, &c, &d, &e, W[7]);
		sha1_round4(&a, &b, &c, &d, &e, W[8]);
		sha1_round4(&a, &b, &c, &d, &e, W[9]);
		sha1_round4(&a, &b, &c, &d, &e, W[10]);
		sha1_round4(&a, &b, &c, &d, &e, W[11]);
		sha1_round4(&a, &b, &c, &d, &e, W[12]);
		sha1_round4(&a, &b, &c, &d, &e, W[13]);
		sha1_round4(&a, &b, &c, &d, &e, W[14]);
		sha1_round4(&a, &b, &c, &d, &e, W[15]);

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
	}
}
#endif

#ifndef HAVE_SHA1_BLOCK
void
__sha1_block(uint32_t state[5], const uint8_t *in, size_t num)
{
	__sha1_block_generic(state, in, num);
}
#endif

void
SHA1Init(SHA1_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.1. */
	ctx->state[0] = 0x67452301UL;
	ctx->state[1] = 0xefcdab89UL;
	ctx->state[2] = 0x98badcfeUL;
	ctx->state[3] = 0x10325476UL;
	ctx->state[4] = 0xc3d2e1f0UL;
}
DEF_WEAK(SHA1Init);

void
SHA1Transform(uint32_t state[5], const uint8_t data[SHA1_BLOCK_LENGTH])
{
	__sha1_block(state, data, 1);
}
DEF_WEAK(SHA1Transform);

void
SHA1Update(SHA1_CTX *ctx, const uint8_t *data, size_t len)
{
	size_t blocks, m, n;

	if (len == 0)
		return;

	n = (ctx->count >> 3) % SHA1_BLOCK_LENGTH;
	ctx->count += (uint64_t)len << 3;

	if (n > 0) {
		if ((m = SHA1_BLOCK_LENGTH - n) > len)
			m = len;

		memcpy(&ctx->buffer[n], data, m);
		data += m;
		len -= m;

		if (n + m == SHA1_BLOCK_LENGTH) {
			__sha1_block(ctx->state, ctx->buffer, 1);
			memset(ctx->buffer, 0, sizeof(ctx->buffer));
		}
	}

	if (len >= SHA1_BLOCK_LENGTH) {
		blocks = len / SHA1_BLOCK_LENGTH;
		__sha1_block(ctx->state, data, blocks);
		data += blocks * SHA1_BLOCK_LENGTH;
		len -= blocks * SHA1_BLOCK_LENGTH;
	}

	if (len > 0)
		memcpy(ctx->buffer, data, len);
}
DEF_WEAK(SHA1Update);

void
SHA1Pad(SHA1_CTX *ctx)
{
	size_t n;

	n = (ctx->count >> 3) % SHA1_BLOCK_LENGTH;
	ctx->buffer[n++] = 0x80;

	if ((SHA1_BLOCK_LENGTH - n) < 8) {
		__sha1_block(ctx->state, ctx->buffer, 1);
		memset(ctx->buffer, 0, sizeof(ctx->buffer));
	}

	crypto_store_htobe64(&ctx->buffer[SHA1_BLOCK_LENGTH - 8],
	    ctx->count);

	__sha1_block(ctx->state, ctx->buffer, 1);
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
	ctx->count = 0;
}
DEF_WEAK(SHA1Pad);

void
SHA1Final(uint8_t digest[SHA1_DIGEST_LENGTH], SHA1_CTX *ctx)
{
	int i;

	SHA1Pad(ctx);

	for (i = 0; i < SHA1_DIGEST_LENGTH / 4; i++)
		crypto_store_htobe32(&digest[i * 4], ctx->state[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA1Final);
