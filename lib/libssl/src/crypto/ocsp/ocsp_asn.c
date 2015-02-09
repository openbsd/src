/* $OpenBSD: ocsp_asn.c,v 1.7 2015/02/09 16:04:46 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/ocsp.h>

ASN1_SEQUENCE(OCSP_SIGNATURE) = {
	ASN1_SIMPLE(OCSP_SIGNATURE, signatureAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OCSP_SIGNATURE, signature, ASN1_BIT_STRING),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_SIGNATURE, certs, X509, 0)
} ASN1_SEQUENCE_END(OCSP_SIGNATURE)


OCSP_SIGNATURE *
d2i_OCSP_SIGNATURE(OCSP_SIGNATURE **a, const unsigned char **in, long len)
{
	return (OCSP_SIGNATURE *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_SIGNATURE_it);
}

int
i2d_OCSP_SIGNATURE(OCSP_SIGNATURE *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_SIGNATURE_it);
}

OCSP_SIGNATURE *
OCSP_SIGNATURE_new(void)
{
	return (OCSP_SIGNATURE *)ASN1_item_new(&OCSP_SIGNATURE_it);
}

void
OCSP_SIGNATURE_free(OCSP_SIGNATURE *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_SIGNATURE_it);
}

ASN1_SEQUENCE(OCSP_CERTID) = {
	ASN1_SIMPLE(OCSP_CERTID, hashAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OCSP_CERTID, issuerNameHash, ASN1_OCTET_STRING),
	ASN1_SIMPLE(OCSP_CERTID, issuerKeyHash, ASN1_OCTET_STRING),
	ASN1_SIMPLE(OCSP_CERTID, serialNumber, ASN1_INTEGER)
} ASN1_SEQUENCE_END(OCSP_CERTID)


OCSP_CERTID *
d2i_OCSP_CERTID(OCSP_CERTID **a, const unsigned char **in, long len)
{
	return (OCSP_CERTID *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_CERTID_it);
}

int
i2d_OCSP_CERTID(OCSP_CERTID *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_CERTID_it);
}

OCSP_CERTID *
OCSP_CERTID_new(void)
{
	return (OCSP_CERTID *)ASN1_item_new(&OCSP_CERTID_it);
}

void
OCSP_CERTID_free(OCSP_CERTID *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_CERTID_it);
}

ASN1_SEQUENCE(OCSP_ONEREQ) = {
	ASN1_SIMPLE(OCSP_ONEREQ, reqCert, OCSP_CERTID),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_ONEREQ, singleRequestExtensions, X509_EXTENSION, 0)
} ASN1_SEQUENCE_END(OCSP_ONEREQ)


OCSP_ONEREQ *
d2i_OCSP_ONEREQ(OCSP_ONEREQ **a, const unsigned char **in, long len)
{
	return (OCSP_ONEREQ *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_ONEREQ_it);
}

int
i2d_OCSP_ONEREQ(OCSP_ONEREQ *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_ONEREQ_it);
}

OCSP_ONEREQ *
OCSP_ONEREQ_new(void)
{
	return (OCSP_ONEREQ *)ASN1_item_new(&OCSP_ONEREQ_it);
}

void
OCSP_ONEREQ_free(OCSP_ONEREQ *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_ONEREQ_it);
}

ASN1_SEQUENCE(OCSP_REQINFO) = {
	ASN1_EXP_OPT(OCSP_REQINFO, version, ASN1_INTEGER, 0),
	ASN1_EXP_OPT(OCSP_REQINFO, requestorName, GENERAL_NAME, 1),
	ASN1_SEQUENCE_OF(OCSP_REQINFO, requestList, OCSP_ONEREQ),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_REQINFO, requestExtensions, X509_EXTENSION, 2)
} ASN1_SEQUENCE_END(OCSP_REQINFO)


OCSP_REQINFO *
d2i_OCSP_REQINFO(OCSP_REQINFO **a, const unsigned char **in, long len)
{
	return (OCSP_REQINFO *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_REQINFO_it);
}

int
i2d_OCSP_REQINFO(OCSP_REQINFO *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_REQINFO_it);
}

OCSP_REQINFO *
OCSP_REQINFO_new(void)
{
	return (OCSP_REQINFO *)ASN1_item_new(&OCSP_REQINFO_it);
}

void
OCSP_REQINFO_free(OCSP_REQINFO *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_REQINFO_it);
}

ASN1_SEQUENCE(OCSP_REQUEST) = {
	ASN1_SIMPLE(OCSP_REQUEST, tbsRequest, OCSP_REQINFO),
	ASN1_EXP_OPT(OCSP_REQUEST, optionalSignature, OCSP_SIGNATURE, 0)
} ASN1_SEQUENCE_END(OCSP_REQUEST)


OCSP_REQUEST *
d2i_OCSP_REQUEST(OCSP_REQUEST **a, const unsigned char **in, long len)
{
	return (OCSP_REQUEST *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_REQUEST_it);
}

int
i2d_OCSP_REQUEST(OCSP_REQUEST *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_REQUEST_it);
}

OCSP_REQUEST *
OCSP_REQUEST_new(void)
{
	return (OCSP_REQUEST *)ASN1_item_new(&OCSP_REQUEST_it);
}

void
OCSP_REQUEST_free(OCSP_REQUEST *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_REQUEST_it);
}

/* OCSP_RESPONSE templates */

