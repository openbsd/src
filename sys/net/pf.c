/*	$OpenBSD: pf.c,v 1.16 2001/06/24 23:50:11 itojun Exp $ */

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
#include <net/route.h>
#include <net/pfvar.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

/*
 * Tree data structure
 */

struct tree_node {
	struct tree_key {
		u_int32_t	 addr[2];
		u_int16_t	 port[2];
		u_int8_t	 proto;
	}			 key;
	struct state		*state;
	signed char		 balance;
	struct tree_node	*left;
	struct tree_node	*right;
};

/*
 * Global variables
 */

struct rule		*rulehead;
struct nat		*nathead;
struct rdr		*rdrhead;
struct state		*statehead;
struct tree_node	*tree_lan_ext, *tree_ext_gwy;
struct timeval		 pftv;
struct status		 status;
struct ifnet		*status_ifp;

u_int32_t		 last_purge = 0;
u_int16_t		 next_port_tcp = 50001;
u_int16_t		 next_port_udp = 50001;

struct pool		pf_tree_pl;
struct pool		pf_rule_pl;
struct pool		pf_nat_pl;
struct pool		pf_rdr_pl;
struct pool		pf_state_pl;

/*
 * Prototypes
 */

signed char	 tree_key_compare (struct tree_key *a, struct tree_key *b);
void		 tree_rotate_left (struct tree_node **p);
void		 tree_rotate_right (struct tree_node **p);
int		 tree_insert (struct tree_node **p, struct tree_key *key,
		    struct state *state);
int		 tree_remove (struct tree_node **p, struct tree_key *key);
struct state	*find_state (struct tree_node *p, struct tree_key *key);
void		 insert_state (struct state *state);
void		 purge_expired_states (void);
void		 print_ip (struct ifnet *ifp, struct ip *h);
void		 print_host (u_int32_t a, u_int16_t p);
void		 print_state (int direction, struct state *s);
void		 print_flags (u_int8_t f);
void		 pfattach (int num);
int		 pfopen (dev_t dev, int flags, int fmt, struct proc *p);
int		 pfclose (dev_t dev, int flags, int fmt, struct proc *p);
int		 pfioctl (dev_t dev, u_long cmd, caddr_t addr, int flags,
		    struct proc *p);
u_int16_t	 fix (u_int16_t cksum, u_int16_t old, u_int16_t new);
void		 change_ap (u_int32_t *a, u_int16_t *p, u_int16_t *ic, u_int16_t
		    *pc, u_int32_t an, u_int16_t pn);
void		 change_a (u_int32_t *a, u_int16_t *c, u_int32_t an);
void		 change_icmp (u_int32_t *ia, u_int16_t *ip, u_int32_t *oa,
		    u_int32_t na, u_int16_t np, u_int16_t *pc, u_int16_t *h2c,
		    u_int16_t *ic, u_int16_t *hc);
void		 send_reset (int direction, struct ifnet *ifp, struct ip *h,
		    int off, struct tcphdr *th);
int		 match_addr (u_int8_t n, u_int32_t a, u_int32_t m, u_int32_t b);
int		 match_port (u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p);
struct nat	*get_nat (struct ifnet *ifp, u_int8_t proto, u_int32_t addr);
struct rdr	*get_rdr (struct ifnet *ifp, u_int8_t proto, u_int32_t addr,
		    u_int16_t port);
int		 pf_test_tcp (int direction, struct ifnet *ifp, int,
		    struct ip *h, struct tcphdr *th);
int		 pf_test_udp (int direction, struct ifnet *ifp, int,
		    struct ip *h, struct udphdr *uh);
int		 pf_test_icmp (int direction, struct ifnet *ifp, int,
		    struct ip *h, struct icmp *ih);
struct state	*pf_test_state_tcp (int direction, struct ifnet *ifp,
		    struct mbuf **, int, struct ip *h, struct tcphdr *th);
struct state	*pf_test_state_udp (int direction, struct ifnet *ifp,
		    struct mbuf **, int, struct ip *h, struct udphdr *uh);
struct state	*pf_test_state_icmp (int direction, struct ifnet *ifp,
		    struct mbuf **, int, struct ip *h, struct icmp *ih);
inline void	*pull_hdr (struct ifnet *ifp, struct mbuf **m, int, int, int,
		    struct ip *h, int *action);
int		 pf_test (int direction, struct ifnet *ifp, struct mbuf **m);

inline signed char
tree_key_compare(struct tree_key *a, struct tree_key *b)
{
	/*
	 * could use memcmp(), but with the best manual order, we can
	 * minimize the number of average compares. what is faster?
	 */
	if (a->proto   < b->proto  )
		return -1;
	if (a->proto   > b->proto  )
		return  1;
	if (a->addr[0] < b->addr[0])
		return -1;
	if (a->addr[0] > b->addr[0])
		return  1;
	if (a->addr[1] < b->addr[1])
		return -1;
	if (a->addr[1] > b->addr[1])
		return  1;
	if (a->port[0] < b->port[0])
		return -1;
	if (a->port[0] > b->port[0])
		return  1;
	if (a->port[1] < b->port[1])
		return -1;
	if (a->port[1] > b->port[1])
		return  1;
	return 0;
}

