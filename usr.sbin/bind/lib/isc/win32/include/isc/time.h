/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: time.h,v 1.19.2.1 2001/09/05 00:38:13 gson Exp $ */

#ifndef ISC_TIME_H
#define ISC_TIME_H 1

#include <windows.h>

#include <isc/lang.h>
#include <isc/types.h>

/***
 *** Intervals
 ***/

/*
 * The contents of this structure are private, and MUST NOT be accessed
 * directly by callers.
 *
 * The contents are exposed only to allow callers to avoid dynamic allocation.
 */
struct isc_interval {
	isc_int64_t interval;
};

extern isc_interval_t *isc_interval_zero;

ISC_LANG_BEGINDECLS

void
isc_interval_set(isc_interval_t *i,
		 unsigned int seconds, unsigned int nanoseconds);
/*
 * Set 'i' to a value representing an interval of 'seconds' seconds and
 * 'nanoseconds' nanoseconds, suitable for use in isc_time_add() and
 * isc_time_subtract().
 *
 * Requires:
 *
 *	't' is a valid pointer.
 *	nanoseconds < 1000000000.
 */

isc_boolean_t
isc_interval_iszero(isc_interval_t *i);
/*
 * Returns ISC_TRUE iff. 'i' is the zero interval.
 *
 * Requires:
 *
 *	'i' is a valid pointer.
 */

/***
 *** Absolute Times
 ***/

/*
 * The contents of this structure are private, and MUST NOT be accessed
 * directly by callers.
 *
 * The contents are exposed only to allow callers to avoid dynamic allocation.
 */

struct isc_time {
	FILETIME absolute;
};

extern isc_time_t *isc_time_epoch;

void
isc_time_settoepoch(isc_time_t *t);
/*
 * Set 't' to the time of the epoch.
 *
 * Notes:
 * 	The date of the epoch is platform-dependent.
 *
 * Requires:
 *
 *	't' is a valid pointer.
 */

isc_boolean_t
isc_time_isepoch(isc_time_t *t);
/*
 * Returns ISC_TRUE iff. 't' is the epoch ("time zero").
 *
 * Requires:
 *
 *	't' is a valid pointer.
 */

isc_result_t
isc_time_now(isc_time_t *t);
/*
 * Set 't' to the current absolute time.
 *
 * Requires:
 *
 *	't' is a valid pointer.
 *
 * Returns:
 *
 *	Success
 *	Unexpected error
 *		Getting the time from the system failed.
 *	Out of range
 *		The time from the system is too large to be represented
 *		in the current definition of isc_time_t.
 */

isc_result_t
isc_time_nowplusinterval(isc_time_t *t, isc_interval_t *i);
/*
 * Set *t to the current absolute time + i.
 *
 * Note:
 *	This call is equivalent to:
 *
 *		isc_time_now(t);
 *		isc_time_add(t, i, t);
 *
 * Requires:
 *
 *	't' and 'i' are valid pointers.
 *
 * Returns:
 *
 *	Success
 *	Unexpected error
 *		Getting the time from the system failed.
 *	Out of range
 *		The interval added to the time from the system is too large to
 *		be represented in the current definition of isc_time_t.
 */

int
isc_time_compare(isc_time_t *t1, isc_time_t *t2);
/*
 * Compare the times referenced by 't1' and 't2'
 *
 * Requires:
 *
 *	't1' and 't2' are valid pointers.
 *
 * Returns:
 *
 *	-1		t1 < t2		(comparing times, not pointers)
 *	0		t1 = t2
 *	1		t1 > t2
 */

isc_result_t
isc_time_add(isc_time_t *t, isc_interval_t *i, isc_time_t *result);
/*
 * Add 'i' to 't', storing the result in 'result'.
 *
 * Requires:
 *
 *	't', 'i', and 'result' are valid pointers.
 *
 * Returns:
 * 	Success
 *	Out of range
 * 		The interval added to the time is too large to
 *		be represented in the current definition of isc_time_t.
 */

isc_result_t
isc_time_subtract(isc_time_t *t, isc_interval_t *i, isc_time_t *result);
/*
 * Subtract 'i' from 't', storing the result in 'result'.
 *
 * Requires:
 *
 *	't', 'i', and 'result' are valid pointers.
 *
 * Returns:
 *	Success
 *	Out of range
 *		The interval is larger than the time since the epoch.
 */

isc_uint64_t
isc_time_microdiff(isc_time_t *t1, isc_time_t *t2);
/*
 * Find the difference in milliseconds between time t1 and time t2.
 * t2 is the subtrahend of t1; ie, difference = t1 - t2.
 *
 * Requires:
 *
 *	't1' and 't2' are valid pointers.
 *
 * Returns:
 *	The difference of t1 - t2, or 0 if t1 <= t2.
 */

isc_uint32_t
isc_time_nanoseconds(isc_time_t *t);
/*
 * Return the number of nanoseconds stored in a time structure.
 *
 * Notes:
 *	This is the number of nanoseconds in excess of the the number
 *	of seconds since the epoch; it will always be less than one
 *	full second.
 *
 * Requires:
 *	't' is a valid pointer.
 *
 * Ensures:
 *	The returned value is less than 1*10^9.
 */

void
isc_time_formattimestamp(const isc_time_t *t, char *buf, unsigned int len);
/*
 * Format the time 't' into the buffer 'buf' of length 'len',
 * using a format like "Aug 30 04:06:47.997" and the local time zone.
 * If the text does not fit in the buffer, the result is indeterminate,
 * but is always guaranteed to be null terminated.
 *
 *  Requires:
 *      'len' > 0
 *      'buf' points to an array of at least len chars
 *
 */

ISC_LANG_ENDDECLS

#endif /* ISC_TIME_H */
