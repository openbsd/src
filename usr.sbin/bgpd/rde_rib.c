/*	$OpenBSD: rde_rib.c,v 1.77 2006/01/12 14:05:13 claudio Exp $ */

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
#include <sys/hash.h>

#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

/*
 * BGP RIB -- Routing Information Base
 *
 * The RIB is build with one aspect in mind. Speed -- actually update speed.
 * Therefor one thing needs to be absolutely avoided, long table walks.
 * This is achieved by heavily linking the different parts together.
 */

/* path specific functions */

static void	path_link(struct rde_aspath *, struct rde_peer *);

struct path_table pathtable;

/* XXX the hash should also include communities and the other attrs */
#define PATH_HASH(x)				\
	&pathtable.path_hashtbl[hash32_buf((x)->data, (x)->len, HASHINIT) & \
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
path_update(struct rde_peer *peer, struct rde_aspath *nasp,
    struct bgpd_addr *prefix, int prefixlen)
{
	struct rde_aspath	*asp;
	struct prefix		*p;

	rde_send_pftable(nasp->pftableid, prefix, prefixlen, 0);
	rde_send_pftable_commit();

	if ((p = prefix_get(peer, prefix, prefixlen)) != NULL) {
		if (path_compare(nasp, p->aspath) == 0) {
			/* update last change */
			p->lastchange = time(NULL);
			return;
		}
	}

	/*
	 * Either the prefix does not exist or the path changed.
	 * In both cases lookup the new aspath to make sure it is not
	 * already in the RIB.
	 */
	if ((asp = path_lookup(nasp, peer)) == NULL) {
		/* path not available, create and link new one */
		asp = path_copy(nasp);
		path_link(asp, peer);
	}

	/* if the prefix was found move it else add it to the aspath */
	if (p != NULL)
		prefix_move(asp, p);
	else
		prefix_add(asp, prefix, prefixlen);
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
	struct prefix	*p;
	struct bgpd_addr addr;

	while ((p = LIST_FIRST(&asp->prefix_h)) != NULL) {
		/* Commit is done in peer_down() */
		pt_getaddr(p->prefix, &addr);
		rde_send_pftable(p->aspath->pftableid,
		    &addr, p->prefix->prefixlen, 1);

		prefix_destroy(p);
	}
	path_destroy(asp);
}

void
path_updateall(struct rde_aspath *asp, enum nexthop_state state)
{
	if (rde_noevaluate())
		/* if the decision process is turned off this is a no-op */
		return;

	prefix_updateall(asp, state);
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
 * copy asp to a new UNLINKED one manly for filtering
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

/* free a unlinked element */
void
path_put(struct rde_aspath *asp)
{
	if (asp == NULL)
		return;

	if (asp->flags & F_ATTR_LINKED)
		fatalx("path_put: linked object");

	rtlabel_unref(asp->rtlabelid);
	aspath_put(asp->aspath);
	attr_freeall(asp);
	rdemem.path_cnt--;
	free(asp);
}

/* prefix specific functions */

static struct prefix	*prefix_alloc(void);
static void		 prefix_free(struct prefix *);
static void		 prefix_link(struct prefix *, struct pt_entry *,
			     struct rde_aspath *);
static void		 prefix_unlink(struct prefix *);

int
prefix_compare(const struct bgpd_addr *a, const struct bgpd_addr *b,
    int prefixlen)
{
	in_addr_t	mask, aa, ba;
	int		i;
	u_int8_t	m;

	if (a->af != b->af)
		return (a->af - b->af);

	switch (a->af) {
	case AF_INET:
		if (prefixlen > 32)
			fatalx("prefix_cmp: bad IPv4 prefixlen");
		mask = htonl(prefixlen2mask(prefixlen));
		aa = ntohl(a->v4.s_addr & mask);
		ba = ntohl(b->v4.s_addr & mask);
		if (aa != ba)
			return (aa - ba);
		return (0);
	case AF_INET6:
		for (i = 0; i < prefixlen / 8; i++)
			if (a->v6.s6_addr[i] != b->v6.s6_addr[i])
				return (a->v6.s6_addr[i] - b->v6.s6_addr[i]);
		i = prefixlen % 8;
		if (i) {
			m = 0xff00 >> i;
			if ((a->v6.s6_addr[prefixlen / 8] & m) !=
			    (b->v6.s6_addr[prefixlen / 8] & m))
				return ((a->v6.s6_addr[prefixlen / 8] & m) -
				    (b->v6.s6_addr[prefixlen / 8] & m));
		}
		return (0);
	default:
		fatalx("prefix_cmp: unknown af");
	}
	return (-1);
}

/*
 * search for specified prefix of a peer. Returns NULL if not found.
 */
struct prefix *
prefix_get(struct rde_peer *peer, struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_entry	*pte;

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)
		return (NULL);
	return (prefix_bypeer(pte, peer));
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

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)
		pte = pt_add(prefix, prefixlen);

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

	if (asp->peer != p->aspath->peer)
		fatalx("prefix_move: cross peer move");

	/* create new prefix node */
	np = prefix_alloc();
	np->aspath = asp;
	/* peer and prefix pointers are still equal */
	np->prefix = p->prefix;
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

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)	/* Got a dummy withdrawn request */
		return;

	p = prefix_bypeer(pte, peer);
	if (p == NULL)		/* Got a dummy withdrawn request. */
		return;

	asp = p->aspath;

	rde_send_pftable(asp->pftableid, prefix, prefixlen, 1);
	rde_send_pftable_commit();

	prefix_unlink(p);
	prefix_free(p);

	if (pt_empty(pte))
		pt_remove(pte);
	if (path_empty(asp))
		path_destroy(asp);
}

