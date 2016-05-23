/*	$OpenBSD: ldpe.c,v 1.51 2016/05/23 18:33:56 renato Exp $ */

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
#include <netinet/tcp.h>
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

void	 ldpe_sig_handler(int, short, void *);
void	 ldpe_shutdown(void);

struct ldpd_conf	*leconf = NULL, *nconf;
struct imsgev		*iev_main;
struct imsgev		*iev_lde;
struct event		 disc_ev;
struct event		 edisc_ev;
struct event             pfkey_ev;
struct ldpd_sysdep	 sysdep;

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

	leconf = xconf;

	setproctitle("ldp engine");
	ldpd_process = PROC_LDP_ENGINE;

	/* create ldpd control socket outside chroot */
	if (control_init() == -1)
		fatalx("control socket setup failed");

	LIST_INIT(&global.addr_list);
	TAILQ_INIT(&global.pending_conns);
	global.pfkeysock = pfkey_init(&sysdep);

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

	if (pledge("stdio cpath inet mcast recvfd", NULL) == -1)
		fatal("pledge");

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

	event_set(&pfkey_ev, global.pfkeysock, EV_READ | EV_PERSIST,
	    ldpe_dispatch_pfkey, NULL);
	event_add(&pfkey_ev, NULL);

	/* mark sockets as closed */
	global.ldp_disc_socket = -1;
	global.ldp_edisc_socket = -1;
	global.ldp_session_socket = -1;

	/* listen on ldpd control socket */
	TAILQ_INIT(&ctl_conns);
	control_listen();

	if ((pkt_ptr = calloc(1, IBUF_READ_SIZE)) == NULL)
		fatal(__func__);

	event_dispatch();

	ldpe_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ldpe_shutdown(void)
{
	struct if_addr		*if_addr;

	control_cleanup();
	config_clear(leconf);

	event_del(&pfkey_ev);
	close(global.pfkeysock);

	ldpe_close_sockets();

	/* remove addresses from global list */
	while ((if_addr = LIST_FIRST(&global.addr_list)) != NULL) {
		LIST_REMOVE(if_addr, entry);
		free(if_addr);
	}

	/* clean up */
	msgbuf_write(&iev_lde->ibuf.w);
	msgbuf_clear(&iev_lde->ibuf.w);
	free(iev_lde);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	free(iev_main);
	free(pkt_ptr);

	log_info("ldp engine exiting");
	_exit(0);
}

/* imesg */
int
ldpe_imsg_compose_parent(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
ldpe_imsg_compose_lde(int type, uint32_t peerid, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_lde, type, peerid, pid, -1,
	    data, datalen));
}

