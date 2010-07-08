/*	$OpenBSD: ldpe.c,v 1.11 2010/07/08 09:41:05 claudio Exp $ */

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
#include <stdlib.h>

#include "ldp.h"
#include "ldpd.h"
#include "ldpe.h"
#include "lde.h"
#include "control.h"
#include "log.h"

void	 ldpe_sig_handler(int, short, void *);
void	 ldpe_shutdown(void);

void	 recv_packet(int, short, void *);

struct ldpd_conf	*leconf = NULL, *nconf;
struct imsgev		*iev_main;
struct imsgev		*iev_lde;
int			 oe_nofib;

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

	/* create ldpd control socket outside chroot */
	if (control_init() == -1)
		fatalx("control socket setup failed");

	/* create the discovery UDP socket */
	disc_addr.sin_family = AF_INET;
	disc_addr.sin_port = htons(LDP_PORT);
	disc_addr.sin_addr.s_addr = INADDR_ANY;

	if ((xconf->ldp_discovery_socket = socket(AF_INET, SOCK_DGRAM,
	    IPPROTO_UDP)) == -1)
		fatal("error creating discovery socket");

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

	/* create the session TCP socket */
	sess_addr.sin_family = AF_INET;
	sess_addr.sin_port = htons(LDP_PORT);
	sess_addr.sin_addr.s_addr = INADDR_ANY;

	if ((xconf->ldp_session_socket = socket(AF_INET, SOCK_STREAM,
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
	session_socket_blockmode(xconf->ldp_session_socket, BM_NONBLOCK);

	leconf = xconf;
	if (leconf->flags & LDPD_FLAG_NO_LFIB_UPDATE)
		oe_nofib = 1;

	if ((pw = getpwnam(LDPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("ldp engine");
	ldpd_process = PROC_LDP_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	event_init();
	nbr_init(NBR_HASHSIZE);

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
	    EV_READ|EV_PERSIST, disc_recv_packet, leconf);
	event_add(&leconf->disc_ev, NULL);

	event_set(&leconf->sess_ev, leconf->ldp_session_socket,
	    EV_READ|EV_PERSIST, session_accept, leconf);
	event_add(&leconf->sess_ev, NULL);

	/* listen on ldpd control socket */
	TAILQ_INIT(&ctl_conns);
	control_listen();

	if ((pkt_ptr = calloc(1, IBUF_READ_SIZE)) == NULL)
		fatal("ldpe");

	/* start interfaces */
	LIST_FOREACH(iface, &leconf->iface_list, entry) {
		if_init(xconf, iface);
		if (if_fsm(iface, IF_EVT_UP)) {
			log_debug("error starting interface %s",
			    iface->name);
		}
	}

	event_dispatch();

	ldpe_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ldpe_shutdown(void)
{
	struct iface	*iface;

	/* stop all interfaces */
	LIST_FOREACH(iface, &leconf->iface_list, entry) {
		if (if_fsm(iface, IF_EVT_DOWN)) {
			log_debug("error stopping interface %s",
			    iface->name);
		}
	}

	close(leconf->ldp_discovery_socket);

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
	struct kif	*kif;
	int		 n, link_new, link_old, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("ldpe_dispatch_main: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldpe_dispatch_main: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_IFINFO:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kif))
				fatalx("IFINFO imsg with wrong len");
			kif = imsg.data;
			link_new = (kif->flags & IFF_UP) &&
			    (LINK_STATE_IS_UP(kif->link_state) ||
			    (kif->link_state == LINK_STATE_UNKNOWN &&
			    kif->media_type != IFT_CARP));

			LIST_FOREACH(iface, &leconf->iface_list, entry) {
				if (kif->ifindex == iface->ifindex) {
					link_old = (iface->flags & IFF_UP) &&
					    (LINK_STATE_IS_UP(iface->linkstate)
					    || (iface->linkstate ==
					    LINK_STATE_UNKNOWN &&
					    iface->media_type != IFT_CARP));
					iface->flags = kif->flags;
					iface->linkstate = kif->link_state;

					if (link_new == link_old)
						continue;
					if (link_new) {
						if_fsm(iface, IF_EVT_UP);
						log_warnx("interface %s up",
						    iface->name);
						/* XXX: send address msg */
					} else {
						if_fsm(iface, IF_EVT_DOWN);
						log_warnx("interface %s down",
						    iface->name);
						/* XXX: send address withdraw
						   msg */
					}
				}
			}
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
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("ldpe_dispatch_lde: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldpe_dispatch_lde: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MAPPING_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of OE request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}

			nbr_mapping_add(nbr, &nbr->mapping_list, &map);
			break;
		case IMSG_MAPPING_ADD_END:
			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}

			send_labelmapping(nbr);
			break;
		case IMSG_RELEASE_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of OE request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}

			nbr_mapping_add(nbr, &nbr->release_list, &map);
			break;
		case IMSG_RELEASE_ADD_END:
			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}
			send_labelrelease(nbr);
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

			send_notification_nbr(nbr, nm.status,
			    htonl(nm.messageid), htonl(nm.type));
			break;
		case IMSG_REQUEST_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of OE request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}

			nbr_mapping_add(nbr, &nbr->request_list, &map);
			break;
		case IMSG_REQUEST_ADD_END:
			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				return;
			}
			send_labelrequest(nbr);
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
ldpe_fib_update(int type)
{
	if (type == IMSG_CTL_LFIB_COUPLE)
		oe_nofib = 0;
	if (type == IMSG_CTL_LFIB_DECOUPLE)
		oe_nofib = 1;
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

void
ldpe_nbr_ctl(struct ctl_conn *c)
{
	struct iface	*iface;
	struct nbr	*nbr;
	struct ctl_nbr	*nctl;

	LIST_FOREACH(iface, &leconf->iface_list, entry) {
		LIST_FOREACH(nbr, &iface->nbr_list, entry) {
			nctl = nbr_to_ctl(nbr);
			imsg_compose_event(&c->iev,
			    IMSG_CTL_SHOW_NBR, 0, 0, -1, nctl,
			    sizeof(struct ctl_nbr));
		}
	}

	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}
