/*	$OpenBSD: rde.c,v 1.32 2005/11/04 10:38:03 claudio Exp $ */

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

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "log.h"
#include "rde.h"

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);
void		 rde_dispatch_parent(int, short, void *);

void		 rde_send_summary(pid_t);
void		 rde_send_summary_area(struct area *, pid_t);
void		 rde_nbr_init(u_int32_t);
struct rde_nbr	*rde_nbr_find(u_int32_t);
struct rde_nbr	*rde_nbr_new(u_int32_t, struct rde_nbr *);
void		 rde_nbr_del(struct rde_nbr *);

void		 rde_req_list_add(struct rde_nbr *, struct lsa_hdr *);
int		 rde_req_list_exists(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_del(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_free(struct rde_nbr *);

int		 rde_redistribute(struct kroute *);
void		 rde_update_redistribute(int);
struct lsa	*rde_asext_get(struct kroute *);
struct lsa	*rde_asext_put(struct kroute *);

struct lsa	*orig_asext_lsa(struct kroute *, u_int16_t);
struct lsa	*orig_sum_lsa(struct rt_node *, u_int8_t);

volatile sig_atomic_t	 rde_quit = 0;
struct ospfd_conf	*rdeconf = NULL;
struct imsgbuf		*ibuf_ospfe;
struct imsgbuf		*ibuf_main;
struct rde_nbr		*nbrself;

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
		/* NOTREACHED */
	}
}

/* route decision engine */
pid_t
rde(struct ospfd_conf *xconf, int pipe_parent2rde[2], int pipe_ospfe2rde[2],
    int pipe_parent2ospfe[2])
{
	struct passwd		*pw;
	struct event		 ev_sigint, ev_sigterm;
	pid_t			 pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	rdeconf = xconf; /* XXX may not be replaced because of the lsa_tree */

	if ((pw = getpwnam(OSPFD_USER)) == NULL)
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
	lsa_init(&rdeconf->lsa_tree);

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);

	/* setup pipes */
	close(pipe_ospfe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2ospfe[0]);
	close(pipe_parent2ospfe[1]);

	if ((ibuf_ospfe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_ospfe, pipe_ospfe2rde[1], rde_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_parent);

	/* setup event handler */
	ibuf_ospfe->events = EV_READ;
	event_set(&ibuf_ospfe->ev, ibuf_ospfe->fd, ibuf_ospfe->events,
	    ibuf_ospfe->handler, ibuf_ospfe);
	event_add(&ibuf_ospfe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	evtimer_set(&rdeconf->spf_timer, spf_timer, rdeconf);
	cand_list_init();
	rt_init();

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	stop_spf_timer(rdeconf);
	cand_list_clr();
	rt_clear();

	msgbuf_write(&ibuf_ospfe->w);
	msgbuf_clear(&ibuf_ospfe->w);
	free(ibuf_ospfe);
	msgbuf_write(&ibuf_main->w);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);

	log_info("route decision engine exiting");
	_exit(0);
}

/* imesg */
int
rde_imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose(ibuf_main, type, 0, pid, -1, data, datalen));
}

int
rde_imsg_compose_ospfe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_ospfe, type, peerid, pid, -1, data, datalen));
}

