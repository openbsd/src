/*	$OpenBSD: frontend.c,v 1.50 2020/01/28 15:44:13 bket Exp $	*/

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
#include <sys/tree.h>
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
#include "libunbound/sldns/str2wire.h"
#include "libunbound/sldns/wire2str.h"
#include "libunbound/util/data/dname.h"
#include "libunbound/util/data/msgparse.h"
#include "libunbound/util/data/msgreply.h"

#include "log.h"
#include "unwind.h"
#include "frontend.h"
#include "control.h"

#define	ROUTE_SOCKET_BUF_SIZE   16384

/*
 * size of a resource record with name a two octed pointer to qname
 * 2 octets pointer to qname
 * 2 octets TYPE
 * 2 octets CLASS
 * 4 octets TTL
 * 2 octets RDLENGTH
 */
#define COMPRESSED_RR_SIZE	12

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
	struct sldns_buffer		*qbuf;
	ssize_t				 len;
	uint64_t			 imsg_id;
	int				 fd;
	int				 bogus;
	int				 rcode_override;
	ssize_t				 answer_len;
	uint8_t				*answer;
};

TAILQ_HEAD(, pending_query)	 pending_queries;

struct bl_node {
	RB_ENTRY(bl_node)	 entry;
	char			*domain;
};

__dead void		 frontend_shutdown(void);
void			 frontend_sig_handler(int, short, void *);
void			 frontend_startup(void);
void			 udp_receive(int, short, void *);
int			 check_query(sldns_buffer*);
void			 chaos_answer(struct pending_query *);
void			 send_answer(struct pending_query *);
void			 route_receive(int, short, void *);
void			 handle_route_message(struct rt_msghdr *,
			     struct sockaddr **);
void			 get_rtaddrs(int, struct sockaddr *,
			     struct sockaddr **);
void			 rtmget_default(void);
struct pending_query	*find_pending_query(uint64_t);
void			 parse_trust_anchor(struct trust_anchor_head *, int);
void			 send_trust_anchors(struct trust_anchor_head *);
void			 write_trust_anchors(struct trust_anchor_head *, int);
void			 parse_blocklist(int);
int			 bl_cmp(struct bl_node *, struct bl_node *);
void			 free_bl(void);
int			 pending_query_cnt(void);

struct uw_conf		*frontend_conf;
struct imsgev		*iev_main;
struct imsgev		*iev_resolver;
struct event		 ev_route;
int			 udp4sock = -1, udp6sock = -1, routesock = -1;
int			 ta_fd = -1;

static struct trust_anchor_head	 trust_anchors, new_trust_anchors;

RB_HEAD(bl_tree, bl_node)	 bl_head = RB_INITIALIZER(&bl_head);
RB_PROTOTYPE(bl_tree, bl_node, entry, bl_cmp)
RB_GENERATE(bl_tree, bl_node, entry, bl_cmp)

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
	struct event	 ev_sigint, ev_sigterm;
	struct passwd	*pw;

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

	uw_process = PROC_FRONTEND;
	setproctitle("%s", log_procnames[uw_process]);
	log_procinit(log_procnames[uw_process]);

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
	if (iev_main != NULL)
		fatal("iev_main");
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = frontend_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

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

	TAILQ_INIT(&pending_queries);

	TAILQ_INIT(&trust_anchors);
	TAILQ_INIT(&new_trust_anchors);

	add_new_ta(&trust_anchors, KSK2017);

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
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(frontend_conf);

	free(iev_resolver);
	free(iev_main);

	log_info("frontend exiting");
	exit(0);
}

int
frontend_imsg_compose_main(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
frontend_imsg_compose_resolver(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_resolver, type, 0, pid, -1, data,
	    datalen));
}

