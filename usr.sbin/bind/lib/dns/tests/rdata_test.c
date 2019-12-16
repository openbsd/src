/*
 * Copyright (C) 2012, 2013, 2015, 2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: rdata_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/lex.h>
#include <isc/types.h>

#include <dns/callbacks.h>
#include <dns/rdata.h>

#include "dnstest.h"

/*
 * Individual unit tests
 */

/* Successful load test */
ATF_TC(hip);
ATF_TC_HEAD(hip, tc) {
	atf_tc_set_md_var(tc, "descr", "that a oversized HIP record will "
				       "be rejected");
}
ATF_TC_BODY(hip, tc) {
	unsigned char hipwire[DNS_RDATA_MAXLENGTH] = {
				    0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
				    0x04, 0x41, 0x42, 0x43, 0x44, 0x00 };
	unsigned char buf[1024*1024];
	isc_buffer_t source, target;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;

	UNUSED(tc);

	/*
	 * Fill the rest of input buffer with compression pointers.
	 */
	for (i = 12; i < sizeof(hipwire) - 2; i += 2) {
		hipwire[i] = 0xc0;
		hipwire[i+1] = 0x06;
	}

	isc_buffer_init(&source, hipwire, sizeof(hipwire));
	isc_buffer_add(&source, sizeof(hipwire));
	isc_buffer_setactive(&source, i);
	isc_buffer_init(&target, buf, sizeof(buf));
	dns_rdata_init(&rdata);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
	result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
				    dns_rdatatype_hip, &source, &dctx,
				    0, &target);
	dns_decompress_invalidate(&dctx);
	ATF_REQUIRE_EQ(result, DNS_R_FORMERR);
}

ATF_TC(edns_client_subnet);
ATF_TC_HEAD(edns_client_subnet, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "check EDNS client subnet option parsing");
}
ATF_TC_BODY(edns_client_subnet, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* option code with no content */
			{ 0x00, 0x08, 0x0, 0x00 }, 4, ISC_FALSE
		},
		{
			/* Option code family 0, source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x00, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* Option code family 1 (ipv4), source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x01, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* Option code family 2 (ipv6) , source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x02, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* extra octet */
			{
			  0x00, 0x08, 0x00, 0x05,
			  0x00, 0x00, 0x00, 0x00,
			  0x00
			},
			9, ISC_FALSE
		},
		{
			/* source too long for IPv4 */
			{
			  0x00, 0x08, 0x00,    8,
			  0x00, 0x01,   33, 0x00,
			  0x00, 0x00, 0x00, 0x00
			},
			12, ISC_FALSE
		},
		{
			/* source too long for IPv6 */
			{
			  0x00, 0x08, 0x00,   20,
			  0x00, 0x02,  129, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			24, ISC_FALSE
		},
		{
			/* scope too long for IPv4 */
			{
			  0x00, 0x08, 0x00,    8,
			  0x00, 0x01, 0x00,   33,
			  0x00, 0x00, 0x00, 0x00
			},
			12, ISC_FALSE
		},
		{
			/* scope too long for IPv6 */
			{
			  0x00, 0x08, 0x00,   20,
			  0x00, 0x02, 0x00,  129,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			24, ISC_FALSE
		},
		{
			/* length too short for source generic */
			{
			  0x00, 0x08, 0x00,    5,
			  0x00, 0x00,   17, 0x00,
			  0x00, 0x00,
			},
			19, ISC_FALSE
		},
		{
			/* length too short for source ipv4 */
			{
			  0x00, 0x08, 0x00,    7,
			  0x00, 0x01,   32, 0x00,
			  0x00, 0x00, 0x00, 0x00
			},
			11, ISC_FALSE
		},
		{
			/* length too short for source ipv6 */
			{
			  0x00, 0x08, 0x00,   19,
			  0x00, 0x02,  128, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			23, ISC_FALSE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf[1024*1024];
	isc_buffer_t source, target1;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target1, buf, sizeof(buf));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_opt, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		else
			ATF_CHECK(result != ISC_R_SUCCESS);
	}
}

ATF_TC(wks);
ATF_TC_HEAD(wks, tc) {
	atf_tc_set_md_var(tc, "descr", "wks to/from struct");
}
ATF_TC_BODY(wks, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* too short */
			{ 0x00, 0x08, 0x0, 0x00 }, 4, ISC_FALSE
		},
		{
			/* minimal TCP */
			{ 0x00, 0x08, 0x0, 0x00, 6 }, 5, ISC_TRUE
		},
		{
			/* minimal UDP */
			{ 0x00, 0x08, 0x0, 0x00, 17 }, 5, ISC_TRUE
		},
		{
			/* minimal other */
			{ 0x00, 0x08, 0x0, 0x00, 1 }, 5, ISC_TRUE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf1[1024];
	unsigned char buf2[1024];
	isc_buffer_t source, target1, target2;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;
	dns_rdata_in_wks_t wks;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_wks, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &wks, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, dns_rdataclass_in,
					      dns_rdatatype_wks, &wks,
					      &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     test_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, test_data[i].data,
				      test_data[i].len), 0);
	}
}

