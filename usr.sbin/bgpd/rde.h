/*	$OpenBSD: rde.h,v 1.342 2026/02/13 12:47:36 claudio Exp $ */

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
#include "chash.h"
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
TAILQ_HEAD(rib_queue, rib_entry);
LIST_HEAD(rib_pq_head, rib_pq);

struct rib_entry {
	RB_ENTRY(rib_entry)	 rib_e;
	TAILQ_ENTRY(rib_entry)	 rib_queue;
	struct prefix_queue	 prefix_h;
	struct pt_entry		*prefix;
	struct rib_pq_head	 rib_pq_list;
	uint32_t		 pq_peer_id;
	uint16_t		 rib_id;
	uint8_t			 lock;
	uint8_t			 pq_mode;
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
RB_HEAD(peer_tree, rde_peer);

CH_HEAD(pend_prefix_hash, pend_prefix);
TAILQ_HEAD(pend_prefix_queue, pend_prefix);
CH_HEAD(pend_attr_hash, pend_prefix);
TAILQ_HEAD(pend_attr_queue, pend_attr);
struct rde_filter;

struct rde_peer {
	RB_ENTRY(rde_peer)		 entry;
	struct peer_config		 conf;
	struct rde_peer_stats		 stats;
	struct bgpd_addr		 remote_addr;
	struct bgpd_addr		 local_v4_addr;
	struct bgpd_addr		 local_v6_addr;
	struct capabilities		 capa;
	struct addpath_eval		 eval;
	struct pend_attr_queue		 updates[AID_MAX];
	struct pend_prefix_queue	 withdraws[AID_MAX];
	struct pend_attr_hash		 pend_attrs;
	struct pend_prefix_hash		 pend_prefixes;
	struct rde_filter		*out_rules;
	struct ibufqueue		*ibufq;
	struct rib_queue		 rib_pq_head;
	monotime_t			 staletime[AID_MAX];
	uint32_t			 adjout_bid;
	uint32_t			 remote_bgpid;
	uint32_t			 path_id_tx;
	unsigned int			 local_if_scope;
	enum peer_state			 state;
	enum export_type		 export_type;
	enum role			 role;
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

struct rde_aspa;
struct rde_aspa_state {
	uint8_t		onlyup;
	uint8_t		downup;
};

#define AS_SET			1
#define AS_SEQUENCE		2
#define AS_CONFED_SEQUENCE	3
#define AS_CONFED_SET		4
#define ASPATH_HEADER_SIZE	(offsetof(struct aspath, data))

struct aspath {
	uint32_t		source_as;	/* cached source_as */
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
	ATTR_PMSI_TUNNEL=22,
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

/* default attribute flags for well-known attributes */
#define ATTR_WELL_KNOWN		ATTR_TRANSITIVE

struct attr {
	uint64_t			 hash;
	u_char				*data;
	int				 refcnt;
	uint16_t			 len;
	uint8_t				 flags;
	uint8_t				 type;
};

struct rde_community {
	uint64_t			 hash;
	int				 size;
	int				 nentries;
	int				 flags;
	int				 refcnt;
	struct community		*communities;
};

#define	PARTIAL_COMMUNITIES		0x01
#define	PARTIAL_LARGE_COMMUNITIES	0x02
#define	PARTIAL_EXT_COMMUNITIES		0x04
#define	PARTIAL_DIRTY			0x08

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
#define	F_ATTR_OTC_LEAK		0x02000 /* otc leak, not eligible */
#define	F_ATTR_PARSE_ERR	0x10000 /* parse error, not eligible */
#define	F_ATTR_LINKED		0x20000 /* if set path is on various lists */

#define ORIGIN_IGP		0
#define ORIGIN_EGP		1
#define ORIGIN_INCOMPLETE	2

#define DEFAULT_LPREF		100

struct rde_aspath {
	uint64_t			 hash;
	struct attr			**others;
	struct aspath			*aspath;
	struct rde_aspa_state		 aspa_state;
	int				 refcnt;
	uint32_t			 flags;		/* internally used */
	uint32_t			 med;		/* multi exit disc */
	uint32_t			 lpref;		/* local pref */
	uint32_t			 weight;	/* low prio lpref */
	uint16_t			 rtlabelid;	/* route label id */
	uint16_t			 pftableid;	/* pf table id */
	uint8_t				 origin;
	uint8_t				 others_len;
	uint8_t				 aspa_generation;
};
#define PATH_HASHOFF		offsetof(struct rde_aspath, med)
#define PATH_HASHSTART(x)	((const uint8_t *)x + PATH_HASHOFF)
#define PATH_HASHSIZE		(sizeof(struct rde_aspath) - PATH_HASHOFF)

enum nexthop_state {
	NEXTHOP_LOOKUP,
	NEXTHOP_UNREACH,
	NEXTHOP_REACH,
	NEXTHOP_FLAPPED		/* only used by oldstate */
};

struct nexthop {
	RB_ENTRY(nexthop)	entry;
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

struct adjout_prefix;

/* generic entry without address specific part */
struct pt_entry {
	RB_ENTRY(pt_entry)		 pt_e;
	struct adjout_prefix		*adjout;
	uint32_t			 adjoutlen;
	uint32_t			 adjoutavail;
	uint8_t				 aid;
	uint8_t				 prefixlen;
	uint16_t			 len;
	uint32_t			 refcnt;
	uint8_t				 data[0]; /* data depending on aid */
};

struct prefix {
	TAILQ_ENTRY(prefix)	 rib_l;
	LIST_ENTRY(prefix)	 nexthop_l;
	struct rib_entry	*re;
	struct pt_entry		*pt;
	struct rde_aspath	*aspath;
	struct rde_community	*communities;
	struct rde_peer		*peer;
	struct nexthop		*nexthop;	/* may be NULL */
	monotime_t		 lastchange;
	uint32_t		 path_id;
	uint32_t		 path_id_tx;
	uint8_t			 flags;
	uint8_t			 validation_state;
	uint8_t			 nhflags;
	int8_t			 dmetric;	/* decision metric */
};

#define	PREFIX_NEXTHOP_LINKED	0x01	/* prefix is linked onto nexthop list */
#define	PREFIX_FLAG_FILTERED	0x04	/* prefix is filtered (ineligible) */

#define	PREFIX_DMETRIC_NONE	0
#define	PREFIX_DMETRIC_INVALID	1
#define	PREFIX_DMETRIC_VALID	2
#define	PREFIX_DMETRIC_AS_WIDE	3
#define	PREFIX_DMETRIC_ECMP	4
#define	PREFIX_DMETRIC_BEST	5

/* possible states for nhflags */
#define	NEXTHOP_SELF		0x01
#define	NEXTHOP_REJECT		0x02
#define	NEXTHOP_BLACKHOLE	0x04
#define	NEXTHOP_NOMODIFY	0x08
#define	NEXTHOP_MASK		0x0f
#define	NEXTHOP_VALID		0x80

struct adjout_attr {
	uint64_t		 hash;
	struct rde_aspath	*aspath;
	struct rde_community	*communities;
	struct nexthop		*nexthop;
	int			 refcnt;
};

struct adjout_prefix {
	uint32_t			 path_id_tx;
	struct adjout_attr		*attrs;
	struct bitmap			 peermap;
};

struct pend_attr {
	TAILQ_ENTRY(pend_attr)		 entry;
	struct pend_prefix_queue	 prefixes;
	struct adjout_attr		*attrs;
	uint8_t				 aid;
};

struct pend_prefix {
	TAILQ_ENTRY(pend_prefix)	 entry;
	struct pt_entry			*pt;
	struct pend_attr		*attrs;
	uint32_t			 path_id_tx;
};

struct filterstate {
	struct rde_aspath	 aspath;
	struct rde_community	 communities;
	struct nexthop		*nexthop;
	uint8_t			 nhflags;
	uint8_t			 vstate;
};

enum eval_mode {
	EVAL_NONE,
	EVAL_RECONF,
	EVAL_DEFAULT,
	EVAL_ALL,
};

struct rib_context {
	LIST_ENTRY(rib_context)		 entry;
	struct rib_entry		*ctx_re;
	struct pt_entry			*ctx_pt;
	uint32_t			 ctx_id;
	void		(*ctx_rib_call)(struct rib_entry *, void *);
	void		(*ctx_prefix_call)(struct rde_peer *,
			    struct pt_entry *, struct adjout_prefix *, void *);
	void		(*ctx_done)(void *, uint8_t);
	int		(*ctx_throttle)(void *);
	void				*ctx_arg;
	struct bgpd_addr		 ctx_subtree;
	unsigned int			 ctx_count;
	uint8_t				 ctx_aid;
	uint8_t				 ctx_subtreelen;
};

extern struct rde_memstats rdemem;

/* prototypes */
/* mrt.c */
int		mrt_dump_v2_hdr(struct mrt *, struct bgpd_config *);
void		mrt_dump_upcall(struct rib_entry *, void *);

/* rde.c */
void		 rde_update_err(struct rde_peer *, uint8_t , uint8_t,
		    struct ibuf *);
void		 rde_update_log(const char *, uint16_t,
		    const struct rde_peer *, const struct bgpd_addr *,
		    const struct bgpd_addr *, uint8_t);
void		rde_send_kroute_flush(struct rib *);
void		rde_send_kroute(struct rib *, struct prefix *, struct prefix *);
void		rde_send_nexthop(struct bgpd_addr *, int);
void		rde_pftable_add(uint16_t, struct prefix *);
void		rde_pftable_del(uint16_t, struct prefix *);

int		rde_evaluate_all(void);
uint32_t	rde_local_as(void);
int		rde_decisionflags(void);
void		rde_peer_send_rrefresh(struct rde_peer *, uint8_t, uint8_t);
int		rde_match_peer(struct rde_peer *, struct ctl_neighbor *);

/* rde_peer.c */
int		 peer_has_as4byte(struct rde_peer *);
int		 peer_has_add_path(struct rde_peer *, uint8_t, int);
int		 peer_has_ext_msg(struct rde_peer *);
int		 peer_has_ext_nexthop(struct rde_peer *, uint8_t);
int		 peer_permit_as_set(struct rde_peer *);
void		 peer_init(struct filter_head *);
void		 peer_shutdown(void);
void		 peer_foreach(void (*)(struct rde_peer *, void *), void *);
struct rde_peer	*peer_get(uint32_t);
struct rde_peer *peer_match(struct ctl_neighbor *, uint32_t);
struct rde_peer	*peer_add(uint32_t, struct peer_config *, struct filter_head *);
struct rde_filter	*peer_apply_out_filter(struct rde_peer *,
			    struct filter_head *);

void		 rde_generate_updates(struct rib_entry *, struct prefix *,
		    uint32_t, enum eval_mode);
void		 peer_process_updates(struct rde_peer *, void *);

void		 peer_up(struct rde_peer *, struct session_up *);
void		 peer_down(struct rde_peer *);
void		 peer_delete(struct rde_peer *);
void		 peer_flush(struct rde_peer *, uint8_t, monotime_t);
void		 peer_stale(struct rde_peer *, uint8_t, int);
void		 peer_blast(struct rde_peer *, uint8_t);
void		 peer_dump(struct rde_peer *, uint8_t);
void		 peer_begin_rrefresh(struct rde_peer *, uint8_t);
int		 peer_work_pending(void);

void		 peer_imsg_push(struct rde_peer *, struct imsg *);
int		 peer_imsg_pop(struct rde_peer *, struct imsg *);
void		 peer_imsg_flush(struct rde_peer *);

static inline int
peer_is_up(struct rde_peer *peer)
{
	return (peer->state == PEER_UP);
}

RB_PROTOTYPE(peer_tree, rde_peer, entry, peer_cmp);

/* rde_attr.c */
int		 attr_writebuf(struct ibuf *, uint8_t, uint8_t, void *,
		    uint16_t);
void		 attr_init(void);
int		 attr_optadd(struct rde_aspath *, uint8_t, uint8_t,
		    void *, uint16_t);
struct attr	*attr_optget(const struct rde_aspath *, uint8_t);
void		 attr_copy(struct rde_aspath *, const struct rde_aspath *);
int		 attr_equal(const struct rde_aspath *,
		    const struct rde_aspath *);
void		 attr_freeall(struct rde_aspath *);
void		 attr_free(struct rde_aspath *, struct attr *);

struct aspath	*aspath_get(void *, uint16_t);
struct aspath	*aspath_copy(struct aspath *);
void		 aspath_put(struct aspath *);
u_char		*aspath_deflate(u_char *, uint16_t *, int *);
void		 aspath_merge(struct rde_aspath *, struct attr *);
uint32_t	 aspath_neighbor(struct aspath *);
int		 aspath_loopfree(struct aspath *, uint32_t);
int		 aspath_compare(const struct aspath *, const struct aspath *);
int		 aspath_match(struct aspath *, struct filter_as *, uint32_t);
u_char		*aspath_prepend(struct aspath *, uint32_t, int, uint16_t *);
u_char		*aspath_override(struct aspath *, uint32_t, uint32_t,
		    uint16_t *);
int		 aspath_lenmatch(struct aspath *, enum aslen_spec, u_int);

static inline u_char *
aspath_dump(struct aspath *aspath)
{
	return (aspath->data);
}

static inline uint16_t
aspath_length(struct aspath *aspath)
{
	return (aspath->len);
}

static inline uint32_t
aspath_origin(struct aspath *aspath)
{
	return (aspath->source_as);
}

/* rde_community.c */
int	community_match(struct rde_community *, struct community *,
	    struct rde_peer *);
int	community_count(struct rde_community *, uint8_t type);
int	community_set(struct rde_community *, const struct community *,
	    struct rde_peer *);
void	community_delete(struct rde_community *, const struct community *,
	    struct rde_peer *);

int	community_add(struct rde_community *, int, struct ibuf *);
int	community_large_add(struct rde_community *, int, struct ibuf *);
int	community_ext_add(struct rde_community *, int, int, struct ibuf *);
int	community_writebuf(struct rde_community *, uint8_t, int, struct ibuf *);

void			 communities_init(void);
struct rde_community	*communities_lookup(struct rde_community *);
struct rde_community	*communities_link(struct rde_community *);
void			 communities_unlink(struct rde_community *);

int	 communities_equal(const struct rde_community *,
	    const struct rde_community *);
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
void	rde_apply_set(const struct rde_filter_set *, struct rde_peer *,
	    struct rde_peer *, struct filterstate *, u_int8_t);
int	rde_l3vpn_import(struct rde_community *, struct l3vpn *);
void	rde_filter_unref(struct rde_filter *);
struct rde_filter *rde_filter_new(size_t);
struct rde_filter *rde_filter_getcache(struct rde_filter *);
void	rde_filter_fill(struct rde_filter *, size_t,
	    const struct filter_rule *);
void	rde_filterstate_init(struct filterstate *);
void	rde_filterstate_prep(struct filterstate *, struct prefix *);
void	rde_filterstate_copy(struct filterstate *, struct filterstate *);
void	rde_filterstate_set_vstate(struct filterstate *, uint8_t, uint8_t);
void	rde_filterstate_clean(struct filterstate *);
uint64_t	rde_filterset_calc_hash(const struct rde_filter_set *);
int	rde_filter_skip_rule(struct rde_peer *, struct filter_rule *);
int	rde_filter_equal(struct filter_head *, struct filter_head *);
struct rde_filter_set	*rde_filterset_imsg_recv(struct imsg *);
void	rde_filter_calc_skip_steps(struct filter_head *);
enum filter_actions rde_filter(struct filter_head *, struct rde_peer *,
	    struct rde_peer *, struct bgpd_addr *, uint8_t,
	    struct filterstate *);
enum filter_actions rde_filter_out(struct rde_filter *, struct rde_peer *,
	    struct rde_peer *, struct bgpd_addr *, uint8_t,
	    struct filterstate *);

/* rde_prefix.c */
void	 pt_init(void);
void	 pt_shutdown(void);
void	 pt_getaddr(struct pt_entry *, struct bgpd_addr *);
int	 pt_getflowspec(struct pt_entry *, uint8_t **);
struct pt_entry	*pt_fill(struct bgpd_addr *, int);
struct pt_entry	*pt_get(struct bgpd_addr *, int);
struct pt_entry	*pt_get_next(struct bgpd_addr *, int);
struct pt_entry	*pt_add(struct bgpd_addr *, int);
struct pt_entry	*pt_get_flow(struct flowspec *);
struct pt_entry	*pt_add_flow(struct flowspec *);
struct pt_entry	*pt_first(uint8_t);
struct pt_entry	*pt_next(struct pt_entry *);
void	 pt_remove(struct pt_entry *);
struct pt_entry	*pt_lookup(struct bgpd_addr *);
int	 pt_prefix_cmp(const struct pt_entry *, const struct pt_entry *);
int	 pt_writebuf(struct ibuf *, struct pt_entry *, int, int, uint32_t);

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
struct rib_entry *rib_get(struct rib *, struct pt_entry *);
struct rib_entry *rib_get_addr(struct rib *, struct bgpd_addr *, int);
struct rib_entry *rib_match(struct rib *, struct bgpd_addr *);
int		 rib_dump_pending(void);
void		 rib_dump_runner(void);
void		 rib_dump_insert(struct rib_context *);
int		 rib_dump_new(uint16_t, uint8_t, unsigned int, void *,
		    void (*)(struct rib_entry *, void *),
		    void (*)(void *, uint8_t),
		    int (*)(void *));
int		 rib_dump_subtree(uint16_t, struct bgpd_addr *, uint8_t,
		    unsigned int count, void *arg,
		    void (*)(struct rib_entry *, void *),
		    void (*)(void *, uint8_t),
		    int (*)(void *));
void		 rib_dump_terminate(void *);
void		 rib_dequeue(struct rib_entry *);

extern struct rib flowrib;

static inline struct rib *
re_rib(struct rib_entry *re)
{
	if (re->prefix->aid == AID_FLOWSPECv4 ||
	    re->prefix->aid == AID_FLOWSPECv6)
		return &flowrib;
	return rib_byid(re->rib_id);
}

void		 path_init(void);
struct rde_aspath *path_ref(struct rde_aspath *);
void		 path_unref(struct rde_aspath *);
int		 path_equal(const struct rde_aspath *,
		    const struct rde_aspath *);
struct rde_aspath *path_getcache(struct rde_aspath *);
struct rde_aspath *path_copy(struct rde_aspath *, const struct rde_aspath *);
struct rde_aspath *path_prep(struct rde_aspath *);
struct rde_aspath *path_get(void);
void		 path_clean(struct rde_aspath *);
void		 path_put(struct rde_aspath *);

#define	PREFIX_SIZE(x)	(((x) + 7) / 8 + 1)
struct prefix	*prefix_get(struct rib *, struct rde_peer *, uint32_t,
		    struct bgpd_addr *, int);
int		 prefix_update(struct rib *, struct rde_peer *, uint32_t,
		    uint32_t, struct filterstate *, int, struct bgpd_addr *,
		    int);
int		 prefix_withdraw(struct rib *, struct rde_peer *, uint32_t,
		    struct bgpd_addr *, int);
int		 prefix_flowspec_update(struct rde_peer *, struct filterstate *,
		    struct pt_entry *, uint32_t);
int		 prefix_flowspec_withdraw(struct rde_peer *, struct pt_entry *);
void		 prefix_flowspec_dump(uint8_t, void *,
		    void (*)(struct rib_entry *, void *),
		    void (*)(void *, uint8_t));

struct prefix	*prefix_bypeer(struct rib_entry *, struct rde_peer *,
		    uint32_t);
void		 prefix_destroy(struct prefix *);

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
prefix_roa_vstate(struct prefix *p)
{
	return (p->validation_state & ROA_MASK);
}

static inline uint8_t
prefix_aspa_vstate(struct prefix *p)
{
	return (p->validation_state >> 4);
}

static inline void
prefix_set_vstate(struct prefix *p, uint8_t roa_vstate, uint8_t aspa_vstate)
{
	p->validation_state = roa_vstate & ROA_MASK;
	p->validation_state |= aspa_vstate << 4;
}

static inline struct rib_entry *
prefix_re(struct prefix *p)
{
	return (p->re);
}

static inline int
prefix_filtered(struct prefix *p)
{
	return ((p->flags & PREFIX_FLAG_FILTERED) != 0);
}

void		 nexthop_shutdown(void);
int		 nexthop_pending(void);
void		 nexthop_runner(void);
void		 nexthop_modify(struct nexthop *, enum action_types, uint8_t,
		    struct nexthop **, uint8_t *);
void		 nexthop_link(struct prefix *);
void		 nexthop_unlink(struct prefix *);
void		 nexthop_update(struct kroute_nexthop *);
struct nexthop	*nexthop_get(const struct bgpd_addr *);
struct nexthop	*nexthop_ref(struct nexthop *);
int		 nexthop_unref(struct nexthop *);

/* rde_adjout.c */
void			 adjout_init(void);
struct adjout_prefix	*adjout_prefix_get(struct rde_peer *, uint32_t,
			    struct pt_entry *);
struct adjout_prefix	*adjout_prefix_first(struct rde_peer *,
			    struct pt_entry *);
struct adjout_prefix	*adjout_prefix_next(struct rde_peer *,
			    struct pt_entry *, struct adjout_prefix *);

void		 prefix_add_eor(struct rde_peer *, uint8_t);
void		 adjout_prefix_update(struct adjout_prefix *, struct rde_peer *,
		    struct filterstate *, struct pt_entry *, uint32_t);
void		 adjout_prefix_withdraw(struct rde_peer *, struct pt_entry *,
		    struct adjout_prefix *);
void		 adjout_prefix_reaper(struct rde_peer *);
void		 adjout_prefix_dump_cleanup(struct rib_context *);
void		 adjout_prefix_dump_r(struct rib_context *);
int		 adjout_prefix_dump_new(struct rde_peer *, uint8_t,
		    unsigned int, void *,
		    void (*)(struct rde_peer *, struct pt_entry *,
			struct adjout_prefix *, void *),
		    void (*)(void *, uint8_t), int (*)(void *));
int		 adjout_prefix_dump_subtree(struct rde_peer *,
		    struct bgpd_addr *, uint8_t, unsigned int, void *,
		    void (*)(struct rde_peer *, struct pt_entry *,
			struct adjout_prefix *, void *),
		    void (*)(void *, uint8_t), int (*)(void *));
void		 adjout_peer_init(struct rde_peer *);
void		 adjout_peer_flush_pending(struct rde_peer *);
void		 adjout_peer_free(struct rde_peer *);

void		 pend_attr_done(struct pend_attr *, struct rde_peer *);
void		 pend_eor_add(struct rde_peer *, uint8_t);
void		 pend_prefix_add(struct rde_peer *, struct adjout_attr *,
		    struct pt_entry *, uint32_t);
void		 pend_prefix_free(struct pend_prefix *,
		    struct pend_prefix_queue *, struct rde_peer *);

/* rde_update.c */
void		 up_generate_updates(struct rde_peer *, struct rib_entry *);
void		 up_generate_addpath(struct rde_peer *, struct rib_entry *);
void		 up_generate_addpath_all(struct rde_peer *, struct rib_entry *,
		    struct prefix *, uint32_t);
void		 up_generate_default(struct rde_peer *, uint8_t);
int		 up_is_eor(struct rde_peer *, uint8_t);
void		 up_dump_withdraws(struct imsgbuf *, struct rde_peer *,
		    uint8_t);
void		 up_dump_update(struct imsgbuf *, struct rde_peer *, uint8_t);

/* rde_aspa.c */
void		 aspa_validation(struct rde_aspa *, struct aspath *,
		    struct rde_aspa_state *);
struct rde_aspa	*aspa_table_prep(uint32_t, size_t);
void		 aspa_add_set(struct rde_aspa *, uint32_t, const uint32_t *,
		    uint32_t);
void		 aspa_table_free(struct rde_aspa *);
void		 aspa_table_stats(const struct rde_aspa *,
		    struct ctl_show_set *);
int		 aspa_table_equal(const struct rde_aspa *,
		    const struct rde_aspa *);
void		 aspa_table_unchanged(struct rde_aspa *,
		    const struct rde_aspa *);

#endif /* __RDE_H__ */
