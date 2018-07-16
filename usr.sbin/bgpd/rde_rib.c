/*	$OpenBSD: rde_rib.c,v 1.171 2018/07/16 09:09:20 claudio Exp $ */

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
int rib_compare(const struct rib_entry *, const struct rib_entry *);
void rib_remove(struct rib_entry *);
int rib_empty(struct rib_entry *);
struct rib_entry *rib_restart(struct rib_context *);

RB_PROTOTYPE(rib_tree, rib_entry, rib_e, rib_compare);
RB_GENERATE(rib_tree, rib_entry, rib_e, rib_compare);

int		 prefix_add(struct rib *, struct rde_aspath *,
		    struct bgpd_addr *, int, int);
void		 prefix_move(struct rde_aspath *, struct prefix *, int);

static inline void
re_lock(struct rib_entry *re)
{
	re->__rib = (struct rib *)((intptr_t)re->__rib | 1);
}

static inline void
re_unlock(struct rib_entry *re)
{
	re->__rib = (struct rib *)((intptr_t)re->__rib & ~1);
}

static inline int
re_is_locked(struct rib_entry *re)
{
	return ((intptr_t)re->__rib & 1);
}

static inline struct rib_tree *
rib_tree(struct rib *rib)
{
	return (&rib->tree);
}

/* RIB specific functions */
struct rib *
rib_new(char *name, u_int rtableid, u_int16_t flags)
{
	struct rib_desc	*xribs;
	u_int16_t	id;

	for (id = 0; id < rib_size; id++) {
		if (*ribs[id].name == '\0')
			break;
	}

	if (id >= rib_size) {
		if ((xribs = reallocarray(ribs, id + 1,
		    sizeof(struct rib_desc))) == NULL) {
			/* XXX this is not clever */
			fatal("rib_add");
		}
		ribs = xribs;
		rib_size = id + 1;
	}

	bzero(&ribs[id], sizeof(struct rib_desc));
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

	return (&ribs[id].rib);
}

struct rib *
rib_find(char *name)
{
	u_int16_t id;

	/* no name returns the first Loc-RIB */
	if (name == NULL || *name == '\0')
		return (&ribs[RIB_LOC_START].rib);

	for (id = 0; id < rib_size; id++) {
		if (!strcmp(ribs[id].name, name))
			return (&ribs[id].rib);
	}

	return (NULL);
}

struct rib_desc *
rib_desc(struct rib *rib)
{
	return (&ribs[rib->id]);
}

void
rib_free(struct rib *rib)
{
	struct rib_desc *rd;
	struct rib_entry *re, *xre;
	struct prefix *p, *np;

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
			np = LIST_NEXT(p, rib_l);
			if (asp->pftableid) {
				struct bgpd_addr addr;

				pt_getaddr(p->re->prefix, &addr);
				/* Commit is done in peer_down() */
				rde_send_pftable(asp->pftableid, &addr,
				    p->re->prefix->prefixlen, 1);
			}
			prefix_destroy(p);
			if (np == NULL)
				break;
		}
	}
	rd = &ribs[rib->id];
	filterlist_free(rd->in_rules_tmp);
	filterlist_free(rd->in_rules);
	bzero(rd, sizeof(struct rib_desc));
}

int
rib_compare(const struct rib_entry *a, const struct rib_entry *b)
{
	return (pt_prefix_cmp(a->prefix, b->prefix));
}

struct rib_entry *
rib_get(struct rib *rib, struct bgpd_addr *prefix, int prefixlen)
{
	struct rib_entry xre;
	struct pt_entry	*pte;

	pte = pt_fill(prefix, prefixlen);
	bzero(&xre, sizeof(xre));
	xre.prefix = pte;

	return (RB_FIND(rib_tree, rib_tree(rib), &xre));
}

struct rib_entry *
rib_lookup(struct rib *rib, struct bgpd_addr *addr)
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
		for (i = 128; i >= 0; i--) {
			re = rib_get(rib, addr, i);
			if (re != NULL)
				return (re);
		}
		break;
	default:
		fatalx("rib_lookup: unknown af");
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
	re->prefix = pte;
	re->__rib = rib;

        if (RB_INSERT(rib_tree, rib_tree(rib), re) != NULL) {
		log_warnx("rib_add: insert failed");
		free(re);
		return (NULL);
	}

	pt_ref(pte);

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
	if (pt_empty(re->prefix))
		pt_remove(re->prefix);

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

