/* $OpenBSD: tls_key_share.c,v 1.10 2026/01/01 12:47:52 tb Exp $ */
/*
 * Copyright (c) 2020, 2021 Joel Sing <jsing@openbsd.org>
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
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/mlkem.h>

#include "bytestring.h"
#include "ssl_local.h"
#include "tls_internal.h"

struct tls_key_share {
	int nid;
	uint16_t group_id;
	size_t key_bits;

	DH *dhe;
	DH *dhe_peer;

	EC_KEY *ecdhe;
	EC_KEY *ecdhe_peer;

	uint8_t *x25519_public;
	uint8_t *x25519_private;
	uint8_t *x25519_peer_public;

	uint8_t *mlkem_public;
	size_t mlkem_public_len;
	MLKEM_private_key *mlkem_private;
	MLKEM_public_key *mlkem_peer_public;

	/* The ciphertext from MLKEM_encap. */
	uint8_t *mlkem_encap;
	size_t mlkem_encap_len;

	/* The shared secret from an ML-KEM encapsulation. */
	uint8_t *mlkem_shared_secret;
	size_t mlkem_shared_secret_len;
};

static struct tls_key_share *
tls_key_share_new_internal(int nid, uint16_t group_id)
{
	struct tls_key_share *ks;

	if ((ks = calloc(1, sizeof(struct tls_key_share))) == NULL)
		return NULL;

	ks->group_id = group_id;
	ks->nid = nid;

	return ks;
}

struct tls_key_share *
tls_key_share_new(uint16_t group_id)
{
	int nid;

	if (!tls1_ec_group_id2nid(group_id, &nid))
		return NULL;

	return tls_key_share_new_internal(nid, group_id);
}

struct tls_key_share *
tls_key_share_new_nid(int nid)
{
	uint16_t group_id = 0;

	if (nid != NID_dhKeyAgreement) {
		if (!tls1_ec_nid2group_id(nid, &group_id))
			return NULL;
	}

	return tls_key_share_new_internal(nid, group_id);
}

void
tls_key_share_free(struct tls_key_share *ks)
{
	if (ks == NULL)
		return;

	DH_free(ks->dhe);
	DH_free(ks->dhe_peer);

	EC_KEY_free(ks->ecdhe);
	EC_KEY_free(ks->ecdhe_peer);

	freezero(ks->x25519_public, X25519_KEY_LENGTH);
	freezero(ks->x25519_private, X25519_KEY_LENGTH);
	freezero(ks->x25519_peer_public, X25519_KEY_LENGTH);

	freezero(ks->mlkem_public, ks->mlkem_public_len);
	MLKEM_private_key_free(ks->mlkem_private);
	MLKEM_public_key_free(ks->mlkem_peer_public);
	freezero(ks->mlkem_encap, ks->mlkem_encap_len);
	freezero(ks->mlkem_shared_secret, ks->mlkem_shared_secret_len);

	freezero(ks, sizeof(*ks));
}

uint16_t
tls_key_share_group(struct tls_key_share *ks)
{
	return ks->group_id;
}

int
tls_key_share_nid(struct tls_key_share *ks)
{
	return ks->nid;
}

void
tls_key_share_set_key_bits(struct tls_key_share *ks, size_t key_bits)
{
	ks->key_bits = key_bits;
}

int
tls_key_share_set_dh_params(struct tls_key_share *ks, DH *dh_params)
{
	if (ks->nid != NID_dhKeyAgreement)
		return 0;
	if (ks->dhe != NULL || ks->dhe_peer != NULL)
		return 0;

	if ((ks->dhe = DHparams_dup(dh_params)) == NULL)
		return 0;
	if ((ks->dhe_peer = DHparams_dup(dh_params)) == NULL)
		return 0;

	return 1;
}

