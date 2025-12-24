/*	$OpenBSD: rde_adjout.c,v 1.15 2025/12/24 07:59:55 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004, 2025 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>

#include <limits.h>
#include <siphash.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"
#include "chash.h"

struct bitmap adjout_id_map;

static struct adjout_attr	*adjout_attr_ref(struct adjout_attr *);
static void			 adjout_attr_unref(struct adjout_attr *);

static uint64_t		pendkey;

static inline uint64_t
pend_prefix_hash(const struct pend_prefix *pp)
{
	uint64_t	h = pendkey;

	h = ch_qhash64(h, (uintptr_t)pp->pt);
	h = ch_qhash64(h, pp->path_id_tx);
	return h;
}

static inline uint64_t
pend_attr_hash(const struct pend_attr *pa)
{
	uint64_t	h = pendkey;

	h = ch_qhash64(h, (uintptr_t)pa->attrs);
	h = ch_qhash64(h, pa->aid);
	return h;
}

CH_PROTOTYPE(pend_prefix_hash, pend_prefix, pend_prefix_hash);
CH_PROTOTYPE(pend_attr_hash, pend_attr, pend_attr_hash);

/* pending prefix queue functions */
static struct pend_attr *
pend_attr_alloc(struct adjout_attr *attrs, uint8_t aid,
    struct rde_peer *peer)
{
	struct pend_attr *pa;

	if ((pa = calloc(1, sizeof(*pa))) == NULL)
		fatal(__func__);
	rdemem.pend_attr_cnt++;
	TAILQ_INIT(&pa->prefixes);
	if (attrs)
		pa->attrs = adjout_attr_ref(attrs);
	pa->aid = aid;

	TAILQ_INSERT_TAIL(&peer->updates[aid], pa, entry);
	if (CH_INSERT(pend_attr_hash, &peer->pend_attrs, pa, NULL) != 1)
		fatalx("corrupted pending attr hash table");
	return pa;
}

static void
pend_attr_free(struct pend_attr *pa, struct rde_peer *peer)
{
	if (!TAILQ_EMPTY(&pa->prefixes)) {
		log_warnx("freeing not empty pending attribute");
		abort();
	}

	TAILQ_REMOVE(&peer->updates[pa->aid], pa, entry);
	CH_REMOVE(pend_attr_hash, &peer->pend_attrs, pa);

	if (pa->attrs != NULL)
		adjout_attr_unref(pa->attrs);

	rdemem.pend_attr_cnt--;
	free(pa);
}

void
pend_attr_done(struct pend_attr *pa, struct rde_peer *peer)
{
	if (pa == NULL)
		return;
	if (TAILQ_EMPTY(&pa->prefixes))
		pend_attr_free(pa, peer);
}

static struct pend_attr *
pend_attr_lookup(struct rde_peer *peer, struct adjout_attr *attrs, uint8_t aid)
{
	struct pend_attr needle = { .attrs = attrs, .aid = aid };

	return CH_FIND(pend_attr_hash, &peer->pend_attrs, &needle);
}

static inline int
pend_attr_eq(const struct pend_attr *a, const struct pend_attr *b)
{
	if (a->attrs != b->attrs)
		return 0;
	if (a->aid != b->aid)
		return 0;
	return 1;
}

CH_GENERATE(pend_attr_hash, pend_attr, pend_attr_eq, pend_attr_hash);

/*
 * Insert an End-of-RIB marker into the update queue.
 */
void
pend_eor_add(struct rde_peer *peer, uint8_t aid)
{
	struct pend_attr *pa;

	pa = pend_attr_lookup(peer, NULL, aid);
	if (pa == NULL)
		pa = pend_attr_alloc(NULL, aid, peer);
}


static struct pend_prefix	*pend_prefix_alloc(struct pend_attr *,
				    struct pt_entry *, uint32_t);

static struct pend_prefix *
pend_prefix_lookup(struct rde_peer *peer, struct pt_entry *pt,
    uint32_t path_id_tx)
{
	struct pend_prefix needle = { .pt = pt, .path_id_tx = path_id_tx };

	return CH_FIND(pend_prefix_hash, &peer->pend_prefixes, &needle);
}

static void
pend_prefix_remove(struct pend_prefix *pp, struct pend_prefix_queue *head,
    struct rde_peer *peer)
{
	if (CH_REMOVE(pend_prefix_hash, &peer->pend_prefixes, pp) != pp) {
		log_warnx("missing pending prefix in hash table");
		abort();
	}
	TAILQ_REMOVE(head, pp, entry);

