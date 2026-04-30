/*	$OpenBSD: log.c,v 1.1 2026/04/30 11:06:29 tb Exp $	*/

/*
 * Copyright (c) 2026 Theo Buehler <tb@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "log.h"

void
log_warn(const char *emsg, ...)
{
	va_list		 ap;
	int		 saved_errno = errno;

	va_start(ap, emsg);
	vwarn(emsg, ap);
	va_end(ap);

	errno = saved_errno;
}

void
log_warnx(const char *emsg, ...)
{
	va_list		 ap;
	int		 saved_errno = errno;

	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);

	errno = saved_errno;
}

void
log_info(const char *emsg, ...)
{
}

void
log_debug(const char *emsg, ...)
{
}

void
fatal(const char *emsg, ...)
{
	va_list	ap;

	va_start(ap, emsg);
	verr(1, emsg, ap);
	va_end(ap);
	exit(1);
}

void
fatalx(const char *emsg, ...)
{
	va_list	ap;

	va_start(ap, emsg);
	verrx(1, emsg, ap);
	va_end(ap);
	exit(1);
}
