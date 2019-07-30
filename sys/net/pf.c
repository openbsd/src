/*	$OpenBSD: pf.c,v 1.1063 2018/03/06 17:35:53 bluhm Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2013 Henning Brauer <henning@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include "bpfilter.h"
#include "carp.h"
#include "pflog.h"
#include "pfsync.h"
#include "pflow.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>

#include <crypto/sha2.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_divert.h>

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_divert.h>
#endif /* INET6 */

#include <net/pfvar.h>
#include <net/pfvar_priv.h>

#if NPFLOG > 0
#include <net/if_pflog.h>
#endif	/* NPFLOG > 0 */

#if NPFLOW > 0
#include <net/if_pflow.h>
#endif	/* NPFLOW > 0 */

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#endif

/*
 * Global variables
 */
struct pf_state_tree	 pf_statetbl;
struct pf_queuehead	 pf_queues[2];
struct pf_queuehead	*pf_queues_active;
struct pf_queuehead	*pf_queues_inactive;

struct pf_status	 pf_status;

int			 pf_hdr_limit = 20;  /* arbitrary limit, tune in ddb */

SHA2_CTX		 pf_tcp_secret_ctx;
u_char			 pf_tcp_secret[16];
int			 pf_tcp_secret_init;
int			 pf_tcp_iss_off;

int		 pf_npurge;
struct task	 pf_purge_task = TASK_INITIALIZER(pf_purge, &pf_npurge);
struct timeout	 pf_purge_to = TIMEOUT_INITIALIZER(pf_purge_timeout, NULL);

enum pf_test_status {
	PF_TEST_FAIL = -1,
	PF_TEST_OK,
	PF_TEST_QUICK
};

struct pf_test_ctx {
	enum pf_test_status	  test_status;
	struct pf_pdesc		 *pd;
	struct pf_rule_actions	  act;
	u_int8_t		  icmpcode;
	u_int8_t		  icmptype;
	int			  icmp_dir;
	int			  state_icmp;
	int			  tag;
	u_short			  reason;
	struct pf_rule_item	 *ri;
	struct pf_src_node	 *sns[PF_SN_MAX];
	struct pf_rule_slist	  rules;
	struct pf_rule		 *nr;
	struct pf_rule		**rm;
	struct pf_rule		 *a;
	struct pf_rule		**am;
	struct pf_ruleset	**rsm;
	struct pf_ruleset	 *arsm;
	struct pf_ruleset	 *aruleset;
	struct tcphdr		 *th;
	int			  depth;
};

#define	PF_ANCHOR_STACK_MAX	64

struct pool		 pf_src_tree_pl, pf_rule_pl, pf_queue_pl;
struct pool		 pf_state_pl, pf_state_key_pl, pf_state_item_pl;
struct pool		 pf_rule_item_pl, pf_sn_item_pl;

void			 pf_add_threshold(struct pf_threshold *);
int			 pf_check_threshold(struct pf_threshold *);
int			 pf_check_tcp_cksum(struct mbuf *, int, int,
			    sa_family_t);
static __inline void	 pf_cksum_fixup(u_int16_t *, u_int16_t, u_int16_t,
			    u_int8_t);
void			 pf_cksum_fixup_a(u_int16_t *, const struct pf_addr *,
			    const struct pf_addr *, sa_family_t, u_int8_t);
int			 pf_modulate_sack(struct pf_pdesc *,
			    struct pf_state_peer *);
int			 pf_icmp_mapping(struct pf_pdesc *, u_int8_t, int *,
			    u_int16_t *, u_int16_t *);
int			 pf_change_icmp_af(struct mbuf *, int,
			    struct pf_pdesc *, struct pf_pdesc *,
			    struct pf_addr *, struct pf_addr *, sa_family_t,
			    sa_family_t);
int			 pf_translate_a(struct pf_pdesc *, struct pf_addr *,
			    struct pf_addr *);
void			 pf_translate_icmp(struct pf_pdesc *, struct pf_addr *,
			    u_int16_t *, struct pf_addr *, struct pf_addr *,
			    u_int16_t);
int			 pf_translate_icmp_af(struct pf_pdesc*, int, void *);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t, int,
			    sa_family_t, struct pf_rule *, u_int);
void			 pf_detach_state(struct pf_state *);
void			 pf_state_key_detach(struct pf_state *, int);
u_int32_t		 pf_tcp_iss(struct pf_pdesc *);
void			 pf_rule_to_actions(struct pf_rule *,
			    struct pf_rule_actions *);
int			 pf_test_rule(struct pf_pdesc *, struct pf_rule **,
			    struct pf_state **, struct pf_rule **,
			    struct pf_ruleset **, u_short *);
static __inline int	 pf_create_state(struct pf_pdesc *, struct pf_rule *,
			    struct pf_rule *, struct pf_rule *,
			    struct pf_state_key **, struct pf_state_key **,
			    int *, struct pf_state **, int,
			    struct pf_rule_slist *, struct pf_rule_actions *,
			    struct pf_src_node *[]);
static __inline int	 pf_state_key_addr_setup(struct pf_pdesc *, void *,
			    int, struct pf_addr *, int, struct pf_addr *,
			    int, int);
int			 pf_state_key_setup(struct pf_pdesc *, struct
			    pf_state_key **, struct pf_state_key **, int);
int			 pf_tcp_track_full(struct pf_pdesc *,
			    struct pf_state **, u_short *, int *, int);
int			 pf_tcp_track_sloppy(struct pf_pdesc *,
			    struct pf_state **, u_short *);
static __inline int	 pf_synproxy(struct pf_pdesc *, struct pf_state **,
			    u_short *);
int			 pf_test_state(struct pf_pdesc *, struct pf_state **,
			    u_short *, int);
int			 pf_icmp_state_lookup(struct pf_pdesc *,
			    struct pf_state_key_cmp *, struct pf_state **,
			    u_int16_t, u_int16_t, int, int *, int, int);
int			 pf_test_state_icmp(struct pf_pdesc *,
			    struct pf_state **, u_short *);
u_int16_t		 pf_calc_mss(struct pf_addr *, sa_family_t, int,
			    u_int16_t);
static __inline int	 pf_set_rt_ifp(struct pf_state *, struct pf_addr *,
			    sa_family_t);
struct pf_divert	*pf_get_divert(struct mbuf *);
int			 pf_walk_header(struct pf_pdesc *, struct ip *,
			    u_short *);
int			 pf_walk_option6(struct pf_pdesc *, struct ip6_hdr *,
			    int, int, u_short *);
int			 pf_walk_header6(struct pf_pdesc *, struct ip6_hdr *,
			    u_short *);
void			 pf_print_state_parts(struct pf_state *,
			    struct pf_state_key *, struct pf_state_key *);
int			 pf_addr_wrap_neq(struct pf_addr_wrap *,
			    struct pf_addr_wrap *);
int			 pf_compare_state_keys(struct pf_state_key *,
			    struct pf_state_key *, struct pfi_kif *, u_int);
struct pf_state		*pf_find_state(struct pfi_kif *,
			    struct pf_state_key_cmp *, u_int, struct mbuf *);
int			 pf_src_connlimit(struct pf_state **);
int			 pf_match_rcvif(struct mbuf *, struct pf_rule *);
int			 pf_step_into_anchor(struct pf_test_ctx *,
			    struct pf_rule *);
int			 pf_match_rule(struct pf_test_ctx *,
			    struct pf_ruleset *);
void			 pf_counters_inc(int, struct pf_pdesc *,
			    struct pf_state *, struct pf_rule *,
			    struct pf_rule *);

int			 pf_state_key_isvalid(struct pf_state_key *);
struct pf_state_key	*pf_state_key_ref(struct pf_state_key *);
void			 pf_state_key_unref(struct pf_state_key *);
void			 pf_state_key_link_reverse(struct pf_state_key *,
			    struct pf_state_key *);
void			 pf_state_key_unlink_reverse(struct pf_state_key *);
void			 pf_state_key_link_inpcb(struct pf_state_key *,
			    struct inpcb *);
void			 pf_state_key_unlink_inpcb(struct pf_state_key *);
void			 pf_inpcb_unlink_state_key(struct inpcb *);

#if NPFLOG > 0
void			 pf_log_matches(struct pf_pdesc *, struct pf_rule *,
			    struct pf_rule *, struct pf_ruleset *,
			    struct pf_rule_slist *);
#endif	/* NPFLOG > 0 */

extern struct pool pfr_ktable_pl;
extern struct pool pfr_kentry_pl;

struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX] = {
	{ &pf_state_pl, PFSTATE_HIWAT, PFSTATE_HIWAT },
	{ &pf_src_tree_pl, PFSNODE_HIWAT, PFSNODE_HIWAT },
	{ &pf_frent_pl, PFFRAG_FRENT_HIWAT, PFFRAG_FRENT_HIWAT },
	{ &pfr_ktable_pl, PFR_KTABLE_HIWAT, PFR_KTABLE_HIWAT },
	{ &pfr_kentry_pl, PFR_KENTRY_HIWAT, PFR_KENTRY_HIWAT }
};

#define STATE_LOOKUP(i, k, d, s, m)					\
	do {								\
		s = pf_find_state(i, k, d, m);				\
		if (s == NULL || (s)->timeout == PFTM_PURGE)		\
			return (PF_DROP);				\
		if ((s)->rule.ptr->pktrate.limit && d == (s)->direction) { \
			pf_add_threshold(&(s)->rule.ptr->pktrate);	\
			if (pf_check_threshold(&(s)->rule.ptr->pktrate)) { \
				s = NULL;				\
				return (PF_DROP);			\
			}						\
		}							\
		if (d == PF_OUT &&					\
		    (((s)->rule.ptr->rt == PF_ROUTETO &&		\
		    (s)->rule.ptr->direction == PF_OUT) ||		\
		    ((s)->rule.ptr->rt == PF_REPLYTO &&			\
		    (s)->rule.ptr->direction == PF_IN)) &&		\
		    (s)->rt_kif != NULL &&				\
		    (s)->rt_kif != i)					\
			return (PF_PASS);				\
	} while (0)

#define BOUND_IFACE(r, k) \
	((r)->rule_flag & PFRULE_IFBOUND) ? (k) : pfi_all

#define STATE_INC_COUNTERS(s)					\
	do {							\
		struct pf_rule_item *mrm;			\
		s->rule.ptr->states_cur++;			\
		s->rule.ptr->states_tot++;			\
		if (s->anchor.ptr != NULL) {			\
			s->anchor.ptr->states_cur++;		\
			s->anchor.ptr->states_tot++;		\
		}						\
		SLIST_FOREACH(mrm, &s->match_rules, entry)	\
			mrm->r->states_cur++;			\
	} while (0)

static __inline int pf_src_compare(struct pf_src_node *, struct pf_src_node *);
static __inline int pf_state_compare_key(struct pf_state_key *,
	struct pf_state_key *);
static __inline int pf_state_compare_id(struct pf_state *,
	struct pf_state *);
#ifdef INET6
static __inline void pf_cksum_uncover(u_int16_t *, u_int16_t, u_int8_t);
static __inline void pf_cksum_cover(u_int16_t *, u_int16_t, u_int8_t);
#endif /* INET6 */
static __inline void pf_set_protostate(struct pf_state *, int, u_int8_t);

struct pf_src_tree tree_src_tracking;

struct pf_state_tree_id tree_id;
struct pf_state_queue state_list;

RB_GENERATE(pf_src_tree, pf_src_node, entry, pf_src_compare);
RB_GENERATE(pf_state_tree, pf_state_key, entry, pf_state_compare_key);
RB_GENERATE(pf_state_tree_id, pf_state,
    entry_id, pf_state_compare_id);

SLIST_HEAD(pf_rule_gcl, pf_rule)	pf_rule_gcl =
	SLIST_HEAD_INITIALIZER(pf_rule_gcl);

__inline int
pf_addr_compare(struct pf_addr *a, struct pf_addr *b, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		if (a->addr32[0] > b->addr32[0])
			return (1);
		if (a->addr32[0] < b->addr32[0])
			return (-1);
		break;
#ifdef INET6
	case AF_INET6:
		if (a->addr32[3] > b->addr32[3])
			return (1);
		if (a->addr32[3] < b->addr32[3])
			return (-1);
		if (a->addr32[2] > b->addr32[2])
			return (1);
		if (a->addr32[2] < b->addr32[2])
			return (-1);
		if (a->addr32[1] > b->addr32[1])
			return (1);
		if (a->addr32[1] < b->addr32[1])
			return (-1);
		if (a->addr32[0] > b->addr32[0])
			return (1);
		if (a->addr32[0] < b->addr32[0])
			return (-1);
		break;
#endif /* INET6 */
	}
	return (0);
}

static __inline int
pf_src_compare(struct pf_src_node *a, struct pf_src_node *b)
{
	int	diff;

	if (a->rule.ptr > b->rule.ptr)
		return (1);
	if (a->rule.ptr < b->rule.ptr)
		return (-1);
	if ((diff = a->type - b->type) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	if ((diff = pf_addr_compare(&a->addr, &b->addr, a->af)) != 0)
		return (diff);
	return (0);
}

static __inline void
pf_set_protostate(struct pf_state *s, int which, u_int8_t newstate)
{
	if (which == PF_PEER_DST || which == PF_PEER_BOTH)
		s->dst.state = newstate;
	if (which == PF_PEER_DST)
		return;

	if (s->src.state == newstate)
		return;
	if (s->key[PF_SK_STACK]->proto == IPPROTO_TCP &&
	    !(TCPS_HAVEESTABLISHED(s->src.state) ||
	    s->src.state == TCPS_CLOSED) &&
	    (TCPS_HAVEESTABLISHED(newstate) || newstate == TCPS_CLOSED))
		pf_status.states_halfopen--;

	s->src.state = newstate;
}

void
pf_addrcpy(struct pf_addr *dst, struct pf_addr *src, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		dst->addr32[0] = src->addr32[0];
		break;
#ifdef INET6
	case AF_INET6:
		dst->addr32[0] = src->addr32[0];
		dst->addr32[1] = src->addr32[1];
		dst->addr32[2] = src->addr32[2];
		dst->addr32[3] = src->addr32[3];
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}
}

void
pf_init_threshold(struct pf_threshold *threshold,
    u_int32_t limit, u_int32_t seconds)
{
	threshold->limit = limit * PF_THRESHOLD_MULT;
	threshold->seconds = seconds;
	threshold->count = 0;
	threshold->last = time_uptime;
}

void
pf_add_threshold(struct pf_threshold *threshold)
{
	u_int32_t t = time_uptime, diff = t - threshold->last;

	if (diff >= threshold->seconds)
		threshold->count = 0;
	else
		threshold->count -= threshold->count * diff /
		    threshold->seconds;
	threshold->count += PF_THRESHOLD_MULT;
	threshold->last = t;
}

int
pf_check_threshold(struct pf_threshold *threshold)
{
	return (threshold->count > threshold->limit);
}

int
pf_src_connlimit(struct pf_state **state)
{
	int			 bad = 0;
	struct pf_src_node	*sn;

	if ((sn = pf_get_src_node((*state), PF_SN_NONE)) == NULL)
		return (0);

	sn->conn++;
	(*state)->src.tcp_est = 1;
	pf_add_threshold(&sn->conn_rate);

	if ((*state)->rule.ptr->max_src_conn &&
	    (*state)->rule.ptr->max_src_conn < sn->conn) {
		pf_status.lcounters[LCNT_SRCCONN]++;
		bad++;
	}

	if ((*state)->rule.ptr->max_src_conn_rate.limit &&
	    pf_check_threshold(&sn->conn_rate)) {
		pf_status.lcounters[LCNT_SRCCONNRATE]++;
		bad++;
	}

	if (!bad)
		return (0);

	if ((*state)->rule.ptr->overload_tbl) {
		struct pfr_addr p;
		u_int32_t	killed = 0;

		pf_status.lcounters[LCNT_OVERLOAD_TABLE]++;
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE,
			    "pf: pf_src_connlimit: blocking address ");
			pf_print_host(&sn->addr, 0,
			    (*state)->key[PF_SK_WIRE]->af);
		}

		memset(&p, 0, sizeof(p));
		p.pfra_af = (*state)->key[PF_SK_WIRE]->af;
		switch ((*state)->key[PF_SK_WIRE]->af) {
		case AF_INET:
			p.pfra_net = 32;
			p.pfra_ip4addr = sn->addr.v4;
			break;
#ifdef INET6
		case AF_INET6:
			p.pfra_net = 128;
			p.pfra_ip6addr = sn->addr.v6;
			break;
#endif /* INET6 */
		}

		pfr_insert_kentry((*state)->rule.ptr->overload_tbl,
		    &p, time_second);

		/* kill existing states if that's required. */
		if ((*state)->rule.ptr->flush) {
			struct pf_state_key *sk;
			struct pf_state *st;

			pf_status.lcounters[LCNT_OVERLOAD_FLUSH]++;
			RB_FOREACH(st, pf_state_tree_id, &tree_id) {
				sk = st->key[PF_SK_WIRE];
				/*
				 * Kill states from this source.  (Only those
				 * from the same rule if PF_FLUSH_GLOBAL is not
				 * set)
				 */
				if (sk->af ==
				    (*state)->key[PF_SK_WIRE]->af &&
				    (((*state)->direction == PF_OUT &&
				    PF_AEQ(&sn->addr, &sk->addr[1], sk->af)) ||
				    ((*state)->direction == PF_IN &&
				    PF_AEQ(&sn->addr, &sk->addr[0], sk->af))) &&
				    ((*state)->rule.ptr->flush &
				    PF_FLUSH_GLOBAL ||
				    (*state)->rule.ptr == st->rule.ptr)) {
					st->timeout = PFTM_PURGE;
					pf_set_protostate(st, PF_PEER_BOTH,
					    TCPS_CLOSED);
					killed++;
				}
			}
			if (pf_status.debug >= LOG_NOTICE)
				addlog(", %u states killed", killed);
		}
		if (pf_status.debug >= LOG_NOTICE)
			addlog("\n");
	}

	/* kill this state */
	(*state)->timeout = PFTM_PURGE;
	pf_set_protostate(*state, PF_PEER_BOTH, TCPS_CLOSED);
	return (1);
}

int
pf_insert_src_node(struct pf_src_node **sn, struct pf_rule *rule,
    enum pf_sn_types type, sa_family_t af, struct pf_addr *src,
    struct pf_addr *raddr)
{
	struct pf_src_node	k;

	if (*sn == NULL) {
		k.af = af;
		k.type = type;
		PF_ACPY(&k.addr, src, af);
		k.rule.ptr = rule;
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
	}
	if (*sn == NULL) {
		if (!rule->max_src_nodes ||
		    rule->src_nodes < rule->max_src_nodes)
			(*sn) = pool_get(&pf_src_tree_pl, PR_NOWAIT | PR_ZERO);
		else
			pf_status.lcounters[LCNT_SRCNODES]++;
		if ((*sn) == NULL)
			return (-1);

		pf_init_threshold(&(*sn)->conn_rate,
		    rule->max_src_conn_rate.limit,
		    rule->max_src_conn_rate.seconds);

		(*sn)->type = type;
		(*sn)->af = af;
		(*sn)->rule.ptr = rule;
		PF_ACPY(&(*sn)->addr, src, af);
		if (raddr)
			PF_ACPY(&(*sn)->raddr, raddr, af);
		if (RB_INSERT(pf_src_tree,
		    &tree_src_tracking, *sn) != NULL) {
			if (pf_status.debug >= LOG_NOTICE) {
				log(LOG_NOTICE,
				    "pf: src_tree insert failed: ");
				pf_print_host(&(*sn)->addr, 0, af);
				addlog("\n");
			}
			pool_put(&pf_src_tree_pl, *sn);
			return (-1);
		}
		(*sn)->creation = time_uptime;
		(*sn)->rule.ptr->src_nodes++;
		pf_status.scounters[SCNT_SRC_NODE_INSERT]++;
		pf_status.src_nodes++;
	} else {
		if (rule->max_src_states &&
		    (*sn)->states >= rule->max_src_states) {
			pf_status.lcounters[LCNT_SRCSTATES]++;
			return (-1);
		}
	}
	return (0);
}

void
pf_remove_src_node(struct pf_src_node *sn)
{
	if (sn->states > 0 || sn->expire > time_uptime)
		return;

	sn->rule.ptr->src_nodes--;
	if (sn->rule.ptr->states_cur == 0 &&
	    sn->rule.ptr->src_nodes == 0)
		pf_rm_rule(NULL, sn->rule.ptr);
	RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
	pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
	pf_status.src_nodes--;
	pool_put(&pf_src_tree_pl, sn);
}

struct pf_src_node *
pf_get_src_node(struct pf_state *s, enum pf_sn_types type)
{
	struct pf_sn_item	*sni;

	SLIST_FOREACH(sni, &s->src_nodes, next)
		if (sni->sn->type == type)
			return (sni->sn);
	return (NULL);
}

void
pf_state_rm_src_node(struct pf_state *s, struct pf_src_node *sn)
{
	struct pf_sn_item	*sni, *snin, *snip = NULL;

	for (sni = SLIST_FIRST(&s->src_nodes); sni; sni = snin) {
		snin = SLIST_NEXT(sni, next);
		if (sni->sn == sn) {
			if (snip)
				SLIST_REMOVE_AFTER(snip, next);
			else
				SLIST_REMOVE_HEAD(&s->src_nodes, next);
			pool_put(&pf_sn_item_pl, sni);
			sni = NULL;
			sn->states--;
		}
		if (sni != NULL)
			snip = sni;
	}
}

/* state table stuff */

static __inline int
pf_state_compare_key(struct pf_state_key *a, struct pf_state_key *b)
{
	int	diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	if ((diff = pf_addr_compare(&a->addr[0], &b->addr[0], a->af)) != 0)
		return (diff);
	if ((diff = pf_addr_compare(&a->addr[1], &b->addr[1], a->af)) != 0)
		return (diff);
	if ((diff = a->port[0] - b->port[0]) != 0)
		return (diff);
	if ((diff = a->port[1] - b->port[1]) != 0)
		return (diff);
	if ((diff = a->rdomain - b->rdomain) != 0)
		return (diff);
	return (0);
}

static __inline int
pf_state_compare_id(struct pf_state *a, struct pf_state *b)
{
	if (a->id > b->id)
		return (1);
	if (a->id < b->id)
		return (-1);
	if (a->creatorid > b->creatorid)
		return (1);
	if (a->creatorid < b->creatorid)
		return (-1);

	return (0);
}

int
pf_state_key_attach(struct pf_state_key *sk, struct pf_state *s, int idx)
{
	struct pf_state_item	*si;
	struct pf_state_key     *cur;
	struct pf_state		*olds = NULL;

	KASSERT(s->key[idx] == NULL);
	if ((cur = RB_INSERT(pf_state_tree, &pf_statetbl, sk)) != NULL) {
		/* key exists. check for same kif, if none, add to key */
		TAILQ_FOREACH(si, &cur->states, entry)
			if (si->s->kif == s->kif &&
			    ((si->s->key[PF_SK_WIRE]->af == sk->af &&
			     si->s->direction == s->direction) ||
			    (si->s->key[PF_SK_WIRE]->af !=
			     si->s->key[PF_SK_STACK]->af &&
			     sk->af == si->s->key[PF_SK_STACK]->af &&
			     si->s->direction != s->direction))) {
				int reuse = 0;

				if (sk->proto == IPPROTO_TCP &&
				    si->s->src.state >= TCPS_FIN_WAIT_2 &&
				    si->s->dst.state >= TCPS_FIN_WAIT_2)
					reuse = 1;
				if (pf_status.debug >= LOG_NOTICE) {
					log(LOG_NOTICE,
					    "pf: %s key attach %s on %s: ",
					    (idx == PF_SK_WIRE) ?
					    "wire" : "stack",
					    reuse ? "reuse" : "failed",
					    s->kif->pfik_name);
					pf_print_state_parts(s,
					    (idx == PF_SK_WIRE) ?  sk : NULL,
					    (idx == PF_SK_STACK) ?  sk : NULL);
					addlog(", existing: ");
					pf_print_state_parts(si->s,
					    (idx == PF_SK_WIRE) ?  sk : NULL,
					    (idx == PF_SK_STACK) ?  sk : NULL);
					addlog("\n");
				}
				if (reuse) {
					pf_set_protostate(si->s, PF_PEER_BOTH,
					    TCPS_CLOSED);
					/* remove late or sks can go away */
					olds = si->s;
				} else {
					pool_put(&pf_state_key_pl, sk);
					return (-1);	/* collision! */
				}
			}
		pool_put(&pf_state_key_pl, sk);
		s->key[idx] = cur;
	} else
		s->key[idx] = sk;

	if ((si = pool_get(&pf_state_item_pl, PR_NOWAIT)) == NULL) {
		pf_state_key_detach(s, idx);
		return (-1);
	}
	si->s = s;

	/* list is sorted, if-bound states before floating */
	if (s->kif == pfi_all)
		TAILQ_INSERT_TAIL(&s->key[idx]->states, si, entry);
	else
		TAILQ_INSERT_HEAD(&s->key[idx]->states, si, entry);

	if (olds)
		pf_remove_state(olds);

	return (0);
}

void
pf_detach_state(struct pf_state *s)
{
	if (s->key[PF_SK_WIRE] == s->key[PF_SK_STACK])
		s->key[PF_SK_WIRE] = NULL;

	if (s->key[PF_SK_STACK] != NULL)
		pf_state_key_detach(s, PF_SK_STACK);

	if (s->key[PF_SK_WIRE] != NULL)
		pf_state_key_detach(s, PF_SK_WIRE);
}

void
pf_state_key_detach(struct pf_state *s, int idx)
{
	struct pf_state_item	*si;
	struct pf_state_key	*sk;

	if (s->key[idx] == NULL)
		return;

	si = TAILQ_FIRST(&s->key[idx]->states);
	while (si && si->s != s)
	    si = TAILQ_NEXT(si, entry);

	if (si) {
		TAILQ_REMOVE(&s->key[idx]->states, si, entry);
		pool_put(&pf_state_item_pl, si);
	}

	sk = s->key[idx];
	s->key[idx] = NULL;
	if (TAILQ_EMPTY(&sk->states)) {
		RB_REMOVE(pf_state_tree, &pf_statetbl, sk);
		sk->removed = 1;
		pf_state_key_unlink_reverse(sk);
		pf_state_key_unlink_inpcb(sk);
		pf_state_key_unref(sk);
	}
}

struct pf_state_key *
pf_alloc_state_key(int pool_flags)
{
	struct pf_state_key	*sk;

	if ((sk = pool_get(&pf_state_key_pl, pool_flags)) == NULL)
		return (NULL);
	TAILQ_INIT(&sk->states);

	return (sk);
}

static __inline int
pf_state_key_addr_setup(struct pf_pdesc *pd, void *arg, int sidx,
    struct pf_addr *saddr, int didx, struct pf_addr *daddr, int af, int multi)
{
	struct pf_state_key_cmp *key = arg;
#ifdef INET6
	struct pf_addr *target;

	if (af == AF_INET || pd->proto != IPPROTO_ICMPV6)
		goto copy;

	switch (pd->hdr.icmp6.icmp6_type) {
	case ND_NEIGHBOR_SOLICIT:
		if (multi)
			return (-1);
		target = (struct pf_addr *)&pd->hdr.nd_ns.nd_ns_target;
		daddr = target;
		break;
	case ND_NEIGHBOR_ADVERT:
		if (multi)
			return (-1);
		target = (struct pf_addr *)&pd->hdr.nd_ns.nd_ns_target;
		saddr = target;
		if (IN6_IS_ADDR_MULTICAST(&pd->dst->v6)) {
			key->addr[didx].addr32[0] = 0;
			key->addr[didx].addr32[1] = 0;
			key->addr[didx].addr32[2] = 0;
			key->addr[didx].addr32[3] = 0;
			daddr = NULL; /* overwritten */
		}
		break;
	default:
		if (multi) {
			key->addr[sidx].addr32[0] = __IPV6_ADDR_INT32_MLL;
			key->addr[sidx].addr32[1] = 0;
			key->addr[sidx].addr32[2] = 0;
			key->addr[sidx].addr32[3] = __IPV6_ADDR_INT32_ONE;
			saddr = NULL; /* overwritten */
		}
	}
 copy:
#endif	/* INET6 */
	if (saddr)
		PF_ACPY(&key->addr[sidx], saddr, af);
	if (daddr)
		PF_ACPY(&key->addr[didx], daddr, af);

	return (0);
}

int
pf_state_key_setup(struct pf_pdesc *pd, struct pf_state_key **skw,
    struct pf_state_key **sks, int rtableid)
{
	/* if returning error we MUST pool_put state keys ourselves */
	struct pf_state_key *sk1, *sk2;
	u_int wrdom = pd->rdomain;
	int afto = pd->af != pd->naf;

