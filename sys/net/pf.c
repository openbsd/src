/*	$OpenBSD: pf.c,v 1.253 2002/10/07 14:53:00 dhartmei Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>

#include <dev/rndvar.h>
#include <net/pfvar.h>

#include "bpfilter.h"
#include "pflog.h"

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#endif /* INET6 */


#define DPFPRINTF(n, x)	if (pf_status.debug >= (n)) printf x
struct pf_state_tree;

/*
 * Global variables
 */

struct pf_rulequeue	 pf_rules[2];
struct pf_rulequeue	*pf_rules_active;
struct pf_rulequeue	*pf_rules_inactive;
struct pf_natqueue	*pf_nats_active;
struct pf_natqueue	*pf_nats_inactive;
struct pf_binatqueue	*pf_binats_active;
struct pf_binatqueue	*pf_binats_inactive;
struct pf_rdrqueue	*pf_rdrs_active;
struct pf_rdrqueue	*pf_rdrs_inactive;
struct pf_status	 pf_status;
struct ifnet		*status_ifp;

u_int32_t		 ticket_rules_active;
u_int32_t		 ticket_rules_inactive;
u_int32_t		 ticket_nats_active;
u_int32_t		 ticket_nats_inactive;
u_int32_t		 ticket_binats_active;
u_int32_t		 ticket_binats_inactive;
u_int32_t		 ticket_rdrs_active;
u_int32_t		 ticket_rdrs_inactive;

/* Timeouts */
int			 pftm_tcp_first_packet = 120;	/* First TCP packet */
int			 pftm_tcp_opening = 30;		/* No response yet */
int			 pftm_tcp_established = 24*60*60;  /* established */
int			 pftm_tcp_closing = 15 * 60;	/* Half closed */
int			 pftm_tcp_fin_wait = 45;	/* Got both FINs */
int			 pftm_tcp_closed = 90;		/* Got a RST */

int			 pftm_udp_first_packet = 60;	/* First UDP packet */
int			 pftm_udp_single = 30;		/* Unidirectional */
int			 pftm_udp_multiple = 60;	/* Bidirectional */

int			 pftm_icmp_first_packet = 20;	/* First ICMP packet */
int			 pftm_icmp_error_reply = 10;	/* Got error response */

int			 pftm_other_first_packet = 60;	/* First packet */
int			 pftm_other_single = 30;	/* Unidirectional */
int			 pftm_other_multiple = 60;	/* Bidirectional */

int			 pftm_frag = 30;		/* Fragment expire */

int			 pftm_interval = 10;		/* expire interval */
struct timeout		 pf_expire_to;			/* expire timeout */

int			*pftm_timeouts[PFTM_MAX] = { &pftm_tcp_first_packet,
				&pftm_tcp_opening, &pftm_tcp_established,
				&pftm_tcp_closing, &pftm_tcp_fin_wait,
				&pftm_tcp_closed, &pftm_udp_first_packet,
				&pftm_udp_single, &pftm_udp_multiple,
				&pftm_icmp_first_packet, &pftm_icmp_error_reply,
				&pftm_other_first_packet, &pftm_other_single,
				&pftm_other_multiple, &pftm_frag, &pftm_interval };


struct pool		 pf_tree_pl, pf_rule_pl, pf_nat_pl, pf_sport_pl;
struct pool		 pf_rdr_pl, pf_state_pl, pf_binat_pl, pf_addr_pl;

void			 pf_addrcpy(struct pf_addr *, struct pf_addr *,
			    u_int8_t);
int			 pf_compare_rules(struct pf_rule *,
			    struct pf_rule *);
int			 pf_compare_nats(struct pf_nat *, struct pf_nat *);
int			 pf_compare_binats(struct pf_binat *,
			    struct pf_binat *);
int			 pf_compare_rdrs(struct pf_rdr *, struct pf_rdr *);
int		 	 pf_insert_state(struct pf_state *);
struct pf_state 	*pf_find_state(struct pf_state_tree *,
			    struct pf_tree_node *);
void			 pf_purge_expired_states(void);
void			 pf_purge_timeout(void *);
void			 pf_dynaddr_update(void *);

void			 pf_print_host(struct pf_addr *, u_int16_t, u_int8_t);
void			 pf_print_state(struct pf_state *);
void			 pf_print_flags(u_int8_t);

u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t,
			    u_int8_t);
void			 pf_change_ap(struct pf_addr *, u_int16_t *,
			    u_int16_t *, u_int16_t *, struct pf_addr *,
			    u_int16_t, u_int8_t, int);
void			 pf_change_a(u_int32_t *, u_int16_t *, u_int32_t,
			    u_int8_t);
#ifdef INET6
void			 pf_change_a6(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, u_int8_t);
#endif /* INET6 */
void			 pf_change_icmp(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, struct pf_addr *, u_int16_t,
			    u_int16_t *, u_int16_t *, u_int16_t *,
			    u_int16_t *, u_int8_t, int);
void			 pf_send_reset(int, struct tcphdr *,
			    struct pf_pdesc *, int, u_int8_t);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t, int);
u_int16_t		 pf_map_port_range(struct pf_rdr *, u_int16_t);
struct pf_nat		*pf_get_nat(struct ifnet *, u_int8_t,
			    struct pf_addr *, u_int16_t,
			    struct pf_addr *, u_int16_t, int);
struct pf_binat		*pf_get_binat(int, struct ifnet *, u_int8_t,
			    struct pf_addr *, struct pf_addr *, int);
struct pf_rdr		*pf_get_rdr(struct ifnet *, u_int8_t,
			    struct pf_addr *, struct pf_addr *, u_int16_t, int);
int			 pf_test_tcp(struct pf_rule **, int, struct ifnet *,
			    struct mbuf *, int, int, void *, struct pf_pdesc *);
int			 pf_test_udp(struct pf_rule **, int, struct ifnet *,
			    struct mbuf *, int, int, void *, struct pf_pdesc *);
int			 pf_test_icmp(struct pf_rule **, int, struct ifnet *,
			    struct mbuf *, int, int, void *, struct pf_pdesc *);
int			 pf_test_other(struct pf_rule **, int, struct ifnet *,
			    struct mbuf *, void *, struct pf_pdesc *);
int			 pf_test_fragment(struct pf_rule **, int, struct ifnet *,
			    struct mbuf *, void *, struct pf_pdesc *);
int			 pf_test_state_tcp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_udp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_icmp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_other(struct pf_state **, int,
			    struct ifnet *, struct pf_pdesc *);
void			*pf_pull_hdr(struct mbuf *, int, void *, int,
			    u_short *, u_short *, int);
void			 pf_calc_skip_steps(struct pf_rulequeue *);
int			 pf_get_sport(u_int8_t, u_int8_t,
			    struct pf_addr *, struct pf_addr *,
			    u_int16_t, u_int16_t *, u_int16_t, u_int16_t);
int			 pf_normalize_tcp(int, struct ifnet *, struct mbuf *,
			    int, int, void *, struct pf_pdesc *);
void			 pf_route(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *);
void			 pf_route6(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *);
int			 pf_socket_lookup(uid_t *, gid_t *, int, int, int,
			     struct pf_pdesc *);
struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX] = { { &pf_state_pl, UINT_MAX },
	{ &pf_frent_pl, PFFRAG_FRENT_HIWAT } };



#if NPFLOG > 0
#define	PFLOG_PACKET(i,x,a,b,c,d,e) \
	do { \
		if (b == AF_INET) { \
			HTONS(((struct ip *)x)->ip_len); \
			HTONS(((struct ip *)x)->ip_off); \
			pflog_packet(i,a,b,c,d,e); \
			NTOHS(((struct ip *)x)->ip_len); \
			NTOHS(((struct ip *)x)->ip_off); \
		} else { \
			pflog_packet(i,a,b,c,d,e); \
		} \
	} while (0)
#else
#define	PFLOG_PACKET(i,x,a,b,c,d,e)	((void)0)
#endif

#define	STATE_TRANSLATE(s) \
	(s)->lan.addr.addr32[0] != (s)->gwy.addr.addr32[0] || \
	((s)->af == AF_INET6 && \
	((s)->lan.addr.addr32[1] != (s)->gwy.addr.addr32[1] || \
	(s)->lan.addr.addr32[2] != (s)->gwy.addr.addr32[2] || \
	(s)->lan.addr.addr32[3] != (s)->gwy.addr.addr32[3])) || \
	(s)->lan.port != (s)->gwy.port

#define TIMEOUT(r,i) \
	(((r) && (r)->timeout[(i)]) ? (r)->timeout[(i)] : *pftm_timeouts[(i)])

static __inline int pf_state_compare(struct pf_tree_node *,
			struct pf_tree_node *);

struct pf_state_tree tree_lan_ext, tree_ext_gwy;
RB_GENERATE(pf_state_tree, pf_tree_node, entry, pf_state_compare);

struct pf_rulequeue		 pf_rules[2];
struct pf_natqueue		 pf_nats[2];
struct pf_binatqueue		 pf_binats[2];
struct pf_rdrqueue		 pf_rdrs[2];

static __inline int
pf_state_compare(struct pf_tree_node *a, struct pf_tree_node *b)
{
	int diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[0].addr32[1] > b->addr[0].addr32[1])
			return (1);
		if (a->addr[0].addr32[1] < b->addr[0].addr32[1])
			return (-1);
		if (a->addr[0].addr32[2] > b->addr[0].addr32[2])
			return (1);
		if (a->addr[0].addr32[2] < b->addr[0].addr32[2])
			return (-1);
		if (a->addr[0].addr32[3] > b->addr[0].addr32[3])
			return (1);
		if (a->addr[0].addr32[3] < b->addr[0].addr32[3])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		if (a->addr[1].addr32[1] > b->addr[1].addr32[1])
			return (1);
		if (a->addr[1].addr32[1] < b->addr[1].addr32[1])
			return (-1);
		if (a->addr[1].addr32[2] > b->addr[1].addr32[2])
			return (1);
		if (a->addr[1].addr32[2] < b->addr[1].addr32[2])
			return (-1);
		if (a->addr[1].addr32[3] > b->addr[1].addr32[3])
			return (1);
		if (a->addr[1].addr32[3] < b->addr[1].addr32[3])
			return (-1);
		break;
#endif /* INET6 */
	}

	if ((diff = a->port[0] - b->port[0]) != 0)
		return (diff);
	if ((diff = a->port[1] - b->port[1]) != 0)
		return (diff);

	return (0);
}

