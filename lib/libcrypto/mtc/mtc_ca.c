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

static int
range_cmp(const struct mtc_serial_range *a, const struct mtc_serial_range *b)
{
	if (a->start != b->start)
		return a->start < b->start ? -1 : 1;

	return 0;
}

RB_PROTOTYPE_STATIC(mtc_range_tree, mtc_serial_range, entry, range_cmp);
RB_GENERATE_STATIC(mtc_range_tree, mtc_serial_range, entry, range_cmp);

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
	RB_INIT(&ca->revoked);
	CBS_init(&cbs, id, id_len);
	if (!CBS_stow(&cbs, &ca->id, &ca->id_len))
		goto err;
	ca->hash = hash;
	ca->min_serial = min_serial;
	ca->max_serial = UINT64_MAX;
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
	struct mtc_serial_range *range, *next;

	if (ca == NULL)
		return;

	SLIST_FOREACH_SAFE(cosigner, &ca->cosigners, entry, next_cosigner) {
		free(cosigner->id);
		EVP_PKEY_free(cosigner->pkey);
		free(cosigner);
	}
	RB_FOREACH_SAFE(range, mtc_range_tree, &ca->revoked, next) {
		RB_REMOVE(mtc_range_tree, &ca->revoked, range);
		free(range);
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

uint64_t
mtc_serial(uint16_t log_number, uint64_t index)
{
	return ((uint64_t)log_number << 48) | index;
}

/* Returns the range with the greatest start not above serial, if any. */
static struct mtc_serial_range *
find_range(struct mtc_range_tree *tree, uint64_t serial)
{
	struct mtc_serial_range key, *range;

	key.start = serial;
	if ((range = RB_NFIND(mtc_range_tree, tree, &key)) == NULL)
		return RB_MAX(mtc_range_tree, tree);
	if (range->start == serial)
		return range;

	return RB_PREV(mtc_range_tree, tree, range);
}

int
mtc_ca_add_revoked_range(struct mtc_ca *ca, uint64_t start, uint64_t end)
{
	struct mtc_serial_range *range, *other;

	if (start >= end)
		return 0;

	if ((range = calloc(1, sizeof(*range))) == NULL)
		return 0;
	range->start = start;
	range->end = end;

	if (pthread_mutex_lock(&ca->lock) != 0) {
		free(range);
		return 0;
	}

	/* Absorb the range reaching the new one from below. */
	if ((other = find_range(&ca->revoked, start)) != NULL &&
	    other->end >= start) {
		RB_REMOVE(mtc_range_tree, &ca->revoked, other);
		range->start = other->start;
		if (other->end > range->end)
			range->end = other->end;
		free(other);
	}

	/* Absorb the ranges starting within or at the end of the new one. */
	while ((other = RB_NFIND(mtc_range_tree, &ca->revoked, range)) !=
	    NULL && other->start <= range->end) {
		RB_REMOVE(mtc_range_tree, &ca->revoked, other);
		if (other->end > range->end)
			range->end = other->end;
		free(other);
	}

	RB_INSERT(mtc_range_tree, &ca->revoked, range);

	pthread_mutex_unlock(&ca->lock);

	return 1;
}

int
mtc_ca_set_max_serial(struct mtc_ca *ca, uint64_t max_serial)
{
	if (pthread_mutex_lock(&ca->lock) != 0)
		return 0;
	ca->max_serial = max_serial;
	pthread_mutex_unlock(&ca->lock);

	return 1;
}

/* Reports revoked if the lock cannot be taken. */
int
mtc_ca_serial_is_revoked(struct mtc_ca *ca, uint64_t serial)
{
	struct mtc_serial_range *range;
	int revoked = 0;

	if (pthread_mutex_lock(&ca->lock) != 0)
		return 1;

	if (serial < ca->min_serial || serial > ca->max_serial)
		revoked = 1;
	else if ((range = find_range(&ca->revoked, serial)) != NULL &&
	    serial < range->end)
		revoked = 1;

	pthread_mutex_unlock(&ca->lock);

	return revoked;
}
