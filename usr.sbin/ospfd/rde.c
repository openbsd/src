/*	$OpenBSD: rde.c,v 1.6 2005/02/10 14:05:48 claudio Exp $ */

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

void		 rde_nbr_init(u_int32_t);
struct rde_nbr	*rde_nbr_find(u_int32_t);
struct rde_nbr	*rde_nbr_new(u_int32_t, struct rde_nbr *);
void		 rde_nbr_del(struct rde_nbr *);

volatile sig_atomic_t	 rde_quit = 0;
struct ospfd_conf	*rdeconf = NULL;
struct imsgbuf		*ibuf_ospfe;
struct imsgbuf		*ibuf_main;

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

	rdeconf = xconf;

	if ((pw = getpwnam(OSPFD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	ospfd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid)) {
		fatal("can't drop privileges");
	}

	endpwent();

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
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_imsg);

	/* setup event handler */
	ibuf_ospfe->events = EV_READ;
	event_set(&ibuf_ospfe->ev, ibuf_ospfe->fd, ibuf_ospfe->events,
	    ibuf_ospfe->handler, ibuf_ospfe);
	event_add(&ibuf_ospfe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{

	/* ... */

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
			break;
		case IMSG_DB_SNAPSHOT:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");
			log_debug("rde_dispatch_imsg: IMSG_DB_SNAPSHOT, "
			    "neighbor %s", inet_ntoa(nbr->id));

			lsa_snap(nbr->area, imsg.hdr.peerid);

			imsg_compose(ibuf_ospfe, IMSG_DB_END, imsg.hdr.peerid,
			    0, -1, NULL, 0);
			break;
		case IMSG_DD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");
			log_debug("rde_dispatch_imsg: IMSG_DD, "
			    "neighbor %s", inet_ntoa(nbr->id));

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

				if (lsa_newer(&lsa_hdr, db_hdr) > 0)
					/*
					 * only request LSA's that are
					 * newer or missing
					 */
					/* XXX add to local REQ list */
					imsg_compose(ibuf_ospfe, IMSG_DD,
					    imsg.hdr.peerid, 0, -1,
					    &lsa_hdr, sizeof(lsa_hdr));
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
			log_debug("rde_dispatch_imsg: IMSG_LS_REQ, "
			    "neighbor %s len %d", inet_ntoa(nbr->id),
			    imsg.hdr.len - IMSG_HEADER_SIZE);

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
			log_debug("rde_dispatch_imsg: IMSG_LS_UPD, "
			    "neighbor %s len %d", inet_ntoa(nbr->id),
			    imsg.hdr.len - IMSG_HEADER_SIZE);

			lsa = malloc(imsg.hdr.len - IMSG_HEADER_SIZE);
			if (lsa == NULL)
				fatal(NULL);
			memcpy(lsa, imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			if (!lsa_check(nbr, lsa,
			    imsg.hdr.len - IMSG_HEADER_SIZE))
				break;

			v = lsa_find(nbr->area, lsa->hdr.type, lsa->hdr.ls_id,
				    lsa->hdr.adv_rtr);
			if (v == NULL)
				db_hdr = NULL;
			else
				db_hdr = &v->lsa->hdr;

			if (nbr->self) {
				lsa_merge(nbr, lsa, v);
				break;
			}

			r = lsa_newer(&lsa->hdr, db_hdr);
			if (r > 0) {
				/* new LSA newer than DB */
				if (v && v->flooded &&
				    v->changed + MIN_LS_ARRIVAL >= now)
					break;
				if (!(self = lsa_self(nbr, lsa, v)))
					lsa_add(nbr, lsa);

				/* flood and perhaps ack LSA */
				imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
				    imsg.hdr.peerid, 0, -1,
				    lsa, ntohs(lsa->hdr.len));

				/* reflood self originated LSA */
				if (self && v)
					imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
					    v->nbr->peerid, 0, -1,
					    v->lsa, ntohs(v->lsa->hdr.len));
			/* TODO LSA on req list -> BadLSReq */
			} else if (r < 0) {
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
			}
			break;
		case IMSG_LS_MAXAGE:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				fatalx("rde_dispatch_imsg: "
				    "neighbor does not exist");
			log_debug("rde_dispatch_imsg: IMSG_LS_MAXAGE, "
			    "neighbor %s len %d", inet_ntoa(nbr->id),
			    imsg.hdr.len - IMSG_HEADER_SIZE);

			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct lsa_hdr))
				fatalx("invalid size of OE request");
			memcpy(&lsa_hdr, imsg.data, sizeof(lsa_hdr));

			if (rde_nbr_loading(nbr->area))
				break;

			lsa_del(nbr, &lsa_hdr);
			break;
		case IMSG_CTL_SHOW_DATABASE:
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
					lsa_dump(&area->lsa_tree, imsg.hdr.pid);
				}
				lsa_dump(&rdeconf->lsa_tree, imsg.hdr.pid);
			} else {
				memcpy(&aid, imsg.data, sizeof(aid));
				if ((area = area_find(rdeconf, aid)) != NULL) {
					imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
					    0, imsg.hdr.pid, -1, area,
					    sizeof(*area));
					lsa_dump(&area->lsa_tree, imsg.hdr.pid);
					if (!area->stub)
						lsa_dump(&rdeconf->lsa_tree,
						    imsg.hdr.pid);
				}
			}
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

u_int32_t
rde_router_id(void)
{
	return (rdeconf->rtr_id.s_addr);
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
	u_int32_t        hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	rdenbrtable.hashtbl = calloc(hs, sizeof(struct rde_nbr_head));
	if (rdenbrtable.hashtbl == NULL)
		fatal("rde_nbr_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&rdenbrtable.hashtbl[i]);

	rdenbrtable.hashmask = hs - 1;
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

