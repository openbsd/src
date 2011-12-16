/*	$OpenBSD: queue_backend.c,v 1.16 2011/12/16 17:35:00 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <event.h>
#include <imsg.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static int envelope_validate(struct envelope *);

/* fsqueue backend */
extern struct queue_backend	queue_backend_fs;


struct queue_backend *
queue_backend_lookup(enum queue_type type)
{
	switch (type) {
	case QT_FS:
		return &queue_backend_fs;

	default:
		fatalx("invalid queue type");
	}

	return (NULL);
}

int
queue_message_create(enum queue_kind qkind, u_int32_t *msgid)
{
	return env->sc_queue->message(qkind, QOP_CREATE, msgid);
}

int
queue_message_delete(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_DELETE, &msgid);
}

int
queue_message_commit(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_COMMIT, &msgid);
}

int
queue_message_purge(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_PURGE, &msgid);
}

int
queue_message_corrupt(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_CORRUPT, &msgid);
}

int
queue_message_fd_r(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_FD_R, &msgid);
}

int
queue_message_fd_rw(enum queue_kind qkind, u_int32_t msgid)
{
	return env->sc_queue->message(qkind, QOP_FD_RW, &msgid);
}

int
queue_envelope_create(enum queue_kind qkind, struct envelope *ep)
{
	ep->id >>= 32;
	return env->sc_queue->envelope(qkind, QOP_CREATE, ep);
}

int
queue_envelope_delete(enum queue_kind qkind, struct envelope *ep)
{
	return env->sc_queue->envelope(qkind, QOP_DELETE, ep);
}

int
queue_envelope_load(enum queue_kind qkind, u_int64_t evpid, struct envelope *ep)
{
	ep->id = evpid;
	if (env->sc_queue->envelope(qkind, QOP_LOAD, ep))
		return envelope_validate(ep);
	return 0;
}

int
queue_envelope_update(enum queue_kind qkind, struct envelope *ep)
{
	return env->sc_queue->envelope(qkind, QOP_UPDATE, ep);
}

void *
qwalk_new(enum queue_kind kind, u_int32_t msgid)
{
	return env->sc_queue->qwalk_new(kind, msgid);
}

int
qwalk(void *hdl, u_int64_t *evpid)
{
	return env->sc_queue->qwalk(hdl, evpid);
}

void
qwalk_close(void *hdl)
{
	return env->sc_queue->qwalk_close(hdl);
}

u_int32_t
queue_generate_msgid(void)
{
	u_int32_t msgid;

	while((msgid = arc4random_uniform(0xffffffff)) == 0)
		;

	return msgid;
}

u_int64_t
queue_generate_evpid(u_int32_t msgid)
{
	u_int32_t rnd;
	u_int64_t evpid;

	while((rnd = arc4random_uniform(0xffffffff)) == 0)
		;

	evpid = msgid;
	evpid <<= 32;
	evpid |= rnd;

	return evpid;
}


/**/
static int
envelope_validate(struct envelope *ep)
{
	if (ep->version != SMTPD_ENVELOPE_VERSION)
		return 0;

	if ((ep->id & 0xffffffff) == 0 ||
	    ((ep->id >> 32) & 0xffffffff) == 0)
		return 0;

	if (ep->helo[0] == '\0')
		return 0;

	if (ep->hostname[0] == '\0')
		return 0;

	if (ep->errorline[0] != '\0') {
		if (! isdigit(ep->errorline[0]) ||
		    ! isdigit(ep->errorline[1]) ||
		    ! isdigit(ep->errorline[2]) ||
		    ep->errorline[3] != ' ')
			return 0;
	}

	return 1;
}
