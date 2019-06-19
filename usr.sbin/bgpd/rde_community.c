/*	$OpenBSD: rde_community.c,v 1.1 2019/06/17 11:02:19 claudio Exp $ */

/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>

#include <endian.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

static int
apply_flag(u_int32_t in, u_int8_t flag, struct rde_peer *peer, u_int32_t *out,
    u_int32_t *mask)
{
	switch (flag) {
	case COMMUNITY_ANY:
		if (mask == NULL)
			return -1;
		*out = 0;
		*mask = 0;
		return 0;
	case COMMUNITY_NEIGHBOR_AS:
		if (peer == NULL)
			return -1;
		*out = peer->conf.remote_as;
		break;
	case COMMUNITY_LOCAL_AS:
		if (peer == NULL)
			return -1;
		*out = peer->conf.local_as;
		break;
	default:
		*out = in;
		break;
	}
	if (mask)
		*mask = UINT32_MAX;
	return 0;
}

static int
fc2c(struct community *fc, struct rde_peer *peer, struct community *c,
    struct community *m)
{
	short type;
	u_int8_t subtype;

	memset(c, 0, sizeof(*c));
	if (m)
		memset(m, 0xff, sizeof(*m));

	c->flags = (u_int8_t)fc->flags;

	switch ((u_int8_t)c->flags) {
	case COMMUNITY_TYPE_BASIC:
		if (apply_flag(fc->data1, fc->flags >> 8, peer,
		    &c->data1, m ? &m->data1 : NULL))
			return -1;
		if (apply_flag(fc->data2, fc->flags >> 16, peer,
		    &c->data2, m ? &m->data2 : NULL))
			return -1;

		/* check that values fit */
		if (c->data1 > USHRT_MAX || c->data2 > USHRT_MAX)
			return -1;
		return 0;
	case COMMUNITY_TYPE_LARGE:
		if (apply_flag(fc->data1, fc->flags >> 8, peer,
		    &c->data1, m ? &m->data1 : NULL))
			return -1;
		if (apply_flag(fc->data2, fc->flags >> 16, peer,
		    &c->data2, m ? &m->data2 : NULL))
			return -1;
		if (apply_flag(fc->data3, fc->flags >> 24, peer,
		    &c->data3, m ? &m->data3 : NULL))
			return -1;
		return 0;
	case COMMUNITY_TYPE_EXT:
		type = (int32_t)fc->data3 >> 8;
		subtype = fc->data3 & 0xff;

		c->data3 = type << 8 | subtype;
		switch (type) {
		case -1:
			/* special case for 'ext-community rt *' */
			if ((fc->flags >> 8 & 0xff) != COMMUNITY_ANY ||
			    m == NULL)
				return -1;
			c->data3 = subtype;
			m->data1 = 0;
			m->data2 = 0;
			m->data3 = 0xff;
			return 0;
		case EXT_COMMUNITY_TRANS_TWO_AS:
			if ((fc->flags >> 8 & 0xff) == COMMUNITY_ANY)
				break;

			if (apply_flag(fc->data1, fc->flags >> 8, peer,
			    &c->data1, m ? &m->data1 : NULL))
				return -1;
			if (apply_flag(fc->data2, fc->flags >> 16, peer,
			    &c->data2, m ? &m->data2 : NULL))
				return -1;
			/* check that values fit */
			if (c->data1 > USHRT_MAX)
				return -1;
			return 0;
		case EXT_COMMUNITY_TRANS_FOUR_AS:
		case EXT_COMMUNITY_TRANS_IPV4:
			if ((fc->flags >> 8 & 0xff) == COMMUNITY_ANY)
				break;

			if (apply_flag(fc->data1, fc->flags >> 8, peer,
			    &c->data1, m ? &m->data1 : NULL))
				return -1;
			if (apply_flag(fc->data2, fc->flags >> 16, peer,
			    &c->data2, m ? &m->data2 : NULL))
				return -1;
			/* check that values fit */
			if (c->data2 > USHRT_MAX)
				return -1;
			return 0;
		case EXT_COMMUNITY_TRANS_OPAQUE:
		case EXT_COMMUNITY_TRANS_EVPN:
		case EXT_COMMUNITY_NON_TRANS_OPAQUE:
			if ((fc->flags >> 8 & 0xff) == COMMUNITY_ANY)
				break;

			c->data1 = fc->data1;
			c->data2 = fc->data2;
			return 0;;
		}

		if (m) {
			m->data1 = 0;
			m->data2 = 0;
		}
		return 0;
	default:
		fatalx("%s: unknown type %d", __func__, (u_int8_t)c->flags);
	}
}

