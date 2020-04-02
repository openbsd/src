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

/* $Id: time.h,v 1.4 2020/04/02 16:57:45 florian Exp $ */

#ifndef DNS_TIME_H
#define DNS_TIME_H 1

/*! \file dns/time.h */

/***
 ***	Imports
 ***/

#include <isc/buffer.h>

/***
 ***	Functions
 ***/

isc_result_t
dns_time32_totext(uint32_t value, isc_buffer_t *target);
/*%<
 * Convert 32-bit cyclic time value into a YYYYMMDDHHMMSS text representation
 * and append it to 'target'.
 * Of those dates whose counts of seconds since Jan 1 1970 0:00 GMT
 * are congruent with 'value' modulo 2^32, the one closest to the
 * current date is chosen.
 */

#endif /* DNS_TIME_H */
