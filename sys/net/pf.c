/*	$OpenBSD: pf.c,v 1.916 2015/05/26 16:17:51 mikeb Exp $ */

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
#include <net/if_types.h>
#include <net/route.h>
#include <net/radix_mpath.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp_var.h>
#include <netinet/icmp_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_divert.h>

#include <net/pfvar.h>
#include <net/if_pflog.h>
#include <net/if_pflow.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_divert.h>
#endif /* INET6 */


/*
 * Global variables
 */
struct pf_state_tree	 pf_statetbl;
struct pf_queuehead	 pf_queues[2];
struct pf_queuehead	*pf_queues_active;
struct pf_queuehead	*pf_queues_inactive;

struct pf_status	 pf_status;

SHA2_CTX		 pf_tcp_secret_ctx;
u_char			 pf_tcp_secret[16];
int			 pf_tcp_secret_init;
int			 pf_tcp_iss_off;

struct pf_anchor_stackframe {
	struct pf_ruleset			*rs;
	struct pf_rule				*r;
	struct pf_anchor_node			*parent;
	struct pf_anchor			*child;
} pf_anchor_stack[64];

/*
 * Cannot fold into pf_pdesc directly, unknown storage size outside pf.c.
 * Keep in sync with union pf_headers in pflog_bpfcopy() in if_pflog.c.
 */
union pf_headers {
	struct tcphdr		tcp;
	struct udphdr		udp;
	struct icmp		icmp;
#ifdef INET6
	struct icmp6_hdr	icmp6;
	struct mld_hdr		mld;
	struct nd_neighbor_solicit nd_ns;
#endif /* INET6 */
};


struct pool		 pf_src_tree_pl, pf_rule_pl, pf_queue_pl;
struct pool		 pf_state_pl, pf_state_key_pl, pf_state_item_pl;
struct pool		 pf_rule_item_pl, pf_sn_item_pl;

void			 pf_init_threshold(struct pf_threshold *, u_int32_t,
			    u_int32_t);
void			 pf_add_threshold(struct pf_threshold *);
int			 pf_check_threshold(struct pf_threshold *);

void			 pf_change_ap(struct pf_pdesc *, struct pf_addr *,
			    u_int16_t *, struct pf_addr *, u_int16_t,
			    sa_family_t);
int			 pf_modulate_sack(struct pf_pdesc *,
			    struct pf_state_peer *);
void			 pf_change_a6(struct pf_pdesc *, struct pf_addr *a,
			    struct pf_addr *an);
int			 pf_icmp_mapping(struct pf_pdesc *, u_int8_t, int *,
			    u_int16_t *, u_int16_t *);
void			 pf_change_icmp(struct pf_pdesc *, struct pf_addr *,
			    u_int16_t *, struct pf_addr *, struct pf_addr *,
			    u_int16_t, sa_family_t);
int			 pf_change_icmp_af(struct mbuf *, int,
			    struct pf_pdesc *, struct pf_pdesc *,
			    struct pf_addr *, struct pf_addr *, sa_family_t,
			    sa_family_t);
int			 pf_translate_icmp_af(int, void *);
void			 pf_send_tcp(const struct pf_rule *, sa_family_t,
			    const struct pf_addr *, const struct pf_addr *,
			    u_int16_t, u_int16_t, u_int32_t, u_int32_t,
			    u_int8_t, u_int16_t, u_int16_t, u_int8_t, int,
			    u_int16_t, u_int, struct ether_header *,
			    struct ifnet *);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t,
			    sa_family_t, struct pf_rule *, u_int);
void			 pf_detach_state(struct pf_state *);
void			 pf_state_key_detach(struct pf_state *, int);
u_int32_t		 pf_tcp_iss(struct pf_pdesc *);
void			 pf_rule_to_actions(struct pf_rule *,
			    struct pf_rule_actions *);
int			 pf_test_rule(struct pf_pdesc *, struct pf_rule **,
			    struct pf_state **, struct pf_rule **,
			    struct pf_ruleset **);
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
			    struct pf_state_peer *, struct pf_state_peer *,
			    struct pf_state **, u_short *, int *);
int			 pf_tcp_track_sloppy(struct pf_pdesc *,
			    struct pf_state_peer *, struct pf_state_peer *,
			    struct pf_state **, u_short *);
static __inline int	 pf_synproxy(struct pf_pdesc *, struct pf_state **,
			    u_short *);
int			 pf_test_state(struct pf_pdesc *, struct pf_state **,
			    u_short *);
int			 pf_icmp_state_lookup(struct pf_pdesc *,
			    struct pf_state_key_cmp *, struct pf_state **,
			    u_int16_t, u_int16_t, int, int *, int, int);
int			 pf_test_state_icmp(struct pf_pdesc *,
			    struct pf_state **, u_short *);
u_int8_t		 pf_get_wscale(struct pf_pdesc *);
u_int16_t		 pf_get_mss(struct pf_pdesc *);
u_int16_t		 pf_calc_mss(struct pf_addr *, sa_family_t, int,
				u_int16_t);
void			 pf_set_rt_ifp(struct pf_state *,
			    struct pf_addr *);
struct pf_divert	*pf_get_divert(struct mbuf *);
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
void			 pf_step_into_anchor(int *, struct pf_ruleset **,
			    struct pf_rule **, struct pf_rule **);
int			 pf_step_out_of_anchor(int *, struct pf_ruleset **,
			     struct pf_rule **, struct pf_rule **,
			     int *);
void			 pf_counters_inc(int, struct pf_pdesc *,
			    struct pf_state *, struct pf_rule *,
			    struct pf_rule *);
void			 pf_log_matches(struct pf_pdesc *, struct pf_rule *,
			    struct pf_rule *, struct pf_ruleset *,
			    struct pf_rule_slist *);

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
		s = pf_find_state(i, k, d, m);			\
		if (s == NULL || (s)->timeout == PFTM_PURGE)		\
			return (PF_DROP);				\
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

#define STATE_DEC_COUNTERS(s)					\
	do {							\
		struct pf_rule_item *mrm;			\
		if (s->anchor.ptr != NULL)			\
			s->anchor.ptr->states_cur--;		\
		s->rule.ptr->states_cur--;			\
		SLIST_FOREACH(mrm, &s->match_rules, entry)	\
			mrm->r->states_cur--;			\
	} while (0)

static __inline int pf_src_compare(struct pf_src_node *, struct pf_src_node *);
static __inline int pf_state_compare_key(struct pf_state_key *,
	struct pf_state_key *);
static __inline int pf_state_compare_id(struct pf_state *,
	struct pf_state *);

struct pf_src_tree tree_src_tracking;

struct pf_state_tree_id tree_id;
struct pf_state_queue state_list;

RB_GENERATE(pf_src_tree, pf_src_node, entry, pf_src_compare);
RB_GENERATE(pf_state_tree, pf_state_key, entry, pf_state_compare_key);
RB_GENERATE(pf_state_tree_id, pf_state,
    entry_id, pf_state_compare_id);

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

#ifdef INET6
void
pf_addrcpy(struct pf_addr *dst, struct pf_addr *src, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		dst->addr32[0] = src->addr32[0];
		break;
	case AF_INET6:
		dst->addr32[0] = src->addr32[0];
		dst->addr32[1] = src->addr32[1];
		dst->addr32[2] = src->addr32[2];
		dst->addr32[3] = src->addr32[3];
		break;
	}
}
#endif /* INET6 */

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

		bzero(&p, sizeof(p));
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
					st->src.state = st->dst.state =
					    TCPS_CLOSED;
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
	(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
	return (1);
}

int
pf_insert_src_node(struct pf_src_node **sn, struct pf_rule *rule,
    enum pf_sn_types type, sa_family_t af, struct pf_addr *src,
    struct pf_addr *raddr, int global)
{
	struct pf_src_node	k;

	if (*sn == NULL) {
		k.af = af;
		k.type = type;
		PF_ACPY(&k.addr, src, af);
		if (global)
			k.rule.ptr = NULL;
		else
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
		if (global)
			(*sn)->rule.ptr = NULL;
		else
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
		if ((*sn)->rule.ptr != NULL)
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

	if (sn->rule.ptr != NULL) {
		sn->rule.ptr->src_nodes--;
		if (sn->rule.ptr->states_cur == 0 &&
		    sn->rule.ptr->src_nodes == 0)
			pf_rm_rule(NULL, sn->rule.ptr);
		RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
		pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
		pf_status.src_nodes--;
		pool_put(&pf_src_tree_pl, sn);
	}
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
				if (sk->proto == IPPROTO_TCP &&
				    si->s->src.state >= TCPS_FIN_WAIT_2 &&
				    si->s->dst.state >= TCPS_FIN_WAIT_2) {
					si->s->src.state = si->s->dst.state =
					    TCPS_CLOSED;
					/* unlink late or sks can go away */
					olds = si->s;
				} else {
					if (pf_status.debug >= LOG_NOTICE) {
						log(LOG_NOTICE,
						    "pf: %s key attach "
						    "failed on %s: ",
						    (idx == PF_SK_WIRE) ?
						    "wire" : "stack",
						    s->kif->pfik_name);
						pf_print_state_parts(s,
						    (idx == PF_SK_WIRE) ?
						    sk : NULL,
						    (idx == PF_SK_STACK) ?
						    sk : NULL);
						addlog(", existing: ");
						pf_print_state_parts(si->s,
						    (idx == PF_SK_WIRE) ?
						    sk : NULL,
						    (idx == PF_SK_STACK) ?
						    sk : NULL);
						addlog("\n");
					}
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
		pf_unlink_state(olds);

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

	if (s->key[idx] == NULL)
		return;

	si = TAILQ_FIRST(&s->key[idx]->states);
	while (si && si->s != s)
	    si = TAILQ_NEXT(si, entry);

	if (si) {
		TAILQ_REMOVE(&s->key[idx]->states, si, entry);
		pool_put(&pf_state_item_pl, si);
	}

	if (TAILQ_EMPTY(&s->key[idx]->states)) {
		RB_REMOVE(pf_state_tree, &pf_statetbl, s->key[idx]);
		if (s->key[idx]->reverse)
			s->key[idx]->reverse->reverse = NULL;
		if (s->key[idx]->inp)
			s->key[idx]->inp->inp_pf_sk = NULL;
		pool_put(&pf_state_key_pl, s->key[idx]);
	}
	s->key[idx] = NULL;
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
	struct nd_neighbor_solicit *nd;
	struct pf_addr *target;

	if (af == AF_INET || pd->proto != IPPROTO_ICMPV6)
		goto copy;

	switch (pd->hdr.icmp6->icmp6_type) {
	case ND_NEIGHBOR_SOLICIT:
		if (multi)
			return (-1);
		nd = (void *)pd->hdr.icmp6;
		target = (struct pf_addr *)&nd->nd_ns_target;
		daddr = target;
		break;
	case ND_NEIGHBOR_ADVERT:
		if (multi)
			return (-1);
		nd = (void *)pd->hdr.icmp6;
		target = (struct pf_addr *)&nd->nd_ns_target;
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
#endif
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
	splsoftassert(IPL_SOFTNET);

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
#endif
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
	struct pf_state_key	*sk;
	struct pf_state_item	*si;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	if (pf_status.debug >= LOG_DEBUG) {
		log(LOG_DEBUG, "pf: key search, if=%s: ", kif->pfik_name);
		pf_print_state_parts(NULL, (struct pf_state_key *)key, NULL);
		addlog("\n");
	}

	if (dir == PF_OUT && m->m_pkthdr.pf.statekey &&
	    m->m_pkthdr.pf.statekey->reverse)
		sk = m->m_pkthdr.pf.statekey->reverse;
	else if (dir == PF_OUT && m->m_pkthdr.pf.inp &&
	    m->m_pkthdr.pf.inp->inp_pf_sk)
		sk = m->m_pkthdr.pf.inp->inp_pf_sk;
	else {
		if ((sk = RB_FIND(pf_state_tree, &pf_statetbl,
		    (struct pf_state_key *)key)) == NULL)
			return (NULL);
		if (dir == PF_OUT && m->m_pkthdr.pf.statekey &&
		    pf_compare_state_keys(m->m_pkthdr.pf.statekey, sk,
		    kif, dir) == 0) {
			m->m_pkthdr.pf.statekey->reverse = sk;
			sk->reverse = m->m_pkthdr.pf.statekey;
		} else if (dir == PF_OUT && m->m_pkthdr.pf.inp && !sk->inp) {
			m->m_pkthdr.pf.inp->inp_pf_sk = sk;
			sk->inp = m->m_pkthdr.pf.inp;
		}
	}

	if (dir == PF_OUT) {
		m->m_pkthdr.pf.statekey = NULL;
		m->m_pkthdr.pf.inp = NULL;
	}

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

	bzero(sp, sizeof(struct pfsync_state));

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
	sp->log = st->log;
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
pf_purge_thread(void *v)
{
	int nloops = 0, s;

	for (;;) {
		tsleep(pf_purge_thread, PWAIT, "pftm", 1 * hz);

		s = splsoftnet();

		/* process a fraction of the state table every second */
		pf_purge_expired_states(1 + (pf_status.states
		    / pf_default_rule.timeout[PFTM_INTERVAL]));

		/* purge other expired types every PFTM_INTERVAL seconds */
		if (++nloops >= pf_default_rule.timeout[PFTM_INTERVAL]) {
			pf_purge_expired_fragments();
			pf_purge_expired_src_nodes(0);
			nloops = 0;
		}

		splx(s);
	}
}

int32_t
pf_state_expires(const struct pf_state *state)
{
	int32_t		timeout;
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

		timeout = timeout * (end - states) / (end - start);
	}

	return (state->expire + timeout);
}

void
pf_purge_expired_src_nodes(int waslocked)
{
	struct pf_src_node		*cur, *next;
	int				 locked = waslocked;

	for (cur = RB_MIN(pf_src_tree, &tree_src_tracking); cur; cur = next) {
	next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);

		if (cur->states <= 0 && cur->expire <= time_uptime) {
			if (! locked) {
				rw_enter_write(&pf_consistency_lock);
				next = RB_NEXT(pf_src_tree,
				    &tree_src_tracking, cur);
				locked = 1;
			}
			pf_remove_src_node(cur);
		}
	}

	if (locked && !waslocked)
		rw_exit_write(&pf_consistency_lock);
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
		if (--sni->sn->states <= 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
				    pf_default_rule.timeout[PFTM_SRC_NODE];
			sni->sn->expire = time_uptime + timeout;
		}
		pool_put(&pf_sn_item_pl, sni);
	}
}

/* callers should be at splsoftnet */
void
pf_unlink_state(struct pf_state *cur)
{
	splsoftassert(IPL_SOFTNET);

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
		    cur->key[PF_SK_WIRE]->rdomain, NULL, NULL);
	}
	RB_REMOVE(pf_state_tree_id, &tree_id, cur);
