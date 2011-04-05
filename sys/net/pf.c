/*	$OpenBSD: pf.c,v 1.734 2011/04/05 13:48:18 mikeb Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2010 Henning Brauer
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

#include <crypto/md5.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/radix_mpath.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <netinet/icmp_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_divert.h>

#include <dev/rndvar.h>
#include <net/pfvar.h>
#include <net/if_pflog.h>
#include <net/if_pflow.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_divert.h>
#endif /* INET6 */


/*
 * Global variables
 */

/* state tables */
struct pf_state_tree	 pf_statetbl;

struct pf_altqqueue	 pf_altqs[2];
struct pf_altqqueue	*pf_altqs_active;
struct pf_altqqueue	*pf_altqs_inactive;
struct pf_status	 pf_status;

u_int32_t		 ticket_altqs_active;
u_int32_t		 ticket_altqs_inactive;
int			 altqs_inactive_open;

MD5_CTX			 pf_tcp_secret_ctx;
u_char			 pf_tcp_secret[16];
int			 pf_tcp_secret_init;
int			 pf_tcp_iss_off;

struct pf_anchor_stackframe {
	struct pf_ruleset			*rs;
	struct pf_rule				*r;
	struct pf_anchor_node			*parent;
	struct pf_anchor			*child;
} pf_anchor_stack[64];

/* cannot fold into pf_pdesc directly, unknown storage size outside pf.c */
union pf_headers {
	struct tcphdr		tcp;
	struct udphdr		udp;
	struct icmp		icmp;
#ifdef INET6
	struct icmp6_hdr	icmp6;
#endif /* INET6 */
};


struct pool		 pf_src_tree_pl, pf_rule_pl;
struct pool		 pf_state_pl, pf_state_key_pl, pf_state_item_pl;
struct pool		 pf_altq_pl, pf_rule_item_pl, pf_sn_item_pl;

void			 pf_init_threshold(struct pf_threshold *, u_int32_t,
			    u_int32_t);
void			 pf_add_threshold(struct pf_threshold *);
int			 pf_check_threshold(struct pf_threshold *);

void			 pf_change_ap(struct pf_addr *, u_int16_t *,
			    u_int16_t *, struct pf_addr *, u_int16_t,
			    u_int8_t, sa_family_t);
int			 pf_modulate_sack(struct mbuf *, int, struct pf_pdesc *,
			    struct tcphdr *, struct pf_state_peer *);
#ifdef INET6
void			 pf_change_a6(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, u_int8_t);
#endif /* INET6 */
int			 pf_icmp_mapping(struct pf_pdesc *, u_int8_t, int *,
			    int *, u_int16_t *, u_int16_t *);
void			 pf_change_icmp(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, struct pf_addr *, u_int16_t,
			    u_int16_t *, u_int16_t *, u_int16_t *,
			    u_int8_t, sa_family_t);
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
int			 pf_test_rule(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **, struct ifqueue *, int);
static __inline int	 pf_create_state(struct pf_rule *, struct pf_rule *,
			    struct pf_pdesc *, struct pf_state_key **,
			    struct pf_state_key **, struct mbuf *, int,
			    int *, struct pfi_kif *, struct pf_state **, int,
			    struct pf_rule_slist *,
			    struct pf_rule_actions *, struct pf_src_node *[]);
int			 pf_state_key_setup(struct pf_pdesc *, struct
			    pf_state_key **, struct pf_state_key **, int);
int			 pf_test_fragment(struct pf_rule **, int,
			    struct pfi_kif *, struct mbuf *,
			    struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **);
int			 pf_tcp_track_full(struct pf_state_peer *,
			    struct pf_state_peer *, struct pf_state **,
			    struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *, u_short *, int *);
int			 pf_tcp_track_sloppy(struct pf_state_peer *,
			    struct pf_state_peer *, struct pf_state **,
			    struct pf_pdesc *, u_short *);
int			 pf_test_state_tcp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *, u_short *);
int			 pf_test_state_udp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *);
int			 pf_icmp_state_lookup(struct pf_state_key_cmp *,
			    struct pf_pdesc *, struct pf_state **, struct mbuf *,
			    int, struct pfi_kif *, u_int16_t, u_int16_t,
			    int, int *, int, int);
int			 pf_test_state_icmp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *, u_short *);
int			 pf_test_state_other(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, struct pf_pdesc *);
void			 pf_route(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *);
void			 pf_route6(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *);
int			 pf_socket_lookup(int, struct pf_pdesc *);
u_int8_t		 pf_get_wscale(struct mbuf *, int, u_int16_t,
			    sa_family_t);
u_int16_t		 pf_get_mss(struct mbuf *, int, u_int16_t,
			    sa_family_t);
u_int16_t		 pf_calc_mss(struct pf_addr *, sa_family_t, int,
				u_int16_t);
void			 pf_set_rt_ifp(struct pf_state *,
			    struct pf_addr *);
int			 pf_check_proto_cksum(struct mbuf *, int, int,
			    u_int8_t, sa_family_t);
struct pf_divert	*pf_get_divert(struct mbuf *);
void			 pf_print_state_parts(struct pf_state *,
			    struct pf_state_key *, struct pf_state_key *);
int			 pf_addr_wrap_neq(struct pf_addr_wrap *,
			    struct pf_addr_wrap *);
int			 pf_compare_state_keys(struct pf_state_key *,
			    struct pf_state_key *, struct pfi_kif *, u_int);
struct pf_state		*pf_find_state(struct pfi_kif *,
			    struct pf_state_key_cmp *, u_int, struct mbuf *);
int			 pf_src_connlimit(struct pf_state **);
int			 pf_check_congestion(struct ifqueue *);
int			 pf_match_rcvif(struct mbuf *, struct pf_rule *);
void			 pf_counters_inc(int, int,
			    struct pf_pdesc *, struct pfi_kif *,
			    struct pf_state *, struct pf_rule *,
			    struct pf_rule *);

extern struct pool pfr_ktable_pl;
extern struct pool pfr_kentry_pl;

struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX] = {
	{ &pf_state_pl, PFSTATE_HIWAT, PFSTATE_HIWAT },
	{ &pf_src_tree_pl, PFSNODE_HIWAT, PFSNODE_HIWAT },
	{ &pf_frent_pl, PFFRAG_FRENT_HIWAT, PFFRAG_FRENT_HIWAT },
	{ &pfr_ktable_pl, PFR_KTABLE_HIWAT, PFR_KTABLE_HIWAT },
	{ &pfr_kentry_pl, PFR_KENTRY_HIWAT, PFR_KENTRY_HIWAT }
};

enum { PF_ICMP_MULTI_NONE, PF_ICMP_MULTI_SOLICITED, PF_ICMP_MULTI_LINK };


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
#ifdef INET
	case AF_INET:
		if (a->addr32[0] > b->addr32[0])
			return (1);
		if (a->addr32[0] < b->addr32[0])
			return (-1);
		break;
#endif /* INET */
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
#ifdef INET
	case AF_INET:
		dst->addr32[0] = src->addr32[0];
		break;
#endif /* INET */
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
	threshold->last = time_second;
}

void
pf_add_threshold(struct pf_threshold *threshold)
{
	u_int32_t t = time_second, diff = t - threshold->last;

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
#ifdef INET
		case AF_INET:
			p.pfra_net = 32;
			p.pfra_ip4addr = sn->addr.v4;
			break;
#endif /* INET */
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
		(*sn)->creation = time_second;
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
	if (sn->states > 0 || sn->expire > time_second)
		return;

	if (sn->rule.ptr != NULL) {
		sn->rule.ptr->src_nodes--;
		if (sn->rule.ptr->states_cur <= 0 &&
		    sn->rule.ptr->max_src_nodes <= 0)
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
				SLIST_REMOVE_NEXT(&s->src_nodes, snip, next);
			else
				SLIST_REMOVE_HEAD(&s->src_nodes, next);
			pool_put(&pf_sn_item_pl, sni);
			sn->states--;
		}
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
			    si->s->direction == s->direction) {
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

int
pf_state_key_setup(struct pf_pdesc *pd, struct pf_state_key **skw,
    struct pf_state_key **sks, int rtableid)
{
	/* if returning error we MUST pool_put state keys ourselves */
	struct pf_state_key *sk1, *sk2;
	u_int wrdom = pd->rdomain;

	if ((sk1 = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL)
		return (ENOMEM);

	PF_ACPY(&sk1->addr[pd->sidx], pd->src, pd->af);
	PF_ACPY(&sk1->addr[pd->didx], pd->dst, pd->af);
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
	    wrdom != pd->rdomain) {	/* NAT */
		if ((sk2 = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL) {
			pool_put(&pf_state_key_pl, sk1);
			return (ENOMEM);
		}
		PF_ACPY(&sk2->addr[pd->sidx], &pd->nsaddr, pd->af);
		PF_ACPY(&sk2->addr[pd->didx], &pd->ndaddr, pd->af);
		sk2->port[pd->sidx] = pd->nsport;
		sk2->port[pd->didx] = pd->ndport;
		sk2->proto = pd->proto;
		sk2->af = pd->af;
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
pf_state_insert(struct pfi_kif *kif, struct pf_state_key *skw,
    struct pf_state_key *sks, struct pf_state *s)
{
	splsoftassert(IPL_SOFTNET);

	s->kif = kif;
	if (skw == sks) {
		if (pf_state_key_attach(skw, s, PF_SK_WIRE))
			return (-1);
		s->key[PF_SK_STACK] = s->key[PF_SK_WIRE];
	} else {
		if (pf_state_key_attach(skw, s, PF_SK_WIRE)) {
			pool_put(&pf_state_key_pl, sks);
			return (-1);
		}
		if (pf_state_key_attach(sks, s, PF_SK_STACK)) {
			pf_state_key_detach(s, PF_SK_WIRE);
			return (-1);
		}
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
	    ((struct pf_state_key *)m->m_pkthdr.pf.statekey)->reverse)
		sk = ((struct pf_state_key *)m->m_pkthdr.pf.statekey)->reverse;
	else {
		if ((sk = RB_FIND(pf_state_tree, &pf_statetbl,
		    (struct pf_state_key *)key)) == NULL)
			return (NULL);
		if (dir == PF_OUT && m->m_pkthdr.pf.statekey &&
		    pf_compare_state_keys(m->m_pkthdr.pf.statekey, sk,
		    kif, dir) == 0) {
			((struct pf_state_key *)
			    m->m_pkthdr.pf.statekey)->reverse = sk;
			sk->reverse = m->m_pkthdr.pf.statekey;
		}
	}

	if (dir == PF_OUT)
		m->m_pkthdr.pf.statekey = NULL;

	/* list is sorted, if-bound states before floating ones */
	TAILQ_FOREACH(si, &sk->states, entry)
		if ((si->s->kif == pfi_all || si->s->kif == kif) &&
		    sk == (dir == PF_IN ? si->s->key[PF_SK_WIRE] :
		    si->s->key[PF_SK_STACK]))
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

u_int32_t
pf_state_expires(const struct pf_state *state)
{
	u_int32_t	timeout;
	u_int32_t	start;
	u_int32_t	end;
	u_int32_t	states;

	/* handle all PFTM_* > PFTM_MAX here */
	if (state->timeout == PFTM_PURGE)
		return (time_second);
	if (state->timeout == PFTM_UNTIL_PACKET)
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
		if (states < end)
			return (state->expire + timeout * (end - states) /
			    (end - start));
		else
			return (time_second);
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

		if (cur->states <= 0 && cur->expire <= time_second) {
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
			sni->sn->expire = time_second + timeout;
		}
		pool_put(&pf_sn_item_pl, sni);
	}
}

/* callers should be at splsoftnet */
void
pf_unlink_state(struct pf_state *cur)
{
	splsoftassert(IPL_SOFTNET);

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
	if (--cur->rule.ptr->states_cur <= 0 &&
	    cur->rule.ptr->src_nodes <= 0)
		pf_rm_rule(NULL, cur->rule.ptr);
	if (cur->anchor.ptr != NULL)
		if (--cur->anchor.ptr->states_cur <= 0)
			pf_rm_rule(NULL, cur->anchor.ptr);
	while ((ri = SLIST_FIRST(&cur->match_rules))) {
		SLIST_REMOVE_HEAD(&cur->match_rules, entry);
		if (--ri->r->states_cur <= 0 &&
		    ri->r->src_nodes <= 0)
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
		} else if (pf_state_expires(cur) <= time_second) {
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
#ifdef INET
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
#endif /* INET */
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
		if (cur->af != prev->af)
			PF_SET_SKIP_STEPS(PF_SKIP_AF);
		if (cur->proto != prev->proto)
			PF_SET_SKIP_STEPS(PF_SKIP_PROTO);
		if (cur->src.neg != prev->src.neg ||
		    pf_addr_wrap_neq(&cur->src.addr, &prev->src.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_ADDR);
		if (cur->src.port[0] != prev->src.port[0] ||
		    cur->src.port[1] != prev->src.port[1] ||
		    cur->src.port_op != prev->src.port_op)
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_PORT);
		if (cur->dst.neg != prev->dst.neg ||
		    pf_addr_wrap_neq(&cur->dst.addr, &prev->dst.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_DST_ADDR);
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

u_int16_t
pf_cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new, u_int8_t udp)
{
	u_int32_t	l;

	if (udp && !cksum)
		return (0x0000);
	l = cksum + old - new;
	l = (l >> 16) + (l & 65535);
	l = l & 65535;
	if (udp && !l)
		return (0xFFFF);
	return (l);
}

void
pf_change_ap(struct pf_addr *a, u_int16_t *p, u_int16_t *pc,
    struct pf_addr *an, u_int16_t pn, u_int8_t u, sa_family_t af)
{
	struct pf_addr	ao;
	u_int16_t	po = *p;

	PF_ACPY(&ao, a, af);
	PF_ACPY(a, an, af);
	*p = pn;

	switch (af) {
#ifdef INET
	case AF_INET:
		*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
		    ao.addr16[0], an->addr16[0], u),
		    ao.addr16[1], an->addr16[1], u),
		    po, pn, u);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
		    ao.addr16[0], an->addr16[0], u),
		    ao.addr16[1], an->addr16[1], u),
		    ao.addr16[2], an->addr16[2], u),
		    ao.addr16[3], an->addr16[3], u),
		    ao.addr16[4], an->addr16[4], u),
		    ao.addr16[5], an->addr16[5], u),
		    ao.addr16[6], an->addr16[6], u),
		    ao.addr16[7], an->addr16[7], u),
		    po, pn, u);
		break;
