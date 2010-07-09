/*	$OpenBSD: rde.c,v 1.49 2010/07/09 12:39:46 bluhm Exp $ */

/*
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
#include <sys/param.h>
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

#include "ospf6.h"
#include "ospf6d.h"
#include "ospfe.h"
#include "log.h"
#include "rde.h"

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);
void		 rde_dispatch_parent(int, short, void *);
void		 rde_dump_area(struct area *, int, pid_t);

void		 rde_send_summary(pid_t);
void		 rde_send_summary_area(struct area *, pid_t);
void		 rde_nbr_init(u_int32_t);
void		 rde_nbr_free(void);
struct rde_nbr	*rde_nbr_new(u_int32_t, struct rde_nbr *);
void		 rde_nbr_del(struct rde_nbr *);

void		 rde_req_list_add(struct rde_nbr *, struct lsa_hdr *);
int		 rde_req_list_exists(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_del(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_free(struct rde_nbr *);

struct lsa	*rde_asext_get(struct rroute *);
struct lsa	*rde_asext_put(struct rroute *);

int		 comp_asext(struct lsa *, struct lsa *);
struct lsa	*orig_asext_lsa(struct rroute *, u_int16_t);
struct lsa	*orig_sum_lsa(struct rt_node *, struct area *, u_int8_t, int);
struct lsa	*orig_intra_lsa_net(struct area *, struct iface *,
		 struct vertex *);
struct lsa	*orig_intra_lsa_rtr(struct area *, struct vertex *);
void		 append_prefix_lsa(struct lsa **, u_int16_t *,
		    struct lsa_prefix *);

/* A 32-bit value != any ifindex.
 * We assume ifindex is bound by [1, USHRT_MAX] inclusive. */
#define	LS_ID_INTRA_RTR	0x01000000

/* Tree of prefixes with global scope on given a link,
 * see orig_intra_lsa_*() */
struct prefix_node {
	RB_ENTRY(prefix_node)	 entry;
	struct lsa_prefix	*prefix;
};
RB_HEAD(prefix_tree, prefix_node);
RB_PROTOTYPE(prefix_tree, prefix_node, entry, prefix_compare);
int		 prefix_compare(struct prefix_node *, struct prefix_node *);
void		 prefix_tree_add(struct prefix_tree *, struct lsa_link *);