static int
fast_match(const void *va, const void *vb)
{
	const struct community *a = va;
	const struct community *b = vb;

	if ((u_int8_t)a->flags != (u_int8_t)b->flags)
		return (u_int8_t)a->flags > (u_int8_t)b->flags ? 1 : -1;

	if (a->data1 != b->data1)
		return a->data1 > b->data1 ? 1 : -1;
	if (a->data2 != b->data2)
		return a->data2 > b->data2 ? 1 : -1;
	if (a->data3 != b->data3)
		return a->data3 > b->data3 ? 1 : -1;
	return 0;
}

static int
mask_match(struct community *a, struct community *b, struct community *m)
{
	if ((u_int8_t)a->flags != (u_int8_t)b->flags)
		return (u_int8_t)a->flags > (u_int8_t)b->flags ? 1 : -1;

	if ((a->data1 & m->data1) != (b->data1 & m->data1)) {
		if ((a->data1 & m->data1) > (b->data1 & m->data1))
			return 1;
		return -1;
	}
	if ((a->data2 & m->data2) != (b->data2 & m->data2)) {
		if ((a->data2 & m->data2) > (b->data2 & m->data2))
			return 1;
		return -1;
	}
	if ((a->data3 & m->data3) != (b->data3 & m->data3)) {
		if ((a->data3 & m->data3) > (b->data3 & m->data3))
			return 1;
		return -1;
	}
	return 0;
}

/*
 * Insert a community keeping the list sorted. Don't add if already present.
 */
static void
insert_community(struct rde_community *comm, struct community *c)
{
	size_t l;
	int r;

	if (comm->nentries + 1 > comm->size) {
		struct community *new;
		size_t newsize = comm->size + 8;

		if ((new = reallocarray(comm->communities, newsize,
		    sizeof(struct community))) == NULL)
			fatal(__func__);
		memset(new + comm->size, 0, 8 * sizeof(struct community));
		comm->communities = new;
		comm->size = newsize;
	}

	/* XXX can be made faster by binary search */
	for (l = 0; l < comm->nentries; l++) {
		r = fast_match(comm->communities + l, c);
		if (r == 0) {
			/* already present, nothing to do */
			return;
		} else if (r > 0) {
			/* shift reminder by one slot */
			memmove(comm->communities + l + 1,
			    comm->communities + l,
			    (comm->nentries - l) * sizeof(*c));
			break;
		}
	}

	/* insert community at slot l */
	comm->communities[l] = *c;
	comm->nentries++;
}

static int
non_transitive_community(struct community *c)
{
	if ((u_int8_t)c->flags == COMMUNITY_TYPE_EXT &&
	    !((ntohl(c->data1) >> 24) & EXT_COMMUNITY_NON_TRANSITIVE))
		return 1;
	return 0;
}

/*
 * Check if a community is present. This function will expand local-as and
 * neighbor-as and also mask of bits to support partial matches.
 */
int
community_match(struct rde_community *comm, struct community *fc,
struct rde_peer *peer)
{
	struct community test, mask;
	size_t l;

	if (fc->flags >> 8 == 0) {
		/* fast path */
		return (bsearch(fc, comm->communities, comm->nentries,
		    sizeof(*fc), fast_match) != NULL);
	} else {
		/* slow path */

		if (fc2c(fc, peer, &test, &mask) == -1)
			return 0;

		for (l = 0; l < comm->nentries; l++) {
			if (mask_match(&comm->communities[l], &test,
			    &mask) == 0)
				return 1;
		}
		return 0;
	}
}

