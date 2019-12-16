/*
 * Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: time_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <dns/time.h>

#include "dnstest.h"

#define TEST_ORIGIN	"test"

/*
 * Individual unit tests
 */

/* value = 0xfffffffff <-> 19691231235959 */
ATF_TC(epoch_minus_one);
ATF_TC_HEAD(epoch_minus_one, tc) {
  atf_tc_set_md_var(tc, "descr", "0xffffffff <-> 19691231235959");
}
ATF_TC_BODY(epoch_minus_one, tc) {
	const char *test_text = "19691231235959";
	const isc_uint32_t test_time = 0xffffffff;
	isc_result_t result;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/* value = 0x000000000 <-> 19700101000000*/
ATF_TC(epoch);
ATF_TC_HEAD(epoch, tc) {
  atf_tc_set_md_var(tc, "descr", "0x00000000 <-> 19700101000000");
}
ATF_TC_BODY(epoch, tc) {
	const char *test_text = "19700101000000";
	const isc_uint32_t test_time = 0x00000000;
	isc_result_t result;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/* value = 0x7fffffff <-> 20380119031407 */
ATF_TC(half_maxint);
ATF_TC_HEAD(half_maxint, tc) {
  atf_tc_set_md_var(tc, "descr", "0x7fffffff <-> 20380119031407");
}
ATF_TC_BODY(half_maxint, tc) {
	const char *test_text = "20380119031407";
	const isc_uint32_t test_time = 0x7fffffff;
	isc_result_t result;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/* value = 0x80000000 <-> 20380119031408 */
ATF_TC(half_plus_one);
ATF_TC_HEAD(half_plus_one, tc) {
  atf_tc_set_md_var(tc, "descr", "0x80000000 <-> 20380119031408");
}
ATF_TC_BODY(half_plus_one, tc) {
	const char *test_text = "20380119031408";
	const isc_uint32_t test_time = 0x80000000;
	isc_result_t result;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/* value = 0xef68f5d0 <-> 19610307130000 */
ATF_TC(fifty_before);
ATF_TC_HEAD(fifty_before, tc) {
  atf_tc_set_md_var(tc, "descr", "0xef68f5d0 <-> 19610307130000");
}
ATF_TC_BODY(fifty_before, tc) {
	isc_result_t result;
	const char *test_text = "19610307130000";
	const isc_uint32_t test_time = 0xef68f5d0;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/* value = 0x4d74d6d0 <-> 20110307130000 */
ATF_TC(some_ago);
ATF_TC_HEAD(some_ago, tc) {
  atf_tc_set_md_var(tc, "descr", "0x4d74d6d0 <-> 20110307130000");
}
ATF_TC_BODY(some_ago, tc) {
	const char *test_text = "20110307130000";
	const isc_uint32_t test_time = 0x4d74d6d0;
	isc_result_t result;
	isc_buffer_t target;
	isc_uint32_t when;
	char buf[128];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	memset(buf, 0, sizeof(buf));
	isc_buffer_init(&target, buf, sizeof(buf));
	result = dns_time32_totext(test_time, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_STREQ(buf, test_text);
	result = dns_time32_fromtext(test_text, &when);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(when, test_time);
	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, epoch_minus_one);
	ATF_TP_ADD_TC(tp, epoch);
	ATF_TP_ADD_TC(tp, half_maxint);
	ATF_TP_ADD_TC(tp, half_plus_one);
	ATF_TP_ADD_TC(tp, fifty_before);
	ATF_TP_ADD_TC(tp, some_ago);

	return (atf_no_error());
}

