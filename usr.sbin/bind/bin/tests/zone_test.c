/*
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: zone_test.c,v 1.26.2.2 2002/08/05 06:57:06 marka Exp $ */

#include <config.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/socket.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/zone.h>

#ifdef ISC_PLATFORM_NEEDSYSSELECTH
#include <sys/select.h>
#endif

static int debug = 0;
static int quiet = 0;
static int stats = 0;
static isc_mem_t *mctx = NULL;
dns_zone_t *zone = NULL;
isc_taskmgr_t *taskmgr = NULL;
isc_timermgr_t *timermgr = NULL;
isc_socketmgr_t *socketmgr = NULL;
dns_zonemgr_t *zonemgr = NULL;
dns_zonetype_t zonetype = dns_zone_master;
isc_sockaddr_t addr;

#define ERRRET(result, function) \
	do { \
		if (result != ISC_R_SUCCESS) { \
			fprintf(stderr, "%s() returned %s\n", \
				function, dns_result_totext(result)); \
			return; \
		} \
	} while (0)

#define ERRCONT(result, function) \
		if (result != ISC_R_SUCCESS) { \
			fprintf(stderr, "%s() returned %s\n", \
				function, dns_result_totext(result)); \
			continue; \
		} else \
			(void)NULL

static void
usage() {
	fprintf(stderr,
		"usage: zone_test [-dqsSM] [-c class] [-f file] zone\n");
	exit(1);
}

static void
setup(const char *zonename, const char *filename, const char *classname) {
	isc_result_t result;
	dns_rdataclass_t rdclass;
	isc_consttextregion_t region;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	const char *rbt = "rbt";

	if (debug)
		fprintf(stderr, "loading \"%s\" from \"%s\" class \"%s\"\n",
			zonename, filename, classname);
	result = dns_zone_create(&zone, mctx);
	ERRRET(result, "dns_zone_new");

	dns_zone_settype(zone, zonetype);

	isc_buffer_init(&buffer, zonename, strlen(zonename));
	isc_buffer_add(&buffer, strlen(zonename));
	dns_fixedname_init(&fixorigin);
	result = dns_name_fromtext(dns_fixedname_name(&fixorigin),
			  	   &buffer, dns_rootname, ISC_FALSE, NULL);
	ERRRET(result, "dns_name_fromtext");
	origin = dns_fixedname_name(&fixorigin);

	result = dns_zone_setorigin(zone, origin);
	ERRRET(result, "dns_zone_setorigin");

	result = dns_zone_setdbtype(zone, 1, &rbt);
	ERRRET(result, "dns_zone_setdatabase");

	result = dns_zone_setfile(zone, filename);
	ERRRET(result, "dns_zone_setfile");

	region.base = classname;
	region.length = strlen(classname);
	result = dns_rdataclass_fromtext(&rdclass,
					 (isc_textregion_t *)&region);
	ERRRET(result, "dns_rdataclass_fromtext");

	dns_zone_setclass(zone, rdclass);

	if (zonetype == dns_zone_slave)
		dns_zone_setmasters(zone, &addr, 1);

	result = dns_zone_load(zone);
	ERRRET(result, "dns_zone_load");

	result = dns_zonemgr_managezone(zonemgr, zone);
	ERRRET(result, "dns_zonemgr_managezone");
}

static void
print_rdataset(dns_name_t *name, dns_rdataset_t *rdataset) {
        isc_buffer_t text;
        char t[1000];
        isc_result_t result;
        isc_region_t r;

        isc_buffer_init(&text, t, sizeof(t));
        result = dns_rdataset_totext(rdataset, name, ISC_FALSE, ISC_FALSE,
				     &text);
        isc_buffer_usedregion(&text, &r);
        if (result == ISC_R_SUCCESS)
                printf("%.*s", (int)r.length, (char *)r.base);
        else
                printf("%s\n", dns_result_totext(result));
}