/*
 * Insert a community, expanding local-as and neighbor-as if needed.
 */
int
community_set(struct rde_community *comm, struct community *fc,
struct rde_peer *peer)
{
	struct community set;

	if (fc->flags >> 8 == 0) {
		/* fast path */
		insert_community(comm, fc);
	} else {
		if (fc2c(fc, peer, &set, NULL) == -1)
			return 0;
		insert_community(comm, &set);
	}
	return 1;
}

/*
 * Remove a community if present, This function will expand local-as and
 * neighbor-as and also mask of bits to support partial matches.
 */
void
community_delete(struct rde_community *comm, struct community *fc,
struct rde_peer *peer)
{
	struct community test, mask;
	struct community *match;
	size_t l = 0;

	if (fc->flags >> 8 == 0) {
		/* fast path */
		match = bsearch(fc, comm->communities, comm->nentries,
		    sizeof(*fc), fast_match);
		if (match == NULL)
			return;
		/* move everything after match down by 1 */
		memmove(match, match + 1,
		    (char *)(comm->communities + comm->nentries) -
		    (char *)(match + 1));
		comm->nentries--;
		return;
	} else {
		if (fc2c(fc, peer, &test, &mask) == -1)
			return;

		while (l < comm->nentries) {
			if (mask_match(&comm->communities[l], &test,
			    &mask) == 0) {
				memmove(comm->communities + l,
				    comm->communities + l + 1,
				    (comm->nentries - l - 1) * sizeof(test));
				comm->nentries--;
				continue;
			}
			l++;
		}
	}
}

/*
 * Internalize communities from the wireformat.
 * Store the partial flag in struct rde_community so it is not lost.
 * - community_add for ATTR_COMMUNITUES
 * - community_large_add for ATTR_LARGE_COMMUNITIES
 * - community_ext_add for ATTR_EXT_COMMUNITIES
 */
int
community_add(struct rde_community *comm, int flags, void *buf, size_t len)
{
	struct community set = { .flags = COMMUNITY_TYPE_BASIC };
	u_int8_t *b = buf;
	u_int16_t c;
	size_t l;

	if (len == 0 || len % 4 != 0)
		return -1;

	if (flags & ATTR_PARTIAL)
		comm->flags |= PARTIAL_COMMUNITIES;

	for (l = 0; l < len; l += 4, b += 4) {
		memcpy(&c, b, sizeof(c));
		set.data1 = ntohs(c);
		memcpy(&c, b + 2, sizeof(c));
		set.data2 = ntohs(c);
		insert_community(comm, &set);
	}

	return 0;
}

int
community_large_add(struct rde_community *comm, int flags, void *buf,
    size_t len)
{
	struct community set = { .flags = COMMUNITY_TYPE_LARGE };
	u_int8_t *b = buf;
	size_t l;

	if (len == 0 || len % 12 != 0)
		return -1;

	if (flags & ATTR_PARTIAL)
		comm->flags |= PARTIAL_LARGE_COMMUNITIES;

	for (l = 0; l < len; l += 12, b += 12) {
		memcpy(&set.data1, b, sizeof(set.data1));
		memcpy(&set.data2, b + 4, sizeof(set.data2));
		memcpy(&set.data3, b + 8, sizeof(set.data3));
		set.data1 = ntohl(set.data1);
		set.data2 = ntohl(set.data2);
		set.data3 = ntohl(set.data3);
		insert_community(comm, &set);
	}

	return 0;
}

