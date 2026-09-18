/*	$OpenBSD: tables.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

#include <sys/types.h>
#include <sys/tree.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"

/*
 * VRP4
 */

static inline int
vrp4cmp(struct vrp4 *a, struct vrp4 *b)
{
	int ret;

	/* Announcement order */

	/* prefix ordering */
	/* descending */
	ret = memcmp(&a->prefix, &b->prefix, sizeof(struct in_addr));
	if (ret)
		return -ret;

	/* prefix length ordering */
	/* descending */
	if (a->prefix_length > b->prefix_length)
		return -1;
	if (a->prefix_length < b->prefix_length)
		return 1;

	/* max length ordering */
	/* descending */
	if (a->max_prefix_length > b->max_prefix_length)
		return -1;
	if (a->max_prefix_length < b->max_prefix_length)
		return 1;

	/* AS ordering */
	/* descending */
	if (a->asn > b->asn)
		return -1;
	if (a->asn < b->asn)
		return 1;
	return 0;
}

RB_GENERATE(vrp4_tree, vrp4, entry, vrp4cmp)

/* 1 if error */
int
insert_vrp4(struct vrp4_tree *vrp4tree, struct vrp4 *vrp4, time_t present)
{
	struct vrp4 *node;

	assert(vrp4tree);
	assert(vrp4);

	if (vrp4->expire > 0 && present >= vrp4->expire)
		return 0;

	node = malloc(sizeof(struct vrp4));
	if (node == NULL)
		err(1, NULL);

	node->expire = vrp4->expire;
	node->prefix = vrp4->prefix;
	node->prefix_length = vrp4->prefix_length;
	node->max_prefix_length = vrp4->max_prefix_length;
	node->zero = 0;
	node->asn = vrp4->asn;

	if (RB_INSERT(vrp4_tree, vrp4tree, node) != NULL) {
		free(node);
		return 1;
	}
	return 0;
}

/* 1 if error */
int
remove_vrp4(struct vrp4_tree *vrp4tree, struct vrp4 *vrp4)
{
	struct vrp4 *node;

	assert(vrp4tree);
	assert(vrp4);

	node = RB_FIND(vrp4_tree, vrp4tree, vrp4);
	if (!node)
		return 1;

	RB_REMOVE(vrp4_tree, vrp4tree, node);
	free(node);
	return 0;
}

void
free_vrp4_tree(struct vrp4_tree *vrp4tree)
{
	struct vrp4 *vrp4, *tmpvrp4;

	if (!vrp4tree)
		return;

	RB_FOREACH_SAFE(vrp4, vrp4_tree, vrp4tree, tmpvrp4) {
		RB_REMOVE(vrp4_tree, vrp4tree, vrp4);
		free(vrp4);
	}
}

int
vrp4_treecmp(struct vrp4_tree *a, struct vrp4_tree *b)
{
	struct vrp4 *vrp4;

	assert(a);
	assert(b);

	RB_FOREACH(vrp4, vrp4_tree, a) {
		if (!RB_FIND(vrp4_tree, b, vrp4))
			return 1;
	}

	RB_FOREACH(vrp4, vrp4_tree, b) {
		if (!RB_FIND(vrp4_tree, a, vrp4))
			return -1;
	}

	return 0;
}

void
vrp4_treecpy(struct vrp4_tree *src, struct vrp4_tree *dst)
{
	struct vrp4 *vrp4, *node;

	assert(src);
	assert(dst);

	free_vrp4_tree(dst);

	RB_FOREACH(vrp4, vrp4_tree, src) {
		node = malloc(sizeof(struct vrp4));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp4->expire;
		node->prefix = vrp4->prefix;
		node->prefix_length = vrp4->prefix_length;
		node->max_prefix_length = vrp4->max_prefix_length;
		node->zero = 0;
		node->asn = vrp4->asn;
		RB_INSERT(vrp4_tree, dst, node);
	}
}

void
vrp4_treecpy_expire(struct vrp4_tree *src, struct vrp4_tree *dst,
    time_t present)
{
	struct vrp4 *vrp4, *node;

	assert(src);
	assert(dst);

	free_vrp4_tree(dst);

	RB_FOREACH(vrp4, vrp4_tree, src) {
		if (vrp4->expire > 0 && present >= vrp4->expire)
			continue;

		node = malloc(sizeof(struct vrp4));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp4->expire;
		node->prefix = vrp4->prefix;
		node->prefix_length = vrp4->prefix_length;
		node->max_prefix_length = vrp4->max_prefix_length;
		node->zero = 0;
		node->asn = vrp4->asn;
		RB_INSERT(vrp4_tree, dst, node);
	}
}

int64_t
vrp4_treecount(struct vrp4_tree *vrp4tree)
{
	struct vrp4 *vrp4;
	int64_t i = 0;

	assert(vrp4tree);

	RB_FOREACH(vrp4, vrp4_tree, vrp4tree) {
		i++;
		if (i < 0)
			break;
	}

	if (i < -1)
		i = -1;
	return i;
}

int
vrp4_treeempty(struct vrp4_tree *vrp4tree)
{
	assert(vrp4tree);

	return RB_EMPTY(vrp4tree);
}

/* update old tree with the expirations from new tree */
/* only run on identical trees */

