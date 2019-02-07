/*	$OpenBSD: frontend.c,v 1.11 2019/02/07 17:20:35 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "libunbound/config.h"
#include "libunbound/sldns/pkthdr.h"
#include "libunbound/sldns/sbuffer.h"
#include "libunbound/sldns/wire2str.h"

#include "uw_log.h"
#include "unwind.h"
#include "frontend.h"
#include "control.h"

#define	ROUTE_SOCKET_BUF_SIZE   16384

struct udp_ev {
	struct event		 ev;
	uint8_t			 query[65536];
	struct msghdr		 rcvmhdr;
	struct iovec		 rcviov[1];
	struct sockaddr_storage	 from;
} udp4ev, udp6ev;

struct pending_query {
	TAILQ_ENTRY(pending_query)	 entry;
	struct sockaddr_storage		 from;
	uint8_t				*query;
	ssize_t				 len;
	uint64_t			 imsg_id;
	int				 fd;
	int				 bogus;
};

TAILQ_HEAD(, pending_query)	 pending_queries;

__dead void		 frontend_shutdown(void);
void			 frontend_sig_handler(int, short, void *);
void			 frontend_startup(void);
void			 udp_receive(int, short, void *);
void			 send_answer(struct pending_query *, uint8_t *,
			     ssize_t);
void			 route_receive(int, short, void *);
void			 handle_route_message(struct rt_msghdr *,
			     struct sockaddr **);
void			 get_rtaddrs(int, struct sockaddr *,
			     struct sockaddr **);
void			 rtmget_default(void);
struct pending_query	*find_pending_query(uint64_t);
void			 parse_dhcp_lease(int);
void			 parse_trust_anchor(struct trust_anchor_head *, int);
void			 send_trust_anchors(struct trust_anchor_head *);
void			 write_trust_anchors(struct trust_anchor_head *, int);

struct unwind_conf	*frontend_conf;
struct imsgev		*iev_main;
struct imsgev		*iev_resolver;
struct imsgev		*iev_captiveportal;
struct event		 ev_route;
int			 udp4sock = -1, udp6sock = -1, routesock = -1;

static struct trust_anchor_head	 built_in_trust_anchors;
static struct trust_anchor_head	 trust_anchors, new_trust_anchors;

void
frontend_sig_handler(int sig, short event, void *bula)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		frontend_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
frontend(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;
	size_t			 rcvcmsglen, sndcmsgbuflen;
	uint8_t			*rcvcmsgbuf;
	uint8_t			*sndcmsgbuf = NULL;

	frontend_conf = config_new_empty();
	control_state.fd = -1;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(UNWIND_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	unwind_process = PROC_FRONTEND;
	setproctitle("%s", log_procnames[unwind_process]);
	log_procinit(log_procnames[unwind_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio unix recvfd", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, frontend_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, frontend_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the parent process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = frontend_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL)
		fatal("malloc");

	udp4ev.rcviov[0].iov_base = (caddr_t)udp4ev.query;
	udp4ev.rcviov[0].iov_len = sizeof(udp4ev.query);
	udp4ev.rcvmhdr.msg_name = (caddr_t)&udp4ev.from;
	udp4ev.rcvmhdr.msg_namelen = sizeof(udp4ev.from);
	udp4ev.rcvmhdr.msg_iov = udp4ev.rcviov;
	udp4ev.rcvmhdr.msg_iovlen = 1;

	udp6ev.rcviov[0].iov_base = (caddr_t)udp6ev.query;
	udp6ev.rcviov[0].iov_len = sizeof(udp6ev.query);
	udp6ev.rcvmhdr.msg_name = (caddr_t)&udp6ev.from;
	udp6ev.rcvmhdr.msg_namelen = sizeof(udp6ev.from);
	udp6ev.rcvmhdr.msg_iov = udp6ev.rcviov;
	udp6ev.rcvmhdr.msg_iovlen = 1;

	sndcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if ((sndcmsgbuf = malloc(sndcmsgbuflen)) == NULL)
		fatal("%s", __func__);

	TAILQ_INIT(&pending_queries);

	TAILQ_INIT(&built_in_trust_anchors);
	TAILQ_INIT(&trust_anchors);
	TAILQ_INIT(&new_trust_anchors);

	add_new_ta(&built_in_trust_anchors, KSK2017);

	event_dispatch();

	frontend_shutdown();
}

__dead void
frontend_shutdown(void)
{
	/* Close pipes. */
	msgbuf_write(&iev_resolver->ibuf.w);
	msgbuf_clear(&iev_resolver->ibuf.w);
	close(iev_resolver->ibuf.fd);
	msgbuf_write(&iev_captiveportal->ibuf.w);
	msgbuf_clear(&iev_captiveportal->ibuf.w);
	close(iev_captiveportal->ibuf.fd);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(frontend_conf);

	free(iev_resolver);
	free(iev_captiveportal);
	free(iev_main);

	log_info("frontend exiting");
	exit(0);
}

