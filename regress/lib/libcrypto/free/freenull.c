/* $OpenBSD: freenull.c,v 1.9 2018/04/23 08:09:57 tb Exp $ */
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
	ACCESS_DESCRIPTION_free(NULL);			/* ASN1_item_free */
	ASN1_BIT_STRING_free(NULL);			/* ASN1_item_free */
	ASN1_BMPSTRING_free(NULL);			/* ASN1_item_free */
	ASN1_ENUMERATED_free(NULL);			/* ASN1_item_free */
	ASN1_GENERALIZEDTIME_free(NULL);		/* ASN1_item_free */
	ASN1_GENERALSTRING_free(NULL);			/* ASN1_item_free */
	ASN1_IA5STRING_free(NULL);			/* ASN1_item_free */
	ASN1_INTEGER_free(NULL);			/* ASN1_item_free */
	ASN1_NULL_free(NULL);				/* ASN1_item_free */
	ASN1_OBJECT_free(NULL);
	ASN1_OCTET_STRING_free(NULL);			/* ASN1_item_free */
	ASN1_PCTX_free(NULL);
	ASN1_PRINTABLESTRING_free(NULL);		/* ASN1_item_free */
	ASN1_PRINTABLE_free(NULL);			/* ASN1_item_free */
	ASN1_STRING_free(NULL);
	ASN1_T61STRING_free(NULL);			/* ASN1_item_free */
	ASN1_TIME_free(NULL);				/* ASN1_item_free */
	ASN1_TYPE_free(NULL);				/* ASN1_item_free */
	ASN1_UNIVERSALSTRING_free(NULL);		/* ASN1_item_free */
	ASN1_UTCTIME_free(NULL);			/* ASN1_item_free */
	ASN1_UTF8STRING_free(NULL);			/* ASN1_item_free */
	ASN1_VISIBLESTRING_free(NULL);			/* ASN1_item_free */
	AUTHORITY_INFO_ACCESS_free(NULL);		/* ASN1_item_free */
	AUTHORITY_KEYID_free(NULL);			/* ASN1_item_free */
	BASIC_CONSTRAINTS_free(NULL);			/* ASN1_item_free */
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
	CERTIFICATEPOLICIES_free(NULL);			/* ASN1_item_free */
	CMAC_CTX_free(NULL);
	COMP_CTX_free(NULL);
	CONF_free(NULL);
	CRL_DIST_POINTS_free(NULL);			/* ASN1_item_free */
	DH_free(NULL);
	DIRECTORYSTRING_free(NULL);			/* ASN1_item_free */
	DISPLAYTEXT_free(NULL);				/* ASN1_item_free */
	DIST_POINT_NAME_free(NULL);			/* ASN1_item_free */
	DIST_POINT_free(NULL);				/* ASN1_item_free */
	DSA_SIG_free(NULL);
	DSA_free(NULL);
	DSA_meth_free(NULL);
	ECDSA_SIG_free(NULL);				/* ASN1_item_free */
	EC_GROUP_clear_free(NULL);
	EC_GROUP_free(NULL);
	EC_KEY_free(NULL);
	EC_POINT_clear_free(NULL);
	EC_POINT_free(NULL);
	EDIPARTYNAME_free(NULL);			/* ASN1_item_free */
#ifndef OPENSSL_NO_ENGINE
	ENGINE_free(NULL);
