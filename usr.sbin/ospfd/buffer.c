/*	$OpenBSD: buffer.c,v 1.8 2006/02/10 18:30:47 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ospfd.h"

int	buf_realloc(struct buf *, size_t);
void	buf_enqueue(struct msgbuf *, struct buf *);
void	buf_dequeue(struct msgbuf *, struct buf *);

struct buf *
buf_open(size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		return (NULL);
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf->size = buf->max = len;

	return (buf);
}

struct buf *
buf_dynamic(size_t len, size_t max)
{
	struct buf	*buf;

	if (max < len)
		return (NULL);

	if ((buf = buf_open(len)) == NULL)
		return (NULL);

	if (max > 0)
		buf->max = max;

	return (buf);
}

int
buf_realloc(struct buf *buf, size_t len)
{
	u_char	*b;

	/* on static buffers max is eq size and so the following fails */
	if (buf->wpos + len > buf->max) {
		errno = ENOMEM;
		return (-1);
	}

	b = realloc(buf->buf, buf->wpos + len);
	if (b == NULL)
		return (-1);
	buf->buf = b;
	buf->size = buf->wpos + len;

	return (0);
}

int
buf_add(struct buf *buf, void *data, size_t len)
{
	if (buf->wpos + len > buf->size)
		if (buf_realloc(buf, len) == -1)
			return (-1);

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
	return (0);
}

void *
buf_reserve(struct buf *buf, size_t len)
{
	void	*b;

	if (buf->wpos + len > buf->size)
		if (buf_realloc(buf, len) == -1)
			return (NULL);

	b = buf->buf + buf->wpos;
	buf->wpos += len;
	return (b);
}

void *
buf_seek(struct buf *buf, size_t pos, size_t len)
{
	/* only allowed to seek in already written parts */
	if (pos + len > buf->wpos)
		return (NULL);

	return (buf->buf + pos);
}

int
buf_close(struct msgbuf *msgbuf, struct buf *buf)
{
	buf_enqueue(msgbuf, buf);
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
	msgbuf->fd = -1;
	TAILQ_INIT(&msgbuf->bufs);
}

void
msgbuf_clear(struct msgbuf *msgbuf)
{
	struct buf	*buf;

	while ((buf = TAILQ_FIRST(&msgbuf->bufs)) != NULL)
		buf_dequeue(msgbuf, buf);
}

int
msgbuf_write(struct msgbuf *msgbuf)
{
	struct iovec	 iov[IOV_MAX];
	struct buf	*buf, *next;
	int		 i = 0;
	ssize_t		 n;
	struct msghdr	 msg;

	bzero(&iov, sizeof(iov));
	bzero(&msg, sizeof(msg));
	TAILQ_FOREACH(buf, &msgbuf->bufs, entry) {
		if (i >= IOV_MAX)
			break;
		iov[i].iov_base = buf->buf + buf->rpos;
		iov[i].iov_len = buf->size - buf->rpos;
		i++;
	}

	msg.msg_iov = iov;
	msg.msg_iovlen = i;

	if ((n = sendmsg(msgbuf->fd, &msg, 0)) == -1) {
		if (errno == EAGAIN || errno == ENOBUFS ||
		    errno == EINTR)	/* try later */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL && n > 0;
	    buf = next) {
		next = TAILQ_NEXT(buf, entry);
		if (buf->rpos + n >= buf->size) {
			n -= buf->size - buf->rpos;
			buf_dequeue(msgbuf, buf);
		} else {
			buf->rpos += n;
			n = 0;
		}
	}

	return (0);
}

void
buf_enqueue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_INSERT_TAIL(&msgbuf->bufs, buf, entry);
	msgbuf->queued++;
}

void
buf_dequeue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_REMOVE(&msgbuf->bufs, buf, entry);
	msgbuf->queued--;
	buf_free(buf);
}
