/*	$OpenBSD: buffer.c,v 1.10 2003/12/26 17:13:52 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

int	buf_write(int, struct buf *);
void	buf_enqueue(struct msgbuf *, struct buf *);
void	buf_dequeue(struct msgbuf *, struct buf *);

struct buf *
buf_open(ssize_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		return (NULL);
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf->size = len;

	return (buf);
}

int
buf_add(struct buf *buf, void *data, ssize_t len)
{
	if (buf->wpos + len > buf->size)
		return (-1);

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
	return (0);
}

void *
buf_reserve(struct buf *buf, ssize_t len)
{
	void	*b;

	if (buf->wpos + len > buf->size)
		return (NULL);

	b = buf->buf + buf->wpos;
	buf->wpos += len;
	return (b);
}

int
buf_close(struct msgbuf *msgbuf, struct buf *buf)
{
	int	n;

	/* first try to write out directly */
	if (msgbuf->queued == 0) {
		if ((n = buf_write(msgbuf->sock, buf)) < 0)
			return (n);

		if (n == 1) {		/* all data written out */
			buf_free(buf);
			return (0);
		}
	}

	/* we have to queue */
	buf_enqueue(msgbuf, buf);
	return (1);
}

int
buf_write(int sock, struct buf *buf)
{
	ssize_t	n;

	if ((n = write(sock, buf->buf + buf->rpos,
	    buf->size-buf->rpos)) == -1) {
		if (errno == EAGAIN)	/* cannot write immediately */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	if (n < buf->size - buf->rpos) {	/* not all data written yet */
		buf->rpos += n;
		return (0);
	} else
		return (1);
}

void
buf_free(struct buf *buf)
{
	free(buf->buf);
	free(buf);
}

void
msgbuf_init(struct msgbuf *msgbuf)
{
	msgbuf->queued = 0;
	msgbuf->sock = -1;
	TAILQ_INIT(&msgbuf->bufs);
}

void
msgbuf_clear(struct msgbuf *msgbuf)
{
	struct buf	*buf, *next;

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL; buf = next) {
		next = TAILQ_NEXT(buf, entries);
		buf_dequeue(msgbuf, buf);
	}
	msgbuf->queued = 0;
	msgbuf->sock = -1;
}

int
msgbuf_write(struct msgbuf *msgbuf)
{
	/*
	 * possible race here
	 * when we cannot write out data completely from a buffer,
	 * we MUST return and NOT try to write out stuff from later buffers -
	 * the socket might have become writeable again
	 */
	struct buf	*buf, *next;
	int		 n;

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL; buf = next) {
		next = TAILQ_NEXT(buf, entries);
		if ((n = buf_write(msgbuf->sock, buf)) < 0)
			return (n);

		if (n == 1)	/* everything written out */
			buf_dequeue(msgbuf, buf);
		else
			return (0);
	}
	return (0);
}

void
buf_enqueue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_INSERT_TAIL(&msgbuf->bufs, buf, entries);
	msgbuf->queued++;
}

void
buf_dequeue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_REMOVE(&msgbuf->bufs, buf, entries);
	msgbuf->queued--;
	buf_free(buf);
}
