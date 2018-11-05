/*	$OpenBSD: ecp_nistz256.c,v 1.7 2018/11/05 20:18:21 tb Exp $	*/
/* Copyright (c) 2014, Intel Corporation.
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

/* Developers and authors:
 * Shay Gueron (1, 2), and Vlad Krasnov (1)
 * (1) Intel Corporation, Israel Development Center
 * (2) University of Haifa
 * Reference:
 * S.Gueron and V.Krasnov, "Fast Prime Field Elliptic Curve Cryptography with
 *                          256 Bit Primes" */

/*
 * The following license applies to _booth_recode_w5() and
 * _booth_recode_w7():
 */
/* Copyright (c) 2015, Google Inc.
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

#include <string.h>

#include <openssl/crypto.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "ec_lcl.h"

#if BN_BITS2 != 64
#define	TOBN(hi,lo)	lo,hi
#else
#define	TOBN(hi,lo)	((BN_ULONG)hi << 32 | lo)
#endif

#if defined(__GNUC__)
#define	ALIGN32		__attribute((aligned(32)))
#elif defined(_MSC_VER)
#define	ALIGN32		__declspec(align(32))
#else
#define	ALIGN32
#endif

#define	P256_LIMBS	(256 / BN_BITS2)

typedef struct {
	BN_ULONG X[P256_LIMBS];
	BN_ULONG Y[P256_LIMBS];
	BN_ULONG Z[P256_LIMBS];
} P256_POINT;

typedef struct {
	BN_ULONG X[P256_LIMBS];
	BN_ULONG Y[P256_LIMBS];
} P256_POINT_AFFINE;

typedef P256_POINT_AFFINE PRECOMP256_ROW[64];

/* structure for precomputed multiples of the generator */
typedef struct ec_pre_comp_st {
	const EC_GROUP *group;	/* Parent EC_GROUP object */
	size_t w;		/* Window size */
	/*
	 * Constant time access to the X and Y coordinates of the pre-computed,
	 * generator multiplies, in the Montgomery domain. Pre-calculated
	 * multiplies are stored in affine form.
	 */
	PRECOMP256_ROW *precomp;
	int references;
} EC_PRE_COMP;

/*
 * Arithmetic on field elements using Almost Montgomery Multiplication. The
 * "almost" means, in particular, that the inputs and outputs of these
 * functions are in the range [0, 2**BN_BITS2), not [0, P). Only
 * |ecp_nistz256_from_mont| outputs a fully reduced value in [0, P). Almost
 * Montgomery Arithmetic is described clearly in "Efficient Software
 * Implementations of Modular Exponentiation" by Shay Gueron.
 */

/* Modular neg: res = -a mod P, where res is not fully reduced. */
void	ecp_nistz256_neg(BN_ULONG res[P256_LIMBS],
	    const BN_ULONG a[P256_LIMBS]);
/* Montgomery mul: res = a*b*2^-256 mod P, where res is not fully reduced. */
void	ecp_nistz256_mul_mont(BN_ULONG res[P256_LIMBS],
	    const BN_ULONG a[P256_LIMBS], const BN_ULONG b[P256_LIMBS]);
/* Montgomery sqr: res = a*a*2^-256 mod P, where res is not fully reduced. */
void	ecp_nistz256_sqr_mont(BN_ULONG res[P256_LIMBS],
	    const BN_ULONG a[P256_LIMBS]);
/* Convert a number from Montgomery domain, by multiplying with 1, where res
 * will be fully reduced mod P. */
void	ecp_nistz256_from_mont(BN_ULONG res[P256_LIMBS],
	    const BN_ULONG in[P256_LIMBS]);

/* Functions that perform constant time access to the precomputed tables */
void	ecp_nistz256_select_w5(P256_POINT *val, const P256_POINT *in_t,
	    int index);
void	ecp_nistz256_select_w7(P256_POINT_AFFINE *val,
	    const P256_POINT_AFFINE *in_t, int index);

/* One converted into the Montgomery domain */
static const BN_ULONG ONE[P256_LIMBS] = {
	TOBN(0x00000000, 0x00000001), TOBN(0xffffffff, 0x00000000),
	TOBN(0xffffffff, 0xffffffff), TOBN(0x00000000, 0xfffffffe)
};

static void *ecp_nistz256_pre_comp_dup(void *);
static void ecp_nistz256_pre_comp_free(void *);
static void ecp_nistz256_pre_comp_clear_free(void *);
static EC_PRE_COMP *ecp_nistz256_pre_comp_new(const EC_GROUP *group);

/* Precomputed tables for the default generator */
#include "ecp_nistz256_table.h"