struct ospfd_conf	*rdeconf = NULL, *nconf = NULL;
struct imsgev		*iev_ospfe;
struct imsgev		*iev_main;
struct rde_nbr		*nbrself;
struct lsa_tree		 asext_tree;

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
rde(struct ospfd_conf *xconf, int pipe_parent2rde[2], int pipe_ospfe2rde[2],
    int pipe_parent2ospfe[2])
{
	struct event		 ev_sigint, ev_sigterm;
	struct timeval		 now;
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

	if ((pw = getpwnam(OSPF6D_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	ospfd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	event_init();
	rde_nbr_init(NBR_HASHSIZE);
	lsa_init(&asext_tree);

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_ospfe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2ospfe[0]);
	close(pipe_parent2ospfe[1]);

	if ((iev_ospfe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_ospfe->ibuf, pipe_ospfe2rde[1]);
	iev_ospfe->handler = rde_dispatch_imsg;
	imsg_init(&iev_main->ibuf, pipe_parent2rde[1]);
	iev_main->handler = rde_dispatch_parent;

	/* setup event handler */
	iev_ospfe->events = EV_READ;
	event_set(&iev_ospfe->ev, iev_ospfe->ibuf.fd, iev_ospfe->events,
	    iev_ospfe->handler, iev_ospfe);
	event_add(&iev_ospfe->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	evtimer_set(&rdeconf->ev, spf_timer, rdeconf);
	cand_list_init();
	rt_init();

	while ((r = SIMPLEQ_FIRST(&rdeconf->redist_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&rdeconf->redist_list, entry);
		free(r);
	}

	gettimeofday(&now, NULL);
	rdeconf->uptime = now.tv_sec;

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	struct area	*a;

	stop_spf_timer(rdeconf);
	cand_list_clr();
	rt_clear();

	while ((a = LIST_FIRST(&rdeconf->area_list)) != NULL) {
		LIST_REMOVE(a, entry);
		area_del(a);
	}
	rde_nbr_free();

	msgbuf_clear(&iev_ospfe->ibuf.w);
	free(iev_ospfe);
	msgbuf_clear(&iev_main->ibuf.w);
	free(iev_main);
	free(rdeconf);

	log_info("route decision engine exiting");
	_exit(0);
}

int
rde_imsg_compose_ospfe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose_event(iev_ospfe, type, peerid, pid, -1,
	    data, datalen));
}

/* ARGSUSED */
void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct in_addr		 aid;
	struct ls_req_hdr	 req_hdr;
	struct lsa_hdr		 lsa_hdr, *db_hdr;
	struct rde_nbr		 rn, *nbr;
	struct timespec		 tp;
	struct lsa		*lsa;
	struct area		*area;
	struct vertex		*v;
	struct iface		*iface, *ifp;
	char			*buf;
	ssize_t			 n;
	time_t			 now;
	int			 r, state, self, shut = 0, verbose;
	u_int16_t		 l;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	clock_gettime(CLOCK_MONOTONIC, &tp);
	now = tp.tv_sec;

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NEIGHBOR_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rn))
				fatalx("invalid size of OE request");
			memcpy(&rn, imsg.data, sizeof(rn));

			if (rde_nbr_new(imsg.hdr.peerid, &rn) == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor already exists");
			break;
		case IMSG_NEIGHBOR_DOWN:
			rde_nbr_del(rde_nbr_find(imsg.hdr.peerid));
			break;
		case IMSG_NEIGHBOR_CHANGE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(state))
				fatalx("invalid size of OE request");
			memcpy(&state, imsg.data, sizeof(state));

			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			if (state != nbr->state &&
			    (nbr->state & NBR_STA_FULL ||
			    state & NBR_STA_FULL)) {
				nbr->state = state;
				area_track(nbr->area, state);
				orig_intra_area_prefix_lsas(nbr->area);
			}

			nbr->state = state;
			if (nbr->state & NBR_STA_FULL)
				rde_req_list_free(nbr);
			break;
		case IMSG_DB_SNAPSHOT:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			lsa_snap(nbr, imsg.hdr.peerid);

			imsg_compose_event(iev_ospfe, IMSG_DB_END, imsg.hdr.peerid,
			    0, -1, NULL, 0);
			break;
		case IMSG_DD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(lsa_hdr); l -= sizeof(lsa_hdr)) {
				memcpy(&lsa_hdr, buf, sizeof(lsa_hdr));
				buf += sizeof(lsa_hdr);

				v = lsa_find(nbr->iface, lsa_hdr.type,
				    lsa_hdr.ls_id, lsa_hdr.adv_rtr);
				if (v == NULL)
					db_hdr = NULL;
				else
					db_hdr = &v->lsa->hdr;

				if (lsa_newer(&lsa_hdr, db_hdr) > 0) {
					/*
					 * only request LSAs that are
					 * newer or missing
					 */
					rde_req_list_add(nbr, &lsa_hdr);
					imsg_compose_event(iev_ospfe, IMSG_DD,
					    imsg.hdr.peerid, 0, -1, &lsa_hdr,
					    sizeof(lsa_hdr));
				}
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in Database Description "
				    "packet", imsg.hdr.peerid);

			imsg_compose_event(iev_ospfe, IMSG_DD_END,
			    imsg.hdr.peerid, 0, -1, NULL, 0);
			break;
		case IMSG_LS_REQ:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(req_hdr); l -= sizeof(req_hdr)) {
				memcpy(&req_hdr, buf, sizeof(req_hdr));
				buf += sizeof(req_hdr);

				if ((v = lsa_find(nbr->iface,
				    req_hdr.type, req_hdr.ls_id,
				    req_hdr.adv_rtr)) == NULL) {
					imsg_compose_event(iev_ospfe,
					    IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, -1, NULL, 0);
					continue;
				}
				imsg_compose_event(iev_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, -1, v->lsa,
				    ntohs(v->lsa->hdr.len));
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in LS Request "
				    "packet", imsg.hdr.peerid);
			break;
		case IMSG_LS_UPD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			lsa = malloc(imsg.hdr.len - IMSG_HEADER_SIZE);
			if (lsa == NULL)
				fatal(NULL);
			memcpy(lsa, imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			if (!lsa_check(nbr, lsa,
			    imsg.hdr.len - IMSG_HEADER_SIZE)) {
				free(lsa);
				break;
			}

			v = lsa_find(nbr->iface, lsa->hdr.type, lsa->hdr.ls_id,
				    lsa->hdr.adv_rtr);
			if (v == NULL)
				db_hdr = NULL;
			else
				db_hdr = &v->lsa->hdr;

			if (nbr->self) {
				lsa_merge(nbr, lsa, v);
				/* lsa_merge frees the right lsa */
				break;
			}

			r = lsa_newer(&lsa->hdr, db_hdr);
			if (r > 0) {
				/* new LSA newer than DB */
				if (v && v->flooded &&
				    v->changed + MIN_LS_ARRIVAL >= now) {
					free(lsa);
					break;
				}

				rde_req_list_del(nbr, &lsa->hdr);

				self = lsa_self(lsa);
				if (self) {
					if (v == NULL)
						/* LSA is no longer announced,
						 * remove by premature aging. */
						lsa_flush(nbr, lsa);
					else
						lsa_reflood(v, lsa);
				} else if (lsa_add(nbr, lsa))
					/* delayed lsa, don't flood yet */
					break;

				/* flood and perhaps ack LSA */
				imsg_compose_event(iev_ospfe, IMSG_LS_FLOOD,
				    imsg.hdr.peerid, 0, -1, lsa,
				    ntohs(lsa->hdr.len));

				/* reflood self originated LSA */
				if (self && v)
					imsg_compose_event(iev_ospfe,
					    IMSG_LS_FLOOD,
					    v->peerid, 0, -1, v->lsa,
					    ntohs(v->lsa->hdr.len));
				/* new LSA was not added so free it */
				if (self)
					free(lsa);
			} else if (r < 0) {
				/*
				 * point 6 of "The Flooding Procedure"
				 * We are violating the RFC here because
				 * it does not make sense to reset a session
				 * because an equal LSA is already in the table.
				 * Only if the LSA sent is older than the one
				 * in the table we should reset the session.
				 */
				if (rde_req_list_exists(nbr, &lsa->hdr)) {
					imsg_compose_event(iev_ospfe,
					    IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, -1, NULL, 0);
					free(lsa);
					break;
				}

				/* lsa no longer needed */
				free(lsa);

				/* new LSA older than DB */
				if (ntohl(db_hdr->seq_num) == MAX_SEQ_NUM &&
				    ntohs(db_hdr->age) == MAX_AGE)
					/* seq-num wrap */
					break;

				if (v->changed + MIN_LS_ARRIVAL >= now)
					break;

				/* directly send current LSA, no ack */
				imsg_compose_event(iev_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, -1, v->lsa,
				    ntohs(v->lsa->hdr.len));
			} else {
				/* LSA equal send direct ack */
				imsg_compose_event(iev_ospfe, IMSG_LS_ACK,
				    imsg.hdr.peerid, 0, -1, &lsa->hdr,
				    sizeof(lsa->hdr));
				free(lsa);
			}
			break;
		case IMSG_LS_MAXAGE:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct lsa_hdr))
				fatalx("invalid size of OE request");
			memcpy(&lsa_hdr, imsg.data, sizeof(lsa_hdr));

			if (rde_nbr_loading(nbr->area))
				break;

			v = lsa_find(nbr->iface, lsa_hdr.type, lsa_hdr.ls_id,
				    lsa_hdr.adv_rtr);
			if (v == NULL)
				db_hdr = NULL;
			else
				db_hdr = &v->lsa->hdr;

			/*
			 * only delete LSA if the one in the db is not newer
			 */
			if (lsa_newer(db_hdr, &lsa_hdr) <= 0)
				lsa_del(nbr, &lsa_hdr);
			break;
		case IMSG_CTL_SHOW_DATABASE:
		case IMSG_CTL_SHOW_DB_EXT:
		case IMSG_CTL_SHOW_DB_LINK:
		case IMSG_CTL_SHOW_DB_NET:
		case IMSG_CTL_SHOW_DB_RTR:
		case IMSG_CTL_SHOW_DB_INTRA:
		case IMSG_CTL_SHOW_DB_SELF:
		case IMSG_CTL_SHOW_DB_SUM:
		case IMSG_CTL_SHOW_DB_ASBR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE &&
			    imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(aid)) {
				log_warnx("rde_dispatch_imsg: wrong imsg len");
				break;
			}
			if (imsg.hdr.len == IMSG_HEADER_SIZE) {
				LIST_FOREACH(area, &rdeconf->area_list, entry) {
					rde_dump_area(area, imsg.hdr.type,
					    imsg.hdr.pid);
				}
				lsa_dump(&asext_tree, imsg.hdr.type,
				    imsg.hdr.pid);
			} else {
				memcpy(&aid, imsg.data, sizeof(aid));
				if ((area = area_find(rdeconf, aid)) != NULL) {
					rde_dump_area(area, imsg.hdr.type,
					    imsg.hdr.pid);
					if (!area->stub)
						lsa_dump(&asext_tree,
						    imsg.hdr.type,
						    imsg.hdr.pid);
				}
			}
			imsg_compose_event(iev_ospfe, IMSG_CTL_END, 0,
			    imsg.hdr.pid, -1, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB:
			LIST_FOREACH(area, &rdeconf->area_list, entry) {
				imsg_compose_event(iev_ospfe, IMSG_CTL_AREA,
				    0, imsg.hdr.pid, -1, area, sizeof(*area));

				rt_dump(area->id, imsg.hdr.pid, RIB_RTR);
				rt_dump(area->id, imsg.hdr.pid, RIB_NET);
			}
			aid.s_addr = 0;
			rt_dump(aid, imsg.hdr.pid, RIB_EXT);

			imsg_compose_event(iev_ospfe, IMSG_CTL_END, 0,
			    imsg.hdr.pid, -1, NULL, 0);
			break;
		case IMSG_CTL_SHOW_SUM:
			rde_send_summary(imsg.hdr.pid);
			LIST_FOREACH(area, &rdeconf->area_list, entry)
				rde_send_summary_area(area, imsg.hdr.pid);
			imsg_compose_event(iev_ospfe, IMSG_CTL_END, 0,
			    imsg.hdr.pid, -1, NULL, 0);
			break;
		case IMSG_IFINFO:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct iface))
				fatalx("IFINFO imsg with wrong len");

			ifp = imsg.data;

			iface = if_find(ifp->ifindex);
			if (iface == NULL)
				fatalx("interface lost in rde");
			iface->flags = ifp->flags;
			iface->linkstate = ifp->linkstate;
			iface->nh_reachable = ifp->nh_reachable;
			if (iface->state != ifp->state) {
				iface->state = ifp->state;
				area = area_find(rdeconf, iface->area_id);
				if (!area)
					fatalx("interface lost area");
				orig_intra_area_prefix_lsas(area);
			}
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by ospfe */
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
	static struct area	*narea;
	struct area		*area;
	struct iface		*iface;
	struct ifaddrchange	*ifc;
	struct iface_addr	*ia, *nia;
	struct imsg		 imsg;
	struct kroute		 kr;
	struct rroute		 rr;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct lsa		*lsa;
	struct vertex		*v;
	struct rt_node		*rn;
	ssize_t			 n;
	int			 shut = 0;
	unsigned int		 ifindex;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(rr)) {
				log_warnx("rde_dispatch_parent: "
				    "wrong imsg len");
				break;
			}
			memcpy(&rr, imsg.data, sizeof(rr));

			if ((lsa = rde_asext_get(&rr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				lsa_merge(nbrself, lsa, v);
			}
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(rr)) {
				log_warnx("rde_dispatch_parent: "
				    "wrong imsg len");
				break;
			}
			memcpy(&rr, imsg.data, sizeof(rr));

			if ((lsa = rde_asext_put(&rr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				/*
				 * if v == NULL no LSA is in the table and
				 * nothing has to be done.
				 */
				if (v)
					lsa_merge(nbrself, lsa, v);
				else
					free(lsa);
			}
			break;
		case IMSG_KROUTE_GET:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("rde_dispatch_parent: "
				    "wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			if ((rn = rt_find(&kr.prefix, kr.prefixlen,
			    DT_NET)) != NULL)
				rde_send_change_kroute(rn);
			else
				/* should not happen */
				imsg_compose_event(iev_main, IMSG_KROUTE_DELETE, 0,
				    0, -1, &kr, sizeof(kr));
			break;
		case IMSG_IFADD:
			if ((iface = malloc(sizeof(struct iface))) == NULL)
				fatal(NULL);
			memcpy(iface, imsg.data, sizeof(struct iface));

			LIST_INIT(&iface->nbr_list);
			TAILQ_INIT(&iface->ls_ack_list);
			RB_INIT(&iface->lsa_tree);

			area = area_find(rdeconf, iface->area_id);
			LIST_INSERT_HEAD(&area->iface_list, iface, entry);
			break;
		case IMSG_IFDELETE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(ifindex))
				fatalx("IFDELETE imsg with wrong len");

			memcpy(&ifindex, imsg.data, sizeof(ifindex));
			iface = if_find(ifindex);
			if (iface == NULL)
				fatalx("interface lost in ospfe");

			LIST_REMOVE(iface, entry);
			if_del(iface);
			break;
		case IMSG_IFADDRNEW:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct ifaddrchange))
				fatalx("IFADDRNEW imsg with wrong len");
			ifc = imsg.data;

			iface = if_find(ifc->ifindex);
			if (iface == NULL)
				fatalx("IFADDRNEW interface lost in rde");

			if ((ia = calloc(1, sizeof(struct iface_addr))) ==
			    NULL)
				fatal("rde_dispatch_parent IFADDRNEW");
			ia->addr = ifc->addr;
			ia->dstbrd = ifc->dstbrd;
			ia->prefixlen = ifc->prefixlen;

			TAILQ_INSERT_TAIL(&iface->ifa_list, ia, entry);
			area = area_find(rdeconf, iface->area_id);
			if (area)
				orig_intra_area_prefix_lsas(area);
			break;
		case IMSG_IFADDRDEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct ifaddrchange))
				fatalx("IFADDRDEL imsg with wrong len");
			ifc = imsg.data;

			iface = if_find(ifc->ifindex);
			if (iface == NULL)
				fatalx("IFADDRDEL interface lost in rde");

			for (ia = TAILQ_FIRST(&iface->ifa_list); ia != NULL;
			    ia = nia) {
				nia = TAILQ_NEXT(ia, entry);

				if (IN6_ARE_ADDR_EQUAL(&ia->addr,
				    &ifc->addr)) {
					TAILQ_REMOVE(&iface->ifa_list, ia,
					    entry);
					free(ia);
					break;
				}
			}
			area = area_find(rdeconf, iface->area_id);
			if (area)
				orig_intra_area_prefix_lsas(area);
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct ospfd_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct ospfd_conf));

			LIST_INIT(&nconf->area_list);
			LIST_INIT(&nconf->cand_list);
			break;
		case IMSG_RECONF_AREA:
			if ((narea = area_new()) == NULL)
				fatal(NULL);
			memcpy(narea, imsg.data, sizeof(struct area));

			LIST_INIT(&narea->iface_list);
			LIST_INIT(&narea->nbr_list);
			RB_INIT(&narea->lsa_tree);

			LIST_INSERT_HEAD(&nconf->area_list, narea, entry);
			break;
		case IMSG_RECONF_END:
			merge_config(rdeconf, nconf);
			nconf = NULL;
			break;
		default:
			log_debug("rde_dispatch_parent: unexpected imsg %d",
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
rde_dump_area(struct area *area, int imsg_type, pid_t pid)
{
	struct iface	*iface;

	/* dump header */
	imsg_compose_event(iev_ospfe, IMSG_CTL_AREA, 0, pid, -1,
	    area, sizeof(*area));

	/* dump link local lsa */
	LIST_FOREACH(iface, &area->iface_list, entry) {
		imsg_compose_event(iev_ospfe, IMSG_CTL_IFACE,
		    0, pid, -1, iface, sizeof(*iface));
		lsa_dump(&iface->lsa_tree, imsg_type, pid);
	}

	/* dump area lsa */
	lsa_dump(&area->lsa_tree, imsg_type, pid);
}

u_int32_t
rde_router_id(void)
{
	return (rdeconf->rtr_id.s_addr);
}

void
rde_send_change_kroute(struct rt_node *r)
{
	struct kroute		 kr;
	struct rt_nexthop	*rn;

	TAILQ_FOREACH(rn, &r->nexthop, entry) {
		if (!rn->invalid)
			break;
	}
	if (!rn)
		fatalx("rde_send_change_kroute: no valid nexthop found");

	bzero(&kr, sizeof(kr));
	kr.prefix = r->prefix;
	kr.nexthop = rn->nexthop;
	if (IN6_IS_ADDR_LINKLOCAL(&rn->nexthop) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&rn->nexthop))
		kr.scope = rn->ifindex;
	kr.ifindex = rn->ifindex;
	kr.prefixlen = r->prefixlen;
	kr.ext_tag = r->ext_tag;

	imsg_compose_event(iev_main, IMSG_KROUTE_CHANGE, 0, 0, -1,
	    &kr, sizeof(kr));
}

