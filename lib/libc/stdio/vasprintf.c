/*	$OpenBSD: vasprintf.c,v 1.9 2004/09/28 18:12:44 otto Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: vasprintf.c,v 1.9 2004/09/28 18:12:44 otto Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int
vasprintf(char **str, const char *fmt, _BSD_VA_LIST_ ap)
{
	int ret;
	FILE f;
	unsigned char *_base;

	f._file = -1;
	f._flags = __SWR | __SSTR | __SALC;
	f._bf._base = f._p = (unsigned char *)malloc(128);
	if (f._bf._base == NULL)
		goto err;
	f._bf._size = f._w = 127;		/* Leave room for the NUL */
	ret = vfprintf(&f, fmt, ap);
	if (ret == -1)
		goto err;
	*f._p = '\0';
	_base = realloc(f._bf._base, ret + 1);
	if (_base == NULL)
		goto err;
	*str = (char *)_base;
	return (ret);

err:
	if (f._bf._base) {
		free(f._bf._base);
		f._bf._base = NULL;
	}
	*str = NULL;
	errno = ENOMEM;
	return (-1);
}
