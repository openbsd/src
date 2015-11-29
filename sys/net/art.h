/* $OpenBSD: art.h,v 1.7 2015/11/29 16:02:18 mpi Exp $ */

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

#define ART_MAXLVL	16	/* We currently use 16 levels for IPv6. */

/*
 * Root of the ART tables, equivalent to the radix head.
 */
struct art_root {
	struct art_table	*ar_root;	/* First table */
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
	struct sockaddr		*an_dst;	/* Destination address (key) */
	int			 an_plen;	/* Prefix length */

	struct srpl		 an_rtlist;	/* Route related to this node */
};

void		 art_init(void);
struct art_root	*art_alloc(unsigned int, int);
void		 art_free(struct art_root *);
struct art_node *art_insert(struct art_root *, struct art_node *, uint8_t *,
		     int);
struct art_node *art_delete(struct art_root *, struct art_node *, uint8_t *,
		     int);
struct art_node	*art_match(struct art_root *, uint8_t *);
struct art_node *art_lookup(struct art_root *, uint8_t *, int);
int		 art_walk(struct art_root *,
		     int (*)(struct art_node *, void *), void *);

#endif /* _NET_ART_H_ */