/* ARGSUSED */
void
ldpe_dispatch_main(int fd, short event, void *bula)
{
	struct iface		*niface;
	struct tnbr		*ntnbr;
	struct nbr_params	*nnbrp;
	static struct l2vpn	*nl2vpn;
	struct l2vpn_if		*nlif;
	struct l2vpn_pw		*npw;
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct iface		*iface = NULL;
	struct kif		*kif;
	enum socket_type	*socket_type;
	static int		 disc_socket = -1;
	static int		 edisc_socket = -1;
	static int		 session_socket = -1;
	int			 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
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
			fatal("ldpe_dispatch_main: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_IFSTATUS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kif))
				fatalx("IFSTATUS imsg with wrong len");

			kif = imsg.data;
			iface = if_lookup(leconf, kif->ifindex);
			if (!iface)
				break;

			iface->flags = kif->flags;
			iface->linkstate = kif->link_state;
			if_update(iface);
			break;
		case IMSG_NEWADDR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kaddr))
				fatalx("NEWADDR imsg with wrong len");

			if_addr_add(imsg.data);
			break;
		case IMSG_DELADDR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kaddr))
				fatalx("DELADDR imsg with wrong len");

			if_addr_del(imsg.data);
			break;
		case IMSG_CLOSE_SOCKETS:
			ldpe_close_sockets();
			if_update_all();
			tnbr_update_all();

			disc_socket = -1;
			edisc_socket = -1;
			session_socket = -1;
			ldpe_imsg_compose_parent(IMSG_REQUEST_SOCKETS, 0,
			    NULL, 0);
			break;
		case IMSG_SOCKET_NET:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(enum socket_type))
				fatalx("SOCKET_NET imsg with wrong len");
			socket_type = imsg.data;

			switch (*socket_type) {
			case LDP_SOCKET_DISC:
				disc_socket = imsg.fd;
				break;
			case LDP_SOCKET_EDISC:
				edisc_socket = imsg.fd;
				break;
			case LDP_SOCKET_SESSION:
				session_socket = imsg.fd;
				break;
			}
			break;
		case IMSG_SETUP_SOCKETS:
			if (disc_socket == -1 || edisc_socket == -1 ||
			    session_socket == -1) {
				if (disc_socket != -1)
					close(disc_socket);
				if (edisc_socket != -1)
					close(edisc_socket);
				if (session_socket != -1)
					close(session_socket);
				break;
			}

			ldpe_setup_sockets(disc_socket, edisc_socket,
			    session_socket);
			if_update_all();
			tnbr_update_all();
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct ldpd_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct ldpd_conf));

			LIST_INIT(&nconf->iface_list);
			LIST_INIT(&nconf->tnbr_list);
			LIST_INIT(&nconf->nbrp_list);
			LIST_INIT(&nconf->l2vpn_list);
			break;
		case IMSG_RECONF_IFACE:
			if ((niface = malloc(sizeof(struct iface))) == NULL)
				fatal(NULL);
			memcpy(niface, imsg.data, sizeof(struct iface));

			LIST_INIT(&niface->addr_list);
			LIST_INIT(&niface->adj_list);

			LIST_INSERT_HEAD(&nconf->iface_list, niface, entry);
			break;
		case IMSG_RECONF_TNBR:
			if ((ntnbr = malloc(sizeof(struct tnbr))) == NULL)
				fatal(NULL);
			memcpy(ntnbr, imsg.data, sizeof(struct tnbr));

			LIST_INSERT_HEAD(&nconf->tnbr_list, ntnbr, entry);
			break;
		case IMSG_RECONF_NBRP:
			if ((nnbrp = malloc(sizeof(struct nbr_params))) == NULL)
				fatal(NULL);
			memcpy(nnbrp, imsg.data, sizeof(struct nbr_params));

			LIST_INSERT_HEAD(&nconf->nbrp_list, nnbrp, entry);
			break;
		case IMSG_RECONF_L2VPN:
			if ((nl2vpn = malloc(sizeof(struct l2vpn))) == NULL)
				fatal(NULL);
			memcpy(nl2vpn, imsg.data, sizeof(struct l2vpn));

			LIST_INIT(&nl2vpn->if_list);
			LIST_INIT(&nl2vpn->pw_list);

			LIST_INSERT_HEAD(&nconf->l2vpn_list, nl2vpn, entry);
			break;
		case IMSG_RECONF_L2VPN_IF:
			if ((nlif = malloc(sizeof(struct l2vpn_if))) == NULL)
				fatal(NULL);
			memcpy(nlif, imsg.data, sizeof(struct l2vpn_if));

			nlif->l2vpn = nl2vpn;
			LIST_INSERT_HEAD(&nl2vpn->if_list, nlif, entry);
			break;
		case IMSG_RECONF_L2VPN_PW:
			if ((npw = malloc(sizeof(struct l2vpn_pw))) == NULL)
				fatal(NULL);
			memcpy(npw, imsg.data, sizeof(struct l2vpn_pw));

			npw->l2vpn = nl2vpn;
			LIST_INSERT_HEAD(&nl2vpn->pw_list, npw, entry);
			break;
		case IMSG_RECONF_END:
			merge_config(leconf, nconf);
			nconf = NULL;
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
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
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
			fatal("ldpe_dispatch_lde: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MAPPING_ADD:
		case IMSG_RELEASE_ADD:
		case IMSG_REQUEST_ADD:
		case IMSG_WITHDRAW_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(map))
				fatalx("invalid size of map request");
			memcpy(&map, imsg.data, sizeof(map));

			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				break;
			}
			if (nbr->state != NBR_STA_OPER)
				break;

			switch (imsg.hdr.type) {
			case IMSG_MAPPING_ADD:
				mapping_list_add(&nbr->mapping_list, &map);
				break;
			case IMSG_RELEASE_ADD:
				mapping_list_add(&nbr->release_list, &map);
				break;
			case IMSG_REQUEST_ADD:
				mapping_list_add(&nbr->request_list, &map);
				break;
			case IMSG_WITHDRAW_ADD:
				mapping_list_add(&nbr->withdraw_list, &map);
				break;
			}
			break;
		case IMSG_MAPPING_ADD_END:
		case IMSG_RELEASE_ADD_END:
		case IMSG_REQUEST_ADD_END:
		case IMSG_WITHDRAW_ADD_END:
			nbr = nbr_find_peerid(imsg.hdr.peerid);
			if (nbr == NULL) {
				log_debug("ldpe_dispatch_lde: cannot find "
				    "neighbor");
				break;
			}
			if (nbr->state != NBR_STA_OPER)
				break;

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
			case IMSG_WITHDRAW_ADD_END:
				send_labelmessage(nbr, MSG_TYPE_LABELWITHDRAW,
				    &nbr->withdraw_list);
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
				break;
			}
			if (nbr->state != NBR_STA_OPER)
				break;

			send_notification_full(nbr->tcp, &nm);
			break;
		case IMSG_CTL_END:
		case IMSG_CTL_SHOW_LIB:
		case IMSG_CTL_SHOW_L2VPN_PW:
		case IMSG_CTL_SHOW_L2VPN_BINDING:
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