void
frontend_dispatch_main(int fd, short event, void *bula)
{
	static struct uw_conf	*nconf;
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	int			 n, shut = 0;

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

			if (iev_resolver != NULL)
				fatal("iev_resolver");
			iev_resolver = malloc(sizeof(struct imsgev));
			if (iev_resolver == NULL)
				fatal(NULL);

			imsg_init(&iev_resolver->ibuf, fd);
			iev_resolver->handler = frontend_dispatch_resolver;
			iev_resolver->events = EV_READ;

			event_set(&iev_resolver->ev, iev_resolver->ibuf.fd,
			    iev_resolver->events, iev_resolver->handler,
			    iev_resolver);
			event_add(&iev_resolver->ev, NULL);
			break;
		case IMSG_RECONF_CONF:
		case IMSG_RECONF_BLOCKLIST_FILE:
		case IMSG_RECONF_FORWARDER:
		case IMSG_RECONF_DOT_FORWARDER:
		case IMSG_RECONF_FORCE:
			imsg_receive_config(&imsg, &nconf);
			break;
		case IMSG_RECONF_END:
			if (nconf == NULL)
				fatalx("%s: IMSG_RECONF_END without "
				    "IMSG_RECONF_CONF", __func__);
			merge_config(frontend_conf, nconf);
			if (frontend_conf->blocklist_file == NULL)
				free_bl();
			nconf = NULL;
			break;
		case IMSG_UDP6SOCK:
			if (udp6sock != -1)
				fatalx("%s: received unexpected udp6sock",
				    __func__);
			if ((udp6sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "UDP6 fd but didn't receive any", __func__);
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
				    "UDP4 fd but didn't receive any", __func__);
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
				fatalx("%s: expected to receive imsg control "
				    "fd but didn't receive any", __func__);
			control_state.fd = fd;
			/* Listen on control socket. */
			TAILQ_INIT(&ctl_conns);
			control_listen();
			break;
		case IMSG_TAFD:
			if ((ta_fd = imsg.fd) != -1)
				parse_trust_anchor(&trust_anchors, ta_fd);
			if (!TAILQ_EMPTY(&trust_anchors))
				send_trust_anchors(&trust_anchors);
			break;
		case IMSG_BLFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg block "
				   "list fd but didn't receive any", __func__);
			parse_blocklist(fd);
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
			if (pq != NULL)
				fatalx("expected IMSG_ANSWER but got HEADER");
			if (IMSG_DATA_SIZE(imsg) != sizeof(*query_imsg))
				fatalx("%s: IMSG_ANSWER_HEADER wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			query_imsg = (struct query_imsg *)imsg.data;
			if ((pq = find_pending_query(query_imsg->id)) ==
			    NULL) {
				log_warnx("cannot find pending query %llu",
				    query_imsg->id);
				break;
			}
			if (query_imsg->err) {
				send_answer(pq);
				pq = NULL;
				break;
			}
			pq->bogus = query_imsg->bogus;
			break;
		case IMSG_ANSWER:
			if (pq == NULL)
				fatalx("IMSG_ANSWER without HEADER");

			if (pq->answer)
				fatal("pq->answer");
			if ((pq->answer = malloc(IMSG_DATA_SIZE(imsg))) !=
			    NULL) {
				pq->answer_len = IMSG_DATA_SIZE(imsg);
				memcpy(pq->answer, imsg.data, pq->answer_len);
			} else
				pq->rcode_override = LDNS_RCODE_SERVFAIL;
			send_answer(pq);
			pq = NULL;
			break;
		case IMSG_CTL_RESOLVER_INFO:
		case IMSG_CTL_AUTOCONF_RESOLVER_INFO:
		case IMSG_CTL_MEM_INFO:
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
		case IMSG_NEW_TA:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			add_new_ta(&new_trust_anchors, imsg.data);
			break;
		case IMSG_NEW_TAS_ABORT:
			free_tas(&new_trust_anchors);
			break;
		case IMSG_NEW_TAS_DONE:
			chg = merge_tas(&new_trust_anchors, &trust_anchors);
			if (chg)
				send_trust_anchors(&trust_anchors);

			/*
			 * always write trust anchors, the modify date on
			 * the file is an indication when we made progress
			 */
			if (ta_fd != -1)
				write_trust_anchors(&trust_anchors, ta_fd);
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
frontend_startup(void)
{
	if (!event_initialized(&ev_route))
		fatalx("%s: did not receive a route socket from the main "
		    "process", __func__);

	event_add(&ev_route, NULL);

	frontend_imsg_compose_main(IMSG_STARTUP_DONE, 0, NULL, 0);
}

void
udp_receive(int fd, short events, void *arg)
{
	struct udp_ev		*udpev = (struct udp_ev *)arg;
	struct pending_query	*pq;
	struct query_imsg	 query_imsg;
	struct query_info	 qinfo;
	struct bl_node		 find;
	ssize_t			 len, dname_len;
	int			 ret;
	char			*str;
	char			 dname[LDNS_MAX_DOMAINLEN + 1];
	char			 qclass_buf[16];
	char			 qtype_buf[16];

	memset(&qinfo, 0, sizeof(qinfo));

	if ((len = recvmsg(fd, &udpev->rcvmhdr, 0)) == -1) {
		log_warn("recvmsg");
		return;
	}

	if ((pq = calloc(1, sizeof(*pq))) == NULL) {
		log_warn(NULL);
		return;
	}

	pq->rcode_override = LDNS_RCODE_NOERROR;
	pq->len = len;
	pq->from = udpev->from;
	pq->fd = fd;

	do {
		arc4random_buf(&pq->imsg_id, sizeof(pq->imsg_id));
	} while(find_pending_query(pq->imsg_id) != NULL);

	if ((pq->qbuf = sldns_buffer_new(len)) == NULL) {
		log_warnx("sldns_buffer_new");
		goto drop;
	}
	sldns_buffer_clear(pq->qbuf);
	sldns_buffer_write(pq->qbuf, udpev->query, len);
	sldns_buffer_flip(pq->qbuf);

	if (log_getverbose() & OPT_VERBOSE2 && (str =
	    sldns_wire2str_pkt(udpev->query, len)) != NULL) {
		log_debug("from: %s\n%s", ip_port((struct sockaddr *)
		    &udpev->from), str);
		free(str);
	}

	if ((ret = check_query(pq->qbuf)) != LDNS_RCODE_NOERROR) {
		if (ret == -1)
			goto drop;
		else
			pq->rcode_override = ret;
		goto send_answer;
	}

	if (!query_info_parse(&qinfo, pq->qbuf)) {
		pq->rcode_override = LDNS_RCODE_FORMERR;
		goto send_answer;
	}

	if ((dname_len = dname_valid(qinfo.qname, qinfo.qname_len)) == 0) {
		pq->rcode_override = LDNS_RCODE_FORMERR;
		goto send_answer;
	}
	dname_str(qinfo.qname, dname);

	sldns_wire2str_class_buf(qinfo.qclass, qclass_buf, sizeof(qclass_buf));
	sldns_wire2str_type_buf(qinfo.qtype, qtype_buf, sizeof(qtype_buf));
	log_debug("%s: %s %s %s ?", ip_port((struct sockaddr *)&udpev->from),
	    dname, qclass_buf, qtype_buf);

	find.domain = dname;
	if (RB_FIND(bl_tree, &bl_head, &find) != NULL) {
		if (frontend_conf->blocklist_log)
			log_info("blocking %s", dname);
		pq->rcode_override = LDNS_RCODE_REFUSED;
		goto send_answer;
	}

	if (qinfo.qtype == LDNS_RR_TYPE_AXFR || qinfo.qtype ==
	    LDNS_RR_TYPE_IXFR) {
		pq->rcode_override = LDNS_RCODE_REFUSED;
		goto send_answer;
	}

	if(qinfo.qtype == LDNS_RR_TYPE_OPT ||
	    qinfo.qtype == LDNS_RR_TYPE_TSIG ||
	    qinfo.qtype == LDNS_RR_TYPE_TKEY ||
	    qinfo.qtype == LDNS_RR_TYPE_MAILA ||
	    qinfo.qtype == LDNS_RR_TYPE_MAILB ||
	    (qinfo.qtype >= 128 && qinfo.qtype <= 248)) {
		pq->rcode_override = LDNS_RCODE_FORMERR;
		goto send_answer;
	}

	if (qinfo.qclass == LDNS_RR_CLASS_CH) {
		if (strcasecmp(dname, "version.server.") == 0 ||
		    strcasecmp(dname, "version.bind.") == 0) {
			chaos_answer(pq);
		} else
			pq->rcode_override = LDNS_RCODE_REFUSED;
		goto send_answer;
	}

	if (strlcpy(query_imsg.qname, dname, sizeof(query_imsg.qname)) >=
	    sizeof(query_imsg.qname)) {
		log_warnx("qname too long");
		pq->rcode_override = LDNS_RCODE_FORMERR;
		goto send_answer;
	}
	query_imsg.id = pq->imsg_id;
	query_imsg.t = qinfo.qtype;
	query_imsg.c = qinfo.qclass;

	if (frontend_imsg_compose_resolver(IMSG_QUERY, 0, &query_imsg,
	    sizeof(query_imsg)) != -1)
		TAILQ_INSERT_TAIL(&pending_queries, pq, entry);
	else {
		pq->rcode_override = LDNS_RCODE_SERVFAIL;
		goto send_answer;
	}
	return;

 send_answer:
	TAILQ_INSERT_TAIL(&pending_queries, pq, entry);
	send_answer(pq);
	pq = NULL;
 drop:
	if (pq != NULL)
		sldns_buffer_free(pq->qbuf);
	free(pq);
}

void
chaos_answer(struct pending_query *pq)
{
	struct sldns_buffer	 buf, *pkt = &buf;
	size_t			 size, len;
	char			*name = "unwind";

	len = strlen(name);
	size = sldns_buffer_capacity(pq->qbuf) + COMPRESSED_RR_SIZE + 1 + len;

	if (pq->answer != 0)
		fatal("chaos_answer");
	if ((pq->answer = calloc(1, size)) == NULL)
		return;
	pq->answer_len = size;
	memcpy(pq->answer, sldns_buffer_begin(pq->qbuf),
	    sldns_buffer_capacity(pq->qbuf));
	sldns_buffer_init_frm_data(pkt, pq->answer, size);

	sldns_buffer_clear(pkt);

	sldns_buffer_skip(pkt, sizeof(uint16_t));	/* skip id */
	sldns_buffer_write_u16(pkt, 0);			/* clear flags */
	LDNS_QR_SET(sldns_buffer_begin(pkt));
	LDNS_RA_SET(sldns_buffer_begin(pkt));
	if (LDNS_RD_WIRE(sldns_buffer_begin(pq->qbuf)))
		LDNS_RD_SET(sldns_buffer_begin(pkt));
	if (LDNS_CD_WIRE(sldns_buffer_begin(pq->qbuf)))
		LDNS_CD_SET(sldns_buffer_begin(pkt));
	LDNS_RCODE_SET(sldns_buffer_begin(pkt), LDNS_RCODE_NOERROR);
	sldns_buffer_write_u16(pkt, 1);			/* qdcount */
	sldns_buffer_write_u16(pkt, 1);			/* ancount */
	sldns_buffer_write_u16(pkt, 0);			/* nscount */
	sldns_buffer_write_u16(pkt, 0);			/* arcount */
	(void)query_dname_len(pkt);			/* skip qname */
	sldns_buffer_skip(pkt, sizeof(uint16_t));	/* skip qtype */
	sldns_buffer_skip(pkt, sizeof(uint16_t));	/* skip qclass */

	sldns_buffer_write_u16(pkt, 0xc00c);		/* ptr to query */
	sldns_buffer_write_u16(pkt, LDNS_RR_TYPE_TXT);
	sldns_buffer_write_u16(pkt, LDNS_RR_CLASS_CH);
	sldns_buffer_write_u32(pkt, 0);			/* TTL */
	sldns_buffer_write_u16(pkt, 1 + len);		/* RDLENGTH */
	sldns_buffer_write_u8(pkt, len);		/* length octed */
	sldns_buffer_write(pkt, name, len);
}

int
check_query(sldns_buffer* pkt)
{
	if(sldns_buffer_limit(pkt) < LDNS_HEADER_SIZE) {
		log_warnx("bad query: too short, dropped");
		return -1;
	}
	if(LDNS_QR_WIRE(sldns_buffer_begin(pkt))) {
		log_warnx("bad query: QR set, dropped");
		return -1;
	}
	if(LDNS_TC_WIRE(sldns_buffer_begin(pkt))) {
		LDNS_TC_CLR(sldns_buffer_begin(pkt));
		log_warnx("bad query: TC set");
		return (LDNS_RCODE_FORMERR);
	}
	if(!(LDNS_RD_WIRE(sldns_buffer_begin(pkt)))) {
		log_warnx("bad query: RD not set");
		return (LDNS_RCODE_REFUSED);
	}
	if(LDNS_OPCODE_WIRE(sldns_buffer_begin(pkt)) != LDNS_PACKET_QUERY) {
		log_warnx("bad query: unknown opcode %d",
		    LDNS_OPCODE_WIRE(sldns_buffer_begin(pkt)));
		return (LDNS_RCODE_NOTIMPL);
	}

	if (LDNS_QDCOUNT(sldns_buffer_begin(pkt)) != 1 &&
	    LDNS_ANCOUNT(sldns_buffer_begin(pkt))!= 0 &&
	    LDNS_NSCOUNT(sldns_buffer_begin(pkt))!= 0 &&
	    LDNS_ARCOUNT(sldns_buffer_begin(pkt)) > 1) {
		log_warnx("bad query: qdcount: %d, ancount: %d "
		    "nscount: %d, arcount: %d",
		    LDNS_QDCOUNT(sldns_buffer_begin(pkt)),
		    LDNS_ANCOUNT(sldns_buffer_begin(pkt)),
		    LDNS_NSCOUNT(sldns_buffer_begin(pkt)),
		    LDNS_ARCOUNT(sldns_buffer_begin(pkt)));
		return (LDNS_RCODE_FORMERR);
	}
	return 0;
}

void
send_answer(struct pending_query *pq)
{
	ssize_t	 len;
	char	*str;
	uint8_t	*answer;

	answer = pq->answer;
	len = pq->answer_len;

	if (answer == NULL) {
		answer = sldns_buffer_begin(pq->qbuf);
		len = pq->len;

		LDNS_QR_SET(answer);
		LDNS_RA_SET(answer);
		if (pq->rcode_override != LDNS_RCODE_NOERROR)
			LDNS_RCODE_SET(answer, pq->rcode_override);
		else
			LDNS_RCODE_SET(answer, LDNS_RCODE_SERVFAIL);
	} else {
		if (pq->bogus) {
			if(LDNS_CD_WIRE(sldns_buffer_begin(pq->qbuf))) {
				LDNS_ID_SET(answer, LDNS_ID_WIRE(
				    sldns_buffer_begin(pq->qbuf)));
				LDNS_CD_SET(answer);
			} else {
				answer = sldns_buffer_begin(pq->qbuf);
				len = pq->len;

				LDNS_QR_SET(answer);
				LDNS_RA_SET(answer);
				LDNS_RCODE_SET(answer, LDNS_RCODE_SERVFAIL);
			}
		} else {
			LDNS_ID_SET(answer, LDNS_ID_WIRE(sldns_buffer_begin(
			    pq->qbuf)));
		}
	}

	if (log_getverbose() & OPT_VERBOSE2 && (str =
	    sldns_wire2str_pkt(answer, len)) != NULL) {
		log_debug("to: %s\n%s",
		    ip_port((struct sockaddr *)&pq->from),str);
		free(str);
		log_debug("pending query count: %d", pending_query_cnt());
	}

	if(sendto(pq->fd, answer, len, 0, (struct sockaddr *)&pq->from,
	    pq->from.ss_len) == -1)
		log_warn("sendto");

	TAILQ_REMOVE(&pending_queries, pq, entry);
	sldns_buffer_free(pq->qbuf);
	free(pq->answer);
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
	static uint8_t		*buf;

	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	ssize_t			 n;

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
	struct imsg_rdns_proposal	 rdns_proposal;
	struct sockaddr_rtdns		*rtdns;

	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		frontend_imsg_compose_resolver(IMSG_NETWORK_CHANGED, 0, NULL,
		    0);
		break;
	case RTM_PROPOSAL:
		if (!(rtm->rtm_addrs & RTA_DNS))
			break;

		rtdns = (struct sockaddr_rtdns*)rti_info[RTAX_DNS];
		switch (rtdns->sr_family) {
		case AF_INET:
			if ((rtdns->sr_len - 2) % sizeof(struct in_addr) != 0) {
				log_warnx("ignoring invalid RTM_PROPOSAL");
				return;
			}
			break;
		case AF_INET6:
			if ((rtdns->sr_len - 2) % sizeof(struct in6_addr) != 0) {
				log_warnx("ignoring invalid RTM_PROPOSAL");
				return;
			}
			break;
		default:
			log_warnx("ignoring invalid RTM_PROPOSAL");
			return;
		}
		rdns_proposal.if_index = rtm->rtm_index;
		rdns_proposal.src = rtm->rtm_priority;
		memcpy(&rdns_proposal.rtdns, rtdns, sizeof(rdns_proposal.rtdns));
		frontend_imsg_compose_resolver(IMSG_REPLACE_DNS, 0,
		    &rdns_proposal, sizeof(rdns_proposal));
		break;
	default:
		break;
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
	if (j != NULL)
		chg = 1;

	if (chg) {
		free_tas(oldh);
		TAILQ_CONCAT(oldh, newh, entry);
	} else {
		free_tas(newh);
	}
	return (chg);
}

void
parse_trust_anchor(struct trust_anchor_head *tah, int fd)
{
	size_t	 len, dname_len;
	ssize_t	 n, sz;
	uint8_t	 rr[LDNS_RR_BUF_SIZE];
	char	*str, *p, buf[512], *line;

	sz = 0;
	str = NULL;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		p = recallocarray(str, sz, sz + n, 1);
		if (p == NULL) {
			log_warn("%s", __func__);
			goto out;
		}
		str = p;
		memcpy(str + sz, buf, n);
		sz += n;
	}

	if (n == -1) {
		log_warn("%s", __func__);
		goto out;
	}

	/* make it a string */
	p = recallocarray(str, sz, sz + 1, 1);
	if (p == NULL) {
		log_warn("%s", __func__);
		goto out;
	}
	str = p;
	sz++;

	len = sizeof(rr);

	while ((line = strsep(&str, "\n")) != NULL) {
		if (sldns_str2wire_rr_buf(line, rr, &len, &dname_len,
		    ROOT_DNSKEY_TTL, NULL, 0, NULL, 0) != 0)
			continue;
		if (sldns_wirerr_get_type(rr, len, dname_len) ==
		    LDNS_RR_TYPE_DNSKEY)
			add_new_ta(tah, line);
	}

out:
	free(str);
	return;
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
	struct trust_anchor	*ta;
	size_t			 len = 0;
	ssize_t			 n;
	char			*str;

	if (lseek(fd, 0, SEEK_SET) == -1) {
		log_warn("%s", __func__);
		goto out;
	}

	TAILQ_FOREACH(ta, tah, entry) {
		if ((n = asprintf(&str, "%s\n", ta->ta)) == -1) {
			log_warn("%s", __func__);
			len = 0;
			goto out;
		}
		len += n;
		if (write(fd, str, n) != n) {
			log_warn("%s", __func__);
			free(str);
			len = 0;
			goto out;
		}
		free(str);
	}
out:
	ftruncate(fd, len);
	fsync(fd);
}

