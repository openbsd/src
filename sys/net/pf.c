/*	$OpenBSD: pf.c,v 1.68 2001/06/27 01:57:17 provos Exp $ */

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
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include "bpfilter.h"
#include "pflog.h"

/*
 * Tree data structure
 */

struct pf_tree_node {
	struct pf_tree_key {
		u_int32_t	 addr[2];
		u_int16_t	 port[2];
		u_int8_t	 proto;
	}			 key;
	struct pf_state		*state;
	struct pf_tree_node	*left;
	struct pf_tree_node	*right;
	signed char		 balance;
};

/*
 * Global variables
 */

TAILQ_HEAD(pf_rulequeue, pf_rule)	pf_rules[2];
TAILQ_HEAD(pf_natqueue, pf_nat)		pf_nats[2];
TAILQ_HEAD(pf_rdrqueue, pf_rdr)		pf_rdrs[2];
TAILQ_HEAD(pf_statequeue, pf_state)	pf_states;
struct pf_rulequeue	*pf_rules_active;
struct pf_rulequeue	*pf_rules_inactive;
struct pf_natqueue	*pf_nats_active;
struct pf_natqueue	*pf_nats_inactive;
struct pf_rdrqueue	*pf_rdrs_active;
struct pf_rdrqueue	*pf_rdrs_inactive;
struct pf_tree_node	*tree_lan_ext, *tree_ext_gwy;
struct timeval		 pftv;
struct pf_status	 pf_status;
struct ifnet		*status_ifp;

u_int32_t		 pf_last_purge = 0;
u_int32_t		 ticket_rules_active = 0;
u_int32_t		 ticket_rules_inactive = 0;
u_int32_t		 ticket_nats_active = 0;
u_int32_t		 ticket_nats_inactive = 0;
u_int32_t		 ticket_rdrs_active = 0;
u_int32_t		 ticket_rdrs_inactive = 0;
u_int16_t		 pf_next_port_tcp = 50001;
u_int16_t		 pf_next_port_udp = 50001;

struct pool		pf_tree_pl;
struct pool		pf_rule_pl;
struct pool		pf_nat_pl;
struct pool		pf_rdr_pl;
struct pool		pf_state_pl;

/*
 * Prototypes
 */

int		 tree_key_compare(struct pf_tree_key *, struct pf_tree_key *);
void		 tree_rotate_left(struct pf_tree_node **);
void		 tree_rotate_right(struct pf_tree_node **);
int		 tree_insert(struct pf_tree_node **, struct pf_tree_key *,
		    struct pf_state *);
int		 tree_remove(struct pf_tree_node **, struct pf_tree_key *);
struct pf_state	*find_state(struct pf_tree_node *, struct pf_tree_key *);
void		 insert_state(struct pf_state *);
void		 purge_expired_states(void);

void		 print_ip(struct ifnet *, struct ip *);
void		 print_host(u_int32_t, u_int16_t);
void		 print_state(int, struct pf_state *);
void		 print_flags(u_int8_t);

void		 pfattach(int);
int		 pfopen(dev_t, int, int, struct proc *);
int		 pfclose(dev_t, int, int, struct proc *);
int		 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);

u_int16_t	 cksum_fixup(u_int16_t, u_int16_t, u_int16_t);
void		 change_ap(u_int32_t *, u_int16_t *, u_int16_t *, u_int16_t *,
		    u_int32_t, u_int16_t);
void		 change_a(u_int32_t *, u_int16_t *, u_int32_t);
void		 change_icmp(u_int32_t *, u_int16_t *, u_int32_t *, u_int32_t,
		    u_int16_t, u_int16_t *, u_int16_t *, u_int16_t *,
		    u_int16_t *);
void		 send_reset(int, struct ifnet *, struct ip *, int,
		    struct tcphdr *);
int		 match_addr(u_int8_t, u_int32_t, u_int32_t, u_int32_t);
int		 match_port(u_int8_t, u_int16_t, u_int16_t, u_int16_t);
struct pf_nat	*get_nat(struct ifnet *, u_int8_t, u_int32_t);
struct pf_rdr	*get_rdr(struct ifnet *, u_int8_t, u_int32_t, u_int16_t);
int		 pf_test_tcp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct tcphdr *);
int		 pf_test_udp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct udphdr *);
int		 pf_test_icmp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct icmp *);
struct pf_state	*pf_test_state_tcp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct tcphdr *);
struct pf_state	*pf_test_state_udp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct udphdr *);
struct pf_state	*pf_test_state_icmp(int, struct ifnet *, struct mbuf *,
		    int, int, struct ip *, struct icmp *);
void		*pull_hdr(struct ifnet *, struct mbuf *, int, int, void *, int,
		    struct ip *, u_short *, u_short *);
int		 pflog_packet(struct mbuf *, int, u_short, u_short,
		    struct pf_rule *);

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