#ifdef INET6
void
pf_addrcpy(struct pf_addr *dst, struct pf_addr *src, u_int8_t af)
{
	switch(af) {
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
#endif

int
pf_compare_rules(struct pf_rule *a, struct pf_rule *b)
{
	if (a->return_icmp != b->return_icmp ||
	    a->return_icmp6 != b->return_icmp6 ||
	    a->action != b->action ||
	    a->direction != b->direction ||
	    a->log != b->log ||
	    a->quick != b->quick ||
	    a->keep_state != b->keep_state ||
	    a->af != b->af ||
	    a->proto != b->proto ||
	    a->type != b->type ||
	    a->code != b->code ||
	    a->flags != b->flags ||
	    a->flagset != b->flagset ||
	    a->rule_flag != b->rule_flag ||
	    a->min_ttl != b->min_ttl ||
	    a->tos != b->tos ||
	    a->allow_opts != b->allow_opts)
		return (1);
	if (PF_ANEQ(&a->src.addr.addr, &b->src.addr.addr, a->af) ||
	    PF_ANEQ(&a->src.mask, &b->src.mask, a->af) ||
	    a->src.port[0] != b->src.port[0] ||
	    a->src.port[1] != b->src.port[1] ||
	    a->src.not != b->src.not ||
	    a->src.port_op != b->src.port_op)
		return (1);
	if (PF_ANEQ(&a->dst.addr.addr, &b->dst.addr.addr, a->af) ||
	    PF_ANEQ(&a->dst.mask, &b->dst.mask, a->af) ||
	    a->dst.port[0] != b->dst.port[0] ||
	    a->dst.port[1] != b->dst.port[1] ||
	    a->dst.not != b->dst.not ||
	    a->dst.port_op != b->dst.port_op)
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	if (a->ifnot != b->ifnot)
		return (1);
	return (0);
}

int
pf_compare_nats(struct pf_nat *a, struct pf_nat *b)
{
	if (a->proto != b->proto ||
	    a->af != b->af ||
	    a->ifnot != b->ifnot ||
	    a->no != b->no)
		return (1);
	if (PF_ANEQ(&a->src.addr.addr, &b->src.addr.addr, a->af) ||
	    PF_ANEQ(&a->src.mask, &b->src.mask, a->af) ||
	    a->src.port[0] != b->src.port[0] ||
	    a->src.port[1] != b->src.port[1] ||
	    a->src.not != b->src.not ||
	    a->src.port_op != b->src.port_op)
		return (1);
	if (PF_ANEQ(&a->dst.addr.addr, &b->dst.addr.addr, a->af) ||
	    PF_ANEQ(&a->dst.mask, &b->dst.mask, a->af) ||
	    a->dst.port[0] != b->dst.port[0] ||
	    a->dst.port[1] != b->dst.port[1] ||
	    a->dst.not != b->dst.not ||
	    a->dst.port_op != b->dst.port_op)
		return (1);
	if (PF_ANEQ(&a->raddr.addr, &b->raddr.addr, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pf_compare_binats(struct pf_binat *a, struct pf_binat *b)
{
	if (a->proto != b->proto ||
	    a->dnot != b->dnot ||
	    a->af != b->af ||
	    a->no != b->no)
		return (1);
	if (PF_ANEQ(&a->saddr.addr, &b->saddr.addr, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr.addr, &b->daddr.addr, a->af))
		return (1);
	if (PF_ANEQ(&a->dmask, &b->dmask, a->af))
		return (1);
	if (PF_ANEQ(&a->raddr.addr, &b->raddr.addr, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pf_compare_rdrs(struct pf_rdr *a, struct pf_rdr *b)
{
	if (a->dport != b->dport ||
	    a->dport2 != b->dport2 ||
	    a->rport != b->rport ||
	    a->proto != b->proto ||
	    a->af != b->af ||
	    a->snot != b->snot ||
	    a->dnot != b->dnot ||
	    a->ifnot != b->ifnot ||
	    a->opts != b->opts ||
	    a->no != b->no)
		return (1);
	if (PF_ANEQ(&a->saddr.addr, &b->saddr.addr, a->af))
		return (1);
	if (PF_ANEQ(&a->smask, &b->smask, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr.addr, &b->daddr.addr, a->af))
		return (1);
	if (PF_ANEQ(&a->dmask, &b->dmask, a->af))
		return (1);
	if (PF_ANEQ(&a->raddr.addr, &b->raddr.addr, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pflog_packet(struct ifnet *ifp, struct mbuf *m, int af, u_short dir,
    u_short reason, struct pf_rule *rm)
{
#if NBPFILTER > 0
	struct ifnet *ifn;
	struct pfloghdr hdr;
	struct mbuf m1;

	if (ifp == NULL || m == NULL || rm == NULL)
		return (-1);

	hdr.af = htonl(af);
	memcpy(hdr.ifname, ifp->if_xname, sizeof(hdr.ifname));

	hdr.rnr = htons(rm->nr);
	hdr.reason = htons(reason);
	hdr.dir = htons(dir);
	hdr.action = htons(rm->action);

#ifdef INET
	if (af == AF_INET && dir == PF_OUT) {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
	}
#endif /* INET */

	m1.m_next = m;
	m1.m_len = PFLOG_HDRLEN;
	m1.m_data = (char *) &hdr;

	ifn = &(pflogif[0].sc_if);

	if (ifn->if_bpf)
		bpf_mtap(ifn->if_bpf, &m1);
#endif

	return (0);
}

struct pf_state *
pf_find_state(struct pf_state_tree *tree, struct pf_tree_node *key)
{
	struct pf_tree_node *k;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	k = RB_FIND(pf_state_tree, tree, key);
	if (k)
		return (k->state);
	else
		return (NULL);
}

int
pf_insert_state(struct pf_state *state)
{
	struct pf_tree_node *keya, *keyb;

	keya = pool_get(&pf_tree_pl, PR_NOWAIT);
	if (keya == NULL)
		return (-1);
	keya->state = state;
	keya->proto = state->proto;
	keya->af = state->af;
	PF_ACPY(&keya->addr[0], &state->lan.addr, state->af);
	keya->port[0] = state->lan.port;
	PF_ACPY(&keya->addr[1], &state->ext.addr, state->af);
	keya->port[1] = state->ext.port;

	/* Thou MUST NOT insert multiple duplicate keys */
	if (RB_INSERT(pf_state_tree, &tree_lan_ext, keya) != NULL) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state insert failed: tree_lan_ext");
			printf(" lan: ");
			pf_print_host(&state->lan.addr, state->lan.port,
			    state->af);
			printf(" gwy: ");
			pf_print_host(&state->gwy.addr, state->gwy.port,
			    state->af);
			printf(" ext: ");
			pf_print_host(&state->ext.addr, state->ext.port,
			    state->af);
			printf("\n");
		}
		pool_put(&pf_tree_pl, keya);
		return (-1);
	}

	keyb = pool_get(&pf_tree_pl, PR_NOWAIT);
	if (keyb == NULL) {
		/* Need to pull out the other state */
		RB_REMOVE(pf_state_tree, &tree_lan_ext, keya);
		pool_put(&pf_tree_pl, keya);
		return (-1);
	}
	keyb->state = state;
	keyb->proto = state->proto;
	keyb->af = state->af;
	PF_ACPY(&keyb->addr[0], &state->ext.addr, state->af);
	keyb->port[0] = state->ext.port;
	PF_ACPY(&keyb->addr[1], &state->gwy.addr, state->af);
	keyb->port[1] = state->gwy.port;

	if (RB_INSERT(pf_state_tree, &tree_ext_gwy, keyb) != NULL) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state insert failed: tree_ext_gwy");
			printf(" lan: ");
			pf_print_host(&state->lan.addr, state->lan.port,
			    state->af);
			printf(" gwy: ");
			pf_print_host(&state->gwy.addr, state->gwy.port,
			    state->af);
			printf(" ext: ");
			pf_print_host(&state->ext.addr, state->ext.port,
			    state->af);
			printf("\n");
		}
		RB_REMOVE(pf_state_tree, &tree_lan_ext, keya);
		pool_put(&pf_tree_pl, keya);
		pool_put(&pf_tree_pl, keyb);
		return (-1);
	}

	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
	return (0);
}

void
pf_purge_timeout(void *arg)
{
	struct timeout *to = arg;
	int s;

	s = splsoftnet();
	pf_purge_expired_states();
	pf_purge_expired_fragments();
	splx(s);

	timeout_add(to, pftm_interval * hz);
}

void
pf_purge_expired_states(void)
{
	struct pf_tree_node *cur, *peer, *next;
	struct pf_tree_node key;

	for (cur = RB_MIN(pf_state_tree, &tree_ext_gwy); cur; cur = next) {
		next = RB_NEXT(pf_state_tree, &tree_ext_gwy, cur);

		if (cur->state->expire <= (unsigned)time.tv_sec) {
			RB_REMOVE(pf_state_tree, &tree_ext_gwy, cur);

			/* Need this key's peer (in the other tree) */
			key.state = cur->state;
			key.proto = cur->state->proto;
			key.af = cur->state->af;
			PF_ACPY(&key.addr[0], &cur->state->lan.addr,
			    cur->state->af);
			key.port[0] = cur->state->lan.port;
			PF_ACPY(&key.addr[1], &cur->state->ext.addr,
			    cur->state->af);
			key.port[1] = cur->state->ext.port;

			peer = RB_FIND(pf_state_tree, &tree_lan_ext, &key);
			KASSERT(peer);
			KASSERT(peer->state == cur->state);
			RB_REMOVE(pf_state_tree, &tree_lan_ext, peer);

			if (cur->state->rule.ptr != NULL)
				cur->state->rule.ptr->states--;
			pool_put(&pf_state_pl, cur->state);
			pool_put(&pf_tree_pl, cur);
			pool_put(&pf_tree_pl, peer);
			pf_status.fcounters[FCNT_STATE_REMOVALS]++;
			pf_status.states--;
		}
	}
}

int
pf_dynaddr_setup(struct pf_addr_wrap *aw, u_int8_t af)
{
	if (aw->addr_dyn == NULL)
		return (0);
	aw->addr_dyn = pool_get(&pf_addr_pl, PR_NOWAIT);
	if (aw->addr_dyn == NULL)
		return (1);
	bcopy(aw->addr.pfa.ifname, aw->addr_dyn->ifname,
	    sizeof(aw->addr_dyn->ifname));
	aw->addr_dyn->ifp = ifunit(aw->addr_dyn->ifname);
	if (aw->addr_dyn->ifp == NULL) {
		pool_put(&pf_addr_pl, aw->addr_dyn);
		aw->addr_dyn = NULL;
		return (1);
	}
	aw->addr_dyn->addr = &aw->addr;
	aw->addr_dyn->af = af;
	aw->addr_dyn->undefined = 1;
	aw->addr_dyn->hook_cookie = hook_establish(
	    aw->addr_dyn->ifp->if_addrhooks, 1,
	    pf_dynaddr_update, aw->addr_dyn);
	if (aw->addr_dyn->hook_cookie == NULL) {
		pool_put(&pf_addr_pl, aw->addr_dyn);
		aw->addr_dyn = NULL;
		return (1);
	}
	pf_dynaddr_update(aw->addr_dyn);
	return (0);
}

void
pf_dynaddr_update(void *p)
{
	struct pf_addr_dyn *ad = (struct pf_addr_dyn *)p;
	struct ifaddr *ia;
	int s, changed = 0;

	if (ad == NULL || ad->ifp == NULL)
		panic("pf_dynaddr_update");
	s = splsoftnet();
	TAILQ_FOREACH(ia, &ad->ifp->if_addrlist, ifa_list)
		if (ia->ifa_addr != NULL &&
		    ia->ifa_addr->sa_family == ad->af) {
			if (ad->af == AF_INET) {
				struct in_addr *a, *b;

				a = &ad->addr->v4;
				b = &((struct sockaddr_in *)ia->ifa_addr)
				    ->sin_addr;
				if (ad->undefined ||
				    memcmp(a, b, sizeof(*a))) {
					bcopy(b, a, sizeof(*a));
					changed = 1;
				}
			} else if (ad->af == AF_INET6) {
				struct in6_addr *a, *b;

				a = &ad->addr->v6;
				b = &((struct sockaddr_in6 *)ia->ifa_addr)
				    ->sin6_addr;
				if (ad->undefined ||
				    memcmp(a, b, sizeof(*a))) {
					bcopy(b, a, sizeof(*a));
					changed = 1;
				}
			}
			if (changed)
				ad->undefined = 0;
			break;
		}
	if (ia == NULL)
		ad->undefined = 1;
	splx(s);
}

void
pf_dynaddr_remove(struct pf_addr_wrap *aw)
{
	if (aw->addr_dyn == NULL)
		return;
	hook_disestablish(aw->addr_dyn->ifp->if_addrhooks,
	    aw->addr_dyn->hook_cookie);
	pool_put(&pf_addr_pl, aw->addr_dyn);
	aw->addr_dyn = NULL;
}

void
pf_dynaddr_copyout(struct pf_addr_wrap *aw)
{
	if (aw->addr_dyn == NULL)
		return;
	bcopy(aw->addr_dyn->ifname, aw->addr.pfa.ifname,
	    sizeof(aw->addr.pfa.ifname));
	aw->addr_dyn = (struct pf_addr_dyn *)1;
}

void
pf_print_host(struct pf_addr *addr, u_int16_t p, u_int8_t af)
{
	switch(af) {
#ifdef INET
	case AF_INET: {
		u_int32_t a = ntohl(addr->addr32[0]);
		p = ntohs(p);
		printf("%u.%u.%u.%u:%u", (a>>24)&255, (a>>16)&255,
		    (a>>8)&255, a&255, p);
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		u_int16_t b;
		u_int8_t i, curstart = 255, curend = 0,
		    maxstart = 0, maxend = 0;
		for (i = 0; i < 8; i++) {
			if (!addr->addr16[i]) {
				if (curstart == 255)
					curstart = i;
				else
					curend = i;
			} else {
				if (curstart) {
					if ((curend - curstart) >
					    (maxend - maxstart)) {
						maxstart = curstart;
						maxend = curend;
						curstart = 255;
					}
				}
			}
		}
		for (i = 0; i < 8; i++) {
			if (i >= maxstart && i <= maxend) {
				if (maxend != 7) {
					if (i == maxstart)
						printf(":");
				} else {
					if (i == maxend)
						printf(":");
				}
			} else {
				b = ntohs(addr->addr16[i]);
				printf("%x", b);
				if (i < 7)
					printf(":");
			}
		}
		p = ntohs(p);
		printf("[%u]", p);
		break;
	}
#endif /* INET6 */
	}
}

void
pf_print_state(struct pf_state *s)
{
	switch (s->proto) {
	case IPPROTO_TCP:
		printf("TCP ");
		break;
	case IPPROTO_UDP:
		printf("UDP ");
		break;
	case IPPROTO_ICMP:
		printf("ICMP ");
		break;
	case IPPROTO_ICMPV6:
		printf("ICMPV6 ");
		break;
	default:
		printf("%u ", s->proto);
		break;
	}
	pf_print_host(&s->lan.addr, s->lan.port, s->af);
	printf(" ");
	pf_print_host(&s->gwy.addr, s->gwy.port, s->af);
	printf(" ");
	pf_print_host(&s->ext.addr, s->ext.port, s->af);
	printf(" [lo=%lu high=%lu win=%u modulator=%u]", s->src.seqlo,
	    s->src.seqhi, s->src.max_win, s->src.seqdiff);
	printf(" [lo=%lu high=%lu win=%u modulator=%u]", s->dst.seqlo,
	    s->dst.seqhi, s->dst.max_win, s->dst.seqdiff);
	printf(" %u:%u", s->src.state, s->dst.state);
}

void
pf_print_flags(u_int8_t f)
{
	if (f)
		printf(" ");
	if (f & TH_FIN)
		printf("F");
	if (f & TH_SYN)
		printf("S");
	if (f & TH_RST)
		printf("R");
	if (f & TH_PUSH)
		printf("P");
	if (f & TH_ACK)
		printf("A");
	if (f & TH_URG)
		printf("U");
	if (f & TH_ECE)
		printf("E");
	if (f & TH_CWR)
		printf("W");
}

#define		 PF_CALC_SKIP_STEP(i, c) \
		do { \
			if (a & 1 << i) { \
				if (c) \
					r->skip[i] = TAILQ_NEXT(s, entries); \
				else \
					a ^= 1 << i; \
			} \
		} while (0)

void
pf_calc_skip_steps(struct pf_rulequeue *rules)
{
	struct pf_rule *r, *s;
	int a, i;

	r = TAILQ_FIRST(rules);
	while (r != NULL) {
		a = 0;
		for (i = 0; i < PF_SKIP_COUNT; ++i) {
			a |= 1 << i;
			r->skip[i] = TAILQ_NEXT(r, entries);
		}
		s = TAILQ_NEXT(r, entries);
		while (a && s != NULL) {
			PF_CALC_SKIP_STEP(PF_SKIP_ACTION,
			    (s->action == PF_SCRUB && r->action == PF_SCRUB) ||
			    (s->action != PF_SCRUB && r->action != PF_SCRUB));
			PF_CALC_SKIP_STEP(PF_SKIP_IFP, 
			    s->ifp == r->ifp && s->ifnot == r->ifnot);
			PF_CALC_SKIP_STEP(PF_SKIP_DIR,
			    s->direction == r->direction);
			PF_CALC_SKIP_STEP(PF_SKIP_AF, s->af == r->af);
			PF_CALC_SKIP_STEP(PF_SKIP_PROTO, s->proto == r->proto);
			PF_CALC_SKIP_STEP(PF_SKIP_SRC_ADDR,
			    s->src.addr.addr_dyn == NULL &&
			    r->src.addr.addr_dyn == NULL &&
			    PF_AEQ(&s->src.addr.addr, &r->src.addr.addr, r->af) &&
			    PF_AEQ(&s->src.mask, &r->src.mask, r->af) &&
			    s->src.not == r->src.not);
			PF_CALC_SKIP_STEP(PF_SKIP_SRC_PORT,
			    s->src.port[0] == r->src.port[0] &&
			    s->src.port[1] == r->src.port[1] &&
			    s->src.port_op == r->src.port_op);
			PF_CALC_SKIP_STEP(PF_SKIP_DST_ADDR,
			    s->dst.addr.addr_dyn == NULL &&
			    r->dst.addr.addr_dyn == NULL &&
			    PF_AEQ(&s->dst.addr.addr, &r->dst.addr.addr, r->af) &&
			    PF_AEQ(&s->dst.mask, &r->dst.mask, r->af) &&
			    s->dst.not == r->dst.not);
			PF_CALC_SKIP_STEP(PF_SKIP_DST_PORT,
			    s->dst.port[0] == r->dst.port[0] &&
			    s->dst.port[1] == r->dst.port[1] &&
			    s->dst.port_op == r->dst.port_op);
			s = TAILQ_NEXT(s, entries);
		}
		r = TAILQ_NEXT(r, entries);
	}
}

u_int16_t
pf_cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new, u_int8_t udp)
{
	u_int32_t l;

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
pf_change_ap(struct pf_addr *a, u_int16_t *p, u_int16_t *ic, u_int16_t *pc,
    struct pf_addr *an, u_int16_t pn, u_int8_t u, int af)
{
	struct pf_addr ao;
	u_int16_t po = *p;

	PF_ACPY(&ao, a, af);
	PF_ACPY(a, an, af);

	*p = pn;

	switch (af) {
#ifdef INET
	case AF_INET:
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    ao.addr16[0], an->addr16[0], 0),
		    ao.addr16[1], an->addr16[1], 0);
		*p = pn;
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

void
pf_change_a(u_int32_t *a, u_int16_t *c, u_int32_t an, u_int8_t u)
{
	u_int32_t ao = *a;

	*a = an;
	*c = pf_cksum_fixup(pf_cksum_fixup(*c, ao / 65536, an / 65536, u),
	   ao % 65536, an % 65536, u);
}

#ifdef INET6
void
pf_change_a6(struct pf_addr *a, u_int16_t *c, struct pf_addr *an, u_int8_t u)
{
	struct pf_addr ao;

	PF_ACPY(&ao, a, AF_INET6);
	PF_ACPY(a, an, AF_INET6);

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

void
pf_change_icmp(struct pf_addr *ia, u_int16_t *ip, struct pf_addr *oa,
    struct pf_addr *na, u_int16_t np, u_int16_t *pc, u_int16_t *h2c,
    u_int16_t *ic, u_int16_t *hc, u_int8_t u, int af)
{
	struct pf_addr oia, ooa;
	u_int32_t opc, oh2c = *h2c;
	u_int16_t oip = *ip;

	PF_ACPY(&oia, ia, af);
	PF_ACPY(&ooa, oa, af);

	if (pc != NULL)
		opc = *pc;
	/* Change inner protocol port, fix inner protocol checksum. */
	*ip = np;
	if (pc != NULL)
		*pc = pf_cksum_fixup(*pc, oip, *ip, u);
	*ic = pf_cksum_fixup(*ic, oip, *ip, 0);
	if (pc != NULL)
		*ic = pf_cksum_fixup(*ic, opc, *pc, 0);
	PF_ACPY(ia, na, af);
	/* Change inner ip address, fix inner ipv4 checksum and icmp checksum. */
	switch (af) {
#ifdef INET
	case AF_INET:
		*h2c = pf_cksum_fixup(pf_cksum_fixup(*h2c,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		break;
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
	*ic = pf_cksum_fixup(*ic, oh2c, *h2c, 0);
	/* Change outer ip address, fix outer ipv4 or icmpv6 checksum. */
	PF_ACPY(oa, na, af);
	switch (af) {
#ifdef INET
	case AF_INET:
		*hc = pf_cksum_fixup(pf_cksum_fixup(*hc,
		    ooa.addr16[0], oa->addr16[0], 0),
		    ooa.addr16[1], oa->addr16[1], 0);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
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
		break;
#endif /* INET6 */
	}
}

void
pf_send_reset(int off, struct tcphdr *th, struct pf_pdesc *pd, int af,
    u_int8_t return_ttl)
{
	struct mbuf *m;
	struct m_tag *mtag;
	int len;
#ifdef INET
	struct ip *h2;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr *h2_6;
#endif /* INET6 */
	struct tcphdr *th2;

	switch (af) {
#ifdef INET
	case AF_INET:
		len = sizeof(struct ip) + sizeof(struct tcphdr);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		len = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		break;
#endif /* INET6 */
	}

	/* don't reply to RST packets */
	if (th->th_flags & TH_RST)
		return;

	/* create outgoing mbuf */
	mtag = m_tag_get(PACKET_TAG_PF_GENERATED, 0, M_NOWAIT);
	if (mtag == NULL)
		return;
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		m_tag_free(mtag);
		return;
	}
	m_tag_prepend(m, mtag);
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m->m_data, len);
	switch (af) {
#ifdef INET
	case AF_INET:
		h2 = mtod(m, struct ip *);

		/* IP header fields included in the TCP checksum */
		h2->ip_p = IPPROTO_TCP;
		h2->ip_len = htons(sizeof(*th2));
		h2->ip_src.s_addr = pd->dst->v4.s_addr;
		h2->ip_dst.s_addr = pd->src->v4.s_addr;

		th2 = (struct tcphdr *)((caddr_t)h2 + sizeof(struct ip));
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		h2_6 = mtod(m, struct ip6_hdr *);

		/* IP header fields included in the TCP checksum */
		h2_6->ip6_nxt = IPPROTO_TCP;
		h2_6->ip6_plen = htons(sizeof(*th2));
		memcpy(&h2_6->ip6_src, pd->dst, sizeof(struct in6_addr));
		memcpy(&h2_6->ip6_dst, pd->src, sizeof(struct in6_addr));

		th2 = (struct tcphdr *)((caddr_t)h2_6 + sizeof(struct ip6_hdr));
		break;
#endif /* INET6 */
	}

	/* TCP header */
	th2->th_sport = th->th_dport;
	th2->th_dport = th->th_sport;
	if (th->th_flags & TH_ACK) {
		th2->th_seq = th->th_ack;
		th2->th_flags = TH_RST;
	} else {
		int tlen = pd->p_len;
		if (th->th_flags & TH_SYN)
			tlen++;
		if (th->th_flags & TH_FIN)
			tlen++;
		th2->th_ack = htonl(ntohl(th->th_seq) + tlen);
		th2->th_flags = TH_RST | TH_ACK;
	}
	th2->th_off = sizeof(*th2) >> 2;

	switch (af) {
#ifdef INET
	case AF_INET:
		/* TCP checksum */
		th2->th_sum = in_cksum(m, len);

		/* Finish the IP header */
		h2->ip_v = 4;
		h2->ip_hl = sizeof(*h2) >> 2;
		if (!return_ttl)
			return_ttl = ip_defttl;
		h2->ip_ttl = return_ttl;
		h2->ip_sum = 0;
		h2->ip_len = len;
		h2->ip_off = ip_mtudisc ? IP_DF : 0;
		ip_output(m, (void *)NULL, (void *)NULL, 0, (void *)NULL,
		  (void *)NULL);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* TCP checksum */
		th2->th_sum = in6_cksum(m, IPPROTO_TCP,
		    sizeof(struct ip6_hdr), sizeof(*th));

		h2_6->ip6_vfc |= IPV6_VERSION;
		if (!return_ttl)
			return_ttl = IPV6_DEFHLIM;
		h2_6->ip6_hlim = return_ttl;

		ip6_output(m, NULL, NULL, 0, NULL, NULL);
#endif /* INET6 */
	}
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code, int af)
{
	struct m_tag *mtag;
	struct mbuf *m0;

	mtag = m_tag_get(PACKET_TAG_PF_GENERATED, 0, M_NOWAIT);
	if (mtag == NULL)
		return;
	m0 = m_copy(m, 0, M_COPYALL);
	if (m0 == NULL) {
		m_tag_free(mtag);
		return;
	}
	m_tag_prepend(m0, mtag);
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
    struct pf_addr *b, int af)
{
	int match = 0;
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

int
pf_match(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	switch (op) {
	case PF_OP_IRG:
		return ((p > a1) && (p < a2));
	case PF_OP_XRG:
		return ((p < a1) || (p > a2));
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
pf_get_sport(u_int8_t af, u_int8_t proto,
    struct pf_addr *daddr, struct pf_addr *raddr,
    u_int16_t dport, u_int16_t *port, u_int16_t low, u_int16_t high)
{
	struct pf_tree_node key;

	int			step;
	u_int16_t		cut;

	if (!(proto == IPPROTO_TCP || proto == IPPROTO_UDP))
		return (EINVAL);
	if (low == 0 && high == 0) {
		NTOHS(*port);
		return (0);
	}
	if (low == high) {
		*port = low;
		return (0);
	}

	key.af = af;
	key.proto = proto;
	PF_ACPY(&key.addr[0], daddr, key.af);
	PF_ACPY(&key.addr[1], raddr, key.af);
	key.port[0] = dport;

	/* port search; start random, step; similar 2 portloop in in_pcbbind */
	if (low == high) {
		key.port[1] = htons(low);
		if (pf_find_state(&tree_ext_gwy, &key) == NULL) {
			*port = low;
			return (0);
		}
		return (1);
	} else if (low < high) {
		step = 1;
		cut = arc4random() % (1 + high - low) + low;
	} else {
		step = -1;
		cut = arc4random() % (1 + low - high) + high;
	}

	*port = cut - step;
	do {
		*port += step;
		key.port[1] = htons(*port);
		if (pf_find_state(&tree_ext_gwy, &key) == NULL)
			return (0);
	} while (*port != low && *port != high);

	step = -step;
	*port = cut;
	do {
		*port += step;
		key.port[1] = htons(*port);
		if (pf_find_state(&tree_ext_gwy, &key) == NULL)
			return (0);
	} while (*port != low && *port != high);

	return (1);					/* none available */
}

struct pf_nat *
pf_get_nat(struct ifnet *ifp, u_int8_t proto, struct pf_addr *saddr,
    u_int16_t sport, struct pf_addr *daddr, u_int16_t dport, int af)
{
	struct pf_nat *n, *nm = NULL;

	n = TAILQ_FIRST(pf_nats_active);
	while (n && nm == NULL) {
		if (((n->ifp == NULL) || (n->ifp == ifp && !n->ifnot) ||
		    (n->ifp != ifp && n->ifnot)) &&
		    (!n->proto || n->proto == proto) &&
		    (!n->af || n->af == af) &&
		    (n->src.addr.addr_dyn == NULL ||
		    !n->src.addr.addr_dyn->undefined) &&
		    PF_MATCHA(n->src.not, &n->src.addr.addr, &n->src.mask,
		    saddr, af) &&
		    (!n->src.port_op ||
		    (proto != IPPROTO_TCP && proto != IPPROTO_UDP) ||
		    pf_match_port(n->src.port_op, n->src.port[0],
		    n->src.port[1], sport)) &&
		    (n->dst.addr.addr_dyn == NULL ||
		    !n->dst.addr.addr_dyn->undefined) &&
		    PF_MATCHA(n->dst.not, &n->dst.addr.addr, &n->dst.mask,
		    daddr, af) &&
		    (!n->dst.port_op ||
		    (proto != IPPROTO_TCP && proto != IPPROTO_UDP) ||
		    pf_match_port(n->dst.port_op, n->dst.port[0],
		    n->dst.port[1], dport)))
			nm = n;
		else
			n = TAILQ_NEXT(n, entries);
	}
	if (nm && (nm->no || (nm->raddr.addr_dyn != NULL &&
	    nm->raddr.addr_dyn->undefined)))
		return (NULL);
	return (nm);
}

struct pf_binat *
pf_get_binat(int direction, struct ifnet *ifp, u_int8_t proto,
    struct pf_addr *saddr, struct pf_addr *daddr, int af)
{
	struct pf_binat *b, *bm = NULL;
	struct pf_addr fullmask;

	memset(&fullmask, 0xff, sizeof(fullmask));

	b = TAILQ_FIRST(pf_binats_active);
	while (b && bm == NULL) {
		if (direction == PF_OUT && b->ifp == ifp &&
		    (!b->proto || b->proto == proto) &&
		    (!b->af || b->af == af) &&
		    (b->saddr.addr_dyn == NULL ||
		    !b->saddr.addr_dyn->undefined) &&
		    PF_MATCHA(0, &b->saddr.addr, &fullmask, saddr, af) &&
		    (b->daddr.addr_dyn == NULL ||
		    !b->daddr.addr_dyn->undefined) &&
		    PF_MATCHA(b->dnot, &b->daddr.addr, &b->dmask, daddr, af))
			bm = b;
		else if (direction == PF_IN && b->ifp == ifp &&
		    (!b->proto || b->proto == proto) &&
		    (!b->af || b->af == af) &&
		    (b->raddr.addr_dyn == NULL ||
		    !b->raddr.addr_dyn->undefined) &&
		    PF_MATCHA(0, &b->raddr.addr, &fullmask, saddr, af) &&
		    (b->daddr.addr_dyn == NULL ||
		    !b->daddr.addr_dyn->undefined) &&
		    PF_MATCHA(b->dnot, &b->daddr.addr, &b->dmask, daddr, af))
			bm = b;
		else
			b = TAILQ_NEXT(b, entries);
	}
	if (bm && bm->no)
		return (NULL);
	if (bm && direction == PF_OUT && bm->raddr.addr_dyn != NULL &&
	    bm->raddr.addr_dyn->undefined)
		return (NULL);
	if (bm && direction == PF_IN && bm->saddr.addr_dyn != NULL &&
	    bm->saddr.addr_dyn->undefined)
		return (NULL);
	return (bm);
}

struct pf_rdr *
pf_get_rdr(struct ifnet *ifp, u_int8_t proto, struct pf_addr *saddr,
    struct pf_addr *daddr, u_int16_t dport, int af)
{
	struct pf_rdr *r, *rm = NULL;

	r = TAILQ_FIRST(pf_rdrs_active);
	while (r && rm == NULL) {
		if (((r->ifp == NULL) || (r->ifp == ifp && !r->ifnot) ||
		    (r->ifp != ifp && r->ifnot)) &&
		    (!r->proto || r->proto == proto) &&
		    (!r->af || r->af == af) &&
		    (r->saddr.addr_dyn == NULL ||
		    !r->saddr.addr_dyn->undefined) &&
		    PF_MATCHA(r->snot, &r->saddr.addr, &r->smask, saddr, af) &&
		    (r->daddr.addr_dyn == NULL ||
		    !r->daddr.addr_dyn->undefined) &&
		    PF_MATCHA(r->dnot, &r->daddr.addr, &r->dmask, daddr, af) &&
		    ((!r->dport2 && (!r->dport || dport == r->dport)) ||
		    (r->dport2 && (ntohs(dport) >= ntohs(r->dport)) &&
		    ntohs(dport) <= ntohs(r->dport2))))
			rm = r;
		else
			r = TAILQ_NEXT(r, entries);
	}
	if (rm && (rm->no || (rm->raddr.addr_dyn != NULL &&
	    rm->raddr.addr_dyn->undefined)))
		return (NULL);
	return (rm);
}

u_int16_t
pf_map_port_range(struct pf_rdr *rdr, u_int16_t port)
{
	u_int32_t nport;

	nport = ntohs(rdr->rport) - ntohs(rdr->dport) + ntohs(port);
	/* wrap around if necessary */
	if (nport > 65535)
		nport -= 65535;
	return (htons((u_int16_t)nport));
}

int
pf_socket_lookup(uid_t *uid, gid_t *gid, int direction, int af, int proto,
    struct pf_pdesc *pd)
{
	struct pf_addr *saddr, *daddr;
	u_int16_t sport, dport;
	struct inpcbtable *tb;
	struct inpcb *inp;

	*uid = UID_MAX;
	*gid = GID_MAX;
	if (af != AF_INET)
		return (0);
	switch (proto) {
	case IPPROTO_TCP:
		sport = pd->hdr.tcp->th_sport;
		dport = pd->hdr.tcp->th_dport;
		tb = &tcbtable;
		break;
	case IPPROTO_UDP:
		sport = pd->hdr.udp->uh_sport;
		dport = pd->hdr.udp->uh_dport;
		tb = &udbtable;
		break;
	default:
		return (0);
	}
	if (direction == PF_IN) {
		saddr = pd->src;
		daddr = pd->dst;
	} else {
		u_int16_t p;

		p = sport;
		sport = dport;
		dport = p;
		saddr = pd->dst;
		daddr = pd->src;
	}
	inp = in_pcbhashlookup(tb, saddr->v4, sport, daddr->v4, dport);
	if (inp == NULL) {
		inp = in_pcblookup(tb, &saddr->v4, sport, &daddr->v4, dport,
		    INPLOOKUP_WILDCARD);
		if (inp == NULL)
			return (0);
	}
	*uid = inp->inp_socket->so_euid;
	*gid = inp->inp_socket->so_egid;
	return (1);
}

int
pf_test_tcp(struct pf_rule **rm, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_nat *nat = NULL;
	struct pf_binat *binat = NULL;
	struct pf_rdr *rdr = NULL;
	struct pf_addr *saddr = pd->src, *daddr = pd->dst, baddr;
	struct tcphdr *th = pd->hdr.tcp;
	u_int16_t bport, nport = 0, af = pd->af;
	int lookup = -1;
	uid_t uid;
	gid_t gid;
	struct pf_rule *r;
	u_short reason;
	int rewrite = 0, error;

	*rm = NULL;

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT */
		if ((binat = pf_get_binat(PF_OUT, ifp, IPPROTO_TCP,
		    saddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			bport = th->th_sport;
			pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &binat->raddr.addr, th->th_sport, 0, af);
			rewrite++;
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, IPPROTO_TCP,
		    saddr, th->th_sport, daddr, th->th_dport, af)) != NULL) {
			bport = nport = th->th_sport;
			error = pf_get_sport(af, IPPROTO_TCP, daddr,
			    &nat->raddr.addr, th->th_dport, &nport,
			    nat->proxy_port[0], nat->proxy_port[1]);
			if (error) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: NAT proxy port allocation "
				    "(tcp %u-%u) failed\n",
				    nat->proxy_port[0], nat->proxy_port[1]));
				return (PF_DROP);
			}
			PF_ACPY(&baddr, saddr, af);
			pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &nat->raddr.addr, htons(nport),
			    0, af);
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_TCP, saddr, daddr,
		    th->th_dport, af)) != NULL) {
			bport = th->th_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, th->th_dport);
			else if (rdr->rport)
				nport = rdr->rport;
			else
				nport = bport;
			PF_ACPY(&baddr, daddr, af);
			pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &rdr->raddr.addr, nport, 0, af);
			rewrite++;
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_TCP,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			bport = th->th_dport;
			pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &binat->saddr.addr, th->th_dport, 0, af);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		r->evaluations++;
		if (r->action == PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && ((r->ifp != ifp && !r->ifnot) ||
		    (r->ifp == ifp && r->ifnot)))
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != direction)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != IPPROTO_TCP)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(saddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr.addr, &r->src.mask, saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT];
		else if (r->dst.noroute && pf_routable(daddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->dst.noroute &&
		    !PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr.addr, &r->dst.mask, daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT];
		else if (r->tos && !(r->tos & pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if ((r->flagset & th->th_flags) != r->flags)
			r = TAILQ_NEXT(r, entries);
		else if (r->uid.op && (lookup != -1 || (lookup =
		    pf_socket_lookup(&uid, &gid, direction, af, IPPROTO_TCP, pd),
		    1)) && !pf_match_uid(r->uid.op, r->uid.uid[0],
		    r->uid.uid[1], uid))
			r = TAILQ_NEXT(r, entries);
		else if (r->gid.op && (lookup != -1 || (lookup =
		    pf_socket_lookup(&uid, &gid, direction, af, IPPROTO_TCP, pd),
		    1)) && !pf_match_gid(r->gid.op, r->gid.gid[0],
		    r->gid.gid[1], gid))
			r = TAILQ_NEXT(r, entries);
		else {
			*rm = r;
			if ((*rm)->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		}
	}

	if (*rm != NULL) {
		(*rm)->packets++;
		(*rm)->bytes += pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);

		if ((*rm)->log) {
			if (rewrite)
				m_copyback(m, off, sizeof(*th), (caddr_t)th);
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);
		}

		if (((*rm)->action == PF_DROP) &&
		    (((*rm)->rule_flag & PFRULE_RETURNRST) ||
		    ((*rm)->rule_flag & PFRULE_RETURNICMP) ||
		    ((*rm)->rule_flag & PFRULE_RETURN))) {
			/* undo NAT/RST changes, if they have taken place */
			if (nat != NULL ||
			    (binat != NULL && direction == PF_OUT)) {
				pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
				    &th->th_sum, &baddr, bport, 0, af);
				rewrite++;
			} else if (rdr != NULL ||
			    (binat != NULL && direction == PF_IN)) {
				pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
				    &th->th_sum, &baddr, bport, 0, af);
				rewrite++;
			}
			if (((*rm)->rule_flag & PFRULE_RETURNRST) ||
			    ((*rm)->rule_flag & PFRULE_RETURN))
				pf_send_reset(off, th, pd, af,
				    (*rm)->return_ttl);
			else if ((af == AF_INET) && (*rm)->return_icmp)
				pf_send_icmp(m, (*rm)->return_icmp >> 8,
				    (*rm)->return_icmp & 255, af);
			else if ((af == AF_INET6) && (*rm)->return_icmp6)
				pf_send_icmp(m, (*rm)->return_icmp6 >> 8,
				    (*rm)->return_icmp6 & 255, af);
		}

		if ((*rm)->action == PF_DROP) 
			return (PF_DROP);
	}

	if (((*rm != NULL) && (*rm)->keep_state) || nat != NULL ||
	    binat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s = NULL;

		len = pd->tot_len - off - (th->th_off << 2);
		if (*rm == NULL || !(*rm)->max_states ||
		    (*rm)->states < (*rm)->max_states)
			s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
			return (PF_DROP);
		}
		if (*rm != NULL)
			(*rm)->states++;

		s->rule.ptr = *rm;
		s->allow_opts = *rm && (*rm)->allow_opts;
		s->log = *rm && ((*rm)->log & 2);
		s->proto = IPPROTO_TCP;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = th->th_sport;		/* sport */
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = th->th_dport;
			if (nat != NULL || binat != NULL) {
				PF_ACPY(&s->lan.addr, &baddr, af);
				s->lan.addr = baddr;
				s->lan.port = bport;
			} else {
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
				s->lan.port = s->gwy.port;
			}
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = th->th_dport;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = th->th_sport;
			if (binat != NULL ||rdr != NULL) {
				PF_ACPY(&s->gwy.addr, &baddr, af);
				s->gwy.port = bport;
			} else {
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
				s->gwy.port = s->lan.port;
			}
		}

		s->src.seqlo = ntohl(th->th_seq);
		s->src.seqhi = s->src.seqlo + len + 1;
		if ((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN &&
		    *rm != NULL && (*rm)->keep_state == PF_STATE_MODULATE) {
			/* Generate sequence number modulator */
			while ((s->src.seqdiff = arc4random()) == 0)
				;
			pf_change_a(&th->th_seq, &th->th_sum,
			    htonl(s->src.seqlo + s->src.seqdiff), 0);
			rewrite = 1;
		} else
			s->src.seqdiff = 0;
		if (th->th_flags & TH_SYN)
			s->src.seqhi++;
		if (th->th_flags & TH_FIN)
			s->src.seqhi++;
		s->src.max_win = MAX(ntohs(th->th_win), 1);
		s->dst.seqlo = 0;	/* Haven't seen these yet */
		s->dst.seqhi = 1;
		s->dst.max_win = 1;
		s->dst.seqdiff = 0;	/* Defer random generation */
		s->src.state = TCPS_SYN_SENT;
		s->dst.state = TCPS_CLOSED;
		s->creation = time.tv_sec;
		s->expire = s->creation + TIMEOUT(*rm, PFTM_TCP_FIRST_PACKET);
		s->packets = 1;
		s->bytes = pd->tot_len;
		if (pf_insert_state(s)) {
			REASON_SET(&reason, PFRES_MEMORY);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);
}

