/*	$OpenBSD: bn_mod_sqrt.c,v 1.1 2022/03/15 16:28:42 tb Exp $ */
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

#include <openssl/bn.h>

/* Test that sqrt * sqrt = A (mod p) where p is a prime */
struct mod_sqrt_test {
	const char *sqrt;
	const char *a;
	const char *p;
	int bn_mod_sqrt_fails;
} mod_sqrt_test_data[] = {
	{
		.sqrt = "1",
		.a = "1",
		.p = "2",
		.bn_mod_sqrt_fails = 0,
	},
	{
		.sqrt = "-1",
		.a = "20a7ee",
		.p = "460201", /* 460201 == 4D5 * E7D */
		.bn_mod_sqrt_fails = 1,
	},
	{
		.sqrt = "-1",
		.a = "65bebdb00a96fc814ec44b81f98b59fba3c30203928fa521"
		     "4c51e0a97091645280c947b005847f239758482b9bfc45b0"
		     "66fde340d1fe32fc9c1bf02e1b2d0ed",
		.p = "9df9d6cc20b8540411af4e5357ef2b0353cb1f2ab5ffc3e2"
		     "46b41c32f71e951f",
		.bn_mod_sqrt_fails = 1,
	},
};

const size_t N_TESTS = sizeof(mod_sqrt_test_data) / sizeof(*mod_sqrt_test_data);

int mod_sqrt_test(struct mod_sqrt_test *test);

int
mod_sqrt_test(struct mod_sqrt_test *test)
{
	BN_CTX *ctx = NULL;
	BIGNUM *a = NULL, *p = NULL, *want = NULL, *got = NULL, *diff = NULL;
	int failed = 1;

	if ((ctx = BN_CTX_new()) == NULL) {
		fprintf(stderr, "BN_CTX_new failed\n");
		goto out;
	}

	if (!BN_hex2bn(&a, test->a)) {
		fprintf(stderr, "BN_hex2bn(a) failed\n");
		goto out;
	}
	if (!BN_hex2bn(&p, test->p)) {
		fprintf(stderr, "BN_hex2bn(p) failed\n");
		goto out;
	}
	if (!BN_hex2bn(&want, test->sqrt)) {
		fprintf(stderr, "BN_hex2bn(want) failed\n");
		goto out;
	}

	if (((got = BN_mod_sqrt(NULL, a, p, ctx)) == NULL) !=
	   test->bn_mod_sqrt_fails) {
		fprintf(stderr, "BN_mod_sqrt %s unexpectedly\n",
		    test->bn_mod_sqrt_fails ? "succeeded" : "failed");
		goto out;
	}

	if (test->bn_mod_sqrt_fails) {
		failed = 0;
		goto out;
	}

	if ((diff = BN_new()) == NULL) {
		fprintf(stderr, "diff = BN_new() failed\n");
		goto out;
	}

	if (!BN_mod_sub(diff, want, got, p, ctx)) {
		fprintf(stderr, "BN_mod_sub failed\n");
		goto out;
	}

	if (!BN_is_zero(diff)) {
		fprintf(stderr, "want != got\n");
		goto out;
	}

	failed = 0;

 out:
	BN_CTX_free(ctx);
	BN_free(a);
	BN_free(p);
	BN_free(want);
	BN_free(got);
	BN_free(diff);

	return failed;
}

int
main(void)
{
	size_t i;
	int failed = 0;

	for (i = 0; i < N_TESTS; i++)
		failed |= mod_sqrt_test(&mod_sqrt_test_data[i]);

	if (!failed)
		printf("SUCCESS\n");

	return failed;
}