#endif /* INET6 */
	}
}


/* Changes a u_int32_t.  Uses a void * so there are no align restrictions */
void
pf_change_a(void *a, u_int16_t *c, u_int32_t an, u_int8_t u)
{
	u_int32_t	ao;

	memcpy(&ao, a, sizeof(ao));
	memcpy(a, &an, sizeof(u_int32_t));
	if (c != NULL)
		*c = pf_cksum_fixup(pf_cksum_fixup(*c, ao / 65536, an / 65536,
		    u), ao % 65536, an % 65536, u);
}

#ifdef INET6
void
pf_change_a6(struct pf_addr *a, u_int16_t *c, struct pf_addr *an, u_int8_t u)
{
	struct pf_addr	ao;

	PF_ACPY(&ao, a, AF_INET6);
	PF_ACPY(a, an, AF_INET6);

	if (c)
		*c = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(*c,
		    ao.addr16[0], an->addr16[0], u),
		    ao.addr16[1], an->addr16[1], u),
		    ao.addr16[2], an->addr16[2], u),
		    ao.addr16[3], an->addr16[3], u),
		    ao.addr16[4], an->addr16[4], u),
		    ao.addr16[5], an->addr16[5], u),
		    ao.addr16[6], an->addr16[6], u),
		    ao.addr16[7], an->addr16[7], u);
}
#endif /* INET6 */

int
pf_icmp_mapping(struct pf_pdesc *pd, u_int8_t type,
    int *icmp_dir, int *multi, u_int16_t *virtual_id, u_int16_t *virtual_type)
{
	/*
	 * ICMP types marked with PF_OUT are typically responses to
	 * PF_IN, and will match states in the opposite direction.
	 * PF_IN ICMP types need to match a state with that type.
	 */
	*icmp_dir = PF_OUT;
	*multi = PF_ICMP_MULTI_LINK;

	/* Queries (and responses) */
	switch (pd->af) {
#ifdef INET
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
#endif /* INET */
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

			*virtual_type = MLD_LISTENER_QUERY;
			/* generate fake id for these messages */
			*virtual_id = (mld->mld_addr.s6_addr32[0] ^
				mld->mld_addr.s6_addr32[1] ^
				mld->mld_addr.s6_addr32[2] ^
				mld->mld_addr.s6_addr32[3]) & 0xffff;
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

			*virtual_type = ND_NEIGHBOR_SOLICIT;
			*multi = PF_ICMP_MULTI_SOLICITED;
			/* generate fake id for these messages */
			*virtual_id = (nd->nd_ns_target.s6_addr32[0] ^
				nd->nd_ns_target.s6_addr32[1] ^
				nd->nd_ns_target.s6_addr32[2] ^
				nd->nd_ns_target.s6_addr32[3]) & 0xffff;
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
	default:
		*icmp_dir = PF_IN;
		*virtual_type = type;
		*virtual_id = 0;
		break;
	}
	HTONS(*virtual_type);
	return (0);  /* These types match to their own state */
}

void
pf_change_icmp(struct pf_addr *ia, u_int16_t *ip, struct pf_addr *oa,
    struct pf_addr *na, u_int16_t np, u_int16_t *pc, u_int16_t *h2c,
    u_int16_t *ic, u_int8_t u, sa_family_t af)
{
	struct pf_addr	oia, ooa;

	PF_ACPY(&oia, ia, af);
	if (oa)
		PF_ACPY(&ooa, oa, af);

	/* Change inner protocol port, fix inner protocol checksum. */
	if (ip != NULL) {
		u_int16_t	oip = *ip;
		u_int32_t	opc;

		if (pc != NULL)
			opc = *pc;
		*ip = np;
		if (pc != NULL)
			*pc = pf_cksum_fixup(*pc, oip, *ip, u);
		*ic = pf_cksum_fixup(*ic, oip, *ip, 0);
		if (pc != NULL)
			*ic = pf_cksum_fixup(*ic, opc, *pc, 0);
	}
	/* Change inner ip address, fix inner ip and icmp checksums. */
	PF_ACPY(ia, na, af);
	switch (af) {
#ifdef INET
	case AF_INET: {
		u_int32_t	 oh2c = *h2c;

		/* XXX just in_cksum() */
		*h2c = pf_cksum_fixup(pf_cksum_fixup(*h2c,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(*ic, oh2c, *h2c, 0);
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], u),
		    oia.addr16[1], ia->addr16[1], u),
		    oia.addr16[2], ia->addr16[2], u),
		    oia.addr16[3], ia->addr16[3], u),
		    oia.addr16[4], ia->addr16[4], u),
		    oia.addr16[5], ia->addr16[5], u),
		    oia.addr16[6], ia->addr16[6], u),
		    oia.addr16[7], ia->addr16[7], u);
		break;
#endif /* INET6 */
	}
	/* Outer ip address, fix outer icmpv6 checksum, if necessary. */
	if (oa) {
		PF_ACPY(oa, na, af);
#ifdef INET6
		if (af == AF_INET6)
			*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(*ic,
			    ooa.addr16[0], oa->addr16[0], u),
			    ooa.addr16[1], oa->addr16[1], u),
			    ooa.addr16[2], oa->addr16[2], u),
			    ooa.addr16[3], oa->addr16[3], u),
			    ooa.addr16[4], oa->addr16[4], u),
			    ooa.addr16[5], oa->addr16[5], u),
			    ooa.addr16[6], oa->addr16[6], u),
			    ooa.addr16[7], oa->addr16[7], u);
#endif /* INET6 */
	}
}


/*
 * Need to modulate the sequence numbers in the TCP SACK option
 * (credits to Krzysztof Pfaff for report and patch)
 */
