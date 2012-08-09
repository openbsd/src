/*	$OpenBSD: bounce.c,v 1.44 2012/08/09 09:48:02 eric Exp $	*/

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <inttypes.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define BOUNCE_HIWAT	65535

enum {
	BOUNCE_EHLO,
	BOUNCE_MAIL,
	BOUNCE_RCPT,
	BOUNCE_DATA,
	BOUNCE_DATA_NOTICE,
	BOUNCE_DATA_MESSAGE,
	BOUNCE_QUIT,
	BOUNCE_CLOSE,
};

struct bounce {
	struct envelope	 evp;
	FILE		*msgfp;
	int		 state;
	struct iobuf	 iobuf;
	struct io	 io;
};

static void bounce_send(struct bounce *, const char *, ...);
static int  bounce_next(struct bounce *);
static void bounce_status(struct bounce *, const char *, ...);
static void bounce_io(struct io *, int);

static void
bounce_send(struct bounce *bounce, const char *fmt, ...)
{
	va_list	 ap;
	char	*p;
	int	 len;

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		fatal("bounce: vasprintf");
	va_end(ap);

	log_trace(TRACE_BOUNCE, "bounce: %p: >>> %s", bounce, p);

	iobuf_fqueue(&bounce->iobuf, "%s\n", p);

        free(p);
}

/* This can simplified once we support PIPELINING */
static int
bounce_next(struct bounce *bounce)
{
	char	*line;
	size_t	 len, s;

	switch(bounce->state) {
	case BOUNCE_EHLO:
		bounce_send(bounce, "EHLO %s", env->sc_hostname);
		bounce->state = BOUNCE_MAIL;
		break;

	case BOUNCE_MAIL:
		bounce_send(bounce, "MAIL FROM: <>");
		bounce->state = BOUNCE_RCPT;
		break;

	case BOUNCE_RCPT:
		bounce_send(bounce, "RCPT TO: <%s@%s>",
		    bounce->evp.sender.user, bounce->evp.sender.domain);
		bounce->state = BOUNCE_DATA;
		break;

	case BOUNCE_DATA:
		bounce_send(bounce, "DATA");
		bounce->state = BOUNCE_DATA_NOTICE;
		break;

	case BOUNCE_DATA_NOTICE:
		/* Construct an appropriate reason line. */
		line = bounce->evp.errorline;
		if (strlen(line) > 4 && (*line == '1' || *line == '6'))
			line += 4;
	
		iobuf_fqueue(&bounce->iobuf,
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
		    bounce->evp.sender.user, bounce->evp.sender.domain,
		    time_to_text(time(NULL)),
		    bounce->evp.dest.user, bounce->evp.dest.domain,
		    line);

		log_trace(TRACE_BOUNCE, "bounce: %p: >>> [... %zu bytes ...]",
		    bounce, iobuf_queued(&bounce->iobuf));

		bounce->state = BOUNCE_DATA_MESSAGE;
		break;

	case BOUNCE_DATA_MESSAGE:

		s = iobuf_queued(&bounce->iobuf);

		while (iobuf_len(&bounce->iobuf) < BOUNCE_HIWAT) {
			line = fgetln(bounce->msgfp, &len);
			if (line == NULL)
				break;
			line[len - 1] = '\0';
			if(len == 2 && line[0] == '.')
				iobuf_queue(&bounce->iobuf, ".", 1);
			iobuf_queue(&bounce->iobuf, line, len);
			iobuf_queue(&bounce->iobuf, "\n", 1);
		}

		if (ferror(bounce->msgfp)) {
			bounce_status(bounce, "460 Error reading message");
			return (-1);
		}

		log_trace(TRACE_BOUNCE, "bounce: %p: >>> [... %zu bytes ...]",
		    bounce, iobuf_queued(&bounce->iobuf) - s);

		if (feof(bounce->msgfp)) {
			bounce_send(bounce, ".");
			bounce->state = BOUNCE_QUIT;
		}

		break;

	case BOUNCE_QUIT:
		bounce_send(bounce, "QUIT");
		bounce->state = BOUNCE_CLOSE;
		break;

	default:
		fatalx("bounce: bad state");
	}

	return (0);
}

