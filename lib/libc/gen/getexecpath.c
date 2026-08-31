/*	$OpenBSD: getexecpath.c,v 1.1 2026/08/31 15:16:21 deraadt Exp $	*/

/*
 * Copyright (c) 2026 Theo de Raadt
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern char *_execpath;

int
getexecpath(char *buf, size_t size)
{
	size_t r;

	if (_execpath == NULL) {
		errno = ENOENT;
		return (-1);
	}
	if (strlen(_execpath) + 1 > size) {
		errno = ERANGE;
		return (-1);
	}
	(void) strlcpy(buf, _execpath, size);
	return (0);
}
DEF_WEAK(getexecpath);
