/*	$OpenBSD: pf_ioctl.c,v 1.22 2002/12/01 19:56:38 henning Exp $ */

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
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/pfvar.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#endif /* INET6 */

#ifdef ALTQ
#include <altq/altq.h>
#endif

void			 pfattach(int);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
struct pf_pool		*pf_get_pool(u_int32_t, u_int8_t, u_int8_t,
			    u_int8_t, u_int8_t);
int			 pf_add_addr(struct pf_pool *, struct pf_pooladdr *,
			    u_int8_t);
int			 pf_compare_addr_wrap(struct pf_addr_wrap *,
			    struct pf_addr_wrap *, sa_family_t);
int			 pf_compare_pool(struct pf_pool *, struct pf_pool *,
			    sa_family_t);
int			 pf_compare_rules(struct pf_rule *,
			    struct pf_rule *);
int			 pf_compare_nats(struct pf_nat *, struct pf_nat *);
int			 pf_compare_binats(struct pf_binat *,
			    struct pf_binat *);
int			 pf_compare_pooladdrs(struct pf_pooladdr *,
			    struct pf_pooladdr *, sa_family_t);
int			 pf_compare_rdrs(struct pf_rdr *, struct pf_rdr *);
void			 pf_mv_pool(struct pf_palist *, struct pf_palist *);
void			 pf_empty_pool(struct pf_palist *);
void			 pf_rm_rule(struct pf_rulequeue *, struct pf_rule *);
void			 pf_rm_nat(struct pf_natqueue *, struct pf_nat *);
void			 pf_rm_binat(struct pf_binatqueue *, struct pf_binat *);
void			 pf_rm_rdr(struct pf_rdrqueue *, struct pf_rdr *);
int			 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);

extern struct timeout	 pf_expire_to;

#define DPFPRINTF(n, x) if (pf_status.debug >= (n)) printf x

void
pfattach(int num)
{
	pool_init(&pf_tree_pl, sizeof(struct pf_tree_node), 0, 0, 0, "pftrpl",
	    NULL);
	pool_init(&pf_rule_pl, sizeof(struct pf_rule), 0, 0, 0, "pfrulepl",
	    &pool_allocator_nointr);
	pool_init(&pf_nat_pl, sizeof(struct pf_nat), 0, 0, 0, "pfnatpl",
	    &pool_allocator_nointr);
	pool_init(&pf_binat_pl, sizeof(struct pf_binat), 0, 0, 0, "pfbinatpl",
	    &pool_allocator_nointr);
	pool_init(&pf_rdr_pl, sizeof(struct pf_rdr), 0, 0, 0, "pfrdrpl",
	    &pool_allocator_nointr);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    NULL);
	pool_init(&pf_addr_pl, sizeof(struct pf_addr_dyn), 0, 0, 0, "pfaddr",
	    NULL);
	pool_init(&pf_altq_pl, sizeof(struct pf_altq), 0, 0, 0, "pfaltqpl",
	    NULL);
	pool_init(&pf_pooladdr_pl, sizeof(struct pf_pooladdr), 0, 0, 0,
	    "pfpooladdrpl", NULL);

	TAILQ_INIT(&pf_rules[0]);
	TAILQ_INIT(&pf_rules[1]);
	TAILQ_INIT(&pf_nats[0]);
	TAILQ_INIT(&pf_nats[1]);
	TAILQ_INIT(&pf_binats[0]);
	TAILQ_INIT(&pf_binats[1]);
	TAILQ_INIT(&pf_rdrs[0]);
	TAILQ_INIT(&pf_rdrs[1]);
	TAILQ_INIT(&pf_altqs[0]);
	TAILQ_INIT(&pf_altqs[1]);
	TAILQ_INIT(&pf_pabuf[0]);
	TAILQ_INIT(&pf_pabuf[1]);
	pf_rules_active = &pf_rules[0];
	pf_rules_inactive = &pf_rules[1];
	pf_nats_active = &pf_nats[0];
	pf_nats_inactive = &pf_nats[1];
	pf_binats_active = &pf_binats[0];
	pf_binats_inactive = &pf_binats[1];
	pf_rdrs_active = &pf_rdrs[0];
	pf_rdrs_inactive = &pf_rdrs[1];
	pf_altqs_active = &pf_altqs[0];
	pf_altqs_inactive = &pf_altqs[1];

	timeout_set(&pf_expire_to, pf_purge_timeout, &pf_expire_to);
	timeout_add(&pf_expire_to, pftm_interval * hz);

	pf_normalize_init();
	pf_status.debug = PF_DEBUG_URGENT;
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

