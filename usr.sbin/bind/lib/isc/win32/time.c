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

/* $ISC: time.c,v 1.24.2.3 2001/10/01 01:42:38 gson Exp $ */

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <windows.h>

#include <isc/assertions.h>
#include <isc/time.h>
#include <isc/util.h>

/*
 * struct FILETIME uses "100-nanoseconds intervals".
 * NS / S = 1000000000 (10^9).
 * While it is reasonably obvious that this makes the needed
 * conversion factor 10^7, it is coded this way for additional clarity.
 */
#define NS_PER_S 	1000000000
#define NS_INTERVAL	100
#define INTERVALS_PER_S (NS_PER_S / NS_INTERVAL)
#define UINT64_MAX	_UI64_MAX

/***
 *** Absolute Times
 ***/

static isc_time_t epoch = { { 0, 0 } };
isc_time_t *isc_time_epoch = &epoch;

/***
 *** Intervals
 ***/

static isc_interval_t zero_interval = { 0 };
isc_interval_t *isc_interval_zero = &zero_interval;

void
isc_interval_set(isc_interval_t *i, unsigned int seconds,
		 unsigned int nanoseconds)
{
	REQUIRE(i != NULL);
	REQUIRE(nanoseconds < NS_PER_S);

	i->interval = (LONGLONG)seconds * INTERVALS_PER_S
		+ nanoseconds / NS_INTERVAL;
}

isc_boolean_t
isc_interval_iszero(isc_interval_t *i) {
	REQUIRE(i != NULL);
	if (i->interval == 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

void
isc_time_settoepoch(isc_time_t *t) {
	REQUIRE(t != NULL);

	t->absolute.dwLowDateTime = 0;
	t->absolute.dwHighDateTime = 0;
}

isc_boolean_t
isc_time_isepoch(isc_time_t *t) {
	REQUIRE(t != NULL);

	if (t->absolute.dwLowDateTime == 0 &&
	    t->absolute.dwHighDateTime == 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

isc_result_t
isc_time_now(isc_time_t *t) {
	REQUIRE(t != NULL);

	GetSystemTimeAsFileTime(&t->absolute);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_nowplusinterval(isc_time_t *t, isc_interval_t *i) {
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL);
	REQUIRE(i != NULL);

	GetSystemTimeAsFileTime(&t->absolute);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (UINT64_MAX - i1.QuadPart < (unsigned __int64)i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart += i->interval;

	t->absolute.dwLowDateTime  = i1.LowPart;
	t->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

int
isc_time_compare(isc_time_t *t1, isc_time_t *t2) {
	REQUIRE(t1 != NULL && t2 != NULL);

	return ((int)CompareFileTime(&t1->absolute, &t2->absolute));
}

isc_result_t
isc_time_add(isc_time_t *t, isc_interval_t *i, isc_time_t *result) {
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL && i != NULL && result != NULL);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (UINT64_MAX - i1.QuadPart < (unsigned __int64)i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart += i->interval;

	result->absolute.dwLowDateTime = i1.LowPart;
	result->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_subtract(isc_time_t *t, isc_interval_t *i, isc_time_t *result) {
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL && i != NULL && result != NULL);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (i1.QuadPart < (unsigned __int64) i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart -= i->interval;

	result->absolute.dwLowDateTime = i1.LowPart;
	result->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

isc_uint64_t
isc_time_microdiff(isc_time_t *t1, isc_time_t *t2) {
	ULARGE_INTEGER i1, i2;
	LONGLONG i3;

	REQUIRE(t1 != NULL && t2 != NULL);

	i1.LowPart  = t1->absolute.dwLowDateTime;
	i1.HighPart = t1->absolute.dwHighDateTime;
	i2.LowPart  = t2->absolute.dwLowDateTime;
	i2.HighPart = t2->absolute.dwHighDateTime;

	if (i1.QuadPart <= i2.QuadPart)
		return (0);

	/*
	 * Convert to microseconds.
	 */
	i3 = (i1.QuadPart - i2.QuadPart) / 10;

	return (i3);
}

isc_uint32_t
isc_time_nanoseconds(isc_time_t *t) {
	SYSTEMTIME st;

	/*
	 * Convert the time to a SYSTEMTIME structure and the grab the
	 * milliseconds
	 */
	FileTimeToSystemTime(&t->absolute, &st);

	return ((isc_uint32_t)(st.wMilliseconds * 1000000));
}

void
isc_time_formattimestamp(const isc_time_t *t, char *buf, unsigned int len) {
	FILETIME localft;
	SYSTEMTIME st;

	static const char badtime[] = "Bad 00 99:99:99.999";
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	
	REQUIRE(len > 0);
	if (FileTimeToLocalFileTime(&t->absolute, &localft) &&
	    FileTimeToSystemTime(&localft, &st))
	{
		snprintf(buf, len, "%s %2u %02u:%02u:%02u.%03u",
		months[st.wMonth - 1], st.wDay, st.wHour, st.wMinute,
		st.wSecond, st.wMilliseconds);
	} else {
		snprintf(buf, len, badtime);
	}
}
