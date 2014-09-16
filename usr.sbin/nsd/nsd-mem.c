/*
 * nsd-mem.c -- nsd-mem(8)
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
#include "tsig.h"
#include "options.h"
#include "namedb.h"
#include "udb.h"
#include "udbzone.h"
#include "util.h"

static void error(const char *format, ...) ATTR_FORMAT(printf, 1, 2);
struct nsd nsd;

/*
 * Print the help text.
 *
 */
static void
usage (void)
{
	fprintf(stderr, "Usage: nsd-mem [-c configfile]\n");
	fprintf(stderr, "Version %s. Report bugs to <%s>.\n",
		PACKAGE_VERSION, PACKAGE_BUGREPORT);
}

/*
 * Something went wrong, give error messages and exit.
 *
 */
static void
error(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_vmsg(LOG_ERR, format, args);
	va_end(args);
	exit(1);
}

/* zone memory structure */
struct zone_mem {
	/* size of data (allocated in db.region) */
	size_t data;
	/* unused space (in db.region) due to alignment */
	size_t data_unused;
	/* udb data allocated */
	size_t udb_data;
	/* udb overhead (chunk2**x - data) */
	size_t udb_overhead;

	/* count of number of domains */
	size_t domaincount;
};

/* total memory structure */
struct tot_mem {
	/* size of data (allocated in db.region) */
	size_t data;
	/* unused space (in db.region) due to alignment */
	size_t data_unused;
	/* udb data allocated */
	size_t udb_data;
	/* udb overhead (chunk2**x - data) */
	size_t udb_overhead;

	/* count of number of domains */
	size_t domaincount;

	/* options data */
	size_t opt_data;
	/* unused in options region */
	size_t opt_unused;
	/* dname compression table */
	size_t compresstable;
#ifdef RATELIMIT
	/* size of rrl tables */
	size_t rrl;
#endif

	/* total ram usage */
	size_t ram;
	/* total nsd.db disk usage */
	size_t disk;
};

static void
account_zone(struct namedb* db, struct zone_mem* zmem)
{
	zmem->data = region_get_mem(db->region);
	zmem->data_unused = region_get_mem_unused(db->region);
	if(db->udb) {
		zmem->udb_data = (size_t)db->udb->alloc->disk->stat_data;
		zmem->udb_overhead = (size_t)(db->udb->alloc->disk->stat_alloc -
			db->udb->alloc->disk->stat_data);
	}
	zmem->domaincount = db->domains->nametree->count;
}

static void
pretty_mem(size_t x, const char* s)
{
	char buf[32];
	memset(buf, 0, sizeof(buf));
	if(snprintf(buf, sizeof(buf), "%12lld", (long long)x) > 12) {
		printf("%12lld %s\n", (long long)x, s);
		return;
	}
	printf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c %s\n",
		buf[0], buf[1], buf[2], (buf[2]==' '?' ':'.'),
		buf[3], buf[4], buf[5], (buf[5]==' '?' ':'.'),
		buf[6], buf[7], buf[8], (buf[8]==' '?' ':'.'),
		buf[9], buf[10], buf[11], s);
}

static void
print_zone_mem(struct zone_mem* z)
{
	pretty_mem(z->data, "zone data");
	pretty_mem(z->data_unused, "zone unused space (due to alignment)");
	pretty_mem(z->udb_data, "data in nsd.db");
	pretty_mem(z->udb_overhead, "overhead in nsd.db");
}

static void
account_total(nsd_options_t* opt, struct tot_mem* t)
{
	t->opt_data = region_get_mem(opt->region);
	t->opt_unused = region_get_mem_unused(opt->region);
	t->compresstable = sizeof(uint16_t) *
		(t->domaincount + 1 + EXTRA_DOMAIN_NUMBERS);
	t->compresstable *= opt->server_count;

#ifdef RATELIMIT
#define SIZE_RRL_BUCKET (8 + 4 + 4 + 4 + 4 + 2)
	t->rrl = opt->rrl_size * SIZE_RRL_BUCKET;
	t->rrl *= opt->server_count;
#endif

	t->ram = t->data + t->data_unused + t->opt_data + t->opt_unused +
		t->compresstable;
#ifdef RATELIMIT
	t->ram += t->rrl;
#endif
	t->disk = t->udb_data + t->udb_overhead;
}

static void
print_tot_mem(struct tot_mem* t)
{
	printf("\ntotal\n");
	pretty_mem(t->data, "data");
	pretty_mem(t->data_unused, "unused space (due to alignment)");
	pretty_mem(t->opt_data, "options");
	pretty_mem(t->opt_unused, "options unused space (due to alignment)");
	pretty_mem(t->compresstable, "name table (depends on servercount)");
#ifdef RATELIMIT
	pretty_mem(t->rrl, "RRL table (depends on servercount)");
#endif
	pretty_mem(t->udb_data, "data in nsd.db");
	pretty_mem(t->udb_overhead, "overhead in nsd.db");
	printf("\nsummary\n");

	pretty_mem(t->ram, "ram usage (excl space for buffers)");
	pretty_mem(t->disk, "disk usage (excl 12% space claimed for growth)");
}