int
community_ext_add(struct rde_community *comm, int flags, void *buf, size_t len)
{
	struct community set = { .flags = COMMUNITY_TYPE_EXT };
	u_int8_t *b = buf, type;
	u_int64_t c;
	size_t l;

	if (len == 0 || len % 8 != 0)
		return -1;

	if (flags & ATTR_PARTIAL)
		comm->flags |= PARTIAL_EXT_COMMUNITIES;

	for (l = 0; l < len; l += 8, b += 8) {
		memcpy(&c, b, 8);

		c = be64toh(c);
		type = c >> 56;
		switch (type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
		case EXT_COMMUNITY_TRANS_OPAQUE:
		case EXT_COMMUNITY_TRANS_EVPN:
		case EXT_COMMUNITY_NON_TRANS_OPAQUE:
			set.data1 = c >> 32 & 0xffff;
			set.data2 = c;
			break;
		case EXT_COMMUNITY_TRANS_FOUR_AS:
		case EXT_COMMUNITY_TRANS_IPV4:
			set.data1 = c >> 16;
			set.data2 = c & 0xffff;
			break;
		}
		set.data3 = c >> 48;

		insert_community(comm, &set);
	}

	return 0;
}

/*
 * Convert communities back to the wireformat.
 * This function needs to make sure that the attribute buffer is overflowed
 * while writing out the communities.
 * - community_write for ATTR_COMMUNITUES
 * - community_large_write for ATTR_LARGE_COMMUNITIES
 * - community_ext_write for ATTR_EXT_COMMUNITIES
 *   When writing ATTR_EXT_COMMUNITIES non-transitive communities need to
 *   be skipped if it is sent to an ebgp peer.
 */
int
community_write(struct rde_community *comm, void *buf, u_int16_t len)
{
	u_int8_t *b = buf;
	u_int16_t c;
	size_t l, n = 0;
	int r, flags = ATTR_OPTIONAL | ATTR_TRANSITIVE;

	if (comm->flags & PARTIAL_COMMUNITIES)
		flags |= ATTR_PARTIAL;

	/* first count how many communities will be written */
	for (l = 0; l < comm->nentries; l++)
		if ((u_int8_t)comm->communities[l].flags ==
		    COMMUNITY_TYPE_BASIC)
			n++;

	if (n == 0)
		return 0;

	/* write attribute header */
	r = attr_write(b, len, flags, ATTR_COMMUNITIES, NULL, n * 4);
	if (r == -1)
		return -1;
	b += r;

	/* write out the communities */
	for (l = 0; l < comm->nentries; l++)
		if ((u_int8_t)comm->communities[l].flags ==
		    COMMUNITY_TYPE_BASIC) {
			c = htons(comm->communities[l].data1);
			memcpy(b, &c, sizeof(c));
			b += sizeof(c);
			r += sizeof(c);

			c = htons(comm->communities[l].data2);
			memcpy(b, &c, sizeof(c));
			b += sizeof(c);
			r += sizeof(c);
		}

	return r;
}

int
community_large_write(struct rde_community *comm, void *buf, u_int16_t len)
{
	u_int8_t *b = buf;
	u_int32_t c;
	size_t l, n = 0;
	int r, flags = ATTR_OPTIONAL | ATTR_TRANSITIVE;

	if (comm->flags & PARTIAL_LARGE_COMMUNITIES)
		flags |= ATTR_PARTIAL;

	/* first count how many communities will be written */
	for (l = 0; l < comm->nentries; l++)
		if ((u_int8_t)comm->communities[l].flags ==
		    COMMUNITY_TYPE_LARGE)
			n++;

	if (n == 0)
		return 0;

	/* write attribute header */
	r = attr_write(b, len, flags, ATTR_LARGE_COMMUNITIES, NULL, n * 12);
	if (r == -1)
		return -1;
	b += r;

	/* write out the communities */
	for (l = 0; l < comm->nentries; l++)
		if ((u_int8_t)comm->communities[l].flags ==
		    COMMUNITY_TYPE_LARGE) {
			c = htonl(comm->communities[l].data1);
			memcpy(b, &c, sizeof(c));
			b += sizeof(c);
			r += sizeof(c);

			c = htonl(comm->communities[l].data2);
			memcpy(b, &c, sizeof(c));
			b += sizeof(c);
			r += sizeof(c);

			c = htonl(comm->communities[l].data3);
			memcpy(b, &c, sizeof(c));
			b += sizeof(c);
			r += sizeof(c);
		}

	return r;
}

