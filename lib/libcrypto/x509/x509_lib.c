/* $OpenBSD: x509_lib.c,v 1.23 2024/06/17 05:38:08 tb Exp $ */
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
/* X509 v3 extension utilities */

#include <stdio.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "x509_local.h"

extern const X509V3_EXT_METHOD v3_bcons, v3_nscert, v3_key_usage, v3_ext_ku;
extern const X509V3_EXT_METHOD v3_pkey_usage_period, v3_info, v3_sinfo;
extern const X509V3_EXT_METHOD v3_ns_ia5_list[], v3_alt[], v3_skey_id, v3_akey_id;
extern const X509V3_EXT_METHOD v3_crl_num, v3_crl_reason, v3_crl_invdate;
extern const X509V3_EXT_METHOD v3_delta_crl, v3_cpols, v3_crld, v3_freshest_crl;
extern const X509V3_EXT_METHOD v3_ocsp_nonce, v3_ocsp_accresp, v3_ocsp_acutoff;
extern const X509V3_EXT_METHOD v3_ocsp_crlid, v3_ocsp_nocheck, v3_ocsp_serviceloc;
extern const X509V3_EXT_METHOD v3_crl_hold;
extern const X509V3_EXT_METHOD v3_policy_mappings, v3_policy_constraints;
extern const X509V3_EXT_METHOD v3_name_constraints, v3_inhibit_anyp, v3_idp;
extern const X509V3_EXT_METHOD v3_addr, v3_asid;
extern const X509V3_EXT_METHOD v3_ct_scts[3];

static const X509V3_EXT_METHOD *standard_exts[] = {
	&v3_nscert,
	&v3_ns_ia5_list[0],
	&v3_ns_ia5_list[1],
	&v3_ns_ia5_list[2],
	&v3_ns_ia5_list[3],
	&v3_ns_ia5_list[4],
	&v3_ns_ia5_list[5],
	&v3_ns_ia5_list[6],
	&v3_skey_id,
	&v3_key_usage,
	&v3_pkey_usage_period,
	&v3_alt[0],
	&v3_alt[1],
	&v3_bcons,
	&v3_crl_num,
	&v3_cpols,
	&v3_akey_id,
	&v3_crld,
	&v3_ext_ku,
	&v3_delta_crl,
	&v3_crl_reason,
#ifndef OPENSSL_NO_OCSP
	&v3_crl_invdate,
#endif
	&v3_info,
#ifndef OPENSSL_NO_RFC3779
	&v3_addr,
	&v3_asid,
#endif
#ifndef OPENSSL_NO_OCSP
	&v3_ocsp_nonce,
	&v3_ocsp_crlid,
	&v3_ocsp_accresp,
	&v3_ocsp_nocheck,
	&v3_ocsp_acutoff,
	&v3_ocsp_serviceloc,
#endif
	&v3_sinfo,
	&v3_policy_constraints,
#ifndef OPENSSL_NO_OCSP
	&v3_crl_hold,
#endif
	&v3_name_constraints,
	&v3_policy_mappings,
	&v3_inhibit_anyp,
	&v3_idp,
	&v3_alt[2],
	&v3_freshest_crl,
#ifndef OPENSSL_NO_CT
	&v3_ct_scts[0],
	&v3_ct_scts[1],
	&v3_ct_scts[2],
#endif
};

#define STANDARD_EXTENSION_COUNT (sizeof(standard_exts) / sizeof(standard_exts[0]))

const X509V3_EXT_METHOD *
X509V3_EXT_get_nid(int nid)
{
	size_t i;

	for (i = 0; i < STANDARD_EXTENSION_COUNT; i++) {
		if (standard_exts[i]->ext_nid == nid)
			return standard_exts[i];
	}

	return NULL;
}
LCRYPTO_ALIAS(X509V3_EXT_get_nid);

const X509V3_EXT_METHOD *
X509V3_EXT_get(X509_EXTENSION *ext)
{
	int nid;

	if ((nid = OBJ_obj2nid(ext->object)) == NID_undef)
		return NULL;
	return X509V3_EXT_get_nid(nid);
}
LCRYPTO_ALIAS(X509V3_EXT_get);

/* Return an extension internal structure */

void *
X509V3_EXT_d2i(X509_EXTENSION *ext)
{
	const X509V3_EXT_METHOD *method;
	const unsigned char *p;

	if ((method = X509V3_EXT_get(ext)) == NULL)
		return NULL;
	p = ext->value->data;
	if (method->it != NULL)
		return ASN1_item_d2i(NULL, &p, ext->value->length, method->it);
	return method->d2i(NULL, &p, ext->value->length);
}
LCRYPTO_ALIAS(X509V3_EXT_d2i);

/*
 * This API is only safe to call with known nid, crit != NULL and idx == NULL.
 * On NULL return, crit acts as a failure indicator: crit == -1 means an
 * extension of type nid was not present, crit != -1 is fatal: crit == -2
 * means multiple extensions of type nid are present; if crit is 0 or 1, this
 * implies the extension was found but could not be decoded.
 */

