/*	$OpenBSD: rde_attr.c,v 1.100 2017/05/31 10:44:00 claudio Exp $ */

/*
 * Copyright (c) 2004 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2016 Job Snijders <job@instituut.net>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

int
attr_write(void *p, u_int16_t p_len, u_int8_t flags, u_int8_t type,
    void *data, u_int16_t data_len)
{
	u_char		*b = p;
	u_int16_t	 tmp, tot_len = 2; /* attribute header (without len) */

	flags &= ~ATTR_DEFMASK;
	if (data_len > 255) {
		tot_len += 2 + data_len;
		flags |= ATTR_EXTLEN;
	} else {
		tot_len += 1 + data_len;
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

int
attr_writebuf(struct ibuf *buf, u_int8_t flags, u_int8_t type, void *data,
    u_int16_t data_len)
{
	u_char	hdr[4];

	flags &= ~ATTR_DEFMASK;
	if (data_len > 255) {
		flags |= ATTR_EXTLEN;
		hdr[2] = (data_len >> 8) & 0xff;
		hdr[3] = data_len & 0xff;
	} else {
		hdr[2] = data_len & 0xff;
	}

	hdr[0] = flags;
	hdr[1] = type;

	if (ibuf_add(buf, hdr, flags & ATTR_EXTLEN ? 4 : 3) == -1)
		return (-1);
	if (ibuf_add(buf, data, data_len) == -1)
		return (-1);
	return (0);
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

SIPHASH_KEY attrtablekey;

#define ATTR_HASH(x)				\
	&attrtable.hashtbl[(x) & attrtable.hashmask]

void
attr_init(u_int32_t hashsize)
{
	u_int32_t	hs, i;

	arc4random_buf(&attrtablekey, sizeof(attrtablekey));
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
	void		*p;

	/* known optional attributes were validated previously */
	if ((a = attr_lookup(flags, type, data, len)) == NULL)
		a = attr_alloc(flags, type, data, len);

	/* attribute allowed only once */
	for (l = 0; l < asp->others_len; l++) {
		if (asp->others[l] == NULL)
			break;
		if (type == asp->others[l]->type) {
			if (a->refcnt == 0)
				attr_put(a);
			return (-1);
		}
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
	if (asp->others_len == UCHAR_MAX)
		fatalx("attr_optadd: others_len overflow");

	asp->others_len++;
	if ((p = reallocarray(asp->others,
	    asp->others_len, sizeof(struct attr *))) == NULL)
		fatal("attr_optadd");
	asp->others = p;

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
	SIPHASH_CTX	ctx;

	a = calloc(1, sizeof(struct attr));
	if (a == NULL)
		fatal("attr_optadd");
	rdemem.attr_cnt++;

	flags &= ~ATTR_DEFMASK;	/* normalize mask */
	a->flags = flags;
	a->type = type;
	a->len = len;
	if (len != 0) {
		if ((a->data = malloc(len)) == NULL)
			fatal("attr_optadd");

		rdemem.attr_dcnt++;
		rdemem.attr_data += len;
		memcpy(a->data, data, len);
	} else
		a->data = NULL;

	SipHash24_Init(&ctx, &attrtablekey);
	SipHash24_Update(&ctx, &flags, sizeof(flags));
	SipHash24_Update(&ctx, &type, sizeof(type));
	SipHash24_Update(&ctx, &len, sizeof(len));
	SipHash24_Update(&ctx, a->data, a->len);
	a->hash = SipHash24_End(&ctx);
	LIST_INSERT_HEAD(ATTR_HASH(a->hash), a, entry);

	return (a);
}

struct attr *
attr_lookup(u_int8_t flags, u_int8_t type, const void *data, u_int16_t len)
{
	struct attr_list	*head;
	struct attr		*a;
	u_int32_t		 hash;
	SIPHASH_CTX		ctx;

	flags &= ~ATTR_DEFMASK;	/* normalize mask */

	SipHash24_Init(&ctx, &attrtablekey);
	SipHash24_Update(&ctx, &flags, sizeof(flags));
	SipHash24_Update(&ctx, &type, sizeof(type));
	SipHash24_Update(&ctx, &len, sizeof(len));
	SipHash24_Update(&ctx, data, len);
	hash = SipHash24_End(&ctx);
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

u_int16_t	 aspath_countlength(struct aspath *, u_int16_t, int);
void		 aspath_countcopy(struct aspath *, u_int16_t, u_int8_t *,
		     u_int16_t, int);
struct aspath	*aspath_lookup(const void *, u_int16_t);

struct aspath_table {
	struct aspath_list	*hashtbl;
	u_int32_t		 hashmask;
} astable;

SIPHASH_KEY astablekey;

#define ASPATH_HASH(x)				\
	&astable.hashtbl[(x) & astable.hashmask]

int
aspath_verify(void *data, u_int16_t len, int as4byte)
{
	u_int8_t	*seg = data;
	u_int16_t	 seg_size, as_size = 2;
	u_int8_t	 seg_len, seg_type;
	int		 i, error = 0;

	if (len & 1)
		/* odd length aspath are invalid */
		return (AS_ERR_BAD);

	if (as4byte)
		as_size = 4;

	for (; len > 0; len -= seg_size, seg += seg_size) {
		if (len < 2)	/* header length check */
			return (AS_ERR_BAD);
		seg_type = seg[0];
		seg_len = seg[1];

		/*
		 * BGP confederations should not show up but consider them
		 * as a soft error which invalidates the path but keeps the
		 * bgp session running.
		 */
		if (seg_type == AS_CONFED_SEQUENCE || seg_type == AS_CONFED_SET)
			error = AS_ERR_SOFT;
		if (seg_type != AS_SET && seg_type != AS_SEQUENCE &&
		    seg_type != AS_CONFED_SEQUENCE && seg_type != AS_CONFED_SET)
			return (AS_ERR_TYPE);

		seg_size = 2 + as_size * seg_len;

		if (seg_size > len)
			return (AS_ERR_LEN);

		if (seg_size == 0)
			/* empty aspath segments are not allowed */
			return (AS_ERR_BAD);

		 /* RFC 7607 - AS 0 is considered malformed */
		for (i = 0; i < seg_len; i++) {
			if (aspath_extract(seg, i) == 0)
				return (AS_ERR_SOFT);
		}
	}
	return (error);	/* aspath is valid but probably not loop free */
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
	arc4random_buf(&astablekey, sizeof(astablekey));
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
		head = ASPATH_HASH(SipHash24(&astablekey, aspath->data,
		    aspath->len));
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
aspath_inflate(void *data, u_int16_t len, u_int16_t *newlen)
{
	u_int8_t	*seg, *nseg, *ndata;
	u_int16_t	 seg_size, olen, nlen;
	u_int8_t	 seg_len;

	/* first calculate the length of the aspath */
	seg = data;
	nlen = 0;
	for (olen = len; olen > 0; olen -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int16_t) * seg_len;
		nlen += 2 + sizeof(u_int32_t) * seg_len;

		if (seg_size > olen)
			fatalx("aspath_inflate: would overflow");
	}

	*newlen = nlen;
	if ((ndata = malloc(nlen)) == NULL)
		fatal("aspath_inflate");

	/* then copy the aspath */
	seg = data;
	for (nseg = ndata; nseg < ndata + nlen; ) {
		*nseg++ = *seg++;
		*nseg++ = seg_len = *seg++;
		for (; seg_len > 0; seg_len--) {
			*nseg++ = 0;
			*nseg++ = 0;
			*nseg++ = *seg++;
			*nseg++ = *seg++;
		}
	}

	return (ndata);
}

/* convert a 4 byte aspath to a 2byte one. data is freed by aspath_deflate */
u_char *
aspath_deflate(u_char *data, u_int16_t *len, int *flagnew)
{
	u_int8_t	*seg, *nseg, *ndata;
	u_int32_t	 as;
	int		 i;
	u_int16_t	 seg_size, olen, nlen;
	u_int8_t	 seg_len;

	/* first calculate the length of the aspath */
	nlen = 0;
	seg = data;
	olen = *len;
	for (; olen > 0; olen -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;
		nlen += 2 + sizeof(u_int16_t) * seg_len;

		if (seg_size > olen)
			fatalx("aspath_deflate: would overflow");
	}

	if ((ndata = malloc(nlen)) == NULL)
		fatal("aspath_deflate");

	/* then copy the aspath */
	seg = data;
	olen = *len;
	for (nseg = ndata; seg < data + olen; seg += seg_size) {
		*nseg++ = seg[0];
		*nseg++ = seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);
			if (as > USHRT_MAX) {
				as = AS_TRANS;
				*flagnew = 1;
			}
			*nseg++ = (as >> 8) & 0xff;
			*nseg++ = as & 0xff;
		}
	}

	free(data);
	*len = nlen;
	return (ndata);
}

void
aspath_merge(struct rde_aspath *a, struct attr *attr)
{
	u_int8_t	*np;
	u_int16_t	 ascnt, diff, nlen, difflen;
	int		 hroom = 0;

	ascnt = aspath_count(attr->data, attr->len);
	if (ascnt > a->aspath->ascnt) {
		/* ASPATH is shorter then AS4_PATH no way to merge */
		attr_free(a, attr);
		return;
	}

	diff = a->aspath->ascnt - ascnt;
	if (diff && attr->len > 2 && attr->data[0] == AS_SEQUENCE)
		hroom = attr->data[1];
	difflen = aspath_countlength(a->aspath, diff, hroom);
	nlen = attr->len + difflen;

	if ((np = malloc(nlen)) == NULL)
		fatal("aspath_merge");

	/* copy head from old aspath */
	aspath_countcopy(a->aspath, diff, np, difflen, hroom);

	/* copy tail from new aspath */
	if (hroom > 0)
		memcpy(np + nlen - attr->len + 2, attr->data + 2,
		    attr->len - 2);
	else
		memcpy(np + nlen - attr->len, attr->data, attr->len);

	aspath_put(a->aspath);
	a->aspath = aspath_get(np, nlen);
	free(np);
	attr_free(a, attr);
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
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt += 1;
		else
			cnt += seg_len;

		if (seg_size > len)
			fatalx("aspath_count: would overflow");
	}
	return (cnt);
}

u_int16_t
aspath_countlength(struct aspath *aspath, u_int16_t cnt, int headcnt)
{
	const u_int8_t	*seg;
	u_int16_t	 seg_size, len, clen;
	u_int8_t	 seg_type = 0, seg_len = 0;

	seg = aspath->data;
	clen = 0;
	for (len = aspath->len; len > 0 && cnt > 0;
	    len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt -= 1;
		else if (seg_len > cnt) {
			seg_len = cnt;
			clen += 2 + sizeof(u_int32_t) * cnt;
			break;
		} else
			cnt -= seg_len;

		clen += seg_size;

		if (seg_size > len)
			fatalx("aspath_countlength: would overflow");
	}
	if (headcnt > 0 && seg_type == AS_SEQUENCE && headcnt + seg_len < 256)
		/* no need for additional header from the new aspath. */
		clen -= 2;

	return (clen);
}

void
aspath_countcopy(struct aspath *aspath, u_int16_t cnt, u_int8_t *buf,
    u_int16_t size, int headcnt)
{
	const u_int8_t	*seg;
	u_int16_t	 seg_size, len;
	u_int8_t	 seg_type, seg_len;

	if (headcnt > 0)
		/*
		 * additional room because we steal the segment header
		 * from the other aspath
		 */
		size += 2;
	seg = aspath->data;
	for (len = aspath->len; len > 0 && cnt > 0;
	    len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt -= 1;
		else if (seg_len > cnt) {
			seg_len = cnt + headcnt;
			seg_size = 2 + sizeof(u_int32_t) * cnt;
			cnt = 0;
		} else {
			cnt -= seg_len;
			if (cnt == 0)
				seg_len += headcnt;
		}

		memcpy(buf, seg, seg_size);
		buf[0] = seg_type;
		buf[1] = seg_len;
		buf += seg_size;
		if (size < seg_size)
			fatalx("aspath_countlength: would overflow");
		size -= seg_size;
	}
}

u_int32_t
aspath_neighbor(struct aspath *aspath)
{
	/* Empty aspath is OK -- internal AS route. */
	if (aspath->len == 0)
		return (rde_local_as());
	return (aspath_extract(aspath->data, 0));
}

int
aspath_loopfree(struct aspath *aspath, u_int32_t myAS)
{
	u_int8_t	*seg;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len;

	seg = aspath->data;
	for (len = aspath->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			if (myAS == aspath_extract(seg, i))
				return (0);
		}

		if (seg_size > len)
			fatalx("aspath_loopfree: would overflow");
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

	hash = SipHash24(&astablekey, data, len);
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
u_char *
aspath_prepend(struct aspath *asp, u_int32_t as, int quantum, u_int16_t *len)
{
	u_char		*p;
	int		 l, overflow = 0, shift = 0, size, wpos = 0;
	u_int8_t	 type;

	/* lunatic prepends are blocked in the parser and limited */

	/* first calculate new size */
	if (asp->len > 0) {
		if (asp->len < 2)
			fatalx("aspath_prepend: bad aspath length");
		type = asp->data[0];
		size = asp->data[1];
	} else {
		/* empty as path */
		type = AS_SET;
		size = 0;
	}

	if (quantum > 255)
		fatalx("aspath_prepend: preposterous prepend");
	if (quantum == 0) {
		/* no change needed but return a copy */
		p = malloc(asp->len);
		if (p == NULL)
			fatal("aspath_prepend");
		memcpy(p, asp->data, asp->len);
		*len = asp->len;
		return (p);
	} else if (type == AS_SET || size + quantum > 255) {
		/* need to attach a new AS_SEQUENCE */
		l = 2 + quantum * sizeof(u_int32_t) + asp->len;
		if (type == AS_SET)
			overflow = quantum;
		else
			overflow = size + quantum - 255;
	} else
		l = quantum * sizeof(u_int32_t) + asp->len;

	quantum -= overflow;

	p = malloc(l);
	if (p == NULL)
		fatal("aspath_prepend");

	/* first prepends */
	as = htonl(as);
	if (overflow > 0) {
		p[wpos++] = AS_SEQUENCE;
		p[wpos++] = overflow;

		for (; overflow > 0; overflow--) {
			memcpy(p + wpos, &as, sizeof(u_int32_t));
			wpos += sizeof(u_int32_t);
		}
	}
	if (quantum > 0) {
		shift = 2;
		p[wpos++] = AS_SEQUENCE;
		p[wpos++] = quantum + size;

		for (; quantum > 0; quantum--) {
			memcpy(p + wpos, &as, sizeof(u_int32_t));
			wpos += sizeof(u_int32_t);
		}
	}
	memcpy(p + wpos, asp->data + shift, asp->len - shift);

	*len = l;
	return (p);
}

int
aspath_lenmatch(struct aspath *a, enum aslen_spec type, u_int aslen)
{
	u_int8_t	*seg;
	u_int32_t	 as, lastas = 0;
	u_int		 count = 0;
	u_int16_t	 len, seg_size;
	u_int8_t	 i, seg_len;

	if (type == ASLEN_MAX) {
		if (aslen < aspath_count(a->data, a->len))
			return (1);
		else
			return (0);
	}

	/* type == ASLEN_SEQ */
	seg = a->data;
	for (len = a->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(u_int32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			/* what should we do with AS_SET? */
			as = aspath_extract(seg, i);
			if (as == lastas) {
				if (aslen < ++count)
					return (1);
			} else
				count = 1;
			lastas = as;
		}
	}
	return (0);
}

/*
 * Functions handling communities and extended communities.
 */

int community_ext_matchone(struct filter_extcommunity *, u_int16_t, u_int64_t);

int
community_match(struct rde_aspath *asp, int as, int type)
{
	struct attr	*a;
	u_int8_t	*p;
	u_int16_t	 eas, etype, len;

	a = attr_optget(asp, ATTR_COMMUNITIES);
	if (a == NULL)
		/* no communities, no match */
		return (0);

	p = a->data;
	for (len = a->len / 4; len > 0; len--) {
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

	attr = attr_optget(asp, ATTR_COMMUNITIES);
	if (attr != NULL) {
		p = attr->data;
		ncommunities = attr->len / 4;
	}

	/* first check if the community is not already set */
	for (i = 0; i < ncommunities; i++) {
		if (as >> 8 == p[0] && (as & 0xff) == p[1] &&
		    type >> 8 == p[2] && (type & 0xff) == p[3])
			/* already present, nothing todo */
			return (1);
		p += 4;
	}

	if (ncommunities++ >= USHRT_MAX / 4)
		/* overflow */
		return (0);

	if ((p = reallocarray(NULL, ncommunities, 4)) == NULL)
		fatal("community_set");

	p[0] = as >> 8;
	p[1] = as & 0xff;
	p[2] = type >> 8;
	p[3] = type & 0xff;

	if (attr != NULL) {
		memcpy(p + 4, attr->data, attr->len);
		f = attr->flags;
		attr_free(asp, attr);
	}

	attr_optadd(asp, f, ATTR_COMMUNITIES, p, ncommunities * 4);

	free(p);
	return (1);
}

void
community_delete(struct rde_aspath *asp, int as, int type)
{
	struct attr	*attr;
	u_int8_t	*p, *n;
	u_int16_t	 l, len = 0;
	u_int16_t	 eas, etype;
	u_int8_t	 f;

	attr = attr_optget(asp, ATTR_COMMUNITIES);
	if (attr == NULL)
		/* no attr nothing to do */
		return;

	p = attr->data;
	for (l = 0; l < attr->len; l += 4) {
		eas = *p++;
		eas <<= 8;
		eas |= *p++;
		etype = *p++;
		etype <<= 8;
		etype |= *p++;

		if ((as == COMMUNITY_ANY || (u_int16_t)as == eas) &&
		    (type == COMMUNITY_ANY || (u_int16_t)type == etype))
			/* match */
			continue;
		len += 4;
	}

	if (len == 0) {
		attr_free(asp, attr);
		return;
	}

	if ((n = malloc(len)) == NULL)
		fatal("community_delete");

	p = attr->data;
	for (l = 0; l < len && p < attr->data + attr->len; ) {
		eas = *p++;
		eas <<= 8;
		eas |= *p++;
		etype = *p++;
		etype <<= 8;
		etype |= *p++;

		if ((as == COMMUNITY_ANY || (u_int16_t)as == eas) &&
		    (type == COMMUNITY_ANY || (u_int16_t)type == etype))
			/* match */
			continue;
		n[l++] = eas >> 8;
		n[l++] = eas & 0xff;
		n[l++] = etype >> 8;
		n[l++] = etype & 0xff;
	}

	f = attr->flags;

	attr_free(asp, attr);
	attr_optadd(asp, f, ATTR_COMMUNITIES, n, len);
	free(n);
}

int
community_ext_match(struct rde_aspath *asp, struct filter_extcommunity *c,
    u_int16_t neighas)
{
	struct attr	*attr;
	u_int8_t	*p;
	u_int64_t	 ec;
	u_int16_t	 len;

	attr = attr_optget(asp, ATTR_EXT_COMMUNITIES);
	if (attr == NULL)
		/* no communities, no match */
		return (0);

	p = attr->data;
	for (len = attr->len / sizeof(ec); len > 0; len--) {
		memcpy(&ec, p, sizeof(ec));
		if (community_ext_matchone(c, neighas, ec))
			return (1);
		p += sizeof(ec);
	}

	return (0);
}

int
community_ext_set(struct rde_aspath *asp, struct filter_extcommunity *c,
    u_int16_t neighas)
{
	struct attr	*attr;
	u_int8_t	*p = NULL;
	u_int64_t	 community;
	unsigned int	 i, ncommunities = 0;
	u_int8_t	 f = ATTR_OPTIONAL|ATTR_TRANSITIVE;

	if (community_ext_conv(c, neighas, &community))
		return (0);

	attr = attr_optget(asp, ATTR_EXT_COMMUNITIES);
	if (attr != NULL) {
		p = attr->data;
		ncommunities = attr->len / sizeof(community);
	}

	/* first check if the community is not already set */
	for (i = 0; i < ncommunities; i++) {
		if (memcmp(&community, p, sizeof(community)) == 0)
			/* already present, nothing todo */
			return (1);
		p += sizeof(community);
	}

	if (ncommunities++ >= USHRT_MAX / sizeof(community))
		/* overflow */
		return (0);

	if ((p = reallocarray(NULL, ncommunities, sizeof(community))) == NULL)
		fatal("community_ext_set");

	memcpy(p, &community, sizeof(community));
	if (attr != NULL) {
		memcpy(p + sizeof(community), attr->data, attr->len);
		f = attr->flags;
		attr_free(asp, attr);
	}

	attr_optadd(asp, f, ATTR_EXT_COMMUNITIES, p,
	    ncommunities * sizeof(community));

	free(p);
	return (1);
}

void
community_ext_delete(struct rde_aspath *asp, struct filter_extcommunity *c,
    u_int16_t neighas)
{
	struct attr	*attr;
	u_int8_t	*p, *n;
	u_int64_t	 community;
	u_int16_t	 l, len = 0;
	u_int8_t	 f;

	if (community_ext_conv(c, neighas, &community))
		return;

	attr = attr_optget(asp, ATTR_EXT_COMMUNITIES);
	if (attr == NULL)
		/* no attr nothing to do */
		return;

	p = attr->data;
	for (l = 0; l < attr->len; l += sizeof(community)) {
		if (memcmp(&community, p + l, sizeof(community)) == 0)
			/* match */
			continue;
		len += sizeof(community);
	}

	if (len == 0) {
		attr_free(asp, attr);
		return;
	}

	if ((n = malloc(len)) == NULL)
		fatal("community_delete");

	p = attr->data;
	for (l = 0; l < len && p < attr->data + attr->len;
	    p += sizeof(community)) {
		if (memcmp(&community, p, sizeof(community)) == 0)
			/* match */
			continue;
		memcpy(n + l, p, sizeof(community));
		l += sizeof(community);
	}

	f = attr->flags;

	attr_free(asp, attr);
	attr_optadd(asp, f, ATTR_EXT_COMMUNITIES, n, len);
	free(n);
}

int
community_ext_conv(struct filter_extcommunity *c, u_int16_t neighas,
    u_int64_t *community)
{
	u_int64_t	com;
	u_int32_t	ip;

	com = (u_int64_t)c->type << 56;
	switch (c->type & EXT_COMMUNITY_VALUE) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		com |= (u_int64_t)c->subtype << 48;
		com |= (u_int64_t)c->data.ext_as.as << 32;
		com |= c->data.ext_as.val;
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		com |= (u_int64_t)c->subtype << 48;
		ip = ntohl(c->data.ext_ip.addr.s_addr);
		com |= (u_int64_t)ip << 16;
		com |= c->data.ext_ip.val;
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		com |= (u_int64_t)c->subtype << 48;
		com |= (u_int64_t)c->data.ext_as4.as4 << 16;
		com |= c->data.ext_as4.val;
		break;
	case EXT_COMMUNITY_TRANS_OPAQUE:
		com |= (u_int64_t)c->subtype << 48;
		com |= c->data.ext_opaq & EXT_COMMUNITY_OPAQUE_MAX;
		break;
	default:
		com |= c->data.ext_opaq & 0xffffffffffffffULL;
		break;
	}

	*community = htobe64(com);

	return (0);
}

int
community_ext_matchone(struct filter_extcommunity *c, u_int16_t neighas,
    u_int64_t community)
{
	u_int64_t	com, mask;
	u_int32_t	ip;

	community = betoh64(community);

	com = (u_int64_t)c->type << 56;
	mask = 0xffULL << 56;
	if ((com & mask) != (community & mask))
		return (0);

	switch (c->type & EXT_COMMUNITY_VALUE) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
	case EXT_COMMUNITY_TRANS_IPV4:
	case EXT_COMMUNITY_TRANS_FOUR_AS:
	case EXT_COMMUNITY_TRANS_OPAQUE:
		com = (u_int64_t)c->subtype << 48;
		mask = 0xffULL << 48;
		if ((com & mask) != (community & mask))
			return (0);
		break;
	default:
		com = c->data.ext_opaq & 0xffffffffffffffULL;
		mask = 0xffffffffffffffULL;
		if ((com & mask) == (community & mask))
			return (1);
		return (0);
	}


	switch (c->type & EXT_COMMUNITY_VALUE) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		com = (u_int64_t)c->data.ext_as.as << 32;
		mask = 0xffffULL << 32;
		if ((com & mask) != (community & mask))
			return (0);

		com = c->data.ext_as.val;
		mask = 0xffffffffULL;
		if ((com & mask) == (community & mask))
			return (1);
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		ip = ntohl(c->data.ext_ip.addr.s_addr);
		com = (u_int64_t)ip << 16;
		mask = 0xffffffff0000ULL;
		if ((com & mask) != (community & mask))
			return (0);

		com = c->data.ext_ip.val;
		mask = 0xffff;
		if ((com & mask) == (community & mask))
			return (1);
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		com = (u_int64_t)c->data.ext_as4.as4 << 16;
		mask = 0xffffffffULL << 16;
		if ((com & mask) != (community & mask))
			return (0);

		com = c->data.ext_as4.val;
		mask = 0xffff;
		if ((com & mask) == (community & mask))
			return (1);
		break;
	case EXT_COMMUNITY_TRANS_OPAQUE:
		com = c->data.ext_opaq & EXT_COMMUNITY_OPAQUE_MAX;
		mask = EXT_COMMUNITY_OPAQUE_MAX;
		if ((com & mask) == (community & mask))
			return (1);
		break;
	}

	return (0);
}