ATF_TC(isdn);
ATF_TC_HEAD(isdn, tc) {
	atf_tc_set_md_var(tc, "descr", "isdn to/from struct");
}
ATF_TC_BODY(isdn, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* "" */
			{ 0x00 }, 1, ISC_TRUE
		},
		{
			/* "\001" */
			{ 0x1, 0x01 }, 2, ISC_TRUE
		},
		{
			/* "\001" "" */
			{ 0x1, 0x01, 0x00 }, 3, ISC_TRUE
		},
		{
			/* "\000" "\001" */
			{ 0x1, 0x01, 0x01, 0x01 }, 4, ISC_TRUE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf1[1024];
	unsigned char buf2[1024];
	isc_buffer_t source, target1, target2;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;
	dns_rdata_isdn_t isdn;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_isdn, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &isdn, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, dns_rdataclass_in,
					      dns_rdatatype_isdn, &isdn,
					      &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     test_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, test_data[i].data,
				      test_data[i].len), 0);
	}
}

static void
error_callback(dns_rdatacallbacks_t *callbacks, const char *fmt, ...) {
	UNUSED(callbacks);
	UNUSED(fmt);
}

static void
warn_callback(dns_rdatacallbacks_t *callbacks, const char *fmt, ...) {
	UNUSED(callbacks);
	UNUSED(fmt);
}

ATF_TC(nsec);
ATF_TC_HEAD(nsec, tc) {
	atf_tc_set_md_var(tc, "descr", "nsec to/from text/wire");
}
ATF_TC_BODY(nsec, tc) {
	struct {
		const char *data;
		isc_boolean_t ok;
	} text_data[] = {
		{ "", ISC_FALSE },
		{ ".", ISC_FALSE },
		{ ". RRSIG", ISC_TRUE },
		{ NULL, ISC_FALSE },
	};
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} wire_data[] = {
		{ { 0x00 }, 0,  ISC_FALSE },
		{ { 0x00 }, 1, ISC_FALSE },
		{ { 0x00, 0x00 }, 2, ISC_FALSE },
		{ { 0x00, 0x00, 0x00 }, 3, ISC_FALSE },
		{ { 0x00, 0x00, 0x01, 0x02 }, 4, ISC_TRUE }
	};
	unsigned char buf1[1024], buf2[1024];
	isc_buffer_t source, target1, target2;
	isc_result_t result;
	size_t i;
	dns_rdataclass_t rdclass = dns_rdataclass_in;
	dns_rdatatype_t type = dns_rdatatype_nsec;
	isc_lex_t *lex = NULL;
	dns_rdatacallbacks_t callbacks;
	dns_rdata_nsec_t nsec;
	dns_decompress_t dctx;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_lex_create(mctx, 64, &lex);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_rdatacallbacks_init(&callbacks);
	callbacks.error = error_callback;
	callbacks.warn = warn_callback;

	for (i = 0; text_data[i].data != NULL; i++) {
		size_t length = strlen(text_data[i].data);
		isc_buffer_constinit(&source, text_data[i].data, length);
		isc_buffer_add(&source, length);
		result = isc_lex_openbuffer(lex, &source);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

		isc_buffer_init(&target1, buf1, sizeof(buf1));

		result = dns_rdata_fromtext(NULL, rdclass, type, lex,
					    dns_rootname, 0, NULL, &target1,
					    &callbacks);
		if (text_data[i].ok)
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		else
			ATF_CHECK(result != ISC_R_SUCCESS);
	}
	isc_lex_destroy(&lex);

	for (i = 0; i < sizeof(wire_data)/sizeof(wire_data[0]); i++) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		isc_buffer_init(&source, wire_data[i].data, wire_data[i].len);
		isc_buffer_add(&source, wire_data[i].len);
		isc_buffer_setactive(&source, wire_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, rdclass, type, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (wire_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &nsec, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, rdclass, type,
					      &nsec, &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     wire_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, wire_data[i].data,
				      wire_data[i].len), 0);
	}
}

