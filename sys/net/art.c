/*	$OpenBSD: art.c,v 1.9 2015/11/24 12:06:30 mpi Exp $ */

/*
 * Copyright (c) 2015 Martin Pieuchot
 * Copyright (c) 2001 Yoichi Hariguchi
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

/*
 * Allotment Routing Table (ART).
 *
 * Yoichi Hariguchi paper can be found at:
 *	http://www.hariguchi.org/art/art.pdf
 */

#ifndef _KERNEL
#include "kern_compat.h"
#else
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/socket.h>
#endif

#include <net/art.h>

#define ISLEAF(e)	(((unsigned long)(e).node & 1) == 0)
#define SUBTABLE(e)	(((struct art_table *)((unsigned long)(e).child & ~1)))
#define ASNODE(t)	((struct art_node *)((unsigned long)(t) | 1))

/*
 * Allotment Table.
 */
struct art_table {
	struct art_table	*at_parent;	/* Parent table */
	uint32_t		 at_index;	/* Index in the parent table */
	uint32_t		 at_minfringe;	/* Index that fringe begins */
	uint32_t		 at_level;	/* Level of the table */
	uint8_t			 at_bits;	/* Stride length of the table */
	uint8_t			 at_offset;	/* Sum of parents' stride len */

	/*
	 * Items stored in the heap are pointers to nodes, in the leaf
	 * case, or tables otherwise.  One exception is index 0 which
	 * is a route counter.
	 */
	union {
		struct art_node		*node;
		struct art_table	*child;
		unsigned long		 count;
	} *at_heap;				/* Array of 2^(slen+1) items */
};
#define	at_refcnt	at_heap[0].count/* Refcounter (1 per different route) */
#define	at_default	at_heap[1].node	/* Default route (was in parent heap) */

/* Heap size for an ART table of stride length ``slen''. */
#define AT_HEAPSIZE(slen)	((1 << ((slen) + 1)) * sizeof(void *))

int			 art_bindex(struct art_table *, uint8_t *, int);
void			 art_allot(struct art_table *at, int, struct art_node *,
			     struct art_node *);
struct art_table	*art_table_get(struct art_root *, struct art_table *,
			     int);
struct art_table	*art_table_put(struct art_root *, struct art_table *);
struct art_node		*art_table_insert(struct art_root *, struct art_table *,
			     int, struct art_node *);
struct art_node		*art_table_delete(struct art_root *, struct art_table *,
			     int, struct art_node *);
void			 art_table_ref(struct art_root *, struct art_table *);
int			 art_table_free(struct art_root *, struct art_table *);
int			 art_table_walk(struct art_root *, struct art_table *,
			     int (*f)(struct art_node *, void *), void *);

struct pool		at_pool, at_heap_8_pool, at_heap_16_pool;

void
art_init(void)
{
	pool_init(&at_pool, sizeof(struct art_table), 0, 0, 0, "art_table",
	    NULL);
	pool_init(&at_heap_8_pool, AT_HEAPSIZE(8), 0, 0, 0, "art_heap8", NULL);
	pool_init(&at_heap_16_pool, AT_HEAPSIZE(16), 0, 0, 0, "art_heap16",
	    NULL);
}

/*
 * Per routing table initialization API function.
 */
struct art_root *
art_alloc(unsigned int rtableid, int off)
{
	struct art_root		*ar;
	int			 i;

	ar = malloc(sizeof(*ar), M_RTABLE, M_NOWAIT|M_ZERO);
	if (ar == NULL)
		return (NULL);

	/* XXX using the offset is a hack. */
	switch (off) {
	case 4: /* AF_INET && AF_MPLS */
		ar->ar_alen = 32;
		ar->ar_nlvl = 3;
		ar->ar_bits[0] = 16;
		ar->ar_bits[1] = 8;
		ar->ar_bits[2] = 8;
		break;
#ifdef INET6
	case 8: /* AF_INET6 */
		ar->ar_alen = 128;
		ar->ar_nlvl = 16;
		for (i = 0; i < ar->ar_nlvl; i++)
			ar->ar_bits[i] = 8;
		break;
#endif /* INET6 */
	default:
		printf("%s: unknown offset %d\n", __func__, off);
		art_free(ar);
		return (NULL);
	}

	ar->ar_off = off;
	ar->ar_rtableid = rtableid;

	return (ar);
}

