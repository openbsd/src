/*	$OpenBSD: rde.h,v 1.262 2022/08/03 08:56:23 claudio Exp $ */

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
#include <stddef.h>

#include "bgpd.h"
#include "log.h"

/* rde internal structures */

enum peer_state {
	PEER_NONE,
	PEER_DOWN,
	PEER_UP,
	PEER_ERR	/* error occurred going to PEER_DOWN state */
};

LIST_HEAD(prefix_list, prefix);
TAILQ_HEAD(prefix_queue, prefix);
RB_HEAD(rib_tree, rib_entry);

struct rib_entry {
	RB_ENTRY(rib_entry)	 rib_e;
	struct prefix_queue	 prefix_h;
	struct pt_entry		*prefix;
	uint16_t		 rib_id;
	uint16_t		 lock;
};

struct rib {
	struct rib_tree		tree;
	char			name[PEER_DESCR_LEN];
	struct filter_head	*in_rules;
	struct filter_head	*in_rules_tmp;
	u_int			rtableid;
	u_int			rtableid_tmp;
	enum reconf_action	state, fibstate;
	uint16_t		id;
	uint16_t		flags;
	uint16_t		flags_tmp;
};

#define RIB_ADJ_IN	0
#define RIB_LOC_START	1
#define RIB_NOTFOUND	0xffff

/*
 * How do we identify peers between the session handler and the rde?
 * Currently I assume that we can do that with the neighbor_ip...
 */
LIST_HEAD(rde_peer_head, rde_peer);
LIST_HEAD(aspath_list, aspath);
LIST_HEAD(attr_list, attr);
LIST_HEAD(aspath_head, rde_aspath);
RB_HEAD(prefix_tree, prefix);
RB_HEAD(prefix_index, prefix);
struct iq;

