/* $OpenBSD: asn1time.c,v 1.14 2022/07/05 04:49:02 anton Exp $ */
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
	const unsigned char der[32];
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
		.str = "20150923032700+1.09",
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
	/* utc time with omitted seconds, should fail */
	{
		.str = "1609082343Z",
	},
};

struct asn1_time_test asn1_invgentime_tests[] = {
	/* Generalized time with omitted seconds, should fail */
	{
		.str = "201612081934Z",
	},
	/* Valid UTC time, should fail as a generalized time */
	{
		.str = "160908234300Z",
	},
};

struct asn1_time_test asn1_goodtime_tests[] = {
	{
		.str = "99990908234339Z",
		.time = 1,
	},
	{
		.str = "201612081934Z",
		.time = 1,
	},
	{
		.str = "1609082343Z",
		.time = 0,
	},
};

struct asn1_time_test asn1_gentime_tests[] = {
	{
		.str = "20161208193400Z",
		.data = "20161208193400Z",
		.time = 1481225640,
		.der = {
			0x18, 0x0f, 0x32, 0x30, 0x31, 0x36, 0x31, 0x32,
			0x30, 0x38, 0x31, 0x39, 0x33, 0x34, 0x30, 0x30,
			0x5a,
		},
	},
	{
		.str = "19700101000000Z",
		.data = "19700101000000Z",
		.time = 0,
		.der = {
			0x18, 0x0f, 0x31, 0x39, 0x37, 0x30, 0x30, 0x31,
			0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
			0x5a,
		},
	},
	{
		.str = "20150923032700Z",
		.data = "20150923032700Z",
		.time = 1442978820,
		.der = {
			0x18, 0x0f, 0x32, 0x30, 0x31, 0x35, 0x30, 0x39,
			0x32, 0x33, 0x30, 0x33, 0x32, 0x37, 0x30, 0x30,
			0x5a,
		},
	},
};

struct asn1_time_test asn1_utctime_tests[] = {
	{
		.str = "700101000000Z",
		.data = "700101000000Z",
		.time = 0,
		.der = {
			0x17, 0x0d, 0x37, 0x30, 0x30, 0x31, 0x30, 0x31,
			0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a,
		},
	},
	{
		.str = "150923032700Z",
		.data = "150923032700Z",
		.time = 1442978820,
		.der = {
			0x17, 0x0d, 0x31, 0x35, 0x30, 0x39, 0x32, 0x33,
			0x30, 0x33, 0x32, 0x37, 0x30, 0x30, 0x5a,
		},
	},
	{
		.str = "140524144512Z",
		.data = "140524144512Z",
		.time = 1400942712,
		.der = {
			0x17, 0x0d, 0x31, 0x34, 0x30, 0x35, 0x32, 0x34,
			0x31, 0x34, 0x34, 0x35, 0x31, 0x32, 0x5a,
		},
	},
	{
		.str = "240401144512Z",
		.data = "240401144512Z",
		.time = 1711982712,
		.der = {
			0x17, 0x0d, 0x32, 0x34, 0x30, 0x34, 0x30, 0x31,
			0x31, 0x34, 0x34, 0x35, 0x31, 0x32, 0x5a
		},
	},
};

#define N_INVTIME_TESTS \
    (sizeof(asn1_invtime_tests) / sizeof(*asn1_invtime_tests))
#define N_INVGENTIME_TESTS \
    (sizeof(asn1_invgentime_tests) / sizeof(*asn1_invgentime_tests))
#define N_GENTIME_TESTS \
    (sizeof(asn1_gentime_tests) / sizeof(*asn1_gentime_tests))
#define N_UTCTIME_TESTS \
    (sizeof(asn1_utctime_tests) / sizeof(*asn1_utctime_tests))

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static int
asn1_compare_bytes(int test_no, const unsigned char *d1,
    const unsigned char *d2, int len1, int len2)
{
	if (len1 != len2) {
		fprintf(stderr, "FAIL: test %i - byte lengths differ "
		    "(%i != %i)\n", test_no, len1, len2);
		return (1);
	}
	if (memcmp(d1, d2, len1) != 0) {
		fprintf(stderr, "FAIL: test %i - bytes differ\n", test_no);
		fprintf(stderr, "Got:\n");
		hexdump(d1, len1);
		fprintf(stderr, "Want:\n");
		hexdump(d2, len2);
		return (1);
	}
	return (0);
}

static int
asn1_compare_str(int test_no, struct asn1_string_st *asn1str, const char *str)
{
	int length = strlen(str);

	if (asn1str->length != length) {
		fprintf(stderr, "FAIL: test %i - string lengths differ "
		    "(%i != %i)\n", test_no, asn1str->length, length);
		return (1);
	}
	if (strncmp(asn1str->data, str, length) != 0) {
		fprintf(stderr, "FAIL: test %i - strings differ "
		    "('%s' != '%s')\n", test_no, asn1str->data, str);
		return (1);
	}

	return (0);
}

