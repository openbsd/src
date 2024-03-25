/* $OpenBSD: x509_trs.c,v 1.52 2024/03/25 02:18:35 tb Exp $ */
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

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "crypto_internal.h"
#include "x509_internal.h"
#include "x509_local.h"

typedef struct x509_trust_st {
	int trust;
	int (*check_trust)(int, const X509 *);
	int nid;
} X509_TRUST;

static int
obj_trust(int id, const X509 *x)
{
	ASN1_OBJECT *obj;
	int i, nid;
	const X509_CERT_AUX *aux;

	if ((aux = x->aux) == NULL)
		return X509_TRUST_UNTRUSTED;

	for (i = 0; i < sk_ASN1_OBJECT_num(aux->reject); i++) {
		obj = sk_ASN1_OBJECT_value(aux->reject, i);
		nid = OBJ_obj2nid(obj);
		if (nid == id || nid == NID_anyExtendedKeyUsage)
			return X509_TRUST_REJECTED;
	}

	for (i = 0; i < sk_ASN1_OBJECT_num(aux->trust); i++) {
		obj = sk_ASN1_OBJECT_value(aux->trust, i);
		nid = OBJ_obj2nid(obj);
		if (nid == id || nid == NID_anyExtendedKeyUsage)
			return X509_TRUST_TRUSTED;
	}

	return X509_TRUST_UNTRUSTED;
}

static int
trust_compat(int nid, const X509 *x)
{
	/* Extensions already cached in X509_check_trust(). */
	if (x->ex_flags & EXFLAG_SS)
		return X509_TRUST_TRUSTED;
	else
		return X509_TRUST_UNTRUSTED;
}

static int
trust_1oidany(int nid, const X509 *x)
{
	if (x->aux && (x->aux->trust || x->aux->reject))
		return obj_trust(nid, x);
	/* we don't have any trust settings: for compatibility
	 * we return trusted if it is self signed
	 */
	return trust_compat(NID_undef, x);
}

static int
trust_1oid(int nid, const X509 *x)
{
	if (x->aux)
		return obj_trust(nid, x);
	return X509_TRUST_UNTRUSTED;
}

/* WARNING: the following table should be kept in order of trust
 * and without any gaps so we can just subtract the minimum trust
 * value to get an index into the table
 */

static const X509_TRUST trstandard[] = {
	{
		.trust = X509_TRUST_COMPAT,
		.check_trust = trust_compat,
	},
	{
		.trust = X509_TRUST_SSL_CLIENT,
		.check_trust = trust_1oidany,
		.nid = NID_client_auth,
	},
	{
		.trust = X509_TRUST_SSL_SERVER,
		.check_trust = trust_1oidany,
		.nid = NID_server_auth,
	},
	{
		.trust = X509_TRUST_EMAIL,
		.check_trust = trust_1oidany,
		.nid = NID_email_protect,
	},
	{
		.trust = X509_TRUST_OBJECT_SIGN,
		.check_trust = trust_1oidany,
		.nid = NID_code_sign,
	},
	{
		.trust = X509_TRUST_OCSP_SIGN,
		.check_trust = trust_1oid,
		.nid = NID_OCSP_sign,
	},
	{
		.trust = X509_TRUST_OCSP_REQUEST,
		.check_trust = trust_1oid,
		.nid = NID_ad_OCSP,
	},
	{
		.trust = X509_TRUST_TSA,
		.check_trust = trust_1oidany,
		.nid = NID_time_stamp,
	},
};

#define X509_TRUST_COUNT	(sizeof(trstandard) / sizeof(trstandard[0]))

CTASSERT(X509_TRUST_MIN == 1 && X509_TRUST_MAX == X509_TRUST_COUNT);

int
X509_check_trust(X509 *x, int trust_id, int flags)
{
	const X509_TRUST *trust;
	int idx;

	if (trust_id == -1)
		return 1;

	/* Call early so the trust handlers don't need to modify the certs. */
	if (!x509v3_cache_extensions(x))
		return X509_TRUST_UNTRUSTED;

	/*
	 * XXX beck/jsing This enables self signed certs to be trusted for
	 * an unspecified id/trust flag value (this is NOT the
	 * X509_TRUST_DEFAULT), which was the longstanding
	 * openssl behaviour. boringssl does not have this behaviour.
	 *
	 * This should be revisited, but changing the default "not default"
	 * may break things.
	 */
	if (trust_id == 0) {
		int rv;
		rv = obj_trust(NID_anyExtendedKeyUsage, x);
		if (rv != X509_TRUST_UNTRUSTED)
			return rv;
		return trust_compat(NID_undef, x);
	}

	if (trust_id < X509_TRUST_MIN || trust_id > X509_TRUST_MAX)
		return obj_trust(trust_id, x);

	idx = trust_id - X509_TRUST_MIN;
	trust = &trstandard[idx];

	return trust->check_trust(trust->nid, x);
}
LCRYPTO_ALIAS(X509_check_trust);