inline void
tree_rotate_left(struct tree_node **p)
{
	struct tree_node *q = *p;

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

inline void
tree_rotate_right(struct tree_node **p)
{
	struct tree_node *q = *p;

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
tree_insert(struct tree_node **p, struct tree_key *key, struct state *state)
{
	int deltaH = 0;

	if (*p == NULL) {
		*p = pool_get(&pf_tree_pl, PR_NOWAIT);
		if (*p == NULL) {
			return 0;
		}
		bcopy(key, &(*p)->key, sizeof(struct tree_key));
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
	return deltaH;
}

int
tree_remove(struct tree_node **p, struct tree_key *key)
{
	int deltaH = 0;
	signed char c;

	if (*p == NULL)
		return 0;
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
			struct tree_node *p0 = *p;

			*p = (*p)->left;
			pool_put(&pf_tree_pl, p0);
			deltaH = 1;
		} else if ((*p)->left == NULL) {
			struct tree_node *p0 = *p;

			*p = (*p)->right;
			pool_put(&pf_tree_pl, p0);
			deltaH = 1;
		} else {
			struct tree_node **qq = &(*p)->left;

			while ((*qq)->right != NULL)
				qq = &(*qq)->right;
			bcopy(&(*qq)->key, &(*p)->key, sizeof(struct tree_key));
			(*p)->state = (*qq)->state;
			bcopy(key, &(*qq)->key, sizeof(struct tree_key));
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
	return deltaH;
}

inline struct state *
find_state(struct tree_node *p, struct tree_key *key)
{
	signed char c;

	while (p && (c = tree_key_compare(&p->key, key)))
		p = (c > 0) ? p->left : p->right;
	status.state_searches++;
	return p ? p->state : NULL;
}

void
insert_state(struct state *state)
{
	struct tree_key key;

	key.proto = state->proto;
	key.addr[0] = state->lan.addr;
	key.port[0] = state->lan.port;
	key.addr[1] = state->ext.addr;
	key.port[1] = state->ext.port;
	/* sanity checks can be removed later, should never occur */
	if (find_state(tree_lan_ext, &key) != NULL)
		printf("packetfilter: ERROR! insert invalid\n");
	else {
		tree_insert(&tree_lan_ext, &key, state);
		if (find_state(tree_lan_ext, &key) != state)
			printf("packetfilter: ERROR! insert failed\n");
	}

	key.proto   = state->proto;
	key.addr[0] = state->ext.addr;
	key.port[0] = state->ext.port;
	key.addr[1] = state->gwy.addr;
	key.port[1] = state->gwy.port;
	if (find_state(tree_ext_gwy, &key) != NULL)
		printf("packetfilter: ERROR! insert invalid\n");
	else {
		tree_insert(&tree_ext_gwy, &key, state);
		if (find_state(tree_ext_gwy, &key) != state)
			printf("packetfilter: ERROR! insert failed\n");
	}

	state->next = statehead;
	statehead = state;

	status.state_inserts++;
	status.states++;
}

void
purge_expired_states(void)
{
	struct tree_key key;
	struct state *cur = statehead, *prev = NULL;

	while (cur != NULL) {
		if (cur->expire <= pftv.tv_sec) {
			key.proto = cur->proto;
			key.addr[0] = cur->lan.addr;
			key.port[0] = cur->lan.port;
			key.addr[1] = cur->ext.addr;
			key.port[1] = cur->ext.port;
			/* sanity checks can be removed later */
			if (find_state(tree_lan_ext, &key) != cur)
				printf("packetfilter: ERROR! remove invalid\n");
			tree_remove(&tree_lan_ext, &key);
			if (find_state(tree_lan_ext, &key) != NULL)
				printf("packetfilter: ERROR! remove failed\n");
			key.proto   = cur->proto;
			key.addr[0] = cur->ext.addr;
			key.port[0] = cur->ext.port;
			key.addr[1] = cur->gwy.addr;
			key.port[1] = cur->gwy.port;
			if (find_state(tree_ext_gwy, &key) != cur)
				printf("packetfilter: ERROR! remove invalid\n");
			tree_remove(&tree_ext_gwy, &key);
			if (find_state(tree_ext_gwy, &key) != NULL)
				printf("packetfilter: ERROR! remove failed\n");
			(prev ? prev->next : statehead) = cur->next;
			pool_put(&pf_state_pl, cur);
			cur = (prev ? prev->next : statehead);
			status.state_removals++;
			status.states--;
		} else {
			prev = cur;
			cur = cur->next;
		}
	}
}

inline void
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
print_state(int direction, struct state *s)
{
	print_host(s->lan.addr, s->lan.port);
	printf(" ");
	print_host(s->gwy.addr, s->gwy.port);
	printf(" ");
	print_host(s->ext.addr, s->ext.port);
	printf(" [%lu+%lu]", s->src.seqlo, s->src.seqhi - s->src.seqlo);
	printf(" [%lu+%lu]", s->dst.seqlo, s->dst.seqhi - s->dst.seqlo);
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
        pool_init(&pf_tree_pl, sizeof(struct tree_node), 0, 0, 0, "pftrpl",
                0, NULL, NULL, 0);
        pool_init(&pf_rule_pl, sizeof(struct rule), 0, 0, 0, "pfrulepl",
                0, NULL, NULL, 0);
        pool_init(&pf_nat_pl, sizeof(struct nat), 0, 0, 0, "pfnatpl",
                0, NULL, NULL, 0);
        pool_init(&pf_rdr_pl, sizeof(struct rdr), 0, 0, 0, "pfrdrpl",
                0, NULL, NULL, 0);
        pool_init(&pf_state_pl, sizeof(struct state), 0, 0, 0, "pfstatepl",
                0, NULL, NULL, 0);
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
	struct pfioc *ub;
	void *kb = NULL;
	int s;

	if (!(flags & FWRITE))
		return EACCES;
	
	if ((cmd != DIOCSTART) && (cmd != DIOCSTOP) && (cmd != DIOCCLRSTATES)) {
		ub = (struct pfioc *)addr;
		if (ub == NULL)
			return ERROR_INVALID_PARAMETERS;
		kb = malloc(ub->size, M_DEVBUF, M_NOWAIT);
		if (kb == NULL)
			return ERROR_MALLOC;
		if (copyin(ub->buffer, kb, ub->size)) {
			free(kb, M_DEVBUF);
			return ERROR_INVALID_PARAMETERS;
		}
	}

	s = splsoftnet();

	microtime(&pftv);
	if (pftv.tv_sec - last_purge >= 10) {
		purge_expired_states();
		last_purge = pftv.tv_sec;
	}

	switch (cmd) {

	case DIOCSTART:
		if (status.running)
			error = ERROR_ALREADY_RUNNING;
		else {
			u_int32_t states = status.states;
			bzero(&status, sizeof(struct status));
			status.running = 1;
			status.states = states;
			status.since = pftv.tv_sec;
			printf("packetfilter: started\n");
		}
		break;

	case DIOCSTOP:
		if (!status.running)
			error = ERROR_NOT_RUNNING;
		else {
			status.running = 0;
			printf("packetfilter: stopped\n");
		}
		break;

	case DIOCSETRULES: {
		struct rule *rules = (struct rule *)kb, *ruletail = NULL;
		u_int16_t n;
		while (rulehead != NULL) {
			struct rule *next = rulehead->next;
			pool_put(&pf_rule_pl, rulehead);
			rulehead = next;
		}
		for (n = 0; n < ub->entries; ++n) {
			struct rule *rule;

			rule = pool_get(&pf_rule_pl, PR_NOWAIT);
			if (rule == NULL) {
				error = ERROR_MALLOC;
				goto done;
			}
			bcopy(rules + n, rule, sizeof(struct rule));
			rule->ifp = NULL;
			if (rule->ifname[0]) {
				rule->ifp = ifunit(rule->ifname);
				if (rule->ifp == NULL) {
					pool_put(&pf_rule_pl, rule);
					error = ERROR_INVALID_PARAMETERS;
					goto done;
				}
			}
			rule->next = NULL;
			if (ruletail != NULL) {
				ruletail->next = rule;
				ruletail = rule;
			} else
				rulehead = ruletail = rule;
		}
		break;
	}

	case DIOCGETRULES: {
		struct rule *rules = (struct rule *)kb;
		struct rule *rule = rulehead;
		u_int16_t n = 0;
		while ((rule != NULL) && (n < ub->entries)) {
			bcopy(rule, rules + n, sizeof(struct rule));
			n++;
			rule = rule->next;
		}
		ub->entries = n;
		break;
	}

	case DIOCSETNAT: {
		struct nat *nats = (struct nat *)kb;
		u_int16_t n;
		while (nathead != NULL) {
			struct nat *next = nathead->next;

			pool_put(&pf_nat_pl, nathead);
			nathead = next;
		}
		for (n = 0; n < ub->entries; ++n) {
			struct nat *nat;

			nat = pool_get(&pf_nat_pl, PR_NOWAIT);
			if (nat == NULL) {
				error = ERROR_MALLOC;
				goto done;
			}
			bcopy(nats + n, nat, sizeof(struct nat));
			nat->ifp = ifunit(nat->ifname);
			if (nat->ifp == NULL) {
				pool_put(&pf_nat_pl, nat);
				error = ERROR_INVALID_PARAMETERS;
				goto done;
			}
			nat->next = nathead;
			nathead = nat;
		}
		break;
	}

	case DIOCGETNAT: {
		struct nat *nats = (struct nat *)kb;
		struct nat *nat = nathead;
		u_int16_t n = 0;
		while ((nat != NULL) && (n < ub->entries)) {
			bcopy(nat, nats + n, sizeof(struct nat));
			n++;
			nat = nat->next;
		}
		ub->entries = n;
		break;
	}

	case DIOCSETRDR: {
		struct rdr *rdrs = (struct rdr *)kb;
		u_int16_t n;
		while (rdrhead != NULL) {
			struct rdr *next = rdrhead->next;

			pool_put(&pf_rdr_pl, rdrhead);
			rdrhead = next;
		}
		for (n = 0; n < ub->entries; ++n) {
			struct rdr *rdr;

			rdr = pool_get(&pf_rdr_pl, PR_NOWAIT);
			if (rdr == NULL) {
				error = ERROR_MALLOC;
				goto done;
			}
			bcopy(rdrs + n, rdr, sizeof(struct rdr));
			rdr->ifp = ifunit(rdr->ifname);
			if (rdr->ifp == NULL) {
				pool_put(&pf_rdr_pl, rdr);
				error = ERROR_INVALID_PARAMETERS;
				goto done;
			}
			rdr->next = rdrhead;
			rdrhead = rdr;
		}
		break;
	}

	case DIOCGETRDR: {
		struct rdr *rdrs = (struct rdr *)kb;
		struct rdr *rdr = rdrhead;
		u_int16_t n = 0;
		while ((rdr != NULL) && (n < ub->entries)) {
			bcopy(rdr, rdrs + n, sizeof(struct rdr));
			n++;
			rdr = rdr->next;
		}
		ub->entries = n;
		break;
	}

	case DIOCCLRSTATES: {
		struct state *state = statehead;
		while (state != NULL) {
			state->expire = 0;
			state = state->next;
		}
		purge_expired_states();
		break;
	}

	case DIOCGETSTATES: {
		struct state *states = (struct state *)kb;
		struct state *state;
		u_int16_t n = 0;
		state = statehead;
		while ((state != NULL) && (n < ub->entries)) {
			bcopy(state, states + n, sizeof(struct state));
			states[n].creation = pftv.tv_sec - states[n].creation;
			if (states[n].expire <= pftv.tv_sec)
				states[n].expire = 0;
			else
				states[n].expire -= pftv.tv_sec;
			n++;
			state = state->next;
		}
		ub->entries = n;
		break;
	}

	case DIOCSETSTATUSIF: {
		char *ifname = (char *)kb;
		struct ifnet *ifp = ifunit(ifname);
		if (ifp == NULL)
			error = ERROR_INVALID_PARAMETERS;
		else
			status_ifp = ifp;
		break;
	}

	case DIOCGETSTATUS: {
		struct status *st = (struct status *)kb;
		u_int8_t running = status.running;
		u_int32_t states = status.states;
		bcopy(&status, st, sizeof(struct status));
		st->since = st->since ? pftv.tv_sec - st->since : 0;
		ub->entries = 1;
		bzero(&status, sizeof(struct status));
		status.running = running;
		status.states = states;
		status.since = pftv.tv_sec;
		break;
	}

	default:
		error = ERROR_INVALID_OP;
		break;
	}

done:
	splx(s);
	if (kb != NULL) {
		if (copyout(kb, ub->buffer, ub->size))
			error = ERROR_INVALID_PARAMETERS;
		free(kb, M_DEVBUF);
	}
	return error;
}

inline u_int16_t
fix(u_int16_t cksum, u_int16_t old, u_int16_t new)
{
	u_int32_t l = cksum + old - new;
	l = (l >> 16) + (l & 65535); 
	l = l & 65535;
	return l ? l : 65535;
}  

void
change_ap(u_int32_t *a, u_int16_t *p, u_int16_t *ic, u_int16_t *pc, u_int32_t an,
    u_int16_t pn)
{
	u_int32_t ao = *a;
	u_int16_t po = *p;
	*a = an;
	*ic = fix(fix(*ic, ao / 65536, an / 65536), ao % 65536, an % 65536);
	*p = pn;
	*pc = fix(fix(fix(*pc, ao / 65536, an / 65536), ao % 65536, an % 65536),
	    po, pn);
}

void
change_a(u_int32_t *a, u_int16_t *c, u_int32_t an)
{
	u_int32_t ao = *a;
	*a = an;
	*c = fix(fix(*c, ao / 65536, an / 65536), ao % 65536, an % 65536);
}

void
change_icmp(u_int32_t *ia, u_int16_t *ip, u_int32_t *oa, u_int32_t na,
    u_int16_t np, u_int16_t *pc, u_int16_t *h2c, u_int16_t *ic, u_int16_t *hc)
{
	u_int32_t oia = *ia, ooa = *oa, opc = *pc, oh2c = *h2c;
	u_int16_t oip = *ip;
	// change inner protocol port, fix inner protocol checksum
	*ip = np;
	*pc = fix(*pc, oip, *ip);
	*ic = fix(*ic, oip, *ip);
	*ic = fix(*ic, opc, *pc);
	// change inner ip address, fix inner ip checksum and icmp checksum
	*ia = na;
	*h2c = fix(fix(*h2c, oia / 65536, *ia / 65536), oia % 65536, *ia % 65536);
	*ic = fix(fix(*ic, oia / 65536, *ia / 65536), oia % 65536, *ia % 65536);
	*ic = fix(*ic, oh2c, *h2c);
	// change outer ip address, fix outer ip checksum
	*oa = na;
	*hc = fix(fix(*hc, ooa / 65536, *oa / 65536), ooa % 65536, *oa % 65536);
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

inline int
match_addr(u_int8_t n, u_int32_t a, u_int32_t m, u_int32_t b)
{
	return n == !((a & m) == (b & m));
}

inline int
match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	switch (op) {
	case 1:
		return (p >= a1) && (p <= a2);
	case 2:
		return p == a1;
	case 3:
		return p != a1;
	case 4:
		return p <  a1;
	case 5:
		return p <= a1;
	case 6:
		return p >  a1;
	case 7:
		return p >= a1;
	}
	return 0; /* never reached */
}

struct nat *
get_nat(struct ifnet *ifp, u_int8_t proto, u_int32_t addr)
{
	struct nat *n = nathead, *nm = NULL;

	while (n && nm == NULL) {
		if (n->ifp == ifp &&
		    (!n->proto || n->proto == proto) &&
		    match_addr(n->not, n->saddr, n->smask, addr))
			nm = n;
		else
			n = n->next;
	}
	return nm;
}

struct rdr *
get_rdr(struct ifnet *ifp, u_int8_t proto, u_int32_t addr, u_int16_t port)
{
	struct rdr *r = rdrhead, *rm = NULL;
	while (r && rm == NULL) {
		if (r->ifp == ifp &&
		    (!r->proto || r->proto == proto) &&
		    match_addr(r->not, r->daddr, r->dmask, addr) &&
		    r->dport == port)
			rm = r;
		else
			r = r->next;
	}
	return rm;
}

int
pf_test_tcp(int direction, struct ifnet *ifp, int off, struct ip *h,
    struct tcphdr *th)
{
	struct nat *nat = NULL;
	struct rdr *rdr = NULL;
	u_int32_t baddr;
	u_int16_t bport;
	struct rule *r = rulehead, *rm = NULL;
	u_int16_t nr = 1, mnr = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_TCP,
		    h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			bport = th->th_sport;
			change_ap(&h->ip_src.s_addr, &th->th_sport, &h->ip_sum,
			    &th->th_sum, nat->daddr, htons(next_port_tcp));
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = get_rdr(ifp, IPPROTO_TCP, h->ip_dst.s_addr,
		    th->th_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = th->th_dport;
			change_ap(&h->ip_dst.s_addr, &th->th_dport,
			    &h->ip_sum, &th->th_sum, rdr->raddr, rdr->rport);
		}
	}

	while (r != NULL) {
		if (r->direction == direction &&
		    (r->ifp == NULL || r->ifp == ifp) &&
		    (!r->proto || r->proto == IPPROTO_TCP) &&
		    ((th->th_flags & r->flagset) == r->flags) &&
		    (!r->src.addr || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    (!r->dst.addr || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->dst.port_op || match_port(r->dst.port_op, r->dst.port[0],
		    r->dst.port[1], th->th_dport)) &&
		    (!r->src.port_op || match_port(r->src.port_op, r->src.port[0],
		    r->src.port[1], th->th_sport)) ) {
			rm = r;
			mnr = nr;
			if (r->quick)
				break;
		}
		r = r->next;
		nr++;
	}

	if ((rm != NULL) && rm->log) {
		u_int32_t seq = ntohl(th->th_seq);
		u_int16_t len = h->ip_len - off - (th->th_off << 2);

		printf("packetfilter: @%u", mnr);
		printf(" %s %s", rm->action ? "block" : "pass",
		    direction ? "in" : "out");
		printf(" on %s proto tcp", ifp->if_xname);
		printf(" from ");
		print_host(h->ip_src.s_addr, th->th_sport);
		printf(" to ");
		print_host(h->ip_dst.s_addr, th->th_dport);
		print_flags(th->th_flags);
		if (len || (th->th_flags & (TH_SYN | TH_FIN | TH_RST)))
			printf(" %lu:%lu(%u)", seq, seq + len, len);
		if (th->th_ack)
			printf(" ack=%lu", ntohl(th->th_ack));
		printf("\n");
	}

	if ((rm != NULL) && (rm->action == PF_DROP_RST)) {
		/* undo NAT/RST changes, if they have taken place */
		if (nat != NULL)
			change_ap(&h->ip_src.s_addr, &th->th_sport,
			    &h->ip_sum, &th->th_sum, baddr, bport);
		else if (rdr != NULL)
			change_ap(&h->ip_dst.s_addr, &th->th_dport,
			    &h->ip_sum, &th->th_sum, baddr, bport);
		send_reset(direction, ifp, h, off, th);
		return PF_DROP;
	}

	if ((rm != NULL) && (rm->action == PF_DROP))
		return PF_DROP;

	if (((rm != NULL) && rm->keep_state) || (nat != NULL) || (rdr != NULL)) {
		/* create new state */
		u_int16_t len;
		struct state *s;

		len = h->ip_len - off - (th->th_off << 2);
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return PF_DROP;
		}
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
				next_port_tcp++;
				if (next_port_tcp == 65535)
					next_port_tcp = 50001;
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
		s->src.seqlo	= ntohl(th->th_seq) + len; // ???
		s->src.seqhi	= s->src.seqlo + 1;
		s->src.state	= 1;
		s->dst.seqlo	= 0;
		s->dst.seqhi	= 0;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 60;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
	}

	return PF_PASS;
}

int
pf_test_udp(int direction, struct ifnet *ifp, int off, struct ip *h,
    struct udphdr *uh)
{
	struct nat *nat = NULL;
	struct rdr *rdr = NULL;
	u_int32_t baddr;
	u_int16_t bport;
	struct rule *r = rulehead, *rm = NULL;
	u_int16_t nr = 1, mnr = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_UDP, h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			bport = uh->uh_sport;
			change_ap(&h->ip_src.s_addr, &uh->uh_sport, &h->ip_sum,
			    &uh->uh_sum, nat->daddr, htons(next_port_udp));
		}
	} else {
		/* check incoming packet for RDR */
		if ((rdr = get_rdr(ifp, IPPROTO_UDP, h->ip_dst.s_addr,
		    uh->uh_dport)) != NULL) {
			baddr = h->ip_dst.s_addr;
			bport = uh->uh_dport;
			change_ap(&h->ip_dst.s_addr, &uh->uh_dport,
			    &h->ip_sum, &uh->uh_sum, rdr->raddr, rdr->rport);
		}
	}

	while (r != NULL) {
		if ((r->direction == direction) &&
		    ((r->ifp == NULL) || (r->ifp == ifp)) &&
		    (!r->proto || (r->proto == IPPROTO_UDP)) &&
		    (!r->src.addr || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    (!r->dst.addr || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->dst.port_op || match_port(r->dst.port_op, r->dst.port[0],
		    r->dst.port[1], uh->uh_dport)) &&
		    (!r->src.port_op || match_port(r->src.port_op, r->src.port[0],
		    r->src.port[1], uh->uh_sport)) ) {
			rm = r;
			mnr = nr;
			if (r->quick)
				break;
		}
		r = r->next;
		nr++;
	}

	if (rm != NULL && rm->log) {
		printf("packetfilter: @%u", mnr);
		printf(" %s %s", rm->action ? "block" : "pass", direction ? "in" :
		    "out");
		printf(" on %s proto udp", ifp->if_xname);
		printf(" from ");
		print_host(h->ip_src.s_addr, uh->uh_sport);
		printf(" to ");
		print_host(h->ip_dst.s_addr, uh->uh_dport);
		printf("\n");
	}

	if (rm != NULL && rm->action != PF_PASS)
		return PF_DROP;

	if ((rm != NULL && rm->keep_state) || nat != NULL || rdr != NULL) {
		/* create new state */
		u_int16_t len;
		struct state *s;

		len = h->ip_len - off - 8;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return PF_DROP;
		}
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
				next_port_udp++;
				if (next_port_udp == 65535)
					next_port_udp = 50001;
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
		s->src.seqlo	= 0;
		s->src.seqhi	= 0;
		s->src.state	= 1;
		s->dst.seqlo	= 0;
		s->dst.seqhi	= 0;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 30;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
	}

	return PF_PASS;
}

