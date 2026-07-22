/*	$OpenBSD: sha512.c,v 1.2 2026/07/22 14:29:56 jsing Exp $	*/
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
crypto_store_htobe64(uint8_t *dst, uint64_t v)
{
	v = htobe64(v);
	memcpy(dst, &v, sizeof(v));
}

#ifndef HAVE_SHA512_BLOCK_GENERIC
/*
 * SHA-512 constants - see FIPS 180-4 section 4.2.3.
 */
static const uint64_t K512[80] = {
	UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd),
	UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
	UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
	UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
	UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe),
	UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
	UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1),
	UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
	UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
	UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
	UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483),
	UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
	UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210),
	UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
	UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
	UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
	UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926),
	UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
	UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8),
	UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
	UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
	UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
	UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910),
	UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
	UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53),
	UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
	UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
	UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
	UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60),
	UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
	UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9),
	UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
	UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
	UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
	UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6),
	UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
	UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493),
	UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
	UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
	UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817),
};

static inline uint64_t
crypto_load_be64toh(const uint8_t *src)
{
	uint64_t v;

	memcpy(&v, src, sizeof(v));

	return be64toh(v);
}

static inline uint64_t
crypto_ror_u64(uint64_t v, size_t shift)
{
	return (v << (64 - shift)) | (v >> shift);
}

static inline uint64_t
Sigma0(uint64_t x)
{
	return crypto_ror_u64(x, 28) ^ crypto_ror_u64(x, 34) ^
	    crypto_ror_u64(x, 39);
}

static inline uint64_t
Sigma1(uint64_t x)
{
	return crypto_ror_u64(x, 14) ^ crypto_ror_u64(x, 18) ^
	    crypto_ror_u64(x, 41);
}

static inline uint64_t
sigma0(uint64_t x)
{
	return crypto_ror_u64(x, 1) ^ crypto_ror_u64(x, 8) ^ (x >> 7);
}

static inline uint64_t
sigma1(uint64_t x)
{
	return crypto_ror_u64(x, 19) ^ crypto_ror_u64(x, 61) ^ (x >> 6);
}

static inline uint64_t
Ch(uint64_t x, uint64_t y, uint64_t z)
{
	return (x & y) ^ (~x & z);
}

static inline uint64_t
Maj(uint64_t x, uint64_t y, uint64_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline void
sha512_msg_schedule_update(uint64_t *W0, uint64_t W1,
    uint64_t W9, uint64_t W14)
{
	*W0 = sigma1(W14) + W9 + sigma0(W1) + *W0;
}

static inline void
sha512_round(uint64_t *a, uint64_t *b, uint64_t *c, uint64_t *d,
    uint64_t *e, uint64_t *f, uint64_t *g, uint64_t *h,
    uint64_t Kt, uint64_t Wt)
{
	uint64_t T1, T2;

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

#ifndef SHA512_SMALL
void
__sha512_block_generic(uint64_t state[8], const uint8_t *in, size_t num)
{
	const uint64_t *in64;
	uint64_t a, b, c, d, e, f, g, h;
	uint64_t W[16];
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

		if ((size_t)in % sizeof(uint64_t) == 0) {
			/* Input is 64 bit aligned. */
			in64 = (const uint64_t *)in;
			W[0] = be64toh(in64[0]);
			W[1] = be64toh(in64[1]);
			W[2] = be64toh(in64[2]);
			W[3] = be64toh(in64[3]);
			W[4] = be64toh(in64[4]);
			W[5] = be64toh(in64[5]);
			W[6] = be64toh(in64[6]);
			W[7] = be64toh(in64[7]);
			W[8] = be64toh(in64[8]);
			W[9] = be64toh(in64[9]);
			W[10] = be64toh(in64[10]);
			W[11] = be64toh(in64[11]);
			W[12] = be64toh(in64[12]);
			W[13] = be64toh(in64[13]);
			W[14] = be64toh(in64[14]);
			W[15] = be64toh(in64[15]);
		} else {
			/* Input is not 64 bit aligned. */
			W[0] = crypto_load_be64toh(&in[0 * 8]);
			W[1] = crypto_load_be64toh(&in[1 * 8]);
			W[2] = crypto_load_be64toh(&in[2 * 8]);
			W[3] = crypto_load_be64toh(&in[3 * 8]);
			W[4] = crypto_load_be64toh(&in[4 * 8]);
			W[5] = crypto_load_be64toh(&in[5 * 8]);
			W[6] = crypto_load_be64toh(&in[6 * 8]);
			W[7] = crypto_load_be64toh(&in[7 * 8]);
			W[8] = crypto_load_be64toh(&in[8 * 8]);
			W[9] = crypto_load_be64toh(&in[9 * 8]);
			W[10] = crypto_load_be64toh(&in[10 * 8]);
			W[11] = crypto_load_be64toh(&in[11 * 8]);
			W[12] = crypto_load_be64toh(&in[12 * 8]);
			W[13] = crypto_load_be64toh(&in[13 * 8]);
			W[14] = crypto_load_be64toh(&in[14 * 8]);
			W[15] = crypto_load_be64toh(&in[15 * 8]);
		}
		in += SHA512_BLOCK_LENGTH;

		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[0], W[0]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[1], W[1]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[2], W[2]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[3], W[3]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[4], W[4]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[5], W[5]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[6], W[6]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[7], W[7]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[8], W[8]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[9], W[9]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[10], W[10]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[11], W[11]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[12], W[12]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[13], W[13]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[14], W[14]);
		sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[15], W[15]);

		for (i = 16; i < 80; i += 16) {
			sha512_msg_schedule_update(&W[0], W[1], W[9], W[14]);
			sha512_msg_schedule_update(&W[1], W[2], W[10], W[15]);
			sha512_msg_schedule_update(&W[2], W[3], W[11], W[0]);
			sha512_msg_schedule_update(&W[3], W[4], W[12], W[1]);
			sha512_msg_schedule_update(&W[4], W[5], W[13], W[2]);
			sha512_msg_schedule_update(&W[5], W[6], W[14], W[3]);
			sha512_msg_schedule_update(&W[6], W[7], W[15], W[4]);
			sha512_msg_schedule_update(&W[7], W[8], W[0], W[5]);
			sha512_msg_schedule_update(&W[8], W[9], W[1], W[6]);
			sha512_msg_schedule_update(&W[9], W[10], W[2], W[7]);
			sha512_msg_schedule_update(&W[10], W[11], W[3], W[8]);
			sha512_msg_schedule_update(&W[11], W[12], W[4], W[9]);
			sha512_msg_schedule_update(&W[12], W[13], W[5], W[10]);
			sha512_msg_schedule_update(&W[13], W[14], W[6], W[11]);
			sha512_msg_schedule_update(&W[14], W[15], W[7], W[12]);
			sha512_msg_schedule_update(&W[15], W[0], W[8], W[13]);

			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 0], W[0]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 1], W[1]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 2], W[2]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 3], W[3]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 4], W[4]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 5], W[5]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 6], W[6]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 7], W[7]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 8], W[8]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 9], W[9]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 10], W[10]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 11], W[11]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 12], W[12]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 13], W[13]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 14], W[14]);
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i + 15], W[15]);
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

