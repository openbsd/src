/*	$OpenBSD: queue_backend.c,v 1.11 2011/05/16 21:05:52 gilles Exp $	*/

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

/* fsqueue backend */
int	fsqueue_init(void);
int	fsqueue_message(enum queue_kind, enum queue_op, u_int32_t *);
int	fsqueue_envelope(enum queue_kind, enum queue_op , struct envelope *);


struct queue_backend queue_backends[] = {
	{ QT_FS,
	  fsqueue_init,
	  fsqueue_message,
	  fsqueue_envelope }
};

struct queue_backend *
queue_backend_lookup(enum queue_type type)
{
	u_int8_t i;

	for (i = 0; i < nitems(queue_backends); ++i)
		if (queue_backends[i].type == type)
			break;

	if (i == nitems(queue_backends))
		fatalx("invalid queue type");

	return &queue_backends[i];
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
	ep->delivery.id = evpid;
	return env->sc_queue->envelope(qkind, QOP_LOAD, ep);
}

int
queue_envelope_update(enum queue_kind qkind, struct envelope *ep)
{
	return env->sc_queue->envelope(qkind, QOP_UPDATE, ep);
}
