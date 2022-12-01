/*	$OpenBSD: wycheproof-primes.c,v 1.2 2022/12/01 13:49:12 tb Exp $ */
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

#include <err.h>
#include <stdio.h>

#include <openssl/bn.h>

#include "primality_testcases.h"

int
primality_test(struct wycheproof_testcase *test)
{
	BIGNUM *value = NULL;
	int ret;
	int failed = 1;

	if (!BN_hex2bn(&value, test->value))
		errx(1, "%d: failed to set value \"%s\"", test->id, test->value);

	if ((ret = BN_is_prime_ex(value, BN_prime_checks, NULL, NULL)) < 0)
		errx(1, "%d: BN_is_prime_ex errored", test->id);

	if (ret != test->result && !test->acceptable) {
		fprintf(stderr, "%d failed, want %d, got %d\n", test->id,
		    test->result, ret);
		goto err;
	}

	failed = 0;
 err:
	BN_free(value);

	return failed;
}

int
main(void)
{
	size_t i;
	int failed = 0;

	for (i = 0; i < N_TESTS; i++)
		failed |= primality_test(&testcases[i]);

	return failed;
}