void
rde_send_delete_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix = r->prefix;
	kr.prefixlen = r->prefixlen;

	imsg_compose_event(iev_main, IMSG_KROUTE_DELETE, 0, 0, -1,
	    &kr, sizeof(kr));
}

void
rde_send_summary(pid_t pid)
{
	static struct ctl_sum	 sumctl;
	struct timeval		 now;
	struct area		*area;
	struct vertex		*v;

	bzero(&sumctl, sizeof(struct ctl_sum));

	sumctl.rtr_id.s_addr = rde_router_id();
	sumctl.spf_delay = rdeconf->spf_delay;
	sumctl.spf_hold_time = rdeconf->spf_hold_time;

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		sumctl.num_area++;

	RB_FOREACH(v, lsa_tree, &asext_tree)
		sumctl.num_ext_lsa++;

	gettimeofday(&now, NULL);
	if (rdeconf->uptime < now.tv_sec)
		sumctl.uptime = now.tv_sec - rdeconf->uptime;
	else
		sumctl.uptime = 0;

	rde_imsg_compose_ospfe(IMSG_CTL_SHOW_SUM, 0, pid, &sumctl,
	    sizeof(sumctl));
}

void
rde_send_summary_area(struct area *area, pid_t pid)
{
	static struct ctl_sum_area	 sumareactl;
	struct iface			*iface;
	struct rde_nbr			*nbr;
	struct lsa_tree			*tree = &area->lsa_tree;
	struct vertex			*v;

	bzero(&sumareactl, sizeof(struct ctl_sum_area));

	sumareactl.area.s_addr = area->id.s_addr;
	sumareactl.num_spf_calc = area->num_spf_calc;

	LIST_FOREACH(iface, &area->iface_list, entry)
		sumareactl.num_iface++;

	LIST_FOREACH(nbr, &area->nbr_list, entry)
		if (nbr->state == NBR_STA_FULL && !nbr->self)
			sumareactl.num_adj_nbr++;

	RB_FOREACH(v, lsa_tree, tree)
		sumareactl.num_lsa++;

	rde_imsg_compose_ospfe(IMSG_CTL_SHOW_SUM_AREA, 0, pid, &sumareactl,
	    sizeof(sumareactl));
}

