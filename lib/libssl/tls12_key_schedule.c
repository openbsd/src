/* $OpenBSD: tls12_key_schedule.c,v 1.1 2021/05/05 10:05:27 jsing Exp $ */
/*
 * Copyright (c) 2021 Joel Sing <jsing@openbsd.org>
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

#include <openssl/evp.h>

#include "bytestring.h"
#include "ssl_locl.h"

struct tls12_key_block {
	CBS client_write_mac_key;
	CBS server_write_mac_key;
	CBS client_write_key;
	CBS server_write_key;
	CBS client_write_iv;
	CBS server_write_iv;

	uint8_t *key_block;
	size_t key_block_len;
};

struct tls12_key_block *
tls12_key_block_new(void)
{
	return calloc(1, sizeof(struct tls12_key_block));
}

static void
tls12_key_block_clear(struct tls12_key_block *kb)
{
	CBS_init(&kb->client_write_mac_key, NULL, 0);
	CBS_init(&kb->server_write_mac_key, NULL, 0);
	CBS_init(&kb->client_write_key, NULL, 0);
	CBS_init(&kb->server_write_key, NULL, 0);
	CBS_init(&kb->client_write_iv, NULL, 0);
	CBS_init(&kb->server_write_iv, NULL, 0);

	freezero(kb->key_block, kb->key_block_len);
	kb->key_block = NULL;
	kb->key_block_len = 0;
}

void
tls12_key_block_free(struct tls12_key_block *kb)
{
	if (kb == NULL)
		return;

	tls12_key_block_clear(kb);

	freezero(kb, sizeof(struct tls12_key_block));
}

void
tls12_key_block_client_write(struct tls12_key_block *kb, CBS *mac_key,
    CBS *key, CBS *iv)
{
	CBS_dup(&kb->client_write_mac_key, mac_key);
	CBS_dup(&kb->client_write_key, key);
	CBS_dup(&kb->client_write_iv, iv);
}

void
tls12_key_block_server_write(struct tls12_key_block *kb, CBS *mac_key,
    CBS *key, CBS *iv)
{
	CBS_dup(&kb->server_write_mac_key, mac_key);
	CBS_dup(&kb->server_write_key, key);
	CBS_dup(&kb->server_write_iv, iv);
}

int
tls12_key_block_generate(struct tls12_key_block *kb, SSL *s,
    const EVP_AEAD *aead, const EVP_CIPHER *cipher, const EVP_MD *mac_hash)
{
	size_t mac_key_len = 0, key_len = 0, iv_len = 0;
	uint8_t *key_block = NULL;
	size_t key_block_len = 0;
	CBS cbs;

	/*
	 * Generate a TLSv1.2 key block and partition into individual secrets,
	 * as per RFC 5246 section 6.3.
	 */

	tls12_key_block_clear(kb);

	/* Must have AEAD or cipher/MAC pair. */
	if (aead == NULL && (cipher == NULL || mac_hash == NULL))
		goto err;

	if (aead != NULL) {
		key_len = EVP_AEAD_key_length(aead);

		/* AEAD fixed nonce length. */
		if (aead == EVP_aead_aes_128_gcm() ||
		    aead == EVP_aead_aes_256_gcm())
			iv_len = 4;
		else if (aead == EVP_aead_chacha20_poly1305())
			iv_len = 12;
		else
			goto err;
	} else if (cipher != NULL && mac_hash != NULL) {
		/*
		 * A negative integer return value will be detected via the
		 * EVP_MAX_* checks against the size_t variables below.
		 */
		mac_key_len = EVP_MD_size(mac_hash);
		key_len = EVP_CIPHER_key_length(cipher);
		iv_len = EVP_CIPHER_iv_length(cipher);

		/* Special handling for GOST... */
		if (EVP_MD_type(mac_hash) == NID_id_Gost28147_89_MAC)
			mac_key_len = 32;
	}

	if (mac_key_len > EVP_MAX_MD_SIZE)
		goto err;
	if (key_len > EVP_MAX_KEY_LENGTH)
		goto err;
	if (iv_len > EVP_MAX_IV_LENGTH)
		goto err;

	key_block_len = 2 * mac_key_len + 2 * key_len + 2 * iv_len;
	if ((key_block = calloc(1, key_block_len)) == NULL)
		goto err;

	if (!tls1_generate_key_block(s, key_block, key_block_len))
		goto err;

	kb->key_block = key_block;
	kb->key_block_len = key_block_len;
	key_block = NULL;
	key_block_len = 0;

	/* Partition key block into individual secrets. */
	CBS_init(&cbs, kb->key_block, kb->key_block_len);
	if (!CBS_get_bytes(&cbs, &kb->client_write_mac_key, mac_key_len))
		goto err;
	if (!CBS_get_bytes(&cbs, &kb->server_write_mac_key, mac_key_len))
		goto err;
	if (!CBS_get_bytes(&cbs, &kb->client_write_key, key_len))
		goto err;
	if (!CBS_get_bytes(&cbs, &kb->server_write_key, key_len))
		goto err;
	if (!CBS_get_bytes(&cbs, &kb->client_write_iv, iv_len))
		goto err;
	if (!CBS_get_bytes(&cbs, &kb->server_write_iv, iv_len))
		goto err;
	if (CBS_len(&cbs) != 0)
		goto err;

	return 1;

 err:
	tls12_key_block_clear(kb);
	freezero(key_block, key_block_len);

	return 0;
}