ATF_TC(csync);
ATF_TC_HEAD(csync, tc) {
	atf_tc_set_md_var(tc, "descr", "csync to/from struct");
}
ATF_TC_BODY(csync, tc) {
	struct {
		const char *data;
		isc_boolean_t ok;
	} text_data[] = {
		{ "", ISC_FALSE },
		{ "0", ISC_FALSE },
		{ "0 0", ISC_TRUE },
		{ "0 0 A", ISC_TRUE },
		{ "0 0 NS", ISC_TRUE },
		{ "0 0 AAAA", ISC_TRUE },
		{ "0 0 A AAAA", ISC_TRUE },
		{ "0 0 A NS AAAA", ISC_TRUE },
		{ "0 0 A NS AAAA BOGUS", ISC_FALSE },
		{ NULL, ISC_FALSE },
	};
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} wire_data[] = {
		/* short */
		{ { 0x00 }, 0,  ISC_FALSE },
		/* short */
		{ { 0x00 }, 1, ISC_FALSE },
		/* short */
		{ { 0x00, 0x00 }, 2, ISC_FALSE },
		/* short */
		{ { 0x00, 0x00, 0x00 }, 3, ISC_FALSE },
		/* short */
		{ { 0x00, 0x00, 0x00, 0x00 }, 4, ISC_FALSE },
		/* short */
		{ { 0x00, 0x00, 0x00, 0x00, 0x00 }, 5, ISC_FALSE },
		/* serial + flags only  */
		{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 6, ISC_TRUE },
		/* bad type map */
		{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 7, ISC_FALSE },
		/* bad type map */
		{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		    8, ISC_FALSE },
		/* good type map */
		{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02 },
		    9, ISC_TRUE }
	};
	unsigned char buf1[1024];
	unsigned char buf2[1024];
	isc_buffer_t source, target1, target2;
	isc_result_t result;
	size_t i;
	dns_rdataclass_t rdclass = dns_rdataclass_in;
	dns_rdatatype_t type = dns_rdatatype_csync;
	isc_lex_t *lex = NULL;
	dns_rdatacallbacks_t callbacks;
	dns_rdata_csync_t csync;
	dns_decompress_t dctx;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_lex_create(mctx, 64, &lex);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_rdatacallbacks_init(&callbacks);
	callbacks.error = error_callback;
	callbacks.warn = warn_callback;

	for (i = 0; text_data[i].data != NULL; i++) {
		size_t length = strlen(text_data[i].data);
		isc_buffer_constinit(&source, text_data[i].data, length);
		isc_buffer_add(&source, length);
		result = isc_lex_openbuffer(lex, &source);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

		isc_buffer_init(&target1, buf1, sizeof(buf1));

		result = dns_rdata_fromtext(NULL, rdclass, type, lex,
					    dns_rootname, 0, NULL, &target1,
					    &callbacks);
		if (text_data[i].ok)
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		else
			ATF_CHECK(result != ISC_R_SUCCESS);
	}
	isc_lex_destroy(&lex);

	for (i = 0; i < sizeof(wire_data)/sizeof(wire_data[0]); i++) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		isc_buffer_init(&source, wire_data[i].data, wire_data[i].len);
		isc_buffer_add(&source, wire_data[i].len);
		isc_buffer_setactive(&source, wire_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, rdclass, type, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (wire_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &csync, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, rdclass, type,
					      &csync, &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     wire_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, wire_data[i].data,
				      wire_data[i].len), 0);
	}
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, hip);
	ATF_TP_ADD_TC(tp, edns_client_subnet);
	ATF_TP_ADD_TC(tp, wks);
	ATF_TP_ADD_TC(tp, isdn);
	ATF_TP_ADD_TC(tp, nsec);
	ATF_TP_ADD_TC(tp, csync);

	return (atf_no_error());
}
