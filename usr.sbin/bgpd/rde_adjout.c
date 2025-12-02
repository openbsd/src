/*	$OpenBSD: rde_adjout.c,v 1.7 2025/12/02 10:50:19 claudio Exp $ */

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
}

/* Alloc, init and add a new entry into the has table. May not fail. */
static struct adjout_attr *
adjout_attr_alloc(struct rde_aspath *asp, struct rde_community *comm,
    struct nexthop *nexthop, uint64_t hash)
{
	struct adjout_attr *a;

	a = calloc(1, sizeof(*a));
	if (a == NULL)
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
adjout_attr_ref(struct adjout_attr *attrs, struct rde_peer *peer)
{
	attrs->refcnt++;
	rdemem.adjout_attr_refs++;
	return attrs;
}

static void
adjout_attr_unref(struct adjout_attr *attrs, struct rde_peer *peer)
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

static inline struct prefix_adjout *
prefix_adjout_lock(struct prefix_adjout *p)
{
	if (p->flags & PREFIX_ADJOUT_FLAG_LOCKED)
		fatalx("%s: locking locked prefix", __func__);
	p->flags |= PREFIX_ADJOUT_FLAG_LOCKED;
	return p;
}

static inline struct prefix_adjout *
prefix_adjout_unlock(struct prefix_adjout *p)
{
	if ((p->flags & PREFIX_ADJOUT_FLAG_LOCKED) == 0)
		fatalx("%s: unlocking unlocked prefix", __func__);
	p->flags &= ~PREFIX_ADJOUT_FLAG_LOCKED;
	return p;
}

static inline int
prefix_is_locked(struct prefix_adjout *p)
{
	return (p->flags & PREFIX_ADJOUT_FLAG_LOCKED) != 0;
}

static inline int
prefix_is_dead(struct prefix_adjout *p)
{
	return (p->flags & PREFIX_ADJOUT_FLAG_DEAD) != 0;
}

static void	 prefix_adjout_link(struct prefix_adjout *, struct rde_peer *,
		    struct adjout_attr *, struct pt_entry *, uint32_t);
static void	 prefix_adjout_unlink(struct prefix_adjout *,
		    struct rde_peer *);

static struct prefix_adjout	*prefix_adjout_alloc(void);
static void			 prefix_adjout_free(struct prefix_adjout *);

/* RB tree comparison function */
static inline int
prefix_index_cmp(struct prefix_adjout *a, struct prefix_adjout *b)
{
	int r;
	r = pt_prefix_cmp(a->pt, b->pt);
	if (r != 0)
		return r;

	if (a->path_id_tx > b->path_id_tx)
		return 1;
	if (a->path_id_tx < b->path_id_tx)
		return -1;
	return 0;
}

static inline int
prefix_cmp(struct prefix_adjout *a, struct prefix_adjout *b)
{
	if ((a->flags & PREFIX_ADJOUT_FLAG_EOR) !=
	    (b->flags & PREFIX_ADJOUT_FLAG_EOR))
		return (a->flags & PREFIX_ADJOUT_FLAG_EOR) ? 1 : -1;
	/* if EOR marker no need to check the rest */
	if (a->flags & PREFIX_ADJOUT_FLAG_EOR)
		return 0;

	if (a->attrs != b->attrs)
		return (a->attrs > b->attrs ? 1 : -1);
	return prefix_index_cmp(a, b);
}

RB_GENERATE(prefix_tree, prefix_adjout, update, prefix_cmp)
RB_GENERATE_STATIC(prefix_index, prefix_adjout, index, prefix_index_cmp)

/*
 * Search for specified prefix in the peer prefix_index.
 * Returns NULL if not found.
 */
struct prefix_adjout *
prefix_adjout_get(struct rde_peer *peer, uint32_t path_id_tx,
    struct pt_entry *pte)
{
	struct prefix_adjout xp;

	memset(&xp, 0, sizeof(xp));
	xp.pt = pte;
	xp.path_id_tx = path_id_tx;

