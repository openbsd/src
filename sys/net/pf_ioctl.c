/*	$OpenBSD: pf_ioctl.c,v 1.32 2002/12/27 15:20:30 dhartmei Exp $ */

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
struct pf_pool		*pf_get_pool(char *, char *, u_int32_t,
			    u_int8_t, u_int8_t, u_int8_t, u_int8_t, u_int8_t);
int			 pf_add_addr(struct pf_pool *, struct pf_pooladdr *,
			    u_int8_t);
int			 pf_get_ruleset_number(u_int8_t);
void			 pf_init_ruleset(struct pf_ruleset *);
struct pf_anchor	*pf_find_anchor(const char *);
struct pf_ruleset	*pf_find_ruleset(char *, char *);
struct pf_ruleset	*pf_find_or_create_ruleset(char *, char *, int);
void			 pf_remove_if_empty_ruleset(struct pf_ruleset *);
void			 pf_mv_pool(struct pf_palist *, struct pf_palist *);
void			 pf_empty_pool(struct pf_palist *);
void			 pf_rm_rule(struct pf_rulequeue *, struct pf_rule *);
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
	pool_init(&pf_addr_pl, sizeof(struct pf_addr_dyn), 0, 0, 0, "pfaddrpl",
	    &pool_allocator_nointr);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    NULL);
	pool_init(&pf_altq_pl, sizeof(struct pf_altq), 0, 0, 0, "pfaltqpl",
	    NULL);
	pool_init(&pf_pooladdr_pl, sizeof(struct pf_pooladdr), 0, 0, 0,
	    "pfpooladdrpl", NULL);

	TAILQ_INIT(&pf_anchors);
	pf_init_ruleset(&pf_main_ruleset);
	TAILQ_INIT(&pf_altqs[0]);
	TAILQ_INIT(&pf_altqs[1]);
	TAILQ_INIT(&pf_pabuf);
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
pf_get_pool(char *anchorname, char *rulesetname, u_int32_t ticket,
    u_int8_t rule_action, u_int8_t rule_number, u_int8_t r_last,
    u_int8_t active, u_int8_t check_ticket)
{
	struct pf_ruleset	*ruleset;
	struct pf_rule		*rule;
	int			 rs_num;

	ruleset = pf_find_ruleset(anchorname, rulesetname);
	if (ruleset == NULL)
		return (NULL);
	rs_num = pf_get_ruleset_number(rule_action);
	if (active) {
		if (check_ticket && ticket !=
		    ruleset->rules[rs_num].active.ticket)
			return (NULL);
		if (r_last)
			rule = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
			    pf_rulequeue);
		else
			rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
	} else {
		if (check_ticket && ticket !=
		    ruleset->rules[rs_num].inactive.ticket)
			return (NULL);
		if (r_last)
			rule = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
			    pf_rulequeue);
		else
			rule = TAILQ_FIRST(ruleset->rules[rs_num].inactive.ptr);
	}
	if (!r_last) {
		while ((rule != NULL) && (rule->nr != rule_number))
			rule = TAILQ_NEXT(rule, entries);
	}
	if (rule == NULL)
		return(NULL);

	return (&rule->rpool);
}

int
pf_add_addr(struct pf_pool *pool, struct pf_pooladdr *addr, u_int8_t af)
{
	struct pf_pooladdr	*pa;

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
	if (pf_dynaddr_setup(&pa->addr.addr, af)) {
		pf_dynaddr_remove(&pa->addr.addr);
		pool_put(&pf_pooladdr_pl, pa);
		return (EBUSY);
	}
	if (TAILQ_EMPTY(&pool->list)) {
		pool->cur = pa;
		PF_ACPY(&pool->counter, &pa->addr.addr.addr, af);
	}
	TAILQ_INSERT_TAIL(&pool->list, pa, entries);

	return (0);
}


int
pf_get_ruleset_number(u_int8_t action)
{
	switch (action) {
	case PF_PASS:
	case PF_DROP:
	case PF_SCRUB:
	default:
		return (PF_RULESET_RULE);
		break;
	case PF_NAT:
	case PF_NONAT:
		return (PF_RULESET_NAT);
		break;
	case PF_BINAT:
	case PF_NOBINAT:
		return (PF_RULESET_BINAT);
		break;
	case PF_RDR:
	case PF_NORDR:
		return (PF_RULESET_RDR);
		break;
	}
}

