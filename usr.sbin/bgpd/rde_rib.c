/*	$OpenBSD: rde_rib.c,v 1.46 2004/05/08 19:17:20 henning Exp $ */

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

#include "bgpd.h"
#include "ensure.h"
#include "rde.h"

/*
 * BGP RIB -- Routing Information Base
 *
 * The RIB is build with one aspect in mind. Speed -- actually update speed.
 * Therefor one thing needs to be absolutely avoided, long table walks.
 * This is achieved by heavily linking the different parts together.
 */

struct rib_stats {
	u_int64_t	path_update;
	u_int64_t	path_get;
	u_int64_t	path_add;
	u_int64_t	path_remove;
	u_int64_t	path_updateall;
	u_int64_t	path_destroy;
	u_int64_t	path_link;
	u_int64_t	path_unlink;
	u_int64_t	path_alloc;
	u_int64_t	path_free;
	u_int64_t	prefix_get;
	u_int64_t	prefix_add;
	u_int64_t	prefix_move;
	u_int64_t	prefix_remove;
	u_int64_t	prefix_updateall;
	u_int64_t	prefix_link;
	u_int64_t	prefix_unlink;
	u_int64_t	prefix_alloc;
	u_int64_t	prefix_free;
	u_int64_t	nexthop_add;
	u_int64_t	nexthop_remove;
	u_int64_t	nexthop_update;
	u_int64_t	nexthop_get;
	u_int64_t	nexthop_alloc;
	u_int64_t	nexthop_free;
} ribstats;
#define RIB_STAT(x)	(ribstats.x++)

/* path specific functions */

static void	path_link(struct rde_aspath *, struct rde_peer *);
static void	path_unlink(struct rde_aspath *);
static struct rde_aspath	*path_alloc(void);
static void	path_free(struct rde_aspath *);

struct path_table pathtable;

#define PATH_HASH(x)				\
	&pathtable.path_hashtbl[aspath_hash((x)) & pathtable.path_hashmask]

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
path_update(struct rde_peer *peer, struct attr_flags *attrs,
    struct bgpd_addr *prefix, int prefixlen)
{
	struct rde_aspath	*asp;
	struct prefix		*p;
	struct pt_entry		*pte;

	RIB_STAT(path_update);

	rde_send_pftable(attrs->pftable, prefix, prefixlen, 0);
	rde_send_pftable_commit();

	if ((asp = path_get(attrs->aspath, peer)) == NULL) {
		/* path not available */
		asp = path_add(peer, attrs);
		pte = prefix_add(asp, prefix, prefixlen);
	} else {
		if (attr_compare(&asp->flags, attrs) == 0) {
			/* path are equal, just add prefix */
			pte = prefix_add(asp, prefix, prefixlen);
			attr_free(attrs);
		} else {
			/* non equal path attributes create new path */
			if ((p = prefix_get(asp, prefix, prefixlen)) == NULL) {
				asp = path_add(peer, attrs);
				pte = prefix_add(asp, prefix, prefixlen);
			} else {
				asp = path_add(peer, attrs);
				pte = prefix_move(asp, p);
			}
		}
	}
}

struct rde_aspath *
path_get(struct aspath *aspath, struct rde_peer *peer)
{
	struct aspath_head	*head;
	struct rde_aspath	*asp;

	RIB_STAT(path_get);

	head = PATH_HASH(aspath);
	ENSURE(head != NULL);

	LIST_FOREACH(asp, head, path_l) {
		if (aspath_compare(asp->flags.aspath, aspath) == 0 &&
		    peer == asp->peer)
			return asp;
	}
	return NULL;
}

struct rde_aspath *
path_add(struct rde_peer *peer, struct attr_flags *attr)
{
	struct rde_aspath	*asp;

	RIB_STAT(path_add);
	ENSURE(peer != NULL);

	asp = path_alloc();

	attr_move(&asp->flags, attr);

	path_link(asp, peer);
	return asp;
}

void
path_remove(struct rde_aspath *asp)
{
	struct prefix	*p;

	RIB_STAT(path_remove);

	while ((p = LIST_FIRST(&asp->prefix_h)) != NULL) {
		/* Commit is done in peer_down() */
		rde_send_pftable(p->aspath->flags.pftable,
		    &p->prefix->prefix, p->prefix->prefixlen, 1);

		prefix_destroy(p);
	}
	path_destroy(asp);
}