void
rib_dump(struct rib *rib, void (*upcall)(struct rib_entry *, void *),
    void *arg, u_int8_t aid)
{
	struct rib_context	*ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		fatal("rib_dump");
	ctx->ctx_rib = rib;
	ctx->ctx_upcall = upcall;
	ctx->ctx_arg = arg;
	ctx->ctx_aid = aid;
	rib_dump_r(ctx);
}

void
rib_dump_r(struct rib_context *ctx)
{
	struct rib_entry	*re;
	unsigned int		 i;

	if (ctx->ctx_re == NULL)
		re = RB_MIN(rib_tree, rib_tree(ctx->ctx_rib));
	else
		re = rib_restart(ctx);

	for (i = 0; re != NULL; re = RB_NEXT(rib_tree, unused, re)) {
		if (ctx->ctx_aid != AID_UNSPEC &&
		    ctx->ctx_aid != re->prefix->aid)
			continue;
		if (ctx->ctx_count && i++ >= ctx->ctx_count &&
		    !re_is_locked(re)) {
			/* store and lock last element */
			ctx->ctx_re = re;
			re_lock(re);
			return;
		}
		ctx->ctx_upcall(re, ctx->ctx_arg);
	}

	if (ctx->ctx_done)
		ctx->ctx_done(ctx->ctx_arg);
	else
		free(ctx);
}

struct rib_entry *
rib_restart(struct rib_context *ctx)
{
	struct rib_entry *re;

	re = ctx->ctx_re;
	re_unlock(re);

	/* find first non empty element */
	while (re && rib_empty(re))
		re = RB_NEXT(rib_tree, unused, re);

	/* free the previously locked rib element if empty */
	if (rib_empty(ctx->ctx_re))
		rib_remove(ctx->ctx_re);
	ctx->ctx_re = NULL;
	return (re);
}

/* path specific functions */

static struct rde_aspath *path_lookup(struct rde_aspath *, struct rde_peer *);
static u_int64_t path_hash(struct rde_aspath *);
static void path_link(struct rde_aspath *, struct rde_peer *);

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_int64_t		 path_hashmask;
} pathtable;

SIPHASH_KEY pathtablekey;

#define PATH_HASH(x)	&pathtable.path_hashtbl[x & pathtable.path_hashmask]

void
path_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	pathtable.path_hashtbl = calloc(hs, sizeof(struct aspath_queue));
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

int
path_update(struct rib *rib, struct rde_peer *peer, struct filterstate *state,
    struct bgpd_addr *prefix, int prefixlen, int flag)
{
	struct rde_aspath	*asp, *nasp = &state->aspath;
	struct prefix		*p;
	int			 pflag = 0;

	if (nasp->pftableid) {
		rde_send_pftable(nasp->pftableid, prefix, prefixlen, 0);
		rde_send_pftable_commit();
	}

	/*
	 * First try to find a prefix in the specified RIB.
	 */
	if ((p = prefix_get(rib, peer, prefix, prefixlen, 0)) != NULL) {
		if (path_compare(nasp, prefix_aspath(p)) == 0) {
			/* no change, update last change */
			p->lastchange = time(NULL);
			return (0);
		}
	}

	/*
	 * Either the prefix does not exist or the path changed.
	 * In both cases lookup the new aspath to make sure it is not
	 * already in the RIB.
	 */
	if ((asp = path_lookup(nasp, peer)) == NULL) {
		/* Path not available, create and link a new one. */
		asp = path_copy(path_get(), nasp);
		path_link(asp, peer);
	}