ASN1_SEQUENCE(OCSP_RESPBYTES) = {
	ASN1_SIMPLE(OCSP_RESPBYTES, responseType, ASN1_OBJECT),
	ASN1_SIMPLE(OCSP_RESPBYTES, response, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OCSP_RESPBYTES)


OCSP_RESPBYTES *
d2i_OCSP_RESPBYTES(OCSP_RESPBYTES **a, const unsigned char **in, long len)
{
	return (OCSP_RESPBYTES *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_RESPBYTES_it);
}

int
i2d_OCSP_RESPBYTES(OCSP_RESPBYTES *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_RESPBYTES_it);
}

OCSP_RESPBYTES *
OCSP_RESPBYTES_new(void)
{
	return (OCSP_RESPBYTES *)ASN1_item_new(&OCSP_RESPBYTES_it);
}

void
OCSP_RESPBYTES_free(OCSP_RESPBYTES *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_RESPBYTES_it);
}

ASN1_SEQUENCE(OCSP_RESPONSE) = {
	ASN1_SIMPLE(OCSP_RESPONSE, responseStatus, ASN1_ENUMERATED),
	ASN1_EXP_OPT(OCSP_RESPONSE, responseBytes, OCSP_RESPBYTES, 0)
} ASN1_SEQUENCE_END(OCSP_RESPONSE)


OCSP_RESPONSE *
d2i_OCSP_RESPONSE(OCSP_RESPONSE **a, const unsigned char **in, long len)
{
	return (OCSP_RESPONSE *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_RESPONSE_it);
}

int
i2d_OCSP_RESPONSE(OCSP_RESPONSE *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_RESPONSE_it);
}

OCSP_RESPONSE *
OCSP_RESPONSE_new(void)
{
	return (OCSP_RESPONSE *)ASN1_item_new(&OCSP_RESPONSE_it);
}

void
OCSP_RESPONSE_free(OCSP_RESPONSE *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_RESPONSE_it);
}

ASN1_CHOICE(OCSP_RESPID) = {
	ASN1_EXP(OCSP_RESPID, value.byName, X509_NAME, 1),
	ASN1_EXP(OCSP_RESPID, value.byKey, ASN1_OCTET_STRING, 2)
} ASN1_CHOICE_END(OCSP_RESPID)


OCSP_RESPID *
d2i_OCSP_RESPID(OCSP_RESPID **a, const unsigned char **in, long len)
{
	return (OCSP_RESPID *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_RESPID_it);
}

int
i2d_OCSP_RESPID(OCSP_RESPID *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_RESPID_it);
}

OCSP_RESPID *
OCSP_RESPID_new(void)
{
	return (OCSP_RESPID *)ASN1_item_new(&OCSP_RESPID_it);
}