struct rde_peer {
	LIST_ENTRY(rde_peer)		 hash_l; /* hash list over all peers */
	LIST_ENTRY(rde_peer)		 peer_l; /* list of all peers */
	SIMPLEQ_HEAD(, iq)		 imsg_queue;
	struct peer_config		 conf;
	struct bgpd_addr		 remote_addr;
	struct bgpd_addr		 local_v4_addr;
	struct bgpd_addr		 local_v6_addr;
	struct capabilities		 capa;
	struct addpath_eval		 eval;
	struct prefix_index		 adj_rib_out;
	struct prefix_tree		 updates[AID_MAX];
	struct prefix_tree		 withdraws[AID_MAX];
	time_t				 staletime[AID_MAX];
	uint64_t			 prefix_rcvd_update;
	uint64_t			 prefix_rcvd_withdraw;
	uint64_t			 prefix_rcvd_eor;
	uint64_t			 prefix_sent_update;
	uint64_t			 prefix_sent_withdraw;
	uint64_t			 prefix_sent_eor;
	uint32_t			 prefix_cnt;
	uint32_t			 prefix_out_cnt;
	uint32_t			 remote_bgpid; /* host byte order! */
	uint32_t			 up_nlricnt;
	uint32_t			 up_wcnt;
	enum peer_state			 state;
	enum export_type		 export_type;
	uint16_t			 loc_rib_id;
	uint16_t			 short_as;
	uint16_t			 mrt_idx;
	uint8_t				 recv_eor;	/* bitfield per AID */
	uint8_t				 sent_eor;	/* bitfield per AID */
	uint8_t				 reconf_out;	/* out filter changed */
	uint8_t				 reconf_rib;	/* rib changed */
	uint8_t				 throttled;
	uint8_t				 flags;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define AS_CONFED_SEQUENCE	3
#define AS_CONFED_SET		4
#define ASPATH_HEADER_SIZE	(offsetof(struct aspath, data))

struct aspath {
	LIST_ENTRY(aspath)	entry;
	uint32_t		source_as;	/* cached source_as */
	int			refcnt;	/* reference count */
	uint16_t		len;	/* total length of aspath in octets */
	uint16_t		ascnt;	/* number of AS hops in data */
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
	ATTR_OTC=35,
	ATTR_FIRST_UNKNOWN,	/* after this all attributes are unknown */
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
	uint64_t			 hash;
	int				 refcnt;
	uint16_t			 len;
	uint8_t				 flags;
	uint8_t				 type;
};

struct mpattr {
	void		*reach;
	void		*unreach;
	uint16_t	 reach_len;
	uint16_t	 unreach_len;
};

struct rde_community {
	LIST_ENTRY(rde_community)	entry;
	size_t				size;
	size_t				nentries;
	int				flags;
	int				refcnt;
	struct community		*communities;
};

#define	PARTIAL_COMMUNITIES		0x01
#define	PARTIAL_LARGE_COMMUNITIES	0x02
#define	PARTIAL_EXT_COMMUNITIES		0x04

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
#define	F_ATTR_OTC		0x01000	/* OTC present */
#define	F_ATTR_OTC_LOOP		0x02000 /* otc loop, not eligable */
#define	F_ATTR_PARSE_ERR	0x10000 /* parse error, not eligable */
#define	F_ATTR_LINKED		0x20000 /* if set path is on various lists */

#define ORIGIN_IGP		0
#define ORIGIN_EGP		1
#define ORIGIN_INCOMPLETE	2

#define DEFAULT_LPREF		100

struct rde_aspath {
	LIST_ENTRY(rde_aspath)		 path_l;
	struct attr			**others;
	struct aspath			*aspath;
	uint64_t			 hash;
	int				 refcnt;
	uint32_t			 flags;		/* internally used */
#define	aspath_hashstart	med
	uint32_t			 med;		/* multi exit disc */
	uint32_t			 lpref;		/* local pref */
	uint32_t			 weight;	/* low prio lpref */
	uint16_t			 rtlabelid;	/* route label id */
	uint16_t			 pftableid;	/* pf table id */
	uint8_t				 origin;
#define	aspath_hashend		others_len
	uint8_t				 others_len;
};

enum nexthop_state {
	NEXTHOP_LOOKUP,
	NEXTHOP_UNREACH,
	NEXTHOP_REACH,
	NEXTHOP_FLAPPED		/* only used by oldstate */
};

struct nexthop {
	LIST_ENTRY(nexthop)	nexthop_l;
	TAILQ_ENTRY(nexthop)	runner_l;
	struct prefix_list	prefix_h;
	struct prefix		*next_prefix;
	struct bgpd_addr	exit_nexthop;
	struct bgpd_addr	true_nexthop;
	struct bgpd_addr	nexthop_net;
#if 0
	/*
	 * currently we use the boolean nexthop state, this could be exchanged
	 * with a variable cost with a max for unreachable.
	 */
	uint32_t		costs;
#endif
	int			refcnt;
	enum nexthop_state	state;
	enum nexthop_state	oldstate;
	uint8_t			nexthop_netlen;
	uint8_t			flags;
#define NEXTHOP_CONNECTED	0x01
};

/* generic entry without address specific part */
struct pt_entry {
	RB_ENTRY(pt_entry)		 pt_e;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 refcnt;
};

struct pt_entry4 {
	RB_ENTRY(pt_entry)		 pt_e;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 refcnt;
	struct in_addr			 prefix4;
};

struct pt_entry6 {
	RB_ENTRY(pt_entry)		 pt_e;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 refcnt;
	struct in6_addr			 prefix6;
};

struct pt_entry_vpn4 {
	RB_ENTRY(pt_entry)		 pt_e;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 refcnt;
	struct in_addr			 prefix4;
	uint64_t			 rd;
	uint8_t				 labelstack[21];
	uint8_t				 labellen;
	uint8_t				 pad1;
	uint8_t				 pad2;
};

struct pt_entry_vpn6 {
	RB_ENTRY(pt_entry)		 pt_e;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 refcnt;
	struct in6_addr			 prefix6;
	uint64_t			 rd;
	uint8_t				 labelstack[21];
	uint8_t				 labellen;
	uint8_t				 pad1;
	uint8_t				 pad2;
};

struct prefix {
	union {
		struct {
			TAILQ_ENTRY(prefix)	 rib;
			LIST_ENTRY(prefix)	 nexthop;
			struct rib_entry	*re;
		} list;
		struct {
			RB_ENTRY(prefix)	 index, update;
		} tree;
	}				 entry;
	struct pt_entry			*pt;
	struct rde_aspath		*aspath;
	struct rde_community		*communities;
	struct rde_peer			*peer;
	struct nexthop			*nexthop;	/* may be NULL */
	time_t				 lastchange;
	uint32_t			 path_id;
	uint32_t			 path_id_tx;
	uint8_t				 validation_state;
	uint8_t				 nhflags;
	int8_t				 dmetric;	/* decision metric */
	uint8_t				 flags;
#define	PREFIX_FLAG_WITHDRAW	0x01	/* enqueued on withdraw queue */
#define	PREFIX_FLAG_UPDATE	0x02	/* enqueued on update queue */
#define	PREFIX_FLAG_DEAD	0x04	/* locked but removed */
#define	PREFIX_FLAG_STALE	0x08	/* stale entry (graceful reload) */
#define	PREFIX_FLAG_MASK	0x0f	/* mask for the prefix types */
#define	PREFIX_FLAG_ADJOUT	0x10	/* prefix is in the adj-out rib */
#define	PREFIX_FLAG_EOR		0x20	/* prefix is EoR */
#define	PREFIX_NEXTHOP_LINKED	0x40	/* prefix is linked onto nexthop list */
#define	PREFIX_FLAG_LOCKED	0x80	/* locked by rib walker */

#define	PREFIX_DMETRIC_NONE	0
#define	PREFIX_DMETRIC_INVALID	1
#define	PREFIX_DMETRIC_VALID	2
#define	PREFIX_DMETRIC_AS_WIDE	3
#define	PREFIX_DMETRIC_ECMP	4
#define	PREFIX_DMETRIC_BEST	5
};

/* possible states for nhflags */
#define	NEXTHOP_SELF		0x01
#define	NEXTHOP_REJECT		0x02
#define	NEXTHOP_BLACKHOLE	0x04
#define	NEXTHOP_NOMODIFY	0x08
#define	NEXTHOP_MASK		0x0f
#define	NEXTHOP_VALID		0x80

struct filterstate {
	struct rde_aspath	 aspath;
	struct rde_community	 communities;
	struct nexthop		*nexthop;
	uint8_t			 nhflags;
};

enum eval_mode {
	EVAL_DEFAULT,
	EVAL_ALL,
	EVAL_RECONF,
};

extern struct rde_memstats rdemem;

/* prototypes */
/* mrt.c */
int		mrt_dump_v2_hdr(struct mrt *, struct bgpd_config *,
		    struct rde_peer_head *);
void		mrt_dump_upcall(struct rib_entry *, void *);

/* rde.c */
void		 rde_update_err(struct rde_peer *, uint8_t , uint8_t,
		    void *, uint16_t);
void		 rde_update_log(const char *, uint16_t,
		    const struct rde_peer *, const struct bgpd_addr *,
		    const struct bgpd_addr *, uint8_t);
void		rde_send_kroute_flush(struct rib *);
void		rde_send_kroute(struct rib *, struct prefix *, struct prefix *);
void		rde_send_nexthop(struct bgpd_addr *, int);
void		rde_pftable_add(uint16_t, struct prefix *);
void		rde_pftable_del(uint16_t, struct prefix *);

int		rde_evaluate_all(void);
void		rde_generate_updates(struct rib *, struct prefix *,
		    struct prefix *, enum eval_mode);
uint32_t	rde_local_as(void);
int		rde_decisionflags(void);
void		rde_peer_send_rrefresh(struct rde_peer *, uint8_t, uint8_t);
int		rde_match_peer(struct rde_peer *, struct ctl_neighbor *);

/* rde_peer.c */
int		 peer_has_as4byte(struct rde_peer *);
int		 peer_has_add_path(struct rde_peer *, uint8_t, int);
int		 peer_has_open_policy(struct rde_peer *, uint8_t *);
int		 peer_accept_no_as_set(struct rde_peer *);
void		 peer_init(uint32_t);
void		 peer_shutdown(void);
void		 peer_foreach(void (*)(struct rde_peer *, void *), void *);
struct rde_peer	*peer_get(uint32_t);
struct rde_peer *peer_match(struct ctl_neighbor *, uint32_t);
struct rde_peer	*peer_add(uint32_t, struct peer_config *);

int		 peer_up(struct rde_peer *, struct session_up *);
void		 peer_down(struct rde_peer *, void *);
void		 peer_flush(struct rde_peer *, uint8_t, time_t);
void		 peer_stale(struct rde_peer *, uint8_t);
void		 peer_dump(struct rde_peer *, uint8_t);
void		 peer_begin_rrefresh(struct rde_peer *, uint8_t);

void		 peer_imsg_push(struct rde_peer *, struct imsg *);
int		 peer_imsg_pop(struct rde_peer *, struct imsg *);
int		 peer_imsg_pending(void);
void		 peer_imsg_flush(struct rde_peer *);

/* rde_attr.c */
int		 attr_write(void *, uint16_t, uint8_t, uint8_t, void *,
		    uint16_t);
int		 attr_writebuf(struct ibuf *, uint8_t, uint8_t, void *,
		    uint16_t);
void		 attr_init(uint32_t);
void		 attr_shutdown(void);
void		 attr_hash_stats(struct rde_hashstats *);
int		 attr_optadd(struct rde_aspath *, uint8_t, uint8_t,
		    void *, uint16_t);
struct attr	*attr_optget(const struct rde_aspath *, uint8_t);
void		 attr_copy(struct rde_aspath *, const struct rde_aspath *);
int		 attr_compare(struct rde_aspath *, struct rde_aspath *);
uint64_t	 attr_hash(struct rde_aspath *);
void		 attr_freeall(struct rde_aspath *);
void		 attr_free(struct rde_aspath *, struct attr *);
#define		 attr_optlen(x)	\
    ((x)->len > 255 ? (x)->len + 4 : (x)->len + 3)

void		 aspath_init(uint32_t);
void		 aspath_shutdown(void);
void		 aspath_hash_stats(struct rde_hashstats *);
struct aspath	*aspath_get(void *, uint16_t);
void		 aspath_put(struct aspath *);
u_char		*aspath_deflate(u_char *, uint16_t *, int *);
void		 aspath_merge(struct rde_aspath *, struct attr *);
u_char		*aspath_dump(struct aspath *);
uint16_t	 aspath_length(struct aspath *);
uint32_t	 aspath_neighbor(struct aspath *);
uint32_t	 aspath_origin(struct aspath *);
int		 aspath_loopfree(struct aspath *, uint32_t);
int		 aspath_compare(struct aspath *, struct aspath *);
int		 aspath_match(struct aspath *, struct filter_as *, uint32_t);
u_char		*aspath_prepend(struct aspath *, uint32_t, int, uint16_t *);
u_char		*aspath_override(struct aspath *, uint32_t, uint32_t,
		    uint16_t *);
int		 aspath_lenmatch(struct aspath *, enum aslen_spec, u_int);

/* rde_community.c */
int	community_match(struct rde_community *, struct community *,
	    struct rde_peer *);
int	community_count(struct rde_community *, uint8_t type);
int	community_set(struct rde_community *, struct community *,
	    struct rde_peer *);
void	community_delete(struct rde_community *, struct community *,
	    struct rde_peer *);

int	community_add(struct rde_community *, int, void *, size_t);
int	community_large_add(struct rde_community *, int, void *, size_t);
int	community_ext_add(struct rde_community *, int, int, void *, size_t);

int	community_write(struct rde_community *, void *, uint16_t);
int	community_large_write(struct rde_community *, void *, uint16_t);
int	community_ext_write(struct rde_community *, int, void *, uint16_t);
int	community_writebuf(struct ibuf *, struct rde_community *);

void			 communities_init(uint32_t);
void			 communities_shutdown(void);
void			 communities_hash_stats(struct rde_hashstats *);
struct rde_community	*communities_lookup(struct rde_community *);
struct rde_community	*communities_link(struct rde_community *);
void			 communities_unlink(struct rde_community *);

int	 communities_equal(struct rde_community *, struct rde_community *);
void	 communities_copy(struct rde_community *, struct rde_community *);
void	 communities_clean(struct rde_community *);

static inline struct rde_community *
communities_ref(struct rde_community *comm)
{
	if (comm->refcnt == 0)
		fatalx("%s: not-referenced community", __func__);
	comm->refcnt++;
	rdemem.comm_refs++;
	return comm;
}

static inline void
communities_unref(struct rde_community *comm)
{
	if (comm == NULL)
		return;
	rdemem.comm_refs--;
	if (--comm->refcnt == 1)	/* last ref is hold internally */
		communities_unlink(comm);
}

int	community_to_rd(struct community *, uint64_t *);

/* rde_decide.c */
int		 prefix_eligible(struct prefix *);
struct prefix	*prefix_best(struct rib_entry *);
void		 prefix_evaluate(struct rib_entry *, struct prefix *,
		    struct prefix *);
void		 prefix_evaluate_nexthop(struct prefix *, enum nexthop_state,
		    enum nexthop_state);

/* rde_filter.c */
void	rde_apply_set(struct filter_set_head *, struct rde_peer *,
	    struct rde_peer *, struct filterstate *, uint8_t);
void	rde_filterstate_prep(struct filterstate *, struct rde_aspath *,
	    struct rde_community *, struct nexthop *, uint8_t);
void	rde_filterstate_clean(struct filterstate *);
int	rde_filter_equal(struct filter_head *, struct filter_head *,
	    struct rde_peer *);
void	rde_filter_calc_skip_steps(struct filter_head *);
enum filter_actions rde_filter(struct filter_head *, struct rde_peer *,
	    struct rde_peer *, struct bgpd_addr *, uint8_t, uint8_t,
	    struct filterstate *);

/* rde_prefix.c */
void	 pt_init(void);
void	 pt_shutdown(void);
void	 pt_getaddr(struct pt_entry *, struct bgpd_addr *);
struct pt_entry	*pt_fill(struct bgpd_addr *, int);
struct pt_entry	*pt_get(struct bgpd_addr *, int);
struct pt_entry *pt_add(struct bgpd_addr *, int);
void	 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct bgpd_addr *);
int	 pt_prefix_cmp(const struct pt_entry *, const struct pt_entry *);

