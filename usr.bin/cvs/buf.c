/*	$OpenBSD: buf.c,v 1.31 2006/01/16 21:02:36 niallo Exp $	*/
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

#include "includes.h"

#include "buf.h"
#include "log.h"
#include "xmalloc.h"

#define BUF_INCR	128

struct cvs_buf {
	u_int	cb_flags;

	/* buffer handle and size */
	u_char	*cb_buf;
	size_t	 cb_size;

	/* start and length of valid data in buffer */
	u_char	*cb_cur;
	size_t	 cb_len;
};

#define SIZE_LEFT(b)	(b->cb_size - (size_t)(b->cb_cur - b->cb_buf) \
			    - b->cb_len)

static ssize_t	cvs_buf_grow(BUF *, size_t);

/*
 * cvs_buf_alloc()
 *
 * Create a new buffer structure and return a pointer to it.  This structure
 * uses dynamically-allocated memory and must be freed with cvs_buf_free(),
 * once the buffer is no longer needed.
 */
BUF *
cvs_buf_alloc(size_t len, u_int flags)
{
	BUF *b;

	b = (BUF *)xmalloc(sizeof(*b));
	/* Postpone creation of zero-sized buffers */
	if (len > 0) {
		b->cb_buf = xmalloc(len);
		memset(b->cb_buf, 0, len);
	} else
		b->cb_buf = NULL;

	b->cb_flags = flags;
	b->cb_size = len;
	b->cb_cur = b->cb_buf;
	b->cb_len = 0;

	return (b);
}

/*
 * cvs_buf_load()
 *
 * Open the file specified by <path> and load all of its contents into a
 * buffer.
 * Returns the loaded buffer on success.
 */
BUF *
cvs_buf_load(const char *path, u_int flags)
{
	int fd;
	ssize_t ret;
	size_t len;
	u_char *bp;
	struct stat st;
	BUF *buf;

	if ((fd = open(path, O_RDONLY, 0600)) == -1)
		fatal("cvs_buf_load: open: `%s': %s", path, strerror(errno));

	if (fstat(fd, &st) == -1)
		fatal("cvs_buf_load: fstat: %s", strerror(errno));

	buf = cvs_buf_alloc((size_t)st.st_size, flags);
	for (bp = buf->cb_cur; ; bp += (size_t)ret) {
		len = SIZE_LEFT(buf);
		ret = read(fd, bp, len);
		if (ret == -1) {
			cvs_buf_free(buf);
			fatal("cvs_buf_load: read: %s", strerror(errno));
		} else if (ret == 0)
			break;

		buf->cb_len += (size_t)ret;
	}

	(void)close(fd);

	return (buf);
}

/*
 * cvs_buf_free()
 *
 * Free the buffer <b> and all associated data.
 */
void
cvs_buf_free(BUF *b)
{
	if (b->cb_buf != NULL)
		xfree(b->cb_buf);
	xfree(b);
}

/*
 * cvs_buf_release()
 *
 * Free the buffer <b>'s structural information but do not free the contents
 * of the buffer.  Instead, they are returned and should be freed later using
 * free().
 */
void *
cvs_buf_release(BUF *b)
{
	u_char *tmp;

	tmp = b->cb_buf;
	xfree(b);
	return (tmp);
}

/*
 * cvs_buf_empty()
 *
 * Empty the contents of the buffer <b> and reset pointers.
 */
void
cvs_buf_empty(BUF *b)
{
	memset(b->cb_buf, 0, b->cb_size);
	b->cb_cur = b->cb_buf;
	b->cb_len = 0;
}

/*
 * cvs_buf_copy()
 *
 * Copy the first <len> bytes of data in the buffer <b> starting at offset
 * <off> in the destination buffer <dst>, which can accept up to <len> bytes.
 * Returns the number of bytes successfully copied, or -1 on failure.
 */
ssize_t
cvs_buf_copy(BUF *b, size_t off, void *dst, size_t len)
{
	size_t rc;

	if (off > b->cb_len)
		fatal("cvs_buf_copy failed");

	rc = MIN(len, (b->cb_len - off));
	memcpy(dst, b->cb_buf + off, rc);

	return (ssize_t)rc;
}

/*
 * cvs_buf_set()
 *
 * Set the contents of the buffer <b> at offset <off> to the first <len>
 * bytes of data found at <src>.  If the buffer was not created with
 * BUF_AUTOEXT, as many bytes as possible will be copied in the buffer.
 */
ssize_t
cvs_buf_set(BUF *b, const void *src, size_t len, size_t off)
{
	size_t rlen = 0;

	if (b->cb_size < (len + off)) {
		if ((b->cb_flags & BUF_AUTOEXT) &&
		    (cvs_buf_grow(b, len + off - b->cb_size) < 0))
			fatal("cvs_buf_set failed");
		else
			rlen = b->cb_size - off;
	} else
		rlen = len;

	memcpy((b->cb_buf + off), src, rlen);

	if (b->cb_len == 0) {
		b->cb_cur = b->cb_buf + off;
		b->cb_len = rlen;
	}

	return (rlen);
}

/*
 * cvs_buf_putc()
 *
 * Append a single character <c> to the end of the buffer <b>.
 * Returns 0 on success.
 */
