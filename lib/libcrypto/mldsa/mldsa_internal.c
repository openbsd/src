/*	$OpenBSD$ */
/* Copyright 2014 The BoringSSL Authors
 * Copyright (c) 2026 Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/mldsa.h>

#include "bytestring.h"
#include "sha3_internal.h"
#include "mldsa_internal.h"
#include "constant_time.h"
#include "crypto_internal.h"

/*
 * ML-DSA, as specified in FIPS 204:
 * https://csrc.nist.gov/pubs/fips/204/final
 *
 * The strength parameter K of the BoringSSL C++ (6 for ML-DSA-65, 8 for
 * ML-DSA-87) is a runtime |rank| argument here, with the companion dimension
 * L derived from it via |mldsa_l|.
 */

#define DEGREE 256

#define kRhoBytes 32
#define kSigmaBytes 64
#define kKBytes 32
#define kTrBytes 64
#define kMuBytes 64
#define kRhoPrimeBytes 64

/* 2^23 - 2^13 + 1 */
static const uint32_t kPrime = 8380417;
/* Inverse of -kPrime modulo 2^32 */
static const uint32_t kPrimeNegInverse = 4236238847;
static const int kDroppedBits = 13;
static const uint32_t kHalfPrime = (/*kPrime=*/8380417 - 1) / 2;
static const uint32_t kGamma2 = (/*kPrime=*/8380417 - 1) / 32;
/* 256^-1 mod kPrime, in Montgomery form. */
static const uint32_t kInverseDegreeMontgomery = 41978;

/*
 * The strength-dependent parameters. In the C++ these were templates
 * parameterised by K; here they are functions of the runtime |rank| (k). They
 * are prefixed |mldsa_| so their short spec names (eta, tau, beta, ...) remain
 * available as local variables without shadowing (the tree builds -Wshadow).
 */

/* The l parameter (number of columns) for a given rank (k). */
static int
mldsa_l(int rank)
{
	if (rank == MLDSA65_RANK)
		return 5;
	return 7;
}

static size_t
mldsa_public_key_bytes(int rank)
{
	if (rank == MLDSA65_RANK)
		return MLDSA65_PUBLIC_KEY_BYTES;
	return MLDSA87_PUBLIC_KEY_BYTES;
}

static size_t
mldsa_signature_bytes(int rank)
{
	if (rank == MLDSA65_RANK)
		return MLDSA65_SIGNATURE_BYTES;
	return MLDSA87_SIGNATURE_BYTES;
}

static size_t
mldsa_private_key_bytes(int rank)
{
	if (rank == MLDSA65_RANK)
		return MLDSA65_PRIVATE_KEY_BYTES;
	return MLDSA87_PRIVATE_KEY_BYTES;
}

static int
mldsa_tau(int rank)
{
	if (rank == MLDSA65_RANK)
		return 49;
	return 60;
}

static int
mldsa_lambda_bytes(int rank)
{
	if (rank == MLDSA65_RANK)
		return 192 / 8;
	return 256 / 8;
}

static int
mldsa_gamma1(void)
{
	return 1 << 19;
}

static int
mldsa_beta(int rank)
{
	if (rank == MLDSA65_RANK)
		return 196;
	return 120;
}

static int
mldsa_omega(int rank)
{
	if (rank == MLDSA65_RANK)
		return 55;
	return 75;
}

static int
mldsa_eta(int rank)
{
	if (rank == MLDSA65_RANK)
		return 4;
	return 2;
}

static int
mldsa_plus_minus_eta_bitlen(int rank)
{
	if (rank == MLDSA65_RANK)
		return 4;
	return 3;
}

/* Fundamental types. */

typedef struct scalar {
	uint32_t c[DEGREE];
} scalar;

/*
 * Arithmetic.
 *
 * This bit of Python will be referenced in some of the following comments:
 *
 * q = 8380417
 * # Inverse of -q modulo 2^32
 * q_neg_inverse = 4236238847
 * # 2^64 modulo q
 * montgomery_square = 2365951
 *
 * def bitreverse(i):
 *     ret = 0
 *     for n in range(8):
 *         bit = i & 1
 *         ret <<= 1
 *         ret |= bit
 *         i >>= 1
 *     return ret
 *
 * def montgomery_reduce(x):
 *     a = (x * q_neg_inverse) % 2**32
 *     b = x + a * q
 *     assert b & 0xFFFF_FFFF == 0
 *     c = b >> 32
 *     assert c < q
 *     return c
 *
 * def montgomery_transform(x):
 *     return montgomery_reduce(x * montgomery_square)
 *
 * kNTTRootsMontgomery = [
 *   montgomery_transform(pow(1753, bitreverse(i), q)) for i in range(256)
 * ]
 */
static const uint32_t kNTTRootsMontgomery[256] = {
	4193792, 25847,   5771523, 7861508, 237124,  7602457, 7504169, 466468,
	1826347, 2353451, 8021166, 6288512, 3119733, 5495562, 3111497, 2680103,
	2725464, 1024112, 7300517, 3585928, 7830929, 7260833, 2619752, 6271868,
	6262231, 4520680, 6980856, 5102745, 1757237, 8360995, 4010497, 280005,
	2706023, 95776,   3077325, 3530437, 6718724, 4788269, 5842901, 3915439,
	4519302, 5336701, 3574422, 5512770, 3539968, 8079950, 2348700, 7841118,
	6681150, 6736599, 3505694, 4558682, 3507263, 6239768, 6779997, 3699596,
	811944,  531354,  954230,  3881043, 3900724, 5823537, 2071892, 5582638,
	4450022, 6851714, 4702672, 5339162, 6927966, 3475950, 2176455, 6795196,
	7122806, 1939314, 4296819, 7380215, 5190273, 5223087, 4747489, 126922,
	3412210, 7396998, 2147896, 2715295, 5412772, 4686924, 7969390, 5903370,
	7709315, 7151892, 8357436, 7072248, 7998430, 1349076, 1852771, 6949987,
	5037034, 264944,  508951,  3097992, 44288,   7280319, 904516,  3958618,
	4656075, 8371839, 1653064, 5130689, 2389356, 8169440, 759969,  7063561,
	189548,  4827145, 3159746, 6529015, 5971092, 8202977, 1315589, 1341330,
	1285669, 6795489, 7567685, 6940675, 5361315, 4499357, 4751448, 3839961,
	2091667, 3407706, 2316500, 3817976, 5037939, 2244091, 5933984, 4817955,
	266997,  2434439, 7144689, 3513181, 4860065, 4621053, 7183191, 5187039,
	900702,  1859098, 909542,  819034,  495491,  6767243, 8337157, 7857917,
	7725090, 5257975, 2031748, 3207046, 4823422, 7855319, 7611795, 4784579,
	342297,  286988,  5942594, 4108315, 3437287, 5038140, 1735879, 203044,
	2842341, 2691481, 5790267, 1265009, 4055324, 1247620, 2486353, 1595974,
	4613401, 1250494, 2635921, 4832145, 5386378, 1869119, 1903435, 7329447,
	7047359, 1237275, 5062207, 6950192, 7929317, 1312455, 3306115, 6417775,
	7100756, 1917081, 5834105, 7005614, 1500165, 777191,  2235880, 3406031,
	7838005, 5548557, 6709241, 6533464, 5796124, 4656147, 594136,  4603424,
	6366809, 2432395, 2454455, 8215696, 1957272, 3369112, 185531,  7173032,
	5196991, 162844,  1616392, 3014001, 810149,  1652634, 4686184, 6581310,
	5341501, 3523897, 3866901, 269760,  2213111, 7404533, 1717735, 472078,
	7953734, 1723600, 6577327, 1910376, 6712985, 7276084, 8119771, 4546524,
	5441381, 6144432, 7959518, 6094090, 183443,  7403526, 1612842, 4834730,
	7826001, 3919660, 8332111, 7018208, 3937738, 1400424, 7534263, 1976782,
};

/* Reduces x mod kPrime in constant time, where 0 <= x < 2*kPrime. */
static uint32_t
reduce_once(uint32_t x)
{
	/* return x < kPrime ? x : x - kPrime; */
	return constant_time_select_int(constant_time_lt(x, kPrime), x,
	    x - kPrime);
}

