/*	$OpenBSD: rde_trie.c,v 1.1 2018/09/07 10:49:22 claudio Exp $ */

/*
 * Copyright (c) 2018 Claudio Jeker <claudio@openbsd.org>
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
#include "rde.h"

/*
 * Bitwise compressed trie for prefix-sets.
 * Since the trie is path compressed the traversing function needs to check
 * that the lookup is still on path before taking a branch.
 * There are two types of nodes in the trie. Internal branch nodes and real
 * nodes. Internal nodes are added when needed because off path nodes are being
 * inserted and are just used for branching.
 * During lookup every node defines which bit is checked for branching. This
 * offset is strictly increasing. For IPv4 the maximum is therefor 32 levels.
 * Every node checks the bit at position plen to decide which branch to take.
 * The real nodes also include a prefixlen mask which represents the prefixlen
 * range that was defined. The prefixlen mask for IPv4 only has 32 bits but
 * the prefixlen range is from 0 - 32 which needs 33bits. Now prefixlen 0 is
 * special and only one element -- the default route -- can have prefixlen 0.
 * So this default route is added as a flag to the trie_head and needs to be
 * checked independent of the trie when doing a lookup.
 * On insertion a prefix is added with its prefixlen plen and a min & max
 * for the prefixlen range. The following precondition needs to hold
 *     plen <= min <= max
 * To only match the prefix itself min and max are set to plen.
 * If a same prefix (addr/plen) is added to the trie but with different
 * min & max values then the masks of both nodes are merged with a binary OR.
 * The match function returns true the moment the first node is found which
 * covers the looked up prefix and where the prefixlen mask matches for the
 * looked up prefixlen. The moment the plen of a node is bigger is bigger than
 * the prefixlen of the looked up prefix the search can be stopped since there
 * will be no match.
 */
struct tentry_v4 {
	struct tentry_v4	*trie[2];
	struct in_addr		 addr;
	struct in_addr		 plenmask;
	u_int8_t		 plen;
	u_int8_t		 node;

	/* roa source-as list pointer */
};

struct tentry_v6 {
	struct tentry_v6	*trie[2];
	struct in6_addr		 addr;
	struct in6_addr		 plenmask;
	u_int8_t		 plen;
	u_int8_t		 node;

	/* roa source-as list pointer */
};

/*
 * Find first different bit between a & b starting from the MSB,
 * a & b have to be different.
 */
static int
inet4findmsb(struct in_addr *a, struct in_addr *b)
{
	int r = 0;
	u_int32_t v;

	v = ntohl(a->s_addr ^ b->s_addr);
	if (v > 0xffff) { r += 16; v >>= 16; }
	if (v > 0x00ff) { r += 8; v >>= 8; }
	if (v > 0x000f) { r += 4; v >>= 4; }
	if (v > 0x0003) { r += 2; v >>= 2; }
	if (v > 0x0001) { r += 1; }

	return 31 - r;
}

/*
 * Find first different bit between a & b starting from the MSB,
 * a & b have to be different.
 */
static int
inet6findmsb(struct in6_addr *a, struct in6_addr *b)
{
	int r = 0;
	u_int8_t i, x;

	for (i = 0; i < sizeof(*a) && a->s6_addr[i] == b->s6_addr[i]; i++)
		;
	/* first different octet */
	x = a->s6_addr[i] ^ b->s6_addr[i];

	/* first different bit */
	if (x > 0xf) { r += 4; x >>= 4; }
	if (x > 0x3) { r += 2; x >>= 2; }
	if (x > 0x1) { r += 1; }

	return i * 8 + 7 - r;
}

static int
inet4isset(struct in_addr *addr, u_int8_t bit)
{
	return addr->s_addr & htonl(1 << (31 - bit));
}

static int
inet6isset(struct in6_addr *addr, u_int8_t bit)
{
	return addr->s6_addr[bit / 8] & (0x80 >> (bit % 8));
}

static void
inet4setbit(struct in_addr *addr, u_int8_t bit)
{
	/* bit 0 sets the MSB and 31 sets the LSB */
	addr->s_addr |= (1 << (31 - bit));
}

