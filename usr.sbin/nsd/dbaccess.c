/*
 * dbaccess.c -- access methods for nsd(8) database
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>		/* DEBUG */

#include "dns.h"
#include "namedb.h"
#include "util.h"
#include "options.h"

int
namedb_lookup(struct namedb    *db,
	      const dname_type *dname,
	      domain_type     **closest_match,
	      domain_type     **closest_encloser)
{
	return domain_table_search(
		db->domains, dname, closest_match, closest_encloser);
}

static int
read_magic(namedb_type *db)
{
	char buf[NAMEDB_MAGIC_SIZE];

	if (fread(buf, sizeof(char), sizeof(buf), db->fd) != sizeof(buf))
		return 0;

	return memcmp(buf, NAMEDB_MAGIC, NAMEDB_MAGIC_SIZE) == 0;
}

static const dname_type *
read_dname(FILE *fd, region_type *region)
{
	uint8_t size;
	uint8_t temp[MAXDOMAINLEN];

	if (fread(&size, sizeof(uint8_t), 1, fd) != 1)
		return NULL;
	if (fread(temp, sizeof(uint8_t), size, fd) != size)
		return NULL;

	return dname_make(region, temp, 1);
}

static int
read_size(namedb_type *db, uint32_t *result)
{
	if (fread(result, sizeof(*result), 1, db->fd) == 1) {
		*result = ntohl(*result);
		return 1;
	} else {
		return 0;
	}
}

static domain_type *
read_domain(namedb_type *db, uint32_t domain_count, domain_type **domains)
{
	uint32_t domain_number;

	if (!read_size(db, &domain_number))
		return NULL;

	if (domain_number == 0 || domain_number > domain_count)
		return NULL;

	return domains[domain_number - 1];
}

static zone_type *
read_zone(namedb_type *db, uint32_t zone_count, zone_type **zones)
{
	uint32_t zone_number;

	if (!read_size(db, &zone_number))
		return NULL;

	if (zone_number == 0 || zone_number > zone_count)
		return NULL;

	return zones[zone_number - 1];
}

static int
read_rdata_atom(namedb_type *db, uint16_t type, int index, uint32_t domain_count, domain_type **domains, rdata_atom_type *result)
{
	uint8_t data[65536];

	if (rdata_atom_is_domain(type, index)) {
		result->domain = read_domain(db, domain_count, domains);
		if (!result->domain)
			return 0;
	} else {
		uint16_t size;

		if (fread(&size, sizeof(size), 1, db->fd) != 1)
			return 0;
		size = ntohs(size);
		if (fread(data, sizeof(uint8_t), size, db->fd) != size)
			return 0;

		result->data = (uint16_t *) region_alloc(
			db->region, sizeof(uint16_t) + size);
		memcpy(result->data, &size, sizeof(uint16_t));
		memcpy((uint8_t *) result->data + sizeof(uint16_t), data, size);
	}

	return 1;
}