	if ((sk1 = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL)
		return (ENOMEM);

	pf_state_key_addr_setup(pd, sk1, pd->sidx, pd->src, pd->didx, pd->dst,
	    pd->af, 0);
	sk1->port[pd->sidx] = pd->osport;
	sk1->port[pd->didx] = pd->odport;
	sk1->proto = pd->proto;
	sk1->af = pd->af;
	sk1->rdomain = pd->rdomain;
	PF_REF_INIT(sk1->refcnt);
	sk1->removed = 0;
	if (rtableid >= 0)
		wrdom = rtable_l2(rtableid);

	if (PF_ANEQ(&pd->nsaddr, pd->src, pd->af) ||
	    PF_ANEQ(&pd->ndaddr, pd->dst, pd->af) ||
	    pd->nsport != pd->osport || pd->ndport != pd->odport ||
	    wrdom != pd->rdomain || afto) {	/* NAT/NAT64 */
		if ((sk2 = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL) {
			pool_put(&pf_state_key_pl, sk1);
			return (ENOMEM);
		}
		pf_state_key_addr_setup(pd, sk2, afto ? pd->didx : pd->sidx,
		    &pd->nsaddr, afto ? pd->sidx : pd->didx, &pd->ndaddr,
		    pd->naf, 0);
		sk2->port[afto ? pd->didx : pd->sidx] = pd->nsport;
		sk2->port[afto ? pd->sidx : pd->didx] = pd->ndport;
		if (afto) {
			switch (pd->proto) {
			case IPPROTO_ICMP:
				sk2->proto = IPPROTO_ICMPV6;
				break;
			case IPPROTO_ICMPV6:
				sk2->proto = IPPROTO_ICMP;
				break;
			default:
				sk2->proto = pd->proto;
			}
		} else
			sk2->proto = pd->proto;
		sk2->af = pd->naf;
		sk2->rdomain = wrdom;
		PF_REF_INIT(sk2->refcnt);
		sk2->removed = 0;
	} else
		sk2 = sk1;

	if (pd->dir == PF_IN) {
		*skw = sk1;
		*sks = sk2;
	} else {
		*sks = sk1;
		*skw = sk2;
	}

	if (pf_status.debug >= LOG_DEBUG) {
		log(LOG_DEBUG, "pf: key setup: ");
		pf_print_state_parts(NULL, *skw, *sks);
		addlog("\n");
	}

	return (0);
}

int
pf_state_insert(struct pfi_kif *kif, struct pf_state_key **skw,
    struct pf_state_key **sks, struct pf_state *s)
{
	PF_ASSERT_LOCKED();

	s->kif = kif;
	if (*skw == *sks) {
		if (pf_state_key_attach(*skw, s, PF_SK_WIRE))
			return (-1);
		*skw = *sks = s->key[PF_SK_WIRE];
		s->key[PF_SK_STACK] = s->key[PF_SK_WIRE];
	} else {
		if (pf_state_key_attach(*skw, s, PF_SK_WIRE)) {
			pool_put(&pf_state_key_pl, *sks);
			return (-1);
		}
		*skw = s->key[PF_SK_WIRE];
		if (pf_state_key_attach(*sks, s, PF_SK_STACK)) {
			pf_state_key_detach(s, PF_SK_WIRE);
			return (-1);
		}
		*sks = s->key[PF_SK_STACK];
	}

	if (s->id == 0 && s->creatorid == 0) {
		s->id = htobe64(pf_status.stateid++);
		s->creatorid = pf_status.hostid;
	}
	if (RB_INSERT(pf_state_tree_id, &tree_id, s) != NULL) {
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: state insert failed: "
			    "id: %016llx creatorid: %08x",
			    betoh64(s->id), ntohl(s->creatorid));
			addlog("\n");
		}
		pf_detach_state(s);
		return (-1);
	}
	TAILQ_INSERT_TAIL(&state_list, s, entry_list);
	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
	pfi_kif_ref(kif, PFI_KIF_REF_STATE);
#if NPFSYNC > 0
	pfsync_insert_state(s);
#endif	/* NPFSYNC > 0 */
	return (0);
}

struct pf_state *
pf_find_state_byid(struct pf_state_cmp *key)
{
	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	return (RB_FIND(pf_state_tree_id, &tree_id, (struct pf_state *)key));
}

int
pf_compare_state_keys(struct pf_state_key *a, struct pf_state_key *b,
    struct pfi_kif *kif, u_int dir)
{
	/* a (from hdr) and b (new) must be exact opposites of each other */
	if (a->af == b->af && a->proto == b->proto &&
	    PF_AEQ(&a->addr[0], &b->addr[1], a->af) &&
	    PF_AEQ(&a->addr[1], &b->addr[0], a->af) &&
	    a->port[0] == b->port[1] &&
	    a->port[1] == b->port[0] && a->rdomain == b->rdomain)
		return (0);
	else {
		/* mismatch. must not happen. */
		if (pf_status.debug >= LOG_ERR) {
			log(LOG_ERR,
			    "pf: state key linking mismatch! dir=%s, "
			    "if=%s, stored af=%u, a0: ",
			    dir == PF_OUT ? "OUT" : "IN",
			    kif->pfik_name, a->af);
			pf_print_host(&a->addr[0], a->port[0], a->af);
			addlog(", a1: ");
			pf_print_host(&a->addr[1], a->port[1], a->af);
			addlog(", proto=%u", a->proto);
			addlog(", found af=%u, a0: ", b->af);
			pf_print_host(&b->addr[0], b->port[0], b->af);
			addlog(", a1: ");
			pf_print_host(&b->addr[1], b->port[1], b->af);
			addlog(", proto=%u", b->proto);
			addlog("\n");
		}
		return (-1);
	}
}

struct pf_state *
pf_find_state(struct pfi_kif *kif, struct pf_state_key_cmp *key, u_int dir,
    struct mbuf *m)
{
	struct pf_state_key	*sk, *pkt_sk, *inp_sk;
	struct pf_state_item	*si;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	if (pf_status.debug >= LOG_DEBUG) {
		log(LOG_DEBUG, "pf: key search, if=%s: ", kif->pfik_name);
		pf_print_state_parts(NULL, (struct pf_state_key *)key, NULL);
		addlog("\n");
	}

	inp_sk = NULL;
	pkt_sk = NULL;
	sk = NULL;
	if (dir == PF_OUT) {
		/* first if block deals with outbound forwarded packet */
		pkt_sk = m->m_pkthdr.pf.statekey;

		if (!pf_state_key_isvalid(pkt_sk)) {
			pf_mbuf_unlink_state_key(m);
			pkt_sk = NULL;
		}

		if (pkt_sk && pf_state_key_isvalid(pkt_sk->reverse))
			sk = pkt_sk->reverse;

		if (pkt_sk == NULL) {
			/* here we deal with local outbound packet */
			if (m->m_pkthdr.pf.inp != NULL) {
				inp_sk = m->m_pkthdr.pf.inp->inp_pf_sk;
				if (pf_state_key_isvalid(inp_sk))
					sk = inp_sk;
				else
					pf_inpcb_unlink_state_key(
					    m->m_pkthdr.pf.inp);
			}
		}
	}

	if (sk == NULL) {
		if ((sk = RB_FIND(pf_state_tree, &pf_statetbl,
		    (struct pf_state_key *)key)) == NULL)
			return (NULL);
		if (dir == PF_OUT && pkt_sk &&
		    pf_compare_state_keys(pkt_sk, sk, kif, dir) == 0)
			pf_state_key_link_reverse(sk, pkt_sk);
		else if (dir == PF_OUT && m->m_pkthdr.pf.inp &&
		    !m->m_pkthdr.pf.inp->inp_pf_sk && !sk->inp)
			pf_state_key_link_inpcb(sk, m->m_pkthdr.pf.inp);
	}

	/* remove firewall data from outbound packet */
	if (dir == PF_OUT)
		pf_pkt_addr_changed(m);

	/* list is sorted, if-bound states before floating ones */
	TAILQ_FOREACH(si, &sk->states, entry)
		if ((si->s->kif == pfi_all || si->s->kif == kif) &&
		    ((si->s->key[PF_SK_WIRE]->af == si->s->key[PF_SK_STACK]->af
		    && sk == (dir == PF_IN ? si->s->key[PF_SK_WIRE] :
		    si->s->key[PF_SK_STACK])) ||
		    (si->s->key[PF_SK_WIRE]->af != si->s->key[PF_SK_STACK]->af
		    && dir == PF_IN && (sk == si->s->key[PF_SK_STACK] ||
		    sk == si->s->key[PF_SK_WIRE]))))
			return (si->s);

	return (NULL);
}

struct pf_state *
pf_find_state_all(struct pf_state_key_cmp *key, u_int dir, int *more)
{
	struct pf_state_key	*sk;
	struct pf_state_item	*si, *ret = NULL;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	sk = RB_FIND(pf_state_tree, &pf_statetbl, (struct pf_state_key *)key);

	if (sk != NULL) {
		TAILQ_FOREACH(si, &sk->states, entry)
			if (dir == PF_INOUT ||
			    (sk == (dir == PF_IN ? si->s->key[PF_SK_WIRE] :
			    si->s->key[PF_SK_STACK]))) {
				if (more == NULL)
					return (si->s);

				if (ret)
					(*more)++;
				else
					ret = si;
			}
	}
	return (ret ? ret->s : NULL);
}

void
pf_state_export(struct pfsync_state *sp, struct pf_state *st)
{
	int32_t expire;

	memset(sp, 0, sizeof(struct pfsync_state));

	/* copy from state key */
	sp->key[PF_SK_WIRE].addr[0] = st->key[PF_SK_WIRE]->addr[0];
	sp->key[PF_SK_WIRE].addr[1] = st->key[PF_SK_WIRE]->addr[1];
	sp->key[PF_SK_WIRE].port[0] = st->key[PF_SK_WIRE]->port[0];
	sp->key[PF_SK_WIRE].port[1] = st->key[PF_SK_WIRE]->port[1];
	sp->key[PF_SK_WIRE].rdomain = htons(st->key[PF_SK_WIRE]->rdomain);
	sp->key[PF_SK_WIRE].af = st->key[PF_SK_WIRE]->af;
	sp->key[PF_SK_STACK].addr[0] = st->key[PF_SK_STACK]->addr[0];
	sp->key[PF_SK_STACK].addr[1] = st->key[PF_SK_STACK]->addr[1];
	sp->key[PF_SK_STACK].port[0] = st->key[PF_SK_STACK]->port[0];
	sp->key[PF_SK_STACK].port[1] = st->key[PF_SK_STACK]->port[1];
	sp->key[PF_SK_STACK].rdomain = htons(st->key[PF_SK_STACK]->rdomain);
	sp->key[PF_SK_STACK].af = st->key[PF_SK_STACK]->af;
	sp->rtableid[PF_SK_WIRE] = htonl(st->rtableid[PF_SK_WIRE]);
	sp->rtableid[PF_SK_STACK] = htonl(st->rtableid[PF_SK_STACK]);
	sp->proto = st->key[PF_SK_WIRE]->proto;
	sp->af = st->key[PF_SK_WIRE]->af;

	/* copy from state */
	strlcpy(sp->ifname, st->kif->pfik_name, sizeof(sp->ifname));
	memcpy(&sp->rt_addr, &st->rt_addr, sizeof(sp->rt_addr));
	sp->creation = htonl(time_uptime - st->creation);
	expire = pf_state_expires(st);
	if (expire <= time_uptime)
		sp->expire = htonl(0);
	else
		sp->expire = htonl(expire - time_uptime);

	sp->direction = st->direction;
#if NPFLOG > 0
	sp->log = st->log;
#endif	/* NPFLOG > 0 */
	sp->timeout = st->timeout;
	sp->state_flags = htons(st->state_flags);
	if (!SLIST_EMPTY(&st->src_nodes))
		sp->sync_flags |= PFSYNC_FLAG_SRCNODE;

	sp->id = st->id;
	sp->creatorid = st->creatorid;
	pf_state_peer_hton(&st->src, &sp->src);
	pf_state_peer_hton(&st->dst, &sp->dst);

	if (st->rule.ptr == NULL)
		sp->rule = htonl(-1);
	else
		sp->rule = htonl(st->rule.ptr->nr);
	if (st->anchor.ptr == NULL)
		sp->anchor = htonl(-1);
	else
		sp->anchor = htonl(st->anchor.ptr->nr);
	sp->nat_rule = htonl(-1);	/* left for compat, nat_rule is gone */

	pf_state_counter_hton(st->packets[0], sp->packets[0]);
	pf_state_counter_hton(st->packets[1], sp->packets[1]);
	pf_state_counter_hton(st->bytes[0], sp->bytes[0]);
	pf_state_counter_hton(st->bytes[1], sp->bytes[1]);

	sp->max_mss = htons(st->max_mss);
	sp->min_ttl = st->min_ttl;
	sp->set_tos = st->set_tos;
	sp->set_prio[0] = st->set_prio[0];
	sp->set_prio[1] = st->set_prio[1];
}

/* END state table stuff */

void
pf_purge_expired_rules(void)
{
	struct pf_rule	*r;

	PF_ASSERT_LOCKED();

	if (SLIST_EMPTY(&pf_rule_gcl))
		return;

	while ((r = SLIST_FIRST(&pf_rule_gcl)) != NULL) {
		SLIST_REMOVE(&pf_rule_gcl, r, pf_rule, gcle);
		KASSERT(r->rule_flag & PFRULE_EXPIRED);
		pf_purge_rule(r);
	}
}

void
pf_purge_timeout(void *unused)
{
	task_add(net_tq(0), &pf_purge_task);
}

void
pf_purge(void *xnloops)
{
	int *nloops = xnloops;

	KERNEL_LOCK();
	NET_LOCK();

	PF_LOCK();
	/* process a fraction of the state table every second */
	pf_purge_expired_states(1 + (pf_status.states
	    / pf_default_rule.timeout[PFTM_INTERVAL]));

	/* purge other expired types every PFTM_INTERVAL seconds */
	if (++(*nloops) >= pf_default_rule.timeout[PFTM_INTERVAL]) {
		pf_purge_expired_src_nodes(0);
		pf_purge_expired_rules();
	}
	PF_UNLOCK();

	/*
	 * Fragments don't require PF_LOCK(), they use their own lock.
	 */
	if ((*nloops) >= pf_default_rule.timeout[PFTM_INTERVAL]) {
		pf_purge_expired_fragments();
		*nloops = 0;
	}
	NET_UNLOCK();
	KERNEL_UNLOCK();

	timeout_add(&pf_purge_to, 1 * hz);
}

int32_t
pf_state_expires(const struct pf_state *state)
{
	u_int32_t	timeout;
	u_int32_t	start;
	u_int32_t	end;
	u_int32_t	states;

	/* handle all PFTM_* > PFTM_MAX here */
	if (state->timeout == PFTM_PURGE)
		return (0);

	KASSERT(state->timeout != PFTM_UNLINKED);
	KASSERT(state->timeout < PFTM_MAX);

	timeout = state->rule.ptr->timeout[state->timeout];
	if (!timeout)
		timeout = pf_default_rule.timeout[state->timeout];

	start = state->rule.ptr->timeout[PFTM_ADAPTIVE_START];
	if (start) {
		end = state->rule.ptr->timeout[PFTM_ADAPTIVE_END];
		states = state->rule.ptr->states_cur;
	} else {
		start = pf_default_rule.timeout[PFTM_ADAPTIVE_START];
		end = pf_default_rule.timeout[PFTM_ADAPTIVE_END];
		states = pf_status.states;
	}
	if (end && states > start && start < end) {
		if (states >= end)
			return (0);

		timeout = (u_int64_t)timeout * (end - states) / (end - start);
	}

	return (state->expire + timeout);
}

void
pf_purge_expired_src_nodes(void)
{
	struct pf_src_node		*cur, *next;

	PF_ASSERT_LOCKED();

	for (cur = RB_MIN(pf_src_tree, &tree_src_tracking); cur; cur = next) {
	next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);

		if (cur->states == 0 && cur->expire <= time_uptime) {
			next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);
			pf_remove_src_node(cur);
		}
	}
}

void
pf_src_tree_remove_state(struct pf_state *s)
{
	u_int32_t		 timeout;
	struct pf_sn_item	*sni;

	while ((sni = SLIST_FIRST(&s->src_nodes)) != NULL) {
		SLIST_REMOVE_HEAD(&s->src_nodes, next);
		if (s->src.tcp_est)
			--sni->sn->conn;
		if (--sni->sn->states == 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
				    pf_default_rule.timeout[PFTM_SRC_NODE];
			sni->sn->expire = time_uptime + timeout;
		}
		pool_put(&pf_sn_item_pl, sni);
	}
}

void
pf_remove_state(struct pf_state *cur)
{
	PF_ASSERT_LOCKED();

	/* handle load balancing related tasks */
	pf_postprocess_addr(cur);

	if (cur->src.state == PF_TCPS_PROXY_DST) {
		pf_send_tcp(cur->rule.ptr, cur->key[PF_SK_WIRE]->af,
		    &cur->key[PF_SK_WIRE]->addr[1],
		    &cur->key[PF_SK_WIRE]->addr[0],
		    cur->key[PF_SK_WIRE]->port[1],
		    cur->key[PF_SK_WIRE]->port[0],
		    cur->src.seqhi, cur->src.seqlo + 1,
		    TH_RST|TH_ACK, 0, 0, 0, 1, cur->tag,
		    cur->key[PF_SK_WIRE]->rdomain);
	}
	if (cur->key[PF_SK_STACK]->proto == IPPROTO_TCP)
		pf_set_protostate(cur, PF_PEER_BOTH, TCPS_CLOSED);

	RB_REMOVE(pf_state_tree_id, &tree_id, cur);
#if NPFLOW > 0
	if (cur->state_flags & PFSTATE_PFLOW)
		export_pflow(cur);
#endif	/* NPFLOW > 0 */
#if NPFSYNC > 0
	pfsync_delete_state(cur);
#endif	/* NPFSYNC > 0 */
	cur->timeout = PFTM_UNLINKED;
	pf_src_tree_remove_state(cur);
	pf_detach_state(cur);
}

void
pf_remove_divert_state(struct pf_state_key *sk)
{
	struct pf_state_item	*si;

	TAILQ_FOREACH(si, &sk->states, entry) {
		if (sk == si->s->key[PF_SK_STACK] && si->s->rule.ptr &&
		    (si->s->rule.ptr->divert.type == PF_DIVERT_TO ||
		    si->s->rule.ptr->divert.type == PF_DIVERT_REPLY)) {
			pf_remove_state(si->s);
			break;
		}
	}
}

void
pf_free_state(struct pf_state *cur)
{
	struct pf_rule_item *ri;

	PF_ASSERT_LOCKED();

#if NPFSYNC > 0
	if (pfsync_state_in_use(cur))
		return;
#endif	/* NPFSYNC > 0 */
	KASSERT(cur->timeout == PFTM_UNLINKED);
	if (--cur->rule.ptr->states_cur == 0 &&
	    cur->rule.ptr->src_nodes == 0)
		pf_rm_rule(NULL, cur->rule.ptr);
	if (cur->anchor.ptr != NULL)
		if (--cur->anchor.ptr->states_cur == 0)
			pf_rm_rule(NULL, cur->anchor.ptr);
	while ((ri = SLIST_FIRST(&cur->match_rules))) {
		SLIST_REMOVE_HEAD(&cur->match_rules, entry);
		if (--ri->r->states_cur == 0 &&
		    ri->r->src_nodes == 0)
			pf_rm_rule(NULL, ri->r);
		pool_put(&pf_rule_item_pl, ri);
	}
	pf_normalize_tcp_cleanup(cur);
	pfi_kif_unref(cur->kif, PFI_KIF_REF_STATE);
	TAILQ_REMOVE(&state_list, cur, entry_list);
	if (cur->tag)
		pf_tag_unref(cur->tag);
	pool_put(&pf_state_pl, cur);
	pf_status.fcounters[FCNT_STATE_REMOVALS]++;
	pf_status.states--;
}

void
pf_purge_expired_states(u_int32_t maxcheck)
{
	static struct pf_state	*cur = NULL;
	struct pf_state		*next;

	PF_ASSERT_LOCKED();

	while (maxcheck--) {
		/* wrap to start of list when we hit the end */
		if (cur == NULL) {
			cur = TAILQ_FIRST(&state_list);
			if (cur == NULL)
				break;	/* list empty */
		}

		/* get next state, as cur may get deleted */
		next = TAILQ_NEXT(cur, entry_list);

		if (cur->timeout == PFTM_UNLINKED) {
			/* free removed state */
			pf_free_state(cur);
		} else if (pf_state_expires(cur) <= time_uptime) {
			/* remove and free expired state */
			pf_remove_state(cur);
			pf_free_state(cur);
		}
		cur = next;
	}
}

int
pf_tbladdr_setup(struct pf_ruleset *rs, struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_TABLE)
		return (0);
	if ((aw->p.tbl = pfr_attach_table(rs, aw->v.tblname, 1)) == NULL)
		return (1);
	return (0);
}

void
pf_tbladdr_remove(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_TABLE || aw->p.tbl == NULL)
		return;
	pfr_detach_table(aw->p.tbl);
	aw->p.tbl = NULL;
}

void
pf_tbladdr_copyout(struct pf_addr_wrap *aw)
{
	struct pfr_ktable *kt = aw->p.tbl;

	if (aw->type != PF_ADDR_TABLE || kt == NULL)
		return;
	if (!(kt->pfrkt_flags & PFR_TFLAG_ACTIVE) && kt->pfrkt_root != NULL)
		kt = kt->pfrkt_root;
	aw->p.tbl = NULL;
	aw->p.tblcnt = (kt->pfrkt_flags & PFR_TFLAG_ACTIVE) ?
		kt->pfrkt_cnt : -1;
}

void
pf_print_host(struct pf_addr *addr, u_int16_t p, sa_family_t af)
{
	switch (af) {
	case AF_INET: {
		u_int32_t a = ntohl(addr->addr32[0]);
		addlog("%u.%u.%u.%u", (a>>24)&255, (a>>16)&255,
		    (a>>8)&255, a&255);
		if (p) {
			p = ntohs(p);
			addlog(":%u", p);
		}
		break;
	}
#ifdef INET6
	case AF_INET6: {
		u_int16_t b;
		u_int8_t i, curstart, curend, maxstart, maxend;
		curstart = curend = maxstart = maxend = 255;
		for (i = 0; i < 8; i++) {
			if (!addr->addr16[i]) {
				if (curstart == 255)
					curstart = i;
				curend = i;
			} else {
				if ((curend - curstart) >
				    (maxend - maxstart)) {
					maxstart = curstart;
					maxend = curend;
				}
				curstart = curend = 255;
			}
		}
		if ((curend - curstart) >
		    (maxend - maxstart)) {
			maxstart = curstart;
			maxend = curend;
		}
		for (i = 0; i < 8; i++) {
			if (i >= maxstart && i <= maxend) {
				if (i == 0)
					addlog(":");
				if (i == maxend)
					addlog(":");
			} else {
				b = ntohs(addr->addr16[i]);
				addlog("%x", b);
				if (i < 7)
					addlog(":");
			}
		}
		if (p) {
			p = ntohs(p);
			addlog("[%u]", p);
		}
		break;
	}
#endif /* INET6 */
	}
}

void
pf_print_state(struct pf_state *s)
{
	pf_print_state_parts(s, NULL, NULL);
}

void
pf_print_state_parts(struct pf_state *s,
    struct pf_state_key *skwp, struct pf_state_key *sksp)
{
	struct pf_state_key *skw, *sks;
	u_int8_t proto, dir;

	/* Do our best to fill these, but they're skipped if NULL */
	skw = skwp ? skwp : (s ? s->key[PF_SK_WIRE] : NULL);
	sks = sksp ? sksp : (s ? s->key[PF_SK_STACK] : NULL);
	proto = skw ? skw->proto : (sks ? sks->proto : 0);
	dir = s ? s->direction : 0;

	switch (proto) {
	case IPPROTO_IPV4:
		addlog("IPv4");
		break;
	case IPPROTO_IPV6:
		addlog("IPv6");
		break;
	case IPPROTO_TCP:
		addlog("TCP");
		break;
	case IPPROTO_UDP:
		addlog("UDP");
		break;
	case IPPROTO_ICMP:
		addlog("ICMP");
		break;
	case IPPROTO_ICMPV6:
		addlog("ICMPv6");
		break;
	default:
		addlog("%u", proto);
		break;
	}
	switch (dir) {
	case PF_IN:
		addlog(" in");
		break;
	case PF_OUT:
		addlog(" out");
		break;
	}
	if (skw) {
		addlog(" wire: (%d) ", skw->rdomain);
		pf_print_host(&skw->addr[0], skw->port[0], skw->af);
		addlog(" ");
		pf_print_host(&skw->addr[1], skw->port[1], skw->af);
	}
	if (sks) {
		addlog(" stack: (%d) ", sks->rdomain);
		if (sks != skw) {
			pf_print_host(&sks->addr[0], sks->port[0], sks->af);
			addlog(" ");
			pf_print_host(&sks->addr[1], sks->port[1], sks->af);
		} else
			addlog("-");
	}
	if (s) {
		if (proto == IPPROTO_TCP) {
			addlog(" [lo=%u high=%u win=%u modulator=%u",
			    s->src.seqlo, s->src.seqhi,
			    s->src.max_win, s->src.seqdiff);
			if (s->src.wscale && s->dst.wscale)
				addlog(" wscale=%u",
				    s->src.wscale & PF_WSCALE_MASK);
			addlog("]");
			addlog(" [lo=%u high=%u win=%u modulator=%u",
			    s->dst.seqlo, s->dst.seqhi,
			    s->dst.max_win, s->dst.seqdiff);
			if (s->src.wscale && s->dst.wscale)
				addlog(" wscale=%u",
				s->dst.wscale & PF_WSCALE_MASK);
			addlog("]");
		}
		addlog(" %u:%u", s->src.state, s->dst.state);
		if (s->rule.ptr)
			addlog(" @%d", s->rule.ptr->nr);
	}
}

void
pf_print_flags(u_int8_t f)
{
	if (f)
		addlog(" ");
	if (f & TH_FIN)
		addlog("F");
	if (f & TH_SYN)
		addlog("S");
	if (f & TH_RST)
		addlog("R");
	if (f & TH_PUSH)
		addlog("P");
	if (f & TH_ACK)
		addlog("A");
	if (f & TH_URG)
		addlog("U");
	if (f & TH_ECE)
		addlog("E");
	if (f & TH_CWR)
		addlog("W");
}

#define	PF_SET_SKIP_STEPS(i)					\
	do {							\
		while (head[i] != cur) {			\
			head[i]->skip[i].ptr = cur;		\
			head[i] = TAILQ_NEXT(head[i], entries);	\
		}						\
	} while (0)

