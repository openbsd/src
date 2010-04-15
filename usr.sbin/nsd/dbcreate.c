/*
 * dbcreate.c -- routines to create an nsd(8) name database
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "namedb.h"

static int write_db (namedb_type *db);
static int write_number(struct namedb *db, uint32_t number);

struct namedb *
namedb_new (const char *filename)
{
	namedb_type *db;
	region_type *region;

#ifdef USE_MMAP_ALLOC
	region = region_create_custom(mmap_alloc, mmap_free,
		MMAP_ALLOC_CHUNK_SIZE, MMAP_ALLOC_LARGE_OBJECT_SIZE,
		MMAP_ALLOC_INITIAL_CLEANUP_SIZE, 1);
#else /* !USE_MMAP_ALLOC */
	region = region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1);
#endif /* !USE_MMAP_ALLOC */

	/* Make a new structure... */
	db = (namedb_type *) region_alloc(region, sizeof(namedb_type));
	db->region = region;
	db->domains = domain_table_create(region);
	db->zones = NULL;
	db->zone_count = 0;
	db->filename = region_strdup(region, filename);
	db->crc = 0xffffffff;
	db->diff_skip = 0;
	db->fd = NULL;

	if (gettimeofday(&(db->diff_timestamp), NULL) != 0) {
		log_msg(LOG_ERR, "unable to load %s: cannot initialize "
						 "timestamp", db->filename);
		region_destroy(region);
		return NULL;
	}

	/*
	 * Unlink the old database, if it exists.  This is useful to
	 * ensure that NSD doesn't see the changes until a reload is done.
	 */
	if (unlink(db->filename) == -1 && errno != ENOENT) {
		region_destroy(region);
		return NULL;
	}

	/* Create the database */
	if ((db->fd = fopen(db->filename, "w")) == NULL) {
		region_destroy(region);
		return NULL;
	}

	if (!write_data_crc(db->fd, NAMEDB_MAGIC, NAMEDB_MAGIC_SIZE, &db->crc)) {
		fclose(db->fd);
		namedb_discard(db);
		return NULL;
	}

	return db;
}


int
namedb_save (struct namedb *db)
{
	if (write_db(db) != 0) {
		return -1;
	}

	/* Finish up and write the crc */
	if (!write_number(db, ~db->crc)) {
		fclose(db->fd);
		return -1;
	}

	/* Write the magic... */
	if (!write_data_crc(db->fd, NAMEDB_MAGIC, NAMEDB_MAGIC_SIZE, &db->crc)) {
		fclose(db->fd);
		return -1;
	}

	/* Close the database */
	fclose(db->fd);

	region_destroy(db->region);
	return 0;
}


void
namedb_discard (struct namedb *db)
{
	unlink(db->filename);
	region_destroy(db->region);
}

static int
write_dname(struct namedb *db, domain_type *domain)
{
	const dname_type *dname = domain_dname(domain);

	if (!write_data_crc(db->fd, &dname->name_size, sizeof(dname->name_size), &db->crc))
		return -1;

	if (!write_data_crc(db->fd, dname_name(dname), dname->name_size, &db->crc))
		return -1;

	return 0;
}

static int
write_number(struct namedb *db, uint32_t number)
{
	number = htonl(number);
	return write_data_crc(db->fd, &number, sizeof(number), &db->crc);
}

static int
write_rrset(struct namedb *db, domain_type *domain, rrset_type *rrset)
{
	uint16_t rr_count;
	int i, j;
	uint16_t type;
	uint16_t klass;

	assert(db);
	assert(domain);
	assert(rrset);

	rr_count = htons(rrset->rr_count);

	if (!write_number(db, domain->number))
		return 1;

	if (!write_number(db, rrset->zone->number))
		return 1;

	type = htons(rrset_rrtype(rrset));
	if (!write_data_crc(db->fd, &type, sizeof(type), &db->crc))
		return 1;

	klass = htons(rrset_rrclass(rrset));
	if (!write_data_crc(db->fd, &klass, sizeof(klass), &db->crc))
		return 1;

	if (!write_data_crc(db->fd, &rr_count, sizeof(rr_count), &db->crc))
		return 1;

	for (i = 0; i < rrset->rr_count; ++i) {
		rr_type *rr = &rrset->rrs[i];
		uint32_t ttl;
		uint16_t rdata_count;

		rdata_count = htons(rr->rdata_count);
		if (!write_data_crc(db->fd, &rdata_count, sizeof(rdata_count), &db->crc))
			return 1;

		ttl = htonl(rr->ttl);
		if (!write_data_crc(db->fd, &ttl, sizeof(ttl), &db->crc))
			return 1;

		for (j = 0; j < rr->rdata_count; ++j) {
			rdata_atom_type atom = rr->rdatas[j];
			if (rdata_atom_is_domain(rr->type, j)) {
				if (!write_number(db, rdata_atom_domain(atom)->number))
					return 1;

			} else {
				uint16_t size = htons(rdata_atom_size(atom));
				if (!write_data_crc(db->fd, &size, sizeof(size), &db->crc))
					return 1;

				if (!write_data_crc(db->fd,
						rdata_atom_data(atom),
						rdata_atom_size(atom), &db->crc))
					return 1;

			}
		}
	}

	return 0;
}

static int
number_dnames_iterator(domain_type *node, void *user_data)
{
	uint32_t *current_number = (uint32_t *) user_data;

	node->number = *current_number;
	++*current_number;

	return 0;
}

static int
write_dname_iterator(domain_type *node, void *user_data)
{
	namedb_type *db = (namedb_type *) user_data;

	return write_dname(db, node);
}

static int
write_domain_iterator(domain_type *node, void *user_data)
{
	namedb_type *db = (namedb_type *) user_data;
	rrset_type *rrset;
	int error = 0;

	for (rrset = node->rrsets; rrset; rrset = rrset->next) {
		error += write_rrset(db, node, rrset);
	}

	return error;
}

/*
 * Writes databse data into open database *db
 *
 * Returns zero if success.
 */
static int
write_db(namedb_type *db)
{
	zone_type *zone;
	uint32_t terminator = 0;
	uint32_t dname_count = 1;
	uint32_t zone_count = 1;
	int errors = 0;

	for (zone = db->zones; zone; zone = zone->next) {
		zone->number = zone_count;
		++zone_count;

		if (!zone->soa_rrset) {
			fprintf(stderr, "SOA record not present in %s\n",
				dname_to_string(domain_dname(zone->apex),
						NULL));
			++errors;
		}
	}

	if (errors > 0)
		return -1;

	--zone_count;
	if (!write_number(db, zone_count))
		return -1;
	for (zone = db->zones; zone; zone = zone->next) {
		if (write_dname(db, zone->apex))
			return -1;
	}

	if (domain_table_iterate(db->domains, number_dnames_iterator, &dname_count))
		return -1;

	--dname_count;
	if (!write_number(db, dname_count))
		return -1;

	DEBUG(DEBUG_ZONEC, 1,
	      (LOG_INFO, "Storing %lu domain names\n", (unsigned long) dname_count));

	if (domain_table_iterate(db->domains, write_dname_iterator, db))
		return -1;

	if (domain_table_iterate(db->domains, write_domain_iterator, db))
		return -1;

	if (!write_data_crc(db->fd, &terminator, sizeof(terminator), &db->crc))
		return -1;

	return 0;
}