void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct in_addr		 aid;
	struct ls_req_hdr	 req_hdr;
	struct lsa_hdr		 lsa_hdr, *db_hdr;
	struct rde_nbr		 rn, *nbr;
	struct lsa		*lsa;
	struct area		*area;
	struct vertex		*v;
	char			*buf;
	time_t			 now;
	int			 r, n, state, self;
	u_int16_t		 l;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			fatalx("pipe closed");
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	now = time(NULL);

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

			if (rde_nbr_find(imsg.hdr.peerid))
				fatalx("rde_dispatch_imsg: "
				    "neighbor already exists");
			rde_nbr_new(imsg.hdr.peerid, &rn);
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
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			nbr->state = state;

			if (nbr->state & NBR_STA_FULL)
				rde_req_list_free(nbr);
			break;
		case IMSG_DB_SNAPSHOT:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			lsa_snap(nbr->area, imsg.hdr.peerid);

			imsg_compose(ibuf_ospfe, IMSG_DB_END, imsg.hdr.peerid,
			    0, -1, NULL, 0);
			break;
		case IMSG_DD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(lsa_hdr); l -= sizeof(lsa_hdr)) {
				memcpy(&lsa_hdr, buf, sizeof(lsa_hdr));
				buf += sizeof(lsa_hdr);

				v = lsa_find(nbr->area, lsa_hdr.type,
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
					imsg_compose(ibuf_ospfe, IMSG_DD,
					    imsg.hdr.peerid, 0, -1,
					    &lsa_hdr, sizeof(lsa_hdr));
				}
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in Database Description "
				    "packet", imsg.hdr.peerid);

			imsg_compose(ibuf_ospfe, IMSG_DD_END, imsg.hdr.peerid,
			    0, -1, NULL, 0);
			break;
		case IMSG_LS_REQ:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(req_hdr); l -= sizeof(req_hdr)) {
				memcpy(&req_hdr, buf, sizeof(req_hdr));
				buf += sizeof(req_hdr);

				if ((v = lsa_find(nbr->area,
				    ntohl(req_hdr.type), req_hdr.ls_id,
				    req_hdr.adv_rtr)) == NULL) {
					imsg_compose(ibuf_ospfe, IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, -1, NULL, 0);
					continue;
				}
				imsg_compose(ibuf_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, -1,
				    v->lsa, ntohs(v->lsa->hdr.len));
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in LS Request "
				    "packet", imsg.hdr.peerid);
			break;
		case IMSG_LS_UPD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			lsa = malloc(imsg.hdr.len - IMSG_HEADER_SIZE);
			if (lsa == NULL)
				fatal(NULL);
			memcpy(lsa, imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			if (!lsa_check(nbr, lsa,
			    imsg.hdr.len - IMSG_HEADER_SIZE)) {
				free(lsa);
				break;
			}

			v = lsa_find(nbr->area, lsa->hdr.type, lsa->hdr.ls_id,
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
				if (!(self = lsa_self(nbr, lsa, v)))
					lsa_add(nbr, lsa);

				rde_req_list_del(nbr, &lsa->hdr);
				/* flood and perhaps ack LSA */
				imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
				    imsg.hdr.peerid, 0, -1,
				    lsa, ntohs(lsa->hdr.len));

				/* reflood self originated LSA */
				if (self && v)
					imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
					    v->nbr->peerid, 0, -1,
					    v->lsa, ntohs(v->lsa->hdr.len));
				/* lsa not added so free it */
				if (self)
					free(lsa);
			} else if (r < 0) {
				/* lsa no longer needed */
				free(lsa);

				/*
				 * point 6 of "The Flooding Procedure"
				 * We are violating the RFC here because
				 * it does not make sense to reset a session
				 * because a equal LSA is already in the table.
				 * Only if the LSA sent is older than the one
				 * in the table we should reset the session.
				 */
				if (rde_req_list_exists(nbr, &lsa->hdr)) {
					imsg_compose(ibuf_ospfe, IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, -1, NULL, 0);
					break;
				}

				/* new LSA older than DB */
				if (ntohl(db_hdr->seq_num) == MAX_SEQ_NUM &&
				    ntohs(db_hdr->age) == MAX_AGE)
					/* seq-num wrap */
					break;

				if (v->changed + MIN_LS_ARRIVAL >= now)
					break;

				/* directly send current LSA, no ack */
				imsg_compose(ibuf_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, -1,
				    v->lsa, ntohs(v->lsa->hdr.len));
			} else {
				/* LSA equal send direct ack */
				imsg_compose(ibuf_ospfe, IMSG_LS_ACK,
				    imsg.hdr.peerid, 0, -1, &lsa->hdr,
				    sizeof(lsa->hdr));
				free(lsa);
			}
			break;
		case IMSG_LS_MAXAGE:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");

			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct lsa_hdr))
				fatalx("invalid size of OE request");
			memcpy(&lsa_hdr, imsg.data, sizeof(lsa_hdr));

			aid.s_addr = lsa_hdr.ls_id;
			log_debug("rde_dispatch_imsg: IMSG_LS_MAXAGE, "
			    "type %d ls_id %s", lsa_hdr.type,
			    inet_ntoa(aid));

			if (rde_nbr_loading(nbr->area)) {
				log_debug("IMSG_LS_MAXAGE still loading");
				break;
			}

			v = lsa_find(nbr->area, lsa_hdr.type, lsa_hdr.ls_id,
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
		case IMSG_CTL_SHOW_DB_NET:
		case IMSG_CTL_SHOW_DB_RTR:
		case IMSG_CTL_SHOW_DB_SELF:
		case IMSG_CTL_SHOW_DB_SUM:
		case IMSG_CTL_SHOW_DB_ASBR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE &&
			    imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(aid)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			if (imsg.hdr.len == IMSG_HEADER_SIZE) {
				LIST_FOREACH(area, &rdeconf->area_list, entry) {
					imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
					    0, imsg.hdr.pid, -1, area,
					    sizeof(*area));
					lsa_dump(&area->lsa_tree, imsg.hdr.type,
					    imsg.hdr.pid);
				}
				lsa_dump(&rdeconf->lsa_tree, imsg.hdr.type,
				    imsg.hdr.pid);
			} else {
				memcpy(&aid, imsg.data, sizeof(aid));
				if ((area = area_find(rdeconf, aid)) != NULL) {
					imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
					    0, imsg.hdr.pid, -1, area,
					    sizeof(*area));
					lsa_dump(&area->lsa_tree, imsg.hdr.type,
					    imsg.hdr.pid);
					if (!area->stub)
						lsa_dump(&rdeconf->lsa_tree,
						    imsg.hdr.type,
						    imsg.hdr.pid);
				}
			}
			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    -1, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB:
			LIST_FOREACH(area, &rdeconf->area_list, entry) {
				imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
				    0, imsg.hdr.pid, -1, area,
				    sizeof(*area));

				rt_dump(area->id, imsg.hdr.pid, RIB_RTR);
				rt_dump(area->id, imsg.hdr.pid, RIB_NET);
			}
			aid.s_addr = 0;
			rt_dump(aid, imsg.hdr.pid, RIB_EXT);

			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    -1, NULL, 0);
			break;
		case IMSG_CTL_SHOW_SUM:
			rde_send_summary(imsg.hdr.pid);
			LIST_FOREACH(area, &rdeconf->area_list, entry)
				rde_send_summary_area(area, imsg.hdr.pid);
			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    -1, NULL, 0);
			break;
		default:
			log_debug("rde_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
rde_dispatch_parent(int fd, short event, void *bula)
{
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct lsa		*lsa;
	struct vertex		*v;
	struct kroute		 kr;
	struct kif		 kif;
	int			 n;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			fatalx("pipe closed");
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
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			log_debug("rde: new announced net %s/%d",
			    inet_ntoa(kr.prefix), kr.prefixlen);
			if ((lsa = rde_asext_get(&kr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				lsa_merge(nbrself, lsa, v);
			}
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			log_debug("rde: removing announced net %s/%d",
				    inet_ntoa(kr.prefix), kr.prefixlen);
			if ((lsa = rde_asext_put(&kr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				lsa_merge(nbrself, lsa, v);
			}
			break;
		case IMSG_IFINFO:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kif)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kif, imsg.data, sizeof(kif));

			log_debug("IMSG_IFINFO: ifindex %i reachable %d",
			    kif.ifindex, kif.nh_reachable);
			kif_update(&kif);
			rde_update_redistribute(kif.ifindex);
			break;
		default:
			log_debug("rde_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

u_int32_t
rde_router_id(void)
{
	return (rdeconf->rtr_id.s_addr);
}

void
rde_send_change_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.prefixlen = r->prefixlen;

	imsg_compose(ibuf_main, IMSG_KROUTE_CHANGE, 0, 0, -1, &kr, sizeof(kr));
}

void
rde_send_delete_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.prefixlen = r->prefixlen;

	imsg_compose(ibuf_main, IMSG_KROUTE_DELETE, 0, 0, -1, &kr, sizeof(kr));
}

void
rde_send_summary(pid_t pid)
{
	static struct ctl_sum	 sumctl;
	struct lsa_tree		*tree = &rdeconf->lsa_tree;
	struct area		*area;
	struct vertex		*v;

	bzero(&sumctl, sizeof(struct ctl_sum));

	sumctl.rtr_id.s_addr = rde_router_id();
	sumctl.spf_delay = rdeconf->spf_delay;
	sumctl.spf_hold_time = rdeconf->spf_hold_time;

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		sumctl.num_area++;

	RB_FOREACH(v, lsa_tree, tree)
		sumctl.num_ext_lsa++;

	sumctl.rfc1583compat = rdeconf->rfc1583compat;

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

	if (rde_nbr_find(peerid))
		return (NULL);
	if ((area = area_find(rdeconf, new->area_id)) == NULL)
		fatalx("rde_nbr_new: unknown area");

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("rde_nbr_new");

	memcpy(nbr, new, sizeof(*nbr));
	nbr->peerid = peerid;
	nbr->area = area;

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

	LIST_FOREACH(nbr, &area->nbr_list, entry) {
		if (nbr->self)
			continue;
		if (nbr->state & NBR_STA_XCHNG ||
		    nbr->state & NBR_STA_LOAD)
			return (1);
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
LIST_HEAD(, rde_asext) rde_asext_list;

int
rde_redistribute(struct kroute *kr)
{
	struct area	*area;
	struct iface	*iface;
	int		 rv = 0;

	if (!(kr->flags & F_KERNEL))
		return (0);

	if ((rdeconf->options & OSPF_OPTION_E) == 0)
		return (0);

	if ((rdeconf->redistribute_flags & REDISTRIBUTE_DEFAULT) &&
	    (kr->prefix.s_addr == INADDR_ANY && kr->prefixlen == 0))
		return (1);

	/* only allow 0.0.0.0/0 if REDISTRIBUTE_DEFAULT */
	if (kr->prefix.s_addr == INADDR_ANY && kr->prefixlen == 0)
		return (0);

	if ((rdeconf->redistribute_flags & REDISTRIBUTE_STATIC) &&
	    (kr->flags & F_STATIC))
		rv = 1;
	if ((rdeconf->redistribute_flags & REDISTRIBUTE_CONNECTED) &&
	    (kr->flags & F_CONNECTED))
		rv = 1;

	/* interface is not up and running so don't announce */
	if (kif_validate(kr->ifindex) == 0)
		return (0);

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		LIST_FOREACH(iface, &area->iface_list, entry) {
			if ((iface->addr.s_addr & iface->mask.s_addr) ==
			    kr->prefix.s_addr && iface->mask.s_addr ==
			    prefixlen2mask(kr->prefixlen))
				rv = 0;	/* already announced as net LSA */
		}

	return (rv);
}

void
rde_update_redistribute(int ifindex)
{
	struct rde_asext	*ae;
	struct lsa		*lsa;
	struct vertex		*v;
	int			 wasused;

	LIST_FOREACH(ae, &rde_asext_list, entry)
		if (ae->kr.ifindex == ifindex) {
			wasused = ae->used;
			ae->used = rde_redistribute(&ae->kr);
			if (ae->used)
				lsa = orig_asext_lsa(&ae->kr, DEFAULT_AGE);
			else if (wasused)
				lsa = orig_asext_lsa(&ae->kr, MAX_AGE);
			else
				continue;

			v = lsa_find(NULL, lsa->hdr.type,
			    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

			lsa_merge(nbrself, lsa, v);
		}
}

struct lsa *
rde_asext_get(struct kroute *kr)
{
	struct rde_asext	*ae;
	int			 wasused;

	LIST_FOREACH(ae, &rde_asext_list, entry)
		if (kr->prefix.s_addr == ae->kr.prefix.s_addr &&
		    kr->prefixlen == ae->kr.prefixlen)
			break;

	if (ae == NULL) {
		if ((ae = calloc(1, sizeof(*ae))) == NULL)
			fatal("rde_asext_get");
		LIST_INSERT_HEAD(&rde_asext_list, ae, entry);
	}

	memcpy(&ae->kr, kr, sizeof(ae->kr));

	wasused = ae->used;
	ae->used = rde_redistribute(kr);

	if (ae->used)
		/* update of seqnum is done by lsa_merge */
		return (orig_asext_lsa(kr, DEFAULT_AGE));
	else if (wasused)
		/* lsa_merge will take care of removing the lsa from the db */
		return (orig_asext_lsa(kr, MAX_AGE));
	else
		/* not in lsdb, superseded by a net lsa */
		return (NULL);
}

struct lsa *
rde_asext_put(struct kroute *kr)
{
	struct rde_asext	*ae;
	int			 used;

	LIST_FOREACH(ae, &rde_asext_list, entry)
		if (kr->prefix.s_addr == ae->kr.prefix.s_addr &&
		    kr->prefixlen == ae->kr.prefixlen) {
			LIST_REMOVE(ae, entry);
			used = ae->used;
			free(ae);
			if (used)
				return (orig_asext_lsa(kr, MAX_AGE));
			break;
		}
	return (NULL);
}

/*
 * summary LSA stuff
 */
void
rde_summary_update(struct rt_node *rte, struct area *area)
{
	struct vertex	*v = NULL;
	struct lsa	*lsa;
	u_int8_t	 type = 0;

	/* first check if we actually need to announce this route */
	if (!(rte->d_type == DT_NET || rte->flags & OSPF_RTR_E))
		return;
	/* never create summaries for as-ext LSA */
	if (rte->p_type == PT_TYPE1_EXT || rte->p_type == PT_TYPE2_EXT)
		return;
	/* no need for summary LSA in the originating area */
	if (rte->area.s_addr == area->id.s_addr)
		return;
	/* TODO nexthop check, nexthop part of area -> no summary */
	if (rte->cost >= LS_INFINITY)
		return;
	/* TODO AS border router specific checks */
	/* TODO inter-area network route stuff */
	/* TODO intra-area stuff -- condense LSA ??? */

	/* update lsa but only if it was changed */
	if (rte->d_type == DT_NET) {
		type = LSA_TYPE_SUM_NETWORK;
		v = lsa_find(area, type, rte->prefix.s_addr, rde_router_id());
	} else if (rte->d_type == DT_RTR) {
		type = LSA_TYPE_SUM_ROUTER;
		v = lsa_find(area, type, rte->adv_rtr.s_addr, rde_router_id());
	} else
		fatalx("orig_sum_lsa: unknown route type");

	lsa = orig_sum_lsa(rte, type);
	lsa_merge(rde_nbr_self(area), lsa, v);

	if (v == NULL) {
		if (rte->d_type == DT_NET)
			v = lsa_find(area, type, rte->prefix.s_addr,
			    rde_router_id());
		else
			v = lsa_find(area, type, rte->adv_rtr.s_addr,
			    rde_router_id());
	}
	v->cost = rte->cost;
}


/*
 * functions for self-originated LSA
 */
struct lsa *
orig_asext_lsa(struct kroute *kr, u_int16_t age)
{
	struct lsa	*lsa;
	size_t		 len;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_asext);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_asext_lsa");

	log_debug("orig_asext_lsa: %s/%d age %d",
	    inet_ntoa(kr->prefix), kr->prefixlen, age);

	/* LSA header */
	lsa->hdr.age = htons(age);
	lsa->hdr.opts = rdeconf->options;	/* XXX not updated */
	lsa->hdr.type = LSA_TYPE_EXTERNAL;
	lsa->hdr.adv_rtr = rdeconf->rtr_id.s_addr;
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);

	/* prefix and mask */
	/*
	 * TODO ls_id must be unique, for overlapping routes this may
	 * not be true. In this case a hack needs to be done to
	 * make the ls_id unique.
	 */
	lsa->hdr.ls_id = kr->prefix.s_addr;
	lsa->data.asext.mask = prefixlen2mask(kr->prefixlen);

	/*
	 * nexthop -- on connected routes we are the nexthop,
	 * on all other cases we announce the true nexthop.
	 * XXX this is wrong as the true nexthop may be outside
	 * of the ospf cloud and so unreachable. For now we force
	 * all traffic to be directed to us.
	 */
	lsa->data.asext.fw_addr = 0;

	lsa->data.asext.metric = htonl(/* LSA_ASEXT_E_FLAG | */ 100);
	/* XXX until now there is no metric */
	lsa->data.asext.ext_tag = 0;

	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
}

struct lsa *
orig_sum_lsa(struct rt_node *rte, u_int8_t type)
{
	struct lsa	*lsa;
	size_t		 len;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_sum);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_sum_lsa");

	/* LSA header */
	lsa->hdr.age = htons(rte->invalid ? MAX_AGE : DEFAULT_AGE);
	lsa->hdr.opts = rdeconf->options;	/* XXX not updated */
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
	if (type == LSA_TYPE_SUM_NETWORK) {
		lsa->hdr.ls_id = rte->prefix.s_addr;
		lsa->data.sum.mask = prefixlen2mask(rte->prefixlen);
	} else {
		lsa->hdr.ls_id = rte->adv_rtr.s_addr;
		lsa->data.sum.mask = 0;	/* must be zero per RFC */
	}

	lsa->data.sum.metric = htonl(rte->cost & LSA_METRIC_MASK);

	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
}

