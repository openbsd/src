/*	$OpenBSD: buffer.c,v 1.26 2005/02/01 21:36:02 henning Exp $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

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
	buf->size = len;
	buf->fd = -1;

	return (buf);
}

int
buf_add(struct buf *buf, void *data, size_t len)
{
	if (buf->wpos + len > buf->size)
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
		return (NULL);

	b = buf->buf + buf->wpos;
	buf->wpos += len;
	return (b);
}

int
buf_close(struct msgbuf *msgbuf, struct buf *buf)
{
	buf_enqueue(msgbuf, buf);
	return (1);
}

int
buf_write(int sock, struct buf *buf)
{
	ssize_t	n;

	if ((n = write(sock, buf->buf + buf->rpos,
	    buf->size - buf->rpos)) == -1) {
		if (errno == EAGAIN || errno == ENOBUFS)	/* try later */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	if (buf->rpos + n < buf->size) {	/* not all data written yet */
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
	/*
	 * possible race here
	 * when we cannot write out data completely from a buffer,
	 * we MUST return and NOT try to write out stuff from later buffers -
	 * the socket might have become writeable again
	 */
	struct iovec	 iov[IOV_MAX];
	struct buf	*buf, *next;
	int		 i = 0;
	ssize_t		 n;
	struct msghdr	 msg;
	struct cmsghdr	*cmsg;
	char		 cmsgbuf[CMSG_SPACE(sizeof(int))];

	bzero(&iov, sizeof(iov));
	bzero(&msg, sizeof(msg));
	TAILQ_FOREACH(buf, &msgbuf->bufs, entry) {
		if (i >= IOV_MAX)
			break;
		iov[i].iov_base = buf->buf + buf->rpos;
		iov[i].iov_len = buf->size - buf->rpos;
		i++;
		if (buf->fd != -1)
			break;
	}

	msg.msg_iov = iov;
	msg.msg_iovlen = i;

	if (buf != NULL && buf->fd != -1) {
		msg.msg_control = (caddr_t)cmsgbuf;
		msg.msg_controllen = CMSG_LEN(sizeof(int));
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = buf->fd;
	}

	if ((n = sendmsg(msgbuf->fd, &msg, 0)) == -1) {
		if (errno == EAGAIN || errno == ENOBUFS)	/* try later */
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

int
msgbuf_writebound(struct msgbuf *msgbuf)
{
	/*
	 * possible race here
	 * when we cannot write out data completely from a buffer,
	 * we MUST return and NOT try to write out stuff from later buffers -
	 * the socket might have become writeable again
	 */
	struct buf	*buf;
	int		 n;

	if (!msgbuf_unbounded(msgbuf))
		return (1);

	buf = TAILQ_FIRST(&msgbuf->bufs);
	if ((n = buf_write(msgbuf->fd, buf)) < 0)
		return (n);

	if (n == 1) {	/* everything written out */
		buf_dequeue(msgbuf, buf);
		return (1);
	} else
		return (0);
}

int
msgbuf_unbounded(struct msgbuf *msgbuf)
{
	struct buf	*buf;

	/* return 1 if last buffer was not completely written. */
	buf = TAILQ_FIRST(&msgbuf->bufs);
	if (buf != NULL && buf->rpos != 0)
		return (1);
	else
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

	if (buf->fd != -1)
		close(buf->fd);

	msgbuf->queued--;
	buf_free(buf);
}