	if (pp->attrs == NULL) {
		peer->stats.pending_withdraw--;
	} else {
		peer->stats.pending_update--;
	}
	pp->attrs = NULL;
}

void
pend_prefix_add(struct rde_peer *peer, struct adjout_attr *attrs,
    struct pt_entry *pt, uint32_t path_id_tx)
{
	struct pend_attr *pa = NULL, *oldpa = NULL;
	struct pend_prefix *pp;
	struct pend_prefix_queue *head;

	if (attrs != NULL) {
		pa = pend_attr_lookup(peer, attrs, pt->aid);
		if (pa == NULL)
			pa = pend_attr_alloc(attrs, pt->aid, peer);
	}

	pp = pend_prefix_lookup(peer, pt, path_id_tx);
	if (pp == NULL) {
		pp = pend_prefix_alloc(pa, pt, path_id_tx);
	} else {
		if (pp->attrs == NULL)
			head = &peer->withdraws[pt->aid];
		else
			head = &pp->attrs->prefixes;
		oldpa = pp->attrs;
		pend_prefix_remove(pp, head, peer);
		pp->attrs = pa;
	}

	if (pa == NULL) {
		head = &peer->withdraws[pt->aid];
		peer->stats.pending_withdraw++;
	} else {
		head = &pa->prefixes;
		peer->stats.pending_update++;
	}

	TAILQ_INSERT_TAIL(head, pp, entry);
	if (CH_INSERT(pend_prefix_hash, &peer->pend_prefixes, pp, NULL) != 1) {
		log_warnx("corrupted pending prefix hash table");
		abort();
	}

	pend_attr_done(oldpa, peer);
}

static struct pend_prefix *
pend_prefix_alloc(struct pend_attr *attrs, struct pt_entry *pt,
    uint32_t path_id_tx)
{
	struct pend_prefix *pp;

	if ((pp = calloc(1, sizeof(*pp))) == NULL)
		fatal(__func__);
	rdemem.pend_prefix_cnt++;
	pp->pt = pt_ref(pt);
	pp->attrs = attrs;
	pp->path_id_tx = path_id_tx;

	return pp;
}

void
pend_prefix_free(struct pend_prefix *pp, struct pend_prefix_queue *head,
    struct rde_peer *peer)
{
	pend_prefix_remove(pp, head, peer);
	pt_unref(pp->pt);
	rdemem.pend_prefix_cnt--;
	free(pp);
}

static inline int
pend_prefix_eq(const struct pend_prefix *a, const struct pend_prefix *b)
{
	if (a->pt != b->pt)
		return 0;
	if (a->path_id_tx != b->path_id_tx)
		return 0;
	return 1;
}

CH_GENERATE(pend_prefix_hash, pend_prefix, pend_prefix_eq, pend_prefix_hash);

/* adj-rib-out specific functions */
static uint64_t		attrkey;

static inline uint64_t
adjout_attr_hash(const struct adjout_attr *a)
{
	return a->hash;
}

static uint64_t
adjout_attr_calc_hash(const struct adjout_attr *a)
{
	uint64_t h;
	h = ch_qhash64(attrkey, (uintptr_t)a->aspath);
	h = ch_qhash64(h, (uintptr_t)a->communities);
	h = ch_qhash64(h, (uintptr_t)a->nexthop);
	return h;
}

static inline int
adjout_attr_eq(const struct adjout_attr *a, const struct adjout_attr *b)
{
	if (a->aspath == b->aspath &&
	    a->communities == b->communities &&
	    a->nexthop == b->nexthop)
		return 1;
	return 0;
}

CH_HEAD(adjout_attr_tree, adjout_attr);
CH_PROTOTYPE(adjout_attr_tree, adjout_attr, adjout_attr_hash);

static struct adjout_attr_tree attrtable = CH_INITIALIZER(&attrtable);

void
adjout_init(void)
{
	arc4random_buf(&attrkey, sizeof(attrkey));
	arc4random_buf(&pendkey, sizeof(pendkey));
}

/* Alloc, init and add a new entry into the has table. May not fail. */
static struct adjout_attr *
adjout_attr_alloc(struct rde_aspath *asp, struct rde_community *comm,
    struct nexthop *nexthop, uint64_t hash)
{
	struct adjout_attr *a;

