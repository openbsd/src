/*	$OpenBSD: rde_attr.c,v 1.62 2006/02/09 20:54:56 claudio Exp $ */

/*
 * Copyright (c) 2004 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/hash.h>
#include <sys/queue.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

void	attr_free(struct rde_aspath *, struct attr *);

int
attr_write(void *p, u_int16_t p_len, u_int8_t flags, u_int8_t type,
    void *data, u_int16_t data_len)
{
	u_char		*b = p;
	u_int16_t	 tmp, tot_len = 2; /* attribute header (without len) */

	if (data_len > 255) {
		tot_len += 2 + data_len;
		flags |= ATTR_EXTLEN;
	} else {
		tot_len += 1 + data_len;
		flags &= ~ATTR_EXTLEN;
	}

	if (tot_len > p_len)
		return (-1);

	*b++ = flags;
	*b++ = type;
	if (data_len > 255) {
		tmp = htons(data_len);
		memcpy(b, &tmp, sizeof(tmp));
		b += 2;
	} else
		*b++ = (u_char)data_len;

	if (data_len != 0)
		memcpy(b, data, data_len);

	return (tot_len);
}

/* optional attribute specific functions */
int		 attr_diff(struct attr *, struct attr *);
struct attr	*attr_alloc(u_int8_t, u_int8_t, const void *, u_int16_t);
struct attr	*attr_lookup(u_int8_t, u_int8_t, const void *, u_int16_t);
void		 attr_put(struct attr *);

struct attr_table {
	struct attr_list	*hashtbl;
	u_int32_t		 hashmask;
} attrtable;

#define ATTR_HASH(x)				\
	&attrtable.hashtbl[(x) & attrtable.hashmask]

void
attr_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	attrtable.hashtbl = calloc(hs, sizeof(struct attr_list));
	if (attrtable.hashtbl == NULL)
		fatal("attr_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&attrtable.hashtbl[i]);

	attrtable.hashmask = hs - 1;
}

void
attr_shutdown(void)
{
	u_int32_t	i;

	for (i = 0; i <= attrtable.hashmask; i++)
		if (!LIST_EMPTY(&attrtable.hashtbl[i]))
			log_warnx("attr_shutdown: free non-free table");

	free(attrtable.hashtbl);
}

int
attr_optadd(struct rde_aspath *asp, u_int8_t flags, u_int8_t type,
    void *data, u_int16_t len)
{
	u_int8_t	 l;
	struct attr	*a, *t;

	/* known optional attributes were validated previously */
	if ((a = attr_lookup(flags, type, data, len)) == NULL)
		a = attr_alloc(flags, type, data, len);

	/* attribute allowed only once */
	for (l = 0; l < asp->others_len; l++) {
		if (asp->others[l] == NULL)
			break;
		if (type == asp->others[l]->type)
			return (-1);
	}

	/* add attribute to the table but first bump refcnt */
	a->refcnt++;
	rdemem.attr_refs++;

	for (l = 0; l < asp->others_len; l++) {
		if (asp->others[l] == NULL) {
			asp->others[l] = a;
			return (0);
		}
		/* list is sorted */
		if (a->type < asp->others[l]->type) {
			t = asp->others[l];
			asp->others[l] = a;
			a = t;
		}
	}

	/* no empty slot found, need to realloc */
	asp->others_len++;
	if ((asp->others = realloc(asp->others,
	    asp->others_len * sizeof(struct attr *))) == NULL)
		fatal("attr_optadd");

	/* l stores the size of others before resize */
	asp->others[l] = a;
	return (0);
}

struct attr *
attr_optget(const struct rde_aspath *asp, u_int8_t type)
{
	u_int8_t	 l;

	for (l = 0; l < asp->others_len; l++) {
		if (asp->others[l] == NULL)
			break;
		if (type == asp->others[l]->type)
			return (asp->others[l]);
		if (type < asp->others[l]->type)
			break;
	}
	return (NULL);
}

void
attr_copy(struct rde_aspath *t, struct rde_aspath *s)
{
	u_int8_t	l;

	if (t->others != NULL)
		attr_freeall(t);

	t->others_len = s->others_len;
	if (t->others_len == 0) {
		t->others = NULL;
		return;
	}

	if ((t->others = calloc(s->others_len, sizeof(struct attr *))) == 0)
		fatal("attr_copy");

	for (l = 0; l < t->others_len; l++) {
		if (s->others[l] == NULL)
			break;
		s->others[l]->refcnt++;
		rdemem.attr_refs++;
		t->others[l] = s->others[l];
	}
}