void
path_updateall(struct rde_aspath *asp, enum nexthop_state state)
{
	RIB_STAT(path_updateall);

	if (rde_noevaluate())
		/* if the decision process is turned off this is a no-op */
		return;

	prefix_updateall(asp, state);
}

/* this function is only called by prefix_remove and path_remove */
void
path_destroy(struct rde_aspath *asp)
{
	RIB_STAT(path_destroy);
	/* path_destroy can only unlink and free empty rde_aspath */
	ENSURE(path_empty(asp));

	path_unlink(asp);
	path_free(asp);
}

int
path_empty(struct rde_aspath *asp)
{
	ENSURE(asp != NULL);

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

	RIB_STAT(path_link);

	head = PATH_HASH(asp->flags.aspath);
	ENSURE(head != NULL);

	LIST_INSERT_HEAD(head, asp, path_l);
	LIST_INSERT_HEAD(&peer->path_h, asp, peer_l);
	asp->peer = peer;

	ENSURE(asp->nexthop == NULL);
	nexthop_add(asp);
}

static void
path_unlink(struct rde_aspath *asp)
{
	RIB_STAT(path_unlink);
	ENSURE(path_empty(asp));
	ENSURE(asp->prefix_cnt == 0 && asp->active_cnt == 0);

	nexthop_remove(asp);
	LIST_REMOVE(asp, path_l);
	LIST_REMOVE(asp, peer_l);
	asp->peer = NULL;
	asp->nexthop = NULL;

	attr_free(&asp->flags);
}

/* alloc and initialize new entry. May not fail. */
static struct rde_aspath *
path_alloc(void)
{
	struct rde_aspath *asp;

	RIB_STAT(path_alloc);

	asp = calloc(1, sizeof(*asp));
	if (asp == NULL)
		fatal("path_alloc");
	LIST_INIT(&asp->prefix_h);
	return asp;
}

/* free a unlinked element */
static void
path_free(struct rde_aspath *asp)
{
	RIB_STAT(path_free);
	ENSURE(asp->peer == NULL &&
	    asp->flags.aspath == NULL &&
	    TAILQ_EMPTY(&asp->flags.others));
	free(asp);
}

/* prefix specific functions */

static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);
static void		 prefix_link(struct prefix *, struct pt_entry *,
			     struct rde_aspath *);
static void		 prefix_unlink(struct prefix *);

/*
 * search in the path list for specified prefix. Returns NULL if not found.
 */
struct prefix *
prefix_get(struct rde_aspath *asp, struct bgpd_addr *prefix, int prefixlen)
{
	struct prefix	*p;

	RIB_STAT(prefix_get);
	ENSURE(asp != NULL);

	LIST_FOREACH(p, &asp->prefix_h, path_l) {
		ENSURE(p->prefix != NULL);
		if (p->prefix->prefixlen == prefixlen &&
		    p->prefix->prefix.v4.s_addr == prefix->v4.s_addr) {
			ENSURE(p->aspath == asp);
			return p;
		}
	}

	return NULL;
}

/*
 * Adds or updates a prefix.
 */
struct pt_entry *
prefix_add(struct rde_aspath *asp, struct bgpd_addr *prefix, int prefixlen)

{
	struct prefix	*p;
	struct pt_entry	*pte;
	int		 needlink = 0;

	RIB_STAT(prefix_add);

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL) {
		pte = pt_add(prefix, prefixlen);
	}
	p = prefix_bypeer(pte, asp->peer);
	if (p == NULL) {
		needlink = 1;
		p = prefix_alloc();
	}

	if (needlink == 1)
		prefix_link(p, pte, asp);
	else {
		if (p->aspath != asp)
			/* prefix belongs to a different aspath so move */
			return prefix_move(asp, p);
		p->lastchange = time(NULL);
	}

	return pte;
}

/*
 * Move the prefix to the specified as path, removes the old asp if needed.
 */