/* dump a prefix into specified buffer */
int
prefix_write(u_char *buf, int len, struct bgpd_addr *prefix, u_int8_t plen)
{
	int	totlen;

	if (prefix->af != AF_INET && prefix->af != AF_INET6)
		return (-1);

	totlen = PREFIX_SIZE(plen);

	if (totlen > len)
		return (-1);
	*buf++ = plen;
	memcpy(buf, &prefix->ba, totlen - 1);
	return (totlen);
}

/*
 * Searches in the prefix list of specified pt_entry for a prefix entry
 * belonging to the peer peer. Returns NULL if no match found.
 */
struct prefix *
prefix_bypeer(struct pt_entry *pte, struct rde_peer *peer)
{
	struct prefix	*p;

	LIST_FOREACH(p, &pte->prefix_h, prefix_l) {
		if (p->aspath->peer == peer)
			return (p);
	}
	return (NULL);
}

void
prefix_updateall(struct rde_aspath *asp, enum nexthop_state state)
{
	struct prefix	*p;

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
prefix_link(struct prefix *pref, struct pt_entry *pte, struct rde_aspath *asp)
{
	LIST_INSERT_HEAD(&asp->prefix_h, pref, path_l);
	asp->prefix_cnt++;
	asp->peer->prefix_cnt++;

	pref->aspath = asp;
	pref->prefix = pte;
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
	/* make route decision */
	LIST_REMOVE(pref, prefix_l);
	prefix_evaluate(NULL, pref->prefix);

	LIST_REMOVE(pref, path_l);
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
 * while in EBGP normaly the address of the router is sent. The exit nexthop
 * may be passed to the external neighbor if the neighbor and the exit nexthop
 * reside in the same subnet -- directly connected.
 */
struct nexthop_table {
	LIST_HEAD(nexthop_head, nexthop)	*nexthop_hashtbl;
	u_int32_t				 nexthop_hashmask;
} nexthoptable;

void
nexthop_init(u_int32_t hashsize)
{
	u_int32_t	 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct nexthop_table));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&nexthoptable.nexthop_hashtbl[i]);

	nexthoptable.nexthop_hashmask = hs - 1;
}

