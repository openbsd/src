/*	$OpenBSD: bounce.c,v 1.47 2012/08/18 15:39:26 eric Exp $	*/

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
#include <errno.h>
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

#define BOUNCE_MAXRUN	10
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
	TAILQ_ENTRY(bounce)	 entry;
	uint64_t		 id;
	uint32_t		 msgid;
	TAILQ_HEAD(, envelope)	 envelopes;
	size_t			 count;
	FILE			*msgfp;
	int			 state;
	struct iobuf		 iobuf;
	struct io		 io;

	struct event		 evt;
};

static void bounce_drain(void);
static void bounce_commit(uint32_t);
static void bounce_send(struct bounce *, const char *, ...);
static int  bounce_next(struct bounce *);
static void bounce_status(struct bounce *, const char *, ...);
static void bounce_io(struct io *, int);
static void bounce_timeout(int, short, void *);
static void bounce_free(struct bounce *);

static struct tree bounces_by_msgid = SPLAY_INITIALIZER(&bounces_by_msgid);
static struct tree bounces_by_uid = SPLAY_INITIALIZER(&bounces_by_uid);

static int running = 0;
static TAILQ_HEAD(, bounce) runnable = TAILQ_HEAD_INITIALIZER(runnable);

void
bounce_add(uint64_t evpid)
{
	struct envelope	*evp;
	struct bounce	*bounce;
	struct timeval	 tv;

	evp = xcalloc(1, sizeof *evp, "bounce_add");

	if (queue_envelope_load(evpid, evp) == 0) {
		evp->id = evpid;
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
		    IMSG_QUEUE_DELIVERY_PERMFAIL, 0, 0, -1, evp, sizeof *evp);
		free(evp);
		return;
	}

	if (evp->type != D_BOUNCE)
		errx(1, "bounce: evp:%016" PRIx64 " is not of type D_BOUNCE!",
		    evp->id);
	evp->lasttry = time(NULL);

	bounce = tree_get(&bounces_by_msgid, evpid_to_msgid(evpid));
	if (bounce == NULL) {
		bounce = xcalloc(1, sizeof(*bounce), "bounce_add");
		bounce->msgid = evpid_to_msgid(evpid);
		tree_xset(&bounces_by_msgid, bounce->msgid, bounce);

		log_debug("bounce: %p: new bounce for msg:%08" PRIx32,
		    bounce, bounce->msgid);

		TAILQ_INIT(&bounce->envelopes);
		evtimer_set(&bounce->evt, bounce_timeout, &bounce->msgid);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		evtimer_add(&bounce->evt, &tv);
	}

	log_debug("bounce: %p: adding evp:%16" PRIx64, bounce, evp->id);

	TAILQ_INSERT_TAIL(&bounce->envelopes, evp, entry);
	bounce->count += 1;

	if (bounce->id)
		return;

	evtimer_del(&bounce->evt);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(&bounce->evt, &tv);
}

void
bounce_run(uint64_t id, int fd)
{
	struct bounce	*bounce;
	int		 msgfd;
	
	log_trace(TRACE_BOUNCE, "bounce: run %016" PRIx64 " fd %i", id, fd);

	bounce = tree_xpop(&bounces_by_uid, id);

	if (fd == -1) {
		bounce_status(bounce, "failed to receive enqueueing socket");
		bounce_free(bounce);
		return;
	}

	if ((msgfd = queue_message_fd_r(bounce->msgid)) == -1) {
		bounce_status(bounce, "could not open message fd");
		bounce_free(bounce);
		return;
	}

	if ((bounce->msgfp = fdopen(msgfd, "r")) == NULL) {
		log_warn("bounce_run: fdopen");
		bounce_status(bounce, "error %i in fdopen", errno);
		close(msgfd);
		return;
	}

	bounce->state = BOUNCE_EHLO;
	iobuf_init(&bounce->iobuf, 0, 0);
	io_init(&bounce->io, fd, bounce, bounce_io, &bounce->iobuf);
	io_set_timeout(&bounce->io, 30000);
	io_set_read(&bounce->io);
}

