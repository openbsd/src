/* $OpenBSD: bn_sqr.c,v 1.22 2023/01/23 12:09:06 jsing Exp $ */
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
#include <stdio.h>
#include <string.h>

#include "bn_arch.h"
#include "bn_local.h"

int bn_sqr(BIGNUM *r, const BIGNUM *a, int max, BN_CTX *ctx);

#ifndef HAVE_BN_SQR_COMBA4
void
bn_sqr_comba4(BN_ULONG *r, const BN_ULONG *a)
{
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	sqr_add_c(a, 0, c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	sqr_add_c2(a, 1, 0, c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	sqr_add_c(a, 1, c3, c1, c2);
	sqr_add_c2(a, 2, 0, c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	sqr_add_c2(a, 3, 0, c1, c2, c3);
	sqr_add_c2(a, 2, 1, c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	sqr_add_c(a, 2, c2, c3, c1);
	sqr_add_c2(a, 3, 1, c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	sqr_add_c2(a, 3, 2, c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	sqr_add_c(a, 3, c1, c2, c3);
	r[6] = c1;
	r[7] = c2;
}
#endif

#ifndef HAVE_BN_SQR_COMBA8
void
bn_sqr_comba8(BN_ULONG *r, const BN_ULONG *a)
{
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	sqr_add_c(a, 0, c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	sqr_add_c2(a, 1, 0, c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	sqr_add_c(a, 1, c3, c1, c2);
	sqr_add_c2(a, 2, 0, c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	sqr_add_c2(a, 3, 0, c1, c2, c3);
	sqr_add_c2(a, 2, 1, c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	sqr_add_c(a, 2, c2, c3, c1);
	sqr_add_c2(a, 3, 1, c2, c3, c1);
	sqr_add_c2(a, 4, 0, c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	sqr_add_c2(a, 5, 0, c3, c1, c2);
	sqr_add_c2(a, 4, 1, c3, c1, c2);
	sqr_add_c2(a, 3, 2, c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	sqr_add_c(a, 3, c1, c2, c3);
	sqr_add_c2(a, 4, 2, c1, c2, c3);
	sqr_add_c2(a, 5, 1, c1, c2, c3);
	sqr_add_c2(a, 6, 0, c1, c2, c3);
	r[6] = c1;
	c1 = 0;
	sqr_add_c2(a, 7, 0, c2, c3, c1);
	sqr_add_c2(a, 6, 1, c2, c3, c1);
	sqr_add_c2(a, 5, 2, c2, c3, c1);
	sqr_add_c2(a, 4, 3, c2, c3, c1);
	r[7] = c2;
	c2 = 0;
	sqr_add_c(a, 4, c3, c1, c2);
	sqr_add_c2(a, 5, 3, c3, c1, c2);
	sqr_add_c2(a, 6, 2, c3, c1, c2);
	sqr_add_c2(a, 7, 1, c3, c1, c2);
	r[8] = c3;
	c3 = 0;
	sqr_add_c2(a, 7, 2, c1, c2, c3);
	sqr_add_c2(a, 6, 3, c1, c2, c3);
	sqr_add_c2(a, 5, 4, c1, c2, c3);
	r[9] = c1;
	c1 = 0;
	sqr_add_c(a, 5, c2, c3, c1);
	sqr_add_c2(a, 6, 4, c2, c3, c1);
	sqr_add_c2(a, 7, 3, c2, c3, c1);
	r[10] = c2;
	c2 = 0;
	sqr_add_c2(a, 7, 4, c3, c1, c2);
	sqr_add_c2(a, 6, 5, c3, c1, c2);
	r[11] = c3;
	c3 = 0;
	sqr_add_c(a, 6, c1, c2, c3);
	sqr_add_c2(a, 7, 5, c1, c2, c3);
	r[12] = c1;
	c1 = 0;
	sqr_add_c2(a, 7, 6, c2, c3, c1);
	r[13] = c2;
	c2 = 0;
	sqr_add_c(a, 7, c3, c1, c2);
	r[14] = c3;
	r[15] = c1;
}
#endif

#ifndef HAVE_BN_SQR_WORDS
#if defined(BN_LLONG) || defined(BN_UMULT_HIGH)
void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0)
		return;

#ifndef OPENSSL_SMALL_FOOTPRINT
	while (n & ~3) {
		sqr(r[0], r[1], a[0]);
		sqr(r[2], r[3], a[1]);
		sqr(r[4], r[5], a[2]);
		sqr(r[6], r[7], a[3]);
		a += 4;
		r += 8;
		n -= 4;
	}
#endif
	while (n) {
		sqr(r[0], r[1], a[0]);
		a++;
		r += 2;
		n--;
	}
}
#else /* !(defined(BN_LLONG) || defined(BN_UMULT_HIGH)) */
void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0)
		return;

#ifndef OPENSSL_SMALL_FOOTPRINT
	while (n & ~3) {
		sqr64(r[0], r[1], a[0]);
		sqr64(r[2], r[3], a[1]);
		sqr64(r[4], r[5], a[2]);
		sqr64(r[6], r[7], a[3]);
		a += 4;
		r += 8;
		n -= 4;
	}
#endif
	while (n) {
		sqr64(r[0], r[1], a[0]);
		a++;
		r += 2;
		n--;
	}
}
#endif
#endif

/* tmp must have 2*n words */
void
bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, int n, BN_ULONG *tmp)
{
	int i, j, max;
	const BN_ULONG *ap;
	BN_ULONG *rp;

	max = n * 2;
	ap = a;
	rp = r;
	rp[0] = rp[max - 1] = 0;
	rp++;
	j = n;

	if (--j > 0) {
		ap++;
		rp[j] = bn_mul_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	for (i = n - 2; i > 0; i--) {
		j--;
		ap++;
		rp[j] = bn_mul_add_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	bn_add_words(r, r, r, max);

	/* There will not be a carry */

	bn_sqr_words(tmp, a, n);

	bn_add_words(r, r, tmp, max);
}

#ifdef BN_RECURSION
/* r is 2*n words in size,
 * a and b are both n words in size.    (There's not actually a 'b' here ...)
 * n must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
void
bn_sqr_recursive(BN_ULONG *r, const BN_ULONG *a, int n2, BN_ULONG *t)
{
	int n = n2 / 2;
	int zero, c1;
	BN_ULONG ln, lo, *p;

	if (n2 == 4) {
		bn_sqr_comba4(r, a);
		return;
	} else if (n2 == 8) {
		bn_sqr_comba8(r, a);
		return;
	}
	if (n2 < BN_SQR_RECURSIVE_SIZE_NORMAL) {
		bn_sqr_normal(r, a, n2, t);
		return;
	}
	/* r=(a[0]-a[1])*(a[1]-a[0]) */
	c1 = bn_cmp_words(a, &(a[n]), n);
	zero = 0;
	if (c1 > 0)
		bn_sub_words(t, a, &(a[n]), n);
	else if (c1 < 0)
		bn_sub_words(t, &(a[n]), a, n);
	else
		zero = 1;

	/* The result will always be negative unless it is zero */
	p = &(t[n2*2]);

	if (!zero)
		bn_sqr_recursive(&(t[n2]), t, n, p);
	else
		memset(&(t[n2]), 0, n2 * sizeof(BN_ULONG));
	bn_sqr_recursive(r, a, n, p);
	bn_sqr_recursive(&(r[n2]), &(a[n]), n, p);

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0]), it is negative or zero
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	/* t[32] is negative */
	c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0])+(a[0]*a[0])+(a[1]*a[1])
	 * r[10] holds (a[0]*a[0])
	 * r[32] holds (a[1]*a[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n + n2]);
		lo= *p;
		ln = (lo + c1) & BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo= *p;
				ln = (lo + 1) & BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}
#endif

/*
 * bn_sqr() computes a * a, storing the result in r. The caller must ensure that
 * r is not the same BIGNUM as a and that r has been expanded to rn = a->top * 2
 * words.
 */
#ifndef HAVE_BN_SQR
int
bn_sqr(BIGNUM *r, const BIGNUM *a, int rn, BN_CTX *ctx)
{
	BIGNUM *tmp;
	int ret = 0;

	BN_CTX_start(ctx);

	if ((tmp = BN_CTX_get(ctx)) == NULL)
		goto err;

#if defined(BN_RECURSION)
	if (a->top < BN_SQR_RECURSIVE_SIZE_NORMAL) {
		BN_ULONG t[BN_SQR_RECURSIVE_SIZE_NORMAL*2];
		bn_sqr_normal(r->d, a->d, a->top, t);
	} else {
		int j, k;

		j = BN_num_bits_word((BN_ULONG)a->top);
		j = 1 << (j - 1);
		k = j + j;
		if (a->top == j) {
			if (!bn_wexpand(tmp, k * 2))
				goto err;
			bn_sqr_recursive(r->d, a->d, a->top, tmp->d);
		} else {
			if (!bn_wexpand(tmp, rn))
				goto err;
			bn_sqr_normal(r->d, a->d, a->top, tmp->d);
		}
	}
#else
	if (!bn_wexpand(tmp, rn))
		goto err;
	bn_sqr_normal(r->d, a->d, a->top, tmp->d);
#endif

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}
#endif

int
BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx)
{
	BIGNUM *rr;
	int rn;
	int ret = 1;

	BN_CTX_start(ctx);

	if (BN_is_zero(a)) {
		BN_zero(r);
		goto done;
	}

	if ((rr = r) == a)
		rr = BN_CTX_get(ctx);
	if (rr == NULL)
		goto err;

	rn = a->top * 2;
	if (rn < a->top)
		goto err;
	if (!bn_wexpand(rr, rn))
		goto err;

	if (a->top == 4) {
		bn_sqr_comba4(rr->d, a->d);
	} else if (a->top == 8) {
		bn_sqr_comba8(rr->d, a->d);
	} else {
		if (!bn_sqr(rr, a, rn, ctx))
			goto err;
	}

	rr->top = rn;
	rr->neg = 0;

	bn_correct_top(rr);

	if (rr != r)
		BN_copy(r, rr);

 done:
	ret = 1;
 err:
	BN_CTX_end(ctx);

	return ret;
}
