/*	$OpenBSD: rtable.h,v 1.22 2018/11/23 16:24:11 claudio Exp $ */

/*
 * Copyright (c) 2014-2016 Martin Pieuchot
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

#ifndef	_NET_RTABLE_H_
#define	_NET_RTABLE_H_

#if !defined(_KERNEL) && !defined(ART)

/*
 * Traditional BSD routing table implementation based on a radix tree.
 */
#include <net/radix.h>

#define	rt_key(rt)	(((struct sockaddr *)(rt)->rt_nodes[0].rn_key))
#define	rt_mask(rt)	(((struct sockaddr *)(rt)->rt_nodes[0].rn_mask))
#define	rt_plen(rt)	(rtable_satoplen(rt_key(rt)->sa_family, rt_mask(rt)))
#define	RT_ROOT(rt)	((rt)->rt_nodes[0].rn_flags & RNF_ROOT)

#else /* defined(_KERNEL) || defined(ART) */

/*
 * Newer routing table implementation based on ART (Allotment Routing
 * Table).
 */
#include <net/art.h>

#define	rt_key(rt)	((rt)->rt_dest)
#define	rt_plen(rt)	((rt)->rt_plen)
#define	RT_ROOT(rt)	(0)

#endif /* defined(_KERNEL) || defined(ART) */

int		 rtable_satoplen(sa_family_t, struct sockaddr *);

void		 rtable_init(void);
int		 rtable_exists(unsigned int);
int		 rtable_empty(unsigned int);
int		 rtable_add(unsigned int);
unsigned int	 rtable_l2(unsigned int);
unsigned int	 rtable_loindex(unsigned int);
void		 rtable_l2set(unsigned int, unsigned int, unsigned int);

struct rtentry	*rtable_lookup(unsigned int, struct sockaddr *,
		     struct sockaddr *, struct sockaddr *, uint8_t);
struct rtentry	*rtable_match(unsigned int, struct sockaddr *, uint32_t *);
struct rtentry	*rtable_iterate(struct rtentry *);
int		 rtable_insert(unsigned int, struct sockaddr *,
		     struct sockaddr *, struct sockaddr *, uint8_t,
		     struct rtentry *);
int		 rtable_delete(unsigned int, struct sockaddr *,
		     struct sockaddr *, struct rtentry *);
int		 rtable_walk(unsigned int, sa_family_t,
		     int (*)(struct rtentry *, void *, unsigned int), void *);

int		 rtable_mpath_capable(unsigned int, sa_family_t);
struct rtentry	*rtable_mpath_match(unsigned int, struct rtentry *,
		     struct sockaddr *, uint8_t);
int		 rtable_mpath_reprio(unsigned int, struct sockaddr *, int,
		     uint8_t, struct rtentry *);
#endif /* _NET_RTABLE_H_ */
