/*	$OpenBSD: rde.c,v 1.7 2009/01/19 20:40:31 michele Exp $ */

/*
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005, 2006 Esben Norby <norby@openbsd.org>
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

#include "igmp.h"
#include "dvmrp.h"
#include "dvmrpd.h"
#include "dvmrpe.h"
#include "log.h"
#include "rde.h"

void		 rde_ds_iface(struct mfc *, struct iface *, struct rt_node *);

void		 rde_nbr_init(u_int32_t);
void		 rde_nbr_free(void);
struct rde_nbr	*rde_nbr_find(u_int32_t);
struct rde_nbr	*rde_nbr_new(u_int32_t, struct rde_nbr *);
void		 rde_nbr_del(struct rde_nbr *);

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);

volatile sig_atomic_t	 rde_quit = 0;
struct dvmrpd_conf	*rdeconf = NULL;
struct rde_nbr		*nbrself;
struct imsgbuf		*ibuf_dvmrpe;
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
	}
}

/* route decision engine */
pid_t
rde(struct dvmrpd_conf *xconf, int pipe_parent2rde[2], int pipe_dvmrpe2rde[2],
    int pipe_parent2dvmrpe[2])
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

	if ((pw = getpwnam(DVMRPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	dvmrpd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	endpwent();

	event_init();
	rde_nbr_init(NBR_HASHSIZE);

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);

	/* setup pipes */
	close(pipe_dvmrpe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2dvmrpe[0]);
	close(pipe_parent2dvmrpe[1]);

	if ((ibuf_dvmrpe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_dvmrpe, pipe_dvmrpe2rde[1], rde_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_imsg);

	/* setup event handler */
	ibuf_dvmrpe->events = EV_READ;
	event_set(&ibuf_dvmrpe->ev, ibuf_dvmrpe->fd, ibuf_dvmrpe->events,
	    ibuf_dvmrpe->handler, ibuf_dvmrpe);
	event_add(&ibuf_dvmrpe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	rt_init();
	mfc_init();

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	struct iface	*iface;

	rt_clear();
	mfc_clear();

	LIST_FOREACH(iface, &rdeconf->iface_list, entry) {
		if_del(iface);
	}

	msgbuf_clear(&ibuf_dvmrpe->w);
	free(ibuf_dvmrpe);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);
	free(rdeconf);

	log_info("route decision engine exiting");
	_exit(0);
}

/* imesg */
int
rde_imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose(ibuf_main, type, 0, pid, data, datalen));
}

int
rde_imsg_compose_dvmrpe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_dvmrpe, type, peerid, pid, data, datalen));
}

void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct mfc		 mfc;
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct route_report	 rr;
	struct rde_nbr		 rn;
	struct rt_node		*r;
	int			 i, n, connected = 0;
	struct iface		*iface;

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
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_RIB:
			rt_dump(imsg.hdr.pid);
			imsg_compose(ibuf_dvmrpe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_CTL_SHOW_MFC:
			mfc_dump(imsg.hdr.pid);
			imsg_compose(ibuf_dvmrpe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_ROUTE_REPORT:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rr))
				fatalx("invalid size of OE request");
			memcpy(&rr, imsg.data, sizeof(rr));

			/* directly connected networks from parent */
			if (imsg.hdr.peerid == 0)
				connected = 1;

			if (srt_check_route(&rr, connected) == -1)
				log_debug("rde_dispatch_imsg: "
				    "packet malformed");
			break;
		case IMSG_FULL_ROUTE_REPORT:
			rt_snap(imsg.hdr.peerid);
			rde_imsg_compose_dvmrpe(IMSG_FULL_ROUTE_REPORT_END,
			    imsg.hdr.peerid, 0, NULL, 0);
			break;
		case IMSG_MFC_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of OE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));
#if 1
			for (i = 0; i < MAXVIFS; i++)
				mfc.ttls[i] = 0;

			r = rt_matchorigin(mfc.origin.s_addr);
			if (r == NULL) {
				log_debug("rde_dispatch_imsg: "
				    "packet from unknown origin");
				break;
			}

			LIST_FOREACH(iface, &rdeconf->iface_list, entry)
				rde_ds_iface(&mfc, iface, r);

			mfc_update(&mfc);
#endif
			break;
		case IMSG_MFC_DEL:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of OE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));
#if 1
			mfc_delete(&mfc);
#endif
			break;
		case IMSG_NEIGHBOR_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rn))
				fatalx("invalid size of OE request"); 
			memcpy(&rn, imsg.data, sizeof(rn));

			if (rde_nbr_find(imsg.hdr.peerid));
				fatalx("rde_rispatch_imsg: "
				    "neighbor already exists");
			rde_nbr_new(imsg.hdr.peerid, &rn);
			break;
		case IMSG_NEIGHBOR_DOWN:
			rde_nbr_del(rde_nbr_find(imsg.hdr.peerid));
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

/* 1) Add interfaces with downstream routers for this
      source.
   2) Add interfaces with member for this group if i am
      the designated forwarder.
*/
void
rde_ds_iface(struct mfc *mfc, struct iface *iface, struct rt_node *r)
{
	if (mfc->ifindex == iface->ifindex) {
		return;
	}

	if (r->ds_cnt[iface->ifindex] != 0) {
		mfc->ttls[iface->ifindex] = 1;
		return;
	}

	if (group_list_find(iface, mfc->group.s_addr) &&
	    r->adv_rtr[iface->ifindex].addr.s_addr == iface->addr.s_addr) {
		mfc->ttls[iface->ifindex] = 1;
		return;
	}
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

	nbrself->peerid = NBR_IDSELF;
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

	if (rde_nbr_find(peerid))
		return (NULL);

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("rde_nbr_new");

	memcpy(nbr, new, sizeof(*nbr));
	nbr->peerid = peerid;

	head = RDE_NBR_HASH(peerid);
	LIST_INSERT_HEAD(head, nbr, hash);

	return (nbr);
}

void
rde_nbr_del(struct rde_nbr *nbr)
{
	if (nbr == NULL)
		return;

	srt_expire_nbr(nbr->addr, nbr->iface);

	LIST_REMOVE(nbr, entry);
	LIST_REMOVE(nbr, hash);

	free(nbr);
}
