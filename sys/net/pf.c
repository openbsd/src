/*	$OpenBSD: pf.c,v 1.102 2001/07/06 22:09:00 dhartmei Exp $ */

/*
 * Copyright (c) 2001, Daniel Hartmeier
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
#include <net/pfvar.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include "bpfilter.h"
#include "pflog.h"

int pf_debug = 0;
#define DPFPRINTF(x)	if (pf_debug) printf x

/*
 * Tree data structure
 */

struct pf_tree_node {
	struct pf_tree_key {
		struct in_addr	 addr[2];
		u_int16_t	 port[2];
		u_int8_t	 proto;
	}			 key;
	struct pf_state		*state;
	struct pf_tree_node	*parent;
	struct pf_tree_node	*left;
	struct pf_tree_node	*right;
	signed char		 balance;
};

struct pf_frent {
	LIST_ENTRY(pf_frent) fr_next;
	struct ip *fr_ip;
	struct mbuf *fr_m;
};

#define PFFRAG_SEENLAST	0x0001		/* Seen the last fragment for this */

struct pf_fragment {
	TAILQ_ENTRY(pf_fragment) frag_next;
	struct in_addr	fr_src;
	struct in_addr	fr_dst;
	u_int8_t	fr_p;		/* protocol of this fragment */
	u_int8_t	fr_flags;	/* status flags */
	u_int16_t	fr_id;		/* fragment id for reassemble */
	u_int16_t	fr_max;		/* fragment data max */
	struct timeval	fr_timeout;
	LIST_HEAD(pf_fragq, pf_frent) fr_queue;
};

/*
 * Global variables
 */

TAILQ_HEAD(pf_fragqueue, pf_fragment)	pf_fragqueue;
TAILQ_HEAD(pf_rulequeue, pf_rule)	pf_rules[2];
TAILQ_HEAD(pf_natqueue, pf_nat)		pf_nats[2];
TAILQ_HEAD(pf_rdrqueue, pf_rdr)		pf_rdrs[2];
struct pf_rulequeue	*pf_rules_active;
struct pf_rulequeue	*pf_rules_inactive;
struct pf_natqueue	*pf_nats_active;
struct pf_natqueue	*pf_nats_inactive;
struct pf_rdrqueue	*pf_rdrs_active;
struct pf_rdrqueue	*pf_rdrs_inactive;
struct pf_tree_node	*tree_lan_ext, *tree_ext_gwy;
struct pf_tree_node	*tree_fragment;
struct timeval		 pftv;
struct pf_status	 pf_status;
struct ifnet		*status_ifp;

u_int32_t		 pf_last_purge;
u_int32_t		 ticket_rules_active;
u_int32_t		 ticket_rules_inactive;
u_int32_t		 ticket_nats_active;
u_int32_t		 ticket_nats_inactive;
u_int32_t		 ticket_rdrs_active;
u_int32_t		 ticket_rdrs_inactive;
u_int16_t		 pf_next_port_tcp = 50001;
u_int16_t		 pf_next_port_udp = 50001;

struct pool		 pf_tree_pl, pf_rule_pl, pf_nat_pl;
struct pool		 pf_rdr_pl, pf_state_pl, pf_frent_pl, pf_frag_pl;
int			 pf_nfrents;

int			 pf_tree_key_compare(struct pf_tree_key *,
			    struct pf_tree_key *);
void			 pf_tree_rotate_left(struct pf_tree_node **);
void			 pf_tree_rotate_right(struct pf_tree_node **);
int			 pf_tree_insert(struct pf_tree_node **,
			    struct pf_tree_node *, struct pf_tree_key *,
			    struct pf_state *);
int			 pf_tree_remove(struct pf_tree_node **,
			    struct pf_tree_node *, struct pf_tree_key *);
struct pf_tree_node	*pf_tree_first(struct pf_tree_node *);
struct pf_tree_node	*pf_tree_next(struct pf_tree_node *);
struct pf_tree_node	*pf_tree_search(struct pf_tree_node *,
			    struct pf_tree_key *);
struct pf_state		*pf_find_state(struct pf_tree_node *,
			    struct pf_tree_key *);
void			 pf_insert_state(struct pf_state *);
void			 pf_purge_expired_states(void);

void			 pf_print_host(u_int32_t, u_int16_t);
void			 pf_print_state(int, struct pf_state *);
void			 pf_print_flags(u_int8_t);

void			 pfattach(int);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
int			 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);

u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t);
void			 pf_change_ap(u_int32_t *, u_int16_t *, u_int16_t *,
			    u_int16_t *, u_int32_t, u_int16_t);
void			 pf_change_a(u_int32_t *, u_int16_t *, u_int32_t);
void			 pf_change_icmp(u_int32_t *, u_int16_t *, u_int32_t *,
			    u_int32_t, u_int16_t, u_int16_t *, u_int16_t *,
			    u_int16_t *, u_int16_t *);
void			 pf_send_reset(struct ip *, int, struct tcphdr *);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t);
int			 pf_match_addr(u_int8_t, u_int32_t, u_int32_t,
			    u_int32_t);
int			 pf_match_port(u_int8_t, u_int16_t, u_int16_t,
			    u_int16_t);
u_int16_t		 pf_map_port_range(struct pf_rdr *, u_int16_t);
struct pf_nat		*pf_get_nat(struct ifnet *, u_int8_t, u_int32_t);
struct pf_rdr		*pf_get_rdr(struct ifnet *, u_int8_t, u_int32_t,
			    u_int16_t);
int			 pf_test_tcp(int, struct ifnet *, struct mbuf *,
			    int, int, struct ip *, struct tcphdr *);
int			 pf_test_udp(int, struct ifnet *, struct mbuf *,
			    int, int, struct ip *, struct udphdr *);
int			 pf_test_icmp(int, struct ifnet *, struct mbuf *,
			    int, int, struct ip *, struct icmp *);
int			 pf_test_other(int, struct ifnet *, struct mbuf *,
			    struct ip *);
int			 pf_test_state_tcp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    struct ip *, struct tcphdr *);
int			 pf_test_state_udp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    struct ip *, struct udphdr *);
int			 pf_test_state_icmp(struct pf_state **, int,
			    struct ifnet *, struct mbuf *, int, int,
			    struct ip *, struct icmp *);
void			*pf_pull_hdr(struct ifnet *, struct mbuf *, int, int,
			    void *, int, struct ip *, u_short *, u_short *);
int			 pflog_packet(struct mbuf *, int, u_short, u_short,
			    struct pf_rule *);

int			 pf_normalize_ip(struct mbuf **, int, struct ifnet *,
			    u_short *);

void			 pf_purge_expired_fragments(void);
void			 pf_ip2key(struct pf_tree_key *, struct ip *);
void			 pf_remove_fragment(struct pf_fragment *);
void			 pf_flush_fragments(void);
void			 pf_free_fragment(struct pf_fragment *);
struct pf_fragment	*pf_find_fragment(struct ip *);
struct mbuf		*pf_reassemble(struct mbuf **, struct pf_fragment *,
			    struct pf_frent *, int);

#if NPFLOG > 0
#define		 PFLOG_PACKET(x,a,b,c,d,e) \
		do { \
			HTONS((x)->ip_len); \
			HTONS((x)->ip_off); \
			pflog_packet(a,b,c,d,e); \
			NTOHS((x)->ip_len); \
			NTOHS((x)->ip_off); \
		} while (0)