static inline struct pt_entry *
pt_ref(struct pt_entry *pt)
{
	++pt->refcnt;
	if (pt->refcnt == 0)
		fatalx("pt_ref: overflow");
	return pt;
}

static inline void
pt_unref(struct pt_entry *pt)
{
	if (pt->refcnt == 0)
		fatalx("pt_unref: underflow");
	if (--pt->refcnt == 0)
		pt_remove(pt);
}

/* rde_rib.c */
extern uint16_t	rib_size;

struct rib	*rib_new(char *, u_int, uint16_t);
int		 rib_update(struct rib *);
struct rib	*rib_byid(uint16_t);
uint16_t	 rib_find(char *);
void		 rib_free(struct rib *);
void		 rib_shutdown(void);
struct rib_entry *rib_get(struct rib *, struct bgpd_addr *, int);
struct rib_entry *rib_match(struct rib *, struct bgpd_addr *);
int		 rib_dump_pending(void);
void		 rib_dump_runner(void);
int		 rib_dump_new(uint16_t, uint8_t, unsigned int, void *,
		    void (*)(struct rib_entry *, void *),
		    void (*)(void *, uint8_t),
		    int (*)(void *));
void		 rib_dump_terminate(void *);

static inline struct rib *
re_rib(struct rib_entry *re)
{
	return rib_byid(re->rib_id);
}

