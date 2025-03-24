/* $OpenBSD: ec_mult.c,v 1.58 2025/03/24 13:07:04 jsing Exp $ */
/*
 * Originally written by Bodo Moeller and Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * Portions of this software developed by SUN MICROSYSTEMS, INC.,
 * and contributed to the OpenSSL project.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "ec_local.h"

/* Holds the wNAF digits of bn and the corresponding odd multiples of point. */
struct ec_wnaf {
	signed char *digits;
	size_t num_digits;
	EC_POINT **multiples;
	size_t num_multiples;
};

static int
ec_window_bits(const BIGNUM *bn)
{
	int bits = BN_num_bits(bn);

	if (bits >= 2000)
		return 6;
	if (bits >= 800)
		return 5;
	if (bits >= 300)
		return 4;
	if (bits >= 70)
		return 3;
	if (bits >= 20)
		return 2;

	return 1;
}

/*
 * Width-(w+1) non-adjacent form of bn = \sum_j n_j 2^j, with odd n_j,
 * where at most one of any (w+1) consecutive digits is non-zero.
 */

static int
ec_compute_wnaf(const BIGNUM *bn, signed char *digits, size_t num_digits)
{
	int digit, bit, next, sign, wbits, window;
	size_t i;
	int ret = 0;

	if (num_digits != BN_num_bits(bn) + 1) {
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}

	sign = BN_is_negative(bn) ? -1 : 1;

	wbits = ec_window_bits(bn);

	bit = 1 << wbits;
	next = bit << 1;

	/* Extract the wbits + 1 lowest bits from bn into window. */
	window = 0;
	for (i = 0; i < wbits + 1; i++) {
		if (BN_is_bit_set(bn, i))
			window |= (1 << i);
	}

	/* Instead of bn >>= 1 in each iteration, slide window to the left. */
	for (i = 0; i < num_digits; i++) {
		digit = 0;

		/*
		 * If window is odd, the i-th wNAF digit is window (mods 2^w),
		 * where mods is the signed modulo in (-2^w-1, 2^w-1]. Subtract
		 * the digit from window, so window is 0 or next, and add the
		 * digit to the wNAF digits.
		 */
		if ((window & 1) != 0) {
			digit = window;
			if ((window & bit) != 0)
				digit = window - next;
			window -= digit;
		}

		digits[i] = sign * digit;

		/* Slide the window to the left. */
		window >>= 1;
		window += bit * BN_is_bit_set(bn, i + wbits + 1);
	}

	ret = 1;

 err:
	return ret;
}

static int
ec_compute_odd_multiples(const EC_GROUP *group, const EC_POINT *point,
    EC_POINT **multiples, size_t num_multiples, BN_CTX *ctx)
{
	EC_POINT *doubled = NULL;
	size_t i;
	int ret = 0;

	if (num_multiples < 1)
		goto err;

	if ((multiples[0] = EC_POINT_dup(point, group)) == NULL)
		goto err;

	if ((doubled = EC_POINT_new(group)) == NULL)
		goto err;
	if (!EC_POINT_dbl(group, doubled, point, ctx))
		goto err;
	for (i = 1; i < num_multiples; i++) {
		if ((multiples[i] = EC_POINT_new(group)) == NULL)
			goto err;
		if (!EC_POINT_add(group, multiples[i], multiples[i - 1], doubled,
		    ctx))
			goto err;
	}

	ret = 1;

 err:
	EC_POINT_free(doubled);

	return ret;
}

/*
 * Bring multiples held in wnaf0 and wnaf1 simultaneously into affine form
 * so that the operations in the loop in ec_wnaf_mul() can take fast paths.
 */

static int
ec_normalize_points(const EC_GROUP *group, struct ec_wnaf *wnaf0,
    struct ec_wnaf *wnaf1, BN_CTX *ctx)
{
	EC_POINT **points0 = wnaf0->multiples, **points1 = wnaf1->multiples;
	size_t len0 = wnaf0->num_multiples, len1 = wnaf1->num_multiples;
	EC_POINT **val = NULL;
	size_t len = 0;
	int ret = 0;

	if (len1 > SIZE_MAX - len0)
		goto err;
	len = len0 + len1;