#else
#define		 PFLOG_PACKET
#endif

#define		MATCH_TUPLE(h,r,d,i) \
		( \
		  (r->direction == d) && \
		  (r->ifp == NULL || r->ifp == i) && \
		  (!r->proto || r->proto == h->ip_p) && \
		  (!r->src.mask || pf_match_addr(r->src.not, r->src.addr, \
		   r->src.mask, h->ip_src.s_addr)) && \
		  (!r->dst.mask || pf_match_addr(r->dst.not, r->dst.addr, \
		   r->dst.mask, h->ip_dst.s_addr)) \
		)

#define PFFRAG_FRENT_HIWAT	5000	/* Number of fragment entries */
#define PFFRAG_FRAG_HIWAT	1000	/* Number of fragmented packets */

int
pf_tree_key_compare(struct pf_tree_key *a, struct pf_tree_key *b)
{
	/*
	 * could use memcmp(), but with the best manual order, we can
	 * minimize the number of average compares. what is faster?
	 */
	if (a->proto < b->proto)
		return (-1);
	if (a->proto > b->proto)
		return ( 1);
	if (a->addr[0].s_addr < b->addr[0].s_addr)
		return (-1);
	if (a->addr[0].s_addr > b->addr[0].s_addr)
		return ( 1);
	if (a->addr[1].s_addr < b->addr[1].s_addr)
		return (-1);
	if (a->addr[1].s_addr > b->addr[1].s_addr)
		return ( 1);
	if (a->port[0] < b->port[0])
		return (-1);
	if (a->port[0] > b->port[0])
		return ( 1);
	if (a->port[1] < b->port[1])
		return (-1);
	if (a->port[1] > b->port[1])
		return ( 1);
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
pflog_packet(struct mbuf *m, int af, u_short dir, u_short reason,
    struct pf_rule *rm)
{
#if NBPFILTER > 0
	struct ifnet *ifn, *ifp = NULL;
	struct pfloghdr hdr;
	struct mbuf m1;

	if (m == NULL)
		return(-1);

	hdr.af = htonl(af);
	/* Set the right interface name */
	if (rm != NULL)
		ifp = rm->ifp;
	if (m->m_pkthdr.rcvif != NULL)
		ifp = m->m_pkthdr.rcvif;
	if (ifp != NULL)
		memcpy(hdr.ifname, ifp->if_xname, sizeof(hdr.ifname));
	else
		strcpy(hdr.ifname, "unkn");

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
	/* go up to root, so the caller can pass any node. useful? */
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
		n = (c > 0) ? n->left : n->right;
	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	return (n);
}

struct pf_state *
pf_find_state(struct pf_tree_node *n, struct pf_tree_key *key)
{
	n = pf_tree_search(n, key);
	return (n ? n->state : NULL);
}

void
pf_insert_state(struct pf_state *state)
{
	struct pf_tree_key key;

	key.proto = state->proto;
	key.addr[0].s_addr = state->lan.addr;
	key.port[0] = state->lan.port;
	key.addr[1].s_addr = state->ext.addr;
	key.port[1] = state->ext.port;
	/* sanity checks can be removed later, should never occur */
	if (pf_find_state(tree_lan_ext, &key) != NULL)
		printf("pf: ERROR! insert invalid\n");
	else {
		pf_tree_insert(&tree_lan_ext, NULL, &key, state);
		if (pf_find_state(tree_lan_ext, &key) != state)
			printf("pf: ERROR! insert failed\n");
	}

	key.proto = state->proto;
	key.addr[0].s_addr = state->ext.addr;
	key.port[0] = state->ext.port;
	key.addr[1].s_addr = state->gwy.addr;
	key.port[1] = state->gwy.port;
	if (pf_find_state(tree_ext_gwy, &key) != NULL)
		printf("pf: ERROR! insert invalid\n");
	else {
		pf_tree_insert(&tree_ext_gwy, NULL, &key, state);
		if (pf_find_state(tree_ext_gwy, &key) != state)
			printf("pf: ERROR! insert failed\n");
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
			key.proto = cur->state->proto;
			key.addr[0].s_addr = cur->state->lan.addr;
			key.port[0] = cur->state->lan.port;
			key.addr[1].s_addr = cur->state->ext.addr;
			key.port[1] = cur->state->ext.port;
			/* remove state from second tree */
			if (pf_find_state(tree_lan_ext, &key) != cur->state)
				printf("pf: ERROR: remove invalid!\n");
			pf_tree_remove(&tree_lan_ext, NULL, &key);
			if (pf_find_state(tree_lan_ext, &key) != NULL)
				printf("pf: ERROR: remove failed\n");
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
					printf(
					    "pf: ERROR: next not refound\n");
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
pf_print_host(u_int32_t a, u_int16_t p)
{
	a = ntohl(a);
	p = ntohs(p);
	printf("%u.%u.%u.%u:%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255,
	    p);
}

void
pf_print_state(int direction, struct pf_state *s)
{
	pf_print_host(s->lan.addr, s->lan.port);
	printf(" ");
	pf_print_host(s->gwy.addr, s->gwy.port);
	printf(" ");
	pf_print_host(s->ext.addr, s->ext.port);
	printf(" [lo=%lu high=%lu win=%u]", s->src.seqlo, s->src.seqhi,
		 s->src.max_win);
	printf(" [lo=%lu high=%lu win=%u]", s->dst.seqlo, s->dst.seqhi,
		 s->dst.max_win);
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
	pool_init(&pf_rdr_pl, sizeof(struct pf_rdr), 0, 0, 0, "pfrdrpl",
	    0, NULL, NULL, 0);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    0, NULL, NULL, 0);
	pool_init(&pf_frent_pl, sizeof(struct pf_frent), 0, 0, 0, "pffrent",
	    0, NULL, NULL, 0);
	pool_init(&pf_frag_pl, sizeof(struct pf_fragment), 0, 0, 0, "pffrag",
	    0, NULL, NULL, 0);

	pool_sethiwat(&pf_frag_pl, PFFRAG_FRAG_HIWAT);
	pool_sethardlimit(&pf_frent_pl, PFFRAG_FRENT_HIWAT, NULL, 0);
	
	TAILQ_INIT(&pf_fragqueue);
	TAILQ_INIT(&pf_rules[0]);
	TAILQ_INIT(&pf_rules[1]);
	TAILQ_INIT(&pf_nats[0]);
	TAILQ_INIT(&pf_nats[1]);
	TAILQ_INIT(&pf_rdrs[0]);
	TAILQ_INIT(&pf_rdrs[1]);
	pf_rules_active = &pf_rules[0];
	pf_rules_inactive = &pf_rules[1];
	pf_nats_active = &pf_nats[0];
	pf_nats_inactive = &pf_nats[1];
	pf_rdrs_active = &pf_rdrs[0];
	pf_rdrs_inactive = &pf_rdrs[1];
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
		case DIOCBEGINRDRS:
		case DIOCADDRDR:
		case DIOCCOMMITRDRS:
		case DIOCCLRSTATES:
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
			printf("pf: started\n");
		}
		break;

	case DIOCSTOP:
		if (!pf_status.running)
			error = ENOENT;
		else {
			pf_status.running = 0;
			printf("pf: stopped\n");
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
		tail = TAILQ_LAST(pf_rules_inactive, pf_rulequeue);
		rule->nr = tail ? tail->nr + 1 : 0;
		rule->ifp = NULL;
		if (rule->ifname[0]) {
			rule->ifp = ifunit(rule->ifname);
			if (rule->ifp == NULL) {
				pool_put(&pf_rule_pl, rule);
				error = EINVAL;
				break;
			}
		}
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
		pr->nr = tail ? tail->nr + 1 : 0;
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
		nat->ifp = ifunit(nat->ifname);
		if (nat->ifp == NULL) {
			pool_put(&pf_nat_pl, nat);
			error = EINVAL;
			break;
		}
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
		rdr->ifp = ifunit(rdr->ifname);
		if (rdr->ifp == NULL) {
			pool_put(&pf_rdr_pl, rdr);
			error = EINVAL;
			break;
		}
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

	case DIOCCLRSTATES: {
		struct pf_tree_node *n;

		s = splsoftnet();
		for (n = pf_tree_first(tree_ext_gwy); n != NULL;
		    n = pf_tree_next(n))
			n->state->expire = 0;
		pf_purge_expired_states();
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
		struct pf_natlook *pnl = (struct pf_natlook *)addr;
		struct pf_state *st;
		struct pf_tree_key key;
		int direction = pnl->direction;
	
		key.proto = pnl->proto;

		/*
		 * userland gives us source and dest of connetion, reverse
		 * the lookup so we ask for what happens with the return 
		 * traffic, enabling us to find it in the state tree.
		 */
		key.addr[1].s_addr = pnl->saddr;
		key.port[1] = pnl->sport;
		key.addr[0].s_addr = pnl->daddr;
		key.port[0] = pnl->dport;
		
		if (!pnl->proto || !pnl->saddr || !pnl->daddr ||
		    !pnl->dport || !pnl->sport)
			error = EINVAL;
		else {
			s = splsoftnet();
			st = pf_find_state((direction == PF_IN) ? 
			    tree_ext_gwy : tree_lan_ext, &key);
			if (st != NULL) {
				if (direction  == PF_IN) {
					pnl->rsaddr = st->lan.addr;
					pnl->rsport = st->lan.port;
					pnl->rdaddr = pnl->daddr;
					pnl->rdport = pnl->dport;
				} else {
					pnl->rdaddr = st->gwy.addr;
					pnl->rdport = st->gwy.port;
					pnl->rsaddr = pnl->saddr;
					pnl->rsport = pnl->sport;
				}
			} else 
				error = ENOENT;
			splx(s);
		}
		break;
	}
	default:
		error = ENODEV;
		break;
	}

	return (error);
}

u_int16_t
pf_cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new)
{
	u_int32_t l = cksum + old - new;

	l = (l >> 16) + (l & 65535);
	l = l & 65535;
	return (l ? l : 65535);
}