int
community_ext_write(struct rde_community *comm, int ebgp, void *buf,
    u_int16_t len)
{
	struct community *cp;
	u_int8_t *b = buf;
	u_int64_t ext;
	size_t l, n = 0;
	int r, flags = ATTR_OPTIONAL | ATTR_TRANSITIVE;

	if (comm->flags & PARTIAL_EXT_COMMUNITIES)
		flags |= ATTR_PARTIAL;

	/* first count how many communities will be written */
	for (l = 0; l < comm->nentries; l++)
		if ((u_int8_t)comm->communities[l].flags ==
		    COMMUNITY_TYPE_EXT && !(ebgp &&
		    non_transitive_community(&comm->communities[l])))
			n++;

	if (n == 0)
		return 0;

	/* write attribute header */
	r = attr_write(b, len, flags, ATTR_EXT_COMMUNITIES, NULL, n * 8);
	if (r == -1)
		return -1;
	b += r;

	/* write out the communities */
	for (l = 0; l < comm->nentries; l++) {
		cp = comm->communities + l;
		if ((u_int8_t)cp->flags == COMMUNITY_TYPE_EXT && !(ebgp &&
		    non_transitive_community(cp))) {
			ext = (u_int64_t)cp->data3 << 48;
			switch (cp->data3 >> 8) {
			case EXT_COMMUNITY_TRANS_TWO_AS:
			case EXT_COMMUNITY_TRANS_OPAQUE:
			case EXT_COMMUNITY_TRANS_EVPN:
			case EXT_COMMUNITY_NON_TRANS_OPAQUE:
				ext |= ((u_int64_t)cp->data1 & 0xffff) << 32;
				ext |= (u_int64_t)cp->data2;
				break;
			case EXT_COMMUNITY_TRANS_FOUR_AS:
			case EXT_COMMUNITY_TRANS_IPV4:
				ext |= (u_int64_t)cp->data1 << 16;
				ext |= (u_int64_t)cp->data2 & 0xffff;
				break;
			}
			ext = htobe64(ext);
			memcpy(b, &ext, sizeof(ext));
			b += sizeof(ext);
			r += sizeof(ext);
		}
	}

	return r;
}

/*
 * Global RIB cache for communities
 */
LIST_HEAD(commhead, rde_community);

static struct comm_table {
	struct commhead		*hashtbl;
	u_int64_t		 hashmask;
} commtable;

static SIPHASH_KEY commtablekey;

static inline struct commhead *
communities_hash(struct rde_community *comm)
{
	SIPHASH_CTX	ctx;
	u_int64_t	hash;

	SipHash24_Init(&ctx, &commtablekey);
	SipHash24_Update(&ctx, &comm->nentries, sizeof(comm->nentries));
	SipHash24_Update(&ctx, &comm->flags, sizeof(comm->flags));
	if (comm->nentries > 0)
		SipHash24_Update(&ctx, comm->communities,
		    comm->nentries * sizeof(*comm->communities));
	hash = SipHash24_End(&ctx);

	return &commtable.hashtbl[hash & commtable.hashmask];
}

void
communities_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	arc4random_buf(&commtablekey, sizeof(commtablekey));
	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	commtable.hashtbl = calloc(hs, sizeof(*commtable.hashtbl));
	if (commtable.hashtbl == NULL)
		fatal(__func__);

	for (i = 0; i < hs; i++)
		LIST_INIT(&commtable.hashtbl[i]);
	commtable.hashmask = hs - 1;
}

void
communities_shutdown(void)
{
	u_int64_t	i;

	for (i = 0; i <= commtable.hashmask; i++)
		if (!LIST_EMPTY(&commtable.hashtbl[i]))
			log_warnx("%s: free non-free table", __func__);

	free(commtable.hashtbl);
}