static rrset_type *
read_rrset(namedb_type *db,
	   uint32_t domain_count, domain_type **domains,
	   uint32_t zone_count, zone_type **zones)
{
	rrset_type *rrset;
	int i, j;
	domain_type *owner;
	uint16_t type;
	uint16_t klass;
	uint32_t soa_minimum;

	owner = read_domain(db, domain_count, domains);
	if (!owner)
		return NULL;

	rrset = (rrset_type *) region_alloc(db->region, sizeof(rrset_type));

	rrset->zone = read_zone(db, zone_count, zones);
	if (!rrset->zone)
		return NULL;

	if (fread(&type, sizeof(type), 1, db->fd) != 1)
		return NULL;
	type = ntohs(type);

	if (fread(&klass, sizeof(klass), 1, db->fd) != 1)
		return NULL;
	klass = ntohs(klass);

	if (fread(&rrset->rr_count, sizeof(rrset->rr_count), 1, db->fd) != 1)
		return NULL;
	rrset->rr_count = ntohs(rrset->rr_count);
	rrset->rrs = (rr_type *) region_alloc(
		db->region, rrset->rr_count * sizeof(rr_type));

	assert(rrset->rr_count > 0);

	for (i = 0; i < rrset->rr_count; ++i) {
		rr_type *rr = &rrset->rrs[i];

		rr->owner = owner;
		rr->type = type;
		rr->klass = klass;

		if (fread(&rr->rdata_count, sizeof(rr->rdata_count), 1, db->fd) != 1)
			return NULL;
		rr->rdata_count = ntohs(rr->rdata_count);
		rr->rdatas = (rdata_atom_type *) region_alloc(
			db->region, rr->rdata_count * sizeof(rdata_atom_type));

		if (fread(&rr->ttl, sizeof(rr->ttl), 1, db->fd) != 1)
			return NULL;
		rr->ttl = ntohl(rr->ttl);

		for (j = 0; j < rr->rdata_count; ++j) {
			if (!read_rdata_atom(db, rr->type, j, domain_count, domains, &rr->rdatas[j]))
				return NULL;
		}
	}

	domain_add_rrset(owner, rrset);

	if (rrset_rrtype(rrset) == TYPE_SOA) {
		assert(owner == rrset->zone->apex);
		rrset->zone->soa_rrset = rrset;

		/* BUG #103 add another soa with a tweaked ttl */
		rrset->zone->soa_nx_rrset = region_alloc(db->region, sizeof(rrset_type));
		rrset->zone->soa_nx_rrset->rrs =
			region_alloc(db->region, rrset->rr_count * sizeof(rr_type));

		memcpy(rrset->zone->soa_nx_rrset->rrs, rrset->rrs, sizeof(rr_type));
		rrset->zone->soa_nx_rrset->rr_count = 1;
		rrset->zone->soa_nx_rrset->next = 0;

		/* also add a link to the zone */
		rrset->zone->soa_nx_rrset->zone = rrset->zone;

		/* check the ttl and MINIMUM value and set accordinly */
		memcpy(&soa_minimum, rdata_atom_data(rrset->rrs->rdatas[6]),
				rdata_atom_size(rrset->rrs->rdatas[6]));
		if (rrset->rrs->ttl > ntohl(soa_minimum)) {
			rrset->zone->soa_nx_rrset->rrs[0].ttl = ntohl(soa_minimum);
		}

	} else if (owner == rrset->zone->apex
		   && rrset_rrtype(rrset) == TYPE_NS)
	{
		rrset->zone->ns_rrset = rrset;
	}

#ifdef DNSSEC
	if (rrset_rrtype(rrset) == TYPE_RRSIG && owner == rrset->zone->apex) {
		for (i = 0; i < rrset->rr_count; ++i) {
			if (rr_rrsig_type_covered(&rrset->rrs[i]) == TYPE_SOA) {
				rrset->zone->is_secure = 1;
				break;
			}
		}
	}
#endif
	return rrset;
}

