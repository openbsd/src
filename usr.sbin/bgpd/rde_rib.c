/*	$OpenBSD: rde_rib.c,v 1.5 2003/12/22 06:42:19 deraadt Exp $ */

/*
 * Copyright (c) 2003 Claudio Jeker <claudio@openbsd.org>
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

#include <errno.h>
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
 * This is achieved by heavily linking the different parts toghether.
 */

struct rib_stats {
	u_int64_t	attr_copy;
	u_int64_t	aspath_create;
	u_int64_t	aspath_destroy;
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
	u_int64_t	nexthop_invalidate;
	u_int64_t	nexthop_validate;
	u_int64_t	nexthop_get;
	u_int64_t	nexthop_alloc;
	u_int64_t	nexthop_free;
} ribstats;
#define RIB_STAT(x)	(ribstats.x++)

/*
 * maximum number of perfixes we allow per prefix. The number should
 * be not to big and ensures only that the prefix count is propperly
 * increased and decreased. Only useful if ENSURE is active.
 */
#define MAX_PREFIX_PER_AS 1500

/* attribute specific functions */

int
attr_equal(struct attr_flags *a, struct attr_flags *b)
{
	/* astags not yet used */
	if (a->origin != b->origin ||
	    aspath_equal(a->aspath, b->aspath) == 0 ||
	    a->nexthop.s_addr != b->nexthop.s_addr ||
	    a->med != b->med ||
	    a->lpref != b->lpref ||
	    a->aggr_atm != b->aggr_atm ||
	    a->aggr_as != b->aggr_as ||
	    a->aggr_ip.s_addr != b->aggr_ip.s_addr)
		return 0;
	return 1;
}

void
attr_copy(struct attr_flags *t, struct attr_flags *s)
{
	/*
	 * first copy the full struct, then replace the path and tags with
	 * a own copy.
	 */
	memcpy(t, s, sizeof(struct attr_flags));
	/* XXX we could speed that a bit with a direct malloc, memcpy */
	t->aspath = aspath_create(s->aspath->data, s->aspath->hdr.len);
	t->astags = NULL;	/* XXX NOT YET */
}

u_int16_t
attr_length(struct attr_flags *attr)
{
	u_int16_t	alen, plen;

	alen = 4 /* origin */ + 7 /* nexthop */ + 7 /* lpref */;
	plen = aspath_length(attr->aspath);
	alen += 2 + plen + (plen > 255 ? 2 : 1);
	if (attr->med != 0)
		alen += 7;
	if (attr->aggr_atm == 1)
		alen += 3;
	if (attr->aggr_as != 0)
		alen += 9;

	return alen;
}

int
attr_dump(void *p, u_int16_t len, struct attr_flags *a)
{
	u_char		*buf = p;
	u_int32_t	 tmp32;
	u_int16_t	 tmp16;
	u_int16_t	 aslen, wlen = 0;

#define ATTR_WRITE(b, a, alen)				\
	do {						\
		if ((wlen + (alen)) > len)		\
			return (-1);			\
		memcpy((b) + wlen, (a), (alen));	\
		wlen += (alen);				\
	} while (0)
#define ATTR_WRITEB(b, c)				\
	do {						\
		if (wlen == len || (c) > 0xff)		\
			return (-1);			\
		(b)[wlen++] = (c);			\
	} while (0)

	/* origin */
	ATTR_WRITEB(buf, ATTR_ORIGIN_FLAGS);
	ATTR_WRITEB(buf, ATTR_ORIGIN);
	ATTR_WRITEB(buf, 1);
	ATTR_WRITEB(buf, a->origin);

	/* aspath */
	aslen = aspath_length(a->aspath);
	ATTR_WRITEB(buf, ATTR_TRANSITIVE | (aslen>255 ? ATTR_EXTLEN : 0));
	ATTR_WRITEB(buf, ATTR_ASPATH);
	if (aslen > 255) {
		tmp16 = htonl(aslen);
		ATTR_WRITE(buf, &tmp16, 4);
	} else
		ATTR_WRITEB(buf, aslen);
	ATTR_WRITE(buf, aspath_dump(a->aspath), aslen);

	/* nexthop */
	ATTR_WRITEB(buf, ATTR_NEXTHOP_FLAGS);
	ATTR_WRITEB(buf, ATTR_NEXTHOP);
	ATTR_WRITEB(buf, 4);
	ATTR_WRITE(buf, &a->nexthop, 4);	/* network byte order */

	/* MED */
	if (a->med != 0) {
		ATTR_WRITEB(buf, ATTR_MED_FLAGS);
		ATTR_WRITEB(buf, ATTR_MED);
		ATTR_WRITEB(buf, 4);
		tmp32 = htonl(a->med);
		ATTR_WRITE(buf, &tmp32, 4);
	}

	/* local preference */
	ATTR_WRITEB(buf, ATTR_LOCALPREF_FLAGS);
	ATTR_WRITEB(buf, ATTR_LOCALPREF);
	ATTR_WRITEB(buf, 4);
	tmp32 = htonl(a->lpref);
	ATTR_WRITE(buf, &tmp32, 4);

	/* atomic aggregate */
	if (a->aggr_atm == 1) {
		ATTR_WRITEB(buf, ATTR_ATOMIC_AGGREGATE_FLAGS);
		ATTR_WRITEB(buf, ATTR_ATOMIC_AGGREGATE);
		ATTR_WRITEB(buf, 0);
	}

	/* aggregator */
	if (a->aggr_as != 0) {
		ATTR_WRITEB(buf, ATTR_AGGREGATOR_FLAGS);
		ATTR_WRITEB(buf, ATTR_AGGREGATOR);
		ATTR_WRITEB(buf, 6);
		tmp16 = htons(a->aggr_as);
		ATTR_WRITE(buf, &tmp16, 2);
		ATTR_WRITE(buf, &a->aggr_ip, 4);	/* network byte order */
	}

	return wlen;
#undef ATTR_WRITEB
#undef ATTR_WRITE
}