int
pf_modulate_sack(struct mbuf *m, int off, struct pf_pdesc *pd,
    struct tcphdr *th, struct pf_state_peer *dst)
{
	int hlen = (th->th_off << 2) - sizeof(*th), thoptlen = hlen;
	u_int8_t opts[MAX_TCPOPTLEN], *opt = opts;
	int copyback = 0, i, olen;
	struct sackblk sack;

#define TCPOLEN_SACKLEN	(TCPOLEN_SACK + 2)
	if (hlen < TCPOLEN_SACKLEN ||
	    !pf_pull_hdr(m, off + sizeof(*th), opts, hlen, NULL, NULL, pd->af))
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
					pf_change_a(&sack.start, &th->th_sum,
					    htonl(ntohl(sack.start) -
					    dst->seqdiff), 0);
					pf_change_a(&sack.end, &th->th_sum,
					    htonl(ntohl(sack.end) -
					    dst->seqdiff), 0);
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
		m_copyback(m, off + sizeof(*th), thoptlen, opts, M_NOWAIT);
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
#ifdef INET
	struct ip	*h;
#endif /* INET */
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
#ifdef INET
	case AF_INET:
		len = sizeof(struct ip) + tlen;
		break;
#endif /* INET */
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
	m->m_pkthdr.rdomain = rdom;

#ifdef ALTQ
	if (r != NULL && r->qid) {
		m->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m->m_pkthdr.pf.hdr = mtod(m, struct ip *);
	}
#endif /* ALTQ */
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m->m_data, len);
	switch (af) {
#ifdef INET
	case AF_INET:
		h = mtod(m, struct ip *);

		/* IP header fields included in the TCP checksum */
		h->ip_p = IPPROTO_TCP;
		h->ip_len = htons(tlen);
		h->ip_src.s_addr = saddr->v4.s_addr;
		h->ip_dst.s_addr = daddr->v4.s_addr;

		th = (struct tcphdr *)((caddr_t)h + sizeof(struct ip));
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		h6 = mtod(m, struct ip6_hdr *);

		/* IP header fields included in the TCP checksum */
		h6->ip6_nxt = IPPROTO_TCP;
		h6->ip6_plen = htons(tlen);
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
		bcopy((caddr_t)&mss, (caddr_t)(opt + 2), 2);
	}

	switch (af) {
#ifdef INET
	case AF_INET:
		/* TCP checksum */
		th->th_sum = in_cksum(m, len);

		/* Finish the IP header */
		h->ip_v = 4;
		h->ip_hl = sizeof(*h) >> 2;
		h->ip_tos = IPTOS_LOWDELAY;
		h->ip_len = htons(len);
		h->ip_off = htons(ip_mtudisc ? IP_DF : 0);
		h->ip_ttl = ttl ? ttl : ip_defttl;
		h->ip_sum = 0;
		if (eh == NULL) {
			ip_output(m, (void *)NULL, (void *)NULL, 0,
			    (void *)NULL, (void *)NULL);
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
			bcopy(eh->ether_dhost, e->ether_shost, ETHER_ADDR_LEN);
			bcopy(eh->ether_shost, e->ether_dhost, ETHER_ADDR_LEN);
			e->ether_type = eh->ether_type;
			ip_output(m, (void *)NULL, &ro, IP_ROUTETOETHER,
			    (void *)NULL, (void *)NULL);
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* TCP checksum */
		th->th_sum = in6_cksum(m, IPPROTO_TCP,
		    sizeof(struct ip6_hdr), tlen);

		h6->ip6_vfc |= IPV6_VERSION;
		h6->ip6_hlim = IPV6_DEFHLIM;

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
	m0->m_pkthdr.rdomain = rdomain;

#ifdef ALTQ
	if (r->qid) {
		m0->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m0->m_pkthdr.pf.hdr = mtod(m0, struct ip *);
	}
#endif /* ALTQ */

	switch (af) {
#ifdef INET
	case AF_INET:
		icmp_error(m0, type, code, 0, 0);
		break;
#endif /* INET */
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
#ifdef INET
	case AF_INET:
		if ((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			match++;
		break;
#endif /* INET */
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
#ifdef INET
	case AF_INET:
		if ((ntohl(a->addr32[0]) < ntohl(b->addr32[0])) ||
		    (ntohl(a->addr32[0]) > ntohl(e->addr32[0])))
			return (0);
		break;
#endif /* INET */
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
		m->m_pkthdr.rdomain = rtableid;
}

void
pf_step_into_anchor(int *depth, struct pf_ruleset **rs,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe	*f;

	(*r)->anchor->match = 0;
	if (match)
		*match = 0;
	if (*depth >= sizeof(pf_anchor_stack) /
	    sizeof(pf_anchor_stack[0])) {
		log(LOG_ERR, "pf_step_into_anchor: stack overflow\n");
		*r = TAILQ_NEXT(*r, entries);
		return;
	} else if (*depth == 0 && a != NULL)
		*a = *r;
	f = pf_anchor_stack + (*depth)++;
	f->rs = *rs;
	f->r = *r;
	if ((*r)->anchor_wildcard) {
		f->parent = &(*r)->anchor->children;
		if ((f->child = RB_MIN(pf_anchor_node, f->parent)) ==
		    NULL) {
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
			if (f->child->match ||
			    (match != NULL && *match)) {
				f->r->anchor->match = 1;
				*match = 0;
			}
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
		*rs = f->rs;
		if (f->r->anchor->match || (match != NULL && *match))
			quick = f->r->quick;
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
#ifdef INET
	case AF_INET:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		break;
#endif /* INET */
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
#ifdef INET
	case AF_INET:
		addr->addr32[0] = htonl(ntohl(addr->addr32[0]) + 1);
		break;
#endif /* INET */
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
pf_socket_lookup(int direction, struct pf_pdesc *pd)
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
	if (direction == PF_IN) {
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
#ifdef INET
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
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		inp = in6_pcbhashlookup(tb, &saddr->v6, sport, &daddr->v6,
		    dport);
		if (inp == NULL) {
			inp = in6_pcblookup_listen(tb, &daddr->v6, dport, 0,
			    NULL);
			if (inp == NULL)
				return (-1);
		}
		break;
#endif /* INET6 */

	default:
		return (-1);
	}
	pd->lookup.uid = inp->inp_socket->so_euid;
	pd->lookup.gid = inp->inp_socket->so_egid;
	pd->lookup.pid = inp->inp_socket->so_cpid;
	return (1);
}

u_int8_t
pf_get_wscale(struct mbuf *m, int off, u_int16_t th_off, sa_family_t af)
{
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
	u_int8_t	 wscale = 0;

	hlen = th_off << 2;		/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(m, off, hdr, hlen, NULL, NULL, af))
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
pf_get_mss(struct mbuf *m, int off, u_int16_t th_off, sa_family_t af)
{
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
	u_int16_t	 mss = tcp_mssdflt;

	hlen = th_off << 2;	/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(m, off, hdr, hlen, NULL, NULL, af))
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
			bcopy((caddr_t)(opt + 2), (caddr_t)&mss, 2);
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
pf_calc_mss(struct pf_addr *addr, sa_family_t af, int rtabelid, u_int16_t offer)
{
#ifdef INET
	struct sockaddr_in	*dst;
	struct route		 ro;
#endif /* INET */
#ifdef INET6
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro6;
#endif /* INET6 */
	struct rtentry		*rt = NULL;
	int			 hlen;
	u_int16_t		 mss = tcp_mssdflt;

	switch (af) {
#ifdef INET
	case AF_INET:
		hlen = sizeof(struct ip);
		bzero(&ro, sizeof(ro));
		dst = (struct sockaddr_in *)&ro.ro_dst;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		ro.ro_tableid = rtabelid;
		rtalloc_noclone(&ro);
		rt = ro.ro_rt;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		bzero(&ro6, sizeof(ro6));
		dst6 = (struct sockaddr_in6 *)&ro6.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		ro6.ro_tableid = rtabelid;
		rtalloc_noclone((struct route *)&ro6);
		rt = ro6.ro_rt;
		break;
#endif /* INET6 */
	}

	if (rt && rt->rt_ifp) {
		mss = rt->rt_ifp->if_mtu - hlen - sizeof(struct tcphdr);
		mss = max(tcp_mssdflt, mss);
		RTFREE(rt);
	}
	mss = min(mss, offer);
	mss = max(mss, 64);		/* sanity - at least max opt space */
	return (mss);
}

void
pf_set_rt_ifp(struct pf_state *s, struct pf_addr *saddr)
{
	struct pf_rule *r = s->rule.ptr;
	struct pf_src_node *sn = NULL;

	s->rt_kif = NULL;
	if (!r->rt)
		return;
	switch (s->key[PF_SK_WIRE]->af) {
#ifdef INET
	case AF_INET:
		pf_map_addr(AF_INET, r, saddr, &s->rt_addr, NULL, &sn,
		    &r->route, PF_SN_ROUTE);
		s->rt_kif = r->route.kif;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		pf_map_addr(AF_INET6, r, saddr, &s->rt_addr, NULL, &sn,
		    &r->route, PF_SN_ROUTE);
		s->rt_kif = r->route.kif;
		break;
#endif /* INET6 */
	}
}

u_int32_t
pf_tcp_iss(struct pf_pdesc *pd)
{
	MD5_CTX ctx;
	u_int32_t digest[4];

	if (pf_tcp_secret_init == 0) {
		arc4random_buf(pf_tcp_secret, sizeof(pf_tcp_secret));
		MD5Init(&pf_tcp_secret_ctx);
		MD5Update(&pf_tcp_secret_ctx, pf_tcp_secret,
		    sizeof(pf_tcp_secret));
		pf_tcp_secret_init = 1;
	}
	ctx = pf_tcp_secret_ctx;

	MD5Update(&ctx, (char *)&pd->hdr.tcp->th_sport, sizeof(u_short));
	MD5Update(&ctx, (char *)&pd->hdr.tcp->th_dport, sizeof(u_short));
	if (pd->af == AF_INET6) {
		MD5Update(&ctx, (char *)&pd->src->v6, sizeof(struct in6_addr));
		MD5Update(&ctx, (char *)&pd->dst->v6, sizeof(struct in6_addr));
	} else {
		MD5Update(&ctx, (char *)&pd->src->v4, sizeof(struct in_addr));
		MD5Update(&ctx, (char *)&pd->dst->v4, sizeof(struct in_addr));
	}
	MD5Final((u_char *)digest, &ctx);
	pf_tcp_iss_off += 4096;
	return (digest[0] + tcp_iss + pf_tcp_iss_off);
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
	    PFSTATE_SETTOS|PFSTATE_SCRUB_TCP));
}

int
pf_test_rule(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off,
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq, int hdrlen)
{
	struct pf_rule		*lastr = NULL;
	sa_family_t		 af = pd->af;
	struct pf_rule		*r, *a = NULL;
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
	int			 state_icmp = 0, icmp_dir, multi;
	u_int16_t		 virtual_type, virtual_id;
	u_int8_t		 icmptype = 0, icmpcode = 0;

	PF_ACPY(&pd->nsaddr, pd->src, pd->af);
	PF_ACPY(&pd->ndaddr, pd->dst, pd->af);

	bzero(&act, sizeof(act));
	bzero(sns, sizeof(sns));
	act.rtableid = pd->rdomain;
	SLIST_INIT(&rules);

	if (direction == PF_IN && pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

	switch (pd->proto) {
	case IPPROTO_TCP:
		pd->nsport = th->th_sport;
		pd->ndport = th->th_dport;
		break;
	case IPPROTO_UDP:
		pd->nsport = pd->hdr.udp->uh_sport;
		pd->ndport = pd->hdr.udp->uh_dport;
		break;
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpcode = pd->hdr.icmp->icmp_code;
		state_icmp = pf_icmp_mapping(pd, icmptype,
		    &icmp_dir, &multi, &virtual_id, &virtual_type);
		if (icmp_dir == PF_IN) {
			pd->nsport = virtual_id;
			pd->ndport = virtual_type;
		} else {
			pd->nsport = virtual_type;
			pd->ndport = virtual_id;
		}
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpcode = pd->hdr.icmp6->icmp6_code;
		state_icmp = pf_icmp_mapping(pd, icmptype,
		    &icmp_dir, &multi, &virtual_id, &virtual_type);
		if (icmp_dir == PF_IN) {
			pd->nsport = virtual_id;
			pd->ndport = virtual_type;
		} else {
			pd->nsport = virtual_type;
			pd->ndport = virtual_id;
		}
		break;
#endif /* INET6 */
	default:
		pd->nsport = pd->ndport;
		break;
	}

	pd->osport = pd->nsport;
	pd->odport = pd->ndport;

	r = TAILQ_FIRST(pf_main_ruleset.rules.active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, &pd->nsaddr, af,
		    r->src.neg, kif, act.rtableid))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], pd->nsport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, &pd->ndaddr, af,
		    r->dst.neg, NULL, act.rtableid))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], pd->ndport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		/* icmp only. type always 0 in other cases */
		else if (r->type && r->type != icmptype + 1)
			r = TAILQ_NEXT(r, entries);
		/* icmp only. type always 0 in other cases */
		else if (r->code && r->code != icmpcode + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_TCP &&
		    (r->flagset & th->th_flags) != r->flags)
			r = TAILQ_NEXT(r, entries);
		/* tcp/udp only. uid.op always 0 in other cases */
		else if (r->uid.op && (pd->lookup.done || (pd->lookup.done =
		    pf_socket_lookup(direction, pd), 1)) &&
		    !pf_match_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1],
		    pd->lookup.uid))
			r = TAILQ_NEXT(r, entries);
		/* tcp/udp only. gid.op always 0 in other cases */
		else if (r->gid.op && (pd->lookup.done || (pd->lookup.done =
		    pf_socket_lookup(direction, pd), 1)) &&
		    !pf_match_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1],
		    pd->lookup.gid))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob &&
		    r->prob <= arc4random_uniform(UINT_MAX - 1) + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->rcv_kif && !pf_match_rcvif(m, r))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY &&
		    (pd->proto != IPPROTO_TCP || !pf_osfp_match(
		    pf_osfp_fingerprint(pd, m, off, th),
		    r->os_fingerprint)))
			r = TAILQ_NEXT(r, entries);
		else {
			if (r->tag)
				tag = r->tag;
			if (r->anchor == NULL) {
				lastr = r;
				if (r->action == PF_MATCH) {
					if ((ri = pool_get(&pf_rule_item_pl,
					    PR_NOWAIT)) == NULL) {
						REASON_SET(&reason,
						    PFRES_MEMORY);
						goto cleanup;
					}
					ri->r = r;
					/* order is irrelevant */
					SLIST_INSERT_HEAD(&rules, ri, entry);
					pf_rule_to_actions(r, &act);
					if (pf_get_transaddr(r, pd, sns) ==
					    -1) {
						REASON_SET(&reason,
						    PFRES_MEMORY);
						goto cleanup;
					}
					if (r->log || act.log & PF_LOG_MATCHES)
						PFLOG_PACKET(kif, h, m, af,
						    direction, reason, r,
						    a, ruleset, pd);
				} else {
					match = 1;
					*rm = r;
					*am = a;
					*rsm = ruleset;
					if (act.log & PF_LOG_MATCHES)
						PFLOG_PACKET(kif, h, m, af,
						    direction, reason, r,
						    a, ruleset, pd);
				}

				if ((*rm)->quick)
					break;
				r = TAILQ_NEXT(r, entries);
			} else
				pf_step_into_anchor(&asd, &ruleset,
				    &r, &a, &match);
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    &r, &a, &match))
			break;
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	/* apply actions for last matching pass/block rule */
	pf_rule_to_actions(r, &act);
	if (pf_get_transaddr(r, pd, sns) == -1) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto cleanup;
	}
	REASON_SET(&reason, PFRES_MATCH);

	if (r->log || act.log & PF_LOG_MATCHES)
		PFLOG_PACKET(kif, h, m, af, direction, reason,
		    r, a, ruleset, pd);

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		if (pd->proto == IPPROTO_TCP &&
		    ((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(th->th_flags & TH_RST)) {
			u_int32_t	 ack = ntohl(th->th_seq) + pd->p_len;
			int		 len = 0;
			struct ip	*h4;
			struct ip6_hdr	*h6;

			switch (af) {
			case AF_INET:
				h4 = mtod(m, struct ip *);
				len = ntohs(h4->ip_len) - off;
				break;
			case AF_INET6:
				h6 = mtod(m, struct ip6_hdr *);
				len = ntohs(h6->ip6_plen) - (off - sizeof(*h6));
				break;
			}

			if (pf_check_proto_cksum(m, off, len, IPPROTO_TCP, af))
				REASON_SET(&reason, PFRES_PROTCKSUM);
			else {
				if (th->th_flags & TH_SYN)
					ack++;
				if (th->th_flags & TH_FIN)
					ack++;
				pf_send_tcp(r, af, pd->dst,
				    pd->src, th->th_dport, th->th_sport,
				    ntohl(th->th_ack), ack, TH_RST|TH_ACK, 0, 0,
				    r->return_ttl, 1, 0, pd->rdomain,
				    pd->eh, kif->pfik_ifp);
			}
		} else if (pd->proto != IPPROTO_ICMP && af == AF_INET &&
		    r->return_icmp)
			pf_send_icmp(m, r->return_icmp >> 8,
			    r->return_icmp & 255, af, r, pd->rdomain);
		else if (pd->proto != IPPROTO_ICMPV6 && af == AF_INET6 &&
		    r->return_icmp6)
			pf_send_icmp(m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, af, r, pd->rdomain);
	}

	if (r->action == PF_DROP)
		goto cleanup;

	pf_tag_packet(m, tag, act.rtableid);
	if (act.rtableid >= 0 &&
	    rtable_l2(act.rtableid) != pd->rdomain)
		pd->destchg = 1;

	if (r->action == PF_PASS && af == AF_INET && ! r->allow_opts) {
		struct ip	*h4 = mtod(m, struct ip *);
			
		if (h4->ip_hl > 5) {
			REASON_SET(&reason, PFRES_IPOPTIONS);
			pd->pflog |= PF_LOG_FORCE;
			DPFPRINTF(LOG_NOTICE, "dropping packet with "
			    "ip options in pf_test_rule()");
			goto cleanup;
		}
	}

	if (!state_icmp && r->keep_state) {
		int action;

		if (r->rule_flag & PFRULE_SRCTRACK &&
		    pf_insert_src_node(&sns[PF_SN_NONE], r, PF_SN_NONE, pd->af,
		    pd->src, NULL, 0) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}

		action = pf_create_state(r, a, pd, &skw, &sks, m, off,
		    &rewrite, kif, sm, tag, &rules, &act, sns);

		if (action != PF_PASS)
			return (action);
		if (sks != skw) {
			struct pf_state_key	*sk;

			if (pd->dir == PF_IN)
				sk = sks;
			else
				sk = skw;
			rewrite += pf_translate(pd,
			    &sk->addr[pd->sidx], sk->port[pd->sidx],
			    &sk->addr[pd->didx], sk->port[pd->didx],
			    virtual_type, icmp_dir);
		}
	} else {
		while ((ri = SLIST_FIRST(&rules))) {
			SLIST_REMOVE_HEAD(&rules, entry);
			pool_put(&pf_rule_item_pl, ri);
		}
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite && hdrlen)
		m_copyback(m, off, hdrlen, pd->hdr.any, M_NOWAIT);

#if NPFSYNC > 0
	if (*sm != NULL && !ISSET((*sm)->state_flags, PFSTATE_NOSYNC) &&
	    direction == PF_OUT && pfsync_up()) {
		/*
		 * We want the state created, but we dont
		 * want to send this in case a partner
		 * firewall has to know about it to allow
		 * replies through it.
		 */
		if (pfsync_defer(*sm, m))
			return (PF_DEFER);
	}
#endif

	return (PF_PASS);

cleanup:
	while ((ri = SLIST_FIRST(&rules))) {
		SLIST_REMOVE_HEAD(&rules, entry);
		pool_put(&pf_rule_item_pl, ri);
	}

	return (PF_DROP);
}

