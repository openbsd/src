/*	$OpenBSD: buf.c,v 1.5 2004/12/06 21:03:12 deraadt Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "buf.h"
#include "log.h"


#define BUF_INCR   128


struct cvs_buf {
	u_int    cb_flags;

	/* buffer handle and size */
	void    *cb_buf;
	size_t   cb_size;

	/* start and length of valid data in buffer */
	u_char  *cb_cur;
	size_t   cb_len;
};



#define SIZE_LEFT(b)  ((size_t)((u_char *)b->cb_buf - b->cb_cur) + b->cb_size)


static ssize_t   cvs_buf_grow (BUF *, size_t);



/*
 * cvs_buf_alloc()
 *
 * Create a new buffer structure and return a pointer to it.  This structure
 * uses dynamically-allocated memory and must be freed with cvs_buf_free(),
 * once the buffer is no longer needed.
 */

BUF*
cvs_buf_alloc(size_t len, u_int flags)
{
	BUF *b;

	b = (BUF *)malloc(sizeof(*b));
	if (b == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate buffer");
		return (NULL);
	}

	b->cb_buf = malloc(len);
	if (b->cb_buf == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate buffer");
		free(b);
		return (NULL);
	}
	memset(b->cb_buf, 0, len);

	b->cb_flags = flags;
	b->cb_size = len;
	b->cb_cur = (u_char *)b->cb_buf;
	b->cb_len = 0;

	return (b);
}


/*
 * cvs_buf_load()
 *
 * Open the file specified by <path> and load all of its contents into a
 * buffer.
 * Returns the loaded buffer on success, or NULL on failure.
 */

BUF*
cvs_buf_load(const char *path, u_int flags)
{
	int fd;
	ssize_t ret;
	size_t len;
	void *bp;
	struct stat st;
	BUF *buf;

	fd = open(path, O_RDONLY, 0600);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open buffer source");
		return (NULL);
	}

	if (fstat(fd, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat buffer source");
		(void)close(fd);
		return (NULL);
	}

	buf = cvs_buf_alloc((size_t)st.st_size, flags);
	if (buf == NULL) {
		(void)close(fd);
		return (NULL);
	}

	for (bp = buf->cb_cur; ; bp += (size_t)ret) {
		len = MIN(SIZE_LEFT(buf), 4096);
		ret = read(fd, bp, len);
		if (ret == -1) {
			cvs_log(LP_ERRNO, "read failed from buffer source");
			(void)close(fd);
			cvs_buf_free(bp);
			return (NULL);
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
	free(b->cb_buf);
	free(b);
}


/*
 * cvs_buf_release()
 *
 * Free the buffer <b>'s structural information but do not free the contents
 * of the buffer.  Instead, they are returned and should be freed later using
 * free().
 */

void*
cvs_buf_release(BUF *b)
{
	void *tmp;
	tmp = b->cb_buf;
	free(b);
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
	b->cb_cur = (u_char *)b->cb_buf;
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
		return (-1);

	rc = MIN(len, (b->cb_len - off));
	memcpy(dst, b->cb_buf, rc);

	return (ssize_t)rc;
}


/*
 * cvs_buf_set()
 *
 * Set the contents of the buffer <b> to the first <len> bytes of data found
 * at <src>.  If the buffer was not created with BUF_AUTOEXT, as many bytes
 * as possible will be copied in the buffer.
 */

int
cvs_buf_set(BUF *b, const void *src, size_t len, size_t off)
{
	size_t rlen;

	if (b->cb_size < (len + off)) {
		if ((b->cb_flags & BUF_AUTOEXT) && (cvs_buf_grow(b, 
		    len + off - b->cb_size) < 0))
			return (-1);
		else
			rlen = b->cb_size - off;
	} else
		rlen = len;

	memcpy((b->cb_buf + off), src, rlen);

	if (b->cb_len == 0) {
		b->cb_cur = b->cb_buf + off;
		b->cb_len = rlen;
	}

	return (int)rlen;
}


/*
 * cvs_buf_putc()
 *
 * Append a single character <c> to the end of the buffer <b>.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_buf_putc(BUF *b, int c)
{
	u_char *bp;

	bp = b->cb_cur + b->cb_len;
	if (bp == (b->cb_buf + b->cb_size)) {
		/* extend */
		if (!(b->cb_flags & BUF_AUTOEXT) ||
		    (cvs_buf_grow(b, BUF_INCR) < 0))
			return (-1);

		/* the buffer might have been moved */
		bp = b->cb_cur + b->cb_len;
	}
	*bp = (u_char)c;
	b->cb_len++;

	return (0);
}


/*
 * cvs_buf_append()
 *
 * Append <len> bytes of data pointed to by <data> to the buffer <b>.  If the
 * buffer is too small to accept all data, it will attempt to append as much
 * data as possible, or if the BUF_AUTOEXT flag is set for the buffer, it
 * will get resized to an appropriate size to accept all data.
 * Returns the number of bytes successfully appended to the buffer, or -1
 * on failure.
 */

ssize_t
cvs_buf_append(BUF *b, const void *data, size_t len)
{
	size_t left, rlen;
	void *bp, *bep;

	bp = b->cb_cur + b->cb_len;
	bep = b->cb_buf + b->cb_size;
	left = bep - bp;
	rlen = len;

	if (left < len) {
		if (b->cb_flags & BUF_AUTOEXT) {
			if (cvs_buf_grow(b, len - left) < 0)
				return (-1);
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

	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to format data");
		return (-1);
	}

	ret = cvs_buf_append(b, str, ret);
	free(str);
	return (ret);
}


/*
 * cvs_buf_size()
 *
 * Returns the size of the buffer that is being used.
 */

size_t
cvs_buf_size(BUF *b)
{
	return (b->cb_len);
}


/*
 * cvs_buf_peek()
 *
 * Peek at the contents of the buffer <b> at offset <off>.
 */

const void*
cvs_buf_peek(BUF *b, size_t off)
{
	if (off >= b->cb_len)
		return (NULL);

	return (b->cb_buf + off);
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
	u_char *bp;
	size_t len;
	ssize_t ret;

	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open file `%s'", path);
		return (-1);
	}

	len = b->cb_len;
	bp = b->cb_cur;

	do {
		ret = write(fd, bp, MIN(len, 8192));
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to write to file `%s'", path);
			(void)close(fd);
			(void)unlink(path);
			return (-1);
		}

		len -= (size_t)ret;
		bp += (size_t)ret;
	} while (len > 0);

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

	diff = b->cb_cur - (u_char *)b->cb_buf;
	tmp = realloc(b->cb_buf, b->cb_size + len);
	if (tmp == NULL) {
		cvs_log(LP_ERRNO, "failed to grow buffer");
		return (-1);
	}
	b->cb_buf = (u_char *)tmp;
	b->cb_size += len;

	/* readjust pointers in case the buffer moved in memory */
	b->cb_cur = b->cb_buf + diff;

	return (ssize_t)b->cb_size;
}
