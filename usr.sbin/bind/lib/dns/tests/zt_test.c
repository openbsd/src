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

/* $Id: zt_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/task.h>
#include <isc/timer.h>

#include <dns/db.h>
#include <dns/name.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include "dnstest.h"

struct args {
	void *arg1;
	void *arg2;
};

/*
 * Helper functions
 */
static isc_result_t
count_zone(dns_zone_t *zone, void *uap) {
	int *nzones = (int *)uap;

	UNUSED(zone);

	*nzones += 1;
	return (ISC_R_SUCCESS);
}

static isc_result_t
load_done(dns_zt_t *zt, dns_zone_t *zone, isc_task_t *task) {
	/* We treat zt as a pointer to a boolean for testing purposes */
	isc_boolean_t *done = (isc_boolean_t *) zt;

	UNUSED(zone);
	UNUSED(task);

	*done = ISC_TRUE;
	isc_app_shutdown();
	return (ISC_R_SUCCESS);
}

static isc_result_t
all_done(void *arg) {
	isc_boolean_t *done = (isc_boolean_t *) arg;

	*done = ISC_TRUE;
	isc_app_shutdown();
	return (ISC_R_SUCCESS);
}

static void
start_zt_asyncload(isc_task_t *task, isc_event_t *event) {
	struct args *args = (struct args *)(event->ev_arg);

	UNUSED(task);

	dns_zt_asyncload(args->arg1, all_done, args->arg2);

	isc_event_free(&event);
}

static void
start_zone_asyncload(isc_task_t *task, isc_event_t *event) {
	struct args *args = (struct args *)(event->ev_arg);

	UNUSED(task);

	dns_zone_asyncload(args->arg1, load_done, args->arg2);
	isc_event_free(&event);
}

/*
 * Individual unit tests
 */
ATF_TC(apply);
ATF_TC_HEAD(apply, tc) {
	atf_tc_set_md_var(tc, "descr", "apply a function to a zone table");
}
ATF_TC_BODY(apply, tc) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_view_t *view = NULL;
	int nzones = 0;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_makezone("foo", &zone, NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	view = dns_zone_getview(zone);
	ATF_REQUIRE(view->zonetable != NULL);

	ATF_CHECK_EQ(0, nzones);
	result = dns_zt_apply(view->zonetable, ISC_FALSE, count_zone, &nzones);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(1, nzones);

	/* These steps are necessary so the zone can be detached properly */
	result = dns_test_setupzonemgr();
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = dns_test_managezone(zone);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_test_releasezone(zone);
	dns_test_closezonemgr();

	/* The view was left attached in dns_test_makezone() */
	dns_view_detach(&view);
	dns_zone_detach(&zone);

	dns_test_end();
}

ATF_TC(asyncload_zone);
ATF_TC_HEAD(asyncload_zone, tc) {
	atf_tc_set_md_var(tc, "descr", "asynchronous zone load");
}
ATF_TC_BODY(asyncload_zone, tc) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_view_t *view = NULL;
	dns_db_t *db = NULL;
	isc_boolean_t done = ISC_FALSE;
	int i = 0;
	struct args args;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_makezone("foo", &zone, NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_setupzonemgr();
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = dns_test_managezone(zone);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	view = dns_zone_getview(zone);
	ATF_REQUIRE(view->zonetable != NULL);

	ATF_CHECK(!dns__zone_loadpending(zone));
	ATF_CHECK(!done);
	dns_zone_setfile(zone, "testdata/zt/zone1.db");

	args.arg1 = zone;
	args.arg2 = &done;
	isc_app_onrun(mctx, maintask, start_zone_asyncload, &args);

	isc_app_run();
	while (dns__zone_loadpending(zone) && i++ < 5000)
		dns_test_nap(1000);
	ATF_CHECK(done);

	/* The zone should now be loaded; test it */
	result = dns_zone_getdb(zone, &db);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK(db != NULL);
	if (db != NULL)
		dns_db_detach(&db);

	dns_test_releasezone(zone);
	dns_test_closezonemgr();

	dns_zone_detach(&zone);
	dns_view_detach(&view);

	dns_test_end();
}

ATF_TC(asyncload_zt);
ATF_TC_HEAD(asyncload_zt, tc) {
	atf_tc_set_md_var(tc, "descr", "asynchronous zone table load");
}
ATF_TC_BODY(asyncload_zt, tc) {
	isc_result_t result;
	dns_zone_t *zone1 = NULL, *zone2 = NULL, *zone3 = NULL;
	dns_view_t *view;
	dns_zt_t *zt;
	dns_db_t *db = NULL;
	isc_boolean_t done = ISC_FALSE;
	int i = 0;
	struct args args;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_makezone("foo", &zone1, NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_zone_setfile(zone1, "testdata/zt/zone1.db");
	view = dns_zone_getview(zone1);

	result = dns_test_makezone("bar", &zone2, view, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_zone_setfile(zone2, "testdata/zt/zone1.db");

	/* This one will fail to load */
	result = dns_test_makezone("fake", &zone3, view, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_zone_setfile(zone3, "testdata/zt/nonexistent.db");

	zt = view->zonetable;
	ATF_REQUIRE(zt != NULL);

	result = dns_test_setupzonemgr();
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = dns_test_managezone(zone1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = dns_test_managezone(zone2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = dns_test_managezone(zone3);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_CHECK(!dns__zone_loadpending(zone1));
	ATF_CHECK(!dns__zone_loadpending(zone2));
	ATF_CHECK(!done);

	args.arg1 = zt;
	args.arg2 = &done;
	isc_app_onrun(mctx, maintask, start_zt_asyncload, &args);

	isc_app_run();
	while (!done && i++ < 5000)
		dns_test_nap(1000);
	ATF_CHECK(done);

	/* Both zones should now be loaded; test them */
	result = dns_zone_getdb(zone1, &db);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK(db != NULL);
	if (db != NULL)
		dns_db_detach(&db);

	result = dns_zone_getdb(zone2, &db);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK(db != NULL);
	if (db != NULL)
		dns_db_detach(&db);

	dns_test_releasezone(zone3);
	dns_test_releasezone(zone2);
	dns_test_releasezone(zone1);
	dns_test_closezonemgr();

	dns_zone_detach(&zone1);
	dns_zone_detach(&zone2);
	dns_zone_detach(&zone3);
	dns_view_detach(&view);

	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, apply);
	ATF_TP_ADD_TC(tp, asyncload_zone);
	ATF_TP_ADD_TC(tp, asyncload_zt);
	return (atf_no_error());
}
