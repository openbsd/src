/*	$OpenBSD: bounce.c,v 1.12 2009/11/11 10:04:05 chl Exp $	*/

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
#include <time.h>
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
	char			*reason;

	/* init smtp session */
	if ((cc = calloc(1, sizeof(*cc))) == NULL)
		goto fail;
	if ((cc->sp = client_init(fd, env->sc_hostname)) == NULL)
		goto fail;
	cc->m = *messagep;

	if (client_ssl_optional(cc->sp) < 0)
		goto fail;

	/* assign recipient */
	if (client_rcpt(cc->sp, "%s@%s", messagep->sender.user,
	    messagep->sender.domain) < 0)
		goto fail;

	/* Construct an appropriate reason line. */
	reason = messagep->session_errorline;
	if (strlen(reason) > 4 && (*reason == '1' || *reason == '6'))
		reason += 4;
	
	/* create message header */
	/* XXX - The Date: header should be added during SMTP pickup. */
	if (client_data_printf(cc->sp,
	    "Subject: Delivery status notification\n"
	    "From: Mailer Daemon <MAILER-DAEMON@%s>\n"
	    "To: %s@%s\n"
	    "Date: %s\n"
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
	    time_to_text(time(NULL)),
	    messagep->recipient.user, messagep->recipient.domain,
	    reason) < 0)
		goto fail;

	/* append original message */
	if ((msgfd = queue_open_message_file(messagep->message_id)) == -1)
		goto fail;
	if (client_data_fd(cc->sp, msgfd) < 0)
		goto fail;
	close(msgfd);
	msgfd = -1;

	/* setup event */
	session_socket_blockmode(fd, BM_NONBLOCK);
	event_set(&cc->ev, fd, EV_WRITE, bounce_event, cc);
	event_add(&cc->ev, client_timeout(cc->sp));

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
	char			*ep = NULL;
	int			 error = 0;
	int			(*iofunc)(struct smtp_client *);

	if (event & EV_TIMEOUT) {
		message_set_errormsg(&cc->m, "150 timeout");
		cc->m.status = S_MESSAGE_TEMPFAILURE;
		queue_message_update(&cc->m);
		client_close(cc->sp);
		free(cc);
		return;
	}

	if (event & EV_READ)
		iofunc = client_read;
	else
		iofunc = client_write;

	switch (iofunc(cc->sp)) {
	case CLIENT_WANT_READ:
		event_set(&cc->ev, fd, EV_READ, bounce_event, cc);
		event_add(&cc->ev, client_timeout(cc->sp));
		return;
	case CLIENT_WANT_WRITE:
		event_set(&cc->ev, fd, EV_WRITE, bounce_event, cc);
		event_add(&cc->ev, client_timeout(cc->sp));
		return;
	case CLIENT_ERROR:
		error = 1;
	case CLIENT_RCPT_FAIL:
	case CLIENT_DONE:
		break;
	}

	if (error)
		ep = client_strerror(cc->sp);
	else
		ep = client_reply(cc->sp);

	if (*ep == '2')
		queue_remove_envelope(&cc->m);
	else {
		if (*ep == '5')
			cc->m.status = S_MESSAGE_PERMFAILURE;
		else
			cc->m.status = S_MESSAGE_TEMPFAILURE;
		message_set_errormsg(&cc->m, "%s", ep);
		queue_message_update(&cc->m);
	}

	client_close(cc->sp);
	free(cc);
}