void
OCSP_RESPID_free(OCSP_RESPID *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_RESPID_it);
}

ASN1_SEQUENCE(OCSP_REVOKEDINFO) = {
	ASN1_SIMPLE(OCSP_REVOKEDINFO, revocationTime, ASN1_GENERALIZEDTIME),
	ASN1_EXP_OPT(OCSP_REVOKEDINFO, revocationReason, ASN1_ENUMERATED, 0)
} ASN1_SEQUENCE_END(OCSP_REVOKEDINFO)


OCSP_REVOKEDINFO *
d2i_OCSP_REVOKEDINFO(OCSP_REVOKEDINFO **a, const unsigned char **in, long len)
{
	return (OCSP_REVOKEDINFO *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_REVOKEDINFO_it);
}

int
i2d_OCSP_REVOKEDINFO(OCSP_REVOKEDINFO *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_REVOKEDINFO_it);
}

OCSP_REVOKEDINFO *
OCSP_REVOKEDINFO_new(void)
{
	return (OCSP_REVOKEDINFO *)ASN1_item_new(&OCSP_REVOKEDINFO_it);
}

void
OCSP_REVOKEDINFO_free(OCSP_REVOKEDINFO *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_REVOKEDINFO_it);
}

ASN1_CHOICE(OCSP_CERTSTATUS) = {
	ASN1_IMP(OCSP_CERTSTATUS, value.good, ASN1_NULL, 0),
	ASN1_IMP(OCSP_CERTSTATUS, value.revoked, OCSP_REVOKEDINFO, 1),
	ASN1_IMP(OCSP_CERTSTATUS, value.unknown, ASN1_NULL, 2)
} ASN1_CHOICE_END(OCSP_CERTSTATUS)


OCSP_CERTSTATUS *
d2i_OCSP_CERTSTATUS(OCSP_CERTSTATUS **a, const unsigned char **in, long len)
{
	return (OCSP_CERTSTATUS *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_CERTSTATUS_it);
}

int
i2d_OCSP_CERTSTATUS(OCSP_CERTSTATUS *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_CERTSTATUS_it);
}

OCSP_CERTSTATUS *
OCSP_CERTSTATUS_new(void)
{
	return (OCSP_CERTSTATUS *)ASN1_item_new(&OCSP_CERTSTATUS_it);
}

void
OCSP_CERTSTATUS_free(OCSP_CERTSTATUS *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_CERTSTATUS_it);
}

ASN1_SEQUENCE(OCSP_SINGLERESP) = {
	ASN1_SIMPLE(OCSP_SINGLERESP, certId, OCSP_CERTID),
	ASN1_SIMPLE(OCSP_SINGLERESP, certStatus, OCSP_CERTSTATUS),
	ASN1_SIMPLE(OCSP_SINGLERESP, thisUpdate, ASN1_GENERALIZEDTIME),
	ASN1_EXP_OPT(OCSP_SINGLERESP, nextUpdate, ASN1_GENERALIZEDTIME, 0),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_SINGLERESP, singleExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(OCSP_SINGLERESP)


OCSP_SINGLERESP *
d2i_OCSP_SINGLERESP(OCSP_SINGLERESP **a, const unsigned char **in, long len)
{
	return (OCSP_SINGLERESP *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_SINGLERESP_it);
}

int
i2d_OCSP_SINGLERESP(OCSP_SINGLERESP *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_SINGLERESP_it);
}

OCSP_SINGLERESP *
OCSP_SINGLERESP_new(void)
{
	return (OCSP_SINGLERESP *)ASN1_item_new(&OCSP_SINGLERESP_it);
}

void
OCSP_SINGLERESP_free(OCSP_SINGLERESP *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_SINGLERESP_it);
}

