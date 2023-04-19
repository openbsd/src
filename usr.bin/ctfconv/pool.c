/*	$OpenBSD: pool.c,v 1.4 2023/04/19 12:58:15 jsg Exp $ */

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

#ifndef NOPOOL

#include <sys/types.h>
#include <sys/queue.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"
#include "pool.h"

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

struct pool_item {
	SLIST_ENTRY(pool_item) pi_list;
};

SIMPLEQ_HEAD(, pool) pool_head = SIMPLEQ_HEAD_INITIALIZER(pool_head);

void
pool_init(struct pool *pp, const char *name, size_t nmemb, size_t size)
{
	size = MAXIMUM(size, sizeof(struct pool_item));

	SLIST_INIT(&pp->pr_free);
	pp->pr_name = name;
	pp->pr_nmemb = nmemb;
	pp->pr_size = size;
	pp->pr_nitems = 0;
	pp->pr_nfree = 0;

	SIMPLEQ_INSERT_TAIL(&pool_head, pp, pr_list);
}

void *
pool_get(struct pool *pp)
{
	struct pool_item *pi;

	if (SLIST_EMPTY(&pp->pr_free)) {
		char *p;
		size_t i;

		p = xreallocarray(NULL, pp->pr_nmemb, pp->pr_size);
		for (i = 0; i < pp->pr_nmemb; i++) {
			pi = (struct pool_item *)p;
			SLIST_INSERT_HEAD(&pp->pr_free, pi, pi_list);
			p += pp->pr_size;
		}
		pp->pr_nitems += pp->pr_nmemb;
		pp->pr_nfree += pp->pr_nmemb;
	}

	pi = SLIST_FIRST(&pp->pr_free);
	SLIST_REMOVE_HEAD(&pp->pr_free, pi_list);
	pp->pr_nfree--;

	return pi;
}

void
pool_put(struct pool *pp, void *p)
{
	struct pool_item *pi = (struct pool_item *)p;

	if (pi == NULL)
		return;

	assert(pp->pr_nfree < pp->pr_nitems);

	SLIST_INSERT_HEAD(&pp->pr_free, pi, pi_list);
	pp->pr_nfree++;
}

void
pool_dump(void)
{
	struct pool *pp;

	SIMPLEQ_FOREACH(pp, &pool_head, pr_list)
		printf("%s: %zd items, %zd free\n", pp->pr_name, pp->pr_nitems,
		    pp->pr_nfree);
}
#endif /* NOPOOL */
