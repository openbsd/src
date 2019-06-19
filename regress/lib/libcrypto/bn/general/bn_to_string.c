/*	$OpenBSD: bn_to_string.c,v 1.1 2019/04/13 22:06:31 tb Exp $ */
/*
 * Copyright (c) 2019 Theo Buehler <tb@openbsd.org>
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
#include <string.h>

#include <openssl/bn.h>

char *bn_to_string(const BIGNUM *bn);

struct convert_st {
	const char	*input;
	const char	*expected;
};

struct convert_st testcases[] = {
	{"0", "0"},
	{"-0", "-0"},
	{"7", "7"},
	{"-7", "-7"},
	{"8", "8"},
	{"-8", "-8"},
	{"F", "15"},
	{"-F", "-15"},
	{"10", "16"},
	{"-10", "-16"},
	{"7F", "127"},
	{"-7F", "-127"},
	{"80", "128"},
	{"-80", "-128"},
	{"FF", "255"},
	{"-FF", "-255"},
	{"100", "256"},
	{"7FFF", "32767"},
	{"-7FFF", "-32767"},
	{"8000", "32768"},
	{"-8000", "-32768"},
	{"FFFF", "65535"},
	{"-FFFF", "-65535"},
	{"10000", "65536"},
	{"-10000", "-65536"},
	{"7FFFFFFF", "2147483647"},
	{"-7FFFFFFF", "-2147483647"},
	{"80000000", "2147483648"},
	{"-80000000", "-2147483648"},
	{"FFFFFFFF", "4294967295"},
	{"-FFFFFFFF", "-4294967295"},
	{"100000000", "4294967296"},
	{"-100000000", "-4294967296"},
	{"7FFFFFFFFFFFFFFF", "9223372036854775807"},
	{"-7FFFFFFFFFFFFFFF", "-9223372036854775807"},
	{"8000000000000000", "9223372036854775808"},
	{"-8000000000000000", "-9223372036854775808"},
	{"FFFFFFFFFFFFFFFF", "18446744073709551615"},
	{"-FFFFFFFFFFFFFFFF", "-18446744073709551615"},
	{"10000000000000000", "18446744073709551616"},
	{"-10000000000000000", "-18446744073709551616"},
	{"7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	    "170141183460469231731687303715884105727"},
	{"-7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	    "-170141183460469231731687303715884105727"},
	{"80000000000000000000000000000000",
	    "0x80000000000000000000000000000000"},
	{"-80000000000000000000000000000000",
	    "-0x80000000000000000000000000000000"},
	{"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	    "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"},
	{"-FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
	    "-0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"},
	{"100000000000000000000000000000000",
	    "0x0100000000000000000000000000000000"},
	{"-100000000000000000000000000000000",
	    "-0x0100000000000000000000000000000000"},
	{ NULL, NULL },
};

int
main(int argc, char *argv[])
{
	struct convert_st	*test;
	BIGNUM			*bn = NULL;
	char			*bnstr;
	int			 failed = 0;

	for (test = testcases; test->input != NULL; test++) {
		if (!BN_hex2bn(&bn, test->input))
			errx(1, "BN_hex2bn(%s)", test->input);
		if ((bnstr = bn_to_string(bn)) == NULL)
			errx(1, "bn_to_string(%s)", test->input);
		if (strcmp(bnstr, test->expected) != 0) {
			warnx("%s != %s", bnstr, test->expected);
			failed = 1;
		}
		free(bnstr);
	}

	BN_free(bn);

	printf("%s\n", failed ? "FAILED" : "SUCCESS");
	return failed;
}
