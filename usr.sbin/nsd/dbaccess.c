/*
 * dbaccess.c -- access methods for nsd(8) database
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "dns.h"
#include "namedb.h"
#include "util.h"
#include "options.h"
#include "rdata.h"
#include "udb.h"
#include "udbradtree.h"
#include "udbzone.h"
#include "zonec.h"
#include "nsec3.h"
#include "difffile.h"
#include "nsd.h"

static time_t udb_time = 0;
static unsigned long udb_rrsets = 0;
static unsigned long udb_rrset_count = 0;

void
namedb_close(struct namedb* db)
{
	if(db) {
		if(db->udb) {
			udb_base_close(db->udb);
			udb_base_free(db->udb);
			db->udb = NULL;
		}
		zonec_desetup_parser();
		region_destroy(db->region);
	}
}

void
namedb_close_udb(struct namedb* db)
{
	if(db) {
		/* we cannot actually munmap the data, because other
		 * processes still need to access the udb, so cleanup the
		 * udb */
		udb_base_free_keep_mmap(db->udb);
		db->udb = NULL;
	}
}

void
apex_rrset_checks(namedb_type* db, rrset_type* rrset, domain_type* domain)
{
	uint32_t soa_minimum;
	unsigned i;
	zone_type* zone = rrset->zone;
	assert(domain == zone->apex);
	(void)domain;
	if (rrset_rrtype(rrset) == TYPE_SOA) {
		zone->soa_rrset = rrset;

		/* BUG #103 add another soa with a tweaked ttl */
		if(zone->soa_nx_rrset == 0) {
			zone->soa_nx_rrset = region_alloc(db->region,
				sizeof(rrset_type));
			zone->soa_nx_rrset->rr_count = 1;
			zone->soa_nx_rrset->next = 0;
			zone->soa_nx_rrset->zone = zone;
			zone->soa_nx_rrset->rrs = region_alloc(db->region,
				sizeof(rr_type));
		}
		memcpy(zone->soa_nx_rrset->rrs, rrset->rrs, sizeof(rr_type));

		/* check the ttl and MINIMUM value and set accordingly */
		memcpy(&soa_minimum, rdata_atom_data(rrset->rrs->rdatas[6]),
				rdata_atom_size(rrset->rrs->rdatas[6]));
		if (rrset->rrs->ttl > ntohl(soa_minimum)) {
			zone->soa_nx_rrset->rrs[0].ttl = ntohl(soa_minimum);
		}
	} else if (rrset_rrtype(rrset) == TYPE_NS) {
		zone->ns_rrset = rrset;
	} else if (rrset_rrtype(rrset) == TYPE_RRSIG) {
		for (i = 0; i < rrset->rr_count; ++i) {
			if(rr_rrsig_type_covered(&rrset->rrs[i])==TYPE_DNSKEY){
				zone->is_secure = 1;
				break;
			}
		}
	}
}

/** read rr */
static void
read_rr(namedb_type* db, rr_type* rr, udb_ptr* urr, domain_type* domain)
{
	buffer_type buffer;
	ssize_t c;
	assert(udb_ptr_get_type(urr) == udb_chunk_type_rr);
	rr->owner = domain;
	rr->type = RR(urr)->type;
	rr->klass = RR(urr)->klass;
	rr->ttl = RR(urr)->ttl;

	buffer_create_from(&buffer, RR(urr)->wire, RR(urr)->len);
	c = rdata_wireformat_to_rdata_atoms(db->region, db->domains,
		rr->type, RR(urr)->len, &buffer, &rr->rdatas);
	if(c == -1) {
		/* safe on error */
		rr->rdata_count = 0;
		rr->rdatas = NULL;
		return;
	}
	rr->rdata_count = c;
}

/** calculate rr count */
static uint16_t
calculate_rr_count(udb_base* udb, udb_ptr* rrset)
{
	udb_ptr rr;
	uint16_t num = 0;
	udb_ptr_new(&rr, udb, &RRSET(rrset)->rrs);
	while(rr.data) {
		num++;
		udb_ptr_set_rptr(&rr, udb, &RR(&rr)->next);
	}
	udb_ptr_unlink(&rr, udb);
	return num;
}

