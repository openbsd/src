/*	$OpenBSD: util.c,v 1.5 2015/01/16 00:05:12 deraadt Exp $	*/

/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "installboot.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define BUFSIZE 512

void
filecopy(const char *srcfile, const char *dstfile)
{
	struct stat sb;
	ssize_t sz, n;
	int sfd, dfd;
	char *buf;

	if ((buf = malloc(BUFSIZE)) == NULL)
		err(1, "malloc");

	sfd = open(srcfile, O_RDONLY);
	if (sfd == -1)
		err(1, "open %s", srcfile);
	if (fstat(sfd, &sb) == -1)
		err(1, "fstat");
	sz = sb.st_size;

	dfd = open(dstfile, O_WRONLY|O_CREAT);
	if (dfd == -1)
		err(1, "open %s", dstfile);
	if (fchown(dfd, 0, 0) == -1)
		err(1, "chown");
	if (fchmod(dfd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) == -1)
		err(1, "chmod");

	while (sz > 0) {
		n = MINIMUM(sz, BUFSIZE);
		if ((n = read(sfd, buf, n)) == -1)
			err(1, "read");
		sz -= n;
		if (write(dfd, buf, n) != n)
			err(1, "write");
	}

	ftruncate(dfd, sb.st_size);

	close(dfd);
	close(sfd);
	free(buf);
}

char *
fileprefix(const char *base, const char *path)
{
	char *r, *s;
	int n;

	if ((s = malloc(PATH_MAX)) == NULL)
		err(1, "malloc");
	n = snprintf(s, PATH_MAX, "%s/%s", base, path);
	if (n < 1 || n >= PATH_MAX)
		err(1, "snprintf");
	if ((r = realpath(s, NULL)) == NULL)
		err(1, "realpath");
	free(s);

	return r;
}