	if ((a = calloc(1, sizeof(*a))) == NULL)
		fatal(__func__);
	rdemem.adjout_attr_cnt++;

	a->aspath = path_ref(asp);
	a->communities = communities_ref(comm);
	a->nexthop = nexthop_ref(nexthop);

	a->hash = hash;
	if (CH_INSERT(adjout_attr_tree, &attrtable, a, NULL) != 1)
		fatalx("%s: object already in table", __func__);

	return a;
}

/* Free a entry after removing it from the hash table */
static void
adjout_attr_free(struct adjout_attr *a)
{
	CH_REMOVE(adjout_attr_tree, &attrtable, a);

	/* destroy all references to other objects */
	/* remove nexthop ref ... */
	nexthop_unref(a->nexthop);
	a->nexthop = NULL;
	/* ... communities ... */
	communities_unref(a->communities);
	a->communities = NULL;
	/* and unlink from aspath */
	path_unref(a->aspath);
	a->aspath = NULL;

	rdemem.adjout_attr_cnt--;
	free(a);
}

static struct adjout_attr *
adjout_attr_ref(struct adjout_attr *attrs)
{
	attrs->refcnt++;
	rdemem.adjout_attr_refs++;
	return attrs;
}

static void
adjout_attr_unref(struct adjout_attr *attrs)
{
	attrs->refcnt--;
	rdemem.adjout_attr_refs--;
	if (attrs->refcnt > 0)
		return;

	adjout_attr_free(attrs);
}

static struct adjout_attr *
adjout_attr_get(struct filterstate *state)
{
	struct adjout_attr *attr, needle = { 0 };
	struct rde_aspath *asp;
	struct rde_community *comm;
	struct nexthop *nexthop;

	/* lookup or insert new */
	asp = path_getcache(&state->aspath);
	if ((comm = communities_lookup(&state->communities)) == NULL) {
		/* Communities not available, create and link a new one. */
		comm = communities_link(&state->communities);
	}
	nexthop = state->nexthop;

	needle.aspath = asp;
	needle.communities = comm;
	needle.nexthop = nexthop;
	needle.hash = adjout_attr_calc_hash(&needle);

	if ((attr = CH_FIND(adjout_attr_tree, &attrtable, &needle)) == NULL) {
		attr = adjout_attr_alloc(asp, comm, nexthop, needle.hash);
	}

	return attr;
}

CH_GENERATE(adjout_attr_tree, adjout_attr, adjout_attr_eq, adjout_attr_hash);

static void	 adjout_prefix_link(struct pt_entry *, struct rde_peer *,
		    struct adjout_attr *, uint32_t);
static void	 adjout_prefix_unlink(struct adjout_prefix *,
		    struct pt_entry *, struct rde_peer *);

static struct adjout_prefix	*adjout_prefix_alloc(struct pt_entry *,
				    uint32_t);
static void			 adjout_prefix_free(struct pt_entry *,
				    struct adjout_prefix *);

static inline uint32_t
adjout_prefix_index(struct pt_entry *pte, struct adjout_prefix *p)
{
	ptrdiff_t idx = p - pte->adjout;

	if (idx < 0 || idx > pte->adjoutlen)
		fatalx("corrupt pte adjout list");

	return idx;
}

/*
 * Search for specified prefix in the pte adjout array that is for the
 * specified path_id_tx and peer. Returns NULL if not found.
 */
struct adjout_prefix *
adjout_prefix_get(struct rde_peer *peer, uint32_t path_id_tx,
    struct pt_entry *pte)
{
	struct adjout_prefix *p;
	uint32_t i;

	for (i = 0; i < pte->adjoutlen; i++) {
		p = &pte->adjout[i];
		if (p->path_id_tx < path_id_tx)
			continue;
		if (p->path_id_tx > path_id_tx)
			break;
		if (bitmap_test(&p->peermap, peer->adjout_bid))
			return p;
	}

	return NULL;
}

/*
 * Search for specified prefix in the pte adjout array that is for the
 * specified path_id_tx and attrs. Returns NULL if not found.
 */
static struct adjout_prefix *
adjout_prefix_with_attrs(struct pt_entry *pte, uint32_t path_id_tx,
    struct adjout_attr *attrs)
{
	struct adjout_prefix *p;
	uint32_t i;

