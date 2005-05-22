/*	$OpenBSD: net.c,v 1.2 2005/05/22 20:35:48 ho Exp $	*/

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

#include <openssl/aes.h>
#include <openssl/sha.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sasyncd.h"
#include "net.h"

struct msg {
	u_int8_t	*buf;
	u_int32_t	 len;
	int		 refcnt;
};

struct qmsg {
	SIMPLEQ_ENTRY(qmsg)	next;
	struct msg	*msg;
};

int	listen_socket;
AES_KEY	aes_key[2];
#define AES_IV_LEN AES_BLOCK_SIZE

/* Local prototypes. */
static u_int8_t *net_read(struct syncpeer *, u_int32_t *, u_int32_t *);
static int	 net_set_sa(struct sockaddr *, char *, in_port_t);
static void	 net_check_peers(void *);

static void
dump_buf(int lvl, u_int8_t *b, u_int32_t len, char *title)
{
	u_int32_t	i, off, blen = len*2 + 3 + strlen(title);
	u_int8_t	*buf = calloc(1, blen);

	if (!buf || cfgstate.verboselevel < lvl)
		return;

	snprintf(buf, blen, "%s:\n", title);
	off = strlen(buf);
	for (i = 0; i < len; i++, off+=2)
		snprintf(buf + off, blen - off, "%02x", b[i]);
	log_msg(lvl, "%s", buf);
	free(buf);
}

