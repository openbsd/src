/*	$OpenBSD: rde.c,v 1.4 2015/10/10 05:12:33 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>

#include "eigrp.h"
#include "eigrpd.h"
#include "eigrpe.h"
#include "log.h"
#include "rde.h"

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);
void		 rde_dispatch_parent(int, short, void *);

struct eigrpd_conf	*rdeconf = NULL, *nconf;
struct imsgev		*iev_eigrpe;
struct imsgev		*iev_main;

extern struct iface_id_head ifaces_by_id;
RB_PROTOTYPE(iface_id_head, eigrp_iface, id_tree, iface_id_compare)

RB_PROTOTYPE(rt_tree, rt_node, entry, rt_compare)

extern struct rde_nbr_head rde_nbrs;
RB_PROTOTYPE(rde_nbr_head, rde_nbr, entry, rde_nbr_compare)

/* ARGSUSED */
void
rde_sig_handler(int sig, short event, void *arg)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		rde_shutdown();
		/* NOTREACHED */
	default:
		fatalx("unexpected signal");
	}
}

/* route decision engine */
pid_t
rde(struct eigrpd_conf *xconf, int pipe_parent2rde[2], int pipe_eigrpe2rde[2],
    int pipe_parent2eigrpe[2])
{
	struct event		 ev_sigint, ev_sigterm;
	struct timeval		 now;
	struct passwd		*pw;
	pid_t			 pid;
	struct eigrp		*eigrp;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
		/* NOTREACHED */
	case 0:
		break;
	default:
		return (pid);
	}

	rdeconf = xconf;

	if ((pw = getpwnam(EIGRPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	eigrpd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio", NULL) == -1)
		fatal("pledge");

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_eigrpe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2eigrpe[0]);
	close(pipe_parent2eigrpe[1]);

	if ((iev_eigrpe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_eigrpe->ibuf, pipe_eigrpe2rde[1]);
	iev_eigrpe->handler = rde_dispatch_imsg;
	imsg_init(&iev_main->ibuf, pipe_parent2rde[1]);
	iev_main->handler = rde_dispatch_parent;

	/* setup event handler */
	iev_eigrpe->events = EV_READ;
	event_set(&iev_eigrpe->ev, iev_eigrpe->ibuf.fd, iev_eigrpe->events,
	    iev_eigrpe->handler, iev_eigrpe);
	event_add(&iev_eigrpe->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	gettimeofday(&now, NULL);
	rdeconf->uptime = now.tv_sec;

	TAILQ_FOREACH(eigrp, &rdeconf->instances, entry)
		rde_instance_init(eigrp);

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	config_clear(rdeconf);

	msgbuf_clear(&iev_eigrpe->ibuf.w);
	free(iev_eigrpe);
	msgbuf_clear(&iev_main->ibuf.w);
	free(iev_main);

	log_info("route decision engine exiting");
	_exit(0);
}

int
rde_imsg_compose_parent(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1,
	    data, datalen));
}

int
rde_imsg_compose_eigrpe(int type, uint32_t peerid, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_eigrpe, type, peerid, pid, -1,
	    data, datalen));
}

