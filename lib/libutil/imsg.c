/*	$OpenBSD: imsg.c,v 1.22 2023/11/18 07:14:13 claudio Exp $	*/

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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imsg.h"

int	 imsg_fd_overhead = 0;

static int	 imsg_dequeue_fd(struct imsgbuf *);

void
imsg_init(struct imsgbuf *imsgbuf, int fd)
{
	msgbuf_init(&imsgbuf->w);
	memset(&imsgbuf->r, 0, sizeof(imsgbuf->r));
	imsgbuf->fd = fd;
	imsgbuf->w.fd = fd;
	imsgbuf->pid = getpid();
	TAILQ_INIT(&imsgbuf->fds);
}

ssize_t
imsg_read(struct imsgbuf *imsgbuf)
{
	struct msghdr		 msg;
	struct cmsghdr		*cmsg;
	union {
		struct cmsghdr hdr;
		char	buf[CMSG_SPACE(sizeof(int) * 1)];
	} cmsgbuf;
	struct iovec		 iov;
	ssize_t			 n = -1;
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
	if (getdtablecount() + imsg_fd_overhead +
	    (int)((CMSG_SPACE(sizeof(int))-CMSG_SPACE(0))/sizeof(int))
	    >= getdtablesize()) {
		errno = EAGAIN;
		free(ifd);
		return (-1);
	}

	if ((n = recvmsg(imsgbuf->fd, &msg, 0)) == -1) {
		if (errno == EINTR)
			goto again;
		goto fail;
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

fail:
	free(ifd);
	return (n);
}

ssize_t
imsg_get(struct imsgbuf *imsgbuf, struct imsg *imsg)
{
	size_t			 av, left, datalen;

	av = imsgbuf->r.wpos;

	if (IMSG_HEADER_SIZE > av)
		return (0);

	memcpy(&imsg->hdr, imsgbuf->r.buf, sizeof(imsg->hdr));
	if (imsg->hdr.len < IMSG_HEADER_SIZE ||
	    imsg->hdr.len > MAX_IMSGSIZE) {
		errno = ERANGE;
		return (-1);
	}
	if (imsg->hdr.len > av)
		return (0);
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	imsgbuf->r.rptr = imsgbuf->r.buf + IMSG_HEADER_SIZE;
	if (datalen == 0)
		imsg->data = NULL;
	else if ((imsg->data = malloc(datalen)) == NULL)
		return (-1);

	if (imsg->hdr.flags & IMSGF_HASFD)
		imsg->fd = imsg_dequeue_fd(imsgbuf);
	else
		imsg->fd = -1;

	if (datalen != 0)
		memcpy(imsg->data, imsgbuf->r.rptr, datalen);

	if (imsg->hdr.len < av) {
		left = av - imsg->hdr.len;
		memmove(&imsgbuf->r.buf, imsgbuf->r.buf + imsg->hdr.len, left);
		imsgbuf->r.wpos = left;
	} else
		imsgbuf->r.wpos = 0;

	return (datalen + IMSG_HEADER_SIZE);
}

int
imsg_compose(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id, pid_t pid,
    int fd, const void *data, uint16_t datalen)
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
	int		 i, datalen = 0;

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

int
imsg_compose_ibuf(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id,
    pid_t pid, struct ibuf *buf)
{
	struct ibuf	*wbuf = NULL;
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

	if ((wbuf = ibuf_open(IMSG_HEADER_SIZE)) == NULL)
		goto fail;
	if (imsg_add(wbuf, &hdr, sizeof(hdr)) == -1)
		goto fail;

	ibuf_close(&imsgbuf->w, wbuf);
	ibuf_close(&imsgbuf->w, buf);
	return (1);

 fail:
	save_errno = errno;
	ibuf_free(buf);
	ibuf_free(wbuf);
	errno = save_errno;
	return (-1);
}

struct ibuf *
imsg_create(struct imsgbuf *imsgbuf, uint32_t type, uint32_t id, pid_t pid,
    uint16_t datalen)
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
imsg_add(struct ibuf *msg, const void *data, uint16_t datalen)
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

	ibuf_close(&imsgbuf->w, msg);
}

void
imsg_free(struct imsg *imsg)
{
	freezero(imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE);
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

int
imsg_flush(struct imsgbuf *imsgbuf)
{
	while (imsgbuf->w.queued)
		if (msgbuf_write(&imsgbuf->w) <= 0)
			return (-1);
	return (0);
}

void
imsg_clear(struct imsgbuf *imsgbuf)
{
	int	fd;

	msgbuf_clear(&imsgbuf->w);
	while ((fd = imsg_dequeue_fd(imsgbuf)) != -1)
		close(fd);
}
