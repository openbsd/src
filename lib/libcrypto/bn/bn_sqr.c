/* $OpenBSD: bn_sqr.c,v 1.30 2023/04/19 10:51:22 jsing Exp $ */
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
#include "bn_internal.h"

int bn_sqr(BIGNUM *r, const BIGNUM *a, int max, BN_CTX *ctx);

/*
 * bn_sqr_comba4() computes r[] = a[] * a[] using Comba multiplication
 * (https://everything2.com/title/Comba+multiplication), where a is a
 * four word array, producing an eight word array result.
 */
#ifndef HAVE_BN_SQR_COMBA4
void
bn_sqr_comba4(BN_ULONG *r, const BN_ULONG *a)
{
	BN_ULONG c2, c1, c0;

	bn_mulw_addtw(a[0], a[0], 0, 0, 0, &c2, &c1, &r[0]);

	bn_mul2_mulw_addtw(a[1], a[0], 0, c2, c1, &c2, &c1, &r[1]);

	bn_mulw_addtw(a[1], a[1], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[2], a[0], c2, c1, c0, &c2, &c1, &r[2]);

	bn_mul2_mulw_addtw(a[3], a[0], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[2], a[1], c2, c1, c0, &c2, &c1, &r[3]);

	bn_mulw_addtw(a[2], a[2], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[3], a[1], c2, c1, c0, &c2, &c1, &r[4]);

	bn_mul2_mulw_addtw(a[3], a[2], 0, c2, c1, &c2, &c1, &r[5]);

	bn_mulw_addtw(a[3], a[3], 0, c2, c1, &c2, &r[7], &r[6]);
}
#endif

/*
 * bn_sqr_comba8() computes r[] = a[] * a[] using Comba multiplication
 * (https://everything2.com/title/Comba+multiplication), where a is an
 * eight word array, producing an 16 word array result.
 */
#ifndef HAVE_BN_SQR_COMBA8
void
bn_sqr_comba8(BN_ULONG *r, const BN_ULONG *a)
{
	BN_ULONG c2, c1, c0;

	bn_mulw_addtw(a[0], a[0], 0, 0, 0, &c2, &c1, &r[0]);

	bn_mul2_mulw_addtw(a[1], a[0], 0, c2, c1, &c2, &c1, &r[1]);

	bn_mulw_addtw(a[1], a[1], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[2], a[0], c2, c1, c0, &c2, &c1, &r[2]);

	bn_mul2_mulw_addtw(a[3], a[0], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[2], a[1], c2, c1, c0, &c2, &c1, &r[3]);

	bn_mulw_addtw(a[2], a[2], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[3], a[1], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[4], a[0], c2, c1, c0, &c2, &c1, &r[4]);

	bn_mul2_mulw_addtw(a[5], a[0], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[4], a[1], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[3], a[2], c2, c1, c0, &c2, &c1, &r[5]);

	bn_mulw_addtw(a[3], a[3], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[4], a[2], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[5], a[1], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[0], c2, c1, c0, &c2, &c1, &r[6]);

	bn_mul2_mulw_addtw(a[7], a[0], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[1], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[5], a[2], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[4], a[3], c2, c1, c0, &c2, &c1, &r[7]);

	bn_mulw_addtw(a[4], a[4], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[5], a[3], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[2], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[7], a[1], c2, c1, c0, &c2, &c1, &r[8]);

	bn_mul2_mulw_addtw(a[7], a[2], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[3], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[5], a[4], c2, c1, c0, &c2, &c1, &r[9]);

	bn_mulw_addtw(a[5], a[5], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[4], c2, c1, c0, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[7], a[3], c2, c1, c0, &c2, &c1, &r[10]);

	bn_mul2_mulw_addtw(a[7], a[4], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[6], a[5], c2, c1, c0, &c2, &c1, &r[11]);

	bn_mulw_addtw(a[6], a[6], 0, c2, c1, &c2, &c1, &c0);
	bn_mul2_mulw_addtw(a[7], a[5], c2, c1, c0, &c2, &c1, &r[12]);

	bn_mul2_mulw_addtw(a[7], a[6], 0, c2, c1, &c2, &c1, &r[13]);

	bn_mulw_addtw(a[7], a[7], 0, c2, c1, &c2, &r[15], &r[14]);
}
#endif

#ifndef HAVE_BN_SQR_WORDS
/*
 * bn_sqr_words() computes (r[i*2+1]:r[i*2]) = a[i] * a[i].
 */
void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0)
		return;

#ifndef OPENSSL_SMALL_FOOTPRINT
	while (n & ~3) {
		bn_mulw(a[0], a[0], &r[1], &r[0]);
		bn_mulw(a[1], a[1], &r[3], &r[2]);
		bn_mulw(a[2], a[2], &r[5], &r[4]);
		bn_mulw(a[3], a[3], &r[7], &r[6]);
		a += 4;
		r += 8;
		n -= 4;
	}
#endif
	while (n) {
		bn_mulw(a[0], a[0], &r[1], &r[0]);
		a++;
		r += 2;
		n--;
	}
}
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

	if (!bn_wexpand(tmp, rn))
		goto err;
	bn_sqr_normal(r->d, a->d, a->top, tmp->d);

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
	bn_correct_top(rr);

	rr->neg = 0;

	if (!bn_copy(r, rr))
		goto err;
 done:
	ret = 1;
 err:
	BN_CTX_end(ctx);

	return ret;
}
