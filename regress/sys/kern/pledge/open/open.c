/*	$OpenBSD: open.c,v 1.1 2026/03/27 05:06:33 dgl Exp $	*/
/*
 * Copyright (c) 2026 David Leadbeater <dgl@openbsd.org>
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

#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/*
 * __pledge_open(2) is libc internal. This is for testing kernel semantics
 * only, do not use this outside of regress.
 */
int _libc___pledge_open(char *path, int flags, ...);

#define NO_PLEDGE "[NO PLEDGE]"

int
main(int argc, char **argv)
{
	int fd;
	char *promise, *path;
	struct stat sb;

	if (argc != 3)
		errx(1, "argc: %d", argc);

	promise = argv[1];
	path = argv[2];

	if (strcmp(promise, NO_PLEDGE) != 0 && pledge(promise, NULL) == -1)
		err(1, "pledge %s", promise);

	fd = _libc___pledge_open(path, O_RDONLY);
	if (fd == -1)
		err(2, "open %s", path);

	if (fstat(fd, &sb) == -1)
		err(3, "fstat %s", path);

	/* __pledge_open marks fds so certain operations are not allowed. */
	if (strcmp(promise, NO_PLEDGE) != 0 && S_ISREG(sb.st_mode))
		if (fchmod(fd, 0) != -1)
			errx(4, "fchmod succeeded");
}