void
art_free(struct art_root *ar)
{
	KASSERT(ar->ar_root == NULL);
	free(ar, M_RTABLE, sizeof(*ar));
}

/*
 * Return 1 if ``old'' and ``new`` are identical, 0 otherwise.
 */
static inline int
art_check_duplicate(struct art_root *ar, struct art_node *old,
    struct art_node *new)
{
	if (old == NULL)
		return (0);

	if (old->an_plen == new->an_plen)
		return (1);

	return (0);
}

/*
 * Return the base index of the part of ``addr'' and ``plen''
 * corresponding to the range covered by the table ``at''.
 *
 * In other words, this function take the multi-level (complete)
 * address ``addr'' and prefix length ``plen'' and return the
 * single level base index for the table ``at''.
 *
 * For example with an address size of 32bit divided into four
 * 8bit-long tables, there's a maximum of 4 base indexes if the
 * prefix length is > 24.
 */
int
art_bindex(struct art_table *at, uint8_t *addr, int plen)
{
	uint8_t			boff, bend;
	uint32_t 		k;

	if (plen < at->at_offset || plen > (at->at_offset + at->at_bits))
		return (-1);

	/*
	 * We are only interested in the part of the prefix length
	 * corresponding to the range of this table.
	 */
	plen -= at->at_offset;

	/*
	 * Jump to the first byte of the address containing bits
	 * covered by this table.
	 */
	addr += (at->at_offset / 8);

	/* ``at'' covers the bit range between ``boff'' & ``bend''. */
	boff = (at->at_offset % 8);
	bend = (at->at_bits + boff);

	KASSERT(bend <= 32);

	if (bend > 24) {
		k = (addr[0] & ((1 << (8 - boff)) - 1)) << (bend - 8);
		k |= addr[1] << (bend - 16);
		k |= addr[2] << (bend - 24);
		k |= addr[3] >> (32 - bend);
	} else if (bend > 16) {
		k = (addr[0] & ((1 << (8 - boff)) - 1)) << (bend - 8);
		k |= addr[1] << (bend - 16);
		k |= addr[2] >> (24 - bend);
	} else if (bend > 8) {
		k = (addr[0] & ((1 << (8 - boff)) - 1)) << (bend - 8);
		k |= addr[1] >> (16 - bend);
	} else {
		k = (addr[0] >> (8 - bend)) & ((1 << at->at_bits) - 1);
	}

	/*
	 * Single level base index formula:
	 */
	return ((k >> (at->at_bits - plen)) + (1 << plen));
}

/*
 * Single level lookup function.
 *
 * Return the fringe index of the part of ``addr''
 * corresponding to the range covered by the table ``at''.
 */
static inline int
art_findex(struct art_table *at, uint8_t *addr)
{
	return art_bindex(at, addr, (at->at_offset + at->at_bits));
}

/*
 * (Non-perfect) lookup API function.
 *
 * Return the best existing match for a destination.
 */
struct art_node *
art_match(struct art_root *ar, uint8_t *addr)
{
	struct art_table	*at;
	struct art_node		*dflt = NULL;
	int			 j;

	at = ar->ar_root;
	if (at == NULL)
		return (NULL);

	/*
	 * Iterate until we find a leaf.
	 */
	while (1) {
		/*
		 * Rember the default route of this table in case
		 * we do not find a better matching route.
		 */
		if (at->at_default != NULL)
			dflt = at->at_default;

		/* Do a single level route lookup. */
		j = art_findex(at, addr);

		/* If this is a leaf we're done. */
		if (ISLEAF(at->at_heap[j]))
			break;

		at = SUBTABLE(at->at_heap[j]);
	}

	if (at->at_heap[j].node != NULL)
		return (at->at_heap[j].node);

	return (dflt);
}

/*
 * Perfect lookup API function.
 *
 * Return a perfect match for a destination/prefix-length pair or NULL if
 * it does not exist.
 */
