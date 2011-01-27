/*
 * nsd-patch - read database and ixfrs and patch up zone files.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "options.h"
#include "difffile.h"
#include "namedb.h"
#include "util.h"

extern char *optarg;
extern int optind;

static void
usage(void)
{
	fprintf(stderr, "usage: nsd-patch [options]\n");
	fprintf(stderr, "       Reads database and ixfrs and patches up zone files.\n");
	fprintf(stderr, "       Version %s. Report bugs to <%s>.\n\n", PACKAGE_VERSION, PACKAGE_BUGREPORT);
	fprintf(stderr, "-c configfile	Specify config file to use, instead of %s\n", CONFIGFILE);
	fprintf(stderr, "-f		Force writing of zone files.\n");
	fprintf(stderr, "-h		Print this help information.\n");
	fprintf(stderr, "-l		List contents of transfer journal difffile, %s\n", DIFFFILE);
	fprintf(stderr, "-o dbfile	Specify dbfile to output the result "
			"directly to dbfile, nsd.db.\n");
	fprintf(stderr, "-s		Skip writing of zone files.\n");
	fprintf(stderr, "-x difffile	Specify diff file to use, instead of diff file from config.\n");
	exit(1);
}

static void
list_xfr(FILE *in)
{
	uint32_t timestamp[2];
	uint32_t skiplen, len, new_serial;
	char zone_name[3072];
	uint16_t id;
	uint32_t seq_nr, len2;

	if(!diff_read_32(in, &timestamp[0]) ||
		!diff_read_32(in, &timestamp[1]) ||
		!diff_read_32(in, &len) ||
		!diff_read_str(in, zone_name, sizeof(zone_name)) ||
		!diff_read_32(in, &new_serial) ||
		!diff_read_16(in, &id) ||
		!diff_read_32(in, &seq_nr)) {
		fprintf(stderr, "incomplete zone transfer content packet\n");
		return;
	}
	skiplen = len - (sizeof(uint32_t)*3 + sizeof(uint16_t) + strlen(zone_name));
	fprintf(stdout, "zone %s transfer id %x serial %u timestamp %u.%u: "
			"seq_nr %d of %d bytes\n", zone_name, id, new_serial,
		timestamp[0], timestamp[1], seq_nr, skiplen);

	if(fseeko(in, skiplen, SEEK_CUR) == -1)
		fprintf(stderr, "fseek failed: %s\n", strerror(errno));

	if(!diff_read_32(in, &len2)) {
		fprintf(stderr, "incomplete zone transfer content packet\n");
		return;
	}
	if(len != len2) {
		fprintf(stderr, "packet seq %d had bad length check bytes!\n",
			seq_nr);
	}
}

static const char*
get_date(const char* log)
{
	const char *entry = strstr(log, " time ");
	time_t t = 0;
	static char timep[30];
	if(!entry) return "<notime>";
	t = strtol(entry + 6, NULL, 10);
	if(t == 0) return "0";
	timep[0]=0;
	strlcpy(timep, ctime(&t), sizeof(timep));
	/* remove newline at end */
	if(strlen(timep) > 0 && timep[strlen(timep)-1]=='\n')
		timep[strlen(timep)-1] = 0;
	return timep;
}

static void
list_commit(FILE *in)
{
	uint32_t timestamp[2];
	uint32_t len;
	char zone_name[3072];
	uint32_t old_serial, new_serial;
	uint16_t id;
	uint32_t num;
	uint8_t commit;
	char log_msg[10240];
	uint32_t len2;

	if(!diff_read_32(in, &timestamp[0]) ||
		!diff_read_32(in, &timestamp[1]) ||
		!diff_read_32(in, &len) ||
		!diff_read_str(in, zone_name, sizeof(zone_name)) ||
		!diff_read_32(in, &old_serial) ||
		!diff_read_32(in, &new_serial) ||
		!diff_read_16(in, &id) ||
		!diff_read_32(in, &num) ||
		!diff_read_8(in, &commit) ||
		!diff_read_str(in, log_msg, sizeof(log_msg)) ||
		!diff_read_32(in, &len2)) {
		fprintf(stderr, "incomplete commit/rollback packet\n");
		return;
	}
	fprintf(stdout, "zone %s transfer id %x serial %d: %s of %d packets\n",
		zone_name, id, new_serial, commit?"commit":"rollback", num);
	fprintf(stdout, "   time %s, from serial %d, log message: %s\n",
		get_date(log_msg), old_serial, log_msg);
	if(len != len2) {
		fprintf(stderr, "  commit packet with bad length check \
			bytes!\n");
	}
}