int
tls_key_share_peer_pkey(struct tls_key_share *ks, EVP_PKEY *pkey)
{
	if (ks->nid == NID_dhKeyAgreement && ks->dhe_peer != NULL)
		return EVP_PKEY_set1_DH(pkey, ks->dhe_peer);

	if (ks->nid == NID_X25519 && ks->x25519_peer_public != NULL)
		return ssl_kex_dummy_ecdhe_x25519(pkey);

	if (ks->ecdhe_peer != NULL)
		return EVP_PKEY_set1_EC_KEY(pkey, ks->ecdhe_peer);

	return 0;
}

static int
tls_key_share_generate_dhe(struct tls_key_share *ks)
{
	/*
	 * If auto params are not being used then we must already have DH
	 * parameters set.
	 */
	if (ks->key_bits == 0) {
		if (ks->dhe == NULL)
			return 0;

		return ssl_kex_generate_dhe(ks->dhe, ks->dhe);
	}

	if (ks->dhe != NULL || ks->dhe_peer != NULL)
		return 0;

	if ((ks->dhe = DH_new()) == NULL)
		return 0;
	if (!ssl_kex_generate_dhe_params_auto(ks->dhe, ks->key_bits))
		return 0;
	if ((ks->dhe_peer = DHparams_dup(ks->dhe)) == NULL)
		return 0;

	return 1;
}

static int
tls_key_share_generate_ecdhe_ecp(struct tls_key_share *ks)
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
tls_key_share_generate_x25519(struct tls_key_share *ks)
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

static int
tls_key_share_generate_mlkem(struct tls_key_share *ks, int rank)
{
	MLKEM_private_key *private = NULL;
	uint8_t *public = NULL;
	size_t p_len = 0;
	int ret = 0;

	if (ks->mlkem_public != NULL || ks->mlkem_private != NULL)
		goto err;

	if ((private = MLKEM_private_key_new(rank)) == NULL)
		goto err;

	if (!MLKEM_generate_key(private, &public, &p_len, NULL, NULL))
		goto err;

	ks->mlkem_public = public;
	ks->mlkem_public_len = p_len;
	ks->mlkem_private = private;
	public = NULL;
	private = NULL;

	ret = 1;

 err:
	freezero(public, p_len);
	MLKEM_private_key_free(private);

	return ret;
}

static int
tls_key_share_client_generate_mlkem768x25519(struct tls_key_share *ks)
{
	if (!tls_key_share_generate_mlkem(ks, MLKEM768_RANK))
		return 0;

	if (!tls_key_share_generate_x25519(ks))
		return 0;

	return 1;
}

static int
tls_key_share_server_generate_mlkem768x25519(struct tls_key_share *ks)
{
	if (ks->mlkem_private != NULL)
		return 0;

	/* The server side needs the client's parsed share */

	if (ks->x25519_peer_public == NULL)
		return 0;

	if (ks->mlkem_peer_public == NULL)
		return 0;

	if (!tls_key_share_generate_x25519(ks))
		return 0;

	return MLKEM_encap(ks->mlkem_peer_public, &ks->mlkem_encap,
	    &ks->mlkem_encap_len, &ks->mlkem_shared_secret,
	    &ks->mlkem_shared_secret_len);
}

static int
tls_key_share_generate(struct tls_key_share *ks)
{
	if (ks->nid == NID_dhKeyAgreement)
		return tls_key_share_generate_dhe(ks);

	if (ks->nid == NID_X25519)
		return tls_key_share_generate_x25519(ks);

	return tls_key_share_generate_ecdhe_ecp(ks);
}

int
tls_key_share_client_generate(struct tls_key_share *ks)
{
	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_client_generate_mlkem768x25519(ks);

	return tls_key_share_generate(ks);
}

int
tls_key_share_server_generate(struct tls_key_share *ks)
{
	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_server_generate_mlkem768x25519(ks);

	return tls_key_share_generate(ks);
}

static int
tls_key_share_params_dhe(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->dhe == NULL)
		return 0;

	return ssl_kex_params_dhe(ks->dhe, cbb);
}

int
tls_key_share_params(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->nid == NID_dhKeyAgreement)
		return tls_key_share_params_dhe(ks, cbb);

	return 0;
}

