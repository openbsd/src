/*	$OpenBSD: atomic.c,v 1.3 2008/11/05 12:49:58 sobrado Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Charles Longeau <chl@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

ssize_t
atomic_write(int d, const void *buf, size_t nbytes)
{
	ssize_t ret;
	size_t n = nbytes;

	do {
		ret = write(d, buf, n);
		if (ret == -1 && errno != EINTR)
			return -1;
		if (ret != -1)
			n -= ret;
	} while (n > 0 || (ret == -1 && errno == EINTR));

	return nbytes;
}

ssize_t
atomic_read(int d, void *buf, size_t nbytes)
{
	ssize_t ret;
	size_t n = nbytes;

	do {
		ret = read(d, buf, n);
		if (ret == -1 && errno != EINTR)
			return -1;
		if (ret != -1)
			n -= ret;

		if (ret == 0 && n != 0)
			return -1;

	} while (n > 0 || (ret == -1 && errno == EINTR));

	return nbytes;
}

ssize_t
atomic_printfd(int d, const char *fmt, ...)
{
	int ret;
	char *buf;

	va_list ap;
	va_start(ap, fmt);

	if ((ret = vasprintf(&buf, fmt, ap)) == -1) {
		va_end(ap);
		return -1;
	}
	va_end(ap);

	ret = atomic_write(d, buf, ret);
	free(buf);

	return ret;
}
