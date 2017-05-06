/* $OpenBSD: freenull.c,v 1.4 2017/05/06 21:23:57 jsing Exp $ */
/*
 * Copyright (c) 2017 Bob Beck <beck@openbsd.org>
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
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

/* Make sure we do the right thing. Add here if you convert ones in tree */
int
main(int argc, char **argv)
{
	ASN1_INTEGER_free(NULL);
	ASN1_OBJECT_free(NULL);
	ASN1_OCTET_STRING_free(NULL);

	BIO_free_all(NULL);

	DIST_POINT_free(NULL);

	EVP_PKEY_free(NULL);

	GENERAL_NAME_free(NULL);
	GENERAL_SUBTREE_free(NULL);

	NAME_CONSTRAINTS_free(NULL);

	sk_GENERAL_NAME_pop_free(NULL, GENERAL_NAME_free);
	sk_X509_NAME_ENTRY_pop_free(NULL, X509_NAME_ENTRY_free);

	X509_NAME_ENTRY_free(NULL);

	printf("PASS\n");

	return (0);
}