void
vrp4_treeupdate(struct vrp4_tree *oldtree, struct vrp4_tree *newtree)
{
	struct vrp4 *oldvrp4, *newvrp4;

	assert(oldtree);
	assert(newtree);

	RB_FOREACH(oldvrp4, vrp4_tree, oldtree) {
		newvrp4 = RB_FIND(vrp4_tree, newtree, oldvrp4);
		if (newvrp4)
			oldvrp4->expire = newvrp4->expire;
	}
}

uint32_t
vrp4_treehash(struct vrp4_tree *vrp4tree)
{
	struct vrp4 *vrp4;

	uint32_t hash;

	assert(vrp4tree);

	hash = fnv32_init;

	RB_FOREACH(vrp4, vrp4_tree, vrp4tree) {
		hash = fnv32_hash(&vrp4->prefix, sizeof(vrp4->prefix), hash);
		hash = fnv32_hash(&vrp4->prefix_length,
		    sizeof(vrp4->prefix_length), hash);
		hash = fnv32_hash(&vrp4->max_prefix_length,
		    sizeof(vrp4->max_prefix_length), hash);
		hash = fnv32_hash(&vrp4->asn, sizeof(vrp4->asn), hash);
	}
	return hash;
}

void
vrp4_treecpy_count_and_hash(struct vrp4_tree *src, struct vrp4_tree *dst,
    int64_t *count, uint32_t *hash)
{
	struct vrp4 *vrp4, *node;

	assert(src);
	assert(dst);
	assert(hash);
	assert(count);

	free_vrp4_tree(dst);

	*count = 0;
	*hash = fnv32_init;

	RB_FOREACH(vrp4, vrp4_tree, src) {
		node = malloc(sizeof(struct vrp4));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp4->expire;
		node->prefix = vrp4->prefix;
		node->prefix_length = vrp4->prefix_length;
		node->max_prefix_length = vrp4->max_prefix_length;
		node->zero = 0;
		node->asn = vrp4->asn;
		RB_INSERT(vrp4_tree, dst, node);

		(*count)++;

		*hash = fnv32_hash(&vrp4->prefix, sizeof(vrp4->prefix),
		    *hash);
		*hash = fnv32_hash(&vrp4->prefix_length,
		    sizeof(vrp4->prefix_length), *hash);
		*hash = fnv32_hash(&vrp4->max_prefix_length,
		    sizeof(vrp4->max_prefix_length), *hash);
		*hash = fnv32_hash(&vrp4->asn, sizeof(vrp4->asn),
		    *hash);
	}
}

struct cache_vrp4_tree *
cache_vrp4_tree_find(struct vrp4_tree *vrp4tree)
{
	int i;
	uint8_t v;
	uint32_t hash;

	assert(vrp4tree);

	hash = vrp4_treehash(vrp4tree);

	for (v = RTR_VERSION_0; v <= RTR_MAX_VERSION; v++) {
		if (cache[v].head == -1 && cache[v].tail == -1)
			continue;

		for (i = cache[v].tail; i != cache[v].head;
		    i = (i + 1) % cache[v].frame_count) {
			if (CFRAME(v,i).rtr_vrp4s->hash == hash) {
				if (vrp4_treecmp(vrp4tree,
				    &CFRAME(v,i).rtr_vrp4s->vrp4s) == 0) {
					return CFRAME(v,i).rtr_vrp4s;
				}
			}
		}

		if (CHEAD(v).rtr_vrp4s->hash == hash) {
			if (vrp4_treecmp(vrp4tree,
			    &CHEAD(v).rtr_vrp4s->vrp4s) == 0) {
				return CHEAD(v).rtr_vrp4s;
			}
		}
	}
	return NULL;
}

struct cache_vrp4_tree *
cache_vrp4_tree_dup(struct cache_vrp4_tree *cache_vrp4)
{
	assert(cache_vrp4);

	cache_vrp4->reference_count++;

	return cache_vrp4;
}

struct cache_vrp4_tree *
cache_vrp4_tree_new(struct vrp4_tree *vrp4tree)
{
	struct cache_vrp4_tree *cache_vrp4;

	assert(vrp4tree);

	cache_vrp4 = cache_vrp4_tree_find(vrp4tree);

	if (cache_vrp4) {
		cache_vrp4 = cache_vrp4_tree_dup(cache_vrp4);
		return cache_vrp4;
	}

	cache_vrp4 = malloc(sizeof(struct cache_vrp4_tree));
	if (cache_vrp4 == NULL)
		err(1, NULL);

	RB_INIT(&cache_vrp4->vrp4s);

	cache_vrp4->reference_count = 1;
	vrp4_treecpy_count_and_hash(vrp4tree, &cache_vrp4->vrp4s,
	    &cache_vrp4->entry_count, &cache_vrp4->hash);
	rtr_gettime();
	cache_vrp4->creation_time = now.tv_sec;
	return cache_vrp4;
}

void
cache_vrp4_tree_free(struct cache_vrp4_tree *cache_vrp4)
{
	if (!cache_vrp4)
		return;

	cache_vrp4->reference_count--;

	if (cache_vrp4->reference_count < 1) {
		free_vrp4_tree(&cache_vrp4->vrp4s);
		free(cache_vrp4);
	}
}

/*
 * VRP6
 */

