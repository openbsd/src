/*	$OpenBSD: ldpe.c,v 1.27 2015/02/10 01:03:54 claudio Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2008 Esben Norby <norby@openbsd.org>
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
#include <net/if_types.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <event.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>

#include "ldp.h"
#include "ldpd.h"
#include "ldpe.h"
#include "lde.h"
#include "control.h"
#include "log.h"

extern struct nbr_id_head	nbrs_by_id;
RB_PROTOTYPE(nbr_id_head, nbr, id_tree, nbr_id_compare)

void	 ldpe_sig_handler(int, short, void *);
void	 ldpe_shutdown(void);

struct ldpd_conf	*leconf = NULL, *nconf;
struct imsgev		*iev_main;
struct imsgev		*iev_lde;

/* ARGSUSED */
void
ldpe_sig_handler(int sig, short event, void *bula)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		ldpe_shutdown();
		/* NOTREACHED */
	default:
		fatalx("unexpected signal");
	}
}

/* label distribution protocol engine */
pid_t
ldpe(struct ldpd_conf *xconf, int pipe_parent2ldpe[2], int pipe_ldpe2lde[2],
    int pipe_parent2lde[2])
{
	struct iface		*iface;
	struct tnbr		*tnbr;
	struct passwd		*pw;
	struct event		 ev_sigint, ev_sigterm;
	struct sockaddr_in	 disc_addr, sess_addr;
	pid_t			 pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	leconf = xconf;

	setproctitle("ldp engine");
	ldpd_process = PROC_LDP_ENGINE;

	/* create ldpd control socket outside chroot */
	if (control_init() == -1)
		fatalx("control socket setup failed");

	/* create the discovery UDP socket */
	disc_addr.sin_family = AF_INET;
	disc_addr.sin_port = htons(LDP_PORT);
	disc_addr.sin_addr.s_addr = INADDR_ANY;

	if ((xconf->ldp_discovery_socket = socket(AF_INET,
	    SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    IPPROTO_UDP)) == -1)
		fatal("error creating discovery socket");

	if (if_set_reuse(xconf->ldp_discovery_socket, 1) == -1)
		fatal("if_set_reuse");

	if (bind(xconf->ldp_discovery_socket, (struct sockaddr *)&disc_addr,
	    sizeof(disc_addr)) == -1)
		fatal("error binding discovery socket");

	/* set some defaults */
	if (if_set_mcast_ttl(xconf->ldp_discovery_socket,
	    IP_DEFAULT_MULTICAST_TTL) == -1)
		fatal("if_set_mcast_ttl");
	if (if_set_mcast_loop(xconf->ldp_discovery_socket) == -1)
		fatal("if_set_mcast_loop");
	if (if_set_tos(xconf->ldp_discovery_socket,
	    IPTOS_PREC_INTERNETCONTROL) == -1)
		fatal("if_set_tos");
	if (if_set_recvif(xconf->ldp_discovery_socket, 1) == -1)
		fatal("if_set_recvif");
	if_set_recvbuf(xconf->ldp_discovery_socket);

	/* create the extended discovery UDP socket */
	disc_addr.sin_family = AF_INET;
	disc_addr.sin_port = htons(LDP_PORT);
	disc_addr.sin_addr.s_addr = xconf->rtr_id.s_addr;

	if ((xconf->ldp_ediscovery_socket = socket(AF_INET,
	    SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    IPPROTO_UDP)) == -1)
		fatal("error creating extended discovery socket");

	if (if_set_reuse(xconf->ldp_ediscovery_socket, 1) == -1)
		fatal("if_set_reuse");

	if (bind(xconf->ldp_ediscovery_socket, (struct sockaddr *)&disc_addr,
	    sizeof(disc_addr)) == -1)
		fatal("error binding extended discovery socket");

	/* set some defaults */
	if (if_set_tos(xconf->ldp_ediscovery_socket,
	    IPTOS_PREC_INTERNETCONTROL) == -1)
		fatal("if_set_tos");
	if (if_set_recvif(xconf->ldp_ediscovery_socket, 1) == -1)
		fatal("if_set_recvif");
	if_set_recvbuf(xconf->ldp_ediscovery_socket);

	/* create the session TCP socket */
	sess_addr.sin_family = AF_INET;
	sess_addr.sin_port = htons(LDP_PORT);
	sess_addr.sin_addr.s_addr = INADDR_ANY;

	if ((xconf->ldp_session_socket = socket(AF_INET,
	    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    IPPROTO_TCP)) == -1)
		fatal("error creating session socket");

	if (if_set_reuse(xconf->ldp_session_socket, 1) == -1)
		fatal("if_set_reuse");

	if (bind(xconf->ldp_session_socket, (struct sockaddr *)&sess_addr,
	    sizeof(sess_addr)) == -1)
		fatal("error binding session socket");

	if (listen(xconf->ldp_session_socket, LDP_BACKLOG) == -1)
		fatal("error in listen on session socket");

	/* set some defaults */
	if (if_set_tos(xconf->ldp_session_socket,
	    IPTOS_PREC_INTERNETCONTROL) == -1)
		fatal("if_set_tos");

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
	accept_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, ldpe_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, ldpe_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_parent2ldpe[0]);
	close(pipe_ldpe2lde[1]);
	close(pipe_parent2lde[0]);
	close(pipe_parent2lde[1]);

	if ((iev_lde = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_lde->ibuf, pipe_ldpe2lde[0]);
	iev_lde->handler = ldpe_dispatch_lde;
	imsg_init(&iev_main->ibuf, pipe_parent2ldpe[1]);
	iev_main->handler = ldpe_dispatch_main;

	/* setup event handler */
	iev_lde->events = EV_READ;
	event_set(&iev_lde->ev, iev_lde->ibuf.fd, iev_lde->events,
	    iev_lde->handler, iev_lde);
	event_add(&iev_lde->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	event_set(&leconf->disc_ev, leconf->ldp_discovery_socket,
	    EV_READ|EV_PERSIST, disc_recv_packet, NULL);
	event_add(&leconf->disc_ev, NULL);

	event_set(&leconf->edisc_ev, leconf->ldp_ediscovery_socket,
	    EV_READ|EV_PERSIST, disc_recv_packet, NULL);
	event_add(&leconf->edisc_ev, NULL);

	accept_add(leconf->ldp_session_socket, session_accept, NULL);
	/* listen on ldpd control socket */
	TAILQ_INIT(&ctl_conns);
	control_listen();

	if ((pkt_ptr = calloc(1, IBUF_READ_SIZE)) == NULL)
		fatal("ldpe");

	/* initialize interfaces */
	LIST_FOREACH(iface, &leconf->iface_list, entry)
		if_init(xconf, iface);

	/* start configured targeted neighbors */
	LIST_FOREACH(tnbr, &leconf->tnbr_list, entry)
		tnbr_init(xconf, tnbr);

	event_dispatch();

	ldpe_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ldpe_shutdown(void)
{
	struct iface	*iface;
	struct tnbr	*tnbr;

	/* stop all interfaces */
	while ((iface = LIST_FIRST(&leconf->iface_list)) != NULL) {
		if (if_fsm(iface, IF_EVT_DOWN)) {
			log_debug("error stopping interface %s",
			    iface->name);
		}
		LIST_REMOVE(iface, entry);
		if_del(iface);
	}

	/* stop all targeted neighbors */
	while ((tnbr = LIST_FIRST(&leconf->tnbr_list)) != NULL) {
		LIST_REMOVE(tnbr, entry);
		tnbr_del(tnbr);
	}

	close(leconf->ldp_discovery_socket);
	close(leconf->ldp_session_socket);

	/* clean up */
	msgbuf_write(&iev_lde->ibuf.w);
	msgbuf_clear(&iev_lde->ibuf.w);
	free(iev_lde);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	free(iev_main);
	free(leconf);
	free(pkt_ptr);

	log_info("ldp engine exiting");
	_exit(0);
}

/* imesg */
int
ldpe_imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
ldpe_imsg_compose_lde(int type, u_int32_t peerid, pid_t pid,
    void *data, u_int16_t datalen)
{
	return (imsg_compose_event(iev_lde, type, peerid, pid, -1,
	    data, datalen));
}

/* ARGSUSED */
void
ldpe_dispatch_main(int fd, short event, void *bula)
{
	struct imsg	 imsg;
	struct imsgev	*iev = bula;
	struct imsgbuf  *ibuf = &iev->ibuf;
	struct iface	*iface = NULL;
	struct if_addr	*if_addr = NULL, *a;
	struct kif	*kif;
	struct kaddr	*kaddr;
	int		 n, shut = 0;
	struct nbr	*nbr;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("ldpe_dispatch_main: msgbuf_write");
		if (n == 0)
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldpe_dispatch_main: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_IFSTATUS:
		case IMSG_IFUP:
		case IMSG_IFDOWN:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kif))
				fatalx("IFINFO imsg with wrong len");

			kif = imsg.data;
			iface = if_lookup(kif->ifindex);
			if (!iface)
				break;

			iface->flags = kif->flags;
			iface->linkstate = kif->link_state;
			switch (imsg.hdr.type) {
			case IMSG_IFUP:
				if_fsm(iface, IF_EVT_UP);
				break;
			case IMSG_IFDOWN:
				if_fsm(iface, IF_EVT_DOWN);
				break;
			default:
				break;
			}
			break;
		case IMSG_NEWADDR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kaddr))
				fatalx("NEWADDR imsg with wrong len");
			kaddr = imsg.data;

			if ((if_addr = calloc(1, sizeof(*if_addr))) == NULL)
				fatal("ldpe_dispatch_main");

			if_addr->addr.s_addr = kaddr->addr.s_addr;
			if_addr->mask.s_addr = kaddr->mask.s_addr;
			if_addr->dstbrd.s_addr = kaddr->dstbrd.s_addr;

			LIST_INSERT_HEAD(&leconf->addr_list, if_addr,
			    global_entry);
			RB_FOREACH(nbr, nbr_id_head, &nbrs_by_id) {
				if (nbr->state != NBR_STA_OPER)
					continue;
				send_address(nbr, if_addr);
			}

			iface = if_lookup(kaddr->ifindex);
			if (iface) {
				LIST_INSERT_HEAD(&iface->addr_list, if_addr,
				    iface_entry);
				if_fsm(iface, IF_EVT_NEWADDR);
			}
			break;
		case IMSG_DELADDR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kaddr))
				fatalx("DELADDR imsg with wrong len");
			kaddr = imsg.data;

			LIST_FOREACH(a, &leconf->addr_list, global_entry)
				if (a->addr.s_addr == kaddr->addr.s_addr &&
				    a->mask.s_addr == kaddr->mask.s_addr &&
				    a->dstbrd.s_addr == kaddr->dstbrd.s_addr)
					break;
			if_addr = a;
			if (!if_addr)
				break;

			LIST_REMOVE(if_addr, global_entry);
			RB_FOREACH(nbr, nbr_id_head, &nbrs_by_id) {
				if (nbr->state != NBR_STA_OPER)
					continue;
				send_address_withdraw(nbr, if_addr);
			}

			iface = if_lookup(kaddr->ifindex);
			if (iface) {
				LIST_REMOVE(if_addr, iface_entry);
				if_fsm(iface, IF_EVT_DELADDR);
			}
			free(if_addr);
			break;
		case IMSG_RECONF_CONF:
			break;
		case IMSG_RECONF_IFACE:
			break;
		case IMSG_RECONF_END:
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_IFINFO:
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
		default:
			log_debug("ldpe_dispatch_main: error handling imsg %d",
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
ldpe_dispatch_lde(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct map		 map;
	struct notify_msg	 nm;
	int			 n, shut = 0;
	struct nbr		*nbr = NULL;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("ldpe_dispatch_lde: msgbuf_write");
		if (n == 0)
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldpe_dispatch_lde: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MAPPING_ADD:
		case IMSG_RELEASE_ADD:
		case IMSG_REQUEST_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of map request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}
			if (nbr->state != NBR_STA_OPER)
				return;

			switch (imsg.hdr.type) {
			case IMSG_MAPPING_ADD:
				nbr_mapping_add(nbr, &nbr->mapping_list, &map);
				break;
			case IMSG_RELEASE_ADD:
				nbr_mapping_add(nbr, &nbr->release_list, &map);
				break;
			case IMSG_REQUEST_ADD:
				nbr_mapping_add(nbr, &nbr->request_list, &map);
				break;
			}
			break;
		case IMSG_MAPPING_ADD_END:
		case IMSG_RELEASE_ADD_END:
		case IMSG_REQUEST_ADD_END:
			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}
			if (nbr->state != NBR_STA_OPER)
				return;

			switch (imsg.hdr.type) {
			case IMSG_MAPPING_ADD_END:
				send_labelmessage(nbr, MSG_TYPE_LABELMAPPING,
				    &nbr->mapping_list);
				break;
			case IMSG_RELEASE_ADD_END:
				send_labelmessage(nbr, MSG_TYPE_LABELRELEASE,
				    &nbr->release_list);
				break;
			case IMSG_REQUEST_ADD_END:
				send_labelmessage(nbr, MSG_TYPE_LABELREQUEST,
				    &nbr->request_list);
				break;
			}
			break;
		case IMSG_NOTIFICATION_SEND:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(nm))
				fatalx("invalid size of OE request");
			memcpy(&nm, imsg.data, sizeof(nm));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}
			if (nbr->state != NBR_STA_OPER)
				return;

			send_notification_nbr(nbr, nm.status,
			    htonl(nm.messageid), htonl(nm.type));
			break;
		case IMSG_CTL_END:
		case IMSG_CTL_SHOW_LIB:
			control_imsg_relay(&imsg);
			break;
		default:
			log_debug("ldpe_dispatch_lde: error handling imsg %d",
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
ldpe_router_id(void)
{
	return (leconf->rtr_id.s_addr);
}

void
ldpe_iface_ctl(struct ctl_conn *c, unsigned int idx)
{
	struct iface		*iface;
	struct ctl_iface	*ictl;

	LIST_FOREACH(iface, &leconf->iface_list, entry) {
		if (idx == 0 || idx == iface->ifindex) {
			ictl = if_to_ctl(iface);
			imsg_compose_event(&c->iev,
			     IMSG_CTL_SHOW_INTERFACE,
			    0, 0, -1, ictl, sizeof(struct ctl_iface));
		}
	}
}
