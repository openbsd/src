/* $OpenBSD: ts_local.h,v 1.1 2022/07/24 08:16:47 tb Exp $ */
/* Written by Zoltan Glozik (zglozik@opentsa.org) for the OpenSSL
 * project 2002, 2003, 2004.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_TS_LOCAL_H
#define HEADER_TS_LOCAL_H

__BEGIN_HIDDEN_DECLS

/*
 * ESSCertIDv2 ::=  SEQUENCE {
 *     hashAlgorithm           AlgorithmIdentifier
 *            DEFAULT {algorithm id-sha256},
 *     certHash                 Hash,
 *     issuerSerial             IssuerSerial OPTIONAL }
 */

struct ESS_cert_id_v2 {
	X509_ALGOR *hash_alg;	/* Default SHA-256. */
	ASN1_OCTET_STRING *hash;
	ESS_ISSUER_SERIAL *issuer_serial;
};

/*
 * SigningCertificateV2 ::=  SEQUENCE {
 *     certs        SEQUENCE OF ESSCertIDv2,
 *     policies     SEQUENCE OF PolicyInformation OPTIONAL }
 */

struct ESS_signing_cert_v2 {
	STACK_OF(ESS_CERT_ID_V2) *cert_ids;
	STACK_OF(POLICYINFO) *policy_info;
};

/*
 * Public OpenSSL API that we do not currently want to expose.
 */

ESS_CERT_ID_V2 *ESS_CERT_ID_V2_new(void);
void ESS_CERT_ID_V2_free(ESS_CERT_ID_V2 *a);
int i2d_ESS_CERT_ID_V2(const ESS_CERT_ID_V2 *a, unsigned char **pp);
ESS_CERT_ID_V2 *d2i_ESS_CERT_ID_V2(ESS_CERT_ID_V2 **a, const unsigned char **pp,
    long length);
ESS_CERT_ID_V2 *ESS_CERT_ID_V2_dup(ESS_CERT_ID_V2 *a);

ESS_SIGNING_CERT_V2 *ESS_SIGNING_CERT_V2_new(void);
void ESS_SIGNING_CERT_V2_free(ESS_SIGNING_CERT_V2 *a);
int i2d_ESS_SIGNING_CERT_V2(const ESS_SIGNING_CERT_V2 *a,
    unsigned char **pp);
ESS_SIGNING_CERT_V2 *d2i_ESS_SIGNING_CERT_V2(ESS_SIGNING_CERT_V2 **a,
    const unsigned char **pp, long length);
ESS_SIGNING_CERT_V2 *ESS_SIGNING_CERT_V2_dup(ESS_SIGNING_CERT_V2 *a);

__END_HIDDEN_DECLS

#endif /* HEADER_TS_LOCAL_H */
