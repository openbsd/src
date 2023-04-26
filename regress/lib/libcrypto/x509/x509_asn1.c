/* $OpenBSD: x509_asn1.c,v 1.3 2023/04/26 10:55:58 job Exp $ */
/*
 * Copyright (c) 2023 Job Snijders <job@openbsd.org>
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

/*
 * This program tests whether the presence of "->enc.modified = 1;"
 * in select X509 setter functions properly triggers invalidation of cached
 * DER.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

static void
x509_setup(unsigned char **der, unsigned char **der2, X509 **x,
    const unsigned char **cpder2, long dersz, long *der2sz)
{
	const unsigned char *cpder;

	cpder = *der;
	if ((*x = d2i_X509(NULL, &cpder, dersz)) == NULL)
		errx(1, "d2i_X509");
	if ((*der2sz = i2d_X509(*x, der2)) <= 0)
		errx(1, "i2d_X509");
	*cpder2 = *der2;
}

static void
x509_cleanup(X509 **x, unsigned char **der)
{
	X509_free(*x);
	*x = NULL;
	free(*der);
	*der = NULL;
}

static void
x509_set_integer(int (*f)(X509 *x, ASN1_INTEGER *ai), X509 **x, int i)
{
	ASN1_INTEGER *ai;

	if ((ai = ASN1_INTEGER_new()) == NULL)
		err(1, NULL);
	if (!ASN1_INTEGER_set(ai, i))
		errx(1, "ASN1_INTEGER_set");
	if (!(*f)(*x, ai))
		err(1, NULL);

	ASN1_INTEGER_free(ai);
}

static void
x509_set_name(int (*f)(X509 *x, X509_NAME *name), X509 **x,
    const unsigned char *n)
{
	X509_NAME *xn;

	if ((xn = X509_NAME_new()) == NULL)
		err(1, NULL);
	if (!X509_NAME_add_entry_by_txt(xn, "C", MBSTRING_ASC, n, -1, -1, 0))
		errx(1, "X509_NAME_add_entry_by_txt");
	if (!(*f)(*x, xn))
		err(1, NULL);

	X509_NAME_free(xn);
}

static void
x509_set_time(int (*f)(X509 *x, const ASN1_TIME *tm), X509 **x, int t)
{
	ASN1_TIME *at;

	if ((at = ASN1_TIME_new()) == NULL)
		err(1, NULL);
	if ((at = X509_gmtime_adj(NULL, t)) == NULL)
		errx(1, "X509_gmtime_adj");
	if (!(*f)(*x, at))
		err(1, NULL);

	ASN1_TIME_free(at);
}

static int
x509_compare(char *f, X509 *a, const unsigned char *der, long dersz)
{
	unsigned char *der_test = NULL;
	long der_testsz;
	int rc = 0;

	if ((der_testsz = i2d_X509(a, &der_test)) <= 0)
		errx(1, "i2d_X509");

	if (dersz == der_testsz) {
		if (memcmp(der, der_test, dersz) == 0) {
			warnx("%s() didn't invalidate DER cache", f);
			rc = 1;
		} else
			warnx("%s() OK", f);
	} else
		warnx("%s() OK", f);

	free(der_test);
	return rc;
}

int
main(void)
{
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pkey_ctx = NULL;
	X509 *a, *x;
	const unsigned char *cpder2;
	unsigned char *der = NULL, *der2 = NULL;
	long dersz, der2sz;
	int ret = 0;

	if ((x = X509_new()) == NULL)
		err(1, NULL);

	if ((pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)) == NULL)
		errx(1, "EVP_PKEY_CTX_new_id");
	if (EVP_PKEY_keygen_init(pkey_ctx) != 1)
		errx(1, "EVP_PKEY_keygen_init");
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(pkey_ctx, 2048) <= 0)
		errx(1, "EVP_PKEY_CTX_set_rsa_keygen_bits");
	if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0)
		errx(1, "EVP_PKEY_keygen");
	if (X509_set_pubkey(x, pkey) != 1)
		errx(1, "X509_set_pubkey");

	x509_set_integer(X509_set_serialNumber, &x, 1);
	x509_set_time(X509_set_notBefore, &x, 0);
	x509_set_time(X509_set_notAfter, &x, 60);
	x509_set_name(X509_set_issuer_name, &x, "NL");
	x509_set_name(X509_set_subject_name, &x, "BE");

	// one time creation of the original DER
	if (!X509_sign(x, pkey, EVP_sha256()))
		errx(1, "X509_sign");
	if ((dersz = i2d_X509(x, &der)) <= 0)
		errx(1, "i2d_X509");

	/* test X509_set_version */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	if (!X509_set_version(a, 2))
		errx(1, "X509_set_version");
	ret += x509_compare("X509_set_version", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_serialNumber */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	x509_set_integer(X509_set_serialNumber, &a, 2);
	ret += x509_compare("X509_set_serialNumber", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_issuer_name */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	x509_set_name(X509_set_issuer_name, &a, "DE");
	ret += x509_compare("X509_set_issuer_name", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_subject_name */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	x509_set_name(X509_set_subject_name, &a, "FR");
	ret += x509_compare("X509_set_subject_name", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_notBefore */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	x509_set_time(X509_set_notBefore, &a, 120);
	ret += x509_compare("X509_set_notBefore", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_notAfter */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	x509_set_time(X509_set_notAfter, &a, 180);
	ret += x509_compare("X509_set_notAfter", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	/* test X509_set_pubkey */
	x509_setup(&der, &der2, &a, &cpder2, dersz, &der2sz);
	if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0)
		errx(1, "EVP_PKEY_keygen");
	if (X509_set_pubkey(a, pkey) != 1)
		errx(1, "X509_set_pubkey");
	EVP_PKEY_CTX_free(pkey_ctx);
	EVP_PKEY_free(pkey);
	ret += x509_compare("X509_set_pubkey", a, cpder2, der2sz);
	x509_cleanup(&a, &der2);

	if (ret)
		return 1;
	return 0;
}