#endif
	ESS_CERT_ID_free(NULL);				/* ASN1_item_free */
	ESS_ISSUER_SERIAL_free(NULL);			/* ASN1_item_free */
	ESS_SIGNING_CERT_free(NULL);			/* ASN1_item_free */
	EVP_CIPHER_CTX_free(NULL);
	EVP_MD_CTX_free(NULL);
	EVP_PKEY_CTX_free(NULL);
	EVP_PKEY_asn1_free(NULL);
	EVP_PKEY_free(NULL);
	EVP_PKEY_meth_free(NULL);
	EXTENDED_KEY_USAGE_free(NULL);			/* ASN1_item_free */
	GENERAL_NAMES_free(NULL);			/* ASN1_item_free */
	GENERAL_NAME_free(NULL);			/* ASN1_item_free */
	GENERAL_SUBTREE_free(NULL);			/* ASN1_item_free */
	GOST_CIPHER_PARAMS_free(NULL);			/* ASN1_item_free */
	GOST_KEY_free(NULL);
	HMAC_CTX_free(NULL);
	ISSUING_DIST_POINT_free(NULL);			/* ASN1_item_free */
	NAME_CONSTRAINTS_free(NULL);			/* ASN1_item_free */
	NCONF_free(NULL);
	NCONF_free_data(NULL);
	NETSCAPE_CERT_SEQUENCE_free(NULL);		/* ASN1_item_free */
	NETSCAPE_SPKAC_free(NULL);			/* ASN1_item_free */
	NETSCAPE_SPKI_free(NULL);			/* ASN1_item_free */
	NETSCAPE_X509_free(NULL);			/* ASN1_item_free */
	NOTICEREF_free(NULL);				/* ASN1_item_free */
	OCSP_BASICRESP_free(NULL);			/* ASN1_item_free */
	OCSP_CERTID_free(NULL);				/* ASN1_item_free */
	OCSP_CERTSTATUS_free(NULL);			/* ASN1_item_free */
	OCSP_CRLID_free(NULL);				/* ASN1_item_free */
	OCSP_ONEREQ_free(NULL);				/* ASN1_item_free */
	OCSP_REQINFO_free(NULL);			/* ASN1_item_free */
	OCSP_REQUEST_free(NULL);			/* ASN1_item_free */
	OCSP_REQ_CTX_free(NULL);
	OCSP_RESPBYTES_free(NULL);			/* ASN1_item_free */
	OCSP_RESPDATA_free(NULL);			/* ASN1_item_free */
	OCSP_RESPID_free(NULL);				/* ASN1_item_free */
	OCSP_RESPONSE_free(NULL);			/* ASN1_item_free */
	OCSP_REVOKEDINFO_free(NULL);			/* ASN1_item_free */
	OCSP_SERVICELOC_free(NULL);			/* ASN1_item_free */
	OCSP_SIGNATURE_free(NULL);			/* ASN1_item_free */
	OCSP_SINGLERESP_free(NULL);			/* ASN1_item_free */
	OTHERNAME_free(NULL);				/* ASN1_item_free */
	PBEPARAM_free(NULL);				/* ASN1_item_free */
	PBKDF2PARAM_free(NULL);				/* ASN1_item_free */
	PKCS12_BAGS_free(NULL);				/* ASN1_item_free */
	PKCS12_MAC_DATA_free(NULL);			/* ASN1_item_free */
	PKCS12_SAFEBAG_free(NULL);			/* ASN1_item_free */
	PKCS12_free(NULL);				/* ASN1_item_free */
	PKCS7_DIGEST_free(NULL);			/* ASN1_item_free */
	PKCS7_ENCRYPT_free(NULL);			/* ASN1_item_free */
	PKCS7_ENC_CONTENT_free(NULL);			/* ASN1_item_free */
	PKCS7_ENVELOPE_free(NULL);			/* ASN1_item_free */
	PKCS7_ISSUER_AND_SERIAL_free(NULL);		/* ASN1_item_free */
	PKCS7_RECIP_INFO_free(NULL);			/* ASN1_item_free */
	PKCS7_SIGNED_free(NULL);			/* ASN1_item_free */
	PKCS7_SIGNER_INFO_free(NULL);			/* ASN1_item_free */
	PKCS7_SIGN_ENVELOPE_free(NULL);			/* ASN1_item_free */
	PKCS7_free(NULL);				/* ASN1_item_free */
	PKCS8_PRIV_KEY_INFO_free(NULL);			/* ASN1_item_free */
	PKEY_USAGE_PERIOD_free(NULL);			/* ASN1_item_free */
	POLICYINFO_free(NULL);				/* ASN1_item_free */
	POLICYQUALINFO_free(NULL);			/* ASN1_item_free */
	POLICY_CONSTRAINTS_free(NULL);			/* ASN1_item_free */
	POLICY_MAPPING_free(NULL);			/* ASN1_item_free */
	PROXY_CERT_INFO_EXTENSION_free(NULL);		/* ASN1_item_free */
	PROXY_POLICY_free(NULL);			/* ASN1_item_free */
	RSA_PSS_PARAMS_free(NULL);			/* ASN1_item_free */
	RSA_free(NULL);
	RSA_meth_free(NULL);
	SXNETID_free(NULL);				/* ASN1_item_free */
	SXNET_free(NULL);				/* ASN1_item_free */
	TS_ACCURACY_free(NULL);				/* ASN1_item_free */
	TS_MSG_IMPRINT_free(NULL);			/* ASN1_item_free */
	TS_REQ_ext_free(NULL);
	TS_REQ_free(NULL);				/* ASN1_item_free */
	TS_RESP_CTX_free(NULL);
	TS_RESP_free(NULL);				/* ASN1_item_free */
	TS_STATUS_INFO_free(NULL);			/* ASN1_item_free */
	TS_TST_INFO_ext_free(NULL);
	TS_TST_INFO_free(NULL);				/* ASN1_item_free */
	TS_VERIFY_CTX_free(NULL);
	TXT_DB_free(NULL);
	UI_free(NULL);
	USERNOTICE_free(NULL);				/* ASN1_item_free */
	X509V3_conf_free(NULL);
	X509_ALGOR_free(NULL);				/* ASN1_item_free */
	X509_ATTRIBUTE_free(NULL);			/* ASN1_item_free */
	X509_CERT_AUX_free(NULL);			/* ASN1_item_free */
	X509_CERT_PAIR_free(NULL);			/* ASN1_item_free */
	X509_CINF_free(NULL);				/* ASN1_item_free */
	X509_CRL_INFO_free(NULL);			/* ASN1_item_free */
	X509_CRL_free(NULL);				/* ASN1_item_free */
	X509_EXTENSION_free(NULL);			/* ASN1_item_free */
	X509_INFO_free(NULL);
	X509_LOOKUP_free(NULL);
	X509_NAME_ENTRY_free(NULL);			/* ASN1_item_free */
	X509_NAME_free(NULL);				/* ASN1_item_free */
	X509_PKEY_free(NULL);
	X509_PUBKEY_free(NULL);				/* ASN1_item_free */
	X509_REQ_INFO_free(NULL);			/* ASN1_item_free */
	X509_REQ_free(NULL);				/* ASN1_item_free */
	X509_REVOKED_free(NULL);			/* ASN1_item_free */
	X509_SIG_free(NULL);				/* ASN1_item_free */
	X509_STORE_CTX_free(NULL);
	X509_STORE_free(NULL);
	X509_VAL_free(NULL);				/* ASN1_item_free */
	X509_VERIFY_PARAM_free(NULL);
	X509_email_free(NULL);
	X509_free(NULL);				/* ASN1_item_free */
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