LIST_HEAD(rde_nbr_head, rde_nbr);

struct nbr_table {
	struct rde_nbr_head	*hashtbl;
	u_int32_t		 hashmask;
} rdenbrtable;

#define RDE_NBR_HASH(x)		\
	&rdenbrtable.hashtbl[(x) & rdenbrtable.hashmask]

void
rde_nbr_init(u_int32_t hashsize)
{
	struct rde_nbr_head	*head;
	u_int32_t		 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	rdenbrtable.hashtbl = calloc(hs, sizeof(struct rde_nbr_head));
	if (rdenbrtable.hashtbl == NULL)
		fatal("rde_nbr_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&rdenbrtable.hashtbl[i]);

	rdenbrtable.hashmask = hs - 1;

	if ((nbrself = calloc(1, sizeof(*nbrself))) == NULL)
		fatal("rde_nbr_init");

	nbrself->id.s_addr = rde_router_id();
	nbrself->peerid = NBR_IDSELF;
	nbrself->state = NBR_STA_DOWN;
	nbrself->self = 1;
	head = RDE_NBR_HASH(NBR_IDSELF);
	LIST_INSERT_HEAD(head, nbrself, hash);
}

void
rde_nbr_free(void)
{
	free(nbrself);
	free(rdenbrtable.hashtbl);
}

struct rde_nbr *
rde_nbr_find(u_int32_t peerid)
{
	struct rde_nbr_head	*head;
	struct rde_nbr		*nbr;

	head = RDE_NBR_HASH(peerid);

	LIST_FOREACH(nbr, head, hash) {
		if (nbr->peerid == peerid)
			return (nbr);
	}

	return (NULL);
}

struct rde_nbr *
rde_nbr_new(u_int32_t peerid, struct rde_nbr *new)
{
	struct rde_nbr_head	*head;
	struct rde_nbr		*nbr;
	struct area		*area;
	struct iface		*iface;

	if (rde_nbr_find(peerid))
		return (NULL);
	if ((area = area_find(rdeconf, new->area_id)) == NULL)
		fatalx("rde_nbr_new: unknown area");

	LIST_FOREACH(iface, &area->iface_list, entry) {
		if (iface->ifindex == new->ifindex)
			break;
	}
	if (iface == NULL)
		fatalx("rde_nbr_new: unknown interface");

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("rde_nbr_new");

	memcpy(nbr, new, sizeof(*nbr));
	nbr->peerid = peerid;
	nbr->area = area;
	nbr->iface = iface;

	TAILQ_INIT(&nbr->req_list);

	head = RDE_NBR_HASH(peerid);
	LIST_INSERT_HEAD(head, nbr, hash);
	LIST_INSERT_HEAD(&area->nbr_list, nbr, entry);

	return (nbr);
}

void
rde_nbr_del(struct rde_nbr *nbr)
{
	if (nbr == NULL)
		return;

	rde_req_list_free(nbr);

	LIST_REMOVE(nbr, entry);
	LIST_REMOVE(nbr, hash);

	free(nbr);
}

int
rde_nbr_loading(struct area *area)
{
	struct rde_nbr		*nbr;
	int			 checkall = 0;

	if (area == NULL) {
		area = LIST_FIRST(&rdeconf->area_list);
		checkall = 1;
	}

	while (area != NULL) {
		LIST_FOREACH(nbr, &area->nbr_list, entry) {
			if (nbr->self)
				continue;
			if (nbr->state & NBR_STA_XCHNG ||
			    nbr->state & NBR_STA_LOAD)
				return (1);
		}
		if (!checkall)
			break;
		area = LIST_NEXT(area, entry);
	}

	return (0);
}

struct rde_nbr *
rde_nbr_self(struct area *area)
{
	struct rde_nbr		*nbr;

	LIST_FOREACH(nbr, &area->nbr_list, entry)
		if (nbr->self)
			return (nbr);

	/* this may not happen */
	fatalx("rde_nbr_self: area without self");
	return (NULL);
}

/*
 * LSA req list
 */
void
rde_req_list_add(struct rde_nbr *nbr, struct lsa_hdr *lsa)
{
	struct rde_req_entry	*le;

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("rde_req_list_add");

	TAILQ_INSERT_TAIL(&nbr->req_list, le, entry);
	le->type = lsa->type;
	le->ls_id = lsa->ls_id;
	le->adv_rtr = lsa->adv_rtr;
}

int
rde_req_list_exists(struct rde_nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct rde_req_entry	*le;

	TAILQ_FOREACH(le, &nbr->req_list, entry) {
		if ((lsa_hdr->type == le->type) &&
		    (lsa_hdr->ls_id == le->ls_id) &&
		    (lsa_hdr->adv_rtr == le->adv_rtr))
			return (1);
	}
	return (0);
}

void
rde_req_list_del(struct rde_nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct rde_req_entry	*le;

	TAILQ_FOREACH(le, &nbr->req_list, entry) {
		if ((lsa_hdr->type == le->type) &&
		    (lsa_hdr->ls_id == le->ls_id) &&
		    (lsa_hdr->adv_rtr == le->adv_rtr)) {
			TAILQ_REMOVE(&nbr->req_list, le, entry);
			free(le);
			return;
		}
	}
}

void
rde_req_list_free(struct rde_nbr *nbr)
{
	struct rde_req_entry	*le;

	while ((le = TAILQ_FIRST(&nbr->req_list)) != NULL) {
		TAILQ_REMOVE(&nbr->req_list, le, entry);
		free(le);
	}
}

/*
 * as-external LSA handling
 */
struct lsa *
rde_asext_get(struct rroute *rr)
{
	struct area		*area;
	struct iface		*iface;
	struct iface_addr	*ia;
	struct in6_addr		 addr;

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		LIST_FOREACH(iface, &area->iface_list, entry)
			TAILQ_FOREACH(ia, &iface->ifa_list, entry) {
				if (IN6_IS_ADDR_LINKLOCAL(&ia->addr))
					continue;

				inet6applymask(&addr, &ia->addr,
				    rr->kr.prefixlen);
				if (!memcmp(&addr, &rr->kr.prefix,
				    sizeof(addr)) && rr->kr.prefixlen ==
				    ia->prefixlen) {
					/* already announced as Prefix LSA */
					log_debug("rde_asext_get: %s/%d is "
					    "part of prefix LSA",
					    log_in6addr(&rr->kr.prefix),
					    rr->kr.prefixlen);
					return (NULL);
				}
			}

	/* update of seqnum is done by lsa_merge */
	return (orig_asext_lsa(rr, DEFAULT_AGE));
}

struct lsa *
rde_asext_put(struct rroute *rr)
{
	/*
	 * just try to remove the LSA. If the prefix is announced as
	 * stub net LSA lsa_find() will fail later and nothing will happen.
	 */

	/* remove by reflooding with MAX_AGE */
	return (orig_asext_lsa(rr, MAX_AGE));
}

/*
 * summary LSA stuff
 */
void
rde_summary_update(struct rt_node *rte, struct area *area)
{
	struct vertex		*v = NULL;
//XXX	struct lsa		*lsa;
	u_int16_t		 type = 0;

	/* first check if we actually need to announce this route */
	if (!(rte->d_type == DT_NET || rte->flags & OSPF_RTR_E))
		return;
	/* never create summaries for as-ext LSA */
	if (rte->p_type == PT_TYPE1_EXT || rte->p_type == PT_TYPE2_EXT)
		return;
	/* no need for summary LSA in the originating area */
	if (rte->area.s_addr == area->id.s_addr)
		return;
	/* no need to originate inter-area routes to the backbone */
	if (rte->p_type == PT_INTER_AREA && area->id.s_addr == INADDR_ANY)
		return;
	/* TODO nexthop check, nexthop part of area -> no summary */
	if (rte->cost >= LS_INFINITY)
		return;
	/* TODO AS border router specific checks */
	/* TODO inter-area network route stuff */
	/* TODO intra-area stuff -- condense LSA ??? */

	if (rte->d_type == DT_NET) {
		type = LSA_TYPE_INTER_A_PREFIX;
	} else if (rte->d_type == DT_RTR) {
		type = LSA_TYPE_INTER_A_ROUTER;
	} else

#if 0 /* XXX a lot todo */
	/* update lsa but only if it was changed */
	v = lsa_find(area, type, rte->prefix.s_addr, rde_router_id());
	lsa = orig_sum_lsa(rte, area, type, rte->invalid);
	lsa_merge(rde_nbr_self(area), lsa, v);

	if (v == NULL)
		v = lsa_find(area, type, rte->prefix.s_addr, rde_router_id());
#endif

	/* suppressed/deleted routes are not found in the second lsa_find */
	if (v)
		v->cost = rte->cost;
}

/*
 * Functions for self-originated LSAs
 */

/* Prefix LSAs have variable size. We have to be careful to copy the right
 * amount of bytes, and to realloc() the right amount of memory. */
void
append_prefix_lsa(struct lsa **lsa, u_int16_t *len, struct lsa_prefix *prefix)
{
	struct lsa_prefix	*copy;
	unsigned int		 lsa_prefix_len;
	unsigned int		 new_len;
	char			*new_lsa;

	lsa_prefix_len = sizeof(struct lsa_prefix)
	    + LSA_PREFIXSIZE(prefix->prefixlen);

	new_len = *len + lsa_prefix_len;

	/* Make sure we have enough space for this prefix. */
	if ((new_lsa = realloc(*lsa, new_len)) == NULL)
		fatalx("append_prefix_lsa");

	/* Append prefix to LSA. */
	copy = (struct lsa_prefix *)(new_lsa + *len);
	memcpy(copy, prefix, lsa_prefix_len);
	copy->metric = 0;

	*lsa = (struct lsa *)new_lsa;
	*len = new_len;
}

int
prefix_compare(struct prefix_node *a, struct prefix_node *b)
{
	struct lsa_prefix	*p;
	struct lsa_prefix	*q;
	int			 i;
	int			 len;

	p = a->prefix;
	q = b->prefix;

	len = MIN(LSA_PREFIXSIZE(p->prefixlen), LSA_PREFIXSIZE(q->prefixlen));

	i = memcmp(p + 1, q + 1, len);
	if (i)
		return (i);
	if (p->prefixlen < q->prefixlen)
		return (-1);
	if (p->prefixlen > q->prefixlen)
		return (1);
	return (0);
}

void
prefix_tree_add(struct prefix_tree *tree, struct lsa_link *lsa)
{
	struct prefix_node	*old;
	struct prefix_node	*new;
	struct in6_addr		 addr;
	unsigned int		 len;
	unsigned int		 i;
	char			*cur_prefix;

	cur_prefix = (char *)(lsa + 1);

	for (i = 0; i < ntohl(lsa->numprefix); i++) {
		if ((new = calloc(1, sizeof(*new))) == NULL)
			fatal("prefix_tree_add");
		new->prefix = (struct lsa_prefix *)cur_prefix;

		len = sizeof(*new->prefix)
		    + LSA_PREFIXSIZE(new->prefix->prefixlen);

		bzero(&addr, sizeof(addr));
		memcpy(&addr, new->prefix + 1,
		    LSA_PREFIXSIZE(new->prefix->prefixlen));

		if (!(IN6_IS_ADDR_LINKLOCAL(&addr)) &&
		    (new->prefix->options & OSPF_PREFIX_NU) == 0 &&
		    (new->prefix->options & OSPF_PREFIX_LA) == 0) {
			old = RB_INSERT(prefix_tree, tree, new);
			if (old != NULL) {
				old->prefix->options |= new->prefix->options;
				free(new);
			}
		}

		cur_prefix = cur_prefix + len;
	}
}

RB_GENERATE(prefix_tree, prefix_node, entry, prefix_compare)

struct lsa *
orig_intra_lsa_net(struct area *area, struct iface *iface, struct vertex *old)
{
	struct lsa		*lsa;
	struct vertex		*v;
	struct rde_nbr		*nbr;
	struct prefix_node	*node;
	struct prefix_tree	 tree;
	int			 num_full_nbr;
	u_int16_t		 len;
	u_int16_t		 numprefix;

	log_debug("orig_intra_lsa_net: area %s, interface %s",
	    inet_ntoa(area->id), iface->name);

	RB_INIT(&tree);

	if (iface->state & IF_STA_DR) {
		num_full_nbr = 0;
		LIST_FOREACH(nbr, &area->nbr_list, entry) {
			if (nbr->self ||
			    nbr->iface->ifindex != iface->ifindex ||
			    (nbr->state & NBR_STA_FULL) == 0)
				continue;
			num_full_nbr++;
			v = lsa_find(iface, htons(LSA_TYPE_LINK),
			    htonl(nbr->iface_id), nbr->id.s_addr);
			if (v)
				prefix_tree_add(&tree, &v->lsa->data.link);
		}
		if (num_full_nbr == 0) {
			/* There are no adjacent neighbors on link.
			 * If a copy of this LSA already exists in DB,
			 * it needs to be flushed. orig_intra_lsa_rtr()
			 * will take care of prefixes configured on
			 * this interface. */
			if (!old)
				return NULL;
		} else {
			/* Add our own prefixes configured for this link. */
			v = lsa_find(iface, htons(LSA_TYPE_LINK),
			    htonl(iface->ifindex), rde_router_id());
			if (v)
				prefix_tree_add(&tree, &v->lsa->data.link);
		}
	/* Continue only if a copy of this LSA already exists in DB.
	 * It needs to be flushed. */
	} else if (!old)
		return NULL;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_intra_prefix);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_intra_lsa_net");

	lsa->data.pref_intra.ref_type = htons(LSA_TYPE_NETWORK);
	lsa->data.pref_intra.ref_ls_id = htonl(iface->ifindex);
	lsa->data.pref_intra.ref_adv_rtr = rde_router_id();

	numprefix = 0;
	RB_FOREACH(node, prefix_tree, &tree) {
		append_prefix_lsa(&lsa, &len, node->prefix);
		numprefix++;
	}

	lsa->data.pref_intra.numprefix = htons(numprefix);

	while (!RB_EMPTY(&tree))
		free(RB_REMOVE(prefix_tree, &tree, RB_ROOT(&tree)));

	/* LSA header */
	/* If numprefix is zero, originate with MAX_AGE to flush LSA. */
	lsa->hdr.age = numprefix == 0 ? htons(MAX_AGE) : htons(DEFAULT_AGE);
	lsa->hdr.type = htons(LSA_TYPE_INTRA_A_PREFIX);
	lsa->hdr.ls_id = htonl(iface->ifindex);
	lsa->hdr.adv_rtr = rde_router_id();
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);
	lsa->hdr.ls_chksum = htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return lsa;
}