	/* If the prefix was found move it else add it to the aspath. */
	if (p != NULL)
		prefix_move(asp, p, pflag);
	else
		return (prefix_add(rib, asp, prefix, prefixlen, pflag));
	return (0);
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
	if ((a->flags & ~(F_ATTR_LINKED | F_ATTR_UPDATE)) >
	    (b->flags & ~(F_ATTR_LINKED | F_ATTR_UPDATE)))
		return (1);
	if ((a->flags & ~(F_ATTR_LINKED | F_ATTR_UPDATE)) <
	    (b->flags & ~(F_ATTR_LINKED | F_ATTR_UPDATE)))
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
	if (r == 0)
		r = nexthop_compare(a->nexthop, b->nexthop);
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
path_lookup(struct rde_aspath *aspath, struct rde_peer *peer)
{
	struct aspath_head	*head;
	struct rde_aspath	*asp;
	u_int64_t		 hash;

	hash = path_hash(aspath);
	head = PATH_HASH(hash);

	LIST_FOREACH(asp, head, path_l) {
		if (asp->hash == hash && peer == asp->peer &&
		    path_compare(aspath, asp) == 0)
			return (asp);
	}
	return (NULL);
}

void
path_remove(struct rde_aspath *asp)
{
	struct prefix	*p, *np;
	int		 has_updates;

	/*
	 * Must check if we actually have updates before removing prefixes
	 * because if this is the case than the last prefix on prefixes will
	 * free the asp and so the access to updates is a use after free.
	 */
	has_updates = !TAILQ_EMPTY(&asp->updates);

	for (p = TAILQ_FIRST(&asp->prefixes); p != NULL; p = np) {
		np = TAILQ_NEXT(p, path_l);
		if (asp->pftableid) {
			struct bgpd_addr addr;

			pt_getaddr(p->re->prefix, &addr);
			/* Commit is done in peer_down() */
			rde_send_pftable(prefix_aspath(p)->pftableid, &addr,
			    p->re->prefix->prefixlen, 1);
		}
		prefix_destroy(p);
	}
	if (has_updates)
		for (p = TAILQ_FIRST(&asp->updates); p != NULL; p = np) {
			np = TAILQ_NEXT(p, path_l);
			/* no need to worry about pftable on Adj-RIB-Out */
			prefix_destroy(p);
		}
}

/* remove all stale routes or if staletime is 0 remove all routes for
   a specified AID. */
u_int32_t
path_remove_stale(struct rde_aspath *asp, u_int8_t aid)
{
	struct prefix	*p, *np;
	time_t		 staletime;
	u_int32_t	 rprefixes;
	int		 has_updates;

	rprefixes=0;
	staletime = asp->peer->staletime[aid];
	/*
	 * Same magic as in path_remove() but probably not needed here.
	 * This is called when a session flapped and during that time
	 * the pending updates for that peer are getting reset.
	 */
	has_updates = !TAILQ_EMPTY(&asp->updates);
	for (p = TAILQ_FIRST(&asp->prefixes); p != NULL; p = np) {
		np = TAILQ_NEXT(p, path_l);
		if (p->re->prefix->aid != aid)
			continue;

		if (staletime && p->lastchange > staletime)
			continue;

		if (asp->pftableid) {
			struct bgpd_addr addr;

			pt_getaddr(p->re->prefix, &addr);
			/* Commit is done in peer_flush() */
			rde_send_pftable(prefix_aspath(p)->pftableid, &addr,
			    p->re->prefix->prefixlen, 1);
		}

		/* only count Adj-RIB-In */
		if (re_rib(p->re) == &ribs[RIB_ADJ_IN].rib)
			rprefixes++;

		prefix_destroy(p);
	}
	if (has_updates)
		for (p = TAILQ_FIRST(&asp->updates); p != NULL; p = np) {
			np = TAILQ_NEXT(p, path_l);
			if (p->re->prefix->aid != aid)
				continue;

			if (staletime && p->lastchange > staletime)
				continue;

			/* no need to worry about pftable on Adj-RIB-Out */
			prefix_destroy(p);
		}
	return (rprefixes);
}


/*
 * This function can only called when all prefix have been removed first.
 * Normally this happens directly out of the prefix removal functions.
 */
void
path_destroy(struct rde_aspath *asp)
{
	/* path_destroy can only unlink and free empty rde_aspath */
	if (!TAILQ_EMPTY(&asp->prefixes) || !TAILQ_EMPTY(&asp->updates))
		log_warnx("path_destroy: still has prefixes, leaking");

	nexthop_unlink(asp);
	LIST_REMOVE(asp, path_l);
	TAILQ_REMOVE(&asp->peer->path_h, asp, peer_l);
	asp->peer = NULL;
	asp->flags &= ~F_ATTR_LINKED;

	path_put(asp);
}

int
path_empty(struct rde_aspath *asp)
{
	return TAILQ_EMPTY(&asp->prefixes) && TAILQ_EMPTY(&asp->updates);
}

/*
 * the path object is linked into multiple lists for fast access.
 * These are peer_l, path_l and nexthop_l.
 * peer_l: list of all aspaths that belong to that peer
 * path_l: hash list to find paths quickly
 * nexthop_l: list of all aspaths with an equal exit nexthop
 */
static void
path_link(struct rde_aspath *asp, struct rde_peer *peer)
{
	struct aspath_head	*head;

	asp->peer = peer;

	asp->hash = path_hash(asp);
	head = PATH_HASH(asp->hash);

	LIST_INSERT_HEAD(head, asp, path_l);
	TAILQ_INSERT_HEAD(&peer->path_h, asp, peer_l);
	nexthop_link(asp);
	asp->flags |= F_ATTR_LINKED;
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
	dst->nexthop = nexthop_ref(src->nexthop);
	dst->hash = 0;
	dst->med = src->med;
	dst->lpref = src->lpref;
	dst->weight = src->weight;
	dst->origin = src->origin;
	dst->rtlabelid = rtlabel_ref(src->rtlabelid);
	dst->pftableid = pftable_ref(src->pftableid);

	dst->flags = src->flags & ~(F_ATTR_LINKED | F_ATTR_UPDATE);
	attr_copy(dst, src);

	return (dst);
}

/* initialize or pepare an aspath for use */
struct rde_aspath *
path_prep(struct rde_aspath *asp)
{
	memset(asp, 0, sizeof(*asp));
	TAILQ_INIT(&asp->prefixes);
	TAILQ_INIT(&asp->updates);
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

void
path_clean(struct rde_aspath *asp)
{
	if (asp->flags & F_ATTR_LINKED)
		fatalx("path_clean: linked object");

	rtlabel_unref(asp->rtlabelid);
	pftable_unref(asp->pftableid);
	aspath_put(asp->aspath);
	nexthop_put(asp->nexthop);
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

static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);
static void		 prefix_link(struct prefix *, struct rib_entry *,
			     struct rde_aspath *, int);
static void		 prefix_unlink(struct prefix *);

/*
 * search for specified prefix of a peer. Returns NULL if not found.
 */
struct prefix *
prefix_get(struct rib *rib, struct rde_peer *peer, struct bgpd_addr *prefix,
    int prefixlen, u_int32_t flags)
{
	struct rib_entry	*re;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)
		return (NULL);
	return (prefix_bypeer(re, peer, flags));
}

/*
 * Adds or updates a prefix.
 */
int
prefix_add(struct rib *rib, struct rde_aspath *asp, struct bgpd_addr *prefix,
    int prefixlen, int flag)

{
	struct prefix		*p;
	struct rib_entry	*re;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)
		re = rib_add(rib, prefix, prefixlen);