#if NPFLOW > 0
	if (cur->state_flags & PFSTATE_PFLOW)
		export_pflow(cur);
#endif
#if NPFSYNC > 0
	pfsync_delete_state(cur);
#endif
	cur->timeout = PFTM_UNLINKED;
	pf_src_tree_remove_state(cur);
	pf_detach_state(cur);
}

/* callers should be at splsoftnet and hold the
 * write_lock on pf_consistency_lock */
void
pf_free_state(struct pf_state *cur)
{
	struct pf_rule_item *ri;

	splsoftassert(IPL_SOFTNET);

#if NPFSYNC > 0
	if (pfsync_state_in_use(cur))
		return;
#endif
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
	int			 locked = 0;

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
			/* free unlinked state */
			if (! locked) {
				rw_enter_write(&pf_consistency_lock);
				locked = 1;
			}
			pf_free_state(cur);
		} else if (pf_state_expires(cur) <= time_uptime) {
			/* unlink and free expired state */
			pf_unlink_state(cur);
			if (! locked) {
				rw_enter_write(&pf_consistency_lock);
				locked = 1;
			}
			pf_free_state(cur);
		}
		cur = next;
	}

	if (locked)
		rw_exit_write(&pf_consistency_lock);
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

void
pf_change_ap(struct pf_pdesc *pd, struct pf_addr *a, u_int16_t *p,
    struct pf_addr *an, u_int16_t pn, sa_family_t naf)
{
	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);
	if (pd->af == naf)
		PF_ACPY(a, an, naf);
	if (p != NULL)
		*p = pn;
}

/* Changes a u_int32_t.  Uses a void * so there are no align restrictions */
void
pf_change_a(struct pf_pdesc *pd, void *a, u_int32_t an)
{
	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);
	memcpy(a, &an, sizeof(u_int32_t));
}

#ifdef INET6
void
pf_change_a6(struct pf_pdesc *pd, struct pf_addr *a, struct pf_addr *an)
{
	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);
	PF_ACPY(a, an, AF_INET6);
}
#endif /* INET6 */

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
		case ICMP_ECHOREPLY:
			*virtual_type = ICMP_ECHO;
			*virtual_id = pd->hdr.icmp->icmp_id;
			break;

		case ICMP_TSTAMP:
			*icmp_dir = PF_IN;
		case ICMP_TSTAMPREPLY:
			*virtual_type = ICMP_TSTAMP;
			*virtual_id = pd->hdr.icmp->icmp_id;
			break;

		case ICMP_IREQ:
			*icmp_dir = PF_IN;
		case ICMP_IREQREPLY:
			*virtual_type = ICMP_IREQ;
			*virtual_id = pd->hdr.icmp->icmp_id;
			break;

		case ICMP_MASKREQ:
			*icmp_dir = PF_IN;
		case ICMP_MASKREPLY:
			*virtual_type = ICMP_MASKREQ;
			*virtual_id = pd->hdr.icmp->icmp_id;
			break;

		case ICMP_IPV6_WHEREAREYOU:
			*icmp_dir = PF_IN;
		case ICMP_IPV6_IAMHERE:
			*virtual_type = ICMP_IPV6_WHEREAREYOU;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ICMP_MOBILE_REGREQUEST:
			*icmp_dir = PF_IN;
		case ICMP_MOBILE_REGREPLY:
			*virtual_type = ICMP_MOBILE_REGREQUEST;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ICMP_ROUTERSOLICIT:
			*icmp_dir = PF_IN;
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
			*virtual_type = type;
			*virtual_id = 0;
			HTONS(*virtual_type);
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
		case ICMP6_ECHO_REPLY:
			*virtual_type = ICMP6_ECHO_REQUEST;
			*virtual_id = pd->hdr.icmp6->icmp6_id;
			break;

		case MLD_LISTENER_QUERY:
			*icmp_dir = PF_IN;
		case MLD_LISTENER_REPORT: {
			struct mld_hdr *mld = (void *)pd->hdr.icmp6;
			u_int32_t h;

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
		case ICMP6_WRUREPLY:
			*virtual_type = ICMP6_WRUREQUEST;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case MLD_MTRACE:
			*icmp_dir = PF_IN;
		case MLD_MTRACE_RESP:
			*virtual_type = MLD_MTRACE;
			*virtual_id = 0; /* Nothing sane to match on! */
			break;

		case ND_NEIGHBOR_SOLICIT:
			*icmp_dir = PF_IN;
		case ND_NEIGHBOR_ADVERT: {
			struct nd_neighbor_solicit *nd = (void *)pd->hdr.icmp6;
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
			*virtual_type = type;
			*virtual_id = 0;
			HTONS(*virtual_type);
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
	HTONS(*virtual_type);
	return (0);  /* These types match to their own state */
}

void
pf_change_icmp(struct pf_pdesc *pd, struct pf_addr *ia, u_int16_t *ip,
    struct pf_addr *oa, struct pf_addr *na, u_int16_t np, sa_family_t af)
{
	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);

	/* Change inner protocol port */
	if (ip != NULL)
		*ip = np;

	/* Change inner ip address */
	PF_ACPY(ia, na, af);

	/* Outer ip address, fix outer icmpv6 checksum, if necessary. */
	if (oa)
		PF_ACPY(oa, na, af);
}

#if INET && INET6
int
pf_translate_af(struct pf_pdesc *pd)
{
	struct mbuf		*mp;
	struct ip		*ip4;
	struct ip6_hdr		*ip6;
	struct icmp6_hdr	*icmp;
	int			 hlen;

	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);

	hlen = pd->naf == AF_INET ? sizeof(*ip4) : sizeof(*ip6);

	/* trim the old header */
	m_adj(pd->m, pd->off);

	/* prepend a new one */
	if ((M_PREPEND(pd->m, hlen, M_DONTWAIT)) == NULL)
		return (-1);

	switch (pd->naf) {
	case AF_INET:
		ip4 = mtod(pd->m, struct ip *);
		bzero(ip4, hlen);
		ip4->ip_v   = IPVERSION;
		ip4->ip_hl  = hlen >> 2;
		ip4->ip_tos = pd->tos;
		ip4->ip_len = htons(hlen + (pd->tot_len - pd->off));
		ip4->ip_id  = htons(ip_randomid());
		ip4->ip_off = htons(IP_DF);
		ip4->ip_ttl = pd->ttl;
		ip4->ip_p   = pd->proto;
		ip4->ip_src = pd->nsaddr.v4;
		ip4->ip_dst = pd->ndaddr.v4;
		break;
	case AF_INET6:
		ip6 = mtod(pd->m, struct ip6_hdr *);
		bzero(ip6, hlen);
		ip6->ip6_vfc  = IPV6_VERSION;
		ip6->ip6_flow |= htonl((u_int32_t)pd->tos << 20);
		ip6->ip6_plen = htons(pd->tot_len - pd->off);
		ip6->ip6_nxt  = pd->proto;
		if (!pd->ttl || pd->ttl > IPV6_DEFHLIM)
			ip6->ip6_hlim = IPV6_DEFHLIM;
		else
			ip6->ip6_hlim = pd->ttl;
		ip6->ip6_src  = pd->nsaddr.v6;
		ip6->ip6_dst  = pd->ndaddr.v6;
		break;
	default:
		return (-1);
	}

	/* recalculate icmp/icmp6 checksums */
	if (pd->proto == IPPROTO_ICMP || pd->proto == IPPROTO_ICMPV6) {
		int off;
		if ((mp = m_pulldown(pd->m, hlen, sizeof(*icmp), &off)) ==
		    NULL) {
			pd->m = NULL;
			return (-1);
		}
		icmp = (struct icmp6_hdr *)(mp->m_data + off);
		icmp->icmp6_cksum = 0;
		icmp->icmp6_cksum = pd->naf == AF_INET ?
		    in4_cksum(pd->m, 0, hlen, ntohs(ip4->ip_len) - hlen) :
		    in6_cksum(pd->m, IPPROTO_ICMPV6, hlen,
		    ntohs(ip6->ip6_plen));
	}

	return (0);
}

int
pf_change_icmp_af(struct mbuf *m, int off, struct pf_pdesc *pd,
    struct pf_pdesc *pd2, struct pf_addr *src, struct pf_addr *dst,
    sa_family_t af, sa_family_t naf)
{
	struct mbuf		*n = NULL;
	struct ip		*ip4;
	struct ip6_hdr		*ip6;
	int			 hlen, olen, mlen;

	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);

	if (af == naf || (af != AF_INET && af != AF_INET6) ||
	    (naf != AF_INET && naf != AF_INET6))
		return (-1);

	/* split the mbuf chain on the inner ip/ip6 header boundary */
	if ((n = m_split(m, off, M_DONTWAIT)) == NULL)
		return (-1);

	/* old header */
	olen = pd2->off - off;
	/* new header */
	hlen = naf == AF_INET ? sizeof(*ip4) : sizeof(*ip6);

	/* trim old header */
	m_adj(n, olen);

	/* prepend a new one */
	if ((M_PREPEND(n, hlen, M_DONTWAIT)) == NULL)
		return (-1);

	/* translate inner ip/ip6 header */
	switch (naf) {
	case AF_INET:
		ip4 = mtod(n, struct ip *);
		bzero(ip4, sizeof(*ip4));
		ip4->ip_v   = IPVERSION;
		ip4->ip_hl  = sizeof(*ip4) >> 2;
		ip4->ip_len = htons(sizeof(*ip4) + pd2->tot_len - olen);
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
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc  = IPV6_VERSION;
		ip6->ip6_plen = htons(pd2->tot_len - olen);
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

	/* adjust payload offset and total packet length */
	pd2->off += hlen - olen;
	pd->tot_len += hlen - olen;

	/* merge modified inner packet with the original header */
	mlen = n->m_pkthdr.len;
	m_cat(m, n);
	m->m_pkthdr.len += mlen;

	return (0);
}


#define PTR_IP(field)	(offsetof(struct ip, field))
#define PTR_IP6(field)	(offsetof(struct ip6_hdr, field))

int
pf_translate_icmp_af(int af, void *arg)
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
		icmp6->icmp6_type = type;
		icmp6->icmp6_code = code;
		/* aligns well with a icmpv4 nextmtu */
		icmp6->icmp6_mtu = htonl(mtu);
		/* icmpv4 pptr is a one most significant byte */
		if (ptr >= 0)
			icmp6->icmp6_pptr = htonl(ptr << 24);
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
		icmp4->icmp_type = type;
		icmp4->icmp_code = code;
		icmp4->icmp_nextmtu = htons(mtu);
		if (ptr >= 0)
			icmp4->icmp_void = htonl(ptr);
		break;
	}

	return (0);
}
#endif /* INET && INET6 */

