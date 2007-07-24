/*	$OpenBSD: imsg.c,v 1.9 2007/07/24 16:46:09 pyr Exp $ */

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

#include "ospfd.h"
#include "log.h"

void
imsg_init(struct imsgbuf *ibuf, int fd, void (*handler)(int, short, void *))
{
	msgbuf_init(&ibuf->w);
	bzero(&ibuf->r, sizeof(ibuf->r));
	ibuf->fd = fd;
	ibuf->w.fd = fd;
	ibuf->pid = getpid();
	ibuf->handler = handler;
	TAILQ_INIT(&ibuf->fds);
}

ssize_t
imsg_read(struct imsgbuf *ibuf)
{
	ssize_t			 n;

	if ((n = recv(ibuf->fd, ibuf->r.buf + ibuf->r.wpos,
	    sizeof(ibuf->r.buf) - ibuf->r.wpos, 0)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_warn("imsg_read: pipe read error");
			return (-1);
		}
		return (-2);
	}

	ibuf->r.wpos += n;

	return (n);
}

ssize_t
imsg_get(struct imsgbuf *ibuf, struct imsg *imsg)
{
	size_t			 av, left, datalen;

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
		memmove(&ibuf->r.buf, ibuf->r.buf + imsg->hdr.len, left);
		ibuf->r.wpos = left;
	} else
		ibuf->r.wpos = 0;

	return (datalen + IMSG_HEADER_SIZE);
}

int
imsg_compose(struct imsgbuf *ibuf, enum imsg_type type, u_int32_t peerid,
    pid_t pid, void *data, u_int16_t datalen)
{
	struct buf	*wbuf;
	int		 n;

	if ((wbuf = imsg_create(ibuf, type, peerid, pid, datalen)) == NULL)
		return (-1);

	if (imsg_add(wbuf, data, datalen) == -1)
		return (-1);

	if ((n = imsg_close(ibuf, wbuf)) < 0)
		return (-1);

	return (n);
}

/* ARGSUSED */
struct buf *
imsg_create(struct imsgbuf *ibuf, enum imsg_type type, u_int32_t peerid,
    pid_t pid, u_int16_t datalen)
{
	struct buf	*wbuf;
	struct imsg_hdr	 hdr;

	datalen += IMSG_HEADER_SIZE;
	if (datalen > MAX_IMSGSIZE) {
		log_warnx("imsg_create: len %u > MAX_IMSGSIZE; "
		    "type %u peerid %lu", datalen + IMSG_HEADER_SIZE,
		    type, peerid);
		return (NULL);
	}

	hdr.type = type;
	hdr.peerid = peerid;
	if ((hdr.pid = pid) == 0)
		hdr.pid = ibuf->pid;
	if ((wbuf = buf_dynamic(datalen, MAX_IMSGSIZE)) == NULL) {
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
	int		 n;
	struct imsg_hdr	*hdr;

	hdr = (struct imsg_hdr *)msg->buf;
	hdr->len = (u_int16_t)msg->wpos;
	if ((n = buf_close(&ibuf->w, msg)) < 0) {
			log_warnx("imsg_close: buf_close error");
			buf_free(msg);
			return (-1);
	}
	imsg_event_add(ibuf);

	return (n);
}

void
imsg_free(struct imsg *imsg)
{
	free(imsg->data);
}