int
pf_test_udp(struct pf_rule **rm, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_nat *nat = NULL;
	struct pf_binat *binat = NULL;
	struct pf_rdr *rdr = NULL;
	struct pf_addr *saddr = pd->src, *daddr = pd->dst, baddr;
	struct udphdr *uh = pd->hdr.udp;
	u_int16_t bport, nport = 0, af = pd->af;
	int lookup = -1;
	uid_t uid;
	gid_t gid;
	struct pf_rule *r;
	u_short reason;
	int rewrite = 0, error;

	*rm = NULL;

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT */
		if ((binat = pf_get_binat(PF_OUT, ifp, IPPROTO_UDP,
		    saddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			bport = uh->uh_sport;
			pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &binat->raddr.addr, uh->uh_sport, 1, af);
			rewrite++;
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, IPPROTO_UDP,
		    saddr, uh->uh_sport, daddr, uh->uh_dport, af)) != NULL) {
			bport = nport = uh->uh_sport;
			error = pf_get_sport(af, IPPROTO_UDP, daddr,
			    &nat->raddr.addr, uh->uh_dport, &nport,
			    nat->proxy_port[0], nat->proxy_port[1]);
			if (error) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: NAT proxy port allocation "
				    "(udp %u-%u) failed\n",
				    nat->proxy_port[0], nat->proxy_port[1]));
				return (PF_DROP);
			}
			PF_ACPY(&baddr, saddr, af);
			pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &nat->raddr.addr, htons(nport),
			    1, af);
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_UDP, saddr, daddr,
		    uh->uh_dport, af)) != NULL) {
			bport = uh->uh_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, uh->uh_dport);
			else if (rdr->rport)
				nport = rdr->rport;
			else
				nport = bport;

			PF_ACPY(&baddr, daddr, af);
			pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &rdr->raddr.addr, nport, 1, af);
			rewrite++;
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_UDP,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			bport = uh->uh_dport;
			pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &binat->saddr.addr, uh->uh_dport, 1, af);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		r->evaluations++;
		if (r->action == PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && ((r->ifp != ifp && !r->ifnot) ||
		    (r->ifp == ifp && r->ifnot)))
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != direction)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != IPPROTO_UDP)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(saddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->src.mask, af) &&
		    !PF_MATCHA(r->src.not, &r->src.addr.addr, &r->src.mask,
		    saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], uh->uh_sport))
			r = r->skip[PF_SKIP_SRC_PORT];
		else if (r->dst.noroute && pf_routable(daddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->dst.noroute &&
		    !PF_AZERO(&r->dst.mask, af) &&
		    !PF_MATCHA(r->dst.not, &r->dst.addr.addr, &r->dst.mask,
			daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], uh->uh_dport))
			r = r->skip[PF_SKIP_DST_PORT];
		else if (r->tos && !(r->tos & pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (r->uid.op && (lookup != -1 || (lookup =
		    pf_socket_lookup(&uid, &gid, direction, af, IPPROTO_UDP, pd),
		    1)) && !pf_match_uid(r->uid.op, r->uid.uid[0],
		    r->uid.uid[1], uid))
			r = TAILQ_NEXT(r, entries);
		else if (r->gid.op && (lookup != -1 || (lookup =
		    pf_socket_lookup(&uid, &gid, direction, af, IPPROTO_UDP, pd),
		    1)) && !pf_match_gid(r->gid.op, r->gid.gid[0],
		    r->gid.gid[1], gid))
			r = TAILQ_NEXT(r, entries);
		else {
			*rm = r;
			if ((*rm)->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		}
	}

	if (*rm != NULL) {
		(*rm)->packets++;
		(*rm)->bytes += pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);

		if ((*rm)->log) {
			if (rewrite)
				m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);
		}

		if (((*rm)->action == PF_DROP) && 
		    (((*rm)->rule_flag & PFRULE_RETURNICMP) ||
		    ((*rm)->rule_flag & PFRULE_RETURN))) {
			/* undo NAT/RST changes, if they have taken place */
			if (nat != NULL ||
			    (binat != NULL && direction == PF_OUT)) {
				pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
				    &uh->uh_sum, &baddr, bport, 1, af);
				rewrite++;
			} else if (rdr != NULL ||
			    (binat != NULL && direction == PF_IN)) {
				pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
				    &uh->uh_sum, &baddr, bport, 1, af);
				rewrite++;
			}
			if ((af == AF_INET) && (*rm)->return_icmp)
				pf_send_icmp(m, (*rm)->return_icmp >> 8,
				    (*rm)->return_icmp & 255, af);
			else if ((af == AF_INET6) && (*rm)->return_icmp6)
				pf_send_icmp(m, (*rm)->return_icmp6 >> 8,
				    (*rm)->return_icmp6 & 255, af);
		}

		if ((*rm)->action == PF_DROP) 
			return (PF_DROP);
	}

	if ((*rm != NULL && (*rm)->keep_state) || nat != NULL ||
	    binat != NULL || rdr != NULL) {
		/* create new state */
		struct pf_state *s = NULL;

		if (*rm == NULL || !(*rm)->max_states ||
		    (*rm)->states < (*rm)->max_states)
			s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);
		if (*rm != NULL)
			(*rm)->states++;

		s->rule.ptr = *rm;
		s->allow_opts = *rm && (*rm)->allow_opts;
		s->log = *rm && ((*rm)->log & 2);
		s->proto = IPPROTO_UDP;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = uh->uh_sport;
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = uh->uh_dport;
			if (nat != NULL || binat != NULL) {
				PF_ACPY(&s->lan.addr, &baddr, af);
				s->lan.port = bport;
			} else {
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
				s->lan.port = s->gwy.port;
			}
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = uh->uh_dport;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = uh->uh_sport;
			if (binat != NULL || rdr != NULL) {
				PF_ACPY(&s->gwy.addr, &baddr, af);
				s->gwy.port = bport;
			} else {
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
				s->gwy.port = s->lan.port;
			}
		}
		s->src.seqlo = 0;
		s->src.seqhi = 0;
		s->src.seqdiff = 0;
		s->src.max_win = 0;
		s->src.state = PFUDPS_SINGLE;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.seqdiff = 0;
		s->dst.max_win = 0;
		s->dst.state = PFUDPS_NO_TRAFFIC;
		s->creation = time.tv_sec;
		s->expire = s->creation + TIMEOUT(*rm, PFTM_UDP_FIRST_PACKET);
		s->packets = 1;
		s->bytes = pd->tot_len;
		if (pf_insert_state(s)) {
			REASON_SET(&reason, PFRES_MEMORY);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);

	return (PF_PASS);
}

