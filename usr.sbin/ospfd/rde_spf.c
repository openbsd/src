/*	$OpenBSD: rde_spf.c,v 1.2 2005/03/02 16:17:04 norby Exp $ */

/*
 * Copyright (c) 2005 Esben Norby <norby@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdlib.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "rde.h"

extern struct ospfd_conf	*rdeconf;
TAILQ_HEAD(, vertex)		 cand_list;
RB_HEAD(rt_tree, rt_node)	rt_tree, rt;
RB_PROTOTYPE(rt_tree, rt_node, entry, rt_compare)
RB_GENERATE(rt_tree, rt_node, entry, rt_compare)
struct vertex			*spf_root = NULL;

void	 spf_dump(struct area *);	/* XXX */
void	 rt_dump(void);			/* XXX */
void	 cand_list_dump(void);		/* XXX */
void	 calc_next_hop(struct vertex *, struct vertex *);
void	 rt_update(struct in_addr, u_int8_t, struct in_addr, u_int32_t, u_int8_t);
bool	 linked(struct vertex *, struct vertex *);
u_int8_t	mask2prefixlen(in_addr_t);

void
spf_dump(struct area *area)
{
	struct lsa_tree	*tree = &area->lsa_tree;
	struct vertex	*v;
	struct vertex	*p;	/* parent */
	struct in_addr	 addr;

	log_debug("spf_dump:");
	RB_FOREACH(v, lsa_tree, tree) {
		addr.s_addr = htonl(v->ls_id);
		log_debug("    id %s type %d cost %d", inet_ntoa(addr),
		     v->type, v->cost);
		log_debug("    nexthop: %s", inet_ntoa(v->nexthop));

#if 1
		log_debug("    --------------------------------------------");
		p = v->prev;
		while (p != NULL) {
			addr.s_addr = htonl(p->ls_id);
			log_debug("        id %15s type %d cost %d",
			    inet_ntoa(addr), p->type, p->cost);
			p = p->prev;
		}
	log_debug("");
#endif
	}

	return;
}

void
rt_dump(void)
{
	struct rt_node	*r;
	int		 i = 0;

	log_debug("rt_dump:");

	RB_FOREACH(r, rt_tree, &rt) {
		log_debug("net: %s/%d", inet_ntoa(r->prefix), r->prefixlen);
		log_debug("   nexthop: %s   cost: %d   type: %s",
		    inet_ntoa(r->nexthop), r->cost, path_type_names[r->p_type]);
		i++;

		rde_send_kroute(r);
	}

	log_debug("count: %d", i);
}

