/* $OpenBSD: v3_genn.c,v 1.10 2015/02/09 16:03:11 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2008 The OpenSSL Project.  All rights reserved.
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
#include <openssl/conf.h>
#include <openssl/x509v3.h>

ASN1_SEQUENCE(OTHERNAME) = {
	ASN1_SIMPLE(OTHERNAME, type_id, ASN1_OBJECT),
	/* Maybe have a true ANY DEFINED BY later */
	ASN1_EXP(OTHERNAME, value, ASN1_ANY, 0)
} ASN1_SEQUENCE_END(OTHERNAME)


OTHERNAME *
d2i_OTHERNAME(OTHERNAME **a, const unsigned char **in, long len)
{
	return (OTHERNAME *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &OTHERNAME_it);
}

int
i2d_OTHERNAME(OTHERNAME *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &OTHERNAME_it);
}

OTHERNAME *
OTHERNAME_new(void)
{
	return (OTHERNAME *)ASN1_item_new(&OTHERNAME_it);
}

void
OTHERNAME_free(OTHERNAME *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &OTHERNAME_it);
}

ASN1_SEQUENCE(EDIPARTYNAME) = {
	ASN1_IMP_OPT(EDIPARTYNAME, nameAssigner, DIRECTORYSTRING, 0),
	ASN1_IMP_OPT(EDIPARTYNAME, partyName, DIRECTORYSTRING, 1)
} ASN1_SEQUENCE_END(EDIPARTYNAME)


EDIPARTYNAME *
d2i_EDIPARTYNAME(EDIPARTYNAME **a, const unsigned char **in, long len)
{
	return (EDIPARTYNAME *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &EDIPARTYNAME_it);
}

int
i2d_EDIPARTYNAME(EDIPARTYNAME *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &EDIPARTYNAME_it);
}

EDIPARTYNAME *
EDIPARTYNAME_new(void)
{
	return (EDIPARTYNAME *)ASN1_item_new(&EDIPARTYNAME_it);
}

void
EDIPARTYNAME_free(EDIPARTYNAME *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &EDIPARTYNAME_it);
}

ASN1_CHOICE(GENERAL_NAME) = {
	ASN1_IMP(GENERAL_NAME, d.otherName, OTHERNAME, GEN_OTHERNAME),
	ASN1_IMP(GENERAL_NAME, d.rfc822Name, ASN1_IA5STRING, GEN_EMAIL),
	ASN1_IMP(GENERAL_NAME, d.dNSName, ASN1_IA5STRING, GEN_DNS),
	/* Don't decode this */
	ASN1_IMP(GENERAL_NAME, d.x400Address, ASN1_SEQUENCE, GEN_X400),
	/* X509_NAME is a CHOICE type so use EXPLICIT */
	ASN1_EXP(GENERAL_NAME, d.directoryName, X509_NAME, GEN_DIRNAME),
	ASN1_IMP(GENERAL_NAME, d.ediPartyName, EDIPARTYNAME, GEN_EDIPARTY),
	ASN1_IMP(GENERAL_NAME, d.uniformResourceIdentifier, ASN1_IA5STRING, GEN_URI),
	ASN1_IMP(GENERAL_NAME, d.iPAddress, ASN1_OCTET_STRING, GEN_IPADD),
	ASN1_IMP(GENERAL_NAME, d.registeredID, ASN1_OBJECT, GEN_RID)
} ASN1_CHOICE_END(GENERAL_NAME)


GENERAL_NAME *
d2i_GENERAL_NAME(GENERAL_NAME **a, const unsigned char **in, long len)
{
	return (GENERAL_NAME *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GENERAL_NAME_it);
}

int
i2d_GENERAL_NAME(GENERAL_NAME *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GENERAL_NAME_it);
}

GENERAL_NAME *
GENERAL_NAME_new(void)
{
	return (GENERAL_NAME *)ASN1_item_new(&GENERAL_NAME_it);
}

void
GENERAL_NAME_free(GENERAL_NAME *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GENERAL_NAME_it);
}

ASN1_ITEM_TEMPLATE(GENERAL_NAMES) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, GeneralNames, GENERAL_NAME)
ASN1_ITEM_TEMPLATE_END(GENERAL_NAMES)


GENERAL_NAMES *
d2i_GENERAL_NAMES(GENERAL_NAMES **a, const unsigned char **in, long len)
{
	return (GENERAL_NAMES *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &GENERAL_NAMES_it);
}

