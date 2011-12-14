/*	$OpenBSD: queue_shared.c,v 1.55 2011/12/14 18:42:27 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

int
bounce_record_message(struct envelope *e, struct envelope *bounce)
{
	u_int32_t msgid;

	bzero(bounce, sizeof(*bounce));

	if (e->type == D_BOUNCE) {
		log_debug("mailer daemons loop detected !");
		return 0;
	}

	*bounce = *e;
	 bounce->type = D_BOUNCE;
	 bounce->status &= ~DS_PERMFAILURE;

	msgid = evpid_to_msgid(e->id);
	if (! queue_message_create(Q_BOUNCE, &msgid))
		return 0;

	bounce->id = msgid_to_evpid(msgid);
	if (! queue_envelope_create(Q_BOUNCE, bounce))
		return 0;

	return queue_message_commit(Q_BOUNCE, msgid);
}

void
queue_message_update(struct envelope *e)
{
	e->batch_id = 0;
	e->status &= ~(DS_ACCEPTED|DS_REJECTED);
	e->retry++;


	if (e->status & DS_PERMFAILURE) {
		if (e->type != D_BOUNCE &&
		    e->sender.user[0] != '\0') {
			struct envelope bounce;

			bounce_record_message(e, &bounce);
		}
		queue_envelope_delete(Q_QUEUE, e);
		return;
	}

	if (e->status & DS_TEMPFAILURE) {
		e->status &= ~DS_TEMPFAILURE;
		queue_envelope_update(Q_QUEUE, e);
		return;
	}

	/* no error, remove envelope */
	queue_envelope_delete(Q_QUEUE, e);
}
