/*	$OpenBSD: bnaddsub.c,v 1.3 2018/07/23 17:15:21 tb Exp $	*/
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

/* Test basic functionality of BN_add(), BN_sub(), BN_uadd() and BN_usub() */

#include <err.h>
#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))

BIO *bio_err;

struct hexinput_st {
	const char	*a_hex;
	const char	*b_hex;
	const char	*e_hex;		/* expected result */
	const char	 ret;		/* check return value */
	int		 compare;	/* use BN_cmp() to verify results */
};

int bn_op_test(int (*)(BIGNUM *, const BIGNUM *, const BIGNUM *),
    struct hexinput_st[], unsigned int, const char *);
void print_failure_case(BIGNUM *, BIGNUM *, BIGNUM *, BIGNUM *, int,
    const char *);

struct hexinput_st test_bn_add[] = {
	{
		"F",
		"F",
		"1E",
		1,
		1,
	},
	{
		"FFFFFFFFFFFFFFFFFFF",
		"1",
		"10000000000000000000",
		1,
		1,
	},
	{
		"7878787878787878",
		"1010101010101010",
		"8888888888888888",
		1,
		1,
	},
	{
		"FFFFFFFFFFFFFFFF0000000000000000",
		"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
		"1FFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF",
		1,
		1,
	},
	{
		"F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0",
		"10101010101010101010101010101010",
		"101010101010101010101010101010100",
		1,
		1,
	},
};

struct hexinput_st test_bn_sub[] = {
	{
		"10",
		"1",
		"F",
		1,
		1,
	},
	{
		"10",
		"1",
		"E",
		1,
		0,
	},
	{
		"100000000001000000000",
		"11100000001",
		"FFFFFFFFFEFEFFFFFFFF",
		1,
		1,
	},
	{
		"-FFFFFFFFFFFFFFFFFFFF",
		"1",
		"-100000000000000000000",
		1,
		1,
	},
};

struct hexinput_st test_bn_usub[] = {
	{
		"10",
		"1",
		"F",
		1,
		1,
	},
	{
		"10",
		"1",
		"E",
		1,
		0,
	},
	{
		"100000000001000000000",
		"11100000001",
		"FFFFFFFFFEFEFFFFFFFF",
		1,
		1,
	},
	{
		"100000000000000000000",
		"1",
		"FFFFFFFFFFFFFFFFFFFF",
		1,
		1,
	},
	{
		"0",
		"1",
		"0",
		0,
		0,
	},
};

void
print_failure_case(BIGNUM *a, BIGNUM *b, BIGNUM *e, BIGNUM *r, int i,
    const char *testname)
{
	BIO_printf(bio_err, "%s #%d failed:", testname, i);
	BIO_printf(bio_err, "\na = ");
	BN_print(bio_err, a);
	BIO_printf(bio_err, "\nb = ");
	BN_print(bio_err, b);
	BIO_printf(bio_err, "\nexpected: e = ");
	BN_print(bio_err, e);
	BIO_printf(bio_err, "\nobtained: r = ");
	BN_print(bio_err, r);
	BIO_printf(bio_err, "\n");
}

int
bn_op_test(int (*bn_op)(BIGNUM *, const BIGNUM *, const BIGNUM *),
    struct hexinput_st tests[], unsigned int ntests, const char *testname)
{
	BIGNUM		*a = NULL, *b = NULL, *e = NULL, *r = NULL;
	unsigned int	 i;
	int		 failed = 0;

	if (((a = BN_new()) == NULL) ||
	    ((b = BN_new()) == NULL) ||
	    ((e = BN_new()) == NULL) ||
	    ((r = BN_new()) == NULL)) {
		failed = 1;
		ERR_print_errors(bio_err);
		goto err;
	}

	for (i = 0; i < ntests; i++) {
		int print = 0;

		if (!BN_hex2bn(&a, tests[i].a_hex) ||
		    !BN_hex2bn(&b, tests[i].b_hex) ||
		    !BN_hex2bn(&e, tests[i].e_hex)) {
			print = 1;
			ERR_print_errors(bio_err);
		}

		if (tests[i].ret != bn_op(r, a, b))
			print = 1;
		if (tests[i].compare == 1 && BN_cmp(e, r) != 0)
			print = 1;
		if (print) {
			failed = 1;
			print_failure_case(a, b, e, r, i, testname);
		}
	}

 err:
	BN_free(a);
	BN_free(b);
	BN_free(e);
	BN_free(r);
	return failed;
}

int
main(int argc, char *argv[])
{
	int failed = 0;

	if ((bio_err = BIO_new_fp(stderr, BIO_NOCLOSE)) == NULL) {
		fprintf(stderr, "bnaddsub: failed to initialize bio_err");
		return 1;
	}

	if (bn_op_test(BN_add, test_bn_add, nitems(test_bn_add),
	    "BN_add with test_bn_add[]"))
		failed = 1;
	if (bn_op_test(BN_uadd, test_bn_add, nitems(test_bn_add),
	    "BN_uadd with test_bn_add[]"))
		failed = 1;
	if (bn_op_test(BN_sub, test_bn_sub, nitems(test_bn_sub),
	    "BN_sub with test_bn_sub[]"))
		failed = 1;
	if (bn_op_test(BN_usub, test_bn_usub, nitems(test_bn_usub),
	    "BN_usub with test_bn_usub[]"))
		failed = 1;

	return failed;
}