void
pf_init_ruleset(struct pf_ruleset *ruleset)
{
	int	i;

	memset(ruleset, 0, sizeof(struct pf_ruleset));
	for(i = 0; i < PF_RULESET_MAX; i++) {
		TAILQ_INIT(&ruleset->rules[i].queues[0]);
		TAILQ_INIT(&ruleset->rules[i].queues[1]);
		ruleset->rules[i].active.ptr = &ruleset->rules[i].queues[0];
		ruleset->rules[i].inactive.ptr = &ruleset->rules[i].queues[1];
	}
}

struct pf_anchor *
pf_find_anchor(const char *anchorname)
{
	struct pf_anchor	*anchor;
	int			 n = -1;

	anchor = TAILQ_FIRST(&pf_anchors);
	while (anchor != NULL && (n = strcmp(anchor->name, anchorname)) < 0)
		anchor = TAILQ_NEXT(anchor, entries);
	if (n == 0)
		return (anchor);
	else
		return (NULL);
}

struct pf_ruleset *
pf_find_ruleset(char *anchorname, char *rulesetname)
{
	struct pf_anchor	*anchor;
	struct pf_ruleset	*ruleset;

	if (!anchorname[0] && !rulesetname[0])
		return (&pf_main_ruleset);
	if (!anchorname[0] || !rulesetname[0])
		return (NULL);
	anchorname[PF_ANCHOR_NAME_SIZE-1] = 0;
	rulesetname[PF_RULESET_NAME_SIZE-1] = 0;
	anchor = pf_find_anchor(anchorname);
	if (anchor == NULL)
		return (NULL);
	ruleset = TAILQ_FIRST(&anchor->rulesets);
	while (ruleset != NULL && strcmp(ruleset->name, rulesetname) < 0)
		ruleset = TAILQ_NEXT(ruleset, entries);
	if (ruleset != NULL && !strcmp(ruleset->name, rulesetname))
		return (ruleset);
	else
		return (NULL);
}

struct pf_ruleset *
pf_find_or_create_ruleset(char *anchorname, char *rulesetname, int rs_num)
{
	struct pf_anchor	*anchor, *a;
	struct pf_ruleset	*ruleset, *r;

	if (!anchorname[0] && !rulesetname[0])
		return (&pf_main_ruleset);
	if (!anchorname[0] || !rulesetname[0])
		return (NULL);
	anchorname[PF_ANCHOR_NAME_SIZE-1] = 0;
	rulesetname[PF_RULESET_NAME_SIZE-1] = 0;
	a = TAILQ_FIRST(&pf_anchors);
	while (a != NULL && strcmp(a->name, anchorname) < 0)
		a = TAILQ_NEXT(a, entries);
	if (a != NULL && !strcmp(a->name, anchorname))
		anchor = a;
	else {
		anchor = (struct pf_anchor *)malloc(sizeof(struct pf_anchor),
		    M_TEMP, M_NOWAIT);
		if (anchor == NULL)
			return (NULL);
		memset(anchor, 0, sizeof(struct pf_anchor));
		bcopy(anchorname, anchor->name, sizeof(anchor->name));
		TAILQ_INIT(&anchor->rulesets);
		if (a != NULL)
			TAILQ_INSERT_BEFORE(a, anchor, entries);
		else
			TAILQ_INSERT_TAIL(&pf_anchors, anchor, entries);
	}
	r = TAILQ_FIRST(&anchor->rulesets);
	while (r != NULL && strcmp(r->name, rulesetname) < 0)
		r = TAILQ_NEXT(r, entries);
	if (r != NULL && !strcmp(r->name, rulesetname))
		return (r);
	ruleset = (struct pf_ruleset *)malloc(sizeof(struct pf_ruleset),
	    M_TEMP, M_NOWAIT);
	if (ruleset != NULL) {
		pf_init_ruleset(ruleset);
		bcopy(rulesetname, ruleset->name, sizeof(ruleset->name));
		ruleset->anchor = anchor;
		if (r != NULL)
			TAILQ_INSERT_BEFORE(r, ruleset, entries);
		else
			TAILQ_INSERT_TAIL(&anchor->rulesets, ruleset, entries);
	}
	return (ruleset);
}