void
spf_calc(struct area *area)
{
	struct lsa_tree	*tree = &area->lsa_tree;
	struct vertex		*v, *w;
	struct lsa_rtr_link	*rtr_link = NULL;
	struct lsa_net_link	*net_link = NULL;
	u_int32_t		 d;
	int			 i;
	struct in_addr		 addr;

	log_debug("spf_calc: calculation started, area ID %s",
	    inet_ntoa(area->id));

	/* clear SPF tree */
	spf_tree_clr(area);
	cand_list_clr();

	/* initialize SPF tree */
	if ((v = spf_root = lsa_find(area, LSA_TYPE_ROUTER, rde_router_id(),
	    rde_router_id())) == NULL)
		fatalx("spf_calc: cannot find self originated router LSA");
	area->transit = false;
	spf_root->cost = 0;
	w = NULL;

	/* calculate SPF tree */
	do {
		/* loop links */
		for (i = 0; i < lsa_num_links(v); i++) {
			switch (v->type) {
			case LSA_TYPE_ROUTER:
				rtr_link = get_rtr_link(v, i);
				switch (rtr_link->type) {
				case LINK_TYPE_STUB_NET:
					/* skip */
					continue;
				case LINK_TYPE_POINTTOPOINT:
				case LINK_TYPE_VIRTUAL:
					/* find router LSA */
					w = lsa_find(area, LSA_TYPE_ROUTER,
					    rtr_link->id, rtr_link->id);
					break;
				case LINK_TYPE_TRANSIT_NET:
					/* find network LSA */
					w = lsa_find_net(area, rtr_link->id);
					break;
				default:
					fatalx("spf_calc: invalid link type");
				}
				break;
			case LSA_TYPE_NETWORK:
				net_link = get_net_link(v, i);
				/* find router LSA */
				w = lsa_find(area, LSA_TYPE_ROUTER,
				    net_link->att_rtr, net_link->att_rtr);
				break;
			default:
				fatalx("spf_calc: invalid LSA type");
			}

			if (w == NULL) {
				log_debug("spf_calc: w = NULL");
				continue;
			}

			if (w->lsa->hdr.age == MAX_AGE) {
				log_debug("spf_calc: age = MAX_AGE");
				continue;
			}

			if (!linked(w, v)) {
				log_debug("spf_calc: w has no link to v");
				continue;
			}
#if 0
			if ((w->cost != LS_INFINITY) && (v->prev != NULL)) {
				log_debug("spf_calc: w already in SPF tree");
				continue;
			}
#endif
			if (v->type == LSA_TYPE_ROUTER)
				d = v->cost + ntohs(rtr_link->metric);
			else
				d = v->cost;

			if (cand_list_present(w)) {
				if (d > w->cost)
					continue;

				if (d == w->cost) {
					/* calc next hop */
					calc_next_hop(w, v);
				}

				if (d < w->cost) {
					/* calc next hop */
					calc_next_hop(w, v);

					w->cost = d;
					w->prev = v;
				}
			} else {
				if (w->cost == LS_INFINITY) {
					w->cost = 0;
					w->cost += d;

					cand_list_add(w);
					w->prev = v;

					/* calc next hop */
					calc_next_hop(w, v);
				}
			}
		}

		cand_list_dump();

		/* get next vertex */
		v = cand_list_pop();
		w = NULL;

	} while (v != NULL);

	/* calculate route table */
	RB_FOREACH(v, lsa_tree, tree) {
		if (ntohs(v->lsa->hdr.age) == MAX_AGE)
			continue;

		switch (v->type) {
		case LSA_TYPE_ROUTER:
			/* stub networks */
			if ((v->cost == LS_INFINITY) ||
			    (v->nexthop.s_addr == 0))
				continue;

			for (i = 0; i < lsa_num_links(v); i++) {
				rtr_link = get_rtr_link(v, i);
				if (rtr_link->type != LINK_TYPE_STUB_NET)
					continue;

				addr.s_addr = rtr_link->id;

				rt_update(addr, mask2prefixlen(rtr_link->data),
				    v->nexthop, v->cost +
				    ntohs(rtr_link->metric), PT_INTRA_AREA);
			}
			break;
		case LSA_TYPE_NETWORK:
			if ((v->cost == LS_INFINITY) ||
			    (v->nexthop.s_addr == 0))
				continue;

			addr.s_addr = htonl(v->ls_id) & v->lsa->data.net.mask;
			rt_update(addr, mask2prefixlen(v->lsa->data.net.mask),
			    v->nexthop, v->cost, PT_INTRA_AREA);
			break;
		case LSA_TYPE_SUM_NETWORK:
			if (rdeconf->flags & OSPF_RTR_B)
				continue;

			if ((w = lsa_find(area, LSA_TYPE_ROUTER,
			    htonl(v->adv_rtr),
			    htonl(v->adv_rtr))) == NULL)
				continue;

			v->nexthop = w->nexthop;
			v->cost = w->cost +
			    ntohl(v->lsa->data.sum.metric);

			if (v->nexthop.s_addr == 0)
				continue;

			addr.s_addr = htonl(v->ls_id) & v->lsa->data.sum.mask;
			rt_update(addr, mask2prefixlen(v->lsa->data.sum.mask),
			    v->nexthop, v->cost, PT_INTER_AREA);
			break;
		case LSA_TYPE_SUM_ROUTER:
			break;
		case LSA_TYPE_EXTERNAL:
			break;
		default:
			fatalx("spf_calc: invalid LSA type");
		}
	}

	spf_dump(area);
	rt_dump();
	log_debug("spf_calc: calculation ended, area ID %s",
	    inet_ntoa(area->id));

	start_spf_timer(rdeconf);

	return;
}

