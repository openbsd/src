/* $OpenBSD: ec_curve.c,v 1.62 2026/11/06 13:15:01 beck Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2010 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/objects.h>

#include "ec_curve_tables.h"
#include "ec_local.h"
#include "err_local.h"

static const struct ec_curve {
	const char *comment;
	int nid;
	const EC_GROUP *group;
} ec_curve_list[] = {
	/* secg curves */
	{
		.comment = "SECG curve secp224k1",
		.nid = NID_secp224k1,
		.group = &secp224k1_group,
	},
	{
		.comment = "NIST/SECG curve secp224r1",
		.nid = NID_secp224r1,
		.group = &secp224r1_group,
	},
	{
		.comment = "SECG curve secp256k1",
		.nid = NID_secp256k1,
		.group = &secp256k1_group,
	},
	{
		/* Everyone except OpenSSL calls this secp256r1 or P-256. */
		.comment = "X9.62/SECG curve prime256v1",
		.nid = NID_X9_62_prime256v1,
		.group = &prime256v1_group,
	},
	{
		.comment = "NIST/SECG curve secp384r1",
		.nid = NID_secp384r1,
		.group = &secp384r1_group,
	},
	{
		.comment = "NIST/SECG curve secp521r1",
		.nid = NID_secp521r1,
		.group = &secp521r1_group,
	},
	/* RFC 5639 curves */
	{
		.comment = "RFC 5639 curve brainpoolP224r1",
		.nid = NID_brainpoolP224r1,
		.group = &brainpoolp224r1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP224t1",
		.nid = NID_brainpoolP224t1,
		.group = &brainpoolp224t1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP256r1",
		.nid = NID_brainpoolP256r1,
		.group = &brainpoolp256r1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP256t1",
		.nid = NID_brainpoolP256t1,
		.group = &brainpoolp256t1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP320r1",
		.nid = NID_brainpoolP320r1,
		.group = &brainpoolp320r1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP320t1",
		.nid = NID_brainpoolP320t1,
		.group = &brainpoolp320t1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP384r1",
		.nid = NID_brainpoolP384r1,
		.group = &brainpoolp384r1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP384t1",
		.nid = NID_brainpoolP384t1,
		.group = &brainpoolp384t1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP512r1",
		.nid = NID_brainpoolP512r1,
		.group = &brainpoolp512r1_group,
	},
	{
		.comment = "RFC 5639 curve brainpoolP512t1",
		.nid = NID_brainpoolP512t1,
		.group = &brainpoolp512t1_group,
	},
	/* ANSSI */
	{
		.comment = "ANSSI curve FRP256v1",
		.nid = NID_FRP256v1,
		.group = &frp256v1_group,
	},
};

#define EC_CURVE_LIST_LENGTH (sizeof(ec_curve_list) / sizeof(ec_curve_list[0]))

EC_GROUP *
EC_GROUP_new_by_curve_name(int nid)
{
	size_t i;

	if (nid <= 0)
		return NULL;

	for (i = 0; i < EC_CURVE_LIST_LENGTH; i++) {
		if (ec_curve_list[i].nid == nid)
			return EC_GROUP_dup(ec_curve_list[i].group);
	}

	ECerror(EC_R_UNKNOWN_GROUP);
	return NULL;
}
LCRYPTO_ALIAS(EC_GROUP_new_by_curve_name);

/*
 * The seed is optional and not used for computation, but must match if both
 * groups have one.
 */
static int
ec_group_seed_matches(const EC_GROUP *a, const EC_GROUP *b)
{
	size_t a_len, b_len;

	a_len = EC_GROUP_get_seed_len(a);
	b_len = EC_GROUP_get_seed_len(b);

	if (a_len == 0 || b_len == 0)
		return 1;
	if (a_len != b_len)
		return 0;

	return memcmp(EC_GROUP_get0_seed(a), EC_GROUP_get0_seed(b), a_len) == 0;
}

int
ec_group_is_builtin_curve(const EC_GROUP *group, int *out_nid)
{
	const EC_GROUP *builtin;
	BN_CTX *ctx;
	size_t i;
	int ret = 0;

	*out_nid = NID_undef;

	if ((ctx = BN_CTX_new()) == NULL)
		goto err;

	for (i = 0; i < EC_CURVE_LIST_LENGTH; i++) {
		builtin = ec_curve_list[i].group;
		if (EC_GROUP_cmp(group, builtin, ctx) != 0)
			continue;
		if (!ec_group_seed_matches(group, builtin))
			continue;
		*out_nid = ec_curve_list[i].nid;
		ret = 1;
		break;
	}

 err:
	BN_CTX_free(ctx);

	return ret;
}

size_t
EC_get_builtin_curves(EC_builtin_curve *curves, size_t nitems)
{
	size_t i;

	if (curves == NULL || nitems == 0)
		return EC_CURVE_LIST_LENGTH;

	if (nitems > EC_CURVE_LIST_LENGTH)
		nitems = EC_CURVE_LIST_LENGTH;

	for (i = 0; i < nitems; i++) {
		curves[i].nid = ec_curve_list[i].nid;
		curves[i].comment = ec_curve_list[i].comment;
	}

	return EC_CURVE_LIST_LENGTH;
}
LCRYPTO_ALIAS(EC_get_builtin_curves);

static const struct {
	const char *name;
	int nid;
} nist_curves[] = {
	{ "B-163", NID_sect163r2 },
	{ "B-233", NID_sect233r1 },
	{ "B-283", NID_sect283r1 },
	{ "B-409", NID_sect409r1 },
	{ "B-571", NID_sect571r1 },
	{ "K-163", NID_sect163k1 },
	{ "K-233", NID_sect233k1 },
	{ "K-283", NID_sect283k1 },
	{ "K-409", NID_sect409k1 },
	{ "K-571", NID_sect571k1 },
	{ "P-192", NID_X9_62_prime192v1 },
	{ "P-224", NID_secp224r1 },
	{ "P-256", NID_X9_62_prime256v1 },
	{ "P-384", NID_secp384r1 },
	{ "P-521", NID_secp521r1 }
};

const char *
EC_curve_nid2nist(int nid)
{
	size_t i;

	for (i = 0; i < sizeof(nist_curves) / sizeof(nist_curves[0]); i++) {
		if (nist_curves[i].nid == nid)
			return nist_curves[i].name;
	}

	return NULL;
}
LCRYPTO_ALIAS(EC_curve_nid2nist);

int
EC_curve_nist2nid(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(nist_curves) / sizeof(nist_curves[0]); i++) {
		if (strcmp(nist_curves[i].name, name) == 0)
			return nist_curves[i].nid;
	}

	return NID_undef;
}
LCRYPTO_ALIAS(EC_curve_nist2nid);
