/*	$OpenBSD: privsep.c,v 1.16 2011/04/04 11:14:52 krw Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE, ABUSE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "dhcpd.h"
#include "privsep.h"

struct buf *
buf_open(size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		error("buf_open: %m");
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		error("buf_open: %m");
	}
	buf->size = len;

	return (buf);
}

void
buf_add(struct buf *buf, void *data, size_t len)
{
	if (len == 0)
		return;

	if (buf->wpos + len > buf->size)
		error("buf_add: %m");

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
}

void
buf_close(int sock, struct buf *buf)
{
	ssize_t	n;

	do {
		n = write(sock, buf->buf + buf->rpos, buf->size - buf->rpos);
		if (n == 0)			/* connection closed */
			error("buf_close (connection closed)");
		if (n != -1 && n < buf->size - buf->rpos)
			error("buf_close (short write): %m");

	} while (n == -1 && (errno == EAGAIN || errno == EINTR));

	if (n == -1)
		error("buf_close: %m");

	free(buf->buf);
	free(buf);
}

void
buf_read(int sock, void *buf, size_t nbytes)
{
	ssize_t	n;

	do {
		n = read(sock, buf, nbytes);
		if (n == 0) {			/* connection closed */
#ifdef DEBUG
			debug("buf_read (connection closed)");
#endif
			exit(1);
		}
		if (n != -1 && n < nbytes)
			error("buf_read (short read): %m");
	} while (n == -1 && (errno == EINTR || errno == EAGAIN));

	if (n == -1)
		error("buf_read: %m");
}

void
dispatch_imsg(int fd)
{
	struct imsg_hdr		 hdr;
	char			*reason, *filename,
				*servername, *prefix;
	size_t			 reason_len, filename_len,
				 servername_len, prefix_len, totlen;
	struct client_lease	 lease;
	int			 ret, i, optlen;
	struct buf		*buf;

	buf_read(fd, &hdr, sizeof(hdr));

	switch (hdr.code) {
	case IMSG_SCRIPT_INIT:
		if (hdr.len < sizeof(hdr) + sizeof(size_t))
			error("corrupted message received");
		buf_read(fd, &reason_len, sizeof(reason_len));
		if (hdr.len < reason_len + sizeof(hdr) + sizeof(size_t) ||
		    reason_len == SIZE_T_MAX)
			error("corrupted message received");
		if (reason_len > 0) {
			if ((reason = calloc(1, reason_len + 1)) == NULL)
				error("%m");
			buf_read(fd, reason, reason_len);
		} else
			reason = NULL;

		priv_script_init(reason);
		free(reason);
		break;
	case IMSG_SCRIPT_WRITE_PARAMS:
		bzero(&lease, sizeof lease);
		totlen = sizeof(hdr) + sizeof(lease) + sizeof(size_t);
		if (hdr.len < totlen)
			error("corrupted message received");
		buf_read(fd, &lease, sizeof(lease));

		buf_read(fd, &filename_len, sizeof(filename_len));
		totlen += filename_len + sizeof(size_t);
		if (hdr.len < totlen || filename_len == SIZE_T_MAX)
			error("corrupted message received");
		if (filename_len > 0) {
			if ((filename = calloc(1, filename_len + 1)) == NULL)
				error("%m");
			buf_read(fd, filename, filename_len);
		} else
			filename = NULL;

		buf_read(fd, &servername_len, sizeof(servername_len));
		totlen += servername_len + sizeof(size_t);
		if (hdr.len < totlen || servername_len == SIZE_T_MAX)
			error("corrupted message received");
		if (servername_len > 0) {
			if ((servername =
			    calloc(1, servername_len + 1)) == NULL)
				error("%m");
			buf_read(fd, servername, servername_len);
		} else
			servername = NULL;

		buf_read(fd, &prefix_len, sizeof(prefix_len));
		totlen += prefix_len;
		if (hdr.len < totlen || prefix_len == SIZE_T_MAX)
			error("corrupted message received");
		if (prefix_len > 0) {
			if ((prefix = calloc(1, prefix_len + 1)) == NULL)
				error("%m");
			buf_read(fd, prefix, prefix_len);
		} else
			prefix = NULL;

		for (i = 0; i < 256; i++) {
			totlen += sizeof(optlen);
			if (hdr.len < totlen)
				error("corrupted message received");
			buf_read(fd, &optlen, sizeof(optlen));
			lease.options[i].data = NULL;
			lease.options[i].len = optlen;
			if (optlen > 0) {
				totlen += optlen;
				if (hdr.len < totlen || optlen == SIZE_T_MAX)
					error("corrupted message received");
				lease.options[i].data =
				    calloc(1, optlen + 1);
				if (lease.options[i].data == NULL)
				    error("%m");
				buf_read(fd, lease.options[i].data, optlen);
			}
		}
		lease.server_name = servername;
		lease.filename = filename;

		priv_script_write_params(prefix, &lease);

		free(servername);
		free(filename);
		free(prefix);
		for (i = 0; i < 256; i++)
			if (lease.options[i].len > 0)
				free(lease.options[i].data);
		break;
	case IMSG_SCRIPT_GO:
		if (hdr.len != sizeof(hdr))
			error("corrupted message received");

		ret = priv_script_go();

		hdr.code = IMSG_SCRIPT_GO_RET;
		hdr.len = sizeof(struct imsg_hdr) + sizeof(int);
		if ((buf = buf_open(hdr.len)) == NULL)
			error("buf_open: %m");
		buf_add(buf, &hdr, sizeof(hdr));
		buf_add(buf, &ret, sizeof(ret));
		buf_close(fd, buf);
		break;
	default:
		error("received unknown message, code %d", hdr.code);
	}
}