/* aspath specific functions */

/* TODO
 * aspath loop detection (partialy done I think),
 * aspath regexp search,
 * aspath to string converter
 */
static u_int16_t	aspath_extract(void *, int);

/*
 * Extract the asnum out of the as segement at the specified position.
 * Direct access is not possible because of non-aligned reads.
 */
static u_int16_t
aspath_extract(void *seg, int pos)
{
	u_char		*ptr = seg;
	u_int16_t	 as = 0;

	ENSURE(0 <= pos && pos < 0xff);

	ptr += 2 + 2 * pos;
	as = *ptr++;
	as <<= 8;
	as |= *ptr;
	return as;
}

int
aspath_verify(void *data, u_int16_t len, u_int16_t myAS)
{
	u_int8_t	*seg = data;
	u_int16_t	 seg_size;
	u_int8_t	 i, seg_len, seg_type;

	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		if (seg_type != AS_SET && seg_type != AS_SEQUENCE) {
			return AS_ERR_TYPE;
		}
		seg_size = 2 + 2 * seg_len;

		if (seg_size > len)
			return AS_ERR_LEN;

		for (i = 0; i < seg_len; i++) {
			if (myAS == aspath_extract(seg, i))
				return AS_ERR_LOOP;
		}
	}
	return 0;	/* all OK */
}

struct aspath *
aspath_create(void *data, u_int16_t len)
{
	struct aspath	*aspath;

	RIB_STAT(aspath_create);

	/* The aspath must already have been checked for correctness. */
	aspath = malloc(ASPATH_HEADER_SIZE + len);
	if (aspath == NULL)
		fatal("aspath_create", errno);
	aspath->hdr.len = len;
	memcpy(aspath->data, data, len);

	aspath->hdr.as_cnt = aspath_count(aspath);

	return aspath;
}

void
aspath_destroy(struct aspath *aspath)
{
	RIB_STAT(aspath_destroy);
	/* currently there is only the aspath that needs to be freed */
	free(aspath);
}

u_char *
aspath_dump(struct aspath *aspath)
{
	return aspath->data;
}

u_int16_t
aspath_length(struct aspath *aspath)
{
	return aspath->hdr.len;
}

u_int16_t
aspath_count(struct aspath *aspath)
{
	u_int8_t	*seg;
	u_int16_t	 cnt, len, seg_size;
	u_int8_t	 seg_type, seg_len;

	cnt = 0;
	seg = aspath->data;
	for (len = aspath->hdr.len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		if (seg_type == AS_SET)
			cnt += 1;
		else
			cnt += seg_len;
	}
	return cnt;
}

u_int16_t
aspath_neighbour(struct aspath *aspath)
{
	/*
	 * Empty aspath is OK -- internal as route.
	 * But what is the neighbour? For now let's return 0 that
	 * should not break anything.
	 */

	if (aspath->hdr.len < 2)
		fatal("aspath_neighbour: aspath has no data", 0);

	if (aspath->data[1] > 0)
		return aspath_extract(aspath->data, 0);
	return 0;
}

#define AS_HASH_INITAL 8271

