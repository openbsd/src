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

/* $Id: dns_time.c,v 1.7 2020/04/02 16:57:45 florian Exp $ */

/*! \file */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <isc/region.h>
#include <isc/serial.h>
#include <isc/result.h>

#include <dns/time.h>

static isc_result_t
dns_time64_totext(time_t t, isc_buffer_t *target) {
	struct tm *tm;
	char buf[sizeof("YYYYMMDDHHMMSS")];
	size_t l;
	isc_region_t region;

	tm = gmtime(&t);
	if ((l = strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", tm)) == 0)
		return (ISC_R_NOSPACE);

	isc_buffer_availableregion(target, &region);

	if (l > region.length)
		return (ISC_R_NOSPACE);

	memmove(region.base, buf, l);
	isc_buffer_add(target, l);
	return (ISC_R_SUCCESS);
}

static time_t
dns_time64_from32(uint32_t value) {
	uint32_t now32;
	time_t start;
	time_t t;

	time(&start);
	now32 = (uint32_t) start;

	/* Adjust the time to the closest epoch. */
	if (isc_serial_gt(value, now32))
		t = start + (value - now32);
	else
		t = start - (now32 - value);

	return (t);
}

isc_result_t
dns_time32_totext(uint32_t value, isc_buffer_t *target) {
	return (dns_time64_totext(dns_time64_from32(value), target));
}