int
community_large_match(struct rde_aspath *asp, int64_t as, int64_t ld1,
    int64_t ld2)
{
	struct wire_largecommunity	*bar;
	struct attr	*a;
	u_int8_t	*p;
	u_int16_t	 len;
	u_int32_t	 eas, eld1, eld2;

	a = attr_optget(asp, ATTR_LARGE_COMMUNITIES);
	if (a == NULL)
		/* no communities, no match */
		return (0);

	p = a->data;
	for (len = a->len / 12; len > 0; len--) {
		bar = (struct wire_largecommunity *)p;
		p += 12;
		eas = betoh32(bar->as);
		eld1 = betoh32(bar->ld1);
		eld2 = betoh32(bar->ld2);

		if ((as == COMMUNITY_ANY || as == eas) &&
		    (ld1 == COMMUNITY_ANY || ld1 == eld1) &&
		    (ld2 == COMMUNITY_ANY || ld2 == eld2))
			return (1);
	}
	return (0);
}

int
community_large_set(struct rde_aspath *asp, int64_t as, int64_t ld1,
    int64_t ld2)
{
	struct wire_largecommunity	*bar;
	struct attr	*attr;
	u_int8_t	*p = NULL;
	unsigned int	 i, ncommunities = 0;
	u_int8_t	 f = ATTR_OPTIONAL|ATTR_TRANSITIVE;

	attr = attr_optget(asp, ATTR_LARGE_COMMUNITIES);
	if (attr != NULL) {
		p = attr->data;
		ncommunities = attr->len / 12;
	}

	/* first check if the community is not already set */
	for (i = 0; i < ncommunities; i++) {
		bar = (struct wire_largecommunity *)p;
		if (bar->as == as && bar->ld1 == ld1 && bar->ld2 == ld2)
			/* already present, nothing todo */
			return (1);
		p += 12;
	}

	if (ncommunities++ >= USHRT_MAX / 12)
		/* overflow */
		return (0);

	if ((p = reallocarray(NULL, ncommunities, 12)) == NULL)
		fatal("community_set");

	bar = (struct wire_largecommunity *)p;
	bar->as = htobe32(as);
	bar->ld1 = htobe32(ld1);
	bar->ld2 = htobe32(ld2);

	if (attr != NULL) {
		memcpy(p + 12, attr->data, attr->len);
		f = attr->flags;
		attr_free(asp, attr);
	}

	attr_optadd(asp, f, ATTR_LARGE_COMMUNITIES, p, ncommunities * 12);

	free(p);
	return (1);
}

