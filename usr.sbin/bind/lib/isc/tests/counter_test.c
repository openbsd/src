/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>
#include <stdlib.h>

#include <atf-c.h>

#include <isc/counter.h>
#include <isc/result.h>

#include "isctest.h"

ATF_TC(isc_counter);
ATF_TC_HEAD(isc_counter, tc) {
	atf_tc_set_md_var(tc, "descr", "isc counter object");
}
ATF_TC_BODY(isc_counter, tc) {
	isc_result_t result;
	isc_counter_t *counter = NULL;
	int i;

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_counter_create(mctx, 0, &counter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (i = 0; i < 10; i++) {
		result = isc_counter_increment(counter);
		ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	}

	ATF_CHECK_EQ(isc_counter_used(counter), 10);

	isc_counter_setlimit(counter, 15);
	for (i = 0; i < 10; i++) {
		result = isc_counter_increment(counter);
		if (result != ISC_R_SUCCESS)
			break;
	}

	ATF_CHECK_EQ(isc_counter_used(counter), 15);

	isc_counter_detach(&counter);
	isc_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_counter);
	return (atf_no_error());
}