int
attr_diff(struct attr *oa, struct attr *ob)
{
	int	r;

	if (ob == NULL)
		return (1);
	if (oa == NULL)
		return (-1);
	if (oa->flags > ob->flags)
		return (1);
	if (oa->flags < ob->flags)
		return (-1);
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

	fatalx("attr_diff: equal attributes encountered");
	return (0);
}

int
attr_compare(struct rde_aspath *a, struct rde_aspath *b)
{
	u_int8_t	l, min;

	min = a->others_len < b->others_len ? a->others_len : b->others_len;
	for (l = 0; l < min; l++)
		if (a->others[l] != b->others[l])
			return (attr_diff(a->others[l], b->others[l]));

	if (a->others_len < b->others_len) {
		for (; l < b->others_len; l++)
			if (b->others[l] != NULL)
				return (-1);
	} else if (a->others_len > b->others_len) {
		for (; l < a->others_len; l++)
			if (a->others[l] != NULL)
				return (1);
	}

	return (0);
}

void
attr_free(struct rde_aspath *asp, struct attr *attr)
{
	u_int8_t	l;

	for (l = 0; l < asp->others_len; l++)
		if (asp->others[l] == attr) {
			attr_put(asp->others[l]);
			for (++l; l < asp->others_len; l++)
				asp->others[l - 1] = asp->others[l];
			asp->others[asp->others_len - 1] = NULL;
			return;
		}

	/* no realloc() because the slot may be reused soon */
}

void
attr_freeall(struct rde_aspath *asp)
{
	u_int8_t	l;

	for (l = 0; l < asp->others_len; l++)
		attr_put(asp->others[l]);

	free(asp->others);
	asp->others = NULL;
	asp->others_len = 0;
}

struct attr *
attr_alloc(u_int8_t flags, u_int8_t type, const void *data, u_int16_t len)
{
	struct attr	*a;

	a = calloc(1, sizeof(struct attr));
	if (a == NULL)
		fatal("attr_optadd");
	rdemem.attr_cnt++;

	a->flags = flags;
	a->hash = hash32_buf(&flags, sizeof(flags), HASHINIT);
	a->type = type;
	a->hash = hash32_buf(&type, sizeof(type), a->hash);
	a->len = len;
	if (len != 0) {
		if ((a->data = malloc(len)) == NULL)
			fatal("attr_optadd");

		rdemem.attr_dcnt++;
		rdemem.attr_data += len;
		memcpy(a->data, data, len);
	} else
		a->data = NULL;

	a->hash = hash32_buf(&len, sizeof(len), a->hash);
	a->hash = hash32_buf(a->data, a->len, a->hash);
	LIST_INSERT_HEAD(ATTR_HASH(a->hash), a, entry);

	return (a);
}

struct attr *
attr_lookup(u_int8_t flags, u_int8_t type, const void *data, u_int16_t len)
{
	struct attr_list	*head;
	struct attr		*a;
	u_int32_t		 hash;

	hash = hash32_buf(&flags, sizeof(flags), HASHINIT);
	hash = hash32_buf(&type, sizeof(type), hash);
	hash = hash32_buf(&len, sizeof(len), hash);
	hash = hash32_buf(data, len, hash);
	head = ATTR_HASH(hash);

	LIST_FOREACH(a, head, entry) {
		if (hash == a->hash && type == a->type &&
		    flags == a->flags && len == a->len &&
		    memcmp(data, a->data, len) == 0)
			return (a);
	}
	return (NULL);
}

void
attr_put(struct attr *a)
{
	if (a == NULL)
		return;

	rdemem.attr_refs--;
	if (--a->refcnt > 0)
		/* somebody still holds a reference */
		return;

	/* unlink */
	LIST_REMOVE(a, entry);

	if (a->len != 0)
		rdemem.attr_dcnt--;
	rdemem.attr_data -= a->len;
	rdemem.attr_cnt--;
	free(a->data);
	free(a);
}

/* aspath specific functions */

struct aspath	*aspath_lookup(const void *, u_int16_t);

struct aspath_table {
	struct aspath_list	*hashtbl;
	u_int32_t		 hashmask;
} astable;

#define ASPATH_HASH(x)				\
	&astable.hashtbl[(x) & astable.hashmask]

int
aspath_verify(void *data, u_int16_t len)
{
	u_int8_t	*seg = data;
	u_int16_t	 seg_size;
	u_int8_t	 seg_len, seg_type;

	if (len & 1)
		/* odd length aspath are invalid */
		return (AS_ERR_BAD);

	for (; len > 0; len -= seg_size, seg += seg_size) {
		if (len < 2)
			return (AS_ERR_BAD);
		seg_type = seg[0];
		seg_len = seg[1];

		if (seg_type != AS_SET && seg_type != AS_SEQUENCE)
			return (AS_ERR_TYPE);

		seg_size = 2 + 2 * seg_len;

		if (seg_size > len)
			return (AS_ERR_LEN);

		if (seg_size == 0)
			/* empty aspath segment are not allowed */
			return (AS_ERR_BAD);
	}
	return (0);	/* aspath is valid but probably not loop free */
}