void
spf_tree_clr(struct area *area)
{
	struct lsa_tree	*tree = &area->lsa_tree;
	struct vertex	*v;

	RB_FOREACH(v, lsa_tree, tree) {
		v->cost = LS_INFINITY;
		v->prev = NULL;
		v->nexthop.s_addr = 0;
	}
}

void
calc_next_hop(struct vertex *dst, struct vertex *parent)
{
	struct lsa_rtr_link	*rtr_link = NULL;
	int			 i;

	/* case 1 */
	if (parent == spf_root) {
		switch (dst->type) {
		case LSA_TYPE_ROUTER:
			for (i = 0; i < lsa_num_links(dst); i++) {
				rtr_link = get_rtr_link(dst, i);
				if (rtr_link->type != LINK_TYPE_POINTTOPOINT &&
				    rtr_link->id != parent->ls_id)
					continue;
				dst->nexthop.s_addr = rtr_link->data;
			}			
			return;
		case LSA_TYPE_NETWORK:
			/* XXX TODO */
			return;
		default:
			fatalx("calc_next_hop: invalid dst type");
		}
	}

	/* case 2 */
	if (parent->type == LSA_TYPE_NETWORK && dst->type == LSA_TYPE_ROUTER &&
	    dst->prev == parent && parent->prev == spf_root) {
		for (i = 0; i < lsa_num_links(dst); i++) {
			rtr_link = get_rtr_link(dst, i);
			if ((rtr_link->type == LINK_TYPE_TRANSIT_NET) &&
			    (rtr_link->data & parent->lsa->data.net.mask) ==
			    (htonl(parent->ls_id) & parent->lsa->data.net.mask))
				dst->nexthop.s_addr = rtr_link->data;
		}

		return;
	}
	/* case 3 */
	dst->nexthop = parent->nexthop;

	return;
}

/* candidate list */
void
cand_list_init(void)
{
	TAILQ_INIT(&cand_list);
}

void
cand_list_add(struct vertex *v)
{
	struct vertex	*c = NULL;

	/* XXX TODO: network vertex takes precedence over router vertex */
	TAILQ_FOREACH(c, &cand_list, cand) {
		if (c->cost > v->cost) {
			TAILQ_INSERT_BEFORE(c, v, cand);
			return;
		}
	}
	TAILQ_INSERT_TAIL(&cand_list, v, cand);

	return;
}

void
cand_list_dump(void)
{
	struct vertex	*c = NULL;
	struct in_addr	 addr;

	log_debug("cand_list_dump:");
	TAILQ_FOREACH(c, &cand_list, cand) {
		addr.s_addr = htonl(c->ls_id);
		log_debug("    id %s type %d cost %d", inet_ntoa(addr),
		     c->type, c->cost);
	}
	log_debug("");
}

struct vertex *
cand_list_pop(void)
{
	struct vertex	*c;

	if ((c = TAILQ_FIRST(&cand_list)) != NULL) {
		TAILQ_REMOVE(&cand_list, c, cand);
	}

	return (c);
}

bool
cand_list_present(struct vertex *v)
{
	struct vertex	*c;

	TAILQ_FOREACH(c, &cand_list, cand) {
		if (c == v)
			return (true);
	}

	return (false);
}

void
cand_list_clr(void)
{
	struct vertex *c;

	while ((c = TAILQ_FIRST(&cand_list)) != NULL) {
		TAILQ_REMOVE(&cand_list, c, cand);
	}
}

bool
cand_list_empty(void)
{
	return (TAILQ_EMPTY(&cand_list));
}

