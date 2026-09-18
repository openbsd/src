/*	$OpenBSD: cache.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
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
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "rtr_config.h"
#include "rtrd.h"

struct cache cache[RTR_MAX_VERSION + 1];

void
init_cache_frame(struct cache_frame *c)
{
	assert(c);

	c->serial_number = 0;
	c->rtr_vrp4s = NULL;
	c->rtr_vrp6s = NULL;
	c->rtr_brks = NULL;
	c->rtr_vaps = NULL;
}

int
init_cache(struct cache *c, uint16_t session_id, uint16_t frame_count)
{
	int i;

	assert(c);

	if (frame_count <= 1)
		return 1;

	c->frames = malloc(sizeof(struct cache_frame) * frame_count);
	if (c->frames == NULL)
		err(1, NULL);

	for (i = 0; i < frame_count; i++)
		init_cache_frame(c->frames + i);

	c->head = -1;
	c->tail = -1;
	c->next_serial_number = 0;
	c->refresh_interval = REFRESH_INTERVAL;
	c->retry_interval = RETRY_INTERVAL;
	c->expire_interval = EXPIRE_INTERVAL;
	c->session_id = session_id;
	c->frame_count = frame_count;

	return 0;
}

int
init_cache_array(uint16_t frame_count)
{
	uint8_t version;
	uint16_t session_id;
	uint32_t hash;

	hash = fnv32_init;
	hash = fnv32_hash(&now.tv_sec, sizeof(now.tv_sec), hash);
	hash = fnv32_hash(&now.tv_usec, sizeof(now.tv_usec), hash);

	session_id = hash % 0x10000;

	for (version = 0; version <= RTR_MAX_VERSION; version++)
		if (init_cache(cache + version,
		    (session_id + version) % 0x10000, frame_count) != 0)
			return 1;

	return 0;
}

int
is_cache_empty(struct cache *c)
{
	assert(c);

	if (c->head == -1 && c->tail == -1)
		return 1;

	return 0;
}

void
free_cache_frame(struct cache_frame *c)
{
	assert(c);

	c->serial_number = 0;

	cache_vrp4_tree_free(c->rtr_vrp4s);
	c->rtr_vrp4s = NULL;

	cache_vrp6_tree_free(c->rtr_vrp6s);
	c->rtr_vrp6s = NULL;

	cache_brk_tree_free(c->rtr_brks);
	c->rtr_brks = NULL;

	cache_vap_tree_free(c->rtr_vaps);
	c->rtr_vaps = NULL;
}

struct cache_frame *
get_latest_cache_frame(uint8_t version)
{
	if (cache[version].head == -1 && cache[version].tail == -1)
		return NULL;

	return cache[version].frames + cache[version].head;
}

struct cache_frame *
get_serial_cache_frame(uint8_t version, uint32_t serial_number)
{
	int i;

	if (cache[version].head == -1 && cache[version].tail == -1)
		return NULL;

	for (i = cache[version].tail; i != cache[version].head;
	    i = (i + 1) % cache[version].frame_count) {
		if (cache[version].frames[i].serial_number == serial_number) {
			return cache[version].frames + i;
		}
	}

	if (cache[version].frames[cache[version].head].serial_number
	    == serial_number)
		return cache[version].frames + cache[version].head;

	return NULL;
}

struct cache_frame *
cache_frame_push(uint8_t version,
    struct cache_vrp4_tree *cache_vrp4, struct cache_vrp6_tree *cache_vrp6,
    struct cache_brk_tree *cache_brk, struct cache_vap_tree *cache_vap)
{
	struct cache_frame *cf;

	int next;

	assert(cache_vrp4);
	assert(cache_vrp6);
	assert(cache_brk || (version < RTR_VERSION_1));
	assert(cache_vap || (version < RTR_VERSION_2));

	if (cache[version].head == -1 && cache[version].tail == -1) {
		/* empty cache */
		cache[version].head = 0;
		cache[version].tail = 0;
		cf = cache[version].frames + cache[version].head;
		cf->serial_number = cache[version].next_serial_number++;
		cf->rtr_vrp4s = cache_vrp4;
		cf->rtr_vrp6s = cache_vrp6;
		cf->rtr_brks = (version >= RTR_VERSION_1) ? cache_brk : NULL;
		cf->rtr_vaps = (version >= RTR_VERSION_2) ? cache_vap : NULL;

		return cf;
	}

	next = (cache[version].head + 1) % cache[version].frame_count;

	if (next == cache[version].tail)
	{
		/* full cache */
		cf = cache[version].frames + cache[version].tail;
		free_cache_frame(cf);
		cache[version].tail = (cache[version].tail + 1) %
		    cache[version].frame_count;
	}

	cache[version].head = next;

	cf = cache[version].frames + cache[version].head;
	cf->serial_number = cache[version].next_serial_number++;
	cf->rtr_vrp4s = cache_vrp4;
	cf->rtr_vrp6s = cache_vrp6;
	cf->rtr_brks = (version >= RTR_VERSION_1) ? cache_brk : NULL;
	cf->rtr_vaps = (version >= RTR_VERSION_2) ? cache_vap : NULL;

	return cf;
}