/* This function looks at 5+1 scalar bits (5 current, 1 adjacent less
 * significant bit), and recodes them into a signed digit for use in fast point
 * multiplication: the use of signed rather than unsigned digits means that
 * fewer points need to be precomputed, given that point inversion is easy (a
 * precomputed point dP makes -dP available as well).
 *
 * BACKGROUND:
 *
 * Signed digits for multiplication were introduced by Booth ("A signed binary
 * multiplication technique", Quart. Journ. Mech. and Applied Math., vol. IV,
 * pt. 2 (1951), pp. 236-240), in that case for multiplication of integers.
 * Booth's original encoding did not generally improve the density of nonzero
 * digits over the binary representation, and was merely meant to simplify the
 * handling of signed factors given in two's complement; but it has since been
 * shown to be the basis of various signed-digit representations that do have
 * further advantages, including the wNAF, using the following general
 * approach:
 *
 * (1) Given a binary representation
 *
 *       b_k  ...  b_2  b_1  b_0,
 *
 *     of a nonnegative integer (b_k in {0, 1}), rewrite it in digits 0, 1, -1
 *     by using bit-wise subtraction as follows:
 *
 *        b_k b_(k-1)  ...  b_2  b_1  b_0
 *      -     b_k      ...  b_3  b_2  b_1  b_0
 *       -------------------------------------
 *        s_k b_(k-1)  ...  s_3  s_2  s_1  s_0
 *
 *     A left-shift followed by subtraction of the original value yields a new
 *     representation of the same value, using signed bits s_i = b_(i+1) - b_i.
 *     This representation from Booth's paper has since appeared in the
 *     literature under a variety of different names including "reversed binary
 *     form", "alternating greedy expansion", "mutual opposite form", and
 *     "sign-alternating {+-1}-representation".
 *
 *     An interesting property is that among the nonzero bits, values 1 and -1
 *     strictly alternate.
 *
 * (2) Various window schemes can be applied to the Booth representation of
 *     integers: for example, right-to-left sliding windows yield the wNAF
 *     (a signed-digit encoding independently discovered by various researchers
 *     in the 1990s), and left-to-right sliding windows yield a left-to-right
 *     equivalent of the wNAF (independently discovered by various researchers
 *     around 2004).
 *
 * To prevent leaking information through side channels in point multiplication,
 * we need to recode the given integer into a regular pattern: sliding windows
 * as in wNAFs won't do, we need their fixed-window equivalent -- which is a few
 * decades older: we'll be using the so-called "modified Booth encoding" due to
 * MacSorley ("High-speed arithmetic in binary computers", Proc. IRE, vol. 49
 * (1961), pp. 67-91), in a radix-2^5 setting.  That is, we always combine five
 * signed bits into a signed digit:
 *
 *       s_(4j + 4) s_(4j + 3) s_(4j + 2) s_(4j + 1) s_(4j)
 *
 * The sign-alternating property implies that the resulting digit values are
 * integers from -16 to 16.
 *
 * Of course, we don't actually need to compute the signed digits s_i as an
 * intermediate step (that's just a nice way to see how this scheme relates
 * to the wNAF): a direct computation obtains the recoded digit from the
 * six bits b_(4j + 4) ... b_(4j - 1).
 *
 * This function takes those five bits as an integer (0 .. 63), writing the
 * recoded digit to *sign (0 for positive, 1 for negative) and *digit (absolute
 * value, in the range 0 .. 8).  Note that this integer essentially provides the
 * input bits "shifted to the left" by one position: for example, the input to
 * compute the least significant recoded digit, given that there's no bit b_-1,
 * has to be b_4 b_3 b_2 b_1 b_0 0. */

static unsigned int
_booth_recode_w5(unsigned int in)
{
	unsigned int s, d;

	/* sets all bits to MSB(in), 'in' seen as 6-bit value */
	s = ~((in >> 5) - 1);
	d = (1 << 6) - in - 1;
	d = (d & s) | (in & ~s);
	d = (d >> 1) + (d & 1);

	return (d << 1) + (s & 1);
}

static unsigned int
_booth_recode_w7(unsigned int in)
{
	unsigned int s, d;

	/* sets all bits to MSB(in), 'in' seen as 8-bit value */
	s = ~((in >> 7) - 1);
	d = (1 << 8) - in - 1;
	d = (d & s) | (in & ~s);
	d = (d >> 1) + (d & 1);

	return (d << 1) + (s & 1);
}

static void
copy_conditional(BN_ULONG dst[P256_LIMBS], const BN_ULONG src[P256_LIMBS],
    BN_ULONG move)
{
	BN_ULONG mask1 = -move;
	BN_ULONG mask2 = ~mask1;

	dst[0] = (src[0] & mask1) ^ (dst[0] & mask2);
	dst[1] = (src[1] & mask1) ^ (dst[1] & mask2);
	dst[2] = (src[2] & mask1) ^ (dst[2] & mask2);
	dst[3] = (src[3] & mask1) ^ (dst[3] & mask2);
	if (P256_LIMBS == 8) {
		dst[4] = (src[4] & mask1) ^ (dst[4] & mask2);
		dst[5] = (src[5] & mask1) ^ (dst[5] & mask2);
		dst[6] = (src[6] & mask1) ^ (dst[6] & mask2);
		dst[7] = (src[7] & mask1) ^ (dst[7] & mask2);
	}
}

static BN_ULONG
is_zero(BN_ULONG in)
{
	in |= (0 - in);
	in = ~in;
	in &= BN_MASK2;
	in >>= BN_BITS2 - 1;
	return in;
}

static BN_ULONG
is_equal(const BN_ULONG a[P256_LIMBS], const BN_ULONG b[P256_LIMBS])
{
	BN_ULONG res;

	res = a[0] ^ b[0];
	res |= a[1] ^ b[1];
	res |= a[2] ^ b[2];
	res |= a[3] ^ b[3];
	if (P256_LIMBS == 8) {
		res |= a[4] ^ b[4];
		res |= a[5] ^ b[5];
		res |= a[6] ^ b[6];
		res |= a[7] ^ b[7];
	}

	return is_zero(res);
}

