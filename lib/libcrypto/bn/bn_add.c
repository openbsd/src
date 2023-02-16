/* $OpenBSD: bn_add.c,v 1.23 2023/02/16 04:42:20 jsing Exp $ */
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <openssl/err.h>

#include "bn_arch.h"
#include "bn_local.h"
#include "bn_internal.h"

BN_ULONG bn_add(BIGNUM *r, int rn, const BIGNUM *a, const BIGNUM *b);
BN_ULONG bn_sub(BIGNUM *r, int rn, const BIGNUM *a, const BIGNUM *b);

/*
 * bn_add_words() computes (carry:r[i]) = a[i] + b[i] + carry, where a and b
 * are both arrays of words. Any carry resulting from the addition is returned.
 */
#ifndef HAVE_BN_ADD_WORDS
BN_ULONG
bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG carry = 0;

	assert(n >= 0);
	if (n <= 0)
		return 0;

#ifndef OPENSSL_SMALL_FOOTPRINT
	while (n & ~3) {
		bn_addw_addw(a[0], b[0], carry, &carry, &r[0]);
		bn_addw_addw(a[1], b[1], carry, &carry, &r[1]);
		bn_addw_addw(a[2], b[2], carry, &carry, &r[2]);
		bn_addw_addw(a[3], b[3], carry, &carry, &r[3]);
		a += 4;
		b += 4;
		r += 4;
		n -= 4;
	}
#endif
	while (n) {
		bn_addw_addw(a[0], b[0], carry, &carry, &r[0]);
		a++;
		b++;
		r++;
		n--;
	}
	return carry;
}
#endif

/*
 * bn_sub_words() computes (borrow:r[i]) = a[i] - b[i] - borrow, where a and b
 * are both arrays of words. Any borrow resulting from the subtraction is
 * returned.
 */
#ifndef HAVE_BN_SUB_WORDS
BN_ULONG
bn_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG borrow = 0;

	assert(n >= 0);
	if (n <= 0)
		return 0;

#ifndef OPENSSL_SMALL_FOOTPRINT
	while (n & ~3) {
		bn_subw_subw(a[0], b[0], borrow, &borrow, &r[0]);
		bn_subw_subw(a[1], b[1], borrow, &borrow, &r[1]);
		bn_subw_subw(a[2], b[2], borrow, &borrow, &r[2]);
		bn_subw_subw(a[3], b[3], borrow, &borrow, &r[3]);
		a += 4;
		b += 4;
		r += 4;
		n -= 4;
	}
#endif
	while (n) {
		bn_subw_subw(a[0], b[0], borrow, &borrow, &r[0]);
		a++;
		b++;
		r++;
		n--;
	}
	return borrow;
}
#endif

/*
 * bn_add() computes a + b, storing the result in r (which may be the same as a
 * or b). The caller must ensure that r has been expanded to max(a->top, b->top)
 * words. Any carry resulting from the addition is returned.
 */
#ifndef HAVE_BN_ADD
BN_ULONG
bn_add(BIGNUM *r, int rn, const BIGNUM *a, const BIGNUM *b)
{
	BN_ULONG *rp, carry, t1, t2;
	const BN_ULONG *ap, *bp;
	int max, min, dif;

	if (a->top < b->top) {
		const BIGNUM *tmp;

		tmp = a;
		a = b;
		b = tmp;
	}
	max = a->top;
	min = b->top;
	dif = max - min;

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

	return carry;
}
#endif

/*
 * bn_sub() computes a - b, storing the result in r (which may be the same as a
 * or b). The caller must ensure that the number of words in a is greater than
 * or equal to the number of words in b and that r has been expanded to
 * a->top words. Any borrow resulting from the subtraction is returned.
 */
#ifndef HAVE_BN_SUB
BN_ULONG
bn_sub(BIGNUM *r, int rn, const BIGNUM *a, const BIGNUM *b)
{
	BN_ULONG t1, t2, borrow, *rp;
	const BN_ULONG *ap, *bp;
	int max, min, dif;

	max = a->top;
	min = b->top;
	dif = max - min;

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

	return borrow;
}
#endif

int
BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	BN_ULONG carry;
	int rn;

	if ((rn = a->top) < b->top)
		rn = b->top;
	if (rn == INT_MAX)
		return 0;
	if (!bn_wexpand(r, rn + 1))
		return 0;

	carry = bn_add(r, rn, a, b);
	r->d[rn] = carry;

	r->top = rn + (carry & 1);
	r->neg = 0;

	return 1;
}

int
BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	BN_ULONG borrow;
	int rn;

	if (a->top < b->top) {
		BNerror(BN_R_ARG2_LT_ARG3);
		return 0;
	}
	rn = a->top;

	if (!bn_wexpand(r, rn))
		return 0;

	borrow = bn_sub(r, rn, a, b);
	if (borrow > 0) {
		BNerror(BN_R_ARG2_LT_ARG3);
		return 0;
	}

	r->top = rn;
	r->neg = 0;

	bn_correct_top(r);

	return 1;
}

int
BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int ret, r_neg;

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

	BN_set_negative(r, r_neg);

	return ret;
}

int
BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int ret, r_neg;

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

	BN_set_negative(r, r_neg);

	return ret;
}
