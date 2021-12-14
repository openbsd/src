/* $OpenBSD: asn1api.c,v 1.1 2021/12/14 17:07:57 jsing Exp $ */
/*
 * Copyright (c) 2021 Joel Sing <jsing@openbsd.org>
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
#include <openssl/err.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

const long asn1_tag2bits[] = {
	[0] = 0,
	[1] = 0,
	[2] = 0,
	[3] = B_ASN1_BIT_STRING,
	[4] = B_ASN1_OCTET_STRING,
	[5] = 0,
	[6] = 0,
	[7] = B_ASN1_UNKNOWN,
	[8] = B_ASN1_UNKNOWN,
	[9] = B_ASN1_UNKNOWN,
	[10] = B_ASN1_UNKNOWN,
	[11] = B_ASN1_UNKNOWN,
	[12] = B_ASN1_UTF8STRING,
	[13] = B_ASN1_UNKNOWN,
	[14] = B_ASN1_UNKNOWN,
	[15] = B_ASN1_UNKNOWN,
	[16] = B_ASN1_SEQUENCE,
	[17] = 0,
	[18] = B_ASN1_NUMERICSTRING,
	[19] = B_ASN1_PRINTABLESTRING,
	[20] = B_ASN1_T61STRING,
	[21] = B_ASN1_VIDEOTEXSTRING,
	[22] = B_ASN1_IA5STRING,
	[23] = B_ASN1_UTCTIME,
	[24] = B_ASN1_GENERALIZEDTIME,
	[25] = B_ASN1_GRAPHICSTRING,
	[26] = B_ASN1_ISO64STRING,
	[27] = B_ASN1_GENERALSTRING,
	[28] = B_ASN1_UNIVERSALSTRING,
	[29] = B_ASN1_UNKNOWN,
	[30] = B_ASN1_BMPSTRING,
};

static int
asn1_tag2bit(void)
{
	int failed = 1;
	long bit;
	int i;

	for (i = -3; i <= V_ASN1_NEG + 30; i++) {
		bit = ASN1_tag2bit(i);
		if (i >= 0 && i <= 30) {
			if (bit != asn1_tag2bits[i]) {
				fprintf(stderr, "FAIL: ASN1_tag2bit(%d) = 0x%lx,"
				    " want 0x%lx\n", i, bit, asn1_tag2bits[i]);
				goto failed;
			}
		} else {
			if (bit != 0) {
				fprintf(stderr, "FAIL: ASN1_tag2bit(%d) = 0x%lx,"
				    " want 0x0\n", i, bit);
				goto failed;
			}
		}
	}

	failed = 0;

 failed:
	return failed;
}

static int
asn1_tag2str(void)
{
	int failed = 1;
	const char *s;
	int i;

	for (i = -3; i <= V_ASN1_NEG + 30; i++) {
		if ((s = ASN1_tag2str(i)) == NULL) {
			fprintf(stderr, "FAIL: ASN1_tag2str(%d) returned "
			    "NULL\n", i);
			goto failed;
		}
		if ((i >= 0 && i <= 30) || i == V_ASN1_NEG_INTEGER ||
		    i == V_ASN1_NEG_ENUMERATED) {
			if (strcmp(s, "(unknown)") == 0) {
				fprintf(stderr, "FAIL: ASN1_tag2str(%d) = '%s',"
				    " want tag name\n", i, s);
				goto failed;
			}
		} else {
			if (strcmp(s, "(unknown)") != 0) {
				fprintf(stderr, "FAIL: ASN1_tag2str(%d) = '%s',"
				    " want '(unknown')\n", i, s);
				goto failed;
			}
		}
	}

	failed = 0;

 failed:
	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= asn1_tag2bit();
	failed |= asn1_tag2str();

	return (failed);
}