struct pf_pool *
pf_get_pool(u_int32_t ticket, u_int8_t r_id, u_int8_t r_num, u_int8_t active,
    u_int8_t check_ticket)
{
	struct pf_rule *rule;
	struct pf_nat *nat;
	struct pf_rdr *rdr;
	u_int32_t nr = 0;

	switch (r_id & PF_POOL_IDMASK) {
	case PF_POOL_RULE_RT:
		if (active) {
			if (check_ticket && ticket != ticket_rules_active)
				break;
			if (r_id & PF_POOL_LAST)
				rule = TAILQ_LAST(pf_rules_active,
				    pf_rulequeue);
			else
				rule = TAILQ_FIRST(pf_rules_active);
		} else {
			if (check_ticket && ticket != ticket_rules_inactive)
				break;
			if (r_id & PF_POOL_LAST)
				rule = TAILQ_LAST(pf_rules_inactive,
				    pf_rulequeue);
			else
				rule = TAILQ_FIRST(pf_rules_inactive);
		}
		if (!(r_id & PF_POOL_LAST)) {
			while ((rule != NULL) && (rule->nr < r_num))
				rule = TAILQ_NEXT(rule, entries);
		}
		if (rule == NULL)
			break;
		return(&rule->rt_pool);
		break;

	case PF_POOL_NAT_R:
		if (active) {
			if (check_ticket && ticket != ticket_nats_active)
				break;
			if (r_id & PF_POOL_LAST)
				nat = TAILQ_LAST(pf_nats_active, pf_natqueue);
			else
				nat = TAILQ_FIRST(pf_nats_active);
		} else {
			if (check_ticket && ticket != ticket_nats_inactive)
				break;
			if (r_id & PF_POOL_LAST)
				nat = TAILQ_LAST(pf_nats_inactive, pf_natqueue);
			else
				nat = TAILQ_FIRST(pf_nats_inactive);
		}
		if (!(r_id & PF_POOL_LAST)) {
			while ((nat != NULL) && (nr < r_num)) {
				nat = TAILQ_NEXT(nat, entries);
				nr++;
			}
		}
		if (nat == NULL)
			break;
		return(&nat->rpool);
		break;

	case PF_POOL_RDR_R:
		if (active) {
			if (check_ticket && ticket != ticket_rdrs_active)
				break;
			if (r_id & PF_POOL_LAST)
				rdr = TAILQ_LAST(pf_rdrs_active, pf_rdrqueue);
			else
				rdr = TAILQ_FIRST(pf_rdrs_active);
		} else {
			if (check_ticket && ticket != ticket_rdrs_inactive)
				break;
			if (r_id & PF_POOL_LAST)
				rdr = TAILQ_LAST(pf_rdrs_inactive, pf_rdrqueue);
			else
				rdr = TAILQ_FIRST(pf_rdrs_inactive);
		}
		if (!(r_id & PF_POOL_LAST)) {
			while ((rdr != NULL) && (nr < r_num)) {
				rdr = TAILQ_NEXT(rdr, entries);
				nr++;
			}
		}
		if (rdr == NULL)
			break;
		return(&rdr->rpool);
		break;
	}
	return (NULL);
}

int
pf_add_addr(struct pf_pool *pool, struct pf_pooladdr *addr, u_int8_t af)
{
	struct pf_pooladdr *pa;

	pa = pool_get(&pf_pooladdr_pl, PR_NOWAIT);
	if (pa == NULL) {
		return (ENOMEM);
	}
	bcopy(addr, pa, sizeof(struct pf_pooladdr));
	if (pa->ifname[0]) {
		pa->ifp = ifunit((char *)&pa->ifname);
		if (pa->ifp == NULL) {
			pool_put(&pf_pooladdr_pl, pa);
			return (EINVAL);
		}
	} else
		pa->ifp = NULL;
	if (pf_dynaddr_setup(&pa->addr, af)) {
		pf_dynaddr_remove(&pa->addr);
		pool_put(&pf_pooladdr_pl, pa);
		return (EBUSY);
	}
	if (TAILQ_EMPTY(&pool->list)) {
		pool->cur = pa;
		PF_ACPY(&pool->counter, &pa->addr.addr, af);
	}
	TAILQ_INSERT_TAIL(&pool->list, pa, entries);

	return (0);
}

int
pf_compare_addr_wrap(struct pf_addr_wrap *a, struct pf_addr_wrap *b,
    sa_family_t af)
{
	if (a->addr_dyn != NULL && b->addr_dyn != NULL) {
		if (strcmp(a->addr_dyn->ifname, b->addr_dyn->ifname))
			return (1);
	} else {
		if (a->addr_dyn != NULL || b->addr_dyn != NULL)
			return (1);
		if (PF_ANEQ(&a->addr, &b->addr, af))
			return (1);
	}
	if (PF_ANEQ(&a->mask, &b->mask, af))
		return (1);
	return (0);
}

