/*	$OpenBSD: rde.h,v 1.83 2006/01/14 22:39:49 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org> and
 *                          Andre Oppermann <oppermann@pipeline.ch>
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
#include <sys/tree.h>

#include "bgpd.h"

/* rde internal structures */

enum peer_state {
	PEER_NONE,
	PEER_DOWN,
	PEER_UP,
	PEER_ERR	/* error occurred going to PEER_DOWN state */
};

/*
 * How do we identify peers between the session handler and the rde?
 * Currently I assume that we can do that with the neighbor_ip...
 */
LIST_HEAD(rde_peer_head, rde_peer);
LIST_HEAD(aspath_head, rde_aspath);
RB_HEAD(uptree_prefix, update_prefix);
RB_HEAD(uptree_attr, update_attr);
TAILQ_HEAD(uplist_prefix, update_prefix);
TAILQ_HEAD(uplist_attr, update_attr);

struct rde_peer {
	LIST_ENTRY(rde_peer)		 hash_l; /* hash list over all peers */
	LIST_ENTRY(rde_peer)		 peer_l; /* list of all peers */
	struct aspath_head		 path_h; /* list of all as paths */
	struct peer_config		 conf;
	struct bgpd_addr		 remote_addr;
	struct bgpd_addr		 local_v4_addr;
	struct bgpd_addr		 local_v6_addr;
	struct uptree_prefix		 up_prefix;
	struct uptree_attr		 up_attrs;
	struct uplist_attr		 updates;
	struct uplist_prefix		 withdraws;
	struct uplist_attr		 updates6;
	struct uplist_prefix		 withdraws6;
	struct capabilities		 capa_announced;
	struct capabilities		 capa_received;
	u_int32_t			 prefix_cnt;
	u_int32_t			 remote_bgpid;
	u_int32_t			 up_pcnt;
	u_int32_t			 up_acnt;
	u_int32_t			 up_nlricnt;
	u_int32_t			 up_wcnt;
	enum peer_state			 state;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define ASPATH_HEADER_SIZE	(sizeof(struct aspath) - sizeof(u_char))

LIST_HEAD(aspath_list, aspath);

struct aspath {
	LIST_ENTRY(aspath)	entry;
	int			refcnt;	/* reference count */
	u_int16_t		len;	/* total length of aspath in octets */
	u_int16_t		ascnt;	/* number of AS hops in data */
	u_char			data[1]; /* placeholder for actual data */
};

enum attrtypes {
	ATTR_UNDEF,
	ATTR_ORIGIN,
	ATTR_ASPATH,
	ATTR_NEXTHOP,
	ATTR_MED,
	ATTR_LOCALPREF,
	ATTR_ATOMIC_AGGREGATE,
	ATTR_AGGREGATOR,
	ATTR_COMMUNITIES,
	ATTR_ORIGINATOR_ID,
	ATTR_CLUSTER_LIST,
	ATTR_MP_REACH_NLRI=14,
	ATTR_MP_UNREACH_NLRI=15
};

/* attribute flags. 4 low order bits reserved */
#define	ATTR_EXTLEN		0x10
#define ATTR_PARTIAL		0x20
#define ATTR_TRANSITIVE		0x40
#define ATTR_OPTIONAL		0x80

/* default attribute flags for well known attributes */
#define ATTR_WELL_KNOWN		ATTR_TRANSITIVE

struct attr {
	LIST_ENTRY(attr)		 entry;
	u_char				*data;
	int				 refcnt;
	u_int32_t			 hash;
	u_int16_t			 len;
	u_int8_t			 flags;
	u_int8_t			 type;
};

struct mpattr {
	void		*reach;
	void		*unreach;
	u_int16_t	 reach_len;
	u_int16_t	 unreach_len;
};

LIST_HEAD(attr_list, attr);

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_int32_t		 path_hashmask;
};

LIST_HEAD(prefix_head, prefix);

#define	F_ATTR_ORIGIN		0x0001
#define	F_ATTR_ASPATH		0x0002
#define	F_ATTR_NEXTHOP		0x0004
#define	F_ATTR_LOCALPREF	0x0008
#define	F_ATTR_MED		0x0010
#define	F_ATTR_MED_ANNOUNCE	0x0020
#define	F_ATTR_MP_REACH		0x0040
#define	F_ATTR_MP_UNREACH	0x0080
#define	F_PREFIX_ANNOUNCED	0x0100
#define	F_NEXTHOP_REJECT	0x0200
#define	F_NEXTHOP_BLACKHOLE	0x0400
#define	F_NEXTHOP_NOMODIFY	0x0800
#define	F_ATTR_LINKED		0x1000
#define	F_LOCAL			0x2000	/* Local-RIB */
#define	F_ORIGINAL		0x4000	/* Adj-RIB-In */