static BN_ULONG
is_one(const BIGNUM *z)
{
	BN_ULONG res = 0;
	BN_ULONG *a = z->d;

	if (z->top == (P256_LIMBS - P256_LIMBS / 8)) {
		res = a[0] ^ ONE[0];
		res |= a[1] ^ ONE[1];
		res |= a[2] ^ ONE[2];
		res |= a[3] ^ ONE[3];
		if (P256_LIMBS == 8) {
			res |= a[4] ^ ONE[4];
			res |= a[5] ^ ONE[5];
			res |= a[6] ^ ONE[6];
			/*
			 * No check for a[7] (being zero) on 32-bit platforms,
			 * because value of "one" takes only 7 limbs.
			 */
		}
		res = is_zero(res);
	}

	return res;
}

static int
ecp_nistz256_set_words(BIGNUM *a, BN_ULONG words[P256_LIMBS])
{
	if (bn_wexpand(a, P256_LIMBS) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return 0;
	}
	memcpy(a->d, words, sizeof(BN_ULONG) * P256_LIMBS);
	a->top = P256_LIMBS;
	bn_correct_top(a);
	return 1;
}

void	ecp_nistz256_point_double(P256_POINT *r, const P256_POINT *a);
void	ecp_nistz256_point_add(P256_POINT *r, const P256_POINT *a,
	    const P256_POINT *b);
void	ecp_nistz256_point_add_affine(P256_POINT *r, const P256_POINT *a,
	    const P256_POINT_AFFINE *b);

/* r = in^-1 mod p */
static void
ecp_nistz256_mod_inverse(BN_ULONG r[P256_LIMBS], const BN_ULONG in[P256_LIMBS])
{
	/*
	 * The poly is ffffffff 00000001 00000000 00000000 00000000 ffffffff
	 * ffffffff ffffffff. We use FLT and use poly-2 as exponent.
	 */
	BN_ULONG p2[P256_LIMBS];
	BN_ULONG p4[P256_LIMBS];
	BN_ULONG p8[P256_LIMBS];
	BN_ULONG p16[P256_LIMBS];
	BN_ULONG p32[P256_LIMBS];
	BN_ULONG res[P256_LIMBS];
	unsigned int i;

	ecp_nistz256_sqr_mont(res, in);
	ecp_nistz256_mul_mont(p2, res, in);	/* 3*p */

	ecp_nistz256_sqr_mont(res, p2);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(p4, res, p2);	/* f*p */

	ecp_nistz256_sqr_mont(res, p4);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(p8, res, p4);	/* ff*p */

	ecp_nistz256_sqr_mont(res, p8);
	for (i = 0; i < 7; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(p16, res, p8);	/* ffff*p */

	ecp_nistz256_sqr_mont(res, p16);
	for (i = 0; i < 15; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(p32, res, p16);	/* ffffffff*p */

	ecp_nistz256_sqr_mont(res, p32);
	for (i = 0; i < 31; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, in);

	for (i = 0; i < 32 * 4; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p32);

	for (i = 0; i < 32; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p32);

	for (i = 0; i < 16; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p16);

	for (i = 0; i < 8; i++)
		ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p8);

	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p4);

	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, p2);

	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_sqr_mont(res, res);
	ecp_nistz256_mul_mont(res, res, in);

	memcpy(r, res, sizeof(res));
}

/*
 * ecp_nistz256_bignum_to_field_elem copies the contents of |in| to |out| and
 * returns one if it fits. Otherwise it returns zero.
 */
static int
ecp_nistz256_bignum_to_field_elem(BN_ULONG out[P256_LIMBS], const BIGNUM *in)
{
	if (in->top > P256_LIMBS)
		return 0;

	memset(out, 0, sizeof(BN_ULONG) * P256_LIMBS);
	memcpy(out, in->d, sizeof(BN_ULONG) * in->top);
	return 1;
}