static void
bounce_commit(uint32_t msgid)
{
	struct bounce	*bounce;

	log_trace(TRACE_BOUNCE, "bounce: commit msg:%08" PRIx32, msgid);

	bounce = tree_xget(&bounces_by_msgid, msgid);
	bounce->id = generate_uid();
	evtimer_del(&bounce->evt);
	TAILQ_INSERT_TAIL(&runnable, bounce, entry);

	bounce_drain();
}

static void
bounce_timeout(int fd, short ev, void *arg)
{
	uint32_t *msgid = arg;

	bounce_commit(*msgid);
}

static void
bounce_drain()
{
	struct bounce	*bounce;

	while ((bounce = TAILQ_FIRST(&runnable))) {

		if (running >= BOUNCE_MAXRUN) {
			log_debug("bounce: max session reached");
			return;
		}

		TAILQ_REMOVE(&runnable, bounce, entry);
		if (TAILQ_FIRST(&bounce->envelopes) == NULL) {
			log_debug("bounce: %p: no envelopes", bounce);
			bounce_free(bounce);
			continue;
		}

		tree_xset(&bounces_by_uid, bounce->id, bounce);

		log_debug("bounce: %p: requesting enqueue socket with id 0x%016" PRIx64,
		    bounce, bounce->id);

		imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_ENQUEUE,
		    0, 0, -1, &bounce->id, sizeof (bounce->id));

		running += 1;
	}
}

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
	struct envelope	*evp;
	char		*line;
	size_t		 len, s;

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
		evp = TAILQ_FIRST(&bounce->envelopes);
		bounce_send(bounce, "RCPT TO: <%s@%s>",
		    evp->sender.user, evp->sender.domain);
		bounce->state = BOUNCE_DATA;
		break;

	case BOUNCE_DATA:
		bounce_send(bounce, "DATA");
		bounce->state = BOUNCE_DATA_NOTICE;
		break;

	case BOUNCE_DATA_NOTICE:
		/* Construct an appropriate reason line. */

		/* prevent more envelopes from being added to this bounce */
		tree_xpop(&bounces_by_msgid, bounce->msgid);

		evp = TAILQ_FIRST(&bounce->envelopes);

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
		    "\n",
		    env->sc_hostname,
		    evp->sender.user, evp->sender.domain,
		    time_to_text(time(NULL)));

		TAILQ_FOREACH(evp, &bounce->envelopes, entry) {
			line = evp->errorline;
			if (strlen(line) > 4 && (*line == '1' || *line == '6'))
				line += 4;
			iobuf_fqueue(&bounce->iobuf,
			    "Recipient: %s@%s\n"
			    "Reason: %s\n",
			    evp->dest.user, evp->dest.domain, line);
		}

		iobuf_fqueue(&bounce->iobuf,
		    "\n"
		    "Below is a copy of the original message:\n"
		    "\n");

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
	struct envelope	*evp;

	/* ignore if the envelopes have already been updated/deleted */
	if (TAILQ_FIRST(&bounce->envelopes) == NULL)
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

	while ((evp = TAILQ_FIRST(&bounce->envelopes))) {
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
		TAILQ_REMOVE(&bounce->envelopes, evp, entry);
		free(evp);
	}

	free(status);
}

static void
bounce_free(struct bounce *bounce)
{
	struct envelope	*evp;

	log_debug("bounce: %p: deleting session", bounce);

	/* if the envelopes where not sent, it is still in the tree */
	tree_pop(&bounces_by_msgid, bounce->msgid);

	while ((evp = TAILQ_FIRST(&bounce->envelopes))) {
		TAILQ_REMOVE(&bounce->envelopes, evp, entry);
		free(evp);
	}

	if (bounce->msgfp)
		fclose(bounce->msgfp);
	iobuf_clear(&bounce->iobuf);
	io_clear(&bounce->io);
	free(bounce);

	running -= 1;
	bounce_drain();
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