int
pf_test_icmp(struct pf_rule **rm, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_nat *nat = NULL;
	struct pf_binat *binat = NULL;
	struct pf_rdr *rdr = NULL;
	struct pf_addr *saddr = pd->src, *daddr = pd->dst, baddr;
	struct pf_rule *r;
	u_short reason;
	u_int16_t icmpid, af = pd->af;
	u_int8_t icmptype, icmpcode;
	int state_icmp = 0;
#ifdef INET6
	int rewrite = 0;
#endif /* INET6 */

	*rm = NULL;

	switch (pd->proto) {
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpcode = pd->hdr.icmp->icmp_code;
		icmpid = pd->hdr.icmp->icmp_id;

		if (icmptype == ICMP_UNREACH ||
		    icmptype == ICMP_SOURCEQUENCH ||
		    icmptype == ICMP_REDIRECT ||
		    icmptype == ICMP_TIMXCEED ||
		    icmptype == ICMP_PARAMPROB)
			state_icmp++;
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpcode = pd->hdr.icmp6->icmp6_code;
		icmpid = pd->hdr.icmp6->icmp6_id;

		if (icmptype == ICMP6_DST_UNREACH ||
		    icmptype == ICMP6_PACKET_TOO_BIG ||
		    icmptype == ICMP6_TIME_EXCEEDED ||
		    icmptype == ICMP6_PARAM_PROB)
			state_icmp++;
		break;
#endif /* INET6 */
	}

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT */
		if ((binat = pf_get_binat(PF_OUT, ifp, IPPROTO_ICMP,
		    saddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    binat->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &binat->raddr.addr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, pd->proto,
		    saddr, 0, daddr, 0, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr,
				    pd->ip_sum, nat->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &nat->raddr.addr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, pd->proto,
		    saddr, daddr, 0, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, rdr->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(daddr, &pd->hdr.icmp6->icmp6_cksum,
				    &rdr->raddr.addr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_ICMP,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, binat->saddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(daddr, &pd->hdr.icmp6->icmp6_cksum,
				    &binat->saddr.addr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		r->evaluations++;
		if (r->action == PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && ((r->ifp != ifp && !r->ifnot) ||
		    (r->ifp == ifp && r->ifnot)))
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != direction)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(saddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr.addr, &r->src.mask, saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->dst.noroute && pf_routable(daddr, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->dst.noroute &&
		    !PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr.addr, &r->dst.mask, daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->type && r->type != icmptype + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->code && r->code != icmpcode + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->tos && !(r->tos & pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else {
			*rm = r;
			if ((*rm)->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		}
	}

	if (*rm != NULL) {
		(*rm)->packets++;
		(*rm)->bytes += pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);

		if ((*rm)->log) {
#ifdef INET6
			if (rewrite)
				m_copyback(m, off, ICMP_MINLEN,
				    (caddr_t)pd->hdr.icmp6);
#endif /* INET6 */
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);
		}

		if ((*rm)->action != PF_PASS)
			return (PF_DROP);
	}

	if (!state_icmp && ((*rm != NULL && (*rm)->keep_state) ||
	    nat != NULL || rdr != NULL || binat != NULL)) {
		/* create new state */
		struct pf_state *s = NULL;

		if (*rm == NULL || !(*rm)->max_states ||
		    (*rm)->states < (*rm)->max_states)
			s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);
		if (*rm != NULL)
			(*rm)->states++;

		s->rule.ptr = *rm;
		s->allow_opts = *rm && (*rm)->allow_opts;
		s->log = *rm && ((*rm)->log & 2);
		s->proto = pd->proto;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = icmpid;
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = icmpid;
			if (nat != NULL || binat != NULL)
				PF_ACPY(&s->lan.addr, &baddr, af);
			else
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
			s->lan.port = icmpid;
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = icmpid;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = icmpid;
			if (binat != NULL || rdr != NULL)
				PF_ACPY(&s->gwy.addr, &baddr, af);
			else
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
			s->gwy.port = icmpid;
		}
		s->src.seqlo = 0;
		s->src.seqhi = 0;
		s->src.seqdiff = 0;
		s->src.max_win = 0;
		s->src.state = 0;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.seqdiff = 0;
		s->dst.max_win = 0;
		s->dst.state = 0;
		s->creation = time.tv_sec;
		s->expire = s->creation + TIMEOUT(*rm, PFTM_ICMP_FIRST_PACKET);
		s->packets = 1;
		s->bytes = pd->tot_len;
		if (pf_insert_state(s)) {
			REASON_SET(&reason, PFRES_MEMORY);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
	}

#ifdef INET6
	/* copy back packet headers if we performed IPv6 NAT operations */
	if (rewrite)
		m_copyback(m, off, ICMP_MINLEN,
		    (caddr_t)pd->hdr.icmp6);
#endif /* INET6 */

	return (PF_PASS);
}

int
pf_test_other(struct pf_rule **rm, int direction, struct ifnet *ifp,
    struct mbuf *m, void *h, struct pf_pdesc *pd)
{
	struct pf_rule *r;
	struct pf_nat *nat = NULL;
	struct pf_binat *binat = NULL;
	struct pf_rdr *rdr = NULL;
	struct pf_addr *saddr = pd->src, *daddr = pd->dst, baddr;
	u_int8_t af = pd->af;
	u_short reason;


	*rm = NULL;

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT */
		if ((binat = pf_get_binat(PF_OUT, ifp, pd->proto,
		    saddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    binat->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(saddr, &binat->raddr.addr, af);
				break;
#endif /* INET6 */
			}
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, pd->proto,
		    saddr, 0, daddr, 0, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr,
				    pd->ip_sum, nat->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(saddr, &nat->raddr.addr, af);
				break;
#endif /* INET6 */
			}
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, pd->proto,
		    saddr, daddr, 0, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, rdr->raddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(daddr, &rdr->raddr.addr, af);
				break;
#endif /* INET6 */
			}
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, pd->proto,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, binat->saddr.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(daddr, &binat->saddr.addr, af);
				break;
#endif /* INET6 */
			}
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		r->evaluations++;
		if (r->action == PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && ((r->ifp != ifp && !r->ifnot) ||
		    (r->ifp == ifp && r->ifnot)))
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != direction)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(pd->src, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr.addr, &r->src.mask, pd->src, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->dst.noroute && pf_routable(pd->dst, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr.addr, &r->dst.mask, pd->dst, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->tos && !(r->tos & pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else {
			*rm = r;
			if ((*rm)->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		}
	}

	if (*rm != NULL) {
		(*rm)->packets++;
		(*rm)->bytes += pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);
		if ((*rm)->log)
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);

		if ((*rm)->action != PF_PASS)
			return (PF_DROP);
	}

	if ((*rm != NULL && (*rm)->keep_state) || nat != NULL ||
	    rdr != NULL || binat != NULL) {
		/* create new state */
		struct pf_state *s = NULL;

		if (*rm == NULL || !(*rm)->max_states ||
		    (*rm)->states < (*rm)->max_states)
			s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);
		if (*rm != NULL)
			(*rm)->states++;

		s->rule.ptr = *rm;
		s->allow_opts = *rm && (*rm)->allow_opts;
		s->log = *rm && ((*rm)->log & 2);
		s->proto = pd->proto;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = 0;
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = 0;
			if (nat != NULL || binat != NULL)
				PF_ACPY(&s->lan.addr, &baddr, af);
			else
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
			s->lan.port = 0;
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = 0;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = 0;
			if (binat != NULL || rdr != NULL)
				PF_ACPY(&s->gwy.addr, &baddr, af);
			else
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
			s->gwy.port = 0;
		}
		s->src.seqlo = 0;
		s->src.seqhi = 0;
		s->src.seqdiff = 0;
		s->src.max_win = 0;
		s->src.state = PFOTHERS_SINGLE;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.seqdiff = 0;
		s->dst.max_win = 0;
		s->dst.state = PFOTHERS_NO_TRAFFIC;
		s->creation = time.tv_sec;
		s->expire = s->creation + TIMEOUT(*rm, PFTM_OTHER_FIRST_PACKET);
		s->packets = 1;
		s->bytes = pd->tot_len;
		if (pf_insert_state(s)) {
			REASON_SET(&reason, PFRES_MEMORY);
			if (*rm && (*rm)->log)
				PFLOG_PACKET(ifp, h, m, af, direction, reason,
				    *rm);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
	}

	return (PF_PASS);
}

int
pf_test_fragment(struct pf_rule **rm, int direction, struct ifnet *ifp,
    struct mbuf *m, void *h, struct pf_pdesc *pd)
{
	struct pf_rule *r;
	u_int8_t af = pd->af;

	*rm = NULL;

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		r->evaluations++;
		if (r->action == PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && ((r->ifp != ifp && !r->ifnot) ||
		    (r->ifp == ifp && r->ifnot)))
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != direction)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(pd->src, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr.addr, &r->src.mask, pd->src, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->dst.noroute && pf_routable(pd->dst, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute &&
		    !PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr.addr, &r->dst.mask, pd->dst, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->tos && !(r->tos & pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->src.port_op || r->dst.port_op ||
		    r->flagset || r->type || r->code)
			r = TAILQ_NEXT(r, entries);
		else {
			*rm = r;
			if ((*rm)->quick)
				break;
			r = TAILQ_NEXT(r, entries);
		}
	}

	if (*rm != NULL) {
		u_short reason;

		(*rm)->packets++;
		(*rm)->bytes += pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);
		if ((*rm)->log)
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);

		if ((*rm)->action != PF_PASS)
			return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_test_state_tcp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_tree_node key;
	struct tcphdr *th = pd->hdr.tcp;
	u_int16_t win = ntohs(th->th_win);
	u_int32_t ack, end, seq;
	int ackskew;
	struct pf_state_peer *src, *dst;

	key.af = pd->af;
	key.proto = IPPROTO_TCP;
	PF_ACPY(&key.addr[0], pd->src, key.af);
	PF_ACPY(&key.addr[1], pd->dst, key.af);
	key.port[0] = th->th_sport;
	key.port[1] = th->th_dport;

	if (direction == PF_IN)
		*state = pf_find_state(&tree_ext_gwy, &key);
	else
		*state = pf_find_state(&tree_lan_ext, &key);
	if (*state == NULL)
		return (PF_DROP);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/*
	 * Sequence tracking algorithm from Guido van Rooij's paper:
	 *   http://www.madison-gurkha.com/publications/tcp_filtering/
	 *	tcp_filtering.ps
	 */

	seq = ntohl(th->th_seq);
	if (src->seqlo == 0) {
		/* First packet from this end. Set its state */

		/* Deferred generation of sequence number modulator */
		if (dst->seqdiff) {
			while ((src->seqdiff = arc4random()) == 0)
				;
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
		} else {
			ack = ntohl(th->th_ack);
		}

		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN)
			end++;
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
		    SEQ_GEQ(end + MAX(1, dst->max_win), src->seqhi))
			src->seqhi = end + MAX(1, dst->max_win);
		if (win > src->max_win)
			src->max_win = win;

	} else {
		ack = ntohl(th->th_ack) - dst->seqdiff;
		if (src->seqdiff) {
			/* Modulate sequence numbers */
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
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
	    /* Many stacks (ours included) will set the ACK number in an
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

#define MAXACKWINDOW (0xffff + 1500)	/* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - dst->max_win) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one window back */
	    (ackskew <= MAXACKWINDOW)) {
	    /* Acking not more than one window forward */

		(*state)->packets++;
		(*state)->bytes += pd->tot_len;

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + win, dst->seqhi))
			dst->seqhi = ack + MAX(win, 1);


		/* update states */
		if (th->th_flags & TH_SYN)
			if (src->state < TCPS_SYN_SENT)
				src->state = TCPS_SYN_SENT;
		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				src->state = TCPS_CLOSING;
		if (th->th_flags & TH_ACK) {
			if (dst->state == TCPS_SYN_SENT)
				dst->state = TCPS_ESTABLISHED;
			else if (dst->state == TCPS_CLOSING)
				dst->state = TCPS_FIN_WAIT_2;
		}
		if (th->th_flags & TH_RST)
			src->state = dst->state = TCPS_TIME_WAIT;

		/* update expire time */
		if (src->state >= TCPS_FIN_WAIT_2 &&
		    dst->state >= TCPS_FIN_WAIT_2)
			(*state)->expire = time.tv_sec +
			    TIMEOUT((*state)->rule.ptr, PFTM_TCP_CLOSED);
		else if (src->state >= TCPS_FIN_WAIT_2 ||
		    dst->state >= TCPS_FIN_WAIT_2)
			(*state)->expire = time.tv_sec +
			    TIMEOUT((*state)->rule.ptr, PFTM_TCP_FIN_WAIT);
		else if (src->state >= TCPS_CLOSING ||
		    dst->state >= TCPS_CLOSING)
			(*state)->expire = time.tv_sec +
			    TIMEOUT((*state)->rule.ptr, PFTM_TCP_CLOSING);
		else if (src->state < TCPS_ESTABLISHED ||
		    dst->state < TCPS_ESTABLISHED)
			(*state)->expire = time.tv_sec +
			    TIMEOUT((*state)->rule.ptr, PFTM_TCP_OPENING);
		else
			(*state)->expire = time.tv_sec +
			    TIMEOUT((*state)->rule.ptr, PFTM_TCP_ESTABLISHED);

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

		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: loose state match: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%lu ack=%lu len=%u ackskew=%d pkts=%d\n",
			    seq, ack, pd->p_len, ackskew, (*state)->packets);
		}

		(*state)->packets++;
		(*state)->bytes += pd->tot_len;

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + win, dst->seqhi))
			dst->seqhi = ack + MAX(win, 1);

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
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: BAD state: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%lu ack=%lu len=%u ackskew=%d pkts=%d "
			    "dir=%s,%s\n", seq, ack, pd->p_len, ackskew,
			    ++(*state)->packets,
			    direction == PF_IN ? "in" : "out",
			    direction == (*state)->direction ? "fwd" : "rev");
			printf("pf: State failure on: %c %c %c %c | %c %c\n",
			    SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
			    SEQ_GEQ(seq, src->seqlo - dst->max_win) ? ' ': '2',
			    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
			    (ackskew <= MAXACKWINDOW) ? ' ' : '4',
			    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) ?' ' :'5',
			    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW) ?' ' :'6');
		}
		return (PF_DROP);
	}

	/* Any packets which have gotten here are to be passed */

	/* translate source/destination address, if needed */
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			pf_change_ap(pd->src, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &(*state)->gwy.addr,
			    (*state)->gwy.port, 0, pd->af);
		else
			pf_change_ap(pd->dst, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &(*state)->lan.addr,
			    (*state)->lan.port, 0, pd->af);
		m_copyback(m, off, sizeof(*th), (caddr_t)th);
	} else if (src->seqdiff) {
		/* Copyback sequence modulation */
		m_copyback(m, off, sizeof(*th), (caddr_t)th);
	}

	if ((*state)->rule.ptr != NULL) {
		(*state)->rule.ptr->packets++;
		(*state)->rule.ptr->bytes += pd->tot_len;
	}
	return (PF_PASS);
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_state_peer *src, *dst;
	struct pf_tree_node key;
	struct udphdr *uh = pd->hdr.udp;

	key.af = pd->af;
	key.proto = IPPROTO_UDP;
	PF_ACPY(&key.addr[0], pd->src, key.af);
	PF_ACPY(&key.addr[1], pd->dst, key.af);
	key.port[0] = pd->hdr.udp->uh_sport;
	key.port[1] = pd->hdr.udp->uh_dport;

	if (direction == PF_IN)
		*state = pf_find_state(&tree_ext_gwy, &key);
	else
		*state = pf_find_state(&tree_lan_ext, &key);
	if (*state == NULL)
		return (PF_DROP);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	(*state)->packets++;
	(*state)->bytes += pd->tot_len;

	/* update states */
	if (src->state < PFUDPS_SINGLE)
		src->state = PFUDPS_SINGLE;
	if (dst->state == PFUDPS_SINGLE)
		dst->state = PFUDPS_MULTIPLE;

	/* update expire time */
	if (src->state == PFUDPS_MULTIPLE && dst->state == PFUDPS_MULTIPLE)
		(*state)->expire = time.tv_sec +
		    TIMEOUT((*state)->rule.ptr, PFTM_UDP_MULTIPLE);
	else
		(*state)->expire = time.tv_sec +
		    TIMEOUT((*state)->rule.ptr, PFTM_UDP_SINGLE);

	/* translate source/destination address, if necessary */
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			pf_change_ap(pd->src, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &(*state)->gwy.addr,
			    (*state)->gwy.port, 1, pd->af);
		else
			pf_change_ap(pd->dst, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &(*state)->lan.addr,
			    (*state)->lan.port, 1, pd->af);
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
	}

	if ((*state)->rule.ptr != NULL) {
		(*state)->rule.ptr->packets++;
		(*state)->rule.ptr->bytes += pd->tot_len;
	}
	return (PF_PASS);
}

