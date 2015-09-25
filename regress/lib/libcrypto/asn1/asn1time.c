/* $OpenBSD: asn1time.c,v 1.1 2015/09/25 16:12:30 jsing Exp $ */
/*
 * Copyright (c) 2015 Joel Sing <jsing@openbsd.org>
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

#include <openssl/asn1.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

struct asn1_time_test {
	const char *str;
	const char *data;
	time_t time;
};

struct asn1_time_test asn1_invtime_tests[] = {
	{
		.str = "",
	},
	{
		.str = "2015",
	},
	{
		.str = "201509",
	},
	{
		.str = "20150923",
	},
	{
		.str = "20150923032700",
	},
	{
		.str = "20150923032700.Z",
	},
	{
		.str = "20150923032700.123",
	},
	{
		.str = "20150923032700+1100Z",
	},
	{
		.str = "20150923032700-11001",
	},
	{
		/* UTC time cannot have fractional seconds. */
		.str = "150923032700.123Z",
	},
	{
		.str = "aaaaaaaaaaaaaaZ",
	},
};

struct asn1_time_test asn1_gentime_tests[] = {
	{
		.str = "19700101000000Z",
		.data = "19700101000000Z",
		.time = 0,
	},
	{
		.str = "20150923032700Z",
		.data = "20150923032700Z",
		.time = 1442978820,
	},
	{
		.str = "20150923032700.22-0700",
		.data = "20150923102700Z",
		.time = 1443004020,
	},
	{
		.str = "20150923032712+1100",
		.data = "20150922162712Z",
		.time = 1442939232,
	},
	{
		.str = "20150923032712+1115",
		.data = "20150922161212Z",
		.time = 1442938332,
	},
	{
		.str = "20150923032700.12345678Z",
		.data = "20150923032700Z",
		.time = 1442978820,
	},
};

struct asn1_time_test asn1_utctime_tests[] = {
	{
		.str = "7001010000Z",
		.data = "700101000000Z",
		.time = 0,
	},
	{
		.str = "150923032700Z",
		.data = "150923032700Z",
		.time = 1442978820,
	},
	{
		.str = "150923032700-0700",
		.data = "150923102700Z",
		.time = 1443004020,
	},
	{
		.str = "150923032712+1100",
		.data = "150922162712Z",
		.time = 1442939232,
	},
};

#define N_INVTIME_TESTS \
    (sizeof(asn1_invtime_tests) / sizeof(*asn1_invtime_tests))
#define N_GENTIME_TESTS \
    (sizeof(asn1_gentime_tests) / sizeof(*asn1_gentime_tests))
#define N_UTCTIME_TESTS \
    (sizeof(asn1_utctime_tests) / sizeof(*asn1_utctime_tests))

static int
asn1_invtime_test(int test_no, struct asn1_time_test *att)
{
	ASN1_GENERALIZEDTIME *gt = NULL;
	ASN1_UTCTIME *ut = NULL;
	ASN1_TIME *t = NULL;
	int failure = 1;

	if ((gt = ASN1_GENERALIZEDTIME_new()) == NULL)
		goto done;
	if ((ut = ASN1_UTCTIME_new()) == NULL)
		goto done;
	if ((t = ASN1_TIME_new()) == NULL)
		goto done;

	if (ASN1_GENERALIZEDTIME_set_string(gt, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set "
		    "GENERALIZEDTIME string '%s'\n", test_no, att->str);
		goto done;
	}
	if (ASN1_UTCTIME_set_string(ut, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set UTCTIME "
		    "string '%s'\n", test_no, att->str);
		goto done;
	}
	if (ASN1_UTCTIME_set_string(ut, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set TIME "
		    "string '%s'\n", test_no, att->str);
		goto done;
	}

	failure = 0;

 done:
	ASN1_GENERALIZEDTIME_free(gt);
	ASN1_UTCTIME_free(ut);
	ASN1_TIME_free(t);

	return (failure);
}