void
pf_change_ap(u_int32_t *a, u_int16_t *p, u_int16_t *ic, u_int16_t *pc,
    u_int32_t an, u_int16_t pn)
{
	u_int32_t ao = *a;
	u_int16_t po = *p;

	*a = an;
	*ic = pf_cksum_fixup(pf_cksum_fixup(*ic, ao / 65536,
	    an / 65536), ao % 65536, an % 65536);
	*p = pn;
	*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc, ao / 65536,
	    an / 65536), ao % 65536, an % 65536),
	    po, pn);
}

void
pf_change_a(u_int32_t *a, u_int16_t *c, u_int32_t an)
{
	u_int32_t ao = *a;

	*a = an;
	*c = pf_cksum_fixup(pf_cksum_fixup(*c, ao / 65536, an / 65536),
	    ao % 65536, an % 65536);
}

void
pf_change_icmp(u_int32_t *ia, u_int16_t *ip, u_int32_t *oa, u_int32_t na,
    u_int16_t np, u_int16_t *pc, u_int16_t *h2c, u_int16_t *ic, u_int16_t *hc)
{
	u_int32_t oia = *ia, ooa = *oa, opc, oh2c = *h2c;
	u_int16_t oip = *ip;

	if (pc != NULL)
		opc = *pc;
	/* Change inner protocol port, fix inner protocol checksum. */
	*ip = np;
	if (pc != NULL)
		*pc = pf_cksum_fixup(*pc, oip, *ip);
	*ic = pf_cksum_fixup(*ic, oip, *ip);
	if (pc != NULL)
		*ic = pf_cksum_fixup(*ic, opc, *pc);
	/* Change inner ip address, fix inner ip checksum and icmp checksum. */
	*ia = na;
	*h2c = pf_cksum_fixup(pf_cksum_fixup(*h2c, oia / 65536, *ia / 65536),
	    oia % 65536, *ia % 65536);
	*ic = pf_cksum_fixup(pf_cksum_fixup(*ic, oia / 65536, *ia / 65536),
	    oia % 65536, *ia % 65536);
	*ic = pf_cksum_fixup(*ic, oh2c, *h2c);
	/* Change outer ip address, fix outer ip checksum. */
	*oa = na;
	*hc = pf_cksum_fixup(pf_cksum_fixup(*hc, ooa / 65536, *oa / 65536),
	    ooa % 65536, *oa % 65536);
}

void
pf_send_reset(struct ip *h, int off, struct tcphdr *th)
{
	struct mbuf *m;
	struct m_tag *mtag;
	int len = sizeof(struct ip) + sizeof(struct tcphdr);
	struct ip *h2;
	struct tcphdr *th2;

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
	h2 = mtod(m, struct ip *);

	/* IP header fields included in the TCP checksum */
	h2->ip_p = IPPROTO_TCP;
	h2->ip_len = htons(sizeof(*th2));
	h2->ip_src.s_addr = h->ip_dst.s_addr;
	h2->ip_dst.s_addr = h->ip_src.s_addr;

	/* TCP header */
	th2 = (struct tcphdr *)((caddr_t)h2 + sizeof(struct ip));
	th2->th_sport = th->th_dport;
	th2->th_dport = th->th_sport;
	if (th->th_flags & TH_ACK) {
		th2->th_seq = th->th_ack;
		th2->th_flags = TH_RST;
	} else {
		int tlen = h->ip_len - off - (th->th_off << 2) +
		    ((th->th_flags & TH_SYN) ? 1 : 0) +
		    ((th->th_flags & TH_FIN) ? 1 : 0);
		th2->th_ack = htonl(ntohl(th->th_seq) + tlen);
		th2->th_flags = TH_RST | TH_ACK;
	}
	th2->th_off = sizeof(*th2) >> 2;

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
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code)
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
	icmp_error(m0, type, code, 0, 0);
}

int
pf_match_addr(u_int8_t n, u_int32_t a, u_int32_t m, u_int32_t b)
{
	return (n == !((a & m) == (b & m)));
}

int
pf_match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	switch (op) {
	case PF_OP_GL:
		return (p >= a1) && (p <= a2);
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

struct pf_nat *
pf_get_nat(struct ifnet *ifp, u_int8_t proto, u_int32_t addr)
{
	struct pf_nat *n, *nm = NULL;

	n = TAILQ_FIRST(pf_nats_active);
	while (n && nm == NULL) {
		if (((n->ifp == ifp) == !n->ifnot) &&
		    (!n->proto || n->proto == proto) &&
		    pf_match_addr(n->not, n->saddr, n->smask, addr))
			nm = n;
		else
			n = TAILQ_NEXT(n, entries);
	}
	return (nm);
}

struct pf_rdr *
pf_get_rdr(struct ifnet *ifp, u_int8_t proto, u_int32_t addr, u_int16_t port)
{
	struct pf_rdr *r, *rm = NULL;

	r = TAILQ_FIRST(pf_rdrs_active);
	while (r && rm == NULL) {
		if (((r->ifp == ifp) == !r->ifnot) &&
		    (!r->proto || r->proto == proto) &&
		    pf_match_addr(r->not, r->daddr, r->dmask, addr) &&
		    (ntohs(port) >= ntohs(r->dport)) && 
		    (ntohs(port) <= ntohs(r->dport2)))
			rm = r;
		else
			r = TAILQ_NEXT(r, entries);
	}
	return (rm);
}

#define ACTION_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
	} while (0)

