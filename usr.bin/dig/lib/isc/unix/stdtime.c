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

/* $Id: stdtime.c,v 1.4 2020/02/13 12:03:51 jsg Exp $ */

/*! \file */



#include <stddef.h>	/* NULL */

#include <sys/time.h>

#include <isc/stdtime.h>
#include <isc/util.h>

void
isc_stdtime_get(isc_stdtime_t *t) {
	struct timeval tv;

	/*
	 * Set 't' to the number of seconds since 00:00:00 UTC, January 1,
	 * 1970.
	 */

	REQUIRE(t != NULL);

	RUNTIME_CHECK(gettimeofday(&tv, NULL) != -1);

	*t = (unsigned int)tv.tv_sec;
}
