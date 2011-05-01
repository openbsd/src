/*	$OpenBSD: bounce.c,v 1.31 2011/05/01 12:57:11 eric Exp $	*/

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
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "client.h"
#include "log.h"

struct client_ctx {
	struct event		 ev;
	struct envelope		 m;
	struct smtp_client	*pcb;
	FILE			*msgfp;
};

int
bounce_session(int fd, struct envelope *m)
{
	struct client_ctx	*cc = NULL;
	int			 msgfd = -1;
	char			*reason;
	FILE			*msgfp = NULL;
	u_int32_t		 msgid;

	msgid = evpid_to_msgid(m->evpid);

	/* get message content */
	if ((msgfd = queue_message_fd_r(Q_QUEUE, msgid)) == -1)
		goto fail;
	msgfp = fdopen(msgfd, "r");
	if (msgfp == NULL)
		fatal("fdopen");
	
	/* init smtp session */
	if ((cc = calloc(1, sizeof(*cc))) == NULL) 
		goto fail;
	cc->pcb = client_init(fd, msgfp, env->sc_hostname, 1);
	cc->m = *m;
	cc->msgfp = msgfp;

	client_ssl_optional(cc->pcb);
	client_sender(cc->pcb, "");
	client_rcpt(cc->pcb, NULL, "%s@%s", m->sender.user,
	    m->sender.domain);

	/* Construct an appropriate reason line. */
	reason = m->session_errorline;
	if (strlen(reason) > 4 && (*reason == '1' || *reason == '6'))
		reason += 4;
	
	/* create message header */
	/* XXX - The Date: header should be added during SMTP pickup. */
	client_printf(cc->pcb,
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
	    m->sender.user, m->sender.domain,
	    time_to_text(time(NULL)),
	    m->recipient.user, m->recipient.domain,
	    reason);

	/* setup event */
	session_socket_blockmode(fd, BM_NONBLOCK);
	event_set(&cc->ev, fd, EV_READ|EV_WRITE, bounce_event, cc);
	event_add(&cc->ev, &cc->pcb->timeout);

	return 1;
fail:
	if (cc)
		fclose(cc->msgfp);
	else if (msgfd != -1)
		close(msgfd);
	free(cc);
	return 0;
}

void
bounce_event(int fd, short event, void *p)
{
	struct client_ctx	*cc = p;
	char			*ep = NULL;

	if (event & EV_TIMEOUT) {
		ep = "150 timeout";
		goto out;
	}

	switch (client_talk(cc->pcb, event & EV_WRITE)) {
	case CLIENT_STOP_WRITE:
		goto ro;
	case CLIENT_WANT_WRITE:
		goto rw;
	case CLIENT_RCPT_FAIL:
		ep = cc->pcb->reply;
		break;
	case CLIENT_DONE:
		ep = cc->pcb->status;
		break;
	default:
		fatalx("bounce_event: unexpected code");
	}

out:
	if (*ep == '2')
		queue_envelope_delete(Q_QUEUE, &cc->m);
	else {
		if (*ep == '5' || *ep == '6')
			cc->m.status = S_MESSAGE_PERMFAILURE;
		else
			cc->m.status = S_MESSAGE_TEMPFAILURE;
		message_set_errormsg(&cc->m, "%s", ep);
		queue_message_update(&cc->m);
	}

	env->stats->runner.active--;
	env->stats->runner.bounces_active--;
	client_close(cc->pcb);
	fclose(cc->msgfp);
	free(cc);
	return;

ro:
	event_set(&cc->ev, fd, EV_READ, bounce_event, cc);
	event_add(&cc->ev, &cc->pcb->timeout);
	return;

rw:
	event_set(&cc->ev, fd, EV_READ|EV_WRITE, bounce_event, cc);
	event_add(&cc->ev, &cc->pcb->timeout);
}
