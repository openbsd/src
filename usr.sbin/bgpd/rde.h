/*	$OpenBSD: rde.h,v 1.168 2018/02/10 05:54:31 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org> and
 *                          Andre Oppermann <oppermann@networx.ch>
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
#include <stdint.h>

#include "bgpd.h"
#include "log.h"

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
LIST_HEAD(aspath_list, aspath);
LIST_HEAD(attr_list, attr);
LIST_HEAD(prefix_list, prefix);
TAILQ_HEAD(prefix_queue, prefix);
LIST_HEAD(aspath_head, rde_aspath);
TAILQ_HEAD(aspath_queue, rde_aspath);
RB_HEAD(uptree_prefix, update_prefix);
RB_HEAD(uptree_attr, update_attr);

struct rib_desc;
struct rib;
RB_HEAD(rib_tree, rib_entry);
TAILQ_HEAD(uplist_prefix, update_prefix);
TAILQ_HEAD(uplist_attr, update_attr);

struct rde_peer {
	LIST_ENTRY(rde_peer)		 hash_l; /* hash list over all peers */
	LIST_ENTRY(rde_peer)		 peer_l; /* list of all peers */
	struct aspath_queue		 path_h; /* list of all as paths */
	struct peer_config		 conf;
	struct bgpd_addr		 remote_addr;
	struct bgpd_addr		 local_v4_addr;
	struct bgpd_addr		 local_v6_addr;
	struct uptree_prefix		 up_prefix;
	struct uptree_attr		 up_attrs;
	struct uplist_attr		 updates[AID_MAX];
	struct uplist_prefix		 withdraws[AID_MAX];
	struct capabilities		 capa;
	time_t				 staletime[AID_MAX];
	u_int64_t			 prefix_rcvd_update;
	u_int64_t			 prefix_rcvd_withdraw;
	u_int64_t			 prefix_rcvd_eor;
	u_int64_t			 prefix_sent_update;
	u_int64_t			 prefix_sent_withdraw;
	u_int64_t			 prefix_sent_eor;
	u_int32_t			 prefix_cnt; /* # of prefixes */
	u_int32_t			 remote_bgpid; /* host byte order! */
	u_int32_t			 up_pcnt;
	u_int32_t			 up_acnt;
	u_int32_t			 up_nlricnt;
	u_int32_t			 up_wcnt;
	enum peer_state			 state;
	struct rib			*rib;
	u_int16_t			 short_as;
	u_int16_t			 mrt_idx;
	u_int8_t			 reconf_out;	/* out filter changed */
	u_int8_t			 reconf_rib;	/* rib changed */
	u_int8_t			 throttled;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define AS_CONFED_SEQUENCE	3
#define AS_CONFED_SET		4
#define ASPATH_HEADER_SIZE	(sizeof(struct aspath) - sizeof(u_char))

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
	ATTR_MP_UNREACH_NLRI=15,
	ATTR_EXT_COMMUNITIES=16,
	ATTR_AS4_PATH=17,
	ATTR_AS4_AGGREGATOR=18,
	ATTR_LARGE_COMMUNITIES=32,
};

/* attribute flags. 4 low order bits reserved */
#define	ATTR_EXTLEN		0x10
#define ATTR_PARTIAL		0x20
#define ATTR_TRANSITIVE		0x40
#define ATTR_OPTIONAL		0x80
#define ATTR_RESERVED		0x0f
/* by default mask the reserved bits and the ext len bit */
#define ATTR_DEFMASK		(ATTR_RESERVED | ATTR_EXTLEN)

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

struct path_table {
	struct aspath_head	*path_hashtbl;
	u_int32_t		 path_hashmask;
};

#define	F_ATTR_ORIGIN		0x00001
#define	F_ATTR_ASPATH		0x00002
#define	F_ATTR_NEXTHOP		0x00004
#define	F_ATTR_LOCALPREF	0x00008
#define	F_ATTR_MED		0x00010
#define	F_ATTR_MED_ANNOUNCE	0x00020
#define	F_ATTR_MP_REACH		0x00040
#define	F_ATTR_MP_UNREACH	0x00080
#define	F_ATTR_AS4BYTE_NEW	0x00100	/* AS4_PATH or AS4_AGGREGATOR */
#define	F_ATTR_LOOP		0x00200 /* path would cause a route loop */
#define	F_PREFIX_ANNOUNCED	0x00400
#define	F_ANN_DYNAMIC		0x00800
#define	F_NEXTHOP_SELF		0x01000
#define	F_NEXTHOP_REJECT	0x02000
#define	F_NEXTHOP_BLACKHOLE	0x04000
#define	F_NEXTHOP_NOMODIFY	0x08000
#define	F_NEXTHOP_MASK		0x0f000
#define	F_ATTR_PARSE_ERR	0x10000 /* parse error, not eligable */
#define	F_ATTR_LINKED		0x20000 /* if set path is on various lists */
#define	F_ATTR_UPDATE		0x20000 /* if set linked on update_l */