int
pf_test_state_icmp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_addr *saddr = pd->src, *daddr = pd->dst;
	u_int16_t icmpid, *icmpsum;
	u_int8_t icmptype;
	int state_icmp = 0;

	switch (pd->proto) {
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpid = pd->hdr.icmp->icmp_id;
		icmpsum = &pd->hdr.icmp->icmp_cksum;

		if (icmptype == ICMP_UNREACH ||
		    icmptype == ICMP_SOURCEQUENCH ||
		    icmptype == ICMP_REDIRECT ||
		    icmptype == ICMP_TIMXCEED ||
		    icmptype == ICMP_PARAMPROB)
			state_icmp++;
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpid = pd->hdr.icmp6->icmp6_id;
		icmpsum = &pd->hdr.icmp6->icmp6_cksum;

		if (icmptype == ICMP6_DST_UNREACH ||
		    icmptype == ICMP6_PACKET_TOO_BIG ||
		    icmptype == ICMP6_TIME_EXCEEDED ||
		    icmptype == ICMP6_PARAM_PROB)
			state_icmp++;
		break;
#endif /* INET6 */
	}

	if (!state_icmp) {

		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		struct pf_tree_node key;

		key.af = pd->af;
		key.proto = pd->proto;
		PF_ACPY(&key.addr[0], saddr, key.af);
		PF_ACPY(&key.addr[1], daddr, key.af);
		key.port[0] = icmpid;
		key.port[1] = icmpid;

		if (direction == PF_IN)
			*state = pf_find_state(&tree_ext_gwy, &key);
		else
			*state = pf_find_state(&tree_lan_ext, &key);
		if (*state == NULL)
			return (PF_DROP);

		(*state)->packets++;
		(*state)->bytes += pd->tot_len;
		(*state)->expire = time.tv_sec +
		    TIMEOUT((*state)->rule.ptr, PFTM_ICMP_ERROR_REPLY);

		/* translate source/destination address, if needed */
		if (PF_ANEQ(&(*state)->lan.addr, &(*state)->gwy.addr, pd->af)) {
			if (direction == PF_OUT) {
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					pf_change_a(&saddr->v4.s_addr,
					    pd->ip_sum,
					    (*state)->gwy.addr.v4.s_addr, 0);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					pf_change_a6(saddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &(*state)->gwy.addr, 0);
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp6);
					break;
#endif /* INET6 */
				}
			} else {
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					pf_change_a(&daddr->v4.s_addr,
					    pd->ip_sum,
					    (*state)->lan.addr.v4.s_addr, 0);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					pf_change_a6(daddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &(*state)->lan.addr, 0);
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp6);
					break;