int
pf_test_icmp(int direction, struct ifnet *ifp, int off, struct ip *h,
    struct icmp *ih)
{
	struct nat *nat = NULL;
	u_int32_t baddr;
	struct rule *r = rulehead, *rm = NULL;
	u_int16_t nr = 1, mnr = 0;

	if (direction == PF_OUT) {
		/* check outgoing packet for NAT */
		if ((nat = get_nat(ifp, IPPROTO_ICMP, h->ip_src.s_addr)) != NULL) {
			baddr = h->ip_src.s_addr;
			change_a(&h->ip_src.s_addr, &h->ip_sum, nat->daddr);
		}
	}

	while (r != NULL) {
		if ((r->direction == direction) &&
		    ((r->ifp == NULL) || (r->ifp == ifp)) &&
		    (!r->proto || (r->proto == IPPROTO_ICMP)) &&
		    (!r->src.addr || match_addr(r->src.not, r->src.addr,
		    r->src.mask, h->ip_src.s_addr)) &&
		    (!r->dst.addr || match_addr(r->dst.not, r->dst.addr,
		    r->dst.mask, h->ip_dst.s_addr)) &&
		    (!r->type || (r->type == ih->icmp_type + 1)) &&
		    (!r->code || (r->code == ih->icmp_code + 1)) ) {
			rm = r;
			mnr = nr;
			if (r->quick)
				break;
		}
		r = r->next;
		nr++;
	}