struct pt_entry *
prefix_move(struct rde_aspath *asp, struct prefix *p)
{
	struct prefix		*np;
	struct rde_aspath	*oasp;

	RIB_STAT(prefix_move);
	ENSURE(asp->peer == p->aspath->peer);

	/* create new prefix node */
	np = prefix_alloc();
	np->aspath = asp;
	/* peer and prefix pointers are still equal */
	np->prefix = p->prefix;
	np->peer = p->peer;
	np->lastchange = time(NULL);

	/* add to new as path */
	LIST_INSERT_HEAD(&asp->prefix_h, np, path_l);
	asp->prefix_cnt++;
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
	LIST_REMOVE(p, prefix_l);
	prefix_evaluate(np, np->prefix);

	/* remove old prefix node */
	oasp = p->aspath;
	LIST_REMOVE(p, path_l);
	ENSURE(oasp->prefix_cnt > 0);
	ENSURE(oasp->peer->prefix_cnt > 0);
	oasp->prefix_cnt--;
	/* as before peer count needs no update because of move */

	/* destroy all references to other objects and free the old prefix */
	p->aspath = NULL;
	p->prefix = NULL;
	prefix_free(p);

	/* destroy old path if empty */
	if (path_empty(oasp))
		path_destroy(oasp);

	return np->prefix;
}

/*
 * Removes a prefix from all lists. If the parent objects -- path or
 * pt_entry -- become empty remove them too.
 */
void
prefix_remove(struct rde_peer *peer, struct bgpd_addr *prefix, int prefixlen)
{
	struct prefix		*p;
	struct pt_entry		*pte;
	struct rde_aspath	*asp;

	RIB_STAT(prefix_remove);

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)	/* Got a dummy withdrawn request */
		return;

	p = prefix_bypeer(pte, peer);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return;

	asp = p->aspath;

	rde_send_pftable(asp->flags.pftable, prefix, prefixlen, 1);
	rde_send_pftable_commit();

	prefix_unlink(p);
	prefix_free(p);

	if (pt_empty(pte))
		pt_remove(pte);
	if (path_empty(asp))
		path_destroy(asp);
}

/*
 * Searches in the prefix list of specified pt_entry for a prefix entry
 * belonging to the peer peer. Returns NULL if no match found.
 */
struct prefix *
prefix_bypeer(struct pt_entry *pte, struct rde_peer *peer)
{
	struct prefix	*p;

	ENSURE(pte != NULL);

	LIST_FOREACH(p, &pte->prefix_h, prefix_l) {
		if (p->aspath->peer == peer)
			return p;
	}
	return NULL;
}

void
prefix_updateall(struct rde_aspath *asp, enum nexthop_state state)
{
	struct prefix	*p;

	RIB_STAT(prefix_updateall);
	ENSURE(asp != NULL);

	if (rde_noevaluate())
		/* if the decision process is turned off this is a no-op */
		return;

	LIST_FOREACH(p, &asp->prefix_h, path_l) {
		/* redo the route decision */
		LIST_REMOVE(p, prefix_l);
		/*
		 * If the prefix is the active one remove it first,
		 * this has to be done because we can not detect when
		 * the active prefix changes it's state. In this case
		 * we know that this is a withdrawl and so the second
		 * prefix_evaluate() will generate no update because
		 * the nexthop is unreachable or ineligible.
		 */
		if (p == p->prefix->active)
			prefix_evaluate(NULL, p->prefix);
		prefix_evaluate(p, p->prefix);
	}
}

/* kill a prefix. Only called by path_remove. */
void
prefix_destroy(struct prefix *p)
{
	struct pt_entry		*pte;

	pte = p->prefix;
	prefix_unlink(p);
	prefix_free(p);

	if (pt_empty(pte))
		pt_remove(pte);
}

/*
 * helper function to clean up the connected networks after a reload
 */
void
prefix_network_clean(struct rde_peer *peer, time_t reloadtime)
{
	struct rde_aspath	*asp, *xasp;
	struct prefix		*p, *xp;
	struct pt_entry		*pte;

	for (asp = LIST_FIRST(&peer->path_h); asp != NULL; asp = xasp) {
		xasp = LIST_NEXT(asp, peer_l);
		for (p = LIST_FIRST(&asp->prefix_h); p != NULL; p = xp) {
			xp = LIST_NEXT(p, path_l);
			if (reloadtime > p->lastchange) {
				pte = p->prefix;
				prefix_unlink(p);
				prefix_free(p);

				if (pt_empty(pte))
					pt_remove(pte);
				if (path_empty(asp))
					path_destroy(asp);
			}
		}
	}
}

/*
 * Link a prefix into the different parent objects.
 */
