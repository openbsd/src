/*	$OpenBSD: rde_prefix.c,v 1.1 2003/12/17 11:46:54 henning Exp $ */

/*
 * Copyright (c) 2003 Claudio Jeker <cjeker@diehard.n-r-g.com>
 *
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

#include "bgpd.h"
#include "ensure.h"
#include "rde.h"

/*
 * Prefix Table functions:
 * pt_add:    create new prefix and link it into the prefix table
 * pt_remove: Checks if there is no bgp prefix linked to the prefix,
 *            unlinks form the prefix table and frees the pt_entry.
 * pt_get:    get a prefix/prefixlen entry. While pt_lookup searches for the
 *            best matching prefix pt_get only finds the prefix/prefixlen
 *            entry. The speed of pt_get is important for the bgp updates.
 * pt_lookup: lookup a IP in the prefix table. Manly for "show ip bgp".
 * pt_empty:  returns true if there is no bgp prefix linked to the pt_entry.
 * pt_init:   initialize prefix table.
 * pt_alloc:  allocate a pt_entry. Internal function.
 * pt_free:   free a pt_entry. Internal funktion.
 */

/* internal prototypes */
static struct pt_entry	*pt_alloc(void);
static void		 pt_free(struct pt_entry *);

/*
 * currently we are using a hash list to store the prefixes. This may be
 * replaced with a red-black trie if neccesary.
 */
LIST_HEAD(pt_entryhead, pt_entry);

struct pt_table {
	struct pt_entryhead	*pt_hashtbl;
	u_long			 pt_hashmask;
};

#define MIN_PREFIX 0
#define MAX_PREFIX 32

/*
 * size of the hashtable per prefixlen. The sizes where choosen from a bgp
 * dump done on Nov 4. 2003.
 */
u_long pthashsize[MAX_PREFIX + 1 - MIN_PREFIX] = {
	/* need to be power of 2 */
	1, 1, 1, 1, 1, 1, 1, 1,
	16, 8, 8, 16, 32, 64, 256, 256,
	4096, 1024, 2048, 8192, 8192, 4096, 8192, 8192,
	32768, 1, 1, 1, 1, 1, 1, 1, 1
};

struct pt_table	pttable[MAX_PREFIX + 1 - MIN_PREFIX];

#define PT_HASH(p, plen)				\
	&pttable[plen].pt_hashtbl[((p >> plen) ^ (p >> (plen + 5))) & pttable[plen].pt_hashmask]

/*
 * Statistics collector.
 * Collected to tune the preifx table. Currently only a few counters where
 * added. More to come as soon as we see where we are going.
 * TODO: add a function that dumps the stats after a specified period of time.
 */
struct pt_stats {
	u_int64_t		 pt_alloc;
	u_int64_t		 pt_free;
	u_int64_t		 pt_add[MAX_PREFIX + 1 - MIN_PREFIX];
	u_int64_t		 pt_get[MAX_PREFIX + 1 - MIN_PREFIX];
	u_int64_t		 pt_remove[MAX_PREFIX + 1 - MIN_PREFIX];
	u_int64_t		 pt_lookup;
	u_int64_t		 pt_dump;
} ptstats;
/* simple macros to update statisic */
#define PT_STAT(x)	(ptstats.x++)
#define PT_STAT2(x, p)	(ptstats.x[p]++)

void
pt_init(void)
{
	int	i;
	u_long	j;

	for (i = MIN_PREFIX; i <= MAX_PREFIX; i++) {
		pttable[i].pt_hashtbl = calloc(pthashsize[i],
		    sizeof(struct pt_entryhead));
		if (pttable[i].pt_hashtbl == NULL)
			fatal("pt_init", errno);

		for (j = 0; j < pthashsize[i]; j++)
			LIST_INIT(&pttable[i].pt_hashtbl[j]);

		pttable[i].pt_hashmask = pthashsize[i] - 1;
	}
}

int
pt_empty(struct pt_entry *pte)
{
	ENSURE(pte != NULL);
	return LIST_EMPTY(&pte->prefix_h);
}

struct pt_entry *
pt_get(struct in_addr prefix, int prefixlen)
{
	struct pt_entryhead	*head;
	struct pt_entry		*p;
	u_int32_t		 p_hbo;

	ENSURE(MIN_PREFIX <= prefixlen && prefixlen <= MAX_PREFIX);
	PT_STAT2(pt_get, prefixlen);

	p_hbo = ntohl(prefix.s_addr);
	head = PT_HASH(p_hbo, prefixlen);
	ENSURE(head != NULL);

	LIST_FOREACH(p, head, pt_l) {
		if (prefix.s_addr == p->prefix.s_addr)
			return p;
	}
	return NULL;
}

struct pt_entry *
pt_add(struct in_addr prefix, int prefixlen)
{
	struct pt_entryhead	*head;
	struct pt_entry		*p;
	u_int32_t		 p_hbo;

	ENSURE(MIN_PREFIX <= prefixlen && prefixlen <= MAX_PREFIX);
	ENSURE(pt_get(prefix, prefixlen) == NULL);
	PT_STAT2(pt_add, prefixlen);

	p_hbo = ntohl(prefix.s_addr);
	head = PT_HASH(p_hbo, prefixlen);
	ENSURE(head != NULL);

	p = pt_alloc();
	p->prefix = prefix;
	p->prefixlen = prefixlen;
	LIST_INIT(&p->prefix_h);

	LIST_INSERT_HEAD(head, p, pt_l);

	return p;
}

void
pt_remove(struct pt_entry *pte)
{
	ENSURE(pt_empty(pte));
	PT_STAT2(pt_remove, pte->prefixlen);

	LIST_REMOVE(pte, pt_l);
	/* XXX we could bzero it for debugging reasons */
	pt_free(pte);
}

struct pt_entry *
pt_lookup(struct in_addr prefix)
{
	struct pt_entry	*p;
	int		 i;

	PT_STAT(pt_lookup);
	for (i = MAX_PREFIX; i >= MIN_PREFIX; i--) {
		p = pt_get(prefix, i);
		if (p != NULL) return p;
	}
	return NULL;
}

/*
 * XXX We need a redblack tree to get a ordered output.
 * XXX A nicer upcall interface wouldn't be luxus too.
 */
void
pt_dump(void (*upcall)(struct pt_entry *, int, int *, void *),
    int fd, int *w, void *arg)
{
	struct pt_entry	*p;
	int		 i;
	u_long		 j;

	PT_STAT(pt_dump);
	for (i = MAX_PREFIX; i >= MIN_PREFIX; i--) {
		for (j = 0; j < pthashsize[i]; j++)
			LIST_FOREACH(p, &pttable[i].pt_hashtbl[j], pt_l)
				upcall(p, fd, w, arg);
	}
}

/* returns a zeroed pt_entry function may not return on fail */
static struct pt_entry *
pt_alloc(void)
{
	struct pt_entry	*p;

	PT_STAT(pt_alloc);
	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("pt_alloc", errno);
	return p;
}

static void
pt_free(struct pt_entry *pte)
{
	PT_STAT(pt_free);
	free(pte);
}