	if (rm != NULL && rm->log) {
		printf("packetfilter: @%u", mnr);
		printf(" %s %s", rm->action ? "block" : "pass", direction ? "in" :
		    "out");
		printf(" on %s proto icmp", ifp->if_xname);
		printf(" from ");
		print_host(h->ip_src.s_addr, 0);
		printf(" to ");
		print_host(h->ip_dst.s_addr, 0);
		printf(" type %u/%u", ih->icmp_type, ih->icmp_code);
		printf("\n");
	}

	if (rm != NULL && rm->action != PF_PASS)
		return PF_DROP;

	if ((rm != NULL && rm->keep_state) || nat != NULL) {
		/* create new state */
		u_int16_t len;
		u_int16_t id;
		struct state *s;

		len = h->ip_len - off - 8;
		id = ih->icmp_hun.ih_idseq.icd_id;
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			return PF_DROP;
		}
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
		s->src.state	= 0;
		s->dst.seqlo	= 0;
		s->dst.seqhi	= 0;
		s->dst.state	= 0;
		s->creation	= pftv.tv_sec;
		s->expire	= pftv.tv_sec + 20;
		s->packets	= 1;
		s->bytes	= len;
		insert_state(s);
	}

	return PF_PASS;
}

struct state *
pf_test_state_tcp(int direction, struct ifnet *ifp, struct mbuf **m, int off,
    struct ip *h, struct tcphdr *th)
{
	struct state *s;
	struct tree_key key;

