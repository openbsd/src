/*	$OpenBSD: rde.h,v 1.38 2004/05/07 10:06:15 djm Exp $ */

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
	PEER_ERR	/* error occured going to PEER_DOWN state */
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
	enum peer_state			 state;
	u_int32_t			 prefix_cnt;
	u_int32_t			 remote_bgpid;
	struct bgpd_addr		 remote_addr;
	struct bgpd_addr		 local_addr;
	u_int32_t			 up_pcnt;
	u_int32_t			 up_acnt;
	u_int32_t			 up_nlricnt;
	u_int32_t			 up_wcnt;
	struct uptree_prefix		 up_prefix;
	struct uptree_attr		 up_attrs;
	struct uplist_attr		 updates;
	struct uplist_prefix		 withdraws;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define ASPATH_HEADER_SIZE	sizeof(struct aspath_hdr)

struct aspath_hdr {
	u_int16_t			len;	/* total length of aspath
						   in octets */
	u_int16_t			as_cnt;	/* number of AS's in data */
	u_int16_t			prepend;
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

enum attrtypes {
	ATTR_UNDEF,
	ATTR_ORIGIN,
	ATTR_ASPATH,
	ATTR_NEXTHOP,
	ATTR_MED,
	ATTR_LOCALPREF,
	ATTR_ATOMIC_AGGREGATE,
	ATTR_AGGREGATOR,
	ATTR_COMMUNITIES
};

/* attribute flags. 4 low order bits reserved */
#define	ATTR_EXTLEN		0x10
#define ATTR_PARTIAL		0x20
#define ATTR_TRANSITIVE		0x40
#define ATTR_OPTIONAL		0x80

/* default attribute flags for well known attributes */
#define ATTR_WELL_KNOWN		ATTR_TRANSITIVE

struct attr {
	u_int8_t			 flags;
	u_int8_t			 type;
	u_int16_t			 len;
	u_char				*data;
	TAILQ_ENTRY(attr)		 attr_l;
};

TAILQ_HEAD(attr_list, attr);

#define ORIGIN_IGP		0
#define ORIGIN_EGP		1
#define ORIGIN_INCOMPLETE	2

struct attr_flags {
	struct aspath			*aspath;
	struct in_addr			 nexthop;	/* exit nexthop */
	char				 pftable[PFTABLE_LEN];
	u_int32_t			 med;		/* multi exit disc */
	u_int32_t			 lpref;		/* local pref */
	u_int8_t			 origin;
	u_int8_t			 wflags;	/* internally used */
	struct attr_list		 others;
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
	/*
	 * currently we use the boolean nexthop state, this could be exchanged
	 * with a variable coast with a max for unreachable.
	 */
	u_int32_t		costs;
#endif
	struct aspath_head	path_h;
	struct bgpd_addr	exit_nexthop;
	struct bgpd_addr	true_nexthop;
	struct bgpd_addr	nexthop_net;
	u_int8_t		nexthop_netlen;
	u_int8_t		flags;
#define NEXTHOP_CONNECTED	0x1
#define NEXTHOP_ANNOUNCE	0x2
};

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_int32_t		 path_hashmask;
};

LIST_HEAD(prefix_head, prefix);

struct rde_aspath {
	LIST_ENTRY(rde_aspath)		 peer_l, path_l, nexthop_l;
	struct prefix_head		 prefix_h;
	struct rde_peer			*peer;
	struct nexthop			*nexthop;
	u_int16_t			 prefix_cnt; /* # of prefixes */
	u_int16_t			 active_cnt; /* # of active prefixes */
	struct attr_flags		 flags;
};

