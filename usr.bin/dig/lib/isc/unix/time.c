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

/* $Id: time.c,v 1.18 2020/02/16 21:11:31 florian Exp $ */

/*! \file */

#include <sys/time.h>
#include <time.h>

#include <isc/time.h>
#include <isc/util.h>

#define NS_PER_S	1000000000	/*%< Nanoseconds per second. */
#define NS_PER_US	1000		/*%< Nanoseconds per microsecond. */

uint64_t
isc_time_microdiff(const struct timespec *t1, const struct timespec *t2) {
	struct timespec res;

	REQUIRE(t1 != NULL && t2 != NULL);

	timespecsub(t1, t2, &res);
	if (res.tv_sec < 0)
		return 0;

	return ((res.tv_sec * NS_PER_S + res.tv_nsec) / NS_PER_US);
}
