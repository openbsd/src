/* $OpenBSD: ecp_oct.c,v 1.29 2024/10/25 18:06:42 tb Exp $ */
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

#include <stddef.h>
#include <stdint.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "ec_local.h"

#include "bytestring.h"

int
ec_GFp_simple_set_compressed_coordinates(const EC_GROUP *group,
    EC_POINT *point, const BIGNUM *x_, int y_bit, BN_CTX *ctx)
{
	BIGNUM *tmp1, *tmp2, *x, *y;
	int ret = 0;

	/* clear error queue */
	ERR_clear_error();

	y_bit = (y_bit != 0);

	BN_CTX_start(ctx);

	if ((tmp1 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((tmp2 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((y = BN_CTX_get(ctx)) == NULL)
		goto err;

	/*
	 * Recover y.  We have a Weierstrass equation y^2 = x^3 + a*x + b, so
	 * y  is one of the square roots of  x^3 + a*x + b.
	 */

	/* tmp1 := x^3 */
	if (!BN_nnmod(x, x_, &group->field, ctx))
		goto err;
	if (group->meth->field_decode == NULL) {
		/* field_{sqr,mul} work on standard representation */
		if (!group->meth->field_sqr(group, tmp2, x_, ctx))
			goto err;
		if (!group->meth->field_mul(group, tmp1, tmp2, x_, ctx))
			goto err;
	} else {
		if (!BN_mod_sqr(tmp2, x_, &group->field, ctx))
			goto err;
		if (!BN_mod_mul(tmp1, tmp2, x_, &group->field, ctx))
			goto err;
	}

	/* tmp1 := tmp1 + a*x */
	if (group->a_is_minus3) {
		if (!BN_mod_lshift1_quick(tmp2, x, &group->field))
			goto err;
		if (!BN_mod_add_quick(tmp2, tmp2, x, &group->field))
			goto err;
		if (!BN_mod_sub_quick(tmp1, tmp1, tmp2, &group->field))
			goto err;
	} else {
		if (group->meth->field_decode) {
			if (!group->meth->field_decode(group, tmp2, &group->a, ctx))
				goto err;
			if (!BN_mod_mul(tmp2, tmp2, x, &group->field, ctx))
				goto err;
		} else {
			/* field_mul works on standard representation */
			if (!group->meth->field_mul(group, tmp2, &group->a, x, ctx))
				goto err;
		}

		if (!BN_mod_add_quick(tmp1, tmp1, tmp2, &group->field))
			goto err;
	}

	/* tmp1 := tmp1 + b */
	if (group->meth->field_decode != NULL) {
		if (!group->meth->field_decode(group, tmp2, &group->b, ctx))
			goto err;
		if (!BN_mod_add_quick(tmp1, tmp1, tmp2, &group->field))
			goto err;
	} else {
		if (!BN_mod_add_quick(tmp1, tmp1, &group->b, &group->field))
			goto err;
	}

	if (!BN_mod_sqrt(y, tmp1, &group->field, ctx)) {
		unsigned long err = ERR_peek_last_error();

		if (ERR_GET_LIB(err) == ERR_LIB_BN && ERR_GET_REASON(err) == BN_R_NOT_A_SQUARE) {
			ERR_clear_error();
			ECerror(EC_R_INVALID_COMPRESSED_POINT);
		} else
			ECerror(ERR_R_BN_LIB);
		goto err;
	}
	if (y_bit != BN_is_odd(y)) {
		if (BN_is_zero(y)) {
			ECerror(EC_R_INVALID_COMPRESSION_BIT);
			goto err;
		}
		if (!BN_usub(y, &group->field, y))
			goto err;
		if (y_bit != BN_is_odd(y)) {
			ECerror(ERR_R_INTERNAL_ERROR);
			goto err;
		}
	}
	if (!EC_POINT_set_affine_coordinates(group, point, x, y, ctx))
		goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}

/*
 * Only the last three bits of the leading octet of a point should be set.
 * Bits 3 and 2 encode the conversion form for all points except the point
 * at infinity. In compressed and hybrid form bit 1 indicates if the even
 * or the odd solution of the quadratic equation for y should be used.
 *
 * The public point_conversion_t enum lacks the point at infinity, so we
 * ignore it except at the API boundary.
 */

#define EC_OCT_YBIT			0x01

#define EC_OCT_POINT_AT_INFINITY	0x00
#define EC_OCT_POINT_COMPRESSED		0x02
#define EC_OCT_POINT_UNCOMPRESSED	0x04
#define EC_OCT_POINT_HYBRID		0x06
#define EC_OCT_POINT_CONVERSION_MASK	0x06

static int
ec_oct_conversion_form_is_valid(uint8_t form)
{
	return (form & EC_OCT_POINT_CONVERSION_MASK) == form;
}

static int
ec_oct_check_hybrid_ybit_is_consistent(uint8_t form, int ybit, const BIGNUM *y)
{
	if (form == EC_OCT_POINT_HYBRID && ybit != BN_is_odd(y)) {
		ECerror(EC_R_INVALID_ENCODING);
		return 0;
	}

	return 1;
}

/* Nonzero y-bit only makes sense with compressed or hybrid encoding. */
static int
ec_oct_nonzero_ybit_allowed(uint8_t form)
{
	return form == EC_OCT_POINT_COMPRESSED || form == EC_OCT_POINT_HYBRID;
}

static int
ec_oct_add_leading_octet_cbb(CBB *cbb, uint8_t form, int ybit)
{
	if (ec_oct_nonzero_ybit_allowed(form) && ybit != 0)
		form |= EC_OCT_YBIT;

	return CBB_add_u8(cbb, form);
}

static int
ec_oct_get_leading_octet_cbs(CBS *cbs, uint8_t *out_form, int *out_ybit)
{
	uint8_t octet;

	if (!CBS_get_u8(cbs, &octet)) {
		ECerror(EC_R_BUFFER_TOO_SMALL);
		return 0;
	}

	*out_ybit = octet & EC_OCT_YBIT;
	*out_form = octet & ~EC_OCT_YBIT;

	if (!ec_oct_conversion_form_is_valid(*out_form)) {
		ECerror(EC_R_INVALID_ENCODING);
		return 0;
	}

	if (*out_ybit != 0 && !ec_oct_nonzero_ybit_allowed(*out_form)) {
		ECerror(EC_R_INVALID_ENCODING);
		return 0;
	}

	return 1;
}

static int
ec_oct_encoded_length(const EC_GROUP *group, uint8_t form, size_t *out_len)
{
	switch (form) {
	case EC_OCT_POINT_AT_INFINITY:
		*out_len = 1;
		return 1;
	case EC_OCT_POINT_COMPRESSED:
		*out_len = 1 + BN_num_bytes(&group->field);
		return 1;
	case EC_OCT_POINT_UNCOMPRESSED:
	case EC_OCT_POINT_HYBRID:
		*out_len = 1 + 2 * BN_num_bytes(&group->field);
		return 1;
	default:
		return 0;
	}
}

static int
ec_oct_field_element_is_valid(const EC_GROUP *group, const BIGNUM *bn)
{
	/* Ensure bn is in the range [0, field). */
	return !BN_is_negative(bn) && BN_cmp(&group->field, bn) > 0;
}

static int
ec_oct_add_field_element_cbb(CBB *cbb, const EC_GROUP *group, const BIGNUM *bn)
{
	uint8_t *buf = NULL;
	int buf_len = BN_num_bytes(&group->field);

	if (!ec_oct_field_element_is_valid(group, bn)) {
		ECerror(EC_R_BIGNUM_OUT_OF_RANGE);
		return 0;
	}
	if (!CBB_add_space(cbb, &buf, buf_len)) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return 0;
	}
	if (BN_bn2binpad(bn, buf, buf_len) != buf_len) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return 0;
	}

	return 1;
}

static int
ec_oct_get_field_element_cbs(CBS *cbs, const EC_GROUP *group, BIGNUM *bn)
{
	CBS field_element;

	if (!CBS_get_bytes(cbs, &field_element, BN_num_bytes(&group->field))) {
		ECerror(EC_R_INVALID_ENCODING);
		return 0;
	}
	if (!BN_bin2bn(CBS_data(&field_element), CBS_len(&field_element), bn)) {
		ECerror(ERR_R_MALLOC_FAILURE);
		return 0;
	}
	if (!ec_oct_field_element_is_valid(group, bn)) {
		ECerror(EC_R_BIGNUM_OUT_OF_RANGE);
		return 0;
	}

	return 1;
}

size_t
ec_GFp_simple_point2oct(const EC_GROUP *group, const EC_POINT *point,
    point_conversion_form_t conversion_form, unsigned char *buf, size_t len,
    BN_CTX *ctx)
{
	CBB cbb;
	uint8_t form;
	BIGNUM *x, *y;
	size_t encoded_length;
	size_t ret = 0;

	if (conversion_form > UINT8_MAX) {
		ECerror(EC_R_INVALID_FORM);
		return 0;
	}

	form = conversion_form;

	/*
	 * Established behavior is to reject a request for the form 0 for the
	 * point at infinity even if it is valid.
	 */
	if (form == 0 || !ec_oct_conversion_form_is_valid(form)) {
		ECerror(EC_R_INVALID_FORM);
		return 0;
	}

	if (EC_POINT_is_at_infinity(group, point))
		form = EC_OCT_POINT_AT_INFINITY;

	if (!ec_oct_encoded_length(group, form, &encoded_length)) {
		ECerror(EC_R_INVALID_FORM);
		return 0;
	}

	if (buf == NULL)
		return encoded_length;

	if (len < encoded_length) {
		ECerror(EC_R_BUFFER_TOO_SMALL);
		return 0;
	}

	BN_CTX_start(ctx);
	if (!CBB_init_fixed(&cbb, buf, len))
		goto err;

	if (form == EC_OCT_POINT_AT_INFINITY) {
		if (!ec_oct_add_leading_octet_cbb(&cbb, form, 0))
			goto err;

		goto done;
	}

	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((y = BN_CTX_get(ctx)) == NULL)
		goto err;
	if (!EC_POINT_get_affine_coordinates(group, point, x, y, ctx))
		goto err;

	if (!ec_oct_add_leading_octet_cbb(&cbb, form, BN_is_odd(y)))
		goto err;

	if (form == EC_OCT_POINT_COMPRESSED) {
		if (!ec_oct_add_field_element_cbb(&cbb, group, x))
			goto err;
	} else {
		if (!ec_oct_add_field_element_cbb(&cbb, group, x))
			goto err;
		if (!ec_oct_add_field_element_cbb(&cbb, group, y))
			goto err;
	}

 done:
	if (!CBB_finish(&cbb, NULL, &ret))
		goto err;

	if (ret != encoded_length) {
		ret = 0;
		goto err;
	}

 err:
	CBB_cleanup(&cbb);
	BN_CTX_end(ctx);

	return ret;
}

int
ec_GFp_simple_oct2point(const EC_GROUP *group, EC_POINT *point,
    const unsigned char *buf, size_t len, BN_CTX *ctx)
{
	CBS cbs;
	uint8_t form;
	int ybit;
	BIGNUM *x, *y;
	int ret = 0;

	BN_CTX_start(ctx);
	CBS_init(&cbs, buf, len);

	if (!ec_oct_get_leading_octet_cbs(&cbs, &form, &ybit))
		goto err;

	if (form == EC_OCT_POINT_AT_INFINITY) {
		if (!EC_POINT_set_to_infinity(group, point))
			goto err;

		goto done;
	}

	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((y = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (form == EC_OCT_POINT_COMPRESSED) {
		if (!ec_oct_get_field_element_cbs(&cbs, group, x))
			goto err;
		if (!EC_POINT_set_compressed_coordinates(group, point, x, ybit, ctx))
			goto err;
	} else {
		if (!ec_oct_get_field_element_cbs(&cbs, group, x))
			goto err;
		if (!ec_oct_get_field_element_cbs(&cbs, group, y))
			goto err;
		if (!ec_oct_check_hybrid_ybit_is_consistent(form, ybit, y))
			goto err;
		if (!EC_POINT_set_affine_coordinates(group, point, x, y, ctx))
			goto err;
	}

 done:
	if (CBS_len(&cbs) > 0) {
		ECerror(EC_R_INVALID_ENCODING);
		goto err;
	}

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}