/* r = sum(scalar[i]*point[i]) */
static int
ecp_nistz256_windowed_mul(const EC_GROUP *group, P256_POINT *r,
    const BIGNUM **scalar, const EC_POINT **point, size_t num, BN_CTX *ctx)
{
	int ret = 0;
	unsigned int i, j, index;
	unsigned char (*p_str)[33] = NULL;
	const unsigned int window_size = 5;
	const unsigned int mask = (1 << (window_size + 1)) - 1;
	unsigned int wvalue;
	BN_ULONG tmp[P256_LIMBS];
	/* avoid warning about ignored alignment for stack variable */
#if defined(__GNUC__) && !defined(__OpenBSD__)
	ALIGN32
#endif
	P256_POINT h;
	const BIGNUM **scalars = NULL;
	P256_POINT (*table)[16] = NULL;

	if (posix_memalign((void **)&table, 64, num * sizeof(*table)) != 0 ||
	    (p_str = reallocarray(NULL, num, sizeof(*p_str))) == NULL ||
	    (scalars = reallocarray(NULL, num, sizeof(*scalars))) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	for (i = 0; i < num; i++) {
		P256_POINT *row = table[i];

		/*
		 * This is an unusual input, we don't guarantee
		 * constant-timeness.
		 */
		if (BN_num_bits(scalar[i]) > 256 || BN_is_negative(scalar[i])) {
			BIGNUM *mod;

			if ((mod = BN_CTX_get(ctx)) == NULL)
				goto err;
			if (!BN_nnmod(mod, scalar[i], &group->order, ctx)) {
				ECerror(ERR_R_BN_LIB);
				goto err;
			}
			scalars[i] = mod;
		} else
			scalars[i] = scalar[i];

		for (j = 0; j < scalars[i]->top * BN_BYTES; j += BN_BYTES) {
			BN_ULONG d = scalars[i]->d[j / BN_BYTES];

			p_str[i][j + 0] = d & 0xff;
			p_str[i][j + 1] = (d >> 8) & 0xff;
			p_str[i][j + 2] = (d >> 16) & 0xff;
			p_str[i][j + 3] = (d >> 24) & 0xff;
			if (BN_BYTES == 8) {
				d >>= 32;
				p_str[i][j + 4] = d & 0xff;
				p_str[i][j + 5] = (d >> 8) & 0xff;
				p_str[i][j + 6] = (d >> 16) & 0xff;
				p_str[i][j + 7] = (d >> 24) & 0xff;
			}
		}
		for (; j < 33; j++)
			p_str[i][j] = 0;

		/*
		 * table[0] is implicitly (0,0,0) (the point at infinity),
		 * therefore it is not stored. All other values are actually
		 * stored with an offset of -1 in table.
		 */

		if (!ecp_nistz256_bignum_to_field_elem(row[1 - 1].X,
		      &point[i]->X) ||
		    !ecp_nistz256_bignum_to_field_elem(row[1 - 1].Y,
		      &point[i]->Y) ||
		    !ecp_nistz256_bignum_to_field_elem(row[1 - 1].Z,
		      &point[i]->Z)) {
			ECerror(EC_R_COORDINATES_OUT_OF_RANGE);
			goto err;
		}

		ecp_nistz256_point_double(&row[ 2 - 1], &row[ 1 - 1]);
		ecp_nistz256_point_add(&row[ 3 - 1], &row[ 2 - 1], &row[1 - 1]);
		ecp_nistz256_point_double(&row[ 4 - 1], &row[ 2 - 1]);
		ecp_nistz256_point_double(&row[ 6 - 1], &row[ 3 - 1]);
		ecp_nistz256_point_double(&row[ 8 - 1], &row[ 4 - 1]);
		ecp_nistz256_point_double(&row[12 - 1], &row[ 6 - 1]);
		ecp_nistz256_point_add(&row[ 5 - 1], &row[ 4 - 1], &row[1 - 1]);
		ecp_nistz256_point_add(&row[ 7 - 1], &row[ 6 - 1], &row[1 - 1]);
		ecp_nistz256_point_add(&row[ 9 - 1], &row[ 8 - 1], &row[1 - 1]);
		ecp_nistz256_point_add(&row[13 - 1], &row[12 - 1], &row[1 - 1]);
		ecp_nistz256_point_double(&row[14 - 1], &row[ 7 - 1]);
		ecp_nistz256_point_double(&row[10 - 1], &row[ 5 - 1]);
		ecp_nistz256_point_add(&row[15 - 1], &row[14 - 1], &row[1 - 1]);
		ecp_nistz256_point_add(&row[11 - 1], &row[10 - 1], &row[1 - 1]);
		ecp_nistz256_point_add(&row[16 - 1], &row[15 - 1], &row[1 - 1]);
	}

	index = 255;

	wvalue = p_str[0][(index - 1) / 8];
	wvalue = (wvalue >> ((index - 1) % 8)) & mask;

	ecp_nistz256_select_w5(r, table[0], _booth_recode_w5(wvalue) >> 1);

	while (index >= 5) {
		for (i = (index == 255 ? 1 : 0); i < num; i++) {
			unsigned int off = (index - 1) / 8;

			wvalue = p_str[i][off] | p_str[i][off + 1] << 8;
			wvalue = (wvalue >> ((index - 1) % 8)) & mask;

			wvalue = _booth_recode_w5(wvalue);

			ecp_nistz256_select_w5(&h, table[i], wvalue >> 1);

			ecp_nistz256_neg(tmp, h.Y);
			copy_conditional(h.Y, tmp, (wvalue & 1));

			ecp_nistz256_point_add(r, r, &h);
		}

		index -= window_size;

		ecp_nistz256_point_double(r, r);
		ecp_nistz256_point_double(r, r);
		ecp_nistz256_point_double(r, r);
		ecp_nistz256_point_double(r, r);
		ecp_nistz256_point_double(r, r);
	}

	/* Final window */
	for (i = 0; i < num; i++) {
		wvalue = p_str[i][0];
		wvalue = (wvalue << 1) & mask;

		wvalue = _booth_recode_w5(wvalue);

		ecp_nistz256_select_w5(&h, table[i], wvalue >> 1);

		ecp_nistz256_neg(tmp, h.Y);
		copy_conditional(h.Y, tmp, wvalue & 1);

		ecp_nistz256_point_add(r, r, &h);
	}

	ret = 1;
 err:
	free(table);
	free(p_str);
	free(scalars);
	return ret;
}

/* Coordinates of G, for which we have precomputed tables */
const static BN_ULONG def_xG[P256_LIMBS] = {
	TOBN(0x79e730d4, 0x18a9143c), TOBN(0x75ba95fc, 0x5fedb601),
	TOBN(0x79fb732b, 0x77622510), TOBN(0x18905f76, 0xa53755c6)
};

const static BN_ULONG def_yG[P256_LIMBS] = {
	TOBN(0xddf25357, 0xce95560a), TOBN(0x8b4ab8e4, 0xba19e45c),
	TOBN(0xd2e88688, 0xdd21f325), TOBN(0x8571ff18, 0x25885d85)
};

/*
 * ecp_nistz256_is_affine_G returns one if |generator| is the standard, P-256
 * generator.
 */
static int
ecp_nistz256_is_affine_G(const EC_POINT *generator)
{
	return generator->X.top == P256_LIMBS &&
	    generator->Y.top == P256_LIMBS &&
	    is_equal(generator->X.d, def_xG) &&
	    is_equal(generator->Y.d, def_yG) &&
	    is_one(&generator->Z);
}

static int
ecp_nistz256_mult_precompute(EC_GROUP *group, BN_CTX *ctx)
{
	/*
	 * We precompute a table for a Booth encoded exponent (wNAF) based
	 * computation. Each table holds 64 values for safe access, with an
	 * implicit value of infinity at index zero. We use a window of size 7,
	 * and therefore require ceil(256/7) = 37 tables.
	 */
	EC_POINT *P = NULL, *T = NULL;
	BN_CTX *new_ctx = NULL;
	const EC_POINT *generator;
	EC_PRE_COMP *ec_pre_comp;
	BIGNUM *order;
	int ret = 0;
	unsigned int i, j, k;
	PRECOMP256_ROW *precomp = NULL;

	/* if there is an old EC_PRE_COMP object, throw it away */
	EC_EX_DATA_free_data(&group->extra_data, ecp_nistz256_pre_comp_dup,
	    ecp_nistz256_pre_comp_free, ecp_nistz256_pre_comp_clear_free);

	generator = EC_GROUP_get0_generator(group);
	if (generator == NULL) {
		ECerror(EC_R_UNDEFINED_GENERATOR);
		return 0;
	}

	if (ecp_nistz256_is_affine_G(generator)) {
		/*
		 * No need to calculate tables for the standard generator
		 * because we have them statically.
		 */
		return 1;
	}

	if ((ec_pre_comp = ecp_nistz256_pre_comp_new(group)) == NULL)
		return 0;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			goto err;
	}

	BN_CTX_start(ctx);
	order = BN_CTX_get(ctx);

	if (order == NULL)
		goto err;

	if (!EC_GROUP_get_order(group, order, ctx))
		goto err;

	if (BN_is_zero(order)) {
		ECerror(EC_R_UNKNOWN_ORDER);
		goto err;
	}

	if (posix_memalign((void **)&precomp, 64, 37 * sizeof(*precomp)) != 0) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	P = EC_POINT_new(group);
	T = EC_POINT_new(group);
	if (P == NULL || T == NULL)
		goto err;

	/*
	 * The zero entry is implicitly infinity, and we skip it, storing other
	 * values with -1 offset.
	 */
	if (!EC_POINT_copy(T, generator))
		goto err;

	for (k = 0; k < 64; k++) {
		if (!EC_POINT_copy(P, T))
			goto err;
		for (j = 0; j < 37; j++) {
			/*
			 * It would be faster to use EC_POINTs_make_affine and
			 * make multiple points affine at the same time.
			 */
			if (!EC_POINT_make_affine(group, P, ctx))
				goto err;
			if (!ecp_nistz256_bignum_to_field_elem(
			      precomp[j][k].X, &P->X) ||
			    !ecp_nistz256_bignum_to_field_elem(
			      precomp[j][k].Y, &P->Y)) {
				ECerror(EC_R_COORDINATES_OUT_OF_RANGE);
				goto err;
			}
			for (i = 0; i < 7; i++) {
				if (!EC_POINT_dbl(group, P, P, ctx))
					goto err;
			}
		}
		if (!EC_POINT_add(group, T, T, generator, ctx))
			goto err;
	}

	ec_pre_comp->group = group;
	ec_pre_comp->w = 7;
	ec_pre_comp->precomp = precomp;

	if (!EC_EX_DATA_set_data(&group->extra_data, ec_pre_comp,
	    ecp_nistz256_pre_comp_dup, ecp_nistz256_pre_comp_free,
	    ecp_nistz256_pre_comp_clear_free)) {
		goto err;
	}

	ec_pre_comp = NULL;
	ret = 1;

 err:
	if (ctx != NULL)
		BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);

	ecp_nistz256_pre_comp_free(ec_pre_comp);
	free(precomp);
	EC_POINT_free(P);
	EC_POINT_free(T);
	return ret;
}