/* ARGSUSED */
void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	struct rde_nbr		*nbr;
	struct rde_nbr		 new;
	struct rinfo		 rinfo;
	ssize_t			 n;
	int			 shut = 0, verbose;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* connection closed */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NEIGHBOR_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct rde_nbr))
				fatalx("invalid size of neighbor request");
			memcpy(&new, imsg.data, sizeof(new));

			if (rde_nbr_find(imsg.hdr.peerid))
				fatalx("rde_dispatch_imsg: "
				    "neighbor already exists");
			rde_nbr_new(imsg.hdr.peerid, &new);
			break;
		case IMSG_NEIGHBOR_DOWN:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("%s: cannot find rde neighbor",
				    __func__);
				break;
			}

			rde_check_link_down_nbr(nbr);
			rde_flush_queries();
			rde_nbr_del(rde_nbr_find(imsg.hdr.peerid), 0);
			break;
		case IMSG_RECV_UPDATE_INIT:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("%s: cannot find rde neighbor",
				    __func__);
				break;
			}

			rt_snap(nbr);
			break;
		case IMSG_RECV_UPDATE:
		case IMSG_RECV_QUERY:
		case IMSG_RECV_REPLY:
		case IMSG_RECV_SIAQUERY:
		case IMSG_RECV_SIAREPLY:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("%s: cannot find rde neighbor",
				    __func__);
				break;
			}

			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rinfo))
				fatalx("invalid size of rinfo");
			memcpy(&rinfo, imsg.data, sizeof(rinfo));

			switch (imsg.hdr.type) {
			case IMSG_RECV_UPDATE:
				rde_check_update(nbr, &rinfo);
				break;
			case IMSG_RECV_QUERY:
				rde_check_query(nbr, &rinfo, 0);
				break;
			case IMSG_RECV_REPLY:
				rde_check_reply(nbr, &rinfo, 0);
				break;
			case IMSG_RECV_SIAQUERY:
				rde_check_query(nbr, &rinfo, 1);
				break;
			case IMSG_RECV_SIAREPLY:
				rde_check_reply(nbr, &rinfo, 1);
				break;
			}
			break;
		case IMSG_CTL_SHOW_TOPOLOGY:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct ctl_show_topology_req)) {
				log_warnx("%s: wrong imsg len", __func__);
				break;
			}

			rt_dump(imsg.data, imsg.hdr.pid);
			rde_imsg_compose_eigrpe(IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by eigrpe */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("rde_dispatch_imsg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

/* ARGSUSED */
void
rde_dispatch_parent(int fd, short event, void *bula)
{
	struct iface		*niface = NULL;
	static struct eigrp	*neigrp;
	struct eigrp_iface	*nei;
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct kif		*kif;
	ssize_t			 n;
	int			 shut = 0;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* connection closed */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_IFDOWN:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kif))
				fatalx("IFDOWN imsg with wrong len");
			kif = imsg.data;
			rde_check_link_down(kif->ifindex);
			break;
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kroute))
				fatalx("IMSG_NETWORK_ADD imsg with wrong len");
			rt_redist_set(imsg.data, 0);
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kroute))
				fatalx("IMSG_NETWORK_DEL imsg with wrong len");
			rt_redist_set(imsg.data, 1);
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct eigrpd_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct eigrpd_conf));

			TAILQ_INIT(&nconf->iface_list);
			TAILQ_INIT(&nconf->instances);
			break;
		case IMSG_RECONF_INSTANCE:
			if ((neigrp = malloc(sizeof(struct eigrp))) == NULL)
				fatal(NULL);
			memcpy(neigrp, imsg.data, sizeof(struct eigrp));

			SIMPLEQ_INIT(&neigrp->redist_list);
			TAILQ_INIT(&neigrp->ei_list);
			RB_INIT(&neigrp->nbrs);
			RB_INIT(&neigrp->topology);
			TAILQ_INSERT_TAIL(&nconf->instances, neigrp, entry);
			break;
		case IMSG_RECONF_IFACE:
			niface = imsg.data;
			niface = if_lookup(nconf, niface->ifindex);
			if (niface)
				break;

			if ((niface = malloc(sizeof(struct iface))) == NULL)
				fatal(NULL);
			memcpy(niface, imsg.data, sizeof(struct iface));

			TAILQ_INIT(&niface->addr_list);
			TAILQ_INSERT_TAIL(&nconf->iface_list, niface, entry);
			break;
		case IMSG_RECONF_EIGRP_IFACE:
			if (niface == NULL)
				break;
			if ((nei = malloc(sizeof(struct eigrp_iface))) == NULL)
				fatal(NULL);
			memcpy(nei, imsg.data, sizeof(struct eigrp_iface));

			nei->iface = niface;
			nei->eigrp = neigrp;
			TAILQ_INIT(&nei->nbr_list);
			TAILQ_INIT(&nei->update_list);
			TAILQ_INIT(&nei->query_list);
			TAILQ_INSERT_TAIL(&niface->ei_list, nei, i_entry);
			TAILQ_INSERT_TAIL(&neigrp->ei_list, nei, e_entry);
			if (RB_INSERT(iface_id_head, &ifaces_by_id, nei) !=
			    NULL)
				fatalx("rde_dispatch_parent: "
				    "RB_INSERT(ifaces_by_id) failed");
			break;
		case IMSG_RECONF_END:
			merge_config(rdeconf, nconf);
			nconf = NULL;
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
rde_instance_init(struct eigrp *eigrp)
{
	struct rde_nbr		nbr;

	memset(&nbr, 0, sizeof(nbr));
	nbr.flags = F_RDE_NBR_SELF | F_RDE_NBR_REDIST;
	eigrp->rnbr_redist = rde_nbr_new(NBR_IDSELF, &nbr);
	eigrp->rnbr_redist->eigrp = eigrp;
	nbr.flags = F_RDE_NBR_SELF | F_RDE_NBR_SUMMARY;
	eigrp->rnbr_summary = rde_nbr_new(NBR_IDSELF, &nbr);
	eigrp->rnbr_summary->eigrp = eigrp;
}

