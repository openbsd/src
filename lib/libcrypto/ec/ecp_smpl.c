/* $OpenBSD: ecp_smpl.c,v 1.34 2022/01/20 11:02:44 inoguchi Exp $ */
/* Includes code written by Lenka Fibikova <fibikova@exp-math.uni-essen.de>
 * for the OpenSSL project.
 * Includes code written by Bodo Moeller for the OpenSSL project.
*/
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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

#include <openssl/err.h>

#include "bn_lcl.h"
#include "ec_lcl.h"

const EC_METHOD *
EC_GFp_simple_method(void)
{
	static const EC_METHOD ret = {
		.flags = EC_FLAGS_DEFAULT_OCT,
		.field_type = NID_X9_62_prime_field,
		.group_init = ec_GFp_simple_group_init,
		.group_finish = ec_GFp_simple_group_finish,
		.group_clear_finish = ec_GFp_simple_group_clear_finish,
		.group_copy = ec_GFp_simple_group_copy,
		.group_set_curve = ec_GFp_simple_group_set_curve,
		.group_get_curve = ec_GFp_simple_group_get_curve,
		.group_get_degree = ec_GFp_simple_group_get_degree,
		.group_order_bits = ec_group_simple_order_bits,
		.group_check_discriminant =
		    ec_GFp_simple_group_check_discriminant,
		.point_init = ec_GFp_simple_point_init,
		.point_finish = ec_GFp_simple_point_finish,
		.point_clear_finish = ec_GFp_simple_point_clear_finish,
		.point_copy = ec_GFp_simple_point_copy,
		.point_set_to_infinity = ec_GFp_simple_point_set_to_infinity,
		.point_set_Jprojective_coordinates =
		    ec_GFp_simple_set_Jprojective_coordinates,
		.point_get_Jprojective_coordinates =
		    ec_GFp_simple_get_Jprojective_coordinates,
		.point_set_affine_coordinates =
		    ec_GFp_simple_point_set_affine_coordinates,
		.point_get_affine_coordinates =
		    ec_GFp_simple_point_get_affine_coordinates,
		.add = ec_GFp_simple_add,
		.dbl = ec_GFp_simple_dbl,
		.invert = ec_GFp_simple_invert,
		.is_at_infinity = ec_GFp_simple_is_at_infinity,
		.is_on_curve = ec_GFp_simple_is_on_curve,
		.point_cmp = ec_GFp_simple_cmp,
		.make_affine = ec_GFp_simple_make_affine,
		.points_make_affine = ec_GFp_simple_points_make_affine,
		.mul_generator_ct = ec_GFp_simple_mul_generator_ct,
		.mul_single_ct = ec_GFp_simple_mul_single_ct,
		.mul_double_nonct = ec_GFp_simple_mul_double_nonct,
		.field_mul = ec_GFp_simple_field_mul,
		.field_sqr = ec_GFp_simple_field_sqr,
		.blind_coordinates = ec_GFp_simple_blind_coordinates,
	};

	return &ret;
}


/* Most method functions in this file are designed to work with
 * non-trivial representations of field elements if necessary
 * (see ecp_mont.c): while standard modular addition and subtraction
 * are used, the field_mul and field_sqr methods will be used for
 * multiplication, and field_encode and field_decode (if defined)
 * will be used for converting between representations.

 * Functions ec_GFp_simple_points_make_affine() and
 * ec_GFp_simple_point_get_affine_coordinates() specifically assume
 * that if a non-trivial representation is used, it is a Montgomery
 * representation (i.e. 'encoding' means multiplying by some factor R).
 */


int
ec_GFp_simple_group_init(EC_GROUP * group)
{
	BN_init(&group->field);
	BN_init(&group->a);
	BN_init(&group->b);
	group->a_is_minus3 = 0;
	return 1;
}


void
ec_GFp_simple_group_finish(EC_GROUP * group)
{
	BN_free(&group->field);
	BN_free(&group->a);
	BN_free(&group->b);
}


void
ec_GFp_simple_group_clear_finish(EC_GROUP * group)
{
	BN_clear_free(&group->field);
	BN_clear_free(&group->a);
	BN_clear_free(&group->b);
}


int
ec_GFp_simple_group_copy(EC_GROUP * dest, const EC_GROUP * src)
{
	if (!BN_copy(&dest->field, &src->field))
		return 0;
	if (!BN_copy(&dest->a, &src->a))
		return 0;
	if (!BN_copy(&dest->b, &src->b))
		return 0;

	dest->a_is_minus3 = src->a_is_minus3;

	return 1;
}