static int
ecp_nistz256_set_from_affine(EC_POINT *out, const EC_GROUP *group,
    const P256_POINT_AFFINE *in, BN_CTX *ctx)
{
	BIGNUM x, y;
	BN_ULONG d_x[P256_LIMBS], d_y[P256_LIMBS];
	int ret = 0;

	memcpy(d_x, in->X, sizeof(d_x));
	x.d = d_x;
	x.dmax = x.top = P256_LIMBS;
	x.neg = 0;
	x.flags = BN_FLG_STATIC_DATA;

	memcpy(d_y, in->Y, sizeof(d_y));
	y.d = d_y;
	y.dmax = y.top = P256_LIMBS;
	y.neg = 0;
	y.flags = BN_FLG_STATIC_DATA;

	ret = EC_POINT_set_affine_coordinates_GFp(group, out, &x, &y, ctx);

	return ret;
}

/* r = scalar*G + sum(scalars[i]*points[i]) */
static int
ecp_nistz256_points_mul(const EC_GROUP *group, EC_POINT *r,
    const BIGNUM *scalar, size_t num, const EC_POINT *points[],
    const BIGNUM *scalars[], BN_CTX *ctx)
{
	int ret = 0, no_precomp_for_generator = 0, p_is_infinity = 0;
	size_t j;
	unsigned char p_str[33] = { 0 };
	const PRECOMP256_ROW *precomp = NULL;
	const EC_PRE_COMP *ec_pre_comp = NULL;
	const EC_POINT *generator = NULL;
	unsigned int i = 0, index = 0;
	BN_CTX *new_ctx = NULL;
	const BIGNUM **new_scalars = NULL;
	const EC_POINT **new_points = NULL;
	const unsigned int window_size = 7;
	const unsigned int mask = (1 << (window_size + 1)) - 1;
	unsigned int wvalue;
	/* avoid warning about ignored alignment for stack variable */
#if defined(__GNUC__) && !defined(__OpenBSD__)
	ALIGN32
#endif
	union {
		P256_POINT p;
		P256_POINT_AFFINE a;
	} t, p;
	BIGNUM *tmp_scalar;

	if (group->meth != r->meth) {
		ECerror(EC_R_INCOMPATIBLE_OBJECTS);
		return 0;
	}

	if (scalar == NULL && num == 0)
		return EC_POINT_set_to_infinity(group, r);

	for (j = 0; j < num; j++) {
		if (group->meth != points[j]->meth) {
			ECerror(EC_R_INCOMPATIBLE_OBJECTS);
			return 0;
		}
	}

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			goto err;
	}

	BN_CTX_start(ctx);

	if (scalar) {
		generator = EC_GROUP_get0_generator(group);
		if (generator == NULL) {
			ECerror(EC_R_UNDEFINED_GENERATOR);
			goto err;
		}

		/* look if we can use precomputed multiples of generator */
		ec_pre_comp = EC_EX_DATA_get_data(group->extra_data,
		    ecp_nistz256_pre_comp_dup, ecp_nistz256_pre_comp_free,
		    ecp_nistz256_pre_comp_clear_free);
		if (ec_pre_comp != NULL) {
			/*
			 * If there is a precomputed table for the generator,
			 * check that it was generated with the same generator.
			 */
			EC_POINT *pre_comp_generator = EC_POINT_new(group);
			if (pre_comp_generator == NULL)
				goto err;

			if (!ecp_nistz256_set_from_affine(pre_comp_generator,
			      group, ec_pre_comp->precomp[0], ctx)) {
				EC_POINT_free(pre_comp_generator);
				goto err;
			}

			if (0 == EC_POINT_cmp(group, generator,
			      pre_comp_generator, ctx))
				precomp = (const PRECOMP256_ROW *)
				    ec_pre_comp->precomp;

			EC_POINT_free(pre_comp_generator);
		}

		if (precomp == NULL && ecp_nistz256_is_affine_G(generator)) {
			/*
			 * If there is no precomputed data, but the generator
			 * is the default, a hardcoded table of precomputed
			 * data is used. This is because applications, such as
			 * Apache, do not use EC_KEY_precompute_mult.
			 */
			precomp =
			    (const PRECOMP256_ROW *)ecp_nistz256_precomputed;
		}

		if (precomp) {
			if (BN_num_bits(scalar) > 256 ||
			    BN_is_negative(scalar)) {
				if ((tmp_scalar = BN_CTX_get(ctx)) == NULL)
					goto err;

				if (!BN_nnmod(tmp_scalar, scalar, &group->order,
				      ctx)) {
					ECerror(ERR_R_BN_LIB);
					goto err;
				}
				scalar = tmp_scalar;
			}

			for (i = 0; i < scalar->top * BN_BYTES; i += BN_BYTES) {
				BN_ULONG d = scalar->d[i / BN_BYTES];

				p_str[i + 0] = d & 0xff;
				p_str[i + 1] = (d >> 8) & 0xff;
				p_str[i + 2] = (d >> 16) & 0xff;
				p_str[i + 3] = (d >> 24) & 0xff;
				if (BN_BYTES == 8) {
					d >>= 32;
					p_str[i + 4] = d & 0xff;
					p_str[i + 5] = (d >> 8) & 0xff;
					p_str[i + 6] = (d >> 16) & 0xff;
					p_str[i + 7] = (d >> 24) & 0xff;
				}
			}

			for (; i < 33; i++)
				p_str[i] = 0;

			/* First window */
			wvalue = (p_str[0] << 1) & mask;
			index += window_size;

			wvalue = _booth_recode_w7(wvalue);

			ecp_nistz256_select_w7(&p.a, precomp[0], wvalue >> 1);

			ecp_nistz256_neg(p.p.Z, p.p.Y);
			copy_conditional(p.p.Y, p.p.Z, wvalue & 1);

			/*
			 * Since affine infinity is encoded as (0,0) and
			 * Jacobian is (,,0), we need to harmonize them
			 * by assigning "one" or zero to Z.
			 */
			BN_ULONG infty;
			infty = (p.p.X[0] | p.p.X[1] | p.p.X[2] | p.p.X[3] |
			         p.p.Y[0] | p.p.Y[1] | p.p.Y[2] | p.p.Y[3]);
			if (P256_LIMBS == 8)
				infty |=
				    (p.p.X[4] | p.p.X[5] | p.p.X[6] | p.p.X[7] |
				     p.p.Y[4] | p.p.Y[5] | p.p.Y[6] | p.p.Y[7]);

			infty = 0 - is_zero(infty);
			infty = ~infty;

			p.p.Z[0] = ONE[0] & infty;
			p.p.Z[1] = ONE[1] & infty;
			p.p.Z[2] = ONE[2] & infty;
			p.p.Z[3] = ONE[3] & infty;
			if (P256_LIMBS == 8) {
				p.p.Z[4] = ONE[4] & infty;
				p.p.Z[5] = ONE[5] & infty;
				p.p.Z[6] = ONE[6] & infty;
				p.p.Z[7] = ONE[7] & infty;
			}

			for (i = 1; i < 37; i++) {
				unsigned int off = (index - 1) / 8;
				wvalue = p_str[off] | p_str[off + 1] << 8;
				wvalue = (wvalue >> ((index - 1) % 8)) & mask;
				index += window_size;

				wvalue = _booth_recode_w7(wvalue);

				ecp_nistz256_select_w7(&t.a, precomp[i],
				    wvalue >> 1);

				ecp_nistz256_neg(t.p.Z, t.a.Y);
				copy_conditional(t.a.Y, t.p.Z, wvalue & 1);

				ecp_nistz256_point_add_affine(&p.p, &p.p, &t.a);
			}
		} else {
			p_is_infinity = 1;
			no_precomp_for_generator = 1;
		}
	} else
		p_is_infinity = 1;

	if (no_precomp_for_generator) {
		/*
		 * Without a precomputed table for the generator, it has to be
		 * handled like a normal point.
		 */
		new_scalars = reallocarray(NULL, num + 1, sizeof(BIGNUM *));
		new_points = reallocarray(NULL, num + 1, sizeof(EC_POINT *));
		if (new_scalars == NULL || new_points == NULL) {
			ECerror(ERR_R_MALLOC_FAILURE);
			goto err;
		}

		memcpy(new_scalars, scalars, num * sizeof(BIGNUM *));
		new_scalars[num] = scalar;
		memcpy(new_points, points, num * sizeof(EC_POINT *));
		new_points[num] = generator;

		scalars = new_scalars;
		points = new_points;
		num++;
	}

	if (num != 0) {
		P256_POINT *out = &t.p;
		if (p_is_infinity)
			out = &p.p;

		if (!ecp_nistz256_windowed_mul(group, out, scalars, points, num,
		      ctx))
			goto err;

		if (!p_is_infinity)
			ecp_nistz256_point_add(&p.p, &p.p, out);
	}

	/* Not constant-time, but we're only operating on the public output. */
	if (!ecp_nistz256_set_words(&r->X, p.p.X) ||
	    !ecp_nistz256_set_words(&r->Y, p.p.Y) ||
	    !ecp_nistz256_set_words(&r->Z, p.p.Z)) {
		goto err;
	}
	r->Z_is_one = is_one(&r->Z) & 1;

	ret = 1;

 err:
	if (ctx)
		BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	free(new_points);
	free(new_scalars);
	return ret;
}

