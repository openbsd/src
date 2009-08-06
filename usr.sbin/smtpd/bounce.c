/*	$OpenBSD: bounce.c,v 1.4 2009/08/06 14:27:41 gilles Exp $	*/

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@openbsd.org>
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

void
bounce_process(struct smtpd *env, struct message *message)
{
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_ENQUEUE, 0, 0, -1,
		message, sizeof(*message));
}

int
bounce_session(struct smtpd *env, int fd, struct message *messagep)
{
	char *buf, *lbuf;
	size_t len;
	FILE *fp;
	enum session_state state = S_INIT;

	fp = fdopen(fd, "r+");
	if (fp == NULL)
		goto fail;

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, "malloc");
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		if (! bounce_session_switch(env, fp, &state, buf, messagep))
			goto fail;
	}
	free(lbuf);

	fclose(fp);
	return 1;
fail:
	if (fp != NULL)
		fclose(fp);
	else
		close(fd);
	return 0;
}

int
bounce_session_switch(struct smtpd *env, FILE *fp, enum session_state *state, char *line,
	struct message *messagep)
{
	switch (*state) {
	case S_INIT:
		if (strncmp(line, "220 ", 4) != 0)
			return 0;
		fprintf(fp, "HELO %s\r\n", env->sc_hostname);
		*state = S_GREETED;
		break;

	case S_GREETED:
		if (strncmp(line, "250 ", 4) != 0)
			return 0;

		fprintf(fp, "MAIL FROM: <MAILER-DAEMON@%s>\r\n", env->sc_hostname);
		*state = S_MAIL;
		break;

	case S_MAIL:
		if (strncmp(line, "250 ", 4) != 0)
			return 0;

		fprintf(fp, "RCPT TO: <%s@%s>\r\n", messagep->sender.user,
			messagep->sender.domain);
		*state = S_RCPT;
		break;

	case S_RCPT:
		if (strncmp(line, "250 ", 4) != 0)
			return 0;

		fprintf(fp, "DATA\r\n");
		*state = S_DATA;
		break;

	case S_DATA: {
		int msgfd;
		FILE *srcfp;

		if (strncmp(line, "354 ", 4) != 0)
			return 0;

		msgfd = queue_open_message_file(messagep->message_id);
		if (msgfd == -1)
			return 0;

		srcfp = fdopen(msgfd, "r");
		if (srcfp == NULL) {
			close(msgfd);
			return 0;
		}

		fprintf(fp, "From: Mailer Daemon <MAILER-DAEMON@%s>\r\n",
			env->sc_hostname);
		fprintf(fp, "To: %s@%s\r\n",
			messagep->sender.user, messagep->sender.domain);
		fprintf(fp, "Subject: Delivery attempt failure\r\n");
		fprintf(fp, "\r\n");

		fprintf(fp, "Hi !\r\n");
		fprintf(fp, "This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail.\r\n");
		fprintf(fp, "An error has occurred while attempting to deliver a message.\r\n");
		fprintf(fp, "\r\n");
		fprintf(fp, "Recipient: %s@%s\r\n", messagep->recipient.user,
			messagep->recipient.domain);
		fprintf(fp, "Reason:\r\n");
		fprintf(fp, "%s\r\n", messagep->session_errorline);

		fprintf(fp, "\r\n");
		fprintf(fp, "Below is a copy of the original message:\r\n\r\n");

		if (! file_copy(fp, srcfp, NULL, 0, 1)) {
			return 0;
		}

		fprintf(fp, ".\r\n");

		*state = S_DONE;
		break;
	}
	case S_DONE:
		if (strncmp(line, "250 ", 4) != 0)
			return 0;

		fprintf(fp, "QUIT\r\n");
		*state = S_QUIT;
		break;

	case S_QUIT:
		if (strncmp(line, "221 ", 4) != 0)
			return 0;

		break;

	default:
		errx(1, "bounce_session_switch: unknown state.");
	}

	fflush(fp);
	return 1;
}
