/* $OpenBSD: tls13_key_share.c,v 1.6 2020/04/18 14:07:56 jsing Exp $ */
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

#include <openssl/curve25519.h>

#include "bytestring.h"
#include "ssl_locl.h"
#include "tls13_internal.h"

struct tls13_key_share {
	int nid;
	uint16_t group_id;

	EC_KEY *ecdhe;
	EC_KEY *ecdhe_peer;

	uint8_t *x25519_public;
	uint8_t *x25519_private;
	uint8_t *x25519_peer_public;
};

struct tls13_key_share *
tls13_key_share_new(uint16_t group_id)
{
	struct tls13_key_share *ks;
	int nid;

	if ((nid = tls1_ec_curve_id2nid(group_id)) == 0)
		return NULL;

	if ((ks = calloc(1, sizeof(struct tls13_key_share))) == NULL)
		return NULL;

	ks->group_id = group_id;
	ks->nid = nid;

	return ks;
}

struct tls13_key_share *
tls13_key_share_new_nid(int nid)
{
	uint16_t group_id;

	if ((group_id = tls1_ec_nid2curve_id(nid)) == 0)
		return NULL;

	return tls13_key_share_new(group_id);
}

void
tls13_key_share_free(struct tls13_key_share *ks)
{
	if (ks == NULL)
		return;

	EC_KEY_free(ks->ecdhe);
	EC_KEY_free(ks->ecdhe_peer);

	freezero(ks->x25519_public, X25519_KEY_LENGTH);
	freezero(ks->x25519_private, X25519_KEY_LENGTH);
	freezero(ks->x25519_peer_public, X25519_KEY_LENGTH);

	freezero(ks, sizeof(*ks));
}

uint16_t
tls13_key_share_group(struct tls13_key_share *ks)
{
	return ks->group_id;
}

int
tls13_key_share_peer_pkey(struct tls13_key_share *ks, EVP_PKEY *pkey)
{
	if (ks->nid == NID_X25519 && ks->x25519_peer_public != NULL) {
		if (!ssl_kex_dummy_ecdhe_x25519(pkey))
			return 0;
	} else if (ks->ecdhe_peer != NULL) {
		if (!EVP_PKEY_set1_EC_KEY(pkey, ks->ecdhe_peer))
			return 0;
	} else {
		return 0;
	}

	return 1;
}

static int
tls13_key_share_generate_ecdhe_ecp(struct tls13_key_share *ks)
{
	EC_KEY *ecdhe = NULL;
	int ret = 0;

	if (ks->ecdhe != NULL)
		goto err;

	if ((ecdhe = EC_KEY_new()) == NULL)
		goto err;
	if (!ssl_kex_generate_ecdhe_ecp(ecdhe, ks->nid))
		goto err;

	ks->ecdhe = ecdhe;
	ecdhe = NULL;

	ret = 1;

 err:
	EC_KEY_free(ecdhe);

	return ret;
}

static int
tls13_key_share_generate_x25519(struct tls13_key_share *ks)
{
	uint8_t *public = NULL, *private = NULL;
	int ret = 0;

	if (ks->x25519_public != NULL || ks->x25519_private != NULL)
		goto err;

	if ((public = calloc(1, X25519_KEY_LENGTH)) == NULL)
		goto err;
	if ((private = calloc(1, X25519_KEY_LENGTH)) == NULL)
		goto err;

	X25519_keypair(public, private);

	ks->x25519_public = public;
	ks->x25519_private = private;
	public = NULL;
	private = NULL;

	ret = 1;

 err:
	freezero(public, X25519_KEY_LENGTH);
	freezero(private, X25519_KEY_LENGTH);

	return ret;
}

int
tls13_key_share_generate(struct tls13_key_share *ks)
{
	if (ks->nid == NID_X25519)
		return tls13_key_share_generate_x25519(ks);

	return tls13_key_share_generate_ecdhe_ecp(ks);
}

static int
tls13_key_share_public_ecdhe_ecp(struct tls13_key_share *ks, CBB *cbb)
{
	if (ks->ecdhe == NULL)
		return 0;

	return ssl_kex_public_ecdhe_ecp(ks->ecdhe, cbb);
}