	key.proto   = IPPROTO_TCP;
	key.addr[0] = h->ip_src.s_addr;
	key.port[0] = th->th_sport;
	key.addr[1] = h->ip_dst.s_addr;
	key.port[1] = th->th_dport;

	s = find_state((direction == PF_IN) ? tree_ext_gwy : tree_lan_ext, &key);
	if (s != NULL) {
		u_int16_t len = h->ip_len - off - (th->th_off << 2);
		u_int32_t seq = ntohl(th->th_seq), ack = ntohl(th->th_ack);
		struct state_peer *src, *dst;

		if (direction == s->direction) {
			src = &s->src;
			dst = &s->dst;
		} else {
			src = &s->dst;
			dst = &s->src;
		}

		/* some senders do that instead of ACKing FIN */
		if (th->th_flags == TH_RST && !ack && !len &&
		    (seq == src->seqhi || seq == src->seqhi-1) &&
		    src->state >= 4 && dst->state >= 3)
			ack = dst->seqhi;

		if ((dst->seqhi >= dst->seqlo ?
		    (ack >= dst->seqlo) && (ack <= dst->seqhi) :
		    (ack >= dst->seqlo) || (ack <= dst->seqhi)) ||
		    (seq == src->seqlo) || (seq == src->seqlo-1)) {

			s->packets++;
			s->bytes += len;

			/* update sequence number range */
			if (th->th_flags & TH_ACK)
				dst->seqlo = ack;
			if (th->th_flags & (TH_SYN | TH_FIN))
				len++;
			if (th->th_flags & TH_SYN) {
				src->seqhi = seq + len;
				src->seqlo = src->seqhi - 1;
			} else if (seq + len - src->seqhi < 65536)
				src->seqhi = seq + len;

			/* update states */
			if (th->th_flags & TH_SYN)
				if (src->state < 1)
					src->state = 1;
			if (th->th_flags & TH_FIN)
				if (src->state < 3)
					src->state = 3;
			if ((th->th_flags & TH_ACK) && ack == dst->seqhi) {
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
			}

		} else {
			printf("packetfilter: BAD state: ");
			print_state(direction, s);
			print_flags(th->th_flags);
			printf(" seq=%lu ack=%lu len=%u ", seq, ack, len);
			printf("\n");
			s = NULL;
		}

		return s;
	}
	return NULL;
}