int
net_init(void)
{
	struct sockaddr_storage sa_storage;
	struct sockaddr *sa = (struct sockaddr *)&sa_storage;
	struct syncpeer *p;
	int		 r;

	/* The shared key needs to be 128, 192 or 256 bits */
	r = (strlen(cfgstate.sharedkey) - 1) << 3;
	if (r != 128 && r != 192 && r != 256) {
		fprintf(stderr, "Bad shared key length (%d bits), "
		    "should be 128, 192 or 256\n", r);
		return -1;
	}
	
	if (AES_set_encrypt_key(cfgstate.sharedkey, r, &aes_key[0]) ||
	    AES_set_decrypt_key(cfgstate.sharedkey, r, &aes_key[1])) {
		fprintf(stderr, "Bad AES shared key\n");
		return -1;
	}

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
net_queue(struct syncpeer *p0, u_int32_t msgtype, u_int8_t *buf, u_int32_t len)
{
	struct syncpeer *p = p0;
	struct msg	*m;
	SHA_CTX		 ctx;
	u_int8_t	 hash[SHA_DIGEST_LENGTH];
	u_int8_t	 iv[AES_IV_LEN], tmp_iv[AES_IV_LEN];
	u_int32_t	 v, padlen = 0;
	int		 i, offset;

	m = (struct msg *)calloc(1, sizeof *m);
	if (!m) {
		log_err("calloc()");
		free(buf);
		return -1;
	}

	/* Generate hash */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, buf, len);
	SHA1_Final(hash, &ctx);
	dump_buf(5, hash, sizeof hash, "Hash");

	/* Padding required? */
	i = len % AES_IV_LEN;
	if (i) {
		u_int8_t *pbuf;
		i = AES_IV_LEN - i;
		pbuf = realloc(buf, len + i);
		if (!pbuf) {
			log_err("net_queue: realloc()");
			free(buf);
			free(m);
			return -1;
		}
		padlen = i;
		while (i > 0)
			pbuf[len++] = (u_int8_t)i--;
		buf = pbuf;
	}

	/* Get random IV */
	for (i = 0; i <= sizeof iv - sizeof v; i += sizeof v) {
		v = arc4random();
		memcpy(&iv[i], &v, sizeof v);
	}
	dump_buf(5, iv, sizeof iv, "IV");
	memcpy(tmp_iv, iv, sizeof tmp_iv);

	/* Encrypt */
	dump_buf(5, buf, len, "Pre-enc");
	AES_cbc_encrypt(buf, buf, len, &aes_key[0], tmp_iv, AES_ENCRYPT);
	dump_buf(5, buf, len, "Post-enc");

	/* Allocate send buffer */
	m->len = len + sizeof iv + sizeof hash + 3 * sizeof(u_int32_t);
	m->buf = (u_int8_t *)malloc(m->len);
	if (!m->buf) {
		free(m);
		free(buf);
		log_err("net_queue: calloc()");
		return -1;
	}
	offset = 0;

	/* Fill it (order must match parsing code in net_read()) */
	v = htonl(m->len - sizeof(u_int32_t));
	memcpy(m->buf + offset, &v, sizeof v);
	offset += sizeof v;
	v = htonl(msgtype);
	memcpy(m->buf + offset, &v, sizeof v);
	offset += sizeof v;
	v = htonl(padlen);
	memcpy(m->buf + offset, &v, sizeof v);
	offset += sizeof v;
	memcpy(m->buf + offset, hash, sizeof hash);
	offset += sizeof hash;
	memcpy(m->buf + offset, iv, sizeof iv);
	offset += sizeof iv;
	memcpy(m->buf + offset, buf, len);
	free(buf);

	if (p)
		net_enqueue(p, m);
	else
		for (p = LIST_FIRST(&cfgstate.peerlist); p;
		     p = LIST_NEXT(p, link))
			net_enqueue(p, m);

	if (!m->refcnt) {
		free(m->buf);
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
	ssize_t		 r;

	for (p = LIST_FIRST(&cfgstate.peerlist); p; p = LIST_NEXT(p, link)) {
		if (p->socket < 0 || !FD_ISSET(p->socket, fds))
			continue;

		qm = SIMPLEQ_FIRST(&p->msgs);
		if (!qm) {
			/* XXX Log */
			continue;
		}
		m = qm->msg;

		log_msg(4, "sending msg %p len %d ref %d to peer %s", m,
		    m->len, m->refcnt, p->name);

		/* write message */
		r = write(p->socket, m->buf, m->len);
		if (r == -1)
			log_err("net_send_messages: write()");
		else if (r < (ssize_t)m->len) {
			/* XXX retransmit? */
			continue;
		}

		/* cleanup */
		SIMPLEQ_REMOVE_HEAD(&p->msgs, next);
		free(qm);

		if (--m->refcnt < 1) {
			log_msg(4, "freeing msg %p", m);
			free(m->buf);
			free(m);
		}
	}
	return;
}

void
net_disconnect_peer(struct syncpeer *p)
{
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
				free(m->buf);
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
}

/*
 * Helper functions (local) below here.
 */

static u_int8_t *
net_read(struct syncpeer *p, u_int32_t *msgtype, u_int32_t *msglen)
{
	u_int8_t	*msg, *blob, *rhash, *iv, hash[SHA_DIGEST_LENGTH];
	u_int32_t	 v, blob_len;
	int		 padlen = 0, offset = 0, r;
	SHA_CTX		 ctx;

	/* Read blob length */
	if (read(p->socket, &v, sizeof v) != (ssize_t)sizeof v)
		return NULL;
	blob_len = ntohl(v);
	if (blob_len < sizeof hash + AES_IV_LEN + 2 * sizeof(u_int32_t))
		return NULL;
	*msglen = blob_len - sizeof hash - AES_IV_LEN - 2 * sizeof(u_int32_t);

	/* Read message blob */
	blob = (u_int8_t *)malloc(blob_len);
	if (!blob) {
		log_err("net_read: malloc()");
		return NULL;
	}
	r = read(p->socket, blob, blob_len);
	if (r == -1) {
		free(blob);
		return NULL;
	} else if (r < (ssize_t)blob_len) {
		/* XXX wait and read more? */
		fprintf(stderr, "net_read: wanted %d, got %d\n", blob_len, r);
		free(blob);
		return NULL;
	}

	offset = 0;
	memcpy(&v, blob + offset, sizeof v);
	*msgtype = ntohl(v);
	offset += sizeof v;

	if (*msgtype > MSG_MAXTYPE) {
		free(blob);
		return NULL;
	}

	memcpy(&v, blob + offset, sizeof v);
	padlen = ntohl(v);
	offset += sizeof v;

	rhash = blob + offset;
	iv    = rhash + sizeof hash;
	msg = (u_int8_t *)malloc(*msglen);
	if (!msg) {
		free(blob);
		return NULL;
	}
	memcpy(msg, iv + AES_IV_LEN, *msglen);

	dump_buf(5, rhash, sizeof hash, "Recv hash");
	dump_buf(5, iv, sizeof iv, "Recv IV");
	dump_buf(5, msg, *msglen, "Pre-decrypt");
	AES_cbc_encrypt(msg, msg, *msglen, &aes_key[1], iv, AES_DECRYPT);
	dump_buf(5, msg, *msglen, "Post-decrypt");
	*msglen -= padlen;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, msg, *msglen);
	SHA1_Final(hash, &ctx);
	dump_buf(5, hash, sizeof hash, "Local hash");

	if (memcmp(hash, rhash, sizeof hash) != 0) {
		free(blob);
		log_msg(0, "net_read: bad msg hash (shared key typo?)");
		return NULL;
	}
	free(blob);
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
		if (p->socket > -1)
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