void
aspath_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	astable.hashtbl = calloc(hs, sizeof(struct aspath_list));
	if (astable.hashtbl == NULL)
		fatal("aspath_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&astable.hashtbl[i]);

	astable.hashmask = hs - 1;
}

void
aspath_shutdown(void)
{
	u_int32_t	i;

	for (i = 0; i <= astable.hashmask; i++)
		if (!LIST_EMPTY(&astable.hashtbl[i]))
			log_warnx("aspath_shutdown: free non-free table");

	free(astable.hashtbl);
}

struct aspath *
aspath_get(void *data, u_int16_t len)
{
	struct aspath_list	*head;
	struct aspath		*aspath;

	/* The aspath must already have been checked for correctness. */
	aspath = aspath_lookup(data, len);
	if (aspath == NULL) {
		aspath = malloc(ASPATH_HEADER_SIZE + len);
		if (aspath == NULL)
			fatal("aspath_get");

		rdemem.aspath_cnt++;
		rdemem.aspath_size += ASPATH_HEADER_SIZE + len;

		aspath->refcnt = 0;
		aspath->len = len;
		aspath->ascnt = aspath_count(data, len);
		memcpy(aspath->data, data, len);

		/* link */
		head = ASPATH_HASH(hash32_buf(aspath->data, aspath->len,
		    HASHINIT));
		LIST_INSERT_HEAD(head, aspath, entry);
	}
	aspath->refcnt++;
	rdemem.aspath_refs++;

	return (aspath);
}

void
aspath_put(struct aspath *aspath)
{
	if (aspath == NULL)
		return;

	rdemem.aspath_refs--;
	if (--aspath->refcnt > 0) {
		/* somebody still holds a reference */
		return;
	}

	/* unlink */
	LIST_REMOVE(aspath, entry);

	rdemem.aspath_cnt--;
	rdemem.aspath_size -= ASPATH_HEADER_SIZE + aspath->len;
	free(aspath);
}

u_char *
aspath_dump(struct aspath *aspath)
{
	return (aspath->data);
}

u_int16_t
aspath_length(struct aspath *aspath)
{
	return (aspath->len);
}

u_int16_t
aspath_count(const void *data, u_int16_t len)
{
	const u_int8_t	*seg;
	u_int16_t	 cnt, seg_size;
	u_int8_t	 seg_type, seg_len;

	cnt = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + 2 * seg_len;

		if (seg_type == AS_SET)
			cnt += 1;
		else
			cnt += seg_len;

		if (seg_size > len)
			fatalx("aspath_count: bula bula");
	}
	return (cnt);
}

u_int16_t
aspath_neighbor(struct aspath *aspath)
{
	/*
	 * Empty aspath is OK -- internal as route.
	 * But what is the neighbor? For now let's return 0.
	 * That should not break anything.
	 */

	if (aspath->len == 0)
		return (0);

	return (aspath_extract(aspath->data, 0));
}

int
aspath_loopfree(struct aspath *aspath, u_int16_t myAS)
{
	u_int8_t	*seg;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len, seg_type;

	seg = aspath->data;
	for (len = aspath->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + 2 * seg_len;

		for (i = 0; i < seg_len; i++) {
			if (myAS == aspath_extract(seg, i))
				return (0);
		}

		if (seg_size > len)
			fatalx("aspath_loopfree: bula bula");
	}
	return (1);
}

int
aspath_compare(struct aspath *a1, struct aspath *a2)
{
	int r;

	if (a1->len > a2->len)
		return (1);
	if (a1->len < a2->len)
		return (-1);
	r = memcmp(a1->data, a2->data, a1->len);
	if (r > 0)
		return (1);
	if (r < 0)
		return (-1);
	return (0);
}

struct aspath *
aspath_lookup(const void *data, u_int16_t len)
{
	struct aspath_list	*head;
	struct aspath		*aspath;
	u_int32_t		 hash;

	hash = hash32_buf(data, len, HASHINIT);
	head = ASPATH_HASH(hash);

	LIST_FOREACH(aspath, head, entry) {
		if (len == aspath->len && memcmp(data, aspath->data, len) == 0)
			return (aspath);
	}
	return (NULL);
}


/*
 * Returns a new prepended aspath. Old needs to be freed by caller.
 */