/* Returns the absolute value in constant time. */
static uint32_t
abs_signed(uint32_t x)
{
	/*
	 * return is_positive(x) ? x : -x;
	 * Note: the negation is written as a bitwise not plus one (assuming
	 * two's complement representation) to avoid applying the unary minus
	 * operator to an unsigned type.
	 */
	return constant_time_select_int(constant_time_lt(x, 0x80000000), x,
	    0u - x);
}

/* Returns the absolute value modulo kPrime. */
static uint32_t
abs_mod_prime(uint32_t x)
{
	/* return x > kHalfPrime ? kPrime - x : x; */
	return constant_time_select_int(constant_time_lt(kHalfPrime, x),
	    kPrime - x, x);
}

/* Returns the maximum of two values in constant time. */
static uint32_t
maximum(uint32_t x, uint32_t y)
{
	/* return x < y ? y : x; */
	return constant_time_select_int(constant_time_lt(x, y), y, x);
}

static uint32_t
mod_sub(uint32_t a, uint32_t b)
{
	return reduce_once(kPrime + a - b);
}

static void
scalar_add(scalar *out, const scalar *lhs, const scalar *rhs)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = reduce_once(lhs->c[i] + rhs->c[i]);
}

static void
scalar_sub(scalar *out, const scalar *lhs, const scalar *rhs)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = mod_sub(lhs->c[i], rhs->c[i]);
}

static uint32_t
reduce_montgomery(uint64_t x)
{
	uint64_t a = (uint32_t)x * kPrimeNegInverse;
	uint64_t b = x + a * kPrime;
	uint32_t c = b >> 32;

	return reduce_once(c);
}

/* Multiply two scalars in the number theoretically transformed state. */
static void
scalar_mult(scalar *out, const scalar *lhs, const scalar *rhs)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = reduce_montgomery((uint64_t)lhs->c[i] *
		    (uint64_t)rhs->c[i]);
}

/*
 * In place number theoretic transform of a given scalar.
 *
 * FIPS 204, Algorithm 41 (`NTT`).
 */
static void
scalar_ntt(scalar *s)
{
	int offset, step, i, j, k;

	/*
	 * Step: 1, 2, 4, 8, ..., 128
	 * Offset: 128, 64, 32, 16, ..., 1
	 */
	offset = DEGREE;
	for (step = 1; step < DEGREE; step <<= 1) {
		offset >>= 1;
		k = 0;
		for (i = 0; i < step; i++) {
			const uint32_t step_root =
			    kNTTRootsMontgomery[step + i];
			for (j = k; j < k + offset; j++) {
				uint32_t even, odd;

				even = s->c[j];
				/*
				 * |reduce_montgomery| works on values up to
				 * kPrime*R and R > 2*kPrime. |step_root| <
				 * kPrime because it's static data. |s->c[...]|
				 * is < kPrime by the invariants of that struct.
				 */
				odd = reduce_montgomery((uint64_t)step_root *
				    (uint64_t)s->c[j + offset]);
				s->c[j] = reduce_once(odd + even);
				s->c[j + offset] = mod_sub(even, odd);
			}
			k += 2 * offset;
		}
	}
}

/*
 * In place inverse number theoretic transform of a given scalar.
 *
 * FIPS 204, Algorithm 42 (`NTT^-1`).
 */
static void
scalar_inverse_ntt(scalar *s)
{
	int step, offset, i, j, k;

	/*
	 * Step: 128, 64, 32, 16, ..., 1
	 * Offset: 1, 2, 4, 8, ..., 128
	 */
	step = DEGREE;
	for (offset = 1; offset < DEGREE; offset <<= 1) {
		step >>= 1;
		k = 0;
		for (i = 0; i < step; i++) {
			const uint32_t step_root =
			    kPrime - kNTTRootsMontgomery[step + (step - 1 - i)];
			for (j = k; j < k + offset; j++) {
				uint32_t even, odd;

				even = s->c[j];
				odd = s->c[j + offset];
				s->c[j] = reduce_once(odd + even);

				/*
				 * |reduce_montgomery| works on values up to
				 * kPrime*R and R > 2*kPrime. kPrime + even <
				 * 2*kPrime because |even| < kPrime, by the
				 * invariants of that structure. Thus kPrime +
				 * even - odd < 2*kPrime because odd >= 0,
				 * because it's unsigned and less than kPrime.
				 * Lastly step_root < kPrime, because
				 * |kNTTRootsMontgomery| is static data.
				 */
				s->c[j + offset] = reduce_montgomery(
				    (uint64_t)step_root *
				    (uint64_t)(kPrime + even - odd));
			}
			k += 2 * offset;
		}
	}
	for (i = 0; i < DEGREE; i++)
		s->c[i] = reduce_montgomery((uint64_t)s->c[i] *
		    (uint64_t)kInverseDegreeMontgomery);
}

/* Rounding & hints. */

/* FIPS 204, Algorithm 35 (`Power2Round`). */
static void
power2_round(uint32_t *r1, uint32_t *r0, uint32_t r)
{
	uint32_t mask, r0_adjusted, r1_adjusted;

	*r1 = r >> kDroppedBits;
	*r0 = r - (*r1 << kDroppedBits);

	r0_adjusted = mod_sub(*r0, 1 << kDroppedBits);
	r1_adjusted = *r1 + 1;

	/* Mask is set iff r0 > 2^(dropped_bits - 1). */
	mask = constant_time_lt((uint32_t)(1 << (kDroppedBits - 1)), *r0);
	/* r0 = mask ? r0_adjusted : r0 */
	*r0 = constant_time_select_int(mask, r0_adjusted, *r0);
	/* r1 = mask ? r1_adjusted : r1 */
	*r1 = constant_time_select_int(mask, r1_adjusted, *r1);
}

/* Scale back previously rounded value. */
static void
scale_power2_round(uint32_t *out, uint32_t r1)
{
	/* Pre-condition: 0 <= r1 <= 2^10 - 1 */
	assert(r1 < (1u << 10));

	*out = r1 << kDroppedBits;

	/* Post-condition: 0 <= out <= 2^23 - 2^13 = kPrime - 1 */
	assert(*out < kPrime);
}

/* FIPS 204, Algorithm 37 (`HighBits`). */
static uint32_t
high_bits(uint32_t x)
{
	uint32_t r1;

	/*
	 * Reference description (given 0 <= x < q):
	 *
	 *   int32_t r0 = x mod+- (2 * kGamma2);
	 *   if (x - r0 == q - 1)
	 *           return 0;
	 *   else
	 *           return (x - r0) / (2 * kGamma2);
	 *
	 * Below is the formula taken from the reference implementation.
	 *
	 * Here, kGamma2 == 2^18 - 2^8
	 * This returns ((ceil(x / 2^7) * (2^10 + 1) + 2^21) / 2^22) mod 2^4
	 */
	r1 = (x + 127) >> 7;
	r1 = (r1 * 1025 + (1 << 21)) >> 22;
	r1 &= 15;
	return r1;
}

/* FIPS 204, Algorithm 36 (`Decompose`). */
static void
decompose(uint32_t *r1, int32_t *r0, uint32_t r)
{
	*r1 = high_bits(r);

	*r0 = r;
	*r0 -= *r1 * 2 * (int32_t)kGamma2;
	*r0 -= (((int32_t)kHalfPrime - *r0) >> 31) & (int32_t)kPrime;
}

/* FIPS 204, Algorithm 38 (`LowBits`). */
static int32_t
low_bits(uint32_t x)
{
	uint32_t r1;
	int32_t r0;

	decompose(&r1, &r0, x);
	return r0;
}

/*
 * FIPS 204, Algorithm 39 (`MakeHint`).
 *
 * In the spec this takes two arguments, z and r, and is called with
 *   z = -ct0
 *   r = w - cs2 + ct0
 *
 * It then computes HighBits (algorithm 37) of z and z+r. But z+r is just w -
 * cs2, so this takes three arguments and saves an addition.
 */
static int32_t
make_hint(uint32_t ct0, uint32_t cs2, uint32_t w)
{
	uint32_t r, r_plus_z;

	r_plus_z = mod_sub(w, cs2);
	r = reduce_once(r_plus_z + ct0);
	return high_bits(r) != high_bits(r_plus_z);
}