#ifdef SHA512_SMALL
void
__sha512_block(uint64_t state[8], const uint8_t *in, size_t num)
{
	const uint64_t *in64;
	uint64_t a, b, c, d, e, f, g, h;
	uint64_t W[16];
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

		for (i = 0; i < 80; i++) {
			if (i < 16) {
				W[i] = crypto_load_be64toh(&in[i * 8]);
			} else {
				sha512_msg_schedule_update(&W[i % 16], W[(i + 1) % 16],
				    W[(i + 9) % 16], W[(i + 14) % 16]);
			}
			sha512_round(&a, &b, &c, &d, &e, &f, &g, &h, K512[i], W[i % 16]);
		}

		in += SHA512_BLOCK_LENGTH;

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

#ifndef HAVE_SHA512_BLOCK
void
__sha512_block(uint64_t state[8], const uint8_t *in, size_t num)
{
	__sha512_block_generic(state, in, num);
}
#endif
#endif

void
SHA512Init(SHA2_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.5. */
	ctx->state.st64[0] = UINT64_C(0x6a09e667f3bcc908);
	ctx->state.st64[1] = UINT64_C(0xbb67ae8584caa73b);
	ctx->state.st64[2] = UINT64_C(0x3c6ef372fe94f82b);
	ctx->state.st64[3] = UINT64_C(0xa54ff53a5f1d36f1);
	ctx->state.st64[4] = UINT64_C(0x510e527fade682d1);
	ctx->state.st64[5] = UINT64_C(0x9b05688c2b3e6c1f);
	ctx->state.st64[6] = UINT64_C(0x1f83d9abfb41bd6b);
	ctx->state.st64[7] = UINT64_C(0x5be0cd19137e2179);
}
DEF_WEAK(SHA512Init);

void
SHA512Transform(uint64_t state[8], const uint8_t data[SHA512_BLOCK_LENGTH])
{
	__sha512_block(state, data, 1);
}
DEF_WEAK(SHA512Transform);

void
SHA512Update(SHA2_CTX *ctx, const uint8_t *data, size_t len)
{
	size_t blocks, m, n;
	uint64_t bits;

	if (len == 0)
		return;

	n = (ctx->bitcount[0] >> 3) % SHA512_BLOCK_LENGTH;
	bits = (uint64_t)len << 3;
	ctx->bitcount[0] += bits;
	if (ctx->bitcount[0] < bits)
		ctx->bitcount[1]++;

	if (n > 0) {
		if ((m = SHA512_BLOCK_LENGTH - n) > len)
			m = len;

		memcpy(&ctx->buffer[n], data, m);
		data += m;
		len -= m;

		if (n + m == SHA512_BLOCK_LENGTH) {
			__sha512_block(ctx->state.st64, ctx->buffer, 1);
			memset(ctx->buffer, 0, sizeof(ctx->buffer));
		}
	}

	if (len >= SHA512_BLOCK_LENGTH) {
		blocks = len / SHA512_BLOCK_LENGTH;
		__sha512_block(ctx->state.st64, data, blocks);
		data += blocks * SHA512_BLOCK_LENGTH;
		len -= blocks * SHA512_BLOCK_LENGTH;
	}

	if (len > 0)
		memcpy(ctx->buffer, data, len);
}
DEF_WEAK(SHA512Update);

void
SHA512Pad(SHA2_CTX *ctx)
{
	size_t n;

	n = (ctx->bitcount[0] >> 3) % SHA512_BLOCK_LENGTH;
	ctx->buffer[n++] = 0x80;

	if ((SHA512_BLOCK_LENGTH - n) < 16) {
		__sha512_block(ctx->state.st64, ctx->buffer, 1);
		memset(ctx->buffer, 0, sizeof(ctx->buffer));
	}

	crypto_store_htobe64(&ctx->buffer[SHA512_BLOCK_LENGTH - 16],
	    ctx->bitcount[1]);
	crypto_store_htobe64(&ctx->buffer[SHA512_BLOCK_LENGTH - 8],
	    ctx->bitcount[0]);

	__sha512_block(ctx->state.st64, ctx->buffer, 1);
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
	ctx->bitcount[1] = 0;
	ctx->bitcount[0] = 0;
}
DEF_WEAK(SHA512Pad);

void
SHA512Final(uint8_t digest[SHA512_DIGEST_LENGTH], SHA2_CTX *ctx)
{
	int i;

	SHA512Pad(ctx);

	for (i = 0; i < SHA512_DIGEST_LENGTH / 8; i++)
		crypto_store_htobe64(&digest[i * 8], ctx->state.st64[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA512Final);

#ifndef SHA512_SMALL
void
SHA384Init(SHA2_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.4. */
	ctx->state.st64[0] = UINT64_C(0xcbbb9d5dc1059ed8);
	ctx->state.st64[1] = UINT64_C(0x629a292a367cd507);
	ctx->state.st64[2] = UINT64_C(0x9159015a3070dd17);
	ctx->state.st64[3] = UINT64_C(0x152fecd8f70e5939);
	ctx->state.st64[4] = UINT64_C(0x67332667ffc00b31);
	ctx->state.st64[5] = UINT64_C(0x8eb44a8768581511);
	ctx->state.st64[6] = UINT64_C(0xdb0c2e0d64f98fa7);
	ctx->state.st64[7] = UINT64_C(0x47b5481dbefa4fa4);
}
DEF_WEAK(SHA384Init);

MAKE_CLONE(SHA384Transform, SHA512Transform);
MAKE_CLONE(SHA384Update, SHA512Update);
MAKE_CLONE(SHA384Pad, SHA512Pad);
DEF_WEAK(SHA384Transform);
DEF_WEAK(SHA384Update);
DEF_WEAK(SHA384Pad);

void
SHA384Final(uint8_t digest[SHA384_DIGEST_LENGTH], SHA2_CTX *ctx)
{
	int i;

	SHA512Pad(ctx);

	for (i = 0; i < SHA384_DIGEST_LENGTH / 8; i++)
		crypto_store_htobe64(&digest[i * 8], ctx->state.st64[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA384Final);

void
SHA512_256Init(SHA2_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* FIPS 180-4 section 5.3.6.2. */
	ctx->state.st64[0] = UINT64_C(0x22312194fc2bf72c);
	ctx->state.st64[1] = UINT64_C(0x9f555fa3c84c64c2);
	ctx->state.st64[2] = UINT64_C(0x2393b86b6f53b151);
	ctx->state.st64[3] = UINT64_C(0x963877195940eabd);
	ctx->state.st64[4] = UINT64_C(0x96283ee2a88effe3);
	ctx->state.st64[5] = UINT64_C(0xbe5e1e2553863992);
	ctx->state.st64[6] = UINT64_C(0x2b0199fc2c85b8aa);
	ctx->state.st64[7] = UINT64_C(0x0eb72ddc81c52ca2);
}
DEF_WEAK(SHA512_256Init);

MAKE_CLONE(SHA512_256Transform, SHA512Transform);
MAKE_CLONE(SHA512_256Update, SHA512Update);
MAKE_CLONE(SHA512_256Pad, SHA512Pad);
DEF_WEAK(SHA512_256Transform);
DEF_WEAK(SHA512_256Update);
DEF_WEAK(SHA512_256Pad);

void
SHA512_256Final(uint8_t digest[SHA512_256_DIGEST_LENGTH], SHA2_CTX *ctx)
{
	int i;

	SHA512Pad(ctx);

	for (i = 0; i < SHA512_256_DIGEST_LENGTH / 8; i++)
		crypto_store_htobe64(&digest[i * 8], ctx->state.st64[i]);

	explicit_bzero(ctx, sizeof(*ctx));
}
DEF_WEAK(SHA512_256Final);
#endif
