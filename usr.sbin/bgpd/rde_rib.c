/*	$OpenBSD: rde_rib.c,v 1.24 2004/01/13 16:08:04 claudio Exp $ */

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
 * This is achieved by heavily linking the different parts together.
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
	u_int64_t	nexthop_update;
	u_int64_t	nexthop_get;
	u_int64_t	nexthop_alloc;
	u_int64_t	nexthop_free;
} ribstats;
#define RIB_STAT(x)	(ribstats.x++)

/*
 * Maximum number of prefixes we allow per prefix. The number should
 * not be too big and ensure only that the prefix count is properly
 * increased and decreased. Only useful if ENSURE is active.
 */
#define MAX_PREFIX_PER_AS 1500

/* attribute specific functions */
void	attr_optfree(struct attr_flags *);

int
attr_compare(struct attr_flags *a, struct attr_flags *b)
{
	struct attr	*oa, *ob;
	int		 r;

	if (a->origin > b->origin)
		return (1);
	if (a->origin < b->origin)
		return (-1);
	if (a->nexthop > b->nexthop)
		return (1);
	if (a->nexthop < b->nexthop)
		return (-1);
	if (a->med > b->med)
		return (1);
	if (a->med < b->med)
		return (-1);
	if (a->lpref > b->lpref)
		return (1);
	if (a->lpref < b->lpref)
		return (-1);
	r = aspath_compare(a->aspath, b->aspath);
	if (r > 0)
		return (1);
	if (r < 0)
		return (-1);

	for (oa = TAILQ_FIRST(&a->others), ob = TAILQ_FIRST(&b->others);
	    oa != TAILQ_END(&a->others) && ob != TAILQ_END(&a->others);
	    oa = TAILQ_NEXT(oa, attr_l), ob = TAILQ_NEXT(ob, attr_l)) {
		if (oa->type > ob->type)
			return (1);
		if (oa->type < ob->type)
			return (-1);
		if (oa->len > ob->len)
			return (1);
		if (oa->len < ob->len)
			return (-1);
		r = memcmp(oa->data, ob->data, oa->len);
		if (r > 0)
			return (1);
		if (r < 0)
			return (-1);
	}
	if (oa != TAILQ_END(&a->others))
		return (1);
	if (ob != TAILQ_END(&a->others))
		return (-1);
	return (0);
}

void
attr_copy(struct attr_flags *t, struct attr_flags *s)
{
	struct attr	*os;
	/*
	 * first copy the full struct, then replace the path and tags with
	 * a own copy.
	 */
	memcpy(t, s, sizeof(struct attr_flags));
	t->aspath = aspath_create(s->aspath->data, s->aspath->hdr.len);
	TAILQ_INIT(&t->others);
	TAILQ_FOREACH(os, &s->others, attr_l)
		attr_optadd(t, os->flags, os->type, os->data, os->len);
}

int
attr_write(void *p, u_int16_t p_len, u_int8_t flags, u_int8_t type,
    void *data, u_int16_t data_len)
{
	u_char		*b = p;
	u_int16_t	 tmp, tot_len = 2; /* attribute header (without len) */

	if (data_len > 255) {
		tot_len += 2 + data_len;
		flags |= ATTR_EXTLEN;
	} else
		tot_len += 1 + data_len;

	if (tot_len > p_len)
		return (-1);

	*b++ = flags;
	*b++ = type;
	if (data_len > 255) {
		tmp = htons(data_len);
		memcpy(b, &tmp, 2);
		b += 2;
	} else
		*b++ = (u_char)(data_len & 0xff);

	if (data_len != 0)
		memcpy(b, data, data_len);

	return (tot_len);
}

void
attr_optadd(struct attr_flags *attr, u_int8_t flags, u_int8_t type,
    u_char *data, u_int16_t len)
{
	struct attr	*a, *p;

	if (flags & ATTR_OPTIONAL && ! flags & ATTR_TRANSITIVE)
		/*
		 * We already know that we're not intrested in this attribute.
		 * Currently only the MED is optional and non-transitive but
		 * MED is directly stored in struct attr_flags.
		 */
		return;

	a = calloc(1, sizeof(struct attr));
	if (a == NULL)
		fatal("attr_optadd");
	a->flags = flags;
	a->type = type;
	a->len = len;
	if (len != 0) {
		a->data = malloc(len);
		if (a->data == NULL)
			fatal("attr_optadd");
		memcpy(a->data, data, len);
	}
	/* keep a sorted list */
	TAILQ_FOREACH_REVERSE(p, &attr->others, attr_l, attr_list) {
		if (type > p->type) {
			TAILQ_INSERT_AFTER(&attr->others, p, a, attr_l);
			return;
		}
		ENSURE(type != p->type);
	}
}

