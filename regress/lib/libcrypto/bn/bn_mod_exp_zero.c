/*	$OpenBSD: bn_mod_exp_zero.c,v 1.1 2022/12/02 17:33:38 tb Exp $ */

/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <err.h>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "bn_local.h"

#define INIT_MOD_EXP_FN(f) { .name = #f, .mod_exp_fn = (f), }
#define INIT_MOD_EXP_MONT_FN(f) { .name = #f, .mod_exp_mont_fn = (f), }

static const struct mod_exp_zero_test {
	const char *name;
	int (*mod_exp_fn)(BIGNUM *,const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *);
	int (*mod_exp_mont_fn)(BIGNUM *,const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *, BN_MONT_CTX *);
} mod_exp_zero_test_data[] = {
	INIT_MOD_EXP_FN(BN_mod_exp),
	INIT_MOD_EXP_FN(BN_mod_exp_ct),
	INIT_MOD_EXP_FN(BN_mod_exp_nonct),
	INIT_MOD_EXP_FN(BN_mod_exp_recp),
	INIT_MOD_EXP_FN(BN_mod_exp_simple),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_ct),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_consttime),
	INIT_MOD_EXP_MONT_FN(BN_mod_exp_mont_nonct),
};

#define N_MOD_EXP_ZERO_TESTS \
    (sizeof(mod_exp_zero_test_data) / sizeof(mod_exp_zero_test_data[0]))

static void
print_failure(const BIGNUM *result, const BIGNUM *a, const char *name)
{
	fprintf(stderr, "%s test failed for a = ", name);
	BN_print_fp(stderr, a);
	fprintf(stderr, "\nwant 0, got ");
	BN_print_fp(stderr, result);
	fprintf(stderr, "\n");
}

static int
bn_mod_exp_zero_test(const struct mod_exp_zero_test *test, BN_CTX *ctx,
    int use_random)
{
	const BIGNUM *one;
	BIGNUM *a, *p, *result;
	int failed = 1;

	BN_CTX_start(ctx);

	if ((a = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((p = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((result = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");

	one = BN_value_one();
	BN_zero(a);
	BN_zero(p);

	if (use_random) {
		if (!BN_rand(a, 1024, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY))
			errx(1, "BN_rand");
	}

	if (test->mod_exp_fn != NULL) {
		if (!test->mod_exp_fn(result, a, p, one, ctx)) {
			fprintf(stderr, "%s failed\n", test->name);
			ERR_print_errors_fp(stderr);
			goto err;
		}
	} else {
		if (!test->mod_exp_mont_fn(result, a, p, one, ctx, NULL)) {
			fprintf(stderr, "%s failed\n", test->name);
			ERR_print_errors_fp(stderr);
			goto err;
		}
	}

	if (!BN_is_zero(result)) {
		print_failure(result, a, test->name);
		goto err;
	}

	failed = 0;

 err:
	BN_CTX_end(ctx);

	return failed;
}

static int
bn_mod_exp_zero_word_test(BN_CTX *ctx)
{
	const char *name = "BN_mod_exp_mont_word";
	const BIGNUM *one;
	BIGNUM *p, *result;
	int failed = 1;

	BN_CTX_start(ctx);

	if ((p = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((result = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");

	one = BN_value_one();
	BN_zero(p);

	if (!BN_mod_exp_mont_word(result, 1, p, one, ctx, NULL)) {
		fprintf(stderr, "%s failed\n", name);
		ERR_print_errors_fp(stderr);
		goto err;
	}

	if (!BN_is_zero(result)) {
		print_failure(result, one, name);
		goto err;
	}

	failed = 0;

 err:
	BN_CTX_end(ctx);

	return failed;
}

static int
run_bn_mod_exp_zero_tests(void)
{
	BN_CTX *ctx;
	size_t i;
	int use_random;
	int failed = 0;

	if ((ctx = BN_CTX_new()) == NULL)
		errx(1, "BN_CTX_new");

	use_random = 1;
	for (i = 0; i < N_MOD_EXP_ZERO_TESTS; i++)
		failed |= bn_mod_exp_zero_test(&mod_exp_zero_test_data[i], ctx,
		    use_random);

	use_random = 0;
	for (i = 0; i < N_MOD_EXP_ZERO_TESTS; i++)
		failed |= bn_mod_exp_zero_test(&mod_exp_zero_test_data[i], ctx,
		    use_random);

	failed |= bn_mod_exp_zero_word_test(ctx);

	BN_CTX_free(ctx);

	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= run_bn_mod_exp_zero_tests();

	return failed;
}
