/* $OpenBSD: freenull.c,v 1.8 2018/04/23 08:07:50 tb Exp $ */
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
#include <openssl/cmac.h>
#include <openssl/comp.h>
#include <openssl/conf_api.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/gost.h>
#include <openssl/hmac.h>
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
	ACCESS_DESCRIPTION_free(NULL);
	ASN1_BIT_STRING_free(NULL);
	ASN1_BMPSTRING_free(NULL);
	ASN1_ENUMERATED_free(NULL);
	ASN1_GENERALIZEDTIME_free(NULL);
	ASN1_GENERALSTRING_free(NULL);
	ASN1_IA5STRING_free(NULL);
	ASN1_INTEGER_free(NULL);
	ASN1_NULL_free(NULL);
	ASN1_OBJECT_free(NULL);
	ASN1_OCTET_STRING_free(NULL);
	ASN1_PCTX_free(NULL);
	ASN1_PRINTABLESTRING_free(NULL);
	ASN1_PRINTABLE_free(NULL);
	ASN1_STRING_free(NULL);
	ASN1_T61STRING_free(NULL);
	ASN1_TIME_free(NULL);
	ASN1_TYPE_free(NULL);
	ASN1_UNIVERSALSTRING_free(NULL);
	ASN1_UTCTIME_free(NULL);
	ASN1_UTF8STRING_free(NULL);
	ASN1_VISIBLESTRING_free(NULL);
	AUTHORITY_INFO_ACCESS_free(NULL);
	AUTHORITY_KEYID_free(NULL);
	BASIC_CONSTRAINTS_free(NULL);
	BIO_free(NULL);
	BIO_free_all(NULL);
	BIO_meth_free(NULL);
	BN_BLINDING_free(NULL);
	BN_CTX_free(NULL);
	BN_GENCB_free(NULL);
	BN_MONT_CTX_free(NULL);
	BN_RECP_CTX_free(NULL);
	BN_clear_free(NULL);
	BN_free(NULL);
	BUF_MEM_free(NULL);
	CERTIFICATEPOLICIES_free(NULL);
	CMAC_CTX_free(NULL);
	COMP_CTX_free(NULL);
	CONF_free(NULL);
	CRL_DIST_POINTS_free(NULL);
	DH_free(NULL);
	DIRECTORYSTRING_free(NULL);
	DISPLAYTEXT_free(NULL);
	DIST_POINT_NAME_free(NULL);
	DIST_POINT_free(NULL);
	DSA_SIG_free(NULL);
	DSA_free(NULL);
	DSA_meth_free(NULL);
	ECDSA_SIG_free(NULL);
	EC_GROUP_clear_free(NULL);
	EC_GROUP_free(NULL);
	EC_KEY_free(NULL);
	EC_POINT_clear_free(NULL);
	EC_POINT_free(NULL);
	EDIPARTYNAME_free(NULL);
#ifndef OPENSSL_NO_ENGINE
	ENGINE_free(NULL);
