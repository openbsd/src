/*	$OpenBSD: rde_attr.c,v 1.28 2004/04/30 05:47:50 deraadt Exp $ */

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
#include <sys/queue.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bgpd.h"
#include "ensure.h"
#include "rde.h"

/* attribute specific functions */
#define UPD_READ(t, p, plen, n) \
	do { \
		memcpy(t, p, n); \
		p += n; \
		plen += n; \
	} while (0)

#define CHECK_FLAGS(s, t, m)	\
	(((s) & ~(ATTR_EXTLEN | (m))) == (t))

#define F_ATTR_ORIGIN		0x01
#define F_ATTR_ASPATH		0x02
#define F_ATTR_NEXTHOP		0x04
#define F_ATTR_LOCALPREF	0x08
#define F_ATTR_MED		0x10

#define WFLAG(s, t)			\
	do {				\
		if ((s) & (t))		\
			return (-1);	\
		(s) |= (t);		\
	} while (0)
void
attr_init(struct attr_flags *a)
{
	bzero(a, sizeof(struct attr_flags));
	a->origin = ORIGIN_INCOMPLETE;
	TAILQ_INIT(&a->others);
}

int
attr_parse(u_char *p, u_int16_t len, struct attr_flags *a, int ebgp,
    enum enforce_as enforce_as, u_int16_t remote_as)
{
	u_int32_t	 tmp32;
	u_int16_t	 attr_len;
	u_int16_t	 plen = 0;
	u_int8_t	 flags;
	u_int8_t	 type;
	u_int8_t	 tmp8;

	if (len < 3)
		return (-1);

	UPD_READ(&flags, p, plen, 1);
	UPD_READ(&type, p, plen, 1);

	if (flags & ATTR_EXTLEN) {
		if (len - plen < 2)
			return (-1);
		UPD_READ(&attr_len, p, plen, 2);
		attr_len = ntohs(attr_len);
	} else {
		UPD_READ(&tmp8, p, plen, 1);
		attr_len = tmp8;
	}

	if (len - plen < attr_len)
		return (-1);

	switch (type) {
	case ATTR_UNDEF:
		/* error! */
		return (-1);
	case ATTR_ORIGIN:
		if (attr_len != 1)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			return (-1);
		UPD_READ(&a->origin, p, plen, 1);
		if (a->origin > ORIGIN_INCOMPLETE)
			return (-1);
		WFLAG(a->wflags, F_ATTR_ORIGIN);
		break;
	case ATTR_ASPATH:
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			return (-1);
		if (aspath_verify(p, attr_len) != 0)
			return (-1);
		WFLAG(a->wflags, F_ATTR_ASPATH);
		a->aspath = aspath_create(p, attr_len);
		if (enforce_as == ENFORCE_AS_ON &&
		    remote_as != aspath_neighbor(a->aspath))
			return (-1);

		plen += attr_len;
		break;
	case ATTR_NEXTHOP:
		if (attr_len != 4)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			return (-1);
		WFLAG(a->wflags, F_ATTR_NEXTHOP);
		UPD_READ(&a->nexthop, p, plen, 4);	/* network byte order */
		/*
		 * Check if the nexthop is a valid IP address. We consider
		 * multicast, experimental and loopback addresses as invalid.
		 */
		tmp32 = ntohl(a->nexthop.s_addr);
		if (IN_MULTICAST(tmp32) || IN_BADCLASS(tmp32) ||
		    (tmp32 & 0x7f000000) == 0x7f000000)
			return (-1);
		break;
	case ATTR_MED:
		if (attr_len != 4)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			return (-1);
		WFLAG(a->wflags, F_ATTR_MED);
		UPD_READ(&tmp32, p, plen, 4);
		a->med = ntohl(tmp32);
		break;
	case ATTR_LOCALPREF:
		if (attr_len != 4)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			return (-1);
		if (ebgp) {
			/* ignore local-pref attr for non ibgp peers */
			a->lpref = 0;	/* set a default value ... */
			plen += 4;	/* and ignore the real value */
			break;
		}
		WFLAG(a->wflags, F_ATTR_LOCALPREF);
		UPD_READ(&tmp32, p, plen, 4);
		a->lpref = ntohl(tmp32);
		break;
	case ATTR_ATOMIC_AGGREGATE:
		if (attr_len != 0)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			return (-1);
		goto optattr;
	case ATTR_AGGREGATOR:
		if (attr_len != 6)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE, 0))
			return (-1);
		goto optattr;
	case ATTR_COMMUNITIES:
		if ((attr_len & 0x3) != 0)
			return (-1);
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			return (-1);
		goto optattr;
	default:
