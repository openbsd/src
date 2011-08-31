/*	$OpenBSD: filter.c,v 1.2 2011/08/31 18:56:30 gilles Exp $	*/

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
#include <sys/uio.h>

#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "filter.h"

static struct filter_internals {
	struct event	ev;
	struct imsgbuf	ibuf;

	int (*helo_cb)(u_int64_t, struct filter_helo *, void *);
	void *helo_cb_arg;

	int (*ehlo_cb)(u_int64_t, struct filter_helo *, void *);
	void *ehlo_cb_arg;

	int (*mail_cb)(u_int64_t, struct filter_mail *, void *);
	void *mail_cb_arg;

	int (*rcpt_cb)(u_int64_t, struct filter_rcpt *, void *);
	void *rcpt_cb_arg;

	int (*dataline_cb)(u_int64_t, struct filter_dataline *, void *);
	void *dataline_cb_arg;
} fi;

static void filter_handler(int, short, void *);
static void filter_register_callback(enum filter_type, void *, void *);

void
filter_init(void)
{
	bzero(&fi, sizeof (fi));

	imsg_init(&fi.ibuf, 0);

	event_init();
	event_set(&fi.ev, 0, EV_READ, filter_handler, (void *)&fi);
	event_add(&fi.ev, NULL);
}

void
filter_loop(void)
{
	if (event_dispatch() < 0)
		errx(1, "event_dispatch");
}

void
filter_register_helo_callback(int (*cb)(u_int64_t, struct filter_helo *, void *), void *cb_arg)
{
	filter_register_callback(FILTER_HELO, cb, cb_arg);
}

void
filter_register_ehlo_callback(int (*cb)(u_int64_t, struct filter_helo *, void *), void *cb_arg)
{
	filter_register_callback(FILTER_EHLO, cb, cb_arg);
}

void
filter_register_mail_callback(int (*cb)(u_int64_t, struct filter_mail *, void *), void *cb_arg)
{
	filter_register_callback(FILTER_MAIL, cb, cb_arg);
}

void
filter_register_rcpt_callback(int (*cb)(u_int64_t, struct filter_rcpt *, void *), void *cb_arg)
{
	filter_register_callback(FILTER_RCPT, cb, cb_arg);
}

void
filter_register_dataline_callback(int (*cb)(u_int64_t, struct filter_dataline *, void *), void *cb_arg)
{
	filter_register_callback(FILTER_DATALINE, cb, cb_arg);
}

static void
filter_register_callback(enum filter_type type, void *cb, void *cb_arg)
{
	switch (type) {
	case FILTER_HELO:
		fi.helo_cb = cb;
		fi.helo_cb_arg = cb_arg;
		break;

	case FILTER_EHLO:
		fi.ehlo_cb = cb;
		fi.ehlo_cb_arg = cb_arg;
		break;

	case FILTER_MAIL:
		fi.mail_cb = cb;
		fi.mail_cb_arg = cb_arg;
		break;

	case FILTER_RCPT:
		fi.rcpt_cb = cb;
		fi.rcpt_cb_arg = cb_arg;
		break;

	case FILTER_DATALINE:
		fi.dataline_cb = cb;
		fi.dataline_cb_arg = cb_arg;
		break;
	}
}

static void
filter_handler(int fd, short event, void *p)
{
	struct imsg		imsg;
	ssize_t			n;
	short			evflags = EV_READ;
	struct filter_msg	fm;

	if (event & EV_READ) {
		n = imsg_read(&fi.ibuf);
		if (n == -1)
			err(1, "imsg_read");
		if (n == 0) {
			event_del(&fi.ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&fi.ibuf.w) == -1)
			err(1, "msgbuf_write");
		if (fi.ibuf.w.queued)
			evflags |= EV_WRITE;
	}

	for (;;) {
		n = imsg_get(&fi.ibuf, &imsg);
		if (n == -1)
			errx(1, "imsg_get");
		if (n == 0)
			break;

		if ((imsg.hdr.len - IMSG_HEADER_SIZE)
		    != sizeof(fm))
			errx(1, "corrupted imsg");

		memcpy(&fm, imsg.data, sizeof (fm));
		if (fm.version != FILTER_API_VERSION)
			errx(1, "API version mismatch");

		switch (imsg.hdr.type) {
		case FILTER_HELO:
			if (fi.helo_cb == NULL)
				goto ignore;
			fm.code = fi.helo_cb(fm.cl_id, &fm.u.helo,
			    fi.helo_cb_arg);
			break;
		case FILTER_EHLO:
			if (fi.ehlo_cb == NULL)
				goto ignore;
			fm.code = fi.ehlo_cb(fm.cl_id, &fm.u.helo,
			    fi.ehlo_cb_arg);
			break;
		case FILTER_MAIL:
			if (fi.mail_cb == NULL)
				goto ignore;
			fm.code = fi.mail_cb(fm.cl_id, &fm.u.mail,
			    fi.mail_cb_arg);
			break;
		case FILTER_RCPT:
			if (fi.rcpt_cb == NULL)
				goto ignore;
			fm.code = fi.rcpt_cb(fm.cl_id, &fm.u.rcpt,
			    fi.rcpt_cb_arg);
			break;
		case FILTER_DATALINE:
			if (fi.dataline_cb == NULL)
				goto ignore;
			fm.code = fi.dataline_cb(fm.cl_id, &fm.u.dataline,
			    fi.dataline_cb_arg);
			break;
		default:
			errx(1, "unsupported imsg");
		}

		if (! fm.code)
			fm.code = -1;
			
		imsg_compose(&fi.ibuf, imsg.hdr.type, 0, 0, -1, &fm, sizeof fm);
		evflags |= EV_WRITE;
		imsg_free(&imsg);
	}

	event_set(&fi.ev, 0, evflags, filter_handler, &fi);
	event_add(&fi.ev, NULL);
	return;

ignore:
	imsg_free(&imsg);
	fm.code = 0;
	imsg_compose(&fi.ibuf, imsg.hdr.type, 0, 0, -1, &fm, sizeof fm);
	evflags |= EV_WRITE;
	event_set(&fi.ev, 0, evflags, filter_handler, &fi);
	event_add(&fi.ev, NULL);
}
