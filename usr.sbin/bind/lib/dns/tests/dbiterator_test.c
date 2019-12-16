/*
 * Copyright (C) 2011, 2012, 2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dbiterator_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>
#include <stdlib.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/name.h>

#include "dnstest.h"

/*
 * Helper functions
 */

#define	BUFLEN		255
#define	BIGBUFLEN	(64 * 1024)
#define TEST_ORIGIN	"test"

static isc_result_t
make_name(const char *src, dns_name_t *name) {
	isc_buffer_t b;
	isc_buffer_constinit(&b, src, strlen(src));
	isc_buffer_add(&b, strlen(src));
	return (dns_name_fromtext(name, &b, dns_rootname, 0, NULL));
}

/*
 * Individual unit tests
 */

/* create: make sure we can create a dbiterator */
static void
test_create(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(create);
ATF_TC_HEAD(create, tc) {
	atf_tc_set_md_var(tc, "descr", "create a database iterator");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
}
ATF_TC_BODY(create, tc) {
	test_create(tc);
}

ATF_TC(create_nsec3);
ATF_TC_HEAD(create_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "create a database iterator (NSEC3)");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
}
ATF_TC_BODY(create_nsec3, tc) {
	test_create(tc);
}

/* walk: walk a database */
static void
test_walk(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;
	dns_dbnode_t *node = NULL;
	dns_name_t *name;
	dns_fixedname_t f;
	int i = 0;

	UNUSED(tc);

	dns_fixedname_init(&f);
	name = dns_fixedname_name(&f);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (result = dns_dbiterator_first(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(iter)) {
		result = dns_dbiterator_current(iter, &node, name);
		if (result == DNS_R_NEWORIGIN)
			result = ISC_R_SUCCESS;
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		dns_db_detachnode(db, &node);
		i++;
	}

	ATF_CHECK_EQ(i, atoi(atf_tc_get_md_var(tc, "X-nodes")));

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(walk);
ATF_TC_HEAD(walk, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
	atf_tc_set_md_var(tc, "X-nodes", "12");
}
ATF_TC_BODY(walk, tc) {
	test_walk(tc);
}

ATF_TC(walk_nsec3);
ATF_TC_HEAD(walk_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
	atf_tc_set_md_var(tc, "X-nodes", "33");
}
ATF_TC_BODY(walk_nsec3, tc) {
	test_walk(tc);
}

/* reverse: walk database backwards */
static void test_reverse(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;
	dns_dbnode_t *node = NULL;
	dns_name_t *name;
	dns_fixedname_t f;
	int i = 0;

	UNUSED(tc);

	dns_fixedname_init(&f);
	name = dns_fixedname_name(&f);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (result = dns_dbiterator_last(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_prev(iter)) {
		result = dns_dbiterator_current(iter, &node, name);
		if (result == DNS_R_NEWORIGIN)
			result = ISC_R_SUCCESS;
		ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		dns_db_detachnode(db, &node);
		i++;
	}

	ATF_CHECK_EQ(i, 12);

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(reverse);
ATF_TC_HEAD(reverse, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database backwards");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
}
ATF_TC_BODY(reverse, tc) {
	test_reverse(tc);
}

ATF_TC(reverse_nsec3);
ATF_TC_HEAD(reverse_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database backwards");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
}
ATF_TC_BODY(reverse_nsec3, tc) {
	test_reverse(tc);
}

/* seek: walk database starting at a particular node */
static void test_seek(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;
	dns_dbnode_t *node = NULL;
	dns_name_t *name, *seekname;
	dns_fixedname_t f1, f2;
	int i = 0;

	UNUSED(tc);

	dns_fixedname_init(&f1);
	name = dns_fixedname_name(&f1);
	dns_fixedname_init(&f2);
	seekname = dns_fixedname_name(&f2);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = make_name("c." TEST_ORIGIN, seekname);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_dbiterator_seek(iter, seekname);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	while (result == ISC_R_SUCCESS) {
		result = dns_dbiterator_current(iter, &node, name);
		if (result == DNS_R_NEWORIGIN)
			result = ISC_R_SUCCESS;
		ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		dns_db_detachnode(db, &node);
		result = dns_dbiterator_next(iter);
		i++;
	}

	ATF_CHECK_EQ(i, atoi(atf_tc_get_md_var(tc, "X-nodes")));

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(seek);
ATF_TC_HEAD(seek, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database starting at "
				       "a particular node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
	atf_tc_set_md_var(tc, "X-nodes", "9");
}
ATF_TC_BODY(seek, tc) {
	test_seek(tc);
}

ATF_TC(seek_nsec3);
ATF_TC_HEAD(seek_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database starting at "
				       "a particular node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
	atf_tc_set_md_var(tc, "X-nodes", "30");
}
ATF_TC_BODY(seek_nsec3, tc) {
	test_seek(tc);
}

/*
 * seek_emty: walk database starting at an empty nonterminal node
 * (should fail)
 */
static void test_seek_empty(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;
	dns_name_t *seekname;
	dns_fixedname_t f1;

	UNUSED(tc);

	dns_fixedname_init(&f1);
	seekname = dns_fixedname_name(&f1);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = make_name("d." TEST_ORIGIN, seekname);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_dbiterator_seek(iter, seekname);
	ATF_CHECK_EQ(result, DNS_R_PARTIALMATCH);

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(seek_empty);
ATF_TC_HEAD(seek_empty, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database starting at an "
				       "empty nonterminal node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
}
ATF_TC_BODY(seek_empty, tc) {
	test_seek_empty(tc);
}

ATF_TC(seek_empty_nsec3);
ATF_TC_HEAD(seek_empty_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "walk database starting at an "
				       "empty nonterminal node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
}
ATF_TC_BODY(seek_empty_nsec3, tc) {
	test_seek_empty(tc);
}

/*
 * seek_emty: walk database starting at an empty nonterminal node
 * (should fail)
 */
static void test_seek_nx(const atf_tc_t *tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbiterator_t *iter = NULL;
	dns_name_t *seekname;
	dns_fixedname_t f1;

	UNUSED(tc);

	dns_fixedname_init(&f1);
	seekname = dns_fixedname_name(&f1);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_cache, TEST_ORIGIN,
				 atf_tc_get_md_var(tc, "X-filename"));
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_createiterator(db, 0, &iter);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = make_name("nonexistent." TEST_ORIGIN, seekname);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_dbiterator_seek(iter, seekname);
	ATF_CHECK_EQ(result, DNS_R_PARTIALMATCH);

	result = make_name("nonexistent.", seekname);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_dbiterator_seek(iter, seekname);
	ATF_CHECK_EQ(result, ISC_R_NOTFOUND);

	dns_dbiterator_destroy(&iter);
	dns_db_detach(&db);
	dns_test_end();
}

ATF_TC(seek_nx);
ATF_TC_HEAD(seek_nx, tc) {
	atf_tc_set_md_var(tc, "descr", "attempt to walk database starting "
				       "at a nonexistent node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone1.data");
}
ATF_TC_BODY(seek_nx, tc) {
	test_seek_nx(tc);
}

ATF_TC(seek_nx_nsec3);
ATF_TC_HEAD(seek_nx_nsec3, tc) {
	atf_tc_set_md_var(tc, "descr", "attempt to walk database starting "
				       "at a nonexistent node");
	atf_tc_set_md_var(tc, "X-filename", "testdata/dbiterator/zone2.data");
}
ATF_TC_BODY(seek_nx_nsec3, tc) {
	test_seek_nx(tc);
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, create);
	ATF_TP_ADD_TC(tp, create_nsec3);
	ATF_TP_ADD_TC(tp, walk);
	ATF_TP_ADD_TC(tp, walk_nsec3);
	ATF_TP_ADD_TC(tp, reverse);
	ATF_TP_ADD_TC(tp, reverse_nsec3);
	ATF_TP_ADD_TC(tp, seek);
	ATF_TP_ADD_TC(tp, seek_nsec3);
	ATF_TP_ADD_TC(tp, seek_empty);
	ATF_TP_ADD_TC(tp, seek_empty_nsec3);
	ATF_TP_ADD_TC(tp, seek_nx);
	ATF_TP_ADD_TC(tp, seek_nx_nsec3);
	return (atf_no_error());
}

/*
 * XXX:
 * dns_dbiterator API calls that are not yet part of this unit test:
 *
 * dns_dbiterator_pause
 * dns_dbiterator_origin
 * dns_dbiterator_setcleanmode
 */