/*
 * Need to modulate the sequence numbers in the TCP SACK option
 * (credits to Krzysztof Pfaff for report and patch)
 */
int
pf_modulate_sack(struct pf_pdesc *pd, struct pf_state_peer *dst)
{
	struct tcphdr	*th = pd->hdr.tcp;
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
					memcpy(&sack, &opt[i], sizeof(sack));
					pf_change_a(pd, &sack.start,
					    htonl(ntohl(sack.start) -
					    dst->seqdiff));
					pf_change_a(pd, &sack.end,
					    htonl(ntohl(sack.end) -
					    dst->seqdiff));
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

void
pf_send_tcp(const struct pf_rule *r, sa_family_t af,
    const struct pf_addr *saddr, const struct pf_addr *daddr,
    u_int16_t sport, u_int16_t dport, u_int32_t seq, u_int32_t ack,
    u_int8_t flags, u_int16_t win, u_int16_t mss, u_int8_t ttl, int tag,
    u_int16_t rtag, u_int rdom, struct ether_header *eh, struct ifnet *ifp)
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

	switch (af) {
	case AF_INET:
		len = sizeof(struct ip) + tlen;
		break;
#ifdef INET6
	case AF_INET6:
		len = sizeof(struct ip6_hdr) + tlen;
		break;
#endif /* INET6 */
	}

	/* create outgoing mbuf */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
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
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
	bzero(m->m_data, len);
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
	}

	/* TCP header */
	th->th_sport = sport;
	th->th_dport = dport;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_off = tlen >> 2;
	th->th_flags = flags;
	th->th_win = htons(win);

	if (mss) {
		opt = (char *)(th + 1);
		opt[0] = TCPOPT_MAXSEG;
		opt[1] = 4;
		HTONS(mss);
		memcpy((opt + 2), &mss, 2);
	}

	switch (af) {
	case AF_INET:
		if (eh == NULL) {
			ip_output(m, NULL, NULL, 0, NULL, NULL, 0);
		} else {
			struct route		 ro;
			struct rtentry		 rt;
			struct ether_header	*e = (void *)ro.ro_dst.sa_data;

			if (ifp == NULL) {
				m_freem(m);
				return;
			}
			rt.rt_ifp = ifp;
			ro.ro_rt = &rt;
			ro.ro_dst.sa_len = sizeof(ro.ro_dst);
			ro.ro_dst.sa_family = pseudo_AF_HDRCMPLT;
			memcpy(e->ether_shost, eh->ether_dhost, ETHER_ADDR_LEN);
			memcpy(e->ether_dhost, eh->ether_shost, ETHER_ADDR_LEN);
			e->ether_type = eh->ether_type;
			ip_output(m, NULL, &ro, IP_ROUTETOETHER, NULL, NULL, 0);
		}
		break;
#ifdef INET6
	case AF_INET6:
		ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
		break;
#endif /* INET6 */
	}
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code, sa_family_t af,
    struct pf_rule *r, u_int rdomain)
{
	struct mbuf	*m0;

	if ((m0 = m_copy(m, 0, M_COPYALL)) == NULL)
		return;

	m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	m0->m_pkthdr.ph_rtableid = rdomain;
	if (r && (r->scrub_flags & PFSTATE_SETPRIO))
		m0->m_pkthdr.pf.prio = r->set_prio[0];
	if (r && r->qid)
		m0->m_pkthdr.pf.qid = r->qid;

	switch (af) {
	case AF_INET:
		icmp_error(m0, type, code, 0, 0);
		break;
#ifdef INET6
	case AF_INET6:
		icmp6_error(m0, type, code, 0);
		break;
#endif /* INET6 */
	}
}

/*
 * Return 1 if the addresses a and b match (with mask m), otherwise return 0.
 * If n is 0, they match if they are equal. If n is != 0, they match if they
 * are different.
 */