/* FIPS 204, Algorithm 40 (`UseHint`). */
static uint32_t
use_hint_vartime(uint32_t h, uint32_t r)
{
	uint32_t r1;
	int32_t r0;

	decompose(&r1, &r0, r);

	if (h) {
		/* m = 16, thus |mod m| in the spec turns into |& 15|. */
		if (r0 > 0)
			return (r1 + 1) & 15;
		else
			return (r1 - 1) & 15;
	}
	return r1;
}

static void
scalar_power2_round(scalar *s1, scalar *s0, const scalar *s)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		power2_round(&s1->c[i], &s0->c[i], s->c[i]);
}

static void
scalar_scale_power2_round(scalar *out, const scalar *in)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		scale_power2_round(&out->c[i], in->c[i]);
}

static void
scalar_high_bits(scalar *out, const scalar *in)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = high_bits(in->c[i]);
}

static void
scalar_low_bits(scalar *out, const scalar *in)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = low_bits(in->c[i]);
}

static void
scalar_max(uint32_t *max, const scalar *s)
{
	uint32_t abs;
	int i;

	for (i = 0; i < DEGREE; i++) {
		abs = abs_mod_prime(s->c[i]);
		*max = maximum(*max, abs);
	}
}

static void
scalar_max_signed(uint32_t *max, const scalar *s)
{
	uint32_t abs;
	int i;

	for (i = 0; i < DEGREE; i++) {
		abs = abs_signed(s->c[i]);
		*max = maximum(*max, abs);
	}
}

static void
scalar_make_hint(scalar *out, const scalar *ct0, const scalar *cs2,
    const scalar *w)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = make_hint(ct0->c[i], cs2->c[i], w->c[i]);
}

static void
scalar_use_hint_vartime(scalar *out, const scalar *h, const scalar *r)
{
	int i;

	for (i = 0; i < DEGREE; i++)
		out->c[i] = use_hint_vartime(h->c[i], r->c[i]);
}

/*
 * Bit packing.
 *
 * These are generic byte-oriented, LSB-first serialisers, so the encoded format
 * is independent of host endianness. |scalar_encode|/|scalar_decode| implement
 * SimpleBitPack and SimpleBitUnpack (unsigned); |scalar_encode_signed|/
 * |scalar_decode_signed| implement BitPack and BitUnpack, storing
 * |max| - coefficient.
 */

static const uint8_t kMasks[8] = {
	0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff,
};

/* FIPS 204, Algorithm 16 (`SimpleBitPack`). */
static void
scalar_encode(uint8_t *out, const scalar *s, int bits)
{
	uint8_t out_byte = 0;
	int i, out_byte_bits = 0;

	assert(bits <= (int)sizeof(*s->c) * 8);
	for (i = 0; i < DEGREE; i++) {
		uint32_t element = s->c[i];
		int element_bits_done = 0;

		while (element_bits_done < bits) {
			int chunk_bits = bits - element_bits_done;
			int out_bits_remaining = 8 - out_byte_bits;

			if (chunk_bits >= out_bits_remaining) {
				chunk_bits = out_bits_remaining;
				out_byte |= (element &
				    kMasks[chunk_bits - 1]) << out_byte_bits;
				*out = out_byte;
				out++;
				out_byte_bits = 0;
				out_byte = 0;
			} else {
				out_byte |= (element &
				    kMasks[chunk_bits - 1]) << out_byte_bits;
				out_byte_bits += chunk_bits;
			}

			element_bits_done += chunk_bits;
			element >>= chunk_bits;
		}
	}

	if (out_byte_bits > 0)
		*out = out_byte;
}

/* FIPS 204, Algorithm 17 (`BitPack`), storing |max| - coefficient. */
static void
scalar_encode_signed(uint8_t *out, const scalar *s, int bits, uint32_t max)
{
	uint8_t out_byte = 0;
	int i, out_byte_bits = 0;

	assert(bits <= (int)sizeof(*s->c) * 8);
	for (i = 0; i < DEGREE; i++) {
		uint32_t element = mod_sub(max, s->c[i]);
		int element_bits_done = 0;

		while (element_bits_done < bits) {
			int chunk_bits = bits - element_bits_done;
			int out_bits_remaining = 8 - out_byte_bits;

			if (chunk_bits >= out_bits_remaining) {
				chunk_bits = out_bits_remaining;
				out_byte |= (element &
				    kMasks[chunk_bits - 1]) << out_byte_bits;
				*out = out_byte;
				out++;
				out_byte_bits = 0;
				out_byte = 0;
			} else {
				out_byte |= (element &
				    kMasks[chunk_bits - 1]) << out_byte_bits;
				out_byte_bits += chunk_bits;
			}

			element_bits_done += chunk_bits;
			element >>= chunk_bits;
		}
	}

	if (out_byte_bits > 0)
		*out = out_byte;
}

/* FIPS 204, Algorithm 18 (`SimpleBitUnpack`). */
static void
scalar_decode(scalar *out, const uint8_t *in, int bits)
{
	uint8_t in_byte = 0;
	int i, in_byte_bits_left = 0;

	assert(bits <= (int)sizeof(*out->c) * 8);
	for (i = 0; i < DEGREE; i++) {
		uint32_t element = 0;
		int element_bits_done = 0;

		while (element_bits_done < bits) {
			int chunk_bits = bits - element_bits_done;

			if (in_byte_bits_left == 0) {
				in_byte = *in;
				in++;
				in_byte_bits_left = 8;
			}

			if (chunk_bits > in_byte_bits_left)
				chunk_bits = in_byte_bits_left;

			element |= (uint32_t)(in_byte & kMasks[chunk_bits - 1]) <<
			    element_bits_done;
			in_byte_bits_left -= chunk_bits;
			in_byte >>= chunk_bits;

			element_bits_done += chunk_bits;
		}

		out->c[i] = element;
	}
}

/*
 * FIPS 204, Algorithm 19 (`BitUnpack`), recovering the coefficient as
 * |max| - value. Returns zero if any value is out of range, i.e. greater than
 * 2 * |max| (only possible for the bounded eta cases, and only for invalid
 * input, so it is fine to leak which value failed).
 */
static int
scalar_decode_signed(scalar *out, const uint8_t *in, int bits, uint32_t max)
{
	uint8_t in_byte = 0;
	int i, in_byte_bits_left = 0;

	assert(bits <= (int)sizeof(*out->c) * 8);
	for (i = 0; i < DEGREE; i++) {
		uint32_t element = 0;
		int element_bits_done = 0;

		while (element_bits_done < bits) {
			int chunk_bits = bits - element_bits_done;

			if (in_byte_bits_left == 0) {
				in_byte = *in;
				in++;
				in_byte_bits_left = 8;
			}

			if (chunk_bits > in_byte_bits_left)
				chunk_bits = in_byte_bits_left;

			element |= (uint32_t)(in_byte & kMasks[chunk_bits - 1]) <<
			    element_bits_done;
			in_byte_bits_left -= chunk_bits;
			in_byte >>= chunk_bits;

			element_bits_done += chunk_bits;
		}

		if (element > 2 * max)
			return 0;
		out->c[i] = mod_sub(max, element);
	}

	return 1;
}

/* Expansion functions. */

/* Loads a 64 bit little-endian value from |in|. */
static uint64_t
load_le64(const uint8_t *in)
{
	return (uint64_t)crypto_load_le32toh(in) |
	    ((uint64_t)crypto_load_le32toh(in + 4) << 32);
}

/* One-shot SHAKE-256 of |in| into |out_len| bytes at |out|. */
static void
shake256(uint8_t *out, size_t out_len, const uint8_t *in, size_t in_len)
{
	sha3_ctx ctx;

	shake256_init(&ctx);
	shake_update(&ctx, in, in_len);
	shake_xof(&ctx);
	shake_out(&ctx, out, out_len);
}

/*
 * FIPS 204, Algorithm 30 (`RejNTTPoly`).
 *
 * Rejection samples a Keccak stream to get uniformly distributed elements. This
 * is used for matrix expansion and only operates on public inputs. The
 * SHAKE-128 rate (168) is a multiple of 3, so block and coefficient boundaries
 * align.
 */