void
pf_calc_skip_steps(struct pf_rulequeue *rules)
{
	struct pf_rule *cur, *prev, *head[PF_SKIP_COUNT];
	int i;

	cur = TAILQ_FIRST(rules);
	prev = cur;
	for (i = 0; i < PF_SKIP_COUNT; ++i)
		head[i] = cur;
	while (cur != NULL) {
		if (cur->kif != prev->kif || cur->ifnot != prev->ifnot)
			PF_SET_SKIP_STEPS(PF_SKIP_IFP);
		if (cur->direction != prev->direction)
			PF_SET_SKIP_STEPS(PF_SKIP_DIR);
		if (cur->onrdomain != prev->onrdomain ||
		    cur->ifnot != prev->ifnot)
			PF_SET_SKIP_STEPS(PF_SKIP_RDOM);
		if (cur->af != prev->af)
			PF_SET_SKIP_STEPS(PF_SKIP_AF);
		if (cur->proto != prev->proto)
			PF_SET_SKIP_STEPS(PF_SKIP_PROTO);
		if (cur->src.neg != prev->src.neg ||
		    pf_addr_wrap_neq(&cur->src.addr, &prev->src.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_ADDR);
		if (cur->dst.neg != prev->dst.neg ||
		    pf_addr_wrap_neq(&cur->dst.addr, &prev->dst.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_DST_ADDR);
		if (cur->src.port[0] != prev->src.port[0] ||
		    cur->src.port[1] != prev->src.port[1] ||
		    cur->src.port_op != prev->src.port_op)
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_PORT);
		if (cur->dst.port[0] != prev->dst.port[0] ||
		    cur->dst.port[1] != prev->dst.port[1] ||
		    cur->dst.port_op != prev->dst.port_op)
			PF_SET_SKIP_STEPS(PF_SKIP_DST_PORT);

		prev = cur;
		cur = TAILQ_NEXT(cur, entries);
	}
	for (i = 0; i < PF_SKIP_COUNT; ++i)
		PF_SET_SKIP_STEPS(i);
}

int
pf_addr_wrap_neq(struct pf_addr_wrap *aw1, struct pf_addr_wrap *aw2)
{
	if (aw1->type != aw2->type)
		return (1);
	switch (aw1->type) {
	case PF_ADDR_ADDRMASK:
	case PF_ADDR_RANGE:
		if (PF_ANEQ(&aw1->v.a.addr, &aw2->v.a.addr, AF_INET6))
			return (1);
		if (PF_ANEQ(&aw1->v.a.mask, &aw2->v.a.mask, AF_INET6))
			return (1);
		return (0);
	case PF_ADDR_DYNIFTL:
		return (aw1->p.dyn->pfid_kt != aw2->p.dyn->pfid_kt);
	case PF_ADDR_NONE:
	case PF_ADDR_NOROUTE:
	case PF_ADDR_URPFFAILED:
		return (0);
	case PF_ADDR_TABLE:
		return (aw1->p.tbl != aw2->p.tbl);
	case PF_ADDR_RTLABEL:
		return (aw1->v.rtlabel != aw2->v.rtlabel);
	default:
		addlog("invalid address type: %d\n", aw1->type);
		return (1);
	}
}

/* This algorithm computes 'a + b - c' in ones-complement using a trick to
 * emulate at most one ones-complement subtraction. This thereby limits net
 * carries/borrows to at most one, eliminating a reduction step and saving one
 * each of +, >>, & and ~.
 *
 * def. x mod y = x - (x//y)*y for integer x,y
 * def. sum = x mod 2^16
 * def. accumulator = (x >> 16) mod 2^16
 *
 * The trick works as follows: subtracting exactly one u_int16_t from the
 * u_int32_t x incurs at most one underflow, wrapping its upper 16-bits, the
 * accumulator, to 2^16 - 1. Adding this to the 16-bit sum preserves the
 * ones-complement borrow:
 *
 *  (sum + accumulator) mod 2^16
 * =	{ assume underflow: accumulator := 2^16 - 1 }
 *  (sum + 2^16 - 1) mod 2^16
 * =	{ mod }
 *  (sum - 1) mod 2^16
 *
 * Although this breaks for sum = 0, giving 0xffff, which is ones-complement's
 * other zero, not -1, that cannot occur: the 16-bit sum cannot be underflown
 * to zero as that requires subtraction of at least 2^16, which exceeds a
 * single u_int16_t's range.
 *
 * We use the following theorem to derive the implementation:
 *
 * th. (x + (y mod z)) mod z  =  (x + y) mod z   (0)
 * proof.
 *     (x + (y mod z)) mod z
 *    =  { def mod }
 *     (x + y - (y//z)*z) mod z
 *    =  { (a + b*c) mod c = a mod c }
 *     (x + y) mod z			[end of proof]
 *
 * ... and thereby obtain:
 *
 *  (sum + accumulator) mod 2^16
 * =	{ def. accumulator, def. sum }
 *  (x mod 2^16 + (x >> 16) mod 2^16) mod 2^16
 * =	{ (0), twice }
 *  (x + (x >> 16)) mod 2^16
 * =	{ x mod 2^n = x & (2^n - 1) }
 *  (x + (x >> 16)) & 0xffff
 *
 * Note: this serves also as a reduction step for at most one add (as the
 * trailing mod 2^16 prevents further reductions by destroying carries).
 */
static __inline void
pf_cksum_fixup(u_int16_t *cksum, u_int16_t was, u_int16_t now,
    u_int8_t proto)
{
	u_int32_t x;
	const int udp = proto == IPPROTO_UDP;

	x = *cksum + was - now;
	x = (x + (x >> 16)) & 0xffff;

	/* optimise: eliminate a branch when not udp */
	if (udp && *cksum == 0x0000)
		return;
	if (udp && x == 0x0000)
		x = 0xffff;

	*cksum = (u_int16_t)(x);
}

#ifdef INET6
/* pre: coverage(cksum) is superset of coverage(covered_cksum) */
static __inline void
pf_cksum_uncover(u_int16_t *cksum, u_int16_t covered_cksum, u_int8_t proto)
{
	pf_cksum_fixup(cksum, ~covered_cksum, 0x0, proto);
}

/* pre: disjoint(coverage(cksum), coverage(uncovered_cksum)) */
static __inline void
pf_cksum_cover(u_int16_t *cksum, u_int16_t uncovered_cksum, u_int8_t proto)
{
	pf_cksum_fixup(cksum, 0x0, ~uncovered_cksum, proto);
}
#endif /* INET6 */

/* pre: *a is 16-bit aligned within its packet
 *
 * This algorithm emulates 16-bit ones-complement sums on a twos-complement
 * machine by conserving ones-complement's otherwise discarded carries in the
 * upper bits of x. These accumulated carries when added to the lower 16-bits
 * over at least zero 'reduction' steps then complete the ones-complement sum.
 *
 * def. sum = x mod 2^16
 * def. accumulator = (x >> 16)
 *
 * At most two reduction steps
 *
 *   x := sum + accumulator
 * =    { def sum, def accumulator }
 *   x := x mod 2^16 + (x >> 16)
 * =    { x mod 2^n = x & (2^n - 1) }
 *   x := (x & 0xffff) + (x >> 16)
 *
 * are necessary to incorporate the accumulated carries (at most one per add)
 * i.e. to reduce x < 2^16 from at most 16 carries in the upper 16 bits.
 *
 * The function is also invariant over the endian of the host. Why?
 *
 * Define the unary transpose operator ~ on a bitstring in python slice
 * notation as lambda m: m[P:] + m[:P] , for some constant pivot P.
 *
 * th. ~ distributes over ones-complement addition, denoted by +_1, i.e.
 *
 *     ~m +_1 ~n  =  ~(m +_1 n)    (for all bitstrings m,n of equal length)
 *
 * proof. Regard the bitstrings in m +_1 n as split at P, forming at most two
 * 'half-adds'. Under ones-complement addition, each half-add carries to the
 * other, so the sum of each half-add is unaffected by their relative
 * order. Therefore:
 *
 *     ~m +_1 ~n
 *   =    { half-adds invariant under transposition }
 *     ~s
 *   =    { substitute }
 *     ~(m +_1 n)                   [end of proof]
 *
 * th. Summing two in-memory ones-complement 16-bit variables m,n on a machine
 * with the converse endian does not alter the result.
 *
 * proof.
 *        { converse machine endian: load/store transposes, P := 8 }
 *     ~(~m +_1 ~n)
 *   =    { ~ over +_1 }
 *     ~~m +_1 ~~n
 *   =    { ~ is an involution }
 *      m +_1 n                     [end of proof]
 *
 */
#define NEG(x) ((u_int16_t)~(x))
void
pf_cksum_fixup_a(u_int16_t *cksum, const struct pf_addr *a,
    const struct pf_addr *an, sa_family_t af, u_int8_t proto)
{
	u_int32_t	 x;
	const u_int16_t	*n = an->addr16;
	const u_int16_t *o = a->addr16;
	const int	 udp = proto == IPPROTO_UDP;

	switch (af) {
	case AF_INET:
		x = *cksum + o[0] + NEG(n[0]) + o[1] + NEG(n[1]);
		break;
#ifdef INET6
	case AF_INET6:
		x = *cksum + o[0] + NEG(n[0]) + o[1] + NEG(n[1]) +\
			     o[2] + NEG(n[2]) + o[3] + NEG(n[3]) +\
			     o[4] + NEG(n[4]) + o[5] + NEG(n[5]) +\
			     o[6] + NEG(n[6]) + o[7] + NEG(n[7]);
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}

	x = (x & 0xffff) + (x >> 16);
	x = (x & 0xffff) + (x >> 16);

	/* optimise: eliminate a branch when not udp */
	if (udp && *cksum == 0x0000)
		return;
	if (udp && x == 0x0000)
		x = 0xffff;

	*cksum = (u_int16_t)(x);
}

int
pf_patch_8(struct pf_pdesc *pd, u_int8_t *f, u_int8_t v, bool hi)
{
	int	rewrite = 0;

	if (*f != v) {
		u_int16_t old = htons(hi ? (*f << 8) : *f);
		u_int16_t new = htons(hi ? ( v << 8) :  v);

		pf_cksum_fixup(pd->pcksum, old, new, pd->proto);
		*f = v;
		rewrite = 1;
	}

	return (rewrite);
}

/* pre: *f is 16-bit aligned within its packet */
int
pf_patch_16(struct pf_pdesc *pd, u_int16_t *f, u_int16_t v)
{
	int	rewrite = 0;

	if (*f != v) {
		pf_cksum_fixup(pd->pcksum, *f, v, pd->proto);
		*f = v;
		rewrite = 1;
	}

	return (rewrite);
}

int
pf_patch_16_unaligned(struct pf_pdesc *pd, void *f, u_int16_t v, bool hi)
{
	int		rewrite = 0;
	u_int8_t       *fb = (u_int8_t*)f;
	u_int8_t       *vb = (u_int8_t*)&v;

	if (hi && ALIGNED_POINTER(f, u_int16_t)) {
		return (pf_patch_16(pd, f, v)); /* optimise */
	}

	rewrite += pf_patch_8(pd, fb++, *vb++, hi);
	rewrite += pf_patch_8(pd, fb++, *vb++,!hi);

	return (rewrite);
}

/* pre: *f is 16-bit aligned within its packet */
/* pre: pd->proto != IPPROTO_UDP */
int
pf_patch_32(struct pf_pdesc *pd, u_int32_t *f, u_int32_t v)
{
	int		rewrite = 0;
	u_int16_t      *pc = pd->pcksum;
	u_int8_t        proto = pd->proto;

	/* optimise: inline udp fixup code is unused; let compiler scrub it */
	if (proto == IPPROTO_UDP)
		panic("pf_patch_32: udp");

	/* optimise: skip *f != v guard; true for all use-cases */
	pf_cksum_fixup(pc, *f / (1 << 16), v / (1 << 16), proto);
	pf_cksum_fixup(pc, *f % (1 << 16), v % (1 << 16), proto);

	*f = v;
	rewrite = 1;

	return (rewrite);
}

int
pf_patch_32_unaligned(struct pf_pdesc *pd, void *f, u_int32_t v, bool hi)
{
	int		rewrite = 0;
	u_int8_t       *fb = (u_int8_t*)f;
	u_int8_t       *vb = (u_int8_t*)&v;

	if (hi && ALIGNED_POINTER(f, u_int32_t)) {
		return (pf_patch_32(pd, f, v)); /* optimise */
	}

	rewrite += pf_patch_8(pd, fb++, *vb++, hi);
	rewrite += pf_patch_8(pd, fb++, *vb++,!hi);
	rewrite += pf_patch_8(pd, fb++, *vb++, hi);
	rewrite += pf_patch_8(pd, fb++, *vb++,!hi);

	return (rewrite);
}

int
pf_icmp_mapping(struct pf_pdesc *pd, u_int8_t type, int *icmp_dir,
    u_int16_t *virtual_id, u_int16_t *virtual_type)
{
	/*
	 * ICMP types marked with PF_OUT are typically responses to
	 * PF_IN, and will match states in the opposite direction.
	 * PF_IN ICMP types need to match a state with that type.
	 */
	*icmp_dir = PF_OUT;

	/* Queries (and responses) */
	switch (pd->af) {
	case AF_INET:
		switch (type) {
		case ICMP_ECHO:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_ECHOREPLY:
			*virtual_type = ICMP_ECHO;
			*virtual_id = pd->hdr.icmp.icmp_id;
			break;

		case ICMP_TSTAMP:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_TSTAMPREPLY:
			*virtual_type = ICMP_TSTAMP;
			*virtual_id = pd->hdr.icmp.icmp_id;
			break;

		case ICMP_IREQ:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_IREQREPLY:
			*virtual_type = ICMP_IREQ;
			*virtual_id = pd->hdr.icmp.icmp_id;
			break;

		case ICMP_MASKREQ:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_MASKREPLY:
			*virtual_type = ICMP_MASKREQ;
			*virtual_id = pd->hdr.icmp.icmp_id;
			break;

		case ICMP_IPV6_WHEREAREYOU:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_IPV6_IAMHERE:
			*virtual_type = ICMP_IPV6_WHEREAREYOU;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ICMP_MOBILE_REGREQUEST:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_MOBILE_REGREPLY:
			*virtual_type = ICMP_MOBILE_REGREQUEST;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ICMP_ROUTERSOLICIT:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP_ROUTERADVERT:
			*virtual_type = ICMP_ROUTERSOLICIT;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		/* These ICMP types map to other connections */
		case ICMP_UNREACH:
		case ICMP_SOURCEQUENCH:
		case ICMP_REDIRECT:
		case ICMP_TIMXCEED:
		case ICMP_PARAMPROB:
			/* These will not be used, but set them anyway */
			*icmp_dir = PF_IN;
			*virtual_type = htons(type);
			*virtual_id = 0;
			return (1);  /* These types match to another state */

		/*
		 * All remaining ICMP types get their own states,
		 * and will only match in one direction.
		 */
		default:
			*icmp_dir = PF_IN;
			*virtual_type = type;
			*virtual_id = 0;
			break;
		}
		break;
#ifdef INET6
	case AF_INET6:
		switch (type) {
		case ICMP6_ECHO_REQUEST:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP6_ECHO_REPLY:
			*virtual_type = ICMP6_ECHO_REQUEST;
			*virtual_id = pd->hdr.icmp6.icmp6_id;
			break;

		case MLD_LISTENER_QUERY:
		case MLD_LISTENER_REPORT: {
			struct mld_hdr *mld = &pd->hdr.mld;
			u_int32_t h;

			/*
			 * Listener Report can be sent by clients
			 * without an associated Listener Query.
			 * In addition to that, when Report is sent as a
			 * reply to a Query its source and destination
			 * address are different.
			 */
			*icmp_dir = PF_IN;
			*virtual_type = MLD_LISTENER_QUERY;
			/* generate fake id for these messages */
			h = mld->mld_addr.s6_addr32[0] ^
			    mld->mld_addr.s6_addr32[1] ^
			    mld->mld_addr.s6_addr32[2] ^
			    mld->mld_addr.s6_addr32[3];
			*virtual_id = (h >> 16) ^ (h & 0xffff);
			break;
		}

		/*
		 * ICMP6_FQDN and ICMP6_NI query/reply are the same type as
		 * ICMP6_WRU
		 */
		case ICMP6_WRUREQUEST:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ICMP6_WRUREPLY:
			*virtual_type = ICMP6_WRUREQUEST;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case MLD_MTRACE:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case MLD_MTRACE_RESP:
			*virtual_type = MLD_MTRACE;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ND_NEIGHBOR_SOLICIT:
			*icmp_dir = PF_IN;
			/* FALLTHROUGH */
		case ND_NEIGHBOR_ADVERT: {
			struct nd_neighbor_solicit *nd = &pd->hdr.nd_ns;
			u_int32_t h;

			*virtual_type = ND_NEIGHBOR_SOLICIT;
			/* generate fake id for these messages */
			h = nd->nd_ns_target.s6_addr32[0] ^
			    nd->nd_ns_target.s6_addr32[1] ^
			    nd->nd_ns_target.s6_addr32[2] ^
			    nd->nd_ns_target.s6_addr32[3];
			*virtual_id = (h >> 16) ^ (h & 0xffff);
			break;
		}

		/*
		 * These ICMP types map to other connections.
		 * ND_REDIRECT can't be in this list because the triggering
		 * packet header is optional.
		 */
		case ICMP6_DST_UNREACH:
		case ICMP6_PACKET_TOO_BIG:
		case ICMP6_TIME_EXCEEDED:
		case ICMP6_PARAM_PROB:
			/* These will not be used, but set them anyway */
			*icmp_dir = PF_IN;
			*virtual_type = htons(type);
			*virtual_id = 0;
			return (1);  /* These types match to another state */
		/*
		 * All remaining ICMP6 types get their own states,
		 * and will only match in one direction.
		 */
		default:
			*icmp_dir = PF_IN;
			*virtual_type = type;
			*virtual_id = 0;
			break;
		}
		break;
#endif /* INET6 */
	}
	*virtual_type = htons(*virtual_type);
	return (0);  /* These types match to their own state */
}

void
pf_translate_icmp(struct pf_pdesc *pd, struct pf_addr *qa, u_int16_t *qp,
    struct pf_addr *oa, struct pf_addr *na, u_int16_t np)
{
	/* note: doesn't trouble to fixup quoted checksums, if any */

	/* change quoted protocol port */
	if (qp != NULL)
		pf_patch_16(pd, qp, np);

	/* change quoted ip address */
	pf_cksum_fixup_a(pd->pcksum, qa, na, pd->af, pd->proto);
	PF_ACPY(qa, na, pd->af);

	/* change network-header's ip address */
	if (oa)
		pf_translate_a(pd, oa, na);
}

/* pre: *a is 16-bit aligned within its packet */
/*      *a is a network header src/dst address */
int
pf_translate_a(struct pf_pdesc *pd, struct pf_addr *a, struct pf_addr *an)
{
	int	rewrite = 0;

	/* warning: !PF_ANEQ != PF_AEQ */
	if (!PF_ANEQ(a, an, pd->af))
		return (0);

	/* fixup transport pseudo-header, if any */
	switch (pd->proto) {
	case IPPROTO_TCP:       /* FALLTHROUGH */
	case IPPROTO_UDP:	/* FALLTHROUGH */
	case IPPROTO_ICMPV6:
		pf_cksum_fixup_a(pd->pcksum, a, an, pd->af, pd->proto);
		break;
	default:
		break;  /* assume no pseudo-header */
	}

	PF_ACPY(a, an, pd->af);
	rewrite = 1;

	return (rewrite);
}

#if INET6
/* pf_translate_af() may change pd->m, adjust local copies after calling */
int
pf_translate_af(struct pf_pdesc *pd)
{
	static const struct pf_addr	zero;
	struct ip		       *ip4;
	struct ip6_hdr		       *ip6;
	int				copyback = 0;
	u_int				hlen, ohlen, dlen;
	u_int16_t		       *pc;
	u_int8_t			af_proto, naf_proto;

	hlen = (pd->naf == AF_INET) ? sizeof(*ip4) : sizeof(*ip6);
	ohlen = pd->off;
	dlen = pd->tot_len - pd->off;
	pc = pd->pcksum;

	af_proto = naf_proto = pd->proto;
	if (naf_proto == IPPROTO_ICMP)
		af_proto = IPPROTO_ICMPV6;
	if (naf_proto == IPPROTO_ICMPV6)
		af_proto = IPPROTO_ICMP;

	/* uncover stale pseudo-header */
	switch (af_proto) {
	case IPPROTO_ICMPV6:
		/* optimise: unchanged for TCP/UDP */
		pf_cksum_fixup(pc, htons(af_proto), 0x0, af_proto);
		pf_cksum_fixup(pc, htons(dlen),     0x0, af_proto);
				/* FALLTHROUGH */
	case IPPROTO_UDP:	/* FALLTHROUGH */
	case IPPROTO_TCP:
		pf_cksum_fixup_a(pc, pd->src, &zero, pd->af, af_proto);
		pf_cksum_fixup_a(pc, pd->dst, &zero, pd->af, af_proto);
		copyback = 1;
		break;
	default:
		break;	/* assume no pseudo-header */
	}

	/* replace the network header */
	m_adj(pd->m, pd->off);
	pd->src = NULL;
	pd->dst = NULL;

	if ((M_PREPEND(pd->m, hlen, M_DONTWAIT)) == NULL) {
		pd->m = NULL;
		return (-1);
	}

	pd->off = hlen;
	pd->tot_len += hlen - ohlen;

	switch (pd->naf) {
	case AF_INET:
		ip4 = mtod(pd->m, struct ip *);
		memset(ip4, 0, hlen);
		ip4->ip_v   = IPVERSION;
		ip4->ip_hl  = hlen >> 2;
		ip4->ip_tos = pd->tos;
		ip4->ip_len = htons(hlen + dlen);
		ip4->ip_id  = htons(ip_randomid());
		ip4->ip_off = htons(IP_DF);
		ip4->ip_ttl = pd->ttl;
		ip4->ip_p   = pd->proto;
		ip4->ip_src = pd->nsaddr.v4;
		ip4->ip_dst = pd->ndaddr.v4;
		break;
	case AF_INET6:
		ip6 = mtod(pd->m, struct ip6_hdr *);
		memset(ip6, 0, hlen);
		ip6->ip6_vfc  = IPV6_VERSION;
		ip6->ip6_flow |= htonl((u_int32_t)pd->tos << 20);
		ip6->ip6_plen = htons(dlen);
		ip6->ip6_nxt  = pd->proto;
		if (!pd->ttl || pd->ttl > IPV6_DEFHLIM)
			ip6->ip6_hlim = IPV6_DEFHLIM;
		else
			ip6->ip6_hlim = pd->ttl;
		ip6->ip6_src  = pd->nsaddr.v6;
		ip6->ip6_dst  = pd->ndaddr.v6;
		break;
	default:
		unhandled_af(pd->naf);
	}

	/* UDP over IPv6 must be checksummed per rfc2460 p27 */
	if (naf_proto == IPPROTO_UDP && *pc == 0x0000 &&
	    pd->naf == AF_INET6) {
		pd->m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
	}

	/* cover fresh pseudo-header */
	switch (naf_proto) {
	case IPPROTO_ICMPV6:
		/* optimise: unchanged for TCP/UDP */
		pf_cksum_fixup(pc, 0x0, htons(naf_proto), naf_proto);
		pf_cksum_fixup(pc, 0x0, htons(dlen),      naf_proto);
				/* FALLTHROUGH */
	case IPPROTO_UDP:	/* FALLTHROUGH */
	case IPPROTO_TCP:
		pf_cksum_fixup_a(pc, &zero, &pd->nsaddr, pd->naf, naf_proto);
		pf_cksum_fixup_a(pc, &zero, &pd->ndaddr, pd->naf, naf_proto);
		copyback = 1;
		break;
	default:
		break;	/* assume no pseudo-header */
	}

	/* flush pd->pcksum */
	if (copyback)
		m_copyback(pd->m, pd->off, pd->hdrlen, &pd->hdr, M_NOWAIT);

	return (0);
}

int
pf_change_icmp_af(struct mbuf *m, int ipoff2, struct pf_pdesc *pd,
    struct pf_pdesc *pd2, struct pf_addr *src, struct pf_addr *dst,
    sa_family_t af, sa_family_t naf)
{
	struct mbuf		*n = NULL;
	struct ip		*ip4;
	struct ip6_hdr		*ip6;
	u_int			 hlen, ohlen, dlen;
	int			 d;

	if (af == naf || (af != AF_INET && af != AF_INET6) ||
	    (naf != AF_INET && naf != AF_INET6))
		return (-1);

	/* split the mbuf chain on the quoted ip/ip6 header boundary */
	if ((n = m_split(m, ipoff2, M_DONTWAIT)) == NULL)
		return (-1);

	/* new quoted header */
	hlen = naf == AF_INET ? sizeof(*ip4) : sizeof(*ip6);
	/* old quoted header */
	ohlen = pd2->off - ipoff2;

	/* trim old quoted header */
	pf_cksum_uncover(pd->pcksum, in_cksum(n, ohlen), pd->proto);
	m_adj(n, ohlen);

	/* prepend a new, translated, quoted header */
	if ((M_PREPEND(n, hlen, M_DONTWAIT)) == NULL)
		return (-1);

	switch (naf) {
	case AF_INET:
		ip4 = mtod(n, struct ip *);
		memset(ip4, 0, sizeof(*ip4));
		ip4->ip_v   = IPVERSION;
		ip4->ip_hl  = sizeof(*ip4) >> 2;
		ip4->ip_len = htons(sizeof(*ip4) + pd2->tot_len - ohlen);
		ip4->ip_id  = htons(ip_randomid());
		ip4->ip_off = htons(IP_DF);
		ip4->ip_ttl = pd2->ttl;
		if (pd2->proto == IPPROTO_ICMPV6)
			ip4->ip_p = IPPROTO_ICMP;
		else
			ip4->ip_p = pd2->proto;
		ip4->ip_src = src->v4;
		ip4->ip_dst = dst->v4;
		ip4->ip_sum = in_cksum(n, ip4->ip_hl << 2);
		break;
	case AF_INET6:
		ip6 = mtod(n, struct ip6_hdr *);
		memset(ip6, 0, sizeof(*ip6));
		ip6->ip6_vfc  = IPV6_VERSION;
		ip6->ip6_plen = htons(pd2->tot_len - ohlen);
		if (pd2->proto == IPPROTO_ICMP)
			ip6->ip6_nxt = IPPROTO_ICMPV6;
		else
			ip6->ip6_nxt = pd2->proto;
		if (!pd2->ttl || pd2->ttl > IPV6_DEFHLIM)
			ip6->ip6_hlim = IPV6_DEFHLIM;
		else
			ip6->ip6_hlim = pd2->ttl;
		ip6->ip6_src  = src->v6;
		ip6->ip6_dst  = dst->v6;
		break;
	}

	/* cover new quoted header */
	/* optimise: any new AF_INET header of ours sums to zero */
	if (naf != AF_INET) {
		pf_cksum_cover(pd->pcksum, in_cksum(n, hlen), pd->proto);
	}

	/* reattach modified quoted packet to outer header */
	{
		int nlen = n->m_pkthdr.len;
		m_cat(m, n);
		m->m_pkthdr.len += nlen;
	}

	/* account for altered length */
	d = hlen - ohlen;

	if (pd->proto == IPPROTO_ICMPV6) {
		/* fixup pseudo-header */
		dlen = pd->tot_len - pd->off;
		pf_cksum_fixup(pd->pcksum,
		    htons(dlen), htons(dlen + d), pd->proto);
	}

	pd->tot_len  += d;
	pd2->tot_len += d;
	pd2->off     += d;

	/* note: not bothering to update network headers as
	   these due for rewrite by pf_translate_af() */

	return (0);
}


#define PTR_IP(field)	(offsetof(struct ip, field))
#define PTR_IP6(field)	(offsetof(struct ip6_hdr, field))

int
pf_translate_icmp_af(struct pf_pdesc *pd, int af, void *arg)
{
	struct icmp		*icmp4;
	struct icmp6_hdr	*icmp6;
	u_int32_t		 mtu;
	int32_t			 ptr = -1;
	u_int8_t		 type;
	u_int8_t		 code;

	switch (af) {
	case AF_INET:
		icmp6 = arg;
		type  = icmp6->icmp6_type;
		code  = icmp6->icmp6_code;
		mtu   = ntohl(icmp6->icmp6_mtu);

		switch (type) {
		case ICMP6_ECHO_REQUEST:
			type = ICMP_ECHO;
			break;
		case ICMP6_ECHO_REPLY:
			type = ICMP_ECHOREPLY;
			break;
		case ICMP6_DST_UNREACH:
			type = ICMP_UNREACH;
			switch (code) {
			case ICMP6_DST_UNREACH_NOROUTE:
			case ICMP6_DST_UNREACH_BEYONDSCOPE:
			case ICMP6_DST_UNREACH_ADDR:
				code = ICMP_UNREACH_HOST;
				break;
			case ICMP6_DST_UNREACH_ADMIN:
				code = ICMP_UNREACH_HOST_PROHIB;
				break;
			case ICMP6_DST_UNREACH_NOPORT:
				code = ICMP_UNREACH_PORT;
				break;
			default:
				return (-1);
			}
			break;
		case ICMP6_PACKET_TOO_BIG:
			type = ICMP_UNREACH;
			code = ICMP_UNREACH_NEEDFRAG;
			mtu -= 20;
			break;
		case ICMP6_TIME_EXCEEDED:
			type = ICMP_TIMXCEED;
			break;
		case ICMP6_PARAM_PROB:
			switch (code) {
			case ICMP6_PARAMPROB_HEADER:
				type = ICMP_PARAMPROB;
				code = ICMP_PARAMPROB_ERRATPTR;
				ptr  = ntohl(icmp6->icmp6_pptr);

				if (ptr == PTR_IP6(ip6_vfc))
					; /* preserve */
				else if (ptr == PTR_IP6(ip6_vfc) + 1)
					ptr = PTR_IP(ip_tos);
				else if (ptr == PTR_IP6(ip6_plen) ||
				    ptr == PTR_IP6(ip6_plen) + 1)
					ptr = PTR_IP(ip_len);
				else if (ptr == PTR_IP6(ip6_nxt))
					ptr = PTR_IP(ip_p);
				else if (ptr == PTR_IP6(ip6_hlim))
					ptr = PTR_IP(ip_ttl);
				else if (ptr >= PTR_IP6(ip6_src) &&
				    ptr < PTR_IP6(ip6_dst))
					ptr = PTR_IP(ip_src);
				else if (ptr >= PTR_IP6(ip6_dst) &&
				    ptr < sizeof(struct ip6_hdr))
					ptr = PTR_IP(ip_dst);
				else {
					return (-1);
				}
				break;
			case ICMP6_PARAMPROB_NEXTHEADER:
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_PROTOCOL;
				break;
			default:
				return (-1);
			}
			break;
		default:
			return (-1);
		}

		pf_patch_8(pd, &icmp6->icmp6_type, type, PF_HI);
		pf_patch_8(pd, &icmp6->icmp6_code, code, PF_LO);

		/* aligns well with a icmpv4 nextmtu */
		pf_patch_32(pd, &icmp6->icmp6_mtu, htonl(mtu));

		/* icmpv4 pptr is a one most significant byte */
		if (ptr >= 0)
			pf_patch_32(pd, &icmp6->icmp6_pptr, htonl(ptr << 24));
		break;
	case AF_INET6:
		icmp4 = arg;
		type  = icmp4->icmp_type;
		code  = icmp4->icmp_code;
		mtu   = ntohs(icmp4->icmp_nextmtu);

		switch (type) {
		case ICMP_ECHO:
			type = ICMP6_ECHO_REQUEST;
			break;
		case ICMP_ECHOREPLY:
			type = ICMP6_ECHO_REPLY;
			break;
		case ICMP_UNREACH:
			type = ICMP6_DST_UNREACH;
			switch (code) {
			case ICMP_UNREACH_NET:
			case ICMP_UNREACH_HOST:
			case ICMP_UNREACH_NET_UNKNOWN:
			case ICMP_UNREACH_HOST_UNKNOWN:
			case ICMP_UNREACH_ISOLATED:
			case ICMP_UNREACH_TOSNET:
			case ICMP_UNREACH_TOSHOST:
				code = ICMP6_DST_UNREACH_NOROUTE;
				break;
			case ICMP_UNREACH_PORT:
				code = ICMP6_DST_UNREACH_NOPORT;
				break;
			case ICMP_UNREACH_NET_PROHIB:
			case ICMP_UNREACH_HOST_PROHIB:
			case ICMP_UNREACH_FILTER_PROHIB:
			case ICMP_UNREACH_PRECEDENCE_CUTOFF:
				code = ICMP6_DST_UNREACH_ADMIN;
				break;
			case ICMP_UNREACH_PROTOCOL:
				type = ICMP6_PARAM_PROB;
				code = ICMP6_PARAMPROB_NEXTHEADER;
				ptr  = offsetof(struct ip6_hdr, ip6_nxt);
				break;
			case ICMP_UNREACH_NEEDFRAG:
				type = ICMP6_PACKET_TOO_BIG;
				code = 0;
				mtu += 20;
				break;
			default:
				return (-1);
			}
			break;
		case ICMP_TIMXCEED:
			type = ICMP6_TIME_EXCEEDED;
			break;
		case ICMP_PARAMPROB:
			type = ICMP6_PARAM_PROB;
			switch (code) {
			case ICMP_PARAMPROB_ERRATPTR:
				code = ICMP6_PARAMPROB_HEADER;
				break;
			case ICMP_PARAMPROB_LENGTH:
				code = ICMP6_PARAMPROB_HEADER;
				break;
			default:
				return (-1);
			}

			ptr = icmp4->icmp_pptr;
			if (ptr == 0 || ptr == PTR_IP(ip_tos))
				; /* preserve */
			else if (ptr == PTR_IP(ip_len) ||
			    ptr == PTR_IP(ip_len) + 1)
				ptr = PTR_IP6(ip6_plen);
			else if (ptr == PTR_IP(ip_ttl))
				ptr = PTR_IP6(ip6_hlim);
			else if (ptr == PTR_IP(ip_p))
				ptr = PTR_IP6(ip6_nxt);
			else if (ptr >= PTR_IP(ip_src) &&
			    ptr < PTR_IP(ip_dst))
				ptr = PTR_IP6(ip6_src);
			else if (ptr >= PTR_IP(ip_dst) &&
			    ptr < sizeof(struct ip))
				ptr = PTR_IP6(ip6_dst);
			else {
				return (-1);
			}
			break;
		default:
			return (-1);
		}

		pf_patch_8(pd, &icmp4->icmp_type, type, PF_HI);
		pf_patch_8(pd, &icmp4->icmp_code, code, PF_LO);
		pf_patch_16(pd, &icmp4->icmp_nextmtu, htons(mtu));
		if (ptr >= 0)
			pf_patch_32(pd, &icmp4->icmp_void, htonl(ptr));
		break;
	}

	return (0);
}
#endif /* INET6 */

/*
 * Need to modulate the sequence numbers in the TCP SACK option
 * (credits to Krzysztof Pfaff for report and patch)
 */
int
pf_modulate_sack(struct pf_pdesc *pd, struct pf_state_peer *dst)
{
	struct tcphdr	*th = &pd->hdr.tcp;
	int		 hlen = (th->th_off << 2) - sizeof(*th);
	int		 thoptlen = hlen;
	u_int8_t	 opts[MAX_TCPOPTLEN], *opt = opts;
	int		 copyback = 0, i, olen;
	struct sackblk	 sack;

#define TCPOLEN_SACKLEN	(TCPOLEN_SACK + 2)
	if (hlen < TCPOLEN_SACKLEN || hlen > MAX_TCPOPTLEN || !pf_pull_hdr(
	    pd->m, pd->off + sizeof(*th), opts, hlen, NULL, NULL, pd->af))
		return 0;

	while (hlen >= TCPOLEN_SACKLEN) {
		olen = opt[1];
		switch (*opt) {
		case TCPOPT_EOL:	/* FALLTHROUGH */
		case TCPOPT_NOP:
			opt++;
			hlen--;
			break;
		case TCPOPT_SACK:
			if (olen > hlen)
				olen = hlen;
			if (olen >= TCPOLEN_SACKLEN) {
				for (i = 2; i + TCPOLEN_SACK <= olen;
				    i += TCPOLEN_SACK) {
					size_t startoff = (opt + i) - opts;
					memcpy(&sack, &opt[i], sizeof(sack));
					pf_patch_32_unaligned(pd, &sack.start,
					    htonl(ntohl(sack.start) -
						dst->seqdiff),
					    PF_ALGNMNT(startoff));
					pf_patch_32_unaligned(pd, &sack.end,
					    htonl(ntohl(sack.end) -
						dst->seqdiff),
					    PF_ALGNMNT(startoff +
						sizeof(sack.start)));
					memcpy(&opt[i], &sack, sizeof(sack));
				}
				copyback = 1;
			}
			/* FALLTHROUGH */
		default:
			if (olen < 2)
				olen = 2;
			hlen -= olen;
			opt += olen;
		}
	}

	if (copyback)
		m_copyback(pd->m, pd->off + sizeof(*th), thoptlen, opts,
		    M_NOWAIT);
	return (copyback);
}

struct mbuf *
pf_build_tcp(const struct pf_rule *r, sa_family_t af,
    const struct pf_addr *saddr, const struct pf_addr *daddr,
    u_int16_t sport, u_int16_t dport, u_int32_t seq, u_int32_t ack,
    u_int8_t flags, u_int16_t win, u_int16_t mss, u_int8_t ttl, int tag,
    u_int16_t rtag, u_int sack, u_int rdom)
{
	struct mbuf	*m;
	int		 len, tlen;
	struct ip	*h;
#ifdef INET6
	struct ip6_hdr	*h6;
#endif /* INET6 */
	struct tcphdr	*th;
	char		*opt;

	/* maximum segment size tcp option */
	tlen = sizeof(struct tcphdr);
	if (mss)
		tlen += 4;
	if (sack)
		tlen += 2;

	switch (af) {
	case AF_INET:
		len = sizeof(struct ip) + tlen;
		break;
#ifdef INET6
	case AF_INET6:
		len = sizeof(struct ip6_hdr) + tlen;
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}

	/* create outgoing mbuf */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return (NULL);
	if (tag)
		m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	m->m_pkthdr.pf.tag = rtag;
	m->m_pkthdr.ph_rtableid = rdom;
	if (r && (r->scrub_flags & PFSTATE_SETPRIO))
		m->m_pkthdr.pf.prio = r->set_prio[0];
	if (r && r->qid)
		m->m_pkthdr.pf.qid = r->qid;
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.ph_ifidx = 0;
	m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
	memset(m->m_data, 0, len);
	switch (af) {
	case AF_INET:
		h = mtod(m, struct ip *);
		h->ip_p = IPPROTO_TCP;
		h->ip_len = htons(tlen);
		h->ip_v = 4;
		h->ip_hl = sizeof(*h) >> 2;
		h->ip_tos = IPTOS_LOWDELAY;
		h->ip_len = htons(len);
		h->ip_off = htons(ip_mtudisc ? IP_DF : 0);
		h->ip_ttl = ttl ? ttl : ip_defttl;
		h->ip_sum = 0;
		h->ip_src.s_addr = saddr->v4.s_addr;
		h->ip_dst.s_addr = daddr->v4.s_addr;

		th = (struct tcphdr *)((caddr_t)h + sizeof(struct ip));
		break;
#ifdef INET6
	case AF_INET6:
		h6 = mtod(m, struct ip6_hdr *);
		h6->ip6_nxt = IPPROTO_TCP;
		h6->ip6_plen = htons(tlen);
		h6->ip6_vfc |= IPV6_VERSION;
		h6->ip6_hlim = IPV6_DEFHLIM;
		memcpy(&h6->ip6_src, &saddr->v6, sizeof(struct in6_addr));
		memcpy(&h6->ip6_dst, &daddr->v6, sizeof(struct in6_addr));

		th = (struct tcphdr *)((caddr_t)h6 + sizeof(struct ip6_hdr));
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}

	/* TCP header */
	th->th_sport = sport;
	th->th_dport = dport;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_off = tlen >> 2;
	th->th_flags = flags;
	th->th_win = htons(win);

	opt = (char *)(th + 1);
	if (mss) {
		opt[0] = TCPOPT_MAXSEG;
		opt[1] = 4;
		mss = htons(mss);
		memcpy((opt + 2), &mss, 2);
		opt += 4;
	}
	if (sack) {
		opt[0] = TCPOPT_SACK_PERMITTED;
		opt[1] = 2;
		opt += 2;
	}

	return (m);
}

void
pf_send_tcp(const struct pf_rule *r, sa_family_t af,
    const struct pf_addr *saddr, const struct pf_addr *daddr,
    u_int16_t sport, u_int16_t dport, u_int32_t seq, u_int32_t ack,
    u_int8_t flags, u_int16_t win, u_int16_t mss, u_int8_t ttl, int tag,
    u_int16_t rtag, u_int rdom)
{
	struct mbuf	*m;

	if ((m = pf_build_tcp(r, af, saddr, daddr, sport, dport, seq, ack,
	    flags, win, mss, ttl, tag, rtag, 0, rdom)) == NULL)
		return;

	switch (af) {
	case AF_INET:
		ip_send(m);
		break;
#ifdef INET6
	case AF_INET6:
		ip6_send(m);
		break;
#endif /* INET6 */
	}
}

static void
pf_send_challenge_ack(struct pf_pdesc *pd, struct pf_state *s,
    struct pf_state_peer *src, struct pf_state_peer *dst)
{
	/*
	 * We are sending challenge ACK as a response to SYN packet, which
	 * matches existing state (modulo TCP window check). Therefore packet
	 * must be sent on behalf of destination.
	 *
	 * We expect sender to remain either silent, or send RST packet
	 * so both, firewall and remote peer, can purge dead state from
	 * memory.
	 */
	pf_send_tcp(s->rule.ptr, pd->af, pd->dst, pd->src,
	    pd->hdr.tcp.th_dport, pd->hdr.tcp.th_sport, dst->seqlo,
	    src->seqlo, TH_ACK, 0, 0, s->rule.ptr->return_ttl, 1, 0,
	    pd->rdomain);
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code, int param,
    sa_family_t af, struct pf_rule *r, u_int rdomain)
{
	struct mbuf	*m0;

	if ((m0 = m_copym(m, 0, M_COPYALL, M_NOWAIT)) == NULL)
		return;

	m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	m0->m_pkthdr.ph_rtableid = rdomain;
	if (r && (r->scrub_flags & PFSTATE_SETPRIO))
		m0->m_pkthdr.pf.prio = r->set_prio[0];
	if (r && r->qid)
		m0->m_pkthdr.pf.qid = r->qid;

	switch (af) {
	case AF_INET:
		icmp_error(m0, type, code, 0, param);
		break;
#ifdef INET6
	case AF_INET6:
		icmp6_error(m0, type, code, param);
		break;
#endif /* INET6 */
	}
}

/*
 * Return ((n = 0) == (a = b [with mask m]))
 * Note: n != 0 => returns (a != b [with mask m])
 */
int
pf_match_addr(u_int8_t n, struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		if ((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			return (n == 0);
		break;
#ifdef INET6
	case AF_INET6:
		if (((a->addr32[0] & m->addr32[0]) ==
		     (b->addr32[0] & m->addr32[0])) &&
		    ((a->addr32[1] & m->addr32[1]) ==
		     (b->addr32[1] & m->addr32[1])) &&
		    ((a->addr32[2] & m->addr32[2]) ==
		     (b->addr32[2] & m->addr32[2])) &&
		    ((a->addr32[3] & m->addr32[3]) ==
		     (b->addr32[3] & m->addr32[3])))
			return (n == 0);
		break;
#endif /* INET6 */
	}

	return (n != 0);
}

/*
 * Return 1 if b <= a <= e, otherwise return 0.
 */
int
pf_match_addr_range(struct pf_addr *b, struct pf_addr *e,
    struct pf_addr *a, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		if ((ntohl(a->addr32[0]) < ntohl(b->addr32[0])) ||
		    (ntohl(a->addr32[0]) > ntohl(e->addr32[0])))
			return (0);
		break;
#ifdef INET6
	case AF_INET6: {
		int	i;

		/* check a >= b */
		for (i = 0; i < 4; ++i)
			if (ntohl(a->addr32[i]) > ntohl(b->addr32[i]))
				break;
			else if (ntohl(a->addr32[i]) < ntohl(b->addr32[i]))
				return (0);
		/* check a <= e */
		for (i = 0; i < 4; ++i)
			if (ntohl(a->addr32[i]) < ntohl(e->addr32[i]))
				break;
			else if (ntohl(a->addr32[i]) > ntohl(e->addr32[i]))
				return (0);
		break;
	}
#endif /* INET6 */
	}
	return (1);
}

int
pf_match(u_int8_t op, u_int32_t a1, u_int32_t a2, u_int32_t p)
{
	switch (op) {
	case PF_OP_IRG:
		return ((p > a1) && (p < a2));
	case PF_OP_XRG:
		return ((p < a1) || (p > a2));
	case PF_OP_RRG:
		return ((p >= a1) && (p <= a2));
	case PF_OP_EQ:
		return (p == a1);
	case PF_OP_NE:
		return (p != a1);
	case PF_OP_LT:
		return (p < a1);
	case PF_OP_LE:
		return (p <= a1);
	case PF_OP_GT:
		return (p > a1);
	case PF_OP_GE:
		return (p >= a1);
	}
	return (0); /* never reached */
}

int
pf_match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	return (pf_match(op, ntohs(a1), ntohs(a2), ntohs(p)));
}