void
rde_instance_del(struct eigrp *eigrp)
{
	struct rde_nbr		*nbr, *safe;
	struct rt_node		*rn;

	/* clear topology */
	while((rn = RB_MIN(rt_tree, &eigrp->topology)) != NULL)
		rt_del(rn);

	/* clear nbrs */
	RB_FOREACH_SAFE(nbr, rde_nbr_head, &rde_nbrs, safe)
		if (nbr->eigrp == eigrp)
			rde_nbr_del(nbr, 0);
	rde_nbr_del(eigrp->rnbr_redist, 0);
	rde_nbr_del(eigrp->rnbr_summary, 0);

	free(eigrp);
}

void
rde_send_change_kroute(struct rt_node *rn, struct eigrp_route *route)
{
	struct eigrp	*eigrp = route->nbr->eigrp;
	struct kroute	 kr;

	log_debug("%s: %s nbr %s", __func__, log_prefix(rn),
	    log_addr(eigrp->af, &route->nbr->addr));

	memset(&kr, 0, sizeof(kr));
	kr.af = eigrp->af;
	memcpy(&kr.prefix, &rn->prefix, sizeof(kr.prefix));
	kr.prefixlen = rn->prefixlen;
	if (eigrp_addrisset(eigrp->af, &route->nexthop))
		memcpy(&kr.nexthop, &route->nexthop, sizeof(kr.nexthop));
	else
		memcpy(&kr.nexthop, &route->nbr->addr, sizeof(kr.nexthop));
	kr.ifindex = route->nbr->ei->iface->ifindex;
	if (route->type == EIGRP_ROUTE_EXTERNAL)
		kr.priority = rdeconf->fib_priority_external;
	else
		kr.priority = rdeconf->fib_priority_internal;

	rde_imsg_compose_parent(IMSG_KROUTE_CHANGE, 0, &kr, sizeof(kr));

	route->flags |= F_EIGRP_ROUTE_INSTALLED;
}

void
rde_send_delete_kroute(struct rt_node *rn, struct eigrp_route *route)
{
	struct eigrp	*eigrp = route->nbr->eigrp;
	struct kroute	 kr;

	log_debug("%s: %s nbr %s", __func__, log_prefix(rn),
	    log_addr(eigrp->af, &route->nbr->addr));

	memset(&kr, 0, sizeof(kr));
	kr.af = eigrp->af;
	memcpy(&kr.prefix, &rn->prefix, sizeof(kr.prefix));
	kr.prefixlen = rn->prefixlen;
	if (eigrp_addrisset(eigrp->af, &route->nexthop))
		memcpy(&kr.nexthop, &route->nexthop, sizeof(kr.nexthop));
	else
		memcpy(&kr.nexthop, &route->nbr->addr, sizeof(kr.nexthop));
	kr.ifindex = route->nbr->ei->iface->ifindex;
	if (route->type == EIGRP_ROUTE_EXTERNAL)
		kr.priority = rdeconf->fib_priority_external;
	else
		kr.priority = rdeconf->fib_priority_internal;

	rde_imsg_compose_parent(IMSG_KROUTE_DELETE, 0, &kr, sizeof(kr));

	route->flags &= ~F_EIGRP_ROUTE_INSTALLED;
}

