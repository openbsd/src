/*	$OpenBSD: bn_mod_exp.c,v 1.11 2022/12/08 07:18:47 tb Exp $	*/
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
#include <stdlib.h>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "bn_local.h"

#define NUM_BITS	(BN_BITS*2)

#define INIT_MOD_EXP_FN(f) { .name = #f, .mod_exp_fn = (f), }
#define INIT_MOD_EXP_MONT_FN(f) { .name = #f, .mod_exp_mont_fn = (f), }

static const struct mod_exp_test {
	const char *name;
	int (*mod_exp_fn)(BIGNUM *,const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *);
	int (*mod_exp_mont_fn)(BIGNUM *,const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *, BN_MONT_CTX *);
} mod_exp_fn[] = {
	INIT_MOD_EXP_FN(BN_mod_exp),
	INIT_MOD_EXP_FN(BN_mod_exp_ct),
	INIT_MOD_EXP_FN(BN_mod_exp_nonct),
	INIT_MOD_EXP_FN(BN_mod_exp_recp),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_ct),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_consttime),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_nonct),
};

#define N_MOD_EXP_FN (sizeof(mod_exp_fn) / sizeof(mod_exp_fn[0]))

static int
test_mod_exp(const BIGNUM *result_simple, const BIGNUM *a, const BIGNUM *b,
    const BIGNUM *m, BN_CTX *ctx, const struct mod_exp_test *test)
{
	BIGNUM *result;
	int ret = 0;

	BN_CTX_start(ctx);

	if ((result = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (test->mod_exp_fn != NULL) {
		if (!test->mod_exp_fn(result, a, b, m, ctx)) {
			printf("%s() problems\n", test->name);
			goto err;
		}
	} else {
		if (!test->mod_exp_mont_fn(result, a, b, m, ctx, NULL)) {
			printf("%s() problems\n", test->name);
			goto err;
		}
	}

	if (BN_cmp(result_simple, result) != 0) {
		printf("\nResults from BN_mod_exp_simple and %s differ\n",
		    test->name);

		printf("a (%3d) = ", BN_num_bits(a));
		BN_print_fp(stdout, a);
		printf("\nb (%3d) = ", BN_num_bits(b));
		BN_print_fp(stdout, b);
		printf("\nm (%3d) = ", BN_num_bits(m));
		BN_print_fp(stdout, m);
		printf("\nsimple = ");
		BN_print_fp(stdout, result_simple);
		printf("\nresult = ");
		BN_print_fp(stdout, result);
		printf("\n");

		goto err;
	}

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}

int
main(int argc, char *argv[])
{
	BIGNUM *result_simple, *a, *b, *m;
	BN_CTX *ctx;
	int c, i;
	size_t j;

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	BN_CTX_start(ctx);

	if ((a = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((b = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((m = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((result_simple = BN_CTX_get(ctx)) == NULL)
		goto err;

	for (i = 0; i < 200; i++) {
		c = arc4random() % BN_BITS - BN_BITS2;
		if (!BN_rand(a, NUM_BITS + c, 0, 0))
			goto err;

		c = arc4random() % BN_BITS - BN_BITS2;
		if (!BN_rand(b, NUM_BITS + c, 0, 0))
			goto err;

		c = arc4random() % BN_BITS - BN_BITS2;
		if (!BN_rand(m, NUM_BITS + c, 0, 1))
			goto err;

		if (!BN_mod(a, a, m, ctx))
			goto err;
		if (!BN_mod(b, b, m, ctx))
			goto err;

		if ((BN_mod_exp_simple(result_simple, a, b, m, ctx)) <= 0) {
			printf("BN_mod_exp_simple() problems\n");
			goto err;
		}

		for (j = 0; j < N_MOD_EXP_FN; j++) {
			const struct mod_exp_test *test = &mod_exp_fn[j];

			if (!test_mod_exp(result_simple, a, b, m, ctx, test))
				goto err;
		}
	}

	BN_CTX_end(ctx);
	BN_CTX_free(ctx);

	return 0;

 err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	ERR_print_errors_fp(stdout);

	return 1;
}