int
pf_match_uid(u_int8_t op, uid_t a1, uid_t a2, uid_t u)
{
	if (u == UID_MAX && op != PF_OP_EQ && op != PF_OP_NE)
		return (0);
	return (pf_match(op, a1, a2, u));
}

int
pf_match_gid(u_int8_t op, gid_t a1, gid_t a2, gid_t g)
{
	if (g == GID_MAX && op != PF_OP_EQ && op != PF_OP_NE)
		return (0);
	return (pf_match(op, a1, a2, g));
}

int
pf_match_tag(struct mbuf *m, struct pf_rule *r, int *tag)
{
	if (*tag == -1)
		*tag = m->m_pkthdr.pf.tag;

	return ((!r->match_tag_not && r->match_tag == *tag) ||
	    (r->match_tag_not && r->match_tag != *tag));
}

int
pf_match_rcvif(struct mbuf *m, struct pf_rule *r)
{
	struct ifnet *ifp;
	struct pfi_kif *kif;

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL)
		return (0);

#if NCARP > 0
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
#endif /* NCARP */
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if_put(ifp);

	if (kif == NULL) {
		DPFPRINTF(LOG_ERR,
		    "%s: kif == NULL, @%d via %s", __func__,
		    r->nr, r->rcv_ifname);
		return (0);
	}

	return (pfi_kif_match(r->rcv_kif, kif));
}

void
pf_tag_packet(struct mbuf *m, int tag, int rtableid)
{
	if (tag > 0)
		m->m_pkthdr.pf.tag = tag;
	if (rtableid >= 0)
		m->m_pkthdr.ph_rtableid = (u_int)rtableid;
}

enum pf_test_status
pf_step_into_anchor(struct pf_test_ctx *ctx, struct pf_rule *r)
{
	int	rv;

	if (ctx->depth >= PF_ANCHOR_STACK_MAX) {
		log(LOG_ERR, "pf_step_into_anchor: stack overflow\n");
		return (PF_TEST_FAIL);
	}

	ctx->depth++;

	if (r->anchor_wildcard) {
		struct pf_anchor	*child;
		rv = PF_TEST_OK;
		RB_FOREACH(child, pf_anchor_node, &r->anchor->children) {
			rv = pf_match_rule(ctx, &child->ruleset);
			if ((rv == PF_TEST_QUICK) || (rv == PF_TEST_FAIL)) {
				/*
				 * we either hit a rule with quick action
				 * (more likely), or hit some runtime
				 * error (e.g. pool_get() failure).
				 */
				break;
			}
		}
	} else {
		rv = pf_match_rule(ctx, &r->anchor->ruleset);
	}

	ctx->depth--;

	return (rv);
}

void
pf_poolmask(struct pf_addr *naddr, struct pf_addr *raddr,
    struct pf_addr *rmask, struct pf_addr *saddr, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		break;
#ifdef INET6
	case AF_INET6:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		naddr->addr32[1] = (raddr->addr32[1] & rmask->addr32[1]) |
		((rmask->addr32[1] ^ 0xffffffff ) & saddr->addr32[1]);
		naddr->addr32[2] = (raddr->addr32[2] & rmask->addr32[2]) |
		((rmask->addr32[2] ^ 0xffffffff ) & saddr->addr32[2]);
		naddr->addr32[3] = (raddr->addr32[3] & rmask->addr32[3]) |
		((rmask->addr32[3] ^ 0xffffffff ) & saddr->addr32[3]);
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}
}

void
pf_addr_inc(struct pf_addr *addr, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		addr->addr32[0] = htonl(ntohl(addr->addr32[0]) + 1);
		break;
#ifdef INET6
	case AF_INET6:
		if (addr->addr32[3] == 0xffffffff) {
			addr->addr32[3] = 0;
			if (addr->addr32[2] == 0xffffffff) {
				addr->addr32[2] = 0;
				if (addr->addr32[1] == 0xffffffff) {
					addr->addr32[1] = 0;
					addr->addr32[0] =
					    htonl(ntohl(addr->addr32[0]) + 1);
				} else
					addr->addr32[1] =
					    htonl(ntohl(addr->addr32[1]) + 1);
			} else
				addr->addr32[2] =
				    htonl(ntohl(addr->addr32[2]) + 1);
		} else
			addr->addr32[3] =
			    htonl(ntohl(addr->addr32[3]) + 1);
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}
}

int
pf_socket_lookup(struct pf_pdesc *pd)
{
	struct pf_addr		*saddr, *daddr;
	u_int16_t		 sport, dport;
	struct inpcbtable	*tb;
	struct inpcb		*inp;

	pd->lookup.uid = UID_MAX;
	pd->lookup.gid = GID_MAX;
	pd->lookup.pid = NO_PID;
	switch (pd->virtual_proto) {
	case IPPROTO_TCP:
		sport = pd->hdr.tcp.th_sport;
		dport = pd->hdr.tcp.th_dport;
		PF_ASSERT_LOCKED();
		NET_ASSERT_LOCKED();
		tb = &tcbtable;
		break;
	case IPPROTO_UDP:
		sport = pd->hdr.udp.uh_sport;
		dport = pd->hdr.udp.uh_dport;
		PF_ASSERT_LOCKED();
		NET_ASSERT_LOCKED();
		tb = &udbtable;
		break;
	default:
		return (-1);
	}
	if (pd->dir == PF_IN) {
		saddr = pd->src;
		daddr = pd->dst;
	} else {
		u_int16_t	p;

		p = sport;
		sport = dport;
		dport = p;
		saddr = pd->dst;
		daddr = pd->src;
	}
	switch (pd->af) {
	case AF_INET:
		/*
		 * Fails when rtable is changed while evaluating the ruleset
		 * The socket looked up will not match the one hit in the end.
		 */
		inp = in_pcbhashlookup(tb, saddr->v4, sport, daddr->v4, dport,
		    pd->rdomain);
		if (inp == NULL) {
			inp = in_pcblookup_listen(tb, daddr->v4, dport,
			    NULL, pd->rdomain);
			if (inp == NULL)
				return (-1);
		}
		break;
#ifdef INET6
	case AF_INET6:
		inp = in6_pcbhashlookup(tb, &saddr->v6, sport, &daddr->v6,
		    dport, pd->rdomain);
		if (inp == NULL) {
			inp = in6_pcblookup_listen(tb, &daddr->v6, dport,
			    NULL, pd->rdomain);
			if (inp == NULL)
				return (-1);
		}
		break;
#endif /* INET6 */
	default:
		unhandled_af(pd->af);
	}
	pd->lookup.uid = inp->inp_socket->so_euid;
	pd->lookup.gid = inp->inp_socket->so_egid;
	pd->lookup.pid = inp->inp_socket->so_cpid;
	return (1);
}

u_int8_t
pf_get_wscale(struct pf_pdesc *pd)
{
	struct tcphdr	*th = &pd->hdr.tcp;
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
	u_int8_t	 wscale = 0;

	hlen = th->th_off << 2;		/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(pd->m, pd->off, hdr, hlen, NULL, NULL, pd->af))
		return (0);
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= 3) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_WINDOW:
			wscale = opt[2];
			if (wscale > TCP_MAX_WINSHIFT)
				wscale = TCP_MAX_WINSHIFT;
			wscale |= PF_WSCALE_FLAG;
			/* FALLTHROUGH */
		default:
			optlen = opt[1];
			if (optlen < 2)
				optlen = 2;
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return (wscale);
}

u_int16_t
pf_get_mss(struct pf_pdesc *pd)
{
	struct tcphdr	*th = &pd->hdr.tcp;
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
	u_int16_t	 mss = tcp_mssdflt;

	hlen = th->th_off << 2;		/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(pd->m, pd->off, hdr, hlen, NULL, NULL, pd->af))
		return (0);
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= TCPOLEN_MAXSEG) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_MAXSEG:
			memcpy(&mss, (opt + 2), 2);
			mss = ntohs(mss);
			/* FALLTHROUGH */
		default:
			optlen = opt[1];
			if (optlen < 2)
				optlen = 2;
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return (mss);
}

u_int16_t
pf_calc_mss(struct pf_addr *addr, sa_family_t af, int rtableid, u_int16_t offer)
{
	struct ifnet		*ifp;
	struct sockaddr_in	*dst;
#ifdef INET6
	struct sockaddr_in6	*dst6;
#endif /* INET6 */
	struct rtentry		*rt = NULL;
	struct sockaddr_storage	 ss;
	int			 hlen;
	u_int16_t		 mss = tcp_mssdflt;

	memset(&ss, 0, sizeof(ss));

	switch (af) {
	case AF_INET:
		hlen = sizeof(struct ip);
		dst = (struct sockaddr_in *)&ss;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		rt = rtalloc(sintosa(dst), 0, rtableid);
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		dst6 = (struct sockaddr_in6 *)&ss;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		rt = rtalloc(sin6tosa(dst6), 0, rtableid);
		break;
#endif /* INET6 */
	}

	if (rt != NULL && (ifp = if_get(rt->rt_ifidx)) != NULL) {
		mss = ifp->if_mtu - hlen - sizeof(struct tcphdr);
		mss = max(tcp_mssdflt, mss);
		if_put(ifp);
	}
	rtfree(rt);
	mss = min(mss, offer);
	mss = max(mss, 64);		/* sanity - at least max opt space */
	return (mss);
}

static __inline int
pf_set_rt_ifp(struct pf_state *s, struct pf_addr *saddr, sa_family_t af)
{
	struct pf_rule *r = s->rule.ptr;
	struct pf_src_node *sns[PF_SN_MAX];
	int	rv;

	s->rt_kif = NULL;
	if (!r->rt)
		return (0);

	memset(sns, 0, sizeof(sns));
	switch (af) {
	case AF_INET:
		rv = pf_map_addr(AF_INET, r, saddr, &s->rt_addr, NULL, sns,
		    &r->route, PF_SN_ROUTE);
		break;
#ifdef INET6
	case AF_INET6:
		rv = pf_map_addr(AF_INET6, r, saddr, &s->rt_addr, NULL, sns,
		    &r->route, PF_SN_ROUTE);
		break;
#endif /* INET6 */
	default:
		rv = 1;
	}

	if (rv == 0) {
		s->rt_kif = r->route.kif;
		s->natrule.ptr = r;
	}

	return (rv);
}

u_int32_t
pf_tcp_iss(struct pf_pdesc *pd)
{
	SHA2_CTX ctx;
	union {
		uint8_t bytes[SHA512_DIGEST_LENGTH];
		uint32_t words[1];
	} digest;

	if (pf_tcp_secret_init == 0) {
		arc4random_buf(pf_tcp_secret, sizeof(pf_tcp_secret));
		SHA512Init(&pf_tcp_secret_ctx);
		SHA512Update(&pf_tcp_secret_ctx, pf_tcp_secret,
		    sizeof(pf_tcp_secret));
		pf_tcp_secret_init = 1;
	}
	ctx = pf_tcp_secret_ctx;

	SHA512Update(&ctx, &pd->rdomain, sizeof(pd->rdomain));
	SHA512Update(&ctx, &pd->hdr.tcp.th_sport, sizeof(u_short));
	SHA512Update(&ctx, &pd->hdr.tcp.th_dport, sizeof(u_short));
	switch (pd->af) {
	case AF_INET:
		SHA512Update(&ctx, &pd->src->v4, sizeof(struct in_addr));
		SHA512Update(&ctx, &pd->dst->v4, sizeof(struct in_addr));
		break;
#ifdef INET6
	case AF_INET6:
		SHA512Update(&ctx, &pd->src->v6, sizeof(struct in6_addr));
		SHA512Update(&ctx, &pd->dst->v6, sizeof(struct in6_addr));
		break;
#endif /* INET6 */
	}
	SHA512Final(digest.bytes, &ctx);
	pf_tcp_iss_off += 4096;
	return (digest.words[0] + tcp_iss + pf_tcp_iss_off);
}

void
pf_rule_to_actions(struct pf_rule *r, struct pf_rule_actions *a)
{
	if (r->qid)
		a->qid = r->qid;
	if (r->pqid)
		a->pqid = r->pqid;
	if (r->rtableid >= 0)
		a->rtableid = r->rtableid;
#if NPFLOG > 0
	a->log |= r->log;
#endif	/* NPFLOG > 0 */
	if (r->scrub_flags & PFSTATE_SETTOS)
		a->set_tos = r->set_tos;
	if (r->min_ttl)
		a->min_ttl = r->min_ttl;
	if (r->max_mss)
		a->max_mss = r->max_mss;
	a->flags |= (r->scrub_flags & (PFSTATE_NODF|PFSTATE_RANDOMID|
	    PFSTATE_SETTOS|PFSTATE_SCRUB_TCP|PFSTATE_SETPRIO));
	if (r->scrub_flags & PFSTATE_SETPRIO) {
		a->set_prio[0] = r->set_prio[0];
		a->set_prio[1] = r->set_prio[1];
	}
}

#define PF_TEST_ATTRIB(t, a)			\
	if (t) {				\
		r = a;				\
		continue;			\
	} else do {				\
	} while (0)

enum pf_test_status
pf_match_rule(struct pf_test_ctx *ctx, struct pf_ruleset *ruleset)
{
	struct pf_rule	*r;
	struct pf_rule	*save_a;
	struct pf_ruleset	*save_aruleset;

	r = TAILQ_FIRST(ruleset->rules.active.ptr);
	while (r != NULL) {
		r->evaluations++;
		PF_TEST_ATTRIB(
		    (pfi_kif_match(r->kif, ctx->pd->kif) == r->ifnot),
			r->skip[PF_SKIP_IFP].ptr);
		PF_TEST_ATTRIB((r->direction && r->direction != ctx->pd->dir),
			r->skip[PF_SKIP_DIR].ptr);
		PF_TEST_ATTRIB((r->onrdomain >= 0  &&
		    (r->onrdomain == ctx->pd->rdomain) == r->ifnot),
			r->skip[PF_SKIP_RDOM].ptr);
		PF_TEST_ATTRIB((r->af && r->af != ctx->pd->af),
			r->skip[PF_SKIP_AF].ptr);
		PF_TEST_ATTRIB((r->proto && r->proto != ctx->pd->proto),
			r->skip[PF_SKIP_PROTO].ptr);
		PF_TEST_ATTRIB((PF_MISMATCHAW(&r->src.addr, &ctx->pd->nsaddr,
		    ctx->pd->naf, r->src.neg, ctx->pd->kif,
		    ctx->act.rtableid)),
			r->skip[PF_SKIP_SRC_ADDR].ptr);
		PF_TEST_ATTRIB((PF_MISMATCHAW(&r->dst.addr, &ctx->pd->ndaddr,
		    ctx->pd->af, r->dst.neg, NULL, ctx->act.rtableid)),
			r->skip[PF_SKIP_DST_ADDR].ptr);

		switch (ctx->pd->virtual_proto) {
		case PF_VPROTO_FRAGMENT:
			/* tcp/udp only. port_op always 0 in other cases */
			PF_TEST_ATTRIB((r->src.port_op || r->dst.port_op),
				TAILQ_NEXT(r, entries));
			PF_TEST_ATTRIB((ctx->pd->proto == IPPROTO_TCP &&
			    r->flagset),
				TAILQ_NEXT(r, entries));
			/* icmp only. type/code always 0 in other cases */
			PF_TEST_ATTRIB((r->type || r->code),
				TAILQ_NEXT(r, entries));
			/* tcp/udp only. {uid|gid}.op always 0 in other cases */
			PF_TEST_ATTRIB((r->gid.op || r->uid.op),
				TAILQ_NEXT(r, entries));
			break;

		case IPPROTO_TCP:
			PF_TEST_ATTRIB(((r->flagset & ctx->th->th_flags) !=
			    r->flags),
				TAILQ_NEXT(r, entries));
			PF_TEST_ATTRIB((r->os_fingerprint != PF_OSFP_ANY &&
			    !pf_osfp_match(pf_osfp_fingerprint(ctx->pd),
			    r->os_fingerprint)),
				TAILQ_NEXT(r, entries));
			/* FALLTHROUGH */

		case IPPROTO_UDP:
			/* tcp/udp only. port_op always 0 in other cases */
			PF_TEST_ATTRIB((r->src.port_op &&
			    !pf_match_port(r->src.port_op, r->src.port[0],
			    r->src.port[1], ctx->pd->nsport)),
				r->skip[PF_SKIP_SRC_PORT].ptr);
			PF_TEST_ATTRIB((r->dst.port_op &&
			    !pf_match_port(r->dst.port_op, r->dst.port[0],
			    r->dst.port[1], ctx->pd->ndport)),
				r->skip[PF_SKIP_DST_PORT].ptr);
			/* tcp/udp only. uid.op always 0 in other cases */
			PF_TEST_ATTRIB((r->uid.op && (ctx->pd->lookup.done ||
			    (ctx->pd->lookup.done =
			    pf_socket_lookup(ctx->pd), 1)) &&
			    !pf_match_uid(r->uid.op, r->uid.uid[0],
			    r->uid.uid[1], ctx->pd->lookup.uid)),
				TAILQ_NEXT(r, entries));
			/* tcp/udp only. gid.op always 0 in other cases */
			PF_TEST_ATTRIB((r->gid.op && (ctx->pd->lookup.done ||
			    (ctx->pd->lookup.done =
			    pf_socket_lookup(ctx->pd), 1)) &&
			    !pf_match_gid(r->gid.op, r->gid.gid[0],
			    r->gid.gid[1], ctx->pd->lookup.gid)),
				TAILQ_NEXT(r, entries));
			break;

		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			/* icmp only. type always 0 in other cases */
			PF_TEST_ATTRIB((r->type &&
			    r->type != ctx->icmptype + 1),
				TAILQ_NEXT(r, entries));
			/* icmp only. type always 0 in other cases */
			PF_TEST_ATTRIB((r->code &&
			    r->code != ctx->icmpcode + 1),
				TAILQ_NEXT(r, entries));
			/* icmp only. don't create states on replies */
			PF_TEST_ATTRIB((r->keep_state && !ctx->state_icmp &&
			    (r->rule_flag & PFRULE_STATESLOPPY) == 0 &&
			    ctx->icmp_dir != PF_IN),
				TAILQ_NEXT(r, entries));
			break;

		default:
			break;
		}

		PF_TEST_ATTRIB((r->rule_flag & PFRULE_FRAGMENT &&
		    ctx->pd->virtual_proto != PF_VPROTO_FRAGMENT),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->tos && !(r->tos == ctx->pd->tos)),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->prob &&
		    r->prob <= arc4random_uniform(UINT_MAX - 1) + 1),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->match_tag &&
		    !pf_match_tag(ctx->pd->m, r, &ctx->tag)),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->rcv_kif && pf_match_rcvif(ctx->pd->m, r) ==
		    r->rcvifnot),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->prio &&
		    (r->prio == PF_PRIO_ZERO ? 0 : r->prio) !=
		    ctx->pd->m->m_pkthdr.pf.prio),
			TAILQ_NEXT(r, entries));

		/* must be last! */
		if (r->pktrate.limit) {
			pf_add_threshold(&r->pktrate);
			PF_TEST_ATTRIB((pf_check_threshold(&r->pktrate)),
				TAILQ_NEXT(r, entries));
		}

		/* FALLTHROUGH */
		if (r->tag)
			ctx->tag = r->tag;
		if (r->anchor == NULL) {
			if (r->action == PF_MATCH) {
				if ((ctx->ri = pool_get(&pf_rule_item_pl,
				    PR_NOWAIT)) == NULL) {
					REASON_SET(&ctx->reason, PFRES_MEMORY);
					ctx->test_status = PF_TEST_FAIL;
					break;
				}
				ctx->ri->r = r;
				/* order is irrelevant */
				SLIST_INSERT_HEAD(&ctx->rules, ctx->ri, entry);
				ctx->ri = NULL;
				pf_rule_to_actions(r, &ctx->act);
				if (r->rule_flag & PFRULE_AFTO)
					ctx->pd->naf = r->naf;
				if (pf_get_transaddr(r, ctx->pd, ctx->sns,
				    &ctx->nr) == -1) {
					REASON_SET(&ctx->reason,
					    PFRES_TRANSLATE);
					ctx->test_status = PF_TEST_FAIL;
					break;
				}
#if NPFLOG > 0
				if (r->log) {
					REASON_SET(&ctx->reason, PFRES_MATCH);
					PFLOG_PACKET(ctx->pd, ctx->reason, r,
					    ctx->a, ruleset, NULL);
				}
#endif	/* NPFLOG > 0 */
			} else {
				/*
				 * found matching r
				 */
				*ctx->rm = r;
				/*
				 * anchor, with ruleset, where r belongs to
				 */
				*ctx->am = ctx->a;
				/*
				 * ruleset where r belongs to
				 */
				*ctx->rsm = ruleset;
				/*
				 * ruleset, where anchor belongs to.
				 */
				ctx->arsm = ctx->aruleset;
			}

#if NPFLOG > 0
			if (ctx->act.log & PF_LOG_MATCHES)
				pf_log_matches(ctx->pd, r, ctx->a, ruleset,
				    &ctx->rules);
#endif	/* NPFLOG > 0 */

			if (r->quick) {
				ctx->test_status = PF_TEST_QUICK;
				break;
			}
		} else {
			save_a = ctx->a;
			save_aruleset = ctx->aruleset;
			ctx->a = r;		/* remember anchor */
			ctx->aruleset = ruleset;	/* and its ruleset */
			/*
			 * Note: we don't need to restore if we are not going
			 * to continue with ruleset evaluation.
			 */
			if (pf_step_into_anchor(ctx, r) != PF_TEST_OK)
				break;
			ctx->a = save_a;
			ctx->aruleset = save_aruleset;
		}
		r = TAILQ_NEXT(r, entries);
	}

	return (ctx->test_status);
}