int
pf_match_addr(u_int8_t n, struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af)
{
	int	match = 0;

	switch (af) {
	case AF_INET:
		if ((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			match++;
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
			match++;
		break;
#endif /* INET6 */
	}
	if (match) {
		if (n)
			return (0);
		else
			return (1);
	} else {
		if (n)
			return (1);
		else
			return (0);
	}
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
	NTOHS(a1);
	NTOHS(a2);
	NTOHS(p);
	return (pf_match(op, a1, a2, p));
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
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct pfi_kif *kif;

	if (ifp == NULL)
		return (0);

	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(LOG_ERR,
		    "pf_test_via: kif == NULL, @%d via %s",
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
		m->m_pkthdr.ph_rtableid = rtableid;
}

void
pf_step_into_anchor(int *depth, struct pf_ruleset **rs,
    struct pf_rule **r, struct pf_rule **a)
{
	struct pf_anchor_stackframe	*f;

	if (*depth >= sizeof(pf_anchor_stack) /
	    sizeof(pf_anchor_stack[0])) {
		log(LOG_ERR, "pf_step_into_anchor: stack overflow\n");
		*r = TAILQ_NEXT(*r, entries);
		return;
	} else if (a != NULL)
		*a = *r;
	f = pf_anchor_stack + (*depth)++;
	f->rs = *rs;
	f->r = *r;
	if ((*r)->anchor_wildcard) {
		f->parent = &(*r)->anchor->children;
		if ((f->child = RB_MIN(pf_anchor_node, f->parent)) == NULL) {
			*r = NULL;
			return;
		}
		*rs = &f->child->ruleset;
	} else {
		f->parent = NULL;
		f->child = NULL;
		*rs = &(*r)->anchor->ruleset;
	}
	*r = TAILQ_FIRST((*rs)->rules.active.ptr);
}

int
pf_step_out_of_anchor(int *depth, struct pf_ruleset **rs,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe	*f;
	int quick = 0;

	do {
		if (*depth <= 0)
			break;
		f = pf_anchor_stack + *depth - 1;
		if (f->parent != NULL && f->child != NULL) {
			f->child = RB_NEXT(pf_anchor_node, f->parent, f->child);
			if (f->child != NULL) {
				*rs = &f->child->ruleset;
				*r = TAILQ_FIRST((*rs)->rules.active.ptr);
				if (*r == NULL)
					continue;
				else
					break;
			}
		}
		(*depth)--;
		if (*depth == 0 && a != NULL)
			*a = NULL;
		else if (a != NULL)
			*a = f->r;
		*rs = f->rs;
		if (*match > *depth) {
			*match = *depth;
			if (f->r->quick)
				quick = 1;
		}
		*r = TAILQ_NEXT(f->r, entries);
	} while (*r == NULL);

	return (quick);
}

#ifdef INET6
void
pf_poolmask(struct pf_addr *naddr, struct pf_addr *raddr,
    struct pf_addr *rmask, struct pf_addr *saddr, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		break;
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
	}
}

void
pf_addr_inc(struct pf_addr *addr, sa_family_t af)
{
	switch (af) {
	case AF_INET:
		addr->addr32[0] = htonl(ntohl(addr->addr32[0]) + 1);
		break;
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
	}
}
#endif /* INET6 */

int
pf_socket_lookup(struct pf_pdesc *pd)
{
	struct pf_addr		*saddr, *daddr;
	u_int16_t		 sport, dport;
	struct inpcbtable	*tb;
	struct inpcb		*inp;

	if (pd == NULL)
		return (-1);
	pd->lookup.uid = UID_MAX;
	pd->lookup.gid = GID_MAX;
	pd->lookup.pid = NO_PID;
	switch (pd->proto) {
	case IPPROTO_TCP:
		if (pd->hdr.tcp == NULL)
			return (-1);
		sport = pd->hdr.tcp->th_sport;
		dport = pd->hdr.tcp->th_dport;
		tb = &tcbtable;
		break;
	case IPPROTO_UDP:
		if (pd->hdr.udp == NULL)
			return (-1);
		sport = pd->hdr.udp->uh_sport;
		dport = pd->hdr.udp->uh_dport;
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
			inp = in_pcblookup_listen(tb, daddr->v4, dport, 0,
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
			inp = in6_pcblookup_listen(tb, &daddr->v6, dport, 0,
			    NULL, pd->rdomain);
			if (inp == NULL)
				return (-1);
		}
		break;
#endif /* INET6 */
	}
	pd->lookup.uid = inp->inp_socket->so_euid;
	pd->lookup.gid = inp->inp_socket->so_egid;
	pd->lookup.pid = inp->inp_socket->so_cpid;
	return (1);
}

u_int8_t
pf_get_wscale(struct pf_pdesc *pd)
{
	struct tcphdr	*th = pd->hdr.tcp;
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
	struct tcphdr	*th = pd->hdr.tcp;
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
			NTOHS(mss);
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
		rt = rtalloc(sintosa(dst), RT_REPORT, rtableid);
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		dst6 = (struct sockaddr_in6 *)&ss;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		rt = rtalloc(sin6tosa(dst6), RT_REPORT, rtableid);
		break;
#endif /* INET6 */
	}

	if (rt && rt->rt_ifp) {
		mss = rt->rt_ifp->if_mtu - hlen - sizeof(struct tcphdr);
		mss = max(tcp_mssdflt, mss);
		rtfree(rt);
	}
	mss = min(mss, offer);
	mss = max(mss, 64);		/* sanity - at least max opt space */
	return (mss);
}

void
pf_set_rt_ifp(struct pf_state *s, struct pf_addr *saddr)
{
	struct pf_rule *r = s->rule.ptr;
	struct pf_src_node *sns[PF_SN_MAX];

	s->rt_kif = NULL;
	if (!r->rt)
		return;
	bzero(sns, sizeof(sns));
	switch (s->key[PF_SK_WIRE]->af) {
	case AF_INET:
		pf_map_addr(AF_INET, r, saddr, &s->rt_addr, NULL, sns,
		    &r->route, PF_SN_ROUTE);
		s->rt_kif = r->route.kif;
		s->natrule.ptr = r;
		break;
#ifdef INET6
	case AF_INET6:
		pf_map_addr(AF_INET6, r, saddr, &s->rt_addr, NULL, sns,
		    &r->route, PF_SN_ROUTE);
		s->rt_kif = r->route.kif;
		s->natrule.ptr = r;
		break;
#endif /* INET6 */
	}
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
	SHA512Update(&ctx, &pd->hdr.tcp->th_sport, sizeof(u_short));
	SHA512Update(&ctx, &pd->hdr.tcp->th_dport, sizeof(u_short));
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
	a->log |= r->log;
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
	do {					\
		if (t) {			\
			r = a;			\
			goto nextrule;		\
		}				\
	} while (0)

int
pf_test_rule(struct pf_pdesc *pd, struct pf_rule **rm, struct pf_state **sm,
    struct pf_rule **am, struct pf_ruleset **rsm)
{
	struct pf_rule		*r;
	struct pf_rule		*nr = NULL;
	struct pf_rule		*a = NULL;
	struct pf_ruleset	*arsm = NULL;
	struct pf_ruleset	*aruleset = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_rule_slist	 rules;
	struct pf_rule_item	*ri;
	struct pf_src_node	*sns[PF_SN_MAX];
	struct tcphdr		*th = pd->hdr.tcp;
	struct pf_state_key	*skw = NULL, *sks = NULL;
	struct pf_rule_actions	 act;
	u_short			 reason;
	int			 rewrite = 0;
	int			 tag = -1;
	int			 asd = 0;
	int			 match = 0;
	int			 state_icmp = 0, icmp_dir = 0;
	u_int16_t		 virtual_type, virtual_id;
	u_int8_t		 icmptype = 0, icmpcode = 0;

	bzero(&act, sizeof(act));
	bzero(sns, sizeof(sns));
	act.rtableid = pd->rdomain;
	SLIST_INIT(&rules);

	if (pd->dir == PF_IN && if_congested()) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

	switch (pd->virtual_proto) {
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpcode = pd->hdr.icmp->icmp_code;
		state_icmp = pf_icmp_mapping(pd, icmptype,
		    &icmp_dir, &virtual_id, &virtual_type);
		if (icmp_dir == PF_IN) {
			pd->osport = pd->nsport = virtual_id;
			pd->odport = pd->ndport = virtual_type;
		} else {
			pd->osport = pd->nsport = virtual_type;
			pd->odport = pd->ndport = virtual_id;
		}
		break;
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpcode = pd->hdr.icmp6->icmp6_code;
		state_icmp = pf_icmp_mapping(pd, icmptype,
		    &icmp_dir, &virtual_id, &virtual_type);
		if (icmp_dir == PF_IN) {
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
	r = TAILQ_FIRST(pf_main_ruleset.rules.active.ptr);
	while (r != NULL) {
		r->evaluations++;
		PF_TEST_ATTRIB((pfi_kif_match(r->kif, pd->kif) == r->ifnot),
			r->skip[PF_SKIP_IFP].ptr);
		PF_TEST_ATTRIB((r->direction && r->direction != pd->dir),
			r->skip[PF_SKIP_DIR].ptr);
		PF_TEST_ATTRIB((r->onrdomain >= 0  &&
		    (r->onrdomain == pd->rdomain) == r->ifnot),
			r->skip[PF_SKIP_RDOM].ptr);
		PF_TEST_ATTRIB((r->af && r->af != pd->af),
			r->skip[PF_SKIP_AF].ptr);
		PF_TEST_ATTRIB((r->proto && r->proto != pd->proto),
			r->skip[PF_SKIP_PROTO].ptr);
		PF_TEST_ATTRIB((PF_MISMATCHAW(&r->src.addr, &pd->nsaddr,
		    pd->naf, r->src.neg, pd->kif, act.rtableid)),
			r->skip[PF_SKIP_SRC_ADDR].ptr);
		PF_TEST_ATTRIB((PF_MISMATCHAW(&r->dst.addr, &pd->ndaddr, pd->af,
		    r->dst.neg, NULL, act.rtableid)),
			r->skip[PF_SKIP_DST_ADDR].ptr);

		switch (pd->virtual_proto) {
		case PF_VPROTO_FRAGMENT:
			/* tcp/udp only. port_op always 0 in other cases */
			PF_TEST_ATTRIB((r->src.port_op || r->dst.port_op),
				TAILQ_NEXT(r, entries));
			PF_TEST_ATTRIB((pd->proto == IPPROTO_TCP && r->flagset),
				TAILQ_NEXT(r, entries));
			/* icmp only. type/code always 0 in other cases */
			PF_TEST_ATTRIB((r->type || r->code),
				TAILQ_NEXT(r, entries));
			/* tcp/udp only. {uid|gid}.op always 0 in other cases */
			PF_TEST_ATTRIB((r->gid.op || r->uid.op),
				TAILQ_NEXT(r, entries));
			break;

		case IPPROTO_TCP:
			PF_TEST_ATTRIB(((r->flagset & th->th_flags) !=
			    r->flags),
				TAILQ_NEXT(r, entries));
			PF_TEST_ATTRIB((r->os_fingerprint != PF_OSFP_ANY &&
			    !pf_osfp_match(pf_osfp_fingerprint(pd),
			    r->os_fingerprint)),
				TAILQ_NEXT(r, entries));
			/* FALLTHROUGH */

		case IPPROTO_UDP:
			/* tcp/udp only. port_op always 0 in other cases */
			PF_TEST_ATTRIB((r->src.port_op &&
			    !pf_match_port(r->src.port_op, r->src.port[0],
			    r->src.port[1], pd->nsport)),
				r->skip[PF_SKIP_SRC_PORT].ptr);
			PF_TEST_ATTRIB((r->dst.port_op &&
			    !pf_match_port(r->dst.port_op, r->dst.port[0],
			    r->dst.port[1], pd->ndport)),
				r->skip[PF_SKIP_DST_PORT].ptr);
			/* tcp/udp only. uid.op always 0 in other cases */
			PF_TEST_ATTRIB((r->uid.op && (pd->lookup.done ||
			    (pd->lookup.done =
			    pf_socket_lookup(pd), 1)) &&
			    !pf_match_uid(r->uid.op, r->uid.uid[0],
			    r->uid.uid[1], pd->lookup.uid)),
				TAILQ_NEXT(r, entries));
			/* tcp/udp only. gid.op always 0 in other cases */
			PF_TEST_ATTRIB((r->gid.op && (pd->lookup.done ||
			    (pd->lookup.done =
			    pf_socket_lookup(pd), 1)) &&
			    !pf_match_gid(r->gid.op, r->gid.gid[0],
			    r->gid.gid[1], pd->lookup.gid)),
				TAILQ_NEXT(r, entries));
			break;

		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			/* icmp only. type always 0 in other cases */
			PF_TEST_ATTRIB((r->type && r->type != icmptype + 1),
				TAILQ_NEXT(r, entries));
			/* icmp only. type always 0 in other cases */
			PF_TEST_ATTRIB((r->code && r->code != icmpcode + 1),
				TAILQ_NEXT(r, entries));
			/* icmp only. don't create states on replies */
			PF_TEST_ATTRIB((r->keep_state && !state_icmp &&
			    (r->rule_flag & PFRULE_STATESLOPPY) == 0 &&
			    icmp_dir != PF_IN),
				TAILQ_NEXT(r, entries));
			break;

		default:
			break;
		}

		PF_TEST_ATTRIB((r->rule_flag & PFRULE_FRAGMENT &&
		    pd->virtual_proto != PF_VPROTO_FRAGMENT),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->tos && !(r->tos == pd->tos)),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->prob &&
		    r->prob <= arc4random_uniform(UINT_MAX - 1) + 1),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->match_tag && !pf_match_tag(pd->m, r, &tag)),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->rcv_kif && pf_match_rcvif(pd->m, r) ==
		    r->rcvifnot),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB((r->prio &&
		    (r->prio == PF_PRIO_ZERO ? 0 : r->prio) != pd->m->m_pkthdr.pf.prio),
			TAILQ_NEXT(r, entries));

		/* FALLTHROUGH */
		if (r->tag)
			tag = r->tag;
		if (r->anchor == NULL) {
			if (r->action == PF_MATCH) {
				if ((ri = pool_get(&pf_rule_item_pl,
				    PR_NOWAIT)) == NULL) {
					REASON_SET(&reason, PFRES_MEMORY);
					goto cleanup;
				}
				ri->r = r;
				/* order is irrelevant */
				SLIST_INSERT_HEAD(&rules, ri, entry);
				pf_rule_to_actions(r, &act);
				if (r->rule_flag & PFRULE_AFTO)
					pd->naf = r->naf;
				if (pf_get_transaddr(r, pd, sns, &nr) == -1) {
					REASON_SET(&reason, PFRES_TRANSLATE);
					goto cleanup;
				}
				if (r->log) {
					REASON_SET(&reason, PFRES_MATCH);
					PFLOG_PACKET(pd, reason, r, a, ruleset,
					    NULL);
				}
			} else {
				match = asd;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				arsm = aruleset;
			}

			if (act.log & PF_LOG_MATCHES)
				pf_log_matches(pd, r, a, ruleset, &rules);

			if (r->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		} else {
			aruleset = ruleset;
			pf_step_into_anchor(&asd, &ruleset, &r, &a);
		}

 nextrule:
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    &r, &a, &match))
			break;
	}
	r = *rm;	/* matching rule */
	a = *am;	/* rule that defines an anchor containing 'r' */
	ruleset = *rsm;	/* ruleset of the anchor defined by the rule 'a' */
	aruleset = arsm;/* ruleset of the 'a' rule itself */

	/* apply actions for last matching pass/block rule */
	pf_rule_to_actions(r, &act);
	if (r->rule_flag & PFRULE_AFTO)
		pd->naf = r->naf;
	if (pf_get_transaddr(r, pd, sns, &nr) == -1) {
		REASON_SET(&reason, PFRES_TRANSLATE);
		goto cleanup;
	}
	REASON_SET(&reason, PFRES_MATCH);

	if (r->log)
		PFLOG_PACKET(pd, reason, r, a, ruleset, NULL);
	if (act.log & PF_LOG_MATCHES)
		pf_log_matches(pd, r, a, ruleset, &rules);

	if (pd->virtual_proto != PF_VPROTO_FRAGMENT &&
	    (r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		if (pd->proto == IPPROTO_TCP &&
		    ((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(th->th_flags & TH_RST)) {
			u_int32_t	 ack = ntohl(th->th_seq) + pd->p_len;

			if (pf_check_proto_cksum(pd, pd->off,
			    pd->tot_len - pd->off, IPPROTO_TCP, pd->af))
				REASON_SET(&reason, PFRES_PROTCKSUM);
			else {
				if (th->th_flags & TH_SYN)
					ack++;
				if (th->th_flags & TH_FIN)
					ack++;
				pf_send_tcp(r, pd->af, pd->dst,
				    pd->src, th->th_dport, th->th_sport,
				    ntohl(th->th_ack), ack, TH_RST|TH_ACK, 0, 0,
				    r->return_ttl, 1, 0, pd->rdomain,
				    pd->eh, pd->kif->pfik_ifp);
			}
		} else if ((pd->proto != IPPROTO_ICMP ||
		    ICMP_INFOTYPE(icmptype)) && pd->af == AF_INET &&
		    r->return_icmp)
			pf_send_icmp(pd->m, r->return_icmp >> 8,
			    r->return_icmp & 255, pd->af, r, pd->rdomain);
		else if ((pd->proto != IPPROTO_ICMPV6 ||
		    (icmptype >= ICMP6_ECHO_REQUEST &&
		    icmptype != ND_REDIRECT)) && pd->af == AF_INET6 &&
		    r->return_icmp6)
			pf_send_icmp(pd->m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, pd->af, r, pd->rdomain);
	}

	if (r->action == PF_DROP)
		goto cleanup;

	pf_tag_packet(pd->m, tag, act.rtableid);
	if (act.rtableid >= 0 &&
	    rtable_l2(act.rtableid) != pd->rdomain)
		pd->destchg = 1;

	if (r->action == PF_PASS && pd->badopts && ! r->allow_opts) {
		REASON_SET(&reason, PFRES_IPOPTIONS);
		pd->pflog |= PF_LOG_FORCE;
		DPFPRINTF(LOG_NOTICE, "dropping packet with "
		    "ip/ipv6 options in pf_test_rule()");
		goto cleanup;
	}

	if (pd->virtual_proto != PF_VPROTO_FRAGMENT
	    && !state_icmp && r->keep_state) {
		int action;

		if (r->rule_flag & PFRULE_SRCTRACK &&
		    pf_insert_src_node(&sns[PF_SN_NONE], r, PF_SN_NONE, pd->af,
		    pd->src, NULL, 0) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}

		if (r->max_states && (r->states_cur >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}

		action = pf_create_state(pd, r, a, nr, &skw, &sks, &rewrite,
		    sm, tag, &rules, &act, sns);

		if (action != PF_PASS)
			return (action);
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
			    virtual_type, icmp_dir);
		}
	} else {
		while ((ri = SLIST_FIRST(&rules))) {
			SLIST_REMOVE_HEAD(&rules, entry);
			pool_put(&pf_rule_item_pl, ri);
		}
	}

	/* copy back packet headers if needed */
	if (rewrite && pd->hdrlen) {
		pf_cksum(pd, pd->m);
		m_copyback(pd->m, pd->off, pd->hdrlen, pd->hdr.any, M_NOWAIT);
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
#endif

	if (r->rule_flag & PFRULE_ONCE)
		pf_purge_rule(ruleset, r, aruleset, a);

#if INET && INET6
	if (rewrite && skw->af != sks->af)
		return (PF_AFRT);
#endif /* INET && INET6 */

	return (PF_PASS);

cleanup:
	while ((ri = SLIST_FIRST(&rules))) {
		SLIST_REMOVE_HEAD(&rules, entry);
		pool_put(&pf_rule_item_pl, ri);
	}

	return (PF_DROP);
}

static __inline int
pf_create_state(struct pf_pdesc *pd, struct pf_rule *r, struct pf_rule *a,
    struct pf_rule *nr, struct pf_state_key **skw, struct pf_state_key **sks,
    int *rewrite, struct pf_state **sm, int tag, struct pf_rule_slist *rules,
    struct pf_rule_actions *act, struct pf_src_node *sns[PF_SN_MAX])
{
	struct pf_state		*s = NULL;
	struct tcphdr		*th = pd->hdr.tcp;
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
	memcpy(&s->match_rules, rules, sizeof(s->match_rules));
	STATE_INC_COUNTERS(s);
	if (r->allow_opts)
		s->state_flags |= PFSTATE_ALLOWOPTS;
	if (r->rule_flag & PFRULE_STATESLOPPY)
		s->state_flags |= PFSTATE_SLOPPY;
	if (r->rule_flag & PFRULE_PFLOW)
		s->state_flags |= PFSTATE_PFLOW;
	s->log = act->log & PF_LOG_ALL;
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
#endif
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
			pf_change_a(pd, &th->th_seq,
			    htonl(s->src.seqlo + s->src.seqdiff));
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
		s->src.state = TCPS_SYN_SENT;
		s->dst.state = TCPS_CLOSED;
		s->timeout = PFTM_TCP_FIRST_PACKET;
		break;
	case IPPROTO_UDP:
		s->src.state = PFUDPS_SINGLE;
		s->dst.state = PFUDPS_NO_TRAFFIC;
		s->timeout = PFTM_UDP_FIRST_PACKET;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif
		s->timeout = PFTM_ICMP_FIRST_PACKET;
		break;
	default:
		s->src.state = PFOTHERS_SINGLE;
		s->dst.state = PFOTHERS_NO_TRAFFIC;
		s->timeout = PFTM_OTHER_FIRST_PACKET;
	}

	s->creation = time_uptime;
	s->expire = time_uptime;

	if (pd->proto == IPPROTO_TCP) {
		if (s->state_flags & PFSTATE_SCRUB_TCP &&
		    pf_normalize_tcp_init(pd, &s->src, &s->dst)) {
			REASON_SET(&reason, PFRES_MEMORY);
			goto csfailed;
		}
		if (s->state_flags & PFSTATE_SCRUB_TCP && s->src.scrub &&
		    pf_normalize_tcp_stateful(pd, &reason, s, &s->src, &s->dst,
		    rewrite)) {
			/* This really shouldn't happen!!! */
			DPFPRINTF(LOG_ERR,
			    "pf_normalize_tcp_stateful failed on first pkt");
			goto csfailed;
		}
	}
	s->direction = pd->dir;

	if (pf_state_key_setup(pd, skw, sks, act->rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto csfailed;
	}

	if (pf_state_insert(BOUND_IFACE(r, pd->kif), skw, sks, s)) {
		pf_state_key_detach(s, PF_SK_STACK);
		pf_state_key_detach(s, PF_SK_WIRE);
		*sks = *skw = NULL;
		REASON_SET(&reason, PFRES_STATEINS);
		goto csfailed;
	} else
		*sm = s;

	/* attach src nodes late, otherwise cleanup on error nontrivial */
	for (i = 0; i < PF_SN_MAX; i++)
		if (sns[i] != NULL) {
			struct pf_sn_item	*sni;

			sni = pool_get(&pf_sn_item_pl, PR_NOWAIT);
			if (sni == NULL) {
				REASON_SET(&reason, PFRES_MEMORY);
				pf_src_tree_remove_state(s);
				STATE_DEC_COUNTERS(s);
				pool_put(&pf_state_pl, s);
				return (PF_DROP);
			}
			sni->sn = sns[i];
			SLIST_INSERT_HEAD(&s->src_nodes, sni, next);
			sni->sn->states++;
		}

	pf_set_rt_ifp(s, pd->src);	/* needs s->state_key set */
	if (tag > 0) {
		pf_tag_ref(tag);
		s->tag = tag;
	}
	if (pd->proto == IPPROTO_TCP && (th->th_flags & (TH_SYN|TH_ACK)) ==
	    TH_SYN && r->keep_state == PF_STATE_SYNPROXY) {
		int rtid = pd->rdomain;
		if (act->rtableid >= 0)
			rtid = act->rtableid;
		s->src.state = PF_TCPS_PROXY_SRC;
		s->src.seqhi = htonl(arc4random());
		/* Find mss option */
		mss = pf_get_mss(pd);
		mss = pf_calc_mss(pd->src, pd->af, rtid, mss);
		mss = pf_calc_mss(pd->dst, pd->af, rtid, mss);
		s->src.mss = mss;
		pf_send_tcp(r, pd->af, pd->dst, pd->src, th->th_dport,
		    th->th_sport, s->src.seqhi, ntohl(th->th_seq) + 1,
		    TH_SYN|TH_ACK, 0, s->src.mss, 0, 1, 0, pd->rdomain,
		    NULL, NULL);
		REASON_SET(&reason, PFRES_SYNPROXY);
		return (PF_SYNPROXY_DROP);
	}

	return (PF_PASS);

csfailed:
	for (i = 0; i < PF_SN_MAX; i++)
		if (sns[i] != NULL)
			pf_remove_src_node(sns[i]);
	if (s) {
		pf_normalize_tcp_cleanup(s);	/* safe even w/o init */
		pf_src_tree_remove_state(s);
		STATE_DEC_COUNTERS(s);
		pool_put(&pf_state_pl, s);
	}

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
	case IPPROTO_TCP:
		if (afto || PF_ANEQ(saddr, pd->src, pd->af) ||
		    *pd->sport != sport) {
			pf_change_ap(pd, pd->src, pd->sport, saddr, sport,
			    pd->naf);
			rewrite = 1;
		}
		if (afto || PF_ANEQ(daddr, pd->dst, pd->af) ||
		    *pd->dport != dport) {
			pf_change_ap(pd, pd->dst, pd->dport, daddr, dport,
			    pd->naf);
			rewrite = 1;
		}
		break;

	case IPPROTO_UDP:
		if (afto || PF_ANEQ(saddr, pd->src, pd->af) ||
		    *pd->sport != sport) {
			pf_change_ap(pd, pd->src, pd->sport, saddr, sport,
			    pd->naf);
			rewrite = 1;
		}
		if (afto || PF_ANEQ(daddr, pd->dst, pd->af) ||
		    *pd->dport != dport) {
			pf_change_ap(pd, pd->dst, pd->dport, daddr, dport,
			    pd->naf);
			rewrite = 1;
		}
		break;

	case IPPROTO_ICMP:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET)
			return (0);

		if (afto) {
#ifdef INET6
			if (pf_translate_icmp_af(AF_INET6, pd->hdr.icmp))
				return (0);
			pd->proto = IPPROTO_ICMPV6;
			rewrite = 1;
#endif /* INET6 */
		} else {
			if (PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a(pd, &pd->src->v4.s_addr,
				    saddr->v4.s_addr);
				rewrite = 1;
			}
			if (PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a(pd, &pd->dst->v4.s_addr,
				    daddr->v4.s_addr);
				rewrite = 1;
			}
		}
		if (virtual_type == htons(ICMP_ECHO)) {
			u_int16_t icmpid = (icmp_dir == PF_IN) ? sport : dport;

			if (icmpid != pd->hdr.icmp->icmp_id) {
				if (pd->csum_status == PF_CSUM_UNKNOWN)
					pf_check_proto_cksum(pd, pd->off,
					    pd->tot_len - pd->off, pd->proto,
					    pd->af);
				pd->hdr.icmp->icmp_id = icmpid;
				rewrite = 1;
			}
		}
		break;

#ifdef INET6
	case IPPROTO_ICMPV6:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET6)
			return (0);

		if (afto) {
			/* ip_sum will be recalculated in pf_translate_af */
			if (pf_translate_icmp_af(AF_INET, pd->hdr.icmp6))
				return (0);
			pd->proto = IPPROTO_ICMP;
			rewrite = 1;
		} else {
			if (PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a6(pd, pd->src, saddr);
				rewrite = 1;
			}
			if (PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a6(pd, pd->dst, daddr);
				rewrite = 1;
			}
		}
		if (virtual_type == htons(ICMP6_ECHO_REQUEST)) {
			u_int16_t icmpid = (icmp_dir == PF_IN) ? sport : dport;

			if (icmpid != pd->hdr.icmp6->icmp6_id) {
				if (pd->csum_status == PF_CSUM_UNKNOWN)
					pf_check_proto_cksum(pd, pd->off,
					    pd->tot_len - pd->off, pd->proto,
					    pd->af);
				pd->hdr.icmp6->icmp6_id = icmpid;
				rewrite = 1;
			}
		}
		break;
#endif /* INET6 */

	default:
		switch (pd->af) {
		case AF_INET:
			if (!afto && PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a(pd, &pd->src->v4.s_addr,
				    saddr->v4.s_addr);
				rewrite = 1;
			}
			if (!afto && PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a(pd, &pd->dst->v4.s_addr,
				    daddr->v4.s_addr);
				rewrite = 1;
			}
			break;
#ifdef INET6
		case AF_INET6:
			if (!afto && PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a6(pd, pd->src, saddr);
				rewrite = 1;
			}
			if (!afto && PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a6(pd, pd->dst, daddr);
				rewrite = 1;
			}
			break;
#endif /* INET6 */
		}
	}
	return (rewrite);
}

