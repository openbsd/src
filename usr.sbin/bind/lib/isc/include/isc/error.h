/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: error.h,v 1.4 2020/01/09 18:16:01 florian Exp $ */

#ifndef ISC_ERROR_H
#define ISC_ERROR_H 1

/*! \file isc/error.h */

#include <sys/cdefs.h>
#include <stdarg.h>

#include <isc/formatcheck.h>
#include <isc/lang.h>
#include <isc/likely.h>
#include <isc/platform.h>

ISC_LANG_BEGINDECLS

typedef void (*isc_errorcallback_t)(const char *, int, const char *, va_list);

/*% set unexpected error */
void
isc_error_setunexpected(isc_errorcallback_t);

/*% set fatal error */
void
isc_error_setfatal(isc_errorcallback_t);

/*% unexpected error */
void
isc_error_unexpected(const char *, int, const char *, ...)
     ISC_FORMAT_PRINTF(3, 4);

/*% fatal error */
__dead void
isc_error_fatal(const char *, int, const char *, ...)
ISC_FORMAT_PRINTF(3, 4);

/*% runtimecheck error */
__dead void
isc_error_runtimecheck(const char *, int, const char *);

#define ISC_ERROR_RUNTIMECHECK(cond) \
	((void) (ISC_LIKELY(cond) || \
		 ((isc_error_runtimecheck)(__FILE__, __LINE__, #cond), 0)))

ISC_LANG_ENDDECLS

#endif /* ISC_ERROR_H */
