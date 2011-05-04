/*	$OpenBSD: connection.c,v 1.13 2011/05/04 21:00:04 claudio Exp $ */

/*
 * Copyright (c) 2009 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <scsi/iscsi.h>

#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

void	conn_dispatch(int, short, void *);
void	conn_write_dispatch(int, short, void *);

int	c_do_connect(struct connection *, enum c_event);
int	c_do_login(struct connection *, enum c_event);
int	c_do_loggedin(struct connection *, enum c_event);
int	c_do_logout(struct connection *, enum c_event);
int	c_do_loggedout(struct connection *, enum c_event);
int	c_do_fail(struct connection *, enum c_event);

const char *conn_state(int);
const char *conn_event(enum c_event);

void
conn_new(struct session *s, struct connection_config *cc)
{
	struct connection *c;
	int nodelay = 1;

	if (!(c = calloc(1, sizeof(*c))))
		fatal("session_add_conn");

	c->fd = -1;
	c->state = CONN_FREE;
	c->session = s;
	c->cid = arc4random();
	c->config = *cc;
	c->mine = initiator_conn_defaults;
	c->mine.HeaderDigest = s->config.HeaderDigest;
	c->mine.DataDigest = s->config.DataDigest;
	c->his = iscsi_conn_defaults;
	c->active = iscsi_conn_defaults;

	TAILQ_INIT(&c->pdu_w);
	TAILQ_INIT(&c->tasks);
	TAILQ_INSERT_TAIL(&s->connections, c, entry);

	if (pdu_readbuf_set(&c->prbuf, PDU_READ_SIZE)) {
		log_warn("conn_new");
		conn_free(c);
		return;
	}

	/* create socket */
	c->fd = socket(c->config.TargetAddr.ss_family, SOCK_STREAM, 0);
	if (c->fd == -1) {
		log_warn("conn_new: socket");
		conn_free(c);
		return;
	}
	if (socket_setblockmode(c->fd, 1)) {
		log_warn("conn_new: socket_setblockmode");
		conn_free(c);
		return;
	}

	/* try to turn off TCP Nagle */
	if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
	    sizeof(nodelay)) == -1)
		log_warn("conn_new: setting TCP_NODELAY");

	event_set(&c->ev, c->fd, EV_READ|EV_PERSIST, conn_dispatch, c);
	event_set(&c->wev, c->fd, EV_WRITE, conn_write_dispatch, c);
	event_add(&c->ev, NULL);

	conn_fsm(c, CONN_EV_CONNECT);
}

void
conn_free(struct connection *c)
{
	pdu_readbuf_free(&c->prbuf);
	pdu_free_queue(&c->pdu_w);

	event_del(&c->ev);
	event_del(&c->wev);
	close(c->fd);

	taskq_cleanup(&c->tasks);

	TAILQ_REMOVE(&c->session->connections, c, entry);
	free(c);
}

void
conn_dispatch(int fd, short event, void *arg)
{
	struct connection *c = arg;
	ssize_t n;

	if (!(event & EV_READ)) {
		log_debug("spurious read call");
		return;
	}
	if ((n = pdu_read(c)) == -1) {
		conn_fsm(c, CONN_EV_FAIL);
		return;
	}
	if (n == 0) {    /* connection closed */
		conn_fsm(c, CONN_EV_CLOSED);
		return;
	}

	pdu_parse(c);
}

void
conn_write_dispatch(int fd, short event, void *arg)
{
	struct connection *c = arg;
	ssize_t n;
	int error;
	socklen_t len;

	if (!(event & EV_WRITE)) {
		log_debug("spurious write call");
		return;
	}

	switch (c->state) {
	case CONN_XPT_WAIT:
		len = sizeof(error);
		if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR,
		    &error, &len) == -1 || (errno = error)) {
			log_warn("cwd connect(%s)",
			    log_sockaddr(&c->config.TargetAddr));
			conn_fsm(c, CONN_EV_FAIL);
			return;
		}
		conn_fsm(c, CONN_EV_CONNECTED);
		break;
	default:
		if ((n = pdu_write(c)) == -1) {
			log_warn("pdu_write");
			conn_fsm(c, CONN_EV_FAIL);
			return;
		}
		if (n == 0) {    /* connection closed */
			conn_fsm(c, CONN_EV_CLOSED);
			return;
		}

		/* check if there is more to send */
		if (pdu_pending(c))
			event_add(&c->wev, NULL);
	}
}

