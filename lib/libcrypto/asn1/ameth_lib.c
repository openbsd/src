/* $OpenBSD: ameth_lib.c,v 1.30 2022/11/26 16:08:50 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
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

#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/asn1t.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#include "asn1_local.h"
#include "evp_local.h"

extern const EVP_PKEY_ASN1_METHOD cmac_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD dh_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD dsa_asn1_meths[];
extern const EVP_PKEY_ASN1_METHOD eckey_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD ed25519_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD gostimit_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD gostr01_asn1_meths[];
extern const EVP_PKEY_ASN1_METHOD hmac_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD rsa_asn1_meths[];
extern const EVP_PKEY_ASN1_METHOD rsa_pss_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD x25519_asn1_meth;

static const EVP_PKEY_ASN1_METHOD *asn1_methods[] = {
	&cmac_asn1_meth,
	&dh_asn1_meth,
	&dsa_asn1_meths[0],
	&dsa_asn1_meths[1],
	&dsa_asn1_meths[2],
	&dsa_asn1_meths[3],
	&dsa_asn1_meths[4],
	&eckey_asn1_meth,
	&ed25519_asn1_meth,
	&gostimit_asn1_meth,
	&gostr01_asn1_meths[0],
	&gostr01_asn1_meths[1],
	&gostr01_asn1_meths[2],
	&hmac_asn1_meth,
	&rsa_asn1_meths[0],
	&rsa_asn1_meths[1],
	&rsa_pss_asn1_meth,
	&x25519_asn1_meth,
};

static const size_t asn1_methods_count =
    sizeof(asn1_methods) / sizeof(asn1_methods[0]);

DECLARE_STACK_OF(EVP_PKEY_ASN1_METHOD)
static STACK_OF(EVP_PKEY_ASN1_METHOD) *asn1_app_methods = NULL;

int
EVP_PKEY_asn1_get_count(void)
{
	int num = asn1_methods_count;

	if (asn1_app_methods != NULL)
		num += sk_EVP_PKEY_ASN1_METHOD_num(asn1_app_methods);

	return num;
}

const EVP_PKEY_ASN1_METHOD *
EVP_PKEY_asn1_get0(int idx)
{
	int num = asn1_methods_count;

	if (idx < 0)
		return NULL;
	if (idx < num)
		return asn1_methods[idx];

	idx -= num;

	return sk_EVP_PKEY_ASN1_METHOD_value(asn1_app_methods, idx);
}

static const EVP_PKEY_ASN1_METHOD *
pkey_asn1_find(int pkey_id)
{
	const EVP_PKEY_ASN1_METHOD *ameth;
	int i;

	for (i = EVP_PKEY_asn1_get_count() - 1; i >= 0; i--) {
		ameth = EVP_PKEY_asn1_get0(i);
		if (ameth->pkey_id == pkey_id)
			return ameth;
	}

	return NULL;
}

/*
 * Find an implementation of an ASN1 algorithm. If 'pe' is not NULL
 * also search through engines and set *pe to a functional reference
 * to the engine implementing 'type' or NULL if no engine implements
 * it.
 */
const EVP_PKEY_ASN1_METHOD *
EVP_PKEY_asn1_find(ENGINE **pe, int type)
{
	const EVP_PKEY_ASN1_METHOD *mp;

	for (;;) {
		if ((mp = pkey_asn1_find(type)) == NULL)
			break;
		if ((mp->pkey_flags & ASN1_PKEY_ALIAS) == 0)
			break;
		type = mp->pkey_base_id;
	}
	if (pe) {
#ifndef OPENSSL_NO_ENGINE
		ENGINE *e;
		/* type will contain the final unaliased type */
		e = ENGINE_get_pkey_asn1_meth_engine(type);
		if (e) {
			*pe = e;
			return ENGINE_get_pkey_asn1_meth(e, type);
		}
#endif
		*pe = NULL;
	}
	return mp;
}

