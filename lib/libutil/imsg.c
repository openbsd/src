/*	$OpenBSD: imsg.c,v 1.35 2024/11/21 13:01:07 claudio Exp $	*/

/*
 * Copyright (c) 2023 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imsg.h"

struct imsg_fd {
	TAILQ_ENTRY(imsg_fd)	entry;
	int			fd;
};

static int	 imsg_dequeue_fd(struct imsgbuf *);

void
imsgbuf_init(struct imsgbuf *imsgbuf, int fd)
{
	imsgbuf->w = msgbuf_new();
	memset(&imsgbuf->r, 0, sizeof(imsgbuf->r));
	imsgbuf->fd = fd;
	imsgbuf->pid = getpid();
	TAILQ_INIT(&imsgbuf->fds);
}

int
imsgbuf_read(struct imsgbuf *imsgbuf)
{
	struct msghdr		 msg;
	struct cmsghdr		*cmsg;
	union {
		struct cmsghdr hdr;
		char	buf[CMSG_SPACE(sizeof(int) * 1)];
	} cmsgbuf;
	struct iovec		 iov;
	ssize_t			 n;
	int			 fd;
	struct imsg_fd		*ifd;

	memset(&msg, 0, sizeof(msg));
	memset(&cmsgbuf, 0, sizeof(cmsgbuf));

	iov.iov_base = imsgbuf->r.buf + imsgbuf->r.wpos;
	iov.iov_len = sizeof(imsgbuf->r.buf) - imsgbuf->r.wpos;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	if ((ifd = calloc(1, sizeof(struct imsg_fd))) == NULL)
		return (-1);

again:
	if ((n = recvmsg(imsgbuf->fd, &msg, 0)) == -1) {
		if (errno == EINTR)
			goto again;
		if (errno == EMSGSIZE)
			/*
			 * Not enough fd slots: fd passing failed, retry
			 * to receive the message without fd.
			 * imsg_get_fd() will return -1 in that case.
			 */
			goto again;
		if (errno == EAGAIN) {
			free(ifd);
			return (1);
		}
		goto fail;
	}

	if (n == 0) {	/* connection closed */
		free(ifd);
		return (0);
	}

	imsgbuf->r.wpos += n;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			int i;
			int j;

			/*
			 * We only accept one file descriptor.  Due to C
			 * padding rules, our control buffer might contain
			 * more than one fd, and we must close them.
			 */
			j = ((char *)cmsg + cmsg->cmsg_len -
			    (char *)CMSG_DATA(cmsg)) / sizeof(int);
			for (i = 0; i < j; i++) {
				fd = ((int *)CMSG_DATA(cmsg))[i];
				if (ifd != NULL) {
					ifd->fd = fd;
					TAILQ_INSERT_TAIL(&imsgbuf->fds, ifd,
					    entry);
					ifd = NULL;
				} else
					close(fd);
			}
		}
		/* we do not handle other ctl data level */
	}

	free(ifd);
	return (1);

fail:
	free(ifd);
	return (-1);
}

int
imsgbuf_write(struct imsgbuf *imsgbuf)
{
	return msgbuf_write(imsgbuf->fd, imsgbuf->w);
}

int
imsgbuf_flush(struct imsgbuf *imsgbuf)
{
	while (imsgbuf_queuelen(imsgbuf) > 0) {
		if (imsgbuf_write(imsgbuf) == -1)
			return (-1);
	}
	return (0);
}

void
imsgbuf_clear(struct imsgbuf *imsgbuf)
{
	int	fd;

	msgbuf_clear(imsgbuf->w);
	msgbuf_free(imsgbuf->w);
	imsgbuf->w = NULL;
	while ((fd = imsg_dequeue_fd(imsgbuf)) != -1)
		close(fd);
}

uint32_t
imsgbuf_queuelen(struct imsgbuf *imsgbuf)
{
	return msgbuf_queuelen(imsgbuf->w);
}

