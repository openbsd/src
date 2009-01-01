/*	$OpenBSD: util.c,v 1.4 2009/01/01 16:15:47 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

int
bsnprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (ret == -1 || ret >= (int)size)
		return 0;

	return 1;
}

/* Close file, signifying temporary error condition (if any) to the caller. */
int
safe_fclose(FILE *fp)
{

	if (fflush(fp)) {
		if (errno == ENOSPC)
			return 0;
		fatal("safe_fclose: fflush");
	}

	if (fsync(fileno(fp)))
		fatal("safe_fclose: fsync");

	if (fclose(fp))
		fatal("safe_fclose: fclose");

	return 1;
}
