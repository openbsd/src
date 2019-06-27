/*	$Id: log.c,v 1.8 2019/06/27 18:03:37 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

extern int verbose;

/*
 * Log a message at level "level", starting at zero, which corresponds
 * to the current verbosity level opts->verbose (whose verbosity starts
 * at one).
 */
void
rsync_log(const char *fname, size_t line, int level, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (verbose < level + 1)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	if (level <= 0 && buf != NULL)
		fprintf(stderr, "%s\n", buf);
	else if (level > 0)
		fprintf(stderr, "%s:%zu%s%s\n", fname, line,
		    (buf != NULL) ? ": " : "",
		    (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * This reports an error---not a warning.
 * However, it is not like errx(3) in that it does not exit.
 */
void
rsync_errx(const char *fname, size_t line, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	fprintf(stderr, "%s:%zu: error%s%s\n", fname, line,
	   (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * This reports an error---not a warning.
 * However, it is not like err(3) in that it does not exit.
 */
void
rsync_err(const char *fname, size_t line, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;
	int	 er = errno;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	fprintf(stderr, "%s:%zu: error%s%s: %s\n", fname, line,
	   (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "", strerror(er));
	free(buf);
}

/*
 * Prints a non-terminal error message, that is, when reporting on the
 * chain of functions from which the actual warning occurred.
 */
void
rsync_errx1(const char *fname, size_t line, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (verbose < 1)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	fprintf(stderr, "%s:%zu: error%s%s\n", fname, line,
	   (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * Prints a warning message.
 */
void
rsync_warnx(const char *fname, size_t line, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	fprintf(stderr, "%s:%zu: warning%s%s\n", fname, line,
	   (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * Prints a warning with an errno.
 * It uses a level detector for when to inhibit printing.
 */
void
rsync_warn(int level, const char *fname, size_t line, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;
	int	 er = errno;

	if (verbose < level)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	fprintf(stderr, "%s:%zu: warning%s%s: %s\n", fname, line,
	   (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "", strerror(er));
	free(buf);
}