int
tree_key_compare(struct pf_tree_key *a, struct pf_tree_key *b)
{
	/*
	 * could use memcmp(), but with the best manual order, we can
	 * minimize the number of average compares. what is faster?
	 */
	if (a->proto   < b->proto  )
		return (-1);
	if (a->proto   > b->proto  )
		return ( 1);
	if (a->addr[0] < b->addr[0])
		return (-1);
	if (a->addr[0] > b->addr[0])
		return ( 1);
	if (a->addr[1] < b->addr[1])
		return (-1);
	if (a->addr[1] > b->addr[1])
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
tree_rotate_left(struct pf_tree_node **p)
{
	struct pf_tree_node *q = *p;

	*p = (*p)->right;
	q->right = (*p)->left;
	(*p)->left = q;
	q->balance--;
	if ((*p)->balance > 0)
		q->balance -= (*p)->balance;
	(*p)->balance--;
	if (q->balance < 0)
		(*p)->balance += q->balance;
}

void
tree_rotate_right(struct pf_tree_node **p)
{
	struct pf_tree_node *q = *p;

	*p = (*p)->left;
	q->left = (*p)->right;
	(*p)->right = q;
	q->balance++;
	if ((*p)->balance < 0)
		q->balance -= (*p)->balance;
	(*p)->balance++;
	if (q->balance > 0)
		(*p)->balance += q->balance;
}

int
tree_insert(struct pf_tree_node **p, struct pf_tree_key *key, struct pf_state *state)
{
	int deltaH = 0;

	if (*p == NULL) {
		*p = pool_get(&pf_tree_pl, PR_NOWAIT);
		if (*p == NULL) {
			return (0);
		}
		bcopy(key, &(*p)->key, sizeof(struct pf_tree_key));
		(*p)->state = state;
		(*p)->balance = 0;
		(*p)->left = (*p)->right = NULL;
		deltaH = 1;
	} else if (tree_key_compare(key, &(*p)->key) > 0) {
		if (tree_insert(&(*p)->right, key, state)) {
			(*p)->balance++;
			if ((*p)->balance == 1)
				deltaH = 1;
			else if ((*p)->balance == 2) {
				if ((*p)->right->balance == -1)
					tree_rotate_right(&(*p)->right);
				tree_rotate_left(p);
			}
		}
	} else {
		if (tree_insert(&(*p)->left, key, state)) {
			(*p)->balance--;
			if ((*p)->balance == -1)
				deltaH = 1;
			else if ((*p)->balance == -2) {
				if ((*p)->left->balance == 1)
					tree_rotate_left(&(*p)->left);
				tree_rotate_right(p);
			}
		}
	}
	return (deltaH);
}

int
tree_remove(struct pf_tree_node **p, struct pf_tree_key *key)
{
	int deltaH = 0;
	int c;

	if (*p == NULL)
		return (0);
	c = tree_key_compare(key, &(*p)->key);
	if (c < 0) {
		if (tree_remove(&(*p)->left, key)) {
			(*p)->balance++;
			if ((*p)->balance == 0)
				deltaH = 1;
			else if ((*p)->balance == 2) {
				if ((*p)->right->balance == -1)
					tree_rotate_right(&(*p)->right);
				tree_rotate_left(p);
				if ((*p)->balance == 0)
					deltaH = 1;
			}
		}
	} else if (c > 0) {
		if (tree_remove(&(*p)->right, key)) {
			(*p)->balance--;
			if ((*p)->balance == 0)
				deltaH = 1;
			else if ((*p)->balance == -2) {
				if ((*p)->left->balance == 1)
					tree_rotate_left(&(*p)->left);
				tree_rotate_right(p);
				if ((*p)->balance == 0)
					deltaH = 1;
			}
		}
	} else {
		if ((*p)->right == NULL) {
			struct pf_tree_node *p0 = *p;

			*p = (*p)->left;
			pool_put(&pf_tree_pl, p0);
			deltaH = 1;
		} else if ((*p)->left == NULL) {
			struct pf_tree_node *p0 = *p;

			*p = (*p)->right;
			pool_put(&pf_tree_pl, p0);
			deltaH = 1;
		} else {
			struct pf_tree_node **qq = &(*p)->left;

			while ((*qq)->right != NULL)
				qq = &(*qq)->right;
			bcopy(&(*qq)->key, &(*p)->key, sizeof(struct pf_tree_key));
			(*p)->state = (*qq)->state;
			bcopy(key, &(*qq)->key, sizeof(struct pf_tree_key));
			if (tree_remove(&(*p)->left, key)) {
				(*p)->balance++;
				if ((*p)->balance == 0)
					deltaH = 1;
				else if ((*p)->balance == 2) {
					if ((*p)->right->balance == -1)
						tree_rotate_right(&(*p)->right);
					tree_rotate_left(p);
					if ((*p)->balance == 0)
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

struct pf_state *
find_state(struct pf_tree_node *p, struct pf_tree_key *key)
{
	int c;

	while (p && (c = tree_key_compare(&p->key, key)))
		p = (c > 0) ? p->left : p->right;
	pf_status.state_searches++;
	return (p ? p->state : NULL);
}

void
insert_state(struct pf_state *state)
{
	struct pf_tree_key key;

	key.proto = state->proto;
	key.addr[0] = state->lan.addr;
	key.port[0] = state->lan.port;
	key.addr[1] = state->ext.addr;
	key.port[1] = state->ext.port;
	/* sanity checks can be removed later, should never occur */
	if (find_state(tree_lan_ext, &key) != NULL)
		printf("pf: ERROR! insert invalid\n");
	else {
		tree_insert(&tree_lan_ext, &key, state);
		if (find_state(tree_lan_ext, &key) != state)
			printf("pf: ERROR! insert failed\n");
	}

	key.proto   = state->proto;
	key.addr[0] = state->ext.addr;
	key.port[0] = state->ext.port;
	key.addr[1] = state->gwy.addr;
	key.port[1] = state->gwy.port;
	if (find_state(tree_ext_gwy, &key) != NULL)
		printf("pf: ERROR! insert invalid\n");
	else {
		tree_insert(&tree_ext_gwy, &key, state);
		if (find_state(tree_ext_gwy, &key) != state)
			printf("pf: ERROR! insert failed\n");
	}

	TAILQ_INSERT_TAIL(&pf_states, state, entries);

	pf_status.state_inserts++;
	pf_status.states++;
}

void
purge_expired_states(void)
{
	struct pf_tree_key key;
	struct pf_state *cur, *next;

	cur = TAILQ_FIRST(&pf_states);
	while (cur != NULL) {
		if (cur->expire <= pftv.tv_sec) {
			key.proto = cur->proto;
			key.addr[0] = cur->lan.addr;
			key.port[0] = cur->lan.port;
			key.addr[1] = cur->ext.addr;
			key.port[1] = cur->ext.port;
			/* sanity checks can be removed later */
			if (find_state(tree_lan_ext, &key) != cur)
				printf("pf: ERROR! remove invalid\n");
			tree_remove(&tree_lan_ext, &key);
			if (find_state(tree_lan_ext, &key) != NULL)
				printf("pf: ERROR! remove failed\n");
			key.proto   = cur->proto;
			key.addr[0] = cur->ext.addr;
			key.port[0] = cur->ext.port;
			key.addr[1] = cur->gwy.addr;
			key.port[1] = cur->gwy.port;
			if (find_state(tree_ext_gwy, &key) != cur)
				printf("pf: ERROR! remove invalid\n");
			tree_remove(&tree_ext_gwy, &key);
			if (find_state(tree_ext_gwy, &key) != NULL)
				printf("pf: ERROR! remove failed\n");
			next = TAILQ_NEXT(cur, entries);
			TAILQ_REMOVE(&pf_states, cur, entries);
			pool_put(&pf_state_pl, cur);
			cur = next;
			pf_status.state_removals++;
			pf_status.states--;
		} else
			cur = TAILQ_NEXT(cur, entries);
	}
}

void
print_ip(struct ifnet *ifp, struct ip *h)
{
	u_int32_t a;
	printf(" %s:", ifp->if_xname);
	a = ntohl(h->ip_src.s_addr);
	printf(" %u.%u.%u.%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255);
	a = ntohl(h->ip_dst.s_addr);
	printf(" -> %u.%u.%u.%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255);
	printf(" hl=%u len=%u id=%u", h->ip_hl << 2, h->ip_len - (h->ip_hl << 2),
	 h->ip_id);
	if (h->ip_off & IP_RF)
		printf(" RF");
	if (h->ip_off & IP_DF)
		printf(" DF");
	if (h->ip_off & IP_MF)
		printf(" MF");
	printf(" off=%u proto=%u\n", (h->ip_off & IP_OFFMASK) << 3, h->ip_p);
}

void
print_host(u_int32_t a, u_int16_t p)
{
	a = ntohl(a);
	p = ntohs(p);
	printf("%u.%u.%u.%u:%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255, p);
}

void
print_state(int direction, struct pf_state *s)
{
	print_host(s->lan.addr, s->lan.port);
	printf(" ");
	print_host(s->gwy.addr, s->gwy.port);
	printf(" ");
	print_host(s->ext.addr, s->ext.port);
	printf(" [lo=%lu high=%lu win=%u]", s->src.seqlo, s->src.seqhi,
		 s->src.max_win);
	printf(" [lo=%lu high=%lu win=%u]", s->dst.seqlo, s->dst.seqhi,
		 s->dst.max_win);
	printf(" %u:%u", s->src.state, s->dst.state);
}

void
print_flags(u_int8_t f)
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
	TAILQ_INIT(&pf_rules[0]);
	TAILQ_INIT(&pf_rules[1]);
	TAILQ_INIT(&pf_nats[0]);
	TAILQ_INIT(&pf_nats[1]);
	TAILQ_INIT(&pf_rdrs[0]);
	TAILQ_INIT(&pf_rdrs[1]);
	TAILQ_INIT(&pf_states);
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
	
	if ((cmd != DIOCSTART) && (cmd != DIOCSTOP) && (cmd != DIOCCLRSTATES)) {
		if (addr == NULL) {
			return (EINVAL);
		}
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
		struct pf_state *state;

		if (*ticket != ticket_rules_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap rules, keep the old. */
		s = splsoftnet();
		/* Rules are about to get freed, clear rule pointers in states */
		TAILQ_FOREACH(state, &pf_states, entries) state->rule = NULL;
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
		nat = TAILQ_FIRST(pf_nats_active);
		while (nat != NULL) {
			pn->nr++;
			nat = TAILQ_NEXT(nat, entries);
		}
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
		rdr = TAILQ_FIRST(pf_rdrs_active);
		while (rdr != NULL) {
			pr->nr++;
			rdr = TAILQ_NEXT(rdr, entries);
		}
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
		struct pf_state *state = TAILQ_FIRST(&pf_states);
		s = splsoftnet();
		while (state != NULL) {
			state->expire = 0;
			state = TAILQ_NEXT(state, entries);
		}
		purge_expired_states();
		splx(s);
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state *ps = (struct pfioc_state *)addr;
		struct pf_state *state;
		u_int32_t nr;

		nr = 0;
		s = splsoftnet();
		state = TAILQ_FIRST(&pf_states);
		while ((state != NULL) && (nr < ps->nr)) {
			state = TAILQ_NEXT(state, entries);
			nr++;
		}
		if (state == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(state, &ps->state, sizeof(struct pf_state));
		splx(s);
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
		u_int8_t running = pf_status.running;
		u_int32_t states = pf_status.states;

		bcopy(&pf_status, s, sizeof(struct pf_status));
		if (s->since)
			s->since = pftv.tv_sec - s->since;
		else
			s->since = 0;
		bzero(&pf_status, sizeof(struct pf_status));
		pf_status.running = running;
		pf_status.states = states;
		pf_status.since = pftv.tv_sec;
		break;
	}

	default:
		error = ENODEV;
		break;
	}

	return (error);
}

u_int16_t
cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new)
{
	u_int32_t l = cksum + old - new;
	l = (l >> 16) + (l & 65535); 
	l = l & 65535;
	return (l ? l : 65535);
}  

void
change_ap(u_int32_t *a, u_int16_t *p, u_int16_t *ic, u_int16_t *pc, u_int32_t an,
    u_int16_t pn)
{
	u_int32_t ao = *a;
	u_int16_t po = *p;
	*a = an;
	*ic = cksum_fixup(cksum_fixup(*ic, ao / 65536, an / 65536), ao % 65536, an % 65536);
	*p = pn;
	*pc = cksum_fixup(cksum_fixup(cksum_fixup(*pc, ao / 65536, an / 65536), ao % 65536, an % 65536),
	    po, pn);
}

void
change_a(u_int32_t *a, u_int16_t *c, u_int32_t an)
{
	u_int32_t ao = *a;
	*a = an;
	*c = cksum_fixup(cksum_fixup(*c, ao / 65536, an / 65536), ao % 65536, an % 65536);
}

void
change_icmp(u_int32_t *ia, u_int16_t *ip, u_int32_t *oa, u_int32_t na,
    u_int16_t np, u_int16_t *pc, u_int16_t *h2c, u_int16_t *ic, u_int16_t *hc)
{
	u_int32_t oia = *ia, ooa = *oa, opc = *pc, oh2c = *h2c;
	u_int16_t oip = *ip;
	/* Change inner protocol port, fix inner protocol checksum. */
	*ip = np;
	*pc = cksum_fixup(*pc, oip, *ip);
	*ic = cksum_fixup(*ic, oip, *ip);
	*ic = cksum_fixup(*ic, opc, *pc);
	/* Change inner ip address, fix inner ip checksum and icmp checksum. */
	*ia = na;
	*h2c = cksum_fixup(cksum_fixup(*h2c, oia / 65536, *ia / 65536), oia % 65536, *ia % 65536);
	*ic = cksum_fixup(cksum_fixup(*ic, oia / 65536, *ia / 65536), oia % 65536, *ia % 65536);
	*ic = cksum_fixup(*ic, oh2c, *h2c);
	/* Change outer ip address, fix outer ip checksum. */
	*oa = na;
	*hc = cksum_fixup(cksum_fixup(*hc, ooa / 65536, *oa / 65536), ooa % 65536, *oa % 65536);
}

void
send_reset(int direction, struct ifnet *ifp, struct ip *h, int off,
    struct tcphdr *th)
{
	struct mbuf *m;
	int len = sizeof(struct ip) + sizeof(struct tcphdr);
	struct ip *h2;
	struct tcphdr *th2;

	/* don't reply to RST packets */
	if (th->th_flags & TH_RST)
		return;

	/* create outgoing mbuf */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m->m_data, len);
	h2 = mtod(m, struct ip *);

	/* IP header fields included in the TCP checksum */
	h2->ip_p = IPPROTO_TCP;
	h2->ip_len = htons(sizeof(struct tcphdr));
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
	h2->ip_hl = sizeof(struct ip) >> 2;
	h2->ip_len = htons(len);
	h2->ip_ttl = 128;
	h2->ip_sum = 0;

	/* IP header checksum */
	h2->ip_sum = in_cksum(m, sizeof(struct ip));

	if (direction == PF_IN) {
		/* set up route and send RST out through the same interface */
		struct route iproute;
		struct route *ro = &iproute;
		struct sockaddr_in *dst;
		int error;
		bzero(ro, sizeof(*ro));
		dst = (struct sockaddr_in *)&ro->ro_dst;
		dst->sin_family = AF_INET;
		dst->sin_addr = h2->ip_dst;
		dst->sin_len = sizeof(*dst);
		rtalloc(ro);
		if (ro->ro_rt != NULL)
			ro->ro_rt->rt_use++;
        	error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
		    ro->ro_rt);
	} else {
		/* send RST through the loopback interface */
		struct sockaddr_in dst;
		dst.sin_family = AF_INET;
		dst.sin_addr = h2->ip_dst;
		dst.sin_len = sizeof(struct sockaddr_in);
		m->m_pkthdr.rcvif = ifp;
		looutput(lo0ifp, m, sintosa(&dst), NULL);
	}
	return;
}

int
match_addr(u_int8_t n, u_int32_t a, u_int32_t m, u_int32_t b)
{
	return (n == !((a & m) == (b & m)));
}

int
match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
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
get_nat(struct ifnet *ifp, u_int8_t proto, u_int32_t addr)
{
	struct pf_nat *n, *nm = NULL;

	n = TAILQ_FIRST(pf_nats_active);
	while (n && nm == NULL) {
		if (n->ifp == ifp &&
		    (!n->proto || n->proto == proto) &&
		    match_addr(n->not, n->saddr, n->smask, addr))
			nm = n;
		else
			n = TAILQ_NEXT(n, entries);
	}
	return (nm);
}

struct pf_rdr *
get_rdr(struct ifnet *ifp, u_int8_t proto, u_int32_t addr, u_int16_t port)
{
	struct pf_rdr *r, *rm = NULL;

	r = TAILQ_FIRST(pf_rdrs_active);
	while (r && rm == NULL) {
		if (r->ifp == ifp &&
		    (!r->proto || r->proto == proto) &&
		    match_addr(r->not, r->daddr, r->dmask, addr) &&
		    r->dport == port)
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

int
pf_test_tcp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct tcphdr *th)
{
	struct pf_nat *nat = NULL;
	struct pf_rdr *rdr = NULL;
	u_int32_t baddr;
	u_int16_t bport;
	struct pf_rule *r, *rm = NULL;
	u_short reason;
	int rewrite = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_TCP,
		    h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			bport = th->th_sport;
			change_ap(&h->ip_src.s_addr, &th->th_sport, &h->ip_sum,
			    &th->th_sum, nat->daddr, htons(pf_next_port_tcp));
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = get_rdr(ifp, IPPROTO_TCP, h->ip_dst.s_addr,
		    th->th_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = th->th_dport;
			change_ap(&h->ip_dst.s_addr, &th->th_dport,
			    &h->ip_sum, &th->th_sum, rdr->raddr, rdr->rport);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->direction == direction &&
		    (r->ifp == NULL || r->ifp == ifp) &&
		    (!r->proto || r->proto == IPPROTO_TCP) &&
		    ((th->th_flags & r->flagset) == r->flags) &&
		    ((!r->src.addr && !r->src.mask) || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    ((!r->dst.addr && !r->dst.mask) || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->dst.port_op || match_port(r->dst.port_op, r->dst.port[0],
		    r->dst.port[1], th->th_dport)) &&
		    (!r->src.port_op || match_port(r->src.port_op, r->src.port[0],
		    r->src.port[1], th->th_sport)) ) {
			rm = r;
			if (r->quick)
				break;
		}
		r = TAILQ_NEXT(r, entries);
	}

	if (rm != NULL) {
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if (rm->action == PF_DROP_RST) {
			/* undo NAT/RST changes, if they have taken place */
			if (nat != NULL) {
				change_ap(&h->ip_src.s_addr, &th->th_sport,
				    &h->ip_sum, &th->th_sum, baddr, bport);
				rewrite++;
			}
			else if (rdr != NULL) {
				change_ap(&h->ip_dst.s_addr, &th->th_dport,
				    &h->ip_sum, &th->th_sum, baddr, bport);
				rewrite++;
			}

			send_reset(direction, ifp, h, off, th);
			return (PF_DROP);
		}

		if (rm->action == PF_DROP)
			return (PF_DROP);
	}

	if (((rm != NULL) && rm->keep_state) || (nat != NULL) || (rdr != NULL)) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = h->ip_len - off - (th->th_off << 2);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return (PF_DROP);
		}
		s->rule		= rm;
		s->log		= rm && (rm->log & 2);
		s->proto	= IPPROTO_TCP;
		s->direction	= direction;
		if (direction == PF_OUT) {
			s->gwy.addr	= h->ip_src.s_addr;
			s->gwy.port	= th->th_sport;
			s->ext.addr	= h->ip_dst.s_addr;
			s->ext.port	= th->th_dport;
			if (nat != NULL) {
				s->lan.addr	= baddr;
				s->lan.port	= bport;
				pf_next_port_tcp++;
				if (pf_next_port_tcp == 65535)
					pf_next_port_tcp = 50001;
			} else {
				s->lan.addr	= s->gwy.addr;
				s->lan.port	= s->gwy.port;
			}
		} else {
			s->lan.addr	= h->ip_dst.s_addr;
			s->lan.port	= th->th_dport;
			s->ext.addr	= h->ip_src.s_addr;
			s->ext.port	= th->th_sport;
			if (rdr != NULL) {
				s->gwy.addr	= baddr;
				s->gwy.port	= bport;
			} else {
				s->gwy.addr	= s->lan.addr;
				s->gwy.port	= s->lan.port;
			}
		}
		s->src.seqlo	= ntohl(th->th_seq) + len +
			((th->th_flags & TH_SYN) ? 1 : 0) +
			((th->th_flags & TH_FIN) ? 1 : 0);
		s->src.seqhi	= s->src.seqlo + 1;
		s->src.max_win	= MAX(ntohs(th->th_win), 1);

		s->dst.seqlo	= 0;	/* Haven't seen these yet */
		s->dst.seqhi	= 1;
		s->dst.max_win	= 1;
		s->src.state	= 1;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 60;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
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
	u_int16_t bport;
	struct pf_rule *r, *rm = NULL;
	u_short reason;
	int rewrite = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_UDP, h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			bport = uh->uh_sport;
			change_ap(&h->ip_src.s_addr, &uh->uh_sport, &h->ip_sum,
			    &uh->uh_sum, nat->daddr, htons(pf_next_port_udp));
			rewrite++;
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = get_rdr(ifp, IPPROTO_UDP, h->ip_dst.s_addr,
		    uh->uh_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = uh->uh_dport;
			change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
			    &h->ip_sum, &uh->uh_sum, rdr->raddr, rdr->rport);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if ((r->direction == direction) &&
		    ((r->ifp == NULL) || (r->ifp == ifp)) &&
		    (!r->proto || (r->proto == IPPROTO_UDP)) &&
		    ((!r->src.addr && !r->src.mask) || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    ((!r->dst.addr && !r->dst.mask) || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->dst.port_op || match_port(r->dst.port_op, r->dst.port[0],
		    r->dst.port[1], uh->uh_dport)) &&
		    (!r->src.port_op || match_port(r->src.port_op, r->src.port[0],
		    r->src.port[1], uh->uh_sport)) ) {
			rm = r;
			if (r->quick)
				break;
		}
		r = TAILQ_NEXT(r, entries);
	}

	if (rm != NULL) {
		REASON_SET(&reason, PFRES_MATCH);

		/* XXX will log packet before rewrite */
		if (rm->log)
			PFLOG_PACKET(h, m, AF_INET, direction, reason, rm);

		if (rm->action != PF_PASS)
			return (PF_DROP);
	}

	if ((rm != NULL && rm->keep_state) || nat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct pf_state *s;

		len = h->ip_len - off - 8;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return (PF_DROP);
		}
		s->rule		= rm;
		s->log		= rm && (rm->log & 2);
		s->proto	= IPPROTO_UDP;
		s->direction	= direction;
		if (direction == PF_OUT) {
			s->gwy.addr	= h->ip_src.s_addr;
			s->gwy.port	= uh->uh_sport;
			s->ext.addr	= h->ip_dst.s_addr;
			s->ext.port	= uh->uh_dport;
			if (nat != NULL) {
				s->lan.addr	= baddr;
				s->lan.port	= bport;
				pf_next_port_udp++;
				if (pf_next_port_udp == 65535)
					pf_next_port_udp = 50001;
			} else {
				s->lan.addr	= s->gwy.addr;
				s->lan.port	= s->gwy.port;
			}
		} else {
			s->lan.addr	= h->ip_dst.s_addr;
			s->lan.port	= uh->uh_dport;
			s->ext.addr	= h->ip_src.s_addr;
			s->ext.port	= uh->uh_sport;
			if (rdr != NULL) {
				s->gwy.addr	= baddr;
				s->gwy.port	= bport;
			} else {
				s->gwy.addr	= s->lan.addr;
				s->gwy.port	= s->lan.port;
			}
		}
		s->src.seqlo 	= 0;
		s->src.seqhi	= 0;
		s->src.max_win	= 0;
		s->src.state	= 1;
		s->dst.seqlo	= 0;
		s->dst.seqhi	= 0;
		s->dst.max_win	= 0;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 30;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
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
	int rewrite = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_ICMP, h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			change_a(&h->ip_src.s_addr, &h->ip_sum, nat->daddr);
			rewrite++;
		}
	}

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if ((r->direction == direction) &&
		    ((r->ifp == NULL) || (r->ifp == ifp)) &&
		    (!r->proto || (r->proto == IPPROTO_ICMP)) &&
		    ((!r->src.addr && !r->src.mask) || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    ((!r->dst.addr && !r->dst.mask) || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->type || (r->type == ih->icmp_type + 1)) &&
		    (!r->code || (r->code == ih->icmp_code + 1)) ) {
			rm = r;
			if (r->quick)
				break;
		}
		r = TAILQ_NEXT(r, entries);
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

		len = h->ip_len - off - 8;
		id = ih->icmp_hun.ih_idseq.icd_id;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return (PF_DROP);
		}
		s->rule		= rm;
		s->log		= rm && (rm->log & 2);
		s->proto	= IPPROTO_ICMP;
		s->direction	= direction;
		if (direction == PF_OUT) {
			s->gwy.addr	= h->ip_src.s_addr;
			s->gwy.port	= id;
			s->ext.addr	= h->ip_dst.s_addr;
			s->ext.port	= id;
			s->lan.addr	= nat ? baddr : s->gwy.addr;
			s->lan.port	= id;
		} else {
			s->lan.addr	= h->ip_dst.s_addr;
			s->lan.port	= id;
			s->ext.addr	= h->ip_src.s_addr;
			s->ext.port	= id;
			s->gwy.addr	= s->lan.addr;
			s->gwy.port	= id;
		}
		s->src.seqlo	= 0;
		s->src.seqhi	= 0;
		s->src.max_win	= 0;
		s->src.state	= 0;
		s->dst.seqlo	= 0;
		s->dst.seqhi	= 0;
		s->dst.max_win	= 0;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 20;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*ih), (caddr_t)ih);

	return (PF_PASS);
}

struct pf_state *
pf_test_state_tcp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct tcphdr *th)
{
	struct pf_state *s;
	struct pf_tree_key key;
	int rewrite = 0;

	key.proto   = IPPROTO_TCP;
	key.addr[0] = h->ip_src.s_addr;
	key.port[0] = th->th_sport;
	key.addr[1] = h->ip_dst.s_addr;
	key.port[1] = th->th_dport;

	s = find_state((direction == PF_IN) ? tree_ext_gwy : tree_lan_ext, &key);
	if (s != NULL) {
		u_int16_t len = h->ip_len - off - (th->th_off << 2);
		u_int16_t win = ntohs(th->th_win);
		u_int32_t seq = ntohl(th->th_seq), ack = ntohl(th->th_ack);
		u_int32_t end = seq + len + ((th->th_flags & TH_SYN) ? 1 : 0) +
			((th->th_flags & TH_FIN) ? 1 : 0);
		int ackskew;
		struct pf_state_peer *src, *dst;

		if (direction == s->direction) {
			src = &s->src;
			dst = &s->dst;
		} else {
			src = &s->dst;
			dst = &s->src;
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
			/* According to Guido, broken tcp stacks dont set ack */
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
				 * deduce how much data passed by what the other
				 * endpoint ACKs.  Thanks Guido!
				 *  (Why MAXACKWINDOW is used)
				 */
				dst->seqlo = ack;
			}

			s->packets++;
			s->bytes += len;

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
				s->expire = pftv.tv_sec + 5;
			else if (src->state >= 3 || dst->state >= 3)
				s->expire = pftv.tv_sec + 300;
			else if (src->state < 2 || dst->state < 2)
				s->expire = pftv.tv_sec + 30;
			else
				s->expire = pftv.tv_sec + 24*60*60;

			/* translate source/destination address, if necessary */
			if (s->lan.addr != s->gwy.addr ||
			    s->lan.port != s->gwy.port) {
				if (direction == PF_OUT)
					change_ap(&h->ip_src.s_addr, &th->th_sport,
					    &h->ip_sum, &th->th_sum,
					    s->gwy.addr, s->gwy.port);
				else
					change_ap(&h->ip_dst.s_addr, &th->th_dport,
					    &h->ip_sum, &th->th_sum,
					    s->lan.addr, s->lan.port);
				rewrite++;
			}

		} else {
			/* XXX Remove these printfs before release */
			printf("pf: BAD state: ");
			print_state(direction, s);
			print_flags(th->th_flags);
			printf(" seq=%lu ack=%lu len=%u ", seq, ack, len);
			printf("\n");
			printf("State failure: %c %c %c %c\n",
				SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
				SEQ_GEQ(seq, src->seqlo - dst->max_win)?' ':'2',
				(ackskew >= -MAXACKWINDOW) ? ' ' : '3',
				(ackskew <= MAXACKWINDOW) ? ' ' : '4');
			s = NULL;
		}

		/* copy back packet headers if we performed NAT operations */
		if (rewrite)
			m_copyback(m, off, sizeof(*th), (caddr_t)th);

		return (s);
	}
	return (NULL);
}

