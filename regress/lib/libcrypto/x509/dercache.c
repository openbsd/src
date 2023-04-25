/* $OpenBSD: dercache.c,v 1.1 2023/04/25 21:51:44 job Exp $ */
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

#define SETUP()							\
	derp = der;						\
	if ((a = d2i_X509(NULL, &derp, dersz)) == NULL)		\
		errx(1, "d2i_X509");				\
	if ((der2sz = i2d_X509(a, &der2)) <= 0)			\
		errx(1, "i2d_X509");				\
	der2p = der2;

#define CLEANUP()						\
	X509_free(a);						\
	a = NULL;						\
	free(der2);						\
	der2 = NULL;

#define CLEANUPSETUP()						\
	CLEANUP()						\
	SETUP()

#define SETX509NAME(fname, value, cert)				\
	if ((xn = X509_NAME_new()) == NULL)			\
		err(1, NULL);					\
	if (!X509_NAME_add_entry_by_txt(xn, "C", MBSTRING_ASC,	\
	    (const unsigned char*) value, -1, -1, 0))		\
		errx(1, "X509_NAME_add_entry_by_txt");		\
	if (!fname(cert, xn))					\
		errx(1, "fname");				\
	X509_NAME_free(xn);					\
	xn = NULL;

#define SETASN1TIME(fname, value, cert)				\
	if ((at = ASN1_TIME_new()) == NULL)			\
		err(1, NULL);					\
	if ((at = X509_gmtime_adj(NULL, value)) == NULL)	\
		errx(1, "X509_gmtime_adj");			\
	if (!fname(cert, at))					\
		errx(1, "fname");				\
	ASN1_TIME_free(at);					\
	at = NULL;

#define SETINTEGER(fname, value, cert)				\
	if ((ai = ASN1_INTEGER_new()) == NULL)			\
		err(1, NULL);					\
	if (!ASN1_INTEGER_set(ai, value))			\
		errx(1, "ASN1_INTEGER_set");			\
	if (!fname(cert, ai))					\
		errx(1, "fname");				\
	ASN1_INTEGER_free(ai);					\
	ai = NULL;


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
	ASN1_INTEGER *ai = NULL;
	ASN1_TIME *at = NULL;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pkey_ctx = NULL;
	X509_NAME *xn = NULL;
	X509 *a, *x;
	const unsigned char *derp, *der2p;
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

	SETINTEGER(X509_set_serialNumber, 1, x)
	SETASN1TIME(X509_set_notBefore, 0, x)
	SETASN1TIME(X509_set_notAfter, 60, x)
	SETX509NAME(X509_set_issuer_name, "NL", x)
	SETX509NAME(X509_set_subject_name, "BE", x)

	// one time creation of the original DER
	if (!X509_sign(x, pkey, EVP_sha256()))
		errx(1, "X509_sign");
	if ((dersz = i2d_X509(x, &der)) <= 0)
		errx(1, "i2d_X509");

	SETUP()

	// test X509_set_version
	if (!X509_set_version(a, 2))
		errx(1, "X509_set_version");
	ret += x509_compare("X509_set_version", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_serialNumber
	SETINTEGER(X509_set_serialNumber, 2, a)
	ret += x509_compare("X509_set_serialNumber", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_issuer_name
	SETX509NAME(X509_set_issuer_name, "DE", a)
	ret += x509_compare("X509_set_issuer_name", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_subject_name
	SETX509NAME(X509_set_subject_name, "FR", a)
	ret += x509_compare("X509_set_subject_name", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_notBefore
	SETASN1TIME(X509_set_notBefore, 120, a)
	ret += x509_compare("X509_set_notBefore", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_notAfter
	SETASN1TIME(X509_set_notAfter, 180, a)
	ret += x509_compare("X509_set_notAfter", a, der2p, der2sz);

	CLEANUPSETUP()

	// test X509_set_pubkey
	if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0)
		errx(1, "EVP_PKEY_keygen");
	if (X509_set_pubkey(a, pkey) != 1)
		errx(1, "X509_set_pubkey");
	ret += x509_compare("X509_set_pubkey", a, der2p, der2sz);

	CLEANUP()

	if (ret)
		return 1;
}
