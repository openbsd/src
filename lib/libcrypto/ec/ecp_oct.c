/* $OpenBSD: ecp_oct.c,v 1.32 2024/11/02 09:21:04 tb Exp $ */
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

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#include "ec_local.h"

int
ec_GFp_simple_set_compressed_coordinates(const EC_GROUP *group,
    EC_POINT *point, const BIGNUM *in_x, int y_bit, BN_CTX *ctx)
{
	const BIGNUM *p = &group->field, *a = &group->a, *b = &group->b;
	BIGNUM *w, *x, *y;
	int ret = 0;

	y_bit = (y_bit != 0);

	BN_CTX_start(ctx);

	if ((w = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((x = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((y = BN_CTX_get(ctx)) == NULL)
		goto err;

	/*
	 * Weierstrass equation: y^2 = x^3 + ax + b, so y is one of the
	 * square roots of x^3 + ax + b. The y-bit indicates which one.
	 */

	/* XXX - should we not insist on 0 <= x < p instead? */
	if (!BN_nnmod(x, in_x, p, ctx))
		goto err;

	if (group->meth->field_encode != NULL) {
		if (!group->meth->field_encode(group, x, x, ctx))
			goto err;
	}

	/* y = x^3 */
	if (!group->meth->field_sqr(group, y, x, ctx))
		goto err;
	if (!group->meth->field_mul(group, y, y, x, ctx))
		goto err;

	/* y += ax */
	if (group->a_is_minus3) {
		if (!BN_mod_lshift1_quick(w, x, p))
			goto err;
		if (!BN_mod_add_quick(w, w, x, p))
			goto err;
		if (!BN_mod_sub_quick(y, y, w, p))
			goto err;
	} else {
		if (!group->meth->field_mul(group, w, a, x, ctx))
			goto err;
		if (!BN_mod_add_quick(y, y, w, p))
			goto err;
	}

	/* y += b */
	if (!BN_mod_add_quick(y, y, b, p))
		goto err;

	if (group->meth->field_decode != NULL) {
		if (!group->meth->field_decode(group, x, x, ctx))
			goto err;
		if (!group->meth->field_decode(group, y, y, ctx))
			goto err;
	}

	if (!BN_mod_sqrt(y, y, p, ctx)) {
		ECerror(EC_R_INVALID_COMPRESSED_POINT);
		goto err;
	}

	if (y_bit == BN_is_odd(y))
		goto done;

	if (BN_is_zero(y)) {
		ECerror(EC_R_INVALID_COMPRESSION_BIT);
		goto err;
	}
	if (!BN_usub(y, &group->field, y))
		goto err;

	if (y_bit != BN_is_odd(y)) {
		/* Can only happen if p is even and should not be reachable. */
		ECerror(ERR_R_INTERNAL_ERROR);
		goto err;
	}

 done:
	if (!EC_POINT_set_affine_coordinates(group, point, x, y, ctx))
		goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}