	p = prefix_bypeer(re, asp->peer, asp->flags);
	if (p == NULL) {
		p = prefix_alloc();
		prefix_link(p, re, asp, flag);
		return (1);
	} else {
		if (prefix_aspath(p) != asp) {
			/* prefix belongs to a different aspath so move */
			prefix_move(asp, p, flag);
		} else
			p->lastchange = time(NULL);
		return (0);
	}
}

/*
 * Move the prefix to the specified as path, removes the old asp if needed.
 */
void
prefix_move(struct rde_aspath *asp, struct prefix *p, int flag)
{
	struct prefix		*np;
	struct rde_aspath	*oasp;

	if (asp->peer != prefix_peer(p))
		fatalx("prefix_move: cross peer move");

	/* create new prefix node */
	np = prefix_alloc();
	np->aspath = asp;
	np->peer = asp->peer;
	np->re = p->re;
	np->lastchange = time(NULL);
	np->flags = flag;

	/* add to new as path */
	if (np->flags & F_PREFIX_USE_UPDATES)
		TAILQ_INSERT_HEAD(&asp->updates, np, path_l);
	else
		TAILQ_INSERT_HEAD(&asp->prefixes, np, path_l);
	/*
	 * no need to update the peer prefix count because we are only moving
	 * the prefix without changing the peer.
	 */

	/*
	 * First kick the old prefix node out of the prefix list,
	 * afterwards run the route decision for new prefix node.
	 * Because of this only one update is generated if the prefix
	 * was active.
	 * This is save because we create a new prefix and so the change
	 * is noticed by prefix_evaluate().
	 */
	LIST_REMOVE(p, rib_l);
	prefix_evaluate(np, np->re);

	/* remove old prefix node */
	oasp = prefix_aspath(p);
	if (p->flags & F_PREFIX_USE_UPDATES)
		TAILQ_REMOVE(&oasp->updates, p, path_l);
	else
		TAILQ_REMOVE(&oasp->prefixes, p, path_l);
	/* as before peer count needs no update because of move */

	/* destroy all references to other objects and free the old prefix */
	p->aspath = NULL;
	p->peer = NULL;
	p->re = NULL;
	prefix_free(p);

	/* destroy old path if empty */
	if (path_empty(oasp))
		path_destroy(oasp);
}