#endif /* INET6 */
				}
			}
		}

		return (PF_PASS);

	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct pf_pdesc pd2;
#ifdef INET
		struct ip h2;
#endif /* INET */
#ifdef INET6
		struct ip6_hdr h2_6;
		int terminal = 0;
#endif /* INET6 */
		int ipoff2;
		int off2;

		pd2.af = pd->af;
		switch (pd->af) {
#ifdef INET
		case AF_INET:
			/* offset of h2 in mbuf chain */
			ipoff2 = off + ICMP_MINLEN;

			if (!pf_pull_hdr(m, ipoff2, &h2, sizeof(h2),
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (ip)\n"));
				return (PF_DROP);
			}
			/* ICMP error messages don't refer to non-first fragments */
			if (ntohs(h2.ip_off) & IP_OFFMASK)
				return (PF_DROP);

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
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (ip6)\n"));
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
					return (PF_DROP);
				case IPPROTO_AH:
				case IPPROTO_HOPOPTS:
				case IPPROTO_ROUTING:
				case IPPROTO_DSTOPTS: {
					/* get next header and header length */
					struct ip6_ext opt6;

					if (!pf_pull_hdr(m, off2, &opt6,
					    sizeof(opt6), NULL, NULL, pd2.af)) {
						DPFPRINTF(PF_DEBUG_MISC,
						    ("pf: ICMPv6 short opt\n"));
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
			struct tcphdr th;
			u_int32_t seq;
			struct pf_tree_node key;
			struct pf_state_peer *src, *dst;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(m, off2, &th, 8, NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (tcp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_TCP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = th.th_dport;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = th.th_sport;

			if (direction == PF_IN)
				*state = pf_find_state(&tree_ext_gwy, &key);
			else
				*state = pf_find_state(&tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			if (direction == (*state)->direction) {
				src = &(*state)->dst;
				dst = &(*state)->src;
			} else {
				src = &(*state)->src;
				dst = &(*state)->dst;
			}

			/* Demodulate sequence number */
			seq = ntohl(th.th_seq) - src->seqdiff;
			if (src->seqdiff)
				pf_change_a(&th.th_seq, &th.th_sum,
				    htonl(seq), 0);

			if (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq, src->seqlo - dst->max_win)) {
				if (pf_status.debug >= PF_DEBUG_MISC) {
					printf("pf: BAD ICMP state: ");
					pf_print_state(*state);
					printf(" seq=%lu\n", seq);
				}
				return (PF_DROP);
			}

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &th.th_sport,
					    saddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &th.th_dport,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				}
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
					    (caddr_t)&h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    (caddr_t)&h2_6);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, 8, (caddr_t)&th);
			} else if (src->seqdiff) {
				m_copyback(m, off2, 8, (caddr_t)&th);
			}

			return (PF_PASS);
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr uh;
			struct pf_tree_node key;

			if (!pf_pull_hdr(m, off2, &uh, sizeof(uh),
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (udp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_UDP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = uh.uh_dport;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = uh.uh_sport;

			if (direction == PF_IN)
				*state = pf_find_state(&tree_ext_gwy, &key);
			else
				*state = pf_find_state(&tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &uh.uh_sport,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &uh.uh_dport,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				}
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
					    (caddr_t)&h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    (caddr_t)&h2_6);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, sizeof(uh),
				    (caddr_t)&uh);
			}

			return (PF_PASS);
			break;
		}
#ifdef INET
		case IPPROTO_ICMP: {
			struct icmp iih;
			struct pf_tree_node key;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (icmp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_ICMP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = iih.icmp_id;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = iih.icmp_id;

			if (direction == PF_IN)
				*state = pf_find_state(&tree_ext_gwy, &key);
			else
				*state = pf_find_state(&tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp_id,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp_id,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				}
				m_copyback(m, off, ICMP_MINLEN,
				    (caddr_t)pd->hdr.icmp);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, ICMP_MINLEN,
				    (caddr_t)&iih);
			}

			return (PF_PASS);
			break;
		}
