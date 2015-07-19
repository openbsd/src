/*	$OpenBSD: lde.c,v 1.32 2015/07/19 20:50:03 renato Exp $ */

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
#include <netmpls/mpls.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <event.h>

#include "ldp.h"
#include "ldpd.h"
#include "ldpe.h"
#include "log.h"
#include "lde.h"

void		 lde_sig_handler(int sig, short, void *);
void		 lde_shutdown(void);
void		 lde_dispatch_imsg(int, short, void *);
void		 lde_dispatch_parent(int, short, void *);

struct lde_nbr	*lde_nbr_find(u_int32_t);
struct lde_nbr	*lde_nbr_new(u_int32_t, struct in_addr *);
void		 lde_nbr_del(struct lde_nbr *);
void		 lde_nbr_clear(void);

void		 lde_map_free(void *);
void		 lde_address_list_free(struct lde_nbr *);

struct ldpd_conf	*ldeconf = NULL, *nconf = NULL;
struct imsgev		*iev_ldpe;
struct imsgev		*iev_main;

/* ARGSUSED */
void
lde_sig_handler(int sig, short event, void *arg)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		lde_shutdown();
		/* NOTREACHED */
	default:
		fatalx("unexpected signal");
	}
}

/* label decision engine */
pid_t
lde(struct ldpd_conf *xconf, int pipe_parent2lde[2], int pipe_ldpe2lde[2],
    int pipe_parent2ldpe[2])
{
	struct event		 ev_sigint, ev_sigterm;
	struct timeval		 now;
	struct passwd		*pw;
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

	ldeconf = xconf;

	setproctitle("label decision engine");
	ldpd_process = PROC_LDE_ENGINE;

	if ((pw = getpwnam(LDPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, lde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, lde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_ldpe2lde[0]);
	close(pipe_parent2lde[0]);
	close(pipe_parent2ldpe[0]);
	close(pipe_parent2ldpe[1]);

	if ((iev_ldpe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_ldpe->ibuf, pipe_ldpe2lde[1]);
	iev_ldpe->handler = lde_dispatch_imsg;
	imsg_init(&iev_main->ibuf, pipe_parent2lde[1]);
	iev_main->handler = lde_dispatch_parent;

	/* setup event handler */
	iev_ldpe->events = EV_READ;
	event_set(&iev_ldpe->ev, iev_ldpe->ibuf.fd, iev_ldpe->events,
	    iev_ldpe->handler, iev_ldpe);
	event_add(&iev_ldpe->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	gettimeofday(&now, NULL);
	ldeconf->uptime = now.tv_sec;

	event_dispatch();

	lde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
lde_shutdown(void)
{
	lde_nbr_clear();
	rt_clear();

	msgbuf_clear(&iev_ldpe->ibuf.w);
	free(iev_ldpe);
	msgbuf_clear(&iev_main->ibuf.w);
	free(iev_main);
	free(ldeconf);

	log_info("label decision engine exiting");
	_exit(0);
}

/* imesg */
int
lde_imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
lde_imsg_compose_ldpe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose_event(iev_ldpe, type, peerid, pid,
	     -1, data, datalen));
}

/* ARGSUSED */
void
lde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct lde_nbr		 *nbr;
	struct map		 map;
	struct in_addr		 addr;
	ssize_t			 n;
	int			 shut = 0, verbose;

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
			fatal("lde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LABEL_MAPPING_FULL:
			nbr = lde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("lde_dispatch_imsg: cannot find "
				    "lde neighbor");
				return;
			}

			rt_snap(nbr);
			lde_imsg_compose_ldpe(IMSG_MAPPING_ADD_END,
			    imsg.hdr.peerid, 0, NULL, 0);
			break;
		case IMSG_LABEL_MAPPING:
		case IMSG_LABEL_REQUEST:
		case IMSG_LABEL_RELEASE:
		case IMSG_LABEL_WITHDRAW:
		case IMSG_LABEL_ABORT:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of OE request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = lde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("lde_dispatch_imsg: cannot find "
				    "lde neighbor");
				return;
			}

			switch (imsg.hdr.type) {
			case IMSG_LABEL_MAPPING:
				lde_check_mapping(&map, nbr);
				break;
			case IMSG_LABEL_REQUEST:
				lde_check_request(&map, nbr);
				break;
			case IMSG_LABEL_RELEASE:
				lde_check_release(&map, nbr);
				break;
			case IMSG_LABEL_WITHDRAW:
				lde_check_withdraw(&map, nbr);
				break;
			default:
				log_warnx("type %d not yet handled. nbr %s",
				    imsg.hdr.type, inet_ntoa(nbr->id));
			}
			break;
		case IMSG_ADDRESS_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(addr))
				fatalx("invalid size of OE request");
			memcpy(&addr, imsg.data, sizeof(addr));

			nbr = lde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("lde_dispatch_imsg: cannot find "
				    "lde neighbor");
				return;
			}

			if (lde_address_add(nbr, &addr) < 0) {
				log_debug("lde_dispatch_imsg: cannot add "
				    "address %s, it already exists",
				    inet_ntoa(addr));
			}

			break;
		case IMSG_ADDRESS_DEL:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(addr))
				fatalx("invalid size of OE request");
			memcpy(&addr, imsg.data, sizeof(addr));

			nbr = lde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("lde_dispatch_imsg: cannot find "
				    "lde neighbor");
				return;
			}

			if (lde_address_del(nbr, &addr) < 0) {
				log_debug("lde_dispatch_imsg: cannot delete "
				    "address %s, it does not exists",
				    inet_ntoa(addr));
			}

			break;
		case IMSG_NEIGHBOR_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(addr))
				fatalx("invalid size of OE request");
			memcpy(&addr, imsg.data, sizeof(addr));

			if (lde_nbr_find(imsg.hdr.peerid))
				fatalx("lde_dispatch_imsg: "
				    "neighbor already exists");
			lde_nbr_new(imsg.hdr.peerid, &addr);
			break;
		case IMSG_NEIGHBOR_DOWN:
			lde_nbr_del(lde_nbr_find(imsg.hdr.peerid));
			break;
		case IMSG_CTL_SHOW_LIB:
			rt_dump(imsg.hdr.pid);

			lde_imsg_compose_ldpe(IMSG_CTL_END, 0,
			    imsg.hdr.pid, NULL, 0);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by ldpe */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("lde_dispatch_imsg: unexpected imsg %d",
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
lde_dispatch_parent(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct kroute		 kr;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	ssize_t			 n;
	int			 shut = 0;

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
			fatal("lde_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("lde_dispatch_parent: "
				    "wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			lde_kernel_insert(&kr);
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("lde_dispatch_parent: "
				    "wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			lde_kernel_remove(&kr);
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct ldpd_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct ldpd_conf));

			break;
		case IMSG_RECONF_IFACE:
			break;
		case IMSG_RECONF_END:
			break;
		default:
			log_debug("lde_dispatch_parent: unexpected imsg %d",
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

u_int32_t
lde_assign_label(void)
{
	static u_int32_t label = MPLS_LABEL_RESERVED_MAX;

	/* XXX some checks needed */
	label++;
	return label;
}

void
lde_send_change_klabel(struct rt_node *rr, struct rt_lsp *rl)
{
	struct kroute	kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = rr->fec.prefix.s_addr;
	kr.prefixlen = rr->fec.prefixlen;
	kr.local_label = rr->local_label;

	kr.nexthop.s_addr = rl->nexthop.s_addr;
	kr.remote_label = rl->remote_label;

	lde_imsg_compose_parent(IMSG_KLABEL_CHANGE, 0, &kr, sizeof(kr));
}

void
lde_send_delete_klabel(struct rt_node *rr, struct rt_lsp *rl)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = rr->fec.prefix.s_addr;
	kr.prefixlen = rr->fec.prefixlen;
	kr.local_label = rr->local_label;

	kr.nexthop.s_addr = rl->nexthop.s_addr;
	kr.remote_label = rl->remote_label;

	lde_imsg_compose_parent(IMSG_KLABEL_DELETE, 0, &kr, sizeof(kr));
}

void
lde_send_labelrequest(struct lde_nbr *ln, struct rt_node *rn)
{
	struct map	 map;

	/* TODO check if status of peer is OK to send requests (SLRq.2 & 6)
	 * For now assume no peer will send no-label-resource notifications */

	/* check if request is already pending */
	if (fec_find(&ln->sent_req, &rn->fec) != NULL)
		return;
	/* and try to add request to pending list */
	lde_req_add(ln, &rn->fec, 1);
	/* msgid does not matter since only one req can be pending */

	bzero(&map, sizeof(map));
	map.prefix = rn->fec.prefix;
	map.prefixlen = rn->fec.prefixlen;

	lde_imsg_compose_ldpe(IMSG_REQUEST_ADD, ln->peerid, 0,
	    &map, sizeof(map));
	lde_imsg_compose_ldpe(IMSG_REQUEST_ADD_END, ln->peerid, 0,
	    NULL, 0);
}

void
lde_send_labelmapping(struct lde_nbr *ln, struct rt_node *rn)
{
	struct lde_req	*lre;
	struct lde_map	*me;
	struct map	 map;

	/*
	 * This function skips SL.1 - 3 and SL.9 - 14 because the lable
	 * allocation is done way earlier (because of the merging nature of
	 * ldpd).
	 */

	bzero(&map, sizeof(map));
	map.label = rn->local_label;
	map.prefix = rn->fec.prefix;
	map.prefixlen = rn->fec.prefixlen;

	/* is there a pending request for this mapping? */
	lre = (struct lde_req *)fec_find(&ln->recv_req, &rn->fec);
	if (lre) {
		/* set label request msg id in the mapping response. */
		map.requestid = lre->msgid;
		map.flags = F_MAP_REQ_ID;
		lde_req_del(ln, lre, 0);
	}

	me = (struct lde_map *)fec_find(&ln->sent_map, &rn->fec);
	if (me == NULL)
		me = lde_map_add(ln, rn, 1);
	me->label = map.label;

	lde_imsg_compose_ldpe(IMSG_MAPPING_ADD, ln->peerid, 0,
	    &map, sizeof(map));
	lde_imsg_compose_ldpe(IMSG_MAPPING_ADD_END, ln->peerid, 0,
	    NULL, 0);
}

void
lde_send_labelrelease(struct lde_nbr *ln, struct rt_node *rn, u_int32_t label)
{
	struct map	 map;

	bzero(&map, sizeof(map));
	map.prefix = rn->fec.prefix;
	map.prefixlen = rn->fec.prefixlen;
	if (label != NO_LABEL) {
		map.flags = F_MAP_OPTLABEL;
		map.label = label;
	}

	lde_imsg_compose_ldpe(IMSG_RELEASE_ADD, ln->peerid, 0,
	    &map, sizeof(map));
	lde_imsg_compose_ldpe(IMSG_RELEASE_ADD_END, ln->peerid, 0,
	    NULL, 0);
}

void
lde_send_notification(u_int32_t peerid, u_int32_t code, u_int32_t msgid,
    u_int32_t type)
{
	struct notify_msg nm;

	bzero(&nm, sizeof(nm));

	/* Every field is in host byte order, to keep things clear */
	nm.status = code;
	nm.messageid = ntohl(msgid);
	nm.type = type;

	lde_imsg_compose_ldpe(IMSG_NOTIFICATION_SEND, peerid, 0,
	    &nm, sizeof(nm));
}

static __inline int lde_nbr_compare(struct lde_nbr *, struct lde_nbr *);

RB_HEAD(nbr_tree, lde_nbr);
RB_PROTOTYPE(nbr_tree, lde_nbr, entry, lde_nbr_compare)
RB_GENERATE(nbr_tree, lde_nbr, entry, lde_nbr_compare)

struct nbr_tree lde_nbrs = RB_INITIALIZER(&lde_nbrs);

static __inline int
lde_nbr_compare(struct lde_nbr *a, struct lde_nbr *b)
{
	return (a->peerid - b->peerid);
}

struct lde_nbr *
lde_nbr_find(u_int32_t peerid)
{
	struct lde_nbr	n;

	n.peerid = peerid;

	return (RB_FIND(nbr_tree, &lde_nbrs, &n));
}

struct lde_nbr *
lde_nbr_new(u_int32_t peerid, struct in_addr *id)
{
	struct lde_nbr	*nbr;

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("lde_nbr_new");

	nbr->id.s_addr = id->s_addr;
	nbr->peerid = peerid;
	fec_init(&nbr->recv_map);
	fec_init(&nbr->sent_map);
	fec_init(&nbr->recv_req);
	fec_init(&nbr->sent_req);
	fec_init(&nbr->sent_wdraw);

	TAILQ_INIT(&nbr->addr_list);

	if (RB_INSERT(nbr_tree, &lde_nbrs, nbr) != NULL)
		fatalx("lde_nbr_new: RB_INSERT failed");

	return (nbr);
}

void
lde_nbr_del(struct lde_nbr *nbr)
{
	struct fec	*f;
	struct rt_node	*rn;
	struct rt_lsp	*rl;

	if (nbr == NULL)
		return;

	/* uninstall received mappings */
	RB_FOREACH(f, fec_tree, &rt) {
		rn = (struct rt_node *)f;

		LIST_FOREACH(rl, &rn->lsp, entry) {
			if (lde_address_find(nbr, &rl->nexthop)) {
				lde_send_delete_klabel(rn, rl);
				rl->remote_label = NO_LABEL;
			}
		}
	}

	lde_address_list_free(nbr);

	fec_clear(&nbr->recv_map, lde_map_free);
	fec_clear(&nbr->sent_map, lde_map_free);
	fec_clear(&nbr->recv_req, free);
	fec_clear(&nbr->sent_req, free);
	fec_clear(&nbr->sent_wdraw, free);

	RB_REMOVE(nbr_tree, &lde_nbrs, nbr);

	free(nbr);
}

void
lde_nbr_clear(void)
{
	struct lde_nbr	*nbr;

	 while ((nbr = RB_ROOT(&lde_nbrs)) != NULL)
		lde_nbr_del(nbr);
}

void
lde_nbr_do_mappings(struct rt_node *rn)
{
	struct lde_nbr	*ln;
	struct lde_map	*me;
	struct lde_req	*lre;

	/* This handles LMp.17-31 for lde_check_mapping() */

	RB_FOREACH(ln, nbr_tree, &lde_nbrs) {
		/* LMp.18 Did we already send a mapping to this peer? */
		me = (struct lde_map *)fec_find(&ln->sent_map, &rn->fec);
		if (me && me->label == rn->local_label)
			/* same mapping already sent, skip */
			/* TODO LMp.22-27 Loop detection check */
			continue;

		/* LMp.28 Is this from a pending request? */
		lre = (struct lde_req *)fec_find(&ln->recv_req, &rn->fec);

		lde_send_labelmapping(ln, rn);
		/* LMp.30 & 31 are not needed because labels are always added */
	}
}

struct lde_map *
lde_map_add(struct lde_nbr *ln, struct rt_node *rn, int sent)
{
	struct lde_map  *me;

	me = calloc(1, sizeof(*me));
	if (me == NULL)
		fatal("lde_map_add");

	me->fec = rn->fec;
	me->nexthop = ln;

	if (sent) {
		LIST_INSERT_HEAD(&rn->upstream, me, entry);
		if (fec_insert(&ln->sent_map, &me->fec))
			log_warnx("failed to add %s/%u to sent map",
			    inet_ntoa(me->fec.prefix), me->fec.prefixlen);
			/* XXX on failure more cleanup is needed */
	} else {
		LIST_INSERT_HEAD(&rn->downstream, me, entry);
		if (fec_insert(&ln->recv_map, &me->fec))
			log_warnx("failed to add %s/%u to recv map",
			    inet_ntoa(me->fec.prefix), me->fec.prefixlen);
	}

	return (me);
}

void
lde_map_del(struct lde_nbr *ln, struct lde_map *me, int sent)
{
	if (sent)
		fec_remove(&ln->sent_map, &me->fec);
	else
		fec_remove(&ln->recv_map, &me->fec);

	lde_map_free(me);
}

void
lde_map_free(void *ptr)
{
	struct lde_map	*map = ptr;

	LIST_REMOVE(map, entry);
	free(map);
}

struct lde_req *
lde_req_add(struct lde_nbr *ln, struct fec *fec, int sent)
{
	struct fec_tree	*ft;
	struct lde_req	*lre;

	ft = sent ? &ln->sent_req : &ln->recv_req;

	lre = calloc(1, sizeof(*lre));
	if (lre != NULL) {
		lre->fec = *fec;

		if (fec_insert(ft, &lre->fec)) {
			log_warnx("failed to add %s/%u to %s req",
			    inet_ntoa(lre->fec.prefix), lre->fec.prefixlen,
			    sent ? "sent" : "recv");
			free(lre);
			return (NULL);
		}
	}

	return (lre);
}

void
lde_req_del(struct lde_nbr *ln, struct lde_req *lre, int sent)
{
	if (sent)
		fec_remove(&ln->sent_req, &lre->fec);
	else
		fec_remove(&ln->recv_req, &lre->fec);

	free(lre);
}

int
lde_address_add(struct lde_nbr *lr, struct in_addr *addr)
{
	struct lde_nbr_address	*address;

	if (lde_address_find(lr, addr) != NULL)
		return (-1);

	if ((address = calloc(1, sizeof(*address))) == NULL)
		fatal("lde_address_add");

	address->addr.s_addr = addr->s_addr;

	TAILQ_INSERT_TAIL(&lr->addr_list, address, entry);

	log_debug("lde_address_add: added %s", inet_ntoa(*addr));

	return (0);
}

struct lde_nbr_address *
lde_address_find(struct lde_nbr *lr, struct in_addr *addr)
{
	struct lde_nbr_address	*address = NULL;

	TAILQ_FOREACH(address, &lr->addr_list, entry) {
		if (address->addr.s_addr == addr->s_addr)
			return (address);
	}

	return (NULL);
}

int
lde_address_del(struct lde_nbr *lr, struct in_addr *addr)
{
	struct lde_nbr_address	*address;

	address = lde_address_find(lr, addr);
	if (address == NULL)
		return (-1);

	TAILQ_REMOVE(&lr->addr_list, address, entry);

	free(address);

	log_debug("lde_address_del: deleted %s", inet_ntoa(*addr));

	return (0);
}

void
lde_address_list_free(struct lde_nbr *nbr)
{
	struct lde_nbr_address	*addr;

	while ((addr = TAILQ_FIRST(&nbr->addr_list)) != NULL) {
		TAILQ_REMOVE(&nbr->addr_list, addr, entry);
		free(addr);
	}
}

struct lde_nbr *
lde_find_address(struct in_addr address)
{
	struct lde_nbr	*ln;

	RB_FOREACH(ln, nbr_tree, &lde_nbrs) {
		if (lde_address_find(ln, &address) != NULL)
			return (ln);
	}

	return (NULL);
}