struct pt_entry {
	LIST_ENTRY(pt_entry)		 pt_l;	/* currently we are using a
						   hash list for prefixes */
	struct bgpd_addr		 prefix;
	u_int8_t			 prefixlen;
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
void		 rde_send_nexthop(struct bgpd_addr *, int);
void		 rde_send_pftable(const char *, struct bgpd_addr *,
		     u_int8_t, int);
void		 rde_send_pftable_commit(void);

void		 rde_generate_updates(struct prefix *, struct prefix *);
u_int16_t	 rde_local_as(void);
int		 rde_noevaluate(void);

/* rde_attr.c */
void		 attr_init(struct attr_flags *);
int		 attr_parse(u_char *, u_int16_t, struct attr_flags *, int,
		     enum enforce_as, u_int16_t);
u_char		*attr_error(u_char *, u_int16_t, struct attr_flags *,
		     u_int8_t *, u_int16_t *);
u_int8_t	 attr_missing(struct attr_flags *, int);
int		 attr_compare(struct attr_flags *, struct attr_flags *);
void		 attr_copy(struct attr_flags *, struct attr_flags *);
void		 attr_move(struct attr_flags *, struct attr_flags *);
void		 attr_free(struct attr_flags *);
int		 attr_write(void *, u_int16_t, u_int8_t, u_int8_t, void *,
		     u_int16_t);
int		 attr_optadd(struct attr_flags *, u_int8_t, u_int8_t,
		     u_char *, u_int16_t);
struct attr	*attr_optget(struct attr_flags *, u_int8_t);
void		 attr_optfree(struct attr_flags *);

int		 aspath_verify(void *, u_int16_t);
#define		 AS_ERR_LEN	-1
#define		 AS_ERR_TYPE	-2
#define		 AS_ERR_BAD	-3
struct aspath	*aspath_create(void *, u_int16_t);
void		 aspath_destroy(struct aspath *);
int		 aspath_write(void *, u_int16_t, struct aspath *, u_int16_t,
		     int);
u_char		*aspath_dump(struct aspath *);
u_int16_t	 aspath_length(struct aspath *);
u_int16_t	 aspath_count(struct aspath *);
u_int16_t	 aspath_neighbor(struct aspath *);
u_int32_t	 aspath_hash(struct aspath *);
int		 aspath_loopfree(struct aspath *, u_int16_t);
int		 aspath_compare(struct aspath *, struct aspath *);
int		 aspath_snprint(char *, size_t, void *, u_int16_t);
int		 aspath_asprint(char **, void *, u_int16_t);
size_t		 aspath_strlen(void *, u_int16_t);
int		 aspath_match(struct aspath *, enum as_spec, u_int16_t);
int		 community_match(void *, u_int16_t, int, int);

/* rde_rib.c */
void		 path_init(u_int32_t);
void		 path_shutdown(void);
void		 path_update(struct rde_peer *, struct attr_flags *,
		     struct bgpd_addr *, int);
struct rde_aspath *path_get(struct aspath *, struct rde_peer *);
struct rde_aspath *path_add(struct rde_peer *, struct attr_flags *);
void		 path_remove(struct rde_aspath *);
void		 path_updateall(struct rde_aspath *, enum nexthop_state);
void		 path_destroy(struct rde_aspath *);
int		 path_empty(struct rde_aspath *);

struct prefix	*prefix_get(struct rde_aspath *, struct bgpd_addr *, int);
struct pt_entry	*prefix_add(struct rde_aspath *, struct bgpd_addr *, int);
struct pt_entry	*prefix_move(struct rde_aspath *, struct prefix *);
void		 prefix_remove(struct rde_peer *, struct bgpd_addr *, int);
struct prefix	*prefix_bypeer(struct pt_entry *, struct rde_peer *);
void		 prefix_updateall(struct rde_aspath *, enum nexthop_state);
void		 prefix_destroy(struct prefix *);
void		 prefix_network_clean(struct rde_peer *, time_t);

void		 nexthop_init(u_int32_t);
void		 nexthop_shutdown(void);
void		 nexthop_add(struct rde_aspath *);
void		 nexthop_remove(struct rde_aspath *);
void		 nexthop_update(struct kroute_nexthop *);

/* rde_decide.c */
void		 prefix_evaluate(struct prefix *, struct pt_entry *);
void		 up_init(struct rde_peer *);
void		 up_down(struct rde_peer *);
void		 up_generate_updates(struct rde_peer *,
		     struct prefix *, struct prefix *);
int		 up_dump_prefix(u_char *, int, struct uplist_prefix *,
		     struct rde_peer *);
int		 up_dump_attrnlri(u_char *, int, struct rde_peer *);
void		 up_dump_upcall(struct pt_entry *, void *);

/* rde_prefix.c */
void		 pt_init(void);
void		 pt_shutdown(void);
int		 pt_empty(struct pt_entry *);
struct pt_entry	*pt_get(struct bgpd_addr *, int);
struct pt_entry *pt_add(struct bgpd_addr *, int);
void		 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct bgpd_addr *);
void		 pt_dump(void (*)(struct pt_entry *, void *), void *);

/* rde_filter.c */
enum filter_actions rde_filter(struct rde_peer *, struct attr_flags *,
    struct bgpd_addr *, u_int8_t, enum directions);
void		 rde_apply_set(struct attr_flags *, struct filter_set *);
int		 rde_filter_community(struct attr_flags *, int, int);

#endif /* __RDE_H__ */