static inline int
vrp6cmp(struct vrp6 *a, struct vrp6 *b)
{
	int ret;

	/* Announcement order */

	/* prefix ordering */
	/* descending */
	ret = memcmp(&a->prefix, &b->prefix, sizeof(struct in6_addr));
	if (ret)
		return -ret;

	/* prefix length ordering */
	/* descending */
	if (a->prefix_length > b->prefix_length)
		return -1;
	if (a->prefix_length < b->prefix_length)
		return 1;

	/* max length ordering */
	/* descending */
	if (a->max_prefix_length > b->max_prefix_length)
		return -1;
	if (a->max_prefix_length < b->max_prefix_length)
		return 1;

	/* AS ordering */
	/* descending */
	if (a->asn > b->asn)
		return -1;
	if (a->asn < b->asn)
		return 1;
	return 0;
}

RB_GENERATE(vrp6_tree, vrp6, entry, vrp6cmp)

/* 1 if error */
int
insert_vrp6(struct vrp6_tree *vrp6tree, struct vrp6 *vrp6, time_t present)
{
	struct vrp6 *node;

	assert(vrp6tree);
	assert(vrp6);

	if (vrp6->expire > 0 && present >= vrp6->expire)
		return 0;

	node = malloc(sizeof(struct vrp6));
	if (node == NULL)
		err(1, NULL);

	node->expire = vrp6->expire;
	node->prefix = vrp6->prefix;
	node->prefix_length = vrp6->prefix_length;
	node->max_prefix_length = vrp6->max_prefix_length;
	node->zero = 0;
	node->asn = vrp6->asn;
	if (RB_INSERT(vrp6_tree, vrp6tree, node) != NULL) {
		free(node);
		return 1;
	}
	return 0;
}

/* 1 if error */
int
remove_vrp6(struct vrp6_tree *vrp6tree, struct vrp6 *vrp6)
{
	struct vrp6 *node;

	assert(vrp6tree);
	assert(vrp6);

	node = RB_FIND(vrp6_tree, vrp6tree, vrp6);

	if (!node)
		return 1;

	RB_REMOVE(vrp6_tree, vrp6tree, node);
	free(node);
	return 0;
}

void
free_vrp6_tree(struct vrp6_tree *vrp6tree)
{
	struct vrp6 *vrp6, *tmpvrp6;

	if (!vrp6tree)
		return;

	RB_FOREACH_SAFE(vrp6, vrp6_tree, vrp6tree, tmpvrp6) {
		RB_REMOVE(vrp6_tree, vrp6tree, vrp6);
		free(vrp6);
	}
}

int
vrp6_treecmp(struct vrp6_tree *a, struct vrp6_tree *b)
{
	struct vrp6 *vrp6;

	assert(a);
	assert(b);

	RB_FOREACH(vrp6, vrp6_tree, a)
		if (!RB_FIND(vrp6_tree, b, vrp6))
			return 1;

	RB_FOREACH(vrp6, vrp6_tree, b)
		if (!RB_FIND(vrp6_tree, a, vrp6))
			return -1;
	return 0;
}

void
vrp6_treecpy(struct vrp6_tree *src, struct vrp6_tree *dst)
{
	struct vrp6 *vrp6, *node;

	assert(src);
	assert(dst);

	free_vrp6_tree(dst);

	RB_FOREACH(vrp6, vrp6_tree, src) {
		node = malloc(sizeof(struct vrp6));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp6->expire;
		node->prefix = vrp6->prefix;
		node->prefix_length = vrp6->prefix_length;
		node->max_prefix_length = vrp6->max_prefix_length;
		node->zero = 0;
		node->asn = vrp6->asn;

		RB_INSERT(vrp6_tree, dst, node);
	}
}

void
vrp6_treecpy_expire(struct vrp6_tree *src, struct vrp6_tree *dst,
    time_t present)
{
	struct vrp6 *vrp6, *node;

	assert(src);
	assert(dst);

	free_vrp6_tree(dst);

	RB_FOREACH(vrp6, vrp6_tree, src) {
		if (vrp6->expire > 0 && present >= vrp6->expire)
			continue;

		node = malloc(sizeof(struct vrp6));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp6->expire;
		node->prefix = vrp6->prefix;
		node->prefix_length = vrp6->prefix_length;
		node->max_prefix_length = vrp6->max_prefix_length;
		node->zero = 0;
		node->asn = vrp6->asn;
		RB_INSERT(vrp6_tree, dst, node);
	}
}

int64_t
vrp6_treecount(struct vrp6_tree *vrp6tree)
{
	struct vrp6 *vrp6;
	int64_t i;

	assert(vrp6tree);

	i = 0;

	RB_FOREACH(vrp6, vrp6_tree, vrp6tree) {
		i++;
		if (i < 0)
			break;
	}

	if (i < -1)
		i = -1;
	return i;
}

int
vrp6_treeempty(struct vrp6_tree *vrp6tree)
{
	assert(vrp6tree);

	return RB_EMPTY(vrp6tree);
}

/* update old tree with the expirations from new tree */
/* only run on identical trees */

void
vrp6_treeupdate(struct vrp6_tree *oldtree, struct vrp6_tree *newtree)
{
	struct vrp6 *oldvrp6, *newvrp6;

	assert(oldtree);
	assert(newtree);

	RB_FOREACH(oldvrp6, vrp6_tree, oldtree) {
		newvrp6 = RB_FIND(vrp6_tree, newtree, oldvrp6);
		if (newvrp6)
			oldvrp6->expire = newvrp6->expire;
	}
}

