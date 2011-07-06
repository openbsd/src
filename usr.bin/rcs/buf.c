/*	$OpenBSD: buf.c,v 1.22 2011/07/06 15:36:52 nicm Exp $	*/
/*
 * Copyright (c) 2003 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "xmalloc.h"
#include "worklist.h"

#define BUF_INCR	128

struct buf {
	/* buffer handle, buffer size, and data length */
	u_char	*cb_buf;
	size_t	 cb_size;
	size_t	 cb_len;
};

#define SIZE_LEFT(b)	(b->cb_size - b->cb_len)

static void	buf_grow(BUF *, size_t);

/*
 * Create a new buffer structure and return a pointer to it.  This structure
 * uses dynamically-allocated memory and must be freed with buf_free(), once
 * the buffer is no longer needed.
 */
BUF *
buf_alloc(size_t len)
{
	BUF *b;

	b = xmalloc(sizeof(*b));
	/* Postpone creation of zero-sized buffers */
	if (len > 0)
		b->cb_buf = xcalloc(1, len);
	else
		b->cb_buf = NULL;

	b->cb_size = len;
	b->cb_len = 0;

	return (b);
}

/*
 * Open the file specified by <path> and load all of its contents into a
 * buffer.
 * Returns the loaded buffer on success or NULL on failure.
 * Sets errno on error.
 */
BUF *
buf_load(const char *path)
{
	int fd;
	ssize_t ret;
	size_t len;
	u_char *bp;
	struct stat st;
	BUF *buf;

	buf = NULL;

	if ((fd = open(path, O_RDONLY, 0600)) == -1)
		goto out;

	if (fstat(fd, &st) == -1)
		goto out;

	if (st.st_size > SIZE_MAX) {
		errno = EFBIG;
		goto out;
	}
	buf = buf_alloc(st.st_size);
	for (bp = buf->cb_buf; ; bp += (size_t)ret) {
		len = SIZE_LEFT(buf);
		ret = read(fd, bp, len);
		if (ret == -1) {
			int saved_errno;

			saved_errno = errno;
			buf_free(buf);
			buf = NULL;
			errno = saved_errno;
			goto out;
		} else if (ret == 0)
			break;

		buf->cb_len += (size_t)ret;
	}

out:
	if (fd != -1) {
		int saved_errno;

		/* We may want to preserve errno here. */
		saved_errno = errno;
		(void)close(fd);
		errno = saved_errno;
	}

	return (buf);
}

void
buf_free(BUF *b)
{
	if (b->cb_buf != NULL)
		xfree(b->cb_buf);
	xfree(b);
}

/*
 * Free the buffer <b>'s structural information but do not free the contents
 * of the buffer.  Instead, they are returned and should be freed later using
 * xfree().
 */
void *
buf_release(BUF *b)
{
	void *tmp;

	tmp = b->cb_buf;
	xfree(b);
	return (tmp);
}

u_char *
buf_get(BUF *b)
{
	return (b->cb_buf);
}

/*
 * Empty the contents of the buffer <b> and reset pointers.
 */
void
buf_empty(BUF *b)
{
	memset(b->cb_buf, 0, b->cb_size);
	b->cb_len = 0;
}

/*
 * Append a single character <c> to the end of the buffer <b>.
 */
void
buf_putc(BUF *b, int c)
{
	u_char *bp;

	if (SIZE_LEFT(b) == 0)
		buf_grow(b, BUF_INCR);
	bp = b->cb_buf + b->cb_len;
	*bp = (u_char)c;
	b->cb_len++;
}

/*
 * Append a string <s> to the end of buffer <b>.
 */
void
buf_puts(BUF *b, const char *str)
{
	buf_append(b, str, strlen(str));
}

/*
 * Return u_char at buffer position <pos>.
 */
u_char
buf_getc(BUF *b, size_t pos)
{
	return (b->cb_buf[pos]);
}

/*
 * Append <len> bytes of data pointed to by <data> to the buffer <b>.  If the
 * buffer is too small to accept all data, it will get resized to an
 * appropriate size to accept all data.
 * Returns the number of bytes successfully appended to the buffer.
 */
size_t
buf_append(BUF *b, const void *data, size_t len)
{
	size_t left, rlen;
	u_char *bp;

	left = SIZE_LEFT(b);
	rlen = len;

	if (left < len)
		buf_grow(b, len - left);
	bp = b->cb_buf + b->cb_len;
	memcpy(bp, data, rlen);
	b->cb_len += rlen;

	return (rlen);
}

/*
 * Returns the size of the buffer that is being used.
 */
size_t
buf_len(BUF *b)
{
	return (b->cb_len);
}

/*
 * Write the contents of the buffer <b> to the specified <fd>
 */
int
buf_write_fd(BUF *b, int fd)
{
	u_char *bp;
	size_t len;
	ssize_t ret;

	len = b->cb_len;
	bp = b->cb_buf;

	do {
		ret = write(fd, bp, len);
		if (ret == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return (-1);
		}

		len -= (size_t)ret;
		bp += (size_t)ret;
	} while (len > 0);

	return (0);
}

/*
 * Write the contents of the buffer <b> to the file whose path is given in
 * <path>.  If the file does not exist, it is created with mode <mode>.
 */
int
buf_write(BUF *b, const char *path, mode_t mode)
{
	int fd;
 open:
	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		if (errno == EACCES && unlink(path) != -1)
			goto open;
		else
			err(1, "%s", path);
	}

	if (buf_write_fd(b, fd) == -1) {
		(void)unlink(path);
		errx(1, "buf_write: buf_write_fd: `%s'", path);
	}

	if (fchmod(fd, mode) < 0)
		warn("permissions not set on file %s", path);

	(void)close(fd);

	return (0);
}

/*
 * Write the contents of the buffer <b> to a temporary file whose path is
 * specified using <template> (see mkstemp.3).
 * NB. This function will modify <template>, as per mkstemp
 */
void
buf_write_stmp(BUF *b, char *template)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		err(1, "%s", template);

	worklist_add(template, &temp_files);

	if (buf_write_fd(b, fd) == -1) {
		(void)unlink(template);
		errx(1, "buf_write_stmp: buf_write_fd: `%s'", template);
	}

	(void)close(fd);
}

/*
 * Grow the buffer <b> by <len> bytes.  The contents are unchanged by this
 * operation regardless of the result.
 */
static void
buf_grow(BUF *b, size_t len)
{
	b->cb_buf = xrealloc(b->cb_buf, 1, b->cb_size + len);
	b->cb_size += len;
}
