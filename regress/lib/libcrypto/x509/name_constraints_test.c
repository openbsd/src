/*	$OpenBSD: name_constraints_test.c,v 1.2 2026/09/18 18:21:50 beck Exp $ */
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

static const char email_root_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBZDCCAQmgAwIBAgIJAJ6zZnV/dv80MAoGCCqGSM49BAMCMBQxEjAQBgNVBAMM\n"
    "CVRlc3QgUm9vdDAgFw0yNjA5MTgwMjM5MjNaGA8yMTI2MDgyNTAyMzkyM1owFDES\n"
    "MBAGA1UEAwwJVGVzdCBSb290MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEB1u0\n"
    "etxVYvFrG2A0b60YmYIhFm3bBRRiCBHJ9MX7MoGIp1rG+gWHzauDzGT9GtSX2hJ2\n"
    "uARhxSuKxbWjdqYr0KNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AQYwHQYDVR0OBBYEFMdiUsYMFhSVHxfGsCvgPKLDU8IIMAoGCCqGSM49BAMCA0kA\n"
    "MEYCIQD2AGKXmW/ROSOzJo10XTqvXUwEODIl3uqnLb+saru3xgIhAPiC8KapHTT1\n"
    "vm1UPtZx/3l0iuaVCPcVlfE9CU2nv519\n"
    "-----END CERTIFICATE-----\n";

static const char email_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBuDCCAV2gAwIBAgIBAjAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDIzOTIzWhgPMjEyNjA4MjUwMjM5MjNaMC4xLDAqBgNVBAMM\n"
    "I1Rlc3QgRW1haWwgQ29uc3RyYWluZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0C\n"
    "AQYIKoZIzj0DAQcDQgAEKTTN7AV8n6vriCrs7iTPqWExXvH893grPR8eo//KhgBS\n"
    "n9YYl9eGJlW25YnjEfphQzwg7oMYM2gJkIiAJN716aOBgzCBgDAPBgNVHRMBAf8E\n"
    "BTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUIMh1YE360lpqRkKxw2ey\n"
    "XFzUv9MwHwYDVR0jBBgwFoAUx2JSxgwWFJUfF8awK+A8osNTwggwHQYDVR0eAQH/\n"
    "BBMwEaAPMA2BC2V4YW1wbGUuY29tMAoGCCqGSM49BAMCA0kAMEYCIQCCNtcyCW88\n"
    "nMntal7XuC0juyjtwbcWK1E65J9sVipe6AIhAO+H877mEsVExqMhx8OsTu+YtF2J\n"
    "rFgKAD6ILBwyw0hU\n"
    "-----END CERTIFICATE-----\n";

static const char email_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBtTCCAVqgAwIBAgIBAzAKBggqhkjOPQQDAjAuMSwwKgYDVQQDDCNUZXN0IEVt\n"
    "YWlsIENvbnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjM5MjNaGA8y\n"
    "MTI2MDgyNTAyMzkyM1owNTESMBAGA1UEAwwJVGVzdCBVc2VyMR8wHQYJKoZIhvcN\n"
    "AQkBFhB1c2VyQGV4YW1wbGUub3JnMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
    "i/q8DtzJvuBQ4c2U9OXbieyL/+cfI1bRkUhHpMQVsbhonV2fjgfT33HRcjvorgaT\n"
    "e+vFxEGJBYuzu1LpYzPD0qNgMF4wDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMC\n"
    "B4AwHQYDVR0OBBYEFGGMF/eHr75J4fsiA3TyskXf8MmiMB8GA1UdIwQYMBaAFCDI\n"
    "dWBN+tJaakZCscNnslxc1L/TMAoGCCqGSM49BAMCA0kAMEYCIQDV5N8xeyrtQThp\n"
    "dGqxQVs5QyqFt0o3PGceR7KeiQQhowIhAOCHFKBjN2b7er8Moa319rMDbs3AN4rw\n"
    "YIEp9QRLgyOu\n"
    "-----END CERTIFICATE-----\n";

static const time_t check_time = 1798761600;

struct chain_test {
	const char *root_pem;
	const char *inter_pem;
	const char *leaf_pem;
	int want_error;
};

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
 * is cached on any of them when verification starts.
 */
static void
test_chain(struct test *t, const void *arg)
{
	const struct chain_test *ct = arg;
	X509 *root = NULL, *intermediate = NULL, *leaf = NULL;
	X509_STORE *store = NULL;
	X509_STORE_CTX *ctx = NULL;
	STACK_OF(X509) *untrusted = NULL;
	int error, ret;

	if ((root = cert_from_pem(t, ct->root_pem)) == NULL)
		goto err;
	if ((intermediate = cert_from_pem(t, ct->inter_pem)) == NULL)
		goto err;
	if ((leaf = cert_from_pem(t, ct->leaf_pem)) == NULL)
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

	ret = X509_verify_cert(ctx);
	error = X509_STORE_CTX_get_error(ctx);

	if (ct->want_error == X509_V_OK) {
		if (ret != 1)
			test_errorf(t, "X509_verify_cert: %s",
			    X509_verify_cert_error_string(error));
	} else {
		if (ret == 1)
			test_errorf(t, "X509_verify_cert succeeded, want %s",
			    X509_verify_cert_error_string(ct->want_error));
		else if (error != ct->want_error)
			test_errorf(t, "X509_verify_cert: %s, want %s",
			    X509_verify_cert_error_string(error),
			    X509_verify_cert_error_string(ct->want_error));
	}

 err:
	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);
	sk_X509_free(untrusted);
	X509_free(root);
	X509_free(intermediate);
	X509_free(leaf);
}

/*
 * The intermediate is constrained to DNS names under example.com. The leaf's
 * subject CN, leaf.example.org, lies outside that subtree while its SAN,
 * www.example.com, lies inside it, so the chain is only accepted if the
 * leaf's cached SAN is in place when its names are checked.
 */
static const struct chain_test dns_permitted_san = {
	.root_pem = dns_root_pem,
	.inter_pem = dns_inter_pem,
	.leaf_pem = dns_leaf_pem,
	.want_error = X509_V_OK,
};

/*
 * The intermediate is constrained to email addresses under example.com. The
 * leaf has no SAN and carries user@example.org as the second entry of its
 * subject, so the chain is only rejected if that entry is found.
 */
static const struct chain_test email_subject_not_permitted = {
	.root_pem = email_root_pem,
	.inter_pem = email_inter_pem,
	.leaf_pem = email_leaf_pem,
	.want_error = X509_V_ERR_PERMITTED_VIOLATION,
};

int
main(int argc, char **argv)
{
	struct test *t = test_init();

	test_run(t, "dns permitted san", test_chain, &dns_permitted_san);
	test_run(t, "email subject not permitted", test_chain,
	    &email_subject_not_permitted);

	return test_result(t);
}
