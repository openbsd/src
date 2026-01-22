/* $OpenBSD: fuse_chan.c,v 1.2 2026/01/22 11:53:31 helg Exp $ */
/*
 * Copyright (c) 2025 Helg Bredow <helg@openbsd.org>
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

#include <errno.h>

#include "debug.h"
#include "fuse_private.h"

int
fuse_chan_fd(struct fuse_chan *ch)
{
	if (ch == NULL)
		return (-1);

	return (ch->fd);
}
DEF(fuse_chan_fd);

int
fuse_chan_recv(struct fuse_chan **chp, char *buf, size_t size)
{
	struct fuse_chan *ch = *chp;
	struct fusebuf *fbuf = (struct fusebuf *)buf;
	struct iovec iov[2];
	ssize_t n;

	if (chp == NULL || *chp == NULL || buf == NULL)
		return (-EINVAL);

	/* XXX
	 * This will change once the kernel protocol is updated to be compatible
	 * with Linux.
	 * buf is contiguous memory but our fbuf is separated into the header
	 * and io structs with a pointer to the data buffer so we need to
	 * overlay our fbuf with pointer to data buffer.
	 */
	iov[0].iov_base = fbuf;
	iov[0].iov_len  = sizeof(fbuf->fb_hdr) + sizeof(fbuf->FD);
	iov[1].iov_base = fbuf->fb_dat;
	iov[1].iov_len  = size - (sizeof(fbuf->fb_hdr) + sizeof(fbuf->FD));

	n = readv(ch->fd, iov, 2);
	if (n == -1)
		return (-errno);

	return (n);
}
DEF(fuse_chan_recv);

int
fuse_chan_send(struct fuse_chan *ch, const struct iovec iov[], size_t count)
{
	ssize_t n;

	n = writev(ch->fd, iov, count);
	if (n == -1) {
		DPERROR(__func__);
		return (-errno);
	}

	return (0);
}
DEF(fuse_chan_send);