void
conn_fail(struct connection *c)
{
	log_debug("conn_fail");
	conn_fsm(c, CONN_EV_FAIL);
}

int
conn_task_ready(struct connection *c)
{
	if ((c->state & CONN_RUNNING) && TAILQ_EMPTY(&c->tasks))
		return 1;
	return 0;
}

void
conn_task_issue(struct connection *c, struct task *t)
{
	TAILQ_INSERT_TAIL(&c->tasks, t, entry);
	conn_task_schedule(c);
}

void
conn_task_schedule(struct connection *c)
{
	struct task *t = TAILQ_FIRST(&c->tasks);
	struct pdu *p, *np;

	if (!t) {
		log_debug("conn_task_schedule: task is hiding");
		return;
	}

	/* move pdus to the write queue */
	for (p = TAILQ_FIRST(&t->sendq); p != NULL; p = np) {
		np = TAILQ_NEXT(p, entry);
		TAILQ_REMOVE(&t->sendq, p, entry);
		conn_pdu_write(c, p);
	}
	if (t->callback == NULL) {
		/* no callback, immediate command expecting no answer */
		conn_task_cleanup(c, t);
		free(t);
	}
}

void
conn_task_cleanup(struct connection *c, struct task *t)
{
	pdu_free_queue(&t->sendq);
	pdu_free_queue(&t->recvq);
	/* XXX need some state to know if queued or not */
	if (c) {
		TAILQ_REMOVE(&c->tasks, t, entry);
		if (!TAILQ_EMPTY(&c->tasks))
			conn_task_schedule(c);
		else
			session_schedule(c->session);
	}
}