static void
bounce_status(struct bounce *bounce, const char *fmt, ...)
{
	va_list		 ap;
	char		*status;
	int		 len, msg;
	struct envelope *evp;

	/* ignore if the envelope has already been updated/deleted */
	if (bounce->evp.id == 0)
		return;

	va_start(ap, fmt);
	if ((len = vasprintf(&status, fmt, ap)) == -1)
		fatal("bounce: vasprintf");
	va_end(ap);

	if (*status == '2')
		msg = IMSG_QUEUE_DELIVERY_OK;
	else if (*status == '5' || *status == '6')
		msg = IMSG_QUEUE_DELIVERY_PERMFAIL;
	else
		msg = IMSG_QUEUE_DELIVERY_TEMPFAIL;

	evp = &bounce->evp;
	if (msg == IMSG_QUEUE_DELIVERY_TEMPFAIL) {
		evp->retry++;
		envelope_set_errormsg(evp, "%s", status);
		queue_envelope_update(evp);
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER], msg, 0, 0, -1,
		    evp, sizeof *evp);
	} else {
		queue_envelope_delete(evp);
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER], msg, 0, 0, -1,
		    &evp->id, sizeof evp->id);
	}

	bounce->evp.id = 0;
	free(status);
}

static void
bounce_free(struct bounce *bounce)
{
	log_debug("bounce: %p: deleting session", bounce);

	fclose(bounce->msgfp);
	iobuf_clear(&bounce->iobuf);
	io_clear(&bounce->io);
	free(bounce);
}

static void
bounce_io(struct io *io, int evt)
{
	struct bounce	*bounce = io->arg;
	const char	*error;
	char		*line, *msg;
	int		 cont;
	size_t		 len;

	log_trace(TRACE_IO, "bounce: %p: %s %s",
	    bounce, io_strevent(evt), io_strio(io));

	switch (evt) {
	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&bounce->iobuf, &len);
		if (line == NULL) {
			if (iobuf_len(&bounce->iobuf) >= SMTP_LINE_MAX) {
				bounce_status(bounce, "150 Input too long");
				bounce_free(bounce);
				return;
			}
			iobuf_normalize(&bounce->iobuf);
			break;
		} 

		log_trace(TRACE_BOUNCE, "bounce: %p: <<< %s", bounce, line);

		if ((error = parse_smtp_response(line, len, &msg, &cont))) {
			bounce_status(bounce, "150 Bad response: %s", error);
			bounce_free(bounce);
			return;
		}
		if (cont)
			goto nextline;

		if (bounce->state == BOUNCE_CLOSE) {
			bounce_free(bounce);
			return;
		}

		if (line[0] != '2' && line[0] != '3') {		/* fail */
			bounce_status(bounce, "%s", line);
			bounce->state = BOUNCE_QUIT;
		} else if (bounce->state == BOUNCE_QUIT) {	/* accepted */
			bounce_status(bounce, "%s", line);
		}

		if (bounce_next(bounce) == -1) {
			bounce_free(bounce);
			return;
		}

		io_set_write(io);
		break;

	case IO_LOWAT:
		if (bounce->state == BOUNCE_DATA_MESSAGE)
			bounce_next(bounce);
		if (iobuf_queued(&bounce->iobuf) == 0)
			io_set_read(io);
		break;

	default:
		bounce_status(bounce, "442 i/o error %i", evt);
		bounce_free(bounce);
		break;
	}
}

int
bounce_session(int fd, struct envelope *evp)
{
	struct bounce	*bounce = NULL;
	int		 msgfd = -1;
	FILE		*msgfp = NULL;
	u_int32_t	 msgid;

	msgid = evpid_to_msgid(evp->id);

	log_debug("bounce: bouncing envelope id %016" PRIx64 "", evp->id);

	/* get message content */
	if ((msgfd = queue_message_fd_r(msgid)) == -1)
		return (0);

	msgfp = fdopen(msgfd, "r");
	if (msgfp == NULL) {
		log_warn("bounce: fdopen");
		close(msgfd);
		return (0);
	}

	if ((bounce = calloc(1, sizeof(*bounce))) == NULL) {
		log_warn("bounce: calloc");
		fclose(msgfp);
		return (0);
	}

	bounce->evp = *evp;
	bounce->msgfp = msgfp;
	bounce->state = BOUNCE_EHLO;

	iobuf_init(&bounce->iobuf, 0, 0);
	io_init(&bounce->io, fd, bounce, bounce_io, &bounce->iobuf);
	io_set_timeout(&bounce->io, 30000);
	io_set_read(&bounce->io);
	return (1);
}

int
bounce_record_message(struct envelope *e, struct envelope *bounce)
{
	if (e->type == D_BOUNCE) {
		log_debug("mailer daemons loop detected !");
		return 0;
	}

	*bounce = *e;
	bounce->type = D_BOUNCE;
	bounce->retry = 0;
	bounce->lasttry = 0;
	return (queue_envelope_create(bounce));
}
