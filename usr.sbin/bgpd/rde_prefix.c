/*	$OpenBSD: rde_prefix.c,v 1.21 2004/11/11 16:53:01 claudio Exp $ */

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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"

/*
 * Prefix Table functions:
 * pt_add:    create new prefix and link it into the prefix table
 * pt_remove: Checks if there is no bgp prefix linked to the prefix,
 *            unlinks form the prefix table and frees the pt_entry.
 * pt_get:    get a prefix/prefixlen entry. While pt_lookup searches for the
 *            best matching prefix pt_get only finds the prefix/prefixlen
 *            entry. The speed of pt_get is important for the bgp updates.
 * pt_getaddr: convert the address into a struct bgpd_addr.
 * pt_lookup: lookup a IP in the prefix table. Manly for "show ip bgp".
 * pt_empty:  returns true if there is no bgp prefix linked to the pt_entry.
 * pt_init:   initialize prefix table.
 * pt_alloc?: allocate a AF specific pt_entry. Internal function.
 * pt_free:   free a pt_entry. Internal function.
 */

/* internal prototypes */
static struct pt_entry4	*pt_alloc4(void);
static struct pt_entry6	*pt_alloc6(void);
static void		 pt_free(struct pt_entry *);

int	pt_prefix_cmp(const struct pt_entry *, const struct pt_entry *);

#define MIN_PREFIX 0
#define MAX_PREFIX 32
RB_HEAD(pt_tree, pt_entry);
RB_PROTOTYPE(pt_tree, pt_entry, pt_e, pt_prefix_cmp);
RB_GENERATE(pt_tree, pt_entry, pt_e, pt_prefix_cmp);

struct pt_tree	pttable4;
struct pt_tree	pttable6;

void
pt_init(void)
{
	RB_INIT(&pttable4);
	RB_INIT(&pttable6);
}

void
pt_shutdown(void)
{
	if (!RB_EMPTY(&pttable4))
		log_debug("pt_shutdown: IPv4 tree is not empty.");
	if (!RB_EMPTY(&pttable6))
		log_debug("pt_shutdown: IPv6 tree is not empty.");
}

int
pt_empty(struct pt_entry *pte)
{
	return LIST_EMPTY(&pte->prefix_h);
}

void
pt_getaddr(struct pt_entry *pte, struct bgpd_addr *addr)
{
	bzero(addr, sizeof(struct bgpd_addr));
	switch (pte->af) {
	case AF_INET:
		addr->af = pte->af;
		addr->v4 = ((struct pt_entry4 *)pte)->prefix4;
		break;
	case AF_INET6:
		addr->af = pte->af;
		memcpy(&addr->v6, &((struct pt_entry6 *)pte)->prefix6,
		    sizeof(addr->v6));
		/* XXX scope_id ??? */
		break;
	default:
		fatalx("pt_getaddr: unknown af");
	}
}

struct pt_entry *
pt_get(struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_entry4	pte4;
	struct pt_entry6	pte6;
	in_addr_t		addr_hbo;

	switch (prefix->af) {
	case AF_INET:
		if (prefixlen > 32)
			fatalx("pt_get: bad IPv4 prefixlen");
		pte4.af = AF_INET;
		addr_hbo = ntohl(prefix->v4.s_addr);
		pte4.prefix4.s_addr = htonl(addr_hbo &
		    prefixlen2mask(prefixlen));
		pte4.prefixlen = prefixlen;
		return RB_FIND(pt_tree, &pttable4, (struct pt_entry *)&pte4);
	case AF_INET6:
		if (prefixlen > 128)
			fatalx("pt_get: bad IPv6 prefixlen");
		pte6.af = AF_INET6;
		pte6.prefixlen = prefixlen;
		inet6applymask(&pte6.prefix6, &prefix->v6, prefixlen);
		return RB_FIND(pt_tree, &pttable6, (struct pt_entry *)&pte6);
	default:
		log_warnx("pt_get: unknown af");
	}
	return (NULL);
}