void
update_cache(struct vrp4_tree *vrp4tree, struct vrp6_tree *vrp6tree,
    struct brk_tree *brktree, struct vap_tree *vaptree)
{
	struct cache_frame *cf;
	struct cache_vrp4_tree *cache_vrp4;
	struct cache_vrp6_tree *cache_vrp6;
	struct cache_brk_tree *cache_brk;
	struct cache_vap_tree *cache_vap;
	int vrp4_diff;
	int vrp6_diff;
	int brk_diff;
	int vap_diff;
	struct pdu_serial_notify sn;

	assert(vrp4tree);
	assert(vrp6tree);
	assert(brktree);
	assert(vaptree);

	/* VERSION 0 */

	cf = get_latest_cache_frame(RTR_VERSION_0);

	if (cf) {
		vrp4_diff = vrp4_treecmp(&cf->rtr_vrp4s->vrp4s, vrp4tree);
		vrp6_diff = vrp6_treecmp(&cf->rtr_vrp6s->vrp6s, vrp6tree);

		/* expirations */
		if (!vrp4_diff)
			vrp4_treeupdate(&cf->rtr_vrp4s->vrp4s, vrp4tree);

		if (!vrp6_diff)
			vrp6_treeupdate(&cf->rtr_vrp6s->vrp6s, vrp6tree);

		if (vrp4_diff || vrp6_diff) {
			/* compare against what we have. dup if its the same. */
			if (vrp4_diff)
				cache_vrp4 = cache_vrp4_tree_new(vrp4tree);
			else
				cache_vrp4 = cache_vrp4_tree_dup(cf->rtr_vrp4s);

			if (vrp6_diff)
				cache_vrp6 = cache_vrp6_tree_new(vrp6tree);
			else
				cache_vrp6 = cache_vrp6_tree_dup(cf->rtr_vrp6s);
		}
	} else {
		vrp4_diff = 1;
		vrp6_diff = 1;
		/* no cache yet. load them in. */

		cache_vrp4 = cache_vrp4_tree_new(vrp4tree);

		cache_vrp6 = cache_vrp6_tree_new(vrp6tree);
	}

	if (vrp4_diff || vrp6_diff) {

		/* version 0 */

		cf = cache_frame_push(RTR_VERSION_0, cache_vrp4, cache_vrp6,
		    NULL, NULL);

		sn.version = RTR_VERSION_0;
		sn.type = SERIAL_NOTIFY;
		sn.length = sizeof(struct pdu_serial_notify);
		sn.serial_number = cf->serial_number;
		sendto_allregisteredclientsversion(&sn, sn.version);

		logx(0, "Updated v0 cache to serial %u\n",
		    cf->serial_number);
	}

	/* VERSION 1 */

	cf = get_latest_cache_frame(RTR_VERSION_1);

	if (cf) {
		vrp4_diff = vrp4_treecmp(&cf->rtr_vrp4s->vrp4s, vrp4tree);
		vrp6_diff = vrp6_treecmp(&cf->rtr_vrp6s->vrp6s, vrp6tree);
		brk_diff = brk_treecmp(&cf->rtr_brks->brks, brktree);

		/* expirations */
		if (!vrp4_diff)
			vrp4_treeupdate(&cf->rtr_vrp4s->vrp4s, vrp4tree);

		if (!vrp6_diff)
			vrp6_treeupdate(&cf->rtr_vrp6s->vrp6s, vrp6tree);

		if (!brk_diff)
			brk_treeupdate(&cf->rtr_brks->brks, brktree);

		if (vrp4_diff || vrp6_diff || brk_diff) {
			/* compare against what we have. dup if its the same. */
			if (vrp4_diff)
				cache_vrp4 = cache_vrp4_tree_new(vrp4tree);
			else
				cache_vrp4 = cache_vrp4_tree_dup(cf->rtr_vrp4s);

			if (vrp6_diff)
				cache_vrp6 = cache_vrp6_tree_new(vrp6tree);
			else
				cache_vrp6 = cache_vrp6_tree_dup(cf->rtr_vrp6s);

			if (brk_diff)
				cache_brk = cache_brk_tree_new(brktree);
			else
				cache_brk = cache_brk_tree_dup(cf->rtr_brks);
		}
	} else {
		vrp4_diff = 1;
		vrp6_diff = 1;
		brk_diff = 1;
		/* no cache yet. load them in. */

		cache_vrp4 = cache_vrp4_tree_new(vrp4tree);

		cache_vrp6 = cache_vrp6_tree_new(vrp6tree);

		cache_brk = cache_brk_tree_new(brktree);
	}

