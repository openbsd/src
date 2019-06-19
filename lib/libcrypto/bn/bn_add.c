/* $OpenBSD: bn_add.c,v 1.13 2018/07/23 18:07:21 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>

#include <openssl/err.h>

#include "bn_lcl.h"

int
BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int ret, r_neg;

	bn_check_top(a);
	bn_check_top(b);

	if (a->neg == b->neg) {
		r_neg = a->neg;
		ret = BN_uadd(r, a, b);
	} else {
		int cmp = BN_ucmp(a, b);

		if (cmp > 0) {
			r_neg = a->neg;
			ret = BN_usub(r, a, b);
		} else if (cmp < 0) {
			r_neg = b->neg;
			ret = BN_usub(r, b, a);
		} else {
			r_neg = 0;
			BN_zero(r);
			ret = 1;
		}
	}

	r->neg = r_neg;
	bn_check_top(r);
	return ret;
}

int
BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max, min, dif;
	const BN_ULONG *ap, *bp;
	BN_ULONG *rp, carry, t1, t2;

	bn_check_top(a);
	bn_check_top(b);

	if (a->top < b->top) {
		const BIGNUM *tmp;

		tmp = a;
		a = b;
		b = tmp;
	}
	max = a->top;
	min = b->top;
	dif = max - min;

	if (bn_wexpand(r, max + 1) == NULL)
		return 0;

	r->top = max;

	ap = a->d;
	bp = b->d;
	rp = r->d;

	carry = bn_add_words(rp, ap, bp, min);
	rp += min;
	ap += min;

	while (dif) {
		dif--;
		t1 = *(ap++);
		t2 = (t1 + carry) & BN_MASK2;
		*(rp++) = t2;
		carry &= (t2 == 0);
	}
	*rp = carry;
	r->top += carry;

	r->neg = 0;
	bn_check_top(r);
	return 1;
}

int
BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max, min, dif;
	const BN_ULONG *ap, *bp;
	BN_ULONG t1, t2, borrow, *rp;

	bn_check_top(a);
	bn_check_top(b);

	max = a->top;
	min = b->top;
	dif = max - min;

	if (dif < 0) {
		BNerror(BN_R_ARG2_LT_ARG3);
		return 0;
	}

	if (bn_wexpand(r, max) == NULL)
		return 0;

	ap = a->d;
	bp = b->d;
	rp = r->d;

	borrow = bn_sub_words(rp, ap, bp, min);
	ap += min;
	rp += min;

	while (dif) {
		dif--;
		t1 = *(ap++);
		t2 = (t1 - borrow) & BN_MASK2;
		*(rp++) = t2;
		borrow &= (t1 == 0);
	}

	while (max > 0 && *--rp == 0)
		max--;

	r->top = max;
	r->neg = 0;
	bn_correct_top(r);
	return 1;
}

int
BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int ret, r_neg;

	bn_check_top(a);
	bn_check_top(b);

	if (a->neg != b->neg) {
		r_neg = a->neg;
		ret = BN_uadd(r, a, b);
	} else {
		int cmp = BN_ucmp(a, b);

		if (cmp > 0) {
			r_neg = a->neg;
			ret = BN_usub(r, a, b);
		} else if (cmp < 0) {
			r_neg = !b->neg;
			ret = BN_usub(r, b, a);
		} else {
			r_neg = 0;
			BN_zero(r);
			ret = 1;
		}
	}

	r->neg = r_neg;
	bn_check_top(r);
	return ret;
}