static int
ecp_nistz256_get_affine(const EC_GROUP *group, const EC_POINT *point,
    BIGNUM *x, BIGNUM *y, BN_CTX *ctx)
{
	BN_ULONG z_inv2[P256_LIMBS];
	BN_ULONG z_inv3[P256_LIMBS];
	BN_ULONG point_x[P256_LIMBS], point_y[P256_LIMBS], point_z[P256_LIMBS];

	if (EC_POINT_is_at_infinity(group, point)) {
		ECerror(EC_R_POINT_AT_INFINITY);
		return 0;
	}

	if (!ecp_nistz256_bignum_to_field_elem(point_x, &point->X) ||
	    !ecp_nistz256_bignum_to_field_elem(point_y, &point->Y) ||
	    !ecp_nistz256_bignum_to_field_elem(point_z, &point->Z)) {
		ECerror(EC_R_COORDINATES_OUT_OF_RANGE);
		return 0;
	}

	ecp_nistz256_mod_inverse(z_inv3, point_z);
	ecp_nistz256_sqr_mont(z_inv2, z_inv3);

	/*
	 * Unlike the |BN_mod_mul_montgomery|-based implementation, we cannot
	 * factor out the two calls to |ecp_nistz256_from_mont| into one call,
	 * because |ecp_nistz256_from_mont| must be the last operation to
	 * ensure that the result is fully reduced mod P.
	 */
	if (x != NULL) {
		BN_ULONG x_aff[P256_LIMBS];
		BN_ULONG x_ret[P256_LIMBS];

		ecp_nistz256_mul_mont(x_aff, z_inv2, point_x);
		ecp_nistz256_from_mont(x_ret, x_aff);
		if (!ecp_nistz256_set_words(x, x_ret))
			return 0;
	}

	if (y != NULL) {
		BN_ULONG y_aff[P256_LIMBS];
		BN_ULONG y_ret[P256_LIMBS];

		ecp_nistz256_mul_mont(z_inv3, z_inv3, z_inv2);
		ecp_nistz256_mul_mont(y_aff, z_inv3, point_y);
		ecp_nistz256_from_mont(y_ret, y_aff);
		if (!ecp_nistz256_set_words(y, y_ret))
			return 0;
	}

	return 1;
}