static void
debug_list(struct nsd_options* opt)
{
	const char* file = opt->difffile;
	FILE *f;
	uint32_t type;
	fprintf(stdout, "debug listing of the contents of %s\n", file);
	f = fopen(file, "r");
	if(!f) {
		fprintf(stderr, "error opening %s: %s\n", file,
			strerror(errno));
		return;
	}
	while(diff_read_32(f, &type)) {
		switch(type) {
		case DIFF_PART_IXFR:
			list_xfr(f);
			break;
		case DIFF_PART_SURE:
			list_commit(f);
			break;
		default:
			fprintf(stderr, "bad part of type %x\n", type);
			break;
		}
	}

	fclose(f);
}

static int
exist_difffile(struct nsd_options* opt)
{
	/* see if diff file exists */
	const char* file = opt->difffile;
	FILE *f;

	f = fopen(file, "r");
	if(!f) {
		if(errno == ENOENT)
			return 0;
		fprintf(stderr, "could not open file %s: %s\n",
			file, strerror(errno));
		return 0;
	}
	return 1;
}

static void
print_rrs(FILE* out, struct zone* zone)
{
	rrset_type *rrset;
	domain_type *domain = zone->apex;
	region_type* region = region_create(xalloc, free);
	struct state_pretty_rr* state = create_pretty_rr(region);
	/* first print the SOA record for the zone */
	if(zone->soa_rrset) {
		size_t i;
		for(i=0; i < zone->soa_rrset->rr_count; i++) {
			if(!print_rr(out, state, &zone->soa_rrset->rrs[i])){
				fprintf(stderr, "There was an error "
				   "printing SOARR to zone %s\n",
				   zone->opts->name);
			}
		}
	}
        /* go through entire tree below the zone apex (incl subzones) */
	while(domain && dname_is_subdomain(
		domain_dname(domain), domain_dname(zone->apex)))
	{
		for(rrset = domain->rrsets; rrset; rrset=rrset->next)
		{
			size_t i;
			if(rrset->zone != zone || rrset == zone->soa_rrset)
				continue;
			for(i=0; i < rrset->rr_count; i++) {
				if(!print_rr(out, state, &rrset->rrs[i])){
					fprintf(stderr, "There was an error "
					   "printing RR to zone %s\n",
					   zone->opts->name);
				}
			}
		}
		domain = domain_next(domain);
	}
	region_destroy(region);
}

static void
print_commit_log(FILE* out, const dname_type* zone, struct diff_log* commit_log)
{
	struct diff_log* p = commit_log;
	region_type* region = region_create(xalloc, free);
	while(p)
	{
		const dname_type* dname = dname_parse(region, p->zone_name);
		if(dname_compare(dname, zone) == 0)
		{
			fprintf(out, "; commit");
			if(p->error)
				fprintf(out, "(%s)", p->error);
			fprintf(out, ": %s\n", p->comment);
		}
		p = p->next;
	}
	region_destroy(region);
}

static void
write_to_zonefile(struct zone* zone, struct diff_log* commit_log)
{
	const char* filename = zone->opts->zonefile;
	time_t now = time(0);
	FILE *out;

	fprintf(stdout, "writing zone %s to file %s\n", zone->opts->name,
		filename);

	if(!zone->apex) {
		fprintf(stderr, "zone %s has no apex, no data.\n", filename);
		return;
	}

	out = fopen(filename, "w");
	if(!out) {
		fprintf(stderr, "cannot open or create file %s for writing: %s\n",
			filename, strerror(errno));
		return;
	}

	/* print zone header */
	fprintf(out, "; NSD version %s\n", PACKAGE_VERSION);
	fprintf(out, "; nsd-patch zone %s run at time %s",
		zone->opts->name, ctime(&now));
	print_commit_log(out, domain_dname(zone->apex), commit_log);

	print_rrs(out, zone);

	fclose(out);
}

