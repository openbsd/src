/*	$OpenBSD: pf.c,v 1.173 2001/11/27 20:29:25 jasoni Exp $ */

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
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

/*
 * Tree data structure
 */

struct pf_tree_node {
	struct pf_tree_key	 key;
	struct pf_state		*state;
	struct pf_tree_node	*parent;
	struct pf_tree_node	*left;
	struct pf_tree_node	*right;
	int			 balance;
};

struct pf_port_node {
	LIST_ENTRY(pf_port_node)	next;
	u_int16_t			port;
};
LIST_HEAD(pf_port_list, pf_port_node);

/* structure for ipsec and ipv6 option header template */
struct _opt6 {
	u_int8_t	opt6_nxt;	/* next header */
	u_int8_t	opt6_hlen;	/* header extension length */
	u_int16_t       _pad;
	u_int32_t       ah_spi;		/* security parameter index
					   for authentication header */
};

/*
 * Global variables
 */

TAILQ_HEAD(pf_natqueue, pf_nat)		pf_nats[2];
TAILQ_HEAD(pf_binatqueue, pf_binat)	pf_binats[2];
TAILQ_HEAD(pf_rdrqueue, pf_rdr)		pf_rdrs[2];
struct pf_rulequeue	 pf_rules[2];
struct pf_rulequeue	*pf_rules_active;
struct pf_rulequeue	*pf_rules_inactive;
struct pf_natqueue	*pf_nats_active;
struct pf_natqueue	*pf_nats_inactive;
struct pf_binatqueue	*pf_binats_active;
struct pf_binatqueue	*pf_binats_inactive;
struct pf_rdrqueue	*pf_rdrs_active;
struct pf_rdrqueue	*pf_rdrs_inactive;
struct pf_tree_node	*tree_lan_ext, *tree_ext_gwy;
struct timeval		 pftv;
struct pf_status	 pf_status;
struct ifnet		*status_ifp;

u_int32_t		 pf_last_purge;
u_int32_t		 ticket_rules_active;
u_int32_t		 ticket_rules_inactive;
u_int32_t		 ticket_nats_active;
u_int32_t		 ticket_nats_inactive;
u_int32_t		 ticket_binats_active;
u_int32_t		 ticket_binats_inactive;
u_int32_t		 ticket_rdrs_active;
u_int32_t		 ticket_rdrs_inactive;
struct pf_port_list	 pf_tcp_ports;
struct pf_port_list	 pf_udp_ports;

/* Timeouts */
int			 pftm_tcp_first_packet = 120;	/* First TCP packet */
int			 pftm_tcp_opening = 30;		/* No response yet */
int			 pftm_tcp_established = 24*60*60;  /* established  */
int			 pftm_tcp_closing = 15 * 60;	/* Half closed */
int			 pftm_tcp_fin_wait = 45;	/* Got both FINs */
int			 pftm_tcp_closed = 90;		/* Got a RST */

int			 pftm_udp_first_packet = 60;	/* First UDP packet */
int			 pftm_udp_single = 30;		/* Unidirectional */
int			 pftm_udp_multiple = 60;	/* Bidirectional */

int			 pftm_icmp_first_packet = 20;	/* First ICMP packet */
int			 pftm_icmp_error_reply = 10;	/* Got error response */

int			 pftm_frag = 30;		/* Fragment expire */

int			 pftm_interval = 10;		/* expire interval */

int			*pftm_timeouts[PFTM_MAX] = { &pftm_tcp_first_packet,
				&pftm_tcp_opening, &pftm_tcp_established,
				&pftm_tcp_closing, &pftm_tcp_fin_wait,
				&pftm_tcp_closed, &pftm_udp_first_packet,
				&pftm_udp_single, &pftm_udp_multiple,
				&pftm_icmp_first_packet, &pftm_icmp_error_reply,
				&pftm_frag, &pftm_interval };


struct pool		 pf_tree_pl, pf_rule_pl, pf_nat_pl, pf_sport_pl;
struct pool		 pf_rdr_pl, pf_state_pl, pf_binat_pl;

int			 pf_tree_key_compare(struct pf_tree_key *,
			    struct pf_tree_key *);
int			 pf_compare_addr(struct pf_addr *, struct pf_addr *,
			    u_int8_t);
void			 pf_addrcpy(struct pf_addr *, struct pf_addr *,
			    u_int8_t);
int			 pf_compare_rules(struct pf_rule *,
			    struct pf_rule *);
int			 pf_compare_nats(struct pf_nat *, struct pf_nat *);
int			 pf_compare_binats(struct pf_binat *, 
			    struct pf_binat *);
int			 pf_compare_rdrs(struct pf_rdr *, struct pf_rdr *);
void			 pf_tree_rotate_left(struct pf_tree_node **);
void			 pf_tree_rotate_right(struct pf_tree_node **);
struct pf_tree_node	*pf_tree_first(struct pf_tree_node *);
struct pf_tree_node	*pf_tree_next(struct pf_tree_node *);
struct pf_tree_node	*pf_tree_search(struct pf_tree_node *,
			    struct pf_tree_key *);
void			 pf_insert_state(struct pf_state *);
void			 pf_purge_expired_states(void);

void			 pf_print_host(struct pf_addr *, u_int16_t, u_int8_t);
void			 pf_print_state(struct pf_state *);
void			 pf_print_flags(u_int8_t);

void			 pfattach(int);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
int			 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);

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
			    struct pf_pdesc *, int);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t, int);
u_int16_t		 pf_map_port_range(struct pf_rdr *, u_int16_t);
struct pf_nat		*pf_get_nat(struct ifnet *, u_int8_t,
			    struct pf_addr *, struct pf_addr *, int);
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
int			 pf_test_state_tcp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_udp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_icmp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    void *, struct pf_pdesc *);
void			*pf_pull_hdr(struct mbuf *, int, void *, int,
			    u_short *, u_short *, int);
void			 pf_calc_skip_steps(struct pf_rulequeue *);

int			 pf_get_sport(u_int8_t, u_int16_t, u_int16_t,
			    u_int16_t *);
void			 pf_put_sport(u_int8_t, u_int16_t);
int			 pf_add_sport(struct pf_port_list *, u_int16_t);
int			 pf_chk_sport(struct pf_port_list *, u_int16_t);
int			 pf_normalize_tcp(int, struct ifnet *, struct mbuf *,
			     int, int, void *, struct pf_pdesc *);
void			 pf_route(struct mbuf *, struct pf_rule *);
void			 pf_route6(struct mbuf *, struct pf_rule *);


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