#define REASON_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
		if (x < PFRES_MAX) \
			pf_status.counters[x]++; \
	} while (0)

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
pf_test_tcp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct tcphdr *th)
{
	struct pf_nat *nat = NULL;
	struct pf_rdr *rdr = NULL;
	u_int32_t baddr;
	u_int16_t bport, nport;
	struct pf_rule *r, *rm = NULL;
	u_short reason;
	int rewrite = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = pf_get_nat(ifp, IPPROTO_TCP,
		    h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			bport = th->th_sport;
			pf_change_ap(&h->ip_src.s_addr, &th->th_sport,
			    &h->ip_sum, &th->th_sum, nat->daddr,
			    htons(pf_next_port_tcp));
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_TCP, h->ip_dst.s_addr,
		    th->th_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = th->th_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, th->th_dport);
			else
				nport = rdr->rport;

			pf_change_ap(&h->ip_dst.s_addr, &th->th_dport,
			    &h->ip_sum, &th->th_sum, rdr->raddr, nport);
			rewrite++;
		}
	}

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if (r->action == PF_SCRUB)
			continue;
		if (MATCH_TUPLE(h, r, direction, ifp) &&
		    ((th->th_flags & r->flagset) == r->flags) &&
		    (!r->dst.port_op || pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], th->th_dport)) &&
		    (!r->src.port_op || pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], th->th_sport)) ) {
			rm = r;
			if (r->quick)
				break;
		}
	}

	if (rm != NULL) {
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if ((rm->action == PF_DROP) &&
		    (rm->return_rst || rm->return_icmp)) {
			/* undo NAT/RST changes, if they have taken place */
			if (nat != NULL) {
				pf_change_ap(&h->ip_src.s_addr, &th->th_sport,
				    &h->ip_sum, &th->th_sum, baddr, bport);
				rewrite++;
			} else if (rdr != NULL) {
				pf_change_ap(&h->ip_dst.s_addr, &th->th_dport,
				    &h->ip_sum, &th->th_sum, baddr, bport);
				rewrite++;
			}
			if (rm->return_rst)
				pf_send_reset(h, off, th);
			else
				pf_send_icmp(m, rm->return_icmp >> 8,
				    rm->return_icmp & 255);
		}

		if (rm->action == PF_DROP)
			return (PF_DROP);
	}

	if (((rm != NULL) && rm->keep_state) || (nat != NULL) || (rdr != NULL))
	    {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = h->ip_len - off - (th->th_off << 2);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);

		s->rule = rm;
		s->log = rm && (rm->log & 2);
		s->proto = IPPROTO_TCP;
		s->direction = direction;
		if (direction == PF_OUT) {
			s->gwy.addr = h->ip_src.s_addr;
			s->gwy.port = th->th_sport;
			s->ext.addr = h->ip_dst.s_addr;
			s->ext.port = th->th_dport;
			if (nat != NULL) {
				s->lan.addr = baddr;
				s->lan.port = bport;
				pf_next_port_tcp++;
				if (pf_next_port_tcp == 65535)
					pf_next_port_tcp = 50001;
			} else {
				s->lan.addr = s->gwy.addr;
				s->lan.port = s->gwy.port;
			}
		} else {
			s->lan.addr = h->ip_dst.s_addr;
			s->lan.port = th->th_dport;
			s->ext.addr = h->ip_src.s_addr;
			s->ext.port = th->th_sport;
			if (rdr != NULL) {
				s->gwy.addr = baddr;
				s->gwy.port = bport;
			} else {
				s->gwy.addr = s->lan.addr;
				s->gwy.port = s->lan.port;
			}
		}
		s->src.seqlo = ntohl(th->th_seq) + len +
		    ((th->th_flags & TH_SYN) ? 1 : 0) +
		    ((th->th_flags & TH_FIN) ? 1 : 0);
		s->src.seqhi = s->src.seqlo + 1;
		s->src.max_win = MAX(ntohs(th->th_win), 1);

		s->dst.seqlo = 0;	/* Haven't seen these yet */
		s->dst.seqhi = 1;
		s->dst.max_win = 1;
		s->src.state = 1;
		s->dst.state = 0;
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + 60;
		s->packets = 1;
		s->bytes = len;
		pf_insert_state(s);
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);
}

int
pf_test_udp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct udphdr *uh)
{
	struct pf_nat *nat = NULL;
	struct pf_rdr *rdr = NULL;
	u_int32_t baddr;
	u_int16_t bport, nport;
	struct pf_rule *r, *rm = NULL;
	u_short reason;
	int rewrite = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = pf_get_nat(ifp, IPPROTO_UDP, h->ip_src.s_addr)) !=
		    NULL) {
			baddr = h->ip_src.s_addr;
			bport = uh->uh_sport;
			pf_change_ap(&h->ip_src.s_addr, &uh->uh_sport,
			    &h->ip_sum, &uh->uh_sum, nat->daddr,
			    htons(pf_next_port_udp));
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = pf_get_rdr(ifp, IPPROTO_UDP, h->ip_dst.s_addr,
		    uh->uh_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = uh->uh_dport;
			if (rdr->opts & PF_RPORT_RANGE)
				nport = pf_map_port_range(rdr, uh->uh_dport);
			else
				nport = rdr->rport;

			pf_change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
			    &h->ip_sum, &uh->uh_sum, rdr->raddr, 
			    nport);

			rewrite++;
		}
	}

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if (r->action == PF_SCRUB)
			continue;
		if (MATCH_TUPLE(h, r, direction, ifp) &&
		    (!r->dst.port_op || pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], uh->uh_dport)) &&
		    (!r->src.port_op || pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], uh->uh_sport))) {
			rm = r;
			if (r->quick)
				break;
		}
	}

	if (rm != NULL) {
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if ((rm->action == PF_DROP) && rm->return_icmp) {
			/* undo NAT/RST changes, if they have taken place */
			if (nat != NULL) {
				pf_change_ap(&h->ip_src.s_addr, &uh->uh_sport,
				    &h->ip_sum, &uh->uh_sum, baddr, bport);
				rewrite++;
			} else if (rdr != NULL) {
				pf_change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
				    &h->ip_sum, &uh->uh_sum, baddr, bport);
				rewrite++;
			}
			pf_send_icmp(m, rm->return_icmp >> 8,
			    rm->return_icmp & 255);
		}

		if (rm->action == PF_DROP)
			return (PF_DROP);
	}

	if ((rm != NULL && rm->keep_state) || nat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = h->ip_len - off - sizeof(*uh);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);

		s->rule = rm;
		s->log = rm && (rm->log & 2);
		s->proto = IPPROTO_UDP;
		s->direction = direction;
		if (direction == PF_OUT) {
			s->gwy.addr = h->ip_src.s_addr;
			s->gwy.port = uh->uh_sport;
			s->ext.addr = h->ip_dst.s_addr;
			s->ext.port = uh->uh_dport;
			if (nat != NULL) {
				s->lan.addr = baddr;
				s->lan.port = bport;
				pf_next_port_udp++;
				if (pf_next_port_udp == 65535)
					pf_next_port_udp = 50001;
			} else {
				s->lan.addr = s->gwy.addr;
				s->lan.port = s->gwy.port;
			}
		} else {
			s->lan.addr = h->ip_dst.s_addr;
			s->lan.port = uh->uh_dport;
			s->ext.addr = h->ip_src.s_addr;
			s->ext.port = uh->uh_sport;
			if (rdr != NULL) {
				s->gwy.addr = baddr;
				s->gwy.port = bport;
			} else {
				s->gwy.addr = s->lan.addr;
				s->gwy.port = s->lan.port;
			}
		}
		s->src.seqlo  = 0;
		s->src.seqhi = 0;
		s->src.max_win = 0;
		s->src.state = 1;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.max_win = 0;
		s->dst.state = 0;
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + 30;
		s->packets = 1;
		s->bytes = len;
		pf_insert_state(s);
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);

	return (PF_PASS);
}

