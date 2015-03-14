/*	$OpenBSD: rde_rib.c,v 1.142 2015/03/14 03:52:42 claudio Exp $ */

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

#include <stdlib.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"

/*
 * BGP RIB -- Routing Information Base
 *
 * The RIB is build with one aspect in mind. Speed -- actually update speed.
 * Therefore one thing needs to be absolutely avoided, long table walks.
 * This is achieved by heavily linking the different parts together.
 */
u_int16_t rib_size;
struct rib *ribs;

LIST_HEAD(, rib_context) rib_dump_h = LIST_HEAD_INITIALIZER(rib_dump_h);

struct rib_entry *rib_add(struct rib *, struct bgpd_addr *, int);
int rib_compare(const struct rib_entry *, const struct rib_entry *);
void rib_remove(struct rib_entry *);
int rib_empty(struct rib_entry *);
struct rib_entry *rib_restart(struct rib_context *);

RB_PROTOTYPE(rib_tree, rib_entry, rib_e, rib_compare);
RB_GENERATE(rib_tree, rib_entry, rib_e, rib_compare);


/* RIB specific functions */
u_int16_t
rib_new(char *name, u_int rtableid, u_int16_t flags)
{
	struct rib	*xribs;
	u_int16_t	id;

	for (id = 0; id < rib_size; id++) {
		if (*ribs[id].name == '\0')
			break;
	}

	if (id == RIB_FAILED)
		fatalx("rib_new: trying to use reserved id");

	if (id >= rib_size) {
		if ((xribs = reallocarray(ribs, id + 1,
		    sizeof(struct rib))) == NULL) {
			/* XXX this is not clever */
			fatal("rib_add");
		}
		ribs = xribs;
		rib_size = id + 1;
	}

	bzero(&ribs[id], sizeof(struct rib));
	strlcpy(ribs[id].name, name, sizeof(ribs[id].name));
	RB_INIT(&ribs[id].rib);
	ribs[id].state = RECONF_REINIT;
	ribs[id].id = id;
	ribs[id].flags = flags;
	ribs[id].rtableid = rtableid;

	ribs[id].in_rules = calloc(1, sizeof(struct filter_head));
	if (ribs[id].in_rules == NULL)
		fatal(NULL);
	TAILQ_INIT(ribs[id].in_rules);

	return (id);
}

u_int16_t
rib_find(char *name)
{
	u_int16_t id;

	if (name == NULL || *name == '\0')
		return (1);	/* no name returns the Loc-RIB */

	for (id = 0; id < rib_size; id++) {
		if (!strcmp(ribs[id].name, name))
			return (id);
	}

	return (RIB_FAILED);
}

