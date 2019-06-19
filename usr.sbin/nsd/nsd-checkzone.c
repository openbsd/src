/*
 * nsd-checkzone.c -- nsd-checkzone(8) checks zones for syntax errors
 *
 * Copyright (c) 2013, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "nsd.h"
#include "options.h"
#include "util.h"
#include "zonec.h"

struct nsd nsd;

/*
 * Print the help text.
 *
 */
static void
usage (void)
{
	fprintf(stderr, "Usage: nsd-checkzone <zone name> <zone file>\n");
	fprintf(stderr, "Version %s. Report bugs to <%s>.\n",
		PACKAGE_VERSION, PACKAGE_BUGREPORT);
}

static void
check_zone(struct nsd* nsd, const char* name, const char* fname)
{
	const dname_type* dname;
	zone_options_type* zo;
	zone_type* zone;
	unsigned errors;

	/* init*/
	nsd->db = namedb_open("", nsd->options);
	dname = dname_parse(nsd->options->region, name);
	if(!dname) {
		/* parse failure */
		error("cannot parse zone name '%s'", name);
	}
	zo = zone_options_create(nsd->options->region);
	memset(zo, 0, sizeof(*zo));
	zo->node.key = dname;
	zo->name = name;
	zone = namedb_zone_create(nsd->db, dname, zo);

	/* read the zone */
	errors = zonec_read(name, fname, zone);
	if(errors > 0) {
		printf("zone %s file %s has %u errors\n", name, fname, errors);
		exit(1);
	}
	printf("zone %s is ok\n", name);
	namedb_close(nsd->db);
}

/* dummy functions to link */
int writepid(struct nsd * ATTR_UNUSED(nsd))
{
	        return 0;
}
void unlinkpid(const char * ATTR_UNUSED(file))
{
}
void bind8_stats(struct nsd * ATTR_UNUSED(nsd))
{
}

void sig_handler(int ATTR_UNUSED(sig))
{
}

extern char *optarg;
extern int optind;

int
main(int argc, char *argv[])
{
	/* Scratch variables... */
	int c;
	struct nsd nsd;
	memset(&nsd, 0, sizeof(nsd));

	log_init("nsd-checkzone");

	/* Parse the command line... */
	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(0);
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* Commandline parse error */
	if (argc != 2) {
		fprintf(stderr, "wrong number of arguments.\n");
		usage();
		exit(1);
	}

	nsd.options = nsd_options_create(region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1));
	if (verbosity == 0)
		verbosity = nsd.options->verbosity;

	check_zone(&nsd, argv[0], argv[1]);
	region_destroy(nsd.options->region);
	/* yylex_destroy(); but, not available in all versions of flex */

	exit(0);
}