ssize_t
imsg_get(struct imsgbuf *imsgbuf, struct imsg *imsg)
{
	struct imsg		 m;
	size_t			 av, left, datalen;

	av = imsgbuf->r.wpos;

	if (IMSG_HEADER_SIZE > av)
		return (0);

	memcpy(&m.hdr, imsgbuf->r.buf, sizeof(m.hdr));
	if (m.hdr.len < IMSG_HEADER_SIZE ||
	    m.hdr.len > MAX_IMSGSIZE) {
		errno = ERANGE;
		return (-1);
	}
	if (m.hdr.len > av)
		return (0);

	m.data = NULL;

	datalen = m.hdr.len - IMSG_HEADER_SIZE;
	imsgbuf->r.rptr = imsgbuf->r.buf + IMSG_HEADER_SIZE;

	if ((m.buf = ibuf_open(datalen)) == NULL)
		return (-1);
	if (datalen != 0) {
		if (ibuf_add(m.buf, imsgbuf->r.rptr, datalen) == -1) {
			/* this should never fail */
			ibuf_free(m.buf);
			return (-1);
		}
		m.data = ibuf_data(m.buf);
	}

	if (m.hdr.flags & IMSGF_HASFD)
		ibuf_fd_set(m.buf, imsg_dequeue_fd(imsgbuf));

	if (m.hdr.len < av) {
		left = av - m.hdr.len;
		memmove(&imsgbuf->r.buf, imsgbuf->r.buf + m.hdr.len, left);
		imsgbuf->r.wpos = left;
	} else
		imsgbuf->r.wpos = 0;

	*imsg = m;
	return (datalen + IMSG_HEADER_SIZE);
}

int
imsg_get_ibuf(struct imsg *imsg, struct ibuf *ibuf)
{
	if (imsg->buf == NULL) {
		errno = EBADMSG;
		return (-1);
	}
	return ibuf_get_ibuf(imsg->buf, ibuf_size(imsg->buf), ibuf);
}

