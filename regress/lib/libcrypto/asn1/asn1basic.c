/* $OpenBSD: asn1basic.c,v 1.2 2021/12/23 18:12:58 tb Exp $ */
/*
 * Copyright (c) 2017, 2021 Joel Sing <jsing@openbsd.org>
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

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static int
asn1_compare_bytes(const char *label, const unsigned char *d1, int len1,
    const unsigned char *d2, int len2)
{
	if (len1 != len2) {
		fprintf(stderr, "FAIL: %s - byte lengths differ "
		    "(%i != %i)\n", label, len1, len2);
		return 0;
	}
	if (memcmp(d1, d2, len1) != 0) {
		fprintf(stderr, "FAIL: %s - bytes differ\n", label);
		fprintf(stderr, "Got:\n");
		hexdump(d1, len1);
		fprintf(stderr, "Want:\n");
		hexdump(d2, len2);
		return 0;
	}
	return 1;
}

const uint8_t asn1_boolean_false[] = {
	0x01, 0x01, 0x00,
};
const uint8_t asn1_boolean_true[] = {
	0x01, 0x01, 0x01,
};

static int
asn1_boolean_test(void)
{
	uint8_t *p = NULL, *pp;
	const uint8_t *q;
	int len;
	int failed = 1;

	if ((len = i2d_ASN1_BOOLEAN(0, NULL)) < 0) {
		fprintf(stderr, "FAIL: i2d_ASN1_BOOLEAN false with NULL\n");
		goto failed;
	}
	if ((p = calloc(1, len)) == NULL)
		errx(1, "calloc");
	pp = p;
	if ((i2d_ASN1_BOOLEAN(0, &pp)) != len) {
		fprintf(stderr, "FAIL: i2d_ASN1_BOOLEAN false\n");
		goto failed;
	}

	if (!asn1_compare_bytes("BOOLEAN false", p, len, asn1_boolean_false,
	    sizeof(asn1_boolean_false)))
		goto failed;

	q = p;
	if (d2i_ASN1_BOOLEAN(NULL, &q, len) != 0) {
		fprintf(stderr, "FAIL: BOOLEAN false did not decode to 0\n");
		goto failed;
	}

	free(p);
	p = NULL;

	if ((len = i2d_ASN1_BOOLEAN(1, NULL)) < 0) {
		fprintf(stderr, "FAIL: i2d_ASN1_BOOLEAN true with NULL\n");
		goto failed;
	}
	if ((p = calloc(1, len)) == NULL)
		errx(1, "calloc");
	pp = p;
	if ((i2d_ASN1_BOOLEAN(1, &pp)) != len) {
		fprintf(stderr, "FAIL: i2d_ASN1_BOOLEAN true\n");
		goto failed;
	}

	if (!asn1_compare_bytes("BOOLEAN true", p, len, asn1_boolean_true,
	    sizeof(asn1_boolean_true)))
		goto failed;

	q = p;
	if (d2i_ASN1_BOOLEAN(NULL, &q, len) != 1) {
		fprintf(stderr, "FAIL: BOOLEAN true did not decode to 1\n");
		goto failed;
	}

	failed = 0;

 failed:
	free(p);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= asn1_boolean_test();

	return (failed);
}