	return RB_FIND(prefix_index, &peer->adj_rib_out, &xp);
}

/*
 * Lookup a prefix without considering path_id in the peer prefix_index.
 * Returns NULL if not found.
 */
struct prefix_adjout *
prefix_adjout_first(struct rde_peer *peer, struct pt_entry *pte)
{
	struct prefix_adjout xp, *np;

	memset(&xp, 0, sizeof(xp));
	xp.pt = pte;

	np = RB_NFIND(prefix_index, &peer->adj_rib_out, &xp);
	if (np == NULL || pt_prefix_cmp(np->pt, xp.pt) != 0)
		return NULL;
	return np;
}

/*
 * Return next prefix after a lookup that is actually an update.
 */
struct prefix_adjout *
prefix_adjout_next(struct rde_peer *peer, struct prefix_adjout *p)
{
	struct prefix_adjout *np;

	np = RB_NEXT(prefix_index, &peer->adj_rib_out, p);
	if (np == NULL || np->pt != p->pt)
		return NULL;
	return np;
}

/*
 * Lookup addr/prefixlen in the peer prefix_index. Returns first match.
 * Returns NULL if not found.
 */
struct prefix_adjout *
prefix_adjout_lookup(struct rde_peer *peer, struct bgpd_addr *addr, int plen)
{
	return prefix_adjout_first(peer, pt_fill(addr, plen));
}

/*
 * Lookup addr in the peer prefix_index. Returns first match.
 * Returns NULL if not found.
 */
struct prefix_adjout *
prefix_adjout_match(struct rde_peer *peer, struct bgpd_addr *addr)
{
	struct prefix_adjout *p;
	int i;

	switch (addr->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		for (i = 32; i >= 0; i--) {
			p = prefix_adjout_lookup(peer, addr, i);
			if (p != NULL)
				return p;
		}
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		for (i = 128; i >= 0; i--) {
			p = prefix_adjout_lookup(peer, addr, i);
			if (p != NULL)
				return p;
		}
		break;
	default:
		fatalx("%s: unknown af", __func__);
	}
	return NULL;
}

/*
 * Insert an End-of-RIB marker into the update queue.
 */
void
prefix_add_eor(struct rde_peer *peer, uint8_t aid)
{
	struct prefix_adjout *p;

	p = prefix_adjout_alloc();
	p->flags = PREFIX_ADJOUT_FLAG_UPDATE | PREFIX_ADJOUT_FLAG_EOR;
	if (RB_INSERT(prefix_tree, &peer->updates[aid], p) != NULL)
		/* no need to add if EoR marker already present */
		prefix_adjout_free(p);
	/* EOR marker is not inserted into the adj_rib_out index */
}

/*
 * Put a prefix from the Adj-RIB-Out onto the update queue.
 */
void
prefix_adjout_update(struct prefix_adjout *p, struct rde_peer *peer,
    struct filterstate *state, struct pt_entry *pte, uint32_t path_id_tx)
{
	struct adjout_attr *attrs;

	if (p == NULL) {
		p = prefix_adjout_alloc();
		/* initially mark DEAD so code below is skipped */
		p->flags |= PREFIX_ADJOUT_FLAG_DEAD;

		p->pt = pt_ref(pte);
		p->path_id_tx = path_id_tx;

		if (RB_INSERT(prefix_index, &peer->adj_rib_out, p) != NULL)
			fatalx("%s: RB index invariant violated", __func__);
	}

	if ((p->flags & (PREFIX_ADJOUT_FLAG_WITHDRAW |
	    PREFIX_ADJOUT_FLAG_DEAD)) == 0) {
		/*
		 * XXX for now treat a different path_id_tx like different
		 * attributes and force out an update. It is unclear how
		 * common it is to have equivalent updates from alternative
		 * paths.
		 */
		if (p->path_id_tx == path_id_tx &&
		    prefix_adjout_nexthop(p) == state->nexthop &&
		    communities_equal(&state->communities,
		    prefix_adjout_communities(p)) &&
		    path_equal(&state->aspath, prefix_adjout_aspath(p))) {
			/* nothing changed */
			p->flags &= ~PREFIX_ADJOUT_FLAG_STALE;
			return;
		}

		/* if pending update unhook it before it is unlinked */
		if (p->flags & PREFIX_ADJOUT_FLAG_UPDATE) {
			RB_REMOVE(prefix_tree, &peer->updates[pte->aid], p);
			peer->stats.pending_update--;
		}

		/* unlink prefix so it can be relinked below */
		prefix_adjout_unlink(p, peer);
		peer->stats.prefix_out_cnt--;
	}
	if (p->flags & PREFIX_ADJOUT_FLAG_WITHDRAW) {
		RB_REMOVE(prefix_tree, &peer->withdraws[pte->aid], p);
		peer->stats.pending_withdraw--;
	}

	/* nothing needs to be done for PREFIX_ADJOUT_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_ADJOUT_FLAG_MASK;

	/* update path_id_tx now that the prefix is unlinked */
	if (p->path_id_tx != path_id_tx) {
		/* path_id_tx is part of the index so remove and re-insert p */
		RB_REMOVE(prefix_index, &peer->adj_rib_out, p);
		p->path_id_tx = path_id_tx;
		if (RB_INSERT(prefix_index, &peer->adj_rib_out, p) != NULL)
			fatalx("%s: RB index invariant violated", __func__);
	}

	attrs = adjout_attr_get(state);

	prefix_adjout_link(p, peer, attrs, p->pt, p->path_id_tx);
	peer->stats.prefix_out_cnt++;

