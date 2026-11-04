/*	$OpenBSD: rtable.h,v 1.37 2026/11/04 08:51:52 claudio Exp $ */

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

#include <sys/rwlock.h>

/*
 * Newer routing table implementation based on ART (Allotment Routing
 * Table).
 */

#define	rt_key(rt)	((rt)->rt_dest)
#define	rt_plen(rt)	((rt)->rt_plen)
#define	RT_ROOT(rt)	(0)

struct art;
struct ip_mrouter;
struct ip6_mrouter;

/*
 * Locks used to protect struct members in this file:
 *	I	immutable after creation
 *	N	net lock
 *	S	SMR pointer
 *	X	only adjusted while the table is empty or unused
 *
 * The next three structs live here rather than in rtable.c because
 * netstat(1) reads them out of a kernel image with kvm(3); a layout
 * change here is a netstat change.  See usr.bin/netstat/route.c.
 */

/* One address family's table within a routing table. */
struct rtidx {
	struct rwlock	 r_lock;
	struct art	*r_art;		/* [I] routing table */
	struct sockaddr *r_source;	/* [N] use optional src addr */
	unsigned int	 r_off;		/* [I] Offset of key in bytes */
};

/* A routing table.  Immortal once created. */
struct rtable {
	unsigned int		 rt_rdomain;	/* [X] */
	unsigned int		 rt_loifidx;	/* [X] */
	struct ip_mrouter	*rt_mrouter;	/* [S] */
	struct ip6_mrouter	*rt_mrouter6;	/* [S] */

	struct rtidx		 rt_idx[0];	/* af2idx_max entries */
};

/*
 * Array of rtable pointers.  The limit lives inside the allocation so
 * that a reader loads one SMR pointer and gets the bound and the array
 * it guards from the same object, as if_idxmap does for if_map.
 */
struct rtable_map {
	unsigned int	  m_limit;
	struct rtable	 *m_tbl[0];	/* [S] m_limit entries */
};

#define RTABLE_MAP_SIZE(n)						\
	(sizeof(struct rtable_map) + (n) * sizeof(struct rtable *))

int		 rtable_satoplen(sa_family_t, const struct sockaddr *);

void		 rtable_init(void);
int		 rtable_exists(unsigned int);
unsigned int	 rtable_limit(void);
int		 rtable_empty(unsigned int);
int		 rtable_add(unsigned int);
unsigned int	 rtable_l2(unsigned int);
unsigned int	 rtable_loindex(unsigned int);
struct ip_mrouter	*rtable_get_mrouter(unsigned int);
struct ip6_mrouter	*rtable_get_mrouter6(unsigned int);
struct ip_mrouter	*rtable_set_mrouter(unsigned int, struct ip_mrouter *);
struct ip6_mrouter	*rtable_set_mrouter6(unsigned int, struct ip6_mrouter*);
void		 rtable_l2set(unsigned int, unsigned int, unsigned int);

int		 rtable_setsource(unsigned int, int, struct sockaddr *);
struct sockaddr *rtable_getsource(unsigned int, int);
void		 rtable_clearsource(unsigned int, struct sockaddr *);
struct rtentry	*rtable_lookup(unsigned int, const struct sockaddr *,
		     const struct sockaddr *, const struct sockaddr *, uint8_t);
struct rtentry	*rtable_match(unsigned int, const struct sockaddr *,
		     uint32_t *);
struct rtentry	*rtable_iterate(struct rtentry *);
int		 rtable_insert(unsigned int, struct sockaddr *,
		     const struct sockaddr *, const struct sockaddr *, uint8_t,
		     struct rtentry *);
int		 rtable_delete(unsigned int, const struct sockaddr *,
		     const struct sockaddr *, struct rtentry *);
int		 rtable_walk(unsigned int, sa_family_t, struct rtentry **,
		     int (*)(struct rtentry *, void *, unsigned int), void *);
int		 rtable_read(unsigned int, sa_family_t,
		     int (*)(const struct rtentry *, void *, unsigned int),
		     void *);

int		 rtable_mpath_capable(unsigned int, sa_family_t);
int		 rtable_mpath_reprio(unsigned int, struct sockaddr *, int,
		     uint8_t, struct rtentry *);

#endif /* _NET_RTABLE_H_ */
