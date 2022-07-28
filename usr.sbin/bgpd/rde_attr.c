/*	$OpenBSD: rde_attr.c,v 1.127 2022/07/28 13:11:51 deraadt Exp $ */

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

#include <sys/queue.h>

#include <endian.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <siphash.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

int
attr_write(void *p, uint16_t p_len, uint8_t flags, uint8_t type,
    void *data, uint16_t data_len)
{
	u_char		*b = p;
	uint16_t	 tmp, tot_len = 2; /* attribute header (without len) */

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

	if (data == NULL)
		return (tot_len - data_len);

	if (data_len != 0)
		memcpy(b, data, data_len);

	return (tot_len);
}

int
attr_writebuf(struct ibuf *buf, uint8_t flags, uint8_t type, void *data,
    uint16_t data_len)
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
	if (data && ibuf_add(buf, data, data_len) == -1)
		return (-1);
	return (0);
}

/* optional attribute specific functions */
int		 attr_diff(struct attr *, struct attr *);
struct attr	*attr_alloc(uint8_t, uint8_t, const void *, uint16_t);
struct attr	*attr_lookup(uint8_t, uint8_t, const void *, uint16_t);
void		 attr_put(struct attr *);

struct attr_table {
	struct attr_list	*hashtbl;
	uint64_t		 hashmask;
} attrtable;

SIPHASH_KEY attrtablekey;

#define ATTR_HASH(x)				\
	&attrtable.hashtbl[(x) & attrtable.hashmask]

void
attr_init(uint32_t hashsize)
{
	uint32_t	hs, i;

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
	uint64_t	i;

	for (i = 0; i <= attrtable.hashmask; i++)
		if (!LIST_EMPTY(&attrtable.hashtbl[i]))
			log_warnx("%s: free non-free table", __func__);

	free(attrtable.hashtbl);
}

void
attr_hash_stats(struct rde_hashstats *hs)
{
	struct attr		*a;
	uint64_t		i;
	int64_t			n;

	memset(hs, 0, sizeof(*hs));
	strlcpy(hs->name, "attr hash", sizeof(hs->name));
	hs->min = LLONG_MAX;
	hs->num = attrtable.hashmask + 1;

	for (i = 0; i <= attrtable.hashmask; i++) {
		n = 0;
		LIST_FOREACH(a, &attrtable.hashtbl[i], entry)
			n++;
		if (n < hs->min)
			hs->min = n;
		if (n > hs->max)
			hs->max = n;
		hs->sum += n;
		hs->sumq += n * n;
	}
}

