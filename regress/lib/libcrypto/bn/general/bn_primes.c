/*	$OpenBSD: bn_primes.c,v 1.1 2022/06/18 19:53:19 tb Exp $ */
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

#include "bn_prime.h"

static int
test_bn_is_prime_fasttest(int do_trial_division)
{
	BIGNUM *n = NULL;
	char *descr = NULL;
	prime_t i, j, max;
	int is_prime, ret;
	int failed = 1;

	if (asprintf(&descr, "with%s trial divisions",
	    do_trial_division ? "" : "out") == -1) {
		descr = NULL;
		fprintf(stderr, "asprintf failed\n");
		goto err;
	}

	if ((n = BN_new()) == NULL) {
		fprintf(stderr, "BN_new failed\n");
		goto err;
	}

	max = primes[NUMPRIMES - 1] + 1;

	failed = 0;
	for (i = 1, j = 0; i < max && j < NUMPRIMES; i++) {
		if (!BN_set_word(n, i)) {
			fprintf(stderr, "BN_set_word(%d) failed", i);
			failed = 1;
			goto err;
		}

		is_prime = i == primes[j];
		if (is_prime)
			j++;

		ret = BN_is_prime_fasttest_ex(n, BN_prime_checks, NULL,
		    do_trial_division, NULL);
		if (ret != is_prime) {
			fprintf(stderr,
			    "BN_is_prime_fasttest_ex(%d) %s: want %d, got %d\n",
			    i, descr, is_prime, ret);
			failed = 1;
		}
	}

	if (i < max || j < NUMPRIMES) {
		fprintf(stderr, "%s: %d < %d or %d < %d\n", descr, i, max, j,
		    NUMPRIMES);
		failed = 1;
	}

 err:
	BN_free(n);
	free(descr);
	return failed;
}

int
main(void)
{
	int failed = 0;

	failed |= test_bn_is_prime_fasttest(0);
	failed |= test_bn_is_prime_fasttest(1);

	printf("%s\n", failed ? "FAILED" : "SUCCESS");

	return failed;
}
