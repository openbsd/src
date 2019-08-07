/*	$OpenBSD: rde_rib.c,v 1.202 2019/08/07 10:26:41 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
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
#include <siphash.h>
#include <time.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

/*
 * BGP RIB -- Routing Information Base
 *
 * The RIB is build with one aspect in mind. Speed -- actually update speed.
 * Therefore one thing needs to be absolutely avoided, long table walks.
 * This is achieved by heavily linking the different parts together.
 */
u_int16_t rib_size;
struct rib_desc *ribs;

struct rib_entry *rib_add(struct rib *, struct bgpd_addr *, int);
static inline int rib_compare(const struct rib_entry *,
			const struct rib_entry *);
void rib_remove(struct rib_entry *);
int rib_empty(struct rib_entry *);
static void rib_dump_abort(u_int16_t);

RB_PROTOTYPE(rib_tree, rib_entry, rib_e, rib_compare);
RB_GENERATE(rib_tree, rib_entry, rib_e, rib_compare);

struct rib_context {
	LIST_ENTRY(rib_context)		 entry;
	struct rib_entry		*ctx_re;
	struct prefix			*ctx_p;
	u_int32_t			 ctx_id;
	void		(*ctx_rib_call)(struct rib_entry *, void *);
	void		(*ctx_prefix_call)(struct prefix *, void *);
	void		(*ctx_done)(void *, u_int8_t);
	int		(*ctx_throttle)(void *);
	void				*ctx_arg;
	unsigned int			 ctx_count;
	u_int8_t			 ctx_aid;
};
LIST_HEAD(, rib_context) rib_dumps = LIST_HEAD_INITIALIZER(rib_dumps);

static int	prefix_add(struct bgpd_addr *, int, struct rib *,
		    struct rde_peer *, struct rde_aspath *,
		    struct rde_community *, struct nexthop *,
		    u_int8_t, u_int8_t);
static int	prefix_move(struct prefix *, struct rde_peer *,
		    struct rde_aspath *, struct rde_community *,
		    struct nexthop *, u_int8_t, u_int8_t);
static void	prefix_dump_r(struct rib_context *);

static inline struct rib_entry *
re_lock(struct rib_entry *re)
{
	if (re->lock != 0)
		log_warnx("%s: entry already locked", __func__);
	re->lock = 1;
	return re;
}

static inline struct rib_entry *
re_unlock(struct rib_entry *re)
{
	if (re->lock == 0)
		log_warnx("%s: entry already unlocked", __func__);
	re->lock = 0;
	return re;
}

static inline int
re_is_locked(struct rib_entry *re)
{
	return (re->lock != 0);
}

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

static inline struct rib_tree *
rib_tree(struct rib *rib)
{
	return (&rib->tree);
}

static inline int
rib_compare(const struct rib_entry *a, const struct rib_entry *b)
{
	return (pt_prefix_cmp(a->prefix, b->prefix));
}

/* RIB specific functions */
struct rib *
rib_new(char *name, u_int rtableid, u_int16_t flags)
{
	struct rib_desc	*xribs;
	u_int16_t	id;

	for (id = 0; id < rib_size; id++) {
		if (!rib_valid(id))
			break;
	}

	if (id >= rib_size) {
		if ((xribs = reallocarray(ribs, id + 1,
		    sizeof(struct rib_desc))) == NULL) {
			/* XXX this is not clever */
			fatal(NULL);
		}
		ribs = xribs;
		rib_size = id + 1;
	}

	memset(&ribs[id], 0, sizeof(struct rib_desc));
	strlcpy(ribs[id].name, name, sizeof(ribs[id].name));
	RB_INIT(rib_tree(&ribs[id].rib));
	ribs[id].state = RECONF_REINIT;
	ribs[id].rib.id = id;
	ribs[id].rib.flags = flags;
	ribs[id].rib.rtableid = rtableid;

	ribs[id].in_rules = calloc(1, sizeof(struct filter_head));
	if (ribs[id].in_rules == NULL)
		fatal(NULL);
	TAILQ_INIT(ribs[id].in_rules);

	log_debug("%s: %s -> %u", __func__, name, id);
	return (&ribs[id].rib);
}

/*
 * This function is only called when the FIB information of a RIB changed.
 * It will flush the FIB if there was one previously and change the fibstate
 * from RECONF_NONE (nothing to do) to either RECONF_RELOAD (reload the FIB)
 * or RECONF_REINIT (rerun the route decision process for every element)
 * depending on the new flags.
 */
void
rib_update(struct rib *rib)
{
	struct rib_desc *rd = rib_desc(rib);

	/* flush fib first if there was one */
	if ((rib->flags & (F_RIB_NOFIB | F_RIB_NOEVALUATE)) == 0)
		rde_send_kroute_flush(rib);

	/* if no evaluate changes then a full reinit is needed */
	if ((rib->flags & F_RIB_NOEVALUATE) !=
	    (rib->flags_tmp & F_RIB_NOEVALUATE))
		rd->fibstate = RECONF_REINIT;

	rib->flags = rib->flags_tmp;
	rib->rtableid = rib->rtableid_tmp;

	/* reload fib if there is no reinit pending and there will be a fib */
	if (rd->fibstate != RECONF_REINIT &&
	    (rib->flags & (F_RIB_NOFIB | F_RIB_NOEVALUATE)) == 0)
		rd->fibstate = RECONF_RELOAD;
}

struct rib *
rib_byid(u_int16_t rid)
{
	if (rib_valid(rid))
		return &ribs[rid].rib;
	return NULL;
}

u_int16_t
rib_find(char *name)
{
	u_int16_t id;

	/* no name returns the first Loc-RIB */
	if (name == NULL || *name == '\0')
		return RIB_LOC_START;

	for (id = 0; id < rib_size; id++) {
		if (!strcmp(ribs[id].name, name))
			return id;
	}

	return RIB_NOTFOUND;
}

struct rib_desc *
rib_desc(struct rib *rib)
{
	return (&ribs[rib->id]);
}

void
rib_free(struct rib *rib)
{
	struct rib_desc *rd = rib_desc(rib);
	struct rib_entry *re, *xre;
	struct prefix *p;

	rib_dump_abort(rib->id);

	/*
	 * flush the rib, disable route evaluation and fib sync to speed up
	 * the prefix removal. Nothing depends on this data anymore.
	 */
	if ((rib->flags & (F_RIB_NOFIB | F_RIB_NOEVALUATE)) == 0)
		rde_send_kroute_flush(rib);
	rib->flags |= F_RIB_NOEVALUATE | F_RIB_NOFIB;

	for (re = RB_MIN(rib_tree, rib_tree(rib)); re != NULL; re = xre) {
		xre = RB_NEXT(rib_tree, rib_tree(rib), re);

		/*
		 * Removing the prefixes is tricky because the last one
		 * will remove the rib_entry as well and because we do
		 * an empty check in prefix_destroy() it is not possible to
		 * use the default for loop.
		 */
		while ((p = LIST_FIRST(&re->prefix_h))) {
			struct rde_aspath *asp = prefix_aspath(p);
			if (asp && asp->pftableid) {
				struct bgpd_addr addr;

				pt_getaddr(p->pt, &addr);
				/* Commit is done in peer_down() */
				rde_send_pftable(asp->pftableid, &addr,
				    p->pt->prefixlen, 1);
			}
			prefix_destroy(p);
		}
	}
	if (rib->id <= RIB_LOC_START)
		return; /* never remove the default ribs */
	filterlist_free(rd->in_rules_tmp);
	filterlist_free(rd->in_rules);
	memset(rd, 0, sizeof(struct rib_desc));
}

