/*	$Id: symlinks.c,v 1.2 2019/02/10 23:24:14 benno Exp $ */
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
#include <sys/param.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

/*
 * Allocate space for a readlink(2) invocation.
 * Returns NULL on failure or a buffer otherwise.
 * The buffer must be passed to free() by the caller.
 */
char *
symlink_read(struct sess *sess, const char *path)
{
	char	*buf = NULL;
	size_t	 sz;
	ssize_t	 nsz = 0;
	void	*pp;

	for (sz = MAXPATHLEN; ; sz *= 2) {
		if (NULL == (pp = realloc(buf, sz + 1))) {
			ERR(sess, "realloc");
			free(buf);
			return NULL;
		}
		buf = pp;

		if (-1 == (nsz = readlink(path, buf, sz))) {
			ERR(sess, "%s: readlink", path);
			free(buf);
			return NULL;
		} else if (0 == nsz) {
			ERRX(sess, "%s: empty link", path);
			free(buf);
			return NULL;
		} else if ((size_t)nsz < sz)
			break;
	}

	assert(NULL != buf);
	assert(nsz > 0);
	buf[nsz] = '\0';
	return buf;
}

/*
 * Allocate space for a readlinkat(2) invocation.
 * Returns NULL on failure or a buffer otherwise.
 * The buffer must be passed to free() by the caller.
 */
char *
symlinkat_read(struct sess *sess, int fd, const char *path)
{
	char	*buf = NULL;
	size_t	 sz;
	ssize_t	 nsz = 0;
	void	*pp;

	for (sz = MAXPATHLEN; ; sz *= 2) {
		if (NULL == (pp = realloc(buf, sz + 1))) {
			ERR(sess, "realloc");
			free(buf);
			return NULL;
		}
		buf = pp;

		if (-1 == (nsz = readlinkat(fd, path, buf, sz))) {
			ERR(sess, "%s: readlinkat", path);
			free(buf);
			return NULL;
		} else if (0 == nsz) {
			ERRX(sess, "%s: empty link", path);
			free(buf);
			return NULL;
		} else if ((size_t)nsz < sz)
			break;
	}

	assert(NULL != buf);
	assert(nsz > 0);
	buf[nsz] = '\0';
	return buf;
}
