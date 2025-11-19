/*	$OpenBSD: rde_adjout.c,v 1.2 2025/11/19 09:49:27 claudio Exp $ */

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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

/* adj-rib-out specific functions */

static inline struct prefix *
prefix_lock(struct prefix *p)
{
	if (p->flags & PREFIX_FLAG_LOCKED)
		fatalx("%s: locking locked prefix", __func__);
	p->flags |= PREFIX_FLAG_LOCKED;
	return p;
}

static inline struct prefix *
prefix_unlock(struct prefix *p)
{
	if ((p->flags & PREFIX_FLAG_LOCKED) == 0)
		fatalx("%s: unlocking unlocked prefix", __func__);
	p->flags &= ~PREFIX_FLAG_LOCKED;
	return p;
}

static inline int
prefix_is_locked(struct prefix *p)
{
	return (p->flags & PREFIX_FLAG_LOCKED) != 0;
}

static inline int
prefix_is_dead(struct prefix *p)
{
	return (p->flags & PREFIX_FLAG_DEAD) != 0;
}

static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);

/* RB tree comparison function */
static inline int
prefix_index_cmp(struct prefix *a, struct prefix *b)
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
prefix_cmp(struct prefix *a, struct prefix *b)
{
	if ((a->flags & PREFIX_FLAG_EOR) != (b->flags & PREFIX_FLAG_EOR))
		return (a->flags & PREFIX_FLAG_EOR) ? 1 : -1;
	/* if EOR marker no need to check the rest */
	if (a->flags & PREFIX_FLAG_EOR)
		return 0;

	if (a->aspath != b->aspath)
		return (a->aspath > b->aspath ? 1 : -1);
	if (a->communities != b->communities)
		return (a->communities > b->communities ? 1 : -1);
	if (a->nexthop != b->nexthop)
		return (a->nexthop > b->nexthop ? 1 : -1);
	if (a->nhflags != b->nhflags)
		return (a->nhflags > b->nhflags ? 1 : -1);
	return prefix_index_cmp(a, b);
}

RB_GENERATE(prefix_tree, prefix, entry.tree.update, prefix_cmp)
RB_GENERATE_STATIC(prefix_index, prefix, entry.tree.index, prefix_index_cmp)

/*
 * Search for specified prefix in the peer prefix_index.
 * Returns NULL if not found.
 */
struct prefix *
prefix_adjout_get(struct rde_peer *peer, uint32_t path_id_tx,
    struct pt_entry *pte)
{
	struct prefix xp;

	memset(&xp, 0, sizeof(xp));
	xp.pt = pte;
	xp.path_id_tx = path_id_tx;

	return RB_FIND(prefix_index, &peer->adj_rib_out, &xp);
}

/*
 * Lookup a prefix without considering path_id in the peer prefix_index.
 * Returns NULL if not found.
 */