static void
scalar_from_keccak_vartime(scalar *out,
    const uint8_t derived_seed[kRhoBytes + 2])
{
	sha3_ctx keccak_ctx;
	int done = 0;

	shake128_init(&keccak_ctx);
	shake_update(&keccak_ctx, derived_seed, kRhoBytes + 2);
	shake_xof(&keccak_ctx);

	while (done < DEGREE) {
		uint8_t block[168];
		size_t i;

		shake_out(&keccak_ctx, block, sizeof(block));
		for (i = 0; i < sizeof(block) && done < DEGREE; i += 3) {
			/* FIPS 204, Algorithm 14 (`CoeffFromThreeBytes`). */
			uint32_t value = (uint32_t)block[i] |
			    ((uint32_t)block[i + 1] << 8) |
			    (((uint32_t)block[i + 2] & 0x7f) << 16);

			if (value < kPrime)
				out->c[done++] = value;
		}
	}
}

/*
 * FIPS 204, Algorithm 15 (`CoefFromHalfByte`), for the supported eta values.
 * Returns one and sets |*result| if |nibble| is in range, zero otherwise.
 */
static int
coefficient_from_nibble(int eta, uint32_t nibble, uint32_t *result)
{
	if (eta == 4) {
		if (nibble < 9) {
			*result = mod_sub(4, nibble);
			return 1;
		}
		return 0;
	}
	/* eta == 2 */
	if (nibble < 15) {
	        /* Constant time nibble % 5 */
                nibble = nibble - 5 * ((205 * nibble) >> 10);
                *result = mod_sub(2, nibble);
		return 1;
	}
	return 0;
}

/* FIPS 204, Algorithm 31 (`RejBoundedPoly`). */
static void
scalar_uniform(int eta, scalar *out,
    const uint8_t derived_seed[kSigmaBytes + 2])
{
	sha3_ctx keccak_ctx;
	int done = 0;

	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, derived_seed, kSigmaBytes + 2);
	shake_xof(&keccak_ctx);

	while (done < DEGREE) {
		uint8_t block[136];
		size_t i;

		shake_out(&keccak_ctx, block, sizeof(block));
		for (i = 0; i < sizeof(block) && done < DEGREE; i++) {
			uint32_t t0 = block[i] & 0x0f;
			uint32_t t1 = block[i] >> 4;
			uint32_t v;

			/*
			 * Although both the input and output here are secret, it
			 * is OK to leak when we rejected a byte. Individual bytes
			 * of the SHAKE-256 stream are (indistinguishable from)
			 * independent of each other and the original seed, so
			 * leaking information about the rejected bytes does not
			 * reveal the input or output.
			 */
			if (coefficient_from_nibble(eta, t0, &v))
				out->c[done++] = v;
			if (done < DEGREE &&
			    coefficient_from_nibble(eta, t1, &v))
				out->c[done++] = v;
		}
	}
}

/* FIPS 204, Algorithm 34 (`ExpandMask`), but just a single step. */
static void
scalar_sample_mask(scalar *out,
    const uint8_t derived_seed[kRhoPrimeBytes + 2])
{
	uint8_t buf[640];

	shake256(buf, sizeof(buf), derived_seed, kRhoPrimeBytes + 2);

	/* Decoding 20 bits into (-2^19, 2^19] cannot fail. */
	scalar_decode_signed(out, buf, 20, 1 << 19);
}

/* FIPS 204, Algorithm 29 (`SampleInBall`). */
static void
scalar_sample_in_ball_vartime(scalar *out, const uint8_t *seed, int len,
    int tau)
{
	sha3_ctx keccak_ctx;
	uint8_t block[136];
	uint64_t signs;
	int offset;
	size_t i;

	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, seed, len);
	shake_xof(&keccak_ctx);
	shake_out(&keccak_ctx, block, sizeof(block));

	signs = load_le64(block);
	offset = 8;

	/*
	 * SampleInBall implements a Fisher-Yates shuffle, which unavoidably
	 * leaks where the zeros are by memory access pattern. Although this leak
	 * happens before bad signatures are rejected, this is safe.
	 */
	memset(out, 0, sizeof(*out));
	for (i = DEGREE - tau; i < DEGREE; i++) {
		size_t byte;

		for (;;) {
			if (offset == 136) {
				shake_out(&keccak_ctx, block, sizeof(block));
				offset = 0;
			}

			byte = block[offset++];
			if (byte <= i)
				break;
		}

		out->c[i] = out->c[byte];
		out->c[byte] = mod_sub(1, 2 * (signs & 1));
		signs >>= 1;
	}
}

static void
vector_zero(scalar *out, size_t len)
{
	memset(out, 0, sizeof(*out) * len);
}

static void
vector_add(scalar *out, const scalar *lhs, const scalar *rhs, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_add(&out[i], &lhs[i], &rhs[i]);
}

static void
vector_sub(scalar *out, const scalar *lhs, const scalar *rhs, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_sub(&out[i], &lhs[i], &rhs[i]);
}

static void
vector_mult_scalar(scalar *out, const scalar *lhs, const scalar *rhs,
    size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_mult(&out[i], &lhs[i], rhs);
}

static void
vector_ntt(scalar *a, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_ntt(&a[i]);
}

static void
vector_inverse_ntt(scalar *a, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_inverse_ntt(&a[i]);
}

/*
 * Multiplies the K*L matrix |m| by the length-L vector |a| into length-K |out|.
 */
static void
matrix_mult(scalar *out, const scalar *m, const scalar *a, size_t k, size_t l)
{
	size_t i, j;

	vector_zero(out, k);
	for (i = 0; i < k; i++) {
		for (j = 0; j < l; j++) {
			scalar product;

			scalar_mult(&product, &m[i * l + j], &a[j]);
			scalar_add(&out[i], &out[i], &product);
		}
	}
}

static void
vector_power2_round(scalar *t1, scalar *t0, const scalar *t, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_power2_round(&t1[i], &t0[i], &t[i]);
}

static void
vector_scale_power2_round(scalar *out, const scalar *in, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_scale_power2_round(&out[i], &in[i]);
}

static void
vector_high_bits(scalar *out, const scalar *in, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_high_bits(&out[i], &in[i]);
}

static void
vector_low_bits(scalar *out, const scalar *in, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_low_bits(&out[i], &in[i]);
}

static uint32_t
vector_max(const scalar *a, size_t len)
{
	uint32_t max = 0;
	size_t i;

	for (i = 0; i < len; i++)
		scalar_max(&max, &a[i]);
	return max;
}

static uint32_t
vector_max_signed(const scalar *a, size_t len)
{
	uint32_t max = 0;
	size_t i;

	for (i = 0; i < len; i++)
		scalar_max_signed(&max, &a[i]);
	return max;
}

/* The input vector contains only zeroes and ones. */
static size_t
vector_count_ones(const scalar *a, size_t len)
{
	size_t count = 0;
	size_t i, j;

	for (i = 0; i < len; i++) {
		for (j = 0; j < DEGREE; j++)
			count += a[i].c[j];
	}
	return count;
}

static void
vector_make_hint(scalar *out, const scalar *ct0, const scalar *cs2,
    const scalar *w, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_make_hint(&out[i], &ct0[i], &cs2[i], &w[i]);
}

static void
vector_use_hint_vartime(scalar *out, const scalar *h, const scalar *r,
    size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_use_hint_vartime(&out[i], &h[i], &r[i]);
}

/* FIPS 204, Algorithm 32 (`ExpandA`). Fills the K*L matrix |out|. */
static void
matrix_expand(scalar *out, const uint8_t rho[kRhoBytes], size_t k, size_t l)
{
	uint8_t derived_seed[kRhoBytes + 2];
	size_t i, j;

	memcpy(derived_seed, rho, kRhoBytes);
	for (i = 0; i < k; i++) {
		for (j = 0; j < l; j++) {
			derived_seed[kRhoBytes + 1] = (uint8_t)i;
			derived_seed[kRhoBytes] = (uint8_t)j;
			scalar_from_keccak_vartime(&out[i * l + j], derived_seed);
		}
	}
}