	for (i = 0; i < pte->adjoutlen; i++) {
		p = &pte->adjout[i];
		if (p->path_id_tx < path_id_tx)
			continue;
		if (p->path_id_tx > path_id_tx)
			break;
		if (p->attrs == attrs)
			return p;
	}

	return NULL;
}

/*
 * Lookup a prefix without considering path_id in the peer prefix_index.
 * Returns NULL if not found.
 */
struct adjout_prefix *
adjout_prefix_first(struct rde_peer *peer, struct pt_entry *pte)
{
	struct adjout_prefix *p;
	uint32_t i;
	int has_add_path = 0;

	if (peer_has_add_path(peer, pte->aid, CAPA_AP_SEND))
		has_add_path = 1;

	for (i = 0; i < pte->adjoutlen; i++) {
		p = &pte->adjout[i];
		if (bitmap_test(&p->peermap, peer->adjout_bid))
			return p;
		if (!has_add_path && p->path_id_tx != 0)
			return NULL;
	}

	return NULL;
}

/*
 * Return next prefix for peer after last.
 */
struct adjout_prefix *
adjout_prefix_next(struct rde_peer *peer, struct pt_entry *pte,
    struct adjout_prefix *last)
{
	struct adjout_prefix *p;
	uint32_t i;

	if (!peer_has_add_path(peer, pte->aid, CAPA_AP_SEND))
		return NULL;

	i = adjout_prefix_index(pte, last);
	for (; i < pte->adjoutlen; i++)
		if (pte->adjout[i].path_id_tx != last->path_id_tx)
			break;
	for (; i < pte->adjoutlen; i++) {
		p = &pte->adjout[i];
		if (bitmap_test(&p->peermap, peer->adjout_bid))
			return p;
	}

	return NULL;
}

/*
 * Put a prefix from the Adj-RIB-Out onto the update queue.
 */
void
adjout_prefix_update(struct adjout_prefix *p, struct rde_peer *peer,
    struct filterstate *state, struct pt_entry *pte, uint32_t path_id_tx)
{
	struct adjout_attr *attrs;

	if (p != NULL) {
		if (p->path_id_tx != path_id_tx ||
		    bitmap_test(&p->peermap, peer->adjout_bid) == 0)
			fatalx("%s: king bula is unhappy", __func__);

		/*
		 * XXX for now treat a different path_id_tx like different
		 * attributes and force out an update. It is unclear how
		 * common it is to have equivalent updates from alternative
		 * paths.
		 */
		attrs = p->attrs;
		if (p->path_id_tx == path_id_tx &&
		    attrs->nexthop == state->nexthop &&
		    communities_equal(&state->communities,
		    attrs->communities) &&
		    path_equal(&state->aspath, attrs->aspath)) {
			/* nothing changed */
			return;
		}

		/* unlink prefix so it can be relinked below */
		adjout_prefix_unlink(p, pte, peer);
		peer->stats.prefix_out_cnt--;
	}

	attrs = adjout_attr_get(state);
	adjout_prefix_link(pte, peer, attrs, path_id_tx);
	peer->stats.prefix_out_cnt++;

	if (peer_is_up(peer))
		pend_prefix_add(peer, attrs, pte, path_id_tx);
}

/*
 * Withdraw a prefix from the Adj-RIB-Out, this unlinks the aspath but leaves
 * the prefix in the RIB linked to the peer withdraw list.
 */
void
adjout_prefix_withdraw(struct rde_peer *peer, struct pt_entry *pte,
    struct adjout_prefix *p)
{
	if (bitmap_test(&p->peermap, peer->adjout_bid) == 0)
		fatalx("%s: king bula is unhappy", __func__);

	if (peer_is_up(peer))
		pend_prefix_add(peer, NULL, pte, p->path_id_tx);

	adjout_prefix_unlink(p, pte, peer);
	peer->stats.prefix_out_cnt--;
}

void
adjout_prefix_reaper(struct rde_peer *peer)
{
	bitmap_id_put(&adjout_id_map, peer->adjout_bid);
}

static struct pt_entry *
prefix_restart(struct rib_context *ctx)
{
	struct pt_entry *pte = NULL;
	struct rde_peer *peer;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		return NULL;

	/* be careful when this is the last reference to pte */
	if (ctx->ctx_pt != NULL) {
		pte = ctx->ctx_pt;
		if (pte->refcnt == 1)
			pte = pt_next(pte);
		pt_unref(ctx->ctx_pt);
	}
	ctx->ctx_pt = NULL;
	return pte;
}

