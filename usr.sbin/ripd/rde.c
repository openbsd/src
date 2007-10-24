/*	$OpenBSD: rde.c,v 1.9 2007/10/24 20:38:03 claudio Exp $ */

/*
 * Copyright (c) 2006 Michele Marchetto <mydecay@openbeer.it>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <event.h>

#include "ripd.h"
#include "rip.h"
#include "ripe.h"
#include "log.h"
#include "rde.h"

struct ripd_conf	*rdeconf = NULL;
struct imsgbuf		*ibuf_ripe;
struct imsgbuf		*ibuf_main;

void	rde_sig_handler(int, short, void *);
void	rde_shutdown(void);
void	rde_dispatch_imsg(int, short, void *);
void	rde_dispatch_parent(int, short, void *);
int	rde_imsg_compose_ripe(int, u_int32_t, pid_t, void *, u_int16_t);
int	rde_check_route(struct rip_route *);
void	triggered_update(struct rt_node *);

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
rde(struct ripd_conf *xconf, int pipe_parent2rde[2], int pipe_ripe2rde[2],
    int pipe_parent2ripe[2])
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;
	struct redistribute	*r;
	pid_t			 pid;

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

	if ((pw = getpwnam(RIPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	ripd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_ripe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2ripe[0]);
	close(pipe_parent2ripe[1]);

	if ((ibuf_ripe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_ripe, pipe_ripe2rde[1], rde_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_parent);

	/* setup event handler */
	ibuf_ripe->events = EV_READ;
	event_set(&ibuf_ripe->ev, ibuf_ripe->fd, ibuf_ripe->events,
	    ibuf_ripe->handler, ibuf_ripe);
	event_add(&ibuf_ripe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);
	rt_init();

	/* remove unneeded config stuff */
	while ((r = SIMPLEQ_FIRST(&rdeconf->redist_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&rdeconf->redist_list, entry);
		free(r);
	}

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	rt_clear();

	msgbuf_clear(&ibuf_ripe->w);
	free(ibuf_ripe);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);
	free(rdeconf);

	log_info("route decision engine exiting");
	_exit(0);
}

int
rde_imsg_compose_ripe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_ripe, type, peerid, pid, data, datalen));
}

/* ARGSUSED */
void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgbuf		*ibuf = bula;
	struct rip_route	 rr;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 shut = 0;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_ROUTE_FEED:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rr))
				fatalx("invalid size of RDE request");

			memcpy(&rr, imsg.data, sizeof(rr));

			if (rde_check_route(&rr) == -1)
				log_debug("rde_dispatch_imsg: "
				    "packet malformed\n");
			break;
		case IMSG_FULL_REQUEST:
			bzero(&rr, sizeof(rr));
			/*
			 * AFI == 0 && metric == INFINITY request the
			 * whole routing table
			 */
			rr.metric = INFINITY;
			rde_imsg_compose_ripe(IMSG_REQUEST_ADD, 0,
			    0, &rr, sizeof(rr));
			rde_imsg_compose_ripe(IMSG_SEND_REQUEST, 0,
			    0, NULL, 0);
			break;
		case IMSG_FULL_RESPONSE:
			rt_snap(imsg.hdr.peerid);
			rde_imsg_compose_ripe(IMSG_SEND_RESPONSE,
			    imsg.hdr.peerid, 0, NULL, 0);
			break;
		case IMSG_ROUTE_REQUEST:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rr))
				fatalx("invalid size of RDE request");

			memcpy(&rr, imsg.data, sizeof(rr));

			rt_complete(&rr);
			rde_imsg_compose_ripe(IMSG_RESPONSE_ADD,
			    imsg.hdr.peerid, 0, &rr, sizeof(rr));

			break;
		case IMSG_ROUTE_REQUEST_END:
			rde_imsg_compose_ripe(IMSG_SEND_RESPONSE,
			    imsg.hdr.peerid, 0, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB:
			rt_dump(imsg.hdr.pid);

			imsg_compose(ibuf_ripe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);

			break;
		default:
			log_debug("rde_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(ibuf);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&ibuf->ev);
		event_loopexit(NULL);
	}
}