/* timers */
void
spf_timer(int fd, short event, void *arg)
{
	struct ospfd_conf	*conf = arg;
	struct area		*area;

	log_debug("spf_timer:");

	switch (conf->spf_state) {
	case SPF_IDLE:
		fatalx("spf_timer: invalid state IDLE");
	case SPF_HOLDQUEUE:
		log_debug("spf_timer: HOLDQUEUE -> DELAY");
		conf->spf_state = SPF_DELAY;
		/* FALLTHROUGH */
	case SPF_DELAY:
		rt_clear();	/* XXX we should save to old rt! */
		LIST_FOREACH(area, &conf->area_list, entry) {
			spf_calc(area);
		}
		start_spf_holdtimer(rdeconf);
		break;
	case SPF_HOLD:
		log_debug("spf_timer: state HOLD -> IDLE");
		conf->spf_state = SPF_IDLE;
		break;
	default:
		fatalx("spf_timer: unknown state");
	}
}

int
start_spf_timer(struct ospfd_conf *conf)
{
	struct timeval	tv;

	log_debug("start_spf_timer:");

	switch (conf->spf_state) {
	case SPF_IDLE:
		log_debug("start_spf_timer: IDLE -> DELAY");
		timerclear(&tv);
		tv.tv_sec = conf->spf_delay;
		conf->spf_state = SPF_DELAY;
		return (evtimer_add(&conf->spf_timer, &tv));
	case SPF_DELAY:
		/* ignore */
		break;
	case SPF_HOLD:
		log_debug("start_spf_timer: HOLD -> HOLDQUEUE");
		conf->spf_state = SPF_HOLDQUEUE;
		break;
	case SPF_HOLDQUEUE:
		break;
	default:
		fatalx("start_spf_timer: invalid spf_state");
	}

	return (1);
}

int
stop_spf_timer(struct ospfd_conf *conf)
{
	return (evtimer_del(&conf->spf_timer));
}

int
start_spf_holdtimer(struct ospfd_conf *conf)
{
	struct timeval	tv;

	switch (conf->spf_state) {
	case SPF_DELAY:
		timerclear(&tv);
		tv.tv_sec = conf->spf_hold_time;
		conf->spf_state = SPF_HOLD;
		log_debug("spf_start_holdtimer: DELAY -> HOLD");
		return (evtimer_add(&conf->spf_timer, &tv));
	case SPF_IDLE:
	case SPF_HOLD:
	case SPF_HOLDQUEUE:
		fatalx("start_spf_holdtimer: invalid state");
	default:
		fatalx("spf_start_holdtimer: unknown state");
	}

	return (1);
}

/* route table */
void
rt_init(void)
{
	RB_INIT(&rt);
}

int
rt_compare(struct rt_node *a, struct rt_node *b)
{
	if (ntohl(a->prefix.s_addr) < ntohl(b->prefix.s_addr))
		return (-1);
	if (ntohl(a->prefix.s_addr) > ntohl(b->prefix.s_addr))
		return (1);
	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);
	return (0);
}

struct rt_node *
rt_find(in_addr_t prefix, u_int8_t prefixlen)
{
	struct rt_node	s;

	s.prefix.s_addr = prefix;
	s.prefixlen = prefixlen;

	return (RB_FIND(rt_tree, &rt, &s));
}

int
rt_insert(struct rt_node *r)
{
	if (RB_INSERT(rt_tree, &rt, r) != NULL) {
		log_warnx("rt_insert failed for %s/%u",
		    inet_ntoa(r->prefix), r->prefixlen);
		free(r);
		return (-1);
	}

	return (0);
}

int
rt_remove(struct rt_node *r)
{
	if (RB_REMOVE(rt_tree, &rt, r) == NULL) {
		log_warnx("rt_remove failed for %s/%u",
		    inet_ntoa(r->prefix), r->prefixlen);
		return (-1);
	}

	free(r);
	return (0);
}

void
rt_clear(void)
{
	struct rt_node	*r;

	while ((r = RB_MIN(rt_tree, &rt)) != NULL)
		rt_remove(r);
}

