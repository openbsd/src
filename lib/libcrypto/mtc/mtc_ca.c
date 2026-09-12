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

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/mtc.h>

#include <openssl/bio.h>
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

static int
trusted_subtree_cmp(const struct mtc_trusted_subtree *a,
    const struct mtc_trusted_subtree *b)
{
	if (a->subtree.start != b->subtree.start)
		return a->subtree.start < b->subtree.start ? -1 : 1;
	if (a->subtree.end != b->subtree.end)
		return a->subtree.end < b->subtree.end ? -1 : 1;

	return 0;
}

RB_PROTOTYPE_STATIC(mtc_subtree_tree, mtc_trusted_subtree, entry,
    trusted_subtree_cmp);
RB_GENERATE_STATIC(mtc_subtree_tree, mtc_trusted_subtree, entry,
    trusted_subtree_cmp);

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
	SLIST_INIT(&ca->logs);
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
	struct mtc_log *log, *next_log;

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
	SLIST_FOREACH_SAFE(log, &ca->logs, entry, next_log) {
		free(log->subtrees);
		free(log);
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

static int
id_cmp(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
	if (a_len != b_len)
		return a_len < b_len ? -1 : 1;

	return memcmp(a, b, a_len);
}

int
mtc_ca_cmp(const OSSL_MTC_CA * const *a, const OSSL_MTC_CA * const *b)
{
	return id_cmp((*a)->id, (*a)->id_len, (*b)->id, (*b)->id_len);
}

int
mtc_ca_stack_add(STACK_OF(OSSL_MTC_CA) *cas, struct mtc_ca *ca)
{
	if (sk_OSSL_MTC_CA_find(cas, ca) >= 0)
		return 0;

	return sk_OSSL_MTC_CA_push(cas, ca) > 0;
}

struct mtc_ca *
mtc_ca_stack_lookup(STACK_OF(OSSL_MTC_CA) *cas, const uint8_t *id,
    size_t id_len)
{
	struct mtc_ca key;
	int idx;

	memset(&key, 0, sizeof(key));
	key.id = (uint8_t *)id;
	key.id_len = id_len;

	if ((idx = sk_OSSL_MTC_CA_find(cas, &key)) < 0)
		return NULL;

	return sk_OSSL_MTC_CA_value(cas, idx);
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

static struct mtc_log *
find_log(struct mtc_ca *ca, uint64_t log_number)
{
	struct mtc_log *log;

	SLIST_FOREACH(log, &ca->logs, entry) {
		if (log->log_number == log_number)
			return log;
	}

	return NULL;
}

static struct mtc_log *
find_or_add_log(struct mtc_ca *ca, uint64_t log_number)
{
	struct mtc_log *log;

	if ((log = find_log(ca, log_number)) != NULL)
		return log;

	if ((log = calloc(1, sizeof(*log))) == NULL)
		return NULL;
	log->log_number = log_number;
	RB_INIT(&log->tree);
	SLIST_INSERT_HEAD(&ca->logs, log, entry);

	return log;
}

static struct mtc_trusted_subtree *
find_subtree(struct mtc_log *log, struct mtc_subtree subtree)
{
	struct mtc_trusted_subtree key;

	key.subtree = subtree;

	return RB_FIND(mtc_subtree_tree, &log->tree, &key);
}

/*
 * Reads one newline-terminated line of in into buf.  Fails on a missing
 * newline or a line longer than the buffer.
 */
static int
read_line(BIO *in, char *buf, size_t buf_len)
{
	int len;

	if ((len = BIO_gets(in, buf, buf_len)) <= 0)
		return 0;
	if (buf[len - 1] != '\n')
		return 0;
	buf[len - 1] = '\0';

	return 1;
}

/* Parses a non-negative decimal integer with nothing before or after it. */
static int
parse_u64(const char *s, uint64_t *out)
{
	unsigned long long v;
	char *ep;

	if (!isdigit((unsigned char)*s))
		return 0;
	errno = 0;
	v = strtoull(s, &ep, 10);
	if (errno != 0 || *ep != '\0')
		return 0;
	*out = v;

	return 1;
}

/*
 * The landmark description of section 6.4.3: a line "<last_landmark>
 * <num_active>", then num_active + 1 lines each holding the tree size of
 * landmark last_landmark - i, strictly decreasing and at most
 * MTC_MAX_TREE_SIZE.
 */
static int
read_landmarks(BIO *in, uint64_t *out_last_landmark, uint64_t **out_sizes,
    size_t *out_size_count)
{
	char line[64], *space;
	uint64_t last_landmark, num_active, i, *sizes = NULL;
	int ret = 0;

	if (!read_line(in, line, sizeof(line)))
		goto err;
	if ((space = strchr(line, ' ')) == NULL)
		goto err;
	*space = '\0';
	if (!parse_u64(line, &last_landmark))
		goto err;
	if (!parse_u64(space + 1, &num_active))
		goto err;
	if (num_active > last_landmark)
		goto err;

	if ((sizes = reallocarray(NULL, num_active + 1, sizeof(*sizes))) ==
	    NULL)
		goto err;
	for (i = 0; i <= num_active; i++) {
		if (!read_line(in, line, sizeof(line)))
			goto err;
		if (!parse_u64(line, &sizes[i]))
			goto err;
		if (sizes[i] > MTC_MAX_TREE_SIZE)
			goto err;
		if (i > 0 && sizes[i] >= sizes[i - 1])
			goto err;
	}
	if (BIO_gets(in, line, sizeof(line)) > 0)
		goto err;

	*out_last_landmark = last_landmark;
	*out_sizes = sizes;
	*out_size_count = num_active + 1;
	sizes = NULL;

	ret = 1;

 err:
	free(sizes);

	return ret;
}

int
mtc_ca_load_landmarks(struct mtc_ca *ca, uint64_t log_number, BIO *in)
{
	struct mtc_trusted_subtree *subtrees = NULL, *prev, *ts;
	struct mtc_subtree_tree tree;
	struct mtc_subtree interval, cover[2];
	struct mtc_log *log;
	uint64_t last_landmark, *sizes = NULL;
	size_t size_count, subtree_count = 0, i, j, n;
	int locked = 0, ret = 0;

	if (!read_landmarks(in, &last_landmark, &sizes, &size_count))
		goto err;

	/* Each active landmark is covered by at most two subtrees. */
	if ((subtrees = calloc(2 * (size_count - 1), sizeof(*subtrees))) ==
	    NULL)
		goto err;
	RB_INIT(&tree);

	if (pthread_mutex_lock(&ca->lock) != 0)
		goto err;
	locked = 1;

	if ((log = find_or_add_log(ca, log_number)) == NULL)
		goto err;

	/*
	 * Landmark last_landmark - i covers [sizes[i + 1], sizes[i]).  Its
	 * subtrees are those covering that interval (section 6.4.1).
	 */
	for (i = 0; i + 1 < size_count; i++) {
		interval.start = sizes[i + 1];
		interval.end = sizes[i];
		n = mtc_find_subtrees(interval, cover);
		for (j = 0; j < n; j++) {
			ts = &subtrees[subtree_count];
			ts->landmark = last_landmark - i;
			ts->subtree = cover[j];
			if (RB_INSERT(mtc_subtree_tree, &tree, ts) != NULL)
				continue;
			if ((prev = find_subtree(log, cover[j])) != NULL &&
			    prev->hashed) {
				memcpy(ts->hash, prev->hash, sizeof(ts->hash));
				ts->hashed = 1;
			}
			subtree_count++;
		}
	}

	free(log->subtrees);
	log->subtrees = subtrees;
	log->subtree_count = subtree_count;
	log->tree = tree;
	log->last_landmark = last_landmark;
	subtrees = NULL;

	ret = 1;

 err:
	if (locked)
		pthread_mutex_unlock(&ca->lock);
	free(subtrees);
	free(sizes);

	return ret;
}

int
mtc_ca_add_subtree_hash(struct mtc_ca *ca, uint64_t log_number,
    struct mtc_subtree subtree, const uint8_t *hash, size_t hash_len)
{
	struct mtc_trusted_subtree *ts;
	struct mtc_log *log;
	int ret = 0;

	if (hash_len != (size_t)EVP_MD_size(ca->hash))
		return 0;

	if (pthread_mutex_lock(&ca->lock) != 0)
		return 0;

	if ((log = find_log(ca, log_number)) == NULL)
		goto err;
	if ((ts = find_subtree(log, subtree)) == NULL)
		goto err;

	if (ts->hashed) {
		ret = memcmp(ts->hash, hash, hash_len) == 0;
		goto err;
	}
	memcpy(ts->hash, hash, hash_len);
	ts->hashed = 1;

	ret = 1;

 err:
	pthread_mutex_unlock(&ca->lock);

	return ret;
}

int
mtc_ca_trusted_subtree_matches(struct mtc_ca *ca, uint64_t log_number,
    struct mtc_subtree subtree, const uint8_t *hash, size_t hash_len,
    int *out_found)
{
	struct mtc_trusted_subtree *ts;
	struct mtc_log *log;
	int ret = 0;

	*out_found = 0;

	if (hash_len != (size_t)EVP_MD_size(ca->hash))
		return 0;

	if (pthread_mutex_lock(&ca->lock) != 0)
		return 0;

	if ((log = find_log(ca, log_number)) != NULL &&
	    (ts = find_subtree(log, subtree)) != NULL) {
		*out_found = 1;
		ret = ts->hashed && memcmp(ts->hash, hash, hash_len) == 0;
	}

	pthread_mutex_unlock(&ca->lock);

	return ret;
}

OSSL_MTC_CA *
OSSL_MTC_CA_new(const uint8_t *ca_id, size_t ca_id_len, const EVP_MD *hash,
    uint64_t min_serial, EVP_PKEY *cosigner_pkey)
{
	if (ca_id == NULL || hash == NULL || cosigner_pkey == NULL)
		return NULL;

	return mtc_ca_new(ca_id, ca_id_len, hash, min_serial, cosigner_pkey);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_new);

void
OSSL_MTC_CA_free(OSSL_MTC_CA *ca)
{
	mtc_ca_free(ca);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_free);

int
OSSL_MTC_CA_add1_cosigner(OSSL_MTC_CA *ca, const uint8_t *id, size_t id_len,
    const char *sig_name, EVP_PKEY *pkey)
{
	if (ca == NULL || id == NULL || pkey == NULL)
		return 0;

	return mtc_ca_add_cosigner(ca, id, id_len, pkey);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_add1_cosigner);

int
OSSL_MTC_CA_add_revoked_range(OSSL_MTC_CA *ca, uint64_t start, uint64_t end)
{
	if (ca == NULL)
		return 0;

	return mtc_ca_add_revoked_range(ca, start, end);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_add_revoked_range);

int
OSSL_MTC_CA_set_max_serial(OSSL_MTC_CA *ca, uint64_t max_serial)
{
	if (ca == NULL)
		return 0;

	return mtc_ca_set_max_serial(ca, max_serial);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_set_max_serial);

int
OSSL_MTC_CA_get0_id(const OSSL_MTC_CA *ca, const uint8_t **out_id,
    size_t *out_id_len)
{
	if (ca == NULL || out_id == NULL || out_id_len == NULL)
		return 0;

	*out_id = mtc_ca_id(ca, out_id_len);

	return 1;
}
LCRYPTO_ALIAS(OSSL_MTC_CA_get0_id);

int
OSSL_MTC_CA_cmp(const OSSL_MTC_CA * const *a, const OSSL_MTC_CA * const *b)
{
	return mtc_ca_cmp(a, b);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_cmp);

int
OSSL_MTC_CA_load_landmarks(OSSL_MTC_CA *ca, uint64_t log_number, BIO *in)
{
	if (ca == NULL || in == NULL)
		return 0;

	return mtc_ca_load_landmarks(ca, log_number, in);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_load_landmarks);

int
OSSL_MTC_CA_add_subtree_hash(OSSL_MTC_CA *ca, uint64_t log_number,
    uint64_t start, uint64_t end, const uint8_t *hash, size_t hash_len)
{
	struct mtc_subtree subtree;

	if (ca == NULL || hash == NULL)
		return 0;

	subtree.start = start;
	subtree.end = end;

	return mtc_ca_add_subtree_hash(ca, log_number, subtree, hash, hash_len);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_add_subtree_hash);

uint64_t
OSSL_MTC_serial(uint16_t log_number, uint64_t index)
{
	return mtc_serial(log_number, index);
}
LCRYPTO_ALIAS(OSSL_MTC_serial);