static int
asn1_invtime_test(int test_no, struct asn1_time_test *att, int gen)
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

	if (gen)  {
		failure = 0;
		goto done;
	}

	if (ASN1_UTCTIME_set_string(ut, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set UTCTIME "
		    "string '%s'\n", test_no, att->str);
		goto done;
	}
	if (ASN1_TIME_set_string(t, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set TIME "
		    "string '%s'\n", test_no, att->str);
		goto done;
	}
	if (ASN1_TIME_set_string_X509(t, att->str) != 0) {
		fprintf(stderr, "FAIL: test %i - successfully set x509 TIME "
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
	const unsigned char *der;
	unsigned char *p = NULL;
	ASN1_GENERALIZEDTIME *gt = NULL;
	int failure = 1;
	int len;
	struct tm tm;

	if (ASN1_GENERALIZEDTIME_set_string(NULL, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if ((gt = ASN1_GENERALIZEDTIME_new()) == NULL)
		goto done;

	if (ASN1_GENERALIZEDTIME_set_string(gt, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}
	if (asn1_compare_str(test_no, gt, att->str) != 0)
		goto done;

	if (ASN1_TIME_to_tm(gt, &tm) == 0)  {
		fprintf(stderr, "FAIL: test %i - ASN1_time_to_tm failed '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if (timegm(&tm) != att->time) {
		/* things with crappy time_t should die in fire */
		int64_t a = timegm(&tm);
		int64_t b = att->time;
		fprintf(stderr, "FAIL: test %i - times don't match, expected %lld got %lld\n",
		    test_no, b, a);
		goto done;
	}

	if ((len = i2d_ASN1_GENERALIZEDTIME(gt, &p)) <= 0) {
		fprintf(stderr, "FAIL: test %i - i2d_ASN1_GENERALIZEDTIME "
		    "failed\n", test_no);
		goto done;
	}
	der = att->der;
	if (asn1_compare_bytes(test_no, p, der, len, strlen(der)) != 0)
		goto done;

	len = strlen(att->der);
	if (d2i_ASN1_GENERALIZEDTIME(&gt, &der, len) == NULL) {
		fprintf(stderr, "FAIL: test %i - d2i_ASN1_GENERALIZEDTIME "
		    "failed\n", test_no);
		goto done;
	}
	if (asn1_compare_str(test_no, gt, att->str) != 0)
		goto done;

	ASN1_GENERALIZEDTIME_free(gt);

	if ((gt = ASN1_GENERALIZEDTIME_set(NULL, att->time)) == NULL) {
		fprintf(stderr, "FAIL: test %i - failed to set time %lli\n",
		    test_no, (long long)att->time);
		goto done;
	}
	if (asn1_compare_str(test_no, gt, att->data) != 0)
		goto done;

	failure = 0;

 done:
	ASN1_GENERALIZEDTIME_free(gt);
	free(p);

	return (failure);
}

static int
asn1_utctime_test(int test_no, struct asn1_time_test *att)
{
	const unsigned char *der;
	unsigned char *p = NULL;
	ASN1_UTCTIME *ut = NULL;
	int failure = 1;
	int len;

	if (ASN1_UTCTIME_set_string(NULL, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if ((ut = ASN1_UTCTIME_new()) == NULL)
		goto done;

	if (ASN1_UTCTIME_set_string(ut, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}
	if (asn1_compare_str(test_no, ut, att->str) != 0)
		goto done;

	if ((len = i2d_ASN1_UTCTIME(ut, &p)) <= 0) {
		fprintf(stderr, "FAIL: test %i - i2d_ASN1_UTCTIME failed\n",
		    test_no);
		goto done;
	}
	der = att->der;
	if (asn1_compare_bytes(test_no, p, der, len, strlen(der)) != 0)
		goto done;

	len = strlen(att->der);
	if (d2i_ASN1_UTCTIME(&ut, &der, len) == NULL) {
		fprintf(stderr, "FAIL: test %i - d2i_ASN1_UTCTIME failed\n",
		    test_no);
		goto done;
	}
	if (asn1_compare_str(test_no, ut, att->str) != 0)
		goto done;

	ASN1_UTCTIME_free(ut);

	if ((ut = ASN1_UTCTIME_set(NULL, att->time)) == NULL) {
		fprintf(stderr, "FAIL: test %i - failed to set time %lli\n",
		    test_no, (long long)att->time);
		goto done;
	}
	if (asn1_compare_str(test_no, ut, att->data) != 0)
		goto done;

	failure = 0;

 done:
	ASN1_UTCTIME_free(ut);
	free(p);

	return (failure);
}

static int
asn1_time_test(int test_no, struct asn1_time_test *att, int type)
{
	ASN1_TIME *t = NULL, *tx509 = NULL;
	int failure = 1;

	if (ASN1_TIME_set_string(NULL, att->str) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if ((t = ASN1_TIME_new()) == NULL)
		goto done;

	if ((tx509 = ASN1_TIME_new()) == NULL)
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

	if (ASN1_TIME_normalize(t) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set normalize '%s'\n",
		    test_no, att->str);
		goto done;
	}

	if (ASN1_TIME_set_string_X509(tx509, t->data) != 1) {
		fprintf(stderr, "FAIL: test %i - failed to set string X509 '%s'\n",
		    test_no, t->data);
		goto done;
	}

	if (t->type != tx509->type) {
		fprintf(stderr, "FAIL: test %i - type %d, different from %d\n",
		    test_no, t->type, tx509->type);
		goto done;
	}

	if (ASN1_TIME_compare(t, tx509) != 0) {
		fprintf(stderr, "FAIL: ASN1_TIME values differ!\n");
		goto done;
	}


	failure = 0;

 done:

	ASN1_TIME_free(t);
	ASN1_TIME_free(tx509);

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
		failed |= asn1_invtime_test(i, att, 0);
	}

	fprintf(stderr, "Invalid generalized time tests...\n");
	for (i = 0; i < N_INVGENTIME_TESTS; i++) {
		att = &asn1_invgentime_tests[i];
		failed |= asn1_invtime_test(i, att, 1);
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
