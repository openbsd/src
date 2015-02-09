/**********************************************************************
 *                          gost_keytrans.c                           *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *   ASN1 structure definition for GOST key transport                 *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_GOST
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/gost.h>

#include "gost_locl.h"
#include "gost_asn1.h"

ASN1_NDEF_SEQUENCE(GOST_KEY_TRANSPORT) = {
	ASN1_SIMPLE(GOST_KEY_TRANSPORT, key_info, GOST_KEY_INFO),
	ASN1_IMP(GOST_KEY_TRANSPORT, key_agreement_info, GOST_KEY_AGREEMENT_INFO, 0)
} ASN1_NDEF_SEQUENCE_END(GOST_KEY_TRANSPORT)

GOST_KEY_TRANSPORT *
d2i_GOST_KEY_TRANSPORT(GOST_KEY_TRANSPORT **a, const unsigned char **in, long len)
{
	return (GOST_KEY_TRANSPORT *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GOST_KEY_TRANSPORT_it);
}

int
i2d_GOST_KEY_TRANSPORT(GOST_KEY_TRANSPORT *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GOST_KEY_TRANSPORT_it);
}

GOST_KEY_TRANSPORT *
GOST_KEY_TRANSPORT_new(void)
{
	return (GOST_KEY_TRANSPORT *)ASN1_item_new(&GOST_KEY_TRANSPORT_it);
}

void
GOST_KEY_TRANSPORT_free(GOST_KEY_TRANSPORT *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GOST_KEY_TRANSPORT_it);
}

ASN1_NDEF_SEQUENCE(GOST_KEY_INFO) = {
	ASN1_SIMPLE(GOST_KEY_INFO, encrypted_key, ASN1_OCTET_STRING),
	ASN1_SIMPLE(GOST_KEY_INFO, imit, ASN1_OCTET_STRING)
} ASN1_NDEF_SEQUENCE_END(GOST_KEY_INFO)

GOST_KEY_INFO *
d2i_GOST_KEY_INFO(GOST_KEY_INFO **a, const unsigned char **in, long len)
{
	return (GOST_KEY_INFO *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GOST_KEY_INFO_it);
}

int
i2d_GOST_KEY_INFO(GOST_KEY_INFO *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GOST_KEY_INFO_it);
}

GOST_KEY_INFO *
GOST_KEY_INFO_new(void)
{
	return (GOST_KEY_INFO *)ASN1_item_new(&GOST_KEY_INFO_it);
}

void
GOST_KEY_INFO_free(GOST_KEY_INFO *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GOST_KEY_INFO_it);
}

ASN1_NDEF_SEQUENCE(GOST_KEY_AGREEMENT_INFO) = {
	ASN1_SIMPLE(GOST_KEY_AGREEMENT_INFO, cipher, ASN1_OBJECT),
	ASN1_IMP_OPT(GOST_KEY_AGREEMENT_INFO, ephem_key, X509_PUBKEY, 0),
	ASN1_SIMPLE(GOST_KEY_AGREEMENT_INFO, eph_iv, ASN1_OCTET_STRING)
} ASN1_NDEF_SEQUENCE_END(GOST_KEY_AGREEMENT_INFO)

GOST_KEY_AGREEMENT_INFO *
d2i_GOST_KEY_AGREEMENT_INFO(GOST_KEY_AGREEMENT_INFO **a, const unsigned char **in, long len)
{
	return (GOST_KEY_AGREEMENT_INFO *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GOST_KEY_AGREEMENT_INFO_it);
}

int
i2d_GOST_KEY_AGREEMENT_INFO(GOST_KEY_AGREEMENT_INFO *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GOST_KEY_AGREEMENT_INFO_it);
}

GOST_KEY_AGREEMENT_INFO *
GOST_KEY_AGREEMENT_INFO_new(void)
{
	return (GOST_KEY_AGREEMENT_INFO *)ASN1_item_new(&GOST_KEY_AGREEMENT_INFO_it);
}

void
GOST_KEY_AGREEMENT_INFO_free(GOST_KEY_AGREEMENT_INFO *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GOST_KEY_AGREEMENT_INFO_it);
}


ASN1_NDEF_SEQUENCE(GOST_KEY_PARAMS) = {
	ASN1_SIMPLE(GOST_KEY_PARAMS, key_params, ASN1_OBJECT),
	ASN1_SIMPLE(GOST_KEY_PARAMS, hash_params, ASN1_OBJECT),
	ASN1_OPT(GOST_KEY_PARAMS, cipher_params, ASN1_OBJECT),
} ASN1_NDEF_SEQUENCE_END(GOST_KEY_PARAMS)

GOST_KEY_PARAMS *
d2i_GOST_KEY_PARAMS(GOST_KEY_PARAMS **a, const unsigned char **in, long len)
{
	return (GOST_KEY_PARAMS *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GOST_KEY_PARAMS_it);
}

int
i2d_GOST_KEY_PARAMS(GOST_KEY_PARAMS *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GOST_KEY_PARAMS_it);
}

GOST_KEY_PARAMS *
GOST_KEY_PARAMS_new(void)
{
	return (GOST_KEY_PARAMS *)ASN1_item_new(&GOST_KEY_PARAMS_it);
}

void
GOST_KEY_PARAMS_free(GOST_KEY_PARAMS *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GOST_KEY_PARAMS_it);
}

ASN1_NDEF_SEQUENCE(GOST_CIPHER_PARAMS) = {
	ASN1_SIMPLE(GOST_CIPHER_PARAMS, iv, ASN1_OCTET_STRING),
	ASN1_SIMPLE(GOST_CIPHER_PARAMS, enc_param_set, ASN1_OBJECT),
} ASN1_NDEF_SEQUENCE_END(GOST_CIPHER_PARAMS)

GOST_CIPHER_PARAMS *
d2i_GOST_CIPHER_PARAMS(GOST_CIPHER_PARAMS **a, const unsigned char **in, long len)
{
	return (GOST_CIPHER_PARAMS *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GOST_CIPHER_PARAMS_it);
}

int
i2d_GOST_CIPHER_PARAMS(GOST_CIPHER_PARAMS *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GOST_CIPHER_PARAMS_it);
}

GOST_CIPHER_PARAMS *
GOST_CIPHER_PARAMS_new(void)
{
	return (GOST_CIPHER_PARAMS *)ASN1_item_new(&GOST_CIPHER_PARAMS_it);
}

void
GOST_CIPHER_PARAMS_free(GOST_CIPHER_PARAMS *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GOST_CIPHER_PARAMS_it);
}

#endif
