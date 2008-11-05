/*	$OpenBSD: debug.c,v 1.2 2008/11/05 12:14:45 sobrado Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

void		debug_display_batch(struct batch *);
void		debug_display_message(struct message *);

void
debug_display_batch(struct batch *p)
{
	log_debug("batch   #     : %qd", p->id);

	if (p->type & T_MDA_BATCH) {
		log_debug("type          : MDA");
	}
	else if (p->type & T_MTA_BATCH) {
		log_debug("type          : MTA");
	}

	if (p->type & T_DAEMON_BATCH) {
		log_debug("mailer-daemon : yes");
	}
	else {
		log_debug("mailer-daemon : no");
	}

	log_debug("creation date : %lu", p->creation);
	log_debug("flags         : %s%s%s%s",
	    (p->flags & F_BATCH_COMPLETE) ? " [COMPLETE] " : "",
	    (p->flags & F_BATCH_RESOLVED) ? " [RESOLVED] " : "",
	    (p->flags & F_BATCH_SCHEDULED) ? " [SCHEDULED] " : "",
	    (p->flags == 0) ? " [NONE] " : "");
}

void
debug_display_message(struct message *p)
{
	log_debug("message #     : %qd", p->id);
	log_debug("session #     : %qd", p->session_id);
	log_debug("batch   #     : %qd", p->batch_id);
	log_debug("message-id    : %s", p->message_id);
	log_debug("message-uid   : %s", p->message_uid);
	log_debug("sender        : %s@%s", p->sender.user,
		p->sender.domain);
	log_debug("recipient     : %s@%s", p->recipient.user,
		p->recipient.domain);

	if (p->type & T_MDA_MESSAGE) {
		log_debug("type          : MDA");
	}
	else if (p->type & T_MTA_MESSAGE) {
		log_debug("type          : MTA");
	}

	if (p->type & T_DAEMON_MESSAGE) {
		log_debug("mailer-daemon : yes");
	}
	else {
		log_debug("mailer-daemon : no");
	}

	log_debug("creation date : %lu", p->creation);
	log_debug("last attempt  : %lu", p->lasttry);
	log_debug("retry count   : %lu", p->retry);
	log_debug("flags         : %lu", p->flags);
	log_debug("status        : %s%s%s%s%s%s%s%s",
	    (p->status & S_MESSAGE_PERMFAILURE) ? " [PERMFAILURE]" : "",
	    (p->status & S_MESSAGE_TEMPFAILURE) ? " [TEMPFAILURE]" : "",
	    (p->status & S_MESSAGE_REJECTED)    ? " [REJECTED]" : "",
	    (p->status & S_MESSAGE_ACCEPTED)    ? " [ACCEPTED]" : "",
	    (p->status & S_MESSAGE_RETRY)       ? " [RETRY]" : "",
	    (p->status & S_MESSAGE_EDNS)        ? " [DNS]" : "",
	    (p->status & S_MESSAGE_ECONNECT)    ? " [CONNECT]" : "",
	    (p->status == 0) ? "[NONE]" : "");

}
