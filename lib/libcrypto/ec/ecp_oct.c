/* $OpenBSD: ecp_oct.c,v 1.30 2024/10/30 18:16:34 tb Exp $ */
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