int
pf_tree_key_compare(struct pf_tree_key *a, struct pf_tree_key *b)
{
	register int diff;

	/*
	 * could use memcmp(), but with the best manual order, we can
	 * minimize the average number of compares. what is faster?
	 */
	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return 1;
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return -1;
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return 1;
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return -1;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return 1;
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return -1;
		if (a->addr[0].addr32[1] > b->addr[0].addr32[1])
			return 1;
		if (a->addr[0].addr32[1] < b->addr[0].addr32[1])
			return -1;
		if (a->addr[0].addr32[2] > b->addr[0].addr32[2])
			return 1;
		if (a->addr[0].addr32[2] < b->addr[0].addr32[2])
			return -1;
		if (a->addr[0].addr32[3] > b->addr[0].addr32[3])
			return 1;
		if (a->addr[0].addr32[3] < b->addr[0].addr32[3])
			return -1;
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return 1;
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return -1;
		if (a->addr[1].addr32[1] > b->addr[1].addr32[1])
			return 1;
		if (a->addr[1].addr32[1] < b->addr[1].addr32[1])
			return -1;
		if (a->addr[1].addr32[2] > b->addr[1].addr32[2])
			return 1;
		if (a->addr[1].addr32[2] < b->addr[1].addr32[2])
			return -1;
		if (a->addr[1].addr32[3] > b->addr[1].addr32[3])
			return 1;
		if (a->addr[1].addr32[3] < b->addr[1].addr32[3])
			return -1;
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
	    a->allow_opts != b->allow_opts)
		return (1);
	if (memcmp(&a->src, &b->src, sizeof(struct pf_rule_addr)))
		return (1);
	if (memcmp(&a->dst, &b->dst, sizeof(struct pf_rule_addr)))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pf_compare_nats(struct pf_nat *a, struct pf_nat *b)
{
	if (a->proto != b->proto ||
	    a->af != b->af ||
	    a->snot != b->snot ||
	    a->dnot != b->dnot ||
	    a->ifnot != b->ifnot)
		return (1);
	if (PF_ANEQ(&a->saddr, &b->saddr, a->af))
		return (1);
	if (PF_ANEQ(&a->smask, &b->smask, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr, &b->daddr, a->af))
		return (1);
	if (PF_ANEQ(&a->dmask, &b->dmask, a->af))
		return (1);
	if (PF_ANEQ(&a->raddr, &b->raddr, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pf_compare_binats(struct pf_binat *a, struct pf_binat *b)
{
	if (PF_ANEQ(&a->saddr, &b->saddr, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr, &b->daddr, a->af))
		return (1);
	if (PF_ANEQ(&a->dmask, &b->dmask, a->af))
		return (1);
	if (PF_ANEQ(&a->raddr, &b->raddr, a->af))
		return (1);
	if (a->proto != b->proto ||
	    a->dnot != b->dnot ||
	    a->af != b->af)
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
	    a->opts != b->opts)
		return (1);
	if (PF_ANEQ(&a->saddr, &b->saddr, a->af))
		return (1);
	if (PF_ANEQ(&a->smask, &b->smask, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr, &b->daddr, a->af))
		return (1);
	if (PF_ANEQ(&a->dmask, &b->dmask, a->af))
		return (1);
	if (PF_ANEQ(&a->raddr, &b->raddr, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

void
pf_tree_rotate_left(struct pf_tree_node **n)
{
	struct pf_tree_node *q = *n, *p = (*n)->parent;

	(*n)->parent = (*n)->right;
	*n = (*n)->right;
	(*n)->parent = p;
	q->right = (*n)->left;
	if (q->right)
		q->right->parent = q;
	(*n)->left = q;
	q->balance--;
	if ((*n)->balance > 0)
		q->balance -= (*n)->balance;
	(*n)->balance--;
	if (q->balance < 0)
		(*n)->balance += q->balance;
}

void
pf_tree_rotate_right(struct pf_tree_node **n)
{
	struct pf_tree_node *q = *n, *p = (*n)->parent;

	(*n)->parent = (*n)->left;
	*n = (*n)->left;
	(*n)->parent = p;
	q->left = (*n)->right;
	if (q->left)
		q->left->parent = q;
	(*n)->right = q;
	q->balance++;
	if ((*n)->balance < 0)
		q->balance -= (*n)->balance;
	(*n)->balance++;
	if (q->balance > 0)
		(*n)->balance += q->balance;
}

int
pf_tree_insert(struct pf_tree_node **n, struct pf_tree_node *p,
    struct pf_tree_key *key, struct pf_state *state)
{
	int deltaH = 0;

	if (*n == NULL) {
		*n = pool_get(&pf_tree_pl, PR_NOWAIT);
		if (*n == NULL)
			return (0);
		bcopy(key, &(*n)->key, sizeof(struct pf_tree_key));
		(*n)->state = state;
		(*n)->balance = 0;
		(*n)->parent = p;
		(*n)->left = (*n)->right = NULL;
		deltaH = 1;
	} else if (pf_tree_key_compare(key, &(*n)->key) > 0) {
		if (pf_tree_insert(&(*n)->right, *n, key, state)) {
			(*n)->balance++;
			if ((*n)->balance == 1)
				deltaH = 1;
			else if ((*n)->balance == 2) {
				if ((*n)->right->balance == -1)
					pf_tree_rotate_right(&(*n)->right);
				pf_tree_rotate_left(n);
			}
		}
	} else {
		if (pf_tree_insert(&(*n)->left, *n, key, state)) {
			(*n)->balance--;
			if ((*n)->balance == -1)
				deltaH = 1;
			else if ((*n)->balance == -2) {
				if ((*n)->left->balance == 1)
					pf_tree_rotate_left(&(*n)->left);
				pf_tree_rotate_right(n);
			}
		}
	}
	return (deltaH);
}

int
pf_tree_remove(struct pf_tree_node **n, struct pf_tree_node *p,
    struct pf_tree_key *key)
{
	int deltaH = 0;
	int c;

	if (*n == NULL)
		return (0);
	c = pf_tree_key_compare(key, &(*n)->key);
	if (c < 0) {
		if (pf_tree_remove(&(*n)->left, *n, key)) {
			(*n)->balance++;
			if ((*n)->balance == 0)
				deltaH = 1;
			else if ((*n)->balance == 2) {
				if ((*n)->right->balance == -1)
					pf_tree_rotate_right(&(*n)->right);
				pf_tree_rotate_left(n);
				if ((*n)->balance == 0)
					deltaH = 1;
			}
		}
	} else if (c > 0) {
		if (pf_tree_remove(&(*n)->right, *n, key)) {
			(*n)->balance--;
			if ((*n)->balance == 0)
				deltaH = 1;
			else if ((*n)->balance == -2) {
				if ((*n)->left->balance == 1)
					pf_tree_rotate_left(&(*n)->left);
				pf_tree_rotate_right(n);
				if ((*n)->balance == 0)
					deltaH = 1;
			}
		}
	} else {
		if ((*n)->right == NULL) {
			struct pf_tree_node *n0 = *n;

			*n = (*n)->left;
			if (*n != NULL)
				(*n)->parent = p;
			pool_put(&pf_tree_pl, n0);
			deltaH = 1;
		} else if ((*n)->left == NULL) {
			struct pf_tree_node *n0 = *n;

			*n = (*n)->right;
			if (*n != NULL)
				(*n)->parent = p;
			pool_put(&pf_tree_pl, n0);
			deltaH = 1;
		} else {
			struct pf_tree_node **qq = &(*n)->left;

			while ((*qq)->right != NULL)
				qq = &(*qq)->right;
			bcopy(&(*qq)->key, &(*n)->key,
			    sizeof(struct pf_tree_key));
			(*n)->state = (*qq)->state;
			bcopy(key, &(*qq)->key, sizeof(struct pf_tree_key));
			if (pf_tree_remove(&(*n)->left, *n, key)) {
				(*n)->balance++;
				if ((*n)->balance == 0)
					deltaH = 1;
				else if ((*n)->balance == 2) {
					if ((*n)->right->balance == -1)
						pf_tree_rotate_right(
						    &(*n)->right);
					pf_tree_rotate_left(n);
					if ((*n)->balance == 0)
						deltaH = 1;
				}
			}
		}
	}
	return (deltaH);
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

	m1.m_next = m;
	m1.m_len = PFLOG_HDRLEN;
	m1.m_data = (char *) &hdr;

	ifn = &(pflogif[0].sc_if);

	if (ifn->if_bpf)
		bpf_mtap(ifn->if_bpf, &m1);
#endif

	return (0);
}

struct pf_tree_node *
pf_tree_first(struct pf_tree_node *n)
{
	if (n == NULL)
		return (NULL);
	while (n->parent)
		n = n->parent;
	while (n->left)
		n = n->left;
	return (n);
}

struct pf_tree_node *
pf_tree_next(struct pf_tree_node *n)
{
	if (n == NULL)
		return (NULL);
	if (n->right) {
		n = n->right;
		while (n->left)
			n = n->left;
	} else {
		if (n->parent && (n == n->parent->left))
			n = n->parent;
		else {
			while (n->parent && (n == n->parent->right))
				n = n->parent;
			n = n->parent;
		}
	}
	return (n);
}

struct pf_tree_node *
pf_tree_search(struct pf_tree_node *n, struct pf_tree_key *key)
{
	int c;

	while (n && (c = pf_tree_key_compare(&n->key, key)))
		if (c > 0)
			n = n->left;
		else
			n = n->right;
	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	return (n);
}

struct pf_state *
pf_find_state(struct pf_tree_node *n, struct pf_tree_key *key)
{
	n = pf_tree_search(n, key);
	if (n)
		return (n->state);
	else
		return (NULL);
}

void
pf_insert_state(struct pf_state *state)
{
	struct pf_tree_key key;
	struct pf_state *s;

	key.af = state->af;
	key.proto = state->proto;
	PF_ACPY(&key.addr[0], &state->lan.addr, state->af);
	key.port[0] = state->lan.port;
	PF_ACPY(&key.addr[1], &state->ext.addr, state->af);
	key.port[1] = state->ext.port;
	/* sanity checks can be removed later, should never occur */
	if ((s = pf_find_state(tree_lan_ext, &key)) != NULL) {
		if (pf_status.debug >= PF_DEBUG_URGENT) {
			printf("pf: ERROR! insert invalid\n");
			printf("    key already in tree_lan_ext\n");
			printf("    key: proto = %u, lan = ", state->proto);
			pf_print_host(&key.addr[0], key.port[0], key.af);
			printf(", ext = ");
			pf_print_host(&key.addr[1], key.port[1], key.af);
			printf("\n    state: ");
			pf_print_state(s);
			printf("\n");
		}
	} else {
		pf_tree_insert(&tree_lan_ext, NULL, &key, state);
		if (pf_find_state(tree_lan_ext, &key) != state)
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf: ERROR! insert failed\n"));
	}

	key.af = state->af;
	key.proto = state->proto;
	PF_ACPY(&key.addr[0], &state->ext.addr, state->af);
	key.port[0] = state->ext.port;
	PF_ACPY(&key.addr[1], &state->gwy.addr, state->af);
	key.port[1] = state->gwy.port;
	if ((s = pf_find_state(tree_ext_gwy, &key)) != NULL) {
		if (pf_status.debug >= PF_DEBUG_URGENT) {
			printf("pf: ERROR! insert invalid\n");
			printf("    key already in tree_ext_gwy\n");
			printf("    key: proto = %u, ext = ", state->proto);
			pf_print_host(&key.addr[0], key.port[0], key.af);
			printf(", gwy = ");
			pf_print_host(&key.addr[1], key.port[1], key.af);
			printf("\n    state: ");
			pf_print_state(s);
			printf("\n");
		}
	} else {
		pf_tree_insert(&tree_ext_gwy, NULL, &key, state);
		if (pf_find_state(tree_ext_gwy, &key) != state)
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf: ERROR! insert failed\n"));
	}
	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
}

void
pf_purge_expired_states(void)
{
	struct pf_tree_node *cur, *next;
	struct pf_tree_key key;

	cur = pf_tree_first(tree_ext_gwy);
	while (cur != NULL) {
		if (cur->state->expire <= pftv.tv_sec) {
			key.af = cur->state->af;
			key.proto = cur->state->proto;
			PF_ACPY(&key.addr[0], &cur->state->lan.addr,
			    cur->state->af);
			key.port[0] = cur->state->lan.port;
			PF_ACPY(&key.addr[1], &cur->state->ext.addr,
			    cur->state->af);
			key.port[1] = cur->state->ext.port;
			/* remove state from second tree */
			if (pf_find_state(tree_lan_ext, &key) != cur->state)
				DPFPRINTF(PF_DEBUG_URGENT,
				    ("pf: ERROR: remove invalid!\n"));
			pf_tree_remove(&tree_lan_ext, NULL, &key);
			if (pf_find_state(tree_lan_ext, &key) != NULL)
				DPFPRINTF(PF_DEBUG_URGENT,
				    ("pf: ERROR: remove failed\n"));
			if (STATE_TRANSLATE(cur->state))
				pf_put_sport(cur->state->proto,
					htons(cur->state->gwy.port));
			/* free state */
			pool_put(&pf_state_pl, cur->state);
			/*
			 * remove state from tree being traversed, use next
			 * state's key to search after removal, since removal
			 * can invalidate pointers.
			 */
			next = pf_tree_next(cur);
			if (next) {
				key = next->key;
				pf_tree_remove(&tree_ext_gwy, NULL, &cur->key);
				cur = pf_tree_search(tree_ext_gwy, &key);
				if (cur == NULL)
					DPFPRINTF(PF_DEBUG_URGENT,
					    ("pf: ERROR: next not found\n"));
			} else {
				pf_tree_remove(&tree_ext_gwy, NULL, &cur->key);
				cur = NULL;
			}
			pf_status.fcounters[FCNT_STATE_REMOVALS]++;
			pf_status.states--;
		} else
			cur = pf_tree_next(cur);
	}
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
}

void
pfattach(int num)
{
	/* XXX - no M_* tags, but they are not used anyway */
	pool_init(&pf_tree_pl, sizeof(struct pf_tree_node), 0, 0, 0, "pftrpl",
	    0, NULL, NULL, 0);
	pool_init(&pf_rule_pl, sizeof(struct pf_rule), 0, 0, 0, "pfrulepl",
	    0, NULL, NULL, 0);
	pool_init(&pf_nat_pl, sizeof(struct pf_nat), 0, 0, 0, "pfnatpl",
	    0, NULL, NULL, 0);
	pool_init(&pf_binat_pl, sizeof(struct pf_binat), 0, 0, 0, "pfbinatpl",
	    0, NULL, NULL, 0);
	pool_init(&pf_rdr_pl, sizeof(struct pf_rdr), 0, 0, 0, "pfrdrpl",
	    0, NULL, NULL, 0);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    0, NULL, NULL, 0);
	pool_init(&pf_sport_pl, sizeof(struct pf_port_node), 0, 0, 0, "pfsport",
	    0, NULL, NULL, 0);

	TAILQ_INIT(&pf_rules[0]);
	TAILQ_INIT(&pf_rules[1]);
	TAILQ_INIT(&pf_nats[0]);
	TAILQ_INIT(&pf_nats[1]);
	TAILQ_INIT(&pf_binats[0]);
	TAILQ_INIT(&pf_binats[1]);
	TAILQ_INIT(&pf_rdrs[0]);
	TAILQ_INIT(&pf_rdrs[1]);
	pf_rules_active = &pf_rules[0];
	pf_rules_inactive = &pf_rules[1];
	pf_nats_active = &pf_nats[0];
	pf_nats_inactive = &pf_nats[1];
	pf_binats_active = &pf_binats[0];
	pf_binats_inactive = &pf_binats[1];
	pf_rdrs_active = &pf_rdrs[0];
	pf_rdrs_inactive = &pf_rdrs[1];

	LIST_INIT(&pf_tcp_ports);
	LIST_INIT(&pf_udp_ports);

	pf_normalize_init();
}

int
pfopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

int
pfclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

int
pfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int error = 0;
	int s;

	if (!(flags & FWRITE))
		return (EACCES);

	/* XXX keep in sync with switch() below */
	if (securelevel > 1)
		switch (cmd) {
		case DIOCSTART:
		case DIOCSTOP:
		case DIOCBEGINRULES:
		case DIOCADDRULE:
		case DIOCCOMMITRULES:
		case DIOCBEGINNATS:
		case DIOCADDNAT:
		case DIOCCOMMITNATS:
		case DIOCBEGINBINATS:
		case DIOCADDBINAT:
		case DIOCCOMMITBINATS:
		case DIOCBEGINRDRS:
		case DIOCADDRDR:
		case DIOCCOMMITRDRS:
		case DIOCCLRSTATES:
		case DIOCCHANGERULE:
		case DIOCCHANGENAT:
		case DIOCCHANGEBINAT:
		case DIOCCHANGERDR:
		case DIOCSETTIMEOUT:
			return EPERM;
		}

	switch (cmd) {

	case DIOCSTART:
		if (pf_status.running)
			error = EEXIST;
		else {
			u_int32_t states = pf_status.states;
			bzero(&pf_status, sizeof(struct pf_status));
			pf_status.running = 1;
			pf_status.states = states;
			microtime(&pftv);
			pf_status.since = pftv.tv_sec;
			DPFPRINTF(PF_DEBUG_MISC, ("pf: started\n"));
		}
		break;

	case DIOCSTOP:
		if (!pf_status.running)
			error = ENOENT;
		else {
			pf_status.running = 0;
			DPFPRINTF(PF_DEBUG_MISC, ("pf: stopped\n"));
		}
		break;

	case DIOCBEGINRULES: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_rule *rule;

		while ((rule = TAILQ_FIRST(pf_rules_inactive)) != NULL) {
			TAILQ_REMOVE(pf_rules_inactive, rule, entries);
			pool_put(&pf_rule_pl, rule);
		}
		*ticket = ++ticket_rules_inactive;
		break;
	}

	case DIOCADDRULE: {
		struct pfioc_rule *pr = (struct pfioc_rule *)addr;
		struct pf_rule *rule, *tail;

		if (pr->ticket != ticket_rules_inactive) {
			error = EBUSY;
			break;
		}
		rule = pool_get(&pf_rule_pl, PR_NOWAIT);
		if (rule == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pr->rule, rule, sizeof(struct pf_rule));
#ifndef INET
		if (rule->af == AF_INET) {
			pool_put(&pf_rule_pl, rule);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (rule->af == AF_INET6) {
			pool_put(&pf_rule_pl, rule);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		tail = TAILQ_LAST(pf_rules_inactive, pf_rulequeue);
		if (tail)
			rule->nr = tail->nr + 1;
		else
			rule->nr = 0;
		rule->ifp = NULL;
		if (rule->ifname[0]) {
			rule->ifp = ifunit(rule->ifname);
			if (rule->ifp == NULL) {
				pool_put(&pf_rule_pl, rule);
				error = EINVAL;
				break;
			}
		} else
			rule->ifp = NULL;
		if (rule->rt_ifname[0]) {
			rule->rt_ifp = ifunit(rule->rt_ifname);
			if (rule->rt_ifname == NULL) {
				pool_put(&pf_rule_pl, rule);
				error = EINVAL;
				break;
			}
		} else
			rule->rt_ifp = NULL;
		rule->evaluations = rule->packets = rule->bytes = 0;
		TAILQ_INSERT_TAIL(pf_rules_inactive, rule, entries);
		break;
	}

	case DIOCCOMMITRULES: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_rulequeue *old_rules;
		struct pf_rule *rule;
		struct pf_tree_node *n;

		if (*ticket != ticket_rules_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap rules, keep the old. */
		s = splsoftnet();
		/*
		 * Rules are about to get freed, clear rule pointers in states
		 */
		for (n = pf_tree_first(tree_ext_gwy); n != NULL;
		    n = pf_tree_next(n))
			n->state->rule = NULL;
		old_rules = pf_rules_active;
		pf_rules_active = pf_rules_inactive;
		pf_rules_inactive = old_rules;
		ticket_rules_active = ticket_rules_inactive;
		pf_calc_skip_steps(pf_rules_active);
		splx(s);

		/* Purge the old rule list. */
		while ((rule = TAILQ_FIRST(old_rules)) != NULL) {
			TAILQ_REMOVE(old_rules, rule, entries);
			pool_put(&pf_rule_pl, rule);
		}
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule *pr = (struct pfioc_rule *)addr;
		struct pf_rule *tail;

		s = splsoftnet();
		tail = TAILQ_LAST(pf_rules_active, pf_rulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ticket_rules_active;
		splx(s);
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule *pr = (struct pfioc_rule *)addr;
		struct pf_rule *rule;

		if (pr->ticket != ticket_rules_active) {
			error = EBUSY;
			break;
		}
		s = splsoftnet();
		rule = TAILQ_FIRST(pf_rules_active);
		while ((rule != NULL) && (rule->nr != pr->nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(rule, &pr->rule, sizeof(struct pf_rule));
		splx(s);
		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_changerule *pcr = (struct pfioc_changerule *)addr;
		struct pf_rule *oldrule = NULL, *newrule = NULL;
		u_int32_t nr = 0;

		if (pcr->action < PF_CHANGE_ADD_HEAD ||
		    pcr->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		if (pcr->action != PF_CHANGE_REMOVE) {
			newrule = pool_get(&pf_rule_pl, PR_NOWAIT);
			if (newrule == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcr->newrule, newrule, sizeof(struct pf_rule));
#ifndef INET
			if (newrule->af == AF_INET) {
				pool_put(&pf_rule_pl, newrule);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (newrule->af == AF_INET6) {
				pool_put(&pf_rule_pl, newrule);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			newrule->ifp = NULL;
			if (newrule->ifname[0]) {
				newrule->ifp = ifunit(newrule->ifname);
				if (newrule->ifp == NULL) {
					pool_put(&pf_rule_pl, newrule);
					error = EINVAL;
					break;
				}
			}
			newrule->evaluations = newrule->packets = 0;
			newrule->bytes = 0;
		}

		s = splsoftnet();

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(pf_rules_active);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(pf_rules_active, pf_rulequeue);
		else {
			oldrule = TAILQ_FIRST(pf_rules_active);
			while ((oldrule != NULL) && pf_compare_rules(oldrule,
			    &pcr->oldrule))
				oldrule = TAILQ_NEXT(oldrule, entries);
			if (oldrule == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE) {
			struct pf_tree_node *n;

			for (n = pf_tree_first(tree_ext_gwy); n != NULL;
			    n = pf_tree_next(n))
				if (n->state->rule == oldrule)
					n->state->rule = NULL;
			TAILQ_REMOVE(pf_rules_active, oldrule, entries);
			pool_put(&pf_rule_pl, oldrule);
		} else {
			if (oldrule == NULL)
				TAILQ_INSERT_TAIL(pf_rules_active, newrule,
				    entries);
			else if (pcr->action == PF_CHANGE_ADD_HEAD ||
			    pcr->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldrule, newrule, entries);
			else
				TAILQ_INSERT_AFTER(pf_rules_active, oldrule,
				    newrule, entries);
		}

		TAILQ_FOREACH(oldrule, pf_rules_active, entries)
			oldrule->nr = nr++;

		pf_calc_skip_steps(pf_rules_active);

		ticket_rules_active++;
		splx(s);
		break;
	}

	case DIOCBEGINNATS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_nat *nat;

		while ((nat = TAILQ_FIRST(pf_nats_inactive)) != NULL) {
			TAILQ_REMOVE(pf_nats_inactive, nat, entries);
			pool_put(&pf_nat_pl, nat);
		}
		*ticket = ++ticket_nats_inactive;
		break;
	}

	case DIOCADDNAT: {
		struct pfioc_nat *pn = (struct pfioc_nat *)addr;
		struct pf_nat *nat;

		if (pn->ticket != ticket_nats_inactive) {
			error = EBUSY;
			break;
		}
		nat = pool_get(&pf_nat_pl, PR_NOWAIT);
		if (nat == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pn->nat, nat, sizeof(struct pf_nat));
#ifndef INET
		if (nat->af == AF_INET) {
			pool_put(&pf_nat_pl, nat);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (nat->af == AF_INET6) {
			pool_put(&pf_nat_pl, nat);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		if (nat->ifname[0]) {
			nat->ifp = ifunit(nat->ifname);
			if (nat->ifp == NULL) {
				pool_put(&pf_nat_pl, nat);
				error = EINVAL;
				break;
			}
		} else
			nat->ifp = NULL;
		TAILQ_INSERT_TAIL(pf_nats_inactive, nat, entries);
		break;
	}

	case DIOCCOMMITNATS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_natqueue *old_nats;
		struct pf_nat *nat;

		if (*ticket != ticket_nats_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap nats, keep the old. */
		s = splsoftnet();
		old_nats = pf_nats_active;
		pf_nats_active = pf_nats_inactive;
		pf_nats_inactive = old_nats;
		ticket_nats_active = ticket_nats_inactive;
		splx(s);

		/* Purge the old nat list */
		while ((nat = TAILQ_FIRST(old_nats)) != NULL) {
			TAILQ_REMOVE(old_nats, nat, entries);
			pool_put(&pf_nat_pl, nat);
		}
		break;
	}

	case DIOCGETNATS: {
		struct pfioc_nat *pn = (struct pfioc_nat *)addr;
		struct pf_nat *nat;

		pn->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(nat, pf_nats_active, entries)
			pn->nr++;
		pn->ticket = ticket_nats_active;
		splx(s);
		break;
	}

	case DIOCGETNAT: {
		struct pfioc_nat *pn = (struct pfioc_nat *)addr;
		struct pf_nat *nat;
		u_int32_t nr;

		if (pn->ticket != ticket_nats_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		s = splsoftnet();
		nat = TAILQ_FIRST(pf_nats_active);
		while ((nat != NULL) && (nr < pn->nr)) {
			nat = TAILQ_NEXT(nat, entries);
			nr++;
		}
		if (nat == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(nat, &pn->nat, sizeof(struct pf_nat));
		splx(s);
		break;
	}

	case DIOCCHANGENAT: {
		struct pfioc_changenat *pcn = (struct pfioc_changenat *)addr;
		struct pf_nat *oldnat = NULL, *newnat = NULL;

		if (pcn->action < PF_CHANGE_ADD_HEAD ||
		    pcn->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		if (pcn->action != PF_CHANGE_REMOVE) {
			newnat = pool_get(&pf_nat_pl, PR_NOWAIT);
			if (newnat == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcn->newnat, newnat, sizeof(struct pf_nat));
#ifndef INET
			if (newnat->af == AF_INET) {
				pool_put(&pf_nat_pl, newnat);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (newnat->af == AF_INET6) {
				pool_put(&pf_nat_pl, newnat);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			newnat->ifp = NULL;
			if (newnat->ifname[0]) {
				newnat->ifp = ifunit(newnat->ifname);
				if (newnat->ifp == NULL) {
					pool_put(&pf_nat_pl, newnat);
					error = EINVAL;
					break;
				}
			}
		}

		s = splsoftnet();

		if (pcn->action == PF_CHANGE_ADD_HEAD)
			oldnat = TAILQ_FIRST(pf_nats_active);
		else if (pcn->action == PF_CHANGE_ADD_TAIL)
			oldnat = TAILQ_LAST(pf_nats_active, pf_natqueue);
		else {
			oldnat = TAILQ_FIRST(pf_nats_active);
			while ((oldnat != NULL) && pf_compare_nats(oldnat,
			    &pcn->oldnat))
				oldnat = TAILQ_NEXT(oldnat, entries);
			if (oldnat == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(pf_nats_active, oldnat, entries);
			pool_put(&pf_nat_pl, oldnat);
		} else {
			if (oldnat == NULL)
				TAILQ_INSERT_TAIL(pf_nats_active, newnat,
				    entries);
			else if (pcn->action == PF_CHANGE_ADD_HEAD ||
			    pcn->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldnat, newnat, entries);
			else
				TAILQ_INSERT_AFTER(pf_nats_active, oldnat,
				     newnat, entries);
		}

		ticket_nats_active++;
		splx(s);
		break;
	}

	case DIOCBEGINBINATS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_binat *binat;

		while ((binat = TAILQ_FIRST(pf_binats_inactive)) != NULL) {
			TAILQ_REMOVE(pf_binats_inactive, binat, entries);
			pool_put(&pf_binat_pl, binat);
		}
		*ticket = ++ticket_binats_inactive;
		break;
	}

	case DIOCADDBINAT: {
		struct pfioc_binat *pb = (struct pfioc_binat *)addr;
		struct pf_binat *binat;

		if (pb->ticket != ticket_binats_inactive) {
			error = EBUSY;
			break;
		}
		binat = pool_get(&pf_binat_pl, PR_NOWAIT);
		if (binat == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pb->binat, binat, sizeof(struct pf_binat));
#ifndef INET
		if (binat->af == AF_INET) {
			pool_put(&pf_binat_pl, binat);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (binat->af == AF_INET6) {
			pool_put(&pf_binat_pl, binat);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		if (binat->ifname[0]) {
			binat->ifp = ifunit(binat->ifname);
			if (binat->ifp == NULL) {
				pool_put(&pf_binat_pl, binat);
				error = EINVAL;
				break;
			}
		} else
			binat->ifp = NULL;
		TAILQ_INSERT_TAIL(pf_binats_inactive, binat, entries);
		break;
	}

	case DIOCCOMMITBINATS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_binatqueue *old_binats;
		struct pf_binat *binat;

		if (*ticket != ticket_binats_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap binats, keep the old. */
		s = splsoftnet();
		old_binats = pf_binats_active;
		pf_binats_active = pf_binats_inactive;
		pf_binats_inactive = old_binats;
		ticket_binats_active = ticket_binats_inactive;
		splx(s);

		/* Purge the old binat list */
		while ((binat = TAILQ_FIRST(old_binats)) != NULL) {
			TAILQ_REMOVE(old_binats, binat, entries);
			pool_put(&pf_binat_pl, binat);
		}
		break;
	}

	case DIOCGETBINATS: {
		struct pfioc_binat *pb = (struct pfioc_binat *)addr;
		struct pf_binat *binat;

		pb->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(binat, pf_binats_active, entries)
			pb->nr++;
		pb->ticket = ticket_binats_active;
		splx(s);
		break;
	}

	case DIOCGETBINAT: {
		struct pfioc_binat *pb = (struct pfioc_binat *)addr;
		struct pf_binat *binat;
		u_int32_t nr;

		if (pb->ticket != ticket_binats_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		s = splsoftnet();
		binat = TAILQ_FIRST(pf_binats_active);
		while ((binat != NULL) && (nr < pb->nr)) {
			binat = TAILQ_NEXT(binat, entries);
			nr++;
		}
		if (binat == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(binat, &pb->binat, sizeof(struct pf_binat));
		splx(s);
		break;
	}

	case DIOCCHANGEBINAT: {
		struct pfioc_changebinat *pcn = (struct pfioc_changebinat *)addr;
		struct pf_binat *oldbinat = NULL, *newbinat = NULL;

		if (pcn->action < PF_CHANGE_ADD_HEAD ||
		    pcn->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		if (pcn->action != PF_CHANGE_REMOVE) {
			newbinat = pool_get(&pf_binat_pl, PR_NOWAIT);
			if (newbinat == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcn->newbinat, newbinat,
				sizeof(struct pf_binat));
#ifndef INET
			if (newbinat->af == AF_INET) {
				pool_put(&pf_binat_pl, newbinat);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (newbinat->af == AF_INET6) {
				pool_put(&pf_binat_pl, newbinat);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			newbinat->ifp = NULL;
			if (newbinat->ifname[0]) {
				newbinat->ifp = ifunit(newbinat->ifname);
				if (newbinat->ifp == NULL) {
					pool_put(&pf_binat_pl, newbinat);
					error = EINVAL;
					break;
				}
			}
		}

		s = splsoftnet();

		if (pcn->action == PF_CHANGE_ADD_HEAD)
			oldbinat = TAILQ_FIRST(pf_binats_active);
		else if (pcn->action == PF_CHANGE_ADD_TAIL)
			oldbinat = TAILQ_LAST(pf_binats_active, pf_binatqueue);
		else {
			oldbinat = TAILQ_FIRST(pf_binats_active);
			while ((oldbinat != NULL) && pf_compare_binats(oldbinat,
			    &pcn->oldbinat))
				oldbinat = TAILQ_NEXT(oldbinat, entries);
			if (oldbinat == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(pf_binats_active, oldbinat, entries);
			pool_put(&pf_binat_pl, oldbinat);
		} else {
			if (oldbinat == NULL)
				TAILQ_INSERT_TAIL(pf_binats_active, newbinat,
				    entries);
			else if (pcn->action == PF_CHANGE_ADD_HEAD ||
			    pcn->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldbinat, newbinat, 
				    entries);
			else
				TAILQ_INSERT_AFTER(pf_binats_active, oldbinat,
				     newbinat, entries);
		}

		ticket_binats_active++;
		splx(s);
		break;
	}

	case DIOCBEGINRDRS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_rdr *rdr;

		while ((rdr = TAILQ_FIRST(pf_rdrs_inactive)) != NULL) {
			TAILQ_REMOVE(pf_rdrs_inactive, rdr, entries);
			pool_put(&pf_rdr_pl, rdr);
		}
		*ticket = ++ticket_rdrs_inactive;
		break;
	}

	case DIOCADDRDR: {
		struct pfioc_rdr *pr = (struct pfioc_rdr *)addr;
		struct pf_rdr *rdr;

		if (pr->ticket != ticket_rdrs_inactive) {
			error = EBUSY;
			break;
		}
		rdr = pool_get(&pf_rdr_pl, PR_NOWAIT);
		if (rdr == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pr->rdr, rdr, sizeof(struct pf_rdr));
#ifndef INET
		if (rdr->af == AF_INET) {
			pool_put(&pf_rdr_pl, rdr);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (rdr->af == AF_INET6) {
			pool_put(&pf_rdr_pl, rdr);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		if (rdr->ifname[0]) {
			rdr->ifp = ifunit(rdr->ifname);
			if (rdr->ifp == NULL) {
				pool_put(&pf_rdr_pl, rdr);
				error = EINVAL;
				break;
			}
		} else
			rdr->ifp = NULL;
		TAILQ_INSERT_TAIL(pf_rdrs_inactive, rdr, entries);
		break;
	}

	case DIOCCOMMITRDRS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_rdrqueue *old_rdrs;
		struct pf_rdr *rdr;

		if (*ticket != ticket_rdrs_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap rdrs, keep the old. */
		s = splsoftnet();
		old_rdrs = pf_rdrs_active;
		pf_rdrs_active = pf_rdrs_inactive;
		pf_rdrs_inactive = old_rdrs;
		ticket_rdrs_active = ticket_rdrs_inactive;
		splx(s);

		/* Purge the old rdr list */
		while ((rdr = TAILQ_FIRST(old_rdrs)) != NULL) {
			TAILQ_REMOVE(old_rdrs, rdr, entries);
			pool_put(&pf_rdr_pl, rdr);
		}
		break;
	}

	case DIOCGETRDRS: {
		struct pfioc_rdr *pr = (struct pfioc_rdr *)addr;
		struct pf_rdr *rdr;

		pr->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(rdr, pf_rdrs_active, entries)
			pr->nr++;
		pr->ticket = ticket_rdrs_active;
		splx(s);
		break;
	}

	case DIOCGETRDR: {
		struct pfioc_rdr *pr = (struct pfioc_rdr *)addr;
		struct pf_rdr *rdr;
		u_int32_t nr;

		if (pr->ticket != ticket_rdrs_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		s = splsoftnet();
		rdr = TAILQ_FIRST(pf_rdrs_active);
		while ((rdr != NULL) && (nr < pr->nr)) {
			rdr = TAILQ_NEXT(rdr, entries);
			nr++;
		}
		if (rdr == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(rdr, &pr->rdr, sizeof(struct pf_rdr));
		splx(s);
		break;
	}

	case DIOCCHANGERDR: {
		struct pfioc_changerdr *pcn = (struct pfioc_changerdr *)addr;
		struct pf_rdr *oldrdr = NULL, *newrdr = NULL;

		if (pcn->action < PF_CHANGE_ADD_HEAD ||
		    pcn->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		if (pcn->action != PF_CHANGE_REMOVE) {
			newrdr = pool_get(&pf_rdr_pl, PR_NOWAIT);
			if (newrdr == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcn->newrdr, newrdr, sizeof(struct pf_rdr));
#ifndef INET
			if (newrdr->af == AF_INET) {
				pool_put(&pf_rdr_pl, newrdr);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (newrdr->af == AF_INET6) {
				pool_put(&pf_rdr_pl, newrdr);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			newrdr->ifp = NULL;
			if (newrdr->ifname[0]) {
				newrdr->ifp = ifunit(newrdr->ifname);
				if (newrdr->ifp == NULL) {
					pool_put(&pf_rdr_pl, newrdr);
					error = EINVAL;
					break;
				}
			}
		}

		s = splsoftnet();

		if (pcn->action == PF_CHANGE_ADD_HEAD)
			oldrdr = TAILQ_FIRST(pf_rdrs_active);
		else if (pcn->action == PF_CHANGE_ADD_TAIL)
			oldrdr = TAILQ_LAST(pf_rdrs_active, pf_rdrqueue);
		else {
			oldrdr = TAILQ_FIRST(pf_rdrs_active);
			while ((oldrdr != NULL) && pf_compare_rdrs(oldrdr,
			    &pcn->oldrdr))
				oldrdr = TAILQ_NEXT(oldrdr, entries);
			if (oldrdr == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(pf_rdrs_active, oldrdr, entries);
			pool_put(&pf_rdr_pl, oldrdr);
		} else {
			if (oldrdr == NULL)
				TAILQ_INSERT_TAIL(pf_rdrs_active, newrdr,
				    entries);
			else if (pcn->action == PF_CHANGE_ADD_HEAD ||
			    pcn->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldrdr, newrdr, entries);
			else
				TAILQ_INSERT_AFTER(pf_rdrs_active, oldrdr,
				     newrdr, entries);
		}

		ticket_rdrs_active++;
		splx(s);
		break;
	}

	case DIOCCLRSTATES: {
		struct pf_tree_node *n;

		s = splsoftnet();
		for (n = pf_tree_first(tree_ext_gwy); n != NULL;
		    n = pf_tree_next(n))
			n->state->expire = 0;
		pf_purge_expired_states();
		pf_status.states = 0;
		splx(s);
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state *ps = (struct pfioc_state *)addr;
		struct pf_tree_node *n;
		u_int32_t nr;

		nr = 0;
		s = splsoftnet();
		n = pf_tree_first(tree_ext_gwy);
		while ((n != NULL) && (nr < ps->nr)) {
			n = pf_tree_next(n);
			nr++;
		}
		if (n == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(n->state, &ps->state, sizeof(struct pf_state));
		splx(s);
		microtime(&pftv);
		ps->state.creation = pftv.tv_sec - ps->state.creation;
		if (ps->state.expire <= pftv.tv_sec)
			ps->state.expire = 0;
		else
			ps->state.expire -= pftv.tv_sec;
		break;
	}

	case DIOCGETSTATES: {
		struct pfioc_states *ps = (struct pfioc_states *)addr;
		struct pf_tree_node *n;
		struct pf_state *p, pstore;
		u_int32_t nr = 0;
		int space = ps->ps_len;

		if (space == 0) {
			s = splsoftnet();
			n = pf_tree_first(tree_ext_gwy);
			while (n != NULL) {
				n = pf_tree_next(n);
				nr++;
			}
			splx(s);
			ps->ps_len = sizeof(struct pf_state) * nr;
			return (0);
		}

		microtime(&pftv);
		s = splsoftnet();
		p = ps->ps_states;
		n = pf_tree_first(tree_ext_gwy);
		while (n && (nr + 1) * sizeof(*p) <= ps->ps_len) {
			bcopy(n->state, &pstore, sizeof(pstore));
			pstore.creation = pftv.tv_sec - pstore.creation;
			if (pstore.expire <= pftv.tv_sec)
				pstore.expire = 0;
			else
				pstore.expire -= pftv.tv_sec;
			error = copyout(&pstore, p, sizeof(*p));
			if (error) {
				splx(s);
				goto fail;
			}
			p++;
			nr++;
			n = pf_tree_next(n);
		}
		ps->ps_len = sizeof(struct pf_state) * nr;
		splx(s);
		break;
	}

	case DIOCSETSTATUSIF: {
		struct pfioc_if *pi = (struct pfioc_if *)addr;
		struct ifnet *ifp;

		if ((ifp = ifunit(pi->ifname)) == NULL)
			error = EINVAL;
		else
			status_ifp = ifp;
		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;
		bcopy(&pf_status, s, sizeof(struct pf_status));
		break;
	}

	case DIOCCLRSTATUS: {
		u_int8_t running = pf_status.running;
		u_int32_t states = pf_status.states;

		bzero(&pf_status, sizeof(struct pf_status));
		pf_status.running = running;
		pf_status.states = states;
		break;
	}

	case DIOCNATLOOK: {
		struct pfioc_natlook *pnl = (struct pfioc_natlook *)addr;
		struct pf_state *st;
		struct pf_tree_key key;
		int direction = pnl->direction;

		key.af = pnl->af;
		key.proto = pnl->proto;

		/*
		 * userland gives us source and dest of connetion, reverse
		 * the lookup so we ask for what happens with the return
		 * traffic, enabling us to find it in the state tree.
		 */
		PF_ACPY(&key.addr[1], &pnl->saddr, pnl->af);
		key.port[1] = pnl->sport;
		PF_ACPY(&key.addr[0], &pnl->daddr, pnl->af);
		key.port[0] = pnl->dport;

		if (!pnl->proto || 
		    PF_AZERO(&pnl->saddr, pnl->af) ||
		    PF_AZERO(&pnl->daddr, pnl->af) ||
		    !pnl->dport || !pnl->sport)
			error = EINVAL;
		else {
			s = splsoftnet();
			if (direction == PF_IN)
				st = pf_find_state(tree_ext_gwy, &key);
			else
				st = pf_find_state(tree_lan_ext, &key);
			if (st != NULL) {
				if (direction  == PF_IN) {
					PF_ACPY(&pnl->rsaddr, &st->lan.addr,
					    st->af);
					pnl->rsport = st->lan.port;
					PF_ACPY(&pnl->rdaddr, &pnl->daddr,
					    pnl->af);
					pnl->rdport = pnl->dport;
				} else {
					PF_ACPY(&pnl->rdaddr, &st->gwy.addr,
					    st->af);
					pnl->rdport = st->gwy.port;
					PF_ACPY(&pnl->rsaddr, &pnl->saddr,
					    pnl->af);
					pnl->rsport = pnl->sport;
				}
			} else
				error = ENOENT;
			splx(s);
		}
		break;
	}

	case DIOCSETTIMEOUT: {
		struct pfioc_tm *pt = (struct pfioc_tm *)addr;
		int old;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX ||
		    pt->seconds < 0) {
			error = EINVAL;
			goto fail;
		}
		old = *pftm_timeouts[pt->timeout];
		*pftm_timeouts[pt->timeout] = pt->seconds;
		pt->seconds = old;
		break;
	}

	case DIOCGETTIMEOUT: {
		struct pfioc_tm *pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX) {
			error = EINVAL;
			goto fail;
		}
		pt->seconds = *pftm_timeouts[pt->timeout];
		break;
	}

	case DIOCSETDEBUG: {
		u_int32_t *level = (u_int32_t *)addr;
		pf_status.debug = *level;
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:

	return (error);
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
			PF_CALC_SKIP_STEP(PF_SKIP_IFP, s->ifp == r->ifp);
			PF_CALC_SKIP_STEP(PF_SKIP_AF, s->af == r->af);
			PF_CALC_SKIP_STEP(PF_SKIP_PROTO, s->proto == r->proto);
			PF_CALC_SKIP_STEP(PF_SKIP_SRC_ADDR,
			    PF_AEQ(&s->src.addr, &r->src.addr, r->af) &&
			    PF_AEQ(&s->src.mask, &r->src.mask, r->af) &&
			    s->src.not == r->src.not);
			PF_CALC_SKIP_STEP(PF_SKIP_SRC_PORT,
			    s->src.port[0] == r->src.port[0] &&	
			    s->src.port[1] == r->src.port[1] &&
			    s->src.port_op == r->src.port_op);
			PF_CALC_SKIP_STEP(PF_SKIP_DST_ADDR,
			    PF_AEQ(&s->dst.addr, &r->dst.addr, r->af) &&
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
		return 0x0000;
	l = cksum + old - new;
	l = (l >> 16) + (l & 65535);
	l = l & 65535;
	if (udp && !l)
		return 0xFFFF;
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
pf_send_reset(int off, struct tcphdr *th, struct pf_pdesc *pd, int af)
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
		h2->ip_ttl = 128;
		h2->ip_sum = 0;
		h2->ip_len = len;
		h2->ip_off = 0;
		ip_output(m, NULL, NULL, 0, NULL, NULL);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* TCP checksum */
		th2->th_sum = in6_cksum(m, IPPROTO_TCP,
		    sizeof(struct ip6_hdr), sizeof(*th));

		h2_6->ip6_vfc |= IPV6_VERSION;
		h2_6->ip6_hlim = 128;

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
pf_match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	NTOHS(a1);
	NTOHS(a2);
	NTOHS(p);
	switch (op) {
	case PF_OP_IRG:
		return (p > a1) && (p < a2);
	case PF_OP_XRG:
		return (p < a1) || (p > a2);
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
pf_chk_sport(struct pf_port_list *plist, u_int16_t port)
{
	struct pf_port_node	*pnode;

	LIST_FOREACH(pnode, plist, next) {
		if (pnode->port == port)
			return (1);
	}

	return (0);
}

int
pf_add_sport(struct pf_port_list *plist, u_int16_t port)
{
	struct pf_port_node *pnode;

	pnode = pool_get(&pf_sport_pl, M_NOWAIT);
	if (pnode == NULL)
		return (ENOMEM);

	pnode->port = port;
	LIST_INSERT_HEAD(plist, pnode, next);

	return (0);
}

void
pf_put_sport(u_int8_t proto, u_int16_t port)
{
	struct pf_port_list	*plist;
	struct pf_port_node	*pnode;

	if (proto == IPPROTO_TCP)
		plist = &pf_tcp_ports;
	else if (proto == IPPROTO_UDP)
		plist = &pf_udp_ports;
	else
		return;

	LIST_FOREACH(pnode, plist, next) {
		if (pnode->port == port) {
			LIST_REMOVE(pnode, next);
			pool_put(&pf_sport_pl, pnode);
			break;
		}
	}
}

int
pf_get_sport(u_int8_t proto, u_int16_t low, u_int16_t high, u_int16_t *port)
{
	struct pf_port_list	*plist;
	int			step;
	u_int16_t		cut;

	if (proto == IPPROTO_TCP)
		plist = &pf_tcp_ports;
	else if (proto == IPPROTO_UDP)
		plist = &pf_udp_ports;
	else
		return (EINVAL);

	/* port search; start random, step; similar 2 portloop in in_pcbbind */
	if (low == high) {
		*port = low;
		if (!pf_chk_sport(plist, *port))
			goto found;
		return (1);
	} else if (low < high) {
		step = 1;
		cut = arc4random() % (high - low) + low;
	} else {
		step = -1;
		cut = arc4random() % (low - high) + high;
	}

	*port = cut - step;
	do {
		*port += step;
		if (!pf_chk_sport(plist, *port))
			goto found;
	} while (*port != low && *port != high);

	step = -step;
	*port = cut;
	do {
		*port += step;
		if (!pf_chk_sport(plist, *port))
			goto found;
	} while (*port != low && *port != high);

	return (1);					/* none available */

found:
	return (pf_add_sport(plist, *port));
}

struct pf_nat *
pf_get_nat(struct ifnet *ifp, u_int8_t proto, struct pf_addr *saddr,
    struct pf_addr *daddr, int af)
{
	struct pf_nat *n, *nm = NULL;

	n = TAILQ_FIRST(pf_nats_active);
	while (n && nm == NULL) {
		if (((n->ifp == NULL) || (n->ifp == ifp && !n->ifnot) ||
		    (n->ifp != ifp && n->ifnot)) &&
		    (!n->proto || n->proto == proto) &&
		    (!n->af || n->af == af) &&
		    PF_MATCHA(n->snot, &n->saddr, &n->smask, saddr, af) &&
		    PF_MATCHA(n->dnot, &n->daddr, &n->dmask, daddr, af))
			nm = n;
		else
			n = TAILQ_NEXT(n, entries);
	}
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
		    PF_MATCHA(0, &b->saddr, &fullmask, saddr, af) &&
		    PF_MATCHA(b->dnot, &b->daddr, &b->dmask, daddr, af))
			bm = b;
                else if (direction == PF_IN && b->ifp == ifp &&
		    (!b->proto || b->proto == proto) &&
		    (!b->af || b->af == af) &&
		    PF_MATCHA(0, &b->raddr, &fullmask, saddr, af) &&
		    PF_MATCHA(b->dnot, &b->daddr, &b->dmask, daddr, af))
			bm = b;
		else
			b = TAILQ_NEXT(b, entries);
	}
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
		    PF_MATCHA(r->snot, &r->saddr, &r->smask, saddr, af) &&
		    PF_MATCHA(r->dnot, &r->daddr, &r->dmask, daddr, af) &&
		    ((!r->dport2 && dport == r->dport) ||
		    (r->dport2 && (ntohs(dport) >= ntohs(r->dport)) &&
		    ntohs(dport) <= ntohs(r->dport2))))
			rm = r;
		else
			r = TAILQ_NEXT(r, entries);
	}
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
	return htons((u_int16_t)nport);
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
	struct pf_rule *r;
	u_int16_t bport, nport = 0, af = pd->af;
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
			    &th->th_sum, &binat->raddr, th->th_sport, 0, af);
			rewrite++;
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, IPPROTO_TCP,
		    saddr, daddr, af)) != NULL) {
			bport = th->th_sport;
			error = pf_get_sport(IPPROTO_TCP, 50001,
			    65535, &nport);
			if (error)
				return (PF_DROP);
			PF_ACPY(&baddr, saddr, af);
			pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &nat->raddr, htons(nport), 0, af);
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_TCP, saddr, daddr,
		    th->th_dport, af)) != NULL) {
			bport = th->th_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, th->th_dport);
			else
				nport = rdr->rport;
			PF_ACPY(&baddr, daddr, af);
			pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &rdr->raddr, nport, 0, af);
			rewrite++;
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_TCP,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			bport = th->th_dport;
			pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &binat->saddr, th->th_dport, 0, af);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action == PF_SCRUB) {
			r = TAILQ_NEXT(r, entries);
			continue;
		}
		r->evaluations++;
		if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != IPPROTO_TCP)
			r = r->skip[PF_SKIP_PROTO];
		else if (!PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr, &r->src.mask, saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT];
		else if (!PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr, &r->dst.mask, daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT];
		else if (r->direction != direction)
			r = TAILQ_NEXT(r, entries);
		else if ((r->flagset & th->th_flags) != r->flags)
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

		/* XXX will log packet before rewrite */
		if ((*rm)->log)
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);

		if (((*rm)->action == PF_DROP) &&
		    (((*rm)->rule_flag & PFRULE_RETURNRST) ||
		    (*rm)->return_icmp)) {
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
			if ((*rm)->rule_flag & PFRULE_RETURNRST)
				pf_send_reset(off, th, pd, af);
			else 
				pf_send_icmp(m, (*rm)->return_icmp >> 8,
				    (*rm)->return_icmp & 255, af);
		}

		if ((*rm)->action == PF_DROP) {
			if (nport && nat != NULL)
				pf_put_sport(IPPROTO_TCP, nport);
			return (PF_DROP);
		}
	}

	if (((*rm != NULL) && (*rm)->keep_state) || nat != NULL ||
	    binat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = pd->tot_len - off - (th->th_off << 2);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			if (nport && nat != NULL)
				pf_put_sport(IPPROTO_TCP, nport);
			return (PF_DROP);
		}

		s->rule = *rm;
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
		if (th->th_flags == TH_SYN && *rm != NULL
		    && (*rm)->keep_state == PF_STATE_MODULATE) {
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
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + pftm_tcp_first_packet;
		s->packets = 1;
		s->bytes = pd->tot_len;
		pf_insert_state(s);
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
			    &uh->uh_sum, &binat->raddr, uh->uh_sport, 1, af);
			rewrite++;
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, IPPROTO_UDP,
		    saddr, daddr, af)) != NULL) {
			bport = uh->uh_sport;
			error = pf_get_sport(IPPROTO_UDP, 50001,
			    65535, &nport);
			if (error)
				return (PF_DROP);
			PF_ACPY(&baddr, saddr, af);
			pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &nat->raddr, htons(nport), 1, af);
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_UDP, saddr, daddr, 
		    uh->uh_dport, af)) != NULL) {
			bport = uh->uh_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, uh->uh_dport);
			else
				nport = rdr->rport;

			PF_ACPY(&baddr, daddr, af);
			pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &rdr->raddr, nport, 1, af);
			rewrite++;
		}
		/* check incoming packet for BINAT */
		else if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_UDP,
		    daddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			bport = uh->uh_dport;
			pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &binat->saddr, uh->uh_dport, 1, af);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action == PF_SCRUB) {
			r = TAILQ_NEXT(r, entries);
			continue;
		}
		r->evaluations++;

		if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != IPPROTO_UDP)
			r = r->skip[PF_SKIP_PROTO];
		else if (!PF_AZERO(&r->src.mask, af) &&
		    !PF_MATCHA(r->src.not, &r->src.addr, &r->src.mask,
			saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
                    r->src.port[0], r->src.port[1], uh->uh_sport))
                        r = r->skip[PF_SKIP_SRC_PORT];
		else if (!PF_AZERO(&r->dst.mask, af) &&
		    !PF_MATCHA(r->dst.not, &r->dst.addr, &r->dst.mask,
			daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], uh->uh_dport))
			r = r->skip[PF_SKIP_DST_PORT];
		else if (r->direction != direction)
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

		/* XXX will log packet before rewrite */
		if ((*rm)->log)
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);

		if (((*rm)->action == PF_DROP) && (*rm)->return_icmp) {
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
			pf_send_icmp(m, (*rm)->return_icmp >> 8,
			    (*rm)->return_icmp & 255, af);
		}

		if ((*rm)->action == PF_DROP) {
			if (nport && nat != NULL)
				pf_put_sport(IPPROTO_UDP, nport);
			return (PF_DROP);
		}
	}

	if ((*rm != NULL && (*rm)->keep_state) || nat != NULL ||
	    binat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = pd->tot_len - off - sizeof(*uh);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			if (nport && nat != NULL)
				pf_put_sport(IPPROTO_UDP, nport);
			return (PF_DROP);
		}

		s->rule = *rm;
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
		s->src.seqlo  = 0;
		s->src.seqhi = 0;
		s->src.seqdiff = 0;
		s->src.max_win = 0;
		s->src.state = 1;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.seqdiff = 0;
		s->dst.max_win = 0;
		s->dst.state = 0;
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + pftm_udp_first_packet;
		s->packets = 1;
		s->bytes = pd->tot_len;
		pf_insert_state(s);
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
	struct pf_addr *saddr = pd->src, *daddr = pd->dst, baddr;
	struct pf_rule *r;
	u_short reason;
	u_int16_t icmpid, af = pd->af;
	u_int8_t icmptype, icmpcode;
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
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpcode = pd->hdr.icmp6->icmp6_code;
		icmpid = pd->hdr.icmp6->icmp6_id;
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
				    binat->raddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &binat->raddr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
		/* check outgoing packet for NAT */
		else if ((nat = pf_get_nat(ifp, pd->proto,
		    saddr, daddr, af)) != NULL) {
			PF_ACPY(&baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr,
				    pd->ip_sum, nat->raddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &nat->raddr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
	} else {
		/* check incoming packet for BINAT */
		if ((binat = pf_get_binat(PF_IN, ifp, IPPROTO_ICMP,
		    daddr, saddr, af)) != NULL) {
			PF_ACPY(&baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, binat->saddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(daddr, &pd->hdr.icmp6->icmp6_cksum,
				    &binat->saddr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action == PF_SCRUB) {
			r = TAILQ_NEXT(r, entries);
			continue;
		}
		r->evaluations++;
		if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (!PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr, &r->src.mask, saddr, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (!PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr, &r->dst.mask, daddr, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->direction != direction)
			r = TAILQ_NEXT(r, entries);
		else if (r->ifp != NULL && r->ifp != ifp)
			r = TAILQ_NEXT(r, entries);
		else if (r->type && r->type != icmptype + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->code && r->code != icmpcode + 1)
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
		(*rm)->bytes +=  pd->tot_len;
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if ((*rm)->log)
			PFLOG_PACKET(ifp, h, m, af, direction, reason, *rm);

		if ((*rm)->action != PF_PASS)
			return (PF_DROP);
	}

	if ((*rm != NULL && (*rm)->keep_state) || nat != NULL || binat != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = pd->tot_len - off - ICMP_MINLEN;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);

		s->rule = *rm;
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
			if (binat != NULL)
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
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + pftm_icmp_first_packet;
		s->packets = 1;
		s->bytes = pd->tot_len;
		pf_insert_state(s);
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
	struct pf_binat *binat = NULL;
	struct pf_addr *saddr = pd->src, *daddr = pd->dst;
	u_int8_t af = pd->af;

	*rm = NULL;

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT */
		if ((binat = pf_get_binat(PF_OUT, ifp, pd->proto,
		    saddr, daddr, af)) != NULL) {
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    binat->raddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(saddr, &binat->raddr, af);
				break;
#endif /* INET6 */
			}
		}
	} else {
		/* check incoming packet for BINAT */
		if ((binat = pf_get_binat(PF_IN, ifp, pd->proto,
		    daddr, saddr, af)) != NULL) {
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, binat->saddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(daddr, &binat->saddr, af);
				break;
#endif /* INET6 */
			}
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action == PF_SCRUB) {
			r = TAILQ_NEXT(r, entries);
			continue;
		}
		r->evaluations++;
		if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (!PF_AZERO(&r->src.mask, af) && !PF_MATCHA(r->src.not,
		    &r->src.addr, &r->src.mask, pd->src, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (!PF_AZERO(&r->dst.mask, af) && !PF_MATCHA(r->dst.not,
		    &r->dst.addr, &r->dst.mask, pd->dst, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->direction != direction)
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
	struct pf_tree_key key;
	struct tcphdr *th = pd->hdr.tcp;
	u_int16_t win = ntohs(th->th_win);
	u_int32_t ack, end, seq;
	int ackskew;
	struct pf_state_peer *src, *dst;

	key.af	    = pd->af;
	key.proto   = IPPROTO_TCP;
	PF_ACPY(&key.addr[0], pd->src, key.af);
	PF_ACPY(&key.addr[1], pd->dst, key.af);
	key.port[0] = th->th_sport;
	key.port[1] = th->th_dport;

	if (direction == PF_IN)
		*state = pf_find_state(tree_ext_gwy, &key);
	else
		*state = pf_find_state(tree_lan_ext, &key);
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
		/* First packet from this end.  Set its state */

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
		if (SEQ_GEQ(end + MAX(1, dst->max_win), src->seqhi))
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
		/* syncronize sequencing */
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
			(*state)->expire = pftv.tv_sec + pftm_tcp_closed;
		else if (src->state >= TCPS_FIN_WAIT_2 ||
		    dst->state >= TCPS_FIN_WAIT_2)
			(*state)->expire = pftv.tv_sec + pftm_tcp_fin_wait;
		else if (src->state >= TCPS_CLOSING ||
		    dst->state >= TCPS_CLOSING)
			(*state)->expire = pftv.tv_sec + pftm_tcp_closing;
		else if (src->state < TCPS_ESTABLISHED ||
		    dst->state < TCPS_ESTABLISHED)
			(*state)->expire = pftv.tv_sec + pftm_tcp_opening;
		else
			(*state)->expire = pftv.tv_sec + pftm_tcp_established;

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
		 * since packet floods will also be caught here.  We don't
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
		/* syncronize sequencing */
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

	if ((*state)->rule != NULL) {
		(*state)->rule->packets++;
		(*state)->rule->bytes += pd->tot_len;
	}
	return (PF_PASS);
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_state_peer *src, *dst;
	struct pf_tree_key key;
	struct udphdr *uh = pd->hdr.udp;

	key.af	    = pd->af;
	key.proto   = IPPROTO_UDP;
	PF_ACPY(&key.addr[0], pd->src, key.af);
	PF_ACPY(&key.addr[1], pd->dst, key.af);
	key.port[0] = pd->hdr.udp->uh_sport;
	key.port[1] = pd->hdr.udp->uh_dport;

	if (direction == PF_IN)
		*state = pf_find_state(tree_ext_gwy, &key);
	else
		*state = pf_find_state(tree_lan_ext, &key);
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
	if (src->state < 1)
		src->state = 1;
	if (dst->state == 1)
		dst->state = 2;

	/* update expire time */
	if (src->state == 2 && dst->state == 2)
		(*state)->expire = pftv.tv_sec + pftm_udp_multiple;
	else
		(*state)->expire = pftv.tv_sec + pftm_udp_single;

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

	if ((*state)->rule != NULL) {
		(*state)->rule->packets++;
		(*state)->rule->bytes += pd->tot_len;
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
		struct pf_tree_key key;

		key.af      = pd->af;
		key.proto   = pd->proto;
		PF_ACPY(&key.addr[0], saddr, key.af);
		PF_ACPY(&key.addr[1], daddr, key.af);
		key.port[0] = icmpid;
		key.port[1] = icmpid;

		if (direction == PF_IN)
			*state = pf_find_state(tree_ext_gwy, &key);
		else
			*state = pf_find_state(tree_lan_ext, &key);
		if (*state == NULL)
			return (PF_DROP);

		(*state)->packets++;
		(*state)->bytes += pd->tot_len;
		(*state)->expire = pftv.tv_sec + pftm_icmp_error_reply;

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
					/* XXX we don't handle fagments yet */
					return (PF_DROP);
				case IPPROTO_AH:
				case IPPROTO_HOPOPTS:
				case IPPROTO_ROUTING:
				case IPPROTO_DSTOPTS: {
					/* get next header and header length */
					struct _opt6 opt6;

					if (!pf_pull_hdr(m, off2, &opt6,
					    sizeof(opt6), NULL, NULL, pd2.af)) {
						DPFPRINTF(PF_DEBUG_MISC,
						    ("pf:  ICMPv6 short opt\n"));
						return(PF_DROP);
					}
					pd2.proto = opt6.opt6_nxt;
					off2 += (opt6.opt6_hlen + 1) * 8;
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
			struct pf_tree_key key;
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
			key.proto   = IPPROTO_TCP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = th.th_dport;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = th.th_sport;

			if (direction == PF_IN)
				*state = pf_find_state(tree_ext_gwy, &key);
			else
				*state = pf_find_state(tree_lan_ext, &key);
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
			struct pf_tree_key key;

			if (!pf_pull_hdr(m, off2, &uh, sizeof(uh),
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (udp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto   = IPPROTO_UDP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = uh.uh_dport;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = uh.uh_sport;

			if (direction == PF_IN)
				*state = pf_find_state(tree_ext_gwy, &key);
			else
				*state = pf_find_state(tree_lan_ext, &key);
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
			struct pf_tree_key key;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (icmp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto   = IPPROTO_ICMP;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = iih.icmp_id;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = iih.icmp_id;

			if (direction == PF_IN)
				*state = pf_find_state(tree_ext_gwy, &key);
			else
				*state = pf_find_state(tree_lan_ext, &key);
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
			struct pf_tree_key key;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, NULL, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short (icmp6)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto   = IPPROTO_ICMPV6;
			PF_ACPY(&key.addr[0], pd2.dst, pd2.af);
			key.port[0] = iih.icmp6_id;
			PF_ACPY(&key.addr[1], pd2.src, pd2.af);
			key.port[1] = iih.icmp6_id;

			if (direction == PF_IN)
				*state = pf_find_state(tree_ext_gwy, &key);
			else
				*state = pf_find_state(tree_lan_ext, &key);
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
		    (ntohs(h->ip6_plen) + sizeof(struct ip6_hdr)) < off + len) {
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

#ifdef INET
void
pf_route(struct mbuf *m, struct pf_rule *r)
{
	struct mbuf *m0, *m1;
	struct route iproute;
	struct route *ro;
	struct sockaddr_in *dst;
	struct ip *ip, *mhip;
	struct ifnet *ifp = r->rt_ifp;
	int hlen;
	int len, off, error = 0;

	if (m == NULL)
		return;

	m0 = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
	if (m0 == NULL)
		return;

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
		error = EMSGSIZE;
		ipstat.ips_cantfrag++;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}
	/*
	 * If we are doing fragmentation, we can't defer TCP/UDP
	 * checksumming; compute the checksum and clear the flag.
	 */
	if (m0->m_pkthdr.csum & (M_TCPV4_CSUM_OUT | M_UDPV4_CSUM_OUT)) {
		in_delayed_cksum(m0);
		m0->m_pkthdr.csum &= ~(M_UDPV4_CSUM_OUT | M_TCPV4_CSUM_OUT);
	}
	
	{
	    int mhlen, firstlen = len;
	    struct mbuf **mnext = &m0->m_nextpkt;
	    
	    /*
	     * Loop through length of segment after first fragment,
	     * make new header and copy data of each part and link onto chain.
	     */
	    m1 = m0;
	    mhlen = sizeof (struct ip);
	    for (off = hlen + len; off < (u_int16_t)ip->ip_len; off += len) {
		    MGETHDR(m0, M_DONTWAIT, MT_HEADER);
		    if (m0 == 0) {
			    error = ENOBUFS;
			    ipstat.ips_odropped++;
			    goto sendorfree;
		    }
		    *mnext = m0;
		    mnext = &m0->m_nextpkt;
		    m0->m_data += max_linkhdr;
		    mhip = mtod(m0, struct ip *);
		    *mhip = *ip;
		    /* we must inherit MCAST and BCAST flags */
		    m0->m_flags |= m1->m_flags & (M_MCAST|M_BCAST);
		    if (hlen > sizeof (struct ip)) {
			    mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			    mhip->ip_hl = mhlen >> 2;
		    }
		    m0->m_len = mhlen;
		    mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		    if (ip->ip_off & IP_MF)
			    mhip->ip_off |= IP_MF;
		    if (off + len >= (u_int16_t)ip->ip_len)
			    len = (u_int16_t)ip->ip_len - off;
		    else
			    mhip->ip_off |= IP_MF;
		    mhip->ip_len = htons((u_int16_t)(len + mhlen));
		    m0->m_next = m_copy(m1, off, len);
		    if (m0->m_next == 0) {
			    error = ENOBUFS;/* ??? */
			    ipstat.ips_odropped++;
			    goto sendorfree;
		    }
		    m0->m_pkthdr.len = mhlen + len;
		    m0->m_pkthdr.rcvif = (struct ifnet *)0;
		    mhip->ip_off = htons((u_int16_t)mhip->ip_off);
		    if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
			ifp->if_bridge == NULL) {
			    m0->m_pkthdr.csum |= M_IPV4_CSUM_OUT;
			    ipstat.ips_outhwcsum++;
		    } else {
			    mhip->ip_sum = 0;
			    mhip->ip_sum = in_cksum(m0, mhlen);
		    }
		    ipstat.ips_ofragments++;
	    }
	    /*
	     * Update first fragment by trimming what's been copied out
	     * and updating header, then send each fragment (in order).
	     */
	    m0 = m1;
	    m_adj(m0, hlen + firstlen - (u_int16_t)ip->ip_len);
	    m0->m_pkthdr.len = hlen + firstlen;
	    ip->ip_len = htons((u_int16_t)m0->m_pkthdr.len);
	    ip->ip_off = htons((u_int16_t)(ip->ip_off | IP_MF));
	    if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		ifp->if_bridge == NULL) {
		    m0->m_pkthdr.csum |= M_IPV4_CSUM_OUT;
		    ipstat.ips_outhwcsum++;
	    } else {
		    ip->ip_sum = 0;
		    ip->ip_sum = in_cksum(m0, hlen);
	    }
sendorfree:
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
	}
	
done:
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
pf_route6(struct mbuf *m, struct pf_rule *r)
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

	m0 = m_copym2(m, 0, M_COPYALL, M_NOWAIT);
	if (m0 == NULL)
		return;

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

	/*
	 * Do not fragment packets (yet).  Not much is done here for dealing
	 * with errors.  Actions on errors depend on whether the packet
	 * was generated locally or being forwarded.
	 */
	if (m0->m_pkthdr.len <= ifp->if_mtu) {
		error = (*ifp->if_output)(ifp, m0, (struct sockaddr *)dst,
		    NULL);
	} else
		m_freem(m0);

done:
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

	/* purge expire states */
	microtime(&pftv);
	if (pftv.tv_sec - pf_last_purge >= pftm_interval) {
		pf_purge_expired_states();
		pf_purge_expired_fragments();
		pf_last_purge = pftv.tv_sec;
	}

	if (m->m_pkthdr.len < sizeof(*h)) {
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
	if (off < sizeof(*h)) {
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
	pd.tot_len = h->ip_len;

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
			r = s->rule;
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
			r = s->rule;
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
			r = s->rule;
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

	if (r && r->rt) {
		pf_route(m, r);
		if (r->rt != PF_DUPTO) {
			m_freem(*m0);
			*m0 = NULL;
                }
	}

	if (log) {
		struct pf_rule r0;

		if (r == NULL) {
			r0.ifp = ifp;
			r0.action = action;
			r0.nr = -1;
			r = &r0;
		}
		PFLOG_PACKET(ifp, h, m, AF_INET, dir, reason, r);
	}
	/* XXX (pf_rule *)r may now be invalid from the above log */
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

	/* purge expire states */
	microtime(&pftv);
	if (pftv.tv_sec - pf_last_purge >= pftm_interval) {
		pf_purge_expired_states();
		pf_purge_expired_fragments();
		pf_last_purge = pftv.tv_sec;
	}

	if (m->m_pkthdr.len < sizeof(*h)) {
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
	pd.tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);

	off = ((caddr_t)h - m->m_data) + sizeof(struct ip6_hdr);
	pd.proto = h->ip6_nxt;
	do {			
		switch (pd.proto) {
		case IPPROTO_FRAGMENT: 
			/* XXX we don't handle fragments yet */
			action = PF_DROP;
			REASON_SET(&reason, PFRES_FRAG);
			goto done;
		case IPPROTO_AH:
		case IPPROTO_HOPOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_DSTOPTS: {
			/* get next header and header length */
			struct _opt6 opt6;

			if (!pf_pull_hdr(m, off, &opt6, sizeof(opt6),
			    NULL, NULL, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short opt\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				log = 1;
				goto done;
			}
			pd.proto = opt6.opt6_nxt;
			off += (opt6.opt6_hlen + 1) * 8;
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
			r = s->rule;
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
			r = s->rule;
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
			r = s->rule;
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
		pf_status.bcounters[1][dir] += h->ip6_plen;
		pf_status.pcounters[1][dir][action]++;
	}

done:   
	/* XXX handle IPv6 options, if not allowed. not implemented. */

	if (r && r->rt) {
		pf_route6(m, r);
		if (r->rt != PF_DUPTO) {
			m_freem(*m0);
			*m0 = NULL;
                }
	}

	if (log) {
		struct pf_rule r0;

		if (r == NULL) {
			r0.ifp = ifp;
			r0.action = action;
			r0.nr = -1;
			r = &r0;
		}
		PFLOG_PACKET(ifp, h, m, AF_INET6, dir, reason, r);
	}
	/* XXX (pf_rule *)r may now be invalid from the above log */
	return (action);
}
#endif /* INET6 */
