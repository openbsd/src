/*
 * Copyright (C) 2011-2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnstest.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <time.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/os.h>
#include <isc/string.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/result.h>
#include <dns/view.h>
#include <dns/zone.h>

#include <dst/dst.h>

#include "dnstest.h"

isc_mem_t *mctx = NULL;
isc_entropy_t *ectx = NULL;
isc_log_t *lctx = NULL;
isc_taskmgr_t *taskmgr = NULL;
isc_task_t *maintask = NULL;
isc_timermgr_t *timermgr = NULL;
isc_socketmgr_t *socketmgr = NULL;
dns_zonemgr_t *zonemgr = NULL;
isc_boolean_t app_running = ISC_FALSE;
int ncpus;
isc_boolean_t debug_mem_record = ISC_TRUE;

static isc_boolean_t hash_active = ISC_FALSE, dst_active = ISC_FALSE;

/*
 * Logging categories: this needs to match the list in bin/named/log.c.
 */
static isc_logcategory_t categories[] = {
		{ "",                0 },
		{ "client",          0 },
		{ "network",         0 },
		{ "update",          0 },
		{ "queries",         0 },
		{ "unmatched",       0 },
		{ "update-security", 0 },
		{ "query-errors",    0 },
		{ NULL,              0 }
};

static void
cleanup_managers(void) {
	if (app_running)
		isc_app_finish();
	if (socketmgr != NULL)
		isc_socketmgr_destroy(&socketmgr);
	if (maintask != NULL)
		isc_task_destroy(&maintask);
	if (taskmgr != NULL)
		isc_taskmgr_destroy(&taskmgr);
	if (timermgr != NULL)
		isc_timermgr_destroy(&timermgr);
}

static isc_result_t
create_managers(void) {
	isc_result_t result;
#ifdef ISC_PLATFORM_USETHREADS
	ncpus = isc_os_ncpus();
#else
	ncpus = 1;
#endif

	CHECK(isc_taskmgr_create(mctx, ncpus, 0, &taskmgr));
	CHECK(isc_timermgr_create(mctx, &timermgr));
	CHECK(isc_socketmgr_create(mctx, &socketmgr));
	CHECK(isc_task_create(taskmgr, 0, &maintask));
	return (ISC_R_SUCCESS);

  cleanup:
	cleanup_managers();
	return (result);
}

isc_result_t
dns_test_begin(FILE *logfile, isc_boolean_t start_managers) {
	isc_result_t result;

	if (start_managers)
		CHECK(isc_app_start());
	if (debug_mem_record)
		isc_mem_debugging |= ISC_MEM_DEBUGRECORD;
	CHECK(isc_mem_create(0, 0, &mctx));
	CHECK(isc_entropy_create(mctx, &ectx));

	CHECK(isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE));
	hash_active = ISC_TRUE;

	CHECK(dst_lib_init(mctx, ectx, ISC_ENTROPY_BLOCKING));
	dst_active = ISC_TRUE;

	if (logfile != NULL) {
		isc_logdestination_t destination;
		isc_logconfig_t *logconfig = NULL;

		CHECK(isc_log_create(mctx, &lctx, &logconfig));
		isc_log_registercategories(lctx, categories);
		isc_log_setcontext(lctx);
		dns_log_init(lctx);
		dns_log_setcontext(lctx);

		destination.file.stream = logfile;
		destination.file.name = NULL;
		destination.file.versions = ISC_LOG_ROLLNEVER;
		destination.file.maximum_size = 0;
		CHECK(isc_log_createchannel(logconfig, "stderr",
					    ISC_LOG_TOFILEDESC,
					    ISC_LOG_DYNAMIC,
					    &destination, 0));
		CHECK(isc_log_usechannel(logconfig, "stderr", NULL, NULL));
	}

	dns_result_register();

	if (start_managers)
		CHECK(create_managers());

	/*
	 * atf-run changes us to a /tmp directory, so tests
	 * that access test data files must first chdir to the proper
	 * location.
	 */
	if (chdir(TESTS) == -1)
		CHECK(ISC_R_FAILURE);

	return (ISC_R_SUCCESS);

  cleanup:
	dns_test_end();
	return (result);
}