#define ORIGIN_IGP		0
#define ORIGIN_EGP		1
#define ORIGIN_INCOMPLETE	2

#define DEFAULT_LPREF		100

struct rde_aspath {
	LIST_ENTRY(rde_aspath)		 path_l, nexthop_l;
	TAILQ_ENTRY(rde_aspath)		 peer_l, update_l;
	struct prefix_queue		 prefixes, updates;
	struct attr			**others;
	struct rde_peer			*peer;
	struct aspath			*aspath;
	struct nexthop			*nexthop;	/* may be NULL */
	u_int32_t			 med;		/* multi exit disc */
	u_int32_t			 lpref;		/* local pref */
	u_int32_t			 weight;	/* low prio lpref */
	u_int32_t			 prefix_cnt; /* # of prefixes */
	u_int32_t			 active_cnt; /* # of active prefixes */
	u_int32_t			 flags;		/* internally used */
	u_int16_t			 rtlabelid;	/* route label id */
	u_int16_t			 pftableid;	/* pf table id */
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
	int			refcnt;	/* filterset reference counter */
	enum nexthop_state	state;
	u_int8_t		nexthop_netlen;
	u_int8_t		flags;
#define NEXTHOP_CONNECTED	0x01
};

/* generic entry without address specific part */
struct pt_entry {
	RB_ENTRY(pt_entry)		 pt_e;
	u_int8_t			 aid;
	u_int8_t			 prefixlen;
	u_int16_t			 refcnt;
};

struct pt_entry4 {
	RB_ENTRY(pt_entry)		 pt_e;
	u_int8_t			 aid;
	u_int8_t			 prefixlen;
	u_int16_t			 refcnt;
	struct in_addr			 prefix4;
};

struct pt_entry6 {
	RB_ENTRY(pt_entry)		 pt_e;
	u_int8_t			 aid;
	u_int8_t			 prefixlen;
	u_int16_t			 refcnt;
	struct in6_addr			 prefix6;
};

struct pt_entry_vpn4 {
	RB_ENTRY(pt_entry)		 pt_e;
	u_int8_t			 aid;
	u_int8_t			 prefixlen;
	u_int16_t			 refcnt;
	struct in_addr			 prefix4;
	u_int64_t			 rd;
	u_int8_t			 labelstack[21];
	u_int8_t			 labellen;
	u_int8_t			 pad1;
	u_int8_t			 pad2;
};

struct rib_context {
	struct rib_entry		*ctx_re;
	struct rib			*ctx_rib;
	void		(*ctx_upcall)(struct rib_entry *, void *);
	void		(*ctx_done)(void *);
	void		(*ctx_wait)(void *);
	void				*ctx_arg;
	unsigned int			 ctx_count;
	u_int8_t			 ctx_aid;
};

struct rib_entry {
	RB_ENTRY(rib_entry)	 rib_e;
	struct prefix_list	 prefix_h;
	struct prefix		*active;	/* for fast access */
	struct pt_entry		*prefix;
	struct rib		*__rib;		/* mangled pointer with flags */
};

struct rib {
	struct rib_tree		tree;
	u_int			rtableid;
	u_int16_t		flags;
	u_int16_t		id;
};

struct rib_desc {
	char			name[PEER_DESCR_LEN];
	struct rib		rib;
	struct filter_head	*in_rules;
	struct filter_head	*in_rules_tmp;
	enum reconf_action 	state;
};

#define RIB_ADJ_IN	0
#define RIB_ADJ_OUT	1
#define RIB_LOC_START	2

struct prefix {
	LIST_ENTRY(prefix)		 rib_l;
	TAILQ_ENTRY(prefix)		 path_l;
	struct rib_entry		*re;
	union {
		struct rde_aspath		*_aspath;
	}				 _p;
	time_t				 lastchange;
	int				 flags;
};

#define F_PREFIX_USE_UPDATES	0x01	/* linked onto the updates list */

extern struct rde_memstats rdemem;

/* prototypes */
/* mrt.c */
int		mrt_dump_v2_hdr(struct mrt *, struct bgpd_config *,
		    struct rde_peer_head *);
void		mrt_dump_upcall(struct rib_entry *, void *);
void		mrt_done(void *);