/* ARGSUSED */
void
rde_dispatch_parent(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct rt_node		*rt;
	struct kroute		 kr;
	struct imsgbuf		*ibuf = bula;
	ssize_t			 n;
	int			 shut = 0;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}

			memcpy(&kr, imsg.data, sizeof(kr));

			rt = rt_new_kr(&kr);
			rt_insert(rt);
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			if ((rt = rt_find(kr.prefix.s_addr,
			    kr.netmask.s_addr)) != NULL)
				rt_remove(rt);
			break;
		case IMSG_KROUTE_GET:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			if ((rt = rt_find(kr.prefix.s_addr,
			    kr.netmask.s_addr)) != NULL)
				rde_send_change_kroute(rt);
			else
				/* should not happen */
				imsg_compose(ibuf_main, IMSG_KROUTE_DELETE, 0,
				    0, &kr, sizeof(kr));

			break;
		default:
			log_debug("rde_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(ibuf);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&ibuf->ev);
		event_loopexit(NULL);
	}
}

void
rde_send_change_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.netmask.s_addr = r->netmask.s_addr;
	kr.metric = r->metric;
	kr.flags = r->flags;
	kr.ifindex = r->ifindex;

	imsg_compose(ibuf_main, IMSG_KROUTE_CHANGE, 0, 0, &kr, sizeof(kr));
}

void
rde_send_delete_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.netmask.s_addr = r->netmask.s_addr;
	kr.metric = r->metric;
	kr.flags = r->flags;
	kr.ifindex = r->ifindex;

	imsg_compose(ibuf_main, IMSG_KROUTE_DELETE, 0, 0, &kr, sizeof(kr));
}

int
rde_check_route(struct rip_route *e)
{
	struct timeval	 tv, now;
	struct rt_node	*rn;
	struct iface	*iface;
	u_int8_t	 metric;

	if ((e->nexthop.s_addr & htonl(IN_CLASSA_NET)) ==
	    htonl(INADDR_LOOPBACK & IN_CLASSA_NET) ||
	    e->nexthop.s_addr == INADDR_ANY)
		return (-1);

	if ((iface = if_find_index(e->ifindex)) == NULL)
		return (-1);

	metric = MIN(INFINITY, e->metric + iface->cost);

	if ((rn = rt_find(e->address.s_addr, e->mask.s_addr)) == NULL) {
		if (metric >= INFINITY)
			return (0);
		rn = rt_new_rr(e, metric);
		rt_insert(rn);
		rde_send_change_kroute(rn);
		route_start_timeout(rn);
		triggered_update(rn);
	} else {
		/*
		 * XXX don't we have to track all incoming routes?
		 * what happens if the kernel route is removed later.
		 */
		if (rn->flags & F_KERNEL)
			return (0);

		if (metric < rn->metric) {
			rn->metric = metric;
			rn->nexthop.s_addr = e->nexthop.s_addr;
			rn->ifindex = e->ifindex;
			rde_send_change_kroute(rn);
			triggered_update(rn);
		} else if (e->nexthop.s_addr == rn->nexthop.s_addr &&
		    metric > rn->metric) {
				rn->metric = metric;
				rde_send_change_kroute(rn);
				triggered_update(rn);
				if (rn->metric == INFINITY)
					route_start_garbage(rn);
		} else if (e->nexthop.s_addr != rn->nexthop.s_addr &&
		    metric == rn->metric) {
			/* If the new metric is the same as the old one,
			 * examine the timeout for the existing route.  If it
			 * is at least halfway to the expiration point, switch
			 * to the new route.
			 */
			timerclear(&tv);
			gettimeofday(&now, NULL);
			evtimer_pending(&rn->timeout_timer, &tv);
			if (tv.tv_sec - now.tv_sec < ROUTE_TIMEOUT / 2) {
				rn->nexthop.s_addr = e->nexthop.s_addr;
				rn->ifindex = e->ifindex;
				rde_send_change_kroute(rn);
			}
		}

		if (e->nexthop.s_addr == rn->nexthop.s_addr &&
		    rn->metric < INFINITY)
			route_reset_timers(rn);
	}

	return (0);
}

void
triggered_update(struct rt_node *rn)
{
	struct rip_route	 rr;

	rr.address.s_addr = rn->prefix.s_addr;
	rr.mask.s_addr = rn->netmask.s_addr;
	rr.nexthop.s_addr = rn->nexthop.s_addr;
	rr.metric = rn->metric;
	rr.ifindex = rn->ifindex;

	rde_imsg_compose_ripe(IMSG_SEND_TRIGGERED_UPDATE, 0, 0, &rr,
	    sizeof(struct rip_route));
}
