/*	$OpenBSD: pfkey.c,v 1.1 2005/03/30 18:44:49 ho Exp $	*/

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


#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <net/pfkeyv2.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sasyncd.h"

struct pfkey_msg 
{
	SIMPLEQ_ENTRY(pfkey_msg)	next;

	u_int8_t	*buf;
	u_int32_t	 len;
};

SIMPLEQ_HEAD(, pfkey_msg)		pfkey_msglist;

static const char *msgtypes[] = {
	"RESERVED", "GETSPI", "UPDATE", "ADD", "DELETE", "GET", "ACQUIRE",
	"REGISTER", "EXPIRE", "FLUSH", "DUMP", "X_PROMISC", "X_ADDFLOW",
	"X_DELFLOW", "X_GRPSPIS", "X_ASKPOLICY"
};

#define CHUNK sizeof(u_int64_t)

static const char *pfkey_print_type(struct sadb_msg *msg);

static int
pfkey_write(u_int8_t *buf, ssize_t len)
{
	struct sadb_msg *msg = (struct sadb_msg *)buf;

	if (cfgstate.pfkey_socket == -1)
		return 0;

	if (write(cfgstate.pfkey_socket, buf, len) != len) {
		log_err("pfkey: msg %s write() failed",
		    pfkey_print_type(msg), cfgstate.pfkey_socket);
		return -1;
	}

	return 0;
}

int
pfkey_set_promisc(void)
{
	struct sadb_msg	msg;
	static u_int32_t seq = 1;

	memset(&msg, 0, sizeof msg);
	msg.sadb_msg_version = PF_KEY_V2;
	msg.sadb_msg_seq = seq++;
	msg.sadb_msg_satype = 1; /* Special; 1 to enable, 0 to disable */
	msg.sadb_msg_type = SADB_X_PROMISC;
	msg.sadb_msg_pid = getpid();
	msg.sadb_msg_len = sizeof msg / CHUNK;

	return pfkey_write((u_int8_t *)&msg, sizeof msg);
}

static const char *
pfkey_print_type(struct sadb_msg *msg)
{
	if (msg->sadb_msg_type < sizeof msgtypes / sizeof msgtypes[0])
		return msgtypes[msg->sadb_msg_type];
	else
		return "<unknown>";
}

static int
pfkey_handle_message(struct sadb_msg *m)
{
	struct sadb_msg *msg = m;

	/*
	 * Report errors, but ignore for DELETE (both isakmpd and kernel will
	 * expire the SA, if the kernel is first, DELETE returns failure).
	 */
	if (msg->sadb_msg_errno && msg->sadb_msg_type != SADB_DELETE) {
		errno = msg->sadb_msg_errno;
		log_err("pfkey error (%s)", pfkey_print_type(msg));
	}

	/* We only want promiscuous messages here, skip all others. */
	if (msg->sadb_msg_type != SADB_X_PROMISC ||
	    (msg->sadb_msg_len * CHUNK) <= 2 * sizeof *msg) {
		free(m);
		return 0;
	}
	msg++;

	/*
	 * We should not listen to PFKEY messages when we are not running
	 * as MASTER, or the pid is our own.
	 */
	if (cfgstate.runstate != MASTER ||
	    msg->sadb_msg_pid == (u_int32_t)getpid()) {
		free(m);
		return 0;
	}

	log_msg(3, "pfkey: got %s len %u seq %d", pfkey_print_type(msg),
	    msg->sadb_msg_len * CHUNK, msg->sadb_msg_seq);

	switch (msg->sadb_msg_type) {
	case SADB_X_PROMISC:
	case SADB_DUMP:
	case SADB_GET:
	case SADB_GETSPI:
		/* Some messages should not be synced. */
		free(m);
		break;

	case SADB_UPDATE:
		/*
		 * Tweak -- the peers do not have a larval SA to update, so
		 * instead we ADD it here.
		 */
		msg->sadb_msg_type = SADB_ADD;
		/* FALLTHROUGH */

	default:
		/* The rest should just be passed along to our peers. */
		return net_queue(NULL, MSG_PFKEYDATA, (u_int8_t *)m, sizeof *m,
		    msg->sadb_msg_len * CHUNK);
	}

	return 0;
}

static int
pfkey_read(void)
{
	struct sadb_msg  hdr, *msg;
	u_int8_t	*data;
	ssize_t		 datalen;
	int		 fd = cfgstate.pfkey_socket;

	if (recv(fd, &hdr, sizeof hdr, MSG_PEEK) != sizeof hdr) {
		log_err("pfkey_read: recv() failed");
		return -1;
	}
	datalen = hdr.sadb_msg_len * CHUNK;
	data = (u_int8_t *)malloc(datalen);
	if (!data) {
		log_err("pfkey_read: malloc(%lu) failed", datalen);
		return -1;
	}
	msg = (struct sadb_msg *)data;

	if (read(fd, data, datalen) != datalen) {
		log_err("pfkey_read: read() failed, %lu bytes", datalen);
		free(data);
		return -1;
	}

	return pfkey_handle_message(msg);
}