int
pf_tcp_track_full(struct pf_pdesc *pd, struct pf_state_peer *src,
    struct pf_state_peer *dst, struct pf_state **state, u_short *reason,
    int *copyback)
{
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 win = ntohs(th->th_win);
	u_int32_t		 ack, end, data_end, seq, orig_seq;
	u_int8_t		 sws, dws;
	int			 ackskew;

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
			if (pf_normalize_tcp_init(pd, src, dst)) {
				REASON_SET(reason, PFRES_MEMORY);
				return (PF_DROP);
			}
		}

		/* Deferred generation of sequence number modulator */
		if (dst->seqdiff && !src->seqdiff) {
			/* use random iss for the TCP server */
			while ((src->seqdiff = arc4random() - seq) == 0)
				;
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_change_a(pd, &th->th_seq, htonl(seq + src->seqdiff));
			pf_change_a(pd, &th->th_ack, htonl(ack));
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
			src->state = TCPS_SYN_SENT;

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
			pf_change_a(pd, &th->th_seq, htonl(seq + src->seqdiff));
			pf_change_a(pd, &th->th_ack, htonl(ack));
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
				src->state = TCPS_SYN_SENT;
		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				src->state = TCPS_CLOSING;
		if (th->th_flags & TH_ACK) {
			if (dst->state == TCPS_SYN_SENT) {
				dst->state = TCPS_ESTABLISHED;
				if (src->state == TCPS_ESTABLISHED &&
				    !SLIST_EMPTY(&(*state)->src_nodes) &&
				    pf_src_connlimit(state)) {
					REASON_SET(reason, PFRES_SRCLIMIT);
					return (PF_DROP);
				}
			} else if (dst->state == TCPS_CLOSING)
				dst->state = TCPS_FIN_WAIT_2;
		}
		if (th->th_flags & TH_RST)
			src->state = dst->state = TCPS_TIME_WAIT;

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
				src->state = TCPS_CLOSING;
		if (th->th_flags & TH_RST)
			src->state = dst->state = TCPS_TIME_WAIT;

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
				    pd->rdomain, pd->eh, pd->kif->pfik_ifp);
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
pf_tcp_track_sloppy(struct pf_pdesc *pd, struct pf_state_peer *src,
    struct pf_state_peer *dst, struct pf_state **state, u_short *reason)
{
	struct tcphdr		*th = pd->hdr.tcp;

	if (th->th_flags & TH_SYN)
		if (src->state < TCPS_SYN_SENT)
			src->state = TCPS_SYN_SENT;
	if (th->th_flags & TH_FIN)
		if (src->state < TCPS_CLOSING)
			src->state = TCPS_CLOSING;
	if (th->th_flags & TH_ACK) {
		if (dst->state == TCPS_SYN_SENT) {
			dst->state = TCPS_ESTABLISHED;
			if (src->state == TCPS_ESTABLISHED &&
			    !SLIST_EMPTY(&(*state)->src_nodes) &&
			    pf_src_connlimit(state)) {
				REASON_SET(reason, PFRES_SRCLIMIT);
				return (PF_DROP);
			}
		} else if (dst->state == TCPS_CLOSING) {
			dst->state = TCPS_FIN_WAIT_2;
		} else if (src->state == TCPS_SYN_SENT &&
		    dst->state < TCPS_SYN_SENT) {
			/*
			 * Handle a special sloppy case where we only see one
			 * half of the connection. If there is a ACK after
			 * the initial SYN without ever seeing a packet from
			 * the destination, set the connection to established.
			 */
			dst->state = src->state = TCPS_ESTABLISHED;
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
			dst->state = TCPS_CLOSING;
		}
	}
	if (th->th_flags & TH_RST)
		src->state = dst->state = TCPS_TIME_WAIT;

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
		struct tcphdr	*th = pd->hdr.tcp;

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
			    0, pd->rdomain, NULL, NULL);
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
			(*state)->src.state = PF_TCPS_PROXY_DST;
	}
	if ((*state)->src.state == PF_TCPS_PROXY_DST) {
		struct tcphdr	*th = pd->hdr.tcp;

		if (pd->dir == (*state)->direction) {
			if (((th->th_flags & (TH_SYN|TH_ACK)) != TH_ACK) ||
			    (ntohl(th->th_ack) != (*state)->src.seqhi + 1) ||
			    (ntohl(th->th_seq) != (*state)->src.seqlo + 1)) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return (PF_DROP);
			}
			(*state)->src.max_win = MAX(ntohs(th->th_win), 1);
			if ((*state)->dst.seqhi == 1)
				(*state)->dst.seqhi = htonl(arc4random());
			pf_send_tcp((*state)->rule.ptr, pd->af,
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->dst.seqhi, 0, TH_SYN, 0,
			    (*state)->src.mss, 0, 0, (*state)->tag,
			    sk->rdomain, NULL, NULL);
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
			    (*state)->tag, pd->rdomain, NULL, NULL);
			pf_send_tcp((*state)->rule.ptr, pd->af,
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->src.seqhi + 1, (*state)->src.seqlo + 1,
			    TH_ACK, (*state)->dst.max_win, 0, 0, 1,
			    0, sk->rdomain, NULL, NULL);
			(*state)->src.seqdiff = (*state)->dst.seqhi -
			    (*state)->src.seqlo;
			(*state)->dst.seqdiff = (*state)->src.seqhi -
			    (*state)->dst.seqlo;
			(*state)->src.seqhi = (*state)->src.seqlo +
			    (*state)->dst.max_win;
			(*state)->dst.seqhi = (*state)->dst.seqlo +
			    (*state)->src.max_win;
			(*state)->src.wscale = (*state)->dst.wscale = 0;
			(*state)->src.state = (*state)->dst.state =
			    TCPS_ESTABLISHED;
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
	}
	return (PF_PASS);
}