struct namedb *
namedb_open (const char *filename, nsd_options_t* opt, size_t num_children)
{
	namedb_type *db;

	/*
	 * Region used to store the loaded database.  The region is
	 * freed in namedb_close.
	 */
	region_type *db_region;

	/*
	 * Temporary region used while loading domain names from the
	 * database.  The region is freed after each time a dname is
	 * read from the database.
	 */
	region_type *dname_region;

	/*
	 * Temporary region used to store array of domains and zones
	 * while loading the database.  The region is freed before
	 * returning.
	 */
	region_type *temp_region;

	uint32_t dname_count;
	domain_type **domains;	/* Indexed by domain number.  */

	uint32_t zone_count;
	zone_type **zones;	/* Indexed by zone number.  */

	uint32_t i;
	uint32_t rrset_count = 0;
	uint32_t rr_count = 0;

	rrset_type *rrset;

	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(namedb_type) = %lu\n", (unsigned long) sizeof(namedb_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(zone_type) = %lu\n", (unsigned long) sizeof(zone_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(domain_type) = %lu\n", (unsigned long) sizeof(domain_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(rrset_type) = %lu\n", (unsigned long) sizeof(rrset_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(rr_type) = %lu\n", (unsigned long) sizeof(rr_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(rdata_atom_type) = %lu\n", (unsigned long) sizeof(rdata_atom_type)));
	DEBUG(DEBUG_DBACCESS, 2,
	      (LOG_INFO, "sizeof(rbnode_t) = %lu\n", (unsigned long) sizeof(rbnode_t)));

	db_region = region_create_custom(xalloc, free, DEFAULT_CHUNK_SIZE,
		DEFAULT_LARGE_OBJECT_SIZE, DEFAULT_INITIAL_CLEANUP_SIZE, 1);
	db = (namedb_type *) region_alloc(db_region, sizeof(struct namedb));
	db->region = db_region;
	db->domains = domain_table_create(db->region);
	db->zones = NULL;
	db->zone_count = 0;
	db->filename = region_strdup(db->region, filename);
	db->crc = 0xffffffff;
	db->diff_skip = 0;

	if (gettimeofday(&(db->diff_timestamp), NULL) != 0) {
		log_msg(LOG_ERR, "unable to load %s: cannot initialize"
				 "timestamp", db->filename);
		region_destroy(db_region);
                return NULL;
        }

	/* Open it... */
	db->fd = fopen(db->filename, "r");
	if (db->fd == NULL) {
		log_msg(LOG_ERR, "unable to load %s: %s",
			db->filename, strerror(errno));
		region_destroy(db_region);
		return NULL;
	}

	if (!read_magic(db)) {
		log_msg(LOG_ERR, "corrupted database (read magic): %s", db->filename);
		namedb_close(db);
		return NULL;
	}

	if (!read_size(db, &zone_count)) {
		log_msg(LOG_ERR, "corrupted database (read size): %s", db->filename);
		namedb_close(db);
		return NULL;
	}

	DEBUG(DEBUG_DBACCESS, 1,
	      (LOG_INFO, "Retrieving %lu zones\n", (unsigned long) zone_count));

	temp_region = region_create(xalloc, free);
	dname_region = region_create(xalloc, free);

	db->zone_count = zone_count;
	zones = (zone_type **) region_alloc(temp_region,
					    zone_count * sizeof(zone_type *));
	for (i = 0; i < zone_count; ++i) {
		const dname_type *dname = read_dname(db->fd, dname_region);
		if (!dname) {
			log_msg(LOG_ERR, "corrupted database (read dname): %s", db->filename);
			region_destroy(dname_region);
			region_destroy(temp_region);
			namedb_close(db);
			return NULL;
		}
		zones[i] = (zone_type *) region_alloc(db->region,
						      sizeof(zone_type));
		zones[i]->next = db->zones;
		db->zones = zones[i];
		zones[i]->apex = domain_table_insert(db->domains, dname);
		zones[i]->soa_rrset = NULL;
		zones[i]->soa_nx_rrset = NULL;
		zones[i]->ns_rrset = NULL;
#ifdef NSEC3
		zones[i]->nsec3_soa_rr = NULL;
		zones[i]->nsec3_last = NULL;
#endif
		zones[i]->opts = zone_options_find(opt, domain_dname(zones[i]->apex));
		zones[i]->number = i + 1;
		zones[i]->is_secure = 0;
		zones[i]->updated = 1;
		zones[i]->is_ok = 0;
		zones[i]->dirty = region_alloc(db->region, sizeof(uint8_t)*num_children);
		memset(zones[i]->dirty, 0, sizeof(uint8_t)*num_children);
		if(!zones[i]->opts) {
			log_msg(LOG_ERR, "cannot load database. Zone %s in db "
					 "%s, but not in config file (might "
					 "happen if you edited the config "
					 "file). Please rebuild database and "
					 "start again.",
				dname_to_string(dname, NULL), db->filename);
			region_destroy(dname_region);
			region_destroy(temp_region);
			namedb_close(db);
			return NULL;
		}

		region_free_all(dname_region);
	}

	if (!read_size(db, &dname_count)) {
		log_msg(LOG_ERR, "corrupted database (read size): %s", db->filename);
		region_destroy(dname_region);
		region_destroy(temp_region);
		namedb_close(db);
		return NULL;
	}

	DEBUG(DEBUG_DBACCESS, 1,
	      (LOG_INFO, "Retrieving %lu domain names\n", (unsigned long) dname_count));

	domains = (domain_type **) region_alloc(
		temp_region, dname_count * sizeof(domain_type *));
	for (i = 0; i < dname_count; ++i) {
		const dname_type *dname = read_dname(db->fd, dname_region);
		if (!dname) {
			log_msg(LOG_ERR, "corrupted database (read dname): %s", db->filename);
			region_destroy(dname_region);
			region_destroy(temp_region);
			namedb_close(db);
			return NULL;
		}
		domains[i] = domain_table_insert(db->domains, dname);
		region_free_all(dname_region);
	}

	region_destroy(dname_region);

#ifndef NDEBUG
	fprintf(stderr, "database region after loading domain names: ");
	region_dump_stats(db->region, stderr);
	fprintf(stderr, "\n");
#endif

	while ((rrset = read_rrset(db, dname_count, domains, zone_count, zones))) {
		++rrset_count;
		rr_count += rrset->rr_count;
	}

	DEBUG(DEBUG_DBACCESS, 1,
	      (LOG_INFO, "Retrieved %lu RRs in %lu RRsets\n",
	       (unsigned long) rr_count, (unsigned long) rrset_count));

	region_destroy(temp_region);

	if ((db->crc_pos = ftello(db->fd)) == -1) {
		log_msg(LOG_ERR, "ftello %s failed: %s",
			db->filename, strerror(errno));
		namedb_close(db);
		return NULL;
	}
	if (!read_size(db, &db->crc)) {
		log_msg(LOG_ERR, "corrupted database (read size): %s", db->filename);
		namedb_close(db);
		return NULL;
	}
	if (!read_magic(db)) {
		log_msg(LOG_ERR, "corrupted database (read magic): %s", db->filename);
		namedb_close(db);
		return NULL;
	}

	fclose(db->fd);
	db->fd = NULL;

#ifndef NDEBUG
	fprintf(stderr, "database region after loading database: ");
	region_dump_stats(db->region, stderr);
	fprintf(stderr, "\n");
#endif

	return db;
}

void
namedb_close (struct namedb *db)
{
	if (db) {
		if (db->fd) {
			fclose(db->fd);
		}
		region_destroy(db->region);
	}
}
