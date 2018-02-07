/* $OpenBSD: freenull.c,v 1.6 2018/02/07 05:07:39 jsing Exp $ */
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
#include <openssl/ocsp.h>
#include <openssl/pkcs12.h>
#include <openssl/ts.h>
#include <openssl/ui.h>
#include <openssl/txt_db.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

/* Make sure we do the right thing. Add here if you convert ones in tree */
int
main(int argc, char **argv)
{
	ASN1_ENUMERATED_free(NULL);
	ASN1_GENERALIZEDTIME_free(NULL);
	ASN1_INTEGER_free(NULL);
	ASN1_OBJECT_free(NULL);
	ASN1_OCTET_STRING_free(NULL);
	ASN1_TIME_free(NULL);
	ASN1_TYPE_free(NULL);
	ASN1_UTCTIME_free(NULL);
	BIO_free(NULL);
	BIO_free_all(NULL);
	BN_clear_free(NULL);
	BN_free(NULL);
	BUF_MEM_free(NULL);
	CONF_free(NULL);
	DH_free(NULL);
	DIST_POINT_free(NULL);
	DSA_SIG_free(NULL);
	DSA_free(NULL);
	ECDSA_SIG_free(NULL);
	EC_GROUP_free(NULL);
	EC_KEY_free(NULL);
	EC_POINT_clear_free(NULL);
	EC_POINT_free(NULL);
	EVP_CIPHER_CTX_free(NULL);
	EVP_PKEY_CTX_free(NULL);
	EVP_PKEY_free(NULL);
	GENERAL_NAME_free(NULL);
	GENERAL_SUBTREE_free(NULL);
	NAME_CONSTRAINTS_free(NULL);
	NCONF_free(NULL);
	NETSCAPE_CERT_SEQUENCE_free(NULL);
	NETSCAPE_SPKI_free(NULL);
	NETSCAPE_X509_free(NULL);
	OCSP_BASICRESP_free(NULL);
	OCSP_CERTID_free(NULL);
	OCSP_REQUEST_free(NULL);
	OCSP_REQ_CTX_free(NULL);
	OCSP_RESPONSE_free(NULL);
	PBEPARAM_free(NULL);
	PKCS12_free(NULL);
	PKCS7_free(NULL);
	PKCS8_PRIV_KEY_INFO_free(NULL);
	RSA_free(NULL);
	TS_MSG_IMPRINT_free(NULL);
	TS_REQ_free(NULL);
	TS_RESP_CTX_free(NULL);
	TS_RESP_free(NULL);
	TS_STATUS_INFO_free(NULL);
	TS_TST_INFO_free(NULL);
	TS_VERIFY_CTX_free(NULL);
	TXT_DB_free(NULL);
	UI_free(NULL);
	X509_ALGOR_free(NULL);
	X509_CRL_free(NULL);
	X509_EXTENSION_free(NULL);
	X509_INFO_free(NULL);
	X509_NAME_ENTRY_free(NULL);
	X509_NAME_free(NULL);
	X509_REQ_free(NULL);
	X509_SIG_free(NULL);
	X509_STORE_CTX_free(NULL);
	X509_STORE_free(NULL);
	X509_VERIFY_PARAM_free(NULL);
	X509_email_free(NULL);
	X509_free(NULL);

	lh_FUNCTION_free(NULL);

	sk_ASN1_OBJECT_pop_free(NULL, NULL);
	sk_CONF_VALUE_pop_free(NULL, NULL);
	sk_GENERAL_NAME_pop_free(NULL, NULL);
	sk_OCSP_CERTID_free(NULL);
	sk_OPENSSL_STRING_free(NULL);
	sk_PKCS12_SAFEBAG_pop_free(NULL, NULL);
	sk_PKCS7_pop_free(NULL, NULL);
	sk_X509_ATTRIBUTE_free(NULL);
	sk_X509_CRL_pop_free(NULL, NULL);
	sk_X509_EXTENSION_pop_free(NULL, NULL);
	sk_X509_INFO_free(NULL);
	sk_X509_INFO_pop_free(NULL, NULL);
	sk_X509_NAME_ENTRY_pop_free(NULL, NULL);
	sk_X509_free(NULL);
	sk_X509_pop_free(NULL, NULL);

	printf("PASS\n");

	return (0);
}