#define ORIGIN_IGP		0
#define ORIGIN_EGP		1
#define ORIGIN_INCOMPLETE	2

#define DEFAULT_LPREF		100

struct rde_aspath {
	LIST_ENTRY(rde_aspath)		 path_l, peer_l, nexthop_l;
	struct prefix_head		 prefix_h;
	struct attr			**others;
	struct rde_peer			*peer;
	struct aspath			*aspath;
	struct nexthop			*nexthop;	/* may be NULL */
	u_int32_t			 med;		/* multi exit disc */
	u_int32_t			 lpref;		/* local pref */
	u_int32_t			 weight;	/* low prio lpref */
	u_int16_t			 rtlabelid;	/* route label id */
	u_int16_t			 pftableid;	/* pf table id */
	u_int16_t			 flags;		/* internally used */
	u_int16_t			 prefix_cnt; /* # of prefixes */
	u_int16_t			 active_cnt; /* # of active prefixes */
	u_int8_t			 origin;
	u_int8_t			 others_len;
};

enum nexthop_state {
	NEXTHOP_LOOKUP,
	NEXTHOP_UNREACH,
	NEXTHOP_REACH
};

struct nexthop {
	LIST_ENTRY(nexthop)	nexthop_l;
	struct aspath_head	path_h;
	struct bgpd_addr	exit_nexthop;
	struct bgpd_addr	true_nexthop;
	struct bgpd_addr	nexthop_net;
#if 0
	/*
	 * currently we use the boolean nexthop state, this could be exchanged
	 * with a variable cost with a max for unreachable.
	 */
	u_int32_t		costs;
#endif
	enum nexthop_state	state;
	u_int8_t		nexthop_netlen;
	u_int8_t		flags;
#define NEXTHOP_CONNECTED	0x01
};

/* generic entry without address specific part */
struct pt_entry {
	RB_ENTRY(pt_entry)		 pt_e;
	sa_family_t			 af;
	u_int8_t			 prefixlen;
	struct prefix_head		 prefix_h;
	struct prefix			*active; /* for fast access */
};

struct pt_entry4 {
	RB_ENTRY(pt_entry)		 pt_e;
	sa_family_t			 af;
	u_int8_t			 prefixlen;
	struct prefix_head		 prefix_h;
	struct prefix			*active; /* for fast access */
	struct in_addr			 prefix4;
	/*
	 * Route Flap Damping structures
	 * Currently I think they belong into the prefix but for the moment
	 * we just ignore the dampening at all.
	 */
};

struct pt_entry6 {
	RB_ENTRY(pt_entry)		 pt_e;
	sa_family_t			 af;
	u_int8_t			 prefixlen;
	struct prefix_head		 prefix_h;
	struct prefix			*active; /* for fast access */
	struct in6_addr			 prefix6;
};

struct prefix {
	LIST_ENTRY(prefix)		 prefix_l, path_l;
	struct rde_aspath		*aspath;
	struct pt_entry			*prefix;
	time_t				 lastchange;
	u_int32_t			 flags;
};

extern struct rde_memstats rdemem;

/* prototypes */
/* rde.c */
void		 rde_send_kroute(struct prefix *, struct prefix *);
void		 rde_send_nexthop(struct bgpd_addr *, int);
void		 rde_send_pftable(u_int16_t, struct bgpd_addr *,
		     u_int8_t, int);
void		 rde_send_pftable_commit(void);

void		 rde_generate_updates(struct prefix *, struct prefix *);
u_int16_t	 rde_local_as(void);
int		 rde_noevaluate(void);
int		 rde_decisionflags(void);

/* rde_attr.c */
int		 attr_write(void *, u_int16_t, u_int8_t, u_int8_t, void *,
		     u_int16_t);
void		 attr_init(u_int32_t);
void		 attr_shutdown(void);
int		 attr_optadd(struct rde_aspath *, u_int8_t, u_int8_t,
		     void *, u_int16_t);
struct attr	*attr_optget(const struct rde_aspath *, u_int8_t);
void		 attr_copy(struct rde_aspath *, struct rde_aspath *);
int		 attr_compare(struct rde_aspath *, struct rde_aspath *);
void		 attr_freeall(struct rde_aspath *);