int
ec_GFp_simple_group_set_curve(EC_GROUP * group,
    const BIGNUM * p, const BIGNUM * a, const BIGNUM * b, BN_CTX * ctx)
{
	int ret = 0;
	BN_CTX *new_ctx = NULL;
	BIGNUM *tmp_a;

	/* p must be a prime > 3 */
	if (BN_num_bits(p) <= 2 || !BN_is_odd(p)) {
		ECerror(EC_R_INVALID_FIELD);
		return 0;
	}
	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((tmp_a = BN_CTX_get(ctx)) == NULL)
		goto err;

	/* group->field */
	if (!BN_copy(&group->field, p))
		goto err;
	BN_set_negative(&group->field, 0);

	/* group->a */
	if (!BN_nnmod(tmp_a, a, p, ctx))
		goto err;
	if (group->meth->field_encode) {
		if (!group->meth->field_encode(group, &group->a, tmp_a, ctx))
			goto err;
	} else if (!BN_copy(&group->a, tmp_a))
		goto err;

	/* group->b */
	if (!BN_nnmod(&group->b, b, p, ctx))
		goto err;
	if (group->meth->field_encode)
		if (!group->meth->field_encode(group, &group->b, &group->b, ctx))
			goto err;

	/* group->a_is_minus3 */
	if (!BN_add_word(tmp_a, 3))
		goto err;
	group->a_is_minus3 = (0 == BN_cmp(tmp_a, &group->field));

	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_group_get_curve(const EC_GROUP * group, BIGNUM * p, BIGNUM * a, BIGNUM * b, BN_CTX * ctx)
{
	int ret = 0;
	BN_CTX *new_ctx = NULL;

	if (p != NULL) {
		if (!BN_copy(p, &group->field))
			return 0;
	}
	if (a != NULL || b != NULL) {
		if (group->meth->field_decode) {
			if (ctx == NULL) {
				ctx = new_ctx = BN_CTX_new();
				if (ctx == NULL)
					return 0;
			}
			if (a != NULL) {
				if (!group->meth->field_decode(group, a, &group->a, ctx))
					goto err;
			}
			if (b != NULL) {
				if (!group->meth->field_decode(group, b, &group->b, ctx))
					goto err;
			}
		} else {
			if (a != NULL) {
				if (!BN_copy(a, &group->a))
					goto err;
			}
			if (b != NULL) {
				if (!BN_copy(b, &group->b))
					goto err;
			}
		}
	}
	ret = 1;

 err:
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_group_get_degree(const EC_GROUP * group)
{
	return BN_num_bits(&group->field);
}


int
ec_GFp_simple_group_check_discriminant(const EC_GROUP * group, BN_CTX * ctx)
{
	int ret = 0;
	BIGNUM *a, *b, *order, *tmp_1, *tmp_2;
	const BIGNUM *p = &group->field;
	BN_CTX *new_ctx = NULL;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL) {
			ECerror(ERR_R_MALLOC_FAILURE);
			goto err;
		}
	}
	BN_CTX_start(ctx);
	if ((a = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((b = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp_1 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp_2 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((order = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (group->meth->field_decode) {
		if (!group->meth->field_decode(group, a, &group->a, ctx))
			goto err;
		if (!group->meth->field_decode(group, b, &group->b, ctx))
			goto err;
	} else {
		if (!BN_copy(a, &group->a))
			goto err;
		if (!BN_copy(b, &group->b))
			goto err;
	}

	/*
	 * check the discriminant: y^2 = x^3 + a*x + b is an elliptic curve
	 * <=> 4*a^3 + 27*b^2 != 0 (mod p) 0 =< a, b < p
	 */
	if (BN_is_zero(a)) {
		if (BN_is_zero(b))
			goto err;
	} else if (!BN_is_zero(b)) {
		if (!BN_mod_sqr(tmp_1, a, p, ctx))
			goto err;
		if (!BN_mod_mul(tmp_2, tmp_1, a, p, ctx))
			goto err;
		if (!BN_lshift(tmp_1, tmp_2, 2))
			goto err;
		/* tmp_1 = 4*a^3 */

		if (!BN_mod_sqr(tmp_2, b, p, ctx))
			goto err;
		if (!BN_mul_word(tmp_2, 27))
			goto err;
		/* tmp_2 = 27*b^2 */

		if (!BN_mod_add(a, tmp_1, tmp_2, p, ctx))
			goto err;
		if (BN_is_zero(a))
			goto err;
	}
	ret = 1;

 err:
	if (ctx != NULL)
		BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_point_init(EC_POINT * point)
{
	BN_init(&point->X);
	BN_init(&point->Y);
	BN_init(&point->Z);
	point->Z_is_one = 0;

	return 1;
}


void
ec_GFp_simple_point_finish(EC_POINT * point)
{
	BN_free(&point->X);
	BN_free(&point->Y);
	BN_free(&point->Z);
}


void
ec_GFp_simple_point_clear_finish(EC_POINT * point)
{
	BN_clear_free(&point->X);
	BN_clear_free(&point->Y);
	BN_clear_free(&point->Z);
	point->Z_is_one = 0;
}


int
ec_GFp_simple_point_copy(EC_POINT * dest, const EC_POINT * src)
{
	if (!BN_copy(&dest->X, &src->X))
		return 0;
	if (!BN_copy(&dest->Y, &src->Y))
		return 0;
	if (!BN_copy(&dest->Z, &src->Z))
		return 0;
	dest->Z_is_one = src->Z_is_one;

	return 1;
}


int
ec_GFp_simple_point_set_to_infinity(const EC_GROUP * group, EC_POINT * point)
{
	point->Z_is_one = 0;
	BN_zero(&point->Z);
	return 1;
}


int
ec_GFp_simple_set_Jprojective_coordinates(const EC_GROUP *group,
    EC_POINT *point, const BIGNUM *x, const BIGNUM *y, const BIGNUM *z,
    BN_CTX *ctx)
{
	BN_CTX *new_ctx = NULL;
	int ret = 0;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	if (x != NULL) {
		if (!BN_nnmod(&point->X, x, &group->field, ctx))
			goto err;
		if (group->meth->field_encode) {
			if (!group->meth->field_encode(group, &point->X, &point->X, ctx))
				goto err;
		}
	}
	if (y != NULL) {
		if (!BN_nnmod(&point->Y, y, &group->field, ctx))
			goto err;
		if (group->meth->field_encode) {
			if (!group->meth->field_encode(group, &point->Y, &point->Y, ctx))
				goto err;
		}
	}
	if (z != NULL) {
		int Z_is_one;

		if (!BN_nnmod(&point->Z, z, &group->field, ctx))
			goto err;
		Z_is_one = BN_is_one(&point->Z);
		if (group->meth->field_encode) {
			if (Z_is_one && (group->meth->field_set_to_one != 0)) {
				if (!group->meth->field_set_to_one(group, &point->Z, ctx))
					goto err;
			} else {
				if (!group->meth->field_encode(group, &point->Z, &point->Z, ctx))
					goto err;
			}
		}
		point->Z_is_one = Z_is_one;
	}
	ret = 1;

 err:
	BN_CTX_free(new_ctx);
	return ret;
}

int
ec_GFp_simple_get_Jprojective_coordinates(const EC_GROUP *group,
    const EC_POINT *point, BIGNUM *x, BIGNUM *y, BIGNUM *z, BN_CTX *ctx)
{
	BN_CTX *new_ctx = NULL;
	int ret = 0;

	if (group->meth->field_decode != 0) {
		if (ctx == NULL) {
			ctx = new_ctx = BN_CTX_new();
			if (ctx == NULL)
				return 0;
		}
		if (x != NULL) {
			if (!group->meth->field_decode(group, x, &point->X, ctx))
				goto err;
		}
		if (y != NULL) {
			if (!group->meth->field_decode(group, y, &point->Y, ctx))
				goto err;
		}
		if (z != NULL) {
			if (!group->meth->field_decode(group, z, &point->Z, ctx))
				goto err;
		}
	} else {
		if (x != NULL) {
			if (!BN_copy(x, &point->X))
				goto err;
		}
		if (y != NULL) {
			if (!BN_copy(y, &point->Y))
				goto err;
		}
		if (z != NULL) {
			if (!BN_copy(z, &point->Z))
				goto err;
		}
	}

	ret = 1;

 err:
	BN_CTX_free(new_ctx);
	return ret;
}

int
ec_GFp_simple_point_set_affine_coordinates(const EC_GROUP * group, EC_POINT * point,
    const BIGNUM * x, const BIGNUM * y, BN_CTX * ctx)
{
	if (x == NULL || y == NULL) {
		/* unlike for projective coordinates, we do not tolerate this */
		ECerror(ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}
	return EC_POINT_set_Jprojective_coordinates(group, point, x, y,
	    BN_value_one(), ctx);
}

int
ec_GFp_simple_point_get_affine_coordinates(const EC_GROUP * group, const EC_POINT * point,
    BIGNUM * x, BIGNUM * y, BN_CTX * ctx)
{
	BN_CTX *new_ctx = NULL;
	BIGNUM *Z, *Z_1, *Z_2, *Z_3;
	const BIGNUM *Z_;
	int ret = 0;

	if (EC_POINT_is_at_infinity(group, point) > 0) {
		ECerror(EC_R_POINT_AT_INFINITY);
		return 0;
	}
	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((Z = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((Z_1 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((Z_2 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((Z_3 = BN_CTX_get(ctx)) == NULL)
		goto err;

	/* transform  (X, Y, Z)  into  (x, y) := (X/Z^2, Y/Z^3) */

	if (group->meth->field_decode) {
		if (!group->meth->field_decode(group, Z, &point->Z, ctx))
			goto err;
		Z_ = Z;
	} else {
		Z_ = &point->Z;
	}

	if (BN_is_one(Z_)) {
		if (group->meth->field_decode) {
			if (x != NULL) {
				if (!group->meth->field_decode(group, x, &point->X, ctx))
					goto err;
			}
			if (y != NULL) {
				if (!group->meth->field_decode(group, y, &point->Y, ctx))
					goto err;
			}
		} else {
			if (x != NULL) {
				if (!BN_copy(x, &point->X))
					goto err;
			}
			if (y != NULL) {
				if (!BN_copy(y, &point->Y))
					goto err;
			}
		}
	} else {
		if (BN_mod_inverse_ct(Z_1, Z_, &group->field, ctx) == NULL) {
			ECerror(ERR_R_BN_LIB);
			goto err;
		}
		if (group->meth->field_encode == 0) {
			/* field_sqr works on standard representation */
			if (!group->meth->field_sqr(group, Z_2, Z_1, ctx))
				goto err;
		} else {
			if (!BN_mod_sqr(Z_2, Z_1, &group->field, ctx))
				goto err;
		}

		if (x != NULL) {
			/*
			 * in the Montgomery case, field_mul will cancel out
			 * Montgomery factor in X:
			 */
			if (!group->meth->field_mul(group, x, &point->X, Z_2, ctx))
				goto err;
		}
		if (y != NULL) {
			if (group->meth->field_encode == 0) {
				/* field_mul works on standard representation */
				if (!group->meth->field_mul(group, Z_3, Z_2, Z_1, ctx))
					goto err;
			} else {
				if (!BN_mod_mul(Z_3, Z_2, Z_1, &group->field, ctx))
					goto err;
			}

			/*
			 * in the Montgomery case, field_mul will cancel out
			 * Montgomery factor in Y:
			 */
			if (!group->meth->field_mul(group, y, &point->Y, Z_3, ctx))
				goto err;
		}
	}

	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}

int
ec_GFp_simple_add(const EC_GROUP * group, EC_POINT * r, const EC_POINT * a, const EC_POINT * b, BN_CTX * ctx)
{
	int (*field_mul) (const EC_GROUP *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
	int (*field_sqr) (const EC_GROUP *, BIGNUM *, const BIGNUM *, BN_CTX *);
	const BIGNUM *p;
	BN_CTX *new_ctx = NULL;
	BIGNUM *n0, *n1, *n2, *n3, *n4, *n5, *n6;
	int ret = 0;

	if (a == b)
		return EC_POINT_dbl(group, r, a, ctx);
	if (EC_POINT_is_at_infinity(group, a) > 0)
		return EC_POINT_copy(r, b);
	if (EC_POINT_is_at_infinity(group, b) > 0)
		return EC_POINT_copy(r, a);

	field_mul = group->meth->field_mul;
	field_sqr = group->meth->field_sqr;
	p = &group->field;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((n0 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n1 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n2 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n3 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n4 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n5 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((n6 = BN_CTX_get(ctx)) == NULL)
		goto end;

	/*
	 * Note that in this function we must not read components of 'a' or
	 * 'b' once we have written the corresponding components of 'r'. ('r'
	 * might be one of 'a' or 'b'.)
	 */

	/* n1, n2 */
	if (b->Z_is_one) {
		if (!BN_copy(n1, &a->X))
			goto end;
		if (!BN_copy(n2, &a->Y))
			goto end;
		/* n1 = X_a */
		/* n2 = Y_a */
	} else {
		if (!field_sqr(group, n0, &b->Z, ctx))
			goto end;
		if (!field_mul(group, n1, &a->X, n0, ctx))
			goto end;
		/* n1 = X_a * Z_b^2 */

		if (!field_mul(group, n0, n0, &b->Z, ctx))
			goto end;
		if (!field_mul(group, n2, &a->Y, n0, ctx))
			goto end;
		/* n2 = Y_a * Z_b^3 */
	}

	/* n3, n4 */
	if (a->Z_is_one) {
		if (!BN_copy(n3, &b->X))
			goto end;
		if (!BN_copy(n4, &b->Y))
			goto end;
		/* n3 = X_b */
		/* n4 = Y_b */
	} else {
		if (!field_sqr(group, n0, &a->Z, ctx))
			goto end;
		if (!field_mul(group, n3, &b->X, n0, ctx))
			goto end;
		/* n3 = X_b * Z_a^2 */

		if (!field_mul(group, n0, n0, &a->Z, ctx))
			goto end;
		if (!field_mul(group, n4, &b->Y, n0, ctx))
			goto end;
		/* n4 = Y_b * Z_a^3 */
	}

	/* n5, n6 */
	if (!BN_mod_sub_quick(n5, n1, n3, p))
		goto end;
	if (!BN_mod_sub_quick(n6, n2, n4, p))
		goto end;
	/* n5 = n1 - n3 */
	/* n6 = n2 - n4 */

	if (BN_is_zero(n5)) {
		if (BN_is_zero(n6)) {
			/* a is the same point as b */
			BN_CTX_end(ctx);
			ret = EC_POINT_dbl(group, r, a, ctx);
			ctx = NULL;
			goto end;
		} else {
			/* a is the inverse of b */
			BN_zero(&r->Z);
			r->Z_is_one = 0;
			ret = 1;
			goto end;
		}
	}
	/* 'n7', 'n8' */
	if (!BN_mod_add_quick(n1, n1, n3, p))
		goto end;
	if (!BN_mod_add_quick(n2, n2, n4, p))
		goto end;
	/* 'n7' = n1 + n3 */
	/* 'n8' = n2 + n4 */

	/* Z_r */
	if (a->Z_is_one && b->Z_is_one) {
		if (!BN_copy(&r->Z, n5))
			goto end;
	} else {
		if (a->Z_is_one) {
			if (!BN_copy(n0, &b->Z))
				goto end;
		} else if (b->Z_is_one) {
			if (!BN_copy(n0, &a->Z))
				goto end;
		} else {
			if (!field_mul(group, n0, &a->Z, &b->Z, ctx))
				goto end;
		}
		if (!field_mul(group, &r->Z, n0, n5, ctx))
			goto end;
	}
	r->Z_is_one = 0;
	/* Z_r = Z_a * Z_b * n5 */

	/* X_r */
	if (!field_sqr(group, n0, n6, ctx))
		goto end;
	if (!field_sqr(group, n4, n5, ctx))
		goto end;
	if (!field_mul(group, n3, n1, n4, ctx))
		goto end;
	if (!BN_mod_sub_quick(&r->X, n0, n3, p))
		goto end;
	/* X_r = n6^2 - n5^2 * 'n7' */

	/* 'n9' */
	if (!BN_mod_lshift1_quick(n0, &r->X, p))
		goto end;
	if (!BN_mod_sub_quick(n0, n3, n0, p))
		goto end;
	/* n9 = n5^2 * 'n7' - 2 * X_r */

	/* Y_r */
	if (!field_mul(group, n0, n0, n6, ctx))
		goto end;
	if (!field_mul(group, n5, n4, n5, ctx))
		goto end;	/* now n5 is n5^3 */
	if (!field_mul(group, n1, n2, n5, ctx))
		goto end;
	if (!BN_mod_sub_quick(n0, n0, n1, p))
		goto end;
	if (BN_is_odd(n0))
		if (!BN_add(n0, n0, p))
			goto end;
	/* now  0 <= n0 < 2*p,  and n0 is even */
	if (!BN_rshift1(&r->Y, n0))
		goto end;
	/* Y_r = (n6 * 'n9' - 'n8' * 'n5^3') / 2 */

	ret = 1;

 end:
	if (ctx)		/* otherwise we already called BN_CTX_end */
		BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_dbl(const EC_GROUP * group, EC_POINT * r, const EC_POINT * a, BN_CTX * ctx)
{
	int (*field_mul) (const EC_GROUP *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
	int (*field_sqr) (const EC_GROUP *, BIGNUM *, const BIGNUM *, BN_CTX *);
	const BIGNUM *p;
	BN_CTX *new_ctx = NULL;
	BIGNUM *n0, *n1, *n2, *n3;
	int ret = 0;

	if (EC_POINT_is_at_infinity(group, a) > 0) {
		BN_zero(&r->Z);
		r->Z_is_one = 0;
		return 1;
	}
	field_mul = group->meth->field_mul;
	field_sqr = group->meth->field_sqr;
	p = &group->field;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((n0 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((n1 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((n2 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((n3 = BN_CTX_get(ctx)) == NULL)
		goto err;

	/*
	 * Note that in this function we must not read components of 'a' once
	 * we have written the corresponding components of 'r'. ('r' might
	 * the same as 'a'.)
	 */

	/* n1 */
	if (a->Z_is_one) {
		if (!field_sqr(group, n0, &a->X, ctx))
			goto err;
		if (!BN_mod_lshift1_quick(n1, n0, p))
			goto err;
		if (!BN_mod_add_quick(n0, n0, n1, p))
			goto err;
		if (!BN_mod_add_quick(n1, n0, &group->a, p))
			goto err;
		/* n1 = 3 * X_a^2 + a_curve */
	} else if (group->a_is_minus3) {
		if (!field_sqr(group, n1, &a->Z, ctx))
			goto err;
		if (!BN_mod_add_quick(n0, &a->X, n1, p))
			goto err;
		if (!BN_mod_sub_quick(n2, &a->X, n1, p))
			goto err;
		if (!field_mul(group, n1, n0, n2, ctx))
			goto err;
		if (!BN_mod_lshift1_quick(n0, n1, p))
			goto err;
		if (!BN_mod_add_quick(n1, n0, n1, p))
			goto err;
		/*
		 * n1 = 3 * (X_a + Z_a^2) * (X_a - Z_a^2) = 3 * X_a^2 - 3 *
		 * Z_a^4
		 */
	} else {
		if (!field_sqr(group, n0, &a->X, ctx))
			goto err;
		if (!BN_mod_lshift1_quick(n1, n0, p))
			goto err;
		if (!BN_mod_add_quick(n0, n0, n1, p))
			goto err;
		if (!field_sqr(group, n1, &a->Z, ctx))
			goto err;
		if (!field_sqr(group, n1, n1, ctx))
			goto err;
		if (!field_mul(group, n1, n1, &group->a, ctx))
			goto err;
		if (!BN_mod_add_quick(n1, n1, n0, p))
			goto err;
		/* n1 = 3 * X_a^2 + a_curve * Z_a^4 */
	}

	/* Z_r */
	if (a->Z_is_one) {
		if (!BN_copy(n0, &a->Y))
			goto err;
	} else {
		if (!field_mul(group, n0, &a->Y, &a->Z, ctx))
			goto err;
	}
	if (!BN_mod_lshift1_quick(&r->Z, n0, p))
		goto err;
	r->Z_is_one = 0;
	/* Z_r = 2 * Y_a * Z_a */

	/* n2 */
	if (!field_sqr(group, n3, &a->Y, ctx))
		goto err;
	if (!field_mul(group, n2, &a->X, n3, ctx))
		goto err;
	if (!BN_mod_lshift_quick(n2, n2, 2, p))
		goto err;
	/* n2 = 4 * X_a * Y_a^2 */

	/* X_r */
	if (!BN_mod_lshift1_quick(n0, n2, p))
		goto err;
	if (!field_sqr(group, &r->X, n1, ctx))
		goto err;
	if (!BN_mod_sub_quick(&r->X, &r->X, n0, p))
		goto err;
	/* X_r = n1^2 - 2 * n2 */

	/* n3 */
	if (!field_sqr(group, n0, n3, ctx))
		goto err;
	if (!BN_mod_lshift_quick(n3, n0, 3, p))
		goto err;
	/* n3 = 8 * Y_a^4 */

	/* Y_r */
	if (!BN_mod_sub_quick(n0, n2, &r->X, p))
		goto err;
	if (!field_mul(group, n0, n1, n0, ctx))
		goto err;
	if (!BN_mod_sub_quick(&r->Y, n0, n3, p))
		goto err;
	/* Y_r = n1 * (n2 - X_r) - n3 */

	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_invert(const EC_GROUP * group, EC_POINT * point, BN_CTX * ctx)
{
	if (EC_POINT_is_at_infinity(group, point) > 0 || BN_is_zero(&point->Y))
		/* point is its own inverse */
		return 1;

	return BN_usub(&point->Y, &group->field, &point->Y);
}


int
ec_GFp_simple_is_at_infinity(const EC_GROUP * group, const EC_POINT * point)
{
	return BN_is_zero(&point->Z);
}


int
ec_GFp_simple_is_on_curve(const EC_GROUP * group, const EC_POINT * point, BN_CTX * ctx)
{
	int (*field_mul) (const EC_GROUP *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
	int (*field_sqr) (const EC_GROUP *, BIGNUM *, const BIGNUM *, BN_CTX *);
	const BIGNUM *p;
	BN_CTX *new_ctx = NULL;
	BIGNUM *rh, *tmp, *Z4, *Z6;
	int ret = -1;

	if (EC_POINT_is_at_infinity(group, point) > 0)
		return 1;

	field_mul = group->meth->field_mul;
	field_sqr = group->meth->field_sqr;
	p = &group->field;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return -1;
	}
	BN_CTX_start(ctx);
	if ((rh = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((Z4 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((Z6 = BN_CTX_get(ctx)) == NULL)
		goto err;

	/*
	 * We have a curve defined by a Weierstrass equation y^2 = x^3 + a*x
	 * + b. The point to consider is given in Jacobian projective
	 * coordinates where  (X, Y, Z)  represents  (x, y) = (X/Z^2, Y/Z^3).
	 * Substituting this and multiplying by  Z^6  transforms the above
	 * equation into Y^2 = X^3 + a*X*Z^4 + b*Z^6. To test this, we add up
	 * the right-hand side in 'rh'.
	 */

	/* rh := X^2 */
	if (!field_sqr(group, rh, &point->X, ctx))
		goto err;

	if (!point->Z_is_one) {
		if (!field_sqr(group, tmp, &point->Z, ctx))
			goto err;
		if (!field_sqr(group, Z4, tmp, ctx))
			goto err;
		if (!field_mul(group, Z6, Z4, tmp, ctx))
			goto err;

		/* rh := (rh + a*Z^4)*X */
		if (group->a_is_minus3) {
			if (!BN_mod_lshift1_quick(tmp, Z4, p))
				goto err;
			if (!BN_mod_add_quick(tmp, tmp, Z4, p))
				goto err;
			if (!BN_mod_sub_quick(rh, rh, tmp, p))
				goto err;
			if (!field_mul(group, rh, rh, &point->X, ctx))
				goto err;
		} else {
			if (!field_mul(group, tmp, Z4, &group->a, ctx))
				goto err;
			if (!BN_mod_add_quick(rh, rh, tmp, p))
				goto err;
			if (!field_mul(group, rh, rh, &point->X, ctx))
				goto err;
		}

		/* rh := rh + b*Z^6 */
		if (!field_mul(group, tmp, &group->b, Z6, ctx))
			goto err;
		if (!BN_mod_add_quick(rh, rh, tmp, p))
			goto err;
	} else {
		/* point->Z_is_one */

		/* rh := (rh + a)*X */
		if (!BN_mod_add_quick(rh, rh, &group->a, p))
			goto err;
		if (!field_mul(group, rh, rh, &point->X, ctx))
			goto err;
		/* rh := rh + b */
		if (!BN_mod_add_quick(rh, rh, &group->b, p))
			goto err;
	}

	/* 'lh' := Y^2 */
	if (!field_sqr(group, tmp, &point->Y, ctx))
		goto err;

	ret = (0 == BN_ucmp(tmp, rh));

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_cmp(const EC_GROUP * group, const EC_POINT * a, const EC_POINT * b, BN_CTX * ctx)
{
	/*
	 * return values: -1   error 0   equal (in affine coordinates) 1
	 * not equal
	 */

	int (*field_mul) (const EC_GROUP *, BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *);
	int (*field_sqr) (const EC_GROUP *, BIGNUM *, const BIGNUM *, BN_CTX *);
	BN_CTX *new_ctx = NULL;
	BIGNUM *tmp1, *tmp2, *Za23, *Zb23;
	const BIGNUM *tmp1_, *tmp2_;
	int ret = -1;

	if (EC_POINT_is_at_infinity(group, a) > 0) {
		return EC_POINT_is_at_infinity(group, b) > 0 ? 0 : 1;
	}
	if (EC_POINT_is_at_infinity(group, b) > 0)
		return 1;

	if (a->Z_is_one && b->Z_is_one) {
		return ((BN_cmp(&a->X, &b->X) == 0) && BN_cmp(&a->Y, &b->Y) == 0) ? 0 : 1;
	}
	field_mul = group->meth->field_mul;
	field_sqr = group->meth->field_sqr;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return -1;
	}
	BN_CTX_start(ctx);
	if ((tmp1 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((tmp2 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((Za23 = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((Zb23 = BN_CTX_get(ctx)) == NULL)
		goto end;

	/*
	 * We have to decide whether (X_a/Z_a^2, Y_a/Z_a^3) = (X_b/Z_b^2,
	 * Y_b/Z_b^3), or equivalently, whether (X_a*Z_b^2, Y_a*Z_b^3) =
	 * (X_b*Z_a^2, Y_b*Z_a^3).
	 */

	if (!b->Z_is_one) {
		if (!field_sqr(group, Zb23, &b->Z, ctx))
			goto end;
		if (!field_mul(group, tmp1, &a->X, Zb23, ctx))
			goto end;
		tmp1_ = tmp1;
	} else
		tmp1_ = &a->X;
	if (!a->Z_is_one) {
		if (!field_sqr(group, Za23, &a->Z, ctx))
			goto end;
		if (!field_mul(group, tmp2, &b->X, Za23, ctx))
			goto end;
		tmp2_ = tmp2;
	} else
		tmp2_ = &b->X;

	/* compare  X_a*Z_b^2  with  X_b*Z_a^2 */
	if (BN_cmp(tmp1_, tmp2_) != 0) {
		ret = 1;	/* points differ */
		goto end;
	}
	if (!b->Z_is_one) {
		if (!field_mul(group, Zb23, Zb23, &b->Z, ctx))
			goto end;
		if (!field_mul(group, tmp1, &a->Y, Zb23, ctx))
			goto end;
		/* tmp1_ = tmp1 */
	} else
		tmp1_ = &a->Y;
	if (!a->Z_is_one) {
		if (!field_mul(group, Za23, Za23, &a->Z, ctx))
			goto end;
		if (!field_mul(group, tmp2, &b->Y, Za23, ctx))
			goto end;
		/* tmp2_ = tmp2 */
	} else
		tmp2_ = &b->Y;

	/* compare  Y_a*Z_b^3  with  Y_b*Z_a^3 */
	if (BN_cmp(tmp1_, tmp2_) != 0) {
		ret = 1;	/* points differ */
		goto end;
	}
	/* points are equal */
	ret = 0;

 end:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_make_affine(const EC_GROUP * group, EC_POINT * point, BN_CTX * ctx)
{
	BN_CTX *new_ctx = NULL;
	BIGNUM *x, *y;
	int ret = 0;

	if (point->Z_is_one || EC_POINT_is_at_infinity(group, point) > 0)
		return 1;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((y = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (!EC_POINT_get_affine_coordinates(group, point, x, y, ctx))
		goto err;
	if (!EC_POINT_set_affine_coordinates(group, point, x, y, ctx))
		goto err;
	if (!point->Z_is_one) {
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}
	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	return ret;
}


int
ec_GFp_simple_points_make_affine(const EC_GROUP * group, size_t num, EC_POINT * points[], BN_CTX * ctx)
{
	BN_CTX *new_ctx = NULL;
	BIGNUM *tmp0, *tmp1;
	size_t pow2 = 0;
	BIGNUM **heap = NULL;
	size_t i;
	int ret = 0;

	if (num == 0)
		return 1;

	if (ctx == NULL) {
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
	}
	BN_CTX_start(ctx);
	if ((tmp0 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp1 = BN_CTX_get(ctx)) == NULL)
		goto err;

	/*
	 * Before converting the individual points, compute inverses of all Z
	 * values. Modular inversion is rather slow, but luckily we can do
	 * with a single explicit inversion, plus about 3 multiplications per
	 * input value.
	 */

	pow2 = 1;
	while (num > pow2)
		pow2 <<= 1;
	/*
	 * Now pow2 is the smallest power of 2 satifsying pow2 >= num. We
	 * need twice that.
	 */
	pow2 <<= 1;

	heap = reallocarray(NULL, pow2, sizeof heap[0]);
	if (heap == NULL)
		goto err;

	/*
	 * The array is used as a binary tree, exactly as in heapsort:
	 *
	 * heap[1] heap[2]                     heap[3] heap[4]       heap[5]
	 * heap[6]       heap[7] heap[8]heap[9] heap[10]heap[11]
	 * heap[12]heap[13] heap[14] heap[15]
	 *
	 * We put the Z's in the last line; then we set each other node to the
	 * product of its two child-nodes (where empty or 0 entries are
	 * treated as ones); then we invert heap[1]; then we invert each
	 * other node by replacing it by the product of its parent (after
	 * inversion) and its sibling (before inversion).
	 */
	heap[0] = NULL;
	for (i = pow2 / 2 - 1; i > 0; i--)
		heap[i] = NULL;
	for (i = 0; i < num; i++)
		heap[pow2 / 2 + i] = &points[i]->Z;
	for (i = pow2 / 2 + num; i < pow2; i++)
		heap[i] = NULL;

	/* set each node to the product of its children */
	for (i = pow2 / 2 - 1; i > 0; i--) {
		heap[i] = BN_new();
		if (heap[i] == NULL)
			goto err;

		if (heap[2 * i] != NULL) {
			if ((heap[2 * i + 1] == NULL) || BN_is_zero(heap[2 * i + 1])) {
				if (!BN_copy(heap[i], heap[2 * i]))
					goto err;
			} else {
				if (BN_is_zero(heap[2 * i])) {
					if (!BN_copy(heap[i], heap[2 * i + 1]))
						goto err;
				} else {
					if (!group->meth->field_mul(group, heap[i],
						heap[2 * i], heap[2 * i + 1], ctx))
						goto err;
				}
			}
		}
	}

	/* invert heap[1] */
	if (!BN_is_zero(heap[1])) {
		if (BN_mod_inverse_ct(heap[1], heap[1], &group->field, ctx) == NULL) {
			ECerror(ERR_R_BN_LIB);
			goto err;
		}
	}
	if (group->meth->field_encode != 0) {
		/*
		 * in the Montgomery case, we just turned  R*H  (representing
		 * H) into  1/(R*H),  but we need  R*(1/H)  (representing
		 * 1/H); i.e. we have need to multiply by the Montgomery
		 * factor twice
		 */
		if (!group->meth->field_encode(group, heap[1], heap[1], ctx))
			goto err;
		if (!group->meth->field_encode(group, heap[1], heap[1], ctx))
			goto err;
	}
	/* set other heap[i]'s to their inverses */
	for (i = 2; i < pow2 / 2 + num; i += 2) {
		/* i is even */
		if ((heap[i + 1] != NULL) && !BN_is_zero(heap[i + 1])) {
			if (!group->meth->field_mul(group, tmp0, heap[i / 2], heap[i + 1], ctx))
				goto err;
			if (!group->meth->field_mul(group, tmp1, heap[i / 2], heap[i], ctx))
				goto err;
			if (!BN_copy(heap[i], tmp0))
				goto err;
			if (!BN_copy(heap[i + 1], tmp1))
				goto err;
		} else {
			if (!BN_copy(heap[i], heap[i / 2]))
				goto err;
		}
	}

	/*
	 * we have replaced all non-zero Z's by their inverses, now fix up
	 * all the points
	 */
	for (i = 0; i < num; i++) {
		EC_POINT *p = points[i];

		if (!BN_is_zero(&p->Z)) {
			/* turn  (X, Y, 1/Z)  into  (X/Z^2, Y/Z^3, 1) */

			if (!group->meth->field_sqr(group, tmp1, &p->Z, ctx))
				goto err;
			if (!group->meth->field_mul(group, &p->X, &p->X, tmp1, ctx))
				goto err;

			if (!group->meth->field_mul(group, tmp1, tmp1, &p->Z, ctx))
				goto err;
			if (!group->meth->field_mul(group, &p->Y, &p->Y, tmp1, ctx))
				goto err;

			if (group->meth->field_set_to_one != 0) {
				if (!group->meth->field_set_to_one(group, &p->Z, ctx))
					goto err;
			} else {
				if (!BN_one(&p->Z))
					goto err;
			}
			p->Z_is_one = 1;
		}
	}

	ret = 1;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);
	if (heap != NULL) {
		/*
		 * heap[pow2/2] .. heap[pow2-1] have not been allocated
		 * locally!
		 */
		for (i = pow2 / 2 - 1; i > 0; i--) {
			BN_clear_free(heap[i]);
		}
		free(heap);
	}
	return ret;
}


int
ec_GFp_simple_field_mul(const EC_GROUP * group, BIGNUM * r, const BIGNUM * a, const BIGNUM * b, BN_CTX * ctx)
{
	return BN_mod_mul(r, a, b, &group->field, ctx);
}

int
ec_GFp_simple_field_sqr(const EC_GROUP * group, BIGNUM * r, const BIGNUM * a, BN_CTX * ctx)
{
	return BN_mod_sqr(r, a, &group->field, ctx);
}

/*
 * Apply randomization of EC point projective coordinates:
 *
 * 	(X, Y, Z) = (lambda^2 * X, lambda^3 * Y, lambda * Z)
 *
 * where lambda is in the interval [1, group->field).
 */
int
ec_GFp_simple_blind_coordinates(const EC_GROUP *group, EC_POINT *p, BN_CTX *ctx)
{
	BIGNUM *lambda = NULL;
	BIGNUM *tmp = NULL;
	int ret = 0;

	BN_CTX_start(ctx);
	if ((lambda = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp = BN_CTX_get(ctx)) == NULL)
		goto err;

	/* Generate lambda in [1, group->field - 1] */
	if (!bn_rand_interval(lambda, BN_value_one(), &group->field))
		goto err;

	if (group->meth->field_encode != NULL &&
	    !group->meth->field_encode(group, lambda, lambda, ctx))
		goto err;

	/* Z = lambda * Z */
	if (!group->meth->field_mul(group, &p->Z, lambda, &p->Z, ctx))
		goto err;

	/* tmp = lambda^2 */
	if (!group->meth->field_sqr(group, tmp, lambda, ctx))
		goto err;

	/* X = lambda^2 * X */
	if (!group->meth->field_mul(group, &p->X, tmp, &p->X, ctx))
		goto err;

	/* tmp = lambda^3 */
	if (!group->meth->field_mul(group, tmp, tmp, lambda, ctx))
		goto err;

	/* Y = lambda^3 * Y */
	if (!group->meth->field_mul(group, &p->Y, tmp, &p->Y, ctx))
		goto err;

	/* Disable optimized arithmetics after replacing Z by lambda * Z. */
	p->Z_is_one = 0;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	return ret;
}


#define EC_POINT_BN_set_flags(P, flags) do {				\
	BN_set_flags(&(P)->X, (flags));         			\
	BN_set_flags(&(P)->Y, (flags));         			\
	BN_set_flags(&(P)->Z, (flags));         			\
} while(0)

#define EC_POINT_CSWAP(c, a, b, w, t) do {      			\
	if (!BN_swap_ct(c, &(a)->X, &(b)->X, w)	||			\
	    !BN_swap_ct(c, &(a)->Y, &(b)->Y, w)	|| 			\
	    !BN_swap_ct(c, &(a)->Z, &(b)->Z, w))			\
		goto err;						\
	t = ((a)->Z_is_one ^ (b)->Z_is_one) & (c);			\
	(a)->Z_is_one ^= (t);						\
	(b)->Z_is_one ^= (t);						\
} while(0)

/*
 * This function computes (in constant time) a point multiplication over the
 * EC group.
 *
 * At a high level, it is Montgomery ladder with conditional swaps.
 *
 * It performs either a fixed point multiplication
 *          (scalar * generator)
 * when point is NULL, or a variable point multiplication
 *          (scalar * point)
 * when point is not NULL.
 *
 * scalar should be in the range [0,n) otherwise all constant time bets are off.
 *
 * NB: This says nothing about EC_POINT_add and EC_POINT_dbl,
 * which of course are not constant time themselves.
 *
 * The product is stored in r.
 *
 * Returns 1 on success, 0 otherwise.
 */
static int
ec_GFp_simple_mul_ct(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
    const EC_POINT *point, BN_CTX *ctx)
{
	int i, cardinality_bits, group_top, kbit, pbit, Z_is_one;
	EC_POINT *s = NULL;
	BIGNUM *k = NULL;
	BIGNUM *lambda = NULL;
	BIGNUM *cardinality = NULL;
	BN_CTX *new_ctx = NULL;
	int ret = 0;

	if (ctx == NULL && (ctx = new_ctx = BN_CTX_new()) == NULL)
		return 0;

	BN_CTX_start(ctx);

	if ((s = EC_POINT_new(group)) == NULL)
		goto err;

	if (point == NULL) {
		if (!EC_POINT_copy(s, group->generator))
			goto err;
	} else {
		if (!EC_POINT_copy(s, point))
			goto err;
	}

	EC_POINT_BN_set_flags(s, BN_FLG_CONSTTIME);

	if ((cardinality = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((lambda = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((k = BN_CTX_get(ctx)) == NULL)
		goto err;
	if (!BN_mul(cardinality, &group->order, &group->cofactor, ctx))
		goto err;

	/*
	 * Group cardinalities are often on a word boundary.
	 * So when we pad the scalar, some timing diff might
	 * pop if it needs to be expanded due to carries.
	 * So expand ahead of time.
	 */
	cardinality_bits = BN_num_bits(cardinality);
	group_top = cardinality->top;
	if ((bn_wexpand(k, group_top + 2) == NULL) ||
	    (bn_wexpand(lambda, group_top + 2) == NULL))
		goto err;

	if (!BN_copy(k, scalar))
		goto err;

	BN_set_flags(k, BN_FLG_CONSTTIME);

	if (BN_num_bits(k) > cardinality_bits || BN_is_negative(k)) {
		/*
		 * This is an unusual input, and we don't guarantee
		 * constant-timeness
		 */
		if (!BN_nnmod(k, k, cardinality, ctx))
			goto err;
	}

	if (!BN_add(lambda, k, cardinality))
		goto err;
	BN_set_flags(lambda, BN_FLG_CONSTTIME);
	if (!BN_add(k, lambda, cardinality))
		goto err;
	/*
	 * lambda := scalar + cardinality
	 * k := scalar + 2*cardinality
	 */
	kbit = BN_is_bit_set(lambda, cardinality_bits);
	if (!BN_swap_ct(kbit, k, lambda, group_top + 2))
		goto err;

	group_top = group->field.top;
	if ((bn_wexpand(&s->X, group_top) == NULL) ||
	    (bn_wexpand(&s->Y, group_top) == NULL) ||
	    (bn_wexpand(&s->Z, group_top) == NULL) ||
	    (bn_wexpand(&r->X, group_top) == NULL) ||
	    (bn_wexpand(&r->Y, group_top) == NULL) ||
	    (bn_wexpand(&r->Z, group_top) == NULL))
		goto err;

	/*
	 * Apply coordinate blinding for EC_POINT if the underlying EC_METHOD
	 * implements it.
	 */
	if (!ec_point_blind_coordinates(group, s, ctx))
		goto err;

	/* top bit is a 1, in a fixed pos */
	if (!EC_POINT_copy(r, s))
		goto err;

	EC_POINT_BN_set_flags(r, BN_FLG_CONSTTIME);

	if (!EC_POINT_dbl(group, s, s, ctx))
		goto err;

	pbit = 0;

	/*
	 * The ladder step, with branches, is
	 *
	 * k[i] == 0: S = add(R, S), R = dbl(R)
	 * k[i] == 1: R = add(S, R), S = dbl(S)
	 *
	 * Swapping R, S conditionally on k[i] leaves you with state
	 *
	 * k[i] == 0: T, U = R, S
	 * k[i] == 1: T, U = S, R
	 *
	 * Then perform the ECC ops.
	 *
	 * U = add(T, U)
	 * T = dbl(T)
	 *
	 * Which leaves you with state
	 *
	 * k[i] == 0: U = add(R, S), T = dbl(R)
	 * k[i] == 1: U = add(S, R), T = dbl(S)
	 *
	 * Swapping T, U conditionally on k[i] leaves you with state
	 *
	 * k[i] == 0: R, S = T, U
	 * k[i] == 1: R, S = U, T
	 *
	 * Which leaves you with state
	 *
	 * k[i] == 0: S = add(R, S), R = dbl(R)
	 * k[i] == 1: R = add(S, R), S = dbl(S)
	 *
	 * So we get the same logic, but instead of a branch it's a
	 * conditional swap, followed by ECC ops, then another conditional swap.
	 *
	 * Optimization: The end of iteration i and start of i-1 looks like
	 *
	 * ...
	 * CSWAP(k[i], R, S)
	 * ECC
	 * CSWAP(k[i], R, S)
	 * (next iteration)
	 * CSWAP(k[i-1], R, S)
	 * ECC
	 * CSWAP(k[i-1], R, S)
	 * ...
	 *
	 * So instead of two contiguous swaps, you can merge the condition
	 * bits and do a single swap.
	 *
	 * k[i]   k[i-1]    Outcome
	 * 0      0         No Swap
	 * 0      1         Swap
	 * 1      0         Swap
	 * 1      1         No Swap
	 *
	 * This is XOR. pbit tracks the previous bit of k.
	 */

	for (i = cardinality_bits - 1; i >= 0; i--) {
		kbit = BN_is_bit_set(k, i) ^ pbit;
		EC_POINT_CSWAP(kbit, r, s, group_top, Z_is_one);
		if (!EC_POINT_add(group, s, r, s, ctx))
			goto err;
		if (!EC_POINT_dbl(group, r, r, ctx))
			goto err;
		/*
		 * pbit logic merges this cswap with that of the
		 * next iteration
		 */
		pbit ^= kbit;
	}
	/* one final cswap to move the right value into r */
	EC_POINT_CSWAP(pbit, r, s, group_top, Z_is_one);

	ret = 1;

 err:
	EC_POINT_free(s);
	if (ctx != NULL)
		BN_CTX_end(ctx);
	BN_CTX_free(new_ctx);

	return ret;
}

#undef EC_POINT_BN_set_flags
#undef EC_POINT_CSWAP

int
ec_GFp_simple_mul_generator_ct(const EC_GROUP *group, EC_POINT *r,
    const BIGNUM *scalar, BN_CTX *ctx)
{
	return ec_GFp_simple_mul_ct(group, r, scalar, NULL, ctx);
}

int
ec_GFp_simple_mul_single_ct(const EC_GROUP *group, EC_POINT *r,
    const BIGNUM *scalar, const EC_POINT *point, BN_CTX *ctx)
{
	return ec_GFp_simple_mul_ct(group, r, scalar, point, ctx);
}

int
ec_GFp_simple_mul_double_nonct(const EC_GROUP *group, EC_POINT *r,
    const BIGNUM *g_scalar, const BIGNUM *p_scalar, const EC_POINT *point,
    BN_CTX *ctx)
{
	return ec_wNAF_mul(group, r, g_scalar, 1, &point, &p_scalar, ctx);
}