static int
tls_key_share_public_dhe(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->dhe == NULL)
		return 0;

	return ssl_kex_public_dhe(ks->dhe, cbb);
}

static int
tls_key_share_public_ecdhe_ecp(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->ecdhe == NULL)
		return 0;

	return ssl_kex_public_ecdhe_ecp(ks->ecdhe, cbb);
}

static int
tls_key_share_public_x25519(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->x25519_public == NULL)
		return 0;

	return CBB_add_bytes(cbb, ks->x25519_public, X25519_KEY_LENGTH);
}

static int
tls_key_share_public_mlkem768x25519(struct tls_key_share *ks, CBB *cbb)
{
	uint8_t *mlkem_part;
	size_t mlkem_part_len;

	if (ks->x25519_public == NULL)
		return 0;

	/*
	 * https://datatracker.ietf.org/doc/draft-ietf-tls-ecdhe-mlkem/
	 * Section 3.1.2:
	 * The server's key exchange value is the concatenation of an
	 * ML-KEM ciphertext returned from encapsulation to the client's
	 * encapsulation key, and the server's ephemeral X25519 share.
	 */
	mlkem_part = ks->mlkem_encap;
	mlkem_part_len = ks->mlkem_encap_len;

	/*
	 * https://datatracker.ietf.org/doc/draft-ietf-tls-ecdhe-mlkem/
	 * Section 3.1.1:
	 * The client's key_exchange value is the concatenation of the
	 * client's ML-KEM-768 encapsulation key and the client's X25519
	 * ephemeral share.
	 */
	if (mlkem_part == NULL) {
		mlkem_part = ks->mlkem_public;
		mlkem_part_len = ks->mlkem_public_len;
	}

	if (mlkem_part == NULL)
		return 0;

	if (!CBB_add_bytes(cbb, mlkem_part, mlkem_part_len))
		return 0;

	/* Both the client and server send their x25519 public keys. */
	return CBB_add_bytes(cbb, ks->x25519_public, X25519_KEY_LENGTH);
}

int
tls_key_share_public(struct tls_key_share *ks, CBB *cbb)
{
	if (ks->nid == NID_dhKeyAgreement)
		return tls_key_share_public_dhe(ks, cbb);

	if (ks->nid == NID_X25519)
		return tls_key_share_public_x25519(ks, cbb);

	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_public_mlkem768x25519(ks, cbb);

	return tls_key_share_public_ecdhe_ecp(ks, cbb);
}

static int
tls_key_share_peer_params_dhe(struct tls_key_share *ks, CBS *cbs,
    int *decode_error, int *invalid_params)
{
	if (ks->dhe != NULL || ks->dhe_peer != NULL)
		return 0;

	if ((ks->dhe_peer = DH_new()) == NULL)
		return 0;
	if (!ssl_kex_peer_params_dhe(ks->dhe_peer, cbs, decode_error,
	    invalid_params))
		return 0;
	if ((ks->dhe = DHparams_dup(ks->dhe_peer)) == NULL)
		return 0;

	return 1;
}

int
tls_key_share_peer_params(struct tls_key_share *ks, CBS *cbs,
    int *decode_error, int *invalid_params)
{
	if (ks->nid != NID_dhKeyAgreement)
		return 0;

	return tls_key_share_peer_params_dhe(ks, cbs, decode_error,
	    invalid_params);
}

static int
tls_key_share_peer_public_dhe(struct tls_key_share *ks, CBS *cbs,
    int *decode_error, int *invalid_key)
{
	if (ks->dhe_peer == NULL)
		return 0;

	return ssl_kex_peer_public_dhe(ks->dhe_peer, cbs, decode_error,
	    invalid_key);
}

