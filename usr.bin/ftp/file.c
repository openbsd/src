/*	$OpenBSD: file.c,v 1.2 2019/05/12 20:58:19 jasper Exp $ */

/*
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@openbsd.org>
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

#include "ftp.h"

struct imsgbuf;

static FILE	*src_fp;

struct url *
file_request(struct imsgbuf *ibuf, struct url *url, off_t *offset, off_t *sz)
{
	struct stat	sb;
	int		src_fd;

	if ((src_fd = fd_request(url->path, O_RDONLY, NULL)) == -1)
		err(1, "Can't open file %s", url->path);

	if (fstat(src_fd, &sb) == 0)
		*sz = sb.st_size;

	if ((src_fp = fdopen(src_fd, "r")) == NULL)
		err(1, "%s: fdopen", __func__);

	if (*offset && fseeko(src_fp, *offset, SEEK_SET) == -1)
		err(1, "%s: fseeko", __func__);

	return url;
}

void
file_save(struct url *url, FILE *dst_fp, off_t *offset)
{
	copy_file(dst_fp, src_fp, offset);
	fclose(src_fp);
}
