/*	$OpenBSD: imsg.c,v 1.7 2003/12/21 23:26:37 henning Exp $ */

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

void	imsg_init_readbuf(struct imsgbuf *);

void
imsg_init_readbuf(struct imsgbuf *ibuf)
{
	bzero(&ibuf->r, sizeof(ibuf->r));
	ibuf->r.wptr = ibuf->r.buf;
	ibuf->r.pkt_len = IMSG_HEADER_SIZE;
}

void
imsg_init(struct imsgbuf *ibuf, int sock)
{
	imsg_init_readbuf(ibuf);
	msgbuf_init(&ibuf->w);
	ibuf->sock = sock;
	ibuf->w.sock = sock;
}

int
get_imsg(struct imsgbuf *ibuf, struct imsg *imsg)
{
	struct imsg_hdr		*hdr;
	ssize_t			 n, read_total = 0, datalen = 0;
	u_char			*rptr;

	do {
		if ((n = read(ibuf->sock, ibuf->r.wptr,
		    ibuf->r.pkt_len - ibuf->r.read_len)) == -1) {
			if (errno != EAGAIN && errno != EINTR)
				fatal("pipe read error", errno);
			return (0);
		}
		read_total += n;
		ibuf->r.wptr += n;
		ibuf->r.read_len += n;
		if (ibuf->r.read_len == ibuf->r.pkt_len) {
			if (!ibuf->r.seen_hdr) {	/* got header */
				hdr = (struct imsg_hdr *)&ibuf->r.buf;
				ibuf->r.type = hdr->type;
				ibuf->r.pkt_len = hdr->len;
				ibuf->r.peerid = hdr->peerid;
				if (hdr->len < IMSG_HEADER_SIZE ||
				    hdr->len > MAX_IMSGSIZE)
					fatal("wrong imsg header len", 0);
				ibuf->r.seen_hdr = 1;
			} else {		/* we got the full packet */
				imsg->hdr.type = ibuf->r.type;
				imsg->hdr.len = ibuf->r.pkt_len;
				imsg->hdr.peerid = ibuf->r.peerid;
				datalen = ibuf->r.pkt_len - IMSG_HEADER_SIZE;
				rptr = ibuf->r.buf + IMSG_HEADER_SIZE;
				if ((imsg->data = malloc(datalen)) == NULL)
					fatal("get_imsg malloc", errno);
				memcpy(imsg->data, rptr, datalen);
				n = 0;	/* give others a chance */
				imsg_init_readbuf(ibuf);
			}
		}
	} while (n > 0);

	if (read_total == 0)	/* connection closed */
		return (0);

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
	if (wbuf == NULL)
		fatal("imsg_compose: buf_open error", 0);
	if (buf_add(wbuf, &hdr, sizeof(hdr)) == -1)
		fatal("imsg_compose: buf_add error", 0);
	if (datalen)
		if (buf_add(wbuf, data, datalen) == -1)
			fatal("imsg_compose: buf_add error", 0);
	if ((n = buf_close(&ibuf->w, wbuf)) == -1)
		fatal("imsg_compose: buf_close error", 0);

	return (n);
}

void
imsg_free(struct imsg *imsg)
{
	free(imsg->data);
}