int
pf_test_rule(struct pf_pdesc *pd, struct pf_rule **rm, struct pf_state **sm,
    struct pf_rule **am, struct pf_ruleset **rsm, u_short *reason)
{
	struct pf_rule		*r = NULL;
	struct pf_rule		*a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_state_key	*skw = NULL, *sks = NULL;
	int			 rewrite = 0;
	u_int16_t		 virtual_type, virtual_id;
	int			 action = PF_DROP;
	struct pf_test_ctx	 ctx;
	int			 rv;

	memset(&ctx, 0, sizeof(ctx));
	ctx.pd = pd;
	ctx.rm = rm;
	ctx.am = am;
	ctx.rsm = rsm;
	ctx.th = &pd->hdr.tcp;
	ctx.act.rtableid = pd->rdomain;
	ctx.tag = -1;
	SLIST_INIT(&ctx.rules);

	if (pd->dir == PF_IN && if_congested()) {
		REASON_SET(&ctx.reason, PFRES_CONGEST);
		return (PF_DROP);
	}

	switch (pd->virtual_proto) {
	case IPPROTO_ICMP:
		ctx.icmptype = pd->hdr.icmp.icmp_type;
		ctx.icmpcode = pd->hdr.icmp.icmp_code;
		ctx.state_icmp = pf_icmp_mapping(pd, ctx.icmptype,
		    &ctx.icmp_dir, &virtual_id, &virtual_type);
		if (ctx.icmp_dir == PF_IN) {
			pd->osport = pd->nsport = virtual_id;
			pd->odport = pd->ndport = virtual_type;
		} else {
			pd->osport = pd->nsport = virtual_type;
			pd->odport = pd->ndport = virtual_id;
		}
		break;
#ifdef INET6
	case IPPROTO_ICMPV6:
		ctx.icmptype = pd->hdr.icmp6.icmp6_type;
		ctx.icmpcode = pd->hdr.icmp6.icmp6_code;
		ctx.state_icmp = pf_icmp_mapping(pd, ctx.icmptype,
		    &ctx.icmp_dir, &virtual_id, &virtual_type);
		if (ctx.icmp_dir == PF_IN) {
			pd->osport = pd->nsport = virtual_id;
			pd->odport = pd->ndport = virtual_type;
		} else {
			pd->osport = pd->nsport = virtual_type;
			pd->odport = pd->ndport = virtual_id;
		}
		break;
#endif /* INET6 */
	}

	ruleset = &pf_main_ruleset;
	rv = pf_match_rule(&ctx, ruleset);
	if (rv == PF_TEST_FAIL) {
		/*
		 * Reason has been set in pf_match_rule() already.
		 */
		goto cleanup;
	}

	r = *ctx.rm;	/* matching rule */
	a = *ctx.am;	/* rule that defines an anchor containing 'r' */
	ruleset = *ctx.rsm;/* ruleset of the anchor defined by the rule 'a' */
	ctx.aruleset = ctx.arsm;/* ruleset of the 'a' rule itself */

	/* apply actions for last matching pass/block rule */
	pf_rule_to_actions(r, &ctx.act);
	if (r->rule_flag & PFRULE_AFTO)
		pd->naf = r->naf;
	if (pf_get_transaddr(r, pd, ctx.sns, &ctx.nr) == -1) {
		REASON_SET(&ctx.reason, PFRES_TRANSLATE);
		goto cleanup;
	}
	REASON_SET(&ctx.reason, PFRES_MATCH);

#if NPFLOG > 0
	if (r->log)
		PFLOG_PACKET(pd, ctx.reason, r, a, ruleset, NULL);
	if (ctx.act.log & PF_LOG_MATCHES)
		pf_log_matches(pd, r, a, ruleset, &ctx.rules);
#endif	/* NPFLOG > 0 */

	if (pd->virtual_proto != PF_VPROTO_FRAGMENT &&
	    (r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		if (pd->proto == IPPROTO_TCP &&
		    ((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(ctx.th->th_flags & TH_RST)) {
			u_int32_t	 ack =
			    ntohl(ctx.th->th_seq) + pd->p_len;

			if (pf_check_tcp_cksum(pd->m, pd->off,
			    pd->tot_len - pd->off, pd->af))
				REASON_SET(&ctx.reason, PFRES_PROTCKSUM);
			else {
				if (ctx.th->th_flags & TH_SYN)
					ack++;
				if (ctx.th->th_flags & TH_FIN)
					ack++;
				pf_send_tcp(r, pd->af, pd->dst,
				    pd->src, ctx.th->th_dport,
				    ctx.th->th_sport, ntohl(ctx.th->th_ack),
				    ack, TH_RST|TH_ACK, 0, 0, r->return_ttl,
				    1, 0, pd->rdomain);
			}
		} else if ((pd->proto != IPPROTO_ICMP ||
		    ICMP_INFOTYPE(ctx.icmptype)) && pd->af == AF_INET &&
		    r->return_icmp)
			pf_send_icmp(pd->m, r->return_icmp >> 8,
			    r->return_icmp & 255, 0, pd->af, r, pd->rdomain);
		else if ((pd->proto != IPPROTO_ICMPV6 ||
		    (ctx.icmptype >= ICMP6_ECHO_REQUEST &&
		    ctx.icmptype != ND_REDIRECT)) && pd->af == AF_INET6 &&
		    r->return_icmp6)
			pf_send_icmp(pd->m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, 0, pd->af, r, pd->rdomain);
	}

	if (r->action == PF_DROP)
		goto cleanup;

	pf_tag_packet(pd->m, ctx.tag, ctx.act.rtableid);
	if (ctx.act.rtableid >= 0 &&
	    rtable_l2(ctx.act.rtableid) != pd->rdomain)
		pd->destchg = 1;

	if (r->action == PF_PASS && pd->badopts && ! r->allow_opts) {
		REASON_SET(&ctx.reason, PFRES_IPOPTIONS);
#if NPFLOG > 0
		pd->pflog |= PF_LOG_FORCE;
#endif	/* NPFLOG > 0 */
		DPFPRINTF(LOG_NOTICE, "dropping packet with "
		    "ip/ipv6 options in pf_test_rule()");
		goto cleanup;
	}

	action = PF_PASS;

	if (pd->virtual_proto != PF_VPROTO_FRAGMENT
	    && !ctx.state_icmp && r->keep_state) {

		if (r->rule_flag & PFRULE_SRCTRACK &&
		    pf_insert_src_node(&ctx.sns[PF_SN_NONE], r, PF_SN_NONE,
		    pd->af, pd->src, NULL) != 0) {
			REASON_SET(&ctx.reason, PFRES_SRCLIMIT);
			goto cleanup;
		}

		if (r->max_states && (r->states_cur >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&ctx.reason, PFRES_MAXSTATES);
			goto cleanup;
		}

		action = pf_create_state(pd, r, a, ctx.nr, &skw, &sks,
		    &rewrite, sm, ctx.tag, &ctx.rules, &ctx.act, ctx.sns);

		if (action != PF_PASS)
			goto cleanup;
		if (sks != skw) {
			struct pf_state_key	*sk;

			if (pd->dir == PF_IN)
				sk = sks;
			else
				sk = skw;
			rewrite += pf_translate(pd,
			    &sk->addr[pd->af == pd->naf ? pd->sidx : pd->didx],
			    sk->port[pd->af == pd->naf ? pd->sidx : pd->didx],
			    &sk->addr[pd->af == pd->naf ? pd->didx : pd->sidx],
			    sk->port[pd->af == pd->naf ? pd->didx : pd->sidx],
			    virtual_type, ctx.icmp_dir);
		}

#ifdef INET6
		if (rewrite && skw->af != sks->af)
			action = PF_AFRT;
#endif /* INET6 */

	} else {
		while ((ctx.ri = SLIST_FIRST(&ctx.rules))) {
			SLIST_REMOVE_HEAD(&ctx.rules, entry);
			pool_put(&pf_rule_item_pl, ctx.ri);
		}
	}

	/* copy back packet headers if needed */
	if (rewrite && pd->hdrlen) {
		m_copyback(pd->m, pd->off, pd->hdrlen, &pd->hdr, M_NOWAIT);
	}

#if NPFSYNC > 0
	if (*sm != NULL && !ISSET((*sm)->state_flags, PFSTATE_NOSYNC) &&
	    pd->dir == PF_OUT && pfsync_up()) {
		/*
		 * We want the state created, but we dont
		 * want to send this in case a partner
		 * firewall has to know about it to allow
		 * replies through it.
		 */
		if (pfsync_defer(*sm, pd->m))
			return (PF_DEFER);
	}
#endif	/* NPFSYNC > 0 */

	if (r->rule_flag & PFRULE_ONCE) {
		r->rule_flag |= PFRULE_EXPIRED;
		r->exptime = time_second;
		SLIST_INSERT_HEAD(&pf_rule_gcl, r, gcle);
	}

	return (action);

cleanup:
	while ((ctx.ri = SLIST_FIRST(&ctx.rules))) {
		SLIST_REMOVE_HEAD(&ctx.rules, entry);
		pool_put(&pf_rule_item_pl, ctx.ri);
	}

	return (action);
}

static __inline int
pf_create_state(struct pf_pdesc *pd, struct pf_rule *r, struct pf_rule *a,
    struct pf_rule *nr, struct pf_state_key **skw, struct pf_state_key **sks,
    int *rewrite, struct pf_state **sm, int tag, struct pf_rule_slist *rules,
    struct pf_rule_actions *act, struct pf_src_node *sns[PF_SN_MAX])
{
	struct pf_state		*s = NULL;
	struct tcphdr		*th = &pd->hdr.tcp;
	u_int16_t		 mss = tcp_mssdflt;
	u_short			 reason;
	u_int			 i;

	s = pool_get(&pf_state_pl, PR_NOWAIT | PR_ZERO);
	if (s == NULL) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto csfailed;
	}
	s->rule.ptr = r;
	s->anchor.ptr = a;
	s->natrule.ptr = nr;
	if (r->allow_opts)
		s->state_flags |= PFSTATE_ALLOWOPTS;
	if (r->rule_flag & PFRULE_STATESLOPPY)
		s->state_flags |= PFSTATE_SLOPPY;
	if (r->rule_flag & PFRULE_PFLOW)
		s->state_flags |= PFSTATE_PFLOW;
#if NPFLOG > 0
	s->log = act->log & PF_LOG_ALL;
#endif	/* NPFLOG > 0 */
	s->qid = act->qid;
	s->pqid = act->pqid;
	s->rtableid[pd->didx] = act->rtableid;
	s->rtableid[pd->sidx] = -1;	/* return traffic is routed normally */
	s->min_ttl = act->min_ttl;
	s->set_tos = act->set_tos;
	s->max_mss = act->max_mss;
	s->state_flags |= act->flags;
#if NPFSYNC > 0
	s->sync_state = PFSYNC_S_NONE;
#endif	/* NPFSYNC > 0 */
	s->set_prio[0] = act->set_prio[0];
	s->set_prio[1] = act->set_prio[1];
	SLIST_INIT(&s->src_nodes);

	switch (pd->proto) {
	case IPPROTO_TCP:
		s->src.seqlo = ntohl(th->th_seq);
		s->src.seqhi = s->src.seqlo + pd->p_len + 1;
		if ((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN &&
		    r->keep_state == PF_STATE_MODULATE) {
			/* Generate sequence number modulator */
			if ((s->src.seqdiff = pf_tcp_iss(pd) - s->src.seqlo) ==
			    0)
				s->src.seqdiff = 1;
			pf_patch_32(pd,
			    &th->th_seq, htonl(s->src.seqlo + s->src.seqdiff));
			*rewrite = 1;
		} else
			s->src.seqdiff = 0;
		if (th->th_flags & TH_SYN) {
			s->src.seqhi++;
			s->src.wscale = pf_get_wscale(pd);
		}
		s->src.max_win = MAX(ntohs(th->th_win), 1);
		if (s->src.wscale & PF_WSCALE_MASK) {
			/* Remove scale factor from initial window */
			int win = s->src.max_win;
			win += 1 << (s->src.wscale & PF_WSCALE_MASK);
			s->src.max_win = (win - 1) >>
			    (s->src.wscale & PF_WSCALE_MASK);
		}
		if (th->th_flags & TH_FIN)
			s->src.seqhi++;
		s->dst.seqhi = 1;
		s->dst.max_win = 1;
		pf_set_protostate(s, PF_PEER_SRC, TCPS_SYN_SENT);
		pf_set_protostate(s, PF_PEER_DST, TCPS_CLOSED);
		s->timeout = PFTM_TCP_FIRST_PACKET;
		pf_status.states_halfopen++;
		break;
	case IPPROTO_UDP:
		pf_set_protostate(s, PF_PEER_SRC, PFUDPS_SINGLE);
		pf_set_protostate(s, PF_PEER_DST, PFUDPS_NO_TRAFFIC);
		s->timeout = PFTM_UDP_FIRST_PACKET;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif	/* INET6 */
		s->timeout = PFTM_ICMP_FIRST_PACKET;
		break;
	default:
		pf_set_protostate(s, PF_PEER_SRC, PFOTHERS_SINGLE);
		pf_set_protostate(s, PF_PEER_DST, PFOTHERS_NO_TRAFFIC);
		s->timeout = PFTM_OTHER_FIRST_PACKET;
	}

	s->creation = time_uptime;
	s->expire = time_uptime;

	if (pd->proto == IPPROTO_TCP) {
		if (s->state_flags & PFSTATE_SCRUB_TCP &&
		    pf_normalize_tcp_init(pd, &s->src)) {
			REASON_SET(&reason, PFRES_MEMORY);
			goto csfailed;
		}
		if (s->state_flags & PFSTATE_SCRUB_TCP && s->src.scrub &&
		    pf_normalize_tcp_stateful(pd, &reason, s, &s->src, &s->dst,
		    rewrite)) {
			/* This really shouldn't happen!!! */
			DPFPRINTF(LOG_ERR,
			    "%s: tcp normalize failed on first pkt", __func__);
			goto csfailed;
		}
	}
	s->direction = pd->dir;

	if (pf_state_key_setup(pd, skw, sks, act->rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto csfailed;
	}

	for (i = 0; i < PF_SN_MAX; i++)
		if (sns[i] != NULL) {
			struct pf_sn_item	*sni;

			sni = pool_get(&pf_sn_item_pl, PR_NOWAIT);
			if (sni == NULL) {
				REASON_SET(&reason, PFRES_MEMORY);
				goto csfailed;
			}
			sni->sn = sns[i];
			SLIST_INSERT_HEAD(&s->src_nodes, sni, next);
			sni->sn->states++;
		}

	if (pf_set_rt_ifp(s, pd->src, (*skw)->af) != 0) {
		REASON_SET(&reason, PFRES_NOROUTE);
		goto csfailed;
	}

	if (pf_state_insert(BOUND_IFACE(r, pd->kif), skw, sks, s)) {
		pf_detach_state(s);
		*sks = *skw = NULL;
		REASON_SET(&reason, PFRES_STATEINS);
		goto csfailed;
	} else
		*sm = s;

	/*
	 * Make state responsible for rules it binds here.
	 */
	memcpy(&s->match_rules, rules, sizeof(s->match_rules));
	memset(rules, 0, sizeof(*rules));
	STATE_INC_COUNTERS(s);

	if (tag > 0) {
		pf_tag_ref(tag);
		s->tag = tag;
	}
	if (pd->proto == IPPROTO_TCP && (th->th_flags & (TH_SYN|TH_ACK)) ==
	    TH_SYN && r->keep_state == PF_STATE_SYNPROXY) {
		int rtid = pd->rdomain;
		if (act->rtableid >= 0)
			rtid = act->rtableid;
		pf_set_protostate(s, PF_PEER_SRC, PF_TCPS_PROXY_SRC);
		s->src.seqhi = arc4random();
		/* Find mss option */
		mss = pf_get_mss(pd);
		mss = pf_calc_mss(pd->src, pd->af, rtid, mss);
		mss = pf_calc_mss(pd->dst, pd->af, rtid, mss);
		s->src.mss = mss;
		pf_send_tcp(r, pd->af, pd->dst, pd->src, th->th_dport,
		    th->th_sport, s->src.seqhi, ntohl(th->th_seq) + 1,
		    TH_SYN|TH_ACK, 0, s->src.mss, 0, 1, 0, pd->rdomain);
		REASON_SET(&reason, PFRES_SYNPROXY);
		return (PF_SYNPROXY_DROP);
	}

	return (PF_PASS);

csfailed:
	if (s) {
		pf_normalize_tcp_cleanup(s);	/* safe even w/o init */
		pf_src_tree_remove_state(s);
		pool_put(&pf_state_pl, s);
	}

	for (i = 0; i < PF_SN_MAX; i++)
		if (sns[i] != NULL)
			pf_remove_src_node(sns[i]);

	return (PF_DROP);
}

int
pf_translate(struct pf_pdesc *pd, struct pf_addr *saddr, u_int16_t sport,
    struct pf_addr *daddr, u_int16_t dport, u_int16_t virtual_type,
    int icmp_dir)
{
	/*
	 * when called from bpf_mtap_pflog, there are extra constraints:
	 * -mbuf is faked, m_data is the bpf buffer
	 * -pd is not fully set up
	 */
	int	rewrite = 0;
	int	afto = pd->af != pd->naf;

	if (afto || PF_ANEQ(daddr, pd->dst, pd->af))
		pd->destchg = 1;

	switch (pd->proto) {
	case IPPROTO_TCP:	/* FALLTHROUGH */
	case IPPROTO_UDP:
		rewrite += pf_patch_16(pd, pd->sport, sport);
		rewrite += pf_patch_16(pd, pd->dport, dport);
		break;

	case IPPROTO_ICMP:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET)
			return (0);

		if (afto) {
#ifdef INET6
			if (pf_translate_icmp_af(pd, AF_INET6, &pd->hdr.icmp))
				return (0);
			pd->proto = IPPROTO_ICMPV6;
			rewrite = 1;
#endif /* INET6 */
		}
		if (virtual_type == htons(ICMP_ECHO)) {
			u_int16_t icmpid = (icmp_dir == PF_IN) ? sport : dport;
			rewrite += pf_patch_16(pd,
			    &pd->hdr.icmp.icmp_id, icmpid);
		}
		break;

#ifdef INET6
	case IPPROTO_ICMPV6:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET6)
			return (0);

		if (afto) {
			if (pf_translate_icmp_af(pd, AF_INET, &pd->hdr.icmp6))
				return (0);
			pd->proto = IPPROTO_ICMP;
			rewrite = 1;
		}
		if (virtual_type == htons(ICMP6_ECHO_REQUEST)) {
			u_int16_t icmpid = (icmp_dir == PF_IN) ? sport : dport;
			rewrite += pf_patch_16(pd,
			    &pd->hdr.icmp6.icmp6_id, icmpid);
		}
		break;
#endif /* INET6 */
	}

	if (!afto) {
		rewrite += pf_translate_a(pd, pd->src, saddr);
		rewrite += pf_translate_a(pd, pd->dst, daddr);
	}

	return (rewrite);
}

int
pf_tcp_track_full(struct pf_pdesc *pd, struct pf_state **state, u_short *reason,
    int *copyback, int reverse)
{
	struct tcphdr		*th = &pd->hdr.tcp;
	struct pf_state_peer	*src, *dst;
	u_int16_t		 win = ntohs(th->th_win);
	u_int32_t		 ack, end, data_end, seq, orig_seq;
	u_int8_t		 sws, dws, psrc, pdst;
	int			 ackskew;

	if ((pd->dir == (*state)->direction && !reverse) ||
	    (pd->dir != (*state)->direction && reverse)) {
		src = &(*state)->src;
		dst = &(*state)->dst;
		psrc = PF_PEER_SRC;
		pdst = PF_PEER_DST;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
		psrc = PF_PEER_DST;
		pdst = PF_PEER_SRC;
	}

	if (src->wscale && dst->wscale && !(th->th_flags & TH_SYN)) {
		sws = src->wscale & PF_WSCALE_MASK;
		dws = dst->wscale & PF_WSCALE_MASK;
	} else
		sws = dws = 0;

	/*
	 * Sequence tracking algorithm from Guido van Rooij's paper:
	 *   http://www.madison-gurkha.com/publications/tcp_filtering/
	 *	tcp_filtering.ps
	 */

	orig_seq = seq = ntohl(th->th_seq);
	if (src->seqlo == 0) {
		/* First packet from this end. Set its state */

		if (((*state)->state_flags & PFSTATE_SCRUB_TCP || dst->scrub) &&
		    src->scrub == NULL) {
			if (pf_normalize_tcp_init(pd, src)) {
				REASON_SET(reason, PFRES_MEMORY);
				return (PF_DROP);
			}
		}

		/* Deferred generation of sequence number modulator */
		if (dst->seqdiff && !src->seqdiff) {
			/* use random iss for the TCP server */
			while ((src->seqdiff = arc4random() - seq) == 0)
				continue;
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_patch_32(pd, &th->th_seq, htonl(seq + src->seqdiff));
			pf_patch_32(pd, &th->th_ack, htonl(ack));
			*copyback = 1;
		} else {
			ack = ntohl(th->th_ack);
		}

		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN) {
			end++;
			if (dst->wscale & PF_WSCALE_FLAG) {
				src->wscale = pf_get_wscale(pd);
				if (src->wscale & PF_WSCALE_FLAG) {
					/* Remove scale factor from initial
					 * window */
					sws = src->wscale & PF_WSCALE_MASK;
					win = ((u_int32_t)win + (1 << sws) - 1)
					    >> sws;
					dws = dst->wscale & PF_WSCALE_MASK;
				} else {
					/* fixup other window */
					dst->max_win = MIN(TCP_MAXWIN,
					    (u_int32_t)dst->max_win <<
					    (dst->wscale & PF_WSCALE_MASK));
					/* in case of a retrans SYN|ACK */
					dst->wscale = 0;
				}
			}
		}
		data_end = end;
		if (th->th_flags & TH_FIN)
			end++;

		src->seqlo = seq;
		if (src->state < TCPS_SYN_SENT)
			pf_set_protostate(*state, psrc, TCPS_SYN_SENT);

		/*
		 * May need to slide the window (seqhi may have been set by
		 * the crappy stack check or if we picked up the connection
		 * after establishment)
		 */
		if (src->seqhi == 1 ||
		    SEQ_GEQ(end + MAX(1, dst->max_win << dws), src->seqhi))
			src->seqhi = end + MAX(1, dst->max_win << dws);
		if (win > src->max_win)
			src->max_win = win;

	} else {
		ack = ntohl(th->th_ack) - dst->seqdiff;
		if (src->seqdiff) {
			/* Modulate sequence numbers */
			pf_patch_32(pd, &th->th_seq, htonl(seq + src->seqdiff));
			pf_patch_32(pd, &th->th_ack, htonl(ack));
			*copyback = 1;
		}
		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN)
			end++;
		data_end = end;
		if (th->th_flags & TH_FIN)
			end++;
	}