void
attr_optfree(struct attr_flags *attr)
{
	struct attr	*a, *xa;

	for (a = TAILQ_FIRST(&attr->others); a != TAILQ_END(&attr->others);
	    a = xa) {
		xa = TAILQ_NEXT(a, attr_l);
		free(a->data);
		free(a);
	}
}

/* aspath specific functions */

/* TODO
 * aspath loop detection (partially done I think),
 * aspath regexp search,
 * aspath to string converter
 */
static u_int16_t	aspath_extract(void *, int);

/*
 * Extract the asnum out of the as segment at the specified position.
 * Direct access is not possible because of non-aligned reads.
 * ATTENTION: no bounds check are done.
 */
static u_int16_t
aspath_extract(void *seg, int pos)
{
	u_char		*ptr = seg;
	u_int16_t	 as = 0;

	ENSURE(0 <= pos && pos < 255);

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

		if (seg_size == 0)
			/* empty aspath segment are not allowed */
			return AS_ERR_BAD;

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
		fatal("aspath_create");
	aspath->hdr.len = len;
	memcpy(aspath->data, data, len);

	aspath->hdr.as_cnt = aspath_count(aspath);

	return aspath;
}

int
aspath_write(void *p, u_int16_t len, struct aspath *aspath, u_int16_t myAS,
    int prepend)
{
	u_char		*b = p;
	int		 tot_len, as_len, size, wpos = 0;
	u_int16_t	 tmp;
	u_int8_t	 type, attr_flag = ATTR_WELL_KNOWN;

	if (prepend > 255)
		/* lunatic prepends need to be blocked in the parser */
		return (-1);

	/* first calculate new size */
	if (aspath->hdr.len > 0) {
		ENSURE(aspath->hdr.len > 2);
		type = aspath->data[0];
		size = aspath->data[1];
	} else {
		/* empty as path */
		type = AS_SET;
		size = 0;
	}
	if (prepend == 0)
		as_len = aspath->hdr.len;
	else if (type == AS_SET || size + prepend > 255)
		/* need to attach a new AS_SEQUENCE */
		as_len = 2 + prepend * 2 + aspath->hdr.len;
	else
		as_len = prepend * 2 + aspath->hdr.len;

	/* check buffer size */
	tot_len = 2 + as_len;
	if (as_len > 255) {
		attr_flag |= ATTR_EXTLEN;
		tot_len += 2;
	} else
		tot_len += 1;

	if (tot_len > len)
		return (-1);

	/* header */
	b[wpos++] = attr_flag;
	b[wpos++] = ATTR_ASPATH;
	if (as_len > 255) {
		tmp = as_len;
		tmp = htons(tmp);
		memcpy(b, &tmp, 2);
		wpos += 2;
	} else
		b[wpos++] = (u_char)(as_len & 0xff);

	/* first prepends */
	myAS = htons(myAS);
	if (type == AS_SET) {
		b[wpos++] = AS_SEQUENCE;
		b[wpos++] = prepend;
		for (; prepend > 0; prepend--) {
			memcpy(b + wpos, &myAS, 2);
			wpos += 2;
		}
		memcpy(b + wpos, aspath->data, aspath->hdr.len);
	} else {
		if (size + prepend > 255) {
			b[wpos++] = AS_SEQUENCE;
			b[wpos++] = size + prepend - 255;
			for (; prepend + size > 255; prepend--) {
				memcpy(b + wpos, &myAS, 2);
				wpos += 2;
			}
		}
		b[wpos++] = AS_SEQUENCE;
		b[wpos++] = size + prepend;
		for (; prepend > 0; prepend--) {
			memcpy(b + wpos, &myAS, 2);
			wpos += 2;
		}
		memcpy(b + wpos, aspath->data + 2, aspath->hdr.len - 2);
	}
	return (tot_len);
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

	if (aspath->hdr.len == 0)
		return 0;

	ENSURE(aspath->hdr.len > 2);
	return aspath_extract(aspath->data, 0);
}

#define AS_HASH_INITIAL 8271