struct aspath *
aspath_prepend(struct aspath *asp, u_int16_t as, int quantum)
{
	u_char		*p;
	int		 len, overflow = 0, shift = 0, size, wpos = 0;
	u_int8_t	 type;

	/* lunatic prepends are blocked in the parser and limited */

	/* first calculate new size */
	if (asp->len > 0) {
		if (asp->len < 2)
			fatalx("aspath_prepend: bula bula");
		type = asp->data[0];
		size = asp->data[1];
	} else {
		/* empty as path */
		type = AS_SET;
		size = 0;
	}

	if (quantum == 0) {
		/* no change needed but increase refcnt as we return a copy */
		asp->refcnt++;
		rdemem.aspath_refs++;
		return (asp);
	} else if (type == AS_SET || size + quantum > 255) {
		/* need to attach a new AS_SEQUENCE */
		len = 2 + quantum * 2 + asp->len;
		overflow = type == AS_SET ? quantum : (size + quantum) & 0xff;
	} else
		len = quantum * 2 + asp->len;

	quantum -= overflow;

	p = malloc(len);
	if (p == NULL)
		fatal("aspath_prepend");

	/* first prepends */
	as = htons(as);
	if (overflow > 0) {
		p[wpos++] = AS_SEQUENCE;
		p[wpos++] = overflow;

		for (; overflow > 0; overflow--) {
			memcpy(p + wpos, &as, 2);
			wpos += 2;
		}
	}
	if (quantum > 0) {
		shift = 2;
		p[wpos++] = AS_SEQUENCE;
		p[wpos++] = quantum + size;

		for (; quantum > 0; quantum--) {
			memcpy(p + wpos, &as, 2);
			wpos += 2;
		}
	}
	memcpy(p + wpos, asp->data + shift, asp->len - shift);

	asp = aspath_get(p, len);
	free(p);

	return (asp);
}

/* we need to be able to search more than one as */
int
aspath_match(struct aspath *a, enum as_spec type, u_int16_t as)
{
	u_int8_t	*seg;
	int		 final;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_type, seg_len;

	if (type == AS_EMPTY) {
		if (a->len == 0)
			return (1);
		else
			return (0);
	}

	final = 0;
	seg = a->data;
	for (len = a->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + 2 * seg_len;

		final = (len == seg_size);

		if (type == AS_SOURCE && !final)
			/* not yet in the final segment */
			continue;

		for (i = 0; i < seg_len; i++)
			if (as == aspath_extract(seg, i)) {
				if (final && i + 1 >= seg_len)
					/* the final (rightmost) as */
					if (type == AS_TRANSIT)
						return (0);
					else
						return (1);
				else if (type != AS_SOURCE)
					return (1);
			}
	}
	return (0);
}

int
community_match(void *data, u_int16_t len, int as, int type)
{
	u_int8_t	*p = data;
	u_int16_t	 eas, etype;

	len >>= 2; /* divide by four */

	for (; len > 0; len--) {
		eas = *p++;
		eas <<= 8;
		eas |= *p++;
		etype = *p++;
		etype <<= 8;
		etype |= *p++;
		if ((as == COMMUNITY_ANY || (u_int16_t)as == eas) &&
		    (type == COMMUNITY_ANY || (u_int16_t)type == etype))
			return (1);
	}
	return (0);
}

int
community_set(struct rde_aspath *asp, int as, int type)
{
	struct attr	*attr;
	u_int8_t	*p = NULL;
	unsigned int	 i, ncommunities = 0;
	u_int8_t	 f = ATTR_OPTIONAL|ATTR_TRANSITIVE;
	u_int8_t	 t = ATTR_COMMUNITIES;

	attr = attr_optget(asp, ATTR_COMMUNITIES);

	if (attr != NULL) {
		p = attr->data;
		ncommunities = attr->len >> 2; /* divide by four */
	}

	/* first check if the community is not already set */
	for (i = 0; i < ncommunities; i++) {
		if (as >> 8 == p[0] && (as & 0xff) == p[1] &&
		    type >> 8 == p[2] && (type & 0xff) == p[3])
			/* already present, nothing todo */
			return (1);
		p += 4;
	}

	if (ncommunities++ >= 0x3fff)
		/* overflow */
		return (0);

	if ((p = malloc(ncommunities << 2)) == NULL)
		fatal("community_set");

	p[0] = as >> 8;
	p[1] = as & 0xff;
	p[2] = type >> 8;
	p[3] = type & 0xff;

	if (attr != NULL) {
		memcpy(p + 4, attr->data, attr->len);
		f = attr->flags;
		t = attr->type;
		attr_free(asp, attr);
	}

	attr_optadd(asp, f, t, p, ncommunities << 2);

	return (1);
}