int
imsg_get_data(struct imsg *imsg, void *data, size_t len)
{
	if (len == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (imsg->buf == NULL || ibuf_size(imsg->buf) != len) {
		errno = EBADMSG;
		return (-1);
	}
	return ibuf_get(imsg->buf, data, len);
}

int
imsg_get_fd(struct imsg *imsg)
{
	return ibuf_fd_get(imsg->buf);
}

uint32_t
imsg_get_id(struct imsg *imsg)
{
	return (imsg->hdr.peerid);
}

size_t
imsg_get_len(struct imsg *imsg)
{
	if (imsg->buf == NULL)
		return 0;
	return ibuf_size(imsg->buf);
}

pid_t
imsg_get_pid(struct imsg *imsg)
{
	return (imsg->hdr.pid);
}

uint32_t
imsg_get_type(struct imsg *imsg)
{
	return (imsg->hdr.type);
}

int
imsg_compose(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id, pid_t pid,
    int fd, const void *data, size_t datalen)
{
	struct ibuf	*wbuf;

	if ((wbuf = imsg_create(imsgbuf, type, id, pid, datalen)) == NULL)
		return (-1);

	if (imsg_add(wbuf, data, datalen) == -1)
		return (-1);

	ibuf_fd_set(wbuf, fd);
	imsg_close(imsgbuf, wbuf);

	return (1);
}

int
imsg_composev(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id, pid_t pid,
    int fd, const struct iovec *iov, int iovcnt)
{
	struct ibuf	*wbuf;
	int		 i;
	size_t		 datalen = 0;

	for (i = 0; i < iovcnt; i++)
		datalen += iov[i].iov_len;

	if ((wbuf = imsg_create(imsgbuf, type, id, pid, datalen)) == NULL)
		return (-1);

	for (i = 0; i < iovcnt; i++)
		if (imsg_add(wbuf, iov[i].iov_base, iov[i].iov_len) == -1)
			return (-1);

	ibuf_fd_set(wbuf, fd);
	imsg_close(imsgbuf, wbuf);

	return (1);
}

/*
 * Enqueue imsg with payload from ibuf buf. fd passing is not possible 
 * with this function.
 */
int
imsg_compose_ibuf(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id,
    pid_t pid, struct ibuf *buf)
{
	struct ibuf	*hdrbuf = NULL;
	struct imsg_hdr	 hdr;
	int save_errno;

	if (ibuf_size(buf) + IMSG_HEADER_SIZE > MAX_IMSGSIZE) {
		errno = ERANGE;
		goto fail;
	}

	hdr.type = type;
	hdr.len = ibuf_size(buf) + IMSG_HEADER_SIZE;
	hdr.flags = 0;
	hdr.peerid = id;
	if ((hdr.pid = pid) == 0)
		hdr.pid = imsgbuf->pid;

	if ((hdrbuf = ibuf_open(IMSG_HEADER_SIZE)) == NULL)
		goto fail;
	if (imsg_add(hdrbuf, &hdr, sizeof(hdr)) == -1)
		goto fail;

	ibuf_close(imsgbuf->w, hdrbuf);
	ibuf_close(imsgbuf->w, buf);
	return (1);

 fail:
	save_errno = errno;
	ibuf_free(buf);
	ibuf_free(hdrbuf);
	errno = save_errno;
	return (-1);
}

/*
 * Forward imsg to another channel. Any attached fd is closed.
 */
int
imsg_forward(struct imsgbuf *imsgbuf, struct imsg *msg)
{
	struct ibuf	*wbuf;
	size_t		 len = 0;

	if (msg->buf != NULL) {
		ibuf_rewind(msg->buf);
		len = ibuf_size(msg->buf);
	}

	if ((wbuf = imsg_create(imsgbuf, msg->hdr.type, msg->hdr.peerid,
	    msg->hdr.pid, len)) == NULL)
		return (-1);

	if (msg->buf != NULL) {
		if (ibuf_add_ibuf(wbuf, msg->buf) == -1) {
			ibuf_free(wbuf);
			return (-1);
		}
	}

	imsg_close(imsgbuf, wbuf);
	return (1);
}

struct ibuf *
imsg_create(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id, pid_t pid,
    size_t datalen)
{
	struct ibuf	*wbuf;
	struct imsg_hdr	 hdr;

	datalen += IMSG_HEADER_SIZE;
	if (datalen > MAX_IMSGSIZE) {
		errno = ERANGE;
		return (NULL);
	}

	hdr.type = type;
	hdr.flags = 0;
	hdr.peerid = id;
	if ((hdr.pid = pid) == 0)
		hdr.pid = imsgbuf->pid;
	if ((wbuf = ibuf_dynamic(datalen, MAX_IMSGSIZE)) == NULL) {
		return (NULL);
	}
	if (imsg_add(wbuf, &hdr, sizeof(hdr)) == -1)
		return (NULL);

	return (wbuf);
}

int
imsg_add(struct ibuf *msg, const void *data, size_t datalen)
{
	if (datalen)
		if (ibuf_add(msg, data, datalen) == -1) {
			ibuf_free(msg);
			return (-1);
		}
	return (datalen);
}

void
imsg_close(struct imsgbuf *imsgbuf, struct ibuf *msg)
{
	struct imsg_hdr	*hdr;

	hdr = (struct imsg_hdr *)msg->buf;

	hdr->flags &= ~IMSGF_HASFD;
	if (ibuf_fd_avail(msg))
		hdr->flags |= IMSGF_HASFD;
	hdr->len = ibuf_size(msg);

	ibuf_close(imsgbuf->w, msg);
}

void
imsg_free(struct imsg *imsg)
{
	ibuf_free(imsg->buf);
}

static int
imsg_dequeue_fd(struct imsgbuf *imsgbuf)
{
	int		 fd;
	struct imsg_fd	*ifd;

	if ((ifd = TAILQ_FIRST(&imsgbuf->fds)) == NULL)
		return (-1);

	fd = ifd->fd;
	TAILQ_REMOVE(&imsgbuf->fds, ifd, entry);
	free(ifd);

	return (fd);
}