static void
inet6setbit(struct in6_addr *addr, u_int8_t bit)
{
	addr->s6_addr[bit / 8] |= (0x80 >> (bit % 8));
}

static int
trie_add_v4(struct trie_head *th, struct in_addr *prefix, u_int8_t plen,
    u_int8_t min, u_int8_t max)
{
	struct tentry_v4 *n, *new, *b, **prev;
	struct in_addr p, plenmask;
	u_int8_t i;

	/*
	 * check for default route, this is special cased since prefixlen 0
	 * can't be checked in the prefixlen mask plenmask. Also there is only
	 * one default route so using a flag for this works.
	 */
	if (min == 0) {
		th->match_default_v4 = 1;
		if (max == 0)	/* just the default route */
			return 0;
		min = 1;
	}

	inet4applymask(&p, prefix, plen);

	/*
	 * The prefixlen mask goes from 1 to 32 but the bitmask
	 * starts at 0 and so all bits are set with an offset of 1.
	 * The default /0 route is handled specially above.
	 */
	memset(&plenmask, 0, sizeof(plenmask));
	for (i = min; i <= max; i++)
		inet4setbit(&plenmask, i - 1);

	/* walk tree finding spot to insert */
	prev = &th->root_v4;
	n = *prev;
	while (n) {
		struct in_addr mp;
		u_int8_t minlen;

		minlen = n->plen > plen ? plen : n->plen;
		inet4applymask(&mp, &p, minlen);
		if (n->addr.s_addr !=  mp.s_addr) {
			/*
			 * out of path, insert intermediary node between
			 * np and n, then insert n and new node there
			 */
			if ((b = calloc(1, sizeof(*b))) == NULL)
				return -1;
			b->plen = inet4findmsb(&n->addr, &mp);
			inet4applymask(&b->addr, &n->addr, b->plen);

			*prev = b;
			if (inet4isset(&n->addr, b->plen)) {
				b->trie[1] = n;
				prev = &b->trie[0];
			} else {
				b->trie[0] = n;
				prev = &b->trie[1];
			}
			n = NULL;
			break;
		}

		if (n->plen > plen) {
			/* n is more specific, just insert new in between */
			break;
		}

		if (n->plen == plen) {
			/* matching node, adjust */
			n->node = 1;
			n->plenmask.s_addr |= plenmask.s_addr;
			return 0;
		}

		/* no need to check for n->plen == 32 because of above if */
		if (inet4isset(&p, n->plen))
			prev = &n->trie[1];
		else
			prev = &n->trie[0];
		n = *prev;
	}

	/* create new node */
	if ((new = calloc(1, sizeof(*new))) == NULL)
		return -1;
	new->addr = p;
	new->plen = plen;
	new->plenmask = plenmask;
	new->node = 1;

	/* link node */
	*prev = new;
	if (n) {
		if (inet4isset(&n->addr, new->plen))
			new->trie[1] = n;
		else
			new->trie[0] = n;
	}
	return 0;
}

