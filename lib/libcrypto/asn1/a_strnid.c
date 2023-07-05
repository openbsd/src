/* $OpenBSD: a_strnid.c,v 1.27 2023/07/05 21:23:36 beck Exp $ */
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/objects.h>

static STACK_OF(ASN1_STRING_TABLE) *stable = NULL;

static ASN1_STRING_TABLE *stable_get(int nid);
static void st_free(ASN1_STRING_TABLE *tbl);
static int sk_table_cmp(const ASN1_STRING_TABLE * const *a,
    const ASN1_STRING_TABLE * const *b);


/*
 * This is the global mask for the mbstring functions: this is used to
 * mask out certain types (such as BMPString and UTF8String) because
 * certain software (e.g. Netscape) has problems with them.
 */

static unsigned long global_mask = B_ASN1_UTF8STRING;

void
ASN1_STRING_set_default_mask(unsigned long mask)
{
	global_mask = mask;
}
LCRYPTO_ALIAS(ASN1_STRING_set_default_mask);

unsigned long
ASN1_STRING_get_default_mask(void)
{
	return global_mask;
}
LCRYPTO_ALIAS(ASN1_STRING_get_default_mask);

/*
 * This function sets the default to various "flavours" of configuration
 * based on an ASCII string. Currently this is:
 * MASK:XXXX : a numerical mask value.
 * nobmp : Don't use BMPStrings (just Printable, T61).
 * pkix : PKIX recommendation in RFC2459.
 * utf8only : only use UTF8Strings (RFC2459 recommendation for 2004).
 * default:   the default value, Printable, T61, BMP.
 */

int
ASN1_STRING_set_default_mask_asc(const char *p)
{
	unsigned long mask;
	char *end;
	int save_errno;

	if (strncmp(p, "MASK:", 5) == 0) {
		if (p[5] == '\0')
			return 0;
		save_errno = errno;
		errno = 0;
		mask = strtoul(p + 5, &end, 0);
		if (errno == ERANGE && mask == ULONG_MAX)
			return 0;
		errno = save_errno;
		if (*end != '\0')
			return 0;
	} else if (strcmp(p, "nombstr") == 0)
		mask = ~((unsigned long)(B_ASN1_BMPSTRING|B_ASN1_UTF8STRING));
	else if (strcmp(p, "pkix") == 0)
		mask = ~((unsigned long)B_ASN1_T61STRING);
	else if (strcmp(p, "utf8only") == 0)
		mask = B_ASN1_UTF8STRING;
	else if (strcmp(p, "default") == 0)
		mask = 0xFFFFFFFFL;
	else
		return 0;
	ASN1_STRING_set_default_mask(mask);
	return 1;
}
LCRYPTO_ALIAS(ASN1_STRING_set_default_mask_asc);

/*
 * The following function generates an ASN1_STRING based on limits in a table.
 * Frequently the types and length of an ASN1_STRING are restricted by a
 * corresponding OID. For example certificates and certificate requests.
 */

ASN1_STRING *
ASN1_STRING_set_by_NID(ASN1_STRING **out, const unsigned char *in, int inlen,
    int inform, int nid)
{
	ASN1_STRING_TABLE *tbl;
	ASN1_STRING *str = NULL;
	unsigned long mask;
	int ret;

	if (out == NULL)
		out = &str;
	tbl = ASN1_STRING_TABLE_get(nid);
	if (tbl != NULL) {
		mask = tbl->mask;
		if ((tbl->flags & STABLE_NO_MASK) == 0)
			mask &= global_mask;
		ret = ASN1_mbstring_ncopy(out, in, inlen, inform, mask,
		    tbl->minsize, tbl->maxsize);
	} else
		ret = ASN1_mbstring_copy(out, in, inlen, inform,
		    DIRSTRING_TYPE & global_mask);
	if (ret <= 0)
		return NULL;
	return *out;
}
LCRYPTO_ALIAS(ASN1_STRING_set_by_NID);

/*
 * Now the tables and helper functions for the string table:
 */

/* size limits: this stuff is taken straight from RFC3280 */

#define ub_name				32768
#define ub_common_name			64
#define ub_locality_name		128
#define ub_state_name			128
#define ub_organization_name		64
#define ub_organization_unit_name	64
#define ub_title			64
#define ub_email_address		128
#define ub_serial_number		64


/* This table must be kept in NID order */

static const ASN1_STRING_TABLE tbl_standard[] = {
	{
		.nid = NID_commonName,
		.minsize = 1,
		.maxsize = ub_common_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_countryName,
		.minsize = 2,
		.maxsize = 2,
		.mask = B_ASN1_PRINTABLESTRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_localityName,
		.minsize = 1,
		.maxsize = ub_locality_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_stateOrProvinceName,
		.minsize = 1,
		.maxsize = ub_state_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_organizationName,
		.minsize = 1,
		.maxsize = ub_organization_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_organizationalUnitName,
		.minsize = 1,
		.maxsize = ub_organization_unit_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_pkcs9_emailAddress,
		.minsize = 1,
		.maxsize = ub_email_address,
		.mask = B_ASN1_IA5STRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_pkcs9_unstructuredName,
		.minsize = 1,
		.maxsize = -1,
		.mask = PKCS9STRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_pkcs9_challengePassword,
		.minsize = 1,
		.maxsize = -1,
		.mask = PKCS9STRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_pkcs9_unstructuredAddress,
		.minsize = 1,
		.maxsize = -1,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_givenName,
		.minsize = 1,
		.maxsize = ub_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_surname,
		.minsize = 1,
		.maxsize = ub_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_initials,
		.minsize = 1,
		.maxsize = ub_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_serialNumber,
		.minsize = 1,
		.maxsize = ub_serial_number,
		.mask = B_ASN1_PRINTABLESTRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_friendlyName,
		.minsize = -1,
		.maxsize = -1,
		.mask = B_ASN1_BMPSTRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_name,
		.minsize = 1,
		.maxsize = ub_name,
		.mask = DIRSTRING_TYPE,
		.flags = 0,
	},
	{
		.nid = NID_dnQualifier,
		.minsize = -1,
		.maxsize = -1,
		.mask = B_ASN1_PRINTABLESTRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_domainComponent,
		.minsize = 1,
		.maxsize = -1,
		.mask = B_ASN1_IA5STRING,
		.flags = STABLE_NO_MASK,
	},
	{
		.nid = NID_ms_csp_name,
		.minsize = -1,
		.maxsize = -1,
		.mask = B_ASN1_BMPSTRING,
		.flags = STABLE_NO_MASK,
	},
};