int
pfkey_init(int reinit)
{
	int fd;

	fd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
	if (fd == -1) {
		perror("failed to open PF_KEY socket");
		return -1;
	}
	cfgstate.pfkey_socket = fd;

	if (reinit) {
		if (cfgstate.runstate == MASTER)
			pfkey_set_promisc();
		return (fd > -1 ? 0 : -1);
	}

	SIMPLEQ_INIT(&pfkey_msglist);
	return 0;
}

void
pfkey_set_rfd(fd_set *fds)
{
	if (cfgstate.pfkey_socket != -1)
		FD_SET(cfgstate.pfkey_socket, fds);
}

void
pfkey_set_pending_wfd(fd_set *fds)
{
	if (cfgstate.pfkey_socket != -1 && SIMPLEQ_FIRST(&pfkey_msglist))
		FD_SET(cfgstate.pfkey_socket, fds);
}

void
pfkey_read_message(fd_set *fds)
{
	if (cfgstate.pfkey_socket != -1)
		if (FD_ISSET(cfgstate.pfkey_socket, fds))
			(void)pfkey_read();
}

void
pfkey_send_message(fd_set *fds)
{
	struct pfkey_msg *pmsg = SIMPLEQ_FIRST(&pfkey_msglist);

	if (!pmsg || !FD_ISSET(cfgstate.pfkey_socket, fds))
		return;

	if (cfgstate.pfkey_socket == -1)
		if (pfkey_init(1)) /* Reinit socket */
			return;

	(void)pfkey_write(pmsg->buf, pmsg->len);

	SIMPLEQ_REMOVE_HEAD(&pfkey_msglist, next);
	free(pmsg->buf);
	free(pmsg);
	return;
}

int
pfkey_queue_message(u_int8_t *data, u_int32_t datalen)
{
	struct pfkey_msg	*pmsg;
	struct sadb_msg		*sadb = (struct sadb_msg *)data;
	static u_int32_t	 seq = 1;

	pmsg = (struct pfkey_msg *)malloc(sizeof *pmsg);
	if (!pmsg) {
		log_err("malloc()");
		return -1;
	}
	memset(pmsg, 0, sizeof *pmsg);

	pmsg->buf = data;
	pmsg->len = datalen;

	sadb->sadb_msg_pid = getpid();
	sadb->sadb_msg_seq = seq++;
	log_msg(3, "sync: pfkey %s len %d seq %d", pfkey_print_type(sadb),
	    sadb->sadb_msg_len * CHUNK, sadb->sadb_msg_seq);

	SIMPLEQ_INSERT_TAIL(&pfkey_msglist, pmsg, next);
	return 0;
}

void
pfkey_shutdown(void)
{
	struct pfkey_msg *p = SIMPLEQ_FIRST(&pfkey_msglist);

	while ((p = SIMPLEQ_FIRST(&pfkey_msglist))) {
		SIMPLEQ_REMOVE_HEAD(&pfkey_msglist, next);
		free(p->buf);
		free(p);
	}

	if (cfgstate.pfkey_socket > -1)
		close(cfgstate.pfkey_socket);
}

/* ------------------------------------------------------------------------- */

void
pfkey_snapshot(void *v)
{
	struct sadb_msg *m;
	struct sadb_ext *e;
	u_int8_t	*buf;
	size_t		 sz, mlen, elen;
	int		 mib[5];

	mib[0] = CTL_NET;
	mib[1] = PF_KEY;
	mib[2] = PF_KEY_V2;
	mib[3] = NET_KEY_SADB_DUMP;
	mib[4] = 0; /* Unspec SA type */

	if (timer_add("pfkey_snapshot", 60, pfkey_snapshot, NULL))
		log_err("pfkey_snapshot: failed to renew event");

	if (sysctl(mib, sizeof mib / sizeof mib[0], NULL, &sz, NULL, 0) == -1
	    || sz == 0)
		return;
	if ((buf = malloc(sz)) == NULL) {
		log_err("malloc");
		return;
	}
	if (sysctl(mib, sizeof mib / sizeof mib[0], buf, &sz, NULL, 0) == -1) {
		log_err("sysctl");
		return;
	}

	m = (struct sadb_msg *)buf;
	while (m < (struct sadb_msg *)(buf + sz) && m->sadb_msg_len > 0) {
		mlen = m->sadb_msg_len * CHUNK;
		
		fprintf(stderr, "snapshot: sadb_msg %p type %s len %u\n",
		    m, pfkey_print_type(m), mlen);

		e = (struct sadb_ext *)(m + 1);
		while ((u_int8_t *)e - (u_int8_t *)m < mlen && 
		    e->sadb_ext_len > 0) {
			elen = e->sadb_ext_len * CHUNK;
			fprintf(stderr, "ext %p len %u\n", e, elen);
			e = (struct sadb_ext *)((u_int8_t *)e + elen);
		}
		/* ... */
		m = (struct sadb_msg *)((u_int8_t *)m + mlen);
	}
	memset(buf, 0, sz);
	free(buf);
	return;
}