/* FIPS 204, Algorithm 33 (`ExpandS`). Fills length-L |s1| and length-K |s2|. */
static void
vector_expand_short(int rank, scalar *s1, scalar *s2,
    const uint8_t sigma[kSigmaBytes])
{
	uint8_t derived_seed[kSigmaBytes + 2];
	int eta, i, k, l;

	eta = mldsa_eta(rank);
	k = rank;
	l = mldsa_l(rank);

	memcpy(derived_seed, sigma, kSigmaBytes);
	derived_seed[kSigmaBytes] = 0;
	derived_seed[kSigmaBytes + 1] = 0;
	for (i = 0; i < l; i++) {
		scalar_uniform(eta, &s1[i], derived_seed);
		derived_seed[kSigmaBytes]++;
	}
	for (i = 0; i < k; i++) {
		scalar_uniform(eta, &s2[i], derived_seed);
		derived_seed[kSigmaBytes]++;
	}
}

/* FIPS 204, Algorithm 34 (`ExpandMask`). Fills the length-L vector |out|. */
static void
vector_expand_mask(scalar *out, const uint8_t seed[kRhoPrimeBytes],
    size_t kappa, size_t l)
{
	uint8_t derived_seed[kRhoPrimeBytes + 2];
	size_t i, index;

	memcpy(derived_seed, seed, kRhoPrimeBytes);
	for (i = 0; i < l; i++) {
		index = kappa + i;
		derived_seed[kRhoPrimeBytes] = index & 0xff;
		derived_seed[kRhoPrimeBytes + 1] = (index >> 8) & 0xff;
		scalar_sample_mask(&out[i], derived_seed);
	}
}

/*
 * FIPS 204, Algorithm 16 (`SimpleBitPack`) over a vector. Encodes into
 * 32*len*|bits| bytes; since DEGREE is a multiple of 8 each entry fills a whole
 * number of bytes.
 */
static void
vector_encode(uint8_t *out, const scalar *a, int bits, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_encode(out + i * bits * DEGREE / 8, &a[i], bits);
}

/* FIPS 204, Algorithm 18 (`SimpleBitUnpack`) over a vector. */
static void
vector_decode(scalar *out, const uint8_t *in, int bits, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_decode(&out[i], in + i * bits * DEGREE / 8, bits);
}

/* FIPS 204, Algorithm 17 (`BitPack`) over a vector. */
static void
vector_encode_signed(uint8_t *out, const scalar *a, int bits, uint32_t max,
    size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		scalar_encode_signed(out + i * bits * DEGREE / 8, &a[i], bits,
		    max);
}

/* FIPS 204, Algorithm 19 (`BitUnpack`) over a vector. */
static int
vector_decode_signed(scalar *out, const uint8_t *in, int bits, uint32_t max,
    size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (!scalar_decode_signed(&out[i], in + i * bits * DEGREE / 8,
		    bits, max))
			return 0;
	}
	return 1;
}

/* FIPS 204, Algorithm 28 (`w1Encode`). */
static void
w1_encode(uint8_t *out, const scalar *w1, size_t k)
{
	vector_encode(out, w1, 4, k);
}

/* FIPS 204, Algorithm 20 (`HintBitPack`). Writes omega(rank) + k bytes. */
static void
hint_bit_pack(uint8_t *out, const scalar *h, int rank)
{
	int i, index, j, k, omega;

	k = rank;
	omega = mldsa_omega(rank);

	memset(out, 0, omega + k);
	index = 0;
	for (i = 0; i < k; i++) {
		for (j = 0; j < DEGREE; j++) {
			if (h[i].c[j]) {
				/* h has at most omega non-zero coefficients. */
				assert(index < omega);
				out[index++] = j;
			}
		}
		out[omega + i] = index;
	}
}

/* FIPS 204, Algorithm 21 (`HintBitUnpack`). Reads omega(rank) + k bytes. */
static int
hint_bit_unpack(scalar *h, const uint8_t *in, int rank)
{
	int byte, i, index, k, last, limit, omega;

	k = rank;
	omega = mldsa_omega(rank);

	vector_zero(h, k);
	index = 0;
	for (i = 0; i < k; i++) {
		limit = in[omega + i];
		if (limit < index || limit > omega)
			return 0;

		last = -1;
		while (index < limit) {
			byte = in[index++];
			if (last >= 0 && byte <= last)
				return 0;
			last = byte;
			h[i].c[byte] = 1;
		}
	}
	for (; index < omega; index++) {
		if (in[index] != 0)
			return 0;
	}
	return 1;
}

/*
 * Working views of the opaque keys and of a signature.
 *
 * The working structs hold pointers into the key's byte storage, computed by
 * the |*_from_external| helpers from the runtime rank. The byte layout matches
 * the sizes declared for the concrete structs in the header:
 *
 *   public key:  rho[32]  t1[k]  public_key_hash[64]
 *   private key: rho[32]  k[32]  public_key_hash[64]  s1[l]  s2[k]  t0[k]
 */

struct public_key {
	uint8_t *rho;
	scalar *t1;
	uint8_t *public_key_hash;
};

struct private_key {
	uint8_t *rho;
	uint8_t *k;
	uint8_t *public_key_hash;
	scalar *s1;
	scalar *s2;
	scalar *t0;
};

struct signature {
	uint8_t *c_tilde;
	scalar *z;
	scalar *h;
};

static void
public_key_from_external(const MLDSA_public_key *external,
    struct public_key *pub)
{
	uint8_t *bytes;

	if (external->rank == MLDSA65_RANK)
		bytes = external->key_65->bytes;
	else
		bytes = external->key_87->bytes;

	pub->rho = bytes;
	pub->t1 = (scalar *)(bytes + kRhoBytes);
	pub->public_key_hash = bytes + kRhoBytes +
	    (size_t)external->rank * sizeof(scalar);
}

static void
private_key_from_external(const MLDSA_private_key *external,
    struct private_key *priv)
{
	uint8_t *bytes;
	int k, l;

	k = external->rank;
	l = mldsa_l(external->rank);
	if (external->rank == MLDSA65_RANK)
		bytes = external->key_65->bytes;
	else
		bytes = external->key_87->bytes;

	priv->rho = bytes;
	priv->k = bytes + kRhoBytes;
	priv->public_key_hash = bytes + kRhoBytes + kKBytes;
	priv->s1 = (scalar *)(bytes + kRhoBytes + kKBytes + kTrBytes);
	priv->s2 = priv->s1 + l;
	priv->t0 = priv->s2 + k;
}

/* Copies |len| bytes out of |cbs| into |out|, advancing |cbs|. */
static int
cbs_copy_bytes(CBS *cbs, uint8_t *out, size_t len)
{
	CBS tmp;

	if (!CBS_get_bytes(cbs, &tmp, len))
		return 0;
	memcpy(out, CBS_data(&tmp), len);

	return 1;
}

/* FIPS 204, Algorithm 22 (`pkEncode`). */
static int
mldsa_marshal_public_key_internal(CBB *out, const struct public_key *pub,
    int rank)
{
	uint8_t *encoded;

	if (!CBB_add_bytes(out, pub->rho, kRhoBytes))
		return 0;
	if (!CBB_add_space(out, &encoded, 320 * (size_t)rank))
		return 0;
	vector_encode(encoded, pub->t1, 10, rank);

	return 1;
}

/* FIPS 204, Algorithm 23 (`pkDecode`), also caching the public key hash. */
static int
mldsa_parse_public_key_internal(struct public_key *pub, CBS *in, int rank)
{
	CBS orig_in = *in;
	CBS t1_bytes;

	if (!cbs_copy_bytes(in, pub->rho, kRhoBytes))
		return 0;
	if (!CBS_get_bytes(in, &t1_bytes, 320 * (size_t)rank))
		return 0;
	if (CBS_len(in) != 0)
		return 0;
	vector_decode(pub->t1, CBS_data(&t1_bytes), 10, rank);

	/* Compute the cached public key hash over the encoded public key. */
	shake256(pub->public_key_hash, kTrBytes, CBS_data(&orig_in),
	    CBS_len(&orig_in));

	return 1;
}