	if (vrp4_diff || vrp6_diff || brk_diff) {

		/* version 1 */

		cf = cache_frame_push(RTR_VERSION_1, cache_vrp4, cache_vrp6,
		    cache_brk, NULL);

		sn.version = RTR_VERSION_1;
		sn.type = SERIAL_NOTIFY;
		sn.length = sizeof(struct pdu_serial_notify);
		sn.serial_number = cf->serial_number;
		sendto_allregisteredclientsversion(&sn, sn.version);

		logx(0, "Updated v1 cache to serial %u\n", cf->serial_number);

	}

	/* VERSION 2 */

	cf = get_latest_cache_frame(RTR_VERSION_2);

	if (cf) {
		vrp4_diff = vrp4_treecmp(&cf->rtr_vrp4s->vrp4s, vrp4tree);
		vrp6_diff = vrp6_treecmp(&cf->rtr_vrp6s->vrp6s, vrp6tree);
		brk_diff = brk_treecmp(&cf->rtr_brks->brks, brktree);
		vap_diff = vap_treecmp(&cf->rtr_vaps->vaps, vaptree);

		/* expirations */
		if (!vrp4_diff)
			vrp4_treeupdate(&cf->rtr_vrp4s->vrp4s, vrp4tree);

		if (!vrp6_diff)
			vrp6_treeupdate(&cf->rtr_vrp6s->vrp6s, vrp6tree);

		if (!brk_diff)
			brk_treeupdate(&cf->rtr_brks->brks, brktree);

		if (!vap_diff)
			vap_treeupdate(&cf->rtr_vaps->vaps, vaptree);


		if (vrp4_diff || vrp6_diff || brk_diff || vap_diff) {
			/* compare against what we have. dup if its the same. */
			if (vrp4_diff)
				cache_vrp4 = cache_vrp4_tree_new(vrp4tree);
			else
				cache_vrp4 = cache_vrp4_tree_dup(cf->rtr_vrp4s);

			if (vrp6_diff)
				cache_vrp6 = cache_vrp6_tree_new(vrp6tree);
			else
				cache_vrp6 = cache_vrp6_tree_dup(cf->rtr_vrp6s);

			if (brk_diff)
				cache_brk = cache_brk_tree_new(brktree);
			else
				cache_brk = cache_brk_tree_dup(cf->rtr_brks);

			if (vap_diff)
				cache_vap = cache_vap_tree_new(vaptree);
			else
				cache_vap = cache_vap_tree_dup(cf->rtr_vaps);
		}
	} else {
		vrp4_diff = 1;
		vrp6_diff = 1;
		brk_diff = 1;
		vap_diff = 1;
		/* no cache yet. load them in. */

		cache_vrp4 = cache_vrp4_tree_new(vrp4tree);

		cache_vrp6 = cache_vrp6_tree_new(vrp6tree);

		cache_brk = cache_brk_tree_new(brktree);

		cache_vap = cache_vap_tree_new(vaptree);
	}

	if (vrp4_diff || vrp6_diff || brk_diff || vap_diff) {

		/* version 2 */

		cf = cache_frame_push(RTR_VERSION_2, cache_vrp4, cache_vrp6,
		    cache_brk, cache_vap);

		sn.version = RTR_VERSION_2;
		sn.type = SERIAL_NOTIFY;
		sn.length = sizeof(struct pdu_serial_notify);
		sn.serial_number = cf->serial_number;
		sendto_allregisteredclientsversion(&sn, sn.version);

		logx(0, "Updated v2 cache to serial %u\n", cf->serial_number);

	}
}