void
adjout_prefix_dump_cleanup(struct rib_context *ctx)
{
	if (ctx->ctx_pt != NULL)
		pt_unref(ctx->ctx_pt);
}

void
adjout_prefix_dump_r(struct rib_context *ctx)
{
	struct pt_entry *pte, *next;
	struct adjout_prefix *p;
	struct rde_peer *peer;
	unsigned int i;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		goto done;

	if (ctx->ctx_pt == NULL && ctx->ctx_subtree.aid == AID_UNSPEC)
		pte = pt_first(ctx->ctx_aid);
	else
		pte = prefix_restart(ctx);

	for (i = 0; pte != NULL; pte = next) {
		next = pt_next(pte);
		if (ctx->ctx_aid != AID_UNSPEC &&
		    ctx->ctx_aid != pte->aid)
			continue;
		if (ctx->ctx_subtree.aid != AID_UNSPEC) {
			struct bgpd_addr addr;
			pt_getaddr(pte, &addr);
			if (prefix_compare(&ctx->ctx_subtree, &addr,
			    ctx->ctx_subtreelen) != 0)
				/* left subtree, walk is done */
				break;
		}
		if (ctx->ctx_count && i++ >= ctx->ctx_count) {
			/* store and lock last element */
			ctx->ctx_pt = pt_ref(pte);
			return;
		}
		p = adjout_prefix_first(peer, pte);
		if (p == NULL)
			continue;
		ctx->ctx_prefix_call(peer, pte, p, ctx->ctx_arg);
	}

done:
	if (ctx->ctx_done)
		ctx->ctx_done(ctx->ctx_arg, ctx->ctx_aid);
	LIST_REMOVE(ctx, entry);
	free(ctx);
}

int
adjout_prefix_dump_new(struct rde_peer *peer, uint8_t aid,
    unsigned int count, void *arg,
    void (*upcall)(struct rde_peer *, struct pt_entry *,
	struct adjout_prefix *, void *),
    void (*done)(void *, uint8_t),
    int (*throttle)(void *))
{
	struct rib_context *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return -1;
	ctx->ctx_id = peer->conf.id;
	ctx->ctx_aid = aid;
	ctx->ctx_count = count;
	ctx->ctx_arg = arg;
	ctx->ctx_prefix_call = upcall;
	ctx->ctx_done = done;
	ctx->ctx_throttle = throttle;

	rib_dump_insert(ctx);

	/* requested a sync traversal */
	if (count == 0)
		adjout_prefix_dump_r(ctx);

	return 0;
}

int
adjout_prefix_dump_subtree(struct rde_peer *peer, struct bgpd_addr *subtree,
    uint8_t subtreelen, unsigned int count, void *arg,
    void (*upcall)(struct rde_peer *, struct pt_entry *,
	struct adjout_prefix *, void *),
    void (*done)(void *, uint8_t),
    int (*throttle)(void *))
{
	struct rib_context *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return -1;
	ctx->ctx_id = peer->conf.id;
	ctx->ctx_aid = subtree->aid;
	ctx->ctx_count = count;
	ctx->ctx_arg = arg;
	ctx->ctx_prefix_call = upcall;
	ctx->ctx_done = done;
	ctx->ctx_throttle = throttle;
	ctx->ctx_subtree = *subtree;
	ctx->ctx_subtreelen = subtreelen;

	/* lookup start of subtree */
	ctx->ctx_pt = pt_get_next(subtree, subtreelen);
	if (ctx->ctx_pt)
		pt_ref(ctx->ctx_pt);	/* store and lock first element */

	rib_dump_insert(ctx);

	/* requested a sync traversal */
	if (count == 0)
		adjout_prefix_dump_r(ctx);

	return 0;
}

/*
 * Link a prefix into the different parent objects.
 */
static void
adjout_prefix_link(struct pt_entry *pte, struct rde_peer *peer,
    struct adjout_attr *attrs, uint32_t path_id_tx)
{
	struct adjout_prefix *p;

	/* assign ids on first use to keep the bitmap as small as possible */
	if (peer->adjout_bid == 0)
		if (bitmap_id_get(&adjout_id_map, &peer->adjout_bid) == -1)
			fatal(__func__);

	if ((p = adjout_prefix_with_attrs(pte, path_id_tx, attrs)) == NULL) {
		p = adjout_prefix_alloc(pte, path_id_tx);
		p->attrs = adjout_attr_ref(attrs);
	}