struct pt_entry *
pt_add(struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_tree		*tree = NULL;
	struct pt_entry		*p = NULL;
	struct pt_entry4	*p4;
	struct pt_entry6	*p6;
	in_addr_t		 addr_hbo;

	switch (prefix->af) {
	case AF_INET:
		p4 = pt_alloc4();
		if (prefixlen > 32)
			fatalx("pt_add: bad IPv4 prefixlen");
		p4->af = AF_INET;
		p4->prefixlen = prefixlen;
		addr_hbo = ntohl(prefix->v4.s_addr);
		p4->prefix4.s_addr = htonl(addr_hbo &
		    prefixlen2mask(prefixlen));
		p = (struct pt_entry *)p4;
		tree = &pttable4;
		break;
	case AF_INET6:
		p6 = pt_alloc6();
		if (prefixlen > 128)
			fatalx("pt_add: bad IPv6 prefixlen");
		p6->af = AF_INET6;
		p6->prefixlen = prefixlen;
		inet6applymask(&p6->prefix6, &prefix->v6, prefixlen);
		p = (struct pt_entry *)p6;
		tree = &pttable6;
		break;
	default:
		fatalx("pt_add: unknown af");
	}
	LIST_INIT(&p->prefix_h);

	if (RB_INSERT(pt_tree, tree, p) != NULL) {
		log_warnx("prefix_add: insert failed");
		return (NULL);
	}

	return (p);
}

void
pt_remove(struct pt_entry *pte)
{
	if (!pt_empty(pte))
		fatalx("pt_remove: entry not empty");

	switch (pte->af) {
	case AF_INET:
		if (RB_REMOVE(pt_tree, &pttable4, pte) == NULL)
			log_warnx("pt_remove: remove failed.");
		break;
	case AF_INET6:
		if (RB_REMOVE(pt_tree, &pttable6, pte) == NULL)
			log_warnx("pt_remove: remove failed.");
		break;
	default:
		fatalx("pt_remove: unknown af");
	}

	pt_free(pte);
}

struct pt_entry *
pt_lookup(struct bgpd_addr *prefix)
{
	struct pt_entry	*p;
	int		 i;

	switch (prefix->af) {
	case AF_INET:
		for (i = 32; i >= 0; i--) {
			p = pt_get(prefix, i);
			if (p != NULL)
				return (p);
		}
		break;
	case AF_INET6:
		for (i = 128; i >= 0; i--) {
			p = pt_get(prefix, i);
			if (p != NULL)
				return (p);
		}
		break;
	default:
		fatalx("pt_lookup: unknown af");
	}
	return (NULL);
}

void
pt_dump(void (*upcall)(struct pt_entry *, void *), void *arg, sa_family_t af)
{
	struct pt_entry *p;

	if (af == AF_INET || af == AF_UNSPEC)
		RB_FOREACH(p, pt_tree, &pttable4)
		    upcall(p, arg);
	if (af == AF_INET6 || af == AF_UNSPEC)
		RB_FOREACH(p, pt_tree, &pttable6)
		    upcall(p, arg);
}

int
pt_prefix_cmp(const struct pt_entry *a, const struct pt_entry *b)
{
	const struct pt_entry4	*a4, *b4;
	const struct pt_entry6	*a6, *b6;
	int			 i;

	if (a->af != b->af)
		fatalx("king bula sez: comparing pears with apples");

	switch (a->af) {
	case AF_INET:
		a4 = (const struct pt_entry4 *)a;
		b4 = (const struct pt_entry4 *)b;
		if (ntohl(a4->prefix4.s_addr) > ntohl(b4->prefix4.s_addr))
			return (1);
		if (ntohl(a4->prefix4.s_addr) < ntohl(b4->prefix4.s_addr))
			return (-1);
		if (a4->prefixlen > b4->prefixlen)
			return (1);
		if (a4->prefixlen < b4->prefixlen)
			return (-1);
		return (0);
	case AF_INET6:
		a6 = (const struct pt_entry6 *)a;
		b6 = (const struct pt_entry6 *)b;

		i = memcmp(&a6->prefix6, &b6->prefix6, sizeof(struct in6_addr));
		if (i > 0)
			return (1);
		if (i < 0)
			return (-1);
		if (a6->prefixlen < b6->prefixlen)
			return (-1);
		if (a6->prefixlen > b6->prefixlen)
			return (1);
		return (0);
	default:
		fatalx("pt_prefix_cmp: unknown af");
	}
	return (-1);
}

/* returns a zeroed pt_entry function may not return on fail */
static struct pt_entry4 *
pt_alloc4(void)
{
	struct pt_entry4	*p;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("pt_alloc");
	return (p);
}

static struct pt_entry6 *
pt_alloc6(void)
{
	struct pt_entry6	*p;

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("pt_alloc");
	return (p);
}

static void
pt_free(struct pt_entry *pte)
{
	free(pte);
}