/* FIPS 204, Algorithm 24 (`skEncode`). */
static int
mldsa_marshal_private_key_internal(CBB *out, const struct private_key *priv,
    int rank)
{
	uint8_t *encoded;
	size_t scalar_bytes;
	int bitlen, eta, k, l;

	k = rank;
	l = mldsa_l(rank);
	eta = mldsa_eta(rank);
	bitlen = mldsa_plus_minus_eta_bitlen(rank);
	scalar_bytes = (DEGREE * bitlen + 7) / 8;

	if (!CBB_add_bytes(out, priv->rho, kRhoBytes))
		return 0;
	if (!CBB_add_bytes(out, priv->k, kKBytes))
		return 0;
	if (!CBB_add_bytes(out, priv->public_key_hash, kTrBytes))
		return 0;

	if (!CBB_add_space(out, &encoded, scalar_bytes * l))
		return 0;
	vector_encode_signed(encoded, priv->s1, bitlen, eta, l);

	if (!CBB_add_space(out, &encoded, scalar_bytes * k))
		return 0;
	vector_encode_signed(encoded, priv->s2, bitlen, eta, k);

	if (!CBB_add_space(out, &encoded, 416 * (size_t)k))
		return 0;
	vector_encode_signed(encoded, priv->t0, 13, 1 << 12, k);

	return 1;
}

/* FIPS 204, Algorithm 25 (`skDecode`). */
static int
mldsa_parse_private_key_internal(struct private_key *priv, CBS *in, int rank)
{
	CBS s1_bytes, s2_bytes, t0_bytes;
	size_t scalar_bytes;
	int bitlen, eta, k, l;

	k = rank;
	l = mldsa_l(rank);
	eta = mldsa_eta(rank);
	bitlen = mldsa_plus_minus_eta_bitlen(rank);
	scalar_bytes = (DEGREE * bitlen + 7) / 8;

	if (!cbs_copy_bytes(in, priv->rho, kRhoBytes))
		return 0;
	if (!cbs_copy_bytes(in, priv->k, kKBytes))
		return 0;
	if (!cbs_copy_bytes(in, priv->public_key_hash, kTrBytes))
		return 0;
	if (!CBS_get_bytes(in, &s1_bytes, scalar_bytes * l))
		return 0;
	if (!vector_decode_signed(priv->s1, CBS_data(&s1_bytes), bitlen, eta,
	    l))
		return 0;
	if (!CBS_get_bytes(in, &s2_bytes, scalar_bytes * k))
		return 0;
	if (!vector_decode_signed(priv->s2, CBS_data(&s2_bytes), bitlen, eta,
	    k))
		return 0;
	if (!CBS_get_bytes(in, &t0_bytes, 416 * (size_t)k))
		return 0;
	/* Decoding 13 bits into (-2^12, 2^12] cannot fail. */
	if (!vector_decode_signed(priv->t0, CBS_data(&t0_bytes), 13, 1 << 12,
	    k))
		return 0;

	return 1;
}

/* FIPS 204, Algorithm 26 (`sigEncode`). */
static int
mldsa_marshal_signature(CBB *out, const struct signature *sign, int rank)
{
	uint8_t *encoded;
	int k, l, lambda;

	k = rank;
	l = mldsa_l(rank);
	lambda = mldsa_lambda_bytes(rank);

	if (!CBB_add_bytes(out, sign->c_tilde, 2 * (size_t)lambda))
		return 0;

	if (!CBB_add_space(out, &encoded, 640 * (size_t)l))
		return 0;
	vector_encode_signed(encoded, sign->z, 20, 1 << 19, l);

	if (!CBB_add_space(out, &encoded, (size_t)mldsa_omega(rank) + k))
		return 0;
	hint_bit_pack(encoded, sign->h, rank);

	return 1;
}

/* FIPS 204, Algorithm 27 (`sigDecode`). */
static int
mldsa_parse_signature_internal(struct signature *sign, CBS *in, int rank)
{
	CBS z_bytes, hint_bytes;
	int k, l, lambda;

	k = rank;
	l = mldsa_l(rank);
	lambda = mldsa_lambda_bytes(rank);

	if (!cbs_copy_bytes(in, sign->c_tilde, 2 * (size_t)lambda))
		return 0;
	if (!CBS_get_bytes(in, &z_bytes, 640 * (size_t)l))
		return 0;
	/* Decoding 20 bits into (-2^19, 2^19] cannot fail. */
	if (!vector_decode_signed(sign->z, CBS_data(&z_bytes), 20, 1 << 19, l))
		return 0;
	if (!CBS_get_bytes(in, &hint_bytes, (size_t)mldsa_omega(rank) + k))
		return 0;
	if (!hint_bit_unpack(sign->h, CBS_data(&hint_bytes), rank))
		return 0;

	return 1;
}

/*
 * FIPS 204, Algorithm 6 (`ML-DSA.KeyGen_internal`). Derives the key pair of the
 * rank of |*out_private_key| from |entropy|, writing the private key in place
 * and returning the newly allocated encoded public key in
 * |*out_encoded_public_key|. Returns one on success and zero on failure.
 */
int
mldsa_generate_key_external_entropy(MLDSA_private_key *out_private_key,
    uint8_t **out_encoded_public_key, size_t *out_encoded_public_key_len,
    const uint8_t entropy[MLDSA_SEED_LENGTH])
{
	struct private_key priv;
	struct public_key pub;
	uint8_t augmented_entropy[MLDSA_SEED_LENGTH + 2];
	uint8_t expanded_seed[kRhoBytes + kSigmaBytes + kKBytes];
	const uint8_t *key, *rho, *sigma;
	scalar *scratch = NULL, *a_ntt, *s1_ntt, *t, *t1;
	uint8_t *encoded = NULL;
	size_t encoded_len, scratch_len;
	CBB cbb;
	int k, l, rank, ret = 0;

	memset(&cbb, 0, sizeof(cbb));
	*out_encoded_public_key = NULL;
	*out_encoded_public_key_len = 0;

	rank = out_private_key->rank;
	k = rank;
	l = mldsa_l(rank);
	encoded_len = mldsa_public_key_bytes(rank);
	scratch_len = ((size_t)k * l + l + 2 * k) * sizeof(scalar);

	private_key_from_external(out_private_key, &priv);

	if ((scratch = calloc(1, scratch_len)) == NULL)
		goto err;
	a_ntt = scratch;
	s1_ntt = a_ntt + (size_t)k * l;
	t = s1_ntt + l;
	t1 = t + k;

	memcpy(augmented_entropy, entropy, MLDSA_SEED_LENGTH);
	/* The k and l parameters are appended to the seed. */
	augmented_entropy[MLDSA_SEED_LENGTH] = k;
	augmented_entropy[MLDSA_SEED_LENGTH + 1] = l;
	shake256(expanded_seed, sizeof(expanded_seed), augmented_entropy,
	    sizeof(augmented_entropy));
	rho = expanded_seed;
	sigma = expanded_seed + kRhoBytes;
	key = expanded_seed + kRhoBytes + kSigmaBytes;

	memcpy(priv.rho, rho, kRhoBytes);
	memcpy(priv.k, key, kKBytes);

	matrix_expand(a_ntt, rho, k, l);
	vector_expand_short(rank, priv.s1, priv.s2, sigma);

	memcpy(s1_ntt, priv.s1, sizeof(scalar) * (size_t)l);
	vector_ntt(s1_ntt, l);

	matrix_mult(t, a_ntt, s1_ntt, k, l);
	vector_inverse_ntt(t, k);
	vector_add(t, t, priv.s2, k);

	vector_power2_round(t1, priv.t0, t, k);

	/* Build the public key working view over the freshly computed t1. */
	pub.rho = priv.rho;
	pub.t1 = t1;
	pub.public_key_hash = priv.public_key_hash;

	if (!CBB_init(&cbb, encoded_len))
		goto err;
	if (!mldsa_marshal_public_key_internal(&cbb, &pub, rank))
		goto err;
	if (!CBB_finish(&cbb, &encoded, &encoded_len))
		goto err;

	shake256(priv.public_key_hash, kTrBytes, encoded, encoded_len);

	*out_encoded_public_key = encoded;
	*out_encoded_public_key_len = encoded_len;
	encoded = NULL;
	ret = 1;

 err:
	CBB_cleanup(&cbb);
	freezero(scratch, scratch_len);
	freezero(encoded, encoded_len);
	explicit_bzero(augmented_entropy, sizeof(augmented_entropy));
	explicit_bzero(expanded_seed, sizeof(expanded_seed));

	return ret;
}