/** read rrset */
static void
read_rrset(udb_base* udb, namedb_type* db, zone_type* zone,
	domain_type* domain, udb_ptr* urrset)
{
	rrset_type* rrset;
	udb_ptr urr;
	unsigned i;
	assert(udb_ptr_get_type(urrset) == udb_chunk_type_rrset);
	/* if no RRs, do not create anything (robust) */
	if(RRSET(urrset)->rrs.data == 0)
		return;
	rrset = (rrset_type *) region_alloc(db->region, sizeof(rrset_type));
	rrset->zone = zone;
	rrset->rr_count = calculate_rr_count(udb, urrset);
	rrset->rrs = (rr_type *) region_alloc_array(
		db->region, rrset->rr_count, sizeof(rr_type));
	/* add the RRs */
	udb_ptr_new(&urr, udb, &RRSET(urrset)->rrs);
	for(i=0; i<rrset->rr_count; i++) {
		read_rr(db, &rrset->rrs[i], &urr, domain);
		udb_ptr_set_rptr(&urr, udb, &RR(&urr)->next);
	}
	udb_ptr_unlink(&urr, udb);
	domain_add_rrset(domain, rrset);
	if(domain == zone->apex)
		apex_rrset_checks(db, rrset, domain);
}

/** read one elem from db, of type domain_d */
static void read_node_elem(udb_base* udb, namedb_type* db, 
	region_type* dname_region, zone_type* zone, struct domain_d* d)
{
	const dname_type* dname;
	domain_type* domain;
	udb_ptr urrset;

	dname = dname_make(dname_region, d->name, 0);
	if(!dname) return;
	domain = domain_table_insert(db->domains, dname);
	assert(domain); /* domain_table_insert should always return non-NULL */

	/* add rrsets */
	udb_ptr_init(&urrset, udb);
	udb_ptr_set_rptr(&urrset, udb, &d->rrsets);
	while(urrset.data) {
		read_rrset(udb, db, zone, domain, &urrset);
		udb_ptr_set_rptr(&urrset, udb, &RRSET(&urrset)->next);

		if(++udb_rrsets % ZONEC_PCT_COUNT == 0 && time(NULL) > udb_time + ZONEC_PCT_TIME) {
			udb_time = time(NULL);
			VERBOSITY(1, (LOG_INFO, "read %s %d %%",
				zone->opts->name,
				(int)(udb_rrsets*((unsigned long)100)/udb_rrset_count)));
		}
	}
	region_free_all(dname_region);
	udb_ptr_unlink(&urrset, udb);
}

/** recurse read radix from disk. This radix tree is by domain name, so max of
 * 256 depth, and thus the stack usage is small. */
static void read_zone_recurse(udb_base* udb, namedb_type* db,
	region_type* dname_region, zone_type* zone, struct udb_radnode_d* node)
{
	if(node->elem.data) {
		/* pre-order process of node->elem, for radix tree this is
		 * also in-order processing (identical to order tree_next()) */
		read_node_elem(udb, db, dname_region, zone, (struct domain_d*)
			(udb->base + node->elem.data));
	}
	if(node->lookup.data) {
		uint16_t i;
		struct udb_radarray_d* a = (struct udb_radarray_d*)
			(udb->base + node->lookup.data);
		/* we do not care for what the exact radix key is, we want
		 * to add all of them and the read routine does not need
		 * the radix-key, it has it stored */
		for(i=0; i<a->len; i++) {
			if(a->array[i].node.data) {
				read_zone_recurse(udb, db, dname_region, zone,
					(struct udb_radnode_d*)(udb->base +
						a->array[i].node.data));
			}
		}
	}
}

