/*	$OpenBSD: rde_attr.c,v 1.2 2004/02/09 01:56:18 henning Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "ensure.h"
#include "rde.h"

/* attribute specific functions */
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
	/* only the aspath needs to be freed */
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