void		 path_init(uint32_t);
void		 path_shutdown(void);
void		 path_hash_stats(struct rde_hashstats *);
int		 path_compare(struct rde_aspath *, struct rde_aspath *);
uint32_t	 path_remove_stale(struct rde_aspath *, uint8_t, time_t);
struct rde_aspath *path_copy(struct rde_aspath *, const struct rde_aspath *);
struct rde_aspath *path_prep(struct rde_aspath *);
struct rde_aspath *path_get(void);
void		 path_clean(struct rde_aspath *);
void		 path_put(struct rde_aspath *);

#define	PREFIX_SIZE(x)	(((x) + 7) / 8 + 1)
struct prefix	*prefix_get(struct rib *, struct rde_peer *, uint32_t,
		    struct bgpd_addr *, int);
struct prefix	*prefix_adjout_get(struct rde_peer *, uint32_t,
		    struct bgpd_addr *, int);
struct prefix	*prefix_match(struct rde_peer *, struct bgpd_addr *);
struct prefix	*prefix_adjout_match(struct rde_peer *, struct bgpd_addr *);
struct prefix	*prefix_adjout_lookup(struct rde_peer *, struct bgpd_addr *,
		    int);
struct prefix	*prefix_adjout_next(struct rde_peer *, struct prefix *);
int		 prefix_update(struct rib *, struct rde_peer *, uint32_t,
		    uint32_t, struct filterstate *, struct bgpd_addr *,
		    int, uint8_t);
