/*	$OpenBSD: imsg.c,v 1.32 2004/09/16 17:58:13 henning Exp $ */

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

void
imsg_init(struct imsgbuf *ibuf, int fd)
{
	msgbuf_init(&ibuf->w);
	bzero(&ibuf->r, sizeof(ibuf->r));
	ibuf->fd = fd;
	ibuf->w.fd = fd;
	ibuf->pid = getpid();
	TAILQ_INIT(&ibuf->fds);
}

int
imsg_read(struct imsgbuf *ibuf)
{
	struct msghdr		 msg;
	struct cmsghdr		*cmsg;
	char			 cmsgbuf[CMSG_SPACE(sizeof(int) * 16)];
	struct iovec		 iov;
	ssize_t			 n;
	int			 fd;
	struct imsg_fd		*ifd;

	bzero(&msg, sizeof(msg));
	iov.iov_base = ibuf->r.buf + ibuf->r.wpos;
	iov.iov_len = sizeof(ibuf->r.buf) - ibuf->r.wpos;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);

	if ((n = recvmsg(ibuf->fd, &msg, 0)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_warn("imsg_read: pipe read error");
			return (-1);
		}
		return (0);
	}

	ibuf->r.wpos += n;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			fd = (*(int *)CMSG_DATA(cmsg));
			if ((ifd = calloc(1, sizeof(struct imsg_fd))) == NULL)
				fatal("imsg_read calloc");
			ifd->fd = fd;
			TAILQ_INSERT_TAIL(&ibuf->fds, ifd, entry);
		} else
			log_warn("imsg_read: got unexpected ctl data level %d "
			    "type %d", cmsg->cmsg_level, cmsg->cmsg_type);
	}

	return (n);
}

int
imsg_get(struct imsgbuf *ibuf, struct imsg *imsg)
{
	ssize_t			 datalen = 0;
	size_t			 av, left;

	av = ibuf->r.wpos;

	if (IMSG_HEADER_SIZE > av)
		return (0);

	memcpy(&imsg->hdr, ibuf->r.buf, sizeof(imsg->hdr));
	if (imsg->hdr.len < IMSG_HEADER_SIZE ||
	    imsg->hdr.len > MAX_IMSGSIZE) {
		log_warnx("imsg_get: imsg hdr len %u out of bounds, type=%u",
		    imsg->hdr.len, imsg->hdr.type);
		return (-1);
	}
	if (imsg->hdr.len > av)
		return (0);
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	ibuf->r.rptr = ibuf->r.buf + IMSG_HEADER_SIZE;
	if ((imsg->data = malloc(datalen)) == NULL) {
		log_warn("imsg_get");
		return (-1);
	}
	memcpy(imsg->data, ibuf->r.rptr, datalen);

	if (imsg->hdr.len < av) {
		left = av - imsg->hdr.len;
		memcpy(&ibuf->r.buf, ibuf->r.buf + imsg->hdr.len, left);
		ibuf->r.wpos = left;
	} else
		ibuf->r.wpos = 0;

	return (datalen + IMSG_HEADER_SIZE);
}

int
imsg_compose(struct imsgbuf *ibuf, enum imsg_type type, u_int32_t peerid,
    pid_t pid, int fd, void *data, u_int16_t datalen)
{
	struct buf	*wbuf;
	int		 n;

	if ((wbuf = imsg_create(ibuf, type, peerid, pid, datalen)) == NULL)
		return (-1);

	if (imsg_add(wbuf, data, datalen) == -1)
		return (-1);

	wbuf->fd = fd;

	if ((n = imsg_close(ibuf, wbuf)) < 0)
			return (-1);

	return (n);
}

struct buf *
imsg_create(struct imsgbuf *ibuf, enum imsg_type type, u_int32_t peerid,
    pid_t pid, u_int16_t datalen)
{
	struct buf	*wbuf;
	struct imsg_hdr	 hdr;

	if (datalen > MAX_IMSGSIZE - IMSG_HEADER_SIZE) {
		log_warnx("imsg_create_core: len %u > MAX_IMSGSIZE; "
		    "type %u peerid %lu", datalen + IMSG_HEADER_SIZE,
		    type, peerid);
		return (NULL);
	}

	hdr.len = datalen + IMSG_HEADER_SIZE;
	hdr.type = type;
	hdr.peerid = peerid;
	hdr.pid = pid;
	if ((wbuf = buf_open(hdr.len)) == NULL) {
		log_warn("imsg_create: buf_open");
		return (NULL);
	}
	if (imsg_add(wbuf, &hdr, sizeof(hdr)) == -1)
		return (NULL);

	return (wbuf);
}

int
imsg_add(struct buf *msg, void *data, u_int16_t datalen)
{
	if (datalen)
		if (buf_add(msg, data, datalen) == -1) {
			log_warnx("imsg_add: buf_add error");
			buf_free(msg);
			return (-1);
		}
	return (datalen);
}

int
imsg_close(struct imsgbuf *ibuf, struct buf *msg)
{
	int	n;

	if ((n = buf_close(&ibuf->w, msg)) < 0) {
			log_warnx("imsg_close: buf_add error");
			buf_free(msg);
			return (-1);
	}
	return (n);
}

void
imsg_free(struct imsg *imsg)
{
	free(imsg->data);
}

int
imsg_get_fd(struct imsgbuf *ibuf)
{
	int		 fd;
	struct imsg_fd	*ifd;

	if ((ifd = TAILQ_FIRST(&ibuf->fds)) == NULL)
		return (-1);

	fd = ifd->fd;
	TAILQ_REMOVE(&ibuf->fds, ifd, entry);
	free(ifd);

	return (fd);
}