int
pf_test_icmp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct icmp *ih)
{
	struct pf_nat *nat = NULL;
	u_int32_t baddr;
	struct pf_rule *r, *rm = NULL;
	u_short reason;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = pf_get_nat(ifp, IPPROTO_ICMP, h->ip_src.s_addr)) !=
		    NULL) {
			baddr = h->ip_src.s_addr;
			pf_change_a(&h->ip_src.s_addr, &h->ip_sum, nat->daddr);
		}
	}

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if (r->action == PF_SCRUB)
			continue;
		if (MATCH_TUPLE(h, r, direction, ifp) &&
		    (!r->type || (r->type == ih->icmp_type + 1)) &&
		    (!r->code || (r->code == ih->icmp_code + 1)) ) {
			rm = r;
			if (r->quick)
				break;
		}
	}

	if (rm != NULL) {
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if (rm->action != PF_PASS)
			return (PF_DROP);
	}

	if ((rm != NULL && rm->keep_state) || nat != NULL) {
		/* create new state */
		u_int16_t len;
		u_int16_t id;
		struct pf_state *s;

		len = h->ip_len - off - ICMP_MINLEN;
		id = ih->icmp_id;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL)
			return (PF_DROP);

		s->rule	 = rm;
		s->log	 = rm && (rm->log & 2);
		s->proto = IPPROTO_ICMP;
		s->direction = direction;
		if (direction == PF_OUT) {
			s->gwy.addr = h->ip_src.s_addr;
			s->gwy.port = id;
			s->ext.addr = h->ip_dst.s_addr;
			s->ext.port = id;
			s->lan.addr = nat ? baddr : s->gwy.addr;
			s->lan.port = id;
		} else {
			s->lan.addr = h->ip_dst.s_addr;
			s->lan.port = id;
			s->ext.addr = h->ip_src.s_addr;
			s->ext.port = id;
			s->gwy.addr = s->lan.addr;
			s->gwy.port = id;
		}
		s->src.seqlo = 0;
		s->src.seqhi = 0;
		s->src.max_win = 0;
		s->src.state = 0;
		s->dst.seqlo = 0;
		s->dst.seqhi = 0;
		s->dst.max_win = 0;
		s->dst.state = 0;
		s->creation = pftv.tv_sec;
		s->expire = pftv.tv_sec + 20;
		s->packets = 1;
		s->bytes = len;
		pf_insert_state(s);
	}

	return (PF_PASS);
}

int
pf_test_other(int direction, struct ifnet *ifp, struct mbuf *m, struct ip *h)
{
	struct pf_rule *r, *rm = NULL;

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if (r->action == PF_SCRUB)
			continue;
		if (MATCH_TUPLE(h, r, direction, ifp)) {
			rm = r;
			if (r->quick)
				break;
		}
	}

	if (rm != NULL) {
		u_short reason;

		REASON_SET(&reason, PFRES_MATCH);
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if (rm->action != PF_PASS)
			return (PF_DROP);
	}
	return (PF_PASS);
}

int
pf_test_state_tcp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, struct ip *h, struct tcphdr *th)
{
	struct pf_tree_key key;
	u_int16_t len = h->ip_len - off - (th->th_off << 2);
	u_int16_t win = ntohs(th->th_win);
	u_int32_t seq = ntohl(th->th_seq), ack = ntohl(th->th_ack);
	u_int32_t end = seq + len + ((th->th_flags & TH_SYN) ? 1 : 0) +
	    ((th->th_flags & TH_FIN) ? 1 : 0);
	int ackskew;
	struct pf_state_peer *src, *dst;

	key.proto   = IPPROTO_TCP;
	key.addr[0] = h->ip_src;
	key.port[0] = th->th_sport;
	key.addr[1] = h->ip_dst;
	key.port[1] = th->th_dport;

	*state = pf_find_state((direction == PF_IN) ? tree_ext_gwy :
	    tree_lan_ext, &key);
	if (*state == NULL)
		return (PF_DROP);

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	if (src->seqlo == 0) {
		/* First packet from this end.  Set its state */
		src->seqlo = end;
		src->seqhi = end + 1;
		src->max_win = 1;
	}

	if ((th->th_flags & TH_ACK) == 0) {
		/* Let it pass through the ack skew check */
		ack = dst->seqlo;
	} else if (ack == 0 &&
	    (th->th_flags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) {
		/* broken tcp stacks do not set ack */
		ack = dst->seqlo;
	}

	if (seq == end) {
		/* Ease sequencing restrictions on no data packets */
		seq = src->seqlo;
		end = seq;
	}

	ackskew = dst->seqlo - ack;

#define MAXACKWINDOW (0xffff + 1500)
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - dst->max_win) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one window back */
	    (ackskew <= MAXACKWINDOW)) {
	    /* Acking not more than one window forward */

		if (ackskew < 0) {
			/* The sequencing algorithm is exteremely lossy
			 * when there is fragmentation since the full
			 * packet length can not be determined.  So we
			 * deduce how much data passed by what the
			 * other endpoint ACKs.  Thanks Guido!
			 * (Why MAXACKWINDOW is used)
			 */
			dst->seqlo = ack;
		}

		(*state)->packets++;
		(*state)->bytes += len;

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
			if (src->state < 1)
				src->state = 1;
		if (th->th_flags & TH_FIN)
			if (src->state < 3)
				src->state = 3;
		if (th->th_flags & TH_ACK) {
			if (dst->state == 1)
				dst->state = 2;
			else if (dst->state == 3)
				dst->state = 4;
		}
		if (th->th_flags & TH_RST)
			src->state = dst->state = 5;

		/* update expire time */
		if (src->state >= 4 && dst->state >= 4)
			(*state)->expire = pftv.tv_sec + 5;
		else if (src->state >= 3 && dst->state >= 3)
			(*state)->expire = pftv.tv_sec + 300;
		else if (src->state < 2 || dst->state < 2)
			(*state)->expire = pftv.tv_sec + 30;
		else
			(*state)->expire = pftv.tv_sec + 24*60*60;

		/* translate source/destination address, if needed */
		if ((*state)->lan.addr != (*state)->gwy.addr ||
		    (*state)->lan.port != (*state)->gwy.port) {
			if (direction == PF_OUT)
				pf_change_ap(&h->ip_src.s_addr,
				    &th->th_sport, &h->ip_sum,
				    &th->th_sum, (*state)->gwy.addr,
				    (*state)->gwy.port);
			else
				pf_change_ap(&h->ip_dst.s_addr,
				    &th->th_dport, &h->ip_sum,
				    &th->th_sum, (*state)->lan.addr,
				    (*state)->lan.port);
			m_copyback(m, off, sizeof(*th), (caddr_t)th);
		}

		return (PF_PASS);

	} else {
		/* XXX Remove these printfs before release */
		printf("pf: BAD state: ");
		pf_print_state(direction, *state);
		pf_print_flags(th->th_flags);
		printf(" seq=%lu ack=%lu len=%u ", seq, ack, len);
		printf("\n");
		printf("State failure: %c %c %c %c\n",
		    SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
		    SEQ_GEQ(seq, src->seqlo - dst->max_win) ? ' ': '2',
		    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
		    (ackskew <= MAXACKWINDOW) ? ' ' : '4');

		return (PF_DROP);
	}
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, struct ip *h, struct udphdr *uh)
{
	u_int16_t len = h->ip_len - off - sizeof(*uh);
	struct pf_state_peer *src, *dst;
	struct pf_tree_key key;

	key.proto   = IPPROTO_UDP;
	key.addr[0] = h->ip_src;
	key.port[0] = uh->uh_sport;
	key.addr[1] = h->ip_dst;
	key.port[1] = uh->uh_dport;

	(*state) = pf_find_state((direction == PF_IN) ? tree_ext_gwy :
	    tree_lan_ext, &key);
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
	(*state)->bytes += len;

	/* update states */
	if (src->state < 1)
		src->state = 1;
	if (dst->state == 1)
		dst->state = 2;

	/* update expire time */
	if (src->state == 2 && dst->state == 2)
		(*state)->expire = pftv.tv_sec + 60;
	else
		(*state)->expire = pftv.tv_sec + 20;

	/* translate source/destination address, if necessary */
	if ((*state)->lan.addr != (*state)->gwy.addr ||
	    (*state)->lan.port != (*state)->gwy.port) {
		if (direction == PF_OUT)
			pf_change_ap(&h->ip_src.s_addr, &uh->uh_sport,
			    &h->ip_sum, &uh->uh_sum,
			    (*state)->gwy.addr, (*state)->gwy.port);
		else
			pf_change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
			    &h->ip_sum, &uh->uh_sum,
			    (*state)->lan.addr, (*state)->lan.port);
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
	}

	return (PF_PASS);
}