static int
tls_key_share_peer_public_ecdhe_ecp(struct tls_key_share *ks, CBS *cbs)
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
tls_key_share_peer_public_x25519(struct tls_key_share *ks, CBS *cbs,
    int *decode_error)
{
	size_t out_len;

	*decode_error = 0;

	if (ks->x25519_peer_public != NULL)
		return 0;

	if (CBS_len(cbs) != X25519_KEY_LENGTH) {
		*decode_error = 1;
		return 0;
	}

	return CBS_stow(cbs, &ks->x25519_peer_public, &out_len);
}

static int
tls_key_share_client_peer_public_mlkem768x25519(struct tls_key_share *ks,
    CBS *cbs, int *decode_error)
{
	CBS x25519_cbs, mlkem_ciphertext_cbs;
	size_t out_len;

	if (ks->mlkem_shared_secret != NULL)
		return 0;

	if (ks->mlkem_private == NULL)
		return 0;

	if (!CBS_get_bytes(cbs, &mlkem_ciphertext_cbs,
	    MLKEM_private_key_ciphertext_length(ks->mlkem_private)))
		return 0;

	if (!CBS_get_bytes(cbs, &x25519_cbs, X25519_KEY_LENGTH))
		return 0;

	if (CBS_len(cbs) != 0)
		return 0;

	if (!CBS_stow(&x25519_cbs, &ks->x25519_peer_public, &out_len))
		return 0;

	if (!CBS_stow(&mlkem_ciphertext_cbs, &ks->mlkem_encap, &ks->mlkem_encap_len))
		return 0;

	return 1;
}

static int
tls_key_share_server_peer_public_mlkem768x25519(struct tls_key_share *ks,
    CBS *cbs, int *decode_error)
{
	CBS x25519_cbs, mlkem768_cbs;
	size_t out_len;

	*decode_error = 0;

	/* The server should not have an mlkem private key */
	if (ks->mlkem_private != NULL)
		return 0;

	if (ks->mlkem_shared_secret != NULL)
		return 0;

	if (ks->mlkem_peer_public != NULL)
		return 0;

	if (ks->x25519_peer_public != NULL)
		return 0;

	/* Nein, ist nur normal (1024 ist gigantisch) */
	if ((ks->mlkem_peer_public = MLKEM_public_key_new(MLKEM768_RANK)) == NULL)
		goto err;

	if (!CBS_get_bytes(cbs, &mlkem768_cbs,
	    MLKEM_public_key_encoded_length(ks->mlkem_peer_public)))
		goto err;

	if (!CBS_get_bytes(cbs, &x25519_cbs, X25519_KEY_LENGTH))
		goto err;

	if (CBS_len(cbs) != 0)
		goto err;

	if (!CBS_stow(&x25519_cbs, &ks->x25519_peer_public, &out_len))
		goto err;

	/* Poetische */
	if (!MLKEM_parse_public_key(ks->mlkem_peer_public,
	    CBS_data(&mlkem768_cbs), CBS_len(&mlkem768_cbs)))
		goto err;

	return 1;

 err:
	*decode_error = 1;

	return 0;
}

static int
tls_key_share_peer_public(struct tls_key_share *ks, CBS *cbs, int *decode_error,
    int *invalid_key)
{
	*decode_error = 0;

	if (invalid_key != NULL)
		*invalid_key = 0;

	if (ks->nid == NID_dhKeyAgreement)
		return tls_key_share_peer_public_dhe(ks, cbs, decode_error,
		    invalid_key);

	if (ks->nid == NID_X25519)
		return tls_key_share_peer_public_x25519(ks, cbs, decode_error);

	return tls_key_share_peer_public_ecdhe_ecp(ks, cbs);
}

/* Called from client to process a server peer */
int
tls_key_share_client_peer_public(struct tls_key_share *ks, CBS *cbs,
    int *decode_error, int *invalid_key)
{
	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_client_peer_public_mlkem768x25519(ks, cbs,
		    decode_error);

	return tls_key_share_peer_public(ks, cbs, decode_error, invalid_key);
}

