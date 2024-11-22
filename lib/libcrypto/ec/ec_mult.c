/* $OpenBSD: ec_mult.c,v 1.42 2024/11/22 14:59:40 tb Exp $ */
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

#include <stdlib.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "bn_local.h"
#include "ec_local.h"

/*
 * This file implements the wNAF-based interleaving multi-exponentation method
 * (<URL:http://www.informatik.tu-darmstadt.de/TI/Mitarbeiter/moeller.html#multiexp>);
 * for multiplication with precomputation, we use wNAF splitting
 * (<URL:http://www.informatik.tu-darmstadt.de/TI/Mitarbeiter/moeller.html#fastexp>).
 */

/* Determine the modified width-(w+1) Non-Adjacent Form (wNAF) of 'scalar'.
 * This is an array  r[]  of values that are either zero or odd with an
 * absolute value less than  2^w  satisfying
 *     scalar = \sum_j r[j]*2^j
 * where at most one of any  w+1  consecutive digits is non-zero
 * with the exception that the most significant digit may be only
 * w-1 zeros away from that next non-zero digit.
 */
static signed char *
compute_wNAF(const BIGNUM *scalar, int w, size_t *ret_len)
{
	int window_val;
	int ok = 0;
	signed char *r = NULL;
	int sign = 1;
	int bit, next_bit, mask;
	size_t len = 0, j;

	if (BN_is_zero(scalar)) {
		r = malloc(1);
		if (!r) {
			ECerror(ERR_R_MALLOC_FAILURE);
			goto err;
		}
		r[0] = 0;
		*ret_len = 1;
		return r;
	}
	if (w <= 0 || w > 7) {
		/* 'signed char' can represent integers with
		 * absolute values less than 2^7 */
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}
	bit = 1 << w;		/* at most 128 */
	next_bit = bit << 1;	/* at most 256 */
	mask = next_bit - 1;	/* at most 255 */

	if (BN_is_negative(scalar)) {
		sign = -1;
	}
	if (scalar->d == NULL || scalar->top == 0) {
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}
	len = BN_num_bits(scalar);
	r = malloc(len + 1);	/* modified wNAF may be one digit longer than
				 * binary representation (*ret_len will be
				 * set to the actual length, i.e. at most
				 * BN_num_bits(scalar) + 1) */
	if (r == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	window_val = scalar->d[0] & mask;
	j = 0;
	while ((window_val != 0) || (j + w + 1 < len)) {
		/* if j+w+1 >= len, window_val will not increase */
		int digit = 0;

		/* 0 <= window_val <= 2^(w+1) */
		if (window_val & 1) {
			/* 0 < window_val < 2^(w+1) */
			if (window_val & bit) {
				digit = window_val - next_bit;	/* -2^w < digit < 0 */

#if 1				/* modified wNAF */
				if (j + w + 1 >= len) {
					/*
					 * special case for generating
					 * modified wNAFs: no new bits will
					 * be added into window_val, so using
					 * a positive digit here will
					 * decrease the total length of the
					 * representation
					 */

					digit = window_val & (mask >> 1);	/* 0 < digit < 2^w */
				}
#endif
			} else {
				digit = window_val;	/* 0 < digit < 2^w */
			}

			if (digit <= -bit || digit >= bit || !(digit & 1)) {
				ECerror(ERR_R_INTERNAL_ERROR);
				goto err;
			}
			window_val -= digit;

			/*
			 * now window_val is 0 or 2^(w+1) in standard wNAF
			 * generation; for modified window NAFs, it may also
			 * be 2^w
			 */
			if (window_val != 0 && window_val != next_bit && window_val != bit) {
				ECerror(ERR_R_INTERNAL_ERROR);
				goto err;
			}
		}
		r[j++] = sign * digit;

		window_val >>= 1;
		window_val += bit * BN_is_bit_set(scalar, j + w);

		if (window_val > next_bit) {
			ECerror(ERR_R_INTERNAL_ERROR);
			goto err;
		}
	}

	if (j > len + 1) {
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}
	len = j;
	ok = 1;

 err:
	if (!ok) {
		free(r);
		r = NULL;
	}
	if (ok)
		*ret_len = len;
	return r;
}


/* TODO: table should be optimised for the wNAF-based implementation,
 *       sometimes smaller windows will give better performance
 *       (thus the boundaries should be increased)
 */
#define EC_window_bits_for_scalar_size(b) \
		((size_t) \
		 ((b) >= 2000 ? 6 : \
		  (b) >=  800 ? 5 : \
		  (b) >=  300 ? 4 : \
		  (b) >=   70 ? 3 : \
		  (b) >=   20 ? 2 : \
		  1))

static int
ec_compute_odd_multiples(const EC_GROUP *group, const EC_POINT *point,
    EC_POINT **row, size_t len, BN_CTX *ctx)
{
	EC_POINT *doubled = NULL;
	size_t i;
	int ret = 0;

	if (len < 1)
		goto err;

	if ((row[0] = EC_POINT_dup(point, group)) == NULL)
		goto err;

	if ((doubled = EC_POINT_new(group)) == NULL)
		goto err;
	if (!EC_POINT_dbl(group, doubled, point, ctx))
		goto err;
	for (i = 1; i < len; i++) {
		if ((row[i] = EC_POINT_new(group)) == NULL)
			goto err;
		if (!EC_POINT_add(group, row[i], row[i - 1], doubled, ctx))
			goto err;
	}

	ret = 1;

 err:
	EC_POINT_free(doubled);

	return ret;
}

/*
 * This computes the wNAF representation of m and n and uses the window size to
 * precompute the two rows of odd multiples of point and generator. On success,
 * out_val owns the out_val_len points in the two rows.
 *
 * XXX - the only reason we need a single array is to be able to pass it to
 * EC_POINTs_make_affine(). Consider writing a suitable variant that doesn't
 * require such grotesque gymnastics.
 */

static int
ec_wNAF_precompute(const EC_GROUP *group, const BIGNUM *m, const EC_POINT *point,
    const BIGNUM *n, signed char *wNAF[2], size_t wNAF_len[2], EC_POINT **row[2],
    EC_POINT ***out_val, size_t *out_val_len, BN_CTX *ctx)
{
	EC_POINT **val = NULL;
	size_t val_len = 0;
	const EC_POINT *generator;
	size_t wsize[2] = { 0 };
	size_t i, len0, len1;
	int ret = 0;

	*out_val = NULL;
	*out_val_len = 0;

	if ((generator = EC_GROUP_get0_generator(group)) == NULL) {
		ECerror(EC_R_UNDEFINED_GENERATOR);
		goto err;
	}

	wsize[0] = EC_window_bits_for_scalar_size(BN_num_bits(n));
	if ((wNAF[0] = compute_wNAF(n, wsize[0], &wNAF_len[0])) == NULL)
		goto err;

	wsize[1] = EC_window_bits_for_scalar_size(BN_num_bits(m));
	if ((wNAF[1] = compute_wNAF(m, wsize[1], &wNAF_len[1])) == NULL)
		goto err;

	len0 = 1 << (wsize[0] - 1);
	len1 = 1 << (wsize[1] - 1);

	if ((val = calloc(len0 + len1, sizeof(*val))) == NULL) {
		ECerror(ERR_R_MALLOC_FAILURE);
		goto err;
	}
	val_len = len0 + len1;

	row[0] = &val[0];
	row[1] = &val[len0];

	if (!ec_compute_odd_multiples(group, point, row[0], len0, ctx))
		goto err;
	if (!ec_compute_odd_multiples(group, generator, row[1], len1, ctx))
		goto err;

	if (!EC_POINTs_make_affine(group, val_len, val, ctx))
		goto err;

	*out_val = val;
	val = NULL;

	*out_val_len = val_len;
	val_len = 0;

	ret = 1;

 err:
	for (i = 0; i < val_len; i++)
		EC_POINT_free(val[i]);
	free(val);

	return ret;
}

/*
 * Compute r = generator * m + point * n in non-constant time.
 */

int
ec_wNAF_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *m,
    const EC_POINT *point, const BIGNUM *n, BN_CTX *ctx)
{
	signed char *wNAF[2] = { 0 };
	size_t wNAF_len[2] = { 0 };
	EC_POINT **row[2] = { 0 };
	EC_POINT **val = NULL;
	size_t val_len = 0;
	size_t i;
	int k;
	int r_is_inverted = 0;
	size_t max_len = 0;
	int ret = 0;

	if (m == NULL || n == NULL) {
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}
	if (group->meth != r->meth || group->meth != point->meth) {
		ECerror(EC_R_INCOMPATIBLE_OBJECTS);
		goto err;
	}

	if (!ec_wNAF_precompute(group, m, point, n, wNAF, wNAF_len, row,
	    &val, &val_len, ctx))
		goto err;

	max_len = wNAF_len[0];
	if (wNAF_len[1] > max_len)
		max_len = wNAF_len[1];

	/*
	 * Set r to the neutral element. Scan through the wNAF representations
	 * of m and n, starting at the most significant digit. Double r and for
	 * each wNAF digit of m add the digit times the point, and for each
	 * wNAF digit of n add the digit times the generator, adjusting the
	 * signs as appropriate.
	 */

	if (!EC_POINT_set_to_infinity(group, r))
		goto err;

	for (k = max_len - 1; k >= 0; k--) {
		if (!EC_POINT_dbl(group, r, r, ctx))
			goto err;

		for (i = 0; i < 2; i++) {
			int digit;
			int is_neg = 0;

			if (k >= wNAF_len[i])
				continue;

			if ((digit = wNAF[i][k]) == 0)
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

			if (!EC_POINT_add(group, r, r, row[i][digit >> 1], ctx))
				goto err;
		}
	}

	if (r_is_inverted) {
		if (!EC_POINT_invert(group, r, ctx))
			goto err;
	}

	ret = 1;

 err:
	free(wNAF[0]);
	free(wNAF[1]);
	for (i = 0; i < val_len; i++)
		EC_POINT_free(val[i]);
	free(val);

	return ret;
}