void
rib_shutdown(void)
{
	u_int16_t id;

	for (id = 0; id < rib_size; id++) {
		if (!rib_valid(id))
			continue;
		if (!RB_EMPTY(rib_tree(&ribs[id].rib))) {
			log_warnx("%s: rib %s is not empty", __func__,
			    ribs[id].name);
		}
		rib_free(&ribs[id].rib);
	}
	for (id = 0; id <= RIB_LOC_START; id++) {
		struct rib_desc *rd = &ribs[id];
		filterlist_free(rd->in_rules_tmp);
		filterlist_free(rd->in_rules);
		memset(rd, 0, sizeof(struct rib_desc));
	}
	free(ribs);
}

struct rib_entry *
rib_get(struct rib *rib, struct bgpd_addr *prefix, int prefixlen)
{
	struct rib_entry xre, *re;
	struct pt_entry	*pte;

	pte = pt_fill(prefix, prefixlen);
	memset(&xre, 0, sizeof(xre));
	xre.prefix = pte;

	re = RB_FIND(rib_tree, rib_tree(rib), &xre);
	if (re && re->rib_id != rib->id)
		fatalx("%s: Unexpected RIB %u != %u.", __func__,
		    re->rib_id, rib->id);
	return re;
}

struct rib_entry *
rib_match(struct rib *rib, struct bgpd_addr *addr)
{
	struct rib_entry *re;
	int		 i;

	switch (addr->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		for (i = 32; i >= 0; i--) {
			re = rib_get(rib, addr, i);
			if (re != NULL)
				return (re);
		}
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		for (i = 128; i >= 0; i--) {
			re = rib_get(rib, addr, i);
			if (re != NULL)
				return (re);
		}
		break;
	default:
		fatalx("%s: unknown af", __func__);
	}
	return (NULL);
}


struct rib_entry *
rib_add(struct rib *rib, struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_entry	*pte;
	struct rib_entry *re;

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)
		pte = pt_add(prefix, prefixlen);

	if ((re = calloc(1, sizeof(*re))) == NULL)
		fatal("rib_add");

	LIST_INIT(&re->prefix_h);
	re->prefix = pt_ref(pte);
	re->rib_id = rib->id;

	if (RB_INSERT(rib_tree, rib_tree(rib), re) != NULL) {
		log_warnx("rib_add: insert failed");
		free(re);
		return (NULL);
	}


	rdemem.rib_cnt++;

	return (re);
}

void
rib_remove(struct rib_entry *re)
{
	if (!rib_empty(re))
		fatalx("rib_remove: entry not empty");

	if (re_is_locked(re))
		/* entry is locked, don't free it. */
		return;

	pt_unref(re->prefix);

	if (RB_REMOVE(rib_tree, rib_tree(re_rib(re)), re) == NULL)
		log_warnx("rib_remove: remove failed.");

	free(re);
	rdemem.rib_cnt--;
}

int
rib_empty(struct rib_entry *re)
{
	return LIST_EMPTY(&re->prefix_h);
}

static struct rib_entry *
rib_restart(struct rib_context *ctx)
{
	struct rib_entry *re;

	re = re_unlock(ctx->ctx_re);

	/* find first non empty element */
	while (re && rib_empty(re))
		re = RB_NEXT(rib_tree, unused, re);

	/* free the previously locked rib element if empty */
	if (rib_empty(ctx->ctx_re))
		rib_remove(ctx->ctx_re);
	ctx->ctx_re = NULL;
	return (re);
}

static void
rib_dump_r(struct rib_context *ctx)
{
	struct rib_entry	*re, *next;
	struct rib		*rib;
	unsigned int		 i;

	rib = rib_byid(ctx->ctx_id);
	if (rib == NULL)
		fatalx("%s: rib id %u gone", __func__, ctx->ctx_id);

	if (ctx->ctx_re == NULL)
		re = RB_MIN(rib_tree, rib_tree(rib));
	else
		re = rib_restart(ctx);

	for (i = 0; re != NULL; re = next) {
		next = RB_NEXT(rib_tree, unused, re);
		if (re->rib_id != ctx->ctx_id)
			fatalx("%s: Unexpected RIB %u != %u.", __func__,
			    re->rib_id, ctx->ctx_id);
		if (ctx->ctx_aid != AID_UNSPEC &&
		    ctx->ctx_aid != re->prefix->aid)
			continue;
		if (ctx->ctx_count && i++ >= ctx->ctx_count &&
		    !re_is_locked(re)) {
			/* store and lock last element */
			ctx->ctx_re = re_lock(re);
			return;
		}
		ctx->ctx_rib_call(re, ctx->ctx_arg);
	}

	if (ctx->ctx_done)
		ctx->ctx_done(ctx->ctx_arg, ctx->ctx_aid);
	LIST_REMOVE(ctx, entry);
	free(ctx);
}

int
rib_dump_pending(void)
{
	struct rib_context *ctx;

	/* return true if at least one context is not throttled */
	LIST_FOREACH(ctx, &rib_dumps, entry) {
		if (ctx->ctx_throttle && ctx->ctx_throttle(ctx->ctx_arg))
			continue;
		return 1;
	}
	return 0;
}

void
rib_dump_runner(void)
{
	struct rib_context *ctx, *next;

	LIST_FOREACH_SAFE(ctx, &rib_dumps, entry, next) {
		if (ctx->ctx_throttle && ctx->ctx_throttle(ctx->ctx_arg))
			continue;
		if (ctx->ctx_rib_call != NULL)
			rib_dump_r(ctx);
		else
			prefix_dump_r(ctx);
	}
}

static void
rib_dump_abort(u_int16_t id)
{
	struct rib_context *ctx, *next;

	LIST_FOREACH_SAFE(ctx, &rib_dumps, entry, next) {
		if (id != ctx->ctx_id)
			continue;
		if (ctx->ctx_done)
			ctx->ctx_done(ctx->ctx_arg, ctx->ctx_aid);
		if (ctx->ctx_re && rib_empty(re_unlock(ctx->ctx_re)))
			rib_remove(ctx->ctx_re);
		if (ctx->ctx_p && prefix_is_dead(prefix_unlock(ctx->ctx_p)))
			prefix_adjout_destroy(ctx->ctx_p);
		LIST_REMOVE(ctx, entry);
		free(ctx);
	}
}