int
attr_optadd(struct rde_aspath *asp, uint8_t flags, uint8_t type,
    void *data, uint16_t len)
{
	uint8_t		 l;
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
attr_optget(const struct rde_aspath *asp, uint8_t type)
{
	uint8_t l;

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
attr_copy(struct rde_aspath *t, const struct rde_aspath *s)
{
	uint8_t l;

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
}

int
attr_compare(struct rde_aspath *a, struct rde_aspath *b)
{
	uint8_t l, min;

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

uint64_t
attr_hash(struct rde_aspath *a)
{
	uint64_t hash = 0;
	uint8_t l;

	for (l = 0; l < a->others_len; l++)
		if (a->others[l] != NULL)
			hash ^= a->others[l]->hash;
	return (hash);
}

void
attr_free(struct rde_aspath *asp, struct attr *attr)
{
	uint8_t l;

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
	uint8_t l;

	for (l = 0; l < asp->others_len; l++)
		attr_put(asp->others[l]);

	free(asp->others);
	asp->others = NULL;
	asp->others_len = 0;
}

struct attr *
attr_alloc(uint8_t flags, uint8_t type, const void *data, uint16_t len)
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
attr_lookup(uint8_t flags, uint8_t type, const void *data, uint16_t len)
{
	struct attr_list	*head;
	struct attr		*a;
	uint64_t		 hash;
	SIPHASH_CTX		 ctx;

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

static uint16_t aspath_count(const void *, uint16_t);
static uint32_t aspath_extract_origin(const void *, uint16_t);
static uint16_t aspath_countlength(struct aspath *, uint16_t, int);
static void	 aspath_countcopy(struct aspath *, uint16_t, uint8_t *,
		    uint16_t, int);
struct aspath	*aspath_lookup(const void *, uint16_t);

struct aspath_table {
	struct aspath_list	*hashtbl;
	uint32_t		 hashmask;
} astable;

SIPHASH_KEY astablekey;

#define ASPATH_HASH(x)				\
	&astable.hashtbl[(x) & astable.hashmask]

void
aspath_init(uint32_t hashsize)
{
	uint32_t	hs, i;

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
	uint32_t	i;

	for (i = 0; i <= astable.hashmask; i++)
		if (!LIST_EMPTY(&astable.hashtbl[i]))
			log_warnx("aspath_shutdown: free non-free table");

	free(astable.hashtbl);
}

void
aspath_hash_stats(struct rde_hashstats *hs)
{
	struct aspath		*a;
	uint32_t		i;
	int64_t			n;

	memset(hs, 0, sizeof(*hs));
	strlcpy(hs->name, "aspath hash", sizeof(hs->name));
	hs->min = LLONG_MAX;
	hs->num = astable.hashmask + 1;

	for (i = 0; i <= astable.hashmask; i++) {
		n = 0;
		LIST_FOREACH(a, &astable.hashtbl[i], entry)
			n++;
		if (n < hs->min)
			hs->min = n;
		if (n > hs->max)
			hs->max = n;
		hs->sum += n;
		hs->sumq += n * n;
	}
}

struct aspath *
aspath_get(void *data, uint16_t len)
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
		aspath->source_as = aspath_extract_origin(data, len);
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

/*
 * convert a 4 byte aspath to a 2 byte one.
 */
u_char *
aspath_deflate(u_char *data, uint16_t *len, int *flagnew)
{
	uint8_t	*seg, *nseg, *ndata;
	uint32_t	 as;
	int		 i;
	uint16_t	 seg_size, olen, nlen;
	uint8_t		 seg_len;

	/* first calculate the length of the aspath */
	nlen = 0;
	seg = data;
	olen = *len;
	for (; olen > 0; olen -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;
		nlen += 2 + sizeof(uint16_t) * seg_len;

		if (seg_size > olen)
			fatalx("%s: would overflow", __func__);
	}

	if ((ndata = malloc(nlen)) == NULL)
		fatal("aspath_deflate");

	/* then copy the aspath */
	seg = data;
	olen = *len;
	for (nseg = ndata; seg < data + olen; seg += seg_size) {
		*nseg++ = seg[0];
		*nseg++ = seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

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

	*len = nlen;
	return (ndata);
}

void
aspath_merge(struct rde_aspath *a, struct attr *attr)
{
	uint8_t		*np;
	uint16_t	 ascnt, diff, nlen, difflen;
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

uint16_t
aspath_length(struct aspath *aspath)
{
	return (aspath->len);
}

uint32_t
aspath_neighbor(struct aspath *aspath)
{
	/*
	 * Empty aspath is OK -- internal AS route.
	 * Additionally the RFC specifies that if the path starts with an
	 * AS_SET the neighbor AS is also the local AS.
	 */
	if (aspath->len == 0 ||
	    aspath->data[0] != AS_SEQUENCE)
		return (rde_local_as());
	return (aspath_extract(aspath->data, 0));
}

uint32_t
aspath_origin(struct aspath *aspath)
{
	return aspath->source_as;
}

static uint16_t
aspath_count(const void *data, uint16_t len)
{
	const uint8_t	*seg;
	uint16_t	 cnt, seg_size;
	uint8_t		 seg_type, seg_len;

	cnt = 0;
	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt += 1;
		else
			cnt += seg_len;

		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
	}
	return (cnt);
}

/*
 * The origin AS number derived from a Route as follows:
 * o  the rightmost AS in the final segment of the AS_PATH attribute
 *    in the Route if that segment is of type AS_SEQUENCE, or
 * o  the BGP speaker's own AS number if that segment is of type
 *    AS_CONFED_SEQUENCE or AS_CONFED_SET or if the AS_PATH is empty,
 * o  the distinguished value "NONE" if the final segment of the
 *    AS_PATH attribute is of any other type.
 */
static uint32_t
aspath_extract_origin(const void *data, uint16_t len)
{
	const uint8_t	*seg;
	uint32_t	 as = AS_NONE;
	uint16_t	 seg_size;
	uint8_t		 seg_len;

	/* AS_PATH is empty */
	if (len == 0)
		return (rde_local_as());

	seg = data;
	for (; len > 0; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		if (len == seg_size && seg[0] == AS_SEQUENCE) {
			as = aspath_extract(seg, seg_len - 1);
		}
		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
	}
	return (as);
}

static uint16_t
aspath_countlength(struct aspath *aspath, uint16_t cnt, int headcnt)
{
	const uint8_t	*seg;
	uint16_t	 seg_size, len, clen;
	uint8_t		 seg_type = 0, seg_len = 0;

	seg = aspath->data;
	clen = 0;
	for (len = aspath->len; len > 0 && cnt > 0;
	    len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt -= 1;
		else if (seg_len > cnt) {
			seg_len = cnt;
			clen += 2 + sizeof(uint32_t) * cnt;
			break;
		} else
			cnt -= seg_len;

		clen += seg_size;

		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
	}
	if (headcnt > 0 && seg_type == AS_SEQUENCE && headcnt + seg_len < 256)
		/* no need for additional header from the new aspath. */
		clen -= 2;

	return (clen);
}

static void
aspath_countcopy(struct aspath *aspath, uint16_t cnt, uint8_t *buf,
    uint16_t size, int headcnt)
{
	const uint8_t	*seg;
	uint16_t	 seg_size, len;
	uint8_t		 seg_type, seg_len;

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
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		if (seg_type == AS_SET)
			cnt -= 1;
		else if (seg_len > cnt) {
			seg_len = cnt + headcnt;
			seg_size = 2 + sizeof(uint32_t) * cnt;
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
			fatalx("%s: would overflow", __func__);
		size -= seg_size;
	}
}

int
aspath_loopfree(struct aspath *aspath, uint32_t myAS)
{
	uint8_t		*seg;
	uint16_t	 len, seg_size;
	uint8_t		 i, seg_len;

	seg = aspath->data;
	for (len = aspath->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			if (myAS == aspath_extract(seg, i))
				return (0);
		}

		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
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
aspath_lookup(const void *data, uint16_t len)
{
	struct aspath_list	*head;
	struct aspath		*aspath;
	uint32_t		 hash;

	hash = SipHash24(&astablekey, data, len);
	head = ASPATH_HASH(hash);

	LIST_FOREACH(aspath, head, entry) {
		if (len == aspath->len && memcmp(data, aspath->data, len) == 0)
			return (aspath);
	}
	return (NULL);
}


static int
as_compare(struct filter_as *f, uint32_t as, uint32_t neighas)
{
	uint32_t match;

	if (f->flags & AS_FLAG_AS_SET_NAME)	/* should not happen */
		return (0);
	if (f->flags & AS_FLAG_AS_SET)
		return (as_set_match(f->aset, as));

	if (f->flags & AS_FLAG_NEIGHBORAS)
		match = neighas;
	else
		match = f->as_min;

	switch (f->op) {
	case OP_NONE:
	case OP_EQ:
		if (as == match)
			return (1);
		break;
	case OP_NE:
		if (as != match)
			return (1);
		break;
	case OP_RANGE:
		if (as >= f->as_min && as <= f->as_max)
			return (1);
		break;
	case OP_XRANGE:
		if (as < f->as_min || as > f->as_max)
			return (1);
		break;
	}
	return (0);
}

/* we need to be able to search more than one as */
int
aspath_match(struct aspath *aspath, struct filter_as *f, uint32_t neighas)
{
	const uint8_t	*seg;
	int		 final;
	uint16_t	 len, seg_size;
	uint8_t		 i, seg_len;
	uint32_t	 as = AS_NONE;

	if (f->type == AS_EMPTY) {
		if (aspath_length(aspath) == 0)
			return (1);
		else
			return (0);
	}

	/* just check the leftmost AS */
	if (f->type == AS_PEER) {
		as = aspath_neighbor(aspath);
		if (as_compare(f, as, neighas))
			return (1);
		else
			return (0);
	}

	seg = aspath->data;
	len = aspath->len;
	for (; len >= 6; len -= seg_size, seg += seg_size) {
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		final = (len == seg_size);

		if (f->type == AS_SOURCE) {
			/*
			 * Just extract the rightmost AS
			 * but if that segment is an AS_SET then the rightmost
			 * AS of a previous AS_SEQUENCE segment should be used.
			 * Because of that just look at AS_SEQUENCE segments.
			 */
			if (seg[0] == AS_SEQUENCE)
				as = aspath_extract(seg, seg_len - 1);
			/* not yet in the final segment */
			if (!final)
				continue;
			if (as_compare(f, as, neighas))
				return (1);
			else
				return (0);
		}
		/* AS_TRANSIT or AS_ALL */
		for (i = 0; i < seg_len; i++) {
			/*
			 * the source (rightmost) AS is excluded from
			 * AS_TRANSIT matches.
			 */
			if (final && i == seg_len - 1 && f->type == AS_TRANSIT)
				return (0);
			as = aspath_extract(seg, i);
			if (as_compare(f, as, neighas))
				return (1);
		}

		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
	}
	return (0);
}

/*
 * Returns a new prepended aspath. Old needs to be freed by caller.
 */
u_char *
aspath_prepend(struct aspath *asp, uint32_t as, int quantum, uint16_t *len)
{
	u_char		*p;
	int		 l, overflow = 0, shift = 0, size, wpos = 0;
	uint8_t		 type;

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
		l = 2 + quantum * sizeof(uint32_t) + asp->len;
		if (type == AS_SET)
			overflow = quantum;
		else
			overflow = size + quantum - 255;
	} else
		l = quantum * sizeof(uint32_t) + asp->len;

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
			memcpy(p + wpos, &as, sizeof(uint32_t));
			wpos += sizeof(uint32_t);
		}
	}
	if (quantum > 0) {
		shift = 2;
		p[wpos++] = AS_SEQUENCE;
		p[wpos++] = quantum + size;

		for (; quantum > 0; quantum--) {
			memcpy(p + wpos, &as, sizeof(uint32_t));
			wpos += sizeof(uint32_t);
		}
	}
	memcpy(p + wpos, asp->data + shift, asp->len - shift);

	*len = l;
	return (p);
}

/*
 * Returns a new aspath where neighbor_as is replaced by local_as.
 */
u_char *
aspath_override(struct aspath *asp, uint32_t neighbor_as, uint32_t local_as,
    uint16_t *len)
{
	u_char		*p, *seg, *nseg;
	uint32_t	 as;
	uint16_t	 l, seg_size;
	uint8_t		 i, seg_len, seg_type;

	p = malloc(asp->len);
	if (p == NULL)
		fatal("aspath_override");

	seg = asp->data;
	nseg = p;
	for (l = asp->len; l > 0; l -= seg_size, seg += seg_size) {
		*nseg++ = seg_type = seg[0];
		*nseg++ = seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);
			if (as == neighbor_as)
				as = local_as;
			as = htonl(as);
			memcpy(nseg, &as, sizeof(as));
			nseg += sizeof(as);
		}

		if (seg_size > l)
			fatalx("%s: would overflow", __func__);
	}

	*len = asp->len;
	return (p);
}

int
aspath_lenmatch(struct aspath *a, enum aslen_spec type, u_int aslen)
{
	uint8_t		*seg;
	uint32_t	 as, lastas = 0;
	u_int		 count = 0;
	uint16_t	 len, seg_size;
	uint8_t		 i, seg_len, seg_type;

	if (type == ASLEN_MAX) {
		if (aslen < aspath_count(a->data, a->len))
			return (1);
		else
			return (0);
	}

	/* type == ASLEN_SEQ */
	seg = a->data;
	for (len = a->len; len > 0; len -= seg_size, seg += seg_size) {
		seg_type = seg[0];
		seg_len = seg[1];
		seg_size = 2 + sizeof(uint32_t) * seg_len;

		for (i = 0; i < seg_len; i++) {
			as = aspath_extract(seg, i);
			if (as == lastas) {
				if (aslen < ++count)
					return (1);
			} else if (seg_type == AS_SET) {
				/* AS path 3 { 4 3 7 } 3 will have count = 3 */
				continue;
			} else
				count = 1;
			lastas = as;
		}

		if (seg_size > len)
			fatalx("%s: would overflow", __func__);
	}
	return (0);
}