static __inline int
pf_create_state(struct pf_rule *r, struct pf_rule *a, struct pf_pdesc *pd,
    struct pf_state_key **skw, struct pf_state_key **sks, struct mbuf *m,
    int off, int *rewrite, struct pfi_kif *kif, struct pf_state **sm,
    int tag, struct pf_rule_slist *rules,
    struct pf_rule_actions *act, struct pf_src_node *sns[PF_SN_MAX])
{
	struct pf_state		*s = NULL;
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 mss = tcp_mssdflt;
	u_short			 reason;
	u_int			 i;

	/* check maximums */
	if (r->max_states && (r->states_cur >= r->max_states)) {
		pf_status.lcounters[LCNT_STATES]++;
		REASON_SET(&reason, PFRES_MAXSTATES);
		return (PF_DROP);
	}

	s = pool_get(&pf_state_pl, PR_NOWAIT | PR_ZERO);
	if (s == NULL) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto csfailed;
	}
	s->rule.ptr = r;
	s->anchor.ptr = a;
	bcopy(rules, &s->match_rules, sizeof(s->match_rules));
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
	s->sync_state = PFSYNC_S_NONE;
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
			pf_change_a(&th->th_seq, &th->th_sum,
			    htonl(s->src.seqlo + s->src.seqdiff), 0);
			*rewrite = 1;
		} else
			s->src.seqdiff = 0;
		if (th->th_flags & TH_SYN) {
			s->src.seqhi++;
			s->src.wscale = pf_get_wscale(m, off,
			    th->th_off, pd->af);
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

	s->creation = time_second;
	s->expire = time_second;

	if (pd->proto == IPPROTO_TCP) {
		if (s->state_flags & PFSTATE_SCRUB_TCP &&
		    pf_normalize_tcp_init(m, off, pd, th, &s->src, &s->dst)) {
			REASON_SET(&reason, PFRES_MEMORY);
			goto csfailed;
		}
		if (s->state_flags & PFSTATE_SCRUB_TCP && s->src.scrub &&
		    pf_normalize_tcp_stateful(m, off, pd, &reason, th, s,
		    &s->src, &s->dst, rewrite)) {
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

	if (pf_state_insert(BOUND_IFACE(r, kif), *skw, *sks, s)) {
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
		mss = pf_get_mss(m, off, th->th_off, pd->af);
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

	if (PF_ANEQ(daddr, pd->dst, pd->af))
		pd->destchg = 1;

	switch (pd->proto) {
	case IPPROTO_TCP:
		if (PF_ANEQ(saddr, pd->src, pd->af) || *pd->sport != sport) {
			pf_change_ap(pd->src, pd->sport, &pd->hdr.tcp->th_sum,
			    saddr, sport, 0, pd->af);	
			rewrite = 1;
		}
		if (PF_ANEQ(daddr, pd->dst, pd->af) || *pd->dport != dport) {
			pf_change_ap(pd->dst, pd->dport, &pd->hdr.tcp->th_sum,
			    daddr, dport, 0, pd->af);
			rewrite = 1;
		}
		break;

	case IPPROTO_UDP:
		if (PF_ANEQ(saddr, pd->src, pd->af) || *pd->sport != sport) {
			pf_change_ap(pd->src, pd->sport, &pd->hdr.udp->uh_sum,
			    saddr, sport, 1, pd->af);
			rewrite = 1;
		}
		if (PF_ANEQ(daddr, pd->dst, pd->af) || *pd->dport != dport) {
			pf_change_ap(pd->dst, pd->dport, &pd->hdr.udp->uh_sum,
			    daddr, dport, 1, pd->af);
			rewrite = 1;
		}
		break;

#ifdef INET
	case IPPROTO_ICMP:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET)
			return (0);

		if (PF_ANEQ(saddr, pd->src, pd->af)) {
			pf_change_a(&pd->src->v4.s_addr, NULL,
			    saddr->v4.s_addr, 0);
			rewrite = 1;
		}
		if (PF_ANEQ(daddr, pd->dst, pd->af)) {
			pf_change_a(&pd->dst->v4.s_addr, NULL,
			    daddr->v4.s_addr, 0);
			rewrite = 1;
		}
		if (virtual_type == htons(ICMP_ECHO)) {
			u_int16_t icmpid = (icmp_dir == PF_IN) ? sport : dport;

			if (icmpid != pd->hdr.icmp->icmp_id) {
				pd->hdr.icmp->icmp_cksum = pf_cksum_fixup(
				    pd->hdr.icmp->icmp_cksum,
				    pd->hdr.icmp->icmp_id, icmpid, 0);
				pd->hdr.icmp->icmp_id = icmpid;
				rewrite = 1;
			}
		}
		break;
#endif /* INET */

#ifdef INET6
	case IPPROTO_ICMPV6:
		/* pf_translate() is also used when logging invalid packets */
		if (pd->af != AF_INET6)
			return (0);

		if (PF_ANEQ(saddr, pd->src, pd->af)) {
			pf_change_a6(pd->src, &pd->hdr.icmp6->icmp6_cksum,
			    saddr, 0);
			rewrite = 1;
		}
		if (PF_ANEQ(daddr, pd->dst, pd->af)) {
			pf_change_a6(pd->dst, &pd->hdr.icmp6->icmp6_cksum,
			    daddr, 0);
			rewrite = 1;
		}
		break;
#endif /* INET6 */

	default:
		switch (pd->af) {
#ifdef INET
		case AF_INET:
			if (PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a(&pd->src->v4.s_addr, NULL,
				    saddr->v4.s_addr, 0);
				rewrite = 1;
			}
			if (PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a(&pd->dst->v4.s_addr, NULL,
				    daddr->v4.s_addr, 0);
				rewrite = 1;
			}
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (PF_ANEQ(saddr, pd->src, pd->af)) {
				pf_change_a6(pd->src, NULL, saddr, 0);
				rewrite = 1;
			}
			if (PF_ANEQ(daddr, pd->dst, pd->af)) {
				pf_change_a6(pd->dst, NULL, daddr, 0);
				rewrite = 1;
			}
			break;
#endif /* INET6 */
		}
	}
	return (rewrite);
}

int
pf_test_fragment(struct pf_rule **rm, int direction, struct pfi_kif *kif,
    struct mbuf *m, struct pf_pdesc *pd, struct pf_rule **am,
    struct pf_ruleset **rsm)
{
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	sa_family_t		 af = pd->af;
	u_short			 reason;
	int			 tag = -1;
	int			 asd = 0;
	int			 match = 0;

	r = TAILQ_FIRST(pf_main_ruleset.rules.active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, pd->src, af,
		    r->src.neg, kif, pd->rdomain))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af,
		    r->dst.neg, NULL, pd->rdomain))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY)
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_UDP &&
		    (r->src.port_op || r->dst.port_op))
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_TCP &&
		    (r->src.port_op || r->dst.port_op || r->flagset))
			r = TAILQ_NEXT(r, entries);
		else if ((pd->proto == IPPROTO_ICMP ||
		    pd->proto == IPPROTO_ICMPV6) &&
		    (r->type || r->code))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <=
		    (arc4random() % (UINT_MAX - 1) + 1))
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, &tag))
			r = TAILQ_NEXT(r, entries);
		else {
			if (r->anchor == NULL) {
				match = 1;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				if ((*rm)->quick)
					break;
				r = TAILQ_NEXT(r, entries);
			} else
				pf_step_into_anchor(&asd, &ruleset,
				    &r, &a, &match);
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    &r, &a, &match))
			break;
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	REASON_SET(&reason, PFRES_MATCH);

	if (r->log)
		PFLOG_PACKET(kif, h, m, af, direction, reason, r, a, ruleset,
		    pd);

	if (r->action == PF_DROP)
		return (PF_DROP);

	pf_tag_packet(m, tag, -1);

	return (PF_PASS);
}

int
pf_tcp_track_full(struct pf_state_peer *src, struct pf_state_peer *dst,
	struct pf_state **state, struct pfi_kif *kif, struct mbuf *m, int off,
	struct pf_pdesc *pd, u_short *reason, int *copyback)
{
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 win = ntohs(th->th_win);
	u_int32_t		 ack, end, seq, orig_seq;
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
			if (pf_normalize_tcp_init(m, off, pd, th, src, dst)) {
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
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			*copyback = 1;
		} else {
			ack = ntohl(th->th_ack);
		}

		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN) {
			end++;
			if (dst->wscale & PF_WSCALE_FLAG) {
				src->wscale = pf_get_wscale(m, off, th->th_off,
				    pd->af);
				if (src->wscale & PF_WSCALE_FLAG) {
					/* Remove scale factor from initial
					 * window */
					sws = src->wscale & PF_WSCALE_MASK;
					win = ((u_int32_t)win + (1 << sws) - 1)
					    >> sws;
					dws = dst->wscale & PF_WSCALE_MASK;
				} else {
					/* fixup other window */
					dst->max_win <<= dst->wscale &
					    PF_WSCALE_MASK;
					/* in case of a retrans SYN|ACK */
					dst->wscale = 0;
				}
			}
		}
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
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			*copyback = 1;
		}
		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN)
			end++;
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
		end = seq;
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
		if (pf_modulate_sack(m, off, pd, th, dst))
			*copyback = 1;
	}


#define MAXACKWINDOW (0xffff + 1500)	/* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one reassembled fragment backwards */
	    (ackskew <= (MAXACKWINDOW << sws)) &&
	    /* Acking not more than one window forward */
	    ((th->th_flags & TH_RST) == 0 || orig_seq == src->seqlo ||
	    (orig_seq == src->seqlo + 1) || (orig_seq + 1 == src->seqlo) ||
	    (pd->flags & PFDESC_IP_REAS) == 0)) {
	    /* Require an exact/+1 sequence match on resets when possible */

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, copyback))
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
		(*state)->expire = time_second;
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
	    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) &&
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
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, copyback))
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
				    pd->rdomain, pd->eh, kif->pfik_ifp);
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
			    SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
			    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) ?
			    ' ': '2',
			    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
			    (ackskew <= (MAXACKWINDOW << sws)) ? ' ' : '4',
			    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) ?' ' :'5',
			    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW) ?' ' :'6');
		}
		REASON_SET(reason, PFRES_BADSTATE);
		return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_tcp_track_sloppy(struct pf_state_peer *src, struct pf_state_peer *dst,
	struct pf_state **state, struct pf_pdesc *pd, u_short *reason)
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
	(*state)->expire = time_second;
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

