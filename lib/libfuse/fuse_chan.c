/* $OpenBSD: fuse_chan.c,v 1.3 2026/06/17 13:29:01 helg Exp $ */
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
#include <unistd.h>

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
	ssize_t n;

	n = read((*chp)->fd, buf, size);
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
	if (n == -1)
		return (-errno);

	return (0);
}
DEF(fuse_chan_send);