static int
trie_add_v6(struct trie_head *th, struct in6_addr *prefix, u_int8_t plen,
    u_int8_t min, u_int8_t max)
{
	struct tentry_v6 *n, *new, *b, **prev;
	struct in6_addr p, plenmask;
	u_int8_t i;

	/*
	 * check for default route, this is special cased since prefixlen 0
	 * can't be checked in the prefixlen mask plenmask. Also there is only
	 * one default route so using a flag for this works.
	 */
	if (min == 0) {
		th->match_default_v6 = 1;
		if (max == 0)	/* just the default route */
			return 0;
		min = 1;
	}

	inet6applymask(&p, prefix, plen);

	/*
	 * The prefixlen mask goes from 1 to 128 but the bitmask
	 * starts at 0 and so all bits are set with an offset of 1.
	 * The default /0 route is handled specially above.
	 */
	memset(&plenmask, 0, sizeof(plenmask));
	for (i = min; i <= max; i++)
		inet6setbit(&plenmask, i - 1);

	/* walk tree finding spot to insert */
	prev = &th->root_v6;
	n = *prev;
	while (n) {
		struct in6_addr mp;
		u_int8_t minlen;

		minlen = n->plen > plen ? plen : n->plen;
		inet6applymask(&mp, &p, minlen);
		if (memcmp(&n->addr, &mp, sizeof(mp)) != 0) {
			/*
			 * out of path, insert intermediary node between
			 * np and n, then insert n and new node there
			 */
			if ((b = calloc(1, sizeof(*b))) == NULL)
				return -1;
			b->plen = inet6findmsb(&n->addr, &mp);
			inet6applymask(&b->addr, &n->addr, b->plen);

			*prev = b;
			if (inet6isset(&n->addr, b->plen)) {
				b->trie[1] = n;
				prev = &b->trie[0];
			} else {
				b->trie[0] = n;
				prev = &b->trie[1];
			}
			n = NULL;
			break;
		}

		if (n->plen > plen) {
			/* n is more specific, just insert new in between */
			break;
		}

		if (n->plen == plen) {
			/* matching node, adjust */
			n->node = 1;
			for (i = 0; i < sizeof(plenmask); i++)
				n->plenmask.s6_addr[i] |= plenmask.s6_addr[i];
			return 0;
		}

		/* no need to check for n->plen == 32 because of above if */
		if (inet6isset(&p, n->plen))
			prev = &n->trie[1];
		else
			prev = &n->trie[0];
		n = *prev;
	}

	/* create new node */
	if ((new = calloc(1, sizeof(*new))) == NULL)
		return -1;
	new->addr = p;
	new->plen = plen;
	new->plenmask = plenmask;
	new->node = 1;

	/* link node */
	*prev = new;
	if (n) {
		if (inet6isset(&n->addr, new->plen))
			new->trie[1] = n;
		else
			new->trie[0] = n;
	}
	return 0;
}

int
trie_add(struct trie_head *th, struct bgpd_addr *prefix, u_int8_t plen,
    u_int8_t min, u_int8_t max)
{
	/* precondition plen <= min <= max */
	if (plen > min || min > max)
		return -1;

	switch (prefix->aid) {
	case AID_INET:
		if (max > 32)
			return -1;
		return trie_add_v4(th, &prefix->v4, plen, min, max);
	case AID_INET6:
		if (max > 128)
			return -1;
		return trie_add_v6(th, &prefix->v6, plen, min, max);
	default:
		/* anything else fails */
		return -1;
	}
}

static void
trie_free_v4(struct tentry_v4 *n)
{
	if (n == NULL)
		return;
	trie_free_v4(n->trie[0]);
	trie_free_v4(n->trie[1]);
	free(n);
}

static void
trie_free_v6(struct tentry_v6 *n)
{
	if (n == NULL)
		return;
	trie_free_v6(n->trie[0]);
	trie_free_v6(n->trie[1]);
	free(n);
}

void
trie_free(struct trie_head *th)
{
	trie_free_v4(th->root_v4);
	trie_free_v6(th->root_v6);
}

static int
trie_match_v4(struct trie_head *th, struct in_addr *prefix, u_int8_t plen)
{
	struct tentry_v4 *n;

	if (plen == 0) {
		/* special handling for default route */
		return th->match_default_v4;
	}

	n = th->root_v4;
	while (n) {
		struct in_addr mp;

		if (n->plen > plen)
			break;	/* too specific, no match possible */

		inet4applymask(&mp, prefix, n->plen);
		if (n->addr.s_addr !=  mp.s_addr)
			break;	/* off path, no match possible */

		/* plen is from 1 - 32 but the bitmask starts with 0 */
		if (n->node && inet4isset(&n->plenmask, plen - 1))
			return 1;	/* prefixlen allowed, match */

		if (n->plen == 32)
			break;	/* can't go deeper */
		if (inet4isset(prefix, n->plen))
			n = n->trie[1];
		else
			n = n->trie[0];
	}

	return 0;
}