	if (p->flags & PREFIX_ADJOUT_FLAG_MASK)
		fatalx("%s: bad flags %x", __func__, p->flags);
	if (peer_is_up(peer)) {
		p->flags |= PREFIX_ADJOUT_FLAG_UPDATE;
		if (RB_INSERT(prefix_tree, &peer->updates[pte->aid], p) != NULL)
			fatalx("%s: RB tree invariant violated", __func__);
		peer->stats.pending_update++;
	}
}

/*
 * Withdraw a prefix from the Adj-RIB-Out, this unlinks the aspath but leaves
 * the prefix in the RIB linked to the peer withdraw list.
 */
void
prefix_adjout_withdraw(struct rde_peer *peer, struct prefix_adjout *p)
{
	/* already a withdraw, shortcut */
	if (p->flags & PREFIX_ADJOUT_FLAG_WITHDRAW) {
		p->flags &= ~PREFIX_ADJOUT_FLAG_STALE;
		return;
	}
	/* pending update just got withdrawn */
	if (p->flags & PREFIX_ADJOUT_FLAG_UPDATE) {
		RB_REMOVE(prefix_tree, &peer->updates[p->pt->aid], p);
		peer->stats.pending_update--;
	}
	/* unlink prefix if it was linked (not a withdraw or dead) */
	if ((p->flags & (PREFIX_ADJOUT_FLAG_WITHDRAW |
	    PREFIX_ADJOUT_FLAG_DEAD)) == 0) {
		prefix_adjout_unlink(p, peer);
		peer->stats.prefix_out_cnt--;
	}

	/* nothing needs to be done for PREFIX_ADJOUT_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_ADJOUT_FLAG_MASK;

	if (peer_is_up(peer)) {
		p->flags |= PREFIX_ADJOUT_FLAG_WITHDRAW;
		if (RB_INSERT(prefix_tree, &peer->withdraws[p->pt->aid],
		    p) != NULL)
			fatalx("%s: RB tree invariant violated", __func__);
		peer->stats.pending_withdraw++;
	} else {
		/* mark prefix dead to skip unlink on destroy */
		p->flags |= PREFIX_ADJOUT_FLAG_DEAD;
		prefix_adjout_destroy(peer, p);
	}
}

void
prefix_adjout_destroy(struct rde_peer *peer, struct prefix_adjout *p)
{
	if (p->flags & PREFIX_ADJOUT_FLAG_EOR) {
		/* EOR marker is not linked in the index */
		prefix_adjout_free(p);
		return;
	}

	if (p->flags & PREFIX_ADJOUT_FLAG_WITHDRAW) {
		RB_REMOVE(prefix_tree, &peer->withdraws[p->pt->aid], p);
		peer->stats.pending_withdraw--;
	}
	if (p->flags & PREFIX_ADJOUT_FLAG_UPDATE) {
		RB_REMOVE(prefix_tree, &peer->updates[p->pt->aid], p);
		peer->stats.pending_update--;
	}
	/* unlink prefix if it was linked (not a withdraw or dead) */
	if ((p->flags & (PREFIX_ADJOUT_FLAG_WITHDRAW |
	    PREFIX_ADJOUT_FLAG_DEAD)) == 0) {
		prefix_adjout_unlink(p, peer);
		peer->stats.prefix_out_cnt--;
	}

	/* nothing needs to be done for PREFIX_ADJOUT_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_ADJOUT_FLAG_MASK;

	if (prefix_is_locked(p)) {
		/* mark prefix dead but leave it for prefix_restart */
		p->flags |= PREFIX_ADJOUT_FLAG_DEAD;
	} else {
		RB_REMOVE(prefix_index, &peer->adj_rib_out, p);
		/* remove the last prefix reference before free */
		pt_unref(p->pt);
		prefix_adjout_free(p);
	}
}

void
prefix_adjout_flush_pending(struct rde_peer *peer)
{
	struct prefix_adjout *p, *np;
	uint8_t aid;

	for (aid = AID_MIN; aid < AID_MAX; aid++) {
		RB_FOREACH_SAFE(p, prefix_tree, &peer->withdraws[aid], np) {
			prefix_adjout_destroy(peer, p);
		}
		RB_FOREACH_SAFE(p, prefix_tree, &peer->updates[aid], np) {
			p->flags &= ~PREFIX_ADJOUT_FLAG_UPDATE;
			RB_REMOVE(prefix_tree, &peer->updates[aid], p);
			if (p->flags & PREFIX_ADJOUT_FLAG_EOR) {
				prefix_adjout_destroy(peer, p);
			} else {
				peer->stats.pending_update--;
			}
		}
	}
}

int
prefix_adjout_reaper(struct rde_peer *peer)
{
	struct prefix_adjout *p, *np;
	int count = RDE_REAPER_ROUNDS;

	RB_FOREACH_SAFE(p, prefix_index, &peer->adj_rib_out, np) {
		prefix_adjout_destroy(peer, p);
		if (count-- <= 0)
			return 0;
	}
	return 1;
}

