/*	$OpenBSD: name_constraints_test.c,v 1.1 2026/09/18 14:37:52 beck Exp $ */
/*
 * Copyright (c) 2026 Bob Beck <beck@openbsd.org>
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

#include <time.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "test.h"

static const char dns_root_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBYzCCAQmgAwIBAgIJAJ/D/fOJFvPDMAoGCCqGSM49BAMCMBQxEjAQBgNVBAMM\n"
    "CVRlc3QgUm9vdDAgFw0yNjA5MTgwMjE5MzlaGA8yMTI2MDgyNTAyMTkzOVowFDES\n"
    "MBAGA1UEAwwJVGVzdCBSb290MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIpna\n"
    "ShBnE5r+xTd9+LMifP2LMngmSfZibz69ektpb//LDZnjt6K6XfBvgRulGq4lhSR3\n"
    "JKZwS0+GjmNDVBPO/qNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AQYwHQYDVR0OBBYEFOSJwXqcbWx4UKfgUSOj490UP/ErMAoGCCqGSM49BAMCA0gA\n"
    "MEUCICKXG3Itms1VrPKCqnm6Cu7nUwVv3/bkmUd4R5EjN7fZAiEA/nfTba6tjdEO\n"
    "lGZ7gWEajGFDCGaG/+BLJw1HrhNIYjo=\n"
    "-----END CERTIFICATE-----\n";

static const char dns_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsDCCAVegAwIBAgIBAjAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDIxOTM5WhgPMjEyNjA4MjUwMjE5MzlaMCgxJjAkBgNVBAMM\n"
    "HVRlc3QgQ29uc3RyYWluZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0CAQYIKoZI\n"
    "zj0DAQcDQgAEl0GDBN804RfCuoHzsVndoq+cma6LqzMNaipDirGn6qplmqKibBwF\n"
    "JlPnfWJBpVBSwjvG6mHTxXnd1zHSavhWgKOBgzCBgDAPBgNVHRMBAf8EBTADAQH/\n"
    "MA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUy2/IuHbpHlhlZ3K44AOWzzoPut8w\n"
    "HwYDVR0jBBgwFoAU5InBepxtbHhQp+BRI6Pj3RQ/8SswHQYDVR0eAQH/BBMwEaAP\n"
    "MA2CC2V4YW1wbGUuY29tMAoGCCqGSM49BAMCA0cAMEQCICmgXrzoVNTbNRAC20gD\n"
    "BYVM1jnEo06m6xmHoP9HnuXPAiA2KLp3OlyuEPHwBxdjs8DHKfHzKiTtfIajBVlD\n"
    "89+/Ug==\n"
    "-----END CERTIFICATE-----\n";

static const char dns_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBxzCCAW2gAwIBAgIBAzAKBggqhkjOPQQDAjAoMSYwJAYDVQQDDB1UZXN0IENv\n"
    "bnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjE5MzlaGA8yMTI2MDgy\n"
    "NTAyMTkzOVowGzEZMBcGA1UEAwwQbGVhZi5leGFtcGxlLm9yZzBZMBMGByqGSM49\n"
    "AgEGCCqGSM49AwEHA0IABHgzZNJNzRHCViBpUc5fsvnt04orFSMuvAKUxNJvcS9/\n"
    "HYnXkytaa4rwoPdaNof2uW5nK8m0wW4y33kPJiu8pKCjgZIwgY8wDAYDVR0TAQH/\n"
    "BAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwEwHQYDVR0O\n"
    "BBYEFOKpxvj7I7kNvJmygh0RAyy1pgnBMB8GA1UdIwQYMBaAFMtvyLh26R5YZWdy\n"
    "uOADls86D7rfMBoGA1UdEQQTMBGCD3d3dy5leGFtcGxlLmNvbTAKBggqhkjOPQQD\n"
    "AgNIADBFAiB3lUinX2NLUvaeZdQ+dh4gyocE2KexDM+jpfVXeFBGKwIhAPWNJFZc\n"
    "oMxDJezkeiSkBxtNIXO4WUS/fkH3+1JdY6mr\n"
    "-----END CERTIFICATE-----\n";

static const time_t check_time = 1798761600;

static X509 *
cert_from_pem(struct test *t, const char *pem)
{
	BIO *bio;
	X509 *x = NULL;

	if ((bio = BIO_new_mem_buf(pem, -1)) == NULL) {
		test_errorf(t, "BIO_new_mem_buf");
		return NULL;
	}
	if ((x = PEM_read_bio_X509(bio, NULL, NULL, NULL)) == NULL)
		test_errorf(t, "PEM_read_bio_X509");
	BIO_free(bio);

	return x;
}

/*
 * Verify a chain of freshly parsed certificates, so that no extension state
 * is cached on any of them when verification starts. The intermediate is
 * name constrained to DNS names under example.com. The leaf's subject CN,
 * leaf.example.org, lies outside that subtree while its SAN, www.example.com,
 * lies inside it, so the chain is only accepted if the leaf's cached SAN is
 * in place when its names are checked.
 */
static void
test_fresh_leaf_name_constraints(struct test *t, const void *arg)
{
	X509 *root = NULL, *intermediate = NULL, *leaf = NULL;
	X509_STORE *store = NULL;
	X509_STORE_CTX *ctx = NULL;
	STACK_OF(X509) *untrusted = NULL;

	if ((root = cert_from_pem(t, dns_root_pem)) == NULL)
		goto err;
	if ((intermediate = cert_from_pem(t, dns_inter_pem)) == NULL)
		goto err;
	if ((leaf = cert_from_pem(t, dns_leaf_pem)) == NULL)
		goto err;

	if ((store = X509_STORE_new()) == NULL) {
		test_errorf(t, "X509_STORE_new");
		goto err;
	}
	if (!X509_STORE_add_cert(store, root)) {
		test_errorf(t, "X509_STORE_add_cert");
		goto err;
	}
	if ((untrusted = sk_X509_new_null()) == NULL) {
		test_errorf(t, "sk_X509_new_null");
		goto err;
	}
	if (!sk_X509_push(untrusted, intermediate)) {
		test_errorf(t, "sk_X509_push");
		goto err;
	}
	if ((ctx = X509_STORE_CTX_new()) == NULL) {
		test_errorf(t, "X509_STORE_CTX_new");
		goto err;
	}
	if (!X509_STORE_CTX_init(ctx, store, leaf, untrusted)) {
		test_errorf(t, "X509_STORE_CTX_init");
		goto err;
	}
	X509_STORE_CTX_set_time(ctx, 0, check_time);

	if (X509_verify_cert(ctx) != 1) {
		test_errorf(t, "X509_verify_cert: %s",
		    X509_verify_cert_error_string(
		    X509_STORE_CTX_get_error(ctx)));
		goto err;
	}

 err:
	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);
	sk_X509_free(untrusted);
	X509_free(root);
	X509_free(intermediate);
	X509_free(leaf);
}

int
main(int argc, char **argv)
{
	struct test *t = test_init();

	test_run(t, "fresh leaf name constraints",
	    test_fresh_leaf_name_constraints, NULL);

	return test_result(t);
}
