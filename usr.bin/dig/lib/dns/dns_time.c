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

/* $Id: dns_time.c,v 1.6 2020/02/24 13:49:38 jsg Exp $ */

/*! \file */

#include <stdio.h>
#include <string.h>		/* Required for HP/UX (and others?) */
#include <time.h>

#include <isc/region.h>
#include <isc/serial.h>
#include <isc/result.h>

#include <dns/time.h>

static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

isc_result_t
dns_time64_totext(int64_t t, isc_buffer_t *target) {
	struct tm tm;
	char buf[sizeof("!!!!!!YYYY!!!!!!!!MM!!!!!!!!DD!!!!!!!!HH!!!!!!!!MM!!!!!!!!SS")];
	int secs;
	unsigned int l;
	isc_region_t region;

/*
 * Warning. Do NOT use arguments with side effects with these macros.
 */
#define is_leap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#define year_secs(y) ((is_leap(y) ? 366 : 365 ) * 86400)
#define month_secs(m,y) ((days[m] + ((m == 1 && is_leap(y)) ? 1 : 0 )) * 86400)

	tm.tm_year = 70;
	while (t < 0) {
		if (tm.tm_year == 0)
			return (ISC_R_RANGE);
		tm.tm_year--;
		secs = year_secs(tm.tm_year + 1900);
		t += secs;
	}
	while ((secs = year_secs(tm.tm_year + 1900)) <= t) {
		t -= secs;
		tm.tm_year++;
		if (tm.tm_year + 1900 > 9999)
			return (ISC_R_RANGE);
	}
	tm.tm_mon = 0;
	while ((secs = month_secs(tm.tm_mon, tm.tm_year + 1900)) <= t) {
		t -= secs;
		tm.tm_mon++;
	}
	tm.tm_mday = 1;
	while (86400 <= t) {
		t -= 86400;
		tm.tm_mday++;
	}
	tm.tm_hour = 0;
	while (3600 <= t) {
		t -= 3600;
		tm.tm_hour++;
	}
	tm.tm_min = 0;
	while (60 <= t) {
		t -= 60;
		tm.tm_min++;
	}
	tm.tm_sec = (int)t;
				 /* yyyy  mm  dd  HH  MM  SS */
	snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		 tm.tm_hour, tm.tm_min, tm.tm_sec);

	isc_buffer_availableregion(target, &region);
	l = strlen(buf);

	if (l > region.length)
		return (ISC_R_NOSPACE);

	memmove(region.base, buf, l);
	isc_buffer_add(target, l);
	return (ISC_R_SUCCESS);
}

int64_t
dns_time64_from32(uint32_t value) {
	time_t now;
	int64_t start;
	int64_t t;

	/*
	 * Adjust the time to the closest epoch.  This should be changed
	 * to use a 64-bit counterpart to time() if one ever
	 * is defined, but even the current code is good until the year
	 * 2106.
	 */
	time(&now);
	start = (int64_t) now;
	if (isc_serial_gt(value, now))
		t = start + (value - now);
	else
		t = start - (now - value);

	return (t);
}

isc_result_t
dns_time32_totext(uint32_t value, isc_buffer_t *target) {
	return (dns_time64_totext(dns_time64_from32(value), target));
}