optattr:
		if (attr_optadd(a, flags, type, p, attr_len) == -1)
			return (-1);
		plen += attr_len;
		break;
	}

	return (plen);
}

u_char *
attr_error(u_char *p, u_int16_t len, struct attr_flags *attr,
    u_int8_t *suberr, u_int16_t *size)
{
	struct attr	*a;
	u_int16_t	 attr_len;
	u_int16_t	 plen = 0;
	u_int8_t	 flags;
	u_int8_t	 type;
	u_int8_t	 tmp8;

	*suberr = ERR_UPD_ATTRLEN;
	*size = len;
	if (len < 3)
		return (p);

	UPD_READ(&flags, p, plen, 1);
	UPD_READ(&type, p, plen, 1);

	if (flags & ATTR_EXTLEN) {
		if (len - plen < 2)
			return (p);
		UPD_READ(&attr_len, p, plen, 2);
	} else {
		UPD_READ(&tmp8, p, plen, 1);
		attr_len = tmp8;
	}

	if (len - plen < attr_len)
		return (p);
	*size = attr_len;

	switch (type) {
	case ATTR_UNDEF:
		/* error! */
		*suberr = ERR_UPD_UNSPECIFIC;
		*size = 0;
		return (NULL);
	case ATTR_ORIGIN:
		if (attr_len != 1)
			return (p);
		if (attr->wflags & F_ATTR_ORIGIN) {
			*suberr = ERR_UPD_ATTRLIST;
			*size = 0;
			return (NULL);
		}
		UPD_READ(&tmp8, p, plen, 1);
		if (tmp8 > ORIGIN_INCOMPLETE) {
			*suberr = ERR_UPD_ORIGIN;
			return (p);
		}
		break;
	case ATTR_ASPATH:
		if (attr->wflags & F_ATTR_ASPATH) {
			*suberr = ERR_UPD_ATTRLIST;
			*size = 0;
			return (NULL);
		}
		if (CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0)) {
			/* malformed aspath detected by exclusion method */
			*size = 0;
			*suberr = ERR_UPD_ASPATH;
			return (NULL);
		}
		break;
	case ATTR_NEXTHOP:
		if (attr_len != 4)
			return (p);
		if (attr->wflags & F_ATTR_NEXTHOP) {
			*suberr = ERR_UPD_ATTRLIST;
			*size = 0;
			return (NULL);
		}
		if (CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0)) {
			/* malformed nexthop detected by exclusion method */
			*suberr = ERR_UPD_NETWORK;
			return (p);
		}
		break;
	case ATTR_MED:
		if (attr_len != 4)
			return (p);
		if (attr->wflags & F_ATTR_MED) {
			*suberr = ERR_UPD_ATTRLIST;
			*size = 0;
			return (NULL);
		}
		break;
	case ATTR_LOCALPREF:
		if (attr_len != 4)
			return (p);
		if (attr->wflags & F_ATTR_LOCALPREF) {
			*suberr = ERR_UPD_ATTRLIST;
			*size = 0;
			return (NULL);
		}
		break;
	case ATTR_ATOMIC_AGGREGATE:
		if (attr_len != 0)
			return (p);
		break;
	case ATTR_AGGREGATOR:
		if (attr_len != 6)
			return (p);
		break;
	case ATTR_COMMUNITIES:
		if ((attr_len & 0x3) != 0)
			return (p);
		/* FALLTHROUGH */
	default:
		if ((flags & ATTR_OPTIONAL) == 0) {
			*suberr = ERR_UPD_UNKNWN_WK_ATTR;
			return (p);
		}
		TAILQ_FOREACH(a, &attr->others, attr_l)
			if (type == a->type) {
				*size = 0;
				*suberr = ERR_UPD_ATTRLIST;
				return (NULL);
			}
		*suberr = ERR_UPD_OPTATTR;
		return (p);
	}
	/* can only be a attribute flag error */
	*suberr = ERR_UPD_ATTRFLAGS;
	return (p);
}
#undef UPD_READ
#undef WFLAG

u_int8_t
attr_missing(struct attr_flags *a, int ebgp)
{
	if ((a->wflags & F_ATTR_ORIGIN) == 0)
		return ATTR_ORIGIN;
	if ((a->wflags & F_ATTR_ASPATH) == 0)
		return ATTR_ASPATH;
	if ((a->wflags & F_ATTR_NEXTHOP) == 0)
		return ATTR_NEXTHOP;
	if (!ebgp)
		if ((a->wflags & F_ATTR_LOCALPREF) == 0)
			return ATTR_LOCALPREF;
	return 0;
}