	if ((val = calloc(len, sizeof(*val))) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	memcpy(&val[0], points0, sizeof(*val) * len0);
	memcpy(&val[len0], points1, sizeof(*val) * len1);

	if (!group->meth->points_make_affine(group, len, val, ctx))
		goto err;

	ret = 1;

 err:
	free(val);

	return ret;
}

static void
ec_points_free(EC_POINT **points, size_t num_points)
{
	size_t i;

	if (points == NULL)
		return;

	for (i = 0; i < num_points; i++)
		EC_POINT_free(points[i]);
	free(points);
}

static void
ec_wnaf_free(struct ec_wnaf *wnaf)
{
	if (wnaf == NULL)
		return;

	free(wnaf->digits);
	ec_points_free(wnaf->multiples, wnaf->num_multiples);
	free(wnaf);
}

/*
 * Calculate wNAF splitting of bn and the corresponding odd multiples of point.
 */

static struct ec_wnaf *
ec_wnaf_new(const EC_GROUP *group, const BIGNUM *scalar, const EC_POINT *point,
    BN_CTX *ctx)
{
	struct ec_wnaf *wnaf;

	if ((wnaf = calloc(1, sizeof(*wnaf))) == NULL)
		goto err;

	wnaf->num_digits = BN_num_bits(scalar) + 1;
	if ((wnaf->digits = calloc(wnaf->num_digits,
	    sizeof(*wnaf->digits))) == NULL)
		goto err;

	if (!ec_compute_wnaf(scalar, wnaf->digits, wnaf->num_digits))
		goto err;

	wnaf->num_multiples = 1ULL << (ec_window_bits(scalar) - 1);
	if ((wnaf->multiples = calloc(wnaf->num_multiples,
	    sizeof(*wnaf->multiples))) == NULL)
		goto err;

	if (!ec_compute_odd_multiples(group, point, wnaf->multiples,
	    wnaf->num_multiples, ctx))
		goto err;

	return wnaf;

 err:
	ec_wnaf_free(wnaf);

	return NULL;
}

static signed char
ec_wnaf_digit(struct ec_wnaf *wnaf, size_t idx)
{
	if (idx >= wnaf->num_digits)
		return 0;

	return wnaf->digits[idx];
}

static const EC_POINT *
ec_wnaf_multiple(struct ec_wnaf *wnaf, signed char digit)
{
	if (digit < 0)
		return NULL;
	if (digit >= 2 * wnaf->num_multiples)
		return NULL;

	return wnaf->multiples[digit >> 1];
}

/*
 * Compute r = scalar1 * point1 + scalar2 * point2 in non-constant time.
 */

int
ec_wnaf_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar1,
    const EC_POINT *point1, const BIGNUM *scalar2, const EC_POINT *point2,
    BN_CTX *ctx)
{
	struct ec_wnaf *wnaf[2] = { NULL, NULL };
	size_t i;
	int k;
	int r_is_inverted = 0;
	size_t num_digits;
	int ret = 0;

	if (scalar1 == NULL || scalar2 == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}
	if (group->meth != r->meth || group->meth != point1->meth ||
	    group->meth != point2->meth) {
		ECerror(EC_R_INCOMPATIBLE_OBJECTS);
		goto err;
	}

	if ((wnaf[0] = ec_wnaf_new(group, scalar1, point1, ctx)) == NULL)
		goto err;
	if ((wnaf[1] = ec_wnaf_new(group, scalar2, point2, ctx)) == NULL)
		goto err;

	if (!ec_normalize_points(group, wnaf[0], wnaf[1], ctx))
		goto err;

	num_digits = wnaf[0]->num_digits;
	if (wnaf[1]->num_digits > num_digits)
		num_digits = wnaf[1]->num_digits;

	/*
	 * Set r to the neutral element. Scan through the wNAF representations
	 * of m and n, starting at the most significant digit. Double r and for
	 * each wNAF digit of scalar1 add the digit times point1, and for each
	 * wNAF digit of scalar2 add the digit times point2, adjusting the signs
	 * as appropriate.
	 */

	if (!EC_POINT_set_to_infinity(group, r))
		goto err;

	for (k = num_digits - 1; k >= 0; k--) {
		if (!EC_POINT_dbl(group, r, r, ctx))
			goto err;

		for (i = 0; i < 2; i++) {
			const EC_POINT *multiple;
			signed char digit;
			int is_neg = 0;

			if ((digit = ec_wnaf_digit(wnaf[i], k)) == 0)
				continue;

			if (digit < 0) {
				is_neg = 1;
				digit = -digit;
			}

			if (is_neg != r_is_inverted) {
				if (!EC_POINT_invert(group, r, ctx))
					goto err;
				r_is_inverted = !r_is_inverted;
			}

			if ((multiple = ec_wnaf_multiple(wnaf[i], digit)) == NULL)
				goto err;

			if (!EC_POINT_add(group, r, r, multiple, ctx))
				goto err;
		}
	}

	if (r_is_inverted) {
		if (!EC_POINT_invert(group, r, ctx))
			goto err;
	}

	ret = 1;

 err:
	ec_wnaf_free(wnaf[0]);
	ec_wnaf_free(wnaf[1]);

	return ret;
}
