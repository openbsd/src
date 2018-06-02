/* $OpenBSD: csi.c,v 1.1 2018/06/02 17:40:33 jsing Exp $ */
/*
 * Copyright (c) 2014, 2018 Joel Sing <jsing@openbsd.org>
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
#include <string.h>

#include <csi.h>

#include "csi_internal.h"

void
csi_err_clear(struct csi_err *err)
{
	err->code = 0;
	err->errnum = 0;
	free(err->msg);
	err->msg = NULL;
}

static int
csi_err_vset(struct csi_err *err, u_int code, int errnum, const char *fmt, va_list ap)
{
	char *errmsg = NULL;
	int rv = -1;

	csi_err_clear(err);

	err->code = code;
	err->errnum = errnum;

	if (vasprintf(&errmsg, fmt, ap) == -1) {
		errmsg = NULL;
		goto err;
	}

	if (errnum == -1) {
		err->msg = errmsg;
		return (0);
	}

	if (asprintf(&err->msg, "%s: %s", errmsg, strerror(errnum)) == -1) {
		err->msg = NULL;
		goto err;
	}
	rv = 0;

 err:
	free(errmsg);

	return (rv);
}

int
csi_err_set(struct csi_err *err, u_int code, const char *fmt, ...)
{
	va_list ap;
	int errnum, rv;

	errnum = errno;

	va_start(ap, fmt);
	rv = csi_err_vset(err, code, errnum, fmt, ap);
	va_end(ap);

	return (rv);
}

int
csi_err_setx(struct csi_err *err, u_int code, const char *fmt, ...)
{
	va_list ap;
	int rv;

	va_start(ap, fmt);
	rv = csi_err_vset(err, code, -1, fmt, ap);
	va_end(ap);

	return (rv);
}
