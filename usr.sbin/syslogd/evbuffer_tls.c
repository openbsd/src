/*	$OpenBSD: evbuffer_tls.c,v 1.4 2015/07/06 16:12:16 millert Exp $ */

/*
 * Copyright (c) 2002-2004 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2014-2015 Alexander Bluhm <bluhm@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tls.h>

#include "evbuffer_tls.h"

/* prototypes */

void bufferevent_read_pressure_cb(struct evbuffer *, size_t, size_t, void *);
int evtls_read(struct evbuffer *, int, int, struct tls *);
int evtls_write(struct evbuffer *, int, struct tls *);

static int
bufferevent_add(struct event *ev, int timeout)
{
	struct timeval tv, *ptv = NULL;

	if (timeout) {
		timerclear(&tv);
		tv.tv_sec = timeout;
		ptv = &tv;
	}

	return (event_add(ev, ptv));
}

static void
buffertls_readcb(int fd, short event, void *arg)
{
	struct buffertls *buftls = arg;
	struct bufferevent *bufev = buftls->bt_bufev;
	struct tls *ctx = buftls->bt_ctx;
	int res = 0;
	short what = EVBUFFER_READ;
	size_t len;
	int howmuch = -1;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto error;
	}

	/*
	 * If we have a high watermark configured then we don't want to
	 * read more data than would make us reach the watermark.
	 */
	if (bufev->wm_read.high != 0) {
		howmuch = bufev->wm_read.high - EVBUFFER_LENGTH(bufev->input);
		/* we might have lowered the watermark, stop reading */
		if (howmuch <= 0) {
			struct evbuffer *buf = bufev->input;
			event_del(&bufev->ev_read);
			evbuffer_setcb(buf,
			    bufferevent_read_pressure_cb, bufev);
			return;
		}
	}

	res = evtls_read(bufev->input, fd, howmuch, ctx);
	switch (res) {
	case TLS_READ_AGAIN:
		event_set(&bufev->ev_read, fd, EV_READ, buffertls_readcb,
		    buftls);
		goto reschedule;
	case TLS_WRITE_AGAIN:
		event_set(&bufev->ev_read, fd, EV_WRITE, buffertls_readcb,
		    buftls);
		goto reschedule;
	case -1:
		if (errno == EAGAIN || errno == EINTR)
			goto reschedule;
		/* error case */
		what |= EVBUFFER_ERROR;
		break;
	case 0:
		/* eof case */
		what |= EVBUFFER_EOF;
		break;
	}
	if (res <= 0)
		goto error;

	event_set(&bufev->ev_read, fd, EV_READ, buffertls_readcb, buftls);
	bufferevent_add(&bufev->ev_read, bufev->timeout_read);

	/* See if this callbacks meets the water marks */
	len = EVBUFFER_LENGTH(bufev->input);
	if (bufev->wm_read.low != 0 && len < bufev->wm_read.low)
		return;
	if (bufev->wm_read.high != 0 && len >= bufev->wm_read.high) {
		struct evbuffer *buf = bufev->input;
		event_del(&bufev->ev_read);

		/* Now schedule a callback for us when the buffer changes */
		evbuffer_setcb(buf, bufferevent_read_pressure_cb, bufev);
	}

	/* Invoke the user callback - must always be called last */
	if (bufev->readcb != NULL)
		(*bufev->readcb)(bufev, bufev->cbarg);
	return;

 reschedule:
	bufferevent_add(&bufev->ev_read, bufev->timeout_read);
	return;

 error:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

static void
buffertls_writecb(int fd, short event, void *arg)
{
	struct buffertls *buftls = arg;
	struct bufferevent *bufev = buftls->bt_bufev;
	struct tls *ctx = buftls->bt_ctx;
	int res = 0;
	short what = EVBUFFER_WRITE;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto error;
	}

	if (EVBUFFER_LENGTH(bufev->output) != 0) {
		res = evtls_write(bufev->output, fd, ctx);
		switch (res) {
		case TLS_READ_AGAIN:
			event_set(&bufev->ev_write, fd, EV_READ,
			    buffertls_writecb, buftls);
			goto reschedule;
		case TLS_WRITE_AGAIN:
			event_set(&bufev->ev_write, fd, EV_WRITE,
			    buffertls_writecb, buftls);
			goto reschedule;
		case -1:
			if (errno == EAGAIN || errno == EINTR ||
			    errno == EINPROGRESS)
				goto reschedule;
			/* error case */
			what |= EVBUFFER_ERROR;
			break;
		case 0:
			/* eof case */
			what |= EVBUFFER_EOF;
			break;
		}
		if (res <= 0)
			goto error;
	}
	buftls->bt_flags &= ~BT_WRITE_AGAIN;

	event_set(&bufev->ev_write, fd, EV_WRITE, buffertls_writecb, buftls);
	if (EVBUFFER_LENGTH(bufev->output) != 0)
		bufferevent_add(&bufev->ev_write, bufev->timeout_write);

	/*
	 * Invoke the user callback if our buffer is drained or below the
	 * low watermark.
	 */
	if (bufev->writecb != NULL &&
	    EVBUFFER_LENGTH(bufev->output) <= bufev->wm_write.low)
		(*bufev->writecb)(bufev, bufev->cbarg);

	return;

 reschedule:
	buftls->bt_flags |= BT_WRITE_AGAIN;
	if (EVBUFFER_LENGTH(bufev->output) != 0)
		bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

 error:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