/*
 * Removes a prefix from all lists. If the parent objects -- path or
 * pt_entry -- become empty remove them too.
 */
int
prefix_remove(struct rib *rib, struct rde_peer *peer, struct bgpd_addr *prefix,
    int prefixlen, u_int32_t flags)
{
	struct prefix		*p;
	struct rib_entry	*re;
	struct rde_aspath	*asp;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)		/* Got a dummy withdrawn request */
		return (0);

	p = prefix_bypeer(re, peer, flags);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return (0);

	asp = prefix_aspath(p);
	if (asp->pftableid) {
		/* only prefixes in the local RIB were pushed into pf */
		rde_send_pftable(asp->pftableid, prefix, prefixlen, 1);
		rde_send_pftable_commit();
	}

	prefix_destroy(p);

	return (1);
}

/* dump a prefix into specified buffer */
int
prefix_write(u_char *buf, int len, struct bgpd_addr *prefix, u_int8_t plen,
    int withdraw)
{
	int	totlen;

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
		memcpy(buf, &prefix->vpn4.addr, PREFIX_SIZE(plen) - 1);
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
prefix_bypeer(struct rib_entry *re, struct rde_peer *peer, u_int32_t flags)
{
	struct prefix	*p;

	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		if (prefix_peer(p) != peer)
			continue;
		if (prefix_aspath(p)->flags & flags &&
		    (flags & F_ANN_DYNAMIC) !=
		    (prefix_aspath(p)->flags & F_ANN_DYNAMIC))
			continue;
		return (p);
	}
	return (NULL);
}

void
prefix_updateall(struct rde_aspath *asp, enum nexthop_state state,
    enum nexthop_state oldstate)
{
	struct prefix	*p;

