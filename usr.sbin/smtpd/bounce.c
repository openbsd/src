/*	$OpenBSD: bounce.c,v 1.6 2009/08/27 11:37:30 jacekm Exp $	*/

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <err.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "client.h"

struct client_ctx {
	struct event		 ev;
	struct message		 m;
	struct smtp_client	*sp;
};

void		 bounce_event(int, short, void *);

void
bounce_process(struct smtpd *env, struct message *message)
{
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_ENQUEUE, 0, 0, -1,
		message, sizeof(*message));
}

int
bounce_session(struct smtpd *env, int fd, struct message *messagep)
{
	struct client_ctx	*cc = NULL;
	int			 msgfd = -1;

	/* init smtp session */
	if ((cc = calloc(1, sizeof(*cc))) == NULL)
		goto fail;
	if ((cc->sp = client_init(fd, env->sc_hostname)) == NULL)
		goto fail;
	if (client_sender(cc->sp, "") < 0)
		goto fail;
	cc->m = *messagep;

	/* assign recipient */
	if (client_rcpt(cc->sp, "%s@%s", messagep->sender.user,
	    messagep->sender.domain) < 0)
		goto fail;

	/* create message header */
	if (client_data_printf(cc->sp,
	    "From: Mailer Daemon <MAILER-DAEMON@%s>\n"
	    "To: %s@%s\n"
	    "\n"
	    "Hi !\n"
	    "\n"
	    "This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail.\n"
	    "An error has occurred while attempting to deliver a message.\n"
	    "\n"
	    "Recipient: %s@%s\n"
	    "Reason:\n"
	    "%s\n"
	    "\n"
	    "Below is a copy of the original message:\n"
	    "\n",
	    env->sc_hostname,
	    messagep->sender.user, messagep->sender.domain,
	    messagep->recipient.user, messagep->recipient.domain,
	    messagep->session_errorline) < 0)
		goto fail;

	/* append original message */
	if ((msgfd = queue_open_message_file(messagep->message_id)) == -1)
		goto fail;
	if (client_data_fd(cc->sp, msgfd) < 0)
		goto fail;

	/* setup event */
	session_socket_blockmode(fd, BM_NONBLOCK);
	event_set(&cc->ev, fd, EV_READ, bounce_event, cc);
	event_add(&cc->ev, NULL);

	return 1;
fail:
	close(msgfd);
	if (cc && cc->sp)
		client_close(cc->sp);
	free(cc);
	return 0;
}

void
bounce_event(int fd, short event, void *p)
{
	struct client_ctx	*cc = p;
	char			*ep;
	int			(*iofunc)(struct smtp_client *, char **);

	if (event & EV_READ)
		iofunc = client_read;
	else
		iofunc = client_write;

	switch (iofunc(cc->sp, &ep)) {
	case CLIENT_WANT_READ:
		event_set(&cc->ev, fd, EV_READ, bounce_event, cc);
		event_add(&cc->ev, NULL);
		return;
	case CLIENT_WANT_WRITE:
		event_set(&cc->ev, fd, EV_WRITE, bounce_event, cc);
		event_add(&cc->ev, NULL);
		return;
	case CLIENT_DONE:
		queue_remove_envelope(&cc->m);
		break;
	case CLIENT_ERROR:
		message_set_errormsg(&cc->m, "SMTP error: %s", ep);
		message_reset_flags(&cc->m);
		break;
	}
	client_close(cc->sp);
	free(cc);
}
