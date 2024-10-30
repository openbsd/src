/* $OpenBSD: ec_print.c,v 1.19 2024/10/30 18:01:52 tb Exp $ */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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

#include <stdlib.h>

#include <openssl/bn.h>
#include <openssl/ec.h>

#include "ec_local.h"

BIGNUM *
EC_POINT_point2bn(const EC_GROUP *group, const EC_POINT *point,
    point_conversion_form_t form, BIGNUM *in_bn, BN_CTX *ctx)
{
	BIGNUM *bn = NULL;
	unsigned char *buf = NULL;
	size_t buf_len = 0;

	if (!ec_point_to_octets(group, point, form, &buf, &buf_len, ctx))
		goto err;
	if ((bn = BN_bin2bn(buf, buf_len, in_bn)) == NULL)
		goto err;

 err:
	freezero(buf, buf_len);

	return bn;
}
LCRYPTO_ALIAS(EC_POINT_point2bn);

EC_POINT *
EC_POINT_bn2point(const EC_GROUP *group,
    const BIGNUM *bn, EC_POINT *point, BN_CTX *ctx)
{
	unsigned char *buf = NULL;
	size_t buf_len = 0;

	/* Of course BN_bn2bin() is in no way symmetric to BN_bin2bn()... */
	if ((buf_len = BN_num_bytes(bn)) == 0)
		goto err;
	if ((buf = calloc(1, buf_len)) == NULL)
		goto err;
	if (!BN_bn2bin(bn, buf))
		goto err;
	if (!ec_point_from_octets(group, buf, buf_len, &point, NULL, ctx))
		goto err;

 err:
	freezero(buf, buf_len);

	return point;
}
LCRYPTO_ALIAS(EC_POINT_bn2point);

char *
EC_POINT_point2hex(const EC_GROUP *group, const EC_POINT *point,
    point_conversion_form_t form, BN_CTX *ctx)
{
	BIGNUM *bn;
	char *hex = NULL;

	if ((bn = EC_POINT_point2bn(group, point, form, NULL, ctx)) == NULL)
		goto err;
	if ((hex = BN_bn2hex(bn)) == NULL)
		goto err;

 err:
	BN_free(bn);

	return hex;
}
LCRYPTO_ALIAS(EC_POINT_point2hex);

EC_POINT *
EC_POINT_hex2point(const EC_GROUP *group, const char *hex,
    EC_POINT *in_point, BN_CTX *ctx)
{
	EC_POINT *point = NULL;
	BIGNUM *bn = NULL;

	if (BN_hex2bn(&bn, hex) == 0)
		goto err;
	if ((point = EC_POINT_bn2point(group, bn, in_point, ctx)) == NULL)
		goto err;

 err:
	BN_free(bn);

	return point;
}
LCRYPTO_ALIAS(EC_POINT_hex2point);