/* rde.c */
void		rde_send_kroute(struct rib *, struct prefix *, struct prefix *);
void		rde_send_nexthop(struct bgpd_addr *, int);
void		rde_send_pftable(u_int16_t, struct bgpd_addr *,
		    u_int8_t, int);
void		rde_send_pftable_commit(void);

void		rde_generate_updates(struct rib *, struct prefix *,
		    struct prefix *);
u_int32_t	rde_local_as(void);
int		rde_noevaluate(void);
int		rde_decisionflags(void);
int		rde_as4byte(struct rde_peer *);

/* rde_attr.c */
int		 attr_write(void *, u_int16_t, u_int8_t, u_int8_t, void *,
		     u_int16_t);
int		 attr_writebuf(struct ibuf *, u_int8_t, u_int8_t, void *,
		     u_int16_t);
void		 attr_init(u_int32_t);
void		 attr_shutdown(void);
int		 attr_optadd(struct rde_aspath *, u_int8_t, u_int8_t,
		     void *, u_int16_t);
struct attr	*attr_optget(const struct rde_aspath *, u_int8_t);
void		 attr_copy(struct rde_aspath *, struct rde_aspath *);
int		 attr_compare(struct rde_aspath *, struct rde_aspath *);
void		 attr_freeall(struct rde_aspath *);
void		 attr_free(struct rde_aspath *, struct attr *);
#define		 attr_optlen(x)	\
    ((x)->len > 255 ? (x)->len + 4 : (x)->len + 3)

int		 aspath_verify(void *, u_int16_t, int);
#define		 AS_ERR_LEN	-1
#define		 AS_ERR_TYPE	-2
#define		 AS_ERR_BAD	-3
#define		 AS_ERR_SOFT	-4
void		 aspath_init(u_int32_t);
void		 aspath_shutdown(void);
struct aspath	*aspath_get(void *, u_int16_t);
void		 aspath_put(struct aspath *);
u_char		*aspath_inflate(void *, u_int16_t, u_int16_t *);
u_char		*aspath_deflate(u_char *, u_int16_t *, int *);
void		 aspath_merge(struct rde_aspath *, struct attr *);
u_char		*aspath_dump(struct aspath *);
u_int16_t	 aspath_length(struct aspath *);
u_int16_t	 aspath_count(const void *, u_int16_t);
u_int32_t	 aspath_neighbor(struct aspath *);
int		 aspath_loopfree(struct aspath *, u_int32_t);
int		 aspath_compare(struct aspath *, struct aspath *);
u_char		*aspath_prepend(struct aspath *, u_int32_t, int, u_int16_t *);
int		 aspath_lenmatch(struct aspath *, enum aslen_spec, u_int);
int		 community_match(struct rde_aspath *, int, int);
int		 community_set(struct rde_aspath *, int, int);
void		 community_delete(struct rde_aspath *, int, int);
int		 community_large_match(struct rde_aspath *, int64_t, int64_t,
		    int64_t);
int		 community_large_set(struct rde_aspath *, int64_t, int64_t,
		    int64_t);
void		 community_large_delete(struct rde_aspath *, int64_t,
		    int64_t, int64_t);
int		 community_ext_match(struct rde_aspath *,
		    struct filter_extcommunity *, u_int16_t);
int		 community_ext_set(struct rde_aspath *,
		    struct filter_extcommunity *, u_int16_t);
void		 community_ext_delete(struct rde_aspath *,
		    struct filter_extcommunity *, u_int16_t);
int		 community_ext_conv(struct filter_extcommunity *, u_int16_t,
		    u_int64_t *);
u_char		*community_ext_delete_non_trans(u_char *, u_int16_t,
		    u_int16_t *);

/* rde_decide.c */
void		 prefix_evaluate(struct prefix *, struct rib_entry *);

/* rde_filter.c */
enum filter_actions rde_filter(struct filter_head *, struct rde_aspath **,
		     struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, u_int8_t, struct rde_peer *);
void		 rde_apply_set(struct rde_aspath *, struct filter_set_head *,
		     u_int8_t, struct rde_peer *, struct rde_peer *);
int		 rde_filter_equal(struct filter_head *, struct filter_head *,
		     struct rde_peer *, struct prefixset_head *);
void		 rde_filter_calc_skip_steps(struct filter_head *);

/* rde_prefix.c */
static inline int
pt_empty(struct pt_entry *pt)
{
	return (pt->refcnt == 0);
}

static inline void
pt_ref(struct pt_entry *pt)
{
	++pt->refcnt;
	if (pt->refcnt == 0)
		fatalx("pt_ref: overflow");
}

static inline void
pt_unref(struct pt_entry *pt)
{
	if (pt->refcnt == 0)
		fatalx("pt_unref: underflow");
	--pt->refcnt;
}

