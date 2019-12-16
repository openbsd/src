/*
 * Copyright (C) 2012, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: rdatasetstats_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/print.h>

#include <dns/stats.h>

#include "dnstest.h"

/*
 * Helper functions
 */
static void
set_typestats(dns_stats_t *stats, dns_rdatatype_t type,
	      isc_boolean_t stale)
{
	dns_rdatastatstype_t which;
	unsigned int attributes;

	attributes = 0;
	if (stale) attributes |= DNS_RDATASTATSTYPE_ATTR_STALE;
	which = DNS_RDATASTATSTYPE_VALUE(type, attributes);
	dns_rdatasetstats_increment(stats, which);

	attributes = DNS_RDATASTATSTYPE_ATTR_NXRRSET;
	if (stale) attributes |= DNS_RDATASTATSTYPE_ATTR_STALE;
	which = DNS_RDATASTATSTYPE_VALUE(type, attributes);
	dns_rdatasetstats_increment(stats, which);
}

static void
set_nxdomainstats(dns_stats_t *stats, isc_boolean_t stale) {
	dns_rdatastatstype_t which;
	unsigned int attributes;

	attributes = DNS_RDATASTATSTYPE_ATTR_NXDOMAIN;
	if (stale) attributes |= DNS_RDATASTATSTYPE_ATTR_STALE;
	which = DNS_RDATASTATSTYPE_VALUE(0, attributes);
	dns_rdatasetstats_increment(stats, which);
}

static void
checkit1(dns_rdatastatstype_t which, isc_uint64_t value, void *arg) {
	unsigned int attributes;
#if debug
	unsigned int type;
#endif

	UNUSED(which);
	UNUSED(arg);

	attributes = DNS_RDATASTATSTYPE_ATTR(which);
#if debug
	type = DNS_RDATASTATSTYPE_BASE(which);

	fprintf(stderr, "%s%s%s%s/%u, %u\n",
		attributes & DNS_RDATASTATSTYPE_ATTR_OTHERTYPE ? "O" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_NXRRSET ? "!" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_STALE ? "#" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_NXDOMAIN ? "X" : " ",
		type, (unsigned)value);
#endif
	if ((attributes & DNS_RDATASTATSTYPE_ATTR_STALE) == 0)
		ATF_REQUIRE_EQ(value, 1);
	else
		ATF_REQUIRE_EQ(value, 0);
}

static void
checkit2(dns_rdatastatstype_t which, isc_uint64_t value, void *arg) {
	unsigned int attributes;
#if debug
	unsigned int type;
#endif

	UNUSED(which);
	UNUSED(arg);

	attributes = DNS_RDATASTATSTYPE_ATTR(which);
#if debug
	type = DNS_RDATASTATSTYPE_BASE(which);

	fprintf(stderr, "%s%s%s%s/%u, %u\n",
		attributes & DNS_RDATASTATSTYPE_ATTR_OTHERTYPE ? "O" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_NXRRSET ? "!" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_STALE ? "#" : " ",
		attributes & DNS_RDATASTATSTYPE_ATTR_NXDOMAIN ? "X" : " ",
		type, (unsigned)value);
#endif
	if ((attributes & DNS_RDATASTATSTYPE_ATTR_STALE) == 0)
		ATF_REQUIRE_EQ(value, 0);
	else
		ATF_REQUIRE_EQ(value, 1);
}
/*
 * Individual unit tests
 */

ATF_TC(rdatasetstats);
ATF_TC_HEAD(rdatasetstats, tc) {
	atf_tc_set_md_var(tc, "descr", "test that rdatasetstats counters are properly set");
}
ATF_TC_BODY(rdatasetstats, tc) {
	unsigned int i;
	dns_stats_t *stats = NULL;
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_rdatasetstats_create(mctx, &stats);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* First 256 types. */
	for (i = 0; i <= 255; i++)
		set_typestats(stats, (dns_rdatatype_t)i, ISC_FALSE);
	/* Specials */
	set_typestats(stats, dns_rdatatype_dlv, ISC_FALSE);
	set_typestats(stats, (dns_rdatatype_t)1000, ISC_FALSE);
	set_nxdomainstats(stats, ISC_FALSE);

	/*
	 * Check that all counters are set to appropriately.
	 */
	dns_rdatasetstats_dump(stats, checkit1, NULL, 1);

	/* First 256 types. */
	for (i = 0; i <= 255; i++)
		set_typestats(stats, (dns_rdatatype_t)i, ISC_TRUE);
	/* Specials */
	set_typestats(stats, dns_rdatatype_dlv, ISC_TRUE);
	set_typestats(stats, (dns_rdatatype_t)1000, ISC_TRUE);
	set_nxdomainstats(stats, ISC_TRUE);

	/*
	 * Check that all counters are set to appropriately.
	 */
	dns_rdatasetstats_dump(stats, checkit2, NULL, 1);

	dns_stats_detach(&stats);
	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, rdatasetstats);
	return (atf_no_error());
}

