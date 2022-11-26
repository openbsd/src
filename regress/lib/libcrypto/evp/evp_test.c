/*	$OpenBSD: evp_test.c,v 1.3 2022/11/26 16:08:56 tb Exp $ */
/*
 * Copyright (c) 2022 Joel Sing <jsing@openbsd.org>
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

#include <openssl/evp.h>
#include <openssl/ossl_typ.h>

#include "evp_local.h"

static int
evp_asn1_method_test(void)
{
	const EVP_PKEY_ASN1_METHOD *method;
	int count, pkey_id, i;
	int failed = 1;

	if ((count = EVP_PKEY_asn1_get_count()) < 1) {
		fprintf(stderr, "FAIL: failed to get pkey asn1 method count\n");
		goto failure;
	}
	for (i = 0; i < count; i++) {
		if ((method = EVP_PKEY_asn1_get0(i)) == NULL) {
			fprintf(stderr, "FAIL: failed to get pkey %d\n", i);
			goto failure;
		}
	}

	if ((method = EVP_PKEY_asn1_find(NULL, EVP_PKEY_RSA)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find(NULL, EVP_PKEY_RSA_PSS)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA-PSS method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find_str(NULL, "RSA", -1)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method by str\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_asn1_find_str(NULL, "RSA-PSS", -1)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	if (!EVP_PKEY_asn1_get0_info(&pkey_id, NULL, NULL, NULL, NULL, method)) {
		fprintf(stderr, "FAIL: failed to get RSA-PSS method info\n");
		goto failure;
	}
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	failed = 0;

 failure:

	return failed;
}

static int
evp_pkey_method_test(void)
{
	const EVP_PKEY_METHOD *method;
	int pkey_id;
	int failed = 1;

	if ((method = EVP_PKEY_meth_find(EVP_PKEY_RSA)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA method\n");
		goto failure;
	}
	EVP_PKEY_meth_get0_info(&pkey_id, NULL, method);
	if (pkey_id != EVP_PKEY_RSA) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA);
		goto failure;
	}

	if ((method = EVP_PKEY_meth_find(EVP_PKEY_RSA_PSS)) == NULL) {
		fprintf(stderr, "FAIL: failed to find RSA-PSS method\n");
		goto failure;
	}
	EVP_PKEY_meth_get0_info(&pkey_id, NULL, method);
	if (pkey_id != EVP_PKEY_RSA_PSS) {
		fprintf(stderr, "FAIL: method ID mismatch (%d != %d)\n",
		    pkey_id, EVP_PKEY_RSA_PSS);
		goto failure;
	}

	failed = 0;

 failure:

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= evp_asn1_method_test();
	failed |= evp_pkey_method_test();

	return failed;
}
