/* $OpenBSD: asn1basic.c,v 1.4 2022/01/12 07:55:25 tb Exp $ */
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

#include "asn1_locl.h"

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
		fprintf(stderr, "Got:\n");
		hexdump(d1, len1);
		fprintf(stderr, "Want:\n");
		hexdump(d2, len2);
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

const uint8_t asn1_bit_string_primitive[] = {
	0x03, 0x07,
	0x04, 0x0a, 0x3b, 0x5f, 0x29, 0x1c, 0xd0,
};

static int
asn1_bit_string_test(void)
{
	uint8_t bs[] = {0x0a, 0x3b, 0x5f, 0x29, 0x1c, 0xd0};
	ASN1_BIT_STRING *abs;
	uint8_t *p = NULL, *pp;
	const uint8_t *q;
	int bit, i, len;
	int failed = 1;

	if ((abs = ASN1_BIT_STRING_new()) == NULL) {
		fprintf(stderr, "FAIL: ASN1_BIT_STRING_new() == NULL\n");
		goto failed;
	}
	if (!ASN1_BIT_STRING_set(abs, bs, sizeof(bs))) {
		fprintf(stderr, "FAIL: failed to set bit string\n");
		goto failed;
	}

	if ((len = i2d_ASN1_BIT_STRING(abs, NULL)) < 0) {
		fprintf(stderr, "FAIL: i2d_ASN1_BIT_STRING with NULL\n");
		goto failed;
	}
	if ((p = malloc(len)) == NULL)
		errx(1, "malloc");
	memset(p, 0xbd, len);
	pp = p;
	if ((i2d_ASN1_BIT_STRING(abs, &pp)) != len) {
		fprintf(stderr, "FAIL: i2d_ASN1_BIT_STRING\n");
		goto failed;
	}

	if (!asn1_compare_bytes("BIT_STRING", p, len, asn1_bit_string_primitive,
	    sizeof(asn1_bit_string_primitive)))
		goto failed;

	/* Test primitive decoding. */
	q = p;
	if (d2i_ASN1_BIT_STRING(&abs, &q, len) == NULL) {
		fprintf(stderr, "FAIL: d2i_ASN1_BIT_STRING primitive\n");
		goto failed;
	}
	if (!asn1_compare_bytes("BIT_STRING primitive data", abs->data, abs->length,
	    bs, sizeof(bs)))
		goto failed;

	/* Test ASN1_BIT_STRING_get_bit(). */
	for (i = 0; i < ((int)sizeof(bs) * 8); i++) {
		bit = (bs[i / 8] >> (7 - i % 8)) & 1;

		if (ASN1_BIT_STRING_get_bit(abs, i) != bit) {
			fprintf(stderr, "FAIL: ASN1_BIT_STRING_get_bit(_, %d) "
			    "= %d, want %d\n", i,
			    ASN1_BIT_STRING_get_bit(abs, i), bit);
			goto failed;
		}
	}

	/* Test ASN1_BIT_STRING_set_bit(). */
	for (i = 0; i < ((int)sizeof(bs) * 8); i++) {
		if (!ASN1_BIT_STRING_set_bit(abs, i, 1)) {
			fprintf(stderr, "FAIL: ASN1_BIT_STRING_set_bit 1\n");
			goto failed;
		}
	}
	for (i = ((int)sizeof(bs) * 8) - 1; i >= 0; i--) {
		bit = (bs[i / 8] >> (7 - i % 8)) & 1;
		if (bit == 1)
			continue;
		if (!ASN1_BIT_STRING_set_bit(abs, i, 0)) {
			fprintf(stderr, "FAIL: ASN1_BIT_STRING_set_bit\n");
			goto failed;
		}
	}

	if ((i2d_ASN1_BIT_STRING(abs, NULL)) != len) {
		fprintf(stderr, "FAIL: i2d_ASN1_BIT_STRING\n");
		goto failed;
	}

	memset(p, 0xbd, len);
	pp = p;
	if ((i2d_ASN1_BIT_STRING(abs, &pp)) != len) {
		fprintf(stderr, "FAIL: i2d_ASN1_BIT_STRING\n");
		goto failed;
	}

	if (!asn1_compare_bytes("BIT_STRING set", p, len, asn1_bit_string_primitive,
	    sizeof(asn1_bit_string_primitive)))
		goto failed;

	failed = 0;

 failed:
	ASN1_BIT_STRING_free(abs);
	free(p);

	return failed;
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
	if ((p = malloc(len)) == NULL)
		errx(1, "calloc");
	memset(p, 0xbd, len);
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

	failed |= asn1_bit_string_test();
	failed |= asn1_boolean_test();

	return (failed);
}
