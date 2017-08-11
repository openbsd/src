/*	$OpenBSD: pool.h,v 1.2 2017/08/11 14:58:56 jasper Exp $ */

/*
 * Copyright (c) 2017 Martin Pieuchot
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

#ifndef _POOL_H_
#define _POOL_H_

#ifndef NOPOOL

struct pool {
	SIMPLEQ_ENTRY(pool)	 pr_list;	/* list of all pools */
	const char		*pr_name;	/* identifier */
	SLIST_HEAD(, pool_item)  pr_free;	/* free list */
	size_t			 pr_nmemb;	/* # of items per allocation */
	size_t			 pr_size;	/* size of an item */
	size_t			 pr_nitems;	/* # of available items */
	size_t			 pr_nfree;	/* # items on the free list */
};

void	 pool_init(struct pool *, const char *, size_t, size_t);
void	*pool_get(struct pool *);
void	 pool_put(struct pool *, void *);
void	 pool_dump(void);

static inline void *
pmalloc(struct pool *pp, size_t sz)
{
	return pool_get(pp);
}

static inline void *
pzalloc(struct pool *pp, size_t sz)
{
	char *p;

	p = pool_get(pp);
	memset(p, 0, sz);

	return p;
}

static inline void
pfree(struct pool *pp, void *p)
{
	pool_put(pp, p);
}

#else /* !NOPOOL */
#define pmalloc(a, b)		malloc(b)
#define pzalloc(a, b)		calloc(1, b)
#define pfree(a, b)		free(b)
#endif /* NOPOOL */

#endif /* _POOL_H_ */
