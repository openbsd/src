/*	$OpenBSD: bn_rand_interval.c,v 1.4 2021/04/06 16:40:34 tb Exp $	*/
/*
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
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

#define NUM_TESTS 1000000

int bn_rand_interval(BIGNUM *rnd, const BIGNUM *lower_incl,
    const BIGNUM *upper_excl);
void print_triple(BIGNUM *a, BIGNUM *b, BIGNUM *x);

void
print_triple(BIGNUM *a, BIGNUM *b, BIGNUM *x) {
	if (a != NULL) {
		printf("a = ");
		BN_print_fp(stdout, a);
		printf("\n");
	}

	if (b != NULL) {
		printf("b = ");
		BN_print_fp(stdout, b);
		printf("\n");
	}

	if (x != NULL) {
		printf("x = ");
		BN_print_fp(stdout, x);
		printf("\n");
	}
}

int
main(int argc, char *argv[])
{
	BIGNUM *a, *b, *x;
	int i, success = 1;

	if ((a = BN_new()) == NULL)
		errx(1, "BN_new(a)");
	if ((b = BN_new()) == NULL)
		errx(1, "BN_new(b)");
	if ((x = BN_new()) == NULL)
		errx(1, "BN_new(c)");

	for (i = 0; i < NUM_TESTS; i++) {
		if (!BN_rand(a, 256, 0, 0))
			errx(1, "BN_rand(a)");

		if (bn_rand_interval(x, a, a) != 0) {
			success = 0;

			printf("bn_rand_interval(a == a) succeeded\n");
			print_triple(a, NULL, x);
		}

		if (!BN_rand(b, 256, 0, 0))
			errx(1, "BN_rand(b)");

		switch(BN_cmp(a, b)) {
		case 0:		/* a == b */
			continue;

		case 1:		/* a > b */
			BN_swap(a, b);
			break;

		default:	/* a < b */
			break;
		}

		if (!bn_rand_interval(x, a, b))
			errx(1, "bn_rand_interval() failed");

		if (BN_cmp(x, a) < 0 || BN_cmp(x, b) >= 0) {
			success = 0;

			printf("generated number x not inside [a,b)\n");
			print_triple(a, b, x);
		}

		if (bn_rand_interval(x, b, a) != 0) {
			success = 0;

			printf("bn_rand_interval(x, b, a) succeeded\n");
			print_triple(a, b, x);
		}
	}

	if (success == 1)
		printf("success\n");
	else
		printf("FAIL");

	BN_free(a);
	BN_free(b);
	BN_free(x);

	return 1 - success;
}