/** read zone data */
static void
read_zone_data(udb_base* udb, namedb_type* db, region_type* dname_region,
	udb_ptr* z, zone_type* zone)
{
	udb_ptr dtree;
	/* recursively read domains, we only read so ptrs stay valid */
	udb_ptr_new(&dtree, udb, &ZONE(z)->domains);
	if(RADTREE(&dtree)->root.data)
		read_zone_recurse(udb, db, dname_region, zone,
			(struct udb_radnode_d*)
			(udb->base + RADTREE(&dtree)->root.data));
	udb_ptr_unlink(&dtree, udb);
}

/** create a zone */
zone_type*
namedb_zone_create(namedb_type* db, const dname_type* dname,
	struct zone_options* zo)
{
	zone_type* zone = (zone_type *) region_alloc(db->region,
		sizeof(zone_type));
	zone->node = radname_insert(db->zonetree, dname_name(dname),
		dname->name_size, zone);
	assert(zone->node);
	zone->apex = domain_table_insert(db->domains, dname);
	zone->apex->usage++; /* the zone.apex reference */
	zone->apex->is_apex = 1;
	zone->soa_rrset = NULL;
	zone->soa_nx_rrset = NULL;
	zone->ns_rrset = NULL;
#ifdef NSEC3
	zone->nsec3_param = NULL;
	zone->nsec3_last = NULL;
	zone->nsec3tree = NULL;
	zone->hashtree = NULL;
	zone->wchashtree = NULL;
	zone->dshashtree = NULL;
#endif
	zone->opts = zo;
	zone->filename = NULL;
	zone->logstr = NULL;
	zone->mtime.tv_sec = 0;
	zone->mtime.tv_nsec = 0;
	zone->zonestatid = 0;
	zone->is_secure = 0;
	zone->is_changed = 0;
	zone->is_ok = 1;
	return zone;
}

void
namedb_zone_delete(namedb_type* db, zone_type* zone)
{
	/* RRs and UDB and NSEC3 and so on must be already deleted */
	radix_delete(db->zonetree, zone->node);

	/* see if apex can be deleted */
	if(zone->apex) {
		zone->apex->usage --;
		zone->apex->is_apex = 0;
		if(zone->apex->usage == 0) {
			/* delete the apex, possibly */
			domain_table_deldomain(db, zone->apex);
		}
	}

	/* soa_rrset is freed when the SOA was deleted */
	if(zone->soa_nx_rrset) {
		region_recycle(db->region, zone->soa_nx_rrset->rrs,
			sizeof(rr_type));
		region_recycle(db->region, zone->soa_nx_rrset,
			sizeof(rrset_type));
	}
#ifdef NSEC3
	hash_tree_delete(db->region, zone->nsec3tree);
	hash_tree_delete(db->region, zone->hashtree);
	hash_tree_delete(db->region, zone->wchashtree);
	hash_tree_delete(db->region, zone->dshashtree);
#endif
	if(zone->filename)
		region_recycle(db->region, zone->filename,
			strlen(zone->filename)+1);
	if(zone->logstr)
		region_recycle(db->region, zone->logstr,
			strlen(zone->logstr)+1);
	region_recycle(db->region, zone, sizeof(zone_type));
}

#ifdef HAVE_MMAP
/** read a zone */
static void
read_zone(udb_base* udb, namedb_type* db, struct nsd_options* opt,
	region_type* dname_region, udb_ptr* z)
{
	/* construct dname */
	const dname_type* dname = dname_make(dname_region, ZONE(z)->name, 0);
	struct zone_options* zo = dname?zone_options_find(opt, dname):NULL;
	zone_type* zone;
	if(!dname) return;
	if(!zo) {
		/* deleted from the options, remove it from the nsd.db too */
		VERBOSITY(2, (LOG_WARNING, "zone %s is deleted",
			dname_to_string(dname, NULL)));
		udb_zone_delete(udb, z);
		region_free_all(dname_region);
		return;
	}
	assert(udb_ptr_get_type(z) == udb_chunk_type_zone);
	udb_rrsets = 0;
	udb_rrset_count = ZONE(z)->rrset_count;
	zone = namedb_zone_create(db, dname, zo);
	region_free_all(dname_region);
	read_zone_data(udb, db, dname_region, z, zone);
	zone->is_changed = (ZONE(z)->is_changed != 0);
#ifdef NSEC3
	prehash_zone_complete(db, zone);
#endif
}
#endif /* HAVE_MMAP */

