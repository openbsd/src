/*	$OpenBSD: imsg.c,v 1.1 2003/12/17 11:46:54 henning Exp $ */

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

static struct imsg_buf_read	buf;

void
init_imsg_buf(void)
{
	bzero(&buf, sizeof(buf));
	buf.wptr = buf.buf;
	buf.pkt_len = IMSG_HEADER_SIZE;
}

int
get_imsg(int fd, struct imsg *imsg)
{
	struct imsg_hdr		*hdr;
	size_t			 n, read_total = 0, datalen = 0;
	u_char			*rptr;

	do {
		if ((n = read(fd, buf.wptr, buf.pkt_len - buf.read_len)) ==
		    -1) {
			if (errno != EAGAIN && errno != EINTR)
				fatal("pipe read error", errno);
			return (0);
		}
		read_total += n;
		buf.wptr += n;
		buf.read_len += n;
		if (buf.read_len == buf.pkt_len) {
			if (!buf.seen_hdr) {	/* got header */
				hdr = (struct imsg_hdr *)&buf.buf;
				buf.type = hdr->type;
				buf.pkt_len = hdr->len;
				buf.peerid = hdr->peerid;
				if (hdr->len < IMSG_HEADER_SIZE ||
				    hdr->len > MAX_IMSGSIZE)
					fatal("wrong imsg header len", 0);
				buf.seen_hdr = 1;
			} else {		/* we got the full packet */
				imsg->hdr.type = buf.type;
				imsg->hdr.len = buf.pkt_len;
				imsg->hdr.peerid = buf.peerid;
				datalen = buf.pkt_len - IMSG_HEADER_SIZE;
				rptr = buf.buf + IMSG_HEADER_SIZE;
				if ((imsg->data = malloc(datalen)) == NULL)
					fatal("get_imsg malloc", errno);
				memcpy(imsg->data, rptr, datalen);
				n = 0;	/* give others a chance */
				init_imsg_buf();
			}
		}
	} while (n > 0);
	if (read_total == 0)	/* connection closed ? */
		fatal("pipe closed", 0);
	return (datalen + IMSG_HEADER_SIZE);
}

int
imsg_compose(int fd, int type, u_int32_t peerid, u_char *data,
    u_int16_t datalen)
{
	struct buf	*wbuf;
	struct imsg_hdr	 hdr;
	int		 n;

	hdr.len = datalen + IMSG_HEADER_SIZE;
	hdr.type = type;
	hdr.peerid = peerid;
	wbuf = buf_open(NULL, fd, hdr.len);
	if (wbuf == NULL)
		fatal("buf_open error", 0);
	if (buf_add(wbuf, (u_char *)&hdr, sizeof(hdr)) == -1)
		fatal("buf_add error", 0);
	if (datalen)
		if (buf_add(wbuf, data, datalen) == -1)
			fatal("buf_add error", 0);
	if ((n = buf_close(wbuf)) == -1)
		fatal("buf_close error", 0);

	return (n);
}

void
imsg_free(struct imsg *imsg)
{
	free(imsg->data);
}