	if (bitmap_set(&p->peermap, peer->adjout_bid) == -1)
		fatal(__func__);
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
adjout_prefix_unlink(struct adjout_prefix *p, struct pt_entry *pte,
    struct rde_peer *peer)
{
	bitmap_clear(&p->peermap, peer->adjout_bid);
	if (bitmap_empty(&p->peermap)) {
		/* destroy all references to other objects */
		adjout_attr_unref(p->attrs);
		p->attrs = NULL;

		adjout_prefix_free(pte, p);
	}
}

static void
adjout_prefix_resize(struct pt_entry *pte)
{
	struct adjout_prefix *new;
	uint32_t newlen, avail;

	avail = pte->adjoutavail;
	newlen = bin_of_adjout_prefixes(avail + 1);
	if ((new = reallocarray(pte->adjout, newlen, sizeof(*new))) == NULL)
		fatal(__func__);
	rdemem.adjout_prefix_size += sizeof(*new) * (newlen - avail);

	memset(&new[avail], 0, sizeof(*new) * (newlen - avail));
	pte->adjout = new;
	pte->adjoutavail = newlen;
}

/*
 * Insert a new entry into the pte adjout array, extending the array if needed.
 * May not fail.
 */
static struct adjout_prefix *
adjout_prefix_alloc(struct pt_entry *pte, uint32_t path_id_tx)
{
	struct adjout_prefix *p;
	uint32_t i;

	if (pte->adjoutlen + 1 > pte->adjoutavail)
		adjout_prefix_resize(pte);

	/* keep array sorted by path_id_tx */
	for (i = 0; i < pte->adjoutlen; i++) {
		if (pte->adjout[i].path_id_tx > path_id_tx)
			break;
	}

	p = &pte->adjout[i];
	/* shift reminder by one slot */
	for (i = pte->adjoutlen; &pte->adjout[i] > p; i--)
		pte->adjout[i] = pte->adjout[i - 1];

	/* initialize new element */
	p->attrs = NULL;
	p->path_id_tx = path_id_tx;
	bitmap_init(&p->peermap);

	pte->adjoutlen++;
	rdemem.adjout_prefix_cnt++;
	return p;
}

/* remove an entry from the pte adjout array */
static void
adjout_prefix_free(struct pt_entry *pte, struct adjout_prefix *p)
{
	uint32_t i, idx;

	bitmap_reset(&p->peermap);

	idx = adjout_prefix_index(pte, p);
	for (i = idx + 1; i < pte->adjoutlen; i++)
		pte->adjout[i - 1] = pte->adjout[i];

	p = &pte->adjout[pte->adjoutlen - 1];
	memset(p, 0, sizeof(*p));
	pte->adjoutlen--;

	/* TODO shrink array if X% empty */

	rdemem.adjout_prefix_cnt--;
}

void
adjout_peer_init(struct rde_peer *peer)
{
	unsigned int i;

	CH_INIT(pend_attr_hash, &peer->pend_attrs);
	CH_INIT(pend_prefix_hash, &peer->pend_prefixes);
	for (i = 0; i < nitems(peer->updates); i++)
		TAILQ_INIT(&peer->updates[i]);
	for (i = 0; i < nitems(peer->withdraws); i++)
		TAILQ_INIT(&peer->withdraws[i]);
}

void
adjout_peer_flush_pending(struct rde_peer *peer)
{
	struct pend_attr *pa, *npa;
	struct pend_prefix *pp, *npp;
	uint8_t aid;

	for (aid = AID_MIN; aid < AID_MAX; aid++) {
		TAILQ_FOREACH_SAFE(pp, &peer->withdraws[aid], entry, npp) {
			pend_prefix_free(pp, &peer->withdraws[aid], peer);
		}
		TAILQ_FOREACH_SAFE(pa, &peer->updates[aid], entry, npa) {
			TAILQ_FOREACH_SAFE(pp, &pa->prefixes, entry, npp) {
				pend_prefix_free(pp, &pa->prefixes, peer);
			}
			pend_attr_done(pa, peer);
		}
	}
}

void
adjout_peer_free(struct rde_peer *peer)
{
	adjout_peer_flush_pending(peer);	/* not strictly needed */
	CH_DESTROY(pend_attr_hash, &peer->pend_attrs);
	CH_DESTROY(pend_prefix_hash, &peer->pend_prefixes);
}
