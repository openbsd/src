/*	$OpenBSD: net.c,v 1.1 2005/03/30 18:44:49 ho Exp $	*/

/*
 * Copyright (c) 2005 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Multicom Security AB.
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "sasyncd.h"
#include "net.h"

struct msg {
	u_int8_t	*buf;
	u_int8_t	*obuf;		/* Original buf w/o offset. */
	u_int32_t	 len;
	u_int32_t	 type;
	int		 refcnt;
};

struct qmsg {
	SIMPLEQ_ENTRY(qmsg)	next;
	struct msg	*msg;
};

int	listen_socket;

/* Local prototypes. */
static u_int8_t *net_read(struct syncpeer *, u_int32_t *, u_int32_t *);
static int	 net_set_sa(struct sockaddr *, char *, in_port_t);
static void	 net_check_peers(void *);

int
net_init(void)
{
	struct sockaddr_storage sa_storage;
	struct sockaddr *sa = (struct sockaddr *)&sa_storage;
	struct syncpeer *p;
	int		 r;

	if (net_SSL_init())
		return -1;

	/* Setup listening socket.  */
	memset(&sa_storage, 0, sizeof sa_storage);
	if (net_set_sa(sa, cfgstate.listen_on, cfgstate.listen_port)) {
		perror("inet_pton");
		return -1;
	}
	listen_socket = socket(sa->sa_family, SOCK_STREAM, 0);
	if (listen_socket < 0) {
		perror("socket()");
		close(listen_socket);
		return -1;
	}
	r = 1;
	if (setsockopt(listen_socket, SOL_SOCKET,
	    cfgstate.listen_on ? SO_REUSEADDR : SO_REUSEPORT, (void *)&r,
	    sizeof r)) {
		perror("setsockopt()");
		close(listen_socket);
		return -1;
	}
	if (bind(listen_socket, sa, sizeof(struct sockaddr_in))) {
		perror("bind()");
		close(listen_socket);
		return -1;
	}
	if (listen(listen_socket, 10)) {
		perror("listen()");
		close(listen_socket);
		return -1;
	}
	log_msg(2, "listening on port %u fd %d", cfgstate.listen_port,
	    listen_socket);

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		p->socket = -1;
		SIMPLEQ_INIT(&p->msgs);
	}

	net_check_peers(0);
	return 0;
}

static void
net_enqueue(struct syncpeer *p, struct msg *m)
{
	struct qmsg	*qm;

	if (p->socket < 0)
		return;

	if (!p->ssl)
		if (net_SSL_connect(p))
			return;

	qm = (struct qmsg *)malloc(sizeof *qm);
	if (!qm) {
		log_err("malloc()");
		return;
	}

	memset(qm, 0, sizeof *qm);
	qm->msg = m;
	m->refcnt++;

	SIMPLEQ_INSERT_TAIL(&p->msgs, qm, next);
	return;
}

/*
 * Queue a message for transmission to a particular peer,
 * or to all peers if no peer is specified.
 */
int
net_queue(struct syncpeer *p0, u_int32_t msgtype, u_int8_t *buf,
    u_int32_t offset, u_int32_t len)
{
	struct syncpeer *p = p0;
	struct msg	*m;

	m = (struct msg *)malloc(sizeof *m);
	if (!m) {
		log_err("malloc()");
		free(buf);
		return -1;
	}
	memset(m, 0, sizeof *m);
	m->obuf = buf;
	m->buf = buf + offset;
	m->len = len;
	m->type = msgtype;

	if (p)
		net_enqueue(p, m);
	else
		for (p = LIST_FIRST(&cfgstate.peerlist); p;
		     p = LIST_NEXT(p, link))
			net_enqueue(p, m);

	if (!m->refcnt) {
		free(m->obuf);
		free(m);
	}

	return 0;
}

/* Set all write pending filedescriptors. */
int
net_set_pending_wfds(fd_set *fds)
{
	struct syncpeer *p;
	int		max_fd = -1;

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link))
		if (p->socket > -1 && SIMPLEQ_FIRST(&p->msgs)) {
			FD_SET(p->socket, fds);
			if (p->socket > max_fd)
				max_fd = p->socket;
		}
	return max_fd + 1;
}

/*
 * Set readable filedescriptors. They are basically the same as for write,
 * plus the listening socket.
 */
