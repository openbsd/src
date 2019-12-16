/*
 * Copyright (C) 2014, 2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: name_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/os.h>
#include <isc/print.h>
#include <isc/thread.h>

#include <dns/name.h>
#include <dns/fixedname.h>
#include "dnstest.h"

/*
 * Individual unit tests
 */

ATF_TC(fullcompare);
ATF_TC_HEAD(fullcompare, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_name_fullcompare test");
}
ATF_TC_BODY(fullcompare, tc) {
	dns_fixedname_t fixed1;
	dns_fixedname_t fixed2;
	dns_name_t *name1;
	dns_name_t *name2;
	dns_namereln_t relation;
	int i;
	isc_result_t result;
	struct {
		const char *name1;
		const char *name2;
		dns_namereln_t relation;
		int order;
		unsigned int nlabels;
	} data[] = {
		/* relative */
		{ "", "", dns_namereln_equal, 0, 0 },
		{ "foo", "", dns_namereln_subdomain, 1, 0 },
		{ "", "foo", dns_namereln_contains, -1, 0 },
		{ "foo", "bar", dns_namereln_none, 4, 0 },
		{ "bar", "foo", dns_namereln_none, -4, 0 },
		{ "bar.foo", "foo", dns_namereln_subdomain, 1, 1 },
		{ "foo", "bar.foo", dns_namereln_contains, -1, 1 },
		{ "baz.bar.foo", "bar.foo", dns_namereln_subdomain, 1, 2 },
		{ "bar.foo", "baz.bar.foo", dns_namereln_contains, -1, 2 },
		{ "foo.example", "bar.example", dns_namereln_commonancestor,
		  4, 1 },

		/* absolute */
		{ ".", ".", dns_namereln_equal, 0, 1 },
		{ "foo.", "bar.", dns_namereln_commonancestor, 4, 1 },
		{ "bar.", "foo.", dns_namereln_commonancestor, -4, 1 },
		{ "foo.example.", "bar.example.", dns_namereln_commonancestor,
		  4, 2 },
		{ "bar.foo.", "foo.", dns_namereln_subdomain, 1, 2 },
		{ "foo.", "bar.foo.", dns_namereln_contains, -1, 2 },
		{ "baz.bar.foo.", "bar.foo.", dns_namereln_subdomain, 1, 3 },
		{ "bar.foo.", "baz.bar.foo.", dns_namereln_contains, -1, 3 },
		{ NULL, NULL, dns_namereln_none, 0, 0 }
	};

	UNUSED(tc);

	dns_fixedname_init(&fixed1);
	name1 = dns_fixedname_name(&fixed1);
	dns_fixedname_init(&fixed2);
	name2 = dns_fixedname_name(&fixed2);
	for (i = 0; data[i].name1 != NULL; i++) {
		int order = 3000;
		unsigned int nlabels = 3000;

		if (data[i].name1[0] == 0) {
			dns_fixedname_init(&fixed1);
		} else {
			result = dns_name_fromstring2(name1, data[i].name1,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		if (data[i].name2[0] == 0) {
			dns_fixedname_init(&fixed2);
		} else {
			result = dns_name_fromstring2(name2, data[i].name2,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		relation = dns_name_fullcompare(name1, name1, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, dns_namereln_equal);
		ATF_REQUIRE_EQ(order, 0);
		ATF_REQUIRE_EQ(nlabels, name1->labels);

		/* Some random initializer */
		order = 3001;
		nlabels = 3001;

		relation = dns_name_fullcompare(name1, name2, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, data[i].relation);
		ATF_REQUIRE_EQ(order, data[i].order);
		ATF_REQUIRE_EQ(nlabels, data[i].nlabels);
	}
}

#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS

/*
 * XXXMUKS: Don't delete this code. It is useful in benchmarking the
 * name parser, but we don't require it as part of the unit test runs.
 */

ATF_TC(benchmark);
ATF_TC_HEAD(benchmark, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Benchmark dns_name_fromwire() implementation");
}

static void *
fromwire_thread(void *arg) {
	unsigned int maxval = 32000000;
	uint8_t data[] = {
		3, 'w', 'w', 'w',
		7, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		7, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
		0
	};
	unsigned char output_data[DNS_NAME_MAXWIRE];
	isc_buffer_t source, target;
	unsigned int i;
	dns_decompress_t dctx;

	UNUSED(arg);

	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, DNS_COMPRESS_NONE);

	isc_buffer_init(&source, data, sizeof(data));
	isc_buffer_add(&source, sizeof(data));
	isc_buffer_init(&target, output_data, sizeof(output_data));

	/* Parse 32 million names in each thread */
	for (i = 0; i < maxval; i++) {
		dns_name_t name;

		isc_buffer_clear(&source);
		isc_buffer_clear(&target);
		isc_buffer_add(&source, sizeof(data));
		isc_buffer_setactive(&source, sizeof(data));

		dns_name_init(&name, NULL);
		(void) dns_name_fromwire(&name, &source, &dctx, 0, &target);
	}

	return (NULL);
}

ATF_TC_BODY(benchmark, tc) {
	isc_result_t result;
	unsigned int i;
	isc_time_t ts1, ts2;
	double t;
	unsigned int nthreads;
	isc_thread_t threads[32];

	UNUSED(tc);

	debug_mem_record = ISC_FALSE;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_time_now(&ts1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	nthreads = ISC_MIN(isc_os_ncpus(), 32);
	nthreads = ISC_MAX(nthreads, 1);
	for (i = 0; i < nthreads; i++) {
		result = isc_thread_create(fromwire_thread, NULL, &threads[i]);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	for (i = 0; i < nthreads; i++) {
		result = isc_thread_join(threads[i], NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	result = isc_time_now(&ts2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	t = isc_time_microdiff(&ts2, &ts1);

	printf("%u dns_name_fromwire() calls, %f seconds, %f calls/second\n",
	       nthreads * 32000000, t / 1000000.0,
	       (nthreads * 32000000) / (t / 1000000.0));

	dns_test_end();
}

#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, fullcompare);
#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS
	ATF_TP_ADD_TC(tp, benchmark);
#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

	return (atf_no_error());
}