struct lsa *
orig_intra_lsa_rtr(struct area *area, struct vertex *old)
{
	char			lsa_prefix_buf[sizeof(struct lsa_prefix)
				    + sizeof(struct in6_addr)];
	struct lsa		*lsa;
	struct lsa_prefix	*lsa_prefix;
	struct in6_addr		*prefix;
	struct iface		*iface;
	struct iface_addr	*ia;
	struct rde_nbr		*nbr;
	u_int16_t		 len;
	u_int16_t		 numprefix;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_intra_prefix);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_intra_lsa_rtr");

	lsa->data.pref_intra.ref_type = htons(LSA_TYPE_ROUTER);
	lsa->data.pref_intra.ref_ls_id = 0;
	lsa->data.pref_intra.ref_adv_rtr = rde_router_id();

	log_debug("orig_intra_lsa_rtr: area %s", inet_ntoa(area->id));

	numprefix = 0;
	LIST_FOREACH(iface, &area->iface_list, entry) {
		if (iface->state & IF_STA_DOWN)
			continue;

		/* Broadcast links with adjacencies are handled
		 * by orig_intra_lsa_net(), ignore. */
		if (iface->type == IF_TYPE_BROADCAST ||
		    iface->type == IF_TYPE_NBMA) {
			if (iface->state & IF_STA_WAITING)
				/* Skip, we're still waiting for
				 * adjacencies to form. */
				continue;

			LIST_FOREACH(nbr, &area->nbr_list, entry)
				if (!nbr->self &&
				    nbr->iface->ifindex == iface->ifindex &&
				    nbr->state & NBR_STA_FULL)
					break;
			if (nbr)
				continue;
		}

		lsa_prefix = (struct lsa_prefix *)lsa_prefix_buf;

		TAILQ_FOREACH(ia, &iface->ifa_list, entry) {
			if (IN6_IS_ADDR_LINKLOCAL(&ia->addr))
				continue;

			bzero(lsa_prefix_buf, sizeof(lsa_prefix_buf));

			if (iface->type == IF_TYPE_POINTOMULTIPOINT ||
			    iface->state & IF_STA_LOOPBACK) {
				lsa_prefix->prefixlen = 128;
			} else {
				lsa_prefix->prefixlen = ia->prefixlen;
				lsa_prefix->metric = htons(iface->metric);
			}

			if (lsa_prefix->prefixlen == 128)
				lsa_prefix->options |= OSPF_PREFIX_LA;

			prefix = (struct in6_addr *)(lsa_prefix + 1);
			inet6applymask(prefix, &ia->addr,
			    lsa_prefix->prefixlen);
			append_prefix_lsa(&lsa, &len, lsa_prefix);
			numprefix++;
		}

		/* TOD: Add prefixes of directly attached hosts, too */
		/* TOD: Add prefixes for virtual links */
	}

	/* If no prefixes were included, continue only if a copy of this
	 * LSA already exists in DB. It needs to be flushed. */
	if (numprefix == 0 && !old) {
		free(lsa);
		return NULL;
	}

	lsa->data.pref_intra.numprefix = htons(numprefix);

	/* LSA header */
	/* If numprefix is zero, originate with MAX_AGE to flush LSA. */
	lsa->hdr.age = numprefix == 0 ? htons(MAX_AGE) : htons(DEFAULT_AGE);
	lsa->hdr.type = htons(LSA_TYPE_INTRA_A_PREFIX);
	lsa->hdr.ls_id = htonl(LS_ID_INTRA_RTR);
	lsa->hdr.adv_rtr = rde_router_id();
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);
	lsa->hdr.ls_chksum = htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return lsa;
}