int
cvs_buf_putc(BUF *b, int c)
{
	u_char *bp;

	bp = b->cb_cur + b->cb_len;
	if (bp == (b->cb_buf + b->cb_size)) {
		/* extend */
		if (!(b->cb_flags & BUF_AUTOEXT) ||
		    (cvs_buf_grow(b, (size_t)BUF_INCR) < 0))
			fatal("cvs_buf_putc failed");

		/* the buffer might have been moved */
		bp = b->cb_cur + b->cb_len;
	}
	*bp = (u_char)c;
	b->cb_len++;

	return (0);
}

/*
 * cvs_buf_getc()
 *
 * Return u_char at buffer position <pos>.
 *
 */
u_char
cvs_buf_getc(BUF *b, u_int pos)
{
	return (b->cb_cur[pos]);
}

/*
 * cvs_buf_append()
 *
 * Append <len> bytes of data pointed to by <data> to the buffer <b>.  If the
 * buffer is too small to accept all data, it will attempt to append as much
 * data as possible, or if the BUF_AUTOEXT flag is set for the buffer, it
 * will get resized to an appropriate size to accept all data.
 * Returns the number of bytes successfully appended to the buffer.
 */
ssize_t
cvs_buf_append(BUF *b, const void *data, size_t len)
{
	size_t left, rlen;
	u_char *bp, *bep;

	bp = b->cb_cur + b->cb_len;
	bep = b->cb_buf + b->cb_size;
	left = bep - bp;
	rlen = len;

	if (left < len) {
		if (b->cb_flags & BUF_AUTOEXT) {
			if (cvs_buf_grow(b, len - left) < 0)
				fatal("cvs_buf_append failed");
			bp = b->cb_cur + b->cb_len;
		} else
			rlen = bep - bp;
	}

	memcpy(bp, data, rlen);
	b->cb_len += rlen;

	return (rlen);
}

/*
 * cvs_buf_fappend()
 *
 */
int
cvs_buf_fappend(BUF *b, const char *fmt, ...)
{
	int ret;
	char *str;
	va_list vap;

	va_start(vap, fmt);
	ret = vasprintf(&str, fmt, vap);
	va_end(vap);

	if (ret == -1)
		fatal("cvs_buf_fappend: failed to format data");

	ret = cvs_buf_append(b, str, (size_t)ret);
	xfree(str);
	return (ret);
}

/*
 * cvs_buf_len()
 *
 * Returns the size of the buffer that is being used.
 */
size_t
cvs_buf_len(BUF *b)
{
	return (b->cb_len);
}

/*
 * cvs_buf_peek()
 *
 * Peek at the contents of the buffer <b> at offset <off>.
 */
const void *
cvs_buf_peek(BUF *b, size_t off)
{
	if (off >= b->cb_len)
		return (NULL);

	return (b->cb_buf + off);
}

/*
 * cvs_buf_write_fd()
 *
 * Write the contents of the buffer <b> to the specified <fd>
 */
int
cvs_buf_write_fd(BUF *b, int fd)
{
	u_char *bp;
	size_t len;
	ssize_t ret;

	len = b->cb_len;
	bp = b->cb_cur;

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
 * cvs_buf_write()
 *
 * Write the contents of the buffer <b> to the file whose path is given in
 * <path>.  If the file does not exist, it is created with mode <mode>.
 */
int
cvs_buf_write(BUF *b, const char *path, mode_t mode)
{
	int fd;

	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1)
		fatal("open: `%s': %s", path, strerror(errno));

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(path);
		fatal("cvs_buf_write: cvs_buf_write_fd: `%s'", path);
	}
	(void)close(fd);

	if (chmod(path, mode) < 0)
		fatal("cvs_buf_write: chmod failed: %s", strerror(errno));

	return (0);
}

/*
 * cvs_buf_write_stmp()
 *
 * Write the contents of the buffer <b> to a temporary file whose path is
 * specified using <template> (see mkstemp.3). NB. This function will modify
 * <template>, as per mkstemp
 */
int
cvs_buf_write_stmp(BUF *b, char *template, mode_t mode)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		fatal("mkstemp: `%s': %s", template, strerror(errno));

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(template);
		fatal("cvs_buf_write_stmp: cvs_buf_write_fd: `%s'", template);
	}
	(void)close(fd);

	return (0);
}

/*
 * cvs_buf_grow()
 *
 * Grow the buffer <b> by <len> bytes.  The contents are unchanged by this
 * operation regardless of the result.
 * Returns the new size on success, or -1 on failure.
 */
static ssize_t
cvs_buf_grow(BUF *b, size_t len)
{
	void *tmp;
	size_t diff;

	diff = b->cb_cur - b->cb_buf;
	/* Buffer not allocated yet */
	if (b->cb_size == 0)
		tmp = xmalloc(len);
	else
		tmp = xrealloc(b->cb_buf, b->cb_size + len);
	b->cb_buf = (u_char *)tmp;
	b->cb_size += len;

	/* readjust pointers in case the buffer moved in memory */
	b->cb_cur = b->cb_buf + diff;

	return (ssize_t)b->cb_size;
}