struct pf_state *
pf_test_state_udp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct udphdr *uh)
{
	struct pf_state *s;
	struct pf_tree_key key;
	int rewrite = 0;

	key.proto   = IPPROTO_UDP;
	key.addr[0] = h->ip_src.s_addr;
	key.port[0] = uh->uh_sport;
	key.addr[1] = h->ip_dst.s_addr;
	key.port[1] = uh->uh_dport;

	s = find_state((direction == PF_IN) ? tree_ext_gwy : tree_lan_ext, &key);
	if (s != NULL) {

		u_int16_t len = h->ip_len - off - 8;

		struct pf_state_peer *src, *dst;
		if (direction == s->direction) {
			src = &s->src;
			dst = &s->dst;
		} else {
			src = &s->dst;
			dst = &s->src;
		}

		s->packets++;
		s->bytes += len;

		/* update states */
		if (src->state < 1)
			src->state = 1;
		if (dst->state == 1)
			dst->state = 2;

		/* update expire time */
		if (src->state == 2 && dst->state == 2)
			s->expire = pftv.tv_sec + 60;
		else
			s->expire = pftv.tv_sec + 20;

		/* translate source/destination address, if necessary */
		if (s->lan.addr != s->gwy.addr ||
		    s->lan.port != s->gwy.port) {
			if (direction == PF_OUT)
				change_ap(&h->ip_src.s_addr, &uh->uh_sport,
				    &h->ip_sum, &uh->uh_sum,
				    s->gwy.addr, s->gwy.port);
			else
				change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
				    &h->ip_sum, &uh->uh_sum,
				    s->lan.addr, s->lan.port);
			rewrite++;
		}

		/* copy back packet headers if we performed NAT operations */
		if (rewrite)
			m_copyback(m, off, sizeof(*uh), (caddr_t)uh);

		return (s);
	}
	return (NULL);
}

