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

/*%
 * Principal Author: Brian Wellington
 * $Id: dst_result.c,v 1.2 2020/02/12 13:05:03 jsg Exp $
 */

#include <isc/util.h>
#include <dst/result.h>

static const char *text[DST_R_NRESULTS] = {
	"algorithm is unsupported",		/*%< 0 */
	"crypto failure",			/*%< 1 */
	"built with no crypto support",		/*%< 2 */
	"illegal operation for a null key",	/*%< 3 */
	"public key is invalid",		/*%< 4 */
	"private key is invalid",		/*%< 5 */
	"external key",				/*%< 6 */
	"error occurred writing key to disk",	/*%< 7 */
	"invalid algorithm specific parameter",	/*%< 8 */
	"UNUSED9",				/*%< 9 */
	"UNUSED10",				/*%< 10 */
	"sign failure",				/*%< 11 */
	"UNUSED12",				/*%< 12 */
	"UNUSED13",				/*%< 13 */
	"verify failure",			/*%< 14 */
	"not a public key",			/*%< 15 */
	"not a private key",			/*%< 16 */
	"not a key that can compute a secret",	/*%< 17 */
	"failure computing a shared secret",	/*%< 18 */
	"no randomness available",		/*%< 19 */
	"bad key type",				/*%< 20 */
	"no engine",				/*%< 21 */
	"illegal operation for an external key",/*%< 22 */
};

#define DST_RESULT_RESULTSET			2

static isc_boolean_t		once = ISC_FALSE;

static void
initialize_action(void) {
	isc_result_t result;

	result = isc_result_register(ISC_RESULTCLASS_DST, DST_R_NRESULTS,
				     text, DST_RESULT_RESULTSET);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_result_register() failed: %u", result);
}

static void
initialize(void) {
	if (!once) {
		once = ISC_TRUE;
		initialize_action();
	}
}

void
dst_result_register(void) {
	initialize();
}

/*! \file */