int		 prefix_withdraw(struct rib *, struct rde_peer *, uint32_t,
		    struct bgpd_addr *, int);
void		 prefix_add_eor(struct rde_peer *, uint8_t);
void		 prefix_adjout_update(struct prefix *, struct rde_peer *,
		    struct filterstate *, struct bgpd_addr *, int,
		    uint32_t, uint8_t);
void		 prefix_adjout_withdraw(struct prefix *);
void		 prefix_adjout_destroy(struct prefix *);
void		 prefix_adjout_dump(struct rde_peer *, void *,
		    void (*)(struct prefix *, void *));
int		 prefix_dump_new(struct rde_peer *, uint8_t, unsigned int,
		    void *, void (*)(struct prefix *, void *),
		    void (*)(void *, uint8_t), int (*)(void *));
int		 prefix_write(u_char *, int, struct bgpd_addr *, uint8_t, int);
int		 prefix_writebuf(struct ibuf *, struct bgpd_addr *, uint8_t);
struct prefix	*prefix_bypeer(struct rib_entry *, struct rde_peer *,
		    uint32_t);
void		 prefix_destroy(struct prefix *);
void		 prefix_relink(struct prefix *, struct rde_aspath *, int);

RB_PROTOTYPE(prefix_tree, prefix, entry, prefix_cmp)