void
orig_intra_area_prefix_lsas(struct area *area)
{
	struct lsa	*lsa;
	struct vertex	*old;
	struct iface	*iface;

	LIST_FOREACH(iface, &area->iface_list, entry) {
		if (iface->type == IF_TYPE_BROADCAST ||
		    iface->type == IF_TYPE_NBMA) {
			old = lsa_find(iface, htons(LSA_TYPE_INTRA_A_PREFIX),
			    htonl(iface->ifindex), rde_router_id());
			lsa = orig_intra_lsa_net(area, iface, old);
			if (lsa)
				lsa_merge(rde_nbr_self(area), lsa, old);
		}
	}

	old = lsa_find_tree(&area->lsa_tree, htons(LSA_TYPE_INTRA_A_PREFIX),
		htonl(LS_ID_INTRA_RTR), rde_router_id());
	lsa = orig_intra_lsa_rtr(area, old);
	if (lsa)
		lsa_merge(rde_nbr_self(area), lsa, old);
}

int
comp_asext(struct lsa *a, struct lsa *b)
{
	/* compare prefixes, if they are equal or not */
	if (a->data.asext.prefix.prefixlen != b->data.asext.prefix.prefixlen)
		return (-1);
	return (memcmp(
	    (char *)a + sizeof(struct lsa_hdr) + sizeof(struct lsa_asext),
	    (char *)b + sizeof(struct lsa_hdr) + sizeof(struct lsa_asext),
	    LSA_PREFIXSIZE(a->data.asext.prefix.prefixlen)));
}

