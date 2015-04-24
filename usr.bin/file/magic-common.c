/* $OpenBSD: magic-common.c,v 1.1 2015/04/24 16:24:11 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "magic.h"

char *
magic_strtoull(const char *s, uint64_t *u)
{
	char	*endptr;

	if (*s == '-')
		return (NULL);
	errno = 0;
	*u = strtoull(s, &endptr, 0);
	if (*s == '\0')
		return (NULL);
	if (errno == ERANGE && *u == ULLONG_MAX)
		return (NULL);
	if (*endptr == 'L')
		endptr++;
	return (endptr);
}

char *
magic_strtoll(const char *s, int64_t *i)
{
	char	*endptr;

	errno = 0;
	*i = strtoll(s, &endptr, 0);
	if (*s == '\0')
		return (NULL);
	if (errno == ERANGE && *i == LLONG_MAX)
		return (NULL);
	if (*endptr == 'L')
		endptr++;
	return (endptr);
}

void
magic_warn(struct magic_line *ml, const char *fmt, ...)
{
	va_list	 ap;
	char	*msg;

	if (!ml->root->warnings)
		return;

	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) == -1) {
		va_end(ap);
		return;
	}
	va_end(ap);

	fprintf(stderr, "%s:%u: %s\n", ml->root->path, ml->line, msg);
	free(msg);
}
