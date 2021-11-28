/*	$OpenBSD: copy.c,v 1.3 2021/11/28 19:28:42 deraadt Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#include "extern.h"

#define _MAXBSIZE (64 * 1024)

/*
 * Return true if all bytes in buffer are zero.
 * A buffer of zero lenght is also considered a zero buffer.
 */
static int
iszero(const void *b, size_t len)
{
	const unsigned char *c = b;

	for (; len > 0; len--) {
		if (*c++ != '\0')
			return 0;
	}
	return 1;
}

static int
copy_internal(int fromfd, int tofd)
{
	char buf[_MAXBSIZE];
	ssize_t r, w;

	while ((r = read(fromfd, buf, sizeof(buf))) > 0) {
		if (iszero(buf, sizeof(buf))) {
			if (lseek(tofd, r, SEEK_CUR) == -1)
				return -1;
		} else {
			w = write(tofd, buf, r);
			if (r != w || w == -1)
				return -1;
		}
	}
	if (r == -1)
		return -1;
	if (ftruncate(tofd, lseek(tofd, 0, SEEK_CUR)) == -1)
		return -1;
	return 0;
}

void
copy_file(int rootfd, const char *basedir, const struct flist *f)
{
	int fromfd, tofd, dfd;

	dfd = openat(rootfd, basedir, O_RDONLY | O_DIRECTORY);
	if (dfd == -1)
		err(ERR_FILE_IO, "%s: openat", basedir);

	fromfd = openat(dfd, f->path, O_RDONLY | O_NOFOLLOW);
	if (fromfd == -1)
		err(ERR_FILE_IO, "%s/%s: openat", basedir, f->path);
	close(dfd);

	tofd = openat(rootfd, f->path,
	    O_WRONLY | O_NOFOLLOW | O_TRUNC | O_CREAT | O_EXCL,
	    0600);
	if (tofd == -1)
		err(ERR_FILE_IO, "%s: openat", f->path);

	if (copy_internal(fromfd, tofd) == -1)
		err(ERR_FILE_IO, "%s: copy file", f->path);

	close(fromfd);
	close(tofd);
}