#define SET_NUM(p, x, v, min, max)				\
do {								\
	if (!strcmp((p)->key, #v)) {				\
		(x)->his.v = text_to_num((p)->value, (min), (max), &err); \
		if (err) {					\
			log_warnx("bad param %s=%s: %s",	\
			    (p)->key, (p)->value, err);		\
			errors++;				\
		}						\
log_debug("SET_NUM: %s = %llu", #v, (u_int64_t)(x)->his.v);	\
	}							\
} while (0)

#define SET_BOOL(p, x, v)					\
do {								\
	if (!strcmp((p)->key, #v)) {				\
		(x)->his.v = text_to_bool((p)->value, &err);	\
		if (err) {					\
			log_warnx("bad param %s=%s: %s",	\
			    (p)->key, (p)->value, err);		\
			errors++;				\
		}						\
log_debug("SET_BOOL: %s = %u", #v, (int)(x)->his.v);		\
	}							\
} while (0)

int
conn_parse_kvp(struct connection *c, struct kvp *kvp)
{
	struct kvp *k;
	struct session *s = c->session;
	const char *err;
	int errors = 0;


	for (k = kvp; k->key; k++) {
		SET_NUM(k, s, MaxBurstLength, 512, 16777215);
		SET_NUM(k, s, FirstBurstLength, 512, 16777215);
		SET_NUM(k, s, DefaultTime2Wait, 0, 3600);
		SET_NUM(k, s, DefaultTime2Retain, 0, 3600);
		SET_NUM(k, s, MaxOutstandingR2T, 1, 65535);
		SET_NUM(k, s, TargetPortalGroupTag, 1, 65535);
		SET_NUM(k, s, MaxConnections, 1, 65535);
		SET_BOOL(k, s, InitialR2T);
		SET_BOOL(k, s, ImmediateData);
		SET_BOOL(k, s, DataPDUInOrder);
		SET_BOOL(k, s, DataSequenceInOrder);
		SET_NUM(k, s, ErrorRecoveryLevel, 0, 2);
		SET_NUM(k, c, MaxRecvDataSegmentLength, 512, 16777215);
	}

	if (errors) {
		log_warnx("conn_parse_kvp: errors found");
		return -1;
	}
	return 0;
}

#undef SET_NUM
#undef SET_BOOL

int
conn_gen_kvp(struct connection *c, struct kvp *kvp, size_t *nkvp)
{
	struct session *s = c->session;
	size_t i = 0;

	if (s->mine.MaxConnections != iscsi_sess_defaults.MaxConnections) {
		i++;
		if (kvp && i < *nkvp) {
			kvp[i].key = strdup("MaxConnections");
			if (kvp[i].key == NULL)
				return (-1);
			if (asprintf(&kvp[i].value, "%u",
			    (unsigned int)s->mine.MaxConnections) == -1) {
				kvp[i].value = NULL;
				return (-1);
			}
		}
	}
	if (c->mine.MaxRecvDataSegmentLength !=
	    iscsi_conn_defaults.MaxRecvDataSegmentLength) {
		i++;
		if (kvp && i < *nkvp) {
			kvp[i].key = strdup("MaxRecvDataSegmentLength");
			if (kvp[i].key == NULL)
				return (-1);
			if (asprintf(&kvp[i].value, "%u",
			    (unsigned int)c->mine.MaxRecvDataSegmentLength) == -1) {
				kvp[i].value = NULL;
				return (-1);
			}
		}
	}

	*nkvp = i;
	return (0);
}

void
conn_pdu_write(struct connection *c, struct pdu *p)
{
	struct iscsi_pdu *ipdu;

/* XXX I GUESS THIS SHOULD BE MOVED TO PDU SOMEHOW... */
	ipdu = pdu_getbuf(p, NULL, PDU_HEADER);
	switch (ISCSI_PDU_OPCODE(ipdu->opcode)) {
	case ISCSI_OP_I_NOP:
	case ISCSI_OP_SCSI_REQUEST:
	case ISCSI_OP_TASK_REQUEST:
	case ISCSI_OP_LOGIN_REQUEST:
	case ISCSI_OP_TEXT_REQUEST:
	case ISCSI_OP_DATA_OUT:
	case ISCSI_OP_LOGOUT_REQUEST:
	case ISCSI_OP_SNACK_REQUEST:
		ipdu->expstatsn = ntohl(c->expstatsn);
		break;
	}

	TAILQ_INSERT_TAIL(&c->pdu_w, p, entry);
	event_add(&c->wev, NULL);
}

/* connection state machine more or less as specified in the RFC */
struct {
	int		state;
	enum c_event	event;
	int		(*action)(struct connection *, enum c_event);
} fsm[] = {
	{ CONN_FREE, CONN_EV_CONNECT, c_do_connect },		/* T1 */
	{ CONN_XPT_WAIT, CONN_EV_CONNECTED, c_do_login },	/* T4 */
	{ CONN_IN_LOGIN, CONN_EV_LOGGED_IN, c_do_loggedin },	/* T5 */
	{ CONN_LOGGED_IN, CONN_EV_LOGOUT, c_do_logout },	/* T9 */
	{ CONN_LOGOUT_REQ, CONN_EV_LOGOUT, c_do_logout },	/* T10 */
	{ CONN_IN_LOGOUT, CONN_EV_LOGGED_OUT, c_do_loggedout },	/* T13 */
	{ CONN_ANYSTATE, CONN_EV_CLOSED, c_do_fail },
	{ CONN_ANYSTATE, CONN_EV_FAIL, c_do_fail },
	{ CONN_ANYSTATE, CONN_EV_FREE, c_do_fail },
	{ 0, 0, NULL }
};

void
conn_fsm(struct connection *c, enum c_event event)
{
	int	i, ns;

	for (i = 0; fsm[i].action != NULL; i++) {
		if (c->state & fsm[i].state && event == fsm[i].event) {
			log_debug("conn_fsm[%s]: %s ev %s",
			    c->session->config.SessionName,
			    conn_state(c->state), conn_event(event));
			ns = fsm[i].action(c, event);
			if (ns == -1)
				/* XXX better please */
				fatalx("conn_fsm: action failed");
			log_debug("conn_fsm[%s]: new state %s",
			    c->session->config.SessionName, conn_state(ns));
			c->state = ns;
			return;
		}
	}
	log_warnx("conn_fsm[%s]: unhandled state transition [%s, %s]",
	    c->session->config.SessionName, conn_state(c->state),
	    conn_event(event));
	fatalx("bork bork bork");
}

int
c_do_connect(struct connection *c, enum c_event ev)
{
	if (c->fd == -1) {
		log_warnx("connect(%s), lost socket",
		    log_sockaddr(&c->config.TargetAddr));
		session_fsm(c->session, SESS_EV_CONN_FAIL, c);
		return (CONN_FREE);
	}

	if (connect(c->fd, (struct sockaddr *)&c->config.TargetAddr,
	    c->config.TargetAddr.ss_len) == -1) {
		if (errno == EINPROGRESS) {
			event_add(&c->wev, NULL);
			return (CONN_XPT_WAIT);
		} else {
			log_warn("connect(%s)",
			    log_sockaddr(&c->config.TargetAddr));
			session_fsm(c->session, SESS_EV_CONN_FAIL, c);
			return (CONN_FREE);
		}
	}
	/* move forward */
	return (c_do_login(c, CONN_EV_CONNECTED));
}

int
c_do_login(struct connection *c, enum c_event ev)
{
	/* start a login session and hope for the best ... */
	initiator_login(c);
	return (CONN_IN_LOGIN);
}

int
c_do_loggedin(struct connection *c, enum c_event ev)
{
	session_fsm(c->session, SESS_EV_CONN_LOGGED_IN, c);

	return (CONN_LOGGED_IN);
}

int
c_do_logout(struct connection *c, enum c_event ev)
{
	/* logout is in progress ... */
	return (CONN_IN_LOGOUT);
}

int
c_do_loggedout(struct connection *c, enum c_event ev)
{
	/* close TCP session and cleanup */
	event_del(&c->ev);
	event_del(&c->wev);
	close(c->fd);

	/* session is informed by the logout handler */
	return (CONN_FREE);
}

int
c_do_fail(struct connection *c, enum c_event ev)
{
	/* cleanup events so that the connection does not retrigger */
	event_del(&c->ev);
	event_del(&c->wev);
	close(c->fd);

	session_fsm(c->session, SESS_EV_CONN_FAIL, c);

	if (ev == CONN_EV_FREE || c->state & CONN_NEVER_LOGGED_IN)
		return (CONN_FREE);
	return (CONN_CLEANUP_WAIT);
}

const char *
conn_state(int s)
{
	static char buf[15];

	switch (s) {
	case CONN_FREE:
		return "FREE";
	case CONN_XPT_WAIT:
		return "XPT_WAIT";
	case CONN_XPT_UP:
		return "XPT_UP";
	case CONN_IN_LOGIN:
		return "IN_LOGIN";
	case CONN_LOGGED_IN:
		return "LOGGED_IN";
	case CONN_IN_LOGOUT:
		return "IN_LOGOUT";
	case CONN_LOGOUT_REQ:
		return "LOGOUT_REQ";
	case CONN_CLEANUP_WAIT:
		return "CLEANUP_WAIT";
	case CONN_IN_CLEANUP:
		return "IN_CLEANUP";
	default:
		snprintf(buf, sizeof(buf), "UKNWN %x", s);
		return buf;
	}
	/* NOTREACHED */
}

const char *
conn_event(enum c_event e)
{
	static char buf[15];

	switch (e) {
	case CONN_EV_FAIL:
		return "fail";
	case CONN_EV_CONNECT:
		return "connect";
	case CONN_EV_CONNECTED:
		return "connected";
	case CONN_EV_LOGGED_IN:
		return "logged in";
	case CONN_EV_LOGOUT:
		return "logout";
	case CONN_EV_LOGGED_OUT:
		return "logged out";
	case CONN_EV_CLOSED:
		return "closed";
	case CONN_EV_FREE:
		return "forced free";
	}

	snprintf(buf, sizeof(buf), "UKNWN %d", e);
	return buf;
}