static int
tls13_key_share_public_x25519(struct tls13_key_share *ks, CBB *cbb)
{
	if (ks->x25519_public == NULL)
		return 0;

	return CBB_add_bytes(cbb, ks->x25519_public, X25519_KEY_LENGTH);
}

int
tls13_key_share_public(struct tls13_key_share *ks, CBB *cbb)
{
	CBB key_exchange;

	if (!CBB_add_u16(cbb, ks->group_id))
		goto err;
	if (!CBB_add_u16_length_prefixed(cbb, &key_exchange))
		goto err;

	if (ks->nid == NID_X25519) {
		if (!tls13_key_share_public_x25519(ks, &key_exchange))
			goto err;
	} else {
		if (!tls13_key_share_public_ecdhe_ecp(ks, &key_exchange))
			goto err;
	}

	if (!CBB_flush(cbb))
		goto err;

	return 1;

 err:
	return 0;
}

static int
tls13_key_share_peer_public_ecdhe_ecp(struct tls13_key_share *ks, CBS *cbs)
{
	EC_KEY *ecdhe = NULL;
	int ret = 0;

	if (ks->ecdhe_peer != NULL)
		goto err;

	if ((ecdhe = EC_KEY_new()) == NULL)
		goto err;
	if (!ssl_kex_peer_public_ecdhe_ecp(ecdhe, ks->nid, cbs))
		goto err;

	ks->ecdhe_peer = ecdhe;
	ecdhe = NULL;

	ret = 1;

 err:
	EC_KEY_free(ecdhe);

	return ret;
}

static int
tls13_key_share_peer_public_x25519(struct tls13_key_share *ks, CBS *cbs)
{
	size_t out_len;

	if (ks->x25519_peer_public != NULL)
		return 0;

	if (CBS_len(cbs) != X25519_KEY_LENGTH)
		return 0;

	return CBS_stow(cbs, &ks->x25519_peer_public, &out_len);
}

int
tls13_key_share_peer_public(struct tls13_key_share *ks, uint16_t group,
    CBS *cbs)
{
	if (ks->group_id != group)
		return 0;

	if (ks->nid == NID_X25519) {
		if (!tls13_key_share_peer_public_x25519(ks, cbs))
			return 0;
	} else {
		if (!tls13_key_share_peer_public_ecdhe_ecp(ks, cbs))
			return 0;
	}

	return 1;
}

static int
tls13_key_share_derive_ecdhe_ecp(struct tls13_key_share *ks,
    uint8_t **shared_key, size_t *shared_key_len)
{
	if (ks->ecdhe == NULL || ks->ecdhe_peer == NULL)
		return 0;

	return ssl_kex_derive_ecdhe_ecp(ks->ecdhe, ks->ecdhe_peer,
	    shared_key, shared_key_len);
}

static int
tls13_key_share_derive_x25519(struct tls13_key_share *ks,
    uint8_t **shared_key, size_t *shared_key_len)
{
	uint8_t *sk = NULL;
	int ret = 0;

	if (ks->x25519_private == NULL || ks->x25519_peer_public == NULL)
		goto err;

	if ((sk = calloc(1, X25519_KEY_LENGTH)) == NULL)
		goto err;
	if (!X25519(sk, ks->x25519_private, ks->x25519_peer_public))
		goto err;

	*shared_key = sk;
	*shared_key_len = X25519_KEY_LENGTH;
	sk = NULL;

	ret = 1;

 err:
	freezero(sk, X25519_KEY_LENGTH);

	return ret;
}

int
tls13_key_share_derive(struct tls13_key_share *ks, uint8_t **shared_key,
    size_t *shared_key_len)
{
	if (*shared_key != NULL)
		return 0;

	*shared_key_len = 0;

	if (ks->nid == NID_X25519)
		return tls13_key_share_derive_x25519(ks, shared_key,
		    shared_key_len);

	return tls13_key_share_derive_ecdhe_ecp(ks, shared_key,
	    shared_key_len);
}
