/*	$OpenBSD$ */
/*
 * Copyright (c) 2026, Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * A relying party's Merkle Tree CA configuration, per section 7.1 of
 * draft-ietf-plants-merkle-tree-certs-05.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "bytestring.h"
#include "mtc_internal.h"

struct mtc_ca *
mtc_ca_new(const uint8_t *id, size_t id_len, const EVP_MD *hash,
    uint64_t min_serial, EVP_PKEY *cosigner_pkey)
{
	struct mtc_ca *ca;
	CBS cbs;

	if (id_len == 0)
		return NULL;

	if ((ca = calloc(1, sizeof(*ca))) == NULL)
		goto err;
	if (pthread_mutex_init(&ca->lock, NULL) != 0) {
		free(ca);
		return NULL;
	}
	SLIST_INIT(&ca->cosigners);
	CBS_init(&cbs, id, id_len);
	if (!CBS_stow(&cbs, &ca->id, &ca->id_len))
		goto err;
	ca->hash = hash;
	ca->min_serial = min_serial;
	if (!EVP_PKEY_up_ref(cosigner_pkey))
		goto err;
	ca->cosigner_pkey = cosigner_pkey;

	return ca;

 err:
	mtc_ca_free(ca);

	return NULL;
}

void
mtc_ca_free(struct mtc_ca *ca)
{
	struct mtc_cosigner *cosigner, *next_cosigner;

	if (ca == NULL)
		return;

	SLIST_FOREACH_SAFE(cosigner, &ca->cosigners, entry, next_cosigner) {
		free(cosigner->id);
		EVP_PKEY_free(cosigner->pkey);
		free(cosigner);
	}
	free(ca->id);
	EVP_PKEY_free(ca->cosigner_pkey);
	pthread_mutex_destroy(&ca->lock);
	free(ca);
}

static int
id_equal(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
	return a_len == b_len && memcmp(a, b, a_len) == 0;
}

int
mtc_ca_add_cosigner(struct mtc_ca *ca, const uint8_t *id, size_t id_len,
    EVP_PKEY *pkey)
{
	struct mtc_cosigner *cosigner = NULL, *c;
	CBS cbs;
	int ret = 0;

	if (id_len == 0)
		return 0;

	if (pthread_mutex_lock(&ca->lock) != 0)
		return 0;

	if (id_equal(id, id_len, ca->id, ca->id_len))
		goto err;
	SLIST_FOREACH(c, &ca->cosigners, entry) {
		if (id_equal(id, id_len, c->id, c->id_len))
			goto err;
	}

	if ((cosigner = calloc(1, sizeof(*cosigner))) == NULL)
		goto err;
	CBS_init(&cbs, id, id_len);
	if (!CBS_stow(&cbs, &cosigner->id, &cosigner->id_len))
		goto err;
	if (!EVP_PKEY_up_ref(pkey))
		goto err;
	cosigner->pkey = pkey;

	SLIST_INSERT_HEAD(&ca->cosigners, cosigner, entry);
	cosigner = NULL;

	ret = 1;

 err:
	if (cosigner != NULL) {
		free(cosigner->id);
		free(cosigner);
	}
	pthread_mutex_unlock(&ca->lock);

	return ret;
}

const uint8_t *
mtc_ca_id(const struct mtc_ca *ca, size_t *out_len)
{
	*out_len = ca->id_len;

	return ca->id;
}

const EVP_MD *
mtc_ca_hash(const struct mtc_ca *ca)
{
	return ca->hash;
}

EVP_PKEY *
mtc_ca_cosigner_pkey(const struct mtc_ca *ca)
{
	return ca->cosigner_pkey;
}