/*
 * mldsa_public_from_private sets |*out_public_key| to the public key
 * corresponding to |*private_key|, which must be of the same rank. Returns one
 * on success and zero on failure.
 */
int
mldsa_public_from_private(const MLDSA_private_key *private_key,
    MLDSA_public_key *out_public_key)
{
	struct private_key priv;
	struct public_key pub;
	scalar *scratch = NULL, *a_ntt, *s1_ntt, *t, *t0;
	size_t scratch_len;
	int k, l, rank, ret = 0;

	rank = private_key->rank;
	k = rank;
	l = mldsa_l(rank);
	scratch_len = ((size_t)k * l + l + 2 * k) * sizeof(scalar);

	private_key_from_external(private_key, &priv);
	public_key_from_external(out_public_key, &pub);

	if ((scratch = calloc(1, scratch_len)) == NULL)
		goto err;
	a_ntt = scratch;
	s1_ntt = a_ntt + (size_t)k * l;
	t = s1_ntt + l;
	t0 = t + k;

	memcpy(pub.rho, priv.rho, kRhoBytes);
	memcpy(pub.public_key_hash, priv.public_key_hash, kTrBytes);

	matrix_expand(a_ntt, priv.rho, k, l);

	memcpy(s1_ntt, priv.s1, sizeof(scalar) * (size_t)l);
	vector_ntt(s1_ntt, l);

	matrix_mult(t, a_ntt, s1_ntt, k, l);
	vector_inverse_ntt(t, k);
	vector_add(t, t, priv.s2, k);

	/* t0 here is a throwaway; t0 in the private key is authoritative. */
	vector_power2_round(pub.t1, t0, t, k);

	ret = 1;

 err:
	freezero(scratch, scratch_len);

	return ret;
}

/*
 * FIPS 204, Algorithm 7 (`ML-DSA.Sign_internal`). Signs |msg| with the given
 * |context_prefix|/|context| and |randomizer|, returning the newly allocated
 * encoded signature. Returns one on success and zero on failure.
 */
int
mldsa_sign_internal(const MLDSA_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[MLDSA_SIGNATURE_RANDOMIZER_LENGTH],
    uint8_t **out_encoded_signature, size_t *out_encoded_signature_len)
{
	struct private_key priv;
	struct signature sign;
	sha3_ctx keccak_ctx;
	uint8_t mu[kMuBytes];
	uint8_t rho_prime[kRhoPrimeBytes];
	uint8_t c_tilde[2 * 32];
	uint8_t w1_encoded[128 * MLDSA87_RANK];
	scalar *scratch = NULL;
	scalar *z, *h, *s1_ntt, *s2_ntt, *t0_ntt, *a_ntt, *y, *w, *w1, *cs1, *cs2;
	size_t kappa, scratch_len;
	CBB cbb;
	int beta, gamma1, k, l, lambda, rank, tau, ret = 0;

	memset(&cbb, 0, sizeof(cbb));
	*out_encoded_signature = NULL;
	*out_encoded_signature_len = 0;

	rank = private_key->rank;
	k = rank;
	l = mldsa_l(rank);
	lambda = mldsa_lambda_bytes(rank);
	tau = mldsa_tau(rank);
	beta = mldsa_beta(rank);
	gamma1 = mldsa_gamma1();
	scratch_len = ((size_t)k * l + 4 * l + 6 * k) * sizeof(scalar);

	private_key_from_external(private_key, &priv);

	if ((scratch = calloc(1, scratch_len)) == NULL)
		goto err;
	z = scratch;
	h = z + l;
	s1_ntt = h + k;
	s2_ntt = s1_ntt + l;
	t0_ntt = s2_ntt + k;
	a_ntt = t0_ntt + k;
	y = a_ntt + (size_t)k * l;
	w = y + l;
	w1 = w + k;
	cs1 = w1 + k;
	cs2 = cs1 + l;

	sign.c_tilde = c_tilde;
	sign.z = z;
	sign.h = h;

	/* mu = H(tr || context_prefix || context || msg) */
	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, priv.public_key_hash, kTrBytes);
	shake_update(&keccak_ctx, context_prefix, context_prefix_len);
	shake_update(&keccak_ctx, context, context_len);
	shake_update(&keccak_ctx, msg, msg_len);
	shake_xof(&keccak_ctx);
	shake_out(&keccak_ctx, mu, kMuBytes);

	/* rho_prime = H(k || randomizer || mu) */
	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, priv.k, kKBytes);
	shake_update(&keccak_ctx, randomizer, MLDSA_SIGNATURE_RANDOMIZER_LENGTH);
	shake_update(&keccak_ctx, mu, kMuBytes);
	shake_xof(&keccak_ctx);
	shake_out(&keccak_ctx, rho_prime, kRhoPrimeBytes);

	memcpy(s1_ntt, priv.s1, sizeof(scalar) * (size_t)l);
	vector_ntt(s1_ntt, l);
	memcpy(s2_ntt, priv.s2, sizeof(scalar) * (size_t)k);
	vector_ntt(s2_ntt, k);
	memcpy(t0_ntt, priv.t0, sizeof(scalar) * (size_t)k);
	vector_ntt(t0_ntt, k);

	matrix_expand(a_ntt, priv.rho, k, l);

	/*
	 * kappa must not exceed 2^16/l. But the probability of it exceeding even
	 * 1000 iterations is vanishingly small.
	 */
	for (kappa = 0;; kappa += l) {
		scalar c_ntt;
		scalar *y_ntt = cs1;
		scalar *r0 = w1;
		scalar *ct0 = w1;
		uint32_t ct0_max, r0_max, z_max;
		size_t h_ones;

		vector_expand_mask(y, rho_prime, kappa, l);

		memcpy(y_ntt, y, sizeof(scalar) * (size_t)l);
		vector_ntt(y_ntt, l);

		matrix_mult(w, a_ntt, y_ntt, k, l);
		vector_inverse_ntt(w, k);

		vector_high_bits(w1, w, k);
		w1_encode(w1_encoded, w1, k);

		shake256_init(&keccak_ctx);
		shake_update(&keccak_ctx, mu, kMuBytes);
		shake_update(&keccak_ctx, w1_encoded, 128 * (size_t)k);
		shake_xof(&keccak_ctx);
		shake_out(&keccak_ctx, sign.c_tilde, 2 * (size_t)lambda);

		scalar_sample_in_ball_vartime(&c_ntt, sign.c_tilde, 2 * lambda,
		    tau);
		scalar_ntt(&c_ntt);

		vector_mult_scalar(cs1, s1_ntt, &c_ntt, l);
		vector_inverse_ntt(cs1, l);
		vector_mult_scalar(cs2, s2_ntt, &c_ntt, k);
		vector_inverse_ntt(cs2, k);

		vector_add(sign.z, y, cs1, l);

		vector_sub(r0, w, cs2, k);
		vector_low_bits(r0, r0, k);

		/*
		 * Leaking the fact that a signature was rejected is fine as the
		 * next attempt will be (indistinguishable from) independent of
		 * this one.
		 */
		z_max = vector_max(sign.z, l);
		r0_max = vector_max_signed(r0, k);
		if (constant_time_ge(z_max, (uint32_t)(gamma1 - beta)) |
		    constant_time_ge(r0_max, kGamma2 - beta))
			continue;

		vector_mult_scalar(ct0, t0_ntt, &c_ntt, k);
		vector_inverse_ntt(ct0, k);
		vector_make_hint(sign.h, ct0, cs2, w, k);

		ct0_max = vector_max(ct0, k);
		h_ones = vector_count_ones(sign.h, k);
		if (constant_time_ge(ct0_max, kGamma2) |
		    constant_time_lt((unsigned int)mldsa_omega(rank),
		    (unsigned int)h_ones))
			continue;

		if (!CBB_init(&cbb, mldsa_signature_bytes(rank)))
			goto err;
		if (!mldsa_marshal_signature(&cbb, &sign, rank))
			goto err;
		if (!CBB_finish(&cbb, out_encoded_signature,
		    out_encoded_signature_len))
			goto err;

		ret = 1;
		break;
	}

 err:
	CBB_cleanup(&cbb);
	freezero(scratch, scratch_len);
	explicit_bzero(mu, sizeof(mu));
	explicit_bzero(rho_prime, sizeof(rho_prime));

	return ret;
}

