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

/* $Id: parseint.c,v 1.4 2020/02/25 05:00:43 jsg Exp $ */

/*! \file */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <isc/parseint.h>
#include <isc/result.h>

isc_result_t
isc_parse_uint32(uint32_t *uip, const char *string, int base) {
	unsigned long n;
	uint32_t r;
	char *e;
	if (! isalnum((unsigned char)(string[0])))
		return (ISC_R_BADNUMBER);
	errno = 0;
	n = strtoul(string, &e, base);
	if (*e != '\0')
		return (ISC_R_BADNUMBER);
	/*
	 * Where long is 64 bits we need to convert to 32 bits then test for
	 * equality.  This is a no-op on 32 bit machines and a good compiler
	 * will optimise it away.
	 */
	r = (uint32_t)n;
	if ((n == ULONG_MAX && errno == ERANGE) || (n != (unsigned long)r))
		return (ISC_R_RANGE);
	*uip = r;
	return (ISC_R_SUCCESS);
}
