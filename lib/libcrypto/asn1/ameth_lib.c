/* $OpenBSD: ameth_lib.c,v 1.41 2023/12/29 19:00:31 tb Exp $ */
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

#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>

#include "evp_local.h"

/*
 * XXX - remove all the API below here in the next major bump.
 */

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

int
EVP_PKEY_asn1_add0(const EVP_PKEY_ASN1_METHOD *ameth)
{
	EVPerror(ERR_R_DISABLED);
	return 0;
}

int
EVP_PKEY_asn1_add_alias(int to, int from)
{
	EVPerror(ERR_R_DISABLED);
	return 0;
}