static int
asn1_gentime_test(int test_no, struct asn1_time_test *att)
{
	ASN1_GENERALIZEDTIME *gt;
	int failure = 1;
	int length;

	if ((gt = ASN1_GENERALIZEDTIME_new()) == NULL)
		goto done;

	if (ASN1_GENERALIZEDTIME_set_string(gt, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	/* ASN.1 preserves the original input. */
	length = strlen(att->str);
	if (gt->length != length) {
		fprintf(stderr, "FAIL: test %i - length differs (%i != %i)\n",
		    test_no, gt->length, length);
		goto done;
	}
	if (strncmp(gt->data, att->str, length) != 0) {
		fprintf(stderr, "FAIL: test %i - data differs ('%s' != '%s')\n",
		    test_no, gt->data, att->str);
		goto done;
	}

	ASN1_GENERALIZEDTIME_free(gt);

	if ((gt = ASN1_GENERALIZEDTIME_set(NULL, att->time)) == NULL) {
		fprintf(stderr, "FAIL: test %i - failed to set time %lli\n",
		    test_no, (long long)att->time);
		goto done;
	}
	length = strlen(att->data);
	if (gt->length != length) {
		fprintf(stderr, "FAIL: test %i - length differs (%i != %i)\n",
		    test_no, gt->length, length);
		goto done;
	}
	if (strncmp(gt->data, att->data, length) != 0) {
		fprintf(stderr, "FAIL: test %i - data differs ('%s' != '%s')\n",
		    test_no, gt->data, att->data);
		goto done;
	}

	failure = 0;

 done:
	ASN1_GENERALIZEDTIME_free(gt);

	return (failure);
}

static int
asn1_utctime_test(int test_no, struct asn1_time_test *att)
{
	ASN1_UTCTIME *ut;
	int failure = 1;
	int length;

	if ((ut = ASN1_UTCTIME_new()) == NULL)
		goto done;

	if (ASN1_UTCTIME_set_string(ut, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	/* ASN.1 preserves the original input. */
	length = strlen(att->str);
	if (ut->length != length) {
		fprintf(stderr, "FAIL: test %i - length differs (%i != %i)\n",
		    test_no, ut->length, length);
		goto done;
	}
	if (strncmp(ut->data, att->str, length) != 0) {
		fprintf(stderr, "FAIL: test %i - data differs ('%s' != '%s')\n",
		    test_no, ut->data, att->str);
		goto done;
	}

	ASN1_UTCTIME_free(ut);

	if ((ut = ASN1_UTCTIME_set(NULL, att->time)) == NULL) {
		fprintf(stderr, "FAIL: test %i - failed to set time %lli\n",
		    test_no, (long long)att->time);
		goto done;
	}
	length = strlen(att->data);
	if (ut->length != length) {
		fprintf(stderr, "FAIL: test %i - length differs (%i != %i)\n",
		    test_no, ut->length, length);
		goto done;
	}
	if (strncmp(ut->data, att->data, length) != 0) {
		fprintf(stderr, "FAIL: test %i - data differs ('%s' != '%s')\n",
		    test_no, ut->data, att->data);
		goto done;
	}

	failure = 0;

 done:
	ASN1_UTCTIME_free(ut);

	return (failure);
}

static int
asn1_time_test(int test_no, struct asn1_time_test *att, int type)
{
	ASN1_TIME *t = NULL;
	int failure = 1;

	if ((t = ASN1_TIME_new()) == NULL)
		goto done;

	if (ASN1_TIME_set_string(t, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if (t->type != type) {
		fprintf(stderr, "FAIL: test %i - got type %i, want %i\n",
		    test_no, t->type, type);
		goto done;
	}

	failure = 0;

 done:

	ASN1_TIME_free(t);

	return (failure);
}

int
main(int argc, char **argv)
{
	struct asn1_time_test *att;
	int failed = 0;
	size_t i;

	fprintf(stderr, "Invalid time tests...\n");
	for (i = 0; i < N_INVTIME_TESTS; i++) {
		att = &asn1_invtime_tests[i];
		failed |= asn1_invtime_test(i, att);
	}

	fprintf(stderr, "GENERALIZEDTIME tests...\n");
	for (i = 0; i < N_GENTIME_TESTS; i++) {
		att = &asn1_gentime_tests[i];
		failed |= asn1_gentime_test(i, att);
	}

	fprintf(stderr, "UTCTIME tests...\n");
	for (i = 0; i < N_UTCTIME_TESTS; i++) {
		att = &asn1_utctime_tests[i];
		failed |= asn1_utctime_test(i, att);
	}

	fprintf(stderr, "TIME tests...\n");
	for (i = 0; i < N_UTCTIME_TESTS; i++) {
		att = &asn1_utctime_tests[i];
		failed |= asn1_time_test(i, att, V_ASN1_UTCTIME);
	}
	for (i = 0; i < N_GENTIME_TESTS; i++) {
		att = &asn1_gentime_tests[i];
		failed |= asn1_time_test(i, att, V_ASN1_GENERALIZEDTIME);
	}

	return (failed);
}