static int
sk_table_cmp(const ASN1_STRING_TABLE * const *a,
    const ASN1_STRING_TABLE * const *b)
{
	return (*a)->nid - (*b)->nid;
}

static int table_cmp_BSEARCH_CMP_FN(const void *, const void *);
static int table_cmp(ASN1_STRING_TABLE const *, ASN1_STRING_TABLE const *);
static ASN1_STRING_TABLE *OBJ_bsearch_table(ASN1_STRING_TABLE *key, ASN1_STRING_TABLE const *base, int num);

static int
table_cmp(const ASN1_STRING_TABLE *a, const ASN1_STRING_TABLE *b)
{
	return a->nid - b->nid;
}


static int
table_cmp_BSEARCH_CMP_FN(const void *a_, const void *b_)
{
	ASN1_STRING_TABLE const *a = a_;
	ASN1_STRING_TABLE const *b = b_;
	return table_cmp(a, b);
}

static ASN1_STRING_TABLE *
OBJ_bsearch_table(ASN1_STRING_TABLE *key, ASN1_STRING_TABLE const *base, int num)
{
	return (ASN1_STRING_TABLE *)OBJ_bsearch_(key, base, num, sizeof(ASN1_STRING_TABLE),
	    table_cmp_BSEARCH_CMP_FN);
}

ASN1_STRING_TABLE *
ASN1_STRING_TABLE_get(int nid)
{
	int idx;
	ASN1_STRING_TABLE fnd;

	fnd.nid = nid;
	if (stable != NULL) {
		idx = sk_ASN1_STRING_TABLE_find(stable, &fnd);
		if (idx >= 0)
			return sk_ASN1_STRING_TABLE_value(stable, idx);
	}
	return OBJ_bsearch_table(&fnd, tbl_standard,
	    sizeof(tbl_standard) / sizeof(tbl_standard[0]));
}
LCRYPTO_ALIAS(ASN1_STRING_TABLE_get);

/*
 * Return a string table pointer which can be modified: either directly
 * from table or a copy of an internal value added to the table.
 */

static ASN1_STRING_TABLE *
stable_get(int nid)
{
	ASN1_STRING_TABLE *tmp, *rv;

	/* Always need a string table so allocate one if NULL */
	if (stable == NULL) {
		stable = sk_ASN1_STRING_TABLE_new(sk_table_cmp);
		if (stable == NULL)
			return NULL;
	}
	tmp = ASN1_STRING_TABLE_get(nid);
	if (tmp != NULL && (tmp->flags & STABLE_FLAGS_MALLOC) != 0)
		return tmp;

	if ((rv = calloc(1, sizeof(*rv))) == NULL) {
		ASN1error(ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	if (!sk_ASN1_STRING_TABLE_push(stable, rv)) {
		free(rv);
		return NULL;
	}
	if (tmp != NULL) {
		rv->nid = tmp->nid;
		rv->minsize = tmp->minsize;
		rv->maxsize = tmp->maxsize;
		rv->mask = tmp->mask;
		rv->flags = tmp->flags | STABLE_FLAGS_MALLOC;
	} else {
		rv->nid = nid;
		rv->minsize = -1;
		rv->maxsize = -1;
		rv->flags = STABLE_FLAGS_MALLOC;
	}
	return rv;
}

int
ASN1_STRING_TABLE_add(int nid, long minsize, long maxsize, unsigned long mask,
    unsigned long flags)
{
	ASN1_STRING_TABLE *tmp;

	if ((tmp = stable_get(nid)) == NULL) {
		ASN1error(ERR_R_MALLOC_FAILURE);
		return 0;
	}
	if (minsize >= 0)
		tmp->minsize = minsize;
	if (maxsize >= 0)
		tmp->maxsize = maxsize;
	if (mask != 0)
		tmp->mask = mask;
	if (flags != 0)
		tmp->flags = flags | STABLE_FLAGS_MALLOC;

	return 1;
}
LCRYPTO_ALIAS(ASN1_STRING_TABLE_add);

void
ASN1_STRING_TABLE_cleanup(void)
{
	STACK_OF(ASN1_STRING_TABLE) *tmp;

	tmp = stable;
	if (tmp == NULL)
		return;
	stable = NULL;
	sk_ASN1_STRING_TABLE_pop_free(tmp, st_free);
}
LCRYPTO_ALIAS(ASN1_STRING_TABLE_cleanup);

static void
st_free(ASN1_STRING_TABLE *tbl)
{
	if (tbl->flags & STABLE_FLAGS_MALLOC)
		free(tbl);
}