void
nexthop_shutdown(void)
{
	u_int32_t		 i;

	for (i = 0; i <= nexthoptable.nexthop_hashmask; i++)
		if (!LIST_EMPTY(&nexthoptable.nexthop_hashtbl[i]))
			log_warnx("nexthop_shutdown: non-free table");

	free(nexthoptable.nexthop_hashtbl);
}

void
nexthop_update(struct kroute_nexthop *msg)
{
	struct nexthop		*nh;
	struct rde_aspath	*asp;

	nh = nexthop_lookup(&msg->nexthop);
	if (nh == NULL) {
		log_warnx("nexthop_update: non-existent nexthop");
		return;
	}

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

	switch (msg->nexthop.af) {
	case AF_INET:
		nh->nexthop_netlen = msg->kr.kr4.prefixlen;
		nh->nexthop_net.af = AF_INET;
		nh->nexthop_net.v4.s_addr = msg->kr.kr4.prefix.s_addr;
		break;
	case AF_INET6:
		nh->nexthop_netlen = msg->kr.kr6.prefixlen;
		nh->nexthop_net.af = AF_INET6;
		memcpy(&nh->nexthop_net.v6, &msg->kr.kr6.prefix,
		    sizeof(struct in6_addr));
		break;
	default:
		fatalx("nexthop_update: unknown af");
	}

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

void
nexthop_modify(struct rde_aspath *asp, struct bgpd_addr *nexthop,
    enum action_types type, sa_family_t af)
{
	struct nexthop	*nh;

	if (type == ACTION_SET_NEXTHOP_REJECT) {
		asp->flags |= F_NEXTHOP_REJECT;
		return;
	}
	if (type  == ACTION_SET_NEXTHOP_BLACKHOLE) {
		asp->flags |= F_NEXTHOP_BLACKHOLE;
		return;
	}
	if (type == ACTION_SET_NEXTHOP_NOMODIFY) {
		asp->flags |= F_NEXTHOP_NOMODIFY;
		return;
	}
	if (af != nexthop->af)
		return;

	nh = nexthop_get(nexthop);
	if (asp->flags & F_ATTR_LINKED)
		nexthop_unlink(asp);
	asp->nexthop = nh;
	if (asp->flags & F_ATTR_LINKED)
		nexthop_link(asp);
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

	if (LIST_EMPTY(&nh->path_h)) {
		LIST_REMOVE(nh, nexthop_l);
		rde_send_nexthop(&nh->exit_nexthop, 0);

		rdemem.nexthop_cnt--;
		free(nh);
	}
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

	if (na == NULL && nb == NULL)
		return (0);
	if (na == NULL)
		return (-1);
	if (nb == NULL)
		return (1);

	a = &na->exit_nexthop;
	b = &nb->exit_nexthop;

	if (a->af != b->af)
		return (a->af - b->af);

	switch (a->af) {
	case AF_INET:
		if (ntohl(a->v4.s_addr) > ntohl(b->v4.s_addr))
			return (1);
		if (ntohl(a->v4.s_addr) < ntohl(b->v4.s_addr))
			return (-1);
		return (0);
	case AF_INET6:
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

	switch (nexthop->af) {
	case AF_INET:
		h = (AF_INET ^ ntohl(nexthop->v4.s_addr) ^
		    ntohl(nexthop->v4.s_addr) >> 13) &
		    nexthoptable.nexthop_hashmask;
		break;
	case AF_INET6:
		h = hash32_buf(nexthop->v6.s6_addr, sizeof(struct in6_addr),
		    HASHINIT) & nexthoptable.nexthop_hashmask;
		break;
	default:
		fatalx("nexthop_hash: unsupported AF");
	}
	return (&nexthoptable.nexthop_hashtbl[h]);
}