static void
prefix_link(struct prefix *pref, struct pt_entry *pte, struct rde_aspath *asp)
{
	RIB_STAT(prefix_link);
	ENSURE(pref->aspath == NULL &&
	    pref->prefix == NULL);
	ENSURE(pref != NULL && pte != NULL && asp != NULL);
	ENSURE(prefix_bypeer(pte, asp->peer) == NULL);

	LIST_INSERT_HEAD(&asp->prefix_h, pref, path_l);
	asp->prefix_cnt++;
	asp->peer->prefix_cnt++;

	pref->aspath = asp;
	pref->prefix = pte;
	pref->peer = asp->peer;
	pref->lastchange = time(NULL);

	/* make route decision */
	prefix_evaluate(pref, pte);
}

/*
 * Unlink a prefix from the different parent objects.
 */
static void
prefix_unlink(struct prefix *pref)
{
	RIB_STAT(prefix_unlink);
	ENSURE(pref != NULL);
	ENSURE(pref->prefix != NULL && pref->aspath != NULL);

	/* make route decision */
	LIST_REMOVE(pref, prefix_l);
	prefix_evaluate(NULL, pref->prefix);

	LIST_REMOVE(pref, path_l);
	ENSURE(pref->aspath->prefix_cnt > 0);
	ENSURE(pref->aspath->peer->prefix_cnt > 0);
	pref->aspath->prefix_cnt--;
	pref->aspath->peer->prefix_cnt--;

	/* destroy all references to other objects */
	pref->aspath = NULL;
	pref->prefix = NULL;

	/*
	 * It's the caller's duty to remove empty aspath respectively pt_entry
	 * structures. Also freeing the unlinked prefix is the caller's duty.
	 */
}

/* alloc and bzero new entry. May not fail. */
static struct prefix *
prefix_alloc(void)
{
	struct prefix *p;

	RIB_STAT(prefix_alloc);

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("prefix_alloc");
	return p;
}

/* free a unlinked entry */
static void
prefix_free(struct prefix *pref)
{
	RIB_STAT(prefix_free);
	ENSURE(pref->aspath == NULL &&
	    pref->prefix == NULL);
	free(pref);
}

/* nexthop functions */

/*
 * XXX
 * Storing the nexthop info in a hash table is not optimal. The problem is
 * that updates (invalidate and validate) come in as prefixes and so storing
 * the nexthops in a hash is not optimal. An (in)validate needs to do a table
 * walk to find all candidates.
 * Currently I think that there are many more adds and removes so that a
 * hash table has more benefits and the table walk should not happen too often.
 */

static struct nexthop	*nexthop_get(struct in_addr);
static struct nexthop	*nexthop_alloc(void);
static void		 nexthop_free(struct nexthop *);

/*
 * In BGP there exist two nexthops: the exit nexthop which was announced via
 * BGP and the true nexthop which is used in the FIB -- forward information
 * base a.k.a kernel routing table. When sending updates it is even more
 * confusing. In IBGP we pass the unmodified exit nexthop to the neighbors
 * while in EBGP normaly the address of the router is sent. The exit nexthop
 * may be passed to the external neighbor if the neighbor and the exit nexthop
 * reside in the same subnet -- directly connected.
 */
struct nexthop_table {
	LIST_HEAD(, nexthop)	*nexthop_hashtbl;
	u_int32_t		 nexthop_hashmask;
} nexthoptable;

#define NEXTHOP_HASH(x)						\
	&nexthoptable.nexthop_hashtbl[ntohl((x.s_addr)) &	\
	    nexthoptable.nexthop_hashmask]

void
nexthop_init(u_int32_t hashsize)
{
	struct nexthop	*nh;
	u_int32_t	 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct nexthop_table));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&nexthoptable.nexthop_hashtbl[i]);

	nexthoptable.nexthop_hashmask = hs - 1;

	/* add dummy entry for connected networks */
	nh = nexthop_alloc();
	nh->state = NEXTHOP_REACH;
	nh->exit_nexthop.af = AF_INET;
	nh->exit_nexthop.v4.s_addr = INADDR_ANY;

	LIST_INSERT_HEAD(NEXTHOP_HASH(nh->exit_nexthop.v4), nh,
	    nexthop_l);

	memcpy(&nh->true_nexthop, &nh->exit_nexthop,
	    sizeof(nh->true_nexthop));
	nh->nexthop_netlen = 0;
	nh->nexthop_net.af = AF_INET;
	nh->nexthop_net.v4.s_addr = INADDR_ANY;

	nh->flags = NEXTHOP_ANNOUNCE;
}