int
net_set_rfds(fd_set *fds)
{
	struct syncpeer *p;
	int		max_fd = -1;

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		if (p->socket > -1)
			FD_SET(p->socket, fds);
		if (p->socket > max_fd)
			max_fd = p->socket;
	}
	FD_SET(listen_socket, fds);
	if (listen_socket > max_fd)
		max_fd = listen_socket;
	return max_fd + 1;
}

void
net_handle_messages(fd_set *fds)
{
	struct sockaddr_storage	sa_storage, sa_storage2;
	struct sockaddr	*sa = (struct sockaddr *)&sa_storage;
	struct sockaddr	*sa2 = (struct sockaddr *)&sa_storage2;
	socklen_t	socklen;
	struct syncpeer *p;
	u_int8_t	*msg;
	u_int32_t	 msgtype, msglen;
	int		 newsock, found;

	if (FD_ISSET(listen_socket, fds)) {
		/* Accept a new incoming connection */
		socklen = sizeof sa_storage;
		newsock = accept(listen_socket, sa, &socklen);
		if (newsock > -1) {
			/* Setup the syncpeer structure */
			found = 0;
			for (p = LIST_FIRST(&cfgstate.peerlist); p && !found;
			     p = LIST_NEXT(p, link)) {
				struct sockaddr_in *sin, *sin2;
				struct sockaddr_in6 *sin6, *sin62;

				/* Match? */
				if (net_set_sa(sa2, p->name, 0))
					continue;
				if (sa->sa_family != sa2->sa_family)
					continue;
				if (sa->sa_family == AF_INET) {
					sin = (struct sockaddr_in *)sa;
					sin2 = (struct sockaddr_in *)sa2;
					if (memcmp(&sin->sin_addr,
					    &sin2->sin_addr,
					    sizeof(struct in_addr)))
						continue;
				} else {
					sin6 = (struct sockaddr_in6 *)sa;
					sin62 = (struct sockaddr_in6 *)sa2;
					if (memcmp(&sin6->sin6_addr,
					    &sin62->sin6_addr,
					    sizeof(struct in6_addr)))
						continue;
				}
				/* Match! */
				found++;
				p->socket = newsock;
				p->ssl = NULL;
				log_msg(1, "peer \"%s\" connected", p->name);
			}
			if (!found) {
				log_msg(1, "Found no matching peer for "
				    "accepted socket, closing.");
				close(newsock);
			}
		} else
			log_err("accept()");
	}

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		if (p->socket < 0 || !FD_ISSET(p->socket, fds))
			continue;
		msg = net_read(p, &msgtype, &msglen);
		if (!msg)
			continue;

		/* XXX check message validity. */

		log_msg(4, "net_handle_messages: got msg type %u len %u from "
		    "peer %s", msgtype, msglen, p->name);

		switch (msgtype) {
		case MSG_SYNCCTL:
			net_ctl_handle_msg(p, msg, msglen);
			free(msg);
			break;

		case MSG_PFKEYDATA:
			if (p->runstate != MASTER ||
			    cfgstate.runstate == MASTER) {
				log_msg(0, "got PFKEY message from non-MASTER "
				    "peer");
				free(msg);
				if (cfgstate.runstate == MASTER)
					net_ctl_send_state(p);
				else
					net_ctl_send_error(p, 0);
			} else if (pfkey_queue_message(msg, msglen))
				free(msg);
			break;

		default:
			log_msg(0, "Got unknown message type %u len %u from "
			    "peer %s", msgtype, msglen, p->name);
			free(msg);
			net_ctl_send_error(p, 0);
		}
	}
}

void
net_send_messages(fd_set *fds)
{
	struct syncpeer *p;
	struct qmsg	*qm;
	struct msg	*m;
	u_int32_t	 v;

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		if (p->socket < 0 || !FD_ISSET(p->socket, fds))
			continue;

		qm = SIMPLEQ_FIRST(&p->msgs);
		if (!qm) {
			/* XXX Log */
			continue;
		}
		m = qm->msg;

		log_msg(4, "sending msg %p (qm %p ref %d) to peer %s", m, qm,
		    m->refcnt, p->name);

		/* Send the message. */
		v = htonl(m->type);
		if (net_SSL_write(p, &v, sizeof v))
			continue;

		v = htonl(m->len);
		if (net_SSL_write(p, &v, sizeof v))
			continue;

		(void)net_SSL_write(p, m->buf, m->len);

		/* Cleanup. */
		SIMPLEQ_REMOVE_HEAD(&p->msgs, next);
		free(qm);

		if (--m->refcnt < 1) {
			log_msg(4, "freeing msg %p", m);
			free(m->obuf);
			free(m);
		}
	}
	return;
}

