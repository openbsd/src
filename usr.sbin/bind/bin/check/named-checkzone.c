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

/* $ISC: named-checkzone.c,v 1.13.2.3 2002/07/11 05:44:10 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/socket.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/zone.h>

#include "check-tool.h"

static int debug = 0;
isc_boolean_t nomerge = ISC_TRUE;
static int quiet = 0;
static isc_mem_t *mctx = NULL;
dns_zone_t *zone = NULL;
dns_zonetype_t zonetype = dns_zone_master;
static const char *dbtype[] = { "rbt" };

#define ERRRET(result, function) \
	do { \
		if (result != ISC_R_SUCCESS) { \
			if (!quiet) \
				fprintf(stderr, "%s() returned %s\n", \
					function, dns_result_totext(result)); \
			return (result); \
		} \
	} while (0)

static void
usage(void) {
	fprintf(stderr,
		"usage: named-checkzone [-djqv] [-c class] zonename filename \n");
	exit(1);
}

static isc_result_t
setup(char *zonename, char *filename, char *classname) {
	isc_result_t result;
	dns_rdataclass_t rdclass;
	isc_textregion_t region;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;

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

	result = dns_zone_setdbtype(zone, 1, (const char * const *) dbtype);
	ERRRET(result, "dns_zone_setdatabase");

	result = dns_zone_setfile(zone, filename);
	ERRRET(result, "dns_zone_setdatabase");

	region.base = classname;
	region.length = strlen(classname);
	result = dns_rdataclass_fromtext(&rdclass, &region);
	ERRRET(result, "dns_rdataclass_fromtext");

	dns_zone_setclass(zone, rdclass);
	dns_zone_setoption(zone, DNS_ZONEOPT_MANYERRORS, ISC_TRUE);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOMERGE, nomerge);

	result = dns_zone_load(zone);

	return (result);
}

static void
destroy(void) {
	if (zone != NULL)
		dns_zone_detach(&zone);
}

int
main(int argc, char **argv) {
	int c;
	char *origin = NULL;
	char *filename = NULL;
	isc_log_t *lctx = NULL;
	isc_result_t result;
	char classname_in[] = "IN";
	char *classname = classname_in;

	while ((c = isc_commandline_parse(argc, argv, "c:djqsv")) != EOF) {
		switch (c) {
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			debug++;
			break;

		case 'j':
			nomerge = ISC_FALSE;
			break;
		case 'q':
			quiet++;
			break;
		case 'v':
			printf(VERSION "\n");
			exit(0);
		default:
			usage();
		}
	}

	if (isc_commandline_index + 2 > argc)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	if (!quiet) {
		RUNTIME_CHECK(setup_logging(mctx, &lctx) == ISC_R_SUCCESS);
		dns_log_init(lctx);
		dns_log_setcontext(lctx);
	}

	dns_result_register();

	origin = argv[isc_commandline_index++];
	filename = argv[isc_commandline_index++];
	result = setup(origin, filename, classname);
	if (!quiet && result == ISC_R_SUCCESS)
		fprintf(stdout, "OK\n");
	destroy();
	if (lctx != NULL)
		isc_log_destroy(&lctx);
	isc_mem_destroy(&mctx);
	return ((result == ISC_R_SUCCESS) ? 0 : 1);
}