u_long
aspath_hash(struct aspath *aspath)
{
	u_int8_t	*seg;
	u_long		 hash;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len, seg_type;

	hash = AS_HASH_INITIAL;
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
aspath_compare(struct aspath *a1, struct aspath *a2)
{
	int r;

	if (a1->hdr.len > a2->hdr.len)
		return (1);
	if (a1->hdr.len < a2->hdr.len)
		return (-1);
	r = memcmp(a1->data, a2->data, a1->hdr.len);
	if (r > 0)
		return (1);
	if (r < 0)
		return (-1);
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
		fatal("path_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&pathtable.path_hashtbl[i]);

	pathtable.path_hashmask = hs - 1;
}

void
path_update(struct rde_peer *peer, struct attr_flags *attrs,
    struct bgpd_addr *prefix, int prefixlen)
{
	struct rde_aspath	*asp;
	struct prefix		*p;
	struct pt_entry		*pte;

	RIB_STAT(path_update);

	if ((asp = path_get(attrs->aspath, peer)) == NULL) {
		/* path not available */
		asp = path_add(peer, attrs);
		pte = prefix_add(asp, prefix, prefixlen);
	} else {
		if (attr_compare(&asp->flags, attrs) == 0)
			/* path are equal, just add prefix */
			pte = prefix_add(asp, prefix, prefixlen);
		else {
			/* non equal path attributes create new path */
			if ((p = prefix_get(asp,
			    prefix, prefixlen)) == NULL) {
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

	/* free the aspath and all other path attributes */
	aspath_destroy(asp->flags.aspath);
	asp->flags.aspath = NULL;
	attr_optfree(&asp->flags);
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
 * Adds or updates a prefix. Returns 1 if a new routing decision needs
 * to be done -- which is actually always.
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
	memcpy(&np->prefix, &p->prefix, sizeof(np->prefix));
	np->peer = p->peer;
	np->lastchange = time(NULL);

	/* add to new as path */
	LIST_INSERT_HEAD(&asp->prefix_h, np, path_l);
	asp->prefix_cnt++;
	asp->peer->prefix_cnt++;
	/* XXX for debugging */
	if (asp->prefix_cnt == MAX_PREFIX_PER_AS)
		logit(LOG_INFO, "RDE: prefix hog, prefix %s/%d",
		    inet_ntoa(np->prefix->prefix.v4), np->prefix->prefixlen);
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
	ENSURE(oasp->peer->prefix_cnt > 0);
	oasp->prefix_cnt--;
	oasp->peer->prefix_cnt--;

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
 * Searches in the prefix list of specified pt_entry for a prefix entry
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
	asp->peer->prefix_cnt++;

	/* XXX for debugging */
	if (asp->prefix_cnt == MAX_PREFIX_PER_AS)
		logit(LOG_INFO, "RDE: prefix hog, prefix %s/%d",
		    inet_ntoa(pte->prefix.v4), pte->prefixlen);
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
	ENSURE(pref->aspath->peer->prefix_cnt > 0);
	pref->aspath->prefix_cnt--;
	pref->aspath->peer->prefix_cnt--;

	/* destroy all references to other objects */
	pref->aspath = NULL;
	pref->prefix = NULL;
	pref->peer = NULL;

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
 * hash table has more benefits and the table walk should not happen too often.
 */

static struct nexthop	*nexthop_get(in_addr_t);
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
	u_long			 nexthop_hashmask;
} nexthoptable;

#define NEXTHOP_HASH(x)					\
	&nexthoptable.nexthop_hashtbl[ntohl((x)) &	\
	    nexthoptable.nexthop_hashmask]

void
nexthop_init(u_long hashsize)
{
	u_long hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	nexthoptable.nexthop_hashtbl = calloc(hs, sizeof(struct aspath_head));
	if (nexthoptable.nexthop_hashtbl == NULL)
		fatal("nextop_init");

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
		nh->state = NEXTHOP_LOOKUP;
		nh->exit_nexthop.af = AF_INET;
		nh->exit_nexthop.v4.s_addr = asp->flags.nexthop;
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

	if (LIST_EMPTY(&nh->path_h)) {
		LIST_REMOVE(nh, nexthop_l);
		rde_send_nexthop(&nh->exit_nexthop, 0);
		nexthop_free(nh);
	}
}

static struct nexthop *
nexthop_get(in_addr_t nexthop)
{
	struct nexthop	*nh;

	RIB_STAT(nexthop_get);

	LIST_FOREACH(nh, NEXTHOP_HASH(nexthop), nexthop_l) {
		if (nh->exit_nexthop.v4.s_addr == nexthop)
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

	nh = nexthop_get(msg->nexthop.v4.s_addr);
	if (nh == NULL) {
		logit(LOG_INFO, "nexthop_update: non-existent nexthop");
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
	nh->nexthop_net.v4.s_addr = msg->kr.prefix;

	nh->connected = msg->connected;

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