static int
trie_match_v6(struct trie_head *th, struct in6_addr *prefix, u_int8_t plen)
{
	struct tentry_v6 *n;

	if (plen == 0) {
		/* special handling for default route */
		return th->match_default_v6;
	}

	n = th->root_v6;
	while (n) {
		struct in6_addr mp;

		if (n->plen > plen)
			break;	/* too specific, no match possible */

		inet6applymask(&mp, prefix, n->plen);
		if (memcmp(&n->addr, &mp, sizeof(mp)) != 0)
			break;	/* off path, no match possible */

		/* plen is from 1 - 128 but the bitmask starts with 0 */
		if (n->node && inet6isset(&n->plenmask, plen - 1))
			return 1;	/* prefixlen allowed, match */

		if (n->plen == 128)
			break;	/* can't go deeper */
		if (inet6isset(prefix, n->plen))
			n = n->trie[1];
		else
			n = n->trie[0];
	}
	return 0;
}

/* find first matching element in the trie for prefix "prefix/plen" */
int
trie_match(struct trie_head *th, struct bgpd_addr *prefix, u_int8_t plen)
{
	switch (prefix->aid) {
	case AID_INET:
		return trie_match_v4(th, &prefix->v4, plen);
	case AID_INET6:
		return trie_match_v6(th, &prefix->v6, plen);
	default:
		/* anything else is no match */
		return 0;
	}
}

static int
trie_equal_v4(struct tentry_v4 *a, struct tentry_v4 *b)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;

	if (a->addr.s_addr != b->addr.s_addr ||
	    a->plen != b->plen ||
	    a->node != b->node ||
	    a->plenmask.s_addr != b->plenmask.s_addr)
		return 0;

	if (trie_equal_v4(a->trie[0], b->trie[0]) == 0 ||
	    trie_equal_v4(a->trie[1], b->trie[1]) == 0)
		return 0;

	return 1;
}

static int
trie_equal_v6(struct tentry_v6 *a, struct tentry_v6 *b)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;

	if (memcmp(&a->addr, &b->addr, sizeof(a->addr)) != 0 ||
	    a->plen != b->plen ||
	    a->node != b->node ||
	    memcmp(&a->plenmask, &b->plenmask, sizeof(a->plenmask)) != 0)
		return 0;

	if (trie_equal_v6(a->trie[0], b->trie[0]) == 0 ||
	    trie_equal_v6(a->trie[1], b->trie[1]) == 0)
		return 0;

	return 1;
}

/* Compare two tires and return 1 if they are the same else 0. */
int
trie_equal(struct trie_head *a, struct trie_head *b)
{
	if (a->match_default_v4 != b->match_default_v4 ||
	    a->match_default_v6 != b->match_default_v6)
		return 0;
	if (trie_equal_v4(a->root_v4, b->root_v4) == 0)
		return 0;
	if (trie_equal_v6(a->root_v6, b->root_v6) == 0)
		return 0;
	return 1;
}

/* debugging functions for printing the trie */
static void
trie_dump_v4(struct tentry_v4 *n)
{
	if (n == NULL)
		return;
	if (n->node)
		printf("%s/%u plenmask %08x\n", inet_ntoa(n->addr), n->plen,
		    n->plenmask.s_addr);
	else
		printf("   %s/%u\n", inet_ntoa(n->addr), n->plen);

	trie_dump_v4(n->trie[0]);
	trie_dump_v4(n->trie[1]);
}

static void
trie_dump_v6(struct tentry_v6 *n)
{
	char buf[48];
	unsigned int i;

	if (n == NULL)
		return;
	if (n->node) {
		printf("%s/%u plenmask ",
		    inet_ntop(AF_INET6, &n->addr, buf, sizeof(buf)), n->plen);
		for (i = 0; i < sizeof(n->plenmask); i++)
			printf("%02x", n->plenmask.s6_addr[i]);
		printf("\n");
	} else
		printf("   %s/%u\n",
		    inet_ntop(AF_INET6, &n->addr, buf, sizeof(buf)), n->plen);

	trie_dump_v6(n->trie[0]);
	trie_dump_v6(n->trie[1]);
}

void
trie_dump(struct trie_head *th)
{
	if (th->match_default_v4)
		printf("0.0.0.0/0 plenmask %08x\n", 0);
	trie_dump_v4(th->root_v4);
	if (th->match_default_v6)
		printf("::/0 plenmask 0\n");
	trie_dump_v6(th->root_v6);
}
