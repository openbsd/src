/* $OpenBSD: bn_kron.c,v 1.7 2022/06/20 19:32:35 tb Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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

#include "bn_lcl.h"

/* least significant word */
#define BN_lsw(n) (((n)->top == 0) ? (BN_ULONG) 0 : (n)->d[0])

/*
 * Kronecker symbol, implemented according to Henri Cohen, "A Course in
 * Computational Algebraic Number Theory", Algorithm 1.4.10.
 *
 * Returns -1, 0, or 1 on success and -2 on error.
 */

int
BN_kronecker(const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
{
	/* tab[BN_lsw(n) & 7] = (-1)^((n^2 - 1)) / 8) for odd values of n. */
	static const int tab[8] = {0, 1, 0, -1, 0, -1, 0, 1};
	BIGNUM *A, *B, *tmp;
	int k, v;
	int ret = -2;

	bn_check_top(a);
	bn_check_top(b);

	BN_CTX_start(ctx);

	if ((A = BN_CTX_get(ctx)) == NULL)
		goto end;
	if ((B = BN_CTX_get(ctx)) == NULL)
		goto end;

	if (BN_copy(A, a) == NULL)
		goto end;
	if (BN_copy(B, b) == NULL)
		goto end;

	/*
	 * Cohen's step 1:
	 */

	/* If B is zero, output 1 if |A| is 1, otherwise output 0. */
	if (BN_is_zero(B)) {
		ret = BN_abs_is_word(A, 1);
		goto end;
	}

	/*
	 * Cohen's step 2:
	 */

	/* If both are even, they have a factor in common, so output 0. */
	if (!BN_is_odd(A) && !BN_is_odd(B)) {
		ret = 0;
		goto end;
	}

	/* Factorize B = 2^v * u with odd u and replace B with u. */
	v = 0;
	while (!BN_is_bit_set(B, v))
		v++;
	if (!BN_rshift(B, B, v))
		goto end;

	/* If v is even set k = 1, otherwise set it to (-1)^((A^2 - 1) / 8). */
	k = 1;
	if (v % 2 != 0)
		k = tab[BN_lsw(A) & 7];

	/*
	 * If B is negative, replace it with -B and if A is also negative
	 * replace k with -k.
	 */
	if (BN_is_negative(B)) {
		BN_set_negative(B, 0);

		if (BN_is_negative(A))
			k = -k;
	}

	/*
	 * Now B is positive and odd, so compute the Jacobi symbol (A/B)
	 * and multiply it by k.
	 */

	while (1) {
		/*
		 * Cohen's step 3:
		 */

		/* B is positive and odd. */

		/* If A is zero output k if B is one, otherwise output 0. */
		if (BN_is_zero(A)) {
			ret = BN_is_one(B) ? k : 0;
			goto end;
		}

		/* Factorize A = 2^v * u with odd u and replace A with u. */
		v = 0;
		while (!BN_is_bit_set(A, v))
			v++;
		if (!BN_rshift(A, A, v))
			goto end;

		/* If v is odd, multiply k with (-1)^((B^2 - 1) / 8). */
		if (v % 2 != 0)
			k *= tab[BN_lsw(B) & 7];

		/*
		 * Cohen's step 4:
		 */

		/*
		 * Apply the reciprocity law: multiply k by (-1)^((A-1)(B-1)/4).
		 *
		 * This expression is -1 if and only if A and B are 3 (mod 4).
		 * In turn, this is the case if and only if their two's
		 * complement representations have the second bit set.
		 * A could be negative in the first iteration, B is positive.
		 */
		if ((BN_is_negative(A) ? ~BN_lsw(A) : BN_lsw(A)) & BN_lsw(B) & 2)
			k = -k;

		/*
		 * (A, B) := (B mod |A|, |A|)
		 *
		 * Once this is done, we know that 0 < A < B at the start of the
		 * loop. Since B is strictly decreasing, the loop terminates.
		 */

		if (!BN_nnmod(B, B, A, ctx))
			goto end;

		tmp = A;
		A = B;
		B = tmp;

		BN_set_negative(B, 0);
	}

 end:
	BN_CTX_end(ctx);

	return ret;
}