static struct redistribute *
eigrp_redistribute(struct eigrp *eigrp, struct kroute *kr)
{
	struct redistribute	*r;
	uint8_t			 is_default = 0;
	union eigrpd_addr	 addr;

	/* only allow the default route via REDIST_DEFAULT */
	if (!eigrp_addrisset(kr->af, &kr->prefix) && kr->prefixlen == 0)
		is_default = 1;

	SIMPLEQ_FOREACH(r, &eigrp->redist_list, entry) {
		switch (r->type & ~REDIST_NO) {
		case REDIST_STATIC:
			if (is_default)
				continue;
			if (kr->flags & F_STATIC)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		case REDIST_RIP:
			if (is_default)
				continue;
			if (kr->priority == RTP_RIP)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		case REDIST_OSPF:
			if (is_default)
				continue;
			if (kr->priority == RTP_OSPF)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		case REDIST_CONNECTED:
			if (is_default)
				continue;
			if (kr->flags & F_CONNECTED)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		case REDIST_ADDR:
			if (eigrp_addrisset(r->af, &r->addr) &&
			    r->prefixlen == 0) {
				if (is_default)
					return (r->type & REDIST_NO ? NULL : r);
				else
					return (0);
			}

			eigrp_applymask(kr->af, &addr, &kr->prefix,
			    r->prefixlen);
			if (eigrp_addrcmp(kr->af, &addr, &r->addr) == 0 &&
			    kr->prefixlen >= r->prefixlen)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		case REDIST_DEFAULT:
			if (is_default)
				return (r->type & REDIST_NO ? NULL : r);
			break;
		}
	}

	return (NULL);
}

void
rt_redist_set(struct kroute *kr, int withdraw)
{
	struct eigrp		*eigrp;
	struct redistribute	*r;
	struct redist_metric	*rmetric;
	struct rinfo		 ri;

	TAILQ_FOREACH(eigrp, &rdeconf->instances, entry) {
		if (eigrp->af != kr->af)
			continue;

		r = eigrp_redistribute(eigrp, kr);
		if (r == NULL)
			continue;

		if (r->metric)
			rmetric = r->metric;
		else if (eigrp->dflt_metric)
			rmetric = eigrp->dflt_metric;
		else
			continue;

		memset(&ri, 0, sizeof(ri));
		ri.af = kr->af;
		ri.type = EIGRP_ROUTE_EXTERNAL;
		memcpy(&ri.prefix, &kr->prefix, sizeof(ri.prefix));
		ri.prefixlen = kr->prefixlen;

		/* metric */
		if (withdraw)
			ri.metric.delay = EIGRP_INFINITE_METRIC;
		else
			ri.metric.delay = eigrp_composite_delay(rmetric->delay);
		ri.metric.bandwidth =
		    eigrp_composite_bandwidth(rmetric->bandwidth);
		metric_encode_mtu(ri.metric.mtu, rmetric->mtu);
		ri.metric.hop_count = 0;
		ri.metric.reliability = rmetric->reliability;
		ri.metric.load = rmetric->load;
		ri.metric.tag = 0;
		ri.metric.flags = 0;

		/* external metric */
		ri.emetric.routerid = htonl(eigrp_router_id(rdeconf));
		ri.emetric.as = r->emetric.as;
		ri.emetric.tag = r->emetric.tag;
		ri.emetric.metric = r->emetric.metric;
		if (kr->priority == rdeconf->fib_priority_internal)
			ri.emetric.protocol = EIGRP_EXT_PROTO_EIGRP;
		else if (kr->priority == RTP_STATIC)
			ri.emetric.protocol = EIGRP_EXT_PROTO_STATIC;
		else if (kr->priority == RTP_RIP)
			ri.emetric.protocol = EIGRP_EXT_PROTO_RIP;
		else if (kr->priority == RTP_OSPF)
			ri.emetric.protocol = EIGRP_EXT_PROTO_OSPF;
		else
			ri.emetric.protocol = EIGRP_EXT_PROTO_CONN;
		ri.emetric.flags = 0;

		rde_check_update(eigrp->rnbr_redist, &ri);
	}
}

