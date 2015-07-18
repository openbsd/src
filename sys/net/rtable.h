/*	$OpenBSD: rtable.h,v 1.1 2015/07/18 15:51:16 mpi Exp $ */

/*
 * Copyright (c) 2014-2015 Martin Pieuchot
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

/*
 * Traditional BSD routing table implementation based on a radix tree.
 */
#include <net/radix.h>
#ifndef SMALL_KERNEL
#include <net/radix_mpath.h>
#endif

#define	rt_key(rt)	(((struct sockaddr *)(rt)->rt_nodes[0].rn_key))
#define	rt_mask(rt)	(((struct sockaddr *)(rt)->rt_nodes[0].rn_mask))
#define	RT_ACTIVE(rt)	((rt)->rt_nodes[0].rn_flags & RNF_ACTIVE)
#define	RT_ROOT(rt)	((rt)->rt_nodes[0].rn_flags & RNF_ROOT)

void		 rtable_init(void);
int		 rtable_attach(void **, int);
struct rtentry	*rtable_lookup(unsigned int, struct sockaddr *,
		     struct sockaddr *);
struct rtentry	*rtable_match(unsigned int, struct sockaddr *);
int		 rtable_insert(unsigned int, struct sockaddr *,
		     struct sockaddr *, uint8_t, struct rtentry *);
int		 rtable_delete(unsigned int, struct sockaddr *,
		     struct sockaddr *, uint8_t, struct rtentry *);

int		 rtable_setid(void **, unsigned int, sa_family_t);
void		*rtable_get(unsigned int, sa_family_t);
int		 rtable_walk(unsigned int, sa_family_t,
		     int (*)(struct rtentry *, void *, unsigned int), void *);

int		 rtable_mpath_capable(unsigned int, sa_family_t);
struct rtentry	*rtable_mpath_match(unsigned int, struct rtentry *,
		     struct sockaddr *, uint8_t);
int		 rtable_mpath_conflict(unsigned int, struct sockaddr *,
		     struct sockaddr *, struct sockaddr *, uint8_t, int);
struct rtentry	*rtable_mpath_select(struct rtentry *, uint32_t *);
void		 rtable_mpath_reprio(struct rtentry *, uint8_t);
#endif /* _NET_RTABLE_H_ */
