/*	$OpenBSD: rde_spf.c,v 1.34 2005/11/04 10:46:23 claudio Exp $ */

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
RB_HEAD(rt_tree, rt_node)	 rt;
RB_PROTOTYPE(rt_tree, rt_node, entry, rt_compare)
RB_GENERATE(rt_tree, rt_node, entry, rt_compare)
struct vertex			*spf_root = NULL;

void		 spf_dump(struct area *);	/* XXX */
void		 cand_list_dump(void);		/* XXX */
void		 calc_next_hop(struct vertex *, struct vertex *);
void		 rt_update(struct in_addr, u_int8_t, struct in_addr, u_int32_t,
		     u_int32_t, struct in_addr, struct in_addr, enum path_type,
		     enum dst_type, u_int8_t, u_int8_t);
struct rt_node	*rt_lookup(enum dst_type, in_addr_t);
void		 rt_invalidate(void);
int		 linked(struct vertex *, struct vertex *);

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
spf_calc(struct area *area)
{
	struct lsa_tree		*tree = &area->lsa_tree;
	struct vertex		*v, *w;
	struct lsa_rtr_link	*rtr_link = NULL;
	struct lsa_net_link	*net_link;
	struct rt_node		*r;
	u_int32_t		 cost2, d;
	int			 i;
	struct in_addr		 addr, adv_rtr, a;
	enum dst_type		 type;

	log_debug("spf_calc: calculation started, area ID %s",
	    inet_ntoa(area->id));

	/* clear SPF tree */
	spf_tree_clr(area);
	cand_list_clr();

	/* initialize SPF tree */
	if ((v = spf_root = lsa_find(area, LSA_TYPE_ROUTER, rde_router_id(),
	    rde_router_id())) == NULL)
		/* empty area because no interface is active */
		return;

	area->transit = 0;
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
				a.s_addr = htonl(w->ls_id);
				log_debug("spf_calc: w id %s type %d has ",
				    inet_ntoa(a), w->type);
				a.s_addr = htonl(v->ls_id);
				log_debug("    no link to v id %s type %d",
				    inet_ntoa(a), v->type);
				continue;
			}

			if (v->type == LSA_TYPE_ROUTER)
				d = v->cost + ntohs(rtr_link->metric);
			else
				d = v->cost;

			if (cand_list_present(w)) {
				if (d > w->cost)
					continue;

				if (d < w->cost) {
					w->cost = d;
					w->prev = v;
					calc_next_hop(w, v);
				}
			} else if (w->cost == LS_INFINITY && d < LS_INFINITY) {
				w->cost = d;

				cand_list_add(w);
				w->prev = v;

				calc_next_hop(w, v);
			}
		}

		/* cand_list_dump(); */

		/* get next vertex */
		v = cand_list_pop();
		w = NULL;

	} while (v != NULL);

	/* calculate route table */
	RB_FOREACH(v, lsa_tree, tree) {
		lsa_age(v);
		if (ntohs(v->lsa->hdr.age) == MAX_AGE)
			continue;

		switch (v->type) {
		case LSA_TYPE_ROUTER:
			/* stub networks */
			if (v->cost >= LS_INFINITY || v->nexthop.s_addr == 0)
				continue;

			for (i = 0; i < lsa_num_links(v); i++) {
				rtr_link = get_rtr_link(v, i);
				if (rtr_link->type != LINK_TYPE_STUB_NET)
					continue;

				addr.s_addr = rtr_link->id;
				adv_rtr.s_addr = htonl(v->adv_rtr);

				rt_update(addr, mask2prefixlen(rtr_link->data),
				    v->nexthop, v->cost +
				    ntohs(rtr_link->metric), 0, area->id,
				    adv_rtr, PT_INTRA_AREA, DT_NET,
				    v->lsa->data.rtr.flags,
				    v->prev == spf_root);
			}

			/* router, only add border and as-external routers */
			if ((v->lsa->data.rtr.flags & (OSPF_RTR_B |
			    OSPF_RTR_E)) == 0)
				continue;

			addr.s_addr = htonl(v->ls_id);
			adv_rtr.s_addr = htonl(v->adv_rtr);

			rt_update(addr, 32, v->nexthop, v->cost, 0, area->id,
			    adv_rtr, PT_INTRA_AREA, DT_RTR,
			    v->lsa->data.rtr.flags, 0);
			break;
		case LSA_TYPE_NETWORK:
			if (v->cost >= LS_INFINITY || v->nexthop.s_addr == 0)
				continue;

			addr.s_addr = htonl(v->ls_id) & v->lsa->data.net.mask;
			adv_rtr.s_addr = htonl(v->adv_rtr);
			rt_update(addr, mask2prefixlen(v->lsa->data.net.mask),
			    v->nexthop, v->cost, 0, area->id, adv_rtr,
			    PT_INTRA_AREA, DT_NET, 0, v->prev == spf_root);
			break;
		case LSA_TYPE_SUM_NETWORK:
		case LSA_TYPE_SUM_ROUTER:
			/* if ABR only look at area 0.0.0.0 LSA */

			/* ignore self-originated stuff */
			if (v->nbr->self)
				continue;

			/* TODO type 3 area address range check */

			if ((w = lsa_find(area, LSA_TYPE_ROUTER,
			    htonl(v->adv_rtr),
			    htonl(v->adv_rtr))) == NULL)
				continue;

			v->nexthop = w->nexthop;
			v->cost = w->cost +
			    (ntohl(v->lsa->data.sum.metric) & LSA_METRIC_MASK);

			if (v->cost >= LS_INFINITY || v->nexthop.s_addr == 0)
				continue;

			adv_rtr.s_addr = htonl(v->adv_rtr);
			if (v->type == LSA_TYPE_SUM_NETWORK) {
				addr.s_addr = htonl(v->ls_id) &
				    v->lsa->data.sum.mask;
				rt_update(addr,
				    mask2prefixlen(v->lsa->data.sum.mask),
				    v->nexthop, v->cost, 0, area->id, adv_rtr,
				    PT_INTER_AREA, DT_NET, 0, 0);
			} else {
				addr.s_addr = htonl(v->ls_id);
				rt_update(addr, 32, v->nexthop, v->cost, 0,
				    area->id, adv_rtr, PT_INTER_AREA, DT_RTR,
				    v->lsa->data.rtr.flags, 0);
			}

			break;
		default:
			/* as-external LSA are stored in a different tree */
			fatalx("spf_calc: invalid LSA type");
		}
	}

	/* calculate as-external routes */
	RB_FOREACH(v, lsa_tree, &rdeconf->lsa_tree) {
		lsa_age(v);
		if (ntohs(v->lsa->hdr.age) == MAX_AGE)
			continue;

		switch (v->type) {
		case LSA_TYPE_EXTERNAL:
			/* ignore self-originated stuff */
			if (v->nbr->self)
				continue;

			if ((r = rt_lookup(DT_RTR, htonl(v->adv_rtr))) == NULL)
				continue;

			if (v->lsa->data.asext.fw_addr != 0 &&
			    (r = rt_lookup(DT_NET,
			    v->lsa->data.asext.fw_addr)) == NULL)
				continue;

#if 0
			if (r->p_type != PT_INTRA_AREA &&
			    r->p_type != PT_INTER_AREA)
				continue;
#endif

			/* XXX RFC1583Compatibility */
			if (r->connected) {
				if (v->lsa->data.asext.fw_addr != 0)
					v->nexthop.s_addr =
					    v->lsa->data.asext.fw_addr;
				else
					v->nexthop.s_addr = htonl(v->adv_rtr);
			} else
				v->nexthop = r->nexthop;

			if (ntohl(v->lsa->data.asext.metric) &
			    LSA_ASEXT_E_FLAG) {
				v->cost = r->cost;
				cost2 = ntohl(v->lsa->data.asext.metric) &
				     LSA_METRIC_MASK;
				type = PT_TYPE2_EXT;
			} else {
				v->cost = r->cost +
				    (ntohl(v->lsa->data.asext.metric) &
				     LSA_METRIC_MASK);
				cost2 = 0;
				type = PT_TYPE1_EXT;
			}

			a.s_addr = 0;
			adv_rtr.s_addr = htonl(v->adv_rtr);
			addr.s_addr = htonl(v->ls_id) & v->lsa->data.asext.mask;
			rt_update(addr, mask2prefixlen(v->lsa->data.asext.mask),
			    v->nexthop, v->cost, cost2, a, adv_rtr, type,
			    DT_NET, 0, 0);
			break;
		default:
			fatalx("spf_calc: invalid LSA type");
		}
	}

	/* spf_dump(area); */
	log_debug("spf_calc: calculation ended, area ID %s",
	    inet_ntoa(area->id));

	area->num_spf_calc++;
	start_spf_timer();

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
			for (i = 0; i < lsa_num_links(parent); i++) {
				rtr_link = get_rtr_link(parent, i);
				switch (rtr_link->type) {
				case LINK_TYPE_POINTTOPOINT:
					/* ignore */
					break;
				case LINK_TYPE_TRANSIT_NET:
					if ((htonl(dst->ls_id) &
					    dst->lsa->data.net.mask) ==
					    (rtr_link->data &
					     dst->lsa->data.net.mask)) {
						dst->nexthop.s_addr =
						    rtr_link->data;
					}
					break;
				case LINK_TYPE_STUB_NET:
					break;

				default:
					fatalx("calc_next_hop: invalid link "
					    "type");
				}
			}
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

	TAILQ_FOREACH(c, &cand_list, cand) {
		if (c->cost > v->cost) {
			TAILQ_INSERT_BEFORE(c, v, cand);
			return;
		} else if (c->cost == v->cost && c->type == LSA_TYPE_ROUTER &&
		    v->type == LSA_TYPE_NETWORK) {
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

int
cand_list_present(struct vertex *v)
{
	struct vertex	*c;

	TAILQ_FOREACH(c, &cand_list, cand) {
		if (c == v)
			return (1);
	}

	return (0);
}

void
cand_list_clr(void)
{
	struct vertex *c;

	while ((c = TAILQ_FIRST(&cand_list)) != NULL) {
		TAILQ_REMOVE(&cand_list, c, cand);
	}
}

int
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
	struct rt_node		*r;

	switch (conf->spf_state) {
	case SPF_IDLE:
		fatalx("spf_timer: invalid state IDLE");
	case SPF_HOLDQUEUE:
		log_debug("spf_timer: HOLDQUEUE -> DELAY");
		conf->spf_state = SPF_DELAY;
		/* FALLTHROUGH */
	case SPF_DELAY:
		rt_invalidate();

		LIST_FOREACH(area, &conf->area_list, entry)
			spf_calc(area);

		RB_FOREACH(r, rt_tree, &rt) {
			LIST_FOREACH(area, &conf->area_list, entry)
				rde_summary_update(r, area);

			if (r->d_type != DT_NET)
				continue;

			if (r->invalid)
				rde_send_delete_kroute(r);
			else
				rde_send_change_kroute(r);
		}

		LIST_FOREACH(area, &conf->area_list, entry)
			lsa_remove_invalid_sums(area);

		start_spf_holdtimer(conf);
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
start_spf_timer(void)
{
	struct timeval	tv;

	switch (rdeconf->spf_state) {
	case SPF_IDLE:
		log_debug("start_spf_timer: IDLE -> DELAY");
		timerclear(&tv);
		tv.tv_sec = rdeconf->spf_delay;
		rdeconf->spf_state = SPF_DELAY;
		return (evtimer_add(&rdeconf->ev, &tv));
	case SPF_DELAY:
		/* ignore */
		break;
	case SPF_HOLD:
		log_debug("start_spf_timer: HOLD -> HOLDQUEUE");
		rdeconf->spf_state = SPF_HOLDQUEUE;
		break;
	case SPF_HOLDQUEUE:
		/* ignore */
		break;
	default:
		fatalx("start_spf_timer: invalid spf_state");
	}

	return (1);
}

int
stop_spf_timer(struct ospfd_conf *conf)
{
	return (evtimer_del(&conf->ev));
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
		return (evtimer_add(&conf->ev, &tv));
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
	if (a->d_type > b->d_type)
		return (-1);
	if (a->d_type < b->d_type)
		return (1);
	return (0);
}

struct rt_node *
rt_find(in_addr_t prefix, u_int8_t prefixlen, enum dst_type d_type)
{
	struct rt_node	s;

	s.prefix.s_addr = prefix;
	s.prefixlen = prefixlen;
	s.d_type = d_type;

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
rt_invalidate(void)
{
	struct rt_node	*r, *nr;

	for (r = RB_MIN(rt_tree, &rt); r != NULL; r = nr) {
		nr = RB_NEXT(rt_tree, &rt, r);
		if (r->invalid)
			rt_remove(r);
		else
			r->invalid = 1;
	}
}

void
rt_clear(void)
{
	struct rt_node	*r;

	while ((r = RB_MIN(rt_tree, &rt)) != NULL)
		rt_remove(r);
}

void
rt_dump(struct in_addr area, pid_t pid, u_int8_t r_type)
{
	static struct ctl_rt	 rtctl;
	struct rt_node		*r;

	RB_FOREACH(r, rt_tree, &rt) {
		if (r->invalid)
			continue;

		if (r->area.s_addr != area.s_addr)
			continue;

		switch (r_type) {
		case RIB_RTR:
			if (r->d_type != DT_RTR)
				continue;
			break;
		case RIB_NET:
			if (r->d_type != DT_NET)
				continue;
			if (r->p_type == PT_TYPE1_EXT ||
			    r->p_type == PT_TYPE2_EXT)
				continue;
			break;
		case RIB_EXT:
			if (r->p_type != PT_TYPE1_EXT &&
			    r->p_type != PT_TYPE2_EXT)
				continue;
			break;
		default:
			fatalx("rt_dump: invalid RIB type");
		}

		rtctl.prefix.s_addr = r->prefix.s_addr;
		rtctl.nexthop.s_addr = r->nexthop.s_addr;
		rtctl.area.s_addr = r->area.s_addr;
		rtctl.adv_rtr.s_addr = r->adv_rtr.s_addr;
		rtctl.cost = r->cost;
		rtctl.cost2 = r->cost2;
		rtctl.p_type = r->p_type;
		rtctl.d_type = r->d_type;
		rtctl.flags = r->flags;
		rtctl.prefixlen = r->prefixlen;

		rde_imsg_compose_ospfe(IMSG_CTL_SHOW_RIB, 0, pid, &rtctl,
		    sizeof(rtctl));
	}
}

void
rt_update(struct in_addr prefix, u_int8_t prefixlen, struct in_addr nexthop,
     u_int32_t cost, u_int32_t cost2, struct in_addr area,
     struct in_addr adv_rtr, enum path_type p_type, enum dst_type d_type,
     u_int8_t flags, u_int8_t connected)
{
	struct rt_node	*rte;
	int		 better = 0;

	if (nexthop.s_addr == 0)	/* XXX remove */
		fatalx("rt_update: invalid nexthop");

	if ((rte = rt_find(prefix.s_addr, prefixlen, d_type)) == NULL) {
		if ((rte = calloc(1, sizeof(struct rt_node))) == NULL)
			fatalx("rt_update");
		rte->prefix.s_addr = prefix.s_addr;
		rte->prefixlen = prefixlen;
		rte->nexthop.s_addr = nexthop.s_addr;
		rte->adv_rtr.s_addr = adv_rtr.s_addr;
		rte->cost = cost;
		rte->cost2 = cost2;
		rte->area = area;
		rte->p_type = p_type;
		rte->d_type = d_type;
		rte->flags = flags;
		rte->invalid = 0;
		rte->connected = connected;

		rt_insert(rte);
	} else {
		if (rte->invalid) {
			/* invalidated entry - just update */
			rte->nexthop.s_addr = nexthop.s_addr;
			rte->adv_rtr.s_addr = adv_rtr.s_addr;
			rte->cost = cost;
			rte->cost2 = cost2;
			rte->area = area;
			rte->p_type = p_type;
			rte->flags = flags;
			rte->invalid = 0;
			rte->connected = connected;
		} else {
			/* order:
			 * 1. intra-area
			 * 2. inter-area
			 * 3. type 1 as ext
			 * 4. type 2 as ext
			 */
			if (p_type < rte->p_type)
				better = 1;
			else if (p_type == rte->p_type)
				switch (p_type) {
				case PT_INTRA_AREA:
				case PT_INTER_AREA:
					if (cost < rte->cost)
						better = 1;
					/* ignore equal pathes */
					break;
				case PT_TYPE1_EXT:
					/* XXX rfc1583 compat */
					if (cost < rte->cost)
						better = 1;
					break;
				case PT_TYPE2_EXT:
					if (cost2 < rte->cost2)
						better = 1;
					/* XXX rfc1583 compat */
					else if (cost2 == rte->cost2 &&
					    cost < rte->cost)
						better = 1;
					break;
				}
			if (better) {
				rte->nexthop.s_addr = nexthop.s_addr;
				rte->adv_rtr.s_addr = adv_rtr.s_addr;
				rte->cost = cost;
				rte->cost2 = cost2;
				rte->area = area;
				rte->p_type = p_type;
				rte->flags = flags;
				rte->invalid = 0;
				rte->connected = connected;
			}
		}
	}
}

struct rt_node *
rt_lookup(enum dst_type type, in_addr_t addr)
{
	struct rt_node	*rn;
	u_int8_t	 i = 32;

	if (type == DT_RTR) {
		rn = rt_find(addr, 32, type);
		if (rn && rn->invalid == 0)
			return (rn);
		return (NULL);
	}

	/* type == DT_NET */
	do {
		if ((rn = rt_find(addr & prefixlen2mask(i), i, type)) &&
		    rn->invalid == 0)
			return (rn);
	} while (i-- != 0);

	return (NULL);
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

	/* nlinks validated earlier by lsa_check() */
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

	/* nlinks validated earlier by lsa_check() */
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
int
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
					return (1);
				break;
			case LSA_TYPE_NETWORK:
				if (rtr_link->id == htonl(v->ls_id))
					return (1);
				break;
			default:
				fatalx("spf_calc: invalid type");
			}
		}
		return (0);
	case LSA_TYPE_NETWORK:
		for (i = 0; i < lsa_num_links(w); i++) {
			net_link = get_net_link(w, i);
			switch (v->type) {
			case LSA_TYPE_ROUTER:
				if (net_link->att_rtr == htonl(v->ls_id))
					return (1);
				break;
			default:
				fatalx("spf_calc: invalid type");
			}
		}
		return (0);
	default:
		fatalx("spf_calc: invalid LSA type");
	}

	return (0);
}