	if ((th->th_flags & TH_ACK) == 0) {
		/* Let it pass through the ack skew check */
		ack = dst->seqlo;
	} else if ((ack == 0 &&
	    (th->th_flags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) ||
	    /* broken tcp stacks do not set ack */
	    (dst->state < TCPS_SYN_SENT)) {
		/*
		 * Many stacks (ours included) will set the ACK number in an
		 * FIN|ACK if the SYN times out -- no sequence to ACK.
		 */
		ack = dst->seqlo;
	}

	if (seq == end) {
		/* Ease sequencing restrictions on no data packets */
		seq = src->seqlo;
		data_end = end = seq;
	}

	ackskew = dst->seqlo - ack;


	/*
	 * Need to demodulate the sequence numbers in any TCP SACK options
	 * (Selective ACK). We could optionally validate the SACK values
	 * against the current ACK window, either forwards or backwards, but
	 * I'm not confident that SACK has been implemented properly
	 * everywhere. It wouldn't surprise me if several stacks accidently
	 * SACK too far backwards of previously ACKed data. There really aren't
	 * any security implications of bad SACKing unless the target stack
	 * doesn't validate the option length correctly. Someone trying to
	 * spoof into a TCP connection won't bother blindly sending SACK
	 * options anyway.
	 */
	if (dst->seqdiff && (th->th_off << 2) > sizeof(struct tcphdr)) {
		if (pf_modulate_sack(pd, dst))
			*copyback = 1;
	}


#define MAXACKWINDOW (0xffff + 1500)	/* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, data_end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one reassembled fragment backwards */
	    (ackskew <= (MAXACKWINDOW << sws)) &&
	    /* Acking not more than one window forward */
	    ((th->th_flags & TH_RST) == 0 || orig_seq == src->seqlo ||
	    (orig_seq == src->seqlo + 1) || (orig_seq + 1 == src->seqlo))) {
	    /* Require an exact/+1 sequence match on resets when possible */

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(pd, reason, *state, src,
			    dst, copyback))
				return (PF_DROP);
		}

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + (win << sws), dst->seqhi))
			dst->seqhi = ack + MAX((win << sws), 1);

		/* update states */
		if (th->th_flags & TH_SYN)
			if (src->state < TCPS_SYN_SENT)
				pf_set_protostate(*state, psrc, TCPS_SYN_SENT);
		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				pf_set_protostate(*state, psrc, TCPS_CLOSING);
		if (th->th_flags & TH_ACK) {
			if (dst->state == TCPS_SYN_SENT) {
				pf_set_protostate(*state, pdst,
				    TCPS_ESTABLISHED);
				if (src->state == TCPS_ESTABLISHED &&
				    !SLIST_EMPTY(&(*state)->src_nodes) &&
				    pf_src_connlimit(state)) {
					REASON_SET(reason, PFRES_SRCLIMIT);
					return (PF_DROP);
				}
			} else if (dst->state == TCPS_CLOSING)
				pf_set_protostate(*state, pdst,
				    TCPS_FIN_WAIT_2);
		}
		if (th->th_flags & TH_RST)
			pf_set_protostate(*state, PF_PEER_BOTH, TCPS_TIME_WAIT);

		/* update expire time */
		(*state)->expire = time_uptime;
		if (src->state >= TCPS_FIN_WAIT_2 &&
		    dst->state >= TCPS_FIN_WAIT_2)
			(*state)->timeout = PFTM_TCP_CLOSED;
		else if (src->state >= TCPS_CLOSING &&
		    dst->state >= TCPS_CLOSING)
			(*state)->timeout = PFTM_TCP_FIN_WAIT;
		else if (src->state < TCPS_ESTABLISHED ||
		    dst->state < TCPS_ESTABLISHED)
			(*state)->timeout = PFTM_TCP_OPENING;
		else if (src->state >= TCPS_CLOSING ||
		    dst->state >= TCPS_CLOSING)
			(*state)->timeout = PFTM_TCP_CLOSING;
		else
			(*state)->timeout = PFTM_TCP_ESTABLISHED;

		/* Fall through to PASS packet */
	} else if ((dst->state < TCPS_SYN_SENT ||
		dst->state >= TCPS_FIN_WAIT_2 ||
		src->state >= TCPS_FIN_WAIT_2) &&
	    SEQ_GEQ(src->seqhi + MAXACKWINDOW, data_end) &&
	    /* Within a window forward of the originating packet */
	    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW)) {
	    /* Within a window backward of the originating packet */

		/*
		 * This currently handles three situations:
		 *  1) Stupid stacks will shotgun SYNs before their peer
		 *     replies.
		 *  2) When PF catches an already established stream (the
		 *     firewall rebooted, the state table was flushed, routes
		 *     changed...)
		 *  3) Packets get funky immediately after the connection
		 *     closes (this should catch Solaris spurious ACK|FINs
		 *     that web servers like to spew after a close)
		 *
		 * This must be a little more careful than the above code
		 * since packet floods will also be caught here. We don't
		 * update the TTL here to mitigate the damage of a packet
		 * flood and so the same code can handle awkward establishment
		 * and a loosened connection close.
		 * In the establishment case, a correct peer response will
		 * validate the connection, go through the normal state code
		 * and keep updating the state TTL.
		 */

		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: loose state match: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			addlog(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu dir=%s,%s\n", seq, orig_seq, ack,
			    pd->p_len, ackskew, (*state)->packets[0],
			    (*state)->packets[1],
			    pd->dir == PF_IN ? "in" : "out",
			    pd->dir == (*state)->direction ? "fwd" : "rev");
		}

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(pd, reason, *state, src,
			    dst, copyback))
				return (PF_DROP);
		}

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + (win << sws), dst->seqhi))
			dst->seqhi = ack + MAX((win << sws), 1);

		/*
		 * Cannot set dst->seqhi here since this could be a shotgunned
		 * SYN and not an already established connection.
		 */
		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				pf_set_protostate(*state, psrc, TCPS_CLOSING);
		if (th->th_flags & TH_RST)
			pf_set_protostate(*state, PF_PEER_BOTH, TCPS_TIME_WAIT);

		/* Fall through to PASS packet */
	} else {
		if ((*state)->dst.state == TCPS_SYN_SENT &&
		    (*state)->src.state == TCPS_SYN_SENT) {
			/* Send RST for state mismatches during handshake */
			if (!(th->th_flags & TH_RST))
				pf_send_tcp((*state)->rule.ptr, pd->af,
				    pd->dst, pd->src, th->th_dport,
				    th->th_sport, ntohl(th->th_ack), 0,
				    TH_RST, 0, 0,
				    (*state)->rule.ptr->return_ttl, 1, 0,
				    pd->rdomain);
			src->seqlo = 0;
			src->seqhi = 1;
			src->max_win = 1;
		} else if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: BAD state: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			addlog(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu dir=%s,%s\n",
			    seq, orig_seq, ack, pd->p_len, ackskew,
			    (*state)->packets[0], (*state)->packets[1],
			    pd->dir == PF_IN ? "in" : "out",
			    pd->dir == (*state)->direction ? "fwd" : "rev");
			addlog("pf: State failure on: %c %c %c %c | %c %c\n",
			    SEQ_GEQ(src->seqhi, data_end) ? ' ' : '1',
			    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) ?
			    ' ': '2',
			    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
			    (ackskew <= (MAXACKWINDOW << sws)) ? ' ' : '4',
			    SEQ_GEQ(src->seqhi + MAXACKWINDOW, data_end) ?
			    ' ' :'5',
			    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW) ?' ' :'6');
		}
		REASON_SET(reason, PFRES_BADSTATE);
		return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_tcp_track_sloppy(struct pf_pdesc *pd, struct pf_state **state,
    u_short *reason)
{
	struct tcphdr		*th = &pd->hdr.tcp;
	struct pf_state_peer	*src, *dst;
	u_int8_t		 psrc, pdst;

	if (pd->dir == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
		psrc = PF_PEER_SRC;
		pdst = PF_PEER_DST;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
		psrc = PF_PEER_DST;
		pdst = PF_PEER_SRC;
	}

	if (th->th_flags & TH_SYN)
		if (src->state < TCPS_SYN_SENT)
			pf_set_protostate(*state, psrc, TCPS_SYN_SENT);
	if (th->th_flags & TH_FIN)
		if (src->state < TCPS_CLOSING)
			pf_set_protostate(*state, psrc, TCPS_CLOSING);
	if (th->th_flags & TH_ACK) {
		if (dst->state == TCPS_SYN_SENT) {
			pf_set_protostate(*state, pdst, TCPS_ESTABLISHED);
			if (src->state == TCPS_ESTABLISHED &&
			    !SLIST_EMPTY(&(*state)->src_nodes) &&
			    pf_src_connlimit(state)) {
				REASON_SET(reason, PFRES_SRCLIMIT);
				return (PF_DROP);
			}
		} else if (dst->state == TCPS_CLOSING) {
			pf_set_protostate(*state, pdst, TCPS_FIN_WAIT_2);
		} else if (src->state == TCPS_SYN_SENT &&
		    dst->state < TCPS_SYN_SENT) {
			/*
			 * Handle a special sloppy case where we only see one
			 * half of the connection. If there is a ACK after
			 * the initial SYN without ever seeing a packet from
			 * the destination, set the connection to established.
			 */
			pf_set_protostate(*state, PF_PEER_BOTH,
			    TCPS_ESTABLISHED);
			if (!SLIST_EMPTY(&(*state)->src_nodes) &&
			    pf_src_connlimit(state)) {
				REASON_SET(reason, PFRES_SRCLIMIT);
				return (PF_DROP);
			}
		} else if (src->state == TCPS_CLOSING &&
		    dst->state == TCPS_ESTABLISHED &&
		    dst->seqlo == 0) {
			/*
			 * Handle the closing of half connections where we
			 * don't see the full bidirectional FIN/ACK+ACK
			 * handshake.
			 */
			pf_set_protostate(*state, pdst, TCPS_CLOSING);
		}
	}
	if (th->th_flags & TH_RST)
		pf_set_protostate(*state, PF_PEER_BOTH, TCPS_TIME_WAIT);

	/* update expire time */
	(*state)->expire = time_uptime;
	if (src->state >= TCPS_FIN_WAIT_2 &&
	    dst->state >= TCPS_FIN_WAIT_2)
		(*state)->timeout = PFTM_TCP_CLOSED;
	else if (src->state >= TCPS_CLOSING &&
	    dst->state >= TCPS_CLOSING)
		(*state)->timeout = PFTM_TCP_FIN_WAIT;
	else if (src->state < TCPS_ESTABLISHED ||
	    dst->state < TCPS_ESTABLISHED)
		(*state)->timeout = PFTM_TCP_OPENING;
	else if (src->state >= TCPS_CLOSING ||
	    dst->state >= TCPS_CLOSING)
		(*state)->timeout = PFTM_TCP_CLOSING;
	else
		(*state)->timeout = PFTM_TCP_ESTABLISHED;

	return (PF_PASS);
}

static __inline int
pf_synproxy(struct pf_pdesc *pd, struct pf_state **state, u_short *reason)
{
	struct pf_state_key	*sk = (*state)->key[pd->didx];

	if ((*state)->src.state == PF_TCPS_PROXY_SRC) {
		struct tcphdr	*th = &pd->hdr.tcp;

		if (pd->dir != (*state)->direction) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
		if (th->th_flags & TH_SYN) {
			if (ntohl(th->th_seq) != (*state)->src.seqlo) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return (PF_DROP);
			}
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
			    pd->src, th->th_dport, th->th_sport,
			    (*state)->src.seqhi, ntohl(th->th_seq) + 1,
			    TH_SYN|TH_ACK, 0, (*state)->src.mss, 0, 1,
			    0, pd->rdomain);
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		} else if ((th->th_flags & (TH_ACK|TH_RST|TH_FIN)) != TH_ACK ||
		    (ntohl(th->th_ack) != (*state)->src.seqhi + 1) ||
		    (ntohl(th->th_seq) != (*state)->src.seqlo + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_DROP);
		} else if (!SLIST_EMPTY(&(*state)->src_nodes) &&
		    pf_src_connlimit(state)) {
			REASON_SET(reason, PFRES_SRCLIMIT);
			return (PF_DROP);
		} else
			pf_set_protostate(*state, PF_PEER_SRC,
			    PF_TCPS_PROXY_DST);
	}
	if ((*state)->src.state == PF_TCPS_PROXY_DST) {
		struct tcphdr	*th = &pd->hdr.tcp;

		if (pd->dir == (*state)->direction) {
			if (((th->th_flags & (TH_SYN|TH_ACK)) != TH_ACK) ||
			    (ntohl(th->th_ack) != (*state)->src.seqhi + 1) ||
			    (ntohl(th->th_seq) != (*state)->src.seqlo + 1)) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return (PF_DROP);
			}
			(*state)->src.max_win = MAX(ntohs(th->th_win), 1);
			if ((*state)->dst.seqhi == 1)
				(*state)->dst.seqhi = arc4random();
			pf_send_tcp((*state)->rule.ptr, pd->af,
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->dst.seqhi, 0, TH_SYN, 0,
			    (*state)->src.mss, 0, 0, (*state)->tag,
			    sk->rdomain);
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		} else if (((th->th_flags & (TH_SYN|TH_ACK)) !=
		    (TH_SYN|TH_ACK)) ||
		    (ntohl(th->th_ack) != (*state)->dst.seqhi + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_DROP);
		} else {
			(*state)->dst.max_win = MAX(ntohs(th->th_win), 1);
			(*state)->dst.seqlo = ntohl(th->th_seq);
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
			    pd->src, th->th_dport, th->th_sport,
			    ntohl(th->th_ack), ntohl(th->th_seq) + 1,
			    TH_ACK, (*state)->src.max_win, 0, 0, 0,
			    (*state)->tag, pd->rdomain);
			pf_send_tcp((*state)->rule.ptr, pd->af,
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->src.seqhi + 1, (*state)->src.seqlo + 1,
			    TH_ACK, (*state)->dst.max_win, 0, 0, 1,
			    0, sk->rdomain);
			(*state)->src.seqdiff = (*state)->dst.seqhi -
			    (*state)->src.seqlo;
			(*state)->dst.seqdiff = (*state)->src.seqhi -
			    (*state)->dst.seqlo;
			(*state)->src.seqhi = (*state)->src.seqlo +
			    (*state)->dst.max_win;
			(*state)->dst.seqhi = (*state)->dst.seqlo +
			    (*state)->src.max_win;
			(*state)->src.wscale = (*state)->dst.wscale = 0;
			pf_set_protostate(*state, PF_PEER_BOTH,
			    TCPS_ESTABLISHED);
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
	}
	return (PF_PASS);
}

int
pf_test_state(struct pf_pdesc *pd, struct pf_state **state, u_short *reason,
    int syncookie)
{
	struct pf_state_key_cmp	 key;
	int			 copyback = 0;
	struct pf_state_peer	*src, *dst;
	int			 action = PF_PASS;
	struct inpcb		*inp;
	u_int8_t		 psrc, pdst;

	key.af = pd->af;
	key.proto = pd->virtual_proto;
	key.rdomain = pd->rdomain;
	PF_ACPY(&key.addr[pd->sidx], pd->src, key.af);
	PF_ACPY(&key.addr[pd->didx], pd->dst, key.af);
	key.port[pd->sidx] = pd->osport;
	key.port[pd->didx] = pd->odport;
	inp = pd->m->m_pkthdr.pf.inp;

	STATE_LOOKUP(pd->kif, &key, pd->dir, *state, pd->m);

	if (pd->dir == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
		psrc = PF_PEER_SRC;
		pdst = PF_PEER_DST;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
		psrc = PF_PEER_DST;
		pdst = PF_PEER_SRC;
	}

	switch (pd->virtual_proto) {
	case IPPROTO_TCP:
		if (syncookie) {
			pf_set_protostate(*state, PF_PEER_SRC,
			    PF_TCPS_PROXY_DST);
			(*state)->dst.seqhi = ntohl(pd->hdr.tcp.th_ack) - 1;
		}
		if ((action = pf_synproxy(pd, state, reason)) != PF_PASS)
			return (action);
		if ((pd->hdr.tcp.th_flags & (TH_SYN|TH_ACK)) == TH_SYN) {

			if (dst->state >= TCPS_FIN_WAIT_2 &&
			    src->state >= TCPS_FIN_WAIT_2) {
				if (pf_status.debug >= LOG_NOTICE) {
					log(LOG_NOTICE, "pf: state reuse ");
					pf_print_state(*state);
					pf_print_flags(pd->hdr.tcp.th_flags);
					addlog("\n");
				}
				/* XXX make sure it's the same direction ?? */
				pf_remove_state(*state);
				*state = NULL;
				pd->m->m_pkthdr.pf.inp = inp;
				return (PF_DROP);
			} else if (dst->state >= TCPS_ESTABLISHED &&
			    src->state >= TCPS_ESTABLISHED) {
				/*
				 * SYN matches existing state???
				 * Typically happens when sender boots up after
				 * sudden panic. Certain protocols (NFSv3) are
				 * always using same port numbers. Challenge
				 * ACK enables all parties (firewall and peers)
				 * to get in sync again.
				 */
				pf_send_challenge_ack(pd, *state, src, dst);
				return (PF_DROP);
			}
		}

		if ((*state)->state_flags & PFSTATE_SLOPPY) {
			if (pf_tcp_track_sloppy(pd, state, reason) == PF_DROP)
				return (PF_DROP);
		} else {
			if (pf_tcp_track_full(pd, state, reason, &copyback,
			    PF_REVERSED_KEY((*state)->key, pd->af)) == PF_DROP)
				return (PF_DROP);
		}
		break;
	case IPPROTO_UDP:
		/* update states */
		if (src->state < PFUDPS_SINGLE)
			pf_set_protostate(*state, psrc, PFUDPS_SINGLE);
		if (dst->state == PFUDPS_SINGLE)
			pf_set_protostate(*state, pdst, PFUDPS_MULTIPLE);

		/* update expire time */
		(*state)->expire = time_uptime;
		if (src->state == PFUDPS_MULTIPLE &&
		    dst->state == PFUDPS_MULTIPLE)
			(*state)->timeout = PFTM_UDP_MULTIPLE;
		else
			(*state)->timeout = PFTM_UDP_SINGLE;
		break;
	default:
		/* update states */
		if (src->state < PFOTHERS_SINGLE)
			pf_set_protostate(*state, psrc, PFOTHERS_SINGLE);
		if (dst->state == PFOTHERS_SINGLE)
			pf_set_protostate(*state, pdst, PFOTHERS_MULTIPLE);

		/* update expire time */
		(*state)->expire = time_uptime;
		if (src->state == PFOTHERS_MULTIPLE &&
		    dst->state == PFOTHERS_MULTIPLE)
			(*state)->timeout = PFTM_OTHER_MULTIPLE;
		else
			(*state)->timeout = PFTM_OTHER_SINGLE;
		break;
	}

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key	*nk;
		int			 afto, sidx, didx;

		if (PF_REVERSED_KEY((*state)->key, pd->af))
			nk = (*state)->key[pd->sidx];
		else
			nk = (*state)->key[pd->didx];

		afto = pd->af != nk->af;
		sidx = afto ? pd->didx : pd->sidx;
		didx = afto ? pd->sidx : pd->didx;

#ifdef INET6
		if (afto) {
			PF_ACPY(&pd->nsaddr, &nk->addr[sidx], nk->af);
			PF_ACPY(&pd->ndaddr, &nk->addr[didx], nk->af);
			pd->naf = nk->af;
			action = PF_AFRT;
		}
#endif /* INET6 */

		if (!afto)
			pf_translate_a(pd, pd->src, &nk->addr[sidx]);

		if (pd->sport != NULL)
			pf_patch_16(pd, pd->sport, nk->port[sidx]);

		if (afto || PF_ANEQ(pd->dst, &nk->addr[didx], pd->af) ||
		    pd->rdomain != nk->rdomain)
			pd->destchg = 1;

		if (!afto)
			pf_translate_a(pd, pd->dst, &nk->addr[didx]);

		if (pd->dport != NULL)
			pf_patch_16(pd, pd->dport, nk->port[didx]);

		pd->m->m_pkthdr.ph_rtableid = nk->rdomain;
		copyback = 1;
	}

	if (copyback && pd->hdrlen > 0) {
		m_copyback(pd->m, pd->off, pd->hdrlen, &pd->hdr, M_NOWAIT);
	}

	return (action);
}

int
pf_icmp_state_lookup(struct pf_pdesc *pd, struct pf_state_key_cmp *key,
    struct pf_state **state, u_int16_t icmpid, u_int16_t type,
    int icmp_dir, int *iidx, int multi, int inner)
{
	int direction;

	key->af = pd->af;
	key->proto = pd->proto;
	key->rdomain = pd->rdomain;
	if (icmp_dir == PF_IN) {
		*iidx = pd->sidx;
		key->port[pd->sidx] = icmpid;
		key->port[pd->didx] = type;
	} else {
		*iidx = pd->didx;
		key->port[pd->sidx] = type;
		key->port[pd->didx] = icmpid;
	}

	if (pf_state_key_addr_setup(pd, key, pd->sidx, pd->src, pd->didx,
	    pd->dst, pd->af, multi))
		return (PF_DROP);

	STATE_LOOKUP(pd->kif, key, pd->dir, *state, pd->m);

	if ((*state)->state_flags & PFSTATE_SLOPPY)
		return (-1);

	/* Is this ICMP message flowing in right direction? */
	if ((*state)->key[PF_SK_WIRE]->af != (*state)->key[PF_SK_STACK]->af)
		direction = (pd->af == (*state)->key[PF_SK_WIRE]->af) ?
		    PF_IN : PF_OUT;
	else
		direction = (*state)->direction;
	if ((((!inner && direction == pd->dir) ||
	    (inner && direction != pd->dir)) ?
	    PF_IN : PF_OUT) != icmp_dir) {
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE,
			    "pf: icmp type %d in wrong direction (%d): ",
			    ntohs(type), icmp_dir);
			pf_print_state(*state);
			addlog("\n");
		}
		return (PF_DROP);
	}
	return (-1);
}

int
pf_test_state_icmp(struct pf_pdesc *pd, struct pf_state **state,
    u_short *reason)
{
	u_int16_t	 virtual_id, virtual_type;
	u_int8_t	 icmptype;
	int		 icmp_dir, iidx, ret, copyback = 0;

	struct pf_state_key_cmp key;

	switch (pd->proto) {
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp.icmp_type;
		break;
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6.icmp6_type;
		break;
#endif /* INET6 */
	default:
		panic("unhandled proto %d", pd->proto);
	}

	if (pf_icmp_mapping(pd, icmptype, &icmp_dir, &virtual_id,
	    &virtual_type) == 0) {
		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		ret = pf_icmp_state_lookup(pd, &key, state,
		    virtual_id, virtual_type, icmp_dir, &iidx,
		    0, 0);
		/* IPv6? try matching a multicast address */
		if (ret == PF_DROP && pd->af == AF_INET6 && icmp_dir == PF_OUT)
			ret = pf_icmp_state_lookup(pd, &key, state, virtual_id,
			    virtual_type, icmp_dir, &iidx, 1, 0);
		if (ret >= 0)
			return (ret);

		(*state)->expire = time_uptime;
		(*state)->timeout = PFTM_ICMP_ERROR_REPLY;

		/* translate source/destination address, if necessary */
		if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
			struct pf_state_key	*nk;
			int			 afto, sidx, didx;

			if (PF_REVERSED_KEY((*state)->key, pd->af))
				nk = (*state)->key[pd->sidx];
			else
				nk = (*state)->key[pd->didx];

			afto = pd->af != nk->af;
			sidx = afto ? pd->didx : pd->sidx;
			didx = afto ? pd->sidx : pd->didx;
			iidx = afto ? !iidx : iidx;
#ifdef	INET6
			if (afto) {
				PF_ACPY(&pd->nsaddr, &nk->addr[sidx], nk->af);
				PF_ACPY(&pd->ndaddr, &nk->addr[didx], nk->af);
				pd->naf = nk->af;
			}
#endif /* INET6 */
			if (!afto) {
				pf_translate_a(pd, pd->src, &nk->addr[sidx]);
				pf_translate_a(pd, pd->dst, &nk->addr[didx]);
			}

			if (pd->rdomain != nk->rdomain)
				pd->destchg = 1;
			if (!afto && PF_ANEQ(pd->dst,
				&nk->addr[didx], pd->af))
				pd->destchg = 1;
			pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

			switch (pd->af) {
			case AF_INET:
#ifdef INET6
				if (afto) {
					if (pf_translate_icmp_af(pd, AF_INET6,
					    &pd->hdr.icmp))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMPV6;
				}
#endif /* INET6 */
				pf_patch_16(pd,
				    &pd->hdr.icmp.icmp_id, nk->port[iidx]);

				m_copyback(pd->m, pd->off, ICMP_MINLEN,
				    &pd->hdr.icmp, M_NOWAIT);
				copyback = 1;
				break;
#ifdef INET6
			case AF_INET6:
				if (afto) {
					if (pf_translate_icmp_af(pd, AF_INET,
					    &pd->hdr.icmp6))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMP;
				}

				pf_patch_16(pd,
				    &pd->hdr.icmp6.icmp6_id, nk->port[iidx]);

				m_copyback(pd->m, pd->off,
				    sizeof(struct icmp6_hdr), &pd->hdr.icmp6,
				    M_NOWAIT);
				copyback = 1;
				break;
#endif /* INET6 */
			}
#ifdef	INET6
			if (afto)
				return (PF_AFRT);
#endif /* INET6 */
		}
	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */
		struct pf_pdesc	 pd2;
		struct ip	 h2;
#ifdef INET6
		struct ip6_hdr	 h2_6;
#endif /* INET6 */
		int		 ipoff2;

		/* Initialize pd2 fields valid for both packets with pd. */
		memset(&pd2, 0, sizeof(pd2));
		pd2.af = pd->af;
		pd2.dir = pd->dir;
		pd2.kif = pd->kif;
		pd2.m = pd->m;
		pd2.rdomain = pd->rdomain;
		/* Payload packet is from the opposite direction. */
		pd2.sidx = (pd2.dir == PF_IN) ? 1 : 0;
		pd2.didx = (pd2.dir == PF_IN) ? 0 : 1;
		switch (pd->af) {
		case AF_INET:
			/* offset of h2 in mbuf chain */
			ipoff2 = pd->off + ICMP_MINLEN;

			if (!pf_pull_hdr(pd2.m, ipoff2, &h2, sizeof(h2),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (ip)");
				return (PF_DROP);
			}
			/*
			 * ICMP error messages don't refer to non-first
			 * fragments
			 */
			if (h2.ip_off & htons(IP_OFFMASK)) {
				REASON_SET(reason, PFRES_FRAG);
				return (PF_DROP);
			}

			/* offset of protocol header that follows h2 */
			pd2.off = ipoff2;
			if (pf_walk_header(&pd2, &h2, reason) != PF_PASS)
				return (PF_DROP);

			pd2.tot_len = ntohs(h2.ip_len);
			pd2.src = (struct pf_addr *)&h2.ip_src;
			pd2.dst = (struct pf_addr *)&h2.ip_dst;
			break;
#ifdef INET6
		case AF_INET6:
			ipoff2 = pd->off + sizeof(struct icmp6_hdr);

			if (!pf_pull_hdr(pd2.m, ipoff2, &h2_6, sizeof(h2_6),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (ip6)");
				return (PF_DROP);
			}

			pd2.off = ipoff2;
			if (pf_walk_header6(&pd2, &h2_6, reason) != PF_PASS)
				return (PF_DROP);

			pd2.tot_len = ntohs(h2_6.ip6_plen) +
			    sizeof(struct ip6_hdr);
			pd2.src = (struct pf_addr *)&h2_6.ip6_src;
			pd2.dst = (struct pf_addr *)&h2_6.ip6_dst;
			break;
#endif /* INET6 */
		default:
			unhandled_af(pd->af);
		}

		switch (pd2.proto) {
		case IPPROTO_TCP: {
			struct tcphdr		*th = &pd2.hdr.tcp;
			u_int32_t		 seq;
			struct pf_state_peer	*src, *dst;
			u_int8_t		 dws;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(pd2.m, pd2.off, th, 8, NULL, reason,
			    pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (tcp)");
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_TCP;
			key.rdomain = pd2.rdomain;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[pd2.sidx] = th->th_sport;
			key.port[pd2.didx] = th->th_dport;

			STATE_LOOKUP(pd2.kif, &key, pd2.dir, *state, pd2.m);

			if (pd2.dir == (*state)->direction) {
				if (PF_REVERSED_KEY((*state)->key, pd->af)) {
					src = &(*state)->src;
					dst = &(*state)->dst;
				} else {
					src = &(*state)->dst;
					dst = &(*state)->src;
				}
			} else {
				if (PF_REVERSED_KEY((*state)->key, pd->af)) {
					src = &(*state)->dst;
					dst = &(*state)->src;
				} else {
					src = &(*state)->src;
					dst = &(*state)->dst;
				}
			}

			if (src->wscale && dst->wscale)
				dws = dst->wscale & PF_WSCALE_MASK;
			else
				dws = 0;

			/* Demodulate sequence number */
			seq = ntohl(th->th_seq) - src->seqdiff;
			if (src->seqdiff) {
				pf_patch_32(pd, &th->th_seq, htonl(seq));
				copyback = 1;
			}

			if (!((*state)->state_flags & PFSTATE_SLOPPY) &&
			    (!SEQ_GEQ(src->seqhi, seq) || !SEQ_GEQ(seq,
			    src->seqlo - (dst->max_win << dws)))) {
				if (pf_status.debug >= LOG_NOTICE) {
					log(LOG_NOTICE,
					    "pf: BAD ICMP %d:%d ",
					    icmptype, pd->hdr.icmp.icmp_code);
					pf_print_host(pd->src, 0, pd->af);
					addlog(" -> ");
					pf_print_host(pd->dst, 0, pd->af);
					addlog(" state: ");
					pf_print_state(*state);
					addlog(" seq=%u\n", seq);
				}
				REASON_SET(reason, PFRES_BADSTATE);
				return (PF_DROP);
			} else {
				if (pf_status.debug >= LOG_DEBUG) {
					log(LOG_DEBUG,
					    "pf: OK ICMP %d:%d ",
					    icmptype, pd->hdr.icmp.icmp_code);
					pf_print_host(pd->src, 0, pd->af);
					addlog(" -> ");
					pf_print_host(pd->dst, 0, pd->af);
					addlog(" state: ");
					pf_print_state(*state);
					addlog(" seq=%u\n", seq);
				}
			}

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key	*nk;
				int			 afto, sidx, didx;

				if (PF_REVERSED_KEY((*state)->key, pd->af))
					nk = (*state)->key[pd->sidx];
				else
					nk = (*state)->key[pd->didx];

				afto = pd->af != nk->af;
				sidx = afto ? pd2.didx : pd2.sidx;
				didx = afto ? pd2.sidx : pd2.didx;

#ifdef INET6
				if (afto) {
					if (pf_translate_icmp_af(pd, nk->af,
					    &pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					if (nk->af == AF_INET)
						pd->proto = IPPROTO_ICMP;
					else
						pd->proto = IPPROTO_ICMPV6;
					pd->m->m_pkthdr.ph_rtableid =
					    nk->rdomain;
					pd->destchg = 1;
					PF_ACPY(&pd->nsaddr,
					    &nk->addr[pd2.sidx], nk->af);
					PF_ACPY(&pd->ndaddr,
					    &nk->addr[pd2.didx], nk->af);
					pd->naf = nk->af;

					pf_patch_16(pd,
					    &th->th_sport, nk->port[sidx]);
					pf_patch_16(pd,
					    &th->th_dport, nk->port[didx]);

					m_copyback(pd2.m, pd2.off, 8, th,
					    M_NOWAIT);
					return (PF_AFRT);
				}
#endif	/* INET6 */
				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != th->th_sport)
					pf_translate_icmp(pd, pd2.src,
					    &th->th_sport, pd->dst,
					    &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx]);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != th->th_dport)
					pf_translate_icmp(pd, pd2.dst,
					    &th->th_dport, pd->src,
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx]);
				copyback = 1;
			}

			if (copyback) {
				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    &pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				m_copyback(pd2.m, pd2.off, 8, th, M_NOWAIT);
			}
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr	*uh = &pd2.hdr.udp;

			if (!pf_pull_hdr(pd2.m, pd2.off, uh, sizeof(*uh),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (udp)");
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_UDP;
			key.rdomain = pd2.rdomain;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[pd2.sidx] = uh->uh_sport;
			key.port[pd2.didx] = uh->uh_dport;

			STATE_LOOKUP(pd2.kif, &key, pd2.dir, *state, pd2.m);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key	*nk;
				int			 afto, sidx, didx;

				if (PF_REVERSED_KEY((*state)->key, pd->af))
					nk = (*state)->key[pd->sidx];
				else
					nk = (*state)->key[pd->didx];

				afto = pd->af != nk->af;
				sidx = afto ? pd2.didx : pd2.sidx;
				didx = afto ? pd2.sidx : pd2.didx;

#ifdef INET6
				if (afto) {
					if (pf_translate_icmp_af(pd, nk->af,
					    &pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					if (nk->af == AF_INET)
						pd->proto = IPPROTO_ICMP;
					else
						pd->proto = IPPROTO_ICMPV6;
					pd->m->m_pkthdr.ph_rtableid =
					    nk->rdomain;
					pd->destchg = 1;
					PF_ACPY(&pd->nsaddr,
					    &nk->addr[pd2.sidx], nk->af);
					PF_ACPY(&pd->ndaddr,
					    &nk->addr[pd2.didx], nk->af);
					pd->naf = nk->af;

					pf_patch_16(pd,
					    &uh->uh_sport, nk->port[sidx]);
					pf_patch_16(pd,
					    &uh->uh_dport, nk->port[didx]);

					m_copyback(pd2.m, pd2.off, sizeof(*uh),
					    uh, M_NOWAIT);
					return (PF_AFRT);
				}
#endif /* INET6 */

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != uh->uh_sport)
					pf_translate_icmp(pd, pd2.src,
					    &uh->uh_sport, pd->dst,
					    &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx]);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != uh->uh_dport)
					pf_translate_icmp(pd, pd2.dst,
					    &uh->uh_dport, pd->src,
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx]);

				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    &pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				/* Avoid recomputing quoted UDP checksum.
				 * note: udp6 0 csum invalid per rfc2460 p27.
				 * but presumed nothing cares in this context */
				pf_patch_16(pd, &uh->uh_sum, 0);
				m_copyback(pd2.m, pd2.off, sizeof(*uh), uh,
				    M_NOWAIT);
				copyback = 1;
			}
			break;
		}
		case IPPROTO_ICMP: {
			struct icmp	*iih = &pd2.hdr.icmp;

			if (pd2.af != AF_INET) {
				REASON_SET(reason, PFRES_NORM);
				return (PF_DROP);
			}

			if (!pf_pull_hdr(pd2.m, pd2.off, iih, ICMP_MINLEN,
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp)");
				return (PF_DROP);
			}

			pf_icmp_mapping(&pd2, iih->icmp_type,
			    &icmp_dir, &virtual_id, &virtual_type);

			ret = pf_icmp_state_lookup(&pd2, &key, state,
			    virtual_id, virtual_type, icmp_dir, &iidx, 0, 1);
			if (ret >= 0)
				return (ret);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key	*nk;
				int			 afto, sidx, didx;

				if (PF_REVERSED_KEY((*state)->key, pd->af))
					nk = (*state)->key[pd->sidx];
				else
					nk = (*state)->key[pd->didx];

				afto = pd->af != nk->af;
				sidx = afto ? pd2.didx : pd2.sidx;
				didx = afto ? pd2.sidx : pd2.didx;
				iidx = afto ? !iidx : iidx;

#ifdef INET6
				if (afto) {
					if (nk->af != AF_INET6)
						return (PF_DROP);
					if (pf_translate_icmp_af(pd, nk->af,
					    &pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMPV6;
					if (pf_translate_icmp_af(pd,
						nk->af, iih))
						return (PF_DROP);
					if (virtual_type == htons(ICMP_ECHO))
						pf_patch_16(pd, &iih->icmp_id,
						    nk->port[iidx]);
					m_copyback(pd2.m, pd2.off, ICMP_MINLEN,
					    iih, M_NOWAIT);
					pd->m->m_pkthdr.ph_rtableid =
					    nk->rdomain;
					pd->destchg = 1;
					PF_ACPY(&pd->nsaddr,
					    &nk->addr[pd2.sidx], nk->af);
					PF_ACPY(&pd->ndaddr,
					    &nk->addr[pd2.didx], nk->af);
					pd->naf = nk->af;
					return (PF_AFRT);
				}
#endif /* INET6 */

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    (virtual_type == htons(ICMP_ECHO) &&
				    nk->port[iidx] != iih->icmp_id))
					pf_translate_icmp(pd, pd2.src,
					    (virtual_type == htons(ICMP_ECHO)) ?
					    &iih->icmp_id : NULL,
					    pd->dst, &nk->addr[pd2.sidx],
					    (virtual_type == htons(ICMP_ECHO)) ?
					    nk->port[iidx] : 0);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_translate_icmp(pd, pd2.dst, NULL,
					    pd->src, &nk->addr[pd2.didx], 0);

				m_copyback(pd->m, pd->off, ICMP_MINLEN,
				    &pd->hdr.icmp, M_NOWAIT);
				m_copyback(pd2.m, ipoff2, sizeof(h2), &h2,
				    M_NOWAIT);
				m_copyback(pd2.m, pd2.off, ICMP_MINLEN, iih,
				    M_NOWAIT);
				copyback = 1;
			}
			break;
		}
#ifdef INET6
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr	*iih = &pd2.hdr.icmp6;

			if (pd2.af != AF_INET6) {
				REASON_SET(reason, PFRES_NORM);
				return (PF_DROP);
			}

			if (!pf_pull_hdr(pd2.m, pd2.off, iih,
			    sizeof(struct icmp6_hdr), NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp6)");
				return (PF_DROP);
			}

			pf_icmp_mapping(&pd2, iih->icmp6_type,
			    &icmp_dir, &virtual_id, &virtual_type);
			ret = pf_icmp_state_lookup(&pd2, &key, state,
			    virtual_id, virtual_type, icmp_dir, &iidx, 0, 1);
			/* IPv6? try matching a multicast address */
			if (ret == PF_DROP && pd2.af == AF_INET6 &&
			    icmp_dir == PF_OUT)
				ret = pf_icmp_state_lookup(&pd2, &key, state,
				    virtual_id, virtual_type, icmp_dir, &iidx,
				    1, 1);
			if (ret >= 0)
				return (ret);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key	*nk;
				int			 afto, sidx, didx;

				if (PF_REVERSED_KEY((*state)->key, pd->af))
					nk = (*state)->key[pd->sidx];
				else
					nk = (*state)->key[pd->didx];

				afto = pd->af != nk->af;
				sidx = afto ? pd2.didx : pd2.sidx;
				didx = afto ? pd2.sidx : pd2.didx;
				iidx = afto ? !iidx : iidx;

				if (afto) {
					if (nk->af != AF_INET)
						return (PF_DROP);
					if (pf_translate_icmp_af(pd, nk->af,
					    &pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMP;
					if (pf_translate_icmp_af(pd,
						nk->af, iih))
						return (PF_DROP);
					if (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
						pf_patch_16(pd, &iih->icmp6_id,
						    nk->port[iidx]);
					m_copyback(pd2.m, pd2.off,
					    sizeof(struct icmp6_hdr), iih,
					    M_NOWAIT);
					pd->m->m_pkthdr.ph_rtableid =
					    nk->rdomain;
					pd->destchg = 1;
					PF_ACPY(&pd->nsaddr,
					    &nk->addr[pd2.sidx], nk->af);
					PF_ACPY(&pd->ndaddr,
					    &nk->addr[pd2.didx], nk->af);
					pd->naf = nk->af;
					return (PF_AFRT);
				}

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    ((virtual_type ==
				    htons(ICMP6_ECHO_REQUEST)) &&
				    nk->port[pd2.sidx] != iih->icmp6_id))
					pf_translate_icmp(pd, pd2.src,
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? &iih->icmp6_id : NULL,
					    pd->dst, &nk->addr[pd2.sidx],
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? nk->port[iidx] : 0);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_translate_icmp(pd, pd2.dst, NULL,
					    pd->src, &nk->addr[pd2.didx], 0);

				m_copyback(pd->m, pd->off,
				    sizeof(struct icmp6_hdr), &pd->hdr.icmp6,
				    M_NOWAIT);
				m_copyback(pd2.m, ipoff2, sizeof(h2_6), &h2_6,
				    M_NOWAIT);
				m_copyback(pd2.m, pd2.off,
				    sizeof(struct icmp6_hdr), iih, M_NOWAIT);
				copyback = 1;
			}
			break;
		}
#endif /* INET6 */
		default: {
			key.af = pd2.af;
			key.proto = pd2.proto;
			key.rdomain = pd2.rdomain;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[0] = key.port[1] = 0;

			STATE_LOOKUP(pd2.kif, &key, pd2.dir, *state, pd2.m);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af))
					pf_translate_icmp(pd, pd2.src, NULL,
					    pd->dst, &nk->addr[pd2.sidx], 0);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_translate_icmp(pd, pd2.dst, NULL,
					    pd->src, &nk->addr[pd2.didx], 0);

				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    &pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    &pd->hdr.icmp6, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				copyback = 1;
			}
			break;
		}
		}
	}
	if (copyback) {
		m_copyback(pd->m, pd->off, pd->hdrlen, &pd->hdr, M_NOWAIT);
	}

	return (PF_PASS);
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pf_pull_hdr(struct mbuf *m, int off, void *p, int len,
    u_short *actionp, u_short *reasonp, sa_family_t af)
{
	int iplen = 0;

	switch (af) {
	case AF_INET: {
		struct ip	*h = mtod(m, struct ip *);
		u_int16_t	 fragoff = (ntohs(h->ip_off) & IP_OFFMASK) << 3;

		if (fragoff) {
			if (fragoff >= len)
				ACTION_SET(actionp, PF_PASS);
			else {
				ACTION_SET(actionp, PF_DROP);
				REASON_SET(reasonp, PFRES_FRAG);
			}
			return (NULL);
		}
		iplen = ntohs(h->ip_len);
		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h = mtod(m, struct ip6_hdr *);

		iplen = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
		break;
	}
#endif /* INET6 */
	}
	if (m->m_pkthdr.len < off + len || iplen < off + len) {
		ACTION_SET(actionp, PF_DROP);
		REASON_SET(reasonp, PFRES_SHORT);
		return (NULL);
	}
	m_copydata(m, off, len, p);
	return (p);
}