static struct prefix_adjout *
prefix_restart(struct rib_context *ctx)
{
	struct prefix_adjout *p = NULL;
	struct rde_peer *peer;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		return NULL;

	if (ctx->ctx_p)
		p = prefix_adjout_unlock(ctx->ctx_p);

	while (p && prefix_is_dead(p)) {
		struct prefix_adjout *next;

		next = RB_NEXT(prefix_index, unused, p);
		prefix_adjout_destroy(peer, p);
		p = next;
	}
	ctx->ctx_p = NULL;
	return p;
}

void
prefix_adjout_dump_cleanup(struct rib_context *ctx)
{
	struct prefix_adjout *p = ctx->ctx_p;
	struct rde_peer *peer;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		return;
	if (prefix_is_dead(prefix_adjout_unlock(p)))
		prefix_adjout_destroy(peer, p);
}

void
prefix_adjout_dump_r(struct rib_context *ctx)
{
	struct prefix_adjout *p, *next;
	struct rde_peer *peer;
	unsigned int i;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		goto done;

	if (ctx->ctx_p == NULL && ctx->ctx_subtree.aid == AID_UNSPEC)
		p = RB_MIN(prefix_index, &peer->adj_rib_out);
	else
		p = prefix_restart(ctx);

	for (i = 0; p != NULL; p = next) {
		next = RB_NEXT(prefix_index, unused, p);
		if (prefix_is_dead(p))
			continue;
		if (ctx->ctx_aid != AID_UNSPEC &&
		    ctx->ctx_aid != p->pt->aid)
			continue;
		if (ctx->ctx_subtree.aid != AID_UNSPEC) {
			struct bgpd_addr addr;
			pt_getaddr(p->pt, &addr);
			if (prefix_compare(&ctx->ctx_subtree, &addr,
			    ctx->ctx_subtreelen) != 0)
				/* left subtree, walk is done */
				break;
		}
		if (ctx->ctx_count && i++ >= ctx->ctx_count &&
		    !prefix_is_locked(p)) {
			/* store and lock last element */
			ctx->ctx_p = prefix_adjout_lock(p);
			return;
		}
		ctx->ctx_prefix_call(p, ctx->ctx_arg);
	}

done:
	if (ctx->ctx_done)
		ctx->ctx_done(ctx->ctx_arg, ctx->ctx_aid);
	LIST_REMOVE(ctx, entry);
	free(ctx);
}

int
prefix_adjout_dump_new(struct rde_peer *peer, uint8_t aid, unsigned int count,
    void *arg, void (*upcall)(struct prefix_adjout *, void *),
    void (*done)(void *, uint8_t), int (*throttle)(void *))
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
		prefix_adjout_dump_r(ctx);

	return 0;
}

int
prefix_adjout_dump_subtree(struct rde_peer *peer, struct bgpd_addr *subtree,
    uint8_t subtreelen, unsigned int count, void *arg,
    void (*upcall)(struct prefix_adjout *, void *),
    void (*done)(void *, uint8_t),
    int (*throttle)(void *))
{
	struct rib_context *ctx;
	struct prefix_adjout xp;

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
	memset(&xp, 0, sizeof(xp));
	xp.pt = pt_fill(subtree, subtreelen);
	ctx->ctx_p = RB_NFIND(prefix_index, &peer->adj_rib_out, &xp);
	if (ctx->ctx_p)
		prefix_adjout_lock(ctx->ctx_p);

	rib_dump_insert(ctx);

	/* requested a sync traversal */
	if (count == 0)
		prefix_adjout_dump_r(ctx);

	return 0;
}

/*
 * Link a prefix into the different parent objects.
 */
static void
prefix_adjout_link(struct prefix_adjout *p, struct rde_peer *peer,
    struct adjout_attr *attrs, struct pt_entry *pt, uint32_t path_id_tx)
{
	p->attrs = adjout_attr_ref(attrs, peer);
	p->pt = pt_ref(pt);
	p->path_id_tx = path_id_tx;
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
prefix_adjout_unlink(struct prefix_adjout *p, struct rde_peer *peer)
{
	/* destroy all references to other objects */
	adjout_attr_unref(p->attrs, peer);
	p->attrs = NULL;
	pt_unref(p->pt);
	/* must keep p->pt valid since there is an extra ref */
}

/* alloc and zero new entry. May not fail. */
static struct prefix_adjout *
prefix_adjout_alloc(void)
{
	struct prefix_adjout *p;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal(__func__);
	rdemem.adjout_prefix_cnt++;
	return p;
}

/* free a unlinked entry */
static void
prefix_adjout_free(struct prefix_adjout *p)
{
	rdemem.adjout_prefix_cnt--;
	free(p);
}