void
community_large_delete(struct rde_aspath *asp, int64_t as, int64_t ld1,
    int64_t ld2)
{
	struct wire_largecommunity	*bar;
	struct attr	*attr;
	u_int8_t	*p, *n;
	u_int16_t	 l = 0, len = 0;
	u_int32_t	 eas, eld1, eld2;
	u_int8_t	 f;

	attr = attr_optget(asp, ATTR_LARGE_COMMUNITIES);
	if (attr == NULL)
		/* no attr nothing to do */
		return;

	p = attr->data;
	for (len = 0; l < attr->len; l += 12) {
		bar = (struct wire_largecommunity *)p;
		p += 12;
		eas = betoh32(bar->as);
		eld1 = betoh32(bar->ld1);
		eld2 = betoh32(bar->ld2);

		if ((as == COMMUNITY_ANY || as == eas) &&
		    (ld1 == COMMUNITY_ANY || ld1 == eld1) &&
		    (ld2 == COMMUNITY_ANY || ld2 == eld2))
			/* match */
			continue;
		len += 12;
	}

	if (len == 0) {
		attr_free(asp, attr);
		return;
	}

	if ((n = malloc(len)) == NULL)
		fatal("community_delete");

	p = attr->data;
	for (l = 0; l < len && p < attr->data + attr->len; ) {
		bar = (struct wire_largecommunity *)p;
		p += 12;
		eas = betoh32(bar->as);
		eld1 = betoh32(bar->ld1);
		eld2 = betoh32(bar->ld2);

		if ((as == COMMUNITY_ANY || as == eas) &&
		    (ld1 == COMMUNITY_ANY || ld1 == eld1) &&
		    (ld2 == COMMUNITY_ANY || ld2 == eld2))
			/* match */
			continue;
		memcpy(n + l, bar, sizeof(*bar));
		l += 12;
	}

	f = attr->flags;

	attr_free(asp, attr);
	attr_optadd(asp, f, ATTR_LARGE_COMMUNITIES, n, len);
	free(n);
}


u_char *
community_ext_delete_non_trans(u_char *data, u_int16_t len, u_int16_t *newlen)
{
	u_int8_t	*ext = data, *newdata;
	u_int16_t	l, nlen = 0;

	for (l = 0; l < len; l += sizeof(u_int64_t)) {
		if (!(ext[l] & EXT_COMMUNITY_TRANSITIVE))
			nlen += sizeof(u_int64_t);
	}

	if (nlen == 0) {
		*newlen = 0;
		return NULL;
	}

	newdata = malloc(nlen);
	if (newdata == NULL)
		fatal("%s", __func__);;

	for (l = 0, nlen = 0; l < len; l += sizeof(u_int64_t)) {
		if (!(ext[l] & EXT_COMMUNITY_TRANSITIVE)) {
			memcpy(newdata + nlen, ext + l, sizeof(u_int64_t));
			nlen += sizeof(u_int64_t);
		}
	}

	*newlen = nlen;
	return newdata;
}