const EVP_PKEY_ASN1_METHOD *
EVP_PKEY_asn1_find_str(ENGINE **pe, const char *str, int len)
{
	const EVP_PKEY_ASN1_METHOD *ameth;
	int i;

	if (len == -1)
		len = strlen(str);
	if (pe) {
#ifndef OPENSSL_NO_ENGINE
		ENGINE *e;
		ameth = ENGINE_pkey_asn1_find_str(&e, str, len);
		if (ameth) {
			/* Convert structural into
			 * functional reference
			 */
			if (!ENGINE_init(e))
				ameth = NULL;
			ENGINE_free(e);
			*pe = e;
			return ameth;
		}
#endif
		*pe = NULL;
	}
	for (i = EVP_PKEY_asn1_get_count() - 1; i >= 0; i--) {
		ameth = EVP_PKEY_asn1_get0(i);
		if (ameth->pkey_flags & ASN1_PKEY_ALIAS)
			continue;
		if (((int)strlen(ameth->pem_str) == len) &&
		    !strncasecmp(ameth->pem_str, str, len))
			return ameth;
	}
	return NULL;
}

int
EVP_PKEY_asn1_add0(const EVP_PKEY_ASN1_METHOD *ameth)
{
	if (asn1_app_methods == NULL) {
		asn1_app_methods = sk_EVP_PKEY_ASN1_METHOD_new(NULL);
		if (asn1_app_methods == NULL)
			return 0;
	}

	if (!sk_EVP_PKEY_ASN1_METHOD_push(asn1_app_methods, ameth))
		return 0;

	return 1;
}

int
EVP_PKEY_asn1_add_alias(int to, int from)
{
	EVP_PKEY_ASN1_METHOD *ameth;

	ameth = EVP_PKEY_asn1_new(from, ASN1_PKEY_ALIAS, NULL, NULL);
	if (ameth == NULL)
		return 0;

	ameth->pkey_base_id = to;
	if (!EVP_PKEY_asn1_add0(ameth)) {
		EVP_PKEY_asn1_free(ameth);
		return 0;
	}
	return 1;
}

int
EVP_PKEY_asn1_get0_info(int *ppkey_id, int *ppkey_base_id, int *ppkey_flags,
    const char **pinfo, const char **ppem_str,
    const EVP_PKEY_ASN1_METHOD *ameth)
{
	if (!ameth)
		return 0;
	if (ppkey_id)
		*ppkey_id = ameth->pkey_id;
	if (ppkey_base_id)
		*ppkey_base_id = ameth->pkey_base_id;
	if (ppkey_flags)
		*ppkey_flags = ameth->pkey_flags;
	if (pinfo)
		*pinfo = ameth->info;
	if (ppem_str)
		*ppem_str = ameth->pem_str;
	return 1;
}

const EVP_PKEY_ASN1_METHOD*
EVP_PKEY_get0_asn1(const EVP_PKEY *pkey)
{
	return pkey->ameth;
}

EVP_PKEY_ASN1_METHOD*
EVP_PKEY_asn1_new(int id, int flags, const char *pem_str, const char *info)
{
	EVP_PKEY_ASN1_METHOD *ameth;

	if ((ameth = calloc(1, sizeof(EVP_PKEY_ASN1_METHOD))) == NULL)
		return NULL;

	ameth->pkey_id = id;
	ameth->pkey_base_id = id;
	ameth->pkey_flags = flags | ASN1_PKEY_DYNAMIC;

	if (info != NULL) {
		if ((ameth->info = strdup(info)) == NULL)
			goto err;
	}

	if (pem_str != NULL) {
		if ((ameth->pem_str = strdup(pem_str)) == NULL)
			goto err;
	}

	return ameth;

 err:
	EVP_PKEY_asn1_free(ameth);
	return NULL;
}

