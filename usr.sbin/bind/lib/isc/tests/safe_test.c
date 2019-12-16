/*
 * Copyright (C) 2013, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: safe_test.c,v 1.1 2019/12/16 16:31:36 deraadt Exp $ */

/* ! \file */

#include <config.h>

#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include <isc/safe.h>
#include <isc/util.h>

ATF_TC(isc_safe_memequal);
ATF_TC_HEAD(isc_safe_memequal, tc) {
	atf_tc_set_md_var(tc, "descr", "safe memequal()");
}
ATF_TC_BODY(isc_safe_memequal, tc) {
	UNUSED(tc);

	ATF_CHECK(isc_safe_memequal("test", "test", 4));
	ATF_CHECK(!isc_safe_memequal("test", "tesc", 4));
	ATF_CHECK(isc_safe_memequal("\x00\x00\x00\x00",
				    "\x00\x00\x00\x00", 4));
	ATF_CHECK(!isc_safe_memequal("\x00\x00\x00\x00",
				     "\x00\x00\x00\x01", 4));
	ATF_CHECK(!isc_safe_memequal("\x00\x00\x00\x02",
				     "\x00\x00\x00\x00", 4));
}

ATF_TC(isc_safe_memcompare);
ATF_TC_HEAD(isc_safe_memcompare, tc) {
	atf_tc_set_md_var(tc, "descr", "safe memcompare()");
}
ATF_TC_BODY(isc_safe_memcompare, tc) {
	UNUSED(tc);

	ATF_CHECK(isc_safe_memcompare("test", "test", 4) == 0);
	ATF_CHECK(isc_safe_memcompare("test", "tesc", 4) > 0);
	ATF_CHECK(isc_safe_memcompare("test", "tesy", 4) < 0);
	ATF_CHECK(isc_safe_memcompare("\x00\x00\x00\x00",
				      "\x00\x00\x00\x00", 4) == 0);
	ATF_CHECK(isc_safe_memcompare("\x00\x00\x00\x00",
				      "\x00\x00\x00\x01", 4) < 0);
	ATF_CHECK(isc_safe_memcompare("\x00\x00\x00\x02",
				      "\x00\x00\x00\x00", 4) > 0);
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_safe_memequal);
	ATF_TP_ADD_TC(tp, isc_safe_memcompare);
	return (atf_no_error());
}