int
frontend_imsg_compose_main(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data,
	    datalen));
}

int
frontend_imsg_compose_resolver(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_resolver, type, 0, pid, -1, data,
	    datalen));
}

int
frontend_imsg_compose_captiveportal(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_captiveportal, type, 0, pid, -1, data,
	    datalen));
}

void
frontend_dispatch_main(int fd, short event, void *bula)
{
	static struct unwind_conf		*nconf;
	struct unwind_forwarder		*unwind_forwarder;
	struct imsg			 imsg;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	int				 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC_RESOLVER:
			/*
			 * Setup pipe and event handler to the resolver
			 * process.
			 */
			if (iev_resolver) {
				fatalx("%s: received unexpected imsg fd "
				    "to frontend", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				fatalx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);
				break;
			}

			iev_resolver = malloc(sizeof(struct imsgev));
			if (iev_resolver == NULL)
				fatal(NULL);

			imsg_init(&iev_resolver->ibuf, fd);
			iev_resolver->handler = frontend_dispatch_resolver;
			iev_resolver->events = EV_READ;

			event_set(&iev_resolver->ev, iev_resolver->ibuf.fd,
			iev_resolver->events, iev_resolver->handler, iev_resolver);
			event_add(&iev_resolver->ev, NULL);
			break;
		case IMSG_SOCKET_IPC_CAPTIVEPORTAL:
			/*
			 * Setup pipe and event handler to the captiveportal
			 * process.
			 */
			if (iev_captiveportal) {
				fatalx("%s: received unexpected imsg fd "
				    "to frontend", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				fatalx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);
				break;
			}

			iev_captiveportal = malloc(sizeof(struct imsgev));
			if (iev_captiveportal == NULL)
				fatal(NULL);

			imsg_init(&iev_captiveportal->ibuf, fd);
			iev_captiveportal->handler = frontend_dispatch_captiveportal;
			iev_captiveportal->events = EV_READ;

			event_set(&iev_captiveportal->ev, iev_captiveportal->ibuf.fd,
			iev_captiveportal->events, iev_captiveportal->handler, iev_captiveportal);
			event_add(&iev_captiveportal->ev, NULL);
			break;
		case IMSG_RECONF_CONF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct unwind_conf))
				fatalx("%s: IMSG_RECONF_CONF wrong length: %d",
				    __func__, imsg.hdr.len);
			if ((nconf = malloc(sizeof(struct unwind_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct unwind_conf));
			nconf->captive_portal_host = NULL;
			nconf->captive_portal_path = NULL;
			nconf->captive_portal_expected_response = NULL;
			SIMPLEQ_INIT(&nconf->unwind_forwarder_list);
			SIMPLEQ_INIT(&nconf->unwind_dot_forwarder_list);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_HOST:
			/* make sure this is a string */
			((char *)imsg.data)[imsg.hdr.len - IMSG_HEADER_SIZE - 1]
			    = '\0';
			if ((nconf->captive_portal_host = strdup(imsg.data)) ==
			    NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_PATH:
			/* make sure this is a string */
			((char *)imsg.data)[imsg.hdr.len - IMSG_HEADER_SIZE - 1]
			    = '\0';
			if ((nconf->captive_portal_path = strdup(imsg.data)) ==
			    NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_EXPECTED_RESPONSE:
			/* make sure this is a string */
			((char *)imsg.data)[imsg.hdr.len - IMSG_HEADER_SIZE - 1]
			    = '\0';
			if ((nconf->captive_portal_expected_response =
			    strdup(imsg.data)) == NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_FORWARDER:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct unwind_forwarder))
				fatalx("%s: IMSG_RECONF_FORWARDER wrong length:"
				    " %d", __func__, imsg.hdr.len);
			if ((unwind_forwarder = malloc(sizeof(struct
			    unwind_forwarder))) == NULL)
				fatal(NULL);
			memcpy(unwind_forwarder, imsg.data, sizeof(struct
			    unwind_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->unwind_forwarder_list,
			    unwind_forwarder, entry);
			break;
		case IMSG_RECONF_DOT_FORWARDER:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct unwind_forwarder))
				fatalx("%s: IMSG_RECONF_DOT_FORWARDER wrong "
				    "length: %d", __func__, imsg.hdr.len);
			if ((unwind_forwarder = malloc(sizeof(struct
			    unwind_forwarder))) == NULL)
				fatal(NULL);
			memcpy(unwind_forwarder, imsg.data, sizeof(struct
			    unwind_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->unwind_dot_forwarder_list,
			    unwind_forwarder, entry);
			break;
		case IMSG_RECONF_END:
			merge_config(frontend_conf, nconf);
			nconf = NULL;
			break;
		case IMSG_UDP6SOCK:
			if (udp6sock != -1)
				fatalx("%s: received unexpected udp6sock",
				    __func__);
			if ((udp6sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "UDP6 fd but didn't receive any",
				    __func__);
			event_set(&udp6ev.ev, udp6sock, EV_READ | EV_PERSIST,
			    udp_receive, &udp6ev);
			event_add(&udp6ev.ev, NULL);
			break;
		case IMSG_UDP4SOCK:
			if (udp4sock != -1)
				fatalx("%s: received unexpected udp4sock",
				    __func__);
			if ((udp4sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "UDP4 fd but didn't receive any",
				    __func__);
			event_set(&udp4ev.ev, udp4sock, EV_READ | EV_PERSIST,
			    udp_receive, &udp4ev);
			event_add(&udp4ev.ev, NULL);
			break;
		case IMSG_ROUTESOCK:
			if (routesock != -1)
				fatalx("%s: received unexpected routesock",
				    __func__);
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "routesocket fd but didn't receive any",
				    __func__);
			routesock = fd;
			event_set(&ev_route, fd, EV_READ | EV_PERSIST,
			    route_receive, NULL);
			break;
		case IMSG_STARTUP:
			frontend_startup();
			break;
		case IMSG_CONTROLFD:
			if (control_state.fd != -1)
				fatalx("%s: received unexpected controlsock",
				    __func__);
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "control fd but didn't receive any",
				    __func__);
			control_state.fd = fd;
			/* Listen on control socket. */
			TAILQ_INIT(&ctl_conns);
			control_listen();
			break;
		case IMSG_LEASEFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "dhcp lease fd but didn't receive any",
				    __func__);
			parse_dhcp_lease(fd);
			break;
		case IMSG_TAFD:
			if ((fd = imsg.fd) != -1)
				parse_trust_anchor(&trust_anchors, fd);
			if (!TAILQ_EMPTY(&trust_anchors))
				send_trust_anchors(&trust_anchors);
			else
				send_trust_anchors(&built_in_trust_anchors);
			break;
		case IMSG_TAFD_W:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg trust "
				    "anchor fd but didn't receive any",
				    __func__);
			write_trust_anchors(&trust_anchors, fd);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_dispatch_resolver(int fd, short event, void *bula)
{
	static struct pending_query	*pq;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	struct imsg			 imsg;
	struct query_imsg		*query_imsg;
	int				 n, shut = 0, chg;
	char				*ta;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_ANSWER_HEADER:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(*query_imsg))
				fatalx("%s: IMSG_ANSWER_HEADER wrong length: "
				    "%d", __func__, imsg.hdr.len);
			query_imsg = (struct query_imsg *)imsg.data;
			if ((pq = find_pending_query(query_imsg->id)) ==
			    NULL) {
				log_warnx("cannot find pending query %llu",
				    query_imsg->id);
				break;
			}
			if (query_imsg->err) {
				send_answer(pq, NULL, 0);
				pq = NULL;
				break;
			}
			pq->bogus = query_imsg->bogus;
			break;
		case IMSG_ANSWER:
			if (pq == NULL)
				fatalx("IMSG_ANSWER without HEADER");
			send_answer(pq, imsg.data, imsg.hdr.len -
			    IMSG_HEADER_SIZE);
			break;
		case IMSG_RESOLVER_DOWN:
			log_debug("%s: IMSG_RESOLVER_DOWN", __func__);
			if (udp4sock != -1) {
				event_del(&udp4ev.ev);
				close(udp4sock);
				udp4sock = -1;
			}
			if (udp6sock != -1) {
				event_del(&udp6ev.ev);
				close(udp6sock);
				udp6sock = -1;
			}
			break;
		case IMSG_RESOLVER_UP:
			log_debug("%s: IMSG_RESOLVER_UP", __func__);
			frontend_imsg_compose_main(IMSG_OPEN_PORTS, 0, NULL, 0);
			break;
		case IMSG_CTL_RESOLVER_INFO:
		case IMSG_CTL_CAPTIVEPORTAL_INFO:
		case IMSG_CTL_RESOLVER_WHY_BOGUS:
		case IMSG_CTL_RESOLVER_HISTOGRAM:
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
		case IMSG_NEW_TA:
			/* make sure this is a string */
			((char *)imsg.data)[imsg.hdr.len - IMSG_HEADER_SIZE - 1]
			    = '\0';
			ta = imsg.data;
			add_new_ta(&new_trust_anchors, ta);
			break;
		case IMSG_NEW_TAS_ABORT:
			log_debug("%s: IMSG_NEW_TAS_ABORT", __func__);
			free_tas(&new_trust_anchors);
			break;
		case IMSG_NEW_TAS_DONE:
			chg = merge_tas(&new_trust_anchors, &trust_anchors);
			log_debug("%s: IMSG_NEW_TAS_DONE: change: %d",
			    __func__, chg);
			if (chg) {
				send_trust_anchors(&trust_anchors);
			}
			/*
			 * always write trust anchors, the modify date on
			 * the file is an indication when we made progress
			 */
			frontend_imsg_compose_main(IMSG_OPEN_TA_W, 0, NULL,
			    0);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_dispatch_captiveportal(int fd, short event, void *bula)
{
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	struct imsg			 imsg;
	int				 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_startup(void)
{
	if (!event_initialized(&ev_route))
		fatalx("%s: did not receive a route socket from the main "
		    "process", __func__);

	event_add(&ev_route, NULL);

	frontend_imsg_compose_main(IMSG_OPEN_TA_RO, 0, NULL, 0);

	frontend_imsg_compose_main(IMSG_STARTUP_DONE, 0, NULL, 0);
	rtmget_default();
}

void
udp_receive(int fd, short events, void *arg)
{
	struct udp_ev		*udpev = (struct udp_ev *)arg;
	struct pending_query	*pq;
	struct query_imsg	*query_imsg;
	ssize_t			 len, rem_len, buf_len;
	uint16_t		 qdcount, ancount, nscount, arcount, t, c;
	uint8_t			*queryp;
	char			*str_from, *str, buf[1024], *bufp;

	if ((len = recvmsg(fd, &udpev->rcvmhdr, 0)) < 0) {
		log_warn("recvmsg");
		return;
	}

	bufp = buf;
	buf_len = sizeof(buf);

	str_from = ip_port((struct sockaddr *)&udpev->from);

	if (len < LDNS_HEADER_SIZE) {
		log_warnx("bad query: too short, from: %s", str_from);
		return;
	}

	qdcount = LDNS_QDCOUNT(udpev->query);
	ancount = LDNS_ANCOUNT(udpev->query);
	nscount = LDNS_NSCOUNT(udpev->query);
	arcount = LDNS_ARCOUNT(udpev->query);

	if (qdcount != 1 && ancount != 0 && nscount != 0 && arcount != 0) {
		log_warnx("invalid query from %s, qdcount: %d, ancount: %d "
		    "nscount: %d, arcount: %d", str_from, qdcount, ancount,
		    nscount, arcount);
		return;
	}

	log_debug("query from %s", str_from);
	if ((str = sldns_wire2str_pkt(udpev->query, len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

	queryp = udpev->query;
	rem_len = len;

	queryp += LDNS_HEADER_SIZE;
	rem_len -= LDNS_HEADER_SIZE;

	sldns_wire2str_dname_scan(&queryp, &rem_len, &bufp, &buf_len,
	     udpev->query, len);

	if (rem_len < 4) {
		log_warnx("malformed query");
		return;
	}

	t = sldns_read_uint16(queryp);
	c = sldns_read_uint16(queryp+2);
	queryp += 4;
	rem_len -= 4;

	if ((pq = malloc(sizeof(*pq))) == NULL) {
		log_warn(NULL);
		return;
	}

	if ((pq->query = malloc(len)) == NULL) {
		log_warn(NULL);
		free(pq);
		return;
	}

	do {
		arc4random_buf(&pq->imsg_id, sizeof(pq->imsg_id));
	} while(find_pending_query(pq->imsg_id) != NULL);

	memcpy(pq->query, udpev->query, len);
	pq->len = len;
	pq->from = udpev->from;
	pq->fd = fd;

	if ((query_imsg = calloc(1, sizeof(*query_imsg))) == NULL) {
		log_warn(NULL);
		return;
	}

	if (strlcpy(query_imsg->qname, buf, sizeof(query_imsg->qname)) >=
	    sizeof(buf)) {
		log_warnx("qname too long");
		free(query_imsg);
		/* XXX SERVFAIL */
		free(pq->query);
		free(pq);
		return;
	}
	query_imsg->id = pq->imsg_id;
	query_imsg->t = t;
	query_imsg->c = c;

	if (frontend_imsg_compose_resolver(IMSG_QUERY, 0, query_imsg,
	    sizeof(*query_imsg)) != -1) {
		TAILQ_INSERT_TAIL(&pending_queries, pq, entry);
	} else {
		free(query_imsg);
		/* XXX SERVFAIL */
		free(pq->query);
		free(pq);
	}

}

void
send_answer(struct pending_query *pq, uint8_t *answer, ssize_t len)
{
	log_debug("result for %s",
	    ip_port((struct sockaddr*)&pq->from));

	if (answer == NULL) {
		answer = pq->query;
		len = pq->len;

		LDNS_QR_SET(answer);
		LDNS_RA_SET(answer);
		LDNS_RCODE_SET(answer, LDNS_RCODE_SERVFAIL);
	} else {
		if (pq->bogus) {
			if(LDNS_CD_WIRE(pq->query)) {
				LDNS_ID_SET(answer, LDNS_ID_WIRE(pq->query));
				LDNS_CD_SET(answer);
			} else {
				answer = pq->query;
				len = pq->len;

				LDNS_QR_SET(answer);
				LDNS_RA_SET(answer);
				LDNS_RCODE_SET(answer, LDNS_RCODE_SERVFAIL);
			}
		} else {
			LDNS_ID_SET(answer, LDNS_ID_WIRE(pq->query));
		}
	}

	if(sendto(pq->fd, answer, len, 0, (struct sockaddr *)
	   &pq->from, pq->from.ss_len) == -1)
		log_warn("sendto");

	TAILQ_REMOVE(&pending_queries, pq, entry);
	free(pq->query);
	free(pq);
}

char*
ip_port(struct sockaddr *sa)
{
	static char	 hbuf[NI_MAXHOST], buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0,
	    NI_NUMERICHOST) != 0) {
		snprintf(buf, sizeof(buf), "%s", "(unknown)");
		return buf;
	}

	if (sa->sa_family == AF_INET6)
		snprintf(buf, sizeof(buf), "[%s]:%d", hbuf, ntohs(
		    ((struct sockaddr_in6 *)sa)->sin6_port));
	if (sa->sa_family == AF_INET)
		snprintf(buf, sizeof(buf), "[%s]:%d", hbuf, ntohs(
		    ((struct sockaddr_in *)sa)->sin_port));

	return buf;
}

struct pending_query*
find_pending_query(uint64_t id)
{
	struct pending_query	*pq;

	TAILQ_FOREACH(pq, &pending_queries, entry)
		if (pq->imsg_id == id)
			return pq;
	return NULL;
}

void
route_receive(int fd, short events, void *arg)
{
	static uint8_t			 *buf;

	struct rt_msghdr		*rtm;
	struct sockaddr			*sa, *rti_info[RTAX_MAX];
	ssize_t				 n;

	if (buf == NULL) {
		buf = malloc(ROUTE_SOCKET_BUF_SIZE);
		if (buf == NULL)
			fatal("malloc");
	}
	rtm = (struct rt_msghdr *)buf;
	if ((n = read(fd, buf, ROUTE_SOCKET_BUF_SIZE)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		log_warn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0)
		fatal("routing socket closed");

	if (n < (ssize_t)sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen) {
		log_warnx("partial rtm of %zd in buffer", n);
		return;
	}

	if (rtm->rtm_version != RTM_VERSION)
		return;

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	handle_route_message(rtm, rti_info);
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
handle_route_message(struct rt_msghdr *rtm, struct sockaddr **rti_info)
{
	char	buf[IF_NAMESIZE], *bufp;

	switch (rtm->rtm_type) {
	case RTM_GET:
		if (rtm->rtm_errno != 0)
			break;
		if (!(rtm->rtm_flags & RTF_UP))
			break;
		if (!(rtm->rtm_addrs & RTA_DST))
			break;
		if (rti_info[RTAX_DST]->sa_family != AF_INET)
			break;
		if (((struct sockaddr_in *)rti_info[RTAX_DST])->sin_addr.
		    s_addr != INADDR_ANY)
			break;
		if (!(rtm->rtm_addrs & RTA_NETMASK))
			break;
		if (rti_info[RTAX_NETMASK]->sa_family != AF_INET)
			break;
		if (((struct sockaddr_in *)rti_info[RTAX_NETMASK])->sin_addr.
		    s_addr != INADDR_ANY)
			break;

		frontend_imsg_compose_main(IMSG_OPEN_DHCP_LEASE, 0,
		    &rtm->rtm_index, sizeof(rtm->rtm_index));

		bufp = if_indextoname(rtm->rtm_index, buf);
		if (bufp)
			log_debug("default route is on %s", buf);

		break;
	default:
		break;
	}

}

void
rtmget_default(void)
{
	static int		 rtm_seq;
	struct rt_msghdr	 rtm;
	struct sockaddr_in	 sin;
	struct iovec		 iov[5];
	long			 pad = 0;
	int			 iovcnt = 0, padlen;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_GET;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = 0; /* XXX imsg->rdomain; */
	rtm.rtm_seq = ++rtm_seq;
	rtm.rtm_addrs = RTA_DST | RTA_NETMASK;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	/* dst */
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	rtm.rtm_msglen += sizeof(sin);
	padlen = ROUNDUP(sizeof(sin)) - sizeof(sin);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	/* mask */
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	rtm.rtm_msglen += sizeof(sin);
	padlen = ROUNDUP(sizeof(sin)) - sizeof(sin);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	if (writev(routesock, iov, iovcnt) == -1)
		log_warn("failed to send route message");
}

void
parse_dhcp_lease(int fd)
{
	FILE	 *f;
	char	 *line = NULL, *cur_ns = NULL, *ns = NULL;
	size_t	  linesize = 0;
	ssize_t	  linelen;
	time_t	  epoch = 0, lease_time = 0, now;
	char	**tok, *toks[4], *p;

	if((f = fdopen(fd, "r")) == NULL) {
		log_warn("cannot read dhcp lease");
		close(fd);
		return;
	}

	now = time(NULL);

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		for (tok = toks; tok < &toks[3] && (*tok = strsep(&line, " \t"))
		    != NULL;) {
			if (**tok != '\0')
				tok++;
		}
		*tok = NULL;
		if (strcmp(toks[0], "option") == 0) {
			if (strcmp(toks[1], "domain-name-servers") == 0) {
				if((p = strchr(toks[2], ';')) != NULL) {
					*p='\0';
					cur_ns = strdup(toks[2]);
				}
			}
			if (strcmp(toks[1], "dhcp-lease-time") == 0) {
				if((p = strchr(toks[2], ';')) != NULL) {
					*p='\0';
					lease_time = strtonum(toks[2], 0,
					    INT64_MAX, NULL);
				}
			}
		} else if (strcmp(toks[0], "epoch") == 0) {
			if((p = strchr(toks[1], ';')) != NULL) {
				*p='\0';
				epoch = strtonum(toks[1], 0,
				    INT64_MAX, NULL);
			}
		}
		else if (*toks[0] == '}') {
			if (epoch + lease_time > now ) {
				free(ns);
				ns = cur_ns;
			} else /* expired lease */
				free(cur_ns);
		}
	}
	free(line);

	if (ferror(f))
		log_warn("getline");
	fclose(f);

	if (ns != NULL) {
		log_debug("%s: ns: %s", __func__, ns);
		frontend_imsg_compose_resolver(IMSG_FORWARDER, 0, ns,
		    strlen(ns) + 1);
	}
}


void
add_new_ta(struct trust_anchor_head *tah, char *val)
{

	struct trust_anchor	*ta, *i;
	int			 cmp;

	if ((ta = malloc(sizeof(*ta))) == NULL)
		fatal("%s", __func__);
	if ((ta->ta = strdup(val)) == NULL)
		fatal("%s", __func__);

	/* keep the list sorted to prevent churn if the order changes in DNS */
	TAILQ_FOREACH(i, tah, entry) {
		cmp = strcmp(i->ta, ta->ta);
		if ( cmp == 0) {
			/* duplicate */
			free(ta->ta);
			free(ta);
			return;
		} else if (cmp > 0) {
			TAILQ_INSERT_BEFORE(i, ta, entry);
			return;
		}
	}
	TAILQ_INSERT_TAIL(tah, ta, entry);
}

void
free_tas(struct trust_anchor_head *tah)
{
	struct trust_anchor	*ta;

	while ((ta = TAILQ_FIRST(tah))) {
		TAILQ_REMOVE(tah, ta, entry);
		free(ta->ta);
		free(ta);
	}
}

int
merge_tas(struct trust_anchor_head *newh, struct trust_anchor_head *oldh)
{
	struct trust_anchor	*i, *j;
	int			 chg = 0;

	j = TAILQ_FIRST(oldh);

	TAILQ_FOREACH(i, newh, entry) {
		if (j == NULL || strcmp(i->ta, j->ta) != 0) {
			chg = 1;
			break;
		}
		j = TAILQ_NEXT(j, entry);	
	}
	if (j!= NULL)
		chg = 1;

	if (chg) {
		free_tas(oldh);
		while((i = TAILQ_FIRST(newh)) != NULL) {
			TAILQ_REMOVE(newh, i, entry);
			TAILQ_INSERT_TAIL(oldh, i, entry);
		}
	} else {
		free_tas(newh);
	}
	return (chg);
}

void
parse_trust_anchor(struct trust_anchor_head *tah, int fd)
{
	FILE	*f;
	char	*line = NULL, *p;
	size_t	 linesize = 0;
	ssize_t	 linelen;

	if((f = fdopen(fd, "r")) == NULL) {
		log_warn("cannot read trust anchor file");
		close(fd);
		return;
	}

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		if (*line == ';')
			continue;
		p = strchr(line, ';');
		if (p == NULL)
			p = strchr(line, '\n');
		if (p != NULL) {
			do {
				p--;
			} while(p != line && *p == ' ');
			*(p + 1) = '\0';
		}
		log_debug("%s: %s", __func__, line);
		add_new_ta(tah, line);
	}
	free(line);
	if (ferror(f))
		log_warn("getline");
	fclose(f);
}

void
send_trust_anchors(struct trust_anchor_head *tah)
{
	struct trust_anchor	*ta;

	TAILQ_FOREACH(ta, tah, entry)
		frontend_imsg_compose_resolver(IMSG_NEW_TA, 0, ta->ta,
		    strlen(ta->ta) + 1);
	frontend_imsg_compose_resolver(IMSG_NEW_TAS_DONE, 0, NULL, 0);
}

void
write_trust_anchors(struct trust_anchor_head *tah, int fd)
{
	FILE			*f;
	struct trust_anchor	*ta;

	if((f = fdopen(fd, "w+")) == NULL) {
		log_warn("cannot open trust anchor file for writing");
		goto err;
	}

	TAILQ_FOREACH(ta, tah, entry)
		if (fprintf(f, "%s\n", ta->ta) < 0)
			goto err;
	if (ferror(f)) {
		log_warn("%s", __func__);
		goto err;
	}
	if (fclose(f) != 0) {
		f = NULL;
		log_warn("%s", __func__);
		goto err;
	}
	frontend_imsg_compose_main(IMSG_TA_W_DONE, 0, NULL, 0);
	return;
err:
	if (f == NULL)
		close(fd);
	else
		fclose(f);

	frontend_imsg_compose_main(IMSG_TA_W_FAILED, 0, NULL, 0);
}