#ifdef HAVE_MMAP
/** read zones from nsd.db */
static void
read_zones(udb_base* udb, namedb_type* db, struct nsd_options* opt,
	region_type* dname_region)
{
	udb_ptr ztree, n, z;
	udb_ptr_init(&z, udb);
	udb_ptr_new(&ztree, udb, udb_base_get_userdata(udb));
	udb_radix_first(udb,&ztree,&n);
	udb_time = time(NULL);
	while(n.data) {
		udb_ptr_set_rptr(&z, udb, &RADNODE(&n)->elem);
		udb_radix_next(udb, &n); /* store in case n is deleted */
		read_zone(udb, db, opt, dname_region, &z);
		udb_ptr_zero(&z, udb);
		if(nsd.signal_hint_shutdown) break;
	}
	udb_ptr_unlink(&ztree, udb);
	udb_ptr_unlink(&n, udb);
	udb_ptr_unlink(&z, udb);
}
#endif /* HAVE_MMAP */

#ifdef HAVE_MMAP
/** try to read the udb file or fail */
static int
try_read_udb(namedb_type* db, int fd, const char* filename,
	struct nsd_options* opt)
{
	/*
	 * Temporary region used while loading domain names from the
	 * database.  The region is freed after each time a dname is
	 * read from the database.
	 */
	region_type* dname_region;

	assert(fd != -1);
	if(!(db->udb=udb_base_create_fd(filename, fd, &namedb_walkfunc,
		NULL))) {
		/* fd is closed by failed udb create call */
		VERBOSITY(1, (LOG_WARNING, "can not use %s, "
			"will create anew", filename));
		return 0;
	}
	/* sanity check if can be opened */
	if(udb_base_get_userflags(db->udb) != 0) {
		log_msg(LOG_WARNING, "%s was not closed properly, it might "
			"be corrupted, will create anew", filename);
		udb_base_free(db->udb);
		db->udb = NULL;
		return 0;
	}
	/* read if it can be opened */
	dname_region = region_create(xalloc, free);
	/* this operation does not fail, we end up with
	 * something, even if that is an empty namedb */
	read_zones(db->udb, db, opt, dname_region);
	region_destroy(dname_region);
	return 1;
}
#endif /* HAVE_MMAP */

struct namedb *
namedb_open (const char* filename, struct nsd_options* opt)
{
	namedb_type* db;

	/*
	 * Region used to store the loaded database.  The region is
	 * freed in namedb_close.
	 */
	region_type* db_region;
	int fd;

#ifdef USE_MMAP_ALLOC
	db_region = region_create_custom(mmap_alloc, mmap_free, MMAP_ALLOC_CHUNK_SIZE,
		MMAP_ALLOC_LARGE_OBJECT_SIZE, MMAP_ALLOC_INITIAL_CLEANUP_SIZE, 1);
#else /* !USE_MMAP_ALLOC */
	db_region = region_create_custom(xalloc, free, DEFAULT_CHUNK_SIZE,
		DEFAULT_LARGE_OBJECT_SIZE, DEFAULT_INITIAL_CLEANUP_SIZE, 1);
#endif /* !USE_MMAP_ALLOC */
	db = (namedb_type *) region_alloc(db_region, sizeof(struct namedb));
	db->region = db_region;
	db->domains = domain_table_create(db->region);
	db->zonetree = radix_tree_create(db->region);
	db->diff_skip = 0;
	db->diff_pos = 0;
	zonec_setup_parser(db);

	if (gettimeofday(&(db->diff_timestamp), NULL) != 0) {
		log_msg(LOG_ERR, "unable to load %s: cannot initialize"
				 "timestamp", filename);
		region_destroy(db_region);
		return NULL;
        }

	/* in dbless mode there is no file to read or mmap */
	if(filename == NULL || filename[0] == 0) {
		db->udb = NULL;
		return db;
	}

#ifndef HAVE_MMAP
	/* no mmap() system call, use dbless mode */
	VERBOSITY(1, (LOG_INFO, "no mmap(), ignoring database %s", filename));
	db->udb = NULL;
	(void)fd; (void)opt;
	return db;
#else /* HAVE_MMAP */

	/* attempt to open, if does not exist, create a new one */
	fd = open(filename, O_RDWR);
	if(fd == -1) {
		if(errno != ENOENT) {
			log_msg(LOG_ERR, "%s: %s", filename, strerror(errno));
			region_destroy(db_region);
			return NULL;
		}
	}
	/* attempt to read the file (if it exists) */
	if(fd != -1) {
		if(!try_read_udb(db, fd, filename, opt))
			fd = -1;
	}
	/* attempt to create the file (if necessary or failed read) */
	if(fd == -1) {
		if(!(db->udb=udb_base_create_new(filename, &namedb_walkfunc,
			NULL))) {
			region_destroy(db_region);
			return NULL;
		}
		if(!udb_dns_init_file(db->udb)) {
			region_destroy(db->region);
			return NULL;
		}
	}
	return db;
#endif /* HAVE_MMAP */
}

