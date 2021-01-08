/*	$OpenBSD: io.c,v 1.12 2021/01/08 08:09:07 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include "extern.h"

void
io_socket_blocking(int fd)
{
	int	 fl;

	if ((fl = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "fcntl");
	if (fcntl(fd, F_SETFL, fl & ~O_NONBLOCK) == -1)
		err(1, "fcntl");
}

void
io_socket_nonblocking(int fd)
{
	int	 fl;

	if ((fl = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "fcntl");
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1)
		err(1, "fcntl");
}

/*
 * Like io_simple_write() but into a buffer.
 */
void
io_simple_buffer(struct ibuf *b, const void *res, size_t sz)
{
	if (ibuf_add(b, res, sz) == -1)
		err(1, NULL);
}

/*
 * Add a sz sized buffer into the io buffer.
 */
void
io_buf_buffer(struct ibuf *b, const void *p, size_t sz)
{
	if (ibuf_add(b, &sz, sizeof(size_t)) == -1)
		err(1, NULL);
	if (sz > 0)
		if (ibuf_add(b, p, sz) == -1)
			err(1, NULL);
}

/*
 * Add a string into the io buffer.
 */
void
io_str_buffer(struct ibuf *b, const char *p)
{
	size_t sz = (p == NULL) ? 0 : strlen(p);

	io_buf_buffer(b, p, sz);
}

/*
 * Read of a binary buffer that must be on a blocking descriptor.
 * Does nothing if "sz" is zero.
 * This will fail and exit on EOF.
 */
void
io_simple_read(int fd, void *res, size_t sz)
{
	ssize_t	 ssz;
	char	*tmp;

	tmp = res; /* arithmetic on a pointer to void is a GNU extension */
again:
	if (sz == 0)
		return;
	if ((ssz = read(fd, tmp, sz)) == -1)
		err(1, "read");
	else if (ssz == 0)
		errx(1, "read: unexpected end of file");
	else if ((size_t)ssz == sz)
		return;
	sz -= ssz;
	tmp += ssz;
	goto again;
}

/*
 * Read a binary buffer, allocating space for it.
 * If the buffer is zero-sized, this won't allocate "res", but
 * will still initialise it to NULL.
 */
void
io_buf_read_alloc(int fd, void **res, size_t *sz)
{

	*res = NULL;
	io_simple_read(fd, sz, sizeof(size_t));
	if (*sz == 0)
		return;
	if ((*res = malloc(*sz)) == NULL)
		err(1, NULL);
	io_simple_read(fd, *res, *sz);
}

/*
 * Read a string (returns NULL for zero-length strings), allocating
 * space for it.
 */
void
io_str_read(int fd, char **res)
{
	size_t	 sz;

	io_simple_read(fd, &sz, sizeof(size_t));
	if (sz == 0) {
		*res = NULL;
		return;
	}
	if ((*res = calloc(sz + 1, 1)) == NULL)
		err(1, NULL);
	io_simple_read(fd, *res, sz);
}