uint32_t
vrp6_treehash(struct vrp6_tree *vrp6tree)
{
	struct vrp6 *vrp6;
	uint32_t hash = fnv32_init;

	assert(vrp6tree);

	RB_FOREACH(vrp6, vrp6_tree, vrp6tree) {
		hash = fnv32_hash(&vrp6->prefix, sizeof(vrp6->prefix),
		    hash);
		hash = fnv32_hash(&vrp6->prefix_length,
		    sizeof(vrp6->prefix_length), hash);
		hash = fnv32_hash(&vrp6->max_prefix_length,
		    sizeof(vrp6->max_prefix_length), hash);
		hash = fnv32_hash(&vrp6->asn, sizeof(vrp6->asn), hash);
	}
	return hash;
}

void
vrp6_treecpy_count_and_hash(struct vrp6_tree *src, struct vrp6_tree *dst,
    int64_t *count, uint32_t *hash)
{
	struct vrp6 *vrp6, *node;

	assert(src);
	assert(dst);
	assert(count);
	assert(hash);

	free_vrp6_tree(dst);

	*count = 0;
	*hash = fnv32_init;

	RB_FOREACH(vrp6, vrp6_tree, src) {
		node = malloc(sizeof(struct vrp6));
		if (node == NULL)
			err(1, NULL);

		node->expire = vrp6->expire;
		node->prefix = vrp6->prefix;
		node->prefix_length = vrp6->prefix_length;
		node->max_prefix_length = vrp6->max_prefix_length;
		node->zero = 0;
		node->asn = vrp6->asn;
		RB_INSERT(vrp6_tree, dst, node);

		(*count)++;

		*hash = fnv32_hash(&vrp6->prefix, sizeof(vrp6->prefix),
		    *hash);
		*hash = fnv32_hash(&vrp6->prefix_length,
		    sizeof(vrp6->prefix_length), *hash);
		*hash = fnv32_hash(&vrp6->max_prefix_length,
		    sizeof(vrp6->max_prefix_length), *hash);
		*hash = fnv32_hash(&vrp6->asn, sizeof(vrp6->asn), *hash);
	}
}

struct cache_vrp6_tree *
cache_vrp6_tree_find(struct vrp6_tree *vrp6tree)
{
	int i;
	uint8_t v;
	uint32_t hash;

	assert(vrp6tree);

	hash = vrp6_treehash(vrp6tree);

	for (v = RTR_VERSION_0; v <= RTR_MAX_VERSION; v++) {
		if (cache[v].head == -1 && cache[v].tail == -1)
			continue;

		for (i = cache[v].tail; i != cache[v].head;
		    i = (i + 1) % cache[v].frame_count) {
			if (CFRAME(v,i).rtr_vrp6s->hash == hash) {
				if (vrp6_treecmp(vrp6tree,
				    &CFRAME(v,i).rtr_vrp6s->vrp6s) == 0) {
					return CFRAME(v,i).rtr_vrp6s;
				}
			}
		}

		if (CHEAD(v).rtr_vrp6s->hash == hash) {
			if (vrp6_treecmp(vrp6tree,
			    &CHEAD(v).rtr_vrp6s->vrp6s) == 0) {
				return CHEAD(v).rtr_vrp6s;
			}
		}
	}
	return NULL;
}

struct cache_vrp6_tree *
cache_vrp6_tree_dup(struct cache_vrp6_tree *cache_vrp6)
{
	assert(cache_vrp6);

	cache_vrp6->reference_count++;

	return cache_vrp6;
}

struct cache_vrp6_tree *
cache_vrp6_tree_new(struct vrp6_tree *vrp6tree)
{
	struct cache_vrp6_tree *cache_vrp6;

	assert(vrp6tree);

	cache_vrp6 = cache_vrp6_tree_find(vrp6tree);
	if (cache_vrp6) {
		cache_vrp6 = cache_vrp6_tree_dup(cache_vrp6);
		return cache_vrp6;
	}

	cache_vrp6 = malloc(sizeof(struct cache_vrp6_tree));
	if (cache_vrp6 == NULL)
		err(1, NULL);

	RB_INIT(&cache_vrp6->vrp6s);

	cache_vrp6->reference_count = 1;
	vrp6_treecpy_count_and_hash(vrp6tree, &cache_vrp6->vrp6s,
	    &cache_vrp6->entry_count, &cache_vrp6->hash);
	rtr_gettime();
	cache_vrp6->creation_time = now.tv_sec;
	return cache_vrp6;
}

void
cache_vrp6_tree_free(struct cache_vrp6_tree *cache_vrp6)
{
	if (!cache_vrp6)
		return;

	cache_vrp6->reference_count--;

	if (cache_vrp6->reference_count < 1) {
		free_vrp6_tree(&cache_vrp6->vrp6s);
		free(cache_vrp6);
	}
}

/*
 * BRK
 */

static inline int
brkcmp(struct brk *a, struct brk *b)
{
	int ret;

	ret = memcmp(&a->ski, &b->ski, SKI_LENGTH);
	if (ret)
		return ret;

	if (a->spki_length > b->spki_length)
		return 1;
	if (a->spki_length < b->spki_length)
		return -1;

	ret = memcmp(&a->spki, &b->spki, a->spki_length);
	if (ret)
		return ret;

	if (a->asn > b->asn)
		return 1;
	if (a->asn < b->asn)
		return -1;
	return 0;
}

RB_GENERATE(brk_tree, brk, entry, brkcmp)