#endif /* INET */
#ifdef INET6
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr iih;
			struct pf_tree_node key;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (icmp6)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_ICMPV6;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = iih.icmp6_id;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = iih.icmp6_id;

			if (direction == PF_IN)
				*state = pf_find_state(&tree_ext_gwy, &key);
			else
				*state = pf_find_state(&tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp6_id,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp6_id,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				}
				m_copyback(m, off, ICMP_MINLEN,
				    (caddr_t)pd->hdr.icmp6);
				m_copyback(m, ipoff2, sizeof(h2_6),
				    (caddr_t)&h2_6);
				m_copyback(m, off2, ICMP_MINLEN,
				    (caddr_t)&iih);
			}

			return (PF_PASS);
			break;
		}
#endif /* INET6 */
		default:
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: ICMP error message for bad proto\n"));
			return (PF_DROP);
		}

	}
}

int
pf_test_state_other(struct pf_state **state, int direction, struct ifnet *ifp,
    struct pf_pdesc *pd)
{
	struct pf_state_peer *src, *dst;
	struct pf_tree_node key;

	key.af = pd->af;
	key.proto = pd->proto;
	PF_ACPY(&key.addr[0], pd->src, key.af);
	PF_ACPY(&key.addr[1], pd->dst, key.af);
	key.port[0] = 0;
	key.port[1] = 0;

	if (direction == PF_IN)
		*state = pf_find_state(&tree_ext_gwy, &key);
	else
		*state = pf_find_state(&tree_lan_ext, &key);
	if (*state == NULL)
		return (PF_DROP);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	(*state)->packets++;
	(*state)->bytes += pd->tot_len;

	/* update states */
	if (src->state < PFOTHERS_SINGLE)
		src->state = PFOTHERS_SINGLE;
	if (dst->state == PFOTHERS_SINGLE)
		dst->state = PFOTHERS_MULTIPLE;

	/* update expire time */
	if (src->state == PFOTHERS_MULTIPLE && dst->state == PFOTHERS_MULTIPLE)
		(*state)->expire = time.tv_sec +
		    TIMEOUT((*state)->rule.ptr, PFTM_OTHER_MULTIPLE);
	else
		(*state)->expire = time.tv_sec +
		    TIMEOUT((*state)->rule.ptr, PFTM_OTHER_SINGLE);

