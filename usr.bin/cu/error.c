/* $OpenBSD: error.c,v 1.1 2012/07/10 10:28:05 nicm Exp $ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cu.h"

/*
 * Once we've configured termios, we need to use \r\n to end lines, so use our
 * own versions of warn/warnx/err/errx.
 */

extern char	*__progname;

void
cu_err(int eval, const char *fmt, ...)
{
	va_list ap;

	restore_termios();

	va_start(ap, fmt);
	verr(eval, fmt, ap);
}

void
cu_errx(int eval, const char *fmt, ...)
{
	va_list ap;

	restore_termios();

	va_start(ap, fmt);
	verrx(eval, fmt, ap);
}

void
cu_warn(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", __progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, ": %s\r\n", strerror(errno));
}

void
cu_warnx(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", __progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\r\n");
}