u_long
aspath_hash(struct aspath *aspath)
{
	u_int8_t	*seg;
	u_long		 hash;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len, seg_type;

	hash = AS_HASH_INITAL;
	seg = aspath->data;
	for (len = aspath->hdr.len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		ENSURE(seg_size <= len);
		for (i = 0; i < seg_len; i++) {
			hash += (hash << 5);
			hash ^= aspath_extract(seg, i);
		}
	}
	return hash;
}

int
aspath_equal(struct aspath *a1, struct aspath *a2)
{
	if (a1->hdr.len == a2->hdr.len &&
	    memcmp(a1->data, a2->data, a1->hdr.len) == 0)
		return 1;
	return 0;
}

/* path specific functions */

static void	path_link(struct rde_aspath *, struct rde_peer *);
static void	path_unlink(struct rde_aspath *);
static struct rde_aspath	*path_alloc(void);
static void	path_free(struct rde_aspath *);

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_long			 path_hashmask;
} pathtable;

#define PATH_HASH(x)				\
	&pathtable.path_hashtbl[aspath_hash((x)) & pathtable.path_hashmask]

void
path_init(u_long hashsize)
{
	u_long	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	pathtable.path_hashtbl = calloc(hs, sizeof(struct aspath_head));
	if (pathtable.path_hashtbl == NULL)
		fatal("path_init", errno);

	for (i = 0; i < hs; i++)
		LIST_INIT(&pathtable.path_hashtbl[i]);

	pathtable.path_hashmask = hs - 1;
}

void
path_update(struct rde_peer *peer, struct attr_flags *attrs,
    struct in_addr prefix, int prefixlen)
{
	struct rde_aspath	*asp;
	struct prefix		*p;
	struct pt_entry		*pte;

	RIB_STAT(path_update);

