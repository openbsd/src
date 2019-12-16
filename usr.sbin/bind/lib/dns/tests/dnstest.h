/*
 * Copyright (C) 2011, 2012, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnstest.h,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/result.h>
#include <dns/zone.h>

#define CHECK(r) \
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (0)

extern isc_mem_t *mctx;
extern isc_entropy_t *ectx;
extern isc_log_t *lctx;
extern isc_taskmgr_t *taskmgr;
extern isc_task_t *maintask;
extern isc_timermgr_t *timermgr;
extern isc_socketmgr_t *socketmgr;
extern dns_zonemgr_t *zonemgr;
extern isc_boolean_t app_running;
extern int ncpus;
extern isc_boolean_t debug_mem_record;

isc_result_t
dns_test_begin(FILE *logfile, isc_boolean_t create_managers);

void
dns_test_end(void);

isc_result_t
dns_test_makezone(const char *name, dns_zone_t **zonep, dns_view_t *view,
				  isc_boolean_t keepview);

isc_result_t
dns_test_setupzonemgr(void);

isc_result_t
dns_test_managezone(dns_zone_t *zone);

void
dns_test_releasezone(dns_zone_t *zone);

void
dns_test_closezonemgr(void);

void
dns_test_nap(isc_uint32_t usec);

isc_result_t
dns_test_loaddb(dns_db_t **db, dns_dbtype_t dbtype, const char *origin,
		const char *testfile);