void
nexthop_shutdown(void)
{
	struct in_addr	 addr;
	struct nexthop	*nh;
	u_int32_t	 i;

	/* remove the dummy entry for connected networks */
	addr.s_addr = INADDR_ANY;
	nh = nexthop_get(addr);
	if (nh != NULL) {
		if (!LIST_EMPTY(&nh->path_h))
			log_warnx("nexthop_free: free non-free announce node");
		LIST_REMOVE(nh, nexthop_l);
		nexthop_free(nh);
	}

	for (i = 0; i <= nexthoptable.nexthop_hashmask; i++)
		if (!LIST_EMPTY(&nexthoptable.nexthop_hashtbl[i]))
			log_warnx("nexthop_free: free non-free table");

	free(nexthoptable.nexthop_hashtbl);
}

void
nexthop_add(struct rde_aspath *asp)
{
	struct nexthop	*nh;

	RIB_STAT(nexthop_add);
	ENSURE(asp != NULL);

	if ((nh = asp->nexthop) == NULL)
		nh = nexthop_get(asp->flags.nexthop);
	if (nh == NULL) {
		nh = nexthop_alloc();
		nh->state = NEXTHOP_LOOKUP;
		nh->exit_nexthop.af = AF_INET;
		nh->exit_nexthop.v4 = asp->flags.nexthop;
		LIST_INSERT_HEAD(NEXTHOP_HASH(asp->flags.nexthop), nh,
		    nexthop_l);
		rde_send_nexthop(&nh->exit_nexthop, 1);
	}
	asp->nexthop = nh;
	LIST_INSERT_HEAD(&nh->path_h, asp, nexthop_l);
}

void
nexthop_remove(struct rde_aspath *asp)
{
	struct nexthop	*nh;

	RIB_STAT(nexthop_remove);
	ENSURE(asp != NULL);

	LIST_REMOVE(asp, nexthop_l);

	/* see if list is empty */
	nh = asp->nexthop;

	/* never remove the dummy announce entry */
	if (nh->flags & NEXTHOP_ANNOUNCE)
		return;

	if (LIST_EMPTY(&nh->path_h)) {
		LIST_REMOVE(nh, nexthop_l);
		rde_send_nexthop(&nh->exit_nexthop, 0);
		nexthop_free(nh);
	}
}

static struct nexthop *
nexthop_get(struct in_addr nexthop)
{
	struct nexthop	*nh;

	RIB_STAT(nexthop_get);

	LIST_FOREACH(nh, NEXTHOP_HASH(nexthop), nexthop_l) {
		if (nh->exit_nexthop.v4.s_addr == nexthop.s_addr)
			return nh;
	}
	return NULL;
}

void
nexthop_update(struct kroute_nexthop *msg)
{
	struct nexthop		*nh;
	struct rde_aspath	*asp;

	RIB_STAT(nexthop_update);

	nh = nexthop_get(msg->nexthop.v4);
	if (nh == NULL) {
		log_warnx("nexthop_update: non-existent nexthop");
		return;
	}
	ENSURE(nh->exit_nexthop.v4.s_addr == msg->nexthop.v4.s_addr);

	if (msg->valid)
		nh->state = NEXTHOP_REACH;
	else
		nh->state = NEXTHOP_UNREACH;

	if (msg->connected)
		memcpy(&nh->true_nexthop, &nh->exit_nexthop,
		    sizeof(nh->true_nexthop));
	else
		memcpy(&nh->true_nexthop, &msg->gateway,
		    sizeof(nh->true_nexthop));

	nh->nexthop_netlen = msg->kr.prefixlen;
	nh->nexthop_net.af = AF_INET;
	nh->nexthop_net.v4.s_addr = msg->kr.prefix.s_addr;

	if (msg->connected)
		nh->flags |= NEXTHOP_CONNECTED;

	if (rde_noevaluate())
		/*
		 * if the decision process is turned off there is no need
		 * for the aspath list walk.
		 */
		return;

	LIST_FOREACH(asp, &nh->path_h, nexthop_l) {
		path_updateall(asp, nh->state);
	}
}

static struct nexthop *
nexthop_alloc(void)
{
	struct nexthop *nh;

	RIB_STAT(nexthop_alloc);

	nh = calloc(1, sizeof(*nh));
	if (nh == NULL)
		fatal("nexthop_alloc");
	LIST_INIT(&nh->path_h);
	return nh;
}

static void
nexthop_free(struct nexthop *nh)
{
	RIB_STAT(nexthop_free);
	ENSURE(LIST_EMPTY(&nh->path_h));

	free(nh);
}