struct state *
pf_test_state_udp(int direction, struct ifnet *ifp, struct mbuf **m, int off,
    struct ip *h, struct udphdr *uh)
{
	struct state *s;
	struct tree_key key;

	key.proto   = IPPROTO_UDP;
	key.addr[0] = h->ip_src.s_addr;
	key.port[0] = uh->uh_sport;
	key.addr[1] = h->ip_dst.s_addr;
	key.port[1] = uh->uh_dport;

	s = find_state((direction == PF_IN) ? tree_ext_gwy : tree_lan_ext, &key);
	if (s != NULL) {

		u_int16_t len = h->ip_len - off - 8;

		struct state_peer *src, *dst;
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
		}

		return s;
	}
	return NULL;
}

struct state *
pf_test_state_icmp(int direction, struct ifnet *ifp, struct mbuf **m, int off,
    struct ip *h, struct icmp *ih)
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

		struct state *s;
		struct tree_key key;

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

			return s;
		}
		return NULL;

	} else {

		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct ip *h2;
		int off2;
		int dummy;

		off += 8;	/* offset of h2 in mbuf chain */
		h2 = pull_hdr(ifp, m, 0, off, sizeof(*h2), h, &dummy);
		if (!h2) {
			printf("packetfilter: ICMP error message too short\n");
			return NULL;
		}
		if (len < off2) {
			printf("packetfilter: ICMP error message too short\n");
			return NULL;
		}

		/* offset of protocol header that follows h2 */
		off2 = off + (h2->ip_hl << 2);

		switch (h2->ip_p) {
		case IPPROTO_TCP: {
			struct tcphdr *th;
			u_int32_t seq;
			struct state *s;
			struct tree_key key;
			struct state_peer *src;

			th = pull_hdr(ifp, m, off, off2, sizeof(*th), h2,
			    &dummy);
			if (!th) {
				printf("packetfilter: "
				    "ICMP error message too short\n");
				return NULL;
			}
			seq = ntohl(th->th_seq);

			key.proto   = IPPROTO_TCP;
			key.addr[0] = h2->ip_dst.s_addr;
			key.port[0] = th->th_dport;
			key.addr[1] = h2->ip_src.s_addr;
			key.port[1] = th->th_sport;

			s = find_state((direction == PF_IN) ? tree_ext_gwy :
			    tree_lan_ext, &key);
			if (s == NULL)
				return NULL;

			src = (direction == s->direction) ?  &s->dst : &s->src;

			if ((src->seqhi >= src->seqlo ?
			    (seq < src->seqlo) || (seq > src->seqhi) :
			    (seq < src->seqlo) && (seq > src->seqhi))) {
				printf("packetfilter: BAD ICMP state: ");
				print_state(direction, s);
				print_flags(th->th_flags);
				printf(" seq=%lu\n", seq);
				return NULL;
			}

			if (s->lan.addr != s->gwy.addr ||
			    s->lan.port != s->gwy.port) {
				if (direction == PF_IN) {
					change_icmp(&h2->ip_src.s_addr,
					    &th->th_sport, &h->ip_dst.s_addr,
					    s->lan.addr, s->lan.port, &th->th_sum,
					    &h2->ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					change_icmp(&h2->ip_dst.s_addr,
					    &th->th_dport, &h->ip_src.s_addr,
					    s->gwy.addr, s->gwy.port, &th->th_sum,
					    &h2->ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
			}
			return s;
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr *uh;
			struct state *s;
			struct tree_key key;

			uh = pull_hdr(ifp, m, off, off2, sizeof(*uh), h2,
			    &dummy);
			if (!uh) {
				printf("packetfilter: "
				    "ICMP error message too short\n");
				return NULL;
			}

			key.proto   = IPPROTO_UDP;
			key.addr[0] = h2->ip_dst.s_addr;
			key.port[0] = uh->uh_dport;
			key.addr[1] = h2->ip_src.s_addr;
			key.port[1] = uh->uh_sport;

			s = find_state(direction == PF_IN ? tree_ext_gwy :
			    tree_lan_ext, &key);
			if (s == NULL)
				return NULL;

			if (s->lan.addr != s->gwy.addr ||
			    s->lan.port != s->gwy.port) {
				if (direction == PF_IN) {
					change_icmp(&h2->ip_src.s_addr,
					    &uh->uh_sport, &h->ip_dst.s_addr,
					    s->lan.addr, s->lan.port, &uh->uh_sum,
					    &h2->ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				} else {
					change_icmp(&h2->ip_dst.s_addr,
					    &uh->uh_dport, &h->ip_src.s_addr,
					    s->gwy.addr, s->gwy.port, &uh->uh_sum,
					    &h2->ip_sum, &ih->icmp_cksum,
					    &h->ip_sum);
				}
			}
			return s;
			break;
		}
		default:
			printf("packetfilter: ICMP error message for bad proto\n");
			return NULL;
		}
		return NULL;

	}
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
inline void *
pull_hdr(struct ifnet *ifp, struct mbuf **m, int ipoff, int off, int len,
    struct ip *h, int *action)
{
	u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;
	struct mbuf *n;
	int newoff;

	/* sanity check */
	if (ipoff > off) {
		printf("packetfilter: assumption failed on header location");
		*action = PF_DROP;
		return NULL;
	}
	if (fragoff) {
		if (fragoff >= len)
			*action = PF_PASS;
		else {
			*action = PF_DROP;
			printf("packetfilter: dropping following fragment");
			print_ip(ifp, h);
		}
		return NULL;
	}
	if ((*m)->m_pkthdr.len < off + len || ipoff + h->ip_len < off + len) {
		*action = PF_DROP;
		printf("packetfilter: dropping short packet");
		print_ip(ifp, h);
		return NULL;
	}
	/*
	 * XXX should use m_copydata, but NAT portion tries to touch mbuf
	 * directly
	 */
	n = m_pulldown((*m), off, len, &newoff);
	if (!n) {
		printf("packetfilter: pullup proto header failed\n");
		*action = PF_DROP;
		*m = NULL;
		return NULL;
	}
	return mtod(n, char *) + newoff;
}

int
pf_test(int direction, struct ifnet *ifp, struct mbuf **m)
{
	int action;
	struct ip *h = mtod(*m, struct ip *);
	int off;

	if (!status.running)
		return PF_PASS;

#ifdef DIAGNOSTIC
	if (((*m)->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif

	/* purge expire states, at most once every 10 seconds */
	microtime(&pftv);
	if (pftv.tv_sec - last_purge >= 10) {
		purge_expired_states();
		last_purge = pftv.tv_sec;
	}

	off = h->ip_hl << 2;

	/* ensure we have at least the complete ip header pulled up */
	if ((*m)->m_len < off)
		if ((*m = m_pullup(*m, off)) == NULL) {
			printf("packetfilter: pullup ip header failed\n");
			action = PF_DROP;
			goto done;
		}

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		struct tcphdr *th = pull_hdr(ifp, m, 0, off, sizeof(*th), h,
		    &action);

		if (th == NULL)
			goto done;
		if (pf_test_state_tcp(direction, ifp, m, off, h, th))
			action = PF_PASS;
		else
			action = pf_test_tcp(direction, ifp, off, h, th);
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr *uh = pull_hdr(ifp, m, 0, off, sizeof(*uh), h,
		    &action);

		if (uh == NULL)
			goto done;
		if (pf_test_state_udp(direction, ifp, m, off, h, uh))
			action = PF_PASS;
		else
			action = pf_test_udp(direction, ifp, off, h, uh);
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp *ih = pull_hdr(ifp, m, 0, off, sizeof(*ih), h,
		    &action);

		if (ih == NULL)
			goto done;
		if (pf_test_state_icmp(direction, ifp, m, off, h, ih))
			action = PF_PASS;
		else
			action = pf_test_icmp(direction, ifp, off, h, ih);
		break;
	}

	default:
		printf("packetfilter: dropping unsupported protocol");
		print_ip(ifp, h);
		action = PF_DROP;
		break;
	}

done:
	if (ifp == status_ifp) {
		status.bytes[direction] += h->ip_len;
		status.packets[direction][action]++;
	}
	return action;
}