static void
buffertls_connectcb(int fd, short event, void *arg)
{
	struct buffertls *buftls = arg;
	struct bufferevent *bufev = buftls->bt_bufev;
	struct tls *ctx = buftls->bt_ctx;
	const char *hostname = buftls->bt_hostname;
	int res = 0;
	short what = EVBUFFER_CONNECT;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto error;
	}

	res = tls_connect_socket(ctx, fd, hostname);
	switch (res) {
	case TLS_READ_AGAIN:
		event_set(&bufev->ev_write, fd, EV_READ,
		    buffertls_connectcb, buftls);
		goto reschedule;
	case TLS_WRITE_AGAIN:
		event_set(&bufev->ev_write, fd, EV_WRITE,
		    buffertls_connectcb, buftls);
		goto reschedule;
	case -1:
		if (errno == EAGAIN || errno == EINTR ||
		    errno == EINPROGRESS)
			goto reschedule;
		/* error case */
		what |= EVBUFFER_ERROR;
		break;
	}
	if (res < 0)
		goto error;

	/*
	 * There might be data available in the tls layer.  Try
	 * an read operation and setup the callbacks.  Call the read
	 * callback after enabling the write callback to give the
	 * read error handler a chance to disable the write event.
	 */
	event_set(&bufev->ev_write, fd, EV_WRITE, buffertls_writecb, buftls);
	if (EVBUFFER_LENGTH(bufev->output) != 0)
		bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	buffertls_readcb(fd, 0, buftls);

	return;

 reschedule:
	bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

 error:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

void
buffertls_set(struct buffertls *buftls, struct bufferevent *bufev,
    struct tls *ctx, int fd)
{
	bufferevent_setfd(bufev, fd);
	event_set(&bufev->ev_read, fd, EV_READ, buffertls_readcb, buftls);
	event_set(&bufev->ev_write, fd, EV_WRITE, buffertls_writecb, buftls);
	buftls->bt_bufev = bufev;
	buftls->bt_ctx = ctx;
	buftls->bt_flags = 0;
}

void
buffertls_connect(struct buffertls *buftls, int fd, const char *hostname)
{
	struct bufferevent *bufev = buftls->bt_bufev;

	event_del(&bufev->ev_read);
	event_del(&bufev->ev_write);

	buftls->bt_hostname = hostname;
	event_set(&bufev->ev_write, fd, EV_WRITE, buffertls_connectcb, buftls);
	bufferevent_add(&bufev->ev_write, bufev->timeout_write);
}

/*
 * Reads data from a file descriptor into a buffer.
 */

#define EVBUFFER_MAX_READ	4096

int
evtls_read(struct evbuffer *buf, int fd, int howmuch, struct tls *ctx)
{
	u_char *p;
	size_t len, oldoff = buf->off;
	int n = EVBUFFER_MAX_READ;

	if (ioctl(fd, FIONREAD, &n) == -1 || n <= 0) {
		n = EVBUFFER_MAX_READ;
	} else if (n > EVBUFFER_MAX_READ && n > howmuch) {
		/*
		 * It's possible that a lot of data is available for
		 * reading.  We do not want to exhaust resources
		 * before the reader has a chance to do something
		 * about it.  If the reader does not tell us how much
		 * data we should read, we artifically limit it.
		 */
		if ((size_t)n > buf->totallen << 2)
			n = buf->totallen << 2;
		if (n < EVBUFFER_MAX_READ)
			n = EVBUFFER_MAX_READ;
	}
	if (howmuch < 0 || howmuch > n)
		howmuch = n;

	/* If we don't have FIONREAD, we might waste some space here */
	if (evbuffer_expand(buf, howmuch) == -1)
		return (-1);

	/* We can append new data at this point */
	p = buf->buffer + buf->off;

	n = tls_read(ctx, p, howmuch, &len);
	if (n < 0 || len == 0)
		return (n);

	buf->off += len;

	/* Tell someone about changes in this buffer */
	if (buf->off != oldoff && buf->cb != NULL)
		(*buf->cb)(buf, oldoff, buf->off, buf->cbarg);

	return (len);
}

int
evtls_write(struct evbuffer *buffer, int fd, struct tls *ctx)
{
	size_t len;
	int n;

	n = tls_write(ctx, buffer->buffer, buffer->off, &len);
	if (n < 0 || len == 0)
		return (n);

	evbuffer_drain(buffer, len);

	return (len);
}
