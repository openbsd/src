/*	$OpenBSD: sha3.c,v 1.21 2026/07/22 14:34:38 jsing Exp $	*/
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Markku-Juhani O. Saarinen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <endian.h>
#include <string.h>

#include "crypto_internal.h"
#include "sha3_internal.h"

#define KECCAKF_ROUNDS 24

static const uint64_t sha3_keccakf_rndc[24] = {
	0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
	0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
	0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
	0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
	0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
	0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
	0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
	0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

static void
sha3_keccakf(uint64_t st[25])
{
	uint64_t bc0, bc1, bc2, bc3, bc4;
	uint64_t d0, d1, d2, d3, d4;
	int i, r;

	for (i = 0; i < 25; i++)
		st[i] = le64toh(st[i]);

	/*
	 * Optimized Keccak algorithm from
	 * KeccakReferenceAndOptimized/Sources/Keccak-inplace.c contained in
	 * https://keccak.team/obsolete/KeccakReferenceAndOptimized-3.2.zip
	 */

	for (r = 0; r < KECCAKF_ROUNDS; r += 4) {
		/*
		 * Round 1
		 */
		bc0 = st[0] ^ st[5] ^ st[10] ^ st[15] ^ st[20];
		bc1 = st[1] ^ st[6] ^ st[11] ^ st[16] ^ st[21];
		bc2 = st[2] ^ st[7] ^ st[12] ^ st[17] ^ st[22];
		bc3 = st[3] ^ st[8] ^ st[13] ^ st[18] ^ st[23];
		bc4 = st[4] ^ st[9] ^ st[14] ^ st[19] ^ st[24];
		d0 = bc4 ^ crypto_rol_u64(bc1, 1);
		d1 = bc0 ^ crypto_rol_u64(bc2, 1);
		d2 = bc1 ^ crypto_rol_u64(bc3, 1);
		d3 = bc2 ^ crypto_rol_u64(bc4, 1);
		d4 = bc3 ^ crypto_rol_u64(bc0, 1);

		bc0 = st[0] ^ d0;
		bc1 = crypto_rol_u64(st[6] ^ d1, 44);
		bc2 = crypto_rol_u64(st[12] ^ d2, 43);
		bc3 = crypto_rol_u64(st[18] ^ d3, 21);
		bc4 = crypto_rol_u64(st[24] ^ d4, 14);
		st[0] = bc0 ^ (~bc1 & bc2) ^ sha3_keccakf_rndc[r + 0];
		st[6] = bc1 ^ (~bc2 & bc3);
		st[12] = bc2 ^ (~bc3 & bc4);
		st[18] = bc3 ^ (~bc4 & bc0);
		st[24] = bc4 ^ (~bc0 & bc1);

		bc2 = crypto_rol_u64(st[10] ^ d0, 3);
		bc3 = crypto_rol_u64(st[16] ^ d1, 45);
		bc4 = crypto_rol_u64(st[22] ^ d2, 61);
		bc0 = crypto_rol_u64(st[3] ^ d3, 28);
		bc1 = crypto_rol_u64(st[9] ^ d4, 20);
		st[10] = bc0 ^ (~bc1 & bc2);
		st[16] = bc1 ^ (~bc2 & bc3);
		st[22] = bc2 ^ (~bc3 & bc4);
		st[3] = bc3 ^ (~bc4 & bc0);
		st[9] = bc4 ^ (~bc0 & bc1);

		bc4 = crypto_rol_u64(st[20] ^ d0, 18);
		bc0 = crypto_rol_u64(st[1] ^ d1, 1);
		bc1 = crypto_rol_u64(st[7] ^ d2, 6);
		bc2 = crypto_rol_u64(st[13] ^ d3, 25);
		bc3 = crypto_rol_u64(st[19] ^ d4, 8);
		st[20] = bc0 ^ (~bc1 & bc2);
		st[1] = bc1 ^ (~bc2 & bc3);
		st[7] = bc2 ^ (~bc3 & bc4);
		st[13] = bc3 ^ (~bc4 & bc0);
		st[19] = bc4 ^ (~bc0 & bc1);

		bc1 = crypto_rol_u64(st[5] ^ d0, 36);
		bc2 = crypto_rol_u64(st[11] ^ d1, 10);
		bc3 = crypto_rol_u64(st[17] ^ d2, 15);
		bc4 = crypto_rol_u64(st[23] ^ d3, 56);
		bc0 = crypto_rol_u64(st[4] ^ d4, 27);
		st[5] = bc0 ^ (~bc1 & bc2);
		st[11] = bc1 ^ (~bc2 & bc3);
		st[17] = bc2 ^ (~bc3 & bc4);
		st[23] = bc3 ^ (~bc4 & bc0);
		st[4] = bc4 ^ (~bc0 & bc1);

		bc3 = crypto_rol_u64(st[15] ^ d0, 41);
		bc4 = crypto_rol_u64(st[21] ^ d1, 2);
		bc0 = crypto_rol_u64(st[2] ^ d2, 62);
		bc1 = crypto_rol_u64(st[8] ^ d3, 55);
		bc2 = crypto_rol_u64(st[14] ^ d4, 39);
		st[15] = bc0 ^ (~bc1 & bc2);
		st[21] = bc1 ^ (~bc2 & bc3);
		st[2] = bc2 ^ (~bc3 & bc4);
		st[8] = bc3 ^ (~bc4 & bc0);
		st[14] = bc4 ^ (~bc0 & bc1);

		/*
		 * Round 2
		 */
		bc0 = st[0] ^ st[5] ^ st[10] ^ st[15] ^ st[20];
		bc1 = st[1] ^ st[6] ^ st[11] ^ st[16] ^ st[21];
		bc2 = st[2] ^ st[7] ^ st[12] ^ st[17] ^ st[22];
		bc3 = st[3] ^ st[8] ^ st[13] ^ st[18] ^ st[23];
		bc4 = st[4] ^ st[9] ^ st[14] ^ st[19] ^ st[24];
		d0 = bc4 ^ crypto_rol_u64(bc1, 1);
		d1 = bc0 ^ crypto_rol_u64(bc2, 1);
		d2 = bc1 ^ crypto_rol_u64(bc3, 1);
		d3 = bc2 ^ crypto_rol_u64(bc4, 1);
		d4 = bc3 ^ crypto_rol_u64(bc0, 1);

		bc0 = st[0] ^ d0;
		bc1 = crypto_rol_u64(st[16] ^ d1, 44);
		bc2 = crypto_rol_u64(st[7] ^ d2, 43);
		bc3 = crypto_rol_u64(st[23] ^ d3, 21);
		bc4 = crypto_rol_u64(st[14] ^ d4, 14);
		st[0] = bc0 ^ (~bc1 & bc2) ^ sha3_keccakf_rndc[r + 1];
		st[16] = bc1 ^ (~bc2 & bc3);
		st[7] = bc2 ^ (~bc3 & bc4);
		st[23] = bc3 ^ (~bc4 & bc0);
		st[14] = bc4 ^ (~bc0 & bc1);

		bc2 = crypto_rol_u64(st[20] ^ d0, 3);
		bc3 = crypto_rol_u64(st[11] ^ d1, 45);
		bc4 = crypto_rol_u64(st[2] ^ d2, 61);
		bc0 = crypto_rol_u64(st[18] ^ d3, 28);
		bc1 = crypto_rol_u64(st[9] ^ d4, 20);
		st[20] = bc0 ^ (~bc1 & bc2);
		st[11] = bc1 ^ (~bc2 & bc3);
		st[2] = bc2 ^ (~bc3 & bc4);
		st[18] = bc3 ^ (~bc4 & bc0);
		st[9] = bc4 ^ (~bc0 & bc1);

		bc4 = crypto_rol_u64(st[15] ^ d0, 18);
		bc0 = crypto_rol_u64(st[6] ^ d1, 1);
		bc1 = crypto_rol_u64(st[22] ^ d2, 6);
		bc2 = crypto_rol_u64(st[13] ^ d3, 25);
		bc3 = crypto_rol_u64(st[4] ^ d4, 8);
		st[15] = bc0 ^ (~bc1 & bc2);
		st[6] = bc1 ^ (~bc2 & bc3);
		st[22] = bc2 ^ (~bc3 & bc4);
		st[13] = bc3 ^ (~bc4 & bc0);
		st[4] = bc4 ^ (~bc0 & bc1);

		bc1 = crypto_rol_u64(st[10] ^ d0, 36);
		bc2 = crypto_rol_u64(st[1] ^ d1, 10);
		bc3 = crypto_rol_u64(st[17] ^ d2, 15);
		bc4 = crypto_rol_u64(st[8] ^ d3, 56);
		bc0 = crypto_rol_u64(st[24] ^ d4, 27);
		st[10] = bc0 ^ (~bc1 & bc2);
		st[1] = bc1 ^ (~bc2 & bc3);
		st[17] = bc2 ^ (~bc3 & bc4);
		st[8] = bc3 ^ (~bc4 & bc0);
		st[24] = bc4 ^ (~bc0 & bc1);

		bc3 = crypto_rol_u64(st[5] ^ d0, 41);
		bc4 = crypto_rol_u64(st[21] ^ d1, 2);
		bc0 = crypto_rol_u64(st[12] ^ d2, 62);
		bc1 = crypto_rol_u64(st[3] ^ d3, 55);
		bc2 = crypto_rol_u64(st[19] ^ d4, 39);
		st[5] = bc0 ^ (~bc1 & bc2);
		st[21] = bc1 ^ (~bc2 & bc3);
		st[12] = bc2 ^ (~bc3 & bc4);
		st[3] = bc3 ^ (~bc4 & bc0);
		st[19] = bc4 ^ (~bc0 & bc1);

		/*
		 * Round 3
		 */
		bc0 = st[0] ^ st[5] ^ st[10] ^ st[15] ^ st[20];
		bc1 = st[1] ^ st[6] ^ st[11] ^ st[16] ^ st[21];
		bc2 = st[2] ^ st[7] ^ st[12] ^ st[17] ^ st[22];
		bc3 = st[3] ^ st[8] ^ st[13] ^ st[18] ^ st[23];
		bc4 = st[4] ^ st[9] ^ st[14] ^ st[19] ^ st[24];
		d0 = bc4 ^ crypto_rol_u64(bc1, 1);
		d1 = bc0 ^ crypto_rol_u64(bc2, 1);
		d2 = bc1 ^ crypto_rol_u64(bc3, 1);
		d3 = bc2 ^ crypto_rol_u64(bc4, 1);
		d4 = bc3 ^ crypto_rol_u64(bc0, 1);

		bc0 = st[0] ^ d0;
		bc1 = crypto_rol_u64(st[11] ^ d1, 44);
		bc2 = crypto_rol_u64(st[22] ^ d2, 43);
		bc3 = crypto_rol_u64(st[8] ^ d3, 21);
		bc4 = crypto_rol_u64(st[19] ^ d4, 14);
		st[0] = bc0 ^ (~bc1 & bc2) ^ sha3_keccakf_rndc[r + 2];
		st[11] = bc1 ^ (~bc2 & bc3);
		st[22] = bc2 ^ (~bc3 & bc4);
		st[8] = bc3 ^ (~bc4 & bc0);
		st[19] = bc4 ^ (~bc0 & bc1);

		bc2 = crypto_rol_u64(st[15] ^ d0, 3);
		bc3 = crypto_rol_u64(st[1] ^ d1, 45);
		bc4 = crypto_rol_u64(st[12] ^ d2, 61);
		bc0 = crypto_rol_u64(st[23] ^ d3, 28);
		bc1 = crypto_rol_u64(st[9] ^ d4, 20);
		st[15] = bc0 ^ (~bc1 & bc2);
		st[1] = bc1 ^ (~bc2 & bc3);
		st[12] = bc2 ^ (~bc3 & bc4);
		st[23] = bc3 ^ (~bc4 & bc0);
		st[9] = bc4 ^ (~bc0 & bc1);

		bc4 = crypto_rol_u64(st[5] ^ d0, 18);
		bc0 = crypto_rol_u64(st[16] ^ d1, 1);
		bc1 = crypto_rol_u64(st[2] ^ d2, 6);
		bc2 = crypto_rol_u64(st[13] ^ d3, 25);
		bc3 = crypto_rol_u64(st[24] ^ d4, 8);
		st[5] = bc0 ^ (~bc1 & bc2);
		st[16] = bc1 ^ (~bc2 & bc3);
		st[2] = bc2 ^ (~bc3 & bc4);
		st[13] = bc3 ^ (~bc4 & bc0);
		st[24] = bc4 ^ (~bc0 & bc1);

		bc1 = crypto_rol_u64(st[20] ^ d0, 36);
		bc2 = crypto_rol_u64(st[6] ^ d1, 10);
		bc3 = crypto_rol_u64(st[17] ^ d2, 15);
		bc4 = crypto_rol_u64(st[3] ^ d3, 56);
		bc0 = crypto_rol_u64(st[14] ^ d4, 27);
		st[20] = bc0 ^ (~bc1 & bc2);
		st[6] = bc1 ^ (~bc2 & bc3);
		st[17] = bc2 ^ (~bc3 & bc4);
		st[3] = bc3 ^ (~bc4 & bc0);
		st[14] = bc4 ^ (~bc0 & bc1);

		bc3 = crypto_rol_u64(st[10] ^ d0, 41);
		bc4 = crypto_rol_u64(st[21] ^ d1, 2);
		bc0 = crypto_rol_u64(st[7] ^ d2, 62);
		bc1 = crypto_rol_u64(st[18] ^ d3, 55);
		bc2 = crypto_rol_u64(st[4] ^ d4, 39);
		st[10] = bc0 ^ (~bc1 & bc2);
		st[21] = bc1 ^ (~bc2 & bc3);
		st[7] = bc2 ^ (~bc3 & bc4);
		st[18] = bc3 ^ (~bc4 & bc0);
		st[4] = bc4 ^ (~bc0 & bc1);

		/*
		 * Round 4
		 */
		bc0 = st[0] ^ st[5] ^ st[10] ^ st[15] ^ st[20];
		bc1 = st[1] ^ st[6] ^ st[11] ^ st[16] ^ st[21];
		bc2 = st[2] ^ st[7] ^ st[12] ^ st[17] ^ st[22];
		bc3 = st[3] ^ st[8] ^ st[13] ^ st[18] ^ st[23];
		bc4 = st[4] ^ st[9] ^ st[14] ^ st[19] ^ st[24];
		d0 = bc4 ^ crypto_rol_u64(bc1, 1);
		d1 = bc0 ^ crypto_rol_u64(bc2, 1);
		d2 = bc1 ^ crypto_rol_u64(bc3, 1);
		d3 = bc2 ^ crypto_rol_u64(bc4, 1);
		d4 = bc3 ^ crypto_rol_u64(bc0, 1);

		bc0 = st[0] ^ d0;
		bc1 = crypto_rol_u64(st[1] ^ d1, 44);
		bc2 = crypto_rol_u64(st[2] ^ d2, 43);
		bc3 = crypto_rol_u64(st[3] ^ d3, 21);
		bc4 = crypto_rol_u64(st[4] ^ d4, 14);
		st[0] = bc0 ^ (~bc1 & bc2) ^ sha3_keccakf_rndc[r + 3];
		st[1] = bc1 ^ (~bc2 & bc3);
		st[2] = bc2 ^ (~bc3 & bc4);
		st[3] = bc3 ^ (~bc4 & bc0);
		st[4] = bc4 ^ (~bc0 & bc1);

		bc2 = crypto_rol_u64(st[5] ^ d0, 3);
		bc3 = crypto_rol_u64(st[6] ^ d1, 45);
		bc4 = crypto_rol_u64(st[7] ^ d2, 61);
		bc0 = crypto_rol_u64(st[8] ^ d3, 28);
		bc1 = crypto_rol_u64(st[9] ^ d4, 20);
		st[5] = bc0 ^ (~bc1 & bc2);
		st[6] = bc1 ^ (~bc2 & bc3);
		st[7] = bc2 ^ (~bc3 & bc4);
		st[8] = bc3 ^ (~bc4 & bc0);
		st[9] = bc4 ^ (~bc0 & bc1);

		bc4 = crypto_rol_u64(st[10] ^ d0, 18);
		bc0 = crypto_rol_u64(st[11] ^ d1, 1);
		bc1 = crypto_rol_u64(st[12] ^ d2, 6);
		bc2 = crypto_rol_u64(st[13] ^ d3, 25);
		bc3 = crypto_rol_u64(st[14] ^ d4, 8);
		st[10] = bc0 ^ (~bc1 & bc2);
		st[11] = bc1 ^ (~bc2 & bc3);
		st[12] = bc2 ^ (~bc3 & bc4);
		st[13] = bc3 ^ (~bc4 & bc0);
		st[14] = bc4 ^ (~bc0 & bc1);

		bc1 = crypto_rol_u64(st[15] ^ d0, 36);
		bc2 = crypto_rol_u64(st[16] ^ d1, 10);
		bc3 = crypto_rol_u64(st[17] ^ d2, 15);
		bc4 = crypto_rol_u64(st[18] ^ d3, 56);
		bc0 = crypto_rol_u64(st[19] ^ d4, 27);
		st[15] = bc0 ^ (~bc1 & bc2);
		st[16] = bc1 ^ (~bc2 & bc3);
		st[17] = bc2 ^ (~bc3 & bc4);
		st[18] = bc3 ^ (~bc4 & bc0);
		st[19] = bc4 ^ (~bc0 & bc1);

		bc3 = crypto_rol_u64(st[20] ^ d0, 41);
		bc4 = crypto_rol_u64(st[21] ^ d1, 2);
		bc0 = crypto_rol_u64(st[22] ^ d2, 62);
		bc1 = crypto_rol_u64(st[23] ^ d3, 55);
		bc2 = crypto_rol_u64(st[24] ^ d4, 39);
		st[20] = bc0 ^ (~bc1 & bc2);
		st[21] = bc1 ^ (~bc2 & bc3);
		st[22] = bc2 ^ (~bc3 & bc4);
		st[23] = bc3 ^ (~bc4 & bc0);
		st[24] = bc4 ^ (~bc0 & bc1);
	}

	for (i = 0; i < 25; i++)
		st[i] = htole64(st[i]);
}

int
sha3_init(sha3_ctx *ctx, int mdlen)
{
	if (mdlen < 0 || mdlen >= KECCAK_BYTE_WIDTH / 2)
		return 0;

	memset(ctx, 0, sizeof(*ctx));

	ctx->mdlen = mdlen;
	ctx->rsize = KECCAK_BYTE_WIDTH - 2 * mdlen;

	return 1;
}

int
sha3_update(sha3_ctx *ctx, const void *_data, size_t len)
{
	const uint8_t *data = _data;
	size_t i, j;

	j = ctx->pt;
	for (i = 0; i < len; i++) {
		ctx->state.b[j++] ^= data[i];
		if (j >= ctx->rsize) {
			sha3_keccakf(ctx->state.q);
			j = 0;
		}
	}
	ctx->pt = j;

	return 1;
}

int
sha3_final(void *_md, sha3_ctx *ctx)
{
	uint8_t *md = _md;
	int i;

	ctx->state.b[ctx->pt] ^= 0x06;
	ctx->state.b[ctx->rsize - 1] ^= 0x80;
	sha3_keccakf(ctx->state.q);

	for (i = 0; i < ctx->mdlen; i++)
		md[i] = ctx->state.b[i];

	return 1;
}

/* SHAKE128 and SHAKE256 extensible-output functionality. */
void
shake_xof(sha3_ctx *ctx)
{
	ctx->state.b[ctx->pt] ^= 0x1f;
	ctx->state.b[ctx->rsize - 1] ^= 0x80;
	sha3_keccakf(ctx->state.q);
	ctx->pt = 0;
}

void
shake_out(sha3_ctx *ctx, void *_out, size_t len)
{
	uint8_t *out = _out;
	size_t i, j;

	j = ctx->pt;
	for (i = 0; i < len; i++) {
		if (j >= ctx->rsize) {
			sha3_keccakf(ctx->state.q);
			j = 0;
		}
		out[i] = ctx->state.b[j++];
	}
	ctx->pt = j;
}
