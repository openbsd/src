/*	$OpenBSD: buffer.c,v 1.1 2003/12/17 11:46:54 henning Exp $ */

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

TAILQ_HEAD(bufs, buf) bufs = TAILQ_HEAD_INITIALIZER(bufs);

void	buf_enqueue(struct buf *);
void	buf_dequeue(struct buf *);

struct buf *
buf_open(struct peer *peer, int sock, size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		return (NULL);
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		return (NULL);
	}

	buf->size = len;
	buf->peer = peer;
	buf->sock = sock;

	return (buf);
}

int
buf_add(struct buf *buf, u_char *data, size_t len)
{
	if (buf->wpos + len > buf->size)
		return (-1);

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
	return (0);
}

u_char *
buf_reserve(struct buf *buf, size_t len)
{
	u_char	*b;

	if (buf->wpos + len > buf->size)
		return (NULL);

	b = buf->buf + buf->wpos;
	buf->wpos += len;
	return (b);
}

int
buf_close(struct buf *buf)
{
	/*
	 * we first try to write out directly
	 * if that fails we add the buffer to the queue
	 */

	int	n;

	if ((n = buf_write(buf)) == -1)
		return (-1);

	if (n == 1) {		/* all data written out */
		buf_free(buf);
		return (0);
	}

	/* we have to queue */
	buf_enqueue(buf);
	return (1);
}

int
buf_write(struct buf *buf)
{
	size_t	n;

	if ((n = write(buf->sock, buf->buf + buf->rpos,
	    buf->size-buf->rpos)) == -1) {
		if (errno == EAGAIN)	/* cannot write immediately */
			return (0);
		else {
			if (buf->peer != NULL)
				log_err(buf->peer, "write error");
			else
				logit(LOG_CRIT, "pipe write error: %s",
				    strerror(errno));
			return (-1);
		}
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
buf_peer_remove(struct peer *peer)
{
	struct buf	*buf, *next;

	for (buf = TAILQ_FIRST(&bufs); buf != NULL; buf = next) {
		next = TAILQ_NEXT(buf, entries);
		if (buf->peer == peer)
			buf_dequeue(buf);
	}
}

int
buf_peer_write(struct peer *peer)
{
	/*
	 * possible race here
	 * when we cannot write out data completely from a buffer,
	 * we MUST return and NOT try to write out stuff from later buffers -
	 * the socket might have become writeable again
	 */
	struct buf	*buf, *next;
	int		 n;

	for (buf = TAILQ_FIRST(&bufs); buf != NULL; buf = next) {
		next = TAILQ_NEXT(buf, entries);
		if (buf->peer == peer) {
			if ((n = buf_write(buf)) == -1)
				return (-1);
			if (n == 1)	/* everything written out */
				buf_dequeue(buf);
			else
				return (0);
		}
	}
	return (0);
}

int
buf_sock_write(int sock)
{
	/*
	 * possible race here
	 * when we cannot write out data completely from a buffer,
	 * we MUST return and NOT try to write out stuff from later buffers -
	 * the socket might have become writeable again
	 */
	struct buf	*buf, *next;
	int		 n, cleared = 0;

	for (buf = TAILQ_FIRST(&bufs); buf != NULL; buf = next) {
		next = TAILQ_NEXT(buf, entries);
		if (buf->sock == sock) {
			if ((n = buf_write(buf)) == -1)
				return (-1);
			if (n == 1) {	/* everything written out */
				buf_dequeue(buf);
				cleared++;
			} else
				return (cleared);
		}
	}
	return (cleared);
}

void
buf_enqueue(struct buf *buf)
{
	/* might want a tailq per peer w/ pointers to the bufs */
	TAILQ_INSERT_TAIL(&bufs, buf, entries);
	if (buf->peer != NULL)
		buf->peer->queued_writes++;
}

void
buf_dequeue(struct buf *buf)
{
	TAILQ_REMOVE(&bufs, buf, entries);
	if (buf->peer != NULL)
		buf->peer->queued_writes--;
	buf_free(buf);
}