int
attr_compare(struct attr_flags *a, struct attr_flags *b)
{
	struct attr	*oa, *ob;
	int		 r;

	if (a->origin > b->origin)
		return (1);
	if (a->origin < b->origin)
		return (-1);
	if (a->nexthop.s_addr > b->nexthop.s_addr)
		return (1);
	if (a->nexthop.s_addr < b->nexthop.s_addr)
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
	    oa != NULL && ob != NULL;
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
	if (oa != NULL)
		return (1);
	if (ob != NULL)
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

void
attr_move(struct attr_flags *t, struct attr_flags *s)
{
	struct attr	*os;
	/*
	 * first copy the full struct, then move the optional attributes.
	 */
	memcpy(t, s, sizeof(struct attr_flags));
	TAILQ_INIT(&t->others);
	while ((os = TAILQ_FIRST(&s->others)) != NULL) {
		TAILQ_REMOVE(&s->others, os, attr_l);
		TAILQ_INSERT_TAIL(&t->others, os, attr_l);
	}
}

void
attr_free(struct attr_flags *a)
{
	/*
	 * free the aspath and all optional path attributes
	 * but not the attr_flags struct.
	 */
	aspath_destroy(a->aspath);
	a->aspath = NULL;
	attr_optfree(a);
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

int
attr_optadd(struct attr_flags *attr, u_int8_t flags, u_int8_t type,
    u_char *data, u_int16_t len)
{
	struct attr	*a, *p;

	/* we need validate known optional attributes */

	if (flags & ATTR_OPTIONAL && ! flags & ATTR_TRANSITIVE)
		/*
		 * We already know that we're not interested in this attribute.
		 * Currently only the MED is optional and non-transitive but
		 * MED is directly stored in struct attr_flags.
		 */
		return (0);

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
	} else
		a->data = NULL;

	/* keep a sorted list */
	TAILQ_FOREACH_REVERSE(p, &attr->others, attr_list, attr_l) {
		if (type == p->type) {
			/* attribute only once allowed */
			free(a->data);
			free(a);
			return (-1);
		}
		if (type > p->type) {
			TAILQ_INSERT_AFTER(&attr->others, p, a, attr_l);
			return (0);
		}
	}
	TAILQ_INSERT_HEAD(&attr->others, a, attr_l);
	return (0);
}

struct attr *
attr_optget(struct attr_flags *attr, u_int8_t type)
{
	struct attr	*a;

	TAILQ_FOREACH(a, &attr->others, attr_l) {
		if (type == a->type)
			return (a);
		if (type < a->type)
			/* list is sorted */
			break;
	}
	return (NULL);
}

void
attr_optfree(struct attr_flags *attr)
{
	struct attr	*a;

	while ((a = TAILQ_FIRST(&attr->others)) != NULL) {
		TAILQ_REMOVE(&attr->others, a, attr_l);
		free(a->data);
		free(a);
	}
}

/* aspath specific functions */

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
aspath_verify(void *data, u_int16_t len)
{
	u_int8_t	*seg = data;
	u_int16_t	 seg_size;
	u_int8_t	 seg_len, seg_type;

	if (len & 1)
		/* odd length aspath are invalid */
		return AS_ERR_BAD;

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
	}
	return 0;	/* aspath is valid but probably not loop free */
}

struct aspath *
aspath_create(void *data, u_int16_t len)
{
	struct aspath	*aspath;

	/* The aspath must already have been checked for correctness. */
	aspath = malloc(ASPATH_HEADER_SIZE + len);
	if (aspath == NULL)
		fatal("aspath_create");
	aspath->hdr.len = len;
	memcpy(aspath->data, data, len);

	aspath->hdr.as_cnt = aspath_count(aspath);
	aspath->hdr.prepend = 0;

	return aspath;
}