int
pf_test_state(struct pf_pdesc *pd, struct pf_state **state, u_short *reason)
{
	struct pf_state_key_cmp	 key;
	int			 copyback = 0;
	struct pf_state_peer	*src, *dst;
	int			 action = PF_PASS;
	struct inpcb		*inp;

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
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	switch (pd->virtual_proto) {
	case IPPROTO_TCP:
		if ((action = pf_synproxy(pd, state, reason)) != PF_PASS)
			return (action); 
		if (((pd->hdr.tcp->th_flags & (TH_SYN|TH_ACK)) == TH_SYN) &&
		    dst->state >= TCPS_FIN_WAIT_2 &&
		    src->state >= TCPS_FIN_WAIT_2) {
			if (pf_status.debug >= LOG_NOTICE) {
				log(LOG_NOTICE, "pf: state reuse ");
				pf_print_state(*state);
				pf_print_flags(pd->hdr.tcp->th_flags);
				addlog("\n");
			}
			/* XXX make sure it's the same direction ?? */
			(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
			pf_unlink_state(*state);
			*state = NULL;
			pd->m->m_pkthdr.pf.inp = inp;
			return (PF_DROP);
		}

		if ((*state)->state_flags & PFSTATE_SLOPPY) {
			if (pf_tcp_track_sloppy(pd, src, dst, state, reason) ==
			    PF_DROP)
				return (PF_DROP);
		} else {
			int	ret;

			if (PF_REVERSED_KEY((*state)->key, pd->af))
				ret = pf_tcp_track_full(pd, dst, src, state,
				    reason, &copyback);
			else
				ret = pf_tcp_track_full(pd, src, dst, state,
				    reason, &copyback);
			if (ret == PF_DROP)
				return (PF_DROP);
		}
		break;
	case IPPROTO_UDP:
		/* update states */
		if (src->state < PFUDPS_SINGLE)
			src->state = PFUDPS_SINGLE;
		if (dst->state == PFUDPS_SINGLE)
			dst->state = PFUDPS_MULTIPLE;

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
			src->state = PFOTHERS_SINGLE;
		if (dst->state == PFOTHERS_SINGLE)
			dst->state = PFOTHERS_MULTIPLE;

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

		if (afto || PF_ANEQ(pd->src, &nk->addr[sidx], pd->af) ||
		    nk->port[sidx] != pd->osport)
			pf_change_ap(pd, pd->src, pd->sport,
			    &nk->addr[sidx], nk->port[sidx], nk->af);

		if (afto || PF_ANEQ(pd->dst, &nk->addr[didx], pd->af) ||
		    pd->rdomain != nk->rdomain)
			pd->destchg = 1;

		if (afto || PF_ANEQ(pd->dst, &nk->addr[didx], pd->af) ||
		    nk->port[didx] != pd->odport)
			pf_change_ap(pd, pd->dst, pd->dport,
			    &nk->addr[didx], nk->port[didx], nk->af);

#if INET && INET6
		if (afto) {
			PF_ACPY(&pd->nsaddr, &nk->addr[sidx], nk->af);
			PF_ACPY(&pd->ndaddr, &nk->addr[didx], nk->af);
			pd->naf = nk->af;
			action = PF_AFRT;
		}
#endif /* INET && INET6 */

		pd->m->m_pkthdr.ph_rtableid = nk->rdomain;
		copyback = 1;
	}

	if (copyback && pd->hdrlen > 0) {
		pf_cksum(pd, pd->m);
		m_copyback(pd->m, pd->off, pd->hdrlen, pd->hdr.any, M_NOWAIT);
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
	struct pf_addr  *saddr = pd->src, *daddr = pd->dst;
	u_int16_t	 virtual_id, virtual_type;
	u_int8_t	 icmptype;
	int		 icmp_dir, iidx, ret, copyback = 0;

	struct pf_state_key_cmp key;

	switch (pd->proto) {
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		break;
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		break;
#endif /* INET6 */
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

			if (pd->rdomain != nk->rdomain)
				pd->destchg = 1;
			pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

			switch (pd->af) {
			case AF_INET:
#ifdef INET6
				if (afto) {
					if (pf_translate_icmp_af(AF_INET6,
					    pd->hdr.icmp))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMPV6;
				}
#endif /* INET6 */
				if (!afto && PF_ANEQ(pd->src,
				    &nk->addr[sidx], AF_INET))
					pf_change_a(pd, &saddr->v4.s_addr,
					    nk->addr[sidx].v4.s_addr);

				if (!afto && PF_ANEQ(pd->dst,
				    &nk->addr[didx], AF_INET)) {
					pf_change_a(pd, &daddr->v4.s_addr,
					    nk->addr[didx].v4.s_addr);
					pd->destchg = 1;
				}

				if (nk->port[iidx] !=  pd->hdr.icmp->icmp_id) {
					if (pd->csum_status == PF_CSUM_UNKNOWN)
						pf_check_proto_cksum(pd,
						    pd->off, pd->tot_len -
						    pd->off, pd->proto, pd->af);
					pd->hdr.icmp->icmp_id = nk->port[iidx];
				}

				m_copyback(pd->m, pd->off, ICMP_MINLEN,
				    pd->hdr.icmp, M_NOWAIT);
				copyback = 1;
				break;
#ifdef INET6
			case AF_INET6:
				if (afto) {
					if (pf_translate_icmp_af(AF_INET,
					    pd->hdr.icmp6))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMP;
				}
				if (!afto && PF_ANEQ(pd->src,
				    &nk->addr[sidx], AF_INET6))
					pf_change_a6(pd, saddr,
					    &nk->addr[sidx]);

				if (!afto && PF_ANEQ(pd->dst,
				    &nk->addr[didx], AF_INET6)) {
					pf_change_a6(pd, daddr,
					    &nk->addr[didx]);
					pd->destchg = 1;
				}

				if (nk->port[iidx] != pd->hdr.icmp6->icmp6_id) {
					if (pd->csum_status == PF_CSUM_UNKNOWN)
						pf_check_proto_cksum(pd,
						    pd->off, pd->tot_len -
						    pd->off, pd->proto, pd->af);
					pd->hdr.icmp6->icmp6_id =
					    nk->port[iidx];
				}

				m_copyback(pd->m, pd->off,
				    sizeof(struct icmp6_hdr), pd->hdr.icmp6,
				    M_NOWAIT);
				copyback = 1;
				break;
#endif /* INET6 */
			}
#if INET && INET6
			if (afto) {
				PF_ACPY(&pd->nsaddr, &nk->addr[sidx], nk->af);
				PF_ACPY(&pd->ndaddr, &nk->addr[didx], nk->af);
				pd->naf = nk->af;
				return (PF_AFRT);
			}
#endif /* INET && INET6 */
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
		bzero(&pd2, sizeof(pd2));
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
			pd2.off = ipoff2 + (h2.ip_hl << 2);

			pd2.proto = h2.ip_p;
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
		}

		switch (pd2.proto) {
		case IPPROTO_TCP: {
			struct tcphdr		 th;
			u_int32_t		 seq;
			struct pf_state_peer	*src, *dst;
			u_int8_t		 dws;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(pd2.m, pd2.off, &th, 8, NULL, reason,
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
			key.port[pd2.sidx] = th.th_sport;
			key.port[pd2.didx] = th.th_dport;

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
			seq = ntohl(th.th_seq) - src->seqdiff;
			if (src->seqdiff) {
				pf_change_a(pd, &th.th_seq, htonl(seq));
				copyback = 1;
			}

			if (!((*state)->state_flags & PFSTATE_SLOPPY) &&
			    (!SEQ_GEQ(src->seqhi, seq) || !SEQ_GEQ(seq,
			    src->seqlo - (dst->max_win << dws)))) {
				if (pf_status.debug >= LOG_NOTICE) {
					log(LOG_NOTICE,
					    "pf: BAD ICMP %d:%d ",
					    icmptype, pd->hdr.icmp->icmp_code);
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
					    icmptype, pd->hdr.icmp->icmp_code);
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

#if INET && INET6
				if (afto) {
					if (pf_translate_icmp_af(nk->af,
					    pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					if (nk->af == AF_INET)
						pd->proto = IPPROTO_ICMP;
					else
						pd->proto = IPPROTO_ICMPV6;
					pf_change_ap(pd, pd2.src, &th.th_sport,
					    &nk->addr[pd2.sidx], nk->port[sidx],
					    nk->af);
					pf_change_ap(pd, pd2.dst, &th.th_dport,
					    &nk->addr[pd2.didx], nk->port[didx],
					    nk->af);
					m_copyback(pd2.m, pd2.off, 8, &th,
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
#endif
				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != th.th_sport)
					pf_change_icmp(pd, pd2.src,
					    &th.th_sport, daddr,
					    &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != th.th_dport)
					pf_change_icmp(pd, pd2.dst,
					    &th.th_dport, saddr,
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], pd2.af);
				copyback = 1;
			}

			if (copyback) {
				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				m_copyback(pd2.m, pd2.off, 8, &th, M_NOWAIT);
			}
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr		uh;

			if (!pf_pull_hdr(pd2.m, pd2.off, &uh, sizeof(uh),
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
			key.port[pd2.sidx] = uh.uh_sport;
			key.port[pd2.didx] = uh.uh_dport;

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

#if INET && INET6
				if (afto) {
					if (pf_translate_icmp_af(nk->af,
					    pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					if (nk->af == AF_INET)
						pd->proto = IPPROTO_ICMP;
					else
						pd->proto = IPPROTO_ICMPV6;
					pf_change_ap(pd, pd2.src, &uh.uh_sport,
					    &nk->addr[pd2.sidx], nk->port[sidx],
					    nk->af);
					pf_change_ap(pd, pd2.dst, &uh.uh_dport,
					    &nk->addr[pd2.didx], nk->port[didx],
					    nk->af);
					m_copyback(pd2.m, pd2.off, sizeof(uh),
					    &uh, M_NOWAIT);
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
#endif /* INET && INET6 */

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != uh.uh_sport)
					pf_change_icmp(pd, pd2.src,
					    &uh.uh_sport, daddr,
					    &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != uh.uh_dport)
					pf_change_icmp(pd, pd2.dst,
					    &uh.uh_dport, saddr,
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], pd2.af);

				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				uh.uh_sum = 0;
				m_copyback(pd2.m, pd2.off, sizeof(uh), &uh,
				    M_NOWAIT);
				copyback = 1;
			}
			break;
		}
		case IPPROTO_ICMP: {
			struct icmp		iih;

			if (pd2.af != AF_INET) {
				REASON_SET(reason, PFRES_NORM);
				return (PF_DROP);
			}

			if (!pf_pull_hdr(pd2.m, pd2.off, &iih, ICMP_MINLEN,
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp)");
				return (PF_DROP);
			}

			pd2.hdr.icmp = &iih;
			pf_icmp_mapping(&pd2, iih.icmp_type,
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
					if (pf_translate_icmp_af(nk->af,
					    pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMPV6;
					if (pf_translate_icmp_af(nk->af, &iih))
						return (PF_DROP);
					if (virtual_type == htons(ICMP_ECHO) &&
					    nk->port[iidx] != iih.icmp_id)
						iih.icmp_id = nk->port[iidx];
					m_copyback(pd2.m, pd2.off, ICMP_MINLEN,
					    &iih, M_NOWAIT);
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
				    nk->port[iidx] != iih.icmp_id))
					pf_change_icmp(pd, pd2.src,
					    (virtual_type == htons(ICMP_ECHO)) ?
					    &iih.icmp_id : NULL,
					    daddr, &nk->addr[pd2.sidx],
					    (virtual_type == htons(ICMP_ECHO)) ?
					    nk->port[iidx] : 0, AF_INET);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd, pd2.dst, NULL,
					    saddr, &nk->addr[pd2.didx], 0,
					    AF_INET);

				m_copyback(pd->m, pd->off, ICMP_MINLEN,
				    pd->hdr.icmp, M_NOWAIT);
				m_copyback(pd2.m, ipoff2, sizeof(h2), &h2,
				    M_NOWAIT);
				m_copyback(pd2.m, pd2.off, ICMP_MINLEN, &iih,
				    M_NOWAIT);
				copyback = 1;
			}
			break;
		}
#ifdef INET6
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr	iih;

			if (pd2.af != AF_INET6) {
				REASON_SET(reason, PFRES_NORM);
				return (PF_DROP);
			}

			if (!pf_pull_hdr(pd2.m, pd2.off, &iih,
			    sizeof(struct icmp6_hdr), NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp6)");
				return (PF_DROP);
			}

			pd2.hdr.icmp6 = &iih;
			pf_icmp_mapping(&pd2, iih.icmp6_type,
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
					if (pf_translate_icmp_af(nk->af,
					    pd->hdr.icmp))
						return (PF_DROP);
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					if (pf_change_icmp_af(pd->m, ipoff2,
					    pd, &pd2, &nk->addr[sidx],
					    &nk->addr[didx], pd->af, nk->af))
						return (PF_DROP);
					pd->proto = IPPROTO_ICMP;
					if (pf_translate_icmp_af(nk->af, &iih))
						return (PF_DROP);
					if (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST) &&
					    nk->port[iidx] != iih.icmp6_id)
						iih.icmp6_id = nk->port[iidx];
					m_copyback(pd2.m, pd2.off,
					    sizeof(struct icmp6_hdr), &iih,
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
				    nk->port[pd2.sidx] != iih.icmp6_id))
					pf_change_icmp(pd, pd2.src,
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? &iih.icmp6_id : NULL,
					    daddr, &nk->addr[pd2.sidx],
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? nk->port[iidx] : 0, AF_INET6);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd, pd2.dst, NULL,
					    saddr, &nk->addr[pd2.didx], 0,
					    AF_INET6);

				m_copyback(pd->m, pd->off,
				    sizeof(struct icmp6_hdr), pd->hdr.icmp6,
				    M_NOWAIT);
				m_copyback(pd2.m, ipoff2, sizeof(h2_6), &h2_6,
				    M_NOWAIT);
				m_copyback(pd2.m, pd2.off,
				    sizeof(struct icmp6_hdr), &iih, M_NOWAIT);
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
					pf_change_icmp(pd, pd2.src, NULL,
					    daddr, &nk->addr[pd2.sidx], 0,
					    pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				pd->m->m_pkthdr.ph_rtableid = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd, pd2.dst, NULL,
					    saddr, &nk->addr[pd2.didx], 0,
					    pd2.af);

				switch (pd2.af) {
				case AF_INET:
					m_copyback(pd->m, pd->off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(pd2.m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#ifdef INET6
				case AF_INET6:
					m_copyback(pd->m, pd->off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
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
		pf_cksum(pd, pd->m);
		m_copyback(pd->m, pd->off, pd->hdrlen, pd->hdr.any, M_NOWAIT);
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
		if (m->m_pkthdr.len < off + len ||
		    ntohs(h->ip_len) < off + len) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return (NULL);
		}
		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h = mtod(m, struct ip6_hdr *);

		if (m->m_pkthdr.len < off + len ||
		    (ntohs(h->ip6_plen) + sizeof(struct ip6_hdr)) <
		    (unsigned)(off + len)) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return (NULL);
		}
		break;
	}
#endif /* INET6 */
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
#endif
	struct rtentry		*rt, *rt0 = NULL;
	struct ifnet		*ifp;

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

	rt0 = rtalloc((struct sockaddr *)&ss, RT_REPORT, rtableid);
	if (rt0 != NULL) {
		/* No interface given, this is a no-route check */
		if (kif == NULL)
			goto out;

		if (kif->pfik_ifp == NULL) {
			ret = 0;
			goto out;
		}

		/* Perform uRPF check if passed input interface */
		ret = 0;
		rt = rt0;
		do {
			if (rt->rt_ifp->if_type == IFT_CARP)
				ifp = rt->rt_ifp->if_carpdev;
			else
				ifp = rt->rt_ifp;

			if (kif->pfik_ifp == ifp)
				ret = 1;
			rt = rt_mpath_next(rt);
		} while (check_mpath == 1 && rt != NULL && ret == 0);
	} else
		ret = 0;
out:
	if (rt0 != NULL)
		rtfree(rt0);
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
#endif
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

	rt = rtalloc((struct sockaddr *)&ss, RT_REPORT|RT_RESOLVE, rtableid);
	if (rt != NULL) {
		if (rt->rt_labelid == aw->v.rtlabel)
			ret = 1;
		rtfree(rt);
	}

	return (ret);
}

void
pf_route(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s)
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

	if (m == NULL || *m == NULL || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL)
		panic("pf_route: invalid parameters");

	if ((*m)->m_pkthdr.pf.routed++ > 3) {
		m0 = *m;
		*m = NULL;
		goto bad;
	}

	if (r->rt == PF_DUPTO) {
		if ((m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT)) == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	if (m0->m_len < sizeof(struct ip)) {
		DPFPRINTF(LOG_ERR,
		    "pf_route: m0->m_len < sizeof(struct ip)");
		goto bad;
	}

	ip = mtod(m0, struct ip *);

	memset(&sin, 0, sizeof(sin));
	dst = &sin;
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;
	rtableid = m0->m_pkthdr.ph_rtableid;

	if (!r->rt) {
		rt = rtalloc(sintosa(dst), RT_REPORT|RT_RESOLVE, rtableid);
		if (rt == NULL) {
			ipstat.ips_noroute++;
			goto bad;
		}

		ifp = rt->rt_ifp;
		rt->rt_use++;

		if (rt->rt_flags & RTF_GATEWAY)
			dst = satosin(rt->rt_gateway);

		m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	} else {
		if (s == NULL) {
			bzero(sns, sizeof(sns));
			if (pf_map_addr(AF_INET, r,
			    (struct pf_addr *)&ip->ip_src,
			    &naddr, NULL, sns, &r->route, PF_SN_ROUTE)) {
				DPFPRINTF(LOG_ERR,
				    "pf_route: pf_map_addr() failed.");
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
	}
	if (ifp == NULL)
		goto bad;


	if (oifp != ifp) {
		if (pf_test(AF_INET, PF_OUT, ifp, &m0, NULL) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
		if (m0->m_len < sizeof(struct ip)) {
			DPFPRINTF(LOG_ERR,
			    "pf_route: m0->m_len < sizeof(struct ip)");
			goto bad;
		}
		ip = mtod(m0, struct ip *);
	}

	in_proto_cksum_out(m0, ifp);

	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		ip->ip_sum = 0;
		if (ifp->if_capabilities & IFCAP_CSUM_IPv4)
			m0->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
		else {
			ipstat.ips_outswcsum++;
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
		}
		error = (*ifp->if_output)(ifp, m0, sintosa(dst), NULL);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
		ipstat.ips_cantfrag++;
		if (r->rt != PF_DUPTO) {
			icmp_error(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			    ifp->if_mtu);
			goto done;
		} else
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
			error = (*ifp->if_output)(ifp, m0, sintosa(dst),
			    NULL);
		else
			m_freem(m0);
	}

	if (error == 0)
		ipstat.ips_fragmented++;

done:
	if (r->rt != PF_DUPTO)
		*m = NULL;
	if (rt != NULL)
		rtfree(rt);
	return;

bad:
	m_freem(m0);
	goto done;
}

#ifdef INET6
void
pf_route6(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s)
{
	struct mbuf		*m0;
	struct sockaddr_in6	*dst, sin6;
	struct ip6_hdr		*ip6;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sns[PF_SN_MAX];

	if (m == NULL || *m == NULL || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL)
		panic("pf_route6: invalid parameters");

	if ((*m)->m_pkthdr.pf.routed++ > 3) {
		m0 = *m;
		*m = NULL;
		goto bad;
	}

	if (r->rt == PF_DUPTO) {
		if ((m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT)) == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	if (m0->m_len < sizeof(struct ip6_hdr)) {
		DPFPRINTF(LOG_ERR,
		    "pf_route6: m0->m_len < sizeof(struct ip6_hdr)");
		goto bad;
	}
	ip6 = mtod(m0, struct ip6_hdr *);

	memset(&sin6, 0, sizeof(sin6));
	dst = &sin6;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;

	if (!r->rt) {
		m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		ip6_output(m0, NULL, NULL, 0, NULL, NULL, NULL);
		return;
	}

	if (s == NULL) {
		bzero(sns, sizeof(sns));
		if (pf_map_addr(AF_INET6, r, (struct pf_addr *)&ip6->ip6_src,
		    &naddr, NULL, sns, &r->route, PF_SN_ROUTE)) {
			DPFPRINTF(LOG_ERR,
			    "pf_route6: pf_map_addr() failed.");
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

	if (oifp != ifp) {
		if (pf_test(AF_INET6, PF_OUT, ifp, &m0, NULL) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
		if (m0->m_len < sizeof(struct ip6_hdr)) {
			DPFPRINTF(LOG_ERR,
			    "pf_route6: m0->m_len < sizeof(struct ip6_hdr)");
			goto bad;
		}
	}

	in6_proto_cksum_out(m0, ifp);

	/*
	 * If the packet is too large for the outgoing interface,
	 * send back an icmp6 error.
	 */
	if (IN6_IS_SCOPE_EMBED(&dst->sin6_addr))
		dst->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	if ((u_long)m0->m_pkthdr.len <= ifp->if_mtu) {
		nd6_output(ifp, m0, dst, NULL);
	} else {
		in6_ifstat_inc(ifp, ifs6_in_toobig);
		if (r->rt != PF_DUPTO)
			icmp6_error(m0, ICMP6_PACKET_TOO_BIG, 0, ifp->if_mtu);
		else
			goto bad;
	}

done:
	if (r->rt != PF_DUPTO)
		*m = NULL;
	return;

bad:
	m_freem(m0);
	goto done;
}
#endif /* INET6 */


/*
 * check protocol (tcp/udp/icmp/icmp6) checksum and set mbuf flag
 *   off is the offset where the protocol header starts
 *   len is the total length of protocol header plus payload
 * returns 0 when the checksum is valid, otherwise returns 1.
 * if the _OUT flag is set the checksum isn't done yet, consider these ok
 */
int
pf_check_proto_cksum(struct pf_pdesc *pd, int off, int len, u_int8_t p,
    sa_family_t af)
{
	u_int16_t flag_ok, flag_bad, flag_out;
	u_int16_t sum;

	if (pd->csum_status == PF_CSUM_OK)
		return (0);
	if (pd->csum_status == PF_CSUM_BAD)
		return (1);

	switch (p) {
	case IPPROTO_TCP:
		flag_ok = M_TCP_CSUM_IN_OK;
		flag_out = M_TCP_CSUM_OUT;
		flag_bad = M_TCP_CSUM_IN_BAD;
		break;
	case IPPROTO_UDP:
		flag_ok = M_UDP_CSUM_IN_OK;
		flag_out = M_UDP_CSUM_OUT;
		flag_bad = M_UDP_CSUM_IN_BAD;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif /* INET6 */
		flag_ok = M_ICMP_CSUM_IN_OK;
		flag_out = M_ICMP_CSUM_OUT;
		flag_bad = M_ICMP_CSUM_IN_BAD;
		break;
	default:
		return (1);
	}
	if (pd->m->m_pkthdr.csum_flags & (flag_ok | flag_out)) {
		pd->csum_status = PF_CSUM_OK;
		return (0);
	}
	if (pd->m->m_pkthdr.csum_flags & flag_bad || off < sizeof(struct ip) ||
	    pd->m->m_pkthdr.len < off + len) {
		pd->csum_status = PF_CSUM_BAD;
		return (1);
	}

	/* need to do it in software */
	if (p == IPPROTO_TCP)
		tcpstat.tcps_inswcsum++;
	else if (p == IPPROTO_UDP)
		udpstat.udps_inswcsum++;
	
	switch (af) {
	case AF_INET:
		if (pd->m->m_len < sizeof(struct ip)) {
			pd->csum_status = PF_CSUM_BAD;
			return (1);
		}
		sum = in4_cksum(pd->m, (p == IPPROTO_ICMP ? 0 : p), off, len);
		break;
#ifdef INET6
	case AF_INET6:
		if (pd->m->m_len < sizeof(struct ip6_hdr)) {
			pd->csum_status = PF_CSUM_BAD;
			return (1);
		}
		sum = in6_cksum(pd->m, p, off, len);
		break;
#endif /* INET6 */
	}
	if (sum) {
		switch (p) {
		case IPPROTO_TCP:
			tcpstat.tcps_rcvbadsum++;
			break;
		case IPPROTO_UDP:
			udpstat.udps_badsum++;
			break;
		case IPPROTO_ICMP:
			icmpstat.icps_checksum++;
			break;
#ifdef INET6
		case IPPROTO_ICMPV6:
			icmp6stat.icp6s_checksum++;
			break;
#endif /* INET6 */
		}
		pd->m->m_pkthdr.csum_flags |= flag_bad;
		pd->csum_status = PF_CSUM_BAD;
		return (1);
	}
	pd->m->m_pkthdr.csum_flags |= flag_ok;
	pd->csum_status = PF_CSUM_OK;
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
		bzero(mtag + 1, sizeof(struct pf_divert));
		m_tag_prepend(m, mtag);
	}

	return ((struct pf_divert *)(mtag + 1));
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
	int			 fraghdr_cnt = 0, rthdr_cnt = 0;

	pd->off += sizeof(struct ip6_hdr);
	end = pd->off + ntohs(h->ip6_plen);
	pd->fragoff = pd->extoff = pd->jumbolen = 0;
	pd->proto = h->ip6_nxt;
	for (;;) {
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
		case IPPROTO_AH:
		case IPPROTO_HOPOPTS:
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
}
#endif /* INET6 */

int
pf_setup_pdesc(struct pf_pdesc *pd, void *pdhdrs, sa_family_t af, int dir,
    struct pfi_kif *kif, struct mbuf *m, u_short *reason)
{
	bzero(pd, sizeof(*pd));
	pd->hdr.any = pdhdrs;
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
		pd->off = h->ip_hl << 2;

		if (pd->off < sizeof(struct ip) ||
		    pd->off > ntohs(h->ip_len) ||
		    pd->m->m_pkthdr.len < ntohs(h->ip_len)) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}

		pd->src = (struct pf_addr *)&h->ip_src;
		pd->dst = (struct pf_addr *)&h->ip_dst;
		pd->virtual_proto = pd->proto = h->ip_p;
		pd->tot_len = ntohs(h->ip_len);
		pd->tos = h->ip_tos & ~IPTOS_ECN_MASK;
		pd->ttl = h->ip_ttl;
		if (h->ip_hl > 5)	/* has options */
			pd->badopts++;

		if (h->ip_off & htons(IP_MF | IP_OFFMASK))
			pd->virtual_proto = PF_VPROTO_FRAGMENT;

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
		pd->off = 0;

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
#endif

		pd->src = (struct pf_addr *)&h->ip6_src;
		pd->dst = (struct pf_addr *)&h->ip6_dst;
		pd->virtual_proto = pd->proto;
		pd->tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
		pd->tos = (ntohl(h->ip6_flow) & 0x0fc00000) >> 20;
		pd->ttl = h->ip6_hlim;

		if (pd->fragoff != 0)
			pd->virtual_proto = PF_VPROTO_FRAGMENT;

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
		struct tcphdr	*th = pd->hdr.tcp;

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
		struct udphdr	*uh = pd->hdr.udp;

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
		if (!pf_pull_hdr(pd->m, pd->off, pd->hdr.icmp, ICMP_MINLEN,
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = ICMP_MINLEN;
		if (pd->off + pd->hdrlen > pd->tot_len) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
		pd->pcksum = &pd->hdr.icmp->icmp_cksum;
		break;
	}
#ifdef INET6
	case IPPROTO_ICMPV6: {
		size_t	icmp_hlen = sizeof(struct icmp6_hdr);

		if (!pf_pull_hdr(pd->m, pd->off, pd->hdr.icmp6, icmp_hlen,
		    NULL, reason, pd->af))
			return (PF_DROP);
		/* ICMP headers we look further into to match state */
		switch (pd->hdr.icmp6->icmp6_type) {
		case MLD_LISTENER_QUERY:
		case MLD_LISTENER_REPORT:
			icmp_hlen = sizeof(struct mld_hdr);
			break;
		case ND_NEIGHBOR_SOLICIT:
		case ND_NEIGHBOR_ADVERT:
			icmp_hlen = sizeof(struct nd_neighbor_solicit);
			break;
		}
		if (icmp_hlen > sizeof(struct icmp6_hdr) &&
		    !pf_pull_hdr(pd->m, pd->off, pd->hdr.icmp6, icmp_hlen,
		    NULL, reason, pd->af))
			return (PF_DROP);
		pd->hdrlen = icmp_hlen;
		if (pd->off + pd->hdrlen > pd->tot_len) {
			REASON_SET(reason, PFRES_SHORT);
			return (PF_DROP);
		}
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
pf_test(sa_family_t af, int fwdir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh)
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0;
	struct pf_rule		*a = NULL, *r = &pf_default_rule;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	union pf_headers	 pdhdrs;
	int			 dir = (fwdir == PF_FWD) ? PF_OUT : fwdir;
	u_int32_t		 qid, pqid = 0;

	if (!pf_status.running)
		return (PF_PASS);

	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(LOG_ERR,
		    "pf_test: kif == NULL, if_xname %s", ifp->if_xname);
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

	action = pf_setup_pdesc(&pd, &pdhdrs, af, dir, kif, *m0, &reason);
	if (action != PF_PASS) {
		pd.pflog |= PF_LOG_FORCE;
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
#endif
	}
	*m0 = pd.m;
	/* if packet sits in reassembly queue, return without error */
	if (pd.m == NULL)
		return PF_PASS;
	if (action != PF_PASS) {
		pd.pflog |= PF_LOG_FORCE;
		goto done;
	}

	/* if packet has been reassembled, update packet description */
	if (pf_status.reass && pd.virtual_proto == PF_VPROTO_FRAGMENT) {
		action = pf_setup_pdesc(&pd, &pdhdrs, af, dir, kif, *m0,
		    &reason);
		if (action != PF_PASS) {
			pd.pflog |= PF_LOG_FORCE;
			goto done;
		}
	}
	pd.eh = eh;
	pd.m->m_pkthdr.pf.flags |= PF_TAG_PROCESSED;

	switch (pd.virtual_proto) {

	case PF_VPROTO_FRAGMENT: {
		/*
		 * handle fragments that aren't reassembled by
		 * normalization
		 */
		action = pf_test_rule(&pd, &r, &s, &a, &ruleset);
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
			goto done;
		}
		action = pf_test_state_icmp(&pd, &s, &reason);
		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset);
		break;
	}

#ifdef INET6
	case IPPROTO_ICMPV6: {
		if (pd.af != AF_INET6) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_NORM);
			DPFPRINTF(LOG_NOTICE,
			    "dropping IPv4 packet with ICMPv6 payload");
			goto done;
		}
		action = pf_test_state_icmp(&pd, &s, &reason);
		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset);
		break;
	}
#endif /* INET6 */

	default:
		if (pd.virtual_proto == IPPROTO_TCP) {
			if ((pd.hdr.tcp->th_flags & TH_ACK) && pd.p_len == 0)
				pqid = 1;
			action = pf_normalize_tcp(&pd);
			if (action == PF_DROP)
				goto done;
		}
		action = pf_test_state(&pd, &s, &reason);
		if (action == PF_PASS || action == PF_AFRT) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&pd, &r, &s, &a, &ruleset);

		if (pd.virtual_proto == IPPROTO_TCP) {
			if (s) {
				if (s->max_mss)
					pf_normalize_mss(&pd, s->max_mss);
			} else if (r->max_mss)
				pf_normalize_mss(&pd, r->max_mss);
		}

		break;
	}

done:
	if (action != PF_DROP) {
		if (s) {
			/* The non-state case is handled in pf_test_rule() */
			if (action == PF_PASS && pd.badopts &&
			    !(s->state_flags & PFSTATE_ALLOWOPTS)) {
				action = PF_DROP;
				REASON_SET(&reason, PFRES_IPOPTIONS);
				pd.pflog |= PF_LOG_FORCE;
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
		pd.m->m_pkthdr.pf.statekey = s->key[PF_SK_STACK];
	if (pd.dir == PF_OUT &&
	    pd.m->m_pkthdr.pf.inp && !pd.m->m_pkthdr.pf.inp->inp_pf_sk &&
	    s && s->key[PF_SK_STACK] && !s->key[PF_SK_STACK]->inp) {
		pd.m->m_pkthdr.pf.inp->inp_pf_sk = s->key[PF_SK_STACK];
		s->key[PF_SK_STACK]->inp = pd.m->m_pkthdr.pf.inp;
	}

	/*
	 * connections redirected to loopback should not match sockets
	 * bound specifically to loopback due to security implications,
	 * see tcp_input() and in_pcblookup_listen().
	 */
	if (pd.destchg)
		if ((pd.af == AF_INET && (ntohl(pd.dst->v4.s_addr) >>
		    IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) ||
		    (pd.af == AF_INET6 && IN6_IS_ADDR_LOOPBACK(&pd.dst->v6)))
			pd.m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
	/* We need to redo the route lookup on outgoing routes. */
	if (pd.destchg && pd.dir == PF_OUT)
		pd.m->m_pkthdr.pf.flags |= PF_TAG_REROUTE;

	if (pd.dir == PF_IN && action == PF_PASS && r->divert.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(pd.m))) {
			pd.m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->port = r->divert.port;
			divert->rdomain = pd.rdomain;
			divert->addr = r->divert.addr;
		}
	}

	if (action == PF_PASS && r->divert_packet.port)
		action = PF_DIVERT;

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

	pf_counters_inc(action, &pd, s, r, a);

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(*m0);
	case PF_DEFER:
		*m0 = NULL;
		action = PF_PASS;
		break;
	case PF_DIVERT:
		switch (pd.af) {
		case AF_INET:
			if (!divert_packet(pd.m, pd.dir, r->divert_packet.port))
				*m0 = NULL;
			break;
#ifdef INET6
		case AF_INET6:
			if (!divert6_packet(pd.m, pd.dir,
			    r->divert_packet.port))
				*m0 = NULL;
			break;
#endif /* INET6 */
		}
		action = PF_PASS;
		break;
#if INET && INET6
	case PF_AFRT:
		if (pf_translate_af(&pd)) {
			if (!pd.m)
				*m0 = NULL;
			action = PF_DROP;
			break;
		}
		if (pd.naf == AF_INET)
			pf_route(&pd.m, r, dir, kif->pfik_ifp, s);
		if (pd.naf == AF_INET6)
			pf_route6(&pd.m, r, dir, kif->pfik_ifp, s);
		*m0 = NULL;
		action = PF_PASS;
		break;
#endif /* INET && INET6 */
	default:
		/* pf_route can free the mbuf causing *m0 to become NULL */
		if (r->rt) {
			switch (pd.af) {
			case AF_INET:
				pf_route(m0, r, pd.dir, pd.kif->pfik_ifp, s);
				break;
#ifdef INET6
			case AF_INET6:
				pf_route6(m0, r, pd.dir, pd.kif->pfik_ifp, s);
				break;
#endif /* INET6 */
			}
		}
		break;
	}

#ifdef INET6
	/* if reassembled packet passed, create new fragments */
	if (pf_status.reass && action == PF_PASS && *m0 && fwdir == PF_FWD) {
		struct m_tag	*mtag;

		if ((mtag = m_tag_find(*m0, PACKET_TAG_PF_REASSEMBLED, NULL)))
			action = pf_refragment6(m0, mtag, fwdir);
	}
#endif
	if (s && action != PF_DROP) {
		if (!s->if_index_in && dir == PF_IN)
			s->if_index_in = ifp->if_index;
		else if (!s->if_index_out && dir == PF_OUT)
			s->if_index_out = ifp->if_index;
	}

	return (action);
}

void
pf_cksum(struct pf_pdesc *pd, struct mbuf *m)
{
	if (pd->csum_status != PF_CSUM_OK)
		return;	/* don't fix broken cksums */

	switch (pd->proto) {
	case IPPROTO_TCP:
		pd->hdr.tcp->th_sum = 0;
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_UDP:
		pd->hdr.udp->uh_sum = 0;
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_ICMP:
		pd->hdr.icmp->icmp_cksum = 0;
		m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;
		break;
#ifdef INET6
	case IPPROTO_ICMPV6:
		pd->hdr.icmp6->icmp6_cksum = 0;
		m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;
		break;
#endif /* INET6 */
	default:
		/* nothing */
		break;
	}
}

/*
 * must be called whenever any addressing information such as
 * address, port, protocol has changed
 */
void
pf_pkt_addr_changed(struct mbuf *m)
{
	m->m_pkthdr.pf.statekey = NULL;
}

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