int
pf_test_state_tcp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, struct pf_pdesc *pd,
    u_short *reason)
{
	struct pf_state_key_cmp	 key;
	struct tcphdr		*th = pd->hdr.tcp;
	int			 copyback = 0;
	struct pf_state_peer	*src, *dst;
	struct pf_state_key	*sk;

	key.af = pd->af;
	key.proto = IPPROTO_TCP;
	key.rdomain = pd->rdomain;
	if (direction == PF_IN)	{	/* wire side, straight */
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = th->th_sport;
		key.port[1] = th->th_dport;
	} else {			/* stack side, reverse */
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = th->th_sport;
		key.port[0] = th->th_dport;
	}

	STATE_LOOKUP(kif, &key, direction, *state, m);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	sk = (*state)->key[pd->didx];

	if ((*state)->src.state == PF_TCPS_PROXY_SRC) {
		if (direction != (*state)->direction) {
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
		} else if (!(th->th_flags & TH_ACK) ||
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
		if (direction == (*state)->direction) {
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

	if (((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN) &&
	    dst->state >= TCPS_FIN_WAIT_2 &&
	    src->state >= TCPS_FIN_WAIT_2) {
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: state reuse ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			addlog("\n");
		}
		/* XXX make sure it's the same direction ?? */
		(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
		pf_unlink_state(*state);
		*state = NULL;
		return (PF_DROP);
	}

	if ((*state)->state_flags & PFSTATE_SLOPPY) {
		if (pf_tcp_track_sloppy(src, dst, state, pd, reason) == PF_DROP)
			return (PF_DROP);
	} else {
		if (pf_tcp_track_full(src, dst, state, kif, m, off, pd, reason,
		    &copyback) == PF_DROP)
			return (PF_DROP);
	}

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

		if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], pd->af) ||
		    nk->port[pd->sidx] != th->th_sport)
			pf_change_ap(pd->src, &th->th_sport, &th->th_sum,
			    &nk->addr[pd->sidx], nk->port[pd->sidx], 0, pd->af);
		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    pd->rdomain != nk->rdomain)
			pd->destchg = 1;
		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    nk->port[pd->didx] != th->th_dport)
			pf_change_ap(pd->dst, &th->th_dport, &th->th_sum,	
			    &nk->addr[pd->didx], nk->port[pd->didx], 0, pd->af);
		m->m_pkthdr.rdomain = nk->rdomain;
		copyback = 1;
	}

	/* Copyback sequence modulation or stateful scrub changes if needed */
	if (copyback)
		m_copyback(m, off, sizeof(*th), th, M_NOWAIT);

	return (PF_PASS);
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_key_cmp	 key;
	struct udphdr		*uh = pd->hdr.udp;

	key.af = pd->af;
	key.proto = IPPROTO_UDP;
	key.rdomain = pd->rdomain;
	if (direction == PF_IN)	{	/* wire side, straight */
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = uh->uh_sport;
		key.port[1] = uh->uh_dport;
	} else {			/* stack side, reverse */
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = uh->uh_sport;
		key.port[0] = uh->uh_dport;
	}

	STATE_LOOKUP(kif, &key, direction, *state, m);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFUDPS_SINGLE)
		src->state = PFUDPS_SINGLE;
	if (dst->state == PFUDPS_SINGLE)
		dst->state = PFUDPS_MULTIPLE;

	/* update expire time */
	(*state)->expire = time_second;
	if (src->state == PFUDPS_MULTIPLE && dst->state == PFUDPS_MULTIPLE)
		(*state)->timeout = PFTM_UDP_MULTIPLE;
	else
		(*state)->timeout = PFTM_UDP_SINGLE;

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

		if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], pd->af) ||
		    nk->port[pd->sidx] != uh->uh_sport)
			pf_change_ap(pd->src, &uh->uh_sport, &uh->uh_sum,
			    &nk->addr[pd->sidx], nk->port[pd->sidx], 1, pd->af);
		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    pd->rdomain != nk->rdomain)
			pd->destchg = 1;
		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    nk->port[pd->didx] != uh->uh_dport)
			pf_change_ap(pd->dst, &uh->uh_dport, &uh->uh_sum,
			    &nk->addr[pd->didx], nk->port[pd->didx], 1, pd->af);
		m->m_pkthdr.rdomain = nk->rdomain;
		m_copyback(m, off, sizeof(*uh), uh, M_NOWAIT);
	}

	return (PF_PASS);
}

int
pf_icmp_state_lookup(struct pf_state_key_cmp *key, struct pf_pdesc *pd,
    struct pf_state **state, struct mbuf *m, int direction, struct pfi_kif *kif,
    u_int16_t icmpid, u_int16_t type, int icmp_dir, int *iidx, int multi,
    int inner)
{
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
	if (pd->af == AF_INET6 && multi != PF_ICMP_MULTI_NONE) {
		switch (multi) {
		case PF_ICMP_MULTI_SOLICITED:
			key->addr[pd->sidx].addr32[0] = IPV6_ADDR_INT32_MLL;
			key->addr[pd->sidx].addr32[1] = 0;
			key->addr[pd->sidx].addr32[2] = IPV6_ADDR_INT32_ONE;
			key->addr[pd->sidx].addr32[3] = pd->src->addr32[3];
			key->addr[pd->sidx].addr8[12] = 0xff;
			break;
		case PF_ICMP_MULTI_LINK:
			key->addr[pd->sidx].addr32[0] = IPV6_ADDR_INT32_MLL;
			key->addr[pd->sidx].addr32[1] = 0;
			key->addr[pd->sidx].addr32[2] = 0;
			key->addr[pd->sidx].addr32[3] = IPV6_ADDR_INT32_ONE;
			break;
		}
	} else
		PF_ACPY(&key->addr[pd->sidx], pd->src, key->af);
	PF_ACPY(&key->addr[pd->didx], pd->dst, key->af);

	STATE_LOOKUP(kif, key, direction, *state, m);

	/* Is this ICMP message flowing in right direction? */
	if ((*state)->rule.ptr->type &&
	    (((!inner && (*state)->direction == direction) ||
	    (inner && (*state)->direction != direction)) ?
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
pf_test_state_icmp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, struct pf_pdesc *pd, u_short *reason)
{
	struct pf_addr  *saddr = pd->src, *daddr = pd->dst;
	u_int16_t	 icmpid, *icmpsum, virtual_id, virtual_type;
	u_int8_t	 icmptype;
	int		 icmp_dir, iidx, ret, multi;
	struct pf_state_key_cmp key;

	switch (pd->proto) {
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpid = pd->hdr.icmp->icmp_id;
		icmpsum = &pd->hdr.icmp->icmp_cksum;
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpid = pd->hdr.icmp6->icmp6_id;
		icmpsum = &pd->hdr.icmp6->icmp6_cksum;
		break;
#endif /* INET6 */
	}

	if (pf_icmp_mapping(pd, icmptype, &icmp_dir, &multi,
	    &virtual_id, &virtual_type) == 0) {
		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		ret = pf_icmp_state_lookup(&key, pd, state, m, direction,
		    kif, virtual_id, virtual_type, icmp_dir, &iidx,
		    PF_ICMP_MULTI_NONE, 0);
		if (ret >= 0) {
			if (ret == PF_DROP && pd->af == AF_INET6 &&
			    icmp_dir == PF_OUT) {
				ret = pf_icmp_state_lookup(&key, pd, state, m,
				    direction, kif, virtual_id, virtual_type,
				    icmp_dir, &iidx, multi, 0);
				if (ret >= 0)
					return (ret);
			} else
				return (ret);
		}

		(*state)->expire = time_second;
		(*state)->timeout = PFTM_ICMP_ERROR_REPLY;

		/* translate source/destination address, if necessary */
		if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
			struct pf_state_key *nk = (*state)->key[pd->didx];

			if (pd->rdomain != nk->rdomain)
				pd->destchg = 1;
			m->m_pkthdr.rdomain = nk->rdomain;

			switch (pd->af) {
#ifdef INET
			case AF_INET:
				if (PF_ANEQ(pd->src,
				    &nk->addr[pd->sidx], AF_INET))
					pf_change_a(&saddr->v4.s_addr, NULL,
					    nk->addr[pd->sidx].v4.s_addr, 0);

				if (PF_ANEQ(pd->dst, &nk->addr[pd->didx],
				    AF_INET)) {
					pf_change_a(&daddr->v4.s_addr, NULL,
					    nk->addr[pd->didx].v4.s_addr, 0);
					pd->destchg = 1;
				}

				if (nk->port[iidx] !=
				    pd->hdr.icmp->icmp_id) {
					pd->hdr.icmp->icmp_cksum =
					    pf_cksum_fixup(
					    pd->hdr.icmp->icmp_cksum,
					     pd->hdr.icmp->icmp_id,
					    nk->port[iidx], 0);
					pd->hdr.icmp->icmp_id = nk->port[iidx];
				}

				m_copyback(m, off, ICMP_MINLEN,
				    pd->hdr.icmp, M_NOWAIT);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (PF_ANEQ(pd->src,
				    &nk->addr[pd->sidx], AF_INET6))
					pf_change_a6(saddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &nk->addr[pd->sidx], 0);

				if (PF_ANEQ(pd->dst,
				    &nk->addr[pd->didx], AF_INET6)) {
					pf_change_a6(daddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &nk->addr[pd->didx], 0);
					pd->destchg = 1;
				}

				m_copyback(m, off,
				    sizeof(struct icmp6_hdr),
				    pd->hdr.icmp6, M_NOWAIT);
				break;
#endif /* INET6 */
			}
		}
		return (PF_PASS);

	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct pf_pdesc	pd2;
#ifdef INET
		struct ip	h2;
#endif /* INET */
#ifdef INET6
		struct ip6_hdr	h2_6;
		int		terminal = 0;
#endif /* INET6 */
		int		ipoff2;
		int		off2;

		pd2.af = pd->af;
		pd2.rdomain = pd->rdomain;
		/* Payload packet is from the opposite direction. */
		pd2.sidx = (direction == PF_IN) ? 1 : 0;
		pd2.didx = (direction == PF_IN) ? 0 : 1;
		switch (pd->af) {
#ifdef INET
		case AF_INET:
			/* offset of h2 in mbuf chain */
			ipoff2 = off + ICMP_MINLEN;

			if (!pf_pull_hdr(m, ipoff2, &h2, sizeof(h2),
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
			off2 = ipoff2 + (h2.ip_hl << 2);

			pd2.proto = h2.ip_p;
			pd2.src = (struct pf_addr *)&h2.ip_src;
			pd2.dst = (struct pf_addr *)&h2.ip_dst;
			pd2.ip_sum = &h2.ip_sum;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			ipoff2 = off + sizeof(struct icmp6_hdr);

			if (!pf_pull_hdr(m, ipoff2, &h2_6, sizeof(h2_6),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (ip6)");
				return (PF_DROP);
			}
			pd2.proto = h2_6.ip6_nxt;
			pd2.src = (struct pf_addr *)&h2_6.ip6_src;
			pd2.dst = (struct pf_addr *)&h2_6.ip6_dst;
			pd2.ip_sum = NULL;
			off2 = ipoff2 + sizeof(h2_6);
			do {
				switch (pd2.proto) {
				case IPPROTO_FRAGMENT:
					/*
					 * ICMPv6 error messages for
					 * non-first fragments
					 */
					REASON_SET(reason, PFRES_FRAG);
					return (PF_DROP);
				case IPPROTO_AH:
				case IPPROTO_HOPOPTS:
				case IPPROTO_ROUTING:
				case IPPROTO_DSTOPTS: {
					/* get next header and header length */
					struct ip6_ext opt6;

					if (!pf_pull_hdr(m, off2, &opt6,
					    sizeof(opt6), NULL, reason,
					    pd2.af)) {
						DPFPRINTF(LOG_NOTICE,
						    "ICMPv6 short opt");
						return (PF_DROP);
					}
					if (pd2.proto == IPPROTO_AH)
						off2 += (opt6.ip6e_len + 2) * 4;
					else
						off2 += (opt6.ip6e_len + 1) * 8;
					pd2.proto = opt6.ip6e_nxt;
					/* goto the next header */
					break;
				}
				default:
					terminal++;
					break;
				}
			} while (!terminal);
			break;
#endif /* INET6 */
		}

		switch (pd2.proto) {
		case IPPROTO_TCP: {
			struct tcphdr		 th;
			u_int32_t		 seq;
			struct pf_state_peer	*src, *dst;
			u_int8_t		 dws;
			int			 copyback = 0;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(m, off2, &th, 8, NULL, reason,
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

			STATE_LOOKUP(kif, &key, direction, *state, m);

			if (direction == (*state)->direction) {
				src = &(*state)->dst;
				dst = &(*state)->src;
			} else {
				src = &(*state)->src;
				dst = &(*state)->dst;
			}

			if (src->wscale && dst->wscale)
				dws = dst->wscale & PF_WSCALE_MASK;
			else
				dws = 0;

			/* Demodulate sequence number */
			seq = ntohl(th.th_seq) - src->seqdiff;
			if (src->seqdiff) {
				pf_change_a(&th.th_seq, icmpsum,
				    htonl(seq), 0);
				copyback = 1;
			}

			if (!((*state)->state_flags & PFSTATE_SLOPPY) &&
			    (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)))) {
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
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != th.th_sport)
					pf_change_icmp(pd2.src, &th.th_sport,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], NULL,
					    pd2.ip_sum, icmpsum, 0, pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				m->m_pkthdr.rdomain = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != th.th_dport)
					pf_change_icmp(pd2.dst, &th.th_dport,
					    saddr, &nk->addr[pd2.didx],
					    nk->port[pd2.didx], NULL,
					    pd2.ip_sum, icmpsum, 0, pd2.af);
				copyback = 1;
			}

			if (copyback) {
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2),
					    &h2, M_NOWAIT);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, 8, &th, M_NOWAIT);
			}

			return (PF_PASS);
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr		uh;

			if (!pf_pull_hdr(m, off2, &uh, sizeof(uh),
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

			STATE_LOOKUP(kif, &key, direction, *state, m);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != uh.uh_sport)
					pf_change_icmp(pd2.src, &uh.uh_sport,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], &uh.uh_sum,
					    pd2.ip_sum, icmpsum, 1, pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				m->m_pkthdr.rdomain = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != uh.uh_dport)
					pf_change_icmp(pd2.dst, &uh.uh_dport,
					    saddr, &nk->addr[pd2.didx],
					    nk->port[pd2.didx], &uh.uh_sum,
					    pd2.ip_sum, icmpsum, 1, pd2.af);

				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2), &h2,
					    M_NOWAIT);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, sizeof(uh), &uh, M_NOWAIT);
			}
			return (PF_PASS);
			break;
		}