void
rt_update(struct in_addr prefix, u_int8_t prefixlen, struct in_addr nexthop,
     u_int32_t cost, u_int8_t p_type)
{
	struct rt_node	*rte;

	if ((rte = rt_find(prefix.s_addr, prefixlen)) == NULL) {
		log_debug("rt_update: creating %s/%d", inet_ntoa(prefix),
		    prefixlen);
		if ((rte = calloc(1, sizeof(struct rt_node))) == NULL)
			fatalx("rt_update");
		rte->prefix.s_addr = prefix.s_addr;
		rte->prefixlen = prefixlen;
		rte->nexthop.s_addr = nexthop.s_addr;
		rte->cost = cost;
		rte->p_type = p_type;

		rt_insert(rte);
	} else {
		log_debug("rt_update: updating %s/%d", inet_ntoa(prefix),
		    prefixlen);

		/* XXX better route ? */
		/* consider intra vs. inter */
		if (cost < rte->cost) {
			rte->nexthop.s_addr = nexthop.s_addr;
			rte->cost = cost;
			rte->p_type = p_type;
		}
	}
}

/* router LSA links */
struct lsa_rtr_link *
get_rtr_link(struct vertex *v, int idx)
{
	struct lsa_rtr_link	*rtr_link = NULL;
	char			*buf = (char *)v->lsa;
	u_int16_t		 i, off, nlinks;

	if (v->type != LSA_TYPE_ROUTER)
		fatalx("get_rtr_link: invalid LSA type");

	off = sizeof(v->lsa->hdr) + sizeof(struct lsa_rtr);

	nlinks = lsa_num_links(v);
	for (i = 0; i < nlinks; i++) {
		rtr_link = (struct lsa_rtr_link *)(buf + off);
		if (i == idx)
			return (rtr_link);

		off += sizeof(struct lsa_rtr_link) +
		    rtr_link->num_tos * sizeof(u_int32_t);
	}

	return (NULL);
}

/* network LSA links */
struct lsa_net_link *
get_net_link(struct vertex *v, int idx)
{
	struct lsa_net_link	*net_link = NULL;
	char			*buf = (char *)v->lsa;
	u_int16_t		 i, off, nlinks;

	if (v->type != LSA_TYPE_NETWORK)
		fatalx("get_net_link: invalid LSA type");

	off = sizeof(v->lsa->hdr) + sizeof(u_int32_t);

	nlinks = lsa_num_links(v);
	for (i = 0; i < nlinks; i++) {
		net_link = (struct lsa_net_link *)(buf + off);
		if (i == idx)
			return (net_link);

		off += sizeof(struct lsa_net_link);
	}

	return (NULL);
}

/* misc */
bool
linked(struct vertex *w, struct vertex *v)
{
	struct lsa_rtr_link	*rtr_link = NULL;
	struct lsa_net_link	*net_link = NULL;
	int			 i;

	switch (w->type) {
	case LSA_TYPE_ROUTER:
		for (i = 0; i < lsa_num_links(w); i++) {
			rtr_link = get_rtr_link(w, i);
			switch (v->type) {
			case LSA_TYPE_ROUTER:
				if (rtr_link->type == LINK_TYPE_POINTTOPOINT &&
				    rtr_link->id == htonl(v->ls_id))
					return (true);
				break;
			case LSA_TYPE_NETWORK:
				if (rtr_link->id == htonl(v->ls_id))
					return (true);
				break;
			default:
				fatalx("spf_calc: invalid type");
			}
		}
		return (false);
	case LSA_TYPE_NETWORK:
		for (i = 0; i < lsa_num_links(w); i++) {
			net_link = get_net_link(w, i);
			switch (v->type) {
			case LSA_TYPE_ROUTER:
				if (net_link->att_rtr == htonl(v->ls_id))
					return (true);
				break;
			default:
				fatalx("spf_calc: invalid type");
			}
		}
		return (false);
	default:
		fatalx("spf_calc: invalid LSA type");
	}

	return (false);
}