/* 1 if error */
int
insert_brk(struct brk_tree *brktree, struct brk *brk, time_t present)
{
	struct brk *node;

	assert(brktree);
	assert(brk);

	if (brk->expire > 0 && present >= brk->expire)
		return 0;

	if (brk->spki_length > (PDU_MAX_LENGTH - sizeof(struct pdu_router_key)))
		return 1;

	node = malloc(sizeof(struct brk) + brk->spki_length);
	if (node == NULL)
		err(1, NULL);

	node->expire = brk->expire;
	node->asn = brk->asn;
	memcpy(node->ski, brk->ski, SKI_LENGTH);
	node->spki_length = brk->spki_length;
	if (brk->spki_length > 0)
		memcpy(node->spki, brk->spki, brk->spki_length);

	if (RB_INSERT(brk_tree, brktree, node) != NULL) {
		free(node);
		return 1;
	}
	return 0;
}

/* 1 if error */
int
remove_brk(struct brk_tree *brktree, struct brk *brk)
{
	struct brk *node;

	assert(brktree);
	assert(brk);

	node = RB_FIND(brk_tree, brktree, brk);
	if (!node)
		return 1;

	RB_REMOVE(brk_tree, brktree, node);
	free(node);
	return 0;
}

void
free_brk_tree(struct brk_tree *brktree)
{
	struct brk *brk, *tmpbrk;

	if (!brktree)
		return;

	RB_FOREACH_SAFE(brk, brk_tree, brktree, tmpbrk) {
		RB_REMOVE(brk_tree, brktree, brk);
		free(brk);
	}
}

int
brk_treecmp(struct brk_tree *a, struct brk_tree *b)
{
	struct brk *brk;

	assert(a);
	assert(b);

	RB_FOREACH(brk, brk_tree, a) {
		if (!RB_FIND(brk_tree, b, brk))
			return 1;
	}

	RB_FOREACH(brk, brk_tree, b) {
		if (!RB_FIND(brk_tree, a, brk))
			return -1;
	}
	return 0;
}

