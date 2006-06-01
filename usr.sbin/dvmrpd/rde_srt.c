/*	$OpenBSD: rde_srt.c,v 1.1 2006/06/01 14:12:20 norby Exp $ */

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
#include "rde.h"
#include "log.h"
#include "dvmrpe.h"

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
rt_update(struct in_addr prefix, u_int8_t prefixlen, struct in_addr nexthop,
    u_int32_t cost, struct in_addr adv_rtr, u_short ifindex, u_int8_t flags,
    u_int8_t connected)
{
	struct timespec	 now;
	struct rt_node	*rte;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if ((rte = rt_find(prefix.s_addr, prefixlen)) == NULL) {
		if ((rte = calloc(1, sizeof(struct rt_node))) == NULL)
			fatalx("rt_update");
		rte->prefix.s_addr = prefix.s_addr;
		rte->prefixlen = prefixlen;
		rte->nexthop.s_addr = nexthop.s_addr;
		rte->adv_rtr.s_addr = adv_rtr.s_addr;
		rte->cost = cost;
		rte->ifindex = ifindex;
		rte->flags = flags;
		rte->invalid = 0;
		rte->connected = connected;
		rte->uptime = now.tv_sec;

		rt_insert(rte);

		evtimer_set(&rte->expiration_timer, rt_expire_timer, rte);

		if (!rte->connected)
			rt_start_expire_timer(rte);

	} else {
		if (!rte->connected)
			rt_start_expire_timer(rte);
	}
}