void *
X509V3_get_d2i(const STACK_OF(X509_EXTENSION) *x509_exts, int nid, int *crit,
    int *idx)
{
	X509_EXTENSION *ext;
	int lastpos = idx == NULL ? -1 : *idx;

	if (crit != NULL)
		*crit = -1;
	if (idx != NULL)
		*idx = -1;

	/*
	 * Nothing to do if no extensions, unknown nid, or missing extension.
	 */

	if (x509_exts == NULL)
		return NULL;
	if ((lastpos = X509v3_get_ext_by_NID(x509_exts, nid, lastpos)) < 0)
		return NULL;
	if ((ext = X509v3_get_ext(x509_exts, lastpos)) == NULL)
		return NULL;

	/*
	 * API madness. Only check for a second extension of type nid if
	 * idx == NULL. Indicate this by setting *crit to -2. If idx != NULL,
	 * don't care and set *idx to the index of the first extension found.
	 */

	if (idx == NULL && X509v3_get_ext_by_NID(x509_exts, nid, lastpos) > 0) {
		if (crit != NULL)
			*crit = -2;
		return NULL;
	}

	/*
	 * Another beautiful API detail: *crit will be set to 0 or 1, so if the
	 * extension fails to decode, we can deduce this from return value NULL
	 * and crit != -1.
	 */

	if (crit != NULL)
		*crit = X509_EXTENSION_get_critical(ext);
	if (idx != NULL)
		*idx = lastpos;

	return X509V3_EXT_d2i(ext);
}
LCRYPTO_ALIAS(X509V3_get_d2i);

int
X509V3_add1_i2d(STACK_OF(X509_EXTENSION) **x509_exts, int nid, void *value,
    int crit, unsigned long flags)
{
	STACK_OF(X509_EXTENSION) *exts = *x509_exts;
	X509_EXTENSION *ext = NULL;
	X509_EXTENSION *existing;
	int extidx;
	int errcode = 0;
	int ret = 0;

	/* See if the extension already exists. */
	extidx = X509v3_get_ext_by_NID(*x509_exts, nid, -1);

	switch (flags & X509V3_ADD_OP_MASK) {
	case X509V3_ADD_DEFAULT:
		/* If the extension exists, adding another one is an error. */
		if (extidx >= 0) {
			errcode = X509V3_R_EXTENSION_EXISTS;
			goto err;
		}
		break;
	case X509V3_ADD_APPEND:
		/*
		 * XXX - Total misfeature. If the extension exists, appending
		 * another one will invalidate the certificate. Unfortunately
		 * things use this, in particular Viktor's DANE code.
		 */
		/* Pretend the extension didn't exist and append the new one. */
		extidx = -1;
		break;
	case X509V3_ADD_REPLACE:
		/* Replace existing extension, otherwise append the new one. */
		break;
	case X509V3_ADD_REPLACE_EXISTING:
		/* Can't replace a non-existent extension. */
		if (extidx < 0) {
			errcode = X509V3_R_EXTENSION_NOT_FOUND;
			goto err;
		}
		break;
	case X509V3_ADD_KEEP_EXISTING:
		/* If the extension exists, there's nothing to do. */
		if (extidx >= 0)
			goto done;
		break;
	case X509V3_ADD_DELETE:
		/* Can't delete a non-existent extension. */
		if (extidx < 0) {
			errcode = X509V3_R_EXTENSION_NOT_FOUND;
			goto err;
		}
		if ((existing = sk_X509_EXTENSION_delete(*x509_exts,
		    extidx)) == NULL) {
			ret = -1;
			goto err;
		}
		X509_EXTENSION_free(existing);
		existing = NULL;
		goto done;
	default:
		errcode = X509V3_R_UNSUPPORTED_OPTION; /* XXX */
		ret = -1;
		goto err;
	}

	if ((ext = X509V3_EXT_i2d(nid, crit, value)) == NULL) {
		X509V3error(X509V3_R_ERROR_CREATING_EXTENSION);
		goto err;
	}

	/* From here, errors are fatal. */
	ret = -1;

	/* If extension exists, replace it. */
	if (extidx >= 0) {
		existing = sk_X509_EXTENSION_value(*x509_exts, extidx);
		X509_EXTENSION_free(existing);
		existing = NULL;
		if (sk_X509_EXTENSION_set(*x509_exts, extidx, ext) == NULL) {
			/*
			 * XXX - Can't happen. If it did happen, |existing| is
			 * now a freed pointer. Nothing we can do here.
			 */
			goto err;
		}
		goto done;
	}

	if (exts == NULL)
		exts = sk_X509_EXTENSION_new_null();
	if (exts == NULL)
		goto err;

	if (!sk_X509_EXTENSION_push(exts, ext))
		goto err;
	ext = NULL;

	*x509_exts = exts;

 done:
	return 1;

 err:
	if ((flags & X509V3_ADD_SILENT) == 0 && errcode != 0)
		X509V3error(errcode);

	if (exts != *x509_exts)
		sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
	X509_EXTENSION_free(ext);

	return ret;
}
LCRYPTO_ALIAS(X509V3_add1_i2d);

int
X509V3_add_standard_extensions(void)
{
	return 1;
}
LCRYPTO_ALIAS(X509V3_add_standard_extensions);
