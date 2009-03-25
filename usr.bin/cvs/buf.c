/*	$OpenBSD: buf.c,v 1.74 2009/03/25 21:19:20 joris Exp $	*/
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

#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "cvs.h"
#include "buf.h"

#define BUF_INCR	128
#define BUF_GROW(bp, len)						\
	do {								\
		b->cb_buf = xrealloc(b->cb_buf, 1, b->cb_size + len);	\
		b->cb_size += len;					\
	} while (0);

struct cvs_buf {
	u_char	*cb_buf;
	size_t	 cb_size;
	size_t	 cb_len;
};

BUF *
cvs_buf_alloc(size_t len)
{
	BUF *b;

	b = xmalloc(sizeof(*b));
	if (len > 0)
		b->cb_buf = xcalloc(1, len);
	else
		b->cb_buf = NULL;

	b->cb_size = len;
	b->cb_len = 0;

	return (b);
}

BUF *
cvs_buf_load(const char *path)
{
	int fd;
	BUF *bp;

	if ((fd = open(path, O_RDONLY, 0600)) == -1)
		fatal("cvs_buf_load: failed to load '%s' : %s", path,
		    strerror(errno));

	bp = cvs_buf_load_fd(fd);
	(void)close(fd);
	return (bp);
}

BUF *
cvs_buf_load_fd(int fd)
{
	struct stat st;
	BUF *buf;

	if (fstat(fd, &st) == -1)
		fatal("cvs_buf_load_fd: fstat: %s", strerror(errno));

	if (lseek(fd, 0, SEEK_SET) == -1)
		fatal("cvs_buf_load_fd: lseek: %s", strerror(errno));

	if (st.st_size > SIZE_MAX)
		fatal("cvs_buf_load_fd: file size too big");
	buf = cvs_buf_alloc(st.st_size);
	if (atomicio(read, fd, buf->cb_buf, buf->cb_size) != buf->cb_size)
		fatal("cvs_buf_load_fd: read: %s", strerror(errno));
	buf->cb_len = buf->cb_size;

	return (buf);
}

void
cvs_buf_free(BUF *b)
{
	if (b->cb_buf != NULL)
		xfree(b->cb_buf);
	xfree(b);
}

u_char *
cvs_buf_release(BUF *b)
{
	u_char *tmp;

	tmp = b->cb_buf;
	xfree(b);
	return (tmp);
}

void
cvs_buf_putc(BUF *b, int c)
{
	u_char *bp;

	bp = b->cb_buf + b->cb_len;
	if (bp == (b->cb_buf + b->cb_size)) {
		BUF_GROW(b, BUF_INCR);
		bp = b->cb_buf + b->cb_len;
	}
	*bp = (u_char)c;
	b->cb_len++;
}

void
cvs_buf_puts(BUF *b, const char *str)
{
	cvs_buf_append(b, str, strlen(str));
}

void
cvs_buf_append(BUF *b, const void *data, size_t len)
{
	size_t left;
	u_char *bp, *bep;

	bp = b->cb_buf + b->cb_len;
	bep = b->cb_buf + b->cb_size;
	left = bep - bp;

	if (left < len) {
		BUF_GROW(b, len - left);
		bp = b->cb_buf + b->cb_len;
	}

	memcpy(bp, data, len);
	b->cb_len += len;
}

size_t
cvs_buf_len(BUF *b)
{
	return (b->cb_len);
}

int
cvs_buf_write_fd(BUF *b, int fd)
{
	if (atomicio(vwrite, fd, b->cb_buf, b->cb_len) != b->cb_len)
		return (-1);
	return (0);
}

int
cvs_buf_write(BUF *b, const char *path, mode_t mode)
{
	int fd;
open:
	if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		if (errno == EACCES && unlink(path) != -1)
			goto open;
		else
			fatal("open: `%s': %s", path, strerror(errno));
	}

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(path);
		fatal("cvs_buf_write: cvs_buf_write_fd: `%s'", path);
	}

	if (fchmod(fd, mode) < 0)
		cvs_log(LP_ERR, "permissions not set on file %s", path);

	(void)close(fd);

	return (0);
}

int
cvs_buf_write_stmp(BUF *b, char *template, struct timeval *tv)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		fatal("mkstemp: `%s': %s", template, strerror(errno));

	if (cvs_buf_write_fd(b, fd) == -1) {
		(void)unlink(template);
		fatal("cvs_buf_write_stmp: cvs_buf_write_fd: `%s'", template);
	}

	if (tv != NULL) {
		if (futimes(fd, tv) == -1)
			fatal("cvs_buf_write_stmp: futimes failed");
	}

	cvs_worklist_add(template, &temp_files);

	if (lseek(fd, 0, SEEK_SET) < 0)
		fatal("cvs_buf_write_stmp: lseek: %s", strerror(errno));

	return (fd);
}

u_char *
cvs_buf_get(BUF *bp)
{
	return (bp->cb_buf);
}

int
cvs_buf_differ(const BUF *b1, const BUF *b2)
{
	if (b1->cb_len != b2->cb_len)
		return (1);

	return (memcmp(b1->cb_buf, b2->cb_buf, b1->cb_len));
}