void
net_disconnect_peer(struct syncpeer *p)
{
	net_SSL_disconnect(p);
	if (p->socket > -1)
		close(p->socket);
	p->socket = -1;
}

void
net_shutdown(void)
{
	struct syncpeer *p;
	struct qmsg	*qm;
	struct msg	*m;

	while ((p = LIST_FIRST(&cfgstate.peerlist))) {
		while ((qm = SIMPLEQ_FIRST(&p->msgs))) {
			SIMPLEQ_REMOVE_HEAD(&p->msgs, next);
			m = qm->msg;
			if (--m->refcnt < 1) {
				free(m->obuf);
				free(m);
			}
			free(qm);
		}
		net_disconnect_peer(p);
		if (p->name)
			free(p->name);
		LIST_REMOVE(p, link);
		free(p);
	}

	if (listen_socket > -1)
		close(listen_socket);
	net_SSL_shutdown();
}

/*
 * Helper functions (local) below here.
 */

static u_int8_t *
net_read(struct syncpeer *p, u_int32_t *msgtype, u_int32_t *msglen)
{
	u_int8_t	*msg;
	u_int32_t	 v;

	if (net_SSL_read(p, &v, sizeof v))
		return NULL;
	*msgtype = ntohl(v);

	if (*msgtype > MSG_MAXTYPE)
		return NULL;

	if (net_SSL_read(p, &v, sizeof v))
		return NULL;
	*msglen = ntohl(v);

	/* XXX msglen sanity */

	msg = (u_int8_t *)malloc(*msglen);
	memset(msg, 0, *msglen);
	if (net_SSL_read(p, msg, *msglen)) {
		free(msg);
		return NULL;
	}

	return msg;
}

static int
net_set_sa(struct sockaddr *sa, char *name, in_port_t port)
{
	struct sockaddr_in	*sin = (struct sockaddr_in *)sa;
	struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *)sa;

	if (name) {
		if (inet_pton(AF_INET, name, &sin->sin_addr) == 1) {
			sa->sa_family = AF_INET;
			sin->sin_port = htons(port);
			sin->sin_len = sizeof *sin;
			return 0;
		}

		if (inet_pton(AF_INET6, name, &sin6->sin6_addr) == 1) {
			sa->sa_family = AF_INET6;
			sin6->sin6_port = htons(port);
			sin6->sin6_len = sizeof *sin6;
			return 0;
		}
	} else {
		/* XXX Assume IPv4 */
		sa->sa_family = AF_INET;
		sin->sin_port = htons(port);
		sin->sin_len = sizeof *sin;
		return 0;
	}

	return 1;
}

static void
got_sigalrm(int s)
{
	return;
}

void
net_connect_peers(void)
{
	struct sockaddr_storage sa_storage;
	struct itimerval	iv;
	struct sockaddr	*sa = (struct sockaddr *)&sa_storage;
	struct syncpeer	*p;

	signal(SIGALRM, got_sigalrm);
	memset(&iv, 0, sizeof iv);
	iv.it_value.tv_sec = 5;
	iv.it_interval.tv_sec = 5;
	setitimer(ITIMER_REAL, &iv, NULL);

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		if (p->ssl || p->socket > -1)
			continue;

		memset(sa, 0, sizeof sa_storage);
		if (net_set_sa(sa, p->name, cfgstate.listen_port))
			continue;
		p->socket = socket(sa->sa_family, SOCK_STREAM, 0);
		if (p->socket < 0) {
			log_err("peer \"%s\": socket()", p->name);
			continue;
		}
		if (connect(p->socket, sa, sa->sa_len)) {
			log_msg(1, "peer \"%s\" not ready yet", p->name);
			net_disconnect_peer(p);
			continue;
		}
		if (net_ctl_send_state(p)) {
			log_msg(0, "peer \"%s\" failed", p->name);
			net_disconnect_peer(p);
			continue;
		}
		log_msg(1, "peer \"%s\" connected", p->name);
	}

	timerclear(&iv.it_value);
	timerclear(&iv.it_interval);
	setitimer(ITIMER_REAL, &iv, NULL);
	signal(SIGALRM, SIG_IGN);

	return;
}

static void
net_check_peers(void *arg)
{
	net_connect_peers();

	(void)timer_add("peer recheck", 600, net_check_peers, 0);
}