/*
 * mldsa_sign generates a signature in the randomized mode, drawing the
 * randomizer from arc4random.
 */
int
mldsa_sign(const MLDSA_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len,
    uint8_t **out_encoded_signature, size_t *out_encoded_signature_len)
{
	uint8_t randomizer[MLDSA_SIGNATURE_RANDOMIZER_LENGTH];
	uint8_t context_prefix[2];
	int ret;

	*out_encoded_signature = NULL;
	*out_encoded_signature_len = 0;

	if (context_len > 255)
		return 0;
	context_prefix[0] = 0;
	context_prefix[1] = (uint8_t)context_len;

	arc4random_buf(randomizer, sizeof(randomizer));

	ret = mldsa_sign_internal(private_key, msg, msg_len, context_prefix,
	    sizeof(context_prefix), context, context_len, randomizer,
	    out_encoded_signature, out_encoded_signature_len);

	explicit_bzero(randomizer, sizeof(randomizer));

	return ret;
}

/* FIPS 204, Algorithm 8 (`ML-DSA.Verify_internal`). */
int
mldsa_verify_internal(const MLDSA_public_key *public_key,
    const uint8_t *signature, size_t signature_len, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len)
{
	struct public_key pub;
	struct signature sign;
	sha3_ctx keccak_ctx;
	scalar c_ntt;
	scalar *scratch = NULL;
	scalar *a_ntt, *az_ntt, *ct1_ntt, *hh, *w1, *z, *z_ntt;
	uint8_t mu[kMuBytes];
	uint8_t c_tilde[2 * 32];
	uint8_t sig_c_tilde[2 * 32];
	uint8_t w1_encoded[128 * MLDSA87_RANK];
	uint32_t z_max;
	size_t scratch_len;
	CBS cbs;
	int k, l, lambda, rank, ret = 0;

	rank = public_key->rank;
	k = rank;
	l = mldsa_l(rank);
	lambda = mldsa_lambda_bytes(rank);
	scratch_len = ((size_t)k * l + 2 * l + 3 * k) * sizeof(scalar);

	public_key_from_external(public_key, &pub);

	if ((scratch = calloc(1, scratch_len)) == NULL)
		goto err;
	z = scratch;
	hh = z + l;
	a_ntt = hh + k;
	z_ntt = a_ntt + (size_t)k * l;
	az_ntt = z_ntt + l;
	ct1_ntt = az_ntt + k;

	sign.c_tilde = sig_c_tilde;
	sign.z = z;
	sign.h = hh;

	CBS_init(&cbs, signature, signature_len);
	if (!mldsa_parse_signature_internal(&sign, &cbs, rank))
		goto err;
	if (CBS_len(&cbs) != 0)
		goto err;

	matrix_expand(a_ntt, pub.rho, k, l);

	/* mu = H(tr || context_prefix || context || msg) */
	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, pub.public_key_hash, kTrBytes);
	shake_update(&keccak_ctx, context_prefix, context_prefix_len);
	shake_update(&keccak_ctx, context, context_len);
	shake_update(&keccak_ctx, msg, msg_len);
	shake_xof(&keccak_ctx);
	shake_out(&keccak_ctx, mu, kMuBytes);

	scalar_sample_in_ball_vartime(&c_ntt, sign.c_tilde, 2 * lambda,
	    mldsa_tau(rank));
	scalar_ntt(&c_ntt);

	memcpy(z_ntt, sign.z, sizeof(scalar) * (size_t)l);
	vector_ntt(z_ntt, l);

	matrix_mult(az_ntt, a_ntt, z_ntt, k, l);

	vector_scale_power2_round(ct1_ntt, pub.t1, k);
	vector_ntt(ct1_ntt, k);
	vector_mult_scalar(ct1_ntt, ct1_ntt, &c_ntt, k);

	/* w1 reuses the az_ntt storage. */
	w1 = az_ntt;
	vector_sub(w1, az_ntt, ct1_ntt, k);
	vector_inverse_ntt(w1, k);

	vector_use_hint_vartime(w1, sign.h, w1, k);
	w1_encode(w1_encoded, w1, k);

	shake256_init(&keccak_ctx);
	shake_update(&keccak_ctx, mu, kMuBytes);
	shake_update(&keccak_ctx, w1_encoded, 128 * (size_t)k);
	shake_xof(&keccak_ctx);
	shake_out(&keccak_ctx, c_tilde, 2 * (size_t)lambda);

	z_max = vector_max(sign.z, l);
	if (z_max < (uint32_t)(mldsa_gamma1() - mldsa_beta(rank)) &&
	    memcmp(c_tilde, sign.c_tilde, 2 * lambda) == 0)
		ret = 1;

 err:
	freezero(scratch, scratch_len);

	return ret;
}

/* FIPS 204, Algorithm 3 (`ML-DSA.Verify`). */
int
mldsa_verify(const MLDSA_public_key *public_key, const uint8_t *signature,
    size_t signature_len, const uint8_t *msg, size_t msg_len,
    const uint8_t *context, size_t context_len)
{
	uint8_t context_prefix[2];

	if (context_len > 255)
		return 0;
	context_prefix[0] = 0;
	context_prefix[1] = (uint8_t)context_len;

	return mldsa_verify_internal(public_key, signature, signature_len, msg,
	    msg_len, context_prefix, sizeof(context_prefix), context,
	    context_len);
}

/*
 * mldsa_private_key_from_seed derives the private key of the rank of
 * |*out_private_key| from |seed|, discarding the encoded public key.
 */
int
mldsa_private_key_from_seed(const uint8_t *seed, size_t seed_len,
    MLDSA_private_key *out_private_key)
{
	uint8_t *encoded_public_key = NULL;
	size_t encoded_public_key_len = 0;
	int ret;

	if (seed_len != MLDSA_SEED_LENGTH)
		return 0;

	ret = mldsa_generate_key_external_entropy(out_private_key,
	    &encoded_public_key, &encoded_public_key_len, seed);
	freezero(encoded_public_key, encoded_public_key_len);

	return ret;
}

int
mldsa_marshal_public_key(const MLDSA_public_key *public_key, uint8_t **output,
    size_t *output_len)
{
	struct public_key pub;
	CBB cbb;
	int ret = 0;

	*output = NULL;
	*output_len = 0;
	memset(&cbb, 0, sizeof(cbb));

	public_key_from_external(public_key, &pub);

	if (!CBB_init(&cbb, mldsa_public_key_bytes(public_key->rank)))
		goto err;
	if (!mldsa_marshal_public_key_internal(&cbb, &pub, public_key->rank))
		goto err;
	if (!CBB_finish(&cbb, output, output_len))
		goto err;
	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mldsa_parse_public_key(const uint8_t *input, size_t input_len,
    MLDSA_public_key *out_public_key)
{
	struct public_key pub;
	CBS cbs;

	public_key_from_external(out_public_key, &pub);
	CBS_init(&cbs, input, input_len);
	if (!mldsa_parse_public_key_internal(&pub, &cbs, out_public_key->rank))
		return 0;

	return 1;
}

int
mldsa_marshal_private_key(const MLDSA_private_key *private_key,
    uint8_t **output, size_t *output_len)
{
	struct private_key priv;
	CBB cbb;
	int ret = 0;

	*output = NULL;
	*output_len = 0;
	memset(&cbb, 0, sizeof(cbb));

	private_key_from_external(private_key, &priv);

	if (!CBB_init(&cbb, mldsa_private_key_bytes(private_key->rank)))
		goto err;
	if (!mldsa_marshal_private_key_internal(&cbb, &priv, private_key->rank))
		goto err;
	if (!CBB_finish(&cbb, output, output_len))
		goto err;
	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mldsa_parse_private_key(const uint8_t *input, size_t input_len,
    MLDSA_private_key *out_private_key)
{
	struct private_key priv;
	CBS cbs;

	private_key_from_external(out_private_key, &priv);
	CBS_init(&cbs, input, input_len);
	if (!mldsa_parse_private_key_internal(&priv, &cbs,
	    out_private_key->rank))
		return 0;
	if (CBS_len(&cbs) != 0)
		return 0;

	return 1;
}