void
dns_test_end(void) {
	if (lctx != NULL)
		isc_log_destroy(&lctx);
	if (dst_active) {
		dst_lib_destroy();
		dst_active = ISC_FALSE;
	}
	if (hash_active) {
		isc_hash_destroy();
		hash_active = ISC_FALSE;
	}
	if (ectx != NULL)
		isc_entropy_detach(&ectx);

	cleanup_managers();

	if (mctx != NULL)
		isc_mem_destroy(&mctx);
}

/*
 * Create a zone with origin 'name', return a pointer to the zone object in
 * 'zonep'.  If 'view' is set, add the zone to that view; otherwise, create
 * a new view for the purpose.
 *
 * If the created view is going to be needed by the caller subsequently,
 * then 'keepview' should be set to true; this will prevent the view
 * from being detached.  In this case, the caller is responsible for
 * detaching the view.
 */
isc_result_t
dns_test_makezone(const char *name, dns_zone_t **zonep, dns_view_t *view,
		  isc_boolean_t keepview)
{
	isc_result_t result;
	dns_zone_t *zone = NULL;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;

	if (view == NULL)
		CHECK(dns_view_create(mctx, dns_rdataclass_in, "view", &view));
	else if (!keepview)
		keepview = ISC_TRUE;

	zone = *zonep;
	if (zone == NULL)
		CHECK(dns_zone_create(&zone, mctx));

	isc_buffer_constinit(&buffer, name, strlen(name));
	isc_buffer_add(&buffer, strlen(name));
	dns_fixedname_init(&fixorigin);
	origin = dns_fixedname_name(&fixorigin);
	CHECK(dns_name_fromtext(origin, &buffer, dns_rootname, 0, NULL));
	CHECK(dns_zone_setorigin(zone, origin));
	dns_zone_setview(zone, view);
	dns_zone_settype(zone, dns_zone_master);
	dns_zone_setclass(zone, view->rdclass);
	dns_view_addzone(view, zone);

	if (!keepview)
		dns_view_detach(&view);

	*zonep = zone;

	return (ISC_R_SUCCESS);

  cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (view != NULL)
		dns_view_detach(&view);
	return (result);
}

isc_result_t
dns_test_setupzonemgr(void) {
	isc_result_t result;
	REQUIRE(zonemgr == NULL);

	result = dns_zonemgr_create(mctx, taskmgr, timermgr, socketmgr,
				    &zonemgr);
	return (result);
}

isc_result_t
dns_test_managezone(dns_zone_t *zone) {
	isc_result_t result;
	REQUIRE(zonemgr != NULL);

	result = dns_zonemgr_setsize(zonemgr, 1);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_zonemgr_managezone(zonemgr, zone);
	return (result);
}

void
dns_test_releasezone(dns_zone_t *zone) {
	REQUIRE(zonemgr != NULL);
	dns_zonemgr_releasezone(zonemgr, zone);
}

void
dns_test_closezonemgr(void) {
	REQUIRE(zonemgr != NULL);

	dns_zonemgr_shutdown(zonemgr);
	dns_zonemgr_detach(&zonemgr);
}

/*
 * Sleep for 'usec' microseconds.
 */
void
dns_test_nap(isc_uint32_t usec) {
#ifdef HAVE_NANOSLEEP
	struct timespec ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;
	nanosleep(&ts, NULL);
#elif HAVE_USLEEP
	usleep(usec);
#else
	/*
	 * No fractional-second sleep function is available, so we
	 * round up to the nearest second and sleep instead
	 */
	sleep((usec / 1000000) + 1);
#endif
}

isc_result_t
dns_test_loaddb(dns_db_t **db, dns_dbtype_t dbtype, const char *origin,
		const char *testfile)
{
	isc_result_t		result;
	dns_fixedname_t		fixed;
	dns_name_t		*name;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	result = dns_name_fromstring(name, origin, 0, NULL);
	if (result != ISC_R_SUCCESS)
		return(result);

	result = dns_db_create(mctx, "rbt", name, dbtype, dns_rdataclass_in,
			       0, NULL, db);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_load(*db, testfile);
	return (result);
}