#ifdef INET
		case IPPROTO_ICMP: {
			struct icmp		iih;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp)");
				return (PF_DROP);
			}

			pd2.hdr.icmp = &iih;
			pf_icmp_mapping(&pd2, iih.icmp_type,
			    &icmp_dir, &multi, &virtual_id, &virtual_type);

			ret = pf_icmp_state_lookup(&key, &pd2, state, m,
			    direction, kif, virtual_id, virtual_type,
			    icmp_dir, &iidx, PF_ICMP_MULTI_NONE, 1);
			if (ret >= 0)
				return (ret);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    (virtual_type == htons(ICMP_ECHO) &&
				    nk->port[iidx] != iih.icmp_id))
					pf_change_icmp(pd2.src,
					    (virtual_type == htons(ICMP_ECHO)) ?
					    &iih.icmp_id : NULL,
					    daddr, &nk->addr[pd2.sidx],
					    (virtual_type == htons(ICMP_ECHO)) ?
					    nk->port[iidx] : 0, NULL,
					    pd2.ip_sum, icmpsum, 0, AF_INET);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				m->m_pkthdr.rdomain = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
                                       pf_change_icmp(pd2.dst, NULL, saddr,
					    &nk->addr[pd2.didx], 0, NULL,
					    pd2.ip_sum, icmpsum, 0, AF_INET);

				m_copyback(m, off, ICMP_MINLEN, pd->hdr.icmp,
				    M_NOWAIT);
				m_copyback(m, ipoff2, sizeof(h2), &h2,
				    M_NOWAIT);
				m_copyback(m, off2, ICMP_MINLEN, &iih,
				    M_NOWAIT);
			}
			return (PF_PASS);
			break;
		}
#endif /* INET */
#ifdef INET6
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr	iih;

			if (!pf_pull_hdr(m, off2, &iih,
			    sizeof(struct icmp6_hdr), NULL, reason, pd2.af)) {
				DPFPRINTF(LOG_NOTICE,
				    "ICMP error message too short (icmp6)");
				return (PF_DROP);
			}

			pd2.hdr.icmp6 = &iih;
			pf_icmp_mapping(&pd2, iih.icmp6_type,
			    &icmp_dir, &multi, &virtual_id, &virtual_type);
			ret = pf_icmp_state_lookup(&key, &pd2, state, m,
			    direction, kif, virtual_id, virtual_type,
			    icmp_dir, &iidx, PF_ICMP_MULTI_NONE, 1);
			if (ret >= 0) {
				if (ret == PF_DROP && pd->af == AF_INET6 &&
				    icmp_dir == PF_OUT) {
					ret = pf_icmp_state_lookup(&key, pd,
					    state, m, direction, kif,
					    virtual_id, virtual_type,
					    icmp_dir, &iidx, multi, 1);
					if (ret >= 0)
						return (ret);
				} else
					return (ret);
			}

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
			 	    ((virtual_type ==
				    htons(ICMP6_ECHO_REQUEST)) &&
				    nk->port[pd2.sidx] != iih.icmp6_id))
					pf_change_icmp(pd2.src,
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? &iih.icmp6_id : NULL,
					    daddr, &nk->addr[pd2.sidx],
					    (virtual_type ==
					    htons(ICMP6_ECHO_REQUEST))
					    ? nk->port[iidx] : 0, NULL,
					    pd2.ip_sum, icmpsum, 0, AF_INET6);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				m->m_pkthdr.rdomain = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd2.dst, NULL, saddr,
					    &nk->addr[pd2.didx], 0, NULL,
					    pd2.ip_sum, icmpsum, 0, AF_INET6);

				m_copyback(m, off, sizeof(struct icmp6_hdr),
				    pd->hdr.icmp6, M_NOWAIT);
				m_copyback(m, ipoff2, sizeof(h2_6), &h2_6,
				    M_NOWAIT);
				m_copyback(m, off2, sizeof(struct icmp6_hdr),
				    &iih, M_NOWAIT);
			}
			return (PF_PASS);
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

			STATE_LOOKUP(kif, &key, direction, *state, m);

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af))
					pf_change_icmp(pd2.src, NULL, daddr,
					    &nk->addr[pd2.sidx], 0, NULL,
					    pd2.ip_sum, icmpsum, 0, pd2.af);

				if (PF_ANEQ(pd2.dst, &nk->addr[pd2.didx],
				    pd2.af) || pd2.rdomain != nk->rdomain)
					pd->destchg = 1;
				m->m_pkthdr.rdomain = nk->rdomain;

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd2.dst, NULL, saddr,
					    &nk->addr[pd2.didx], 0, NULL,
					    pd2.ip_sum, icmpsum, 0, pd2.af);

				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    pd->hdr.icmp, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2), &h2,
					    M_NOWAIT);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    pd->hdr.icmp6, M_NOWAIT);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    &h2_6, M_NOWAIT);
					break;
#endif /* INET6 */
				}
			}
			return (PF_PASS);
			break;
		}
		}
	}
}

int
pf_test_state_other(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_key_cmp	 key;

	key.af = pd->af;
	key.proto = pd->proto;
	key.rdomain = pd->rdomain;
	if (direction == PF_IN)	{
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = key.port[1] = 0;
	} else {
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = key.port[0] = 0;
	}

	STATE_LOOKUP(kif, &key, direction, *state, m);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFOTHERS_SINGLE)
		src->state = PFOTHERS_SINGLE;
	if (dst->state == PFOTHERS_SINGLE)
		dst->state = PFOTHERS_MULTIPLE;

	/* update expire time */
	(*state)->expire = time_second;
	if (src->state == PFOTHERS_MULTIPLE && dst->state == PFOTHERS_MULTIPLE)
		(*state)->timeout = PFTM_OTHER_MULTIPLE;
	else
		(*state)->timeout = PFTM_OTHER_SINGLE;

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

		KASSERT(nk);
		KASSERT(pd);
		KASSERT(pd->src);
		KASSERT(pd->dst);

		switch (pd->af) {
#ifdef INET
		case AF_INET:
			if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], AF_INET))
				pf_change_a(&pd->src->v4.s_addr, NULL,
				    nk->addr[pd->sidx].v4.s_addr,
				    0);
			if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], AF_INET)) {
				pf_change_a(&pd->dst->v4.s_addr, NULL,
				    nk->addr[pd->didx].v4.s_addr,
				    0);
				pd->destchg = 1;
			}
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], AF_INET6))
				PF_ACPY(pd->src, &nk->addr[pd->sidx], pd->af);

			if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], AF_INET6)) {
				PF_ACPY(pd->dst, &nk->addr[pd->didx], pd->af);
				pd->destchg = 1;
			}
			break;
#endif /* INET6 */
		}
		if (pd->rdomain != nk->rdomain)
			pd->destchg = 1;

		m->m_pkthdr.rdomain = nk->rdomain;
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
#ifdef INET
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
#endif /* INET */
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
	struct sockaddr_in	*dst;
	int			 ret = 1;
	int			 check_mpath;
	extern int		 ipmultipath;
#ifdef INET6
	extern int		 ip6_multipath;
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro;
#else
	struct route		 ro;
#endif
	struct radix_node	*rn;
	struct rtentry		*rt;
	struct ifnet		*ifp;

	check_mpath = 0;
	bzero(&ro, sizeof(ro));
	ro.ro_tableid = rtableid;
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
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
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		if (ip6_multipath)
			check_mpath = 1;
		break;
#endif /* INET6 */
	default:
		return (0);
	}

	/* Skip checks for ipsec interfaces */
	if (kif != NULL && kif->pfik_ifp->if_type == IFT_ENC)
		goto out;

	rtalloc_noclone((struct route *)&ro);

	if (ro.ro_rt != NULL) {
		/* No interface given, this is a no-route check */
		if (kif == NULL)
			goto out;

		if (kif->pfik_ifp == NULL) {
			ret = 0;
			goto out;
		}

		/* Perform uRPF check if passed input interface */
		ret = 0;
		rn = (struct radix_node *)ro.ro_rt;
		do {
			rt = (struct rtentry *)rn;
			if (rt->rt_ifp->if_type == IFT_CARP)
				ifp = rt->rt_ifp->if_carpdev;
			else
				ifp = rt->rt_ifp;

			if (kif->pfik_ifp == ifp)
				ret = 1;
			rn = rn_mpath_next(rn, 0);
		} while (check_mpath == 1 && rn != NULL && ret == 0);
	} else
		ret = 0;
out:
	if (ro.ro_rt != NULL)
		RTFREE(ro.ro_rt);
	return (ret);
}

int
pf_rtlabel_match(struct pf_addr *addr, sa_family_t af, struct pf_addr_wrap *aw,
    int rtableid)
{
	struct sockaddr_in	*dst;
#ifdef INET6
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro;
#else
	struct route		 ro;
#endif
	int			 ret = 0;

	bzero(&ro, sizeof(ro));
	ro.ro_tableid = rtableid;
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		break;
#ifdef INET6
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		break;
#endif /* INET6 */
	default:
		return (0);
	}

	rtalloc_noclone((struct route *)&ro);

	if (ro.ro_rt != NULL) {
		if (ro.ro_rt->rt_labelid == aw->v.rtlabel)
			ret = 1;
		RTFREE(ro.ro_rt);
	}

	return (ret);
}