ASN1_SEQUENCE(OCSP_RESPDATA) = {
	ASN1_EXP_OPT(OCSP_RESPDATA, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(OCSP_RESPDATA, responderId, OCSP_RESPID),
	ASN1_SIMPLE(OCSP_RESPDATA, producedAt, ASN1_GENERALIZEDTIME),
	ASN1_SEQUENCE_OF(OCSP_RESPDATA, responses, OCSP_SINGLERESP),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_RESPDATA, responseExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(OCSP_RESPDATA)


OCSP_RESPDATA *
d2i_OCSP_RESPDATA(OCSP_RESPDATA **a, const unsigned char **in, long len)
{
	return (OCSP_RESPDATA *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_RESPDATA_it);
}

int
i2d_OCSP_RESPDATA(OCSP_RESPDATA *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_RESPDATA_it);
}

OCSP_RESPDATA *
OCSP_RESPDATA_new(void)
{
	return (OCSP_RESPDATA *)ASN1_item_new(&OCSP_RESPDATA_it);
}

void
OCSP_RESPDATA_free(OCSP_RESPDATA *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_RESPDATA_it);
}

ASN1_SEQUENCE(OCSP_BASICRESP) = {
	ASN1_SIMPLE(OCSP_BASICRESP, tbsResponseData, OCSP_RESPDATA),
	ASN1_SIMPLE(OCSP_BASICRESP, signatureAlgorithm, X509_ALGOR),
	ASN1_SIMPLE(OCSP_BASICRESP, signature, ASN1_BIT_STRING),
	ASN1_EXP_SEQUENCE_OF_OPT(OCSP_BASICRESP, certs, X509, 0)
} ASN1_SEQUENCE_END(OCSP_BASICRESP)


OCSP_BASICRESP *
d2i_OCSP_BASICRESP(OCSP_BASICRESP **a, const unsigned char **in, long len)
{
	return (OCSP_BASICRESP *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_BASICRESP_it);
}

int
i2d_OCSP_BASICRESP(OCSP_BASICRESP *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_BASICRESP_it);
}

OCSP_BASICRESP *
OCSP_BASICRESP_new(void)
{
	return (OCSP_BASICRESP *)ASN1_item_new(&OCSP_BASICRESP_it);
}

void
OCSP_BASICRESP_free(OCSP_BASICRESP *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_BASICRESP_it);
}

ASN1_SEQUENCE(OCSP_CRLID) = {
	ASN1_EXP_OPT(OCSP_CRLID, crlUrl, ASN1_IA5STRING, 0),
	ASN1_EXP_OPT(OCSP_CRLID, crlNum, ASN1_INTEGER, 1),
	ASN1_EXP_OPT(OCSP_CRLID, crlTime, ASN1_GENERALIZEDTIME, 2)
} ASN1_SEQUENCE_END(OCSP_CRLID)


OCSP_CRLID *
d2i_OCSP_CRLID(OCSP_CRLID **a, const unsigned char **in, long len)
{
	return (OCSP_CRLID *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_CRLID_it);
}

int
i2d_OCSP_CRLID(OCSP_CRLID *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_CRLID_it);
}

OCSP_CRLID *
OCSP_CRLID_new(void)
{
	return (OCSP_CRLID *)ASN1_item_new(&OCSP_CRLID_it);
}

void
OCSP_CRLID_free(OCSP_CRLID *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_CRLID_it);
}

ASN1_SEQUENCE(OCSP_SERVICELOC) = {
	ASN1_SIMPLE(OCSP_SERVICELOC, issuer, X509_NAME),
	ASN1_SEQUENCE_OF_OPT(OCSP_SERVICELOC, locator, ACCESS_DESCRIPTION)
} ASN1_SEQUENCE_END(OCSP_SERVICELOC)


OCSP_SERVICELOC *
d2i_OCSP_SERVICELOC(OCSP_SERVICELOC **a, const unsigned char **in, long len)
{
	return (OCSP_SERVICELOC *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OCSP_SERVICELOC_it);
}

int
i2d_OCSP_SERVICELOC(OCSP_SERVICELOC *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OCSP_SERVICELOC_it);
}

OCSP_SERVICELOC *
OCSP_SERVICELOC_new(void)
{
	return (OCSP_SERVICELOC *)ASN1_item_new(&OCSP_SERVICELOC_it);
}

void
OCSP_SERVICELOC_free(OCSP_SERVICELOC *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OCSP_SERVICELOC_it);
}
