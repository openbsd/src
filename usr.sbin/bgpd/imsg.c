/*	$OpenBSD: imsg.c,v 1.17 2004/01/01 23:46:47 henning Exp $ */

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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

void
imsg_init(struct imsgbuf *ibuf, int sock)
{
	msgbuf_init(&ibuf->w);
	bzero(&ibuf->r, sizeof(ibuf->r));
	ibuf->sock = sock;
	ibuf->w.sock = sock;
}

int
imsg_read(struct imsgbuf *ibuf)
{
	ssize_t			 n;

	if ((n = read(ibuf->sock, ibuf->r.buf + ibuf->r.wpos,
	    sizeof(ibuf->r.buf) - ibuf->r.wpos)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_err("imsg_get: pipe read error");
			return (-1);
		}
		return (0);
	}
	if (n == 0) {	/* connection closed */
		logit(LOG_CRIT, "imsg_get: pipe closed");
		return (-1);
	}

	ibuf->r.wpos += n;

	return (0);
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
		logit(LOG_CRIT, "wrong imsg hdr len");
		return (-1);
	}
	if (imsg->hdr.len > av)
		return (0);
	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	ibuf->r.rptr = ibuf->r.buf + IMSG_HEADER_SIZE;
	if ((imsg->data = malloc(datalen)) == NULL) {
		log_err("imsg_get");
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
imsg_compose(struct imsgbuf *ibuf, int type, u_int32_t peerid, void *data,
    u_int16_t datalen)
{
	struct buf	*wbuf;
	struct imsg_hdr	 hdr;
	int		 n;

	hdr.len = datalen + IMSG_HEADER_SIZE;
	hdr.type = type;
	hdr.peerid = peerid;
	wbuf = buf_open(hdr.len);
	if (wbuf == NULL) {
		logit(LOG_CRIT, "imsg_compose: buf_open error");
		return (-1);
	}
	if (buf_add(wbuf, &hdr, sizeof(hdr)) == -1) {
		logit(LOG_CRIT, "imsg_compose: buf_add error");
		buf_free(wbuf);
		return (-1);
	}
	if (datalen)
		if (buf_add(wbuf, data, datalen) == -1) {
			logit(LOG_CRIT, "imsg_compose: buf_add error");
			buf_free(wbuf);
			return (-1);
		}

	if ((n = buf_close(&ibuf->w, wbuf)) < 0) {
			logit(LOG_CRIT, "imsg_compose: buf_add error");
			buf_free(wbuf);
			return (-1);
	}
	return (n);
}

void
imsg_free(struct imsg *imsg)
{
	free(imsg->data);
}