void
rib_dump_terminate(void *arg)
{
	struct rib_context *ctx, *next;

	LIST_FOREACH_SAFE(ctx, &rib_dumps, entry, next) {
		if (ctx->ctx_arg != arg)
			continue;
		if (ctx->ctx_done)
			ctx->ctx_done(ctx->ctx_arg, ctx->ctx_aid);
		if (ctx->ctx_re && rib_empty(re_unlock(ctx->ctx_re)))
			rib_remove(ctx->ctx_re);
		if (ctx->ctx_p && prefix_is_dead(prefix_unlock(ctx->ctx_p)))
			prefix_adjout_destroy(ctx->ctx_p);
		LIST_REMOVE(ctx, entry);
		free(ctx);
	}
}

int
rib_dump_new(u_int16_t id, u_int8_t aid, unsigned int count, void *arg,
    void (*upcall)(struct rib_entry *, void *), void (*done)(void *, u_int8_t),
    int (*throttle)(void *))
{
	struct rib_context *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return -1;
	ctx->ctx_id = id;
	ctx->ctx_aid = aid;
	ctx->ctx_count = count;
	ctx->ctx_arg = arg;
	ctx->ctx_rib_call = upcall;
	ctx->ctx_done = done;
	ctx->ctx_throttle = throttle;

	LIST_INSERT_HEAD(&rib_dumps, ctx, entry);

	/* requested a sync traversal */
	if (count == 0)
		rib_dump_r(ctx);

	return 0;
}

/* path specific functions */

static struct rde_aspath *path_lookup(struct rde_aspath *);
static u_int64_t path_hash(struct rde_aspath *);
static void path_link(struct rde_aspath *);
static void path_unlink(struct rde_aspath *);

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_int64_t		 path_hashmask;
} pathtable;

SIPHASH_KEY pathtablekey;

#define	PATH_HASH(x)	&pathtable.path_hashtbl[x & pathtable.path_hashmask]

static inline struct rde_aspath *
path_ref(struct rde_aspath *asp)
{
	if ((asp->flags & F_ATTR_LINKED) == 0)
		fatalx("%s: unlinked object", __func__);
	asp->refcnt++;
	rdemem.path_refs++;

	return asp;
}

static inline void
path_unref(struct rde_aspath *asp)
{
	if (asp == NULL)
		return;
	if ((asp->flags & F_ATTR_LINKED) == 0)
		fatalx("%s: unlinked object", __func__);
	asp->refcnt--;
	rdemem.path_refs--;
	if (asp->refcnt <= 0)
		path_unlink(asp);
}

void
path_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	pathtable.path_hashtbl = calloc(hs, sizeof(*pathtable.path_hashtbl));
	if (pathtable.path_hashtbl == NULL)
		fatal("path_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&pathtable.path_hashtbl[i]);

	pathtable.path_hashmask = hs - 1;
	arc4random_buf(&pathtablekey, sizeof(pathtablekey));
}

void
path_shutdown(void)
{
	u_int32_t	i;

	for (i = 0; i <= pathtable.path_hashmask; i++)
		if (!LIST_EMPTY(&pathtable.path_hashtbl[i]))
			log_warnx("path_free: free non-free table");

	free(pathtable.path_hashtbl);
}

void
path_hash_stats(struct rde_hashstats *hs)
{
	struct rde_aspath	*a;
	u_int32_t		i;
	int64_t			n;

	memset(hs, 0, sizeof(*hs));
	strlcpy(hs->name, "path hash", sizeof(hs->name));
	hs->min = LLONG_MAX;
	hs->num = pathtable.path_hashmask + 1;

	for (i = 0; i <= pathtable.path_hashmask; i++) {
		n = 0;
		LIST_FOREACH(a, &pathtable.path_hashtbl[i], path_l)
			n++;
		if (n < hs->min)
			hs->min = n;
		if (n > hs->max)
			hs->max = n;
		hs->sum += n;
		hs->sumq += n * n;
	}
}

/*
 * Update a prefix belonging to a possible new aspath.
 * Return 1 if prefix was newly added, 0 if it was just changed or 2 if no
 * change happened at all.
 */
int
path_update(struct rib *rib, struct rde_peer *peer, struct filterstate *state,
    struct bgpd_addr *prefix, int prefixlen, u_int8_t vstate)
{
	struct rde_aspath	*asp, *nasp = &state->aspath;
	struct rde_community	*comm, *ncomm = &state->communities;
	struct prefix		*p;

	if (nasp->pftableid) {
		rde_send_pftable(nasp->pftableid, prefix, prefixlen, 0);
		rde_send_pftable_commit();
	}

	/*
	 * First try to find a prefix in the specified RIB.
	 */
	if ((p = prefix_get(rib, peer, prefix, prefixlen)) != NULL) {
		if (prefix_nexthop(p) == state->nexthop &&
		    prefix_nhflags(p) == state->nhflags &&
		    communities_equal(ncomm, prefix_communities(p)) &&
		    path_compare(nasp, prefix_aspath(p)) == 0) {
			/* no change, update last change */
			p->lastchange = time(NULL);
			p->validation_state = vstate;
			return (2);
		}
	}

	/*
	 * Either the prefix does not exist or the path changed.
	 * In both cases lookup the new aspath to make sure it is not
	 * already in the RIB.
	 */
	if ((asp = path_lookup(nasp)) == NULL) {
		/* Path not available, create and link a new one. */
		asp = path_copy(path_get(), nasp);
		path_link(asp);
	}

	if ((comm = communities_lookup(ncomm)) == NULL) {
		/* Communities not available, create and link a new one. */
		comm = communities_link(ncomm);
	}

	/* If the prefix was found move it else add it to the aspath. */
	if (p != NULL)
		return (prefix_move(p, peer, asp, comm, state->nexthop,
		    state->nhflags, vstate));
	else
		return (prefix_add(prefix, prefixlen, rib, peer, asp, comm,
		    state->nexthop, state->nhflags, vstate));
}

int
path_compare(struct rde_aspath *a, struct rde_aspath *b)
{
	int		 r;

	if (a == NULL && b == NULL)
		return (0);
	else if (b == NULL)
		return (1);
	else if (a == NULL)
		return (-1);
	if ((a->flags & ~F_ATTR_LINKED) > (b->flags & ~F_ATTR_LINKED))
		return (1);
	if ((a->flags & ~F_ATTR_LINKED) < (b->flags & ~F_ATTR_LINKED))
		return (-1);
	if (a->origin > b->origin)
		return (1);
	if (a->origin < b->origin)
		return (-1);
	if (a->med > b->med)
		return (1);
	if (a->med < b->med)
		return (-1);
	if (a->lpref > b->lpref)
		return (1);
	if (a->lpref < b->lpref)
		return (-1);
	if (a->weight > b->weight)
		return (1);
	if (a->weight < b->weight)
		return (-1);
	if (a->rtlabelid > b->rtlabelid)
		return (1);
	if (a->rtlabelid < b->rtlabelid)
		return (-1);
	if (a->pftableid > b->pftableid)
		return (1);
	if (a->pftableid < b->pftableid)
		return (-1);

	r = aspath_compare(a->aspath, b->aspath);
	if (r > 0)
		return (1);
	if (r < 0)
		return (-1);

	return (attr_compare(a, b));
}