int		 aspath_verify(void *, u_int16_t);
#define		 AS_ERR_LEN	-1
#define		 AS_ERR_TYPE	-2
#define		 AS_ERR_BAD	-3
void		 aspath_init(u_int32_t);
void		 aspath_shutdown(void);
struct aspath	*aspath_get(void *, u_int16_t);
void		 aspath_put(struct aspath *);
u_char		*aspath_dump(struct aspath *);
u_int16_t	 aspath_length(struct aspath *);
u_int16_t	 aspath_count(const void *, u_int16_t);
u_int16_t	 aspath_neighbor(struct aspath *);
int		 aspath_loopfree(struct aspath *, u_int16_t);
int		 aspath_compare(struct aspath *, struct aspath *);
struct aspath	*aspath_prepend(struct aspath *, u_int16_t, int);
int		 aspath_match(struct aspath *, enum as_spec, u_int16_t);
int		 community_match(void *, u_int16_t, int, int);
int		 community_set(struct rde_aspath *, int, int);

/* rde_rib.c */
void		 path_init(u_int32_t);
void		 path_shutdown(void);
void		 path_update(struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, int);
int		 path_compare(struct rde_aspath *, struct rde_aspath *);
struct rde_aspath *path_lookup(struct rde_aspath *, struct rde_peer *);
void		 path_remove(struct rde_aspath *);
void		 path_updateall(struct rde_aspath *, enum nexthop_state);
void		 path_destroy(struct rde_aspath *);
int		 path_empty(struct rde_aspath *);
struct rde_aspath *path_copy(struct rde_aspath *);
struct rde_aspath *path_get(void);
void		 path_put(struct rde_aspath *);

#define	PREFIX_SIZE(x)	(((x) + 7) / 8 + 1)
int		 prefix_compare(const struct bgpd_addr *,
		    const struct bgpd_addr *, int);
struct prefix	*prefix_get(struct rde_peer *, struct bgpd_addr *, int,
		    u_int32_t);
struct pt_entry	*prefix_add(struct rde_aspath *, struct bgpd_addr *, int,
		    u_int32_t);
struct pt_entry	*prefix_move(struct rde_aspath *, struct prefix *);
void		 prefix_remove(struct rde_peer *, struct bgpd_addr *, int,
		    u_int32_t);
int		 prefix_write(u_char *, int, struct bgpd_addr *, u_int8_t);
struct prefix	*prefix_bypeer(struct pt_entry *, struct rde_peer *, u_int32_t);
void		 prefix_updateall(struct rde_aspath *, enum nexthop_state);
void		 prefix_destroy(struct prefix *);
void		 prefix_network_clean(struct rde_peer *, time_t);

void		 nexthop_init(u_int32_t);
void		 nexthop_shutdown(void);
void		 nexthop_modify(struct rde_aspath *, struct bgpd_addr *,
		     enum action_types, sa_family_t);
void		 nexthop_link(struct rde_aspath *);
void		 nexthop_unlink(struct rde_aspath *);
void		 nexthop_update(struct kroute_nexthop *);
struct nexthop	*nexthop_get(struct bgpd_addr *);
int		 nexthop_compare(struct nexthop *, struct nexthop *);

/* rde_decide.c */
void		 prefix_evaluate(struct prefix *, struct pt_entry *);

/* rde_update.c */
void		 up_init(struct rde_peer *);
void		 up_down(struct rde_peer *);
int		 up_test_update(struct rde_peer *, struct prefix *);
int		 up_generate(struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, u_int8_t);
void		 up_generate_updates(struct filter_head *, struct rde_peer *,
		     struct prefix *, struct prefix *);
void		 up_generate_default(struct filter_head *, struct rde_peer *,
		     sa_family_t);
int		 up_dump_prefix(u_char *, int, struct uplist_prefix *,
		     struct rde_peer *);
int		 up_dump_attrnlri(u_char *, int, struct rde_peer *);
char		*up_dump_mp_unreach(u_char *, u_int16_t *, struct rde_peer *);
char		*up_dump_mp_reach(u_char *, u_int16_t *, struct rde_peer *);

/* rde_prefix.c */
void		 pt_init(void);
void		 pt_shutdown(void);
int		 pt_empty(struct pt_entry *);
void		 pt_getaddr(struct pt_entry *, struct bgpd_addr *);
struct pt_entry	*pt_get(struct bgpd_addr *, int);
struct pt_entry *pt_add(struct bgpd_addr *, int);
void		 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct bgpd_addr *);
void		 pt_dump(void (*)(struct pt_entry *, void *), void *,
		     sa_family_t);

/* rde_filter.c */
enum filter_actions rde_filter(struct rde_aspath **, struct filter_head *,
		     struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, u_int8_t, struct rde_peer *,
		     enum directions);
void		 rde_apply_set(struct rde_aspath *, struct filter_set_head *,
		     sa_family_t, struct rde_peer *, enum directions);
int		 rde_filter_community(struct rde_aspath *, int, int);
int		 rde_filter_equal(struct filter_head *, struct filter_head *,
		     enum directions);

/* util.c */
u_int16_t	 aspath_extract(const void *, int);

#endif /* __RDE_H__ */