/* Called from server to process a client peer */
int
tls_key_share_server_peer_public(struct tls_key_share *ks, CBS *cbs,
    int *decode_error, int *invalid_key)
{
	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_server_peer_public_mlkem768x25519(ks, cbs,
		    decode_error);

	return tls_key_share_peer_public(ks, cbs, decode_error, invalid_key);
}

static int
tls_key_share_derive_dhe(struct tls_key_share *ks,
    uint8_t **shared_key, size_t *shared_key_len)
{
	if (ks->dhe == NULL || ks->dhe_peer == NULL)
		return 0;

	return ssl_kex_derive_dhe(ks->dhe, ks->dhe_peer, shared_key,
	    shared_key_len);
}

static int
tls_key_share_derive_ecdhe_ecp(struct tls_key_share *ks,
    uint8_t **shared_key, size_t *shared_key_len)
{
	if (ks->ecdhe == NULL || ks->ecdhe_peer == NULL)
		return 0;

	return ssl_kex_derive_ecdhe_ecp(ks->ecdhe, ks->ecdhe_peer,
	    shared_key, shared_key_len);
}

static int
tls_key_share_derive_x25519(struct tls_key_share *ks,
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

/*
 * https://datatracker.ietf.org/doc/draft-ietf-tls-ecdhe-mlkem/
 * Section 3.1.3:
 * For X25519MLKEM768, the shared secret is the concatenation of the ML-KEM
 * shared secret and the X25519 shared secret.
 */
static int
tls_key_share_derive_mlkem768x25519(struct tls_key_share *ks,
    uint8_t **out_shared_key, size_t *out_shared_key_len)
{
	uint8_t *x25519_shared_key;
	CBB cbb;

	memset(&cbb, 0, sizeof(cbb));

	if (ks->x25519_private == NULL)
		goto err;

	if (ks->x25519_peer_public == NULL)
		goto err;

	if (ks->mlkem_shared_secret == NULL) {
		if (ks->mlkem_private == NULL)
			goto err;

		if (ks->mlkem_encap == NULL)
			goto err;

		if (!MLKEM_decap(ks->mlkem_private, ks->mlkem_encap,
		    MLKEM_private_key_ciphertext_length(ks->mlkem_private),
		    &ks->mlkem_shared_secret, &ks->mlkem_shared_secret_len))
			goto err;
	}

	if (!CBB_init(&cbb,  ks->mlkem_shared_secret_len + X25519_KEY_LENGTH))
		goto err;

	if (!CBB_add_bytes(&cbb, ks->mlkem_shared_secret,
	    ks->mlkem_shared_secret_len))
		goto err;

	if (!CBB_add_space(&cbb, &x25519_shared_key, X25519_KEY_LENGTH))
		goto err;

	if (!X25519(x25519_shared_key, ks->x25519_private,
	    ks->x25519_peer_public))
		goto err;

	if (!CBB_finish(&cbb, out_shared_key, out_shared_key_len))
		goto err;

	return 1;

 err:
	CBB_cleanup(&cbb);

	return 0;
}

int
tls_key_share_derive(struct tls_key_share *ks, uint8_t **shared_key,
    size_t *shared_key_len)
{
	if (*shared_key != NULL)
		return 0;

	*shared_key_len = 0;

	if (ks->nid == NID_dhKeyAgreement)
		return tls_key_share_derive_dhe(ks, shared_key,
		    shared_key_len);

	if (ks->nid == NID_X25519)
		return tls_key_share_derive_x25519(ks, shared_key,
		    shared_key_len);

	if (ks->nid == NID_X25519MLKEM768)
		return tls_key_share_derive_mlkem768x25519(ks, shared_key,
		    shared_key_len);

	return tls_key_share_derive_ecdhe_ecp(ks, shared_key,
	    shared_key_len);
}

int
tls_key_share_peer_security(const SSL *ssl, struct tls_key_share *ks)
{
	switch (ks->nid) {
	case NID_dhKeyAgreement:
		return ssl_security_dh(ssl, ks->dhe_peer);
	default:
		return 0;
	}
}