/* ARGSUSED */
void
ldpe_dispatch_pfkey(int fd, short event, void *bula)
{
	if (event & EV_READ) {
		if (pfkey_read(fd, NULL) == -1) {
			fatal("pfkey_read failed, exiting...");
		}
	}
}

void
ldpe_setup_sockets(int disc_socket, int edisc_socket, int session_socket)
{
	/* discovery socket */
	global.ldp_disc_socket = disc_socket;
	event_set(&disc_ev, global.ldp_disc_socket,
	    EV_READ|EV_PERSIST, disc_recv_packet, NULL);
	event_add(&disc_ev, NULL);

	/* extended discovery socket */
	global.ldp_edisc_socket = edisc_socket;
	event_set(&edisc_ev, global.ldp_edisc_socket,
	    EV_READ|EV_PERSIST, disc_recv_packet, NULL);
	event_add(&edisc_ev, NULL);

	/* session socket */
	global.ldp_session_socket = session_socket;
	accept_add(global.ldp_session_socket, session_accept, NULL);
}

void
ldpe_close_sockets(void)
{
	/* discovery socket */
	if (event_initialized(&disc_ev))
		event_del(&disc_ev);
	if (global.ldp_disc_socket != -1) {
		close(global.ldp_disc_socket);
		global.ldp_disc_socket = -1;
	}

	/* extended discovery socket */
	if (event_initialized(&edisc_ev))
		event_del(&edisc_ev);
	if (global.ldp_edisc_socket != -1) {
		close(global.ldp_edisc_socket);
		global.ldp_edisc_socket = -1;
	}

	/* session socket */
	if (global.ldp_session_socket != -1) {
		accept_del(global.ldp_session_socket);
		close(global.ldp_session_socket);
		global.ldp_session_socket = -1;
	}
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
ldpe_adj_ctl(struct ctl_conn *c)
{
	struct adj	*adj;
	struct iface	*iface;
	struct tnbr	*tnbr;
	struct ctl_adj	*actl;

	/* basic discovery mechanism */
	LIST_FOREACH(iface, &leconf->iface_list, entry)
		LIST_FOREACH(adj, &iface->adj_list, iface_entry) {
			actl = adj_to_ctl(adj);
			imsg_compose_event(&c->iev, IMSG_CTL_SHOW_DISCOVERY,
			    0, 0, -1, actl, sizeof(struct ctl_adj));
		}

	/* extended discovery mechanism */
	LIST_FOREACH(tnbr, &leconf->tnbr_list, entry)
		if (tnbr->adj) {
			actl = adj_to_ctl(tnbr->adj);
			imsg_compose_event(&c->iev, IMSG_CTL_SHOW_DISCOVERY,
			    0, 0, -1, actl, sizeof(struct ctl_adj));
		}

	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

void
ldpe_nbr_ctl(struct ctl_conn *c)
{
	struct nbr	*nbr;
	struct ctl_nbr	*nctl;

	RB_FOREACH(nbr, nbr_pid_head, &nbrs_by_pid) {
		nctl = nbr_to_ctl(nbr);
		imsg_compose_event(&c->iev, IMSG_CTL_SHOW_NBR, 0, 0, -1, nctl,
		    sizeof(struct ctl_nbr));
	}
	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

void
mapping_list_add(struct mapping_head *mh, struct map *map)
{
	struct mapping_entry	*me;

	me = calloc(1, sizeof(*me));
	if (me == NULL)
		fatal(__func__);
	me->map = *map;

	TAILQ_INSERT_TAIL(mh, me, entry);
}

void
mapping_list_clr(struct mapping_head *mh)
{
	struct mapping_entry	*me;

	while ((me = TAILQ_FIRST(mh)) != NULL) {
		TAILQ_REMOVE(mh, me, entry);
		free(me);
	}
}