/** the the file mtime stat (or nonexist or error) */
int
file_get_mtime(const char* file, struct timespec* mtime, int* nonexist)
{
	struct stat s;
	if(stat(file, &s) != 0) {
		mtime->tv_sec = 0;
		mtime->tv_nsec = 0;
		*nonexist = (errno == ENOENT);
		return 0;
	}
	*nonexist = 0;
	mtime->tv_sec = s.st_mtime;
#ifdef HAVE_STRUCT_STAT_ST_MTIMENSEC
	mtime->tv_nsec = s.st_mtimensec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	mtime->tv_nsec = s.st_mtim.tv_nsec;
#else
	mtime->tv_nsec = 0;
#endif
	return 1;
}

void
namedb_read_zonefile(struct nsd* nsd, struct zone* zone, udb_base* taskudb,
	udb_ptr* last_task)
{
	struct timespec mtime;
	int nonexist = 0;
	unsigned int errors;
	const char* fname;
	if(!nsd->db || !zone || !zone->opts || !zone->opts->pattern->zonefile)
		return;
	mtime.tv_sec = 0;
	mtime.tv_nsec = 0;
	fname = config_make_zonefile(zone->opts, nsd);
	if(!file_get_mtime(fname, &mtime, &nonexist)) {
		if(nonexist) {
			VERBOSITY(2, (LOG_INFO, "zonefile %s does not exist",
				fname));
		} else
			log_msg(LOG_ERR, "zonefile %s: %s",
				fname, strerror(errno));
		if(taskudb) task_new_soainfo(taskudb, last_task, zone, 0);
		return;
	} else {
		const char* zone_fname = zone->filename;
		struct timespec zone_mtime = zone->mtime;
		if(nsd->db->udb) {
			zone_fname = udb_zone_get_file_str(nsd->db->udb,
				dname_name(domain_dname(zone->apex)),
				domain_dname(zone->apex)->name_size);
			udb_zone_get_mtime(nsd->db->udb,
				dname_name(domain_dname(zone->apex)),
				domain_dname(zone->apex)->name_size,
				&zone_mtime);
		}
		/* if no zone_fname, then it was acquired in zone transfer,
		 * see if the file is newer than the zone transfer
		 * (regardless if this is a different file), because the
		 * zone transfer is a different content source too */
		if(!zone_fname && timespec_compare(&zone_mtime, &mtime) >= 0) {
			VERBOSITY(3, (LOG_INFO, "zonefile %s is older than "
				"zone transfer in memory", fname));
			return;

		/* if zone_fname, then the file was acquired from reading it,
		 * and see if filename changed or mtime newer to read it */
		} else if(zone_fname && fname &&
		   strcmp(zone_fname, fname) == 0 &&
		   timespec_compare(&zone_mtime, &mtime) == 0) {
			VERBOSITY(3, (LOG_INFO, "zonefile %s is not modified",
				fname));
			return;
		}
	}

	assert(parser);
	/* wipe zone from memory */
#ifdef NSEC3
	nsec3_clear_precompile(nsd->db, zone);
	zone->nsec3_param = NULL;
#endif
	delete_zone_rrs(nsd->db, zone);
	errors = zonec_read(zone->opts->name, fname, zone);
	if(errors > 0) {
		log_msg(LOG_ERR, "zone %s file %s read with %u errors",
			zone->opts->name, fname, errors);
		/* wipe (partial) zone from memory */
		zone->is_ok = 1;
#ifdef NSEC3
		nsec3_clear_precompile(nsd->db, zone);
		zone->nsec3_param = NULL;
#endif
		delete_zone_rrs(nsd->db, zone);
		if(nsd->db->udb) {
			region_type* dname_region;
			udb_ptr z;
			/* see if we can revert to the udb stored version */
			if(!udb_zone_search(nsd->db->udb, &z, dname_name(domain_dname(
				zone->apex)), domain_dname(zone->apex)->name_size)) {
				/* tell that zone contents has been lost */
				if(taskudb) task_new_soainfo(taskudb, last_task, zone, 0);
				return;
			}
			/* read from udb */
			dname_region = region_create(xalloc, free);
			udb_rrsets = 0;
			udb_rrset_count = ZONE(&z)->rrset_count;
			udb_time = time(NULL);
			read_zone_data(nsd->db->udb, nsd->db, dname_region, &z, zone);
			region_destroy(dname_region);
			udb_ptr_unlink(&z, nsd->db->udb);
		} else {
			if(zone->filename)
				region_recycle(nsd->db->region, zone->filename,
					strlen(zone->filename)+1);
			zone->filename = NULL;
			if(zone->logstr)
				region_recycle(nsd->db->region, zone->logstr,
					strlen(zone->logstr)+1);
			zone->logstr = NULL;
		}
	} else {
		VERBOSITY(1, (LOG_INFO, "zone %s read with success",
			zone->opts->name));
		zone->is_ok = 1;
		zone->is_changed = 0;
		/* store zone into udb */
		if(nsd->db->udb) {
			if(!write_zone_to_udb(nsd->db->udb, zone, &mtime,
				fname)) {
				log_msg(LOG_ERR, "failed to store zone in db");
			} else {
				VERBOSITY(2, (LOG_INFO, "zone %s written to db",
					zone->opts->name));
			}
		} else {
			zone->mtime = mtime;
			if(zone->filename)
				region_recycle(nsd->db->region, zone->filename,
					strlen(zone->filename)+1);
			zone->filename = region_strdup(nsd->db->region, fname);
			if(zone->logstr)
				region_recycle(nsd->db->region, zone->logstr,
					strlen(zone->logstr)+1);
			zone->logstr = NULL;
		}
	}
	if(taskudb) task_new_soainfo(taskudb, last_task, zone, 0);
#ifdef NSEC3
	prehash_zone_complete(nsd->db, zone);
#endif
}

void namedb_check_zonefile(struct nsd* nsd, udb_base* taskudb,
	udb_ptr* last_task, struct zone_options* zopt)
{
	zone_type* zone;
	const dname_type* dname = (const dname_type*)zopt->node.key;
	/* find zone to go with it, or create it */
	zone = namedb_find_zone(nsd->db, dname);
	if(!zone) {
		zone = namedb_zone_create(nsd->db, dname, zopt);
	}
	namedb_read_zonefile(nsd, zone, taskudb, last_task);
}

void namedb_check_zonefiles(struct nsd* nsd, struct nsd_options* opt,
	udb_base* taskudb, udb_ptr* last_task)
{
	struct zone_options* zo;
	/* check all zones in opt, create if not exist in main db */
	RBTREE_FOR(zo, struct zone_options*, opt->zone_options) {
		namedb_check_zonefile(nsd, taskudb, last_task, zo);
		if(nsd->signal_hint_shutdown) break;
	}
}