int
pf_test_state_icmp(struct pf_state **state, int direction, struct ifnet *ifp,
    struct mbuf *m, int ipoff, int off, struct ip *h, struct icmp *ih)
{
	u_int16_t len = h->ip_len - off - sizeof(*ih);

	if (ih->icmp_type != ICMP_UNREACH &&
	    ih->icmp_type != ICMP_SOURCEQUENCH &&
	    ih->icmp_type != ICMP_REDIRECT &&
	    ih->icmp_type != ICMP_TIMXCEED &&
	    ih->icmp_type != ICMP_PARAMPROB) {

		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		struct pf_tree_key key;

		key.proto   = IPPROTO_ICMP;
		key.addr[0] = h->ip_src;
		key.port[0] = ih->icmp_id;
		key.addr[1] = h->ip_dst;
		key.port[1] = ih->icmp_id;

		*state = pf_find_state((direction == PF_IN) ? tree_ext_gwy :
		    tree_lan_ext, &key);
		if (*state == NULL)
			return (PF_DROP);

		(*state)->packets++;
		(*state)->bytes += len;
		(*state)->expire = pftv.tv_sec + 10;

		/* translate source/destination address, if needed */
		if ((*state)->lan.addr != (*state)->gwy.addr) {
			if (direction == PF_OUT)
				pf_change_a(&h->ip_src.s_addr,
				    &h->ip_sum, (*state)->gwy.addr);
			else
				pf_change_a(&h->ip_dst.s_addr,
				    &h->ip_sum, (*state)->lan.addr);
		}

		return (PF_PASS);

	} else {

		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct ip h2;
		int ipoff2;
		int off2;

		ipoff2 = off + ICMP_MINLEN;	/* offset of h2 in mbuf chain */
		if (!pf_pull_hdr(ifp, m, 0, ipoff2, &h2, sizeof(h2), h,
		    NULL, NULL)) {
			printf("pf: ICMP error message too short (ip)\n");
			return (PF_DROP);
		}

		/* offset of protocol header that follows h2 */
		off2 = ipoff2 + (h2.ip_hl << 2);

		switch (h2.ip_p) {
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
			if (!pf_pull_hdr(ifp, m, ipoff2, off2, &th, 8,
			    &h2, NULL, NULL)) {
				printf("pf: "
				    "ICMP error message too short (tcp)\n");
				return (PF_DROP);
			}
			seq = ntohl(th.th_seq);

			key.proto   = IPPROTO_TCP;
			key.addr[0] = h2.ip_dst;
			key.port[0] = th.th_dport;
			key.addr[1] = h2.ip_src;
			key.port[1] = th.th_sport;

			*state = pf_find_state((direction == PF_IN) ?
			    tree_ext_gwy : tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			src = (direction == (*state)->direction) ?
			    &(*state)->dst : &(*state)->src;
			dst = (direction == (*state)->direction) ?
			    &(*state)->src : &(*state)->dst;

			if (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq, src->seqlo - dst->max_win)) {

				printf("pf: BAD ICMP state: ");
				pf_print_state(direction, *state);
				printf(" seq=%lu\n", seq);
				return (PF_DROP);
			}

			if ((*state)->lan.addr != (*state)->gwy.addr ||
			    (*state)->lan.port != (*state)->gwy.port) {
				if (direction == PF_IN) {
					pf_change_icmp(&h2.ip_src.s_addr,
					    &th.th_sport, &h->ip_dst.s_addr,
					    (*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					pf_change_icmp(&h2.ip_dst.s_addr,
					    &th.th_dport, &h->ip_src.s_addr,
					    (*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
				m_copyback(m, off, ICMP_MINLEN, (caddr_t)ih);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, 8,
				    (caddr_t)&th);
			}

			return (PF_PASS);
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr uh;
			struct pf_tree_key key;

			if (!pf_pull_hdr(ifp, m, ipoff2, off2, &uh, sizeof(uh),
			    &h2, NULL, NULL)) {
				printf("pf: ICMP error message too short (udp)\n");
				return (PF_DROP);
			}

			key.proto   = IPPROTO_UDP;
			key.addr[0] = h2.ip_dst;
			key.port[0] = uh.uh_dport;
			key.addr[1] = h2.ip_src;
			key.port[1] = uh.uh_sport;

			*state = pf_find_state(direction == PF_IN ?
			    tree_ext_gwy : tree_lan_ext, &key);
			if (*state == NULL)
				return (PF_DROP);

			if ((*state)->lan.addr != (*state)->gwy.addr ||
			    (*state)->lan.port != (*state)->gwy.port) {
				if (direction == PF_IN) {
					pf_change_icmp(&h2.ip_src.s_addr,
					    &uh.uh_sport, &h->ip_dst.s_addr,
					    (*state)->lan.addr,
					    (*state)->lan.port, &uh.uh_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					pf_change_icmp(&h2.ip_dst.s_addr,
					    &uh.uh_dport, &h->ip_src.s_addr,
					    (*state)->gwy.addr,
					    (*state)->gwy.port, &uh.uh_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
				m_copyback(m, off, ICMP_MINLEN, (caddr_t)ih);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, sizeof(uh),
				    (caddr_t)&uh);
			}

			return (PF_PASS);
			break;
		}
		default:
			printf("pf: ICMP error message for bad proto\n");
			return (PF_DROP);
		}

	}
}

#define FRAG_EXPIRE	30

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment *frag;
	struct timeval now, expire;

	microtime(&now);

	timerclear(&expire);
	expire.tv_sec = FRAG_EXPIRE;
	timersub(&now, &expire, &expire);
	
	while ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) != NULL) {
		if (timercmp(&frag->fr_timeout, &expire, >))
			break;

		DPFPRINTF((__FUNCTION__": expiring %p\n", frag));
		pf_free_fragment(frag);
	}

}

/*
 *  Try to flush old fragments to make space for new ones
 */

void
pf_flush_fragments(void)
{
	struct pf_fragment *frag;
	int goal = pf_nfrents * 9 / 10;

	DPFPRINTF((__FUNCTION__": trying to free > %d frents\n",
		   pf_nfrents - goal));
	
	while (goal < pf_nfrents) {
		frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue);
		if (frag == NULL)
			break;
		pf_free_fragment(frag);
	}
}

/* Frees the fragments and all associated entries */

void
pf_free_fragment(struct pf_fragment *frag)
{
	struct pf_frent *frent;

	/* Free all fragments */
	for (frent = LIST_FIRST(&frag->fr_queue); frent;
	    frent = LIST_FIRST(&frag->fr_queue)) {
		LIST_REMOVE(frent, fr_next);

		m_freem(frent->fr_m);
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
	}

	pf_remove_fragment(frag);
}

void
pf_ip2key(struct pf_tree_key *key, struct ip *ip)
{
		key->proto = ip->ip_p;
		key->addr[0] = ip->ip_src;
		key->addr[1] = ip->ip_dst;
		key->port[0] = ip->ip_id;
		key->port[1] = 0;
}

struct pf_fragment *
pf_find_fragment(struct ip *ip)
{
		struct pf_tree_key key;
		struct pf_fragment *frag;

		pf_ip2key(&key, ip);
		
		frag = (struct pf_fragment *)pf_find_state(tree_fragment,
		    &key);

		if (frag != NULL) {
			microtime(&frag->fr_timeout);
			TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
			TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);
		}
		