int
pf_compare_pool(struct pf_pool *a, struct pf_pool *b, sa_family_t af)
{
	struct pf_pooladdr *pa_a, *pa_b;

	if (a->key.key32[0] != b->key.key32[0] ||
	    a->key.key32[0] != b->key.key32[0] ||
	    a->key.key32[0] != b->key.key32[0] ||
	    a->key.key32[0] != b->key.key32[0] ||
	    a->opts != b->opts)
		return(1);
	pa_a = TAILQ_FIRST(&a->list);
	pa_b = TAILQ_FIRST(&b->list);
	while (pa_a != NULL && pa_b != NULL) {
		if (pf_compare_addr_wrap(&pa_a->addr, &pa_b->addr, af))
			return (1);
		if (strcmp(pa_a->ifname, pa_b->ifname))
			return (1);
		pa_a = TAILQ_NEXT(pa_a, entries);
		pa_b = TAILQ_NEXT(pa_b, entries);
	}
	return (0);
}

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
	    a->allow_opts != b->allow_opts ||
	    a->ifnot != b->ifnot)
		return (1);
	if (pf_compare_addr_wrap(&a->src.addr, &b->src.addr, a->af))
		return (1);
	if (a->src.port[0] != b->src.port[0] ||
	    a->src.port[1] != b->src.port[1] ||
	    a->src.not != b->src.not ||
	    a->src.port_op != b->src.port_op)
		return (1);
	if (pf_compare_addr_wrap(&a->dst.addr, &b->dst.addr, a->af))
		return (1);
	if (a->dst.port[0] != b->dst.port[0] ||
	    a->dst.port[1] != b->dst.port[1] ||
	    a->dst.not != b->dst.not ||
	    a->dst.port_op != b->dst.port_op)
		return (1);
	if (pf_compare_pool(&a->rt_pool, &b->rt_pool, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname) ||
	    strcmp(a->label, b->label))
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
	if (pf_compare_addr_wrap(&a->src.addr, &b->src.addr, a->af))
		return (1);
	if (a->src.port[0] != b->src.port[0] ||
	    a->src.port[1] != b->src.port[1] ||
	    a->src.not != b->src.not ||
	    a->src.port_op != b->src.port_op)
		return (1);
	if (pf_compare_addr_wrap(&a->dst.addr, &b->dst.addr, a->af))
		return (1);
	if (a->dst.port[0] != b->dst.port[0] ||
	    a->dst.port[1] != b->dst.port[1] ||
	    a->dst.not != b->dst.not ||
	    a->dst.port_op != b->dst.port_op)
		return (1);
	if (pf_compare_pool(&a->rpool, &b->rpool, a->af))
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
	if (pf_compare_addr_wrap(&a->saddr, &b->saddr, a->af))
		return (1);
	if (PF_ANEQ(&a->saddr.mask, &b->saddr.mask, a->af))
		return (1);
	if (pf_compare_addr_wrap(&a->daddr, &b->daddr, a->af))
		return (1);
	if (PF_ANEQ(&a->daddr.mask, &b->daddr.mask, a->af))
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
	if (pf_compare_addr_wrap(&a->saddr, &b->saddr, a->af))
		return (1);
	if (pf_compare_addr_wrap(&a->daddr, &b->daddr, a->af))
		return (1);
	if (pf_compare_pool(&a->rpool, &b->rpool, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pf_compare_pooladdrs(struct pf_pooladdr *a, struct pf_pooladdr *b,
    sa_family_t af)
{
	if (pf_compare_addr_wrap(&a->addr, &b->addr, af))
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

/*
 * Move the contents of poola to poolb
 */
void
pf_mv_pool(struct pf_palist *poola, struct pf_palist *poolb)
{
	struct pf_pooladdr *mv_pool_pa;

	while ((mv_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		TAILQ_REMOVE(poola, mv_pool_pa, entries);
		TAILQ_INSERT_TAIL(poolb, mv_pool_pa, entries);
	}
}

/*
 * Functions to clean up after ourselves.
 */
void
pf_empty_pool(struct pf_palist *poola)
{
	struct pf_pooladdr *empty_pool_pa;

	while ((empty_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		pf_dynaddr_remove(&empty_pool_pa->addr);
		TAILQ_REMOVE(poola, empty_pool_pa, entries);
		pool_put(&pf_pooladdr_pl, empty_pool_pa);
	}
}

void
pf_rm_rule(struct pf_rulequeue *rulequeue, struct pf_rule *rule)
{
	pf_dynaddr_remove(&rule->src.addr);
	pf_dynaddr_remove(&rule->dst.addr);
	pf_empty_pool(&rule->rt_pool.list);
	if (rulequeue != NULL)
		TAILQ_REMOVE(rulequeue, rule, entries);
	pool_put(&pf_rule_pl, rule);
}

void
pf_rm_nat(struct pf_natqueue *natqueue, struct pf_nat *nat)
{
	pf_dynaddr_remove(&nat->src.addr);
	pf_dynaddr_remove(&nat->dst.addr);
	pf_empty_pool(&nat->rpool.list);
	if (natqueue != NULL)
		TAILQ_REMOVE(natqueue, nat, entries);
	pool_put(&pf_nat_pl, nat);
}

void
pf_rm_binat(struct pf_binatqueue *binatqueue, struct pf_binat *binat)
{
	pf_dynaddr_remove(&binat->saddr);
	pf_dynaddr_remove(&binat->daddr);
	pf_dynaddr_remove(&binat->raddr);
	if (binatqueue != NULL)
		TAILQ_REMOVE(binatqueue, binat, entries);
	pool_put(&pf_binat_pl, binat);
}

void
pf_rm_rdr(struct pf_rdrqueue *rdrqueue, struct pf_rdr *rdr)
{
	pf_dynaddr_remove(&rdr->saddr);
	pf_dynaddr_remove(&rdr->daddr);
	pf_empty_pool(&rdr->rpool.list);
	if (rdrqueue != NULL)
		TAILQ_REMOVE(rdrqueue, rdr, entries);
	pool_put(&pf_rdr_pl, rdr);
}


int
pfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int error = 0;
	struct pf_pooladdr *pa = NULL;
	struct pf_pool *pool = NULL;
	int s;

	/* XXX keep in sync with switch() below */
	if (securelevel > 1)
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETNATS:
		case DIOCGETNAT:
		case DIOCGETBINATS:
		case DIOCGETBINAT:
		case DIOCGETRDRS:
		case DIOCGETRDR:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCSETSTATUSIF:
		case DIOCGETSTATUS:
		case DIOCCLRSTATUS:
		case DIOCNATLOOK:
		case DIOCSETDEBUG:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCCLRRULECTRS:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
			break;
		default:
			return (EPERM);
		}

	if (!(flags & FWRITE))
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETNATS:
		case DIOCGETNAT:
		case DIOCGETRDRS:
		case DIOCGETRDR:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCGETSTATUS:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCGETBINATS:
		case DIOCGETBINAT:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
			break;
		default:
			return (EACCES);
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
			pf_status.since = time.tv_sec;
			if (status_ifp != NULL)
				strlcpy(pf_status.ifname,
				    status_ifp->if_xname, IFNAMSIZ);
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
			pf_rm_rule(pf_rules_inactive, rule);
		}
		*ticket = ++ticket_rules_inactive;
		break;
	}

	case DIOCADDRULE: {
		struct pfioc_rule *pr = (struct pfioc_rule *)addr;
		struct pf_rule *rule, *tail;

		if (pr->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

		rule = pool_get(&pf_rule_pl, PR_NOWAIT);
		if (rule == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pr->rule, rule, sizeof(struct pf_rule));
		TAILQ_INIT(&rule->rt_pool.list);
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
		if (rule->ifname[0]) {
			rule->ifp = ifunit(rule->ifname);
			if (rule->ifp == NULL) {
				pool_put(&pf_rule_pl, rule);
				error = EINVAL;
				break;
			}
		} else
			rule->ifp = NULL;

		if (pf_dynaddr_setup(&rule->src.addr, rule->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&rule->dst.addr, rule->af))
			error = EINVAL;
		if (error) {
			pf_rm_rule(NULL, rule);
			break;
		}
		pf_mv_pool(&pf_pabuf[0], &rule->rt_pool.list);
		rule->rt_pool.cur = TAILQ_FIRST(&rule->rt_pool.list);
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
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
			n->state->rule.ptr = NULL;
		old_rules = pf_rules_active;
		pf_rules_active = pf_rules_inactive;
		pf_rules_inactive = old_rules;
		ticket_rules_active = ticket_rules_inactive;
		pf_calc_skip_steps(pf_rules_active);
		splx(s);

		/* Purge the old rule list. */
		while ((rule = TAILQ_FIRST(old_rules)) != NULL) {
			pf_rm_rule(old_rules, rule);
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
		pf_dynaddr_copyout(&pr->rule.src.addr);
		pf_dynaddr_copyout(&pr->rule.dst.addr);
		splx(s);
		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_changerule *pcr = (struct pfioc_changerule *)addr;
		struct pf_rule *oldrule = NULL, *newrule = NULL;
		u_int32_t nr = 0;

		if (pcr->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

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
			TAILQ_INIT(&newrule->rt_pool.list);
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
			if (newrule->ifname[0]) {
				newrule->ifp = ifunit(newrule->ifname);
				if (newrule->ifp == NULL) {
					pool_put(&pf_rule_pl, newrule);
					error = EINVAL;
					break;
				}
			} else
				newrule->ifp = NULL;

			if (pf_dynaddr_setup(&newrule->src.addr, newrule->af))
				error = EINVAL;
			if (pf_dynaddr_setup(&newrule->dst.addr, newrule->af))
				error = EINVAL;
			if (error) {
				pf_rm_rule(NULL, newrule);
				break;
			}
			pf_mv_pool(&pf_pabuf[0], &newrule->rt_pool.list);
			newrule->evaluations = newrule->packets = 0;
			newrule->bytes = 0;
		}
		pf_empty_pool(&pf_pabuf[0]);

		s = splsoftnet();

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(pf_rules_active);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(pf_rules_active, pf_rulequeue);
		else {
			pf_mv_pool(&pf_pabuf[0], &pcr->oldrule.rt_pool.list);
			oldrule = TAILQ_FIRST(pf_rules_active);
			while ((oldrule != NULL) && pf_compare_rules(oldrule,
			    &pcr->oldrule))
				oldrule = TAILQ_NEXT(oldrule, entries);
			pf_empty_pool(&pcr->oldrule.rt_pool.list);
			if (oldrule == NULL) {
				pf_rm_rule(NULL, newrule);
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE) {
			struct pf_tree_node *n;

			RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
				if (n->state->rule.ptr == oldrule)
					n->state->rule.ptr = NULL;
			pf_rm_rule(NULL, oldrule);
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
			pf_rm_nat(pf_nats_inactive, nat);
		}
		*ticket = ++ticket_nats_inactive;
		break;
	}

	case DIOCADDNAT: {
		struct pfioc_nat *pn = (struct pfioc_nat *)addr;
		struct pf_nat *nat;

		if (pn->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

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
		TAILQ_INIT(&nat->rpool.list);
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

		if (pf_dynaddr_setup(&nat->src.addr, nat->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&nat->dst.addr, nat->af))
			error = EINVAL;
		if (error) {
			pf_rm_nat(NULL, nat);
			break;
		}
		pf_mv_pool(&pf_pabuf[0], &nat->rpool.list);
		nat->rpool.cur = TAILQ_FIRST(&nat->rpool.list);
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
			pf_rm_nat(old_nats, nat);
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
		pf_dynaddr_copyout(&pn->nat.src.addr);
		pf_dynaddr_copyout(&pn->nat.dst.addr);
		splx(s);
		break;
	}

	case DIOCCHANGENAT: {
		struct pfioc_changenat *pcn = (struct pfioc_changenat *)addr;
		struct pf_nat *oldnat = NULL, *newnat = NULL;

		if (pcn->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

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
			TAILQ_INIT(&newnat->rpool.list);
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
			if (newnat->ifname[0]) {
				newnat->ifp = ifunit(newnat->ifname);
				if (newnat->ifp == NULL) {
					pool_put(&pf_nat_pl, newnat);
					error = EINVAL;
					break;
				}
			} else
				newnat->ifp = NULL;

			if (pf_dynaddr_setup(&newnat->src.addr, newnat->af))
				error = EINVAL;
			if (pf_dynaddr_setup(&newnat->dst.addr, newnat->af))
				error = EINVAL;
			if (error) {
				pf_rm_nat(NULL, newnat);
				break;
			}
			pf_mv_pool(&pf_pabuf[0], &newnat->rpool.list);
		}
		pf_empty_pool(&pf_pabuf[0]);

		s = splsoftnet();

		if (pcn->action == PF_CHANGE_ADD_HEAD)
			oldnat = TAILQ_FIRST(pf_nats_active);
		else if (pcn->action == PF_CHANGE_ADD_TAIL)
			oldnat = TAILQ_LAST(pf_nats_active, pf_natqueue);
		else {
			pf_mv_pool(&pf_pabuf[1], &pcn->oldnat.rpool.list);
			oldnat = TAILQ_FIRST(pf_nats_active);
			while ((oldnat != NULL) && pf_compare_nats(oldnat,
			    &pcn->oldnat))
				oldnat = TAILQ_NEXT(oldnat, entries);
			pf_empty_pool(&pcn->oldnat.rpool.list);
			if (oldnat == NULL) {
				pf_rm_nat(NULL, newnat);
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			pf_rm_nat(pf_nats_active, oldnat);
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
			pf_rm_binat(pf_binats_inactive, binat);
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
		if (pf_dynaddr_setup(&binat->saddr, binat->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&binat->daddr, binat->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&binat->raddr, binat->af))
			error = EINVAL;
		if (error) {
			pf_rm_binat(NULL, binat);
			break;
		}
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
			pf_rm_binat(old_binats, binat);
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
		pf_dynaddr_copyout(&pb->binat.saddr);
		pf_dynaddr_copyout(&pb->binat.daddr);
		pf_dynaddr_copyout(&pb->binat.raddr);
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
			if (newbinat->ifname[0]) {
				newbinat->ifp = ifunit(newbinat->ifname);
				if (newbinat->ifp == NULL) {
					pool_put(&pf_binat_pl, newbinat);
					error = EINVAL;
					break;
				}
			} else
				newbinat->ifp = NULL;
			if (pf_dynaddr_setup(&newbinat->saddr, newbinat->af))
				error = EINVAL;
			if (pf_dynaddr_setup(&newbinat->daddr, newbinat->af))
				error = EINVAL;
			if (pf_dynaddr_setup(&newbinat->raddr, newbinat->af))
				error = EINVAL;
			if (error) {
				pf_rm_binat(NULL, newbinat);
				break;
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
				pf_rm_binat(NULL, newbinat);
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			pf_rm_binat(pf_binats_active, oldbinat);
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
			pf_rm_rdr(pf_rdrs_inactive, rdr);
		}
		*ticket = ++ticket_rdrs_inactive;
		break;
	}

	case DIOCADDRDR: {
		struct pfioc_rdr *pr = (struct pfioc_rdr *)addr;
		struct pf_rdr *rdr;

		if (pr->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

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
		TAILQ_INIT(&rdr->rpool.list);
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

		if (pf_dynaddr_setup(&rdr->saddr, rdr->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&rdr->daddr, rdr->af))
			error = EINVAL;
		if (error) {
			pf_rm_rdr(NULL, rdr);
			break;
		}
		pf_mv_pool(&pf_pabuf[0], &rdr->rpool.list);
		rdr->rpool.cur = TAILQ_FIRST(&rdr->rpool.list);
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
			pf_rm_rdr(old_rdrs, rdr);
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
		pf_dynaddr_copyout(&pr->rdr.saddr);
		pf_dynaddr_copyout(&pr->rdr.daddr);
		splx(s);
		break;
	}

	case DIOCCHANGERDR: {
		struct pfioc_changerdr *pcn = (struct pfioc_changerdr *)addr;
		struct pf_rdr *oldrdr = NULL, *newrdr = NULL;

		if (pcn->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

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
			TAILQ_INIT(&newrdr->rpool.list);
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
			if (newrdr->ifname[0]) {
				newrdr->ifp = ifunit(newrdr->ifname);
				if (newrdr->ifp == NULL) {
					pool_put(&pf_rdr_pl, newrdr);
					error = EINVAL;
					break;
				}
			} else
				newrdr->ifp = NULL;

			if (pf_dynaddr_setup(&newrdr->saddr, newrdr->af))
				error = EINVAL;
			if (pf_dynaddr_setup(&newrdr->daddr, newrdr->af))
				error = EINVAL;
			if (error) {
				pf_rm_rdr(pf_rdrs_inactive, newrdr);
				break;
			}
			pf_mv_pool(&pf_pabuf[0], &newrdr->rpool.list);
		}
		pf_empty_pool(&pf_pabuf[0]);

		s = splsoftnet();

		if (pcn->action == PF_CHANGE_ADD_HEAD)
			oldrdr = TAILQ_FIRST(pf_rdrs_active);
		else if (pcn->action == PF_CHANGE_ADD_TAIL)
			oldrdr = TAILQ_LAST(pf_rdrs_active, pf_rdrqueue);
		else {
			pf_mv_pool(&pf_pabuf[1], &pcn->oldrdr.rpool.list);
			oldrdr = TAILQ_FIRST(pf_rdrs_active);
			while ((oldrdr != NULL) && pf_compare_rdrs(oldrdr,
			    &pcn->oldrdr))
				oldrdr = TAILQ_NEXT(oldrdr, entries);
			pf_empty_pool(&pcn->oldrdr.rpool.list);
			if (oldrdr == NULL) {
				pf_rm_rdr(NULL, newrdr);
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcn->action == PF_CHANGE_REMOVE) {
			pf_rm_rdr(pf_rdrs_active, oldrdr);
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
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
			n->state->expire = 0;
		pf_purge_expired_states();
		pf_status.states = 0;
		splx(s);
		break;
	}

	case DIOCKILLSTATES: {
		struct pf_tree_node *n;
		struct pf_state *st;
		struct pfioc_state_kill *psk =
		    (struct pfioc_state_kill *)addr;
		int killed = 0;

		s = splsoftnet();
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy) {
			st = n->state;
			if ((!psk->psk_af || st->af == psk->psk_af) &&
			    (!psk->psk_proto || psk->psk_proto == st->proto) &&
			    PF_MATCHA(psk->psk_src.not, &psk->psk_src.addr.addr,
			    &psk->psk_src.addr.mask, &st->lan.addr, st->af) &&
			    PF_MATCHA(psk->psk_dst.not, &psk->psk_dst.addr.addr,
			    &psk->psk_dst.addr.mask, &st->ext.addr, st->af) &&
			    (psk->psk_src.port_op == 0 ||
			    pf_match_port(psk->psk_src.port_op,
			    psk->psk_src.port[0], psk->psk_src.port[1],
			    st->lan.port)) &&
			    (psk->psk_dst.port_op == 0 ||
			    pf_match_port(psk->psk_dst.port_op,
			    psk->psk_dst.port[0], psk->psk_dst.port[1],
			    st->ext.port))) {
				st->expire = 0;
				killed++;
			}
		}
		pf_purge_expired_states();
		splx(s);
		psk->psk_af = killed;
		break;
	}

	case DIOCADDSTATE: {
		struct pfioc_state *ps = (struct pfioc_state *)addr;
		struct pf_state *state;

		state = pool_get(&pf_state_pl, PR_NOWAIT);
		if (state == NULL) {
			error = ENOMEM;
			break;
		}
		s = splsoftnet();
		bcopy(&ps->state, state, sizeof(struct pf_state));
		state->rule.ptr = NULL;
		state->creation = time.tv_sec;
		state->expire += state->creation;
		state->packets = 0;
		state->bytes = 0;
		if (pf_insert_state(state)) {
			pool_put(&pf_state_pl, state);
			error = ENOMEM;
		}
		splx(s);
	}

	case DIOCGETSTATE: {
		struct pfioc_state *ps = (struct pfioc_state *)addr;
		struct pf_tree_node *n;
		u_int32_t nr;
		int secs;

		nr = 0;
		s = splsoftnet();
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy) {
			if (nr >= ps->nr)
				break;
			nr++;
		}
		if (n == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(n->state, &ps->state, sizeof(struct pf_state));
		if (n->state->rule.ptr == NULL)
			ps->state.rule.nr = -1;
		else
			ps->state.rule.nr = n->state->rule.ptr->nr;
		splx(s);
		secs = time.tv_sec;
		ps->state.creation = secs - ps->state.creation;
		if (ps->state.expire <= (unsigned)secs)
			ps->state.expire = 0;
		else
			ps->state.expire -= secs;
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
			RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
				nr++;
			splx(s);
			ps->ps_len = sizeof(struct pf_state) * nr;
			return (0);
		}

		s = splsoftnet();
		p = ps->ps_states;
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy) {
			int secs = time.tv_sec;

			if ((nr + 1) * sizeof(*p) > (unsigned)ps->ps_len)
				break;

			bcopy(n->state, &pstore, sizeof(pstore));
			if (n->state->rule.ptr == NULL)
				pstore.rule.nr = -1;
			else
				pstore.rule.nr = n->state->rule.ptr->nr;
			pstore.creation = secs - pstore.creation;
			if (pstore.expire <= (unsigned)secs)
				pstore.expire = 0;
			else
				pstore.expire -= secs;
			error = copyout(&pstore, p, sizeof(*p));
			if (error) {
				splx(s);
				goto fail;
			}
			p++;
			nr++;
		}
		ps->ps_len = sizeof(struct pf_state) * nr;
		splx(s);
		break;
	}

	case DIOCSETSTATUSIF: {
		struct pfioc_if *pi = (struct pfioc_if *)addr;
		struct ifnet *ifp;

		if (pi->ifname[0] == 0) {
			status_ifp = NULL;
			bzero(pf_status.ifname, IFNAMSIZ);
		} else
			if ((ifp = ifunit(pi->ifname)) == NULL)
				error = EINVAL;
			else {
				status_ifp = ifp;
				strlcpy(pf_status.ifname, ifp->if_xname, IFNAMSIZ);
			}
		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;
		bcopy(&pf_status, s, sizeof(struct pf_status));
		break;
	}

	case DIOCCLRSTATUS: {
		u_int32_t running = pf_status.running;
		u_int32_t states = pf_status.states;
		u_int32_t since = pf_status.since;
		u_int32_t debug = pf_status.debug;

		bzero(&pf_status, sizeof(struct pf_status));
		pf_status.running = running;
		pf_status.states = states;
		pf_status.since = since;
		pf_status.debug = debug;
		if (status_ifp != NULL)
			strlcpy(pf_status.ifname,
			    status_ifp->if_xname, IFNAMSIZ);
		break;
	}

	case DIOCNATLOOK: {
		struct pfioc_natlook *pnl = (struct pfioc_natlook *)addr;
		struct pf_state *st;
		struct pf_tree_node key;
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
				st = pf_find_state(&tree_ext_gwy, &key);
			else
				st = pf_find_state(&tree_lan_ext, &key);
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

	case DIOCGETLIMIT: {
		struct pfioc_limit *pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			goto fail;
		}
		pl->limit = pf_pool_limits[pl->index].limit;
		break;
	}

	case DIOCSETLIMIT: {
		struct pfioc_limit *pl = (struct pfioc_limit *)addr;
		int old_limit;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			goto fail;
		}
		if (pool_sethardlimit(pf_pool_limits[pl->index].pp,
		    pl->limit, NULL, 0) != 0) {
			error = EBUSY;
			goto fail;
		}
		old_limit = pf_pool_limits[pl->index].limit;
		pf_pool_limits[pl->index].limit = pl->limit;
		pl->limit = old_limit;
		break;
	}

	case DIOCSETDEBUG: {
		u_int32_t *level = (u_int32_t *)addr;
		pf_status.debug = *level;
		break;
	}

	case DIOCCLRRULECTRS: {
		struct pf_rule *rule;

		s = splsoftnet();
		TAILQ_FOREACH(rule, pf_rules_active, entries)
			rule->evaluations = rule->packets =
			    rule->bytes = 0;
		splx(s);
		break;
	}

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq *altq;
		struct ifnet *ifp;
		struct tb_profile tb;

		/* enable all altq interfaces on active list */
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				if ((ifp = ifunit(altq->ifname)) == NULL) {
					error = EINVAL;
					break;
				}
				if (ifp->if_snd.altq_type != ALTQT_NONE)
					error = altq_enable(&ifp->if_snd);
				if (error != 0)
					break;
				/* set tokenbucket regulator */
				tb.rate = altq->ifbandwidth;
				tb.depth = altq->tbrsize;
				error = tbr_set(&ifp->if_snd, &tb);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pfaltq_running = 1;
		splx(s);
		DPFPRINTF(PF_DEBUG_MISC, ("altq: started\n"));
		break;
	}

	case DIOCSTOPALTQ: {
		struct pf_altq *altq;
		struct ifnet *ifp;
		struct tb_profile tb;
		int err;

		/* disable all altq interfaces on active list */
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				if ((ifp = ifunit(altq->ifname)) == NULL) {
					error = EINVAL;
					break;
				}
				if (ifp->if_snd.altq_type != ALTQT_NONE) {
					err = altq_disable(&ifp->if_snd);
					if (err != 0 && error == 0)
						error = err;
				}
				/* clear tokenbucket regulator */
				tb.rate = 0;
				err = tbr_set(&ifp->if_snd, &tb);
				if (err != 0 && error == 0)
					error = err;
			}
		}
		if (error == 0)
			pfaltq_running = 0;
		splx(s);
		DPFPRINTF(PF_DEBUG_MISC, ("altq: stopped\n"));
		break;
	}

	case DIOCBEGINALTQS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_altq *altq;

		/* Purge the old altq list */
		while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
			TAILQ_REMOVE(pf_altqs_inactive, altq, entries);

			if (altq->qname[0] == 0) {
				/* detach and destroy the discipline */
				error = altq_remove(altq);
			}

			pool_put(&pf_altq_pl, altq);
		}

		*ticket = ++ticket_altqs_inactive;
		break;
	}

	case DIOCADDALTQ: {
		struct pfioc_altq *pa = (struct pfioc_altq *)addr;
		struct pf_altq *altq, *a;

		if (pa->ticket != ticket_altqs_inactive) {
			error = EBUSY;
			break;
		}
		altq = pool_get(&pf_altq_pl, PR_NOWAIT);
		if (altq == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pa->altq, altq, sizeof(struct pf_altq));

		/*
		 * if this is for a queue, find the discipline and
		 * copy the necessary fields
		 */
		if (altq->qname[0] != 0) {
			TAILQ_FOREACH(a, pf_altqs_inactive, entries) {
				if (strncmp(a->ifname, altq->ifname, IFNAMSIZ)
				    == 0 && a->qname[0] == 0) {
					altq->altq_disc = a->altq_disc;
					break;
				}
			}
		}

		error = altq_add(altq);

		if (error) {
			pool_put(&pf_altq_pl, altq);
			break;
		}

		TAILQ_INSERT_TAIL(pf_altqs_inactive, altq, entries);

		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCCOMMITALTQS: {
		u_int32_t *ticket = (u_int32_t *)addr;
		struct pf_altqqueue *old_altqs;
		struct pf_altq *altq;
		int err;

		if (*ticket != ticket_altqs_inactive) {
			error = EBUSY;
			break;
		}

		/* Swap altqs, keep the old. */
		s = splsoftnet();
		old_altqs = pf_altqs_active;
		pf_altqs_active = pf_altqs_inactive;
		pf_altqs_inactive = old_altqs;
		ticket_altqs_active = ticket_altqs_inactive;

		/* Attach new disciplines */
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				/* attach the discipline */
				error = altq_pfattach(altq);
				if (error) {
					splx(s);
					goto fail;
				}
			}
		}

		/* Purge the old altq list */
		while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
			TAILQ_REMOVE(pf_altqs_inactive, altq, entries);

			if (altq->qname[0] == 0) {
				/* detach and destroy the discipline */
				err = altq_pfdetach(altq);
				if (err != 0 && error == 0)
					error = err;
				err = altq_remove(altq);
				if (err != 0 && error == 0)
					error = err;
			}

			pool_put(&pf_altq_pl, altq);
		}
		splx(s);
		break;
	}

	case DIOCGETALTQS: {
		struct pfioc_altq *pa = (struct pfioc_altq *)addr;
		struct pf_altq *altq;

		pa->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = ticket_altqs_active;
		splx(s);
		break;
	}

	case DIOCGETALTQ: {
		struct pfioc_altq *pa = (struct pfioc_altq *)addr;
		struct pf_altq *altq;
		u_int32_t nr;

		if (pa->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		s = splsoftnet();
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pa->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		splx(s);
		break;
	}

	case DIOCCHANGEALTQ:
		/* CHANGEALTQ not supported yet! */
		error = ENODEV;
		break;

	case DIOCGETQSTATS: {
		struct pfioc_qstats *pq = (struct pfioc_qstats *)addr;
		struct pf_altq *altq;
		u_int32_t nr;
		int nbytes;

		if (pq->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nbytes = pq->nbytes;
		nr = 0;
		s = splsoftnet();
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pq->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		error = altq_getqstats(altq, pq->buf, &nbytes);
		splx(s);
		if (error == 0) {
			pq->scheduler = altq->scheduler;
			pq->nbytes = nbytes;
		}
		break;
	}
#endif /* ALTQ */

	case DIOCBEGINADDRS: {
		u_int32_t *ticket = (u_int32_t *)addr;

		pf_empty_pool(&pf_pabuf[1]);
		pf_mv_pool(&pf_pabuf[0], &pf_pabuf[1]);

		*ticket = ++ticket_pabuf;

		break;
	}

	case DIOCADDADDR: {
		struct pfioc_pooladdr *pp = (struct pfioc_pooladdr *)addr;

#ifndef INET
		if (pp->af == AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (pp->af == AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		pa = pool_get(&pf_pooladdr_pl, PR_NOWAIT);
		if (pa == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pp->addr, pa, sizeof(struct pf_pooladdr));
		if (pa->ifname[0]) {
			pa->ifp = ifunit(pa->ifname);
			if (pa->ifp == NULL) {
				pool_put(&pf_pooladdr_pl, pa);
				error = EINVAL;
				break;
			}
		}
		if (pf_dynaddr_setup(&pa->addr, pp->af)) {
			pf_dynaddr_remove(&pa->addr);
			pool_put(&pf_pooladdr_pl, pa);
			error = EINVAL;
			break;
		}
		TAILQ_INSERT_TAIL(&pf_pabuf[0], pa, entries);

		break;
	}

	case DIOCGETADDRS: {
		struct pfioc_pooladdr *pp = (struct pfioc_pooladdr *)addr;

		pp->nr = 0;
		s = splsoftnet();
		pool = pf_get_pool(pp->ticket, pp->r_id, pp->r_num, 1, 0);
		if (pool == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		TAILQ_FOREACH(pa, &pool->list, entries)
			pp->nr++;
		splx(s);
		break;
	}

	case DIOCGETADDR: {
		struct pfioc_pooladdr *pp = (struct pfioc_pooladdr *)addr;
		u_int32_t nr = 0;

		s = splsoftnet();
		pool = pf_get_pool(pp->ticket, pp->r_id, pp->r_num, 1, 1);
		if (pool == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		pa = TAILQ_FIRST(&pool->list);
		while ((pa != NULL) && (nr < pp->nr)) {
			pa = TAILQ_NEXT(pa, entries);
			nr++;
		}
		if (pa == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(pa, &pp->addr, sizeof(struct pf_pooladdr));
		pf_dynaddr_copyout(&pp->addr.addr);
		splx(s);
		break;
	}

	case DIOCCHANGEADDR: {
		struct pfioc_changeaddr *pca = (struct pfioc_changeaddr *)addr;
		struct pf_pooladdr *oldpa = NULL, *newpa = NULL;

		if (pca->action < PF_CHANGE_ADD_HEAD ||
		    pca->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		pool = pf_get_pool(0, pca->r_id, pca->r_num, 1, 0);
		if (pool == NULL) {
			error = EBUSY;
			break;
		}
		if (pca->action != PF_CHANGE_REMOVE) {
			newpa = pool_get(&pf_pooladdr_pl, PR_NOWAIT);
			if (newpa == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pca->newaddr, newpa, sizeof(struct pf_pooladdr));
#ifndef INET
			if (pca->af == AF_INET) {
				pool_put(&pf_pooladdr_pl, newpa);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (pca->af == AF_INET6) {
				pool_put(&pf_pooladdr_pl, newpa);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			if (newpa->ifname[0]) {
				newpa->ifp = ifunit(newpa->ifname);
				if (newpa->ifp == NULL) {
					pool_put(&pf_pooladdr_pl, newpa);
					error = EINVAL;
					break;
				}
			} else
				newpa->ifp = NULL;
			if (pf_dynaddr_setup(&newpa->addr, pca->af)) {
				pf_dynaddr_remove(&newpa->addr);
				pool_put(&pf_pooladdr_pl, newpa);
				error = EINVAL;
				break;
			}
		}

		s = splsoftnet();

		if (pca->action == PF_CHANGE_ADD_HEAD)
			oldpa = TAILQ_FIRST(&pool->list);
		else if (pca->action == PF_CHANGE_ADD_TAIL)
			oldpa = TAILQ_LAST(&pool->list, pf_palist);
		else {
			oldpa = TAILQ_FIRST(&pool->list);
			while ((oldpa != NULL) && pf_compare_pooladdrs(oldpa,
			    &pca->oldaddr, pca->af))
				oldpa = TAILQ_NEXT(oldpa, entries);
			if (oldpa == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pca->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(&pool->list, oldpa, entries);
			pf_dynaddr_remove(&oldpa->addr);
			pool_put(&pf_pooladdr_pl, oldpa);
		} else {
			if (oldpa == NULL)
				TAILQ_INSERT_TAIL(&pool->list, newpa, entries);
			else if (pca->action == PF_CHANGE_ADD_HEAD ||
			    pca->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldpa, newpa, entries);
			else
				TAILQ_INSERT_AFTER(&pool->list, oldpa,
				    newpa, entries);
		}

		pool->cur = TAILQ_FIRST(&pool->list);
		PF_ACPY(&pool->counter, &pool->cur->addr.addr, pca->af);

		splx(s);
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:

	return (error);
}