	/* translate source/destination address, if necessary */
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			switch (pd->af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&pd->src->v4.s_addr,
				    pd->ip_sum, (*state)->gwy.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(pd->src, &(*state)->gwy.addr, pd->af);
				break;
#endif /* INET6 */
			}
		else
			switch (pd->af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&pd->dst->v4.s_addr,
				    pd->ip_sum, (*state)->lan.addr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(pd->dst, &(*state)->lan.addr, pd->af);
				break;
#endif /* INET6 */
			}
	}

	if ((*state)->rule.ptr != NULL) {
		(*state)->rule.ptr->packets++;
		(*state)->rule.ptr->bytes += pd->tot_len;
	}
	return (PF_PASS);
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pf_pull_hdr(struct mbuf *m, int off, void *p, int len,
    u_short *actionp, u_short *reasonp, int af)
{
	switch (af) {
#ifdef INET
	case AF_INET: {
		struct ip *h = mtod(m, struct ip *);
		u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;

		if (fragoff) {
			if (fragoff >= len)
				ACTION_SET(actionp, PF_PASS);
			else {
				ACTION_SET(actionp, PF_DROP);
				REASON_SET(reasonp, PFRES_FRAG);
			}
			return (NULL);
		}
		if (m->m_pkthdr.len < off + len || h->ip_len < off + len) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return (NULL);
		}
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *h = mtod(m, struct ip6_hdr *);
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
pf_routable(addr, af)
	struct pf_addr *addr;
	int af;
{
	struct sockaddr_in *dst;
	struct route ro;
	int ret = 0;

	bzero(&ro, sizeof(ro));
	dst = satosin(&ro.ro_dst);
	dst->sin_family = af;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = addr->v4;
	rtalloc_noclone(&ro, NO_CLONING);

	if (ro.ro_rt != NULL) {
		ret = 1;
		RTFREE(ro.ro_rt);
	}

	return (ret);
}

#ifdef INET
void
pf_route(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp)
{
	struct mbuf *m0, *m1;
	struct route iproute;
	struct route *ro;
	struct sockaddr_in *dst;
	struct ip *ip;
	struct ifnet *ifp = r->rt_ifp;
	struct m_tag *mtag;
	int hlen;
	int error = 0;

	if (r->rt == PF_DUPTO) {
		m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT);
		if (m0 == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	ip = mtod(m0, struct ip *);
	hlen = ip->ip_hl << 2;

	ro = &iproute;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = satosin(&ro->ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;

	if (r->rt == PF_FASTROUTE) {
		rtalloc(ro);
		if (ro->ro_rt == 0) {
			ipstat.ips_noroute++;
			goto bad;
		}

		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;

		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);
	} else {
		if (!PF_AZERO(&r->rt_addr, AF_INET))
			dst->sin_addr.s_addr = r->rt_addr.v4.s_addr;
	}

	if (ifp == NULL)
		goto bad;

	if (oifp != ifp) {
		mtag = m_tag_find(m0, PACKET_TAG_PF_ROUTED, NULL);
		if (mtag == NULL) {
			if (pf_test(PF_OUT, ifp, &m0) != PF_PASS)
				goto bad;
			else if (m0 == NULL)
				goto done;
			else {
				mtag = m_tag_get(PACKET_TAG_PF_ROUTED, 0,
				    M_NOWAIT);
				if (mtag == NULL)
					goto bad;
				m_tag_prepend(m0, mtag);
			}
		}
	}

	/* Copied from ip_output. */
	if ((u_int16_t)ip->ip_len <= ifp->if_mtu) {
		ip->ip_len = htons((u_int16_t)ip->ip_len);
		ip->ip_off = htons((u_int16_t)ip->ip_off);
		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    ifp->if_bridge == NULL) {
			m0->m_pkthdr.csum |= M_IPV4_CSUM_OUT;
			ipstat.ips_outhwcsum++;
		} else {
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m0, hlen);
		}
		/* Update relevant hardware checksum stats for TCP/UDP */
		if (m0->m_pkthdr.csum & M_TCPV4_CSUM_OUT)
			tcpstat.tcps_outhwcsum++;
		else if (m0->m_pkthdr.csum & M_UDPV4_CSUM_OUT)
			udpstat.udps_outhwcsum++;
		error = (*ifp->if_output)(ifp, m0, sintosa(dst), NULL);
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		ipstat.ips_cantfrag++;
		if (r->rt != PF_DUPTO) {
			icmp_error(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			    ifp);
			goto done;
		} else
			goto bad;
	}

	m1 = m0;
	error = ip_fragment(m0, ifp, ifp->if_mtu);
	if (error == EMSGSIZE)
		goto bad;

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
pf_route6(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp)
{
	struct mbuf *m0;
	struct m_tag *mtag;
	struct route_in6 ip6route;
	struct route_in6 *ro;
	struct sockaddr_in6 *dst;
	struct ip6_hdr *ip6;
	struct ifnet *ifp = r->rt_ifp;
	int error = 0;

	if (m == NULL)
		return;

	if (r->rt == PF_DUPTO) {
		m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT);
		if (m0 == NULL)
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	ip6 = mtod(m0, struct ip6_hdr *);

	ro = &ip6route;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = (struct sockaddr_in6 *)&ro->ro_dst;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;

	if (!PF_AZERO(&r->rt_addr, AF_INET6))
		dst->sin6_addr = r->rt_addr.v6;

	/* Cheat. */
	if (r->rt == PF_FASTROUTE) {
		mtag = m_tag_get(PACKET_TAG_PF_GENERATED, 0, M_NOWAIT);
		if (mtag == NULL)
			goto bad;
		m_tag_prepend(m0, mtag);
		ip6_output(m0, NULL, NULL, NULL, NULL, NULL);
		return;
	}

	if (ifp == NULL)
		goto bad;

	if (oifp != ifp) {
		mtag = m_tag_find(m0, PACKET_TAG_PF_ROUTED, NULL);
		if (mtag == NULL) {
			if (pf_test(PF_OUT, ifp, &m0) != PF_PASS)
				goto bad;
			else if (m0 == NULL)
				goto done;
			else {
				mtag = m_tag_get(PACKET_TAG_PF_ROUTED, 0,
				    M_NOWAIT);
				if (mtag == NULL)
					goto bad;
				m_tag_prepend(m0, mtag);
			}
		}
	}

	/*
	 * If the packet is too large for the outgoing interface,
	 * send back an icmp6 error.
	 */
	if ((u_long)m0->m_pkthdr.len <= ifp->if_mtu) {
		error = (*ifp->if_output)(ifp, m0, (struct sockaddr *)dst,
		    NULL);
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

#ifdef INET
int
pf_test(int dir, struct ifnet *ifp, struct mbuf **m0)
{
	u_short action, reason = 0, log = 0;
	struct mbuf *m = *m0;
	struct ip *h;
	struct pf_rule *r = NULL;
	struct pf_state *s = NULL;
	struct pf_pdesc pd;
	int off;

	if (!pf_status.running ||
	    (m_tag_find(m, PACKET_TAG_PF_GENERATED, NULL) != NULL))
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	/* We do IP header normalization and packet reassembly here */
	if (pf_normalize_ip(m0, dir, ifp, &reason) != PF_PASS) {
		ACTION_SET(&action, PF_DROP);
		goto done;
	}
	m = *m0;
	h = mtod(m, struct ip *);

	off = h->ip_hl << 2;
	if (off < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	pd.src = (struct pf_addr *)&h->ip_src;
	pd.dst = (struct pf_addr *)&h->ip_dst;
	pd.ip_sum = &h->ip_sum;
	pd.proto = h->ip_p;
	pd.af = AF_INET;
	pd.tos = h->ip_tos;
	pd.tot_len = h->ip_len;

	/* handle fragments that didn't get reassembled by normalization */
	if (h->ip_off & (IP_MF | IP_OFFMASK)) {
		action = pf_test_fragment(&r, dir, ifp, m, h, &pd);
		goto done;
	}

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		struct tcphdr th;
		pd.hdr.tcp = &th;

		if (!pf_pull_hdr(m, off, &th, sizeof(th),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		action = pf_normalize_tcp(dir, ifp, m, 0, off, h, &pd);
		if (action == PF_DROP)
			break;
		action = pf_test_state_tcp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_tcp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr uh;
		pd.hdr.udp = &uh;

		if (!pf_pull_hdr(m, off, &uh, sizeof(uh),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_udp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_udp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp ih;
		pd.hdr.icmp = &ih;

		if (!pf_pull_hdr(m, off, &ih, ICMP_MINLEN,
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			if (r != NULL) {
				r->packets++;
				r->bytes += h->ip_len;
			}
			log = s->log;
		} else if (s == NULL)
			action = pf_test_icmp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	default:
		action = pf_test_state_other(&s, dir, ifp, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_other(&r, dir, ifp, m, h, &pd);
		break;
	}

	if (ifp == status_ifp) {
		pf_status.bcounters[0][dir] += pd.tot_len;
		pf_status.pcounters[0][dir][action]++;
	}

done:
	if (action != PF_DROP && h->ip_hl > 5 &&
	    !((s && s->allow_opts) || (r && r->allow_opts))) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping packet with ip options\n"));
	}

	if (log) {
		if (r == NULL) {
			struct pf_rule r0;
			r0.ifp = ifp;
			r0.action = action;
			r0.nr = -1;
			PFLOG_PACKET(ifp, h, m, AF_INET, dir, reason, &r0);
		} else
			PFLOG_PACKET(ifp, h, m, AF_INET, dir, reason, r);
	}

	/* pf_route can free the mbuf causing *m0 to become NULL */
	if (r && r->rt)
		pf_route(m0, r, dir, ifp);

	return (action);
}
#endif /* INET */

#ifdef INET6
int
pf_test6(int dir, struct ifnet *ifp, struct mbuf **m0)
{
	u_short action, reason = 0, log = 0;
	struct mbuf *m = *m0;
	struct ip6_hdr *h;
	struct pf_rule *r = NULL;
	struct pf_state *s = NULL;
	struct pf_pdesc pd;
	int off, terminal = 0;

	if (!pf_status.running ||
	    (m_tag_find(m, PACKET_TAG_PF_GENERATED, NULL) != NULL))
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	m = *m0;
	h = mtod(m, struct ip6_hdr *);

	pd.src = (struct pf_addr *)&h->ip6_src;
	pd.dst = (struct pf_addr *)&h->ip6_dst;
	pd.ip_sum = NULL;
	pd.af = AF_INET6;
	pd.tos = 0;
	pd.tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);

	off = ((caddr_t)h - m->m_data) + sizeof(struct ip6_hdr);
	pd.proto = h->ip6_nxt;
	do {
		switch (pd.proto) {
		case IPPROTO_FRAGMENT:
			action = pf_test_fragment(&r, dir, ifp, m, h, &pd);
			if (action == PF_DROP)
				REASON_SET(&reason, PFRES_FRAG);
			goto done;
		case IPPROTO_AH:
		case IPPROTO_HOPOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_DSTOPTS: {
			/* get next header and header length */
			struct ip6_ext opt6;

			if (!pf_pull_hdr(m, off, &opt6, sizeof(opt6),
			    NULL, NULL, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short opt\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				log = 1;
				goto done;
			}
			if (pd.proto == IPPROTO_AH)
				off += (opt6.ip6e_len + 2) * 4;
			else
				off += (opt6.ip6e_len + 1) * 8;
			pd.proto = opt6.ip6e_nxt;
			/* goto the next header */
			break;
		}
		default:
			terminal++;
			break;
		}
	} while (!terminal);

	switch (pd.proto) {

	case IPPROTO_TCP: {
		struct tcphdr th;
		pd.hdr.tcp = &th;

		if (!pf_pull_hdr(m, off, &th, sizeof(th),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		action = pf_normalize_tcp(dir, ifp, m, 0, off, h, &pd);
		if (action == PF_DROP)
			break;
		action = pf_test_state_tcp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_tcp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr uh;
		pd.hdr.udp = &uh;

		if (!pf_pull_hdr(m, off, &uh, sizeof(uh),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_udp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_udp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	case IPPROTO_ICMPV6: {
		struct icmp6_hdr ih;
		pd.hdr.icmp6 = &ih;

		if (!pf_pull_hdr(m, off, &ih, sizeof(ih),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, ifp, m, 0, off, h, &pd);
		if (action == PF_PASS) {
			r = s->rule.ptr;
			if (r != NULL) {
				r->packets++;
				r->bytes += h->ip6_plen;
			}
			log = s->log;
		} else if (s == NULL)
			action = pf_test_icmp(&r, dir, ifp, m, 0, off, h, &pd);
		break;
	}

	default:
		action = pf_test_other(&r, dir, ifp, m, h, &pd);
		break;
	}

	if (ifp == status_ifp) {
		pf_status.bcounters[1][dir] += pd.tot_len;
		pf_status.pcounters[1][dir][action]++;
	}

done:
	/* XXX handle IPv6 options, if not allowed. not implemented. */

	if (log) {
		if (r == NULL) {
			struct pf_rule r0;
			r0.ifp = ifp;
			r0.action = action;
			r0.nr = -1;
			PFLOG_PACKET(ifp, h, m, AF_INET6, dir, reason, &r0);
		} else
			PFLOG_PACKET(ifp, h, m, AF_INET6, dir, reason, r);
	}

	/* pf_route6 can free the mbuf causing *m0 to become NULL */
	if (r && r->rt)
		pf_route6(m0, r, dir, ifp);

	return (action);
}
#endif /* INET6 */
