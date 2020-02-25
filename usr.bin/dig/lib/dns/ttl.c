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

/* $Id: ttl.c,v 1.5 2020/02/25 05:00:42 jsg Exp $ */

/*! \file */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/region.h>
#include <isc/util.h>

#include <dns/ttl.h>

#define RETERR(x) do { \
	isc_result_t _r = (x); \
	if (_r != ISC_R_SUCCESS) \
		return (_r); \
	} while (0)

/*
 * Helper for dns_ttl_totext().
 */
static isc_result_t
ttlfmt(unsigned int t, const char *s, isc_boolean_t verbose,
       isc_boolean_t space, isc_buffer_t *target)
{
	char tmp[60];
	unsigned int len;
	isc_region_t region;

	if (verbose)
		len = snprintf(tmp, sizeof(tmp), "%s%u %s%s",
			       space ? " " : "",
			       t, s,
			       t == 1 ? "" : "s");
	else
		len = snprintf(tmp, sizeof(tmp), "%u%c", t, s[0]);

	INSIST(len + 1 <= sizeof(tmp));
	isc_buffer_availableregion(target, &region);
	if (len > region.length)
		return (ISC_R_NOSPACE);
	memmove(region.base, tmp, len);
	isc_buffer_add(target, len);

	return (ISC_R_SUCCESS);
}

/*
 * Derived from bind8 ns_format_ttl().
 */
isc_result_t
dns_ttl_totext(uint32_t src, isc_boolean_t verbose, isc_buffer_t *target) {
	unsigned secs, mins, hours, days, weeks, x;

	secs = src % 60;   src /= 60;
	mins = src % 60;   src /= 60;
	hours = src % 24;  src /= 24;
	days = src % 7;    src /= 7;
	weeks = src;       src = 0;
	POST(src);

	x = 0;
	if (weeks != 0) {
		RETERR(ttlfmt(weeks, "week", verbose, ISC_TF(x > 0), target));
		x++;
	}
	if (days != 0) {
		RETERR(ttlfmt(days, "day", verbose, ISC_TF(x > 0), target));
		x++;
	}
	if (hours != 0) {
		RETERR(ttlfmt(hours, "hour", verbose, ISC_TF(x > 0), target));
		x++;
	}
	if (mins != 0) {
		RETERR(ttlfmt(mins, "minute", verbose, ISC_TF(x > 0), target));
		x++;
	}
	if (secs != 0 ||
	    (weeks == 0 && days == 0 && hours == 0 && mins == 0)) {
		RETERR(ttlfmt(secs, "second", verbose, ISC_TF(x > 0), target));
		x++;
	}
	INSIST (x > 0);
	/*
	 * If only a single unit letter is printed, print it
	 * in upper case. (Why?  Because BIND 8 does that.
	 * Presumably it has a reason.)
	 */
	if (x == 1 && !verbose) {
		isc_region_t region;
		/*
		 * The unit letter is the last character in the
		 * used region of the buffer.
		 *
		 * toupper() does not need its argument to be masked of cast
		 * here because region.base is type unsigned char *.
		 */
		isc_buffer_usedregion(target, &region);
		region.base[region.length - 1] =
			toupper(region.base[region.length - 1]);
	}
	return (ISC_R_SUCCESS);
}
