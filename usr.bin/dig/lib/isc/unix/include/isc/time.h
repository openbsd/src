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

/* $Id: time.h,v 1.15 2020/02/16 21:11:31 florian Exp $ */

#ifndef ISC_TIME_H
#define ISC_TIME_H 1

/*! \file */

#include <sys/time.h>

#include <inttypes.h>
#include <time.h>

uint64_t
isc_time_microdiff(const struct timespec *t1, const struct timespec *t2);
/*%<
 * Find the difference in microseconds between time t1 and time t2.
 * t2 is the subtrahend of t1; ie, difference = t1 - t2.
 *
 * Requires:
 *
 *\li	't1' and 't2' are valid pointers.
 *
 * Returns:
 *\li	The difference of t1 - t2, or 0 if t1 <= t2.
 */

#endif /* ISC_TIME_H */