void
rib_free(struct rib *rib)
{
	struct rib_context *ctx, *next;
	struct rib_entry *re, *xre;
	struct prefix *p, *np;

	/* abort pending rib_dumps */
	for (ctx = LIST_FIRST(&rib_dump_h); ctx != NULL; ctx = next) {
		next = LIST_NEXT(ctx, entry);
		if (ctx->ctx_rib == rib) {
			re = ctx->ctx_re;
			re->flags &= ~F_RIB_ENTRYLOCK;
			LIST_REMOVE(ctx, entry);
			if (ctx->ctx_done)
				ctx->ctx_done(ctx->ctx_arg);
			else
				free(ctx);
		}
	}

	for (re = RB_MIN(rib_tree, &rib->rib); re != NULL; re = xre) {
		xre = RB_NEXT(rib_tree,  &rib->rib, re);

		/*
		 * Removing the prefixes is tricky because the last one
		 * will remove the rib_entry as well and because we do
		 * an empty check in prefix_destroy() it is not possible to
		 * use the default for loop.
		 */
		while ((p = LIST_FIRST(&re->prefix_h))) {
			np = LIST_NEXT(p, rib_l);
			if (p->aspath->pftableid) {
				struct bgpd_addr addr;

				pt_getaddr(p->prefix, &addr);
				/* Commit is done in peer_down() */
				rde_send_pftable(p->aspath->pftableid, &addr,
				    p->prefix->prefixlen, 1);
			}
			prefix_destroy(p);
			if (np == NULL)
				break;
		}
	}
	filterlist_free(rib->in_rules_tmp);
	filterlist_free(rib->in_rules);
	bzero(rib, sizeof(struct rib));
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

	return (RB_FIND(rib_tree, &rib->rib, &xre));
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
	re->flags = rib->flags;
	re->ribid = rib->id;

        if (RB_INSERT(rib_tree, &rib->rib, re) != NULL) {
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

	if (re->flags & F_RIB_ENTRYLOCK)
		/* entry is locked, don't free it. */
		return;

	pt_unref(re->prefix);
	if (pt_empty(re->prefix))
		pt_remove(re->prefix);

	if (RB_REMOVE(rib_tree, &ribs[re->ribid].rib, re) == NULL)
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

	if (ctx->ctx_re == NULL) {
		re = RB_MIN(rib_tree, &ctx->ctx_rib->rib);
		LIST_INSERT_HEAD(&rib_dump_h, ctx, entry);
	} else
		re = rib_restart(ctx);

	for (i = 0; re != NULL; re = RB_NEXT(rib_tree, unused, re)) {
		if (ctx->ctx_aid != AID_UNSPEC &&
		    ctx->ctx_aid != re->prefix->aid)
			continue;
		if (ctx->ctx_count && i++ >= ctx->ctx_count &&
		    (re->flags & F_RIB_ENTRYLOCK) == 0) {
			/* store and lock last element */
			ctx->ctx_re = re;
			re->flags |= F_RIB_ENTRYLOCK;
			return;
		}
		ctx->ctx_upcall(re, ctx->ctx_arg);
	}

	LIST_REMOVE(ctx, entry);
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
	re->flags &= ~F_RIB_ENTRYLOCK;

	/* find first non empty element */
	while (re && rib_empty(re))
		re = RB_NEXT(rib_tree, unused, re);

	/* free the previously locked rib element if empty */
	if (rib_empty(ctx->ctx_re))
		rib_remove(ctx->ctx_re);
	ctx->ctx_re = NULL;
	return (re);
}

void
rib_dump_runner(void)
{
	struct rib_context	*ctx, *next;

	for (ctx = LIST_FIRST(&rib_dump_h); ctx != NULL; ctx = next) {
		next = LIST_NEXT(ctx, entry);
		rib_dump_r(ctx);
	}
}

int
rib_dump_pending(void)
{
	return (!LIST_EMPTY(&rib_dump_h));
}

/* used to bump correct prefix counters */
#define PREFIX_COUNT(x, op)			\
	do {					\
		(x)->prefix_cnt += (op);	\
	} while (0)

/* path specific functions */

static void	path_link(struct rde_aspath *, struct rde_peer *);

struct path_table pathtable;

SIPHASH_KEY pathtablekey;

/* XXX the hash should also include communities and the other attrs */
#define PATH_HASH(x)				\
	&pathtable.path_hashtbl[SipHash24(&pathtablekey, (x)->data, (x)->len) & \
	    pathtable.path_hashmask]

void
path_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	pathtable.path_hashtbl = calloc(hs, sizeof(struct aspath_head));
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

int
path_update(struct rib *rib, struct rde_peer *peer, struct rde_aspath *nasp,
    struct bgpd_addr *prefix, int prefixlen)
{
	struct rde_aspath	*asp;
	struct prefix		*p;

	if (nasp->pftableid) {
		rde_send_pftable(nasp->pftableid, prefix, prefixlen, 0);
		rde_send_pftable_commit();
	}

	/*
	 * First try to find a prefix in the specified RIB.
	 */
	if ((p = prefix_get(rib, peer, prefix, prefixlen, 0)) != NULL) {
		if (path_compare(nasp, p->aspath) == 0) {
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
		asp = path_copy(nasp);
		path_link(asp, peer);
	}

	/* If the prefix was found move it else add it to the aspath. */
	if (p != NULL)
		prefix_move(asp, p);
	else
		return (prefix_add(rib, asp, prefix, prefixlen));
	return (0);
}

int
path_compare(struct rde_aspath *a, struct rde_aspath *b)
{
	int		 r;

	if (a->origin > b->origin)
		return (1);
	if (a->origin < b->origin)
		return (-1);
	if ((a->flags & ~F_ATTR_LINKED) > (b->flags & ~F_ATTR_LINKED))
		return (1);
	if ((a->flags & ~F_ATTR_LINKED) < (b->flags & ~F_ATTR_LINKED))
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

struct rde_aspath *
path_lookup(struct rde_aspath *aspath, struct rde_peer *peer)
{
	struct aspath_head	*head;
	struct rde_aspath	*asp;

	head = PATH_HASH(aspath->aspath);

	LIST_FOREACH(asp, head, path_l) {
		if (peer == asp->peer && path_compare(aspath, asp) == 0)
			return (asp);
	}
	return (NULL);
}

void
path_remove(struct rde_aspath *asp)
{
	struct prefix	*p, *np;

	for (p = LIST_FIRST(&asp->prefix_h); p != NULL; p = np) {
		np = LIST_NEXT(p, path_l);
		if (asp->pftableid) {
			struct bgpd_addr addr;

			pt_getaddr(p->prefix, &addr);
			/* Commit is done in peer_down() */
			rde_send_pftable(p->aspath->pftableid, &addr,
			    p->prefix->prefixlen, 1);
		}
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

	rprefixes=0;
	staletime = asp->peer->staletime[aid];
	for (p = LIST_FIRST(&asp->prefix_h); p != NULL; p = np) {
		np = LIST_NEXT(p, path_l);
		if (p->prefix->aid != aid)
			continue;

		if (staletime && p->lastchange > staletime)
			continue;

		if (asp->pftableid) {
			struct bgpd_addr addr;

			pt_getaddr(p->prefix, &addr);
			/* Commit is done in peer_flush() */
			rde_send_pftable(p->aspath->pftableid, &addr,
			    p->prefix->prefixlen, 1);
		}

		/* only count Adj-RIB-In */
		if (p->rib->ribid == 0)
			rprefixes++;

		prefix_destroy(p);
	}
	return (rprefixes);
}


/* this function is only called by prefix_remove and path_remove */
void
path_destroy(struct rde_aspath *asp)
{
	/* path_destroy can only unlink and free empty rde_aspath */
	if (asp->prefix_cnt != 0 || asp->active_cnt != 0)
		log_warnx("path_destroy: prefix count out of sync");

	nexthop_unlink(asp);
	LIST_REMOVE(asp, path_l);
	LIST_REMOVE(asp, peer_l);
	asp->peer = NULL;
	asp->nexthop = NULL;
	asp->flags &= ~F_ATTR_LINKED;

	path_put(asp);
}

int
path_empty(struct rde_aspath *asp)
{
	return LIST_EMPTY(&asp->prefix_h);
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

	head = PATH_HASH(asp->aspath);

	LIST_INSERT_HEAD(head, asp, path_l);
	LIST_INSERT_HEAD(&peer->path_h, asp, peer_l);
	asp->peer = peer;
	nexthop_link(asp);
	asp->flags |= F_ATTR_LINKED;
}

/*
 * copy asp to a new UNLINKED one mainly for filtering
 */
struct rde_aspath *
path_copy(struct rde_aspath *asp)
{
	struct rde_aspath *nasp;

	nasp = path_get();
	nasp->aspath = asp->aspath;
	if (nasp->aspath != NULL) {
		nasp->aspath->refcnt++;
		rdemem.aspath_refs++;
	}
	nasp->nexthop = asp->nexthop;
	nasp->med = asp->med;
	nasp->lpref = asp->lpref;
	nasp->weight = asp->weight;
	nasp->origin = asp->origin;
	nasp->rtlabelid = asp->rtlabelid;
	rtlabel_ref(nasp->rtlabelid);
	nasp->pftableid = asp->pftableid;
	pftable_ref(nasp->pftableid);

	nasp->flags = asp->flags & ~F_ATTR_LINKED;
	attr_copy(nasp, asp);

	return (nasp);
}

/* alloc and initialize new entry. May not fail. */
struct rde_aspath *
path_get(void)
{
	struct rde_aspath *asp;

	asp = calloc(1, sizeof(*asp));
	if (asp == NULL)
		fatal("path_alloc");
	rdemem.path_cnt++;

	LIST_INIT(&asp->prefix_h);
	asp->origin = ORIGIN_INCOMPLETE;
	asp->lpref = DEFAULT_LPREF;
	/* med = 0 */
	/* weight = 0 */
	/* rtlabel = 0 */

	return (asp);
}

/* free an unlinked element */
void
path_put(struct rde_aspath *asp)
{
	if (asp == NULL)
		return;

	if (asp->flags & F_ATTR_LINKED)
		fatalx("path_put: linked object");

	rtlabel_unref(asp->rtlabelid);
	pftable_unref(asp->pftableid);
	aspath_put(asp->aspath);
	attr_freeall(asp);
	rdemem.path_cnt--;
	free(asp);
}

/* prefix specific functions */

static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);
static void		 prefix_link(struct prefix *, struct rib_entry *,
			     struct rde_aspath *);
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
    int prefixlen)

{
	struct prefix		*p;
	struct rib_entry	*re;

	re = rib_get(rib, prefix, prefixlen);
	if (re == NULL)
		re = rib_add(rib, prefix, prefixlen);

	p = prefix_bypeer(re, asp->peer, asp->flags);
	if (p == NULL) {
		p = prefix_alloc();
		prefix_link(p, re, asp);
		return (1);
	} else {
		if (p->aspath != asp) {
			/* prefix belongs to a different aspath so move */
			prefix_move(asp, p);
		} else
			p->lastchange = time(NULL);
		return (0);
	}
}

/*
 * Move the prefix to the specified as path, removes the old asp if needed.
 */
void
prefix_move(struct rde_aspath *asp, struct prefix *p)
{
	struct prefix		*np;
	struct rde_aspath	*oasp;

	if (asp->peer != p->aspath->peer)
		fatalx("prefix_move: cross peer move");

	/* create new prefix node */
	np = prefix_alloc();
	np->aspath = asp;
	/* peer and prefix pointers are still equal */
	np->prefix = p->prefix;
	np->rib = p->rib;
	np->lastchange = time(NULL);

	/* add to new as path */
	LIST_INSERT_HEAD(&asp->prefix_h, np, path_l);
	PREFIX_COUNT(asp, 1);
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
	prefix_evaluate(np, np->rib);

	/* remove old prefix node */
	oasp = p->aspath;
	LIST_REMOVE(p, path_l);
	PREFIX_COUNT(oasp, -1);
	/* as before peer count needs no update because of move */

	/* destroy all references to other objects and free the old prefix */
	p->aspath = NULL;
	p->prefix = NULL;
	p->rib = NULL;
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
	if (re == NULL)	/* Got a dummy withdrawn request */
		return (0);

	p = prefix_bypeer(re, peer, flags);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return (0);

	asp = p->aspath;

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
prefix_write(u_char *buf, int len, struct bgpd_addr *prefix, u_int8_t plen)
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
		totlen = PREFIX_SIZE(plen) + sizeof(prefix->vpn4.rd) +
		    prefix->vpn4.labellen;
		plen += (sizeof(prefix->vpn4.rd) + prefix->vpn4.labellen) * 8;

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		memcpy(buf, &prefix->vpn4.labelstack, prefix->vpn4.labellen);
		buf += prefix->vpn4.labellen;
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
	if (prefix_write(bptr, totlen, prefix, plen) == -1)
		return (-1);
	return (0);
}

/*
 * Searches in the prefix list of specified pt_entry for a prefix entry
 * belonging to the peer peer. Returns NULL if no match found.
 */
struct prefix *
prefix_bypeer(struct rib_entry *re, struct rde_peer *peer, u_int32_t flags)
{
	struct prefix	*p;

	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		if (p->aspath->peer != peer)
			continue;
		if (p->aspath->flags & flags &&
		    (flags & F_ANN_DYNAMIC) !=
		    (p->aspath->flags & F_ANN_DYNAMIC))
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

	LIST_FOREACH(p, &asp->prefix_h, path_l) {
		/*
		 * skip non local-RIBs or RIBs that are flagged as noeval.
		 */
		if (p->rib->flags & F_RIB_NOEVALUATE)
			continue;

		if (oldstate == state && state == NEXTHOP_REACH) {
			/*
			 * The state of the nexthop did not change. The only
			 * thing that may have changed is the true_nexthop
			 * or other internal infos. This will not change
			 * the routing decision so shortcut here.
			 */
			if ((p->rib->flags & F_RIB_NOFIB) == 0 &&
			    p == p->rib->active)
				rde_send_kroute(p, NULL, p->rib->ribid);
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
		if (p == p->rib->active)
			prefix_evaluate(NULL, p->rib);
		prefix_evaluate(p, p->rib);
	}
}

/* kill a prefix. */
void
prefix_destroy(struct prefix *p)
{
	struct rde_aspath	*asp;

	asp = p->aspath;
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

	for (asp = LIST_FIRST(&peer->path_h); asp != NULL; asp = xasp) {
		xasp = LIST_NEXT(asp, peer_l);
		if ((asp->flags & F_ANN_DYNAMIC) != flags)
			continue;
		for (p = LIST_FIRST(&asp->prefix_h); p != NULL; p = xp) {
			xp = LIST_NEXT(p, path_l);
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
prefix_link(struct prefix *pref, struct rib_entry *re, struct rde_aspath *asp)
{
	LIST_INSERT_HEAD(&asp->prefix_h, pref, path_l);
	PREFIX_COUNT(asp, 1);

	pref->aspath = asp;
	pref->rib = re;
	pref->prefix = re->prefix;
	pt_ref(pref->prefix);
	pref->lastchange = time(NULL);

	/* make route decision */
	prefix_evaluate(pref, re);
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
prefix_unlink(struct prefix *pref)
{
	struct rib_entry	*re = pref->rib;

	/* make route decision */
	LIST_REMOVE(pref, rib_l);
	prefix_evaluate(NULL, re);

	LIST_REMOVE(pref, path_l);
	PREFIX_COUNT(pref->aspath, -1);

	pt_unref(pref->prefix);
	if (pt_empty(pref->prefix))
		pt_remove(pref->prefix);
	if (rib_empty(re))
		rib_remove(re);

	/* destroy all references to other objects */
	pref->aspath = NULL;
	pref->prefix = NULL;
	pref->rib = NULL;

	/*
	 * It's the caller's duty to remove empty aspath structures.
	 * Also freeing the unlinked prefix is the caller's duty.
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
			(void)nexthop_delete(nh);
		}
		if (!LIST_EMPTY(&nexthoptable.nexthop_hashtbl[i]))
			log_warnx("nexthop_shutdown: non-free table");
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

	if (nexthop_delete(nh))
		/* nexthop no longer used */
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
nexthop_modify(struct rde_aspath *asp, struct bgpd_addr *nexthop,
    enum action_types type, u_int8_t aid)
{
	struct nexthop	*nh;

	if (type == ACTION_SET_NEXTHOP && aid != nexthop->aid)
		return;

	asp->flags &= ~F_NEXTHOP_MASK;
	switch (type) {
	case ACTION_SET_NEXTHOP_REJECT:
		asp->flags |= F_NEXTHOP_REJECT;
		break;
	case ACTION_SET_NEXTHOP_BLACKHOLE:
		asp->flags |= F_NEXTHOP_BLACKHOLE;
		break;
	case ACTION_SET_NEXTHOP_NOMODIFY:
		asp->flags |= F_NEXTHOP_NOMODIFY;
		break;
	case ACTION_SET_NEXTHOP_SELF:
		asp->flags |= F_NEXTHOP_SELF;
		break;
	case ACTION_SET_NEXTHOP:
		nh = nexthop_get(nexthop);
		if (asp->flags & F_ATTR_LINKED)
			nexthop_unlink(asp);
		asp->nexthop = nh;
		if (asp->flags & F_ATTR_LINKED)
			nexthop_link(asp);
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

	/* see if list is empty */
	nh = asp->nexthop;
	asp->nexthop = NULL;

	(void)nexthop_delete(nh);
}

int
nexthop_delete(struct nexthop *nh)
{
	/* nexthop still used by some other aspath */
	if (!LIST_EMPTY(&nh->path_h))
		return (0);

	/* either pinned or in a state where it may not be deleted */
	if (nh->refcnt > 0 || nh->state == NEXTHOP_LOOKUP)
		return (0);

	LIST_REMOVE(nh, nexthop_l);
	rde_send_nexthop(&nh->exit_nexthop, 0);

	rdemem.nexthop_cnt--;
	free(nh);
	return (1);
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
		nh->exit_nexthop = *nexthop;
		LIST_INSERT_HEAD(nexthop_hash(nexthop), nh,
		    nexthop_l);

		rde_send_nexthop(&nh->exit_nexthop, 1);
	}

	return (nh);
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