		return (frag);
}

/* Removes a fragment from the fragment queue and frees the fragment */

void
pf_remove_fragment(struct pf_fragment *frag)
{
		struct pf_tree_key key;
		
		key.proto = frag->fr_p;
		key.addr[0] = frag->fr_src;
		key.addr[1] = frag->fr_dst;
		key.port[0] = frag->fr_id;
		key.port[1] = 0;

		pf_tree_remove(&tree_fragment, NULL, &key);
		TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);

		pool_put(&pf_frag_pl, frag);
}

struct mbuf *
pf_reassemble(struct mbuf **m0, struct pf_fragment *frag,
    struct pf_frent *frent, int mff)
{
	struct mbuf *m = *m0, *m2;
	struct pf_frent *frep, *frea, *next;
	struct ip *ip = frent->fr_ip;
	int hlen = ip->ip_hl << 2;
	u_int16_t off = ip->ip_off;
	u_int16_t max = ip->ip_len + off;

	/* Strip off ip header */
	m->m_data += hlen;
	m->m_len -= hlen;

	/* Create a new reassembly queue for this packet */
	if (frag == NULL) {
		struct pf_tree_key key;
		
		frag = pool_get(&pf_frag_pl, M_NOWAIT);
		if (frag == NULL) {
			pf_flush_fragments();
			frag = pool_get(&pf_frag_pl, M_NOWAIT);
			if (frag == NULL)
				goto drop_fragment;
		}

		frag->fr_flags = 0;
		frag->fr_max = 0;
		frag->fr_src = frent->fr_ip->ip_src;
		frag->fr_dst = frent->fr_ip->ip_dst;
		frag->fr_p = frent->fr_ip->ip_p;
		frag->fr_id = frent->fr_ip->ip_id;
		LIST_INIT(&frag->fr_queue);

		pf_ip2key(&key, frent->fr_ip);

		pf_tree_insert(&tree_fragment, NULL, &key,
		    (struct pf_state *)frag);
		TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);

		/* We do not have a previous fragment */
		frep = NULL;
		goto insert;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	LIST_FOREACH(frea, &frag->fr_queue, fr_next) {
		if (frea->fr_ip->ip_off > off)
			break;
		frep = frea;
	}

	KASSERT(frep != NULL || frea != NULL);
	
	if (frep != NULL) {
		u_int16_t precut;

		precut = frep->fr_ip->ip_off + frep->fr_ip->ip_len - off;
		if (precut > ip->ip_len)
			goto drop_fragment;
		if (precut) {
			m_adj(frent->fr_m, precut);

			DPFPRINTF((__FUNCTION__": overlap -%d\n", precut));
			/* Enforce 8 byte boundaries */
			off = ip->ip_off += precut;
			ip->ip_len -= precut;
		}
	}

	for (; frea != NULL && ip->ip_len + off > frea->fr_ip->ip_off;
	    frea = next) {
		u_int16_t aftercut;
		
		aftercut = (ip->ip_len + off) - frea->fr_ip->ip_off;
		DPFPRINTF((__FUNCTION__": adjust overlap %d\n", aftercut));
		if (aftercut < frea->fr_ip->ip_len) {
			frea->fr_ip->ip_len -= aftercut;
			frea->fr_ip->ip_off += aftercut;
			m_adj(frea->fr_m, aftercut);
			break;
		}

		/* This fragment is completely overlapped, loose it */
		next = LIST_NEXT(frea, fr_next);
		m_freem(frea->fr_m);
		LIST_REMOVE(frea, fr_next);
		pool_put(&pf_frent_pl, frea);
		pf_nfrents--;
	}

 insert:
	/* Update maxmimum data size */
	if (frag->fr_max < max)
		frag->fr_max = max;
	/* This is the last segment */
	if (!mff)
		frag->fr_flags |= PFFRAG_SEENLAST;

	if (frep == NULL)
		LIST_INSERT_HEAD(&frag->fr_queue, frent, fr_next);
	else
		LIST_INSERT_AFTER(frep, frent, fr_next);
	
	/* Check if we are completely reassembled */
	if (!(frag->fr_flags & PFFRAG_SEENLAST))
		return (NULL);

	/* Check if we have all the data */
	off = 0;
	for (frep = LIST_FIRST(&frag->fr_queue); frep; frep = next) {
		next = LIST_NEXT(frep, fr_next);
		
		off += frep->fr_ip->ip_len;
		if (off < frag->fr_max &&
		    (next == NULL || next->fr_ip->ip_off != off)) {
			DPFPRINTF((__FUNCTION__": missing fragment at %d, next %d, max %d\n",
				  off, next == NULL ? -1 : next->fr_ip->ip_off, frag->fr_max));
			return (NULL);
		}
	}
	DPFPRINTF((__FUNCTION__": %d < %d?\n", off, frag->fr_max));
	if (off < frag->fr_max)
		return (NULL);