#ifdef INET
void
pf_route(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s)
{
	struct mbuf		*m0, *m1;
	struct route		 iproute;
	struct route		*ro = NULL;
	struct sockaddr_in	*dst;
	struct ip		*ip;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sn = NULL;
	int			 error = 0;
#ifdef IPSEC
	struct m_tag		*mtag;
#endif /* IPSEC */

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

	ro = &iproute;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = satosin(&ro->ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;
	ro->ro_tableid = m0->m_pkthdr.rdomain;

	if (!r->rt) {
		rtalloc(ro);
		if (ro->ro_rt == 0) {
			ipstat.ips_noroute++;
			goto bad;
		}

		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;

		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);

		m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	} else {
		if (s == NULL) {
			if (pf_map_addr(AF_INET, r, (struct pf_addr *)&ip->ip_src,
			    &naddr, NULL, &sn, &r->route, PF_SN_ROUTE)) {
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
		if (pf_test(PF_OUT, ifp, &m0, NULL) != PF_PASS)
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

	/* Copied from ip_output. */
#ifdef IPSEC
	/*
	 * If deferred crypto processing is needed, check that the
	 * interface supports it.
	 */
	if ((mtag = m_tag_find(m0, PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED, NULL))
	    != NULL && (ifp->if_capabilities & IFCAP_IPSEC) == 0) {
		/* Notify IPsec to do its own crypto. */
		ipsp_skipcrypto_unmark((struct tdb_ident *)(mtag + 1));
		goto bad;
	}
#endif /* IPSEC */

	in_proto_cksum_out(m0, ifp);

	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		ip->ip_sum = 0;
		if (ifp->if_capabilities & IFCAP_CSUM_IPv4) {
			m0->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
			ipstat.ips_outhwcsum++;
		} else
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
		/* Update relevant hardware checksum stats for TCP/UDP */
		if (m0->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
			tcpstat.tcps_outhwcsum++;
		else if (m0->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT)
			udpstat.udps_outhwcsum++;
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
	if (ro == &iproute && ro->ro_rt)
		RTFREE(ro->ro_rt);
	return;

bad:
	m_freem(m0);
	goto done;
}
#endif /* INET */

#ifdef INET6
void
pf_route6(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s)
{
	struct mbuf		*m0;
	struct route_in6	 ip6route;
	struct route_in6	*ro;
	struct sockaddr_in6	*dst;
	struct ip6_hdr		*ip6;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sn = NULL;

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

	ro = &ip6route;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = (struct sockaddr_in6 *)&ro->ro_dst;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;

	if (!r->rt) {
		m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		ip6_output(m0, NULL, NULL, 0, NULL, NULL, NULL);
		return;
	}

	if (s == NULL) {
		if (pf_map_addr(AF_INET6, r, (struct pf_addr *)&ip6->ip6_src,
		    &naddr, NULL, &sn, &r->route, PF_SN_ROUTE)) {
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
		if (pf_test6(PF_OUT, ifp, &m0, NULL) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
		if (m0->m_len < sizeof(struct ip6_hdr)) {
			DPFPRINTF(LOG_ERR,
			    "pf_route6: m0->m_len < sizeof(struct ip6_hdr)");
			goto bad;
		}
		ip6 = mtod(m0, struct ip6_hdr *);
	}

	/*
	 * If the packet is too large for the outgoing interface,
	 * send back an icmp6 error.
	 */
	if (IN6_IS_SCOPE_EMBED(&dst->sin6_addr))
		dst->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	if ((u_long)m0->m_pkthdr.len <= ifp->if_mtu) {
		nd6_output(ifp, ifp, m0, dst, NULL);
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
 */
int
pf_check_proto_cksum(struct mbuf *m, int off, int len, u_int8_t p,
    sa_family_t af)
{
	u_int16_t flag_ok, flag_bad;
	u_int16_t sum;

	switch (p) {
	case IPPROTO_TCP:
		flag_ok = M_TCP_CSUM_IN_OK;
		flag_bad = M_TCP_CSUM_IN_BAD;
		break;
	case IPPROTO_UDP:
		flag_ok = M_UDP_CSUM_IN_OK;
		flag_bad = M_UDP_CSUM_IN_BAD;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif /* INET6 */
		flag_ok = flag_bad = 0;
		break;
	default:
		return (1);
	}
	if (m->m_pkthdr.csum_flags & flag_ok)
		return (0);
	if (m->m_pkthdr.csum_flags & flag_bad)
		return (1);
	if (off < sizeof(struct ip) || len < sizeof(struct udphdr))
		return (1);
	if (m->m_pkthdr.len < off + len)
		return (1);
	switch (af) {
#ifdef INET
	case AF_INET:
		if (p == IPPROTO_ICMP) {
			if (m->m_len < off)
				return (1);
			m->m_data += off;
			m->m_len -= off;
			sum = in_cksum(m, len);
			m->m_data -= off;
			m->m_len += off;
		} else {
			if (m->m_len < sizeof(struct ip))
				return (1);
			sum = in4_cksum(m, p, off, len);
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (m->m_len < sizeof(struct ip6_hdr))
			return (1);
		sum = in6_cksum(m, p, off, len);
		break;
#endif /* INET6 */
	default:
		return (1);
	}
	if (sum) {
		m->m_pkthdr.csum_flags |= flag_bad;
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
		return (1);
	}
	m->m_pkthdr.csum_flags |= flag_ok;
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

int
pf_setup_pdesc(sa_family_t af, int dir, struct pf_pdesc *pd, struct mbuf *m,
    u_short *action, u_short *reason, struct pfi_kif *kif, struct pf_rule **a,
    struct pf_rule **r, struct pf_ruleset **ruleset, int *off, int *hdrlen)
{
	if (pd->hdr.any == NULL)
		panic("pf_setup_pdesc: no storage for headers provided");

	*hdrlen = 0;
	switch (af) {
#ifdef INET
	case AF_INET: {
		struct ip	*h;

		h = mtod(m, struct ip *);
		*off = h->ip_hl << 2;
		if (*off < (int)sizeof(*h)) {
			*action = PF_DROP;
			REASON_SET(reason, PFRES_SHORT);
			return (-1);
		}
		pd->src = (struct pf_addr *)&h->ip_src;
		pd->dst = (struct pf_addr *)&h->ip_dst;
		pd->sport = pd->dport = NULL;
		pd->ip_sum = &h->ip_sum;
		pd->proto_sum = NULL;
		pd->proto = h->ip_p;
		pd->dir = dir;
		pd->sidx = (dir == PF_IN) ? 0 : 1;
		pd->didx = (dir == PF_IN) ? 1 : 0;
		pd->af = AF_INET;
		pd->tos = h->ip_tos;
		pd->tot_len = ntohs(h->ip_len);
		pd->rdomain = rtable_l2(m->m_pkthdr.rdomain);

		/* fragments not reassembled handled later */
		if (h->ip_off & htons(IP_MF | IP_OFFMASK))
			return (0);

		switch (h->ip_p) {
		case IPPROTO_TCP: {
			struct tcphdr	*th = pd->hdr.tcp;

			if (!pf_pull_hdr(m, *off, th, sizeof(*th),
			    action, reason, AF_INET))
				return (-1);
			*hdrlen = sizeof(*th);
			pd->p_len = pd->tot_len - *off - (th->th_off << 2);
			pd->sport = &th->th_sport;
			pd->dport = &th->th_dport;
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr	*uh = pd->hdr.udp;

			if (!pf_pull_hdr(m, *off, uh, sizeof(*uh),
			    action, reason, AF_INET))
				return (-1);
			*hdrlen = sizeof(*uh);
			if (uh->uh_dport == 0 ||
			    ntohs(uh->uh_ulen) > m->m_pkthdr.len - *off ||
			    ntohs(uh->uh_ulen) < sizeof(struct udphdr)) {
				*action = PF_DROP;
				REASON_SET(reason, PFRES_SHORT);
				return (-1);
			}
			pd->sport = &uh->uh_sport;
			pd->dport = &uh->uh_dport;
			break;
		}
		case IPPROTO_ICMP: {
			if (!pf_pull_hdr(m, *off, pd->hdr.icmp, ICMP_MINLEN,
			    action, reason, AF_INET))
				return (-1);
			*hdrlen = ICMP_MINLEN;
			break;
		}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h;
		int		 terminal = 0;

		h = mtod(m, struct ip6_hdr *);
		pd->src = (struct pf_addr *)&h->ip6_src;
		pd->dst = (struct pf_addr *)&h->ip6_dst;
		pd->sport = pd->dport = NULL;
		pd->ip_sum = NULL;
		pd->proto_sum = NULL;
		pd->dir = dir;
		pd->sidx = (dir == PF_IN) ? 0 : 1;
		pd->didx = (dir == PF_IN) ? 1 : 0;
		pd->af = AF_INET6;
		pd->tos = 0;
		pd->tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
		*off = ((caddr_t)h - m->m_data) + sizeof(struct ip6_hdr);
		pd->proto = h->ip6_nxt;
		do {
			switch (pd->proto) {
			case IPPROTO_FRAGMENT:
				if (kif == NULL || r == NULL)	/* pflog */
					*action = PF_DROP;
				else
					*action = pf_test_fragment(r, dir, kif,
					     m, pd, a, ruleset);
				if (*action == PF_DROP)
					REASON_SET(reason, PFRES_FRAG);
				return (-1);
			case IPPROTO_ROUTING: {
				struct ip6_rthdr rthdr;

				if (pd->rh_cnt++) {
					DPFPRINTF(LOG_NOTICE,
					    "IPv6 more than one rthdr");
					*action = PF_DROP;
					REASON_SET(reason, PFRES_IPOPTIONS);
					return (-1);
				}
				if (!pf_pull_hdr(m, *off, &rthdr, sizeof(rthdr),
				    NULL, reason, pd->af)) {
					DPFPRINTF(LOG_NOTICE,
					    "IPv6 short rthdr");
					*action = PF_DROP;
					REASON_SET(reason, PFRES_SHORT);
					return (-1);
				}
				if (rthdr.ip6r_type == IPV6_RTHDR_TYPE_0) {
					DPFPRINTF(LOG_NOTICE,
					    "IPv6 rthdr0");
					*action = PF_DROP;
					REASON_SET(reason, PFRES_IPOPTIONS);
					return (-1);
				}
				/* FALLTHROUGH */
			}
			case IPPROTO_AH:
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS: {
				/* get next header and header length */
				struct ip6_ext	opt6;

				if (!pf_pull_hdr(m, *off, &opt6, sizeof(opt6),
				    NULL, reason, pd->af)) {
					DPFPRINTF(LOG_NOTICE,
					    "IPv6 short opt");
					*action = PF_DROP;
					return (-1);
				}
				if (pd->proto == IPPROTO_AH)
					*off += (opt6.ip6e_len + 2) * 4;
				else
					*off += (opt6.ip6e_len + 1) * 8;
				pd->proto = opt6.ip6e_nxt;
				/* goto the next header */
				break;
			}
			default:
				terminal++;
				break;
			}
		} while (!terminal);

		switch (pd->proto) {
		case IPPROTO_TCP: {
			struct tcphdr	*th = pd->hdr.tcp;

			if (!pf_pull_hdr(m, *off, th, sizeof(*th),
			    action, reason, AF_INET6))
				return (-1);
			*hdrlen = sizeof(*th);
			pd->p_len = pd->tot_len - *off - (th->th_off << 2);
			pd->sport = &th->th_sport;
			pd->dport = &th->th_dport;
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr	*uh = pd->hdr.udp;

			if (!pf_pull_hdr(m, *off, uh, sizeof(*uh),
			    action, reason, AF_INET6))
				return (-1);
			*hdrlen = sizeof(*uh);
			if (uh->uh_dport == 0 ||
			    ntohs(uh->uh_ulen) > m->m_pkthdr.len - *off ||
			    ntohs(uh->uh_ulen) < sizeof(struct udphdr)) {
				*action = PF_DROP;
				REASON_SET(reason, PFRES_SHORT);
				return (-1);
			}
			pd->sport = &uh->uh_sport;
			pd->dport = &uh->uh_dport;
			break;
		}
		case IPPROTO_ICMPV6: {
			size_t	icmp_hlen = sizeof(struct icmp6_hdr);

			if (!pf_pull_hdr(m, *off, pd->hdr.icmp6, icmp_hlen,
			    action, reason, AF_INET6))
				return (-1);
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
			    !pf_pull_hdr(m, *off, pd->hdr.icmp6, icmp_hlen,
			    action, reason, AF_INET6))
				return (-1);
			*hdrlen = icmp_hlen;
			break;
		}
		}
		break;
	}
#endif
	default:
		panic("pf_setup_pdesc called with illegal af %u", af);

	}
	return (0);
}

void
pf_counters_inc(int dir, int action, struct pf_pdesc *pd,
    struct pfi_kif *kif, struct pf_state *s,
    struct pf_rule *r, struct pf_rule *a)
{ 
	int dirndx;
	kif->pfik_bytes[pd->af == AF_INET6][dir == PF_OUT][action != PF_PASS]
	    += pd->tot_len;
	kif->pfik_packets[pd->af == AF_INET6][dir == PF_OUT][action != PF_PASS]++;

	if (action == PF_PASS || r->action == PF_DROP) {
		dirndx = (dir == PF_OUT);
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
			dirndx = (dir == s->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd->tot_len;

			SLIST_FOREACH(ri, &s->match_rules, entry) {
				ri->r->packets[dirndx]++;
				ri->r->bytes[dirndx] += pd->tot_len;
			}
		}
		if (r->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(r->src.addr.p.tbl,
			    (s == NULL) ? pd->src :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_OUT)],
			    pd->af, pd->tot_len, dir == PF_OUT,
			    r->action == PF_PASS, r->src.neg);
		if (r->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(r->dst.addr.p.tbl,
			    (s == NULL) ? pd->dst :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_IN)],
			    pd->af, pd->tot_len, dir == PF_OUT,
			    r->action == PF_PASS, r->dst.neg);
	}
}

#ifdef INET
int
pf_test(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh)
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0;
	struct mbuf		*m = *m0;
	struct ip		*h;
	struct pf_rule		*a = NULL, *r = &pf_default_rule;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	union pf_headers	 hdrs;
	int			 off, hdrlen;
	u_int32_t		 qid, pqid = 0;

	if (!pf_status.running)
		return (PF_PASS);

	memset(&pd, 0, sizeof(pd));
	pd.hdr.any = &hdrs;
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
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif /* DIAGNOSTIC */

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		pd.pflog |= PF_LOG_FORCE;
		goto done;
	}

	if (m->m_pkthdr.pf.flags & PF_TAG_GENERATED)
		return (PF_PASS);

	if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED_PACKET)
		return (PF_PASS);

	/* packet reassembly here if 1) enabled 2) we deal with a fragment */
	h = mtod(m, struct ip *);
	if (pf_status.reass && (h->ip_off & htons(IP_MF | IP_OFFMASK)) &&
	    pf_normalize_ip(m0, dir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;	/* pf_normalize messes with m0 */
	if (m == NULL)
		return (PF_PASS);
	h = mtod(m, struct ip *);

	if (pf_setup_pdesc(AF_INET, dir, &pd, m, &action, &reason, kif, &a, &r,
	    &ruleset, &off, &hdrlen) == -1) {
		if (action != PF_PASS)
			pd.pflog |= PF_LOG_FORCE;
		goto done;
	}
	pd.eh = eh;

	/* handle fragments that didn't get reassembled by normalization */
	if (h->ip_off & htons(IP_MF | IP_OFFMASK)) {
		action = pf_test_fragment(&r, dir, kif, m,
		    &pd, &a, &ruleset);
		goto done;
	}

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		if ((pd.hdr.tcp->th_flags & TH_ACK) && pd.p_len == 0)
			pqid = 1;
		action = pf_normalize_tcp(dir, kif, m, 0, off, h, &pd);
		if (action == PF_DROP)
			goto done;
		action = pf_test_state_tcp(&s, dir, kif, m, off, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ipintrq, hdrlen);

		if (s) {
			if (s->max_mss)
				pf_normalize_mss(m, off, &pd, s->max_mss);
		} else if (r->max_mss)
			pf_normalize_mss(m, off, &pd, r->max_mss);

		break;
	}

	case IPPROTO_UDP: {
		action = pf_test_state_udp(&s, dir, kif, m, off, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ipintrq, hdrlen);
		break;
	}

	case IPPROTO_ICMP: {
		action = pf_test_state_icmp(&s, dir, kif, m, off, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ipintrq, hdrlen);
		break;
	}

	case IPPROTO_ICMPV6: {
		action = PF_DROP;
		DPFPRINTF(LOG_NOTICE,
		    "dropping IPv4 packet with ICMPv6 payload");
		goto done;
	}

	default:
		action = pf_test_state_other(&s, dir, kif, m, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif, m, off,
			    &pd, &a, &ruleset, &ipintrq, hdrlen);
		break;
	}

done:
	if (action != PF_DROP) {
		if (s) {
			/* The non-state case is handled in pf_test_rule() */
			if (action == PF_PASS && h->ip_hl > 5 &&
			    !(s->state_flags & PFSTATE_ALLOWOPTS)) {
				action = PF_DROP;
				REASON_SET(&reason, PFRES_IPOPTIONS);
				pd.pflog |= PF_LOG_FORCE;
				DPFPRINTF(LOG_NOTICE, "dropping packet with "
				    "ip options in pf_test()");
			}

			pf_scrub_ip(&m, s->state_flags, s->min_ttl, s->set_tos);
			pf_tag_packet(m, s->tag, s->rtableid[pd.didx]);
			if (pqid || (pd.tos & IPTOS_LOWDELAY))
				qid = s->pqid;
			else
				qid = s->qid;
		} else {
			pf_scrub_ip(&m, r->scrub_flags, r->min_ttl, r->set_tos);
			if (pqid || (pd.tos & IPTOS_LOWDELAY))
				qid = r->pqid;
			else
				qid = r->qid;
		}
	}

	if (dir == PF_IN && s && s->key[PF_SK_STACK])
		m->m_pkthdr.pf.statekey = s->key[PF_SK_STACK];

#ifdef ALTQ
	if (action == PF_PASS && qid) {
		m->m_pkthdr.pf.qid = qid;
		m->m_pkthdr.pf.hdr = h;	/* hints for ecn */
	}
#endif /* ALTQ */

	/*
	 * connections redirected to loopback should not match sockets
	 * bound specifically to loopback due to security implications,
	 * see tcp_input() and in_pcblookup_listen().
	 */
	if (pd.destchg &&
	    (ntohl(pd.dst->v4.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
	/* We need to redo the route lookup on outgoing routes. */
	if (pd.destchg && dir == PF_OUT)
		m->m_pkthdr.pf.flags |= PF_TAG_REROUTE;

	if (dir == PF_IN && action == PF_PASS && r->divert.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m))) {
			m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->port = r->divert.port;
			divert->addr.ipv4 = r->divert.addr.v4;
		}
	}

	if (action == PF_PASS && r->divert_packet.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m)))
			divert->port = r->divert_packet.port;

		action = PF_DIVERT;
	}

	if (pd.pflog) {
		struct pf_rule_item	*ri;

		if (pd.pflog & PF_LOG_FORCE || r->log & PF_LOG_ALL)
			PFLOG_PACKET(kif, h, m, AF_INET, dir, reason, r, a,
			    ruleset, &pd);
		if (s) {
			SLIST_FOREACH(ri, &s->match_rules, entry)
				if (ri->r->log & PF_LOG_ALL)
					PFLOG_PACKET(kif, h, m, AF_INET, dir,
					    reason, ri->r, a, ruleset, &pd);
		}
	}

	pf_counters_inc(dir, action, &pd, kif, s, r, a);

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(*m0);
	case PF_DEFER:
		*m0 = NULL;
		action = PF_PASS;
		break;
	case PF_DIVERT:
		divert_packet(m, dir);
		*m0 = NULL;
		action = PF_PASS;
		break;
	default:
		/* pf_route can free the mbuf causing *m0 to become NULL */
		if (r->rt)
			pf_route(m0, r, dir, kif->pfik_ifp, s);
		break;
	}

	return (action);
}
#endif /* INET */

#ifdef INET6
int
pf_test6(int fwdir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh)
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0;
	struct mbuf		*m = *m0;
	struct m_tag		*mtag;
	struct ip6_hdr		*h;
	struct pf_rule		*a = NULL, *r = &pf_default_rule;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	union pf_headers	 hdrs;
	int			 off, hdrlen;
	int			 dir = (fwdir == PF_FWD) ? PF_OUT : fwdir;

	if (!pf_status.running)
		return (PF_PASS);

	memset(&pd, 0, sizeof(pd));
	pd.hdr.any = &hdrs;
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(LOG_ERR,
		    "pf_test6: kif == NULL, if_xname %s", ifp->if_xname);
		return (PF_DROP);
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP)
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test6");
#endif /* DIAGNOSTIC */

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		pd.pflog |= PF_LOG_FORCE;
		goto done;
	}

	if (m->m_pkthdr.pf.flags & PF_TAG_GENERATED)
		return (PF_PASS);

	if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED_PACKET)
		return (PF_PASS);

	if (m->m_pkthdr.pf.flags & PF_TAG_REFRAGMENTED) {
		m->m_pkthdr.pf.flags &= ~PF_TAG_REFRAGMENTED;
		return (PF_PASS);
	}

	/* packet reassembly */
	if (pf_status.reass &&
	    pf_normalize_ip6(m0, fwdir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;	/* pf_normalize messes with m0 */
	if (m == NULL)
		return (PF_PASS);
	h = mtod(m, struct ip6_hdr *);

#if 1
	/*
	 * we do not support jumbogram yet.  if we keep going, zero ip6_plen
	 * will do something bad, so drop the packet for now.
	 */
	if (htons(h->ip6_plen) == 0) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_NORM);
		pd.pflog |= PF_LOG_FORCE;
		goto done;
	}
