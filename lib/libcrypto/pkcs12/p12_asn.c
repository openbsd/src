/* $OpenBSD: p12_asn.c,v 1.7 2015/02/09 16:04:46 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>

#include <openssl/asn1t.h>
#include <openssl/pkcs12.h>

/* PKCS#12 ASN1 module */

ASN1_SEQUENCE(PKCS12) = {
	ASN1_SIMPLE(PKCS12, version, ASN1_INTEGER),
	ASN1_SIMPLE(PKCS12, authsafes, PKCS7),
	ASN1_OPT(PKCS12, mac, PKCS12_MAC_DATA)
} ASN1_SEQUENCE_END(PKCS12)


PKCS12 *
d2i_PKCS12(PKCS12 **a, const unsigned char **in, long len)
{
	return (PKCS12 *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &PKCS12_it);
}

int
i2d_PKCS12(PKCS12 *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &PKCS12_it);
}

PKCS12 *
PKCS12_new(void)
{
	return (PKCS12 *)ASN1_item_new(&PKCS12_it);
}

void
PKCS12_free(PKCS12 *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &PKCS12_it);
}

ASN1_SEQUENCE(PKCS12_MAC_DATA) = {
	ASN1_SIMPLE(PKCS12_MAC_DATA, dinfo, X509_SIG),
	ASN1_SIMPLE(PKCS12_MAC_DATA, salt, ASN1_OCTET_STRING),
	ASN1_OPT(PKCS12_MAC_DATA, iter, ASN1_INTEGER)
} ASN1_SEQUENCE_END(PKCS12_MAC_DATA)


PKCS12_MAC_DATA *
d2i_PKCS12_MAC_DATA(PKCS12_MAC_DATA **a, const unsigned char **in, long len)
{
	return (PKCS12_MAC_DATA *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &PKCS12_MAC_DATA_it);
}

int
i2d_PKCS12_MAC_DATA(PKCS12_MAC_DATA *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &PKCS12_MAC_DATA_it);
}

PKCS12_MAC_DATA *
PKCS12_MAC_DATA_new(void)
{
	return (PKCS12_MAC_DATA *)ASN1_item_new(&PKCS12_MAC_DATA_it);
}

void
PKCS12_MAC_DATA_free(PKCS12_MAC_DATA *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &PKCS12_MAC_DATA_it);
}

ASN1_ADB_TEMPLATE(bag_default) =
    ASN1_EXP(PKCS12_BAGS, value.other, ASN1_ANY, 0);

ASN1_ADB(PKCS12_BAGS) = {
	ADB_ENTRY(NID_x509Certificate,
	    ASN1_EXP(PKCS12_BAGS, value.x509cert, ASN1_OCTET_STRING, 0)),
	ADB_ENTRY(NID_x509Crl,
	    ASN1_EXP(PKCS12_BAGS, value.x509crl, ASN1_OCTET_STRING, 0)),
	ADB_ENTRY(NID_sdsiCertificate,
	    ASN1_EXP(PKCS12_BAGS, value.sdsicert, ASN1_IA5STRING, 0)),
} ASN1_ADB_END(PKCS12_BAGS, 0, type, 0, &bag_default_tt, NULL);

ASN1_SEQUENCE(PKCS12_BAGS) = {
	ASN1_SIMPLE(PKCS12_BAGS, type, ASN1_OBJECT),
	ASN1_ADB_OBJECT(PKCS12_BAGS),
} ASN1_SEQUENCE_END(PKCS12_BAGS)


PKCS12_BAGS *
d2i_PKCS12_BAGS(PKCS12_BAGS **a, const unsigned char **in, long len)
{
	return (PKCS12_BAGS *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &PKCS12_BAGS_it);
}

int
i2d_PKCS12_BAGS(PKCS12_BAGS *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &PKCS12_BAGS_it);
}

PKCS12_BAGS *
PKCS12_BAGS_new(void)
{
	return (PKCS12_BAGS *)ASN1_item_new(&PKCS12_BAGS_it);
}

void
PKCS12_BAGS_free(PKCS12_BAGS *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &PKCS12_BAGS_it);
}

ASN1_ADB_TEMPLATE(safebag_default) =
    ASN1_EXP(PKCS12_SAFEBAG, value.other, ASN1_ANY, 0);

ASN1_ADB(PKCS12_SAFEBAG) = {
	ADB_ENTRY(NID_keyBag,
	    ASN1_EXP(PKCS12_SAFEBAG, value.keybag, PKCS8_PRIV_KEY_INFO, 0)),
	ADB_ENTRY(NID_pkcs8ShroudedKeyBag,
	    ASN1_EXP(PKCS12_SAFEBAG, value.shkeybag, X509_SIG, 0)),
	ADB_ENTRY(NID_safeContentsBag,
	    ASN1_EXP_SET_OF(PKCS12_SAFEBAG, value.safes, PKCS12_SAFEBAG, 0)),
	ADB_ENTRY(NID_certBag,
	    ASN1_EXP(PKCS12_SAFEBAG, value.bag, PKCS12_BAGS, 0)),
	ADB_ENTRY(NID_crlBag,
	    ASN1_EXP(PKCS12_SAFEBAG, value.bag, PKCS12_BAGS, 0)),
	ADB_ENTRY(NID_secretBag,
	    ASN1_EXP(PKCS12_SAFEBAG, value.bag, PKCS12_BAGS, 0))
} ASN1_ADB_END(PKCS12_SAFEBAG, 0, type, 0, &safebag_default_tt, NULL);

ASN1_SEQUENCE(PKCS12_SAFEBAG) = {
	ASN1_SIMPLE(PKCS12_SAFEBAG, type, ASN1_OBJECT),
	ASN1_ADB_OBJECT(PKCS12_SAFEBAG),
	ASN1_SET_OF_OPT(PKCS12_SAFEBAG, attrib, X509_ATTRIBUTE)
} ASN1_SEQUENCE_END(PKCS12_SAFEBAG)


PKCS12_SAFEBAG *
d2i_PKCS12_SAFEBAG(PKCS12_SAFEBAG **a, const unsigned char **in, long len)
{
	return (PKCS12_SAFEBAG *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &PKCS12_SAFEBAG_it);
}

int
i2d_PKCS12_SAFEBAG(PKCS12_SAFEBAG *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &PKCS12_SAFEBAG_it);
}

PKCS12_SAFEBAG *
PKCS12_SAFEBAG_new(void)
{
	return (PKCS12_SAFEBAG *)ASN1_item_new(&PKCS12_SAFEBAG_it);
}

void
PKCS12_SAFEBAG_free(PKCS12_SAFEBAG *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &PKCS12_SAFEBAG_it);
}

/* SEQUENCE OF SafeBag */
ASN1_ITEM_TEMPLATE(PKCS12_SAFEBAGS) =
ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, PKCS12_SAFEBAGS, PKCS12_SAFEBAG)
ASN1_ITEM_TEMPLATE_END(PKCS12_SAFEBAGS)

/* Authsafes: SEQUENCE OF PKCS7 */
ASN1_ITEM_TEMPLATE(PKCS12_AUTHSAFES) =
ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, PKCS12_AUTHSAFES, PKCS7)
ASN1_ITEM_TEMPLATE_END(PKCS12_AUTHSAFES)