void
communities_hash_stats(struct rde_hashstats *hs)
{
	struct rde_community *c;
	u_int64_t i;
	int64_t n;

	memset(hs, 0, sizeof(*hs));
	strlcpy(hs->name, "comm hash", sizeof(hs->name));
	hs->min = LLONG_MAX;
	hs->num = commtable.hashmask + 1;

	for (i = 0; i <= commtable.hashmask; i++) {
		n = 0;
		LIST_FOREACH(c, &commtable.hashtbl[i], entry)
			n++;
		if (n < hs->min)
			hs->min = n;
		if (n > hs->max)
			hs->max = n;
		hs->sum += n;
		hs->sumq += n * n;
	}
}

struct rde_community *
communities_lookup(struct rde_community *comm)
{
	struct rde_community *c;
	struct commhead *head;

	head = communities_hash(comm);
	LIST_FOREACH(c, head, entry) {
		if (communities_equal(comm, c))
			return c;
	}
	return NULL;
}

struct rde_community *
communities_link(struct rde_community *comm)
{
	struct rde_community *n;
	struct commhead *head;

	if ((n = malloc(sizeof(*n))) == NULL)
		fatal(__func__);

	communities_copy(n, comm);

	head = communities_hash(n);
	LIST_INSERT_HEAD(head, n, entry);
	n->refcnt = 1;	/* initial reference by the cache */

	rdemem.comm_size += n->size;
	rdemem.comm_nmemb += n->nentries;
	rdemem.comm_cnt++;

	return n;
}

void
communities_unlink(struct rde_community *comm)
{
	if (comm->refcnt != 1)
		fatalx("%s: unlinking still referenced communities", __func__);

	LIST_REMOVE(comm, entry);

	rdemem.comm_size -= comm->size;
	rdemem.comm_nmemb -= comm->nentries;
	rdemem.comm_cnt--;

	free(comm->communities);
	free(comm);
}

/*
 * Return true/1 if the two communities collections are identical,
 * otherwise returns zero.
 */
int
communities_equal(struct rde_community *a, struct rde_community *b)
{
	if (a->nentries != b->nentries)
		return 0;
	if (a->flags != b->flags)
		return 0;

	return (memcmp(a->communities, b->communities,
	    a->nentries * sizeof(struct community)) == 0);
}

/*
 * Copy communities to a new unreferenced struct. Needs to call
 * communities_clean() when done. to can be statically allocated,
 * it will be cleaned first.
 */
void
communities_copy(struct rde_community *to, struct rde_community *from)
{
	memset(to, 0, sizeof(*to));

	/* ingore from->size and allocate the perfect amount */
	to->size = from->size;
	to->nentries = from->nentries;
	to->flags = from->flags;

	if ((to->communities = reallocarray(NULL, to->size,
	    sizeof(struct community))) == NULL)
		fatal(__func__);

	memcpy(to->communities, from->communities,
	    to->nentries * sizeof(struct community));
	memset(to->communities + to->nentries, 0, sizeof(struct community) *
	    (to->size - to->nentries));
}

/*
 * Clean up the communities by freeing any dynamically allocated memory.
 */
void
communities_clean(struct rde_community *comm)
{
	if (comm->refcnt != 0)
		fatalx("%s: cleaning still referenced communities", __func__);

	free(comm->communities);
	memset(comm, 0, sizeof(*comm));
}

int
community_to_rd(struct community *fc, u_int64_t *community)
{
	struct community c;
	u_int64_t rd;

	if (fc2c(fc, NULL, &c, NULL) == -1)
		return -1;

	switch (c.data3 >> 8) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		rd = (0ULL << 48);
		rd |= ((u_int64_t)c.data1 & 0xffff) << 32;
		rd |= (u_int64_t)c.data2;
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		rd = (1ULL << 48);
		rd |= (u_int64_t)c.data1 << 16;
		rd |= (u_int64_t)c.data2 & 0xffff;
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		rd = (2ULL << 48);
		rd |= (u_int64_t)c.data1 << 16;
		rd |= (u_int64_t)c.data2 & 0xffff;
		break;
	default:
		return -1;
	}

	*community = htobe64(rd);
	return 0;
}