static void
add_mem(struct tot_mem* t, struct zone_mem* z)
{
	t->data += z->data;
	t->data_unused += z->data_unused;
	t->udb_data += z->udb_data;
	t->udb_overhead += z->udb_overhead;
	t->domaincount += z->domaincount;
}

static void
check_zone_mem(const char* tf, const char* df, zone_options_t* zo,
	nsd_options_t* opt, struct tot_mem* totmem)
{
	struct nsd nsd;
	struct namedb* db;
	const dname_type* dname = (const dname_type*)zo->node.key;
	zone_type* zone;
	struct udb_base* taskudb;
	udb_ptr last_task;
	struct zone_mem zmem;

	printf("zone %s\n", zo->name);

	/* init*/
	memset(&zmem, 0, sizeof(zmem));
	memset(&nsd, 0, sizeof(nsd));
	nsd.db = db = namedb_open(df, opt);
	if(!db) error("cannot open %s: %s", df, strerror(errno));
	zone = namedb_zone_create(db, dname, zo);
	taskudb = udb_base_create_new(tf, &namedb_walkfunc, NULL);
	udb_ptr_init(&last_task, taskudb);

	/* read the zone */
	namedb_read_zonefile(&nsd, zone, taskudb, &last_task);

	/* account the memory for this zone */
	account_zone(db, &zmem);

	/* pretty print the memory for this zone */
	print_zone_mem(&zmem);

	/* delete the zone from memory */
	namedb_close(db);
	udb_base_free(taskudb);
	unlink(df);
	unlink(tf);

	/* add up totals */
	add_mem(totmem, &zmem);
}

static void
check_mem(nsd_options_t* opt)
{
	struct tot_mem totmem;
	zone_options_t* zo;
	char tf[512];
	char df[512];
	memset(&totmem, 0, sizeof(totmem));
	snprintf(tf, sizeof(tf), "./nsd-mem-task-%u.db", (unsigned)getpid());
	if(opt->database == NULL || opt->database[0] == 0)
		df[0] = 0;
	else snprintf(df, sizeof(df), "./nsd-mem-db-%u.db", (unsigned)getpid());

	/* read all zones and account memory */
	RBTREE_FOR(zo, zone_options_t*, opt->zone_options) {
		check_zone_mem(tf, df, zo, opt, &totmem);
	}

	/* calculate more total statistics */
	account_total(opt, &totmem);
	/* print statistics */
	print_tot_mem(&totmem);

	/* final advice */
	if(opt->database != NULL && opt->database[0] != 0) {
		printf("\nFinal advice estimate:\n");
		printf("(The partial mmap causes reload&AXFR to take longer(disk access))\n");
		pretty_mem(totmem.ram + totmem.disk, "data and big mmap");
		pretty_mem(totmem.ram + totmem.disk/6, "data and partial mmap");
	}
}

/* dummy functions to link */
struct nsd;
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
	const char *configfile = CONFIGFILE;
	memset(&nsd, 0, sizeof(nsd));

	log_init("nsd-mem");

	/* Parse the command line... */
	while ((c = getopt(argc, argv, "c:h"
		)) != -1) {
		switch (c) {
		case 'c':
			configfile = optarg;
			break;
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
	if (argc != 0) {
		usage();
		exit(1);
	}

	/* Read options */
	nsd.options = nsd_options_create(region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1));
	tsig_init(nsd.options->region);
	if(!parse_options_file(nsd.options, configfile, NULL, NULL)) {
		error("could not read config: %s\n", configfile);
	}
	if(!parse_zone_list_file(nsd.options)) {
		error("could not read zonelist file %s\n",
			nsd.options->zonelistfile);
	}
	if (verbosity == 0)
		verbosity = nsd.options->verbosity;

#ifdef HAVE_CHROOT
	if(nsd.chrootdir == 0) nsd.chrootdir = nsd.options->chroot;
#ifdef CHROOTDIR
	/* if still no chrootdir, fallback to default */
	if(nsd.chrootdir == 0) nsd.chrootdir = CHROOTDIR;
#endif /* CHROOTDIR */
#endif /* HAVE_CHROOT */
	if(nsd.options->zonesdir && nsd.options->zonesdir[0]) {
		if(chdir(nsd.options->zonesdir)) {
			error("cannot chdir to '%s': %s",
				nsd.options->zonesdir, strerror(errno));
		}
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "changed directory to %s",
			nsd.options->zonesdir));
	}

	/* Chroot */
#ifdef HAVE_CHROOT
	if (nsd.chrootdir && strlen(nsd.chrootdir)) {
		if(chdir(nsd.chrootdir)) {
			error("unable to chdir to chroot: %s", strerror(errno));
		}
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "changed root directory to %s",
			nsd.chrootdir));
	}
#endif /* HAVE_CHROOT */

	check_mem(nsd.options);

	exit(0);
}