static void
query(void) {
	char buf[1024];
	dns_fixedname_t name;
	dns_fixedname_t found;
	dns_db_t *db;
	char *s;
	isc_buffer_t buffer;
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdataset_t sigset;
	fd_set rfdset;

	db = NULL;
	result = dns_zone_getdb(zone, &db);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "%s() returned %s\n", "dns_zone_getdb",
			dns_result_totext(result));
		return;
	}

	dns_fixedname_init(&found);
	dns_rdataset_init(&rdataset);
	dns_rdataset_init(&sigset);

	do {

		fprintf(stdout, "zone_test ");
		fflush(stdout);
		FD_ZERO(&rfdset);
		FD_SET(0, &rfdset);
		select(1, &rfdset, NULL, NULL, NULL);
		if (fgets(buf, sizeof buf, stdin) == NULL) {
			fprintf(stdout, "\n");
			break;
		}
		buf[sizeof(buf) - 1] = '\0';

		s = strchr(buf, '\n');
		if (s != NULL)
			*s = '\0';
		s = strchr(buf, '\r');
		if (s != NULL)
			*s = '\0';
		if (strcmp(buf, "dump") == 0) {
			dns_zone_dumptostream(zone, stdout);
			continue;
		}
		if (strlen(buf) == 0)
			continue;
		dns_fixedname_init(&name);
		isc_buffer_init(&buffer, buf, strlen(buf));
		isc_buffer_add(&buffer, strlen(buf));
		result = dns_name_fromtext(dns_fixedname_name(&name),
				  &buffer, dns_rootname, ISC_FALSE, NULL);
		ERRCONT(result, "dns_name_fromtext");

		result = dns_db_find(db, dns_fixedname_name(&name),
				     NULL /*vesion*/,
				     dns_rdatatype_a,
				     0 /*options*/,
				     0 /*time*/,
				     NULL /*nodep*/,
				     dns_fixedname_name(&found),
				     &rdataset, &sigset);
		fprintf(stderr, "%s() returned %s\n", "dns_db_find",
			dns_result_totext(result));
		switch (result) {
		case DNS_R_DELEGATION:
			print_rdataset(dns_fixedname_name(&found), &rdataset);
			break;
		case ISC_R_SUCCESS:
			print_rdataset(dns_fixedname_name(&name), &rdataset);
			break;
		default:
			break;
		}

		if (dns_rdataset_isassociated(&rdataset))
			dns_rdataset_disassociate(&rdataset);
		if (dns_rdataset_isassociated(&sigset))
			dns_rdataset_disassociate(&sigset);
	} while (1);
	dns_rdataset_invalidate(&rdataset);
	dns_db_detach(&db);
}

int
main(int argc, char **argv) {
	int c;
	char *filename = NULL;
	const char *classname = "IN";

	while ((c = isc_commandline_parse(argc, argv, "cdf:m:qsMS")) != EOF) {
		switch (c) {
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			debug++;
			break;
		case 'f':
			if (filename != NULL)
				usage();
			filename = isc_commandline_argument;
			break;
		case 'm':
			memset(&addr, 0, sizeof addr);
			addr.type.sin.sin_family = AF_INET;
			inet_pton(AF_INET, isc_commandline_argument,
				  &addr.type.sin.sin_addr);
			addr.type.sin.sin_port = htons(53);
			break;
		case 'q':
			quiet++;
			break;
		case 's':
			stats++;
			break;
		case 'S':
			zonetype = dns_zone_slave;
			break;
		case 'M':
			zonetype = dns_zone_master;
			break;
		default:
			usage();
		}
	}

	if (argv[isc_commandline_index] == NULL)
		usage();

	RUNTIME_CHECK(isc_app_start() == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_taskmgr_create(mctx, 2, 0, &taskmgr) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timermgr) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_socketmgr_create(mctx, &socketmgr) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_zonemgr_create(mctx, taskmgr, timermgr, socketmgr,
					 &zonemgr) == ISC_R_SUCCESS);
	if (filename == NULL)
		filename = argv[isc_commandline_index];
	setup(argv[isc_commandline_index], filename, classname);
	query();
	if (zone != NULL)
		dns_zone_detach(&zone);
	dns_zonemgr_shutdown(zonemgr);
	dns_zonemgr_detach(&zonemgr);
	isc_socketmgr_destroy(&socketmgr);
	isc_taskmgr_destroy(&taskmgr);
	isc_timermgr_destroy(&timermgr);
	if (!quiet && stats)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