struct art_node *
art_lookup(struct art_root *ar, uint8_t *addr, int plen)
{
	struct art_table	*at;
	struct art_node		*an;
	int			 i, j;

	KASSERT(plen >= 0 && plen <= ar->ar_alen);

	at = ar->ar_root;
	if (at == NULL)
		return (NULL);

	/* Default route */
	if (plen == 0)
		return (at->at_default);

	/*
	 * If the prefix length is smaller than the sum of
	 * the stride length at this level the entry must
	 * be in the current table.
	 */
	while (plen > (at->at_offset + at->at_bits)) {
		/* Do a single level route lookup. */
		j = art_findex(at, addr);

		/* A leaf is a match, but not a perfect one. */
		if (ISLEAF(at->at_heap[j]))
			return (NULL);

		at = SUBTABLE(at->at_heap[j]);
	}

	i = art_bindex(at, addr, plen);
	if (i == -1)
		return (NULL);

	if (!ISLEAF(at->at_heap[i]))
		an = SUBTABLE(at->at_heap[i])->at_default;
	else
		an = at->at_heap[i].node;

	return (an);
}


/*
 * Insertion API function.
 *
 * Insert the given node or return an existing one if a node with the
 * same destination/mask pair is already present.
 */
struct art_node *
art_insert(struct art_root *ar, struct art_node *an, uint8_t *addr, int plen)
{
	struct art_table	*at;
	int			 i, j;

	KASSERT(plen >= 0 && plen <= ar->ar_alen);

	at = ar->ar_root;
	if (at == NULL) {
		at = art_table_get(ar, NULL, -1);
		if (at == NULL)
			return (NULL);

		ar->ar_root = at;
	}

	/* Default route */
	if (plen == 0) {
		art_table_ref(ar, at);
		at->at_default = an;
		return (an);
	}

	/*
	 * If the prefix length is smaller than the sum of
	 * the stride length at this level the entry must
	 * be in the current table.
	 */
	while (plen > (at->at_offset + at->at_bits)) {
		/* Do a single level route lookup. */
		j = art_findex(at, addr);

		/*
		 * If the node corresponding to the fringe index is
		 * a leaf we need to allocate a subtable.  The route
		 * entry of this node will then become the default
		 * route of the subtable.
		 */
		if (ISLEAF(at->at_heap[j])) {
			struct art_table  *child;

			child = art_table_get(ar, at, j);
			if (child == NULL)
				return (NULL);

			art_table_ref(ar, at);
			at->at_heap[j].node = ASNODE(child);
		}

		at = SUBTABLE(at->at_heap[j]);
	}

	i = art_bindex(at, addr, plen);
	if (i == -1)
		return (NULL);

	return (art_table_insert(ar, at, i, an));
}

/*
 * Single level insertion.
 */
struct art_node *
art_table_insert(struct art_root *ar, struct art_table *at, int i,
    struct art_node *an)
{
	struct art_node	*prev;

	if (!ISLEAF(at->at_heap[i]))
		prev = SUBTABLE(at->at_heap[i])->at_default;
	else
		prev = at->at_heap[i].node;

	if (art_check_duplicate(ar, prev, an))
		return (prev);

	art_table_ref(ar, at);

	/*
	 * If the index `i' of the route that we are inserting is not
	 * a fringe index, we need to allot this new route pointer to
	 * all the corresponding fringe indices.
	 */
	if (i < at->at_minfringe)
		art_allot(at, i, prev, an);
	else if (!ISLEAF(at->at_heap[i]))
		SUBTABLE(at->at_heap[i])->at_default = an;
	else
		at->at_heap[i].node = an;

	return (an);
}


/*
 * Deletion API function.
 */
struct art_node *
art_delete(struct art_root *ar, struct art_node *an, uint8_t *addr, int plen)
{
	struct art_table	*at;
	struct art_node		*dflt;
	int			 i, j;

	KASSERT(plen >= 0 && plen <= ar->ar_alen);

	at = ar->ar_root;
	if (at == NULL)
		return (NULL);

	/* Default route */
	if (plen == 0) {
		dflt = at->at_default;
		at->at_default = NULL;
		art_table_free(ar, at);
		return (dflt);
	}

	/*
	 * If the prefix length is smaller than the sum of
	 * the stride length at this level the entry must
	 * be in the current table.
	 */
	while (plen > (at->at_offset + at->at_bits)) {
		/* Do a single level route lookup. */
		j = art_findex(at, addr);

		/* If this is a leaf, there is no route to delete. */
		if (ISLEAF(at->at_heap[j]))
			return (NULL);

		at = SUBTABLE(at->at_heap[j]);
	}

	i = art_bindex(at, addr, plen);
	if (i == -1)
		return (NULL);

	return (art_table_delete(ar, at, i, an));
}

