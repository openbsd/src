/*	$OpenBSD: bn_mod_exp.c,v 1.22 2023/03/26 19:56:28 tb Exp $ */

/*
 * Copyright (c) 2022,2023 Theo Buehler <tb@openbsd.org>
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

#define N_MOD_EXP_TESTS		400
#define N_MOD_EXP2_TESTS	100

#define INIT_MOD_EXP_FN(f) { .name = #f, .mod_exp_fn = (f), }
#define INIT_MOD_EXP_MONT_FN(f) { .name = #f, .mod_exp_mont_fn = (f), }

static int
bn_mod_exp2_mont_first(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *mctx)
{
	const BIGNUM *one = BN_value_one();

	return BN_mod_exp2_mont(r, a, p, one, one, m, ctx, mctx);
}

static int
bn_mod_exp2_mont_second(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *mctx)
{
	const BIGNUM *one = BN_value_one();

	return BN_mod_exp2_mont(r, one, one, a, p, m, ctx, mctx);
}

static const struct mod_exp_zero_test {
	const char *name;
	int (*mod_exp_fn)(BIGNUM *, const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *);
	int (*mod_exp_mont_fn)(BIGNUM *, const BIGNUM *, const BIGNUM *,
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
	INIT_MOD_EXP_MONT_FN(bn_mod_exp2_mont_first),
	INIT_MOD_EXP_MONT_FN(bn_mod_exp2_mont_second),
};

#define N_MOD_EXP_ZERO_TESTS \
    (sizeof(mod_exp_zero_test_data) / sizeof(mod_exp_zero_test_data[0]))

static void
print_failure(const BIGNUM *got, const BIGNUM *a, const char *name)
{
	fprintf(stderr, "%s test failed for a = ", name);
	BN_print_fp(stderr, a);
	fprintf(stderr, "\nwant 0, got ");
	BN_print_fp(stderr, got);
	fprintf(stderr, "\n");
}

static int
bn_mod_exp_zero_test(const struct mod_exp_zero_test *test, BN_CTX *ctx,
    int use_random)
{
	const BIGNUM *one;
	BIGNUM *a, *p, *got;
	int failed = 1;

	BN_CTX_start(ctx);

	if ((a = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((p = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((got = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");

	one = BN_value_one();
	BN_zero(a);
	BN_zero(p);

	if (use_random) {
		if (!BN_rand(a, 1024, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY))
			errx(1, "BN_rand");
	}

	if (test->mod_exp_fn != NULL) {
		if (!test->mod_exp_fn(got, a, p, one, ctx)) {
			fprintf(stderr, "%s failed\n", test->name);
			ERR_print_errors_fp(stderr);
			goto err;
		}
	} else {
		if (!test->mod_exp_mont_fn(got, a, p, one, ctx, NULL)) {
			fprintf(stderr, "%s failed\n", test->name);
			ERR_print_errors_fp(stderr);
			goto err;
		}
	}

	if (!BN_is_zero(got)) {
		print_failure(got, a, test->name);
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
	BIGNUM *p, *got;
	int failed = 1;

	BN_CTX_start(ctx);

	if ((p = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((got = BN_CTX_get(ctx)) == NULL)
		errx(1, "BN_CTX_get");

	one = BN_value_one();
	BN_zero(p);

	if (!BN_mod_exp_mont_word(got, 1, p, one, ctx, NULL)) {
		fprintf(stderr, "%s failed\n", name);
		ERR_print_errors_fp(stderr);
		goto err;
	}

	if (!BN_is_zero(got)) {
		print_failure(got, one, name);
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

static const struct mod_exp_test {
	const char *name;
	int (*mod_exp_fn)(BIGNUM *, const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *);
	int (*mod_exp_mont_fn)(BIGNUM *, const BIGNUM *, const BIGNUM *,
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
	INIT_MOD_EXP_MONT_FN(bn_mod_exp2_mont_first),
	INIT_MOD_EXP_MONT_FN(bn_mod_exp2_mont_second),
};

#define N_MOD_EXP_FN (sizeof(mod_exp_fn) / sizeof(mod_exp_fn[0]))

static int
generate_bn(BIGNUM *bn, int avg_bits, int deviate, int force_odd)
{
	int bits;

	if (avg_bits <= 0 || deviate <= 0 || deviate >= avg_bits)
		return 0;

	bits = avg_bits + arc4random_uniform(deviate) - deviate;

	return BN_rand(bn, bits, 0, force_odd);
}

static int
generate_test_triple(int reduce, BIGNUM *a, BIGNUM *p, BIGNUM *m, BN_CTX *ctx)
{
	BIGNUM *mmodified;
	BN_ULONG multiple;
	int avg = 2 * BN_BITS, deviate = BN_BITS / 2;
	int ret = 0;

	if (!generate_bn(a, avg, deviate, 0))
		return 0;

	if (!generate_bn(p, avg, deviate, 0))
		return 0;

	if (!generate_bn(m, avg, deviate, 1))
		return 0;

	if (reduce)
		return BN_mod(a, a, m, ctx);

	/*
	 * Add a random multiple of m to a to test unreduced exponentiation.
	 */

	BN_CTX_start(ctx);

	if ((mmodified = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (BN_copy(mmodified, m) == NULL)
		goto err;

	multiple = arc4random_uniform(1023) + 2;

	if (!BN_mul_word(mmodified, multiple))
		goto err;

	if (!BN_add(a, a, mmodified))
		goto err;

	ret = 1;
 err:
	BN_CTX_end(ctx);

	return ret;
}

static int
generate_test_quintuple(int reduce, BIGNUM *a1, BIGNUM *p1,
    BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx)
{
	BIGNUM *mmodified;
	BN_ULONG multiple;
	int avg = 2 * BN_BITS, deviate = BN_BITS / 2;
	int ret = 0;

	if (!generate_bn(a1, avg, deviate, 0))
		return 0;

	if (!generate_bn(p1, avg, deviate, 0))
		return 0;

	if (!generate_bn(a2, avg, deviate, 0))
		return 0;

	if (!generate_bn(p2, avg, deviate, 0))
		return 0;

	if (!generate_bn(m, avg, deviate, 1))
		return 0;

	if (reduce) {
		if (!BN_mod(a1, a1, m, ctx))
			return 0;

		return BN_mod(a2, a2, m, ctx);
	}

	/*
	 * Add a random multiple of m to a to test unreduced exponentiation.
	 */

	BN_CTX_start(ctx);

	if ((mmodified = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (BN_copy(mmodified, m) == NULL)
		goto err;

	multiple = arc4random_uniform(16) + 2;

	if (!BN_mul_word(mmodified, multiple))
		goto err;

	if (!BN_add(a1, a1, mmodified))
		goto err;

	if (!BN_add(a2, a2, mmodified))
		goto err;

	ret = 1;
 err:
	BN_CTX_end(ctx);

	return ret;
}

static void
dump_exp_results(const BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
    const BIGNUM *want, const BIGNUM *got, const char *name)
{
	printf("BN_mod_exp_simple() and %s() disagree", name);

	printf("\nwant: ");
	BN_print_fp(stdout, want);
	printf("\ngot:  ");
	BN_print_fp(stdout, got);

	printf("\na: ");
	BN_print_fp(stdout, a);
	printf("\np: ");
	BN_print_fp(stdout, p);
	printf("\nm: ");
	BN_print_fp(stdout, m);
	printf("\n\n");
}

static int
test_mod_exp(const BIGNUM *want, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, const struct mod_exp_test *test)
{
	BIGNUM *got;
	int mod_exp_ret;
	int ret = 0;

	BN_CTX_start(ctx);

	if ((got = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (test->mod_exp_fn != NULL)
		mod_exp_ret = test->mod_exp_fn(got, a, p, m, ctx);
	else
		mod_exp_ret = test->mod_exp_mont_fn(got, a, p, m, ctx, NULL);

	if (!mod_exp_ret)
		errx(1, "%s() failed", test->name);

	if (BN_cmp(want, got) != 0) {
		dump_exp_results(a, p, m, want, got, test->name);
		goto err;
	}

	ret = 1;

 err:
	BN_CTX_end(ctx);

	return ret;
}

static int
bn_mod_exp_test(int reduce, BIGNUM *want, BIGNUM *a, BIGNUM *p, BIGNUM *m,
    BN_CTX *ctx)
{
	size_t i, j;
	int failed = 0;

	if (!generate_test_triple(reduce, a, p, m, ctx))
		errx(1, "generate_test_triple");

	for (i = 0; i < 4; i++) {
		BN_set_negative(a, i & 1);
		BN_set_negative(p, (i >> 1) & 1);

		if ((BN_mod_exp_simple(want, a, p, m, ctx)) <= 0)
			errx(1, "BN_mod_exp_simple");

		for (j = 0; j < N_MOD_EXP_FN; j++) {
			const struct mod_exp_test *test = &mod_exp_fn[j];

			if (!test_mod_exp(want, a, p, m, ctx, test))
				failed |= 1;
		}
	}

	return failed;
}

static int
run_bn_mod_exp_tests(void)
{
	BIGNUM *a, *p, *m, *want;
	BN_CTX *ctx;
	int i;
	int reduce;
	int failed = 0;

	if ((ctx = BN_CTX_new()) == NULL)
		errx(1, "BN_CTX_new");

	BN_CTX_start(ctx);

	if ((a = BN_CTX_get(ctx)) == NULL)
		errx(1, "a = BN_CTX_get()");
	if ((p = BN_CTX_get(ctx)) == NULL)
		errx(1, "p = BN_CTX_get()");
	if ((m = BN_CTX_get(ctx)) == NULL)
		errx(1, "m = BN_CTX_get()");
	if ((want = BN_CTX_get(ctx)) == NULL)
		errx(1, "want = BN_CTX_get()");

	reduce = 0;
	for (i = 0; i < N_MOD_EXP_TESTS; i++)
		failed |= bn_mod_exp_test(reduce, want, a, p, m, ctx);

	reduce = 1;
	for (i = 0; i < N_MOD_EXP_TESTS; i++)
		failed |= bn_mod_exp_test(reduce, want, a, p, m, ctx);

	BN_CTX_end(ctx);
	BN_CTX_free(ctx);

	return failed;
}

static void
dump_exp2_results(const BIGNUM *a1, const BIGNUM *p1, const BIGNUM *a2,
    const BIGNUM *p2, const BIGNUM *m, const BIGNUM *want, const BIGNUM *got)
{
	printf("BN_mod_exp_simple() and BN_mod_exp2_mont() disagree");

	printf("\nwant: ");
	BN_print_fp(stdout, want);
	printf("\ngot:  ");
	BN_print_fp(stdout, got);

	printf("\na1: ");
	BN_print_fp(stdout, a1);
	printf("\np1: ");
	BN_print_fp(stdout, p1);
	printf("\na2: ");
	BN_print_fp(stdout, a2);
	printf("\np2: ");
	BN_print_fp(stdout, p2);
	printf("\nm: ");
	BN_print_fp(stdout, m);
	printf("\n\n");
}

static int
bn_mod_exp2_simple(BIGNUM *out, const BIGNUM *a1, const BIGNUM *p1,
    const BIGNUM *a2, const BIGNUM *p2, const BIGNUM *m, BN_CTX *ctx)
{
	BIGNUM *fact1, *fact2;
	int ret = 0;

	BN_CTX_start(ctx);

	if ((fact1 = BN_CTX_get(ctx)) == NULL)
		goto err;
	if ((fact2 = BN_CTX_get(ctx)) == NULL)
		goto err;

	if (!BN_mod_exp_simple(fact1, a1, p1, m, ctx))
		goto err;
	if (!BN_mod_exp_simple(fact2, a2, p2, m, ctx))
		goto err;
	if (!BN_mod_mul(out, fact1, fact2, m, ctx))
		goto err;

	ret = 1;
 err:
	BN_CTX_end(ctx);

	return ret;
}

static int
bn_mod_exp2_test(int reduce, BIGNUM *want, BIGNUM *got, BIGNUM *a1, BIGNUM *p1,
    BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx)
{
	size_t i;
	int failed = 0;

	if (!generate_test_quintuple(reduce, a1, p1, a2, p2, m, ctx))
		errx(1, "generate_test_quintuple");

	for (i = 0; i < 16; i++) {
		BN_set_negative(a1, i & 1);
		BN_set_negative(p1, (i >> 1) & 1);
		BN_set_negative(a2, (i >> 2) & 1);
		BN_set_negative(p2, (i >> 3) & 1);

		if (!bn_mod_exp2_simple(want, a1, p1, a2, p2, m, ctx))
			errx(1, "BN_mod_exp_simple");

		if (!BN_mod_exp2_mont(got, a1, p1, a2, p2, m, ctx, NULL))
			errx(1, "BN_mod_exp2_mont");

		if (BN_cmp(want, got) != 0) {
			dump_exp2_results(a1, p1, a2, p2, m, want, got);
			failed |= 1;
		}
	}

	return failed;
}
static int
run_bn_mod_exp2_tests(void)
{
	BIGNUM *a1, *p1, *a2, *p2, *m, *want, *got;
	BN_CTX *ctx;
	int i;
	int reduce;
	int failed = 0;

	if ((ctx = BN_CTX_new()) == NULL)
		errx(1, "BN_CTX_new");

	BN_CTX_start(ctx);

	if ((a1 = BN_CTX_get(ctx)) == NULL)
		errx(1, "a1 = BN_CTX_get()");
	if ((p1 = BN_CTX_get(ctx)) == NULL)
		errx(1, "p1 = BN_CTX_get()");
	if ((a2 = BN_CTX_get(ctx)) == NULL)
		errx(1, "a2 = BN_CTX_get()");
	if ((p2 = BN_CTX_get(ctx)) == NULL)
		errx(1, "p2 = BN_CTX_get()");
	if ((m = BN_CTX_get(ctx)) == NULL)
		errx(1, "m = BN_CTX_get()");
	if ((want = BN_CTX_get(ctx)) == NULL)
		errx(1, "want = BN_CTX_get()");
	if ((got = BN_CTX_get(ctx)) == NULL)
		errx(1, "got = BN_CTX_get()");

	reduce = 0;
	for (i = 0; i < N_MOD_EXP_TESTS; i++)
		failed |= bn_mod_exp2_test(reduce, want, got, a1, p1, a2, p2, m,
		    ctx);

	reduce = 1;
	for (i = 0; i < N_MOD_EXP_TESTS; i++)
		failed |= bn_mod_exp2_test(reduce, want, got, a1, p1, a2, p2, m,
		    ctx);

	BN_CTX_end(ctx);
	BN_CTX_free(ctx);

	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= run_bn_mod_exp_zero_tests();
	failed |= run_bn_mod_exp_tests();
	failed |= run_bn_mod_exp2_tests();

	return failed;
}
