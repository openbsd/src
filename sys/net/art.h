/* $OpenBSD: art.h,v 1.15 2016/08/30 07:42:57 jmatthew Exp $ */

/*
 * Copyright (c) 2015 Martin Pieuchot
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

#ifndef _NET_ART_H_
#define _NET_ART_H_

#include <sys/rwlock.h>

#define ART_MAXLVL	32	/* We currently use 32 levels for IPv6. */

/*
 * Root of the ART tables, equivalent to the radix head.
 */
struct art_root {
	struct srp		 ar_root;	/* First table */
	struct rwlock		 ar_lock;	/* Serialise modifications */
	uint8_t			 ar_bits[ART_MAXLVL];	/* Per level stride */
	uint8_t			 ar_nlvl;	/* Number of levels */
	uint8_t			 ar_alen;	/* Address length in bits */
	uint8_t			 ar_off;	/* Offset of the key in bytes */
	unsigned int		 ar_rtableid;	/* ID of this routing table */
};

/*
 * Forward declaration needed for the list of mpath routes
 * attached to a single ART node.
 */
struct rtentry;

/*
 * A node is the internal representation of a route entry.
 */
struct art_node {
	SRPL_HEAD(, rtentry)	 an_rtlist;	/* Route related to this node */
	union {
		struct sockaddr	*an__dst;	/* Destination address (key) */
		struct art_node	*an__gc;	/* Entry on GC list */
	}			 an_pointer;
	uint8_t			 an_plen;	/* Prefix length */
};
#define an_dst	an_pointer.an__dst
#define an_gc	an_pointer.an__gc

void		 art_init(void);
struct art_root	*art_alloc(unsigned int, unsigned int, unsigned int);
struct art_node *art_insert(struct art_root *, struct art_node *, uint8_t *,
		     int);
struct art_node *art_delete(struct art_root *, struct art_node *, uint8_t *,
		     int);
struct art_node	*art_match(struct art_root *, uint8_t *, struct srp_ref *);
struct art_node *art_lookup(struct art_root *, uint8_t *, int,
		     struct srp_ref *);
int		 art_walk(struct art_root *,
		     int (*)(struct art_node *, void *), void *);

struct art_node	*art_get(struct sockaddr *, uint8_t);
void		 art_put(struct art_node *);

#endif /* _NET_ART_H_ */