int
pf_routable(struct pf_addr *addr, sa_family_t af, struct pfi_kif *kif,
    int rtableid)
{
	struct sockaddr_storage	 ss;
	struct sockaddr_in	*dst;
	int			 ret = 1;
	int			 check_mpath;
#ifdef INET6
	struct sockaddr_in6	*dst6;
#endif	/* INET6 */
	struct rtentry		*rt = NULL;

	check_mpath = 0;
	memset(&ss, 0, sizeof(ss));
	switch (af) {
	case AF_INET:
		dst = (struct sockaddr_in *)&ss;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		if (ipmultipath)
			check_mpath = 1;
		break;
#ifdef INET6
	case AF_INET6:
		/*
		 * Skip check for addresses with embedded interface scope,
		 * as they would always match anyway.
		 */
		if (IN6_IS_SCOPE_EMBED(&addr->v6))
			goto out;
		dst6 = (struct sockaddr_in6 *)&ss;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		if (ip6_multipath)
			check_mpath = 1;
		break;
#endif /* INET6 */
	}

	/* Skip checks for ipsec interfaces */
	if (kif != NULL && kif->pfik_ifp->if_type == IFT_ENC)
		goto out;

	rt = rtalloc(sstosa(&ss), 0, rtableid);
	if (rt != NULL) {
		/* No interface given, this is a no-route check */
		if (kif == NULL)
			goto out;

		if (kif->pfik_ifp == NULL) {
			ret = 0;
			goto out;
		}

		/* Perform uRPF check if passed input interface */
		ret = 0;
		do {
			if (rt->rt_ifidx == kif->pfik_ifp->if_index) {
				ret = 1;
#if NCARP > 0
			} else {
				struct ifnet	*ifp;

				ifp = if_get(rt->rt_ifidx);
				if (ifp != NULL && ifp->if_type == IFT_CARP &&
				    ifp->if_carpdev == kif->pfik_ifp)
					ret = 1;
				if_put(ifp);
#endif /* NCARP */
			}

			rt = rtable_iterate(rt);
		} while (check_mpath == 1 && rt != NULL && ret == 0);
	} else
		ret = 0;
out:
	rtfree(rt);
	return (ret);
}

int
pf_rtlabel_match(struct pf_addr *addr, sa_family_t af, struct pf_addr_wrap *aw,
    int rtableid)
{
	struct sockaddr_storage	 ss;
	struct sockaddr_in	*dst;
#ifdef INET6
	struct sockaddr_in6	*dst6;
#endif	/* INET6 */
	struct rtentry		*rt;
	int			 ret = 0;

	memset(&ss, 0, sizeof(ss));
	switch (af) {
	case AF_INET:
		dst = (struct sockaddr_in *)&ss;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		break;
#ifdef INET6
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ss;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		break;
#endif /* INET6 */
	}

	rt = rtalloc(sstosa(&ss), RT_RESOLVE, rtableid);
	if (rt != NULL) {
		if (rt->rt_labelid == aw->v.rtlabel)
			ret = 1;
		rtfree(rt);
	}

	return (ret);
}

/* pf_route() may change pd->m, adjust local copies after calling */
void
pf_route(struct pf_pdesc *pd, struct pf_rule *r, struct pf_state *s)
{
	struct mbuf		*m0, *m1;
	struct sockaddr_in	*dst, sin;
	struct rtentry		*rt = NULL;
	struct ip		*ip;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sns[PF_SN_MAX];
	int			 error = 0;
	unsigned int		 rtableid;

	if (pd->m->m_pkthdr.pf.routed++ > 3) {
		m_freem(pd->m);
		pd->m = NULL;
		return;
	}

	if (r->rt == PF_DUPTO) {
		if ((m0 = m_dup_pkt(pd->m, max_linkhdr, M_NOWAIT)) == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == pd->dir))
			return;
		m0 = pd->m;
	}

	if (m0->m_len < sizeof(struct ip)) {
		DPFPRINTF(LOG_ERR,
		    "%s: m0->m_len < sizeof(struct ip)", __func__);
		goto bad;
	}

	ip = mtod(m0, struct ip *);

	memset(&sin, 0, sizeof(sin));
	dst = &sin;
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;
	rtableid = m0->m_pkthdr.ph_rtableid;

	if (pd->dir == PF_IN) {
		if (ip->ip_ttl <= IPTTLDEC) {
			if (r->rt != PF_DUPTO)
				pf_send_icmp(m0, ICMP_TIMXCEED,
				    ICMP_TIMXCEED_INTRANS, 0,
				    pd->af, r, pd->rdomain);
			goto bad;
		}
		ip->ip_ttl -= IPTTLDEC;
	}

	if (s == NULL) {
		memset(sns, 0, sizeof(sns));
		if (pf_map_addr(AF_INET, r,
		    (struct pf_addr *)&ip->ip_src,
		    &naddr, NULL, sns, &r->route, PF_SN_ROUTE)) {
			DPFPRINTF(LOG_ERR,
			    "%s: pf_map_addr() failed", __func__);
			goto bad;
		}

		if (!PF_AZERO(&naddr, AF_INET))
			dst->sin_addr.s_addr = naddr.v4.s_addr;
		ifp = r->route.kif ?
		    r->route.kif->pfik_ifp : NULL;
	} else {
		if (!PF_AZERO(&s->rt_addr, AF_INET))
			dst->sin_addr.s_addr =
			    s->rt_addr.v4.s_addr;
		ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
	}
	if (ifp == NULL)
		goto bad;

	if (pd->kif->pfik_ifp != ifp) {
		if (pf_test(AF_INET, PF_OUT, ifp, &m0) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
		if (m0->m_len < sizeof(struct ip)) {
			DPFPRINTF(LOG_ERR,
			    "%s: m0->m_len < sizeof(struct ip)", __func__);
			goto bad;
		}
		ip = mtod(m0, struct ip *);
	}

	rt = rtalloc(sintosa(dst), RT_RESOLVE, rtableid);
	if (!rtisvalid(rt)) {
		ipstat_inc(ips_noroute);
		goto bad;
	}
	/* A locally generated packet may have invalid source address. */
	if ((ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		ip->ip_src = ifatoia(rt->rt_ifa)->ia_addr.sin_addr;

	in_proto_cksum_out(m0, ifp);

	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		ip->ip_sum = 0;
		if (ifp->if_capabilities & IFCAP_CSUM_IPv4)
			m0->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		else {
			ipstat_inc(ips_outswcsum);
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
		}
		error = ifp->if_output(ifp, m0, sintosa(dst), rt);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
		ipstat_inc(ips_cantfrag);
		if (r->rt != PF_DUPTO)
			pf_send_icmp(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG,
			    ifp->if_mtu, pd->af, r, pd->rdomain);
		goto bad;
	}

	m1 = m0;
	error = ip_fragment(m0, ifp, ifp->if_mtu);
	if (error) {
		m0 = NULL;
		goto bad;
	}

	for (m0 = m1; m0; m0 = m1) {
		m1 = m0->m_nextpkt;
		m0->m_nextpkt = 0;
		if (error == 0)
			error = ifp->if_output(ifp, m0, sintosa(dst), rt);
		else
			m_freem(m0);
	}

	if (error == 0)
		ipstat_inc(ips_fragmented);

done:
	if (r->rt != PF_DUPTO)
		pd->m = NULL;
	rtfree(rt);
	return;

bad:
	m_freem(m0);
	goto done;
}

#ifdef INET6
/* pf_route6() may change pd->m, adjust local copies after calling */
void
pf_route6(struct pf_pdesc *pd, struct pf_rule *r, struct pf_state *s)
{
	struct mbuf		*m0;
	struct sockaddr_in6	*dst, sin6;
	struct rtentry		*rt = NULL;
	struct ip6_hdr		*ip6;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sns[PF_SN_MAX];
	struct m_tag		*mtag;
	unsigned int		 rtableid;

	if (pd->m->m_pkthdr.pf.routed++ > 3) {
		m_freem(pd->m);
		pd->m = NULL;
		return;
	}

	if (r->rt == PF_DUPTO) {
		if ((m0 = m_dup_pkt(pd->m, max_linkhdr, M_NOWAIT)) == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == pd->dir))
			return;
		m0 = pd->m;
	}

	if (m0->m_len < sizeof(struct ip6_hdr)) {
		DPFPRINTF(LOG_ERR,
		    "%s: m0->m_len < sizeof(struct ip6_hdr)", __func__);
		goto bad;
	}
	ip6 = mtod(m0, struct ip6_hdr *);

	memset(&sin6, 0, sizeof(sin6));
	dst = &sin6;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;
	rtableid = m0->m_pkthdr.ph_rtableid;

	if (pd->dir == PF_IN) {
		if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
			if (r->rt != PF_DUPTO)
				pf_send_icmp(m0, ICMP6_TIME_EXCEEDED,
				    ICMP6_TIME_EXCEED_TRANSIT, 0,
				    pd->af, r, pd->rdomain);
			goto bad;
		}
		ip6->ip6_hlim -= IPV6_HLIMDEC;
	}

	if (s == NULL) {
		memset(sns, 0, sizeof(sns));
		if (pf_map_addr(AF_INET6, r, (struct pf_addr *)&ip6->ip6_src,
		    &naddr, NULL, sns, &r->route, PF_SN_ROUTE)) {
			DPFPRINTF(LOG_ERR,
			    "%s: pf_map_addr() failed", __func__);
			goto bad;
		}
		if (!PF_AZERO(&naddr, AF_INET6))
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &naddr, AF_INET6);
		ifp = r->route.kif ? r->route.kif->pfik_ifp : NULL;
	} else {
		if (!PF_AZERO(&s->rt_addr, AF_INET6))
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &s->rt_addr, AF_INET6);
		ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
	}
	if (ifp == NULL)
		goto bad;

	if (pd->kif->pfik_ifp != ifp) {
		if (pf_test(AF_INET6, PF_OUT, ifp, &m0) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
		if (m0->m_len < sizeof(struct ip6_hdr)) {
			DPFPRINTF(LOG_ERR,
			    "%s: m0->m_len < sizeof(struct ip6_hdr)", __func__);
			goto bad;
		}
	}

	if (IN6_IS_SCOPE_EMBED(&dst->sin6_addr))
		dst->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	rt = rtalloc(sin6tosa(dst), RT_RESOLVE, rtableid);
	if (!rtisvalid(rt)) {
		ip6stat_inc(ip6s_noroute);
		goto bad;
	}
	/* A locally generated packet may have invalid source address. */
	if (IN6_IS_ADDR_LOOPBACK(&ip6->ip6_src))
		ip6->ip6_src = ifatoia6(rt->rt_ifa)->ia_addr.sin6_addr;

	in6_proto_cksum_out(m0, ifp);

	/*
	 * If packet has been reassembled by PF earlier, we have to
	 * use pf_refragment6() here to turn it back to fragments.
	 */
	if ((mtag = m_tag_find(m0, PACKET_TAG_PF_REASSEMBLED, NULL))) {
		(void) pf_refragment6(&m0, mtag, dst, ifp, rt);
	} else if ((u_long)m0->m_pkthdr.len <= ifp->if_mtu) {
		ifp->if_output(ifp, m0, sin6tosa(dst), rt);
	} else {
		ip6stat_inc(ip6s_cantfrag);
		if (r->rt != PF_DUPTO)
			pf_send_icmp(m0, ICMP6_PACKET_TOO_BIG, 0,
			    ifp->if_mtu, pd->af, r, pd->rdomain);
		goto bad;
	}

done:
	if (r->rt != PF_DUPTO)
		pd->m = NULL;
	rtfree(rt);
	return;

bad:
	m_freem(m0);
	goto done;
}
#endif /* INET6 */


/*
 * check TCP checksum and set mbuf flag
 *   off is the offset where the protocol header starts
 *   len is the total length of protocol header plus payload
 * returns 0 when the checksum is valid, otherwise returns 1.
 * if the _OUT flag is set the checksum isn't done yet, consider these ok
 */
int
pf_check_tcp_cksum(struct mbuf *m, int off, int len, sa_family_t af)
{
	u_int16_t sum;

	if (m->m_pkthdr.csum_flags &
	    (M_TCP_CSUM_IN_OK | M_TCP_CSUM_OUT)) {
		return (0);
	}
	if (m->m_pkthdr.csum_flags & M_TCP_CSUM_IN_BAD ||
	    off < sizeof(struct ip) ||
	    m->m_pkthdr.len < off + len) {
		return (1);
	}

	/* need to do it in software */
	tcpstat_inc(tcps_inswcsum);

	switch (af) {
	case AF_INET:
		if (m->m_len < sizeof(struct ip))
			return (1);

		sum = in4_cksum(m, IPPROTO_TCP, off, len);
		break;
#ifdef INET6
	case AF_INET6:
		if (m->m_len < sizeof(struct ip6_hdr))
			return (1);

		sum = in6_cksum(m, IPPROTO_TCP, off, len);
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}
	if (sum) {
		tcpstat_inc(tcps_rcvbadsum);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_BAD;
		return (1);
	}

	m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK;
	return (0);
}

struct pf_divert *
pf_find_divert(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF_DIVERT, NULL)) == NULL)
		return (NULL);

	return ((struct pf_divert *)(mtag + 1));
}

struct pf_divert *
pf_get_divert(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF_DIVERT, NULL)) == NULL) {
		mtag = m_tag_get(PACKET_TAG_PF_DIVERT, sizeof(struct pf_divert),
		    M_NOWAIT);
		if (mtag == NULL)
			return (NULL);
		memset(mtag + 1, 0, sizeof(struct pf_divert));
		m_tag_prepend(m, mtag);
	}

	return ((struct pf_divert *)(mtag + 1));
}

int
pf_walk_header(struct pf_pdesc *pd, struct ip *h, u_short *reason)
{
	struct ip6_ext		 ext;
	u_int32_t		 hlen, end;
	int			 hdr_cnt;

	hlen = h->ip_hl << 2;
	if (hlen < sizeof(struct ip) || hlen > ntohs(h->ip_len)) {
		REASON_SET(reason, PFRES_SHORT);
		return (PF_DROP);
	}
	if (hlen != sizeof(struct ip))
		pd->badopts++;
	end = pd->off + ntohs(h->ip_len);
	pd->off += hlen;
	pd->proto = h->ip_p;
	/* stop walking over non initial fragments */
	if ((h->ip_off & htons(IP_OFFMASK)) != 0)
		return (PF_PASS);

	for (hdr_cnt = 0; hdr_cnt < pf_hdr_limit; hdr_cnt++) {
		switch (pd->proto) {
		case IPPROTO_AH:
			/* fragments may be short */
			if ((h->ip_off & htons(IP_MF | IP_OFFMASK)) != 0 &&
			    end < pd->off + sizeof(ext))
				return (PF_PASS);
			if (!pf_pull_hdr(pd->m, pd->off, &ext, sizeof(ext),
			    NULL, reason, AF_INET)) {
				DPFPRINTF(LOG_NOTICE, "IP short exthdr");
				return (PF_DROP);
			}
			pd->off += (ext.ip6e_len + 2) * 4;
			pd->proto = ext.ip6e_nxt;
			break;
		default:
			return (PF_PASS);
		}
	}
	DPFPRINTF(LOG_NOTICE, "IPv4 nested authentication header limit");
	REASON_SET(reason, PFRES_IPOPTIONS);
	return (PF_DROP);
}