#endif
	ESS_CERT_ID_free(NULL);
	ESS_ISSUER_SERIAL_free(NULL);
	ESS_SIGNING_CERT_free(NULL);
	EVP_CIPHER_CTX_free(NULL);
	EVP_MD_CTX_free(NULL);
	EVP_PKEY_CTX_free(NULL);
	EVP_PKEY_asn1_free(NULL);
	EVP_PKEY_free(NULL);
	EVP_PKEY_meth_free(NULL);
	EXTENDED_KEY_USAGE_free(NULL);
	GENERAL_NAMES_free(NULL);
	GENERAL_NAME_free(NULL);
	GENERAL_SUBTREE_free(NULL);
	GOST_CIPHER_PARAMS_free(NULL);
	GOST_KEY_free(NULL);
	HMAC_CTX_free(NULL);
	ISSUING_DIST_POINT_free(NULL);
	NAME_CONSTRAINTS_free(NULL);
	NCONF_free(NULL);
	NCONF_free_data(NULL);
	NETSCAPE_CERT_SEQUENCE_free(NULL);
	NETSCAPE_SPKAC_free(NULL);
	NETSCAPE_SPKI_free(NULL);
	NETSCAPE_X509_free(NULL);
	NOTICEREF_free(NULL);
	OCSP_BASICRESP_free(NULL);
	OCSP_CERTID_free(NULL);
	OCSP_CERTSTATUS_free(NULL);
	OCSP_CRLID_free(NULL);
	OCSP_ONEREQ_free(NULL);
	OCSP_REQINFO_free(NULL);
	OCSP_REQUEST_free(NULL);
	OCSP_REQ_CTX_free(NULL);
	OCSP_RESPBYTES_free(NULL);
	OCSP_RESPDATA_free(NULL);
	OCSP_RESPID_free(NULL);
	OCSP_RESPONSE_free(NULL);
	OCSP_REVOKEDINFO_free(NULL);
	OCSP_SERVICELOC_free(NULL);
	OCSP_SIGNATURE_free(NULL);
	OCSP_SINGLERESP_free(NULL);
	OTHERNAME_free(NULL);
	PBEPARAM_free(NULL);
	PBKDF2PARAM_free(NULL);
	PKCS12_BAGS_free(NULL);
	PKCS12_MAC_DATA_free(NULL);
	PKCS12_SAFEBAG_free(NULL);
	PKCS12_free(NULL);
	PKCS7_DIGEST_free(NULL);
	PKCS7_ENCRYPT_free(NULL);
	PKCS7_ENC_CONTENT_free(NULL);
	PKCS7_ENVELOPE_free(NULL);
	PKCS7_ISSUER_AND_SERIAL_free(NULL);
	PKCS7_RECIP_INFO_free(NULL);
	PKCS7_SIGNED_free(NULL);
	PKCS7_SIGNER_INFO_free(NULL);
	PKCS7_SIGN_ENVELOPE_free(NULL);
	PKCS7_free(NULL);
	PKCS8_PRIV_KEY_INFO_free(NULL);
	PKEY_USAGE_PERIOD_free(NULL);
	POLICYINFO_free(NULL);
	POLICYQUALINFO_free(NULL);
	POLICY_CONSTRAINTS_free(NULL);
	POLICY_MAPPING_free(NULL);
	PROXY_CERT_INFO_EXTENSION_free(NULL);
	PROXY_POLICY_free(NULL);
	RSA_PSS_PARAMS_free(NULL);
	RSA_free(NULL);
	RSA_meth_free(NULL);
	SXNETID_free(NULL);
	SXNET_free(NULL);
	TS_ACCURACY_free(NULL);
	TS_MSG_IMPRINT_free(NULL);
	TS_REQ_ext_free(NULL);
	TS_REQ_free(NULL);
	TS_RESP_CTX_free(NULL);
	TS_RESP_free(NULL);
	TS_STATUS_INFO_free(NULL);
	TS_TST_INFO_ext_free(NULL);
	TS_TST_INFO_free(NULL);
	TS_VERIFY_CTX_free(NULL);
	TXT_DB_free(NULL);
	UI_free(NULL);
	USERNOTICE_free(NULL);
	X509V3_conf_free(NULL);
	X509_ALGOR_free(NULL);
	X509_ATTRIBUTE_free(NULL);
	X509_CERT_AUX_free(NULL);
	X509_CERT_PAIR_free(NULL);
	X509_CINF_free(NULL);
	X509_CRL_INFO_free(NULL);
	X509_CRL_free(NULL);
	X509_EXTENSION_free(NULL);
	X509_INFO_free(NULL);
	X509_LOOKUP_free(NULL);
	X509_NAME_ENTRY_free(NULL);
	X509_NAME_free(NULL);
	X509_PKEY_free(NULL);
	X509_PUBKEY_free(NULL);
	X509_REQ_INFO_free(NULL);
	X509_REQ_free(NULL);
	X509_REVOKED_free(NULL);
	X509_SIG_free(NULL);
	X509_STORE_CTX_free(NULL);
	X509_STORE_free(NULL);
	X509_VAL_free(NULL);
	X509_VERIFY_PARAM_free(NULL);
	X509_email_free(NULL);
	X509_free(NULL);
	X509_policy_tree_free(NULL);
	_CONF_free_data(NULL);

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
