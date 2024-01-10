/* $OpenBSD: x509_trs.c,v 1.39 2024/01/10 21:34:53 tb Exp $ */
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

#include "x509_local.h"

static int
obj_trust(int id, X509 *x, int flags)
{
	ASN1_OBJECT *obj;
	int i, nid;
	X509_CERT_AUX *ax;

	ax = x->aux;
	if (!ax)
		return X509_TRUST_UNTRUSTED;
	if (ax->reject) {
		for (i = 0; i < sk_ASN1_OBJECT_num(ax->reject); i++) {
			obj = sk_ASN1_OBJECT_value(ax->reject, i);
			nid = OBJ_obj2nid(obj);
			if (nid == id || nid == NID_anyExtendedKeyUsage)
				return X509_TRUST_REJECTED;
		}
	}
	if (ax->trust) {
		for (i = 0; i < sk_ASN1_OBJECT_num(ax->trust); i++) {
			obj = sk_ASN1_OBJECT_value(ax->trust, i);
			nid = OBJ_obj2nid(obj);
			if (nid == id || nid == NID_anyExtendedKeyUsage)
				return X509_TRUST_TRUSTED;
		}
	}
	return X509_TRUST_UNTRUSTED;
}

static int
trust_compat(X509_TRUST *trust, X509 *x, int flags)
{
	X509_check_purpose(x, -1, 0);
	if (x->ex_flags & EXFLAG_SS)
		return X509_TRUST_TRUSTED;
	else
		return X509_TRUST_UNTRUSTED;
}

static int
trust_1oidany(X509_TRUST *trust, X509 *x, int flags)
{
	if (x->aux && (x->aux->trust || x->aux->reject))
		return obj_trust(trust->arg1, x, flags);
	/* we don't have any trust settings: for compatibility
	 * we return trusted if it is self signed
	 */
	return trust_compat(trust, x, flags);
}

static int
trust_1oid(X509_TRUST *trust, X509 *x, int flags)
{
	if (x->aux)
		return obj_trust(trust->arg1, x, flags);
	return X509_TRUST_UNTRUSTED;
}

/* WARNING: the following table should be kept in order of trust
 * and without any gaps so we can just subtract the minimum trust
 * value to get an index into the table
 */

static X509_TRUST trstandard[] = {
	{
		.trust = X509_TRUST_COMPAT,
		.check_trust = trust_compat,
		.name = "compatible",
	},
	{
		.trust = X509_TRUST_SSL_CLIENT,
		.check_trust = trust_1oidany,
		.name = "SSL Client",
		.arg1 = NID_client_auth,
	},
	{
		.trust = X509_TRUST_SSL_SERVER,
		.check_trust = trust_1oidany,
		.name = "SSL Server",
		.arg1 = NID_server_auth,
	},
	{
		.trust = X509_TRUST_EMAIL,
		.check_trust = trust_1oidany,
		.name = "S/MIME email",
		.arg1 = NID_email_protect,
	},
	{
		.trust = X509_TRUST_OBJECT_SIGN,
		.check_trust = trust_1oidany,
		.name = "Object Signer",
		.arg1 = NID_code_sign,
	},
	{
		.trust = X509_TRUST_OCSP_SIGN,
		.check_trust = trust_1oid,
		.name = "OCSP responder",
		.arg1 = NID_OCSP_sign,
	},
	{
		.trust = X509_TRUST_OCSP_REQUEST,
		.check_trust = trust_1oid,
		.name = "OCSP request",
		.arg1 = NID_ad_OCSP,
	},
	{
		.trust = X509_TRUST_TSA,
		.check_trust = trust_1oidany,
		.name = "TSA server",
		.arg1 = NID_time_stamp,
	},
};

#define X509_TRUST_COUNT	(sizeof(trstandard) / sizeof(trstandard[0]))

static int (*default_trust)(int id, X509 *x, int flags) = obj_trust;

int
(*X509_TRUST_set_default(int (*trust)(int , X509 *, int)))(int, X509 *, int)
{
	int (*oldtrust)(int , X509 *, int);

	oldtrust = default_trust;
	default_trust = trust;
	return oldtrust;
}
LCRYPTO_ALIAS(X509_TRUST_set_default);

int
X509_check_trust(X509 *x, int id, int flags)
{
	X509_TRUST *pt;
	int idx;

	if (id == -1)
		return 1;
	/*
	 * XXX beck/jsing This enables self signed certs to be trusted for
	 * an unspecified id/trust flag value (this is NOT the
	 * X509_TRUST_DEFAULT), which was the longstanding
	 * openssl behaviour. boringssl does not have this behaviour.
	 *
	 * This should be revisited, but changing the default "not default"
	 * may break things.
	 */
	if (id == 0) {
		int rv;
		rv = obj_trust(NID_anyExtendedKeyUsage, x, 0);
		if (rv != X509_TRUST_UNTRUSTED)
			return rv;
		return trust_compat(NULL, x, 0);
	}
	idx = X509_TRUST_get_by_id(id);
	if (idx == -1)
		return default_trust(id, x, flags);
	pt = X509_TRUST_get0(idx);
	return pt->check_trust(pt, x, flags);
}
LCRYPTO_ALIAS(X509_check_trust);

int
X509_TRUST_get_count(void)
{
	return X509_TRUST_COUNT;
}
LCRYPTO_ALIAS(X509_TRUST_get_count);

X509_TRUST *
X509_TRUST_get0(int idx)
{
	if (idx < 0 || (size_t)idx >= X509_TRUST_COUNT)
		return NULL;

	return &trstandard[idx];
}
LCRYPTO_ALIAS(X509_TRUST_get0);

int
X509_TRUST_get_by_id(int id)
{
	/*
	 * Ensure the trust identifier is between MIN and MAX inclusive.
	 * If so, translate it into an index into the trstandard[] table.
	 */
	if (id < X509_TRUST_MIN || id > X509_TRUST_MAX)
		return -1;

	return id - X509_TRUST_MIN;
}
LCRYPTO_ALIAS(X509_TRUST_get_by_id);

int
X509_TRUST_set(int *t, int trust)
{
	if (X509_TRUST_get_by_id(trust) == -1) {
		X509error(X509_R_INVALID_TRUST);
		return 0;
	}
	*t = trust;
	return 1;
}
LCRYPTO_ALIAS(X509_TRUST_set);

int
X509_TRUST_add(int id, int flags, int (*ck)(X509_TRUST *, X509 *, int),
    const char *name, int arg1, void *arg2)
{
	X509error(ERR_R_DISABLED);
	return 0;
}
LCRYPTO_ALIAS(X509_TRUST_add);

void
X509_TRUST_cleanup(void)
{
}
LCRYPTO_ALIAS(X509_TRUST_cleanup);

int
X509_TRUST_get_flags(const X509_TRUST *xp)
{
	return xp->flags;
}
LCRYPTO_ALIAS(X509_TRUST_get_flags);

char *
X509_TRUST_get0_name(const X509_TRUST *xp)
{
	return xp->name;
}
LCRYPTO_ALIAS(X509_TRUST_get0_name);

int
X509_TRUST_get_trust(const X509_TRUST *xp)
{
	return xp->trust;
}
LCRYPTO_ALIAS(X509_TRUST_get_trust);