	/* We have all the data */
	frent = LIST_FIRST(&frag->fr_queue);
	KASSERT(frent != NULL);
	if ((frent->fr_ip->ip_hl << 2) + off > IP_MAXPACKET) {
		DPFPRINTF((__FUNCTION__": drop: too big: %d\n", off));
		pf_free_fragment(frag);
		return (NULL);
	}
	next = LIST_NEXT(frent, fr_next);

	/* Magic from ip_input */
	ip = frent->fr_ip;
	m = frent->fr_m;
	m2 = m->m_next;
	m->m_next = NULL;
	m_cat(m, m2);
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	for (frent = next; frent != NULL; frent = next) {
		next = LIST_NEXT(frent, fr_next);

		m2 = frent->fr_m;
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
		m_cat(m, m2);
	}

	ip->ip_src = frag->fr_src;
	ip->ip_dst = frag->fr_dst;

	/* Remove from fragment queue */
	pf_remove_fragment(frag);
	
	hlen = ip->ip_hl << 2;
	ip->ip_len = off + hlen;
	m->m_len += hlen;
	m->m_data -= hlen;

	/* some debugging cruft by sklower, below, will go away soon */
	/* XXX this should be done elsewhere */
	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m2 = m; m2; m2 = m2->m_next)
			plen += m2->m_len;
		m->m_pkthdr.len = plen;
	}

	DPFPRINTF((__FUNCTION__": complete: %p(%d)\n", m, ip->ip_len));
	
	return (m);
	
 drop_fragment:
	/* Oops - fail safe - drop packet */
	m_freem(m);
	return (NULL);
}

int
pf_normalize_ip(struct mbuf **m0, int dir, struct ifnet *ifp, u_short *reason)
{
	struct mbuf *m = *m0;
	struct pf_rule *r;
	struct pf_frent *frent;
	struct pf_fragment *frag;
	struct ip *h = mtod(m, struct ip *);
	int mff = (h->ip_off & IP_MF), hlen = h->ip_hl << 2;
	u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;
	u_int16_t max;

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if ((r->action == PF_SCRUB) &&
		    MATCH_TUPLE(h, r, dir, ifp))
			break;
	}

	if (r == NULL)
		return (PF_PASS);

	/* Check for illegal packets */
	if (hlen < sizeof(struct ip))
		goto drop;

	if (hlen > h->ip_len)
		goto drop;
	
	/* We will need other tests here */
	if (!fragoff && !mff)
		goto no_fragment;
	
	/* Now we are dealing with a fragmented packet */
	frag = pf_find_fragment(h);

	/* This can not happen */
	if (h->ip_off & IP_DF) {
		DPFPRINTF((__FUNCTION__": IP_DF\n"));
		goto bad;
	}

	h->ip_len -= hlen;
	h->ip_off <<= 3;
	
	/* All fragments are 8 byte aligned */
	if (mff && (h->ip_len & 0x7)) {
		DPFPRINTF((__FUNCTION__": mff and %d\n", h->ip_len));
		goto bad;
	}

	max = fragoff + h->ip_len;
	/* Respect maximum length */
	if (max > IP_MAXPACKET) {
		DPFPRINTF((__FUNCTION__": max packet %d\n", max));
		goto bad;
	}
	/* Check if we saw the last fragment already */
	if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
	    max > frag->fr_max)
		goto bad;

	/* Get an entry for the fragment queue */
	frent = pool_get(&pf_frent_pl, PR_NOWAIT);
	if (frent == NULL) {
		/* Try to clean up old fragments */
		pf_flush_fragments();
		frent = pool_get(&pf_frent_pl, PR_NOWAIT);
		if (frent == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return (PF_DROP);
		}
	}
	pf_nfrents++;
	frent->fr_ip = h;
	frent->fr_m = m;

	/* Might return a completely reassembled mbuf, or NULL */
	DPFPRINTF((__FUNCTION__": reass frag %d @ %d\n", h->ip_id, fragoff));
	*m0 = m = pf_reassemble(m0, frag, frent, mff);

	if (m == NULL)
		return (PF_DROP);

	h = mtod(m, struct ip *);
	
 no_fragment:
	if (dir != PF_OUT)
		return (PF_PASS);
	    
	return (PF_PASS);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(h, m, AF_INET, dir, *reason, r);
	return (PF_DROP);
	
 bad:
	DPFPRINTF((__FUNCTION__": dropping bad fragment\n"));
	
	/* Free assoicated fragments */
	if (frag != NULL)
		pf_free_fragment(frag);

	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(h, m, AF_INET, dir, *reason, r);

	return (PF_DROP);
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pf_pull_hdr(struct ifnet *ifp, struct mbuf *m, int ipoff, int off, void *p,
    int len, struct ip *h, u_short *actionp, u_short *reasonp)
{
	u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;

	/* sanity check */
	if (ipoff > off) {
		ACTION_SET(actionp, PF_DROP);
		REASON_SET(reasonp, PFRES_BADOFF);
		return (NULL);
	}
	if (fragoff) {
		if (fragoff >= len)
			ACTION_SET(actionp, PF_PASS);
		else {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_FRAG);
		}
		return (NULL);
	}
	if (m->m_pkthdr.len < off + len || ipoff + h->ip_len < off + len) {
		ACTION_SET(actionp, PF_DROP);
		REASON_SET(reasonp, PFRES_SHORT);
		return (NULL);
	}
	m_copydata(m, off, len, p);
	return (p);
}

int
pf_test(int dir, struct ifnet *ifp, struct mbuf **m0)
{
	u_short action, reason = 0, log = 0;
	struct mbuf *m = *m0;
	struct ip *h;
	struct pf_rule *r = NULL;
	struct pf_state *s;
	int off;

	if (!pf_status.running ||
	    (m_tag_find(m, PACKET_TAG_PF_GENERATED, NULL) != NULL))
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif

	/* purge expire states, at most once every 10 seconds */
	microtime(&pftv);
	if (pftv.tv_sec - pf_last_purge >= 10) {
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

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		struct tcphdr th;

		if (!pf_pull_hdr(ifp, m, 0, off, &th, sizeof(th), h,
		    &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_tcp(&s, dir, ifp, m, 0, off, h , &th);
		if (action == PF_PASS) {
			r = s->rule;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_tcp(dir, ifp, m, 0, off, h , &th);
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr uh;

		if (!pf_pull_hdr(ifp, m, 0, off, &uh, sizeof(uh), h,
		    &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_udp(&s, dir, ifp, m, 0, off, h, &uh);
		if (action == PF_PASS) {
			r = s->rule;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_udp(dir, ifp, m, 0, off, h, &uh);
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp ih;

		if (!pf_pull_hdr(ifp, m, 0, off, &ih, ICMP_MINLEN, h,
		    &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, ifp, m, 0, off, h, &ih);
		if (action == PF_PASS) {
			r = s->rule;
			log = s->log;
		} else if (s == NULL)
			action = pf_test_icmp(dir, ifp, m, 0, off, h, &ih);
		break;
	}

	default:
		action = pf_test_other(dir, ifp, m, h);
		break;
	}

	if (ifp == status_ifp) {
		pf_status.bcounters[dir] += h->ip_len;
		pf_status.pcounters[dir][action]++;
	}

done:
	if (log) {
		struct pf_rule r0;

		if (r == NULL) {
			r0.ifp = ifp;
			r0.action = action;
			r0.nr = -1;
			r = &r0;
		}
		PFLOG_PACKET(h, m, AF_INET, dir, reason, r);
	}
	return (action);
}
