/*	$OpenBSD: rde.h,v 1.10 2003/12/30 13:03:27 henning Exp $ */

/*
 * Copyright (c) 2003 Claudio Jeker <claudio@openbsd.org> and
 *                    Andre Oppermann <oppermann@pipeline.ch>
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
#ifndef __RDE_H__
#define __RDE_H__

#include <sys/types.h>
#include <sys/queue.h>

#include "bgpd.h"

/* XXX generic stuff, should be somewhere else */
/* Address Family Numbers as per rfc1700 */
#define AFI_IPv4	1
#define AFI_IPv6	2

/* Subsequent Address Family Identifier as per rfc2858 */
#define SAFI_UNICAST	1
#define SAFI_MULTICAST	2
#define SAFI_BOTH	3

/* rde internal structures */

enum peer_state {
	PEER_NONE,
	PEER_DOWN,
	PEER_UP
};

/*
 * How do we identify peers between the session handler and the rde?
 * Currently I assume that we can do that with the neighbor_ip...
 */
LIST_HEAD(rde_peer_head, rde_peer);
LIST_HEAD(aspath_head, rde_aspath);

struct rde_peer {
	LIST_ENTRY(rde_peer)		hash_l;	/* hash list over all peers */
	LIST_ENTRY(rde_peer)		peer_l; /* list of all peers */
	struct aspath_head		path_h; /* list of all as paths */
	struct peer_config		conf;
	u_int32_t			remote_bgpid;
	enum peer_state			state;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define ASPATH_HEADER_SIZE	sizeof(struct aspath_hdr)

struct aspath_hdr {
	u_int16_t			len;	/* total length of aspath
						   in octets */
	u_int16_t			as_cnt;	/* total number of AS's */
	/* probably we should switch these to int or something similar */
	/* char	*str; string representation of aspath for regex search. */
};

struct aspath {
	struct aspath_hdr		hdr;
	u_char				data[1];
	/*
	 * data consists of multiple struct aspath_segment with a length of
	 * len octets. In zebra data is a pointer to some memory location with
	 * the aspath segments. We could do it like this but then we should
	 * remove the pointer from rde_aspath and store the aspath header
	 * directly there.
	 */
};

struct astags {
	u_int16_t			len;
	u_int16_t			size;
	struct community {
		u_int16_t		as_num;
		u_int16_t		value;
	}				astag[1];
	/* this beast is variable sized */
	/*
	 * XXX does not work. Because of possible unaligned access to
	 * u_int16_t. This needs to be solved somewhat differently.
	 */
};

enum attrtypes {
	ATTR_UNDEF,
	ATTR_ORIGIN,
	ATTR_ASPATH,
	ATTR_NEXTHOP,
	ATTR_MED,
	ATTR_LOCALPREF,
	ATTR_ATOMIC_AGGREGATE,
	ATTR_AGGREGATOR
};

/* attribute flags. 4 low order bits reserved */
#define	ATTR_EXTLEN		0x10
#define ATTR_PARTIAL		0x20
#define ATTR_TRANSITIVE		0x40
#define ATTR_OPTIONAL		0x80

/* default attribute flags for well known attributes */
#define ATTR_ORIGIN_FLAGS	ATTR_TRANSITIVE
#define ATTR_NEXTHOP_FLAGS	ATTR_TRANSITIVE
#define ATTR_MED_FLAGS		ATTR_OPTIONAL
#define ATTR_LOCALPREF_FLAGS	ATTR_TRANSITIVE
#define ATTR_ATOMIC_AGGREGATE_FLAGS	ATTR_TRANSITIVE
#define ATTR_AGGREGATOR_FLAGS	(ATTR_OPTIONAL | ATTR_TRANSITIVE)

enum origins {
	ORIGIN_IGP,
	ORIGIN_EGP,
	ORIGIN_INCOMPLETE
};

struct attr_flags {
	enum origins			 origin;
	struct aspath			*aspath;
	struct astags			*astags;
	struct in_addr			 nexthop;	/* exit nexthop */
	u_int32_t			 med;		/* multi exit disc */
	u_int32_t			 lpref;		/* local pref */
	u_int8_t			 aggr_atm;	/* atomic aggregate */
	u_int16_t			 aggr_as;	/* aggregator as */
	struct in_addr			 aggr_ip;	/* aggregator ip */
};

enum nexthop_state {
	NEXTHOP_LOOKUP,
	NEXTHOP_UNREACH,
	NEXTHOP_REACH
};

struct nexthop {
	LIST_ENTRY(nexthop)	nexthop_l;
	enum nexthop_state	state;
#if 0
	u_int32_t		costs;
#endif
	struct aspath_head	path_h;
	struct in_addr		exit_nexthop;
	struct in_addr		true_nexthop;
	u_int8_t		connected;
};

LIST_HEAD(prefix_head, prefix);

struct rde_aspath {
	LIST_ENTRY(rde_aspath)		 peer_l, path_l, nexthop_l;
	struct prefix_head		 prefix_h;
	struct rde_peer			*peer;
	struct nexthop			*nexthop;
	u_int16_t			 prefix_cnt; /* # of prefixes */
	u_int16_t			 active_cnt; /* # of active prefixes */
	/*
	 * currently we use the boolean nexthop state, this could be exchanged
	 * with a variable coast with a max for unreachable.
	 */
#if 0
	u_int32_t			 nexthop_costs;
#endif
	struct attr_flags		 flags;
};

struct pt_entry {
	LIST_ENTRY(pt_entry)		 pt_l;	/* currently we are using a
						   hash list for prefixes */
	struct in_addr			 prefix;
	int				 prefixlen;
	struct prefix_head		 prefix_h;
	struct prefix			*active; /* for fast access */
	/*
	 * Route Flap Damping structures
	 * Currently I think they belong into the prefix but for the moment
	 * we just ignore the dampening at all.
	 */
};

struct prefix {
	LIST_ENTRY(prefix)		 prefix_l, path_l;
	struct rde_aspath		*aspath;
	struct pt_entry			*prefix;
	struct rde_peer			*peer;
	time_t				 lastchange;
/* currently I can't think of additional prefix flags.
 * NOTE: the selected route is stored in prefix->active */
};

/* prototypes */
/* rde.c */
void		 rde_send_kroute(struct prefix *, struct prefix *);
void		 rde_send_nexthop(in_addr_t, int);

/* rde_rib.c */
int		 attr_equal(struct attr_flags *, struct attr_flags *);
void		 attr_copy(struct attr_flags *, struct attr_flags *);
u_int16_t	 attr_length(struct attr_flags *);
int		 attr_dump(void *, u_int16_t, struct attr_flags *);

int		 aspath_verify(void *, u_int16_t, u_int16_t);
#define		 AS_ERR_LEN	-1
#define		 AS_ERR_TYPE	-2
#define		 AS_ERR_LOOP	-3
struct aspath	*aspath_create(void *, u_int16_t);
void		 aspath_destroy(struct aspath *);
u_char		*aspath_dump(struct aspath *);
u_int16_t	 aspath_length(struct aspath *);
u_int16_t	 aspath_count(struct aspath *);
u_int16_t	 aspath_neighbour(struct aspath *);
u_long		 aspath_hash(struct aspath *);
int		 aspath_equal(struct aspath *, struct aspath *);

void		 path_init(u_long);
void		 path_update(struct rde_peer *, struct attr_flags *,
			    struct in_addr , int);
struct rde_aspath *path_get(struct aspath *, struct rde_peer *);
struct rde_aspath *path_add(struct rde_peer *, struct attr_flags *);
void		 path_remove(struct rde_aspath *);
void		 path_updateall(struct rde_aspath *, enum nexthop_state);
void		 path_destroy(struct rde_aspath *);
int		 path_empty(struct rde_aspath *);

struct prefix	*prefix_get(struct rde_aspath *, struct in_addr, int);
struct pt_entry	*prefix_add(struct rde_aspath *, struct in_addr, int);
struct pt_entry	*prefix_move(struct rde_aspath *, struct prefix *);
void		 prefix_remove(struct rde_peer *, struct in_addr, int);
struct prefix	*prefix_bypeer(struct pt_entry *, struct rde_peer *);
void		 prefix_updateall(struct rde_aspath *, enum nexthop_state);
void		 prefix_destroy(struct prefix *);

void		 nexthop_init(u_long);
void		 nexthop_add(struct rde_aspath *);
void		 nexthop_remove(struct rde_aspath *);
void		 nexthop_update(struct kroute_nexthop *);

/* rde_decide.c */
void		 prefix_evaluate(struct prefix *, struct pt_entry *);

/* rde_prefix.c */
void		 pt_init(void);
int		 pt_empty(struct pt_entry *);
struct pt_entry	*pt_get(struct in_addr, int);
struct pt_entry *pt_add(struct in_addr, int);
void		 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct in_addr);
void		 pt_dump(void (*)(struct pt_entry *, void *), void *);


#endif /* __RDE_H__ */