/*
 * Single level deletion.
 */
struct art_node *
art_table_delete(struct art_root *ar, struct art_table *at, int i,
    struct art_node *node)
{
	struct art_node		*next;

#ifdef DIAGNOSTIC
	struct art_node		*prev;

	if (!ISLEAF(at->at_heap[i]))
		prev = SUBTABLE(at->at_heap[i])->at_default;
	else
		prev = at->at_heap[i].node;

	KASSERT(prev == node);
#endif

	/* We are removing an entry from this table. */
	if (art_table_free(ar, at))
		return (node);

	/* Get the next most specific route for the index `i'. */
	if ((i >> 1) > 1)
		next = at->at_heap[i >> 1].node;
	else
		next = NULL;

	/*
	 * If the index `i' of the route that we are removing is not
	 * a fringe index, we need to allot the next most specific
	 * route pointer to all the corresponding fringe indices.
	 */
	if (i < at->at_minfringe)
		art_allot(at, i, node, next);
	else if (!ISLEAF(at->at_heap[i]))
		SUBTABLE(at->at_heap[i])->at_default = next;
	else
		at->at_heap[i].node = next;

	return (node);
}

void
art_table_ref(struct art_root *ar, struct art_table *at)
{
	at->at_refcnt++;
}

int
art_table_free(struct art_root *ar, struct art_table *at)
{
	if (--at->at_refcnt == 0) {
		/*
		 * Garbage collect this table and all its parents
		 * that are empty.
		 */
		do {
			at = art_table_put(ar, at);
		} while (at != NULL && --at->at_refcnt == 0);

		return (1);
	}

	return (0);
}

/*
 * Iteration API function.
 */
int
art_walk(struct art_root *ar, int (*f)(struct art_node *, void *), void *arg)
{
	struct art_table	*at;
	int			 error;

	at = ar->ar_root;
	if (at == NULL)
		return (0);

	/*
	 * The default route should be processed here because the root
	 * table does not have a parent.
	 */
	if (at->at_default != NULL) {
		error = (*f)(at->at_default, arg);
		if (error)
			return (error);
	}

	return (art_table_walk(ar, at, f, arg));
}

int
art_table_walk(struct art_root *ar, struct art_table *at,
    int (*f)(struct art_node *, void *), void *arg)
{
	struct art_node		*next, *an = NULL;
	int			 i, j, error = 0;
	uint32_t		 maxfringe = (at->at_minfringe << 1);

	/* Prevent this table to be freed while we're manipulating it. */
	art_table_ref(ar, at);

	/*
	 * Iterate non-fringe nodes in ``natural'' order.
	 */
	for (j = 1; j < at->at_minfringe; j += 2) {
		/*
		 * The default route (index 1) is processed by the
		 * parent table (where it belongs) otherwise it could
		 * be processed more than once.
		 */
		for (i = max(j, 2); i < at->at_minfringe; i <<= 1) {
			next = at->at_heap[i >> 1].node;
			an = at->at_heap[i].node;
			if ((an != NULL) && (an != next)) {
				error = (*f)(an, arg);
				if (error)
					goto out;
			}
		}
	}

	/*
	 * Iterate fringe nodes.
	 */
	for (i = at->at_minfringe; i < maxfringe; i++) {
		next = at->at_heap[i >> 1].node;
		if (!ISLEAF(at->at_heap[i]))
			an = SUBTABLE(at->at_heap[i])->at_default;
		else
			an = at->at_heap[i].node;
		if ((an != NULL) && (an != next)) {
			error = (*f)(an, arg);
			if (error)
				goto out;
		}

		if (ISLEAF(at->at_heap[i]))
			continue;

		error = art_table_walk(ar, SUBTABLE(at->at_heap[i]), f, arg);
		if (error)
			break;
	}

out:
	art_table_free(ar, at);
	return (error);
}


/*
 * Create a table and use the given index to set its default route.
 *
 * Note:  This function does not modify the root or the parent.
 */