	TAILQ_FOREACH(p, &asp->prefixes, path_l) {
		/*
		 * Skip non local-RIBs or RIBs that are flagged as noeval.
		 * There is no need to run over updates since that is only
		 * used on the Adj-RIB-Out.
		 */
		if (re_rib(p->re)->flags & F_RIB_NOEVALUATE)
			continue;

		if (oldstate == state && state == NEXTHOP_REACH) {
			/*
			 * The state of the nexthop did not change. The only
			 * thing that may have changed is the true_nexthop
			 * or other internal infos. This will not change
			 * the routing decision so shortcut here.
			 */
			if ((re_rib(p->re)->flags & F_RIB_NOFIB) == 0 &&
			    p == p->re->active)
				rde_send_kroute(re_rib(p->re), p, NULL);
			continue;
		}

		/* redo the route decision */
		LIST_REMOVE(p, rib_l);
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
}

/* kill a prefix. */
void
prefix_destroy(struct prefix *p)
{
	struct rde_aspath	*asp;

	asp = prefix_aspath(p);

	prefix_unlink(p);
	prefix_free(p);

	if (path_empty(asp))
		path_destroy(asp);
}

/*
 * helper function to clean up the connected networks after a reload
 */
void
prefix_network_clean(struct rde_peer *peer, time_t reloadtime, u_int32_t flags)
{
	struct rde_aspath	*asp, *xasp;
	struct prefix		*p, *xp;

	for (asp = TAILQ_FIRST(&peer->path_h); asp != NULL; asp = xasp) {
		xasp = TAILQ_NEXT(asp, peer_l);
		if ((asp->flags & F_ANN_DYNAMIC) != flags)
			continue;
		for (p = TAILQ_FIRST(&asp->prefixes); p != NULL; p = xp) {
			xp = TAILQ_NEXT(p, path_l);
			if (reloadtime > p->lastchange) {
				prefix_unlink(p);
				prefix_free(p);
			}
		}
		if (path_empty(asp))
			path_destroy(asp);
	}
}

/*
 * Link a prefix into the different parent objects.
 */
static void
prefix_link(struct prefix *pref, struct rib_entry *re, struct rde_aspath *asp,
    int flag)
{
	if (flag & F_PREFIX_USE_UPDATES)
		TAILQ_INSERT_HEAD(&asp->updates, pref, path_l);
	else
		TAILQ_INSERT_HEAD(&asp->prefixes, pref, path_l);

	pref->aspath = asp;
	pref->peer = asp->peer;
	pref->re = re;
	pref->lastchange = time(NULL);
	pref->flags = flag;

	/* make route decision */
	prefix_evaluate(pref, re);
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
prefix_unlink(struct prefix *pref)
{
	struct rib_entry	*re = pref->re;
	struct prefix_queue	*pq;

	/* make route decision */
	LIST_REMOVE(pref, rib_l);
	prefix_evaluate(NULL, re);

	if (pref->flags & F_PREFIX_USE_UPDATES)
		pq = &prefix_aspath(pref)->updates;
	else
		pq = &prefix_aspath(pref)->prefixes;

	TAILQ_REMOVE(pq, pref, path_l);
	if (rib_empty(re))
		rib_remove(re);

	/* destroy all references to other objects */
	pref->aspath = NULL;
	pref->peer = NULL;
	pref->re = NULL;
	pref->flags = 0;

	/*
	 * It's the caller's duty to do accounting and remove empty aspath
	 * structures. Also freeing the unlinked prefix is the caller's duty.
	 */
}

/* alloc and bzero new entry. May not fail. */
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
prefix_free(struct prefix *pref)
{
	rdemem.prefix_cnt--;
	free(pref);
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

void
nexthop_init(u_int32_t hashsize)
{
	u_int32_t	 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct nexthop_head));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init");

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
			(void)nexthop_put(nh);
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

void
nexthop_update(struct kroute_nexthop *msg)
{
	struct nexthop		*nh;
	struct rde_aspath	*asp;
	enum nexthop_state	 oldstate;

	nh = nexthop_lookup(&msg->nexthop);
	if (nh == NULL) {
		log_warnx("nexthop_update: non-existent nexthop %s",
		    log_addr(&msg->nexthop));
		return;
	}

	oldstate = nh->state;

	if (msg->valid)
		nh->state = NEXTHOP_REACH;
	else
		nh->state = NEXTHOP_UNREACH;

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

	if (oldstate == NEXTHOP_LOOKUP)
		/* drop reference which was hold during the lookup */
		if (nexthop_put(nh))
			return;

	if (rde_noevaluate())
		/*
		 * if the decision process is turned off there is no need
		 * for the aspath list walk.
		 */
		return;

	LIST_FOREACH(asp, &nh->path_h, nexthop_l) {
		prefix_updateall(asp, nh->state, oldstate);
	}
}

void
nexthop_modify(struct nexthop *setnh, enum action_types type, u_int8_t aid,
    struct nexthop **nexthop, u_int32_t *flags)
{
	*flags &= ~F_NEXTHOP_MASK;
	switch (type) {
	case ACTION_SET_NEXTHOP_REJECT:
		*flags |= F_NEXTHOP_REJECT;
		break;
	case ACTION_SET_NEXTHOP_BLACKHOLE:
		*flags |= F_NEXTHOP_BLACKHOLE;
		break;
	case ACTION_SET_NEXTHOP_NOMODIFY:
		*flags |= F_NEXTHOP_NOMODIFY;
		break;
	case ACTION_SET_NEXTHOP_SELF:
		*flags |= F_NEXTHOP_SELF;
		break;
	case ACTION_SET_NEXTHOP:
		/*
		 * it is possible that a prefix matches but has the wrong
		 * address family for the set nexthop. In this case ignore it.
		 */
		if (aid != setnh->exit_nexthop.aid)
			break;
		nexthop_put(*nexthop);
		*nexthop = nexthop_ref(setnh);
		break;
	default:
		break;
	}
}

void
nexthop_link(struct rde_aspath *asp)
{
	if (asp->nexthop == NULL)
		return;

	LIST_INSERT_HEAD(&asp->nexthop->path_h, asp, nexthop_l);
}

void
nexthop_unlink(struct rde_aspath *asp)
{
	struct nexthop	*nh;

	if (asp->nexthop == NULL)
		return;

	LIST_REMOVE(asp, nexthop_l);

	/* remove reference to nexthop */
	nh = asp->nexthop;
	asp->nexthop = NULL;

	(void)nexthop_put(nh);
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

		LIST_INIT(&nh->path_h);
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
nexthop_put(struct nexthop *nh)
{
	if (nh == NULL)
		return (0);

	if (--nh->refcnt > 0)
		return (0);

	/* sanity check */
	if (!LIST_EMPTY(&nh->path_h) || nh->state == NEXTHOP_LOOKUP)
		fatalx("nexthop_put: refcnt error");

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

