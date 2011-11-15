/*	$OpenBSD: queue_shared.c,v 1.54 2011/11/15 23:06:39 gilles Exp $	*/

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

int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);

void		display_envelope(struct envelope *, int);
void		getflag(u_int *, int, char *, char *, size_t);

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

void
show_queue(enum queue_kind kind, int flags)
{
	struct qwalk	*q;
	struct envelope	 envelope;
	u_int64_t	 evpid;

	log_init(1);

	if (chroot(PATH_SPOOL) == -1 || chdir(".") == -1)
		err(1, "%s", PATH_SPOOL);

	q = qwalk_new(kind, 0);

	while (qwalk(q, &evpid)) {
		if (! queue_envelope_load(kind, evpid, &envelope))
			continue;
		display_envelope(&envelope, flags);
	}

	qwalk_close(q);
}

void
display_envelope(struct envelope *e, int flags)
{
	char	 status[128];

	status[0] = '\0';

	getflag(&e->status, DS_TEMPFAILURE, "TEMPFAIL",
	    status, sizeof(status));

	if (e->status)
		errx(1, "%016" PRIx64 ": unexpected status 0x%04x", e->id,
		    e->status);

	getflag(&e->flags, DF_BOUNCE, "BOUNCE",
	    status, sizeof(status));
	getflag(&e->flags, DF_AUTHENTICATED, "AUTH",
	    status, sizeof(status));
	getflag(&e->flags, DF_ENQUEUED, "ENQUEUED",
	    status, sizeof(status));
	getflag(&e->flags, DF_INTERNAL, "INTERNAL",
	    status, sizeof(status));

	if (e->flags)
		errx(1, "%016" PRIx64 ": unexpected flags 0x%04x", e->id,
		    e->flags);
	
	if (status[0])
		status[strlen(status) - 1] = '\0';
	else
		strlcpy(status, "-", sizeof(status));

	switch (e->type) {
	case D_MDA:
		printf("MDA");
		break;
	case D_MTA:
		printf("MTA");
		break;
	case D_BOUNCE:
		printf("BOUNCE");
		break;
	default:
		printf("UNKNOWN");
	}
	
	printf("|%016" PRIx64 "|%s|%s@%s|%s@%s|%" PRId64 "|%" PRId64 "|%u",
	    e->id,
	    status,
	    e->sender.user, e->sender.domain,
	    e->dest.user, e->dest.domain,
	    (int64_t) e->lasttry,
	    (int64_t) e->expire,
	    e->retry);
	
	if (e->errorline[0] != '\0')
		printf("|%s", e->errorline);

	printf("\n");
}

void
getflag(u_int *bitmap, int bit, char *bitstr, char *buf, size_t len)
{
	if (*bitmap & bit) {
		*bitmap &= ~bit;
		strlcat(buf, bitstr, len);
		strlcat(buf, ",", len);
	}
}