static u_int64_t
path_hash(struct rde_aspath *asp)
{
	SIPHASH_CTX	ctx;
	u_int64_t	hash;

	SipHash24_Init(&ctx, &pathtablekey);
	SipHash24_Update(&ctx, &asp->origin, sizeof(asp->origin));
	SipHash24_Update(&ctx, &asp->med, sizeof(asp->med));
	SipHash24_Update(&ctx, &asp->lpref, sizeof(asp->lpref));
	SipHash24_Update(&ctx, &asp->weight, sizeof(asp->weight));
	SipHash24_Update(&ctx, &asp->rtlabelid, sizeof(asp->rtlabelid));
	SipHash24_Update(&ctx, &asp->pftableid, sizeof(asp->pftableid));

	if (asp->aspath)
		SipHash24_Update(&ctx, asp->aspath->data, asp->aspath->len);

	hash = attr_hash(asp);
	SipHash24_Update(&ctx, &hash, sizeof(hash));

	return (SipHash24_End(&ctx));
}

static struct rde_aspath *
path_lookup(struct rde_aspath *aspath)
{
	struct aspath_head	*head;
	struct rde_aspath	*asp;
	u_int64_t		 hash;

	hash = path_hash(aspath);
	head = PATH_HASH(hash);

	LIST_FOREACH(asp, head, path_l) {
		if (asp->hash == hash && path_compare(aspath, asp) == 0)
			return (asp);
	}
	return (NULL);
}

/*
 * Link this aspath into the global hash table.
 * The asp had to be alloced with path_get.
 */
static void
path_link(struct rde_aspath *asp)
{
	struct aspath_head	*head;

	asp->hash = path_hash(asp);
	head = PATH_HASH(asp->hash);

	LIST_INSERT_HEAD(head, asp, path_l);
	asp->flags |= F_ATTR_LINKED;
}

/*
 * This function can only be called when all prefix have been removed first.
 * Normally this happens directly out of the prefix removal functions.
 */
static void
path_unlink(struct rde_aspath *asp)
{
	if (asp == NULL)
		return;

	/* make sure no reference is hold for this rde_aspath */
	if (asp->refcnt != 0)
		fatalx("%s: still holds references", __func__);

	LIST_REMOVE(asp, path_l);
	asp->flags &= ~F_ATTR_LINKED;

	path_put(asp);
}

/*
 * Copy asp to a new UNLINKED aspath.
 * On dst either path_get() or path_prep() had to be called before.
 */
struct rde_aspath *
path_copy(struct rde_aspath *dst, const struct rde_aspath *src)
{
	dst->aspath = src->aspath;
	if (dst->aspath != NULL) {
		dst->aspath->refcnt++;
		rdemem.aspath_refs++;
	}
	dst->hash = 0;
	dst->med = src->med;
	dst->lpref = src->lpref;
	dst->weight = src->weight;
	dst->origin = src->origin;
	dst->rtlabelid = rtlabel_ref(src->rtlabelid);
	dst->pftableid = pftable_ref(src->pftableid);

	dst->flags = src->flags & ~F_ATTR_LINKED;
	dst->refcnt = 0;	/* not linked so no refcnt */

	attr_copy(dst, src);

	return (dst);
}

/* initialize or pepare an aspath for use */
struct rde_aspath *
path_prep(struct rde_aspath *asp)
{
	memset(asp, 0, sizeof(*asp));
	asp->origin = ORIGIN_INCOMPLETE;
	asp->lpref = DEFAULT_LPREF;

	return (asp);
}

/* alloc and initialize new entry. May not fail. */
struct rde_aspath *
path_get(void)
{
	struct rde_aspath *asp;

	asp = malloc(sizeof(*asp));
	if (asp == NULL)
		fatal("path_get");
	rdemem.path_cnt++;

	return (path_prep(asp));
}

/* clean up an asp after use (frees all references to sub-objects) */
void
path_clean(struct rde_aspath *asp)
{
	if (asp->flags & F_ATTR_LINKED)
		fatalx("%s: linked object", __func__);

	rtlabel_unref(asp->rtlabelid);
	pftable_unref(asp->pftableid);
	aspath_put(asp->aspath);
	attr_freeall(asp);
}

/* free an unlinked element */
void
path_put(struct rde_aspath *asp)
{
	if (asp == NULL)
		return;

	path_clean(asp);

	rdemem.path_cnt--;
	free(asp);
}

/* prefix specific functions */

static void		 prefix_link(struct prefix *, struct rib_entry *,
			     struct rde_peer *, struct rde_aspath *,
			     struct rde_community *, struct nexthop *,
			     u_int8_t, u_int8_t);
static void		 prefix_unlink(struct prefix *);
static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);

/* RB tree comparison function */
static inline int
prefix_cmp(struct prefix *a, struct prefix *b)
{
	if (a->eor != b->eor)
		return a->eor - b->eor;
	/* if EOR marker no need to check the rest also a->eor == b->eor */
	if (a->eor)
		return 0;

	if (a->aspath != b->aspath)
		return (a->aspath > b->aspath ? 1 : -1);
	if (a->communities != b->communities)
		return (a->communities > b->communities ? 1 : -1);
	if (a->nexthop != b->nexthop)
		return (a->nexthop > b->nexthop ? 1 : -1);
	if (a->nhflags != b->nhflags)
		return (a->nhflags > b->nhflags ? 1 : -1);
	return pt_prefix_cmp(a->pt, b->pt);
}

static inline int
prefix_index_cmp(struct prefix *a, struct prefix *b)
{
	return pt_prefix_cmp(a->pt, b->pt);
}

RB_GENERATE(prefix_tree, prefix, entry.tree.update, prefix_cmp)
RB_GENERATE_STATIC(prefix_index, prefix, entry.tree.index, prefix_index_cmp)

/*
 * search for specified prefix of a peer. Returns NULL if not found.
 */
struct prefix *
prefix_get(struct rib *rib, struct rde_peer *peer, struct bgpd_addr *prefix,
    int prefixlen)
{
	struct rib_entry	*re;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)
		return (NULL);
	return (prefix_bypeer(re, peer));
}

/*
 * lookup prefix in the peer prefix_index. Returns NULL if not found.
 */
struct prefix *
prefix_lookup(struct rde_peer *peer, struct bgpd_addr *prefix,
    int prefixlen)
{
	struct prefix xp;
	struct pt_entry	*pte;

	memset(&xp, 0, sizeof(xp));
	pte = pt_fill(prefix, prefixlen);
	xp.pt = pte;

	return RB_FIND(prefix_index, &peer->adj_rib_out, &xp);
}