void
parse_blocklist(int fd)
{
	FILE		 *f;
	struct bl_node	*bl_node;
	char		 *line = NULL;
	size_t		  linesize = 0;
	ssize_t		  linelen;

	if((f = fdopen(fd, "r")) == NULL) {
		log_warn("cannot read block list");
		close(fd);
		return;
	}

	free_bl();

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		if (line[linelen - 1] == '\n') {
			if (linelen >= 2 && line[linelen - 2] != '.')
				line[linelen - 1] = '.';
			else
				line[linelen - 1] = '\0';
		}

		bl_node = malloc(sizeof *bl_node);
		if (bl_node == NULL)
			fatal("%s: malloc", __func__);
		if ((bl_node->domain = strdup(line)) == NULL)
			fatal("%s: strdup", __func__);
		RB_INSERT(bl_tree, &bl_head, bl_node);
	}
	free(line);
	if (ferror(f))
		log_warn("getline");
	fclose(f);
}

int
bl_cmp(struct bl_node *e1, struct bl_node *e2) {
	return (strcasecmp(e1->domain, e2->domain));
}

void
free_bl(void)
{
	struct bl_node	*n, *nxt;

	for (n = RB_MIN(bl_tree, &bl_head); n != NULL; n = nxt) {
		nxt = RB_NEXT(bl_tree, &bl_head, n);
		RB_REMOVE(bl_tree, &bl_head, n);
		free(n);
	}
}

int
pending_query_cnt(void)
{
	struct pending_query	*e;
	int			 cnt = 0;

	TAILQ_FOREACH(e, &pending_queries, entry)
		cnt++;
	return cnt;
}