struct prefix *
prefix_adjout_first(struct rde_peer *peer, struct pt_entry *pte)
{
	struct prefix xp, *np;

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
struct prefix *
prefix_adjout_next(struct rde_peer *peer, struct prefix *p)
{
	struct prefix *np;

	np = RB_NEXT(prefix_index, &peer->adj_rib_out, p);
	if (np == NULL || np->pt != p->pt)
		return NULL;
	return np;
}

/*
 * Lookup addr/prefixlen in the peer prefix_index. Returns first match.
 * Returns NULL if not found.
 */
struct prefix *
prefix_adjout_lookup(struct rde_peer *peer, struct bgpd_addr *addr, int plen)
{
	return prefix_adjout_first(peer, pt_fill(addr, plen));
}

/*
 * Lookup addr in the peer prefix_index. Returns first match.
 * Returns NULL if not found.
 */
struct prefix *
prefix_adjout_match(struct rde_peer *peer, struct bgpd_addr *addr)
{
	struct prefix *p;
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
	struct prefix *p;

	p = prefix_alloc();
	p->flags = PREFIX_FLAG_ADJOUT | PREFIX_FLAG_UPDATE | PREFIX_FLAG_EOR;
	if (RB_INSERT(prefix_tree, &peer->updates[aid], p) != NULL)
		/* no need to add if EoR marker already present */
		prefix_free(p);
	/* EOR marker is not inserted into the adj_rib_out index */
}

/*
 * Put a prefix from the Adj-RIB-Out onto the update queue.
 */
void
prefix_adjout_update(struct prefix *p, struct rde_peer *peer,
    struct filterstate *state, struct pt_entry *pte, uint32_t path_id_tx)
{
	struct rde_aspath *asp;
	struct rde_community *comm;

	if (p == NULL) {
		p = prefix_alloc();
		/* initially mark DEAD so code below is skipped */
		p->flags |= PREFIX_FLAG_ADJOUT | PREFIX_FLAG_DEAD;

		p->pt = pt_ref(pte);
		p->peer = peer;
		p->path_id_tx = path_id_tx;

		if (RB_INSERT(prefix_index, &peer->adj_rib_out, p) != NULL)
			fatalx("%s: RB index invariant violated", __func__);
	}

	if ((p->flags & PREFIX_FLAG_ADJOUT) == 0)
		fatalx("%s: prefix without PREFIX_FLAG_ADJOUT hit", __func__);
	if ((p->flags & (PREFIX_FLAG_WITHDRAW | PREFIX_FLAG_DEAD)) == 0) {
		/*
		 * XXX for now treat a different path_id_tx like different
		 * attributes and force out an update. It is unclear how
		 * common it is to have equivalent updates from alternative
		 * paths.
		 */
		if (p->path_id_tx == path_id_tx &&
		    prefix_nhflags(p) == state->nhflags &&
		    prefix_nexthop(p) == state->nexthop &&
		    communities_equal(&state->communities,
		    prefix_communities(p)) &&
		    path_equal(&state->aspath, prefix_aspath(p))) {
			/* nothing changed */
			p->validation_state = state->vstate;
			p->lastchange = getmonotime();
			p->flags &= ~PREFIX_FLAG_STALE;
			return;
		}

		/* if pending update unhook it before it is unlinked */
		if (p->flags & PREFIX_FLAG_UPDATE) {
			RB_REMOVE(prefix_tree, &peer->updates[pte->aid], p);
			peer->stats.pending_update--;
		}

		/* unlink prefix so it can be relinked below */
		prefix_unlink(p);
		peer->stats.prefix_out_cnt--;
	}
	if (p->flags & PREFIX_FLAG_WITHDRAW) {
		RB_REMOVE(prefix_tree, &peer->withdraws[pte->aid], p);
		peer->stats.pending_withdraw--;
	}

	/* nothing needs to be done for PREFIX_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_FLAG_MASK;

	/* update path_id_tx now that the prefix is unlinked */
	if (p->path_id_tx != path_id_tx) {
		/* path_id_tx is part of the index so remove and re-insert p */
		RB_REMOVE(prefix_index, &peer->adj_rib_out, p);
		p->path_id_tx = path_id_tx;
		if (RB_INSERT(prefix_index, &peer->adj_rib_out, p) != NULL)
			fatalx("%s: RB index invariant violated", __func__);
	}

	asp = path_getcache(&state->aspath);

	if ((comm = communities_lookup(&state->communities)) == NULL) {
		/* Communities not available, create and link a new one. */
		comm = communities_link(&state->communities);
	}

	prefix_link(p, NULL, p->pt, peer, 0, p->path_id_tx, asp, comm,
	    state->nexthop, state->nhflags, state->vstate);
	peer->stats.prefix_out_cnt++;

	if (p->flags & PREFIX_FLAG_MASK)
		fatalx("%s: bad flags %x", __func__, p->flags);
	if (peer_is_up(peer)) {
		p->flags |= PREFIX_FLAG_UPDATE;
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
prefix_adjout_withdraw(struct prefix *p)
{
	struct rde_peer *peer = prefix_peer(p);

	if ((p->flags & PREFIX_FLAG_ADJOUT) == 0)
		fatalx("%s: prefix without PREFIX_FLAG_ADJOUT hit", __func__);

	/* already a withdraw, shortcut */
	if (p->flags & PREFIX_FLAG_WITHDRAW) {
		p->lastchange = getmonotime();
		p->flags &= ~PREFIX_FLAG_STALE;
		return;
	}
	/* pending update just got withdrawn */
	if (p->flags & PREFIX_FLAG_UPDATE) {
		RB_REMOVE(prefix_tree, &peer->updates[p->pt->aid], p);
		peer->stats.pending_update--;
	}
	/* unlink prefix if it was linked (not a withdraw or dead) */
	if ((p->flags & (PREFIX_FLAG_WITHDRAW | PREFIX_FLAG_DEAD)) == 0) {
		prefix_unlink(p);
		peer->stats.prefix_out_cnt--;
	}

	/* nothing needs to be done for PREFIX_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_FLAG_MASK;
	p->lastchange = getmonotime();

	if (peer_is_up(peer)) {
		p->flags |= PREFIX_FLAG_WITHDRAW;
		if (RB_INSERT(prefix_tree, &peer->withdraws[p->pt->aid],
		    p) != NULL)
			fatalx("%s: RB tree invariant violated", __func__);
		peer->stats.pending_withdraw++;
	} else {
		/* mark prefix dead to skip unlink on destroy */
		p->flags |= PREFIX_FLAG_DEAD;
		prefix_adjout_destroy(p);
	}
}

void
prefix_adjout_destroy(struct prefix *p)
{
	struct rde_peer *peer = prefix_peer(p);

	if ((p->flags & PREFIX_FLAG_ADJOUT) == 0)
		fatalx("%s: prefix without PREFIX_FLAG_ADJOUT hit", __func__);

	if (p->flags & PREFIX_FLAG_EOR) {
		/* EOR marker is not linked in the index */
		prefix_free(p);
		return;
	}

	if (p->flags & PREFIX_FLAG_WITHDRAW) {
		RB_REMOVE(prefix_tree, &peer->withdraws[p->pt->aid], p);
		peer->stats.pending_withdraw--;
	}
	if (p->flags & PREFIX_FLAG_UPDATE) {
		RB_REMOVE(prefix_tree, &peer->updates[p->pt->aid], p);
		peer->stats.pending_update--;
	}
	/* unlink prefix if it was linked (not a withdraw or dead) */
	if ((p->flags & (PREFIX_FLAG_WITHDRAW | PREFIX_FLAG_DEAD)) == 0) {
		prefix_unlink(p);
		peer->stats.prefix_out_cnt--;
	}

	/* nothing needs to be done for PREFIX_FLAG_DEAD and STALE */
	p->flags &= ~PREFIX_FLAG_MASK;

	if (prefix_is_locked(p)) {
		/* mark prefix dead but leave it for prefix_restart */
		p->flags |= PREFIX_FLAG_DEAD;
	} else {
		RB_REMOVE(prefix_index, &peer->adj_rib_out, p);
		/* remove the last prefix reference before free */
		pt_unref(p->pt);
		prefix_free(p);
	}
}

void
prefix_adjout_flush_pending(struct rde_peer *peer)
{
	struct prefix *p, *np;
	uint8_t aid;

	for (aid = AID_MIN; aid < AID_MAX; aid++) {
		RB_FOREACH_SAFE(p, prefix_tree, &peer->withdraws[aid], np) {
			prefix_adjout_destroy(p);
		}
		RB_FOREACH_SAFE(p, prefix_tree, &peer->updates[aid], np) {
			p->flags &= ~PREFIX_FLAG_UPDATE;
			RB_REMOVE(prefix_tree, &peer->updates[aid], p);
			if (p->flags & PREFIX_FLAG_EOR) {
				prefix_adjout_destroy(p);
			} else {
				peer->stats.pending_update--;
			}
		}
	}
}

int
prefix_adjout_reaper(struct rde_peer *peer)
{
	struct prefix *p, *np;
	int count = RDE_REAPER_ROUNDS;

	RB_FOREACH_SAFE(p, prefix_index, &peer->adj_rib_out, np) {
		prefix_adjout_destroy(p);
		if (count-- <= 0)
			return 0;
	}
	return 1;
}

static struct prefix *
prefix_restart(struct rib_context *ctx)
{
	struct prefix *p = NULL;

	if (ctx->ctx_p)
		p = prefix_unlock(ctx->ctx_p);

	if (p && prefix_is_dead(p)) {
		struct prefix *next;

		next = RB_NEXT(prefix_index, unused, p);
		prefix_adjout_destroy(p);
		p = next;
	}
	ctx->ctx_p = NULL;
	return p;
}

void
prefix_adjout_dump_cleanup(struct prefix *p)
{
	if (prefix_is_dead(prefix_unlock(p)))
		prefix_adjout_destroy(p);
}

void
prefix_adjout_dump_r(struct rib_context *ctx)
{
	struct prefix *p, *next;
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
			ctx->ctx_p = prefix_lock(p);
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
    void *arg, void (*upcall)(struct prefix *, void *),
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
    void (*upcall)(struct prefix *, void *), void (*done)(void *, uint8_t),
    int (*throttle)(void *))
{
	struct rib_context *ctx;
	struct prefix xp;

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
		prefix_lock(ctx->ctx_p);

	rib_dump_insert(ctx);

	/* requested a sync traversal */
	if (count == 0)
		prefix_adjout_dump_r(ctx);

	return 0;
}

/* alloc and zero new entry. May not fail. */
static struct prefix *
prefix_alloc(void)
{
	struct prefix *p;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("prefix_alloc");
	rdemem.prefix_cnt++;
	return p;
}

/* free a unlinked entry */
static void
prefix_free(struct prefix *p)
{
	rdemem.prefix_cnt--;
	free(p);
}