void	 pt_init(void);
void	 pt_shutdown(void);
void	 pt_getaddr(struct pt_entry *, struct bgpd_addr *);
struct pt_entry	*pt_fill(struct bgpd_addr *, int);
struct pt_entry	*pt_get(struct bgpd_addr *, int);
struct pt_entry *pt_add(struct bgpd_addr *, int);
void	 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct bgpd_addr *);
int	 pt_prefix_cmp(const struct pt_entry *, const struct pt_entry *);

/* rde_rib.c */
extern u_int16_t	 rib_size;
extern struct rib_desc	*ribs;

struct rib	*rib_new(char *, u_int, u_int16_t);
struct rib	*rib_find(char *);
struct rib_desc	*rib_desc(struct rib *);
void		 rib_free(struct rib *);
struct rib_entry *rib_get(struct rib *, struct bgpd_addr *, int);
struct rib_entry *rib_lookup(struct rib *, struct bgpd_addr *);
void		 rib_dump(struct rib *, void (*)(struct rib_entry *, void *),
		     void *, u_int8_t);
void		 rib_dump_r(struct rib_context *);

static inline struct rib *
re_rib(struct rib_entry *re)
{
	return (struct rib *)((intptr_t)re->__rib & ~1);
}

void		 path_init(u_int32_t);
void		 path_init(u_int32_t);
void		 path_shutdown(void);
int		 path_update(struct rib *, struct rde_peer *,
		     struct rde_aspath *, struct bgpd_addr *, int, int);
int		 path_compare(struct rde_aspath *, struct rde_aspath *);
void		 path_remove(struct rde_aspath *);
u_int32_t	 path_remove_stale(struct rde_aspath *, u_int8_t);
void		 path_destroy(struct rde_aspath *);
int		 path_empty(struct rde_aspath *);
struct rde_aspath *path_copy(struct rde_aspath *);
struct rde_aspath *path_get(void);
void		 path_put(struct rde_aspath *);

#define	PREFIX_SIZE(x)	(((x) + 7) / 8 + 1)
int		 prefix_remove(struct rib *, struct rde_peer *,
		    struct bgpd_addr *, int, u_int32_t);
int		 prefix_write(u_char *, int, struct bgpd_addr *, u_int8_t, int);
int		 prefix_writebuf(struct ibuf *, struct bgpd_addr *, u_int8_t);
struct prefix	*prefix_bypeer(struct rib_entry *, struct rde_peer *,
		     u_int32_t);
void		 prefix_updateall(struct rde_aspath *, enum nexthop_state,
		     enum nexthop_state);
void		 prefix_destroy(struct prefix *);
void		 prefix_network_clean(struct rde_peer *, time_t, u_int32_t);
void		 prefix_relink(struct prefix *, struct rde_aspath *, int);

static inline struct rde_aspath *
prefix_aspath(struct prefix *p)
{
	return (p->_p._aspath);
}

static inline struct rde_peer *
prefix_peer(struct prefix *p)
{
	return (p->_p._aspath->peer);
}

void		 nexthop_init(u_int32_t);
void		 nexthop_shutdown(void);
void		 nexthop_modify(struct rde_aspath *, struct bgpd_addr *,
		     enum action_types, u_int8_t);
void		 nexthop_link(struct rde_aspath *);
void		 nexthop_unlink(struct rde_aspath *);
int		 nexthop_delete(struct nexthop *);
void		 nexthop_update(struct kroute_nexthop *);
struct nexthop	*nexthop_get(struct bgpd_addr *);
int		 nexthop_compare(struct nexthop *, struct nexthop *);

/* rde_update.c */
void		 up_init(struct rde_peer *);
void		 up_down(struct rde_peer *);
int		 up_test_update(struct rde_peer *, struct prefix *);
int		 up_generate(struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, u_int8_t);
void		 up_generate_updates(struct filter_head *, struct rde_peer *,
		     struct prefix *, struct prefix *);
void		 up_generate_default(struct filter_head *, struct rde_peer *,
		     u_int8_t);
int		 up_generate_marker(struct rde_peer *, u_int8_t);
int		 up_dump_prefix(u_char *, int, struct uplist_prefix *,
		     struct rde_peer *, int);
int		 up_dump_attrnlri(u_char *, int, struct rde_peer *);
u_char		*up_dump_mp_unreach(u_char *, u_int16_t *, struct rde_peer *,
		     u_int8_t);
int		 up_dump_mp_reach(u_char *, u_int16_t *, struct rde_peer *,
		     u_int8_t);

#endif /* __RDE_H__ */