int main(int argc, char* argv[])
{
	int c;
	const char* configfile = CONFIGFILE;
	const char* difffile = NULL;
	const char* dbfile = NULL;
	nsd_options_t *options;
	struct namedb* db = NULL;
	struct namedb* dbout = NULL;
	struct zone* zone;
	struct diff_log* commit_log = 0;
	size_t fake_child_count = 1;
	int debug_list_diff = 0;
	int force_write = 0;
	int skip_write = 0;
	int difffile_exists = 0;

        /* Parse the command line... */
	while ((c = getopt(argc, argv, "c:fhlo:sx:")) != -1) {
	switch (c) {
		case 'c':
			configfile = optarg;
			break;
		case 'l':
			debug_list_diff = 1;
			break;
		case 'f':
			if (skip_write)
			{
				fprintf(stderr, "Cannot force and skip writing "
						"zonefiles at the same time\n");
				exit(1);
			}
			else
				force_write = 1;
			break;
		case 's':
			if (force_write)
			{
				fprintf(stderr, "Cannot skip and force writing "
						"zonefiles at the same time\n");
				exit(1);
			}
			else
				skip_write = 1;
			break;
		case 'o':
			dbfile = optarg;
			break;
		case 'x':
			difffile = optarg;
			break;
		case 'h':
		default:
			usage();
		};
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	/* read config file */
	log_init("nsd-patch");
	options = nsd_options_create(region_create(xalloc, free));
	if(!parse_options_file(options, configfile)) {
		fprintf(stderr, "Could not read config: %s\n", configfile);
		exit(1);
	}
	if(options->zonesdir && options->zonesdir[0]) {
		if (chdir(options->zonesdir)) {
			fprintf(stderr, "nsd-patch: cannot chdir to %s: %s\n",
				options->zonesdir, strerror(errno));
			exit(1);
		}
	}

	/* override difffile if commandline option given */
	if(difffile)
		options->difffile = difffile;

	/* see if necessary */
	if(!exist_difffile(options)) {
		fprintf(stderr, "No diff file.\n");
		if (!force_write)
			exit(0);
	}
	else	difffile_exists = 1;

	if(debug_list_diff) {
		debug_list(options);
		exit(0);
	}

	/* read database and diff file */
	fprintf(stdout, "reading database\n");
	db = namedb_open(options->database, options, fake_child_count);
	if(!db) {
		fprintf(stderr, "could not read database: %s\n",
			options->database);
		exit(1);
	}

	/* set all updated to 0 so we know what has changed */
	for(zone = db->zones; zone; zone = zone->next)
	{
		zone->updated = 0;
	}

	if (dbfile)
		dbout = namedb_new(dbfile);
	if (dbout)
	{
		db->fd = dbout->fd;
		db->filename = (char*) dbfile;
	}

	/* read ixfr diff file */
	if (difffile_exists) {
		fprintf(stdout, "reading updates to database\n");
		if(!diff_read_file(db, options, &commit_log, fake_child_count))
		{
			fprintf(stderr, "unable to load the diff file: %s\n",
				options->difffile);
			exit(1);
		}
	}

	if (skip_write)
		fprintf(stderr, "skip patching up zonefiles.\n");
	else {
		fprintf(stdout, "writing changed zones\n");
		for(zone = db->zones; zone; zone = zone->next)
		{
			if(!force_write && !zone->updated) {
				fprintf(stdout, "zone %s had not changed.\n",
					zone->opts->name);
				continue;
			}
			/* write zone to its zone file */
			write_to_zonefile(zone, commit_log);
		}
	}

	/* output result directly to dbfile */
	if (dbout)
	{
		fprintf(stdout, "storing database to %s.\n", dbout->filename);
	        if (namedb_save(db) != 0) {
			fprintf(stderr, "error writing the database (%s): %s\n",
				dbfile, strerror(errno));
			exit(1);
		}
	}
	fprintf(stdout, "done\n");

	return 0;
}