static EC_PRE_COMP *
ecp_nistz256_pre_comp_new(const EC_GROUP *group)
{
	EC_PRE_COMP *ret;

	if (group == NULL)
		return NULL;

	ret = (EC_PRE_COMP *)malloc(sizeof(EC_PRE_COMP));
	if (ret == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return ret;
	}

	ret->group = group;
	ret->w = 6;		/* default */
	ret->precomp = NULL;
	ret->references = 1;
	return ret;
}

static void *
ecp_nistz256_pre_comp_dup(void *src_)
{
	EC_PRE_COMP *src = src_;

	/* no need to actually copy, these objects never change! */
	CRYPTO_add(&src->references, 1, CRYPTO_LOCK_EC_PRE_COMP);

	return src_;
}

static void
ecp_nistz256_pre_comp_free(void *pre_)
{
	int i;
	EC_PRE_COMP *pre = pre_;

	if (pre == NULL)
		return;

	i = CRYPTO_add(&pre->references, -1, CRYPTO_LOCK_EC_PRE_COMP);
	if (i > 0)
		return;

	free(pre->precomp);
	free(pre);
}

static void
ecp_nistz256_pre_comp_clear_free(void *pre_)
{
	int i;
	EC_PRE_COMP *pre = pre_;

	if (pre == NULL)
		return;

	i = CRYPTO_add(&pre->references, -1, CRYPTO_LOCK_EC_PRE_COMP);
	if (i > 0)
		return;

	if (pre->precomp != NULL) {
		/*
		 * LSSL XXX
		 * The original OpenSSL code uses an obfuscated
		 * computation which is intended to be
		 *   37 * (1 << pre->w) * sizeof(P256_POINT_AFFINE)
		 * here, but the only place where we allocate this uses
		 * PRECOMP256_ROW (i.e. 64 P256_POINT_AFFINE) but sets w == 7.
		 */
		freezero(pre->precomp, 37 * sizeof(PRECOMP256_ROW));
	}
	freezero(pre, sizeof *pre);
}