	if ((asp = path_get(attrs->aspath, peer)) == NULL) {
		asp = path_add(peer, attrs);
		pte = prefix_add(asp, prefix, prefixlen);
	} else {
		if (attr_equal(&asp->flags, attrs) == 0) {
			if ((p = prefix_get(asp,
			    prefix, prefixlen)) == NULL) {
				asp = path_add(peer, attrs);
				pte = prefix_add(asp, prefix, prefixlen);
			} else {
				asp = path_add(peer, attrs);
				pte = prefix_move(asp, p);
			}
		} else
			pte = prefix_add(asp, prefix, prefixlen);
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
		if (aspath_equal(asp->flags.aspath, aspath) &&
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

	attr_copy(&asp->flags, attr);

	path_link(asp, peer);
	return asp;
}

void
path_remove(struct rde_aspath *asp)
{
	struct prefix	*p, *np;

	RIB_STAT(path_remove);

	for (p = LIST_FIRST(&asp->prefix_h);
	    p != LIST_END(&asp->prefix_h);
	    p = np) {
		np = LIST_NEXT(p, path_l);
		prefix_destroy(p);
	}
	LIST_INIT(&asp->prefix_h);
	path_destroy(asp);
}

void
path_updateall(struct rde_aspath *asp, enum nexthop_state state)
{
	RIB_STAT(path_updateall);

	if (asp->state == state)
		return;		/* no need to redo it */
	asp->state = state;
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
 * peer_l: list of all aspathes that belong to that peer
 * path_l: hash list to find pathes quickly
 * nexthop_l: list of all aspathes with an equal exit nexthop
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

	/* free the aspath and astags */
	aspath_destroy(asp->flags.aspath);
	asp->flags.aspath = NULL;

	/*
	 * astags_destory(asp->flags.astags);
	 * asp->flags.astags = NULL;
	 */
}

/* alloc and initalize new entry. May not fail. */
static struct rde_aspath *
path_alloc(void)
{
	struct rde_aspath *asp;

	RIB_STAT(path_alloc);

	asp = calloc(1, sizeof(*asp));
	if (asp == NULL)
		fatal("path_alloc", errno);
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
	    asp->flags.astags == NULL);
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
prefix_get(struct rde_aspath *asp, struct in_addr prefix, int prefixlen)
{
	struct prefix	*p;

	RIB_STAT(prefix_get);
	ENSURE(asp != NULL);

	LIST_FOREACH(p, &asp->prefix_h, path_l) {
		ENSURE(p->prefix != NULL);
		if (p->prefix->prefixlen == prefixlen &&
		    p->prefix->prefix.s_addr == prefix.s_addr) {
			ENSURE(p->aspath == asp);
			return p;
		}
	}

	return NULL;
}

/*
 * Adds or updates a prefix. Returns 1 if a new routing decision needs
 * to be done -- which is acctually always.
 */
struct pt_entry *
prefix_add(struct rde_aspath *asp, struct in_addr prefix, int prefixlen)

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
	ENSURE(asp->peer == p->peer);

	/* create new prefix node */
	np = prefix_alloc();
	np->aspath = asp;
	np->prefix = p->prefix;
	np->peer = p->peer;
	np->lastchange = time(NULL);

	/* add to new as path */
	LIST_INSERT_HEAD(&asp->prefix_h, np, path_l);
	asp->prefix_cnt++;
	/* XXX for debugging */
	if (asp->prefix_cnt == MAX_PREFIX_PER_AS)
		logit(LOG_INFO, "RDE: prefix hog, prefix %#x/%d",
		    np->prefix->prefix.s_addr, np->prefix->prefixlen);
	ENSURE(asp->prefix_cnt < MAX_PREFIX_PER_AS);

	/*
	 * First kick the old prefix node out of the prefix list,
	 * afterwards run the route decision for new prefix node.
	 * Because of this only one update is generated if the prefix
	 * was active.
	 */
	LIST_REMOVE(p, prefix_l);
	prefix_evaluate(np, p->prefix);

	/* remove old prefix node */
	oasp = p->aspath;
	LIST_REMOVE(p, path_l);
	ENSURE(oasp->prefix_cnt > 0);
	oasp->prefix_cnt--;

	/* destroy all references to other objects and free the old prefix */
	p->aspath = NULL;
	p->prefix = NULL;
	p->peer = NULL;
	prefix_free(p);

	/* destroy old path if empty */
	if (path_empty(oasp))
		path_destroy(oasp); /* XXX probably use path_remove */

	return np->prefix;
}

/*
 * Removes a prefix from all lists. If the parent objects -- path or
 * pt_entry -- become empty remove them too.
 */
void
prefix_remove(struct rde_peer *peer, struct in_addr prefix, int prefixlen)
{
	struct prefix		*p;
	struct pt_entry		*pte;
	struct rde_aspath	*asp;

	RIB_STAT(prefix_remove);

	pte = pt_get(prefix, prefixlen);
	if (pte == NULL)	/* Got a dummy withdrawn request */
		return;

	p = prefix_bypeer(pte, peer);
	if (p == NULL)	/* Got a dummy withdrawn request. */
		return;

	asp = p->aspath;
	prefix_unlink(p);
	prefix_free(p);

	if (pt_empty(pte))
		pt_remove(pte);
	if (path_empty(asp))
		path_destroy(asp); /* XXX probably use path_remove */
}

/*
 * seraches in the prefix list of specified pt_entry for a prefix entry
 * belonging to the peer peer. Returns NULL if no match found.
 */
struct prefix *
prefix_bypeer(struct pt_entry *pte, struct rde_peer *peer)
{
	struct prefix	*p;

	ENSURE(pte != NULL);

	LIST_FOREACH(p, &pte->prefix_h, prefix_l) {
		if (p->peer == peer)
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

	LIST_FOREACH(p, &asp->prefix_h, path_l) {
		/* redo the route decision */
		LIST_REMOVE(p, prefix_l);
		prefix_evaluate(p, p->prefix);
	}
}

/* kill a prefix. Only called by path_remove. */
void
prefix_destroy(struct prefix *p)
{
	struct pt_entry		*pte;
	struct rde_aspath	*asp;

	asp = p->aspath;
	pte = p->prefix;
	prefix_unlink(p);
	prefix_free(p);

	if (pt_empty(pte))
		pt_remove(pte);
}

/*
 * Link a prefix into the different parent objects.
 */
static void
prefix_link(struct prefix *pref, struct pt_entry *pte, struct rde_aspath *asp)
{
	RIB_STAT(prefix_link);
	ENSURE(pref->aspath == NULL &&
	    pref->prefix == NULL &&
	    pref->peer == NULL);
	ENSURE(pref != NULL && pte != NULL && asp != NULL);
	ENSURE(prefix_bypeer(pte, asp->peer) == NULL);

	LIST_INSERT_HEAD(&asp->prefix_h, pref, path_l);
	asp->prefix_cnt++;

	/* XXX for debugging */
	if (asp->prefix_cnt == MAX_PREFIX_PER_AS)
		logit(LOG_INFO, "RDE: prefix hog, prefix %#x/%d",
		    pte->prefix.s_addr, pte->prefixlen);
	ENSURE(asp->prefix_cnt < MAX_PREFIX_PER_AS);

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
	pref->aspath->prefix_cnt--;

	/* destroy all references to other objects */
	pref->aspath = NULL;
	pref->prefix = NULL;
	pref->peer = NULL;

	/*
	 * It's the caller duty to remove empty aspath respectivly pt_entry
	 * structures. Also freeing the unlinked prefix is callers duty.
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
		fatal("prefix_alloc", errno);
	return p;
}

/* free a unlinked entry */
static void
prefix_free(struct prefix *pref)
{
	RIB_STAT(prefix_free);
	ENSURE(pref->aspath == NULL &&
	    pref->prefix == NULL &&
	    pref->peer == NULL);
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
 * hash table has more benefits and the table walk shoulds not happen to often.
 */

static struct nexthop	*nexthop_get(struct in_addr);
static void		 nexthop_updateall(struct in_addr, int,
			    enum nexthop_state);
static inline void	 nexthop_update(struct nexthop *, enum nexthop_state);
static struct nexthop	*nexthop_alloc(void);
static void		 nexthop_free(struct nexthop *);

struct nexthop {
	enum nexthop_state	state;
#if 0
	u_int32_t		costs;
#endif
	struct aspath_head	path_h;
	struct in_addr		nexthop;
	LIST_ENTRY(nexthop)	nexthop_l;
};

struct nexthop_table {
	LIST_HEAD(, nexthop)	*nexthop_hashtbl;
	u_long			 nexthop_hashmask;
} nexthoptable;

#define NEXTHOP_HASH(x)					\
	&nexthoptable.nexthop_hashtbl[x.s_addr & nexthoptable.nexthop_hashmask]

void
nexthop_init(u_long hashsize)
{
	u_long hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct aspath_head));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init", errno);

	for (i = 0; i < hs; i++)
		LIST_INIT(&nexthoptable.nexthop_hashtbl[i]);

	nexthoptable.nexthop_hashmask = hs - 1;
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
		/*
		 * XXX nexthop_lookup()
		 * currently I assume that the nexthop is reachable.
		 * Getting that info could end with a big pain in the ass.
		 */
		nh->state = NEXTHOP_REACH;
		nh->nexthop = asp->flags.nexthop;
		LIST_INSERT_HEAD(NEXTHOP_HASH(asp->flags.nexthop), nh,
		    nexthop_l);
	}
	asp->nexthop = nh;
	asp->state = nh->state;
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

	if (LIST_EMPTY(&nh->path_h)) {
		LIST_REMOVE(nh, nexthop_l);
		nexthop_free(nh);
	}
}

void
nexthop_invalidate(struct in_addr prefix, int prefixlen)
{
	RIB_STAT(nexthop_invalidate);

	nexthop_updateall(prefix, prefixlen, NEXTHOP_UNREACH);
}

void
nexthop_validate(struct in_addr prefix, int prefixlen)
{
	RIB_STAT(nexthop_validate);

	nexthop_updateall(prefix, prefixlen, NEXTHOP_REACH);
}

static struct nexthop *
nexthop_get(struct in_addr nexthop)
{
	struct nexthop	*nh;

	RIB_STAT(nexthop_get);

	LIST_FOREACH(nh, NEXTHOP_HASH(nexthop), nexthop_l) {
		if (nh->nexthop.s_addr == nexthop.s_addr)
			return nh;
	}
	return NULL;
}

static void
nexthop_updateall(struct in_addr prefix, int prefixlen,
    enum nexthop_state state)
{
	struct nexthop	*nh;
	u_long		 ul;

	/* XXX probably I get shot for this code ... (: */
	prefix.s_addr >>= (32-prefixlen);

	for (ul = nexthoptable.nexthop_hashmask; ul >= 0; ul--) {
		LIST_FOREACH(nh, &nexthoptable.nexthop_hashtbl[ul], nexthop_l) {
			if (prefix.s_addr ==
			    nh->nexthop.s_addr >> (32-prefixlen)) {
				nh->state = state;
				nexthop_update(nh, state);
			}
		}
	}
}

static inline void
nexthop_update(struct nexthop *nh, enum nexthop_state mode)
{
	struct rde_aspath	*asp;

	LIST_FOREACH(asp, &nh->path_h, nexthop_l) {
		path_updateall(asp, mode);
	}
}

static struct nexthop *
nexthop_alloc(void)
{
	struct nexthop *nh;

	RIB_STAT(nexthop_alloc);

	nh = calloc(1, sizeof(*nh));
	if (nh == NULL)
		fatal("nexthop_alloc", errno);
	return nh;
}

static void
nexthop_free(struct nexthop *nh)
{
	RIB_STAT(nexthop_free);
	ENSURE(LIST_EMPTY(&nh->path_h));

	free(nh);
}