struct lsa *
orig_asext_lsa(struct rroute *rr, u_int16_t age)
{
	struct lsa	*lsa;
	u_int32_t	 ext_tag;
	u_int16_t	 len, ext_off = 0;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_asext) +
	    LSA_PREFIXSIZE(rr->kr.prefixlen);

	/*
	 * nexthop -- on connected routes we are the nexthop,
	 * on all other cases we should announce the true nexthop
	 * unless that nexthop is outside of the ospf cloud.
	 * XXX for now we don't do this.
	 */

	if (rr->kr.ext_tag) {
		ext_off = len;
		len += sizeof(ext_tag);
	}
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_asext_lsa");

	log_debug("orig_asext_lsa: %s/%d age %d",
	    log_in6addr(&rr->kr.prefix), rr->kr.prefixlen, age);

	/* LSA header */
	lsa->hdr.age = htons(age);
	lsa->hdr.type = htons(LSA_TYPE_EXTERNAL);
	lsa->hdr.adv_rtr = rdeconf->rtr_id.s_addr;
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);

	lsa->data.asext.metric = htonl(rr->metric);
	lsa->data.asext.prefix.prefixlen = rr->kr.prefixlen;
	memcpy((char *)lsa + sizeof(struct lsa_hdr) + sizeof(struct lsa_asext),
	    &rr->kr.prefix, LSA_PREFIXSIZE(rr->kr.prefixlen));

	if (rr->kr.ext_tag) {
		lsa->data.asext.prefix.options |= LSA_ASEXT_T_FLAG;
		ext_tag = htonl(rr->kr.ext_tag);
		memcpy((char *)lsa + ext_off, &ext_tag, sizeof(ext_tag));
	}

	lsa->hdr.ls_id = lsa_find_lsid(&asext_tree, lsa->hdr.type,
	    lsa->hdr.adv_rtr, comp_asext, lsa);
	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
}

struct lsa *
orig_sum_lsa(struct rt_node *rte, struct area *area, u_int8_t type, int invalid)
{
#if 0 /* XXX a lot todo */
	struct lsa	*lsa;
	u_int16_t	 len;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_sum);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_sum_lsa");

	/* LSA header */
	lsa->hdr.age = htons(invalid ? MAX_AGE : DEFAULT_AGE);
	lsa->hdr.type = type;
	lsa->hdr.adv_rtr = rdeconf->rtr_id.s_addr;
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);

	/* prefix and mask */
	/*
	 * TODO ls_id must be unique, for overlapping routes this may
	 * not be true. In this case a hack needs to be done to
	 * make the ls_id unique.
	 */
	lsa->hdr.ls_id = rte->prefix.s_addr;
	if (type == LSA_TYPE_SUM_NETWORK)
		lsa->data.sum.mask = prefixlen2mask(rte->prefixlen);
	else
		lsa->data.sum.mask = 0;	/* must be zero per RFC */

	lsa->data.sum.metric = htonl(rte->cost & LSA_METRIC_MASK);

	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
#endif
	return NULL;
}
