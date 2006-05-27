/*	$OpenBSD: getcwd.c,v 1.17 2006/05/27 18:06:29 pedro Exp $	*/

/*
 * Copyright (c) 2005 Marius Eriksen <marius@openbsd.org>
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
#include <errno.h>
#include <stdlib.h>

int __getcwd(char *buf, size_t len);

char *
getcwd(char *buf, size_t size)
{
	char *allocated = NULL;

	if (buf != NULL && size == 0) {
		errno = EINVAL;
		return (NULL);
	}

	if (buf == NULL &&
	    (allocated = buf = malloc(size = MAXPATHLEN)) == NULL)
		return (NULL);

	if (__getcwd(buf, size) == -1) {
		free(allocated);
		return (NULL);
	}

	return (buf);
}