static int
ecp_nistz256_window_have_precompute_mult(const EC_GROUP *group)
{
	/* There is a hard-coded table for the default generator. */
	const EC_POINT *generator = EC_GROUP_get0_generator(group);
	if (generator != NULL && ecp_nistz256_is_affine_G(generator)) {
		/* There is a hard-coded table for the default generator. */
		return 1;
	}

	return
	    EC_EX_DATA_get_data(group->extra_data, ecp_nistz256_pre_comp_dup,
	      ecp_nistz256_pre_comp_free, ecp_nistz256_pre_comp_clear_free) !=
	    NULL;
}

const EC_METHOD *
EC_GFp_nistz256_method(void)
{
	static const EC_METHOD ret = {
		.flags = EC_FLAGS_DEFAULT_OCT,
		.field_type = NID_X9_62_prime_field,
		.group_init = ec_GFp_mont_group_init,
		.group_finish = ec_GFp_mont_group_finish,
		.group_clear_finish = ec_GFp_mont_group_clear_finish,
		.group_copy = ec_GFp_mont_group_copy,
		.group_set_curve = ec_GFp_mont_group_set_curve,
		.group_get_curve = ec_GFp_simple_group_get_curve,
		.group_get_degree = ec_GFp_simple_group_get_degree,
		.group_check_discriminant =
		    ec_GFp_simple_group_check_discriminant,
		.point_init = ec_GFp_simple_point_init,
		.point_finish = ec_GFp_simple_point_finish,
		.point_clear_finish = ec_GFp_simple_point_clear_finish,
		.point_copy = ec_GFp_simple_point_copy,
		.point_set_to_infinity = ec_GFp_simple_point_set_to_infinity,
		.point_set_Jprojective_coordinates_GFp =
		    ec_GFp_simple_set_Jprojective_coordinates_GFp,
		.point_get_Jprojective_coordinates_GFp =
		    ec_GFp_simple_get_Jprojective_coordinates_GFp,
		.point_set_affine_coordinates =
		    ec_GFp_simple_point_set_affine_coordinates,
		.point_get_affine_coordinates = ecp_nistz256_get_affine,
		.add = ec_GFp_simple_add,
		.dbl = ec_GFp_simple_dbl,
		.invert = ec_GFp_simple_invert,
		.is_at_infinity = ec_GFp_simple_is_at_infinity,
		.is_on_curve = ec_GFp_simple_is_on_curve,
		.point_cmp = ec_GFp_simple_cmp,
		.make_affine = ec_GFp_simple_make_affine,
		.points_make_affine = ec_GFp_simple_points_make_affine,
		.mul = ecp_nistz256_points_mul,
		.precompute_mult = ecp_nistz256_mult_precompute,
		.have_precompute_mult =
		    ecp_nistz256_window_have_precompute_mult,
		.field_mul = ec_GFp_mont_field_mul,
		.field_sqr = ec_GFp_mont_field_sqr,
		.field_encode = ec_GFp_mont_field_encode,
		.field_decode = ec_GFp_mont_field_decode,
		.field_set_to_one = ec_GFp_mont_field_set_to_one,
		.blind_coordinates = NULL,
	};

	return &ret;
}
