/*	$OpenBSD: rde_srt.c,v 1.3 2007/01/29 13:38:59 michele Exp $ */

/*
 * Copyright (c) 2005, 2006 Esben Norby <norby@openbsd.org>
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
#include <sys/tree.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdlib.h>

#include "igmp.h"
#include "dvmrp.h"
#include "dvmrpd.h"
#include "log.h"
#include "dvmrpe.h"
#include "rde.h"

/* source route tree */

void	 rt_invalidate(void);
void	 rt_expire_timer(int, short, void *);
int	 rt_start_expire_timer(struct rt_node *);

RB_HEAD(rt_tree, rt_node)	 rt;
RB_PROTOTYPE(rt_tree, rt_node, entry, rt_compare)
RB_GENERATE(rt_tree, rt_node, entry, rt_compare)

extern struct dvmrpd_conf	*rdeconf;

/* timers */
void
rt_expire_timer(int fd, short event, void *arg)
{
	struct rt_node	*rn = arg;

	log_debug("rt_expire_timer: route %s/%d", inet_ntoa(rn->prefix),
	    rn->prefixlen);

	/* XXX tell neighbors */

	/* remove route entry */
	event_del(&rn->expiration_timer);
	rt_remove(rn);
}

int
rt_start_expire_timer(struct rt_node *rn)
{
	struct timeval	tv;

	timerclear(&tv);
	tv.tv_sec = ROUTE_EXPIRATION_TIME;
	return (evtimer_add(&rn->expiration_timer, &tv));

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
	/*
	 * sort route entries based on prefixlen since generating route
	 * reports rely on that.
	 */
	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);
	if (ntohl(a->prefix.s_addr) < ntohl(b->prefix.s_addr))
		return (-1);
	if (ntohl(a->prefix.s_addr) > ntohl(b->prefix.s_addr))
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

struct rt_node *
rr_new_rt(struct route_report *rr, int adj_metric, int connected)
{
	struct timespec	 now;
	struct rt_node  *rn;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if ((rn = calloc(1, sizeof(*rn))) == NULL)
		fatal("rr_new_rt");

	rn->prefix.s_addr = rr->net.s_addr;
	rn->prefixlen = mask2prefixlen(rr->mask.s_addr);
	rn->nexthop.s_addr = rr->nexthop.s_addr;
	rn->cost = adj_metric;
	rn->ifindex = rr->ifindex;
	rn->flags = F_DVMRPD_INSERTED;
	rn->connected = connected;
	rn->uptime = now.tv_sec;

	evtimer_set(&rn->expiration_timer, rt_expire_timer, rn);

	return (rn);
}

int
rt_insert(struct rt_node *r)
{
	if (RB_INSERT(rt_tree, &rt, r) != NULL) {
		log_warnx("rt_insert failed for %s/%u", inet_ntoa(r->prefix),
		    r->prefixlen);
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
rt_snap(u_int32_t peerid)
{
	struct rt_node		*r;
	struct route_report	 rr;

	RB_FOREACH(r, rt_tree, &rt) {
		if (r->invalid)
			continue;

		rr.net = r->prefix;
		rr.mask.s_addr = ntohl(prefixlen2mask(r->prefixlen));
		rr.metric = r->cost;
		rr.ifindex = r->ifindex;
		rde_imsg_compose_dvmrpe(IMSG_FULL_ROUTE_REPORT, peerid, 0, &rr,
		    sizeof(rr));
	}
}

void
rt_dump(pid_t pid)
{
	static struct ctl_rt	 rtctl;
	struct timespec		 now;
	struct timeval		 tv, now2, res;
	struct rt_node		*r;

	clock_gettime(CLOCK_MONOTONIC, &now);

	RB_FOREACH(r, rt_tree, &rt) {
		if (r->invalid)
			continue;

		rtctl.prefix.s_addr = r->prefix.s_addr;
		rtctl.nexthop.s_addr = r->nexthop.s_addr;
		rtctl.adv_rtr.s_addr = r->adv_rtr.s_addr;
		rtctl.cost = r->cost;
		rtctl.flags = r->flags;
		rtctl.prefixlen = r->prefixlen;
		rtctl.uptime = now.tv_sec - r->uptime;

		gettimeofday(&now2, NULL);
		if (evtimer_pending(&r->expiration_timer, &tv)) {
			timersub(&tv, &now2, &res);
			rtctl.expire = res.tv_sec;
		} else
			rtctl.expire = -1;

		rde_imsg_compose_dvmrpe(IMSG_CTL_SHOW_RIB, 0, pid, &rtctl,
		    sizeof(rtctl));
	}
}

void
rt_update(struct rt_node *rn)
{
	if (!rn->connected)
		rt_start_expire_timer(rn);
}

int
rde_check_route(struct route_report *rr, int connected)
{
	struct rt_node		*rn;
	struct iface		*iface;
	u_int32_t		 adj_metric;
	u_int32_t		 nbr_ip, nbr_report;

	if ((iface = if_find_index(rr->ifindex)) == NULL)
		return (-1);

	/* Interpret special case 0.0.0.0/8 as 0.0.0.0/0 */
	if (rr->net.s_addr == 0)
		rr->mask.s_addr = 0;

	adj_metric = rr->metric + iface->metric;

	if (rr->metric >= INFINITY_METRIC)
		return (0);

	if (adj_metric > INFINITY_METRIC)
		adj_metric = INFINITY_METRIC;

	/* If the route is new and the Adjusted Metric is less than infinity,
	   the route should be added. */
	if ((rn = rt_find(rr->net.s_addr, mask2prefixlen(rr->mask.s_addr)))
	    == NULL) {
		if (adj_metric < INFINITY_METRIC) {
			rn = rr_new_rt(rr, adj_metric, connected);
			rt_insert(rn);
		}
		return (0);
	}

	nbr_ip = rr->nexthop.s_addr;
	nbr_report = rn->nexthop.s_addr;

	if (rr->metric < INFINITY_METRIC) {
		if (adj_metric > rn->cost) {	
			if (nbr_ip == nbr_report) {
				rn->cost = adj_metric;	
			}
		} else if (adj_metric < rn->cost) {
			rn->cost = adj_metric;
			if (nbr_ip != nbr_report)
				rn->nexthop.s_addr = nbr_report;
		} else {
			if (nbr_report < nbr_ip)
				rn->nexthop.s_addr = nbr_report;
		}
		rt_update(rn);
	} else if (rr->metric == INFINITY_METRIC) {
		if (nbr_ip == nbr_report) {
			rt_remove(rn);
		}
	} else if (INFINITY_METRIC < rr->metric &&
	    rr->metric < 2 * INFINITY_METRIC) {
		/* XXX */
	}

	return (0);
}