struct pf_state *
pf_test_state_icmp(int direction, struct ifnet *ifp, struct mbuf *m,
    int ipoff, int off, struct ip *h, struct icmp *ih)
{
	u_int16_t len = h->ip_len - off - sizeof(*ih);
	int rewrite = 0;

	if (ih->icmp_type != ICMP_UNREACH &&
	    ih->icmp_type != ICMP_SOURCEQUENCH &&
	    ih->icmp_type != ICMP_REDIRECT &&
	    ih->icmp_type != ICMP_TIMXCEED &&
	    ih->icmp_type != ICMP_PARAMPROB) {

		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */

		struct pf_state *s;
		struct pf_tree_key key;

		key.proto   = IPPROTO_ICMP;
		key.addr[0] = h->ip_src.s_addr;
		key.port[0] = ih->icmp_hun.ih_idseq.icd_id;
		key.addr[1] = h->ip_dst.s_addr;
		key.port[1] = ih->icmp_hun.ih_idseq.icd_id;

		s = find_state((direction == PF_IN) ? tree_ext_gwy :
		    tree_lan_ext, &key);
		if (s != NULL) {

			s->packets++;
			s->bytes += len;
			s->expire = pftv.tv_sec + 10;

			/* translate source/destination address, if necessary */
			if (s->lan.addr != s->gwy.addr) {
				if (direction == PF_OUT)
					change_a(&h->ip_src.s_addr, &h->ip_sum,
					    s->gwy.addr);
				else
					change_a(&h->ip_dst.s_addr, &h->ip_sum,
					    s->lan.addr);
			}

			return (s);
		}
		return (NULL);

	} else {

		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct ip h2;
		int ipoff2;
		int off2;

		ipoff2 = off + 8;	/* offset of h2 in mbuf chain */
		if (!pull_hdr(ifp, m, 0, ipoff2, &h2, sizeof(h2), h,
			      NULL, NULL)) {
			printf("pf: ICMP error message too short\n");
			return (NULL);
		}

		/* offset of protocol header that follows h2 */
		off2 = ipoff2 + (h2.ip_hl << 2);

		switch (h2.ip_p) {
		case IPPROTO_TCP: {
			struct tcphdr th;
			u_int32_t seq, end;
			struct pf_state *s;
			struct pf_tree_key key;
			struct pf_state_peer *src, *dst;
			int ackskew;

			if (!pull_hdr(ifp, m, ipoff2, off2, &th, sizeof(th),
			    &h2, NULL, NULL)) {
				printf("pf: "
				    "ICMP error message too short\n");
				return (NULL);
			}
			seq = ntohl(th.th_seq);
			end = seq + h2.ip_len - ((h2.ip_hl + th.th_off)<<2) +
				((th.th_flags & TH_SYN) ? 1 : 0) +
				((th.th_flags & TH_FIN) ? 1 : 0);

			key.proto   = IPPROTO_TCP;
			key.addr[0] = h2.ip_dst.s_addr;
			key.port[0] = th.th_dport;
			key.addr[1] = h2.ip_src.s_addr;
			key.port[1] = th.th_sport;

			s = find_state((direction == PF_IN) ? tree_ext_gwy :
			    tree_lan_ext, &key);
			if (s == NULL)
				return (NULL);

			src = (direction == s->direction) ?  &s->dst : &s->src;
			dst = (direction == s->direction) ?  &s->src : &s->dst;

			if ((th.th_flags & TH_ACK) == 0 && th.th_ack == 0)
				ackskew = 0;
			else
				ackskew = dst->seqlo - ntohl(th.th_ack);
			if (!SEQ_GEQ(src->seqhi, end) ||
				!SEQ_GEQ(seq, src->seqlo - dst->max_win) ||
				!(ackskew >= -MAXACKWINDOW) ||
				!(ackskew <= MAXACKWINDOW)) {

				printf("pf: BAD ICMP state: ");
				print_state(direction, s);
				print_flags(th.th_flags);
				printf(" seq=%lu\n", seq);
				return (NULL);
			}

			if (s->lan.addr != s->gwy.addr ||
			    s->lan.port != s->gwy.port) {
				if (direction == PF_IN) {
					change_icmp(&h2.ip_src.s_addr,
					    &th.th_sport, &h->ip_dst.s_addr,
					    s->lan.addr, s->lan.port, &th.th_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					change_icmp(&h2.ip_dst.s_addr,
					    &th.th_dport, &h->ip_src.s_addr,
					    s->gwy.addr, s->gwy.port, &th.th_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
				rewrite++;
			}

			/*
			 * copy back packet headers if we performed NAT
			 * operations
			 */
			if (rewrite) {
				m_copyback(m, off, sizeof(*ih), (caddr_t)ih);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, sizeof(th),
				    (caddr_t)&th);
			}

			return (s);
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr uh;
			struct pf_state *s;
			struct pf_tree_key key;

			if (!pull_hdr(ifp, m, ipoff2, off2, &uh, sizeof(uh),
			    &h2, NULL, NULL)) {
				printf("pf: ICMP error message too short\n");
				return (NULL);
			}

			key.proto   = IPPROTO_UDP;
			key.addr[0] = h2.ip_dst.s_addr;
			key.port[0] = uh.uh_dport;
			key.addr[1] = h2.ip_src.s_addr;
			key.port[1] = uh.uh_sport;

			s = find_state(direction == PF_IN ? tree_ext_gwy :
			    tree_lan_ext, &key);
			if (s == NULL)
				return (NULL);

			if (s->lan.addr != s->gwy.addr ||
			    s->lan.port != s->gwy.port) {
				if (direction == PF_IN) {
					change_icmp(&h2.ip_src.s_addr,
					    &uh.uh_sport, &h->ip_dst.s_addr,
					    s->lan.addr, s->lan.port, &uh.uh_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					change_icmp(&h2.ip_dst.s_addr,
					    &uh.uh_dport, &h->ip_src.s_addr,
					    s->gwy.addr, s->gwy.port, &uh.uh_sum,
					    &h2.ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
				rewrite++;
			}

			/*
			 * copy back packet headers if we performed NAT
			 * operations
			 */
			if (rewrite) {
				m_copyback(m, off, sizeof(*ih), (caddr_t)ih);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, sizeof(uh),
				    (caddr_t)&uh);
			}

			return (s);
			break;
		}
		default:
			printf("pf: ICMP error message for bad proto\n");
			return (NULL);
		}
		return (NULL);

	}
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pull_hdr(struct ifnet *ifp, struct mbuf *m, int ipoff, int off, void *p,
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
pf_test(int dir, struct ifnet *ifp, struct mbuf *m)
{
	u_short action, reason = 0, log = 0;
	struct ip *h;
	struct pf_rule *r = NULL;
	struct pf_state *s;
	int off;

	if (!pf_status.running)
		return (PF_PASS);

#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif

	/* purge expire states, at most once every 10 seconds */
	microtime(&pftv);
	if (pftv.tv_sec - pf_last_purge >= 10) {
		purge_expired_states();
		pf_last_purge = pftv.tv_sec;
	}

	if (m->m_pkthdr.len < sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}
	h = mtod(m, struct ip *);

	off = h->ip_hl << 2;

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		struct tcphdr th;

		if (!pull_hdr(ifp, m, 0, off, &th, sizeof(th), h,
		      &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		if ((s = pf_test_state_tcp(dir, ifp, m, 0, off, h, &th))) {
			action = PF_PASS;
			r = s->rule;
			log = s->log;
		} else
			action = pf_test_tcp(dir, ifp, m, 0, off, h, &th);
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr uh;

		if (!pull_hdr(ifp, m, 0, off, &uh, sizeof(uh), h,
		      &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		if ((s = pf_test_state_udp(dir, ifp, m, 0, off, h, &uh))) {
			action = PF_PASS;
			r = s->rule;
			log = s->log;
		} else
			action = pf_test_udp(dir, ifp, m, 0, off, h, &uh);
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp ih;

		if (!pull_hdr(ifp, m, 0, off, &ih, sizeof(ih), h,
		      &action, &reason)) {
			log = action != PF_PASS;
			goto done;
		}
		if ((s = pf_test_state_icmp(dir, ifp, m, 0, off, h, &ih))) {
			action = PF_PASS;
			r = s->rule;
			log = s->log;
		} else
			action = pf_test_icmp(dir, ifp, m, 0, off, h, &ih);
		break;
	}

	default:
		action = PF_PASS;
		break;
	}

done:
	if (ifp == status_ifp) {
		pf_status.bytes[dir] += h->ip_len;
		pf_status.packets[dir][action]++;
	}
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
