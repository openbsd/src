/* $OpenBSD: ssl_kex.c,v 1.2 2020/04/18 14:07:56 jsing Exp $ */
/*
 * Copyright (c) 2020 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>

#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

#include "bytestring.h"

int
ssl_kex_dummy_ecdhe_x25519(EVP_PKEY *pkey)
{
	EC_GROUP *group = NULL;
	EC_POINT *point = NULL;
	EC_KEY *ec_key = NULL;
	BIGNUM *order = NULL;
	int ret = 0;

	/* Fudge up an EC_KEY that looks like X25519... */
	if ((group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1)) == NULL)
		goto err;
	if ((point = EC_POINT_new(group)) == NULL)
		goto err;
	if ((order = BN_new()) == NULL)
		goto err;
	if (!BN_set_bit(order, 252))
		goto err;
	if (!EC_GROUP_set_generator(group, point, order, NULL))
		goto err;
	EC_GROUP_set_curve_name(group, NID_X25519);
	if ((ec_key = EC_KEY_new()) == NULL)
		goto err;
	if (!EC_KEY_set_group(ec_key, group))
		goto err;
	if (!EVP_PKEY_set1_EC_KEY(pkey, ec_key))
		goto err;

	ret = 1;

 err:
	EC_GROUP_free(group);
	EC_POINT_free(point);
	EC_KEY_free(ec_key);
	BN_free(order);

	return ret;
}

int
ssl_kex_generate_ecdhe_ecp(EC_KEY *ecdh, int nid)
{
	EC_GROUP *group;
	int ret = 0;

	if ((group = EC_GROUP_new_by_curve_name(nid)) == NULL)
		goto err;

	if (!EC_KEY_set_group(ecdh, group))
		goto err;
	if (!EC_KEY_generate_key(ecdh))
		goto err;

	ret = 1;

 err:
	EC_GROUP_free(group);

	return ret;
}

int
ssl_kex_public_ecdhe_ecp(EC_KEY *ecdh, CBB *cbb)
{
	const EC_GROUP *group;
	const EC_POINT *point;
	uint8_t *ecp;
	size_t ecp_len;
	int ret = 0;

	if ((group = EC_KEY_get0_group(ecdh)) == NULL)
		goto err;
	if ((point = EC_KEY_get0_public_key(ecdh)) == NULL)
		goto err;

	if ((ecp_len = EC_POINT_point2oct(group, point,
	    POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL)) == 0)
		goto err;
	if (!CBB_add_space(cbb, &ecp, ecp_len))
		goto err;
	if ((EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
	    ecp, ecp_len, NULL)) == 0)
		goto err;

	ret = 1;

 err:
	return ret;
}

int
ssl_kex_peer_public_ecdhe_ecp(EC_KEY *ecdh, int nid, CBS *cbs)
{
	EC_GROUP *group = NULL;
	EC_POINT *point = NULL;
	int ret = 0;

	if ((group = EC_GROUP_new_by_curve_name(nid)) == NULL)
		goto err;

	if (!EC_KEY_set_group(ecdh, group))
		goto err;

	if ((point = EC_POINT_new(group)) == NULL)
		goto err;
	if (EC_POINT_oct2point(group, point, CBS_data(cbs), CBS_len(cbs),
	    NULL) == 0)
		goto err;
	if (!EC_KEY_set_public_key(ecdh, point))
		goto err;

	ret = 1;

 err:
	EC_GROUP_free(group);
	EC_POINT_free(point);

	return ret;
}

int
ssl_kex_derive_ecdhe_ecp(EC_KEY *ecdh, EC_KEY *ecdh_peer,
    uint8_t **shared_key, size_t *shared_key_len)
{
	const EC_POINT *point;
	uint8_t *sk = NULL;
	int sk_len = 0;
	int ret = 0;

	if (!EC_GROUP_check(EC_KEY_get0_group(ecdh), NULL))
		goto err;
	if (!EC_GROUP_check(EC_KEY_get0_group(ecdh_peer), NULL))
		goto err;

	if ((point = EC_KEY_get0_public_key(ecdh_peer)) == NULL)
		goto err;

	if ((sk_len = ECDH_size(ecdh)) <= 0)
		goto err;
	if ((sk = calloc(1, sk_len)) == NULL)
		goto err;

	if (ECDH_compute_key(sk, sk_len, point, ecdh, NULL) <= 0)
		goto err;

	*shared_key = sk;
	*shared_key_len = sk_len;
	sk = NULL;

	ret = 1;

 err:
	freezero(sk, sk_len);

	return ret;
}