void
EVP_PKEY_asn1_copy(EVP_PKEY_ASN1_METHOD *dst, const EVP_PKEY_ASN1_METHOD *src)
{
	EVP_PKEY_ASN1_METHOD preserve;

	preserve.pkey_id = dst->pkey_id;
	preserve.pkey_base_id = dst->pkey_base_id;
	preserve.pkey_flags = dst->pkey_flags;
	preserve.pem_str = dst->pem_str;
	preserve.info = dst->info;

	*dst = *src;

	dst->pkey_id = preserve.pkey_id;
	dst->pkey_base_id = preserve.pkey_base_id;
	dst->pkey_flags = preserve.pkey_flags;
	dst->pem_str = preserve.pem_str;
	dst->info = preserve.info;
}

void
EVP_PKEY_asn1_free(EVP_PKEY_ASN1_METHOD *ameth)
{
	if (ameth && (ameth->pkey_flags & ASN1_PKEY_DYNAMIC)) {
		free(ameth->pem_str);
		free(ameth->info);
		free(ameth);
	}
}

void
EVP_PKEY_asn1_set_public(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pub_decode)(EVP_PKEY *pk, X509_PUBKEY *pub),
    int (*pub_encode)(X509_PUBKEY *pub, const EVP_PKEY *pk),
    int (*pub_cmp)(const EVP_PKEY *a, const EVP_PKEY *b),
    int (*pub_print)(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx),
    int (*pkey_size)(const EVP_PKEY *pk),
    int (*pkey_bits)(const EVP_PKEY *pk))
{
	ameth->pub_decode = pub_decode;
	ameth->pub_encode = pub_encode;
	ameth->pub_cmp = pub_cmp;
	ameth->pub_print = pub_print;
	ameth->pkey_size = pkey_size;
	ameth->pkey_bits = pkey_bits;
}

void
EVP_PKEY_asn1_set_private(EVP_PKEY_ASN1_METHOD *ameth,
    int (*priv_decode)(EVP_PKEY *pk, const PKCS8_PRIV_KEY_INFO *p8inf),
    int (*priv_encode)(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pk),
    int (*priv_print)(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx))
{
	ameth->priv_decode = priv_decode;
	ameth->priv_encode = priv_encode;
	ameth->priv_print = priv_print;
}

void
EVP_PKEY_asn1_set_param(EVP_PKEY_ASN1_METHOD *ameth,
    int (*param_decode)(EVP_PKEY *pkey, const unsigned char **pder, int derlen),
    int (*param_encode)(const EVP_PKEY *pkey, unsigned char **pder),
    int (*param_missing)(const EVP_PKEY *pk),
    int (*param_copy)(EVP_PKEY *to, const EVP_PKEY *from),
    int (*param_cmp)(const EVP_PKEY *a, const EVP_PKEY *b),
    int (*param_print)(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx))
{
	ameth->param_decode = param_decode;
	ameth->param_encode = param_encode;
	ameth->param_missing = param_missing;
	ameth->param_copy = param_copy;
	ameth->param_cmp = param_cmp;
	ameth->param_print = param_print;
}

void
EVP_PKEY_asn1_set_free(EVP_PKEY_ASN1_METHOD *ameth,
    void (*pkey_free)(EVP_PKEY *pkey))
{
	ameth->pkey_free = pkey_free;
}

void
EVP_PKEY_asn1_set_ctrl(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pkey_ctrl)(EVP_PKEY *pkey, int op, long arg1, void *arg2))
{
	ameth->pkey_ctrl = pkey_ctrl;
}

void
EVP_PKEY_asn1_set_security_bits(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pkey_security_bits)(const EVP_PKEY *pkey))
{
	ameth->pkey_security_bits = pkey_security_bits;
}

void
EVP_PKEY_asn1_set_check(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pkey_check)(const EVP_PKEY *pk))
{
	ameth->pkey_check = pkey_check;
}

void
EVP_PKEY_asn1_set_public_check(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pkey_public_check)(const EVP_PKEY *pk))
{
	ameth->pkey_public_check = pkey_public_check;
}

void
EVP_PKEY_asn1_set_param_check(EVP_PKEY_ASN1_METHOD *ameth,
    int (*pkey_param_check)(const EVP_PKEY *pk))
{
	ameth->pkey_param_check = pkey_param_check;
}
