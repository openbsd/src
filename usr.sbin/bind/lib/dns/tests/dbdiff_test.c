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

/* $Id: dbdiff_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>
#include <stdlib.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/name.h>
#include <dns/journal.h>

#include "dnstest.h"

/*
 * Helper functions
 */

#define	BUFLEN		255
#define	BIGBUFLEN	(64 * 1024)
#define TEST_ORIGIN	"test"

static void
test_create(const atf_tc_t *tc, dns_db_t **old, dns_db_t **new) {
	isc_result_t result;

	result = dns_test_loaddb(old, dns_dbtype_zone, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-old"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(new, dns_dbtype_zone, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-new"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
}

/*
 * Individual unit tests
 */

ATF_TC(diffx_same);
ATF_TC_HEAD(diffx_same, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_db_diffx of identical content");
	atf_tc_set_md_var(tc, "X-old", "testdata/diff/zone1.data");
	atf_tc_set_md_var(tc, "X-new", "testdata/diff/zone1.data"); }
ATF_TC_BODY(diffx_same, tc) {
	dns_db_t *new = NULL, *old = NULL;
	isc_result_t result;
	dns_diff_t diff;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	test_create(tc, &old, &new);

	dns_diff_init(mctx, &diff);

	result = dns_db_diffx(&diff, new, NULL, old, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_REQUIRE_EQ(ISC_LIST_EMPTY(diff.tuples), ISC_TRUE);

	dns_diff_clear(&diff);
	dns_db_detach(&new);
	dns_db_detach(&old);
	dns_test_end();
}

ATF_TC(diffx_add);
ATF_TC_HEAD(diffx_add, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "dns_db_diffx of zone with record added");
	atf_tc_set_md_var(tc, "X-old", "testdata/diff/zone1.data");
	atf_tc_set_md_var(tc, "X-new", "testdata/diff/zone2.data");
}
ATF_TC_BODY(diffx_add, tc) {
	dns_db_t *new = NULL, *old = NULL;
	dns_difftuple_t *tuple;
	isc_result_t result;
	dns_diff_t diff;
	int count = 0;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	test_create(tc, &old, &new);

	dns_diff_init(mctx, &diff);

	result = dns_db_diffx(&diff, new, NULL, old, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_REQUIRE_EQ(ISC_LIST_EMPTY(diff.tuples), ISC_FALSE);
	for (tuple = ISC_LIST_HEAD(diff.tuples); tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		ATF_REQUIRE_EQ(tuple->op, DNS_DIFFOP_ADD);
		count++;
	}
	ATF_REQUIRE_EQ(count, 1);

	dns_diff_clear(&diff);
	dns_db_detach(&new);
	dns_db_detach(&old);
	dns_test_end();
}

ATF_TC(diffx_remove);
ATF_TC_HEAD(diffx_remove, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "dns_db_diffx of zone with record removed");
	atf_tc_set_md_var(tc, "X-old", "testdata/diff/zone1.data");
	atf_tc_set_md_var(tc, "X-new", "testdata/diff/zone3.data");
}
ATF_TC_BODY(diffx_remove, tc) {
	dns_db_t *new = NULL, *old = NULL;
	dns_difftuple_t *tuple;
	isc_result_t result;
	dns_diff_t diff;
	int count = 0;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	test_create(tc, &old, &new);

	dns_diff_init(mctx, &diff);

	result = dns_db_diffx(&diff, new, NULL, old, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_REQUIRE_EQ(ISC_LIST_EMPTY(diff.tuples), ISC_FALSE);
	for (tuple = ISC_LIST_HEAD(diff.tuples); tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		ATF_REQUIRE_EQ(tuple->op, DNS_DIFFOP_DEL);
		count++;
	}
	ATF_REQUIRE_EQ(count, 1);

	dns_diff_clear(&diff);
	dns_db_detach(&new);
	dns_db_detach(&old);
	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, diffx_same);
	ATF_TP_ADD_TC(tp, diffx_add);
	ATF_TP_ADD_TC(tp, diffx_remove);
	return (atf_no_error());
}