static inline struct rde_peer *
prefix_peer(struct prefix *p)
{
	return (p->peer);
}

static inline struct rde_aspath *
prefix_aspath(struct prefix *p)
{
	return (p->aspath);
}

static inline struct rde_community *
prefix_communities(struct prefix *p)
{
	return (p->communities);
}

static inline struct nexthop *
prefix_nexthop(struct prefix *p)
{
	return (p->nexthop);
}

static inline uint8_t
prefix_nhflags(struct prefix *p)
{
	return (p->nhflags & NEXTHOP_MASK);
}

static inline int
prefix_nhvalid(struct prefix *p)
{
	return ((p->nhflags & NEXTHOP_VALID) != 0);
}

static inline uint8_t
prefix_vstate(struct prefix *p)
{
	return (p->validation_state & ROA_MASK);
}

static inline struct rib_entry *
prefix_re(struct prefix *p)
{
	if (p->flags & PREFIX_FLAG_ADJOUT)
		return NULL;
	return (p->entry.list.re);
}

void		 nexthop_init(uint32_t);
void		 nexthop_shutdown(void);
int		 nexthop_pending(void);
void		 nexthop_runner(void);
void		 nexthop_modify(struct nexthop *, enum action_types, uint8_t,
		    struct nexthop **, uint8_t *);
void		 nexthop_link(struct prefix *);
void		 nexthop_unlink(struct prefix *);
void		 nexthop_update(struct kroute_nexthop *);
struct nexthop	*nexthop_get(struct bgpd_addr *);
struct nexthop	*nexthop_ref(struct nexthop *);
int		 nexthop_unref(struct nexthop *);
int		 nexthop_compare(struct nexthop *, struct nexthop *);

/* rde_update.c */
void		 up_init(struct rde_peer *);
void		 up_generate_updates(struct filter_head *, struct rde_peer *,
		    struct prefix *, struct prefix *);
void		 up_generate_addpath(struct filter_head *, struct rde_peer *,
		    struct prefix *, struct prefix *);
void		 up_generate_default(struct filter_head *, struct rde_peer *,
		    uint8_t);
int		 up_is_eor(struct rde_peer *, uint8_t);
int		 up_dump_withdraws(u_char *, int, struct rde_peer *, uint8_t);
int		 up_dump_mp_unreach(u_char *, int, struct rde_peer *, uint8_t);
int		 up_dump_attrnlri(u_char *, int, struct rde_peer *);
int		 up_dump_mp_reach(u_char *, int, struct rde_peer *, uint8_t);

#endif /* __RDE_H__ */