struct art_table *
art_table_get(struct art_root *ar, struct art_table *parent, int j)
{
	struct art_table	*at;
	void			*at_heap;
	uint32_t		 lvl;

	KASSERT(j != 0 && j != 1);
	KASSERT(parent != NULL || j == -1);

	if (parent != NULL)
		lvl = parent->at_level + 1;
	else
		lvl = 0;

	KASSERT(lvl < ar->ar_nlvl);

	at = pool_get(&at_pool, PR_NOWAIT|PR_ZERO);
	if (at == NULL)
		return (NULL);

	switch (AT_HEAPSIZE(ar->ar_bits[lvl])) {
	case AT_HEAPSIZE(8):
		at_heap = pool_get(&at_heap_8_pool, PR_NOWAIT|PR_ZERO);
		break;
	case AT_HEAPSIZE(16):
		at_heap = pool_get(&at_heap_16_pool, PR_NOWAIT|PR_ZERO);
		break;
	default:
		panic("incorrect stride length %u", ar->ar_bits[lvl]);
	}

	if (at_heap == NULL) {
		pool_put(&at_pool, at);
		return (NULL);
	}

	at->at_parent = parent;
	at->at_index = j;
	at->at_minfringe = (1 << ar->ar_bits[lvl]);
	at->at_level = lvl;
	at->at_bits = ar->ar_bits[lvl];
	at->at_heap = at_heap;
	at->at_refcnt = 0;

	if (parent != NULL) {
		at->at_default = parent->at_heap[j].node;
		at->at_offset = (parent->at_offset + parent->at_bits);
	}

	return (at);
}


/*
 * Delete a table and use its index to restore its parent's default route.
 *
 * Note:  Modify its parent to unlink the table from it.
 */
struct art_table *
art_table_put(struct art_root *ar, struct art_table *at)
{
	struct art_table	*parent = at->at_parent;
	uint32_t		 lvl = at->at_level;
	uint32_t		 j = at->at_index;

	KASSERT(j != 0 && j != 1);
	KASSERT(parent != NULL || j == -1);

	if (parent != NULL) {
		KASSERT(lvl == parent->at_level + 1);
		KASSERT(parent->at_refcnt >= 1);

		/* Give the route back to its parent. */
		parent->at_heap[j].node = at->at_default;
	} else {
		ar->ar_root = NULL;
	}

	switch (AT_HEAPSIZE(ar->ar_bits[lvl])) {
	case AT_HEAPSIZE(8):
		pool_put(&at_heap_8_pool, at->at_heap);
		break;
	case AT_HEAPSIZE(16):
		pool_put(&at_heap_16_pool, at->at_heap);
		break;
	default:
		panic("incorrect stride length %u", ar->ar_bits[lvl]);
	}

	pool_put(&at_pool, at);

	return (parent);
}

/*
 * Substitute a node by another in the subtree whose root index is given.
 *
 * This function iterates on the table ``at'' at index ``i'' until no
 * more ``old'' node can be replaced by ``new''.
 *
 * This function was originally written by Don Knuth in CWEB. The
 * complicated ``goto''s are the result of expansion of the two
 * following recursions:
 *
 *	art_allot(at, i, old, new)
 *	{
 *		int k = i;
 *		if (at->at_heap[k] == old)
 *			at->at_heap[k] = new;
 *		if (k >= at->at_minfringe)
 *			 return;
 *		k <<= 1;
 *		art_allot(at, k, old, new);
 *		k++;
 *		art_allot(at, k, old, new);
 *	}
 */
void
art_allot(struct art_table *at, int i, struct art_node *old,
    struct art_node *new)
{
	int			k = i;

	KASSERT(i < at->at_minfringe);

again:
	k <<= 1;
	if (k < at->at_minfringe)
		goto nonfringe;

	/* Change fringe nodes. */
	while (1) {
		if (!ISLEAF(at->at_heap[k])) {
			if (SUBTABLE(at->at_heap[k])->at_default == old) {
				SUBTABLE(at->at_heap[k])->at_default = new;
			}
		} else if (at->at_heap[k].node == old) {
			at->at_heap[k].node = new;
		}
		if (k % 2)
			goto moveup;
		k++;
	}

nonfringe:
	if (at->at_heap[k].node == old)
		goto again;
moveon:
	if (k % 2)
		goto moveup;
	k++;
	goto nonfringe;
moveup:
	k >>= 1;
	at->at_heap[k].node = new;

	/* Change non-fringe node. */
	if (k != i)
		goto moveon;
}