void
pf_remove_if_empty_ruleset(struct pf_ruleset *ruleset)
{
	struct pf_anchor	*anchor;

	if (ruleset == NULL || ruleset->anchor == NULL ||
	    !TAILQ_EMPTY(ruleset->rules[0].active.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[0].inactive.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[1].active.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[1].inactive.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[2].active.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[2].inactive.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[3].active.ptr) ||
	    !TAILQ_EMPTY(ruleset->rules[3].inactive.ptr))
		return;

	anchor = ruleset->anchor;
	TAILQ_REMOVE(&anchor->rulesets, ruleset, entries);
	free(ruleset, M_TEMP);

	if (TAILQ_EMPTY(&anchor->rulesets)) {
		TAILQ_REMOVE(&pf_anchors, anchor, entries);
		free(anchor, M_TEMP);
	}
}

void
pf_mv_pool(struct pf_palist *poola, struct pf_palist *poolb)
{
	struct pf_pooladdr	*mv_pool_pa;

	while ((mv_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		TAILQ_REMOVE(poola, mv_pool_pa, entries);
		TAILQ_INSERT_TAIL(poolb, mv_pool_pa, entries);
	}
}

void
pf_empty_pool(struct pf_palist *poola)
{
	struct pf_pooladdr	*empty_pool_pa;

	while ((empty_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		pf_dynaddr_remove(&empty_pool_pa->addr.addr);
		TAILQ_REMOVE(poola, empty_pool_pa, entries);
		pool_put(&pf_pooladdr_pl, empty_pool_pa);
	}
}

void
pf_rm_rule(struct pf_rulequeue *rulequeue, struct pf_rule *rule)
{
	pf_dynaddr_remove(&rule->src.addr);
	pf_dynaddr_remove(&rule->dst.addr);
	pf_empty_pool(&rule->rpool.list);
	if (rulequeue != NULL)
		TAILQ_REMOVE(rulequeue, rule, entries);
	pool_put(&pf_rule_pl, rule);
}

int
pfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	struct pf_pooladdr	*pa = NULL;
	struct pf_pool		*pool = NULL;
	int			 s;
	int			 error = 0;

	/* XXX keep in sync with switch() below */
	if (securelevel > 1)
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
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
		case DIOCGETANCHORS:
		case DIOCGETANCHOR:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
			break;
		default:
			return (EPERM);
		}

	if (!(flags & FWRITE))
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCGETSTATUS:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
		case DIOCGETANCHORS:
		case DIOCGETANCHOR:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
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
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule;
		int			 rs_num;

		ruleset = pf_find_or_create_ruleset(pr->anchor,
		    pr->ruleset, rs_num);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		while ((rule =
		    TAILQ_FIRST(ruleset->rules[rs_num].inactive.ptr)) != NULL)
			pf_rm_rule(ruleset->rules[rs_num].inactive.ptr, rule);
		pr->ticket = ++ruleset->rules[rs_num].inactive.ticket;
		break;
	}

	case DIOCADDRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule, *tail;
		int			 rs_num;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (pr->rule.anchorname[0] && ruleset != &pf_main_ruleset) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules[rs_num].inactive.ticket) {
			error = EBUSY;
			break;
		}
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
		rule->anchor = NULL;
		rule->ifp = NULL;
		TAILQ_INIT(&rule->rpool.list);
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
		tail = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
		    pf_rulequeue);
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
		}

		if (pf_dynaddr_setup(&rule->src.addr, rule->af))
			error = EINVAL;
		if (pf_dynaddr_setup(&rule->dst.addr, rule->af))
			error = EINVAL;
		if (error) {
			pf_rm_rule(NULL, rule);
			break;
		}
		pf_mv_pool(&pf_pabuf, &rule->rpool.list);
		rule->rpool.cur = TAILQ_FIRST(&rule->rpool.list);
		rule->evaluations = rule->packets = rule->bytes = 0;
		TAILQ_INSERT_TAIL(ruleset->rules[rs_num].inactive.ptr,
		    rule, entries);
		break;
	}

	case DIOCCOMMITRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rulequeue	*old_rules;
		struct pf_rule		*rule;
		struct pf_tree_node	*n;
		int			 rs_num;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (pr->ticket != ruleset->rules[rs_num].inactive.ticket) {
			error = EBUSY;
			break;
		}

		/* Swap rules, keep the old. */
		s = splsoftnet();
		/*
		 * Rules are about to get freed, clear rule pointers in states
		 */
		if (rs_num == PF_RULESET_RULE) {
			if (ruleset == &pf_main_ruleset)
				RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
					n->state->rule.ptr = NULL;
		} else
			RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
				n->state->nat_rule = NULL;
		old_rules = ruleset->rules[rs_num].active.ptr;
		ruleset->rules[rs_num].active.ptr =
		    ruleset->rules[rs_num].inactive.ptr;
		ruleset->rules[rs_num].inactive.ptr = old_rules;
		ruleset->rules[rs_num].active.ticket =
		    ruleset->rules[rs_num].inactive.ticket;
		pf_calc_skip_steps(ruleset->rules[rs_num].active.ptr);

		/* Purge the old rule list. */
		while ((rule = TAILQ_FIRST(old_rules)) != NULL)
			pf_rm_rule(old_rules, rule);
		pf_remove_if_empty_ruleset(ruleset);
		pf_update_anchor_rules();
		splx(s);
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*tail;
		int			 rs_num;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		s = splsoftnet();
		tail = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
		    pf_rulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ruleset->rules[rs_num].active.ticket;
		splx(s);
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule;
		int			 rs_num, i;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (pr->ticket != ruleset->rules[rs_num].active.ticket) {
			error = EBUSY;
			break;
		}
		s = splsoftnet();
		rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
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
		for (i = 0; i < PF_SKIP_COUNT; ++i)
			if (rule->skip[i].ptr == NULL)
				pr->rule.skip[i].nr = -1;
			else
				pr->rule.skip[i].nr =
				    rule->skip[i].ptr->nr;
		splx(s);
		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_rule	*pcr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*oldrule = NULL, *newrule = NULL;
		u_int32_t		 nr = 0;
		int			 rs_num;

		if (!(pcr->action == PF_CHANGE_REMOVE ||
		    pcr->action == PF_CHANGE_GET_TICKET) &&
		    pcr->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

		if (pcr->action < PF_CHANGE_ADD_HEAD ||
		    pcr->action > PF_CHANGE_GET_TICKET) {
			error = EINVAL;
			break;
		}
		ruleset = pf_find_ruleset(pcr->anchor, pcr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pcr->rule.action);

		if (pcr->action == PF_CHANGE_GET_TICKET) {
			pcr->ticket = ++ruleset->rules[rs_num].active.ticket;
			break;
		} else {
			if (pcr->ticket !=
			    ruleset->rules[rs_num].active.ticket) {
				error = EINVAL;
				break;
			}
		}

		if (pcr->action != PF_CHANGE_REMOVE) {
			newrule = pool_get(&pf_rule_pl, PR_NOWAIT);
			if (newrule == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcr->rule, newrule, sizeof(struct pf_rule));
			TAILQ_INIT(&newrule->rpool.list);
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
			pf_mv_pool(&pf_pabuf, &newrule->rpool.list);
			newrule->evaluations = newrule->packets = 0;
			newrule->bytes = 0;
		}
		pf_empty_pool(&pf_pabuf);

		s = splsoftnet();

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
			    pf_rulequeue);
		else {
			oldrule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
			while ((oldrule != NULL) && (oldrule->nr != pcr->nr))
				oldrule = TAILQ_NEXT(oldrule, entries);
			if (oldrule == NULL) {
				pf_rm_rule(NULL, newrule);
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE) {
			struct pf_tree_node	*n;

			RB_FOREACH(n, pf_state_tree, &tree_ext_gwy) {
				if (n->state->rule.ptr == oldrule)
					n->state->rule.ptr = NULL;
				if (n->state->nat_rule == oldrule)
					n->state->nat_rule = NULL;
			}
			pf_rm_rule(ruleset->rules[rs_num].active.ptr, oldrule);
		} else {
			if (oldrule == NULL)
				TAILQ_INSERT_TAIL(
				    ruleset->rules[rs_num].active.ptr,
				    newrule, entries);
			else if (pcr->action == PF_CHANGE_ADD_HEAD ||
			    pcr->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldrule, newrule, entries);
			else
				TAILQ_INSERT_AFTER(
				    ruleset->rules[rs_num].active.ptr,
				    oldrule, newrule, entries);
		}

		nr = 0;
		TAILQ_FOREACH(oldrule,
		    ruleset->rules[rs_num].active.ptr, entries)
			oldrule->nr = nr++;

		pf_calc_skip_steps(ruleset->rules[rs_num].active.ptr);
		pf_remove_if_empty_ruleset(ruleset);
		pf_update_anchor_rules();

		ruleset->rules[rs_num].active.ticket++;
		splx(s);
		break;
	}

	case DIOCCLRSTATES: {
		struct pf_tree_node	*n;

		s = splsoftnet();
		RB_FOREACH(n, pf_state_tree, &tree_ext_gwy)
			n->state->expire = 0;
		pf_purge_expired_states();
		pf_status.states = 0;
		splx(s);
		break;
	}

	case DIOCKILLSTATES: {
		struct pf_tree_node	*n;
		struct pf_state		*st;
		struct pfioc_state_kill	*psk = (struct pfioc_state_kill *)addr;
		int			 killed = 0;

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
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_state		*state;

		state = pool_get(&pf_state_pl, PR_NOWAIT);
		if (state == NULL) {
			error = ENOMEM;
			break;
		}
		s = splsoftnet();
		bcopy(&ps->state, state, sizeof(struct pf_state));
		state->rule.ptr = NULL;
		state->nat_rule = NULL;
		state->rt_ifp = NULL;
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
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_tree_node	*n;
		u_int32_t		 nr;
		int			 secs;

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
		struct pfioc_states	*ps = (struct pfioc_states *)addr;
		struct pf_tree_node	*n;
		struct pf_state		*p, pstore;
		u_int32_t		 nr = 0;
		int			 space = ps->ps_len;

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
			int	secs = time.tv_sec;

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
		struct pfioc_if	*pi = (struct pfioc_if *)addr;
		struct ifnet	*ifp;

		if (pi->ifname[0] == 0) {
			status_ifp = NULL;
			bzero(pf_status.ifname, IFNAMSIZ);
		} else
			if ((ifp = ifunit(pi->ifname)) == NULL)
				error = EINVAL;
			else {
				status_ifp = ifp;
				strlcpy(pf_status.ifname, ifp->if_xname,
				    IFNAMSIZ);
			}
		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;
		bcopy(&pf_status, s, sizeof(struct pf_status));
		break;
	}

	case DIOCCLRSTATUS: {
		u_int32_t	running = pf_status.running;
		u_int32_t	states = pf_status.states;
		u_int32_t	since = pf_status.since;
		u_int32_t	debug = pf_status.debug;

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
		struct pfioc_natlook	*pnl = (struct pfioc_natlook *)addr;
		struct pf_state		*st;
		struct pf_tree_node	 key;
		int			 direction = pnl->direction;

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
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;
		int		 old;

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
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX) {
			error = EINVAL;
			goto fail;
		}
		pt->seconds = *pftm_timeouts[pt->timeout];
		break;
	}

	case DIOCGETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			goto fail;
		}
		pl->limit = pf_pool_limits[pl->index].limit;
		break;
	}

	case DIOCSETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;
		int			 old_limit;

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
		u_int32_t	*level = (u_int32_t *)addr;

		pf_status.debug = *level;
		break;
	}

	case DIOCCLRRULECTRS: {
		struct pf_ruleset	*ruleset = &pf_main_ruleset;
		struct pf_rule		*rule;

		s = splsoftnet();
		TAILQ_FOREACH(rule,
		    ruleset->rules[PF_RULESET_RULE].active.ptr, entries)
			rule->evaluations = rule->packets =
			    rule->bytes = 0;
		splx(s);
		break;
	}

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq		*altq;
		struct ifnet		*ifp;
		struct tb_profile	 tb;

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
		struct pf_altq		*altq;
		struct ifnet		*ifp;
		struct tb_profile	 tb;
		int			 err;

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
		u_int32_t	*ticket = (u_int32_t *)addr;
		struct pf_altq	*altq;

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
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq, *a;

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
				if (strncmp(a->ifname, altq->ifname,
				    IFNAMSIZ) == 0 && a->qname[0] == 0) {
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
		u_int32_t		*ticket = (u_int32_t *)addr;
		struct pf_altqqueue	*old_altqs;
		struct pf_altq		*altq;
		int			 err;

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
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;

		pa->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = ticket_altqs_active;
		splx(s);
		break;
	}

	case DIOCGETALTQ: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;

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
		struct pfioc_qstats	*pq = (struct pfioc_qstats *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;
		int			 nbytes;

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
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

		pf_empty_pool(&pf_pabuf);
		pp->ticket = ++ticket_pabuf;
		break;
	}

	case DIOCADDADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

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
		if (pf_dynaddr_setup(&pa->addr.addr, pp->af)) {
			pf_dynaddr_remove(&pa->addr.addr);
			pool_put(&pf_pooladdr_pl, pa);
			error = EINVAL;
			break;
		}
		TAILQ_INSERT_TAIL(&pf_pabuf, pa, entries);
		break;
	}

	case DIOCGETADDRS: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

		pp->nr = 0;
		s = splsoftnet();
		pool = pf_get_pool(pp->anchor, pp->ruleset, pp->ticket,
		    pp->r_action, pp->r_num, 0, 1, 0);
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
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		u_int32_t		 nr = 0;

		s = splsoftnet();
		pool = pf_get_pool(pp->anchor, pp->ruleset, pp->ticket,
		    pp->r_action, pp->r_num, 0, 1, 1);
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
		pf_dynaddr_copyout(&pp->addr.addr.addr);
		splx(s);
		break;
	}

	case DIOCCHANGEADDR: {
		struct pfioc_pooladdr	*pca = (struct pfioc_pooladdr *)addr;
		struct pf_pooladdr	*oldpa = NULL, *newpa = NULL;

		if (pca->action < PF_CHANGE_ADD_HEAD ||
		    pca->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}

		pool = pf_get_pool(pca->anchor, pca->ruleset, 0,
		    pca->r_action, pca->r_num, pca->r_last, 1, 1);
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
			bcopy(&pca->addr, newpa, sizeof(struct pf_pooladdr));
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
			if (pf_dynaddr_setup(&newpa->addr.addr, pca->af)) {
				pf_dynaddr_remove(&newpa->addr.addr);
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
			int	i = 0;

			oldpa = TAILQ_FIRST(&pool->list);
			while ((oldpa != NULL) && (i < pca->nr)) {
				oldpa = TAILQ_NEXT(oldpa, entries);
				i++;
			}
			if (oldpa == NULL) {
				error = EINVAL;
				splx(s);
				break;
			}
		}

		if (pca->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(&pool->list, oldpa, entries);
			pf_dynaddr_remove(&oldpa->addr.addr);
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
		PF_ACPY(&pool->counter, &pool->cur->addr.addr.addr, pca->af);
		splx(s);
		break;
	}

	case DIOCGETANCHORS: {
		struct pfioc_anchor	*pa = (struct pfioc_anchor *)addr;
		struct pf_anchor	*anchor;

		pa->nr = 0;
		TAILQ_FOREACH(anchor, &pf_anchors, entries)
			pa->nr++;
		break;
	}

	case DIOCGETANCHOR: {
		struct pfioc_anchor	*pa = (struct pfioc_anchor *)addr;
		struct pf_anchor	*anchor;
		u_int32_t		 nr = 0;

		anchor = TAILQ_FIRST(&pf_anchors);
		while (anchor != NULL && nr < pa->nr) {
			anchor = TAILQ_NEXT(anchor, entries);
			nr++;
		}
		if (anchor == NULL)
			error = EBUSY;
		else
			bcopy(anchor->name, pa->name, sizeof(pa->name));
		break;
	}

	case DIOCGETRULESETS: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_anchor	*anchor;
		struct pf_ruleset	*ruleset;

		pr->anchor[PF_ANCHOR_NAME_SIZE-1] = 0;
		if ((anchor = pf_find_anchor(pr->anchor)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->nr = 0;
		TAILQ_FOREACH(ruleset, &anchor->rulesets, entries)
			pr->nr++;
		break;
	}

	case DIOCGETRULESET: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_anchor	*anchor;
		struct pf_ruleset	*ruleset;
		u_int32_t		 nr = 0;

		if ((anchor = pf_find_anchor(pr->anchor)) == NULL) {
			error = EINVAL;
			break;
		}
		ruleset = TAILQ_FIRST(&anchor->rulesets);
		while (ruleset != NULL && nr < pr->nr) {
			ruleset = TAILQ_NEXT(ruleset, entries);
			nr++;
		}
		if (ruleset == NULL)
			error = EBUSY;
		else
			bcopy(ruleset->name, pr->name, sizeof(pr->name));
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:

	return (error);
}