int
aspath_write(void *p, u_int16_t len, struct aspath *aspath, u_int16_t myAS,
    int ebgp)
{
	u_char		*b = p;
	int		 tot_len, as_len, prepend, size, wpos = 0;
	u_int16_t	 tmp;
	u_int8_t	 type, attr_flag = ATTR_WELL_KNOWN;

	prepend = aspath->hdr.prepend + (ebgp ? 1 : 0);

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
	/* only the aspath needs to be freed */
	if (aspath == NULL) return;
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
aspath_neighbor(struct aspath *aspath)
{
	/*
	 * Empty aspath is OK -- internal as route.
	 * But what is the neighbor? For now let's return 0 that
	 * should not break anything.
	 */

	if (aspath->hdr.len == 0)
		return 0;

	ENSURE(aspath->hdr.len > 2);
	return aspath_extract(aspath->data, 0);
}

int
aspath_loopfree(struct aspath *aspath, u_int16_t myAS)
{
	u_int8_t	*seg;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len, seg_type;

	seg = aspath->data;
	for (len = aspath->hdr.len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		ENSURE(seg_size <= len);
		for (i = 0; i < seg_len; i++) {
			if (myAS == aspath_extract(seg, i))
				return 0;
		}
	}
	return 1;
}

#define AS_HASH_INITIAL 8271

u_int32_t
aspath_hash(struct aspath *aspath)
{
	u_int8_t	*seg;
	u_int32_t	 hash;
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

int
aspath_snprint(char *buf, size_t size, void *data, u_int16_t len)
{
#define UPDATE()				\
	do {					\
		if (r == -1)			\
			return (-1);		\
		total_size += r;		\
		if ((unsigned int)r < size) {	\
			size -= r;		\
			buf += r;		\
		} else {			\
			buf += size;		\
			size = 0;		\
		}				\
	} while (0)
	u_int8_t	*seg;
	int		 r, total_size;
	u_int16_t	 seg_size;
	u_int8_t	 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		if (seg_type == AS_SET) {
			if (total_size != 0)
				r = snprintf(buf, size, " { ");
			else
				r = snprintf(buf, size, "{ ");
			UPDATE();
		} else if (total_size != 0) {
			r = snprintf(buf, size, " ");
			UPDATE();
		}

		ENSURE(seg_size <= len);
		for (i = 0; i < seg_len; i++) {
			r = snprintf(buf, size, "%hu", aspath_extract(seg, i));
			UPDATE();
			if (i + 1 < seg_len) {
				r = snprintf(buf, size, " ");
				UPDATE();
			}
		}
		if (seg_type == AS_SET) {
			r = snprintf(buf, size, " }");
			UPDATE();
		}
	}
	/* ensure that we have a valid C-string */
	if (size > 0)
		*buf = '\0';

	return total_size;
#undef UPDATE
}

int
aspath_asprint(char **ret, void *data, u_int16_t len)
{
	size_t	slen, plen;

	slen = aspath_strlen(data, len) + 1;
	*ret = malloc(slen);
	if (*ret == NULL)
		return (-1);
	plen = aspath_snprint(*ret, slen, data, len);
	ENSURE(plen < slen);

	return (plen);
}

size_t
aspath_strlen(void *data, u_int16_t len)
{
	u_int8_t	*seg;
	int		 total_size;
	u_int16_t	 as, seg_size;
	u_int8_t	 i, seg_type, seg_len;

	total_size = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		if (seg_type == AS_SET)
			if (total_size != 0)
				total_size += 3;
			else
				total_size += 2;
		else if (total_size != 0)
			total_size += 1;

		ENSURE(seg_size <= len);
		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);
			if (as >= 10000)
				total_size += 5;
			else if (as >= 1000)
				total_size += 4;
			else if (as >= 100)
				total_size += 3;
			else if (as >= 10)
				total_size += 2;
			else
				total_size += 1;

			if (i + 1 < seg_len)
				total_size += 1;
		}

		if (seg_type == AS_SET)
			total_size += 2;
	}
	return total_size;
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
		if (a->hdr.len == 0)
			return (1);
		else
			return (0);
	}

	final = 0;
	seg = a->data;
	for (len = a->hdr.len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		ENSURE(seg_type == AS_SET || seg_type == AS_SEQUENCE);
		seg_size = 2 + 2 * seg_len;

		final = (len == seg_size);

		if (type == AS_SOURCE && !final)
			/* not yet in the final segment */
			continue;

		ENSURE(seg_size <= len);
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

	ENSURE((len & 0x3) == 0);
	len >>= 2; /* devide by four */

	for (; len > 0; len--) {
		eas = *p++;
		eas <<= 8;
		eas |= *p++;
		etype = *p++;
		etype <<= 8;
		etype |= *p++;
		if ((as == COMMUNITY_ANY || (u_int16_t)as == eas) &&
		    (type == COMMUNITY_ANY || type == etype))
			return 1;
	}
	return 0;
}

