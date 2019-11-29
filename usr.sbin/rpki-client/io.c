/*	$OpenBSD: io.c,v 1.8 2019/11/29 05:09:50 benno Exp $ */
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

#include <openssl/x509.h>

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
 * Blocking write of a binary buffer.
 * Buffers of length zero are simply ignored.
 */
void
io_simple_write(int fd, const void *res, size_t sz)
{
	ssize_t	 ssz;

	if (sz == 0)
		return;
	if ((ssz = write(fd, res, sz)) == -1)
		err(1, "write");
	else if ((size_t)ssz != sz)
		errx(1, "write: short write");
}

/*
 * Like io_simple_write() but into a buffer.
 */
void
io_simple_buffer(char **b, size_t *bsz,
	size_t *bmax, const void *res, size_t sz)
{

	if (*bsz + sz > *bmax) {
		if ((*b = realloc(*b, *bsz + sz)) == NULL)
			err(1, NULL);
		*bmax = *bsz + sz;
	}

	memcpy(*b + *bsz, res, sz);
	*bsz += sz;
}

/*
 * Like io_buf_write() but into a buffer.
 */
void
io_buf_buffer(char **b, size_t *bsz,
	size_t *bmax, const void *p, size_t sz)
{

	io_simple_buffer(b, bsz, bmax, &sz, sizeof(size_t));
	if (sz > 0)
		io_simple_buffer(b, bsz, bmax, p, sz);
}

/*
 * Write a binary buffer of the given size, which may be zero.
 */
void
io_buf_write(int fd, const void *p, size_t sz)
{

	io_simple_write(fd, &sz, sizeof(size_t));
	io_simple_write(fd, p, sz);
}

/*
 * Like io_str_write() but into a buffer.
 */
void
io_str_buffer(char **b, size_t *bsz, size_t *bmax, const char *p)
{
	size_t	 sz = (p == NULL) ? 0 : strlen(p);

	io_buf_buffer(b, bsz, bmax, p, sz);
}

/*
 * Write a NUL-terminated string, which may be zero-length.
 */
void
io_str_write(int fd, const char *p)
{
	size_t	 sz = (p == NULL) ? 0 : strlen(p);

	io_buf_write(fd, p, sz);
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
 * Read a string (which may just be \0 and zero-length), allocating
 * space for it.
 */
void
io_str_read(int fd, char **res)
{
	size_t	 sz;

	io_simple_read(fd, &sz, sizeof(size_t));
	if ((*res = calloc(sz + 1, 1)) == NULL)
		err(1, NULL);
	io_simple_read(fd, *res, sz);
}