/* send all known routing information to new neighbor */
void
rt_snap(struct rde_nbr *nbr)
{
	struct eigrp		*eigrp = nbr->eigrp;
	struct rt_node		*rn;
	struct rinfo		 ri;

	RB_FOREACH(rn, rt_tree, &eigrp->topology)
		if (rn->state == DUAL_STA_PASSIVE) {
			rinfo_fill_successor(rn, &ri);
			rde_imsg_compose_eigrpe(IMSG_SEND_UPDATE,
			    nbr->peerid, 0, &ri, sizeof(ri));
		}

	rde_imsg_compose_eigrpe(IMSG_SEND_UPDATE_END, nbr->peerid, 0,
	    NULL, 0);
}

struct ctl_rt *
rt_to_ctl(struct rt_node *rn, struct eigrp_route *route)
{
	static struct ctl_rt	 rtctl;

	memset(&rtctl, 0, sizeof(rtctl));
	rtctl.af = route->nbr->eigrp->af;
	rtctl.as = route->nbr->eigrp->as;
	memcpy(&rtctl.prefix, &rn->prefix, sizeof(rtctl.prefix));
	rtctl.prefixlen = rn->prefixlen;
	rtctl.type = route->type;
	memcpy(&rtctl.nexthop, &route->nbr->addr, sizeof(rtctl.nexthop));
	if (route->nbr->flags & F_RDE_NBR_REDIST)
		strlcpy(rtctl.ifname, "redistribute", sizeof(rtctl.ifname));
	else if (route->nbr->flags & F_RDE_NBR_SUMMARY)
		strlcpy(rtctl.ifname, "summary", sizeof(rtctl.ifname));
	else
		memcpy(rtctl.ifname, route->nbr->ei->iface->name,
		    sizeof(rtctl.ifname));
	rtctl.distance = route->distance;
	rtctl.rdistance = route->rdistance;
	rtctl.fdistance = rn->successor.fdistance;
	rtctl.state = rn->state;
	/* metric */
	rtctl.metric.delay = eigrp_real_delay(route->metric.delay);
	/* translate to microseconds */
	rtctl.metric.delay *= 10;
	rtctl.metric.bandwidth = eigrp_real_bandwidth(route->metric.bandwidth);
	rtctl.metric.mtu = metric_decode_mtu(route->metric.mtu);
	rtctl.metric.hop_count = route->metric.hop_count;
	rtctl.metric.reliability = route->metric.reliability;
	rtctl.metric.load = route->metric.load;
	/* external metric */
	memcpy(&rtctl.emetric, &route->emetric, sizeof(rtctl.emetric));

	if (route->nbr == rn->successor.nbr)
		rtctl.flags |= F_CTL_RT_SUCCESSOR;
	else if (route->rdistance < rn->successor.fdistance)
		rtctl.flags |= F_CTL_RT_FSUCCESSOR;

	return (&rtctl);
}

void
rt_dump(struct ctl_show_topology_req *treq, pid_t pid)
{
	struct eigrp		*eigrp;
	struct rt_node		*rn;
	struct eigrp_route	*route;
	struct ctl_rt		*rtctl;
	int			 first = 1;

	TAILQ_FOREACH(eigrp, &rdeconf->instances, entry) {
		RB_FOREACH(rn, rt_tree, &eigrp->topology) {
			if (eigrp_addrisset(treq->af, &treq->prefix) &&
			    eigrp_addrcmp(treq->af, &treq->prefix,
			    &rn->prefix))
				continue;

			if (treq->prefixlen &&
			    (treq->prefixlen != rn->prefixlen))
				continue;

			first = 1;
			TAILQ_FOREACH(route, &rn->routes, entry) {
				if (treq->flags & F_CTL_ACTIVE &&
				    !(rn->state & DUAL_STA_ACTIVE_ALL))
					continue;
				if (!(treq->flags & F_CTL_ALLLINKS) &&
				    route->rdistance >= rn->successor.fdistance)
					continue;

				rtctl = rt_to_ctl(rn, route);
				if (first) {
					rtctl->flags |= F_CTL_RT_FIRST;
					first = 0;
				}
				rde_imsg_compose_eigrpe(IMSG_CTL_SHOW_TOPOLOGY,
				    0, pid, rtctl, sizeof(*rtctl));
			}
		}
	}
}