#endif

	if (pf_setup_pdesc(AF_INET6, dir, &pd, m, &action, &reason, kif, &a, &r,
	    &ruleset, &off, &hdrlen) == -1) {
		if (action != PF_PASS)
			pd.pflog |= PF_LOG_FORCE;
		goto done;
	}
	pd.eh = eh;

	switch (pd.proto) {

	case IPPROTO_TCP: {
		action = pf_normalize_tcp(dir, kif, m, 0, off, h, &pd);
		if (action == PF_DROP)
			goto done;
		action = pf_test_state_tcp(&s, dir, kif, m, off, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ip6intrq, hdrlen);

		if (s) {
			if (s->max_mss)
				pf_normalize_mss(m, off, &pd, s->max_mss);
		} else if (r->max_mss)
			pf_normalize_mss(m, off, &pd, r->max_mss);

		break;
	}

	case IPPROTO_UDP: {
		action = pf_test_state_udp(&s, dir, kif, m, off, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ip6intrq, hdrlen);
		break;
	}

	case IPPROTO_ICMP: {
		action = PF_DROP;
		DPFPRINTF(LOG_NOTICE,
		    "dropping IPv6 packet with ICMPv4 payload");
		goto done;
	}

	case IPPROTO_ICMPV6: {
		action = pf_test_state_icmp(&s, dir, kif,
		    m, off, &pd, &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, &pd, &a, &ruleset, &ip6intrq, hdrlen);
		break;
	}

	default:
		action = pf_test_state_other(&s, dir, kif, m, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			pd.pflog |= s->log;
		} else if (s == NULL)
			action = pf_test_rule(&r, &s, dir, kif, m, off,
			    &pd, &a, &ruleset, &ip6intrq, hdrlen);
		break;
	}

done:
	/* handle dangerous IPv6 extension headers. */
	if (action == PF_PASS && pd.rh_cnt &&
	    !((s && s->state_flags & PFSTATE_ALLOWOPTS) || r->allow_opts)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_IPOPTIONS);
		pd.pflog |= PF_LOG_FORCE;
		DPFPRINTF(LOG_NOTICE,
		    "dropping packet with dangerous v6 headers");
	}

	if (action != PF_DROP) {
		if (s)
			pf_scrub_ip6(&m, s->min_ttl);
		else
			pf_scrub_ip6(&m, r->min_ttl);
	}
	if (s && s->tag)
		pf_tag_packet(m, s ? s->tag : 0, s->rtableid[pd.didx]);

	if (dir == PF_IN && s && s->key[PF_SK_STACK])
		m->m_pkthdr.pf.statekey = s->key[PF_SK_STACK];

#ifdef ALTQ
	if (action == PF_PASS && s && s->qid) {
		if (pd.tos & IPTOS_LOWDELAY)
			m->m_pkthdr.pf.qid = s->pqid;
		else
			m->m_pkthdr.pf.qid = s->qid;
		/* add hints for ecn */
		m->m_pkthdr.pf.hdr = h;
	}
#endif /* ALTQ */

	if (pd.destchg &&
	    IN6_IS_ADDR_LOOPBACK(&pd.dst->v6))
		m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
	/* We need to redo the route lookup on outgoing routes. */
	if (pd.destchg && dir == PF_OUT)
		m->m_pkthdr.pf.flags |= PF_TAG_REROUTE;

	if (dir == PF_IN && action == PF_PASS && r->divert.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m))) {
			m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->port = r->divert.port;
			divert->addr.ipv6 = r->divert.addr.v6;
		}
	}

	if (action == PF_PASS && r->divert_packet.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m)))
			divert->port = r->divert_packet.port;

		action = PF_DIVERT;
	}

	if (pd.pflog) {
		struct pf_rule_item	*ri;

		if (pd.pflog & PF_LOG_FORCE || r->log & PF_LOG_ALL)
			PFLOG_PACKET(kif, h, m, AF_INET6, dir, reason, r, a,
			    ruleset, &pd);
		if (s) {
			SLIST_FOREACH(ri, &s->match_rules, entry)
				if (ri->r->log & PF_LOG_ALL)
					PFLOG_PACKET(kif, h, m, AF_INET6, dir,
					    reason, ri->r, a, ruleset, &pd);
		}
	}

	pf_counters_inc(dir, action, &pd, kif, s, r, a);

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(*m0);
	case PF_DEFER:
		*m0 = NULL;
		action = PF_PASS;
		break;
	case PF_DIVERT:
		divert6_packet(m, dir);
		*m0 = NULL;
		action = PF_PASS;
		break;
	default:
		/* pf_route6 can free the mbuf causing *m0 to become NULL */
		if (r->rt)
			pf_route6(m0, r, dir, kif->pfik_ifp, s);
		break;
	}

	/* if reassembled packet passed, create new fragments */
	if (pf_status.reass && action == PF_PASS && *m0 && fwdir == PF_FWD &&
	    (mtag = m_tag_find(m, PACKET_TAG_PF_REASSEMBLED, NULL)) != NULL)
		action = pf_refragment6(m0, mtag, fwdir);

	return (action);
}
#endif /* INET6 */

int
pf_check_congestion(struct ifqueue *ifq)
{
	if (ifq->ifq_congestion)
		return (1);
	else
		return (0);
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