struct prefix *
prefix_match(struct rde_peer *peer, struct bgpd_addr *addr)
{
	struct prefix *p;
	int i;

	switch (addr->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		for (i = 32; i >= 0; i--) {
			p = prefix_lookup(peer, addr, i);
			if (p != NULL)
				return p;
		}
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		for (i = 128; i >= 0; i--) {
			p = prefix_lookup(peer, addr, i);
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
 * Adds or updates a prefix.
 */
static int
prefix_add(struct bgpd_addr *prefix, int prefixlen, struct rib *rib,
    struct rde_peer *peer, struct rde_aspath *asp, struct rde_community *comm,
    struct nexthop *nexthop, u_int8_t nhflags, u_int8_t vstate)
{
	struct prefix		*p;
	struct rib_entry	*re;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)
		re = rib_add(rib, prefix, prefixlen);

	p = prefix_alloc();
	prefix_link(p, re, peer, asp, comm, nexthop, nhflags, vstate);
	return (1);
}

/*
 * Move the prefix to the specified as path, removes the old asp if needed.
 */
static int
prefix_move(struct prefix *p, struct rde_peer *peer,
    struct rde_aspath *asp, struct rde_community *comm,
    struct nexthop *nexthop, u_int8_t nhflags, u_int8_t vstate)
{
	struct prefix		*np;

	if (peer != prefix_peer(p))
		fatalx("prefix_move: cross peer move");

	np = prefix_alloc();
	/* add reference to new AS path and communities */
	np->aspath = path_ref(asp);
	np->communities = communities_ref(comm);
	np->peer = peer;
	np->re = p->re;
	np->pt = p->pt; /* skip refcnt update since ref is moved */
	np->validation_state = vstate;
	np->nhflags = nhflags;
	np->nexthop = nexthop_ref(nexthop);
	nexthop_link(np);
	np->lastchange = time(NULL);

	/*
	 * no need to update the peer prefix count because we are only moving
	 * the prefix without changing the peer.
	 */

	/*
	 * First kick the old prefix node out of the prefix list,
	 * afterwards run the route decision for new prefix node.
	 * Because of this only one update is generated if the prefix
	 * was active.
	 * This is safe because we create a new prefix and so the change
	 * is noticed by prefix_evaluate().
	 */
	LIST_REMOVE(p, entry.list.rib);
	prefix_evaluate(np, np->re);

	/* remove old prefix node */
	/* as before peer count needs no update because of move */

	/* destroy all references to other objects and free the old prefix */
	nexthop_unlink(p);
	nexthop_unref(p->nexthop);
	communities_unref(p->communities);
	path_unref(p->aspath);
	p->communities = NULL;
	p->nexthop = NULL;
	p->aspath = NULL;
	p->peer = NULL;
	p->re = NULL;
	p->pt = NULL;
	prefix_free(p);

	return (0);
}

/*
 * Removes a prefix from all lists. If the parent objects -- path or
 * pt_entry -- become empty remove them too.
 */
int
prefix_remove(struct rib *rib, struct rde_peer *peer, struct bgpd_addr *prefix,
    int prefixlen)
{
	struct prefix		*p;
	struct rde_aspath	*asp;

	p = prefix_get(rib, peer, prefix, prefixlen);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return (0);

	asp = prefix_aspath(p);
	if (asp && asp->pftableid) {
		/* only prefixes in the local RIB were pushed into pf */
		rde_send_pftable(asp->pftableid, prefix, prefixlen, 1);
		rde_send_pftable_commit();
	}

	prefix_destroy(p);

	return (1);
}

/*
 * Insert an End-of-RIB marker into the update queue.
 */
void
prefix_add_eor(struct rde_peer *peer, u_int8_t aid)
{
	struct prefix *p;

	p = prefix_alloc();
	p->eor = 1;
	p->flags = PREFIX_FLAG_UPDATE;
	if (RB_INSERT(prefix_tree, &peer->updates[aid], p) != NULL)
		/* no need to add if EoR marker already present */
		prefix_free(p);
	/* EOR marker is not inserted into the adj_rib_out index */
}

/*
 * Put a prefix from the Adj-RIB-Out onto the update queue.
 */
int
prefix_update(struct rde_peer *peer, struct filterstate *state,
    struct bgpd_addr *prefix, int prefixlen, u_int8_t vstate)
{
	struct prefix_tree *prefix_head = NULL;
	struct rde_aspath *asp;
	struct rde_community *comm;
	struct prefix *p;
	int created = 0;

	if ((p = prefix_lookup(peer, prefix, prefixlen)) != NULL) {
		/* prefix is already in the Adj-RIB-Out */
		if (p->flags & PREFIX_FLAG_WITHDRAW) {
			created = 1;	/* consider this a new entry */
			peer->up_wcnt--;
			prefix_head = &peer->withdraws[prefix->aid];
			RB_REMOVE(prefix_tree, prefix_head, p);
		} else if (p->flags & PREFIX_FLAG_DEAD) {
			created = 1;	/* consider this a new entry */
		} else {
			if (prefix_nhflags(p) == state->nhflags &&
			    prefix_nexthop(p) == state->nexthop &&
			    communities_equal(&state->communities,
			    prefix_communities(p)) &&
			    path_compare(&state->aspath, prefix_aspath(p)) ==
			    0) {
				/* nothing changed */
				p->validation_state = vstate;
				p->lastchange = time(NULL);
				return 0;
			}

			if (p->flags & PREFIX_FLAG_UPDATE) {
				/* created = 0 so up_nlricnt is not increased */
				prefix_head = &peer->updates[prefix->aid];
				RB_REMOVE(prefix_tree, prefix_head, p);
			}
		}
		/* unlink from aspath and remove nexthop ref */
		nexthop_unref(p->nexthop);
		communities_unref(p->communities);
		path_unref(p->aspath);
		p->flags &= ~PREFIX_FLAG_MASK;

		/* peer and pt remain */
	} else {
		p = prefix_alloc();
		created = 1;

		p->pt = pt_get(prefix, prefixlen);
		if (p->pt == NULL)
			fatalx("%s: update for non existing prefix", __func__);
		pt_ref(p->pt);
		p->peer = peer;

		if (RB_INSERT(prefix_index, &peer->adj_rib_out, p) != NULL)
			fatalx("%s: RB index invariant violated", __func__);
	}

	if ((asp = path_lookup(&state->aspath)) == NULL) {
		/* Path not available, create and link a new one. */
		asp = path_copy(path_get(), &state->aspath);
		path_link(asp);
	}

	if ((comm = communities_lookup(&state->communities)) == NULL) {
		/* Communities not available, create and link a new one. */
		comm = communities_link(&state->communities);
	}

	p->aspath = path_ref(asp);
	p->communities = communities_ref(comm);
	p->nexthop = nexthop_ref(state->nexthop);
	p->nhflags = state->nhflags;

	p->validation_state = vstate;
	p->lastchange = time(NULL);

	if (p->flags & PREFIX_FLAG_MASK)
		fatalx("%s: bad flags %x", __func__, p->flags);
	p->flags |= PREFIX_FLAG_UPDATE;
	if (RB_INSERT(prefix_tree, &peer->updates[prefix->aid], p) != NULL)
		fatalx("%s: RB tree invariant violated", __func__);

	return created;
}

/*
 * Withdraw a prefix from the Adj-RIB-Out, this unlinks the aspath but leaves
 * the prefix in the RIB linked to the peer withdraw list.
 */
int
prefix_withdraw(struct rde_peer *peer, struct bgpd_addr *prefix, int prefixlen)
{
	struct prefix		*p;

	p = prefix_lookup(peer, prefix, prefixlen);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return (0);

	/* remove nexthop ref ... */
	nexthop_unref(p->nexthop);
	p->nexthop = NULL;
	p->nhflags = 0;

	/* unlink from aspath ...*/
	path_unref(p->aspath);
	p->aspath = NULL;

	/* ... communities ... */
	communities_unref(p->communities);
	p->communities = NULL;
	/* and unlink from aspath */
	path_unref(p->aspath);
	p->aspath = NULL;
	/* re already NULL */

	p->lastchange = time(NULL);

	if (p->flags & PREFIX_FLAG_MASK) {
		struct prefix_tree *prefix_head;
		/* p is a pending update or withdraw, remove first */
		prefix_head = p->flags & PREFIX_FLAG_UPDATE ?
		    &peer->updates[prefix->aid] :
		    &peer->withdraws[prefix->aid];
		RB_REMOVE(prefix_tree, prefix_head, p);
		p->flags &= ~PREFIX_FLAG_MASK;
	}
	p->flags |= PREFIX_FLAG_WITHDRAW;
	if (RB_INSERT(prefix_tree, &peer->withdraws[prefix->aid], p) != NULL)
		fatalx("%s: RB tree invariant violated", __func__);
	return (1);
}

static struct prefix *
prefix_restart(struct rib_context *ctx)
{
	struct prefix *p;

	p = prefix_unlock(ctx->ctx_p);

	if (prefix_is_dead(p)) {
		struct prefix *next;

		next = RB_NEXT(prefix_index, unused, p);
		prefix_adjout_destroy(p);
		p = next;
	}
	ctx->ctx_p = NULL;
	return p;
}

void
prefix_adjout_destroy(struct prefix *p)
{
	struct rde_peer *peer = prefix_peer(p);

	if (p->eor) {
		/* EOR marker is not linked in the index */
		prefix_free(p);
		return;
	}

	if (p->flags & PREFIX_FLAG_WITHDRAW)
		RB_REMOVE(prefix_tree, &peer->withdraws[p->pt->aid], p);
	else if (p->flags & PREFIX_FLAG_UPDATE)
		RB_REMOVE(prefix_tree, &peer->updates[p->pt->aid], p);
	/* nothing needs to be done for PREFIX_FLAG_DEAD */
	p->flags &= ~PREFIX_FLAG_MASK;


	if (prefix_is_locked(p)) {
		/* remove nexthop ref ... */
		nexthop_unref(p->nexthop);
		p->nexthop = NULL;
		/* ... communities ... */
		communities_unref(p->communities);
		p->communities = NULL;
		/* and unlink from aspath */
		path_unref(p->aspath);
		p->aspath = NULL;
		p->nhflags = 0;
		/* re already NULL */

		/* finally mark prefix dead */
		p->flags |= PREFIX_FLAG_DEAD;
		return;
	}

	RB_REMOVE(prefix_index, &peer->adj_rib_out, p);
	
	prefix_unlink(p);
	prefix_free(p);
}

static void
prefix_dump_r(struct rib_context *ctx)
{
	struct prefix *p, *next;
	struct rde_peer *peer;
	unsigned int i;

	if ((peer = peer_get(ctx->ctx_id)) == NULL)
		goto done;

	if (ctx->ctx_p == NULL)
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
prefix_dump_new(struct rde_peer *peer, u_int8_t aid, unsigned int count,
    void *arg, void (*upcall)(struct prefix *, void *),
    void (*done)(void *, u_int8_t), int (*throttle)(void *))
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

	LIST_INSERT_HEAD(&rib_dumps, ctx, entry);

	/* requested a sync traversal */
	if (count == 0)
		prefix_dump_r(ctx);

	return 0;
}

/* dump a prefix into specified buffer */
int
prefix_write(u_char *buf, int len, struct bgpd_addr *prefix, u_int8_t plen,
    int withdraw)
{
	int	totlen, psize;

	switch (prefix->aid) {
	case AID_INET:
	case AID_INET6:
		totlen = PREFIX_SIZE(plen);

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		memcpy(buf, &prefix->ba, totlen - 1);
		return (totlen);
	case AID_VPN_IPv4:
		totlen = PREFIX_SIZE(plen) + sizeof(prefix->vpn4.rd);
		psize = PREFIX_SIZE(plen) - 1;
		plen += sizeof(prefix->vpn4.rd) * 8;
		if (withdraw) {
			/* withdraw have one compat label as placeholder */
			totlen += 3;
			plen += 3 * 8;
		} else {
			totlen += prefix->vpn4.labellen;
			plen += prefix->vpn4.labellen * 8;
		}

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		if (withdraw) {
			/* magic compatibility label as per rfc8277 */
			*buf++ = 0x80;
			*buf++ = 0x0;
			*buf++ = 0x0;
		} else  {
			memcpy(buf, &prefix->vpn4.labelstack,
			    prefix->vpn4.labellen);
			buf += prefix->vpn4.labellen;
		}
		memcpy(buf, &prefix->vpn4.rd, sizeof(prefix->vpn4.rd));
		buf += sizeof(prefix->vpn4.rd);
		memcpy(buf, &prefix->vpn4.addr, psize);
		return (totlen);
	case AID_VPN_IPv6:
		totlen = PREFIX_SIZE(plen) + sizeof(prefix->vpn6.rd);
		psize = PREFIX_SIZE(plen) - 1;
		plen += sizeof(prefix->vpn6.rd) * 8;
		if (withdraw) {
			/* withdraw have one compat label as placeholder */
			totlen += 3;
			plen += 3 * 8;
		} else {
			totlen += prefix->vpn6.labellen;
			plen += prefix->vpn6.labellen * 8;
		}
		if (totlen > len)
			return (-1);
		*buf++ = plen;
		if (withdraw) {
			/* magic compatibility label as per rfc8277 */
			*buf++ = 0x80;
			*buf++ = 0x0;
			*buf++ = 0x0;
		} else  {
			memcpy(buf, &prefix->vpn6.labelstack,
			    prefix->vpn6.labellen);
			buf += prefix->vpn6.labellen;
		}
		memcpy(buf, &prefix->vpn6.rd, sizeof(prefix->vpn6.rd));
		buf += sizeof(prefix->vpn6.rd);
		memcpy(buf, &prefix->vpn6.addr, psize);
		return (totlen);
	default:
		return (-1);
	}
}

int
prefix_writebuf(struct ibuf *buf, struct bgpd_addr *prefix, u_int8_t plen)
{
	int	 totlen;
	void	*bptr;

	switch (prefix->aid) {
	case AID_INET:
	case AID_INET6:
		totlen = PREFIX_SIZE(plen);
		break;
	case AID_VPN_IPv4:
		totlen = PREFIX_SIZE(plen) + sizeof(prefix->vpn4.rd) +
		    prefix->vpn4.labellen;
		break;
	case AID_VPN_IPv6:
		totlen = PREFIX_SIZE(plen) + sizeof(prefix->vpn6.rd) +
		    prefix->vpn6.labellen;
		break;
	default:
		return (-1);
	}

	if ((bptr = ibuf_reserve(buf, totlen)) == NULL)
		return (-1);
	if (prefix_write(bptr, totlen, prefix, plen, 0) == -1)
		return (-1);
	return (0);
}

/*
 * Searches in the prefix list of specified rib_entry for a prefix entry
 * belonging to the peer peer. Returns NULL if no match found.
 */
struct prefix *
prefix_bypeer(struct rib_entry *re, struct rde_peer *peer)
{
	struct prefix	*p;

	LIST_FOREACH(p, &re->prefix_h, entry.list.rib)
		if (prefix_peer(p) == peer)
			return (p);
	return (NULL);
}

static void
prefix_updateall(struct prefix *p, enum nexthop_state state,
    enum nexthop_state oldstate)
{
	/* Skip non local-RIBs or RIBs that are flagged as noeval. */
	if (re_rib(p->re)->flags & F_RIB_NOEVALUATE) {
		log_warnx("%s: prefix with F_RIB_NOEVALUATE hit", __func__);
		return;
	}

	if (oldstate == state) {
		/*
		 * The state of the nexthop did not change. The only
		 * thing that may have changed is the true_nexthop
		 * or other internal infos. This will not change
		 * the routing decision so shortcut here.
		 */
		if (state == NEXTHOP_REACH) {
			if ((re_rib(p->re)->flags & F_RIB_NOFIB) == 0 &&
			    p == p->re->active)
				rde_send_kroute(re_rib(p->re), p, NULL);
		}
		return;
	}

	/* redo the route decision */
	LIST_REMOVE(p, entry.list.rib);
	/*
	 * If the prefix is the active one remove it first,
	 * this has to be done because we can not detect when
	 * the active prefix changes its state. In this case
	 * we know that this is a withdrawal and so the second
	 * prefix_evaluate() will generate no update because
	 * the nexthop is unreachable or ineligible.
	 */
	if (p == p->re->active)
		prefix_evaluate(NULL, p->re);
	prefix_evaluate(p, p->re);
}

/* kill a prefix. */
void
prefix_destroy(struct prefix *p)
{
	/* make route decision */
	LIST_REMOVE(p, entry.list.rib);
	prefix_evaluate(NULL, p->re);

	prefix_unlink(p);
	prefix_free(p);
}

/*
 * Link a prefix into the different parent objects.
 */
static void
prefix_link(struct prefix *p, struct rib_entry *re, struct rde_peer *peer,
    struct rde_aspath *asp, struct rde_community *comm,
    struct nexthop *nexthop, u_int8_t nhflags, u_int8_t vstate)
{
	p->aspath = path_ref(asp);
	p->communities = communities_ref(comm);
	p->peer = peer;
	p->pt = pt_ref(re->prefix);
	p->re = re;
	p->validation_state = vstate;
	p->nhflags = nhflags;
	p->nexthop = nexthop_ref(nexthop);
	nexthop_link(p);
	p->lastchange = time(NULL);

	/* make route decision */
	prefix_evaluate(p, re);
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
prefix_unlink(struct prefix *p)
{
	struct rib_entry	*re = p->re;

	/* destroy all references to other objects */
	nexthop_unlink(p);
	nexthop_unref(p->nexthop);
	communities_unref(p->communities);
	path_unref(p->aspath);
	pt_unref(p->pt);
	p->communities = NULL;
	p->nexthop = NULL;
	p->aspath = NULL;
	p->peer = NULL;
	p->re = NULL;
	p->pt = NULL;

	if (re && rib_empty(re))
		rib_remove(re);

	/*
	 * It's the caller's duty to do accounting and remove empty aspath
	 * structures. Also freeing the unlinked prefix is the caller's duty.
	 */
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

/*
 * nexthop functions
 */
struct nexthop_head	*nexthop_hash(struct bgpd_addr *);
struct nexthop		*nexthop_lookup(struct bgpd_addr *);

/*
 * In BGP there exist two nexthops: the exit nexthop which was announced via
 * BGP and the true nexthop which is used in the FIB -- forward information
 * base a.k.a kernel routing table. When sending updates it is even more
 * confusing. In IBGP we pass the unmodified exit nexthop to the neighbors
 * while in EBGP normally the address of the router is sent. The exit nexthop
 * may be passed to the external neighbor if the neighbor and the exit nexthop
 * reside in the same subnet -- directly connected.
 */
struct nexthop_table {
	LIST_HEAD(nexthop_head, nexthop)	*nexthop_hashtbl;
	u_int32_t				 nexthop_hashmask;
} nexthoptable;

SIPHASH_KEY nexthoptablekey;

TAILQ_HEAD(nexthop_queue, nexthop)	nexthop_runners;

void
nexthop_init(u_int32_t hashsize)
{
	u_int32_t	 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct nexthop_head));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init");

	TAILQ_INIT(&nexthop_runners);
	for (i = 0; i < hs; i++)
		LIST_INIT(&nexthoptable.nexthop_hashtbl[i]);
	arc4random_buf(&nexthoptablekey, sizeof(nexthoptablekey));

	nexthoptable.nexthop_hashmask = hs - 1;
}

void
nexthop_shutdown(void)
{
	u_int32_t		 i;
	struct nexthop		*nh, *nnh;

	for (i = 0; i <= nexthoptable.nexthop_hashmask; i++) {
		for (nh = LIST_FIRST(&nexthoptable.nexthop_hashtbl[i]);
		    nh != NULL; nh = nnh) {
			nnh = LIST_NEXT(nh, nexthop_l);
			nh->state = NEXTHOP_UNREACH;
			nexthop_unref(nh);
		}
		if (!LIST_EMPTY(&nexthoptable.nexthop_hashtbl[i])) {
			nh = LIST_FIRST(&nexthoptable.nexthop_hashtbl[i]);
			log_warnx("nexthop_shutdown: non-free table, "
			    "nexthop %s refcnt %d",
			    log_addr(&nh->exit_nexthop), nh->refcnt);
		}
	}

	free(nexthoptable.nexthop_hashtbl);
}

int
nexthop_pending(void)
{
	return !TAILQ_EMPTY(&nexthop_runners);
}

void
nexthop_runner(void)
{
	struct nexthop *nh;
	struct prefix *p;
	u_int32_t j;

	nh = TAILQ_FIRST(&nexthop_runners);
	if (nh == NULL)
		return;

	/* remove from runnner queue */
	TAILQ_REMOVE(&nexthop_runners, nh, runner_l);

	p = nh->next_prefix;
	for (j = 0; p != NULL && j < RDE_RUNNER_ROUNDS; j++) {
		prefix_updateall(p, nh->state, nh->oldstate);
		p = LIST_NEXT(p, entry.list.nexthop);
	}

	/* prep for next run, if not finished readd to tail of queue */
	nh->next_prefix = p;
	if (p != NULL)
		TAILQ_INSERT_TAIL(&nexthop_runners, nh, runner_l);
	else
		log_debug("nexthop %s update finished",
		    log_addr(&nh->exit_nexthop));
}

void
nexthop_update(struct kroute_nexthop *msg)
{
	struct nexthop		*nh;

	nh = nexthop_lookup(&msg->nexthop);
	if (nh == NULL) {
		log_warnx("nexthop_update: non-existent nexthop %s",
		    log_addr(&msg->nexthop));
		return;
	}

	nh->oldstate = nh->state;
	if (msg->valid)
		nh->state = NEXTHOP_REACH;
	else
		nh->state = NEXTHOP_UNREACH;

	if (nh->oldstate == NEXTHOP_LOOKUP)
		/* drop reference which was hold during the lookup */
		if (nexthop_unref(nh))
			return;		/* nh lost last ref, no work left */

	if (nh->next_prefix)
		/*
		 * If nexthop_runner() is not finished with this nexthop
		 * then ensure that all prefixes are updated by setting
		 * the oldstate to NEXTHOP_FLAPPED.
		 */
		nh->oldstate = NEXTHOP_FLAPPED;

	if (msg->connected) {
		nh->flags |= NEXTHOP_CONNECTED;
		memcpy(&nh->true_nexthop, &nh->exit_nexthop,
		    sizeof(nh->true_nexthop));
	} else
		memcpy(&nh->true_nexthop, &msg->gateway,
		    sizeof(nh->true_nexthop));

	memcpy(&nh->nexthop_net, &msg->net,
	    sizeof(nh->nexthop_net));
	nh->nexthop_netlen = msg->netlen;

	nh->next_prefix = LIST_FIRST(&nh->prefix_h);
	TAILQ_INSERT_HEAD(&nexthop_runners, nh, runner_l);
	log_debug("nexthop %s update starting", log_addr(&nh->exit_nexthop));
}

void
nexthop_modify(struct nexthop *setnh, enum action_types type, u_int8_t aid,
    struct nexthop **nexthop, u_int8_t *flags)
{
	switch (type) {
	case ACTION_SET_NEXTHOP_REJECT:
		*flags = NEXTHOP_REJECT;
		break;
	case ACTION_SET_NEXTHOP_BLACKHOLE:
		*flags = NEXTHOP_BLACKHOLE;
		break;
	case ACTION_SET_NEXTHOP_NOMODIFY:
		*flags = NEXTHOP_NOMODIFY;
		break;
	case ACTION_SET_NEXTHOP_SELF:
		*flags = NEXTHOP_SELF;
		break;
	case ACTION_SET_NEXTHOP:
		/*
		 * it is possible that a prefix matches but has the wrong
		 * address family for the set nexthop. In this case ignore it.
		 */
		if (aid != setnh->exit_nexthop.aid)
			break;
		nexthop_unref(*nexthop);
		*nexthop = nexthop_ref(setnh);
		*flags = 0;
		break;
	default:
		break;
	}
}

void
nexthop_link(struct prefix *p)
{
	if (p->nexthop == NULL)
		return;

	/* no need to link prefixes in RIBs that have no decision process */
	if (re_rib(p->re)->flags & F_RIB_NOEVALUATE)
		return;

	p->flags |= PREFIX_NEXTHOP_LINKED;
	LIST_INSERT_HEAD(&p->nexthop->prefix_h, p, entry.list.nexthop);
}

void
nexthop_unlink(struct prefix *p)
{
	if (p->nexthop == NULL || (p->flags & PREFIX_NEXTHOP_LINKED) == 0)
		return;

	if (p == p->nexthop->next_prefix)
		p->nexthop->next_prefix = LIST_NEXT(p, entry.list.nexthop);

	p->flags &= ~PREFIX_NEXTHOP_LINKED;
	LIST_REMOVE(p, entry.list.nexthop);
}

struct nexthop *
nexthop_get(struct bgpd_addr *nexthop)
{
	struct nexthop	*nh;

	nh = nexthop_lookup(nexthop);
	if (nh == NULL) {
		nh = calloc(1, sizeof(*nh));
		if (nh == NULL)
			fatal("nexthop_alloc");
		rdemem.nexthop_cnt++;

		LIST_INIT(&nh->prefix_h);
		nh->state = NEXTHOP_LOOKUP;
		nexthop_ref(nh);	/* take reference for lookup */
		nh->exit_nexthop = *nexthop;
		LIST_INSERT_HEAD(nexthop_hash(nexthop), nh,
		    nexthop_l);

		rde_send_nexthop(&nh->exit_nexthop, 1);
	}

	return nexthop_ref(nh);
}

struct nexthop *
nexthop_ref(struct nexthop *nexthop)
{
	if (nexthop)
		nexthop->refcnt++;
	return (nexthop);
}

int
nexthop_unref(struct nexthop *nh)
{
	if (nh == NULL)
		return (0);
	if (--nh->refcnt > 0)
		return (0);

	/* sanity check */
	if (!LIST_EMPTY(&nh->prefix_h) || nh->state == NEXTHOP_LOOKUP)
		fatalx("%s: refcnt error", __func__);

	/* is nexthop update running? impossible, that is a refcnt error */
	if (nh->next_prefix)
		fatalx("%s: next_prefix not NULL", __func__);

	LIST_REMOVE(nh, nexthop_l);
	rde_send_nexthop(&nh->exit_nexthop, 0);

	rdemem.nexthop_cnt--;
	free(nh);
	return (1);
}

int
nexthop_compare(struct nexthop *na, struct nexthop *nb)
{
	struct bgpd_addr	*a, *b;

	if (na == nb)
		return (0);
	if (na == NULL)
		return (-1);
	if (nb == NULL)
		return (1);

	a = &na->exit_nexthop;
	b = &nb->exit_nexthop;

	if (a->aid != b->aid)
		return (a->aid - b->aid);

	switch (a->aid) {
	case AID_INET:
		if (ntohl(a->v4.s_addr) > ntohl(b->v4.s_addr))
			return (1);
		if (ntohl(a->v4.s_addr) < ntohl(b->v4.s_addr))
			return (-1);
		return (0);
	case AID_INET6:
		return (memcmp(&a->v6, &b->v6, sizeof(struct in6_addr)));
	default:
		fatalx("nexthop_cmp: unknown af");
	}
	return (-1);
}

struct nexthop *
nexthop_lookup(struct bgpd_addr *nexthop)
{
	struct nexthop	*nh;

	LIST_FOREACH(nh, nexthop_hash(nexthop), nexthop_l) {
		if (memcmp(&nh->exit_nexthop, nexthop,
		    sizeof(struct bgpd_addr)) == 0)
			return (nh);
	}
	return (NULL);
}

struct nexthop_head *
nexthop_hash(struct bgpd_addr *nexthop)
{
	u_int32_t	 h = 0;

	switch (nexthop->aid) {
	case AID_INET:
		h = SipHash24(&nexthoptablekey, &nexthop->v4.s_addr,
		    sizeof(nexthop->v4.s_addr));
		break;
	case AID_INET6:
		h = SipHash24(&nexthoptablekey, &nexthop->v6,
		    sizeof(struct in6_addr));
		break;
	default:
		fatalx("nexthop_hash: unsupported AF");
	}
	return (&nexthoptable.nexthop_hashtbl[h & nexthoptable.nexthop_hashmask]);
}