int
i2d_GENERAL_NAMES(GENERAL_NAMES *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &GENERAL_NAMES_it);
}

GENERAL_NAMES *
GENERAL_NAMES_new(void)
{
	return (GENERAL_NAMES *)ASN1_item_new(&GENERAL_NAMES_it);
}

void
GENERAL_NAMES_free(GENERAL_NAMES *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &GENERAL_NAMES_it);
}

GENERAL_NAME *
GENERAL_NAME_dup(GENERAL_NAME *a)
{
	return (GENERAL_NAME *)ASN1_dup((i2d_of_void *)i2d_GENERAL_NAME,
	    (d2i_of_void *)d2i_GENERAL_NAME, (char *)a);
}

/* Returns 0 if they are equal, != 0 otherwise. */
int
GENERAL_NAME_cmp(GENERAL_NAME *a, GENERAL_NAME *b)
{
	int result = -1;

	if (!a || !b || a->type != b->type)
		return -1;
	switch (a->type) {
	case GEN_X400:
	case GEN_EDIPARTY:
		result = ASN1_TYPE_cmp(a->d.other, b->d.other);
		break;

	case GEN_OTHERNAME:
		result = OTHERNAME_cmp(a->d.otherName, b->d.otherName);
		break;

	case GEN_EMAIL:
	case GEN_DNS:
	case GEN_URI:
		result = ASN1_STRING_cmp(a->d.ia5, b->d.ia5);
		break;

	case GEN_DIRNAME:
		result = X509_NAME_cmp(a->d.dirn, b->d.dirn);
		break;

	case GEN_IPADD:
		result = ASN1_OCTET_STRING_cmp(a->d.ip, b->d.ip);
		break;

	case GEN_RID:
		result = OBJ_cmp(a->d.rid, b->d.rid);
		break;
	}
	return result;
}

/* Returns 0 if they are equal, != 0 otherwise. */
int
OTHERNAME_cmp(OTHERNAME *a, OTHERNAME *b)
{
	int result = -1;

	if (!a || !b)
		return -1;
	/* Check their type first. */
	if ((result = OBJ_cmp(a->type_id, b->type_id)) != 0)
		return result;
	/* Check the value. */
	result = ASN1_TYPE_cmp(a->value, b->value);
	return result;
}

void
GENERAL_NAME_set0_value(GENERAL_NAME *a, int type, void *value)
{
	switch (type) {
	case GEN_X400:
	case GEN_EDIPARTY:
		a->d.other = value;
		break;

	case GEN_OTHERNAME:
		a->d.otherName = value;
		break;

	case GEN_EMAIL:
	case GEN_DNS:
	case GEN_URI:
		a->d.ia5 = value;
		break;

	case GEN_DIRNAME:
		a->d.dirn = value;
		break;

	case GEN_IPADD:
		a->d.ip = value;
		break;

	case GEN_RID:
		a->d.rid = value;
		break;
	}
	a->type = type;
}

void *
GENERAL_NAME_get0_value(GENERAL_NAME *a, int *ptype)
{
	if (ptype)
		*ptype = a->type;
	switch (a->type) {
	case GEN_X400:
	case GEN_EDIPARTY:
		return a->d.other;

	case GEN_OTHERNAME:
		return a->d.otherName;

	case GEN_EMAIL:
	case GEN_DNS:
	case GEN_URI:
		return a->d.ia5;

	case GEN_DIRNAME:
		return a->d.dirn;

	case GEN_IPADD:
		return a->d.ip;

	case GEN_RID:
		return a->d.rid;

	default:
		return NULL;
	}
}

int
GENERAL_NAME_set0_othername(GENERAL_NAME *gen, ASN1_OBJECT *oid,
    ASN1_TYPE *value)
{
	OTHERNAME *oth;

	oth = OTHERNAME_new();
	if (!oth)
		return 0;
	oth->type_id = oid;
	oth->value = value;
	GENERAL_NAME_set0_value(gen, GEN_OTHERNAME, oth);
	return 1;
}

int
GENERAL_NAME_get0_otherName(GENERAL_NAME *gen, ASN1_OBJECT **poid,
    ASN1_TYPE **pvalue)
{
	if (gen->type != GEN_OTHERNAME)
		return 0;
	if (poid)
		*poid = gen->d.otherName->type_id;
	if (pvalue)
		*pvalue = gen->d.otherName->value;
	return 1;
}