void
brk_treecpy(struct brk_tree *src, struct brk_tree *dst)
{
	struct brk *brk;
	struct brk *node;

	assert(src);
	assert(dst);

	free_brk_tree(dst);

	RB_FOREACH(brk, brk_tree, src) {
		node = malloc(sizeof(struct brk) + brk->spki_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = brk->expire;
		node->asn = brk->asn;
		memcpy(node->ski, brk->ski, SKI_LENGTH);
		node->spki_length = brk->spki_length;
		if (brk->spki_length > 0)
			memcpy(node->spki, brk->spki, brk->spki_length);
		RB_INSERT(brk_tree, dst, node);
	}
}

void
brk_treecpy_expire(struct brk_tree *src, struct brk_tree *dst, time_t present)
{
	struct brk *brk, *node;

	assert(src);
	assert(dst);

	free_brk_tree(dst);

	RB_FOREACH(brk, brk_tree, src) {
		if (brk->expire > 0 && present >= brk->expire)
			continue;

		node = malloc(sizeof(struct brk) + brk->spki_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = brk->expire;
		node->asn = brk->asn;
		memcpy(node->ski, brk->ski, SKI_LENGTH);
		node->spki_length = brk->spki_length;
		if (brk->spki_length > 0)
			memcpy(node->spki, brk->spki, brk->spki_length);
		RB_INSERT(brk_tree, dst, node);
	}
}

int64_t
brk_treecount(struct brk_tree *brktree)
{
	struct brk *brk;
	int64_t i = 0;

	assert(brktree);

	RB_FOREACH(brk, brk_tree, brktree) {
		i++;
		if (i < 0)
			break;
	}

	if (i < -1)
		i = -1;
	return i;
}

int
brk_treeempty(struct brk_tree *brktree)
{
	assert(brktree);

	return RB_EMPTY(brktree);
}

/* update old tree with the expirations from new tree */
/* only run on identical trees */

void
brk_treeupdate(struct brk_tree *oldtree, struct brk_tree *newtree)
{
	struct brk *oldbrk, *newbrk;

	assert(oldtree);
	assert(newtree);

	RB_FOREACH(oldbrk, brk_tree, oldtree) {
		newbrk = RB_FIND(brk_tree, newtree, oldbrk);
		if (newbrk)
			oldbrk->expire = newbrk->expire;
	}
}

uint32_t
brk_treehash(struct brk_tree *brktree)
{
	struct brk *brk;
	uint32_t hash = fnv32_init;

	assert(brktree);

	RB_FOREACH(brk, brk_tree, brktree) {
		hash = fnv32_hash(&brk->asn, sizeof(uint32_t), hash);
		hash = fnv32_hash(&brk->ski, SKI_LENGTH, hash);
		if (brk->spki_length > 0)
			hash = fnv32_hash(brk->spki, brk->spki_length, hash);
	}
	return hash;
}

void
brk_treecpy_count_and_hash(struct brk_tree *src, struct brk_tree *dst,
    int64_t *count, uint32_t *hash)
{
	struct brk *brk, *node;

	assert(src);
	assert(dst);
	assert(count);
	assert(hash);

	free_brk_tree(dst);

	*count = 0;
	*hash = fnv32_init;

	RB_FOREACH(brk, brk_tree, src) {
		node = malloc(sizeof(struct brk) + brk->spki_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = brk->expire;
		node->asn = brk->asn;
		memcpy(node->ski, brk->ski, SKI_LENGTH);
		node->spki_length = brk->spki_length;
		if (brk->spki_length > 0)
			memcpy(node->spki, brk->spki, brk->spki_length);
		RB_INSERT(brk_tree, dst, node);

		(*count)++;

		*hash = fnv32_hash(&brk->asn, sizeof(uint32_t), *hash);
		*hash = fnv32_hash(&brk->ski, SKI_LENGTH, *hash);

		if (brk->spki_length > 0) {
			*hash = fnv32_hash(brk->spki, brk->spki_length, *hash);
		}
	}
}

struct cache_brk_tree *
cache_brk_tree_find(struct brk_tree *brktree)
{
	int i;
	uint8_t v;
	uint32_t hash;

	assert(brktree);

	hash = brk_treehash(brktree);

	for (v = RTR_VERSION_1; v <= RTR_MAX_VERSION; v++) {
		if (cache[v].head == -1 && cache[v].tail == -1)
			continue;

		for (i = cache[v].tail; i != cache[v].head;
		    i = (i + 1) % cache[v].frame_count) {
			if (CFRAME(v,i).rtr_brks->hash == hash) {
				if (brk_treecmp(brktree,
				    &CFRAME(v,i).rtr_brks->brks) == 0) {
					return CFRAME(v,i).rtr_brks;
				}
			}
		}

		if (CHEAD(v).rtr_brks->hash == hash) {
			if (brk_treecmp(brktree,
			    &CHEAD(v).rtr_brks->brks) == 0) {
				return CHEAD(v).rtr_brks;
			}
		}
	}
	return NULL;
}

struct cache_brk_tree *
cache_brk_tree_dup(struct cache_brk_tree *cache_brk)
{
	assert(cache_brk);

	cache_brk->reference_count++;

	return cache_brk;
}

struct cache_brk_tree *
cache_brk_tree_new(struct brk_tree *brktree)
{
	struct cache_brk_tree *cache_brk;

	assert(brktree);

	cache_brk = cache_brk_tree_find(brktree);

	if (cache_brk) {
		cache_brk = cache_brk_tree_dup(cache_brk);
		return cache_brk;
	}

	cache_brk = malloc(sizeof(struct cache_brk_tree));
	if (cache_brk == NULL)
		err(1, NULL);

	RB_INIT(&cache_brk->brks);

	cache_brk->reference_count = 1;
	brk_treecpy_count_and_hash(brktree, &cache_brk->brks,
	    &cache_brk->entry_count, &cache_brk->hash);
	rtr_gettime();
	cache_brk->creation_time = now.tv_sec;
	return cache_brk;
}

void
cache_brk_tree_free(struct cache_brk_tree *cache_brk)
{
	if (!cache_brk)
		return;

	cache_brk->reference_count--;

	if (cache_brk->reference_count < 1) {
		free_brk_tree(&cache_brk->brks);
		free(cache_brk);
	}
}

/*
 * ASN
 */

static inline int
asncmp(struct asn *a, struct asn *b)
{
	if (a->asn > b->asn)
		return 1;
	if (a->asn < b->asn)
		return -1;
	return 0;
}

RB_GENERATE(asn_tree, asn, entry, asncmp)

/* 1 if error */
int
insert_asn(struct asn_tree *asntree, uint32_t asn)
{
	struct asn *node;

	assert(asntree);

	node = malloc(sizeof(struct asn));
	if (node == NULL)
		err(1, NULL);

	node->asn = asn;
	if (RB_INSERT(asn_tree, asntree, node) != NULL) {
		free(node);
		return 1;
	}
	return 0;
}

/* 1 if error */
int
remove_asn(struct asn_tree *asntree, uint32_t asn)
{
	struct asn query, *node;

	assert(asntree);

	query.asn = asn;

	node = RB_FIND(asn_tree, asntree, &query);
	if (!node)
		return 1;

	RB_REMOVE(asn_tree, asntree, node);
	free(node);
	return 0;
}

void
free_asn_tree(struct asn_tree *asntree)
{
	struct asn *asn;
	struct asn *tmpasn;

	if (!asntree)
		return;

	RB_FOREACH_SAFE(asn, asn_tree, asntree, tmpasn) {
		RB_REMOVE(asn_tree, asntree, asn);
		free(asn);
	}
}

int
clean_asn_tree(struct asn_tree *asntree)
{
	struct asn *asn, *tmpasn;
	int zero = 0;
	int non_zero = 0;

	if (!asntree)
		return 0;

	RB_FOREACH(asn, asn_tree, asntree) {
		if (asn->asn != 0)
			non_zero = 1;
		else
			zero = 1;
		if (zero && non_zero)
			break;
	}

	if (zero && non_zero) {
		RB_FOREACH_SAFE(asn, asn_tree, asntree, tmpasn) {
			if (asn->asn == 0) {
				RB_REMOVE(asn_tree, asntree, asn);
				free(asn);
				break;
			}
		}
	}
	return zero && non_zero;
}

int32_t
asn_treecount(struct asn_tree *asntree)
{
	struct asn *asn;
	int32_t i = 0;

	assert(asntree);

	RB_FOREACH(asn, asn_tree, asntree) {
		i++;
		if (i < 0)
			break;
	}

	if (i < -1)
		i = -1;
	return i;
}

/*
 * VAP
 */

static inline int
vapcmp(struct vap *a, struct vap *b)
{
	if (a->customer_asn > b->customer_asn)
		return 1;
	if (a->customer_asn < b->customer_asn)
		return -1;
	return 0;
}

RB_GENERATE(vap_tree, vap, entry, vapcmp)

int
vapfullcmp(struct vap *a, struct vap *b)
{
	int32_t provider_index;

	if (a->customer_asn > b->customer_asn)
		return 1;
	if (a->customer_asn < b->customer_asn)
		return -1;

	if (a->provider_count > b->provider_count)
		return 1;
	if (a->provider_count < b->provider_count)
		return -1;

	/* provider counts must be the same. use a. */

	if (a->provider_count == 0)
		return 0;

	for (provider_index = 0;
	    provider_index < a->provider_count;
	    provider_index++) {
		if (a->provider_asns[provider_index] >
		    b->provider_asns[provider_index])
			return 1;
		if (a->provider_asns[provider_index] <
		    b->provider_asns[provider_index])
			return -1;
	}
	return 0;
}

/* 1 if error */
int
insert_vap(struct vap_tree *vaptree, struct vap *vap, time_t present)
{
	struct vap *newnode, *foundnode;
	struct asn_tree asns;
	struct asn *asn, *tmpasn;
	int32_t provider_length, provider_count, provider_index;

	assert(vaptree);
	assert(vap);

	if (vap->expire > 0 && present >= vap->expire)
		return 0;

	if (vap->provider_count > VAP_MAX_PROVIDERS)
		return 1;

	RB_INIT(&asns);

	for (provider_index = 0;
	    provider_index < vap->provider_count;
	    provider_index++)
		insert_asn(&asns, vap->provider_asns[provider_index]);

	if (clean_asn_tree(&asns) != 0)
		logx(0, "Mixed ASPA AS0 record found for AS%u, stripping AS0",
		    vap->customer_asn);

	provider_count = asn_treecount(&asns);
	if (provider_count < 0)
		err(1, NULL);

	provider_length = sizeof(uint32_t) * provider_count;
	newnode = malloc(sizeof(struct vap) + provider_length);
	if (newnode == NULL)
		err(1, NULL);

	newnode->expire = vap->expire;
	newnode->customer_asn = vap->customer_asn;
	newnode->provider_count = provider_count;
	provider_index = 0;
	RB_FOREACH_SAFE(asn, asn_tree, &asns, tmpasn) {
		newnode->provider_asns[provider_index++] = asn->asn;
		RB_REMOVE(asn_tree, &asns, asn);
		free(asn);
	}

	foundnode = RB_INSERT(vap_tree, vaptree, newnode);
	if (!foundnode)
		return 0;

	if (vapfullcmp(newnode, foundnode) == 0) {
		free(newnode);
		return 1;
	}

	RB_REMOVE(vap_tree, vaptree, foundnode);
	free(foundnode);

	RB_INSERT(vap_tree, vaptree, newnode);
	return 0;
}

/* 1 if error */
int
remove_vap(struct vap_tree *vaptree, struct vap *vap)
{
	struct vap query, *node;

	assert(vaptree);
	assert(vap);

	if (vap->provider_count > VAP_MAX_PROVIDERS)
		return 1;

	query.expire = 0;
	query.customer_asn = vap->customer_asn;
	query.provider_count = 0;

	node = RB_FIND(vap_tree, vaptree, &query);
	if (!node)
		return 1;

	RB_REMOVE(vap_tree, vaptree, node);
	free(node);
	return 0;
}

void
free_vap_tree(struct vap_tree *vaptree)
{
	struct vap *vap, *tmpvap;

	if (!vaptree)
		return;

	RB_FOREACH_SAFE(vap, vap_tree, vaptree, tmpvap) {
		RB_REMOVE(vap_tree, vaptree, vap);
		free(vap);
	}
}

int
vap_treecmp(struct vap_tree *a, struct vap_tree *b)
{
	struct vap *vap, *node;
	int i;

	assert(a);
	assert(b);

	RB_FOREACH(vap, vap_tree, a) {
		node = RB_FIND(vap_tree, b, vap);
		if (!node)
			return 1;

		i = vapfullcmp(vap, node);
		if (i != 0)
			return i;
	}

	RB_FOREACH(vap, vap_tree, b) {
		node = RB_FIND(vap_tree, a, vap);
		if (!node)
			return -1;

		i = vapfullcmp(vap, node);
		if (i != 0)
			return i;
	}
	return 0;
}

void
vap_treecpy(struct vap_tree *src, struct vap_tree *dst)
{
	struct vap *vap, *node;

	uint32_t vap_length;
	int32_t provider_index;

	assert(src);
	assert(dst);

	free_vap_tree(dst);

	RB_FOREACH(vap, vap_tree, src) {
		vap_length = sizeof(struct vap) +
		    (vap->provider_count * sizeof(uint32_t));

		node = malloc(vap_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = vap->expire;
		node->customer_asn = vap->customer_asn;
		node->provider_count = vap->provider_count;

		for (provider_index = 0;
		    provider_index < vap->provider_count;
		    provider_index++)
			node->provider_asns[provider_index] =
			    vap->provider_asns[provider_index];

		RB_INSERT(vap_tree, dst, node);
	}
}

void
vap_treecpy_expire(struct vap_tree *src, struct vap_tree *dst, time_t present)
{
	struct vap *vap, *node;

	uint32_t vap_length;
	int32_t provider_index;

	assert(src);
	assert(dst);

	free_vap_tree(dst);

	RB_FOREACH(vap, vap_tree, src) {
		if (vap->expire > 0 && present >= vap->expire)
			continue;

		vap_length = sizeof(struct vap) +
		    (vap->provider_count * sizeof(uint32_t));

		node = malloc(vap_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = vap->expire;
		node->customer_asn = vap->customer_asn;
		node->provider_count = vap->provider_count;

		for (provider_index = 0;
		    provider_index < vap->provider_count;
		    provider_index++)
			node->provider_asns[provider_index] =
			    vap->provider_asns[provider_index];

		RB_INSERT(vap_tree, dst, node);
	}
}

int64_t
vap_treecount(struct vap_tree *vaptree)
{
	struct vap *vap;
	int64_t i = 0;

	assert(vaptree);

	RB_FOREACH(vap, vap_tree, vaptree) {
		i++;
		if (i < 0)
			break;
	}

	if (i < -1)
		i = -1;

	return i;
}

int
vap_treeempty(struct vap_tree *vaptree)
{
	assert(vaptree);

	return RB_EMPTY(vaptree);
}

/* update old tree with the expirations from new tree */
/* only run on identical trees */

void
vap_treeupdate(struct vap_tree *oldtree, struct vap_tree *newtree)
{
	struct vap *oldvap, *newvap;

	assert(oldtree);
	assert(newtree);

	RB_FOREACH(oldvap, vap_tree, oldtree) {
		newvap = RB_FIND(vap_tree, newtree, oldvap);
		if (newvap)
			oldvap->expire = newvap->expire;
	}
}

uint32_t
vap_treehash(struct vap_tree *vaptree)
{
	struct vap *vap;
	uint32_t hash = fnv32_init;

	assert(vaptree);


	RB_FOREACH(vap, vap_tree, vaptree) {
		hash = fnv32_hash(&vap->customer_asn, sizeof(uint32_t), hash);

		if (vap->provider_count > 0) {
			hash = fnv32_hash(&vap->provider_asns,
			    vap->provider_count * sizeof(uint32_t), hash);
		}
	}
	return hash;
}

void
vap_treecpy_count_and_hash(struct vap_tree *src, struct vap_tree *dst,
    int64_t *count, uint32_t *hash)
{
	struct vap *vap, *node;
	uint32_t vap_length;
	int32_t provider_index;

	assert(src);
	assert(dst);
	assert(count);
	assert(hash);

	free_vap_tree(dst);

	*count = 0;
	*hash = fnv32_init;

	RB_FOREACH(vap, vap_tree, src) {
		vap_length = sizeof(struct vap) +
		    (vap->provider_count * sizeof(uint32_t));

		node = malloc(vap_length);
		if (node == NULL)
			err(1, NULL);

		node->expire = vap->expire;
		node->customer_asn = vap->customer_asn;
		node->provider_count = vap->provider_count;

		for (provider_index = 0;
		    provider_index < vap->provider_count;
		    provider_index++)
			node->provider_asns[provider_index] =
			    vap->provider_asns[provider_index];
		RB_INSERT(vap_tree, dst, node);

		(*count)++;

		*hash = fnv32_hash(&vap->customer_asn, sizeof(uint32_t), *hash);

		if (vap->provider_count > 0) {
			*hash = fnv32_hash(&vap->provider_asns,
			    vap->provider_count * sizeof(uint32_t), *hash);
		}
	}
}

struct cache_vap_tree *
cache_vap_tree_find(struct vap_tree *vaptree)
{
	int i;
	uint8_t v;
	uint32_t hash;

	assert(vaptree);

	hash = vap_treehash(vaptree);

	for (v = RTR_VERSION_2; v <= RTR_MAX_VERSION; v++) {
		if (cache[v].head == -1 && cache[v].tail == -1)
			continue;

		for (i = cache[v].tail; i != cache[v].head;
		    i = (i + 1) % cache[v].frame_count) {
			if (CFRAME(v,i).rtr_vaps->hash == hash) {
				if (vap_treecmp(vaptree,
				    &CFRAME(v,i).rtr_vaps->vaps) == 0) {
					return CFRAME(v,i).rtr_vaps;
				}
			}
		}

		if (CHEAD(v).rtr_vaps->hash == hash) {
			if (vap_treecmp(vaptree,
			    &CHEAD(v).rtr_vaps->vaps) == 0) {
				return CHEAD(v).rtr_vaps;
			}
		}
	}
	return NULL;
}

struct cache_vap_tree *
cache_vap_tree_dup(struct cache_vap_tree *cache_vap)
{
	assert(cache_vap);

	cache_vap->reference_count++;

	return cache_vap;
}

struct cache_vap_tree *
cache_vap_tree_new(struct vap_tree *vaptree)
{
	struct cache_vap_tree *cache_vap;

	assert(vaptree);

	cache_vap = cache_vap_tree_find(vaptree);
	if (cache_vap) {
		cache_vap = cache_vap_tree_dup(cache_vap);
		return cache_vap;
	}

	cache_vap = malloc(sizeof(struct cache_vap_tree));
	if (cache_vap == NULL)
		err(1, NULL);

	RB_INIT(&cache_vap->vaps);

	cache_vap->reference_count = 1;
	vap_treecpy_count_and_hash(vaptree, &cache_vap->vaps,
	    &cache_vap->entry_count, &cache_vap->hash);
	rtr_gettime();
	cache_vap->creation_time = now.tv_sec;
	return cache_vap;
}

void
cache_vap_tree_free(struct cache_vap_tree *cache_vap)
{
	if (!cache_vap)
		return;

	cache_vap->reference_count--;

	if (cache_vap->reference_count < 1) {
		free_vap_tree(&cache_vap->vaps);
		free(cache_vap);
	}
}