#ifdef INET6
int
pf_walk_option6(struct pf_pdesc *pd, struct ip6_hdr *h, int off, int end,
    u_short *reason)
{
	struct ip6_opt		 opt;
	struct ip6_opt_jumbo	 jumbo;

	while (off < end) {
		if (!pf_pull_hdr(pd->m, off, &opt.ip6o_type,
		    sizeof(opt.ip6o_type), NULL, reason, AF_INET6)) {
			DPFPRINTF(LOG_NOTICE, "IPv6 short opt type");
			return (PF_DROP);
		}
		if (opt.ip6o_type == IP6OPT_PAD1) {
			off++;
			continue;
		}
		if (!pf_pull_hdr(pd->m, off, &opt, sizeof(opt),
		    NULL, reason, AF_INET6)) {
			DPFPRINTF(LOG_NOTICE, "IPv6 short opt");
			return (PF_DROP);
		}
		if (off + sizeof(opt) + opt.ip6o_len > end) {
			DPFPRINTF(LOG_NOTICE, "IPv6 long opt");
			REASON_SET(reason, PFRES_IPOPTIONS);
			return (PF_DROP);
		}
		switch (opt.ip6o_type) {
		case IP6OPT_JUMBO:
			if (pd->jumbolen != 0) {
				DPFPRINTF(LOG_NOTICE, "IPv6 multiple jumbo");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			if (ntohs(h->ip6_plen) != 0) {
				DPFPRINTF(LOG_NOTICE, "IPv6 bad jumbo plen");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			if (!pf_pull_hdr(pd->m, off, &jumbo, sizeof(jumbo),
			    NULL, reason, AF_INET6)) {
				DPFPRINTF(LOG_NOTICE, "IPv6 short jumbo");
				return (PF_DROP);
			}
			memcpy(&pd->jumbolen, jumbo.ip6oj_jumbo_len,
			    sizeof(pd->jumbolen));
			pd->jumbolen = ntohl(pd->jumbolen);
			if (pd->jumbolen < IPV6_MAXPACKET) {
				DPFPRINTF(LOG_NOTICE, "IPv6 short jumbolen");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			break;
		default:
			break;
		}
		off += sizeof(opt) + opt.ip6o_len;
	}

	return (PF_PASS);
}

int
pf_walk_header6(struct pf_pdesc *pd, struct ip6_hdr *h, u_short *reason)
{
	struct ip6_frag		 frag;
	struct ip6_ext		 ext;
	struct ip6_rthdr	 rthdr;
	u_int32_t		 end;
	int			 hdr_cnt, fraghdr_cnt = 0, rthdr_cnt = 0;

	pd->off += sizeof(struct ip6_hdr);
	end = pd->off + ntohs(h->ip6_plen);
	pd->fragoff = pd->extoff = pd->jumbolen = 0;
	pd->proto = h->ip6_nxt;

	for (hdr_cnt = 0; hdr_cnt < pf_hdr_limit; hdr_cnt++) {
		switch (pd->proto) {
		case IPPROTO_ROUTING:
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS:
			pd->badopts++;
			break;
		}
		switch (pd->proto) {
		case IPPROTO_FRAGMENT:
			if (fraghdr_cnt++) {
				DPFPRINTF(LOG_NOTICE, "IPv6 multiple fragment");
				REASON_SET(reason, PFRES_FRAG);
				return (PF_DROP);
			}
			/* jumbo payload packets cannot be fragmented */
			if (pd->jumbolen != 0) {
				DPFPRINTF(LOG_NOTICE, "IPv6 fragmented jumbo");
				REASON_SET(reason, PFRES_FRAG);
				return (PF_DROP);
			}
			if (!pf_pull_hdr(pd->m, pd->off, &frag, sizeof(frag),
			    NULL, reason, AF_INET6)) {
				DPFPRINTF(LOG_NOTICE, "IPv6 short fragment");
				return (PF_DROP);
			}
			/* stop walking over non initial fragments */
			if (ntohs((frag.ip6f_offlg & IP6F_OFF_MASK)) != 0) {
				pd->fragoff = pd->off;
				return (PF_PASS);
			}
			/* RFC6946:  reassemble only non atomic fragments */
			if (frag.ip6f_offlg & IP6F_MORE_FRAG)
				pd->fragoff = pd->off;
			pd->off += sizeof(frag);
			pd->proto = frag.ip6f_nxt;
			break;
		case IPPROTO_ROUTING:
			if (rthdr_cnt++) {
				DPFPRINTF(LOG_NOTICE, "IPv6 multiple rthdr");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			/* fragments may be short */
			if (pd->fragoff != 0 && end < pd->off + sizeof(rthdr)) {
				pd->off = pd->fragoff;
				pd->proto = IPPROTO_FRAGMENT;
				return (PF_PASS);
			}
			if (!pf_pull_hdr(pd->m, pd->off, &rthdr, sizeof(rthdr),
			    NULL, reason, AF_INET6)) {
				DPFPRINTF(LOG_NOTICE, "IPv6 short rthdr");
				return (PF_DROP);
			}
			if (rthdr.ip6r_type == IPV6_RTHDR_TYPE_0) {
				DPFPRINTF(LOG_NOTICE, "IPv6 rthdr0");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			/* FALLTHROUGH */
		case IPPROTO_HOPOPTS:
			/* RFC2460 4.1:  Hop-by-Hop only after IPv6 header */
			if (pd->proto == IPPROTO_HOPOPTS && hdr_cnt > 0) {
				DPFPRINTF(LOG_NOTICE, "IPv6 hopopts not first");
				REASON_SET(reason, PFRES_IPOPTIONS);
				return (PF_DROP);
			}
			/* FALLTHROUGH */
		case IPPROTO_AH:
		case IPPROTO_DSTOPTS:
			/* fragments may be short */
			if (pd->fragoff != 0 && end < pd->off + sizeof(ext)) {
				pd->off = pd->fragoff;
				pd->proto = IPPROTO_FRAGMENT;
				return (PF_PASS);
			}
			if (!pf_pull_hdr(pd->m, pd->off, &ext, sizeof(ext),
			    NULL, reason, AF_INET6)) {
				DPFPRINTF(LOG_NOTICE, "IPv6 short exthdr");
				return (PF_DROP);
			}
			/* reassembly needs the ext header before the frag */
			if (pd->fragoff == 0)
				pd->extoff = pd->off;
			if (pd->proto == IPPROTO_HOPOPTS && pd->fragoff == 0) {
				if (pf_walk_option6(pd, h,
				    pd->off + sizeof(ext),
				    pd->off + (ext.ip6e_len + 1) * 8, reason)
				    != PF_PASS)
					return (PF_DROP);
				if (ntohs(h->ip6_plen) == 0 &&
				    pd->jumbolen != 0) {
					DPFPRINTF(LOG_NOTICE,
					    "IPv6 missing jumbo");
					REASON_SET(reason, PFRES_IPOPTIONS);
					return (PF_DROP);
				}
			}
			if (pd->proto == IPPROTO_AH)
				pd->off += (ext.ip6e_len + 2) * 4;
			else
				pd->off += (ext.ip6e_len + 1) * 8;
			pd->proto = ext.ip6e_nxt;
			break;
		case IPPROTO_TCP:
		case IPPROTO_UDP:
		case IPPROTO_ICMPV6:
			/* fragments may be short, ignore inner header then */
			if (pd->fragoff != 0 && end < pd->off +
			    (pd->proto == IPPROTO_TCP ? sizeof(struct tcphdr) :
			    pd->proto == IPPROTO_UDP ? sizeof(struct udphdr) :
			    sizeof(struct icmp6_hdr))) {
				pd->off = pd->fragoff;
				pd->proto = IPPROTO_FRAGMENT;
			}
			/* FALLTHROUGH */
		default:
			return (PF_PASS);
		}
	}
	DPFPRINTF(LOG_NOTICE, "IPv6 nested extension header limit");
	REASON_SET(reason, PFRES_IPOPTIONS);
	return (PF_DROP);
}
#endif /* INET6 */

int
pf_setup_pdesc(struct pf_pdesc *pd, sa_family_t af, int dir,
    struct pfi_kif *kif, struct mbuf *m, u_short *reason)
{
	memset(pd, 0, sizeof(*pd));
	pd->dir = dir;
	pd->kif = kif;		/* kif is NULL when called by pflog */
	pd->m = m;
	pd->sidx = (dir == PF_IN) ? 0 : 1;
	pd->didx = (dir == PF_IN) ? 1 : 0;
	pd->af = pd->naf = af;
	pd->rdomain = rtable_l2(pd->m->m_pkthdr.ph_rtableid);

	switch (pd->af) {
	case AF_INET: {
		struct ip	*h;

		/* Check for illegal packets */
		if (pd->m->m_pkthdr.len < (int)sizeof(struct ip)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}

		h = mtod(pd->m, struct ip *);
		if (pd->m->m_pkthdr.len < ntohs(h->ip_len)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}

		if (pf_walk_header(pd, h, reason) != PF_PASS)
			return (PF_DROP);

		pd->src = (struct pf_addr *)&h->ip_src;
		pd->dst = (struct pf_addr *)&h->ip_dst;
		pd->tot_len = ntohs(h->ip_len);
		pd->tos = h->ip_tos & ~IPTOS_ECN_MASK;
		pd->ttl = h->ip_ttl;
		pd->virtual_proto = (h->ip_off & htons(IP_MF | IP_OFFMASK)) ?
		     PF_VPROTO_FRAGMENT : pd->proto;

		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h;

		/* Check for illegal packets */
		if (pd->m->m_pkthdr.len < (int)sizeof(struct ip6_hdr)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}

		h = mtod(pd->m, struct ip6_hdr *);
		if (pd->m->m_pkthdr.len <
		    sizeof(struct ip6_hdr) + ntohs(h->ip6_plen)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}

		if (pf_walk_header6(pd, h, reason) != PF_PASS)
			return (PF_DROP);

#if 1
		/*
		 * we do not support jumbogram yet.  if we keep going, zero
		 * ip6_plen will do something bad, so drop the packet for now.
		 */
		if (pd->jumbolen != 0) {
			REASON_SET(reason, PFRES_NORM);
			return (PF_DROP);
		}
#endif	/* 1 */

		pd->src = (struct pf_addr *)&h->ip6_src;
		pd->dst = (struct pf_addr *)&h->ip6_dst;
		pd->tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
		pd->tos = (ntohl(h->ip6_flow) & 0x0fc00000) >> 20;
		pd->ttl = h->ip6_hlim;
		pd->virtual_proto = (pd->fragoff != 0) ?
			PF_VPROTO_FRAGMENT : pd->proto;

		break;
	}
#endif /* INET6 */
	default:
		panic("pf_setup_pdesc called with illegal af %u", pd->af);

	}

	PF_ACPY(&pd->nsaddr, pd->src, pd->af);
	PF_ACPY(&pd->ndaddr, pd->dst, pd->af);

	switch (pd->virtual_proto) {
	case IPPROTO_TCP: {
		struct tcphdr	*th = &pd->hdr.tcp;

		if (!pf_pull_hdr(pd->m, pd->off, th, sizeof(*th),
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = sizeof(*th);
		if (pd->off + (th->th_off << 2) > pd->tot_len ||
		    (th->th_off << 2) < sizeof(struct tcphdr)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
		pd->p_len = pd->tot_len - pd->off - (th->th_off << 2);
		pd->sport = &th->th_sport;
		pd->dport = &th->th_dport;
		pd->pcksum = &th->th_sum;
		break;
	}
	case IPPROTO_UDP: {
		struct udphdr	*uh = &pd->hdr.udp;

		if (!pf_pull_hdr(pd->m, pd->off, uh, sizeof(*uh),
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = sizeof(*uh);
		if (uh->uh_dport == 0 ||
		    pd->off + ntohs(uh->uh_ulen) > pd->tot_len ||
		    ntohs(uh->uh_ulen) < sizeof(struct udphdr)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
		pd->sport = &uh->uh_sport;
		pd->dport = &uh->uh_dport;
		pd->pcksum = &uh->uh_sum;
		break;
	}
	case IPPROTO_ICMP: {
		if (!pf_pull_hdr(pd->m, pd->off, &pd->hdr.icmp, ICMP_MINLEN,
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = ICMP_MINLEN;
		if (pd->off + pd->hdrlen > pd->tot_len) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
		pd->pcksum = &pd->hdr.icmp.icmp_cksum;
		break;
	}
#ifdef INET6
	case IPPROTO_ICMPV6: {
		size_t	icmp_hlen = sizeof(struct icmp6_hdr);

		if (!pf_pull_hdr(pd->m, pd->off, &pd->hdr.icmp6, icmp_hlen,
		    NULL, reason, pd->af))
			return (PF_DROP);
		/* ICMP headers we look further into to match state */
		switch (pd->hdr.icmp6.icmp6_type) {
		case MLD_LISTENER_QUERY:
		case MLD_LISTENER_REPORT:
			icmp_hlen = sizeof(struct mld_hdr);
			break;
		case ND_NEIGHBOR_SOLICIT:
		case ND_NEIGHBOR_ADVERT:
			icmp_hlen = sizeof(struct nd_neighbor_solicit);
			/* FALLTHROUGH */
		case ND_ROUTER_SOLICIT:
		case ND_ROUTER_ADVERT:
		case ND_REDIRECT:
			if (pd->ttl != 255) {
				REASON_SET(reason, PFRES_NORM);
				return (PF_DROP);
			}
			break;
		}
		if (icmp_hlen > sizeof(struct icmp6_hdr) &&
		    !pf_pull_hdr(pd->m, pd->off, &pd->hdr.icmp6, icmp_hlen,
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = icmp_hlen;
		if (pd->off + pd->hdrlen > pd->tot_len) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
		pd->pcksum = &pd->hdr.icmp6.icmp6_cksum;
		break;
	}
#endif	/* INET6 */
	}

	if (pd->sport)
		pd->osport = pd->nsport = *pd->sport;
	if (pd->dport)
		pd->odport = pd->ndport = *pd->dport;

	return (PF_PASS);
}

void
pf_counters_inc(int action, struct pf_pdesc *pd, struct pf_state *s,
    struct pf_rule *r, struct pf_rule *a)
{
	int dirndx;
	pd->kif->pfik_bytes[pd->af == AF_INET6][pd->dir == PF_OUT]
	    [action != PF_PASS] += pd->tot_len;
	pd->kif->pfik_packets[pd->af == AF_INET6][pd->dir == PF_OUT]
	    [action != PF_PASS]++;

	if (action == PF_PASS || action == PF_AFRT || r->action == PF_DROP) {
		dirndx = (pd->dir == PF_OUT);
		r->packets[dirndx]++;
		r->bytes[dirndx] += pd->tot_len;
		if (a != NULL) {
			a->packets[dirndx]++;
			a->bytes[dirndx] += pd->tot_len;
		}
		if (s != NULL) {
			struct pf_rule_item	*ri;
			struct pf_sn_item	*sni;

			SLIST_FOREACH(sni, &s->src_nodes, next) {
				sni->sn->packets[dirndx]++;
				sni->sn->bytes[dirndx] += pd->tot_len;
			}
			dirndx = (pd->dir == s->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd->tot_len;

			SLIST_FOREACH(ri, &s->match_rules, entry) {
				ri->r->packets[dirndx]++;
				ri->r->bytes[dirndx] += pd->tot_len;

				if (ri->r->src.addr.type == PF_ADDR_TABLE)
					pfr_update_stats(ri->r->src.addr.p.tbl,
					    &s->key[(s->direction == PF_IN)]->
						addr[(s->direction == PF_OUT)],
					    pd, ri->r->action, ri->r->src.neg);
				if (ri->r->dst.addr.type == PF_ADDR_TABLE)
					pfr_update_stats(ri->r->dst.addr.p.tbl,
					    &s->key[(s->direction == PF_IN)]->
						addr[(s->direction == PF_IN)],
					    pd, ri->r->action, ri->r->dst.neg);
			}
		}
		if (r->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(r->src.addr.p.tbl,
			    (s == NULL) ? pd->src :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_OUT)],
			    pd, r->action, r->src.neg);
		if (r->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(r->dst.addr.p.tbl,
			    (s == NULL) ? pd->dst :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_IN)],
			    pd, r->action, r->dst.neg);
	}
}

int
pf_test(sa_family_t af, int fwdir, struct ifnet *ifp, struct mbuf **m0)
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0;
	struct pf_rule		*a = NULL, *r = &pf_default_rule;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	int			 dir = (fwdir == PF_FWD) ? PF_OUT : fwdir;
	u_int32_t		 qid, pqid = 0;

	if (!pf_status.running)
		return (PF_PASS);

#if NCARP > 0
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
#endif /* NCARP */
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(LOG_ERR,
		    "%s: kif == NULL, if_xname %s", __func__, ifp->if_xname);
		return (PF_DROP);
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP)
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if (((*m0)->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif /* DIAGNOSTIC */

	if ((*m0)->m_pkthdr.pf.flags & PF_TAG_GENERATED)
		return (PF_PASS);

	if ((*m0)->m_pkthdr.pf.flags & PF_TAG_DIVERTED_PACKET)
		return (PF_PASS);

	if ((*m0)->m_pkthdr.pf.flags & PF_TAG_REFRAGMENTED) {
		(*m0)->m_pkthdr.pf.flags &= ~PF_TAG_REFRAGMENTED;
		return (PF_PASS);
	}

	action = pf_setup_pdesc(&pd, af, dir, kif, *m0, &reason);
	if (action != PF_PASS) {
#if NPFLOG > 0
		pd.pflog |= PF_LOG_FORCE;
#endif	/* NPFLOG > 0 */
		goto done;
	}

	/* packet normalization and reassembly */
	switch (pd.af) {
	case AF_INET:
		action = pf_normalize_ip(&pd, &reason);
		break;
#ifdef INET6
	case AF_INET6:
		action = pf_normalize_ip6(&pd, &reason);
		break;
#endif	/* INET6 */
	}
	*m0 = pd.m;
	/* if packet sits in reassembly queue, return without error */
	if (pd.m == NULL)
		return PF_PASS;

	if (action != PF_PASS) {
#if NPFLOG > 0
		pd.pflog |= PF_LOG_FORCE;
#endif	/* NPFLOG > 0 */
		goto done;
	}

	/* if packet has been reassembled, update packet description */
	if (pf_status.reass && pd.virtual_proto == PF_VPROTO_FRAGMENT) {
		action = pf_setup_pdesc(&pd, af, dir, kif, pd.m, &reason);
		if (action != PF_PASS) {
#if NPFLOG > 0
			pd.pflog |= PF_LOG_FORCE;
#endif	/* NPFLOG > 0 */
			goto done;
		}
	}
	pd.m->m_pkthdr.pf.flags |= PF_TAG_PROCESSED;

	/*
	 * Avoid pcb-lookups from the forwarding path.  They should never
	 * match and would cause MP locking problems.
	 */
	if (fwdir == PF_FWD) {
		pd.lookup.done = -1;
		pd.lookup.uid = UID_MAX;
		pd.lookup.gid = GID_MAX;
		pd.lookup.pid = NO_PID;
	}

	/* lock the lookup/write section of pf_test() */
	PF_LOCK();

	switch (pd.virtual_proto) {

	case PF_VPROTO_FRAGMENT: {
		/*
		 * handle fragments that aren't reassembled by
		 * normalization
		 */
		action = pf_test_rule(&pd, &r, &s, &a, &ruleset, &reason);
		if (action != PF_PASS)
			REASON_SET(&reason, PFRES_FRAG);
		break;
	}

	case IPPROTO_ICMP: {
		if (pd.af != AF_INET) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_NORM);
			DPFPRINTF(LOG_NOTICE,
			    "dropping IPv6 packet with ICMPv4 payload");
			goto unlock;
		}
		action = pf_test_state_icmp(&pd, &s, &reason);
		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC > 0 */
			r = s->rule.ptr;
			a = s->anchor.ptr;
#if NPFLOG > 0
			pd.pflog |= s->log;
#endif	/* NPFLOG > 0 */
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset,
			    &reason);
		break;
	}

#ifdef INET6
	case IPPROTO_ICMPV6: {
		if (pd.af != AF_INET6) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_NORM);
			DPFPRINTF(LOG_NOTICE,
			    "dropping IPv4 packet with ICMPv6 payload");
			goto unlock;
		}
		action = pf_test_state_icmp(&pd, &s, &reason);
		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC > 0 */
			r = s->rule.ptr;
			a = s->anchor.ptr;
#if NPFLOG > 0
			pd.pflog |= s->log;
#endif	/* NPFLOG > 0 */
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset,
			    &reason);
		break;
	}
#endif /* INET6 */

	default:
		if (pd.virtual_proto == IPPROTO_TCP) {
			if (pd.dir == PF_IN && (pd.hdr.tcp.th_flags &
			    (TH_SYN|TH_ACK)) == TH_SYN &&
			    pf_synflood_check(&pd)) {
				pf_syncookie_send(&pd);
				action = PF_DROP;
				goto done;
			}
			if ((pd.hdr.tcp.th_flags & TH_ACK) && pd.p_len == 0)
				pqid = 1;
			action = pf_normalize_tcp(&pd);
			if (action == PF_DROP)
				goto unlock;
		}
		action = pf_test_state(&pd, &s, &reason, 0);
		if (s == NULL && action != PF_PASS && action != PF_AFRT &&
		    pd.dir == PF_IN && pd.virtual_proto == IPPROTO_TCP &&
		    (pd.hdr.tcp.th_flags & (TH_SYN|TH_ACK|TH_RST)) == TH_ACK &&
		    pf_syncookie_validate(&pd)) {
			struct mbuf	*msyn;
			msyn = pf_syncookie_recreate_syn(&pd);
			if (msyn) {
				PF_UNLOCK();
				action = pf_test(af, fwdir, ifp, &msyn);
				PF_LOCK();
				m_freem(msyn);
				if (action == PF_PASS || action == PF_AFRT) {
					pf_test_state(&pd, &s, &reason, 1);
					if (s == NULL) {
						PF_UNLOCK();
						return (PF_DROP);
					}
					s->src.seqhi =
					    ntohl(pd.hdr.tcp.th_ack) - 1;
					s->src.seqlo =
					    ntohl(pd.hdr.tcp.th_seq) - 1;
					pf_set_protostate(s, PF_PEER_SRC,
					    PF_TCPS_PROXY_DST);
					action = pf_synproxy(&pd, &s, &reason);
					if (action != PF_PASS) {
						PF_UNLOCK();
						return (action);
					}
				}
			} else
				action = PF_DROP;
		}


		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC > 0 */
			r = s->rule.ptr;
			a = s->anchor.ptr;
#if NPFLOG > 0
			pd.pflog |= s->log;
#endif	/* NPFLOG > 0 */
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset,
			    &reason);

		if (pd.virtual_proto == IPPROTO_TCP) {
			if (s) {
				if (s->max_mss)
					pf_normalize_mss(&pd, s->max_mss);
			} else if (r->max_mss)
				pf_normalize_mss(&pd, r->max_mss);
		}

		break;
	}

unlock:
	PF_UNLOCK();

	/*
	 * At the moment, we rely on NET_LOCK() to prevent removal of items
	 * we've collected above ('s', 'r', 'anchor' and 'ruleset').  They'll
	 * have to be refcounted when NET_LOCK() is gone.
	 */

done:
	if (action != PF_DROP) {
		if (s) {
			/* The non-state case is handled in pf_test_rule() */
			if (action == PF_PASS && pd.badopts &&
			    !(s->state_flags & PFSTATE_ALLOWOPTS)) {
				action = PF_DROP;
				REASON_SET(&reason, PFRES_IPOPTIONS);
#if NPFLOG > 0
				pd.pflog |= PF_LOG_FORCE;
#endif	/* NPFLOG > 0 */
				DPFPRINTF(LOG_NOTICE, "dropping packet with "
				    "ip/ipv6 options in pf_test()");
			}

			pf_scrub(pd.m, s->state_flags, pd.af, s->min_ttl,
			    s->set_tos);
			pf_tag_packet(pd.m, s->tag, s->rtableid[pd.didx]);
			if (pqid || (pd.tos & IPTOS_LOWDELAY)) {
				qid = s->pqid;
				if (s->state_flags & PFSTATE_SETPRIO)
					pd.m->m_pkthdr.pf.prio = s->set_prio[1];
			} else {
				qid = s->qid;
				if (s->state_flags & PFSTATE_SETPRIO)
					pd.m->m_pkthdr.pf.prio = s->set_prio[0];
			}
		} else {
			pf_scrub(pd.m, r->scrub_flags, pd.af, r->min_ttl,
			    r->set_tos);
			if (pqid || (pd.tos & IPTOS_LOWDELAY)) {
				qid = r->pqid;
				if (r->scrub_flags & PFSTATE_SETPRIO)
					pd.m->m_pkthdr.pf.prio = r->set_prio[1];
			} else {
				qid = r->qid;
				if (r->scrub_flags & PFSTATE_SETPRIO)
					pd.m->m_pkthdr.pf.prio = r->set_prio[0];
			}
		}
	}

	if (action == PF_PASS && qid)
		pd.m->m_pkthdr.pf.qid = qid;
	if (pd.dir == PF_IN && s && s->key[PF_SK_STACK])
		pf_mbuf_link_state_key(pd.m, s->key[PF_SK_STACK]);
	if (pd.dir == PF_OUT &&
	    pd.m->m_pkthdr.pf.inp && !pd.m->m_pkthdr.pf.inp->inp_pf_sk &&
	    s && s->key[PF_SK_STACK] && !s->key[PF_SK_STACK]->inp)
		pf_state_key_link_inpcb(s->key[PF_SK_STACK],
		    pd.m->m_pkthdr.pf.inp);

	if (s && (pd.m->m_pkthdr.ph_flowid & M_FLOWID_VALID) == 0) {
		pd.m->m_pkthdr.ph_flowid = M_FLOWID_VALID |
		    (M_FLOWID_MASK & bemtoh64(&s->id));
	}

	/*
	 * connections redirected to loopback should not match sockets
	 * bound specifically to loopback due to security implications,
	 * see in_pcblookup_listen().
	 */
	if (pd.destchg)
		if ((pd.af == AF_INET && (ntohl(pd.dst->v4.s_addr) >>
		    IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) ||
		    (pd.af == AF_INET6 && IN6_IS_ADDR_LOOPBACK(&pd.dst->v6)))
			pd.m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
	/* We need to redo the route lookup on outgoing routes. */
	if (pd.destchg && pd.dir == PF_OUT)
		pd.m->m_pkthdr.pf.flags |= PF_TAG_REROUTE;

	if (pd.dir == PF_IN && action == PF_PASS &&
	    (r->divert.type == PF_DIVERT_TO ||
	    r->divert.type == PF_DIVERT_REPLY)) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(pd.m))) {
			pd.m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->addr = r->divert.addr;
			divert->port = r->divert.port;
			divert->rdomain = pd.rdomain;
			divert->type = r->divert.type;
		}
	}

	if (action == PF_PASS && r->divert.type == PF_DIVERT_PACKET)
		action = PF_DIVERT;

#if NPFLOG > 0
	if (pd.pflog) {
		struct pf_rule_item	*ri;

		if (pd.pflog & PF_LOG_FORCE || r->log & PF_LOG_ALL)
			PFLOG_PACKET(&pd, reason, r, a, ruleset, NULL);
		if (s) {
			SLIST_FOREACH(ri, &s->match_rules, entry)
				if (ri->r->log & PF_LOG_ALL)
					PFLOG_PACKET(&pd, reason, ri->r, a,
					    ruleset, NULL);
		}
	}
#endif	/* NPFLOG > 0 */

	pf_counters_inc(action, &pd, s, r, a);

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(pd.m);
		/* FALLTHROUGH */
	case PF_DEFER:
		pd.m = NULL;
		action = PF_PASS;
		break;
	case PF_DIVERT:
		switch (pd.af) {
		case AF_INET:
			if (!divert_packet(pd.m, pd.dir, r->divert.port))
				pd.m = NULL;
			break;
#ifdef INET6
		case AF_INET6:
			if (!divert6_packet(pd.m, pd.dir, r->divert.port))
				pd.m = NULL;
			break;
#endif /* INET6 */
		}
		action = PF_PASS;
		break;
#ifdef INET6
	case PF_AFRT:
		if (pf_translate_af(&pd)) {
			action = PF_DROP;
			break;
		}
		pd.m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		switch (pd.naf) {
		case AF_INET:
			if (pd.dir == PF_IN)
				ip_forward(pd.m, ifp, NULL, 1);
			else
				ip_output(pd.m, NULL, NULL, 0, NULL, NULL, 0);
			break;
		case AF_INET6:
			if (pd.dir == PF_IN)
				ip6_forward(pd.m, NULL, 1);
			else
				ip6_output(pd.m, NULL, NULL, 0, NULL, NULL);
			break;
		}
		pd.m = NULL;
		action = PF_PASS;
		break;
#endif /* INET6 */
	case PF_DROP:
		m_freem(pd.m);
		pd.m = NULL;
		break;
	default:
		if (r->rt) {
			switch (pd.af) {
			case AF_INET:
				pf_route(&pd, r, s);
				break;
#ifdef INET6
			case AF_INET6:
				pf_route6(&pd, r, s);
				break;
#endif /* INET6 */
			}
		}
		break;
	}

#ifdef INET6
	/* if reassembled packet passed, create new fragments */
	if (pf_status.reass && action == PF_PASS && pd.m && fwdir == PF_FWD &&
	    pd.af == AF_INET6) {
		struct m_tag	*mtag;

		if ((mtag = m_tag_find(pd.m, PACKET_TAG_PF_REASSEMBLED, NULL)))
			action = pf_refragment6(&pd.m, mtag, NULL, NULL, NULL);
	}
#endif	/* INET6 */
	if (s && action != PF_DROP) {
		if (!s->if_index_in && dir == PF_IN)
			s->if_index_in = ifp->if_index;
		else if (!s->if_index_out && dir == PF_OUT)
			s->if_index_out = ifp->if_index;
	}

	*m0 = pd.m;

	return (action);
}

int
pf_ouraddr(struct mbuf *m)
{
	struct pf_state_key	*sk;

	if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED)
		return (1);

	sk = m->m_pkthdr.pf.statekey;
	if (sk != NULL) {
		if (sk->inp != NULL)
			return (1);
	}

	return (-1);
}

/*
 * must be called whenever any addressing information such as
 * address, port, protocol has changed
 */
void
pf_pkt_addr_changed(struct mbuf *m)
{
	pf_mbuf_unlink_state_key(m);
	m->m_pkthdr.pf.inp = NULL;
}

struct inpcb *
pf_inp_lookup(struct mbuf *m)
{
	struct inpcb *inp = NULL;
	struct pf_state_key *sk = m->m_pkthdr.pf.statekey;

	if (!pf_state_key_isvalid(sk))
		pf_mbuf_unlink_state_key(m);
	else
		inp = m->m_pkthdr.pf.statekey->inp;

	if (inp && inp->inp_pf_sk)
		KASSERT(m->m_pkthdr.pf.statekey == inp->inp_pf_sk);

	return (inp);
}

void
pf_inp_link(struct mbuf *m, struct inpcb *inp)
{
	struct pf_state_key *sk = m->m_pkthdr.pf.statekey;

	if (!pf_state_key_isvalid(sk)) {
		pf_mbuf_unlink_state_key(m);
		return;
	}

	/*
	 * we don't need to grab PF-lock here. At worst case we link inp to
	 * state, which might be just being marked as deleted by another
	 * thread.
	 */
	if (inp && !sk->inp && !inp->inp_pf_sk)
		pf_state_key_link_inpcb(sk, inp);

	/* The statekey has finished finding the inp, it is no longer needed. */
	pf_mbuf_unlink_state_key(m);
}

void
pf_inp_unlink(struct inpcb *inp)
{
	pf_inpcb_unlink_state_key(inp);
}

void
pf_state_key_link_reverse(struct pf_state_key *sk, struct pf_state_key *skrev)
{
	/* Note that sk and skrev may be equal, then we refcount twice. */
	KASSERT(sk->reverse == NULL);
	KASSERT(skrev->reverse == NULL);
	sk->reverse = pf_state_key_ref(skrev);
	skrev->reverse = pf_state_key_ref(sk);
}

#if NPFLOG > 0
void
pf_log_matches(struct pf_pdesc *pd, struct pf_rule *rm, struct pf_rule *am,
    struct pf_ruleset *ruleset, struct pf_rule_slist *matchrules)
{
	struct pf_rule_item	*ri;

	/* if this is the log(matches) rule, packet has been logged already */
	if (rm->log & PF_LOG_MATCHES)
		return;

	SLIST_FOREACH(ri, matchrules, entry)
		if (ri->r->log & PF_LOG_MATCHES)
			PFLOG_PACKET(pd, PFRES_MATCH, rm, am, ruleset, ri->r);
}
#endif	/* NPFLOG > 0 */

struct pf_state_key *
pf_state_key_ref(struct pf_state_key *sk)
{
	if (sk != NULL)
		PF_REF_TAKE(sk->refcnt);

	return (sk);
}

void
pf_state_key_unref(struct pf_state_key *sk)
{
	if (PF_REF_RELE(sk->refcnt)) {
		/* state key must be removed from tree */
		KASSERT(!pf_state_key_isvalid(sk));
		/* state key must be unlinked from reverse key */
		KASSERT(sk->reverse == NULL);
		/* state key must be unlinked from socket */
		KASSERT(sk->inp == NULL);
		pool_put(&pf_state_key_pl, sk);
	}
}

int
pf_state_key_isvalid(struct pf_state_key *sk)
{
	return ((sk != NULL) && (sk->removed == 0));
}

void
pf_mbuf_unlink_state_key(struct mbuf *m)
{
	struct pf_state_key *sk = m->m_pkthdr.pf.statekey;

	if (sk != NULL) {
		m->m_pkthdr.pf.statekey = NULL;
		pf_state_key_unref(sk);
	}
}

void
pf_mbuf_link_state_key(struct mbuf *m, struct pf_state_key *sk)
{
	KASSERT(m->m_pkthdr.pf.statekey == NULL);
	m->m_pkthdr.pf.statekey = pf_state_key_ref(sk);
}

void
pf_state_key_link_inpcb(struct pf_state_key *sk, struct inpcb *inp)
{
	KASSERT(sk->inp == NULL);
	sk->inp = inp;
	KASSERT(inp->inp_pf_sk == NULL);
	inp->inp_pf_sk = pf_state_key_ref(sk);
}

void
pf_inpcb_unlink_state_key(struct inpcb *inp)
{
	struct pf_state_key *sk = inp->inp_pf_sk;

	if (sk != NULL) {
		KASSERT(sk->inp == inp);
		sk->inp = NULL;
		inp->inp_pf_sk = NULL;
		pf_state_key_unref(sk);
	}
}

void
pf_state_key_unlink_inpcb(struct pf_state_key *sk)
{
	struct inpcb *inp = sk->inp;

	if (inp != NULL) {
		KASSERT(inp->inp_pf_sk == sk);
		sk->inp = NULL;
		inp->inp_pf_sk = NULL;
		pf_state_key_unref(sk);
	}
}

void
pf_state_key_unlink_reverse(struct pf_state_key *sk)
{
	struct pf_state_key *skrev = sk->reverse;

	/* Note that sk and skrev may be equal, then we unref twice. */
	if (skrev != NULL) {
		KASSERT(skrev->reverse == sk);
		sk->reverse = NULL;
		skrev->reverse = NULL;
		pf_state_key_unref(skrev);
		pf_state_key_unref(sk);
	}
}
