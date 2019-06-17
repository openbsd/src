/*	$OpenBSD: realpath.c,v 1.26 2019/06/17 03:13:17 deraadt Exp $ */
/*
 * Copyright (c) 2019 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2019 Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <stdarg.h>

int __realpath(const char *pathname, char *resolved);
PROTO_NORMAL(__realpath);

/*
 * wrapper for kernel __realpath
 */

char *
realpath(const char *path, char *resolved)
{
	char rbuf[PATH_MAX];

	if (__realpath(path, rbuf) == -1) {
		/*
		 * XXX XXX XXX
		 *
		 * The old userland implementation strips trailing slashes.
		 * According to Dr. POSIX, realpathing "/bsd" should be fine,
		 * realpathing "/bsd/" should return ENOTDIR.
		 *
		 * Similar, but *different* to the above, The old userland
		 * implementation allows for realpathing "/nonexistent" but
		 * not "/nonexistent/", Both those should return ENOENT
		 * according to POSIX.
		 *
		 * This hack should go away once we decide to match POSIX.
		 * which we should as soon as is convenient.
		 */
		if (errno == ENOTDIR) {
			char pbuf[PATH_MAX];
			ssize_t i;

			if (strlcpy(pbuf, path, sizeof(pbuf)) >= sizeof(pbuf)) {
				errno = ENAMETOOLONG;
				return NULL;
			}
			/* Try again without the trailing slashes. */
			for (i = strlen(pbuf); i > 1 && pbuf[i - 1] == '/'; i--)
				pbuf[i - 1] = '\0';
			if (__realpath(pbuf, rbuf) == -1)
				return NULL;
		} else
			return NULL;
	}

	if (resolved == NULL)
		return (strdup(rbuf));
	strlcpy(resolved, rbuf, PATH_MAX);
	return (resolved);
}
