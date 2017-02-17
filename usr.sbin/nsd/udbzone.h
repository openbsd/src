/*
 * udbzone -- store zone and rrset information in udb file.
 *
 * Copyright (c) 2011, NLnet Labs.  See LICENSE for license.
 */
#ifndef UDB_ZONE_H
#define UDB_ZONE_H
#include "udb.h"
#include "dns.h"
#include "udbradtree.h"

/**
 * Store the DNS information in udb file on disk.
 * udb_global
 *     |
 *     v
 * zonetree -> zone -- zone_name
 * radtree       |--> nsec3param
 *               |--> log_str
 *               |--> file_str
 *               |
 *               v
 *            domain --> rrset -> rr
 *            radtree    list     list
 *            |-- name
 */

/** zone information in the nsd.udb.  Name allocated after it. */
struct zone_d {
	/** radtree node in the zonetree for this zone */
	udb_rel_ptr node;
	/** the radtree for the domain names in the zone */
	udb_rel_ptr domains;
	/** the NSEC3PARAM rr used for hashing (or 0), rr_d pointer */
	udb_rel_ptr nsec3param;
	/** the log_str for the AXFR change, or 0 */
	udb_rel_ptr log_str;
	/** the file name when read from a file, or 0 */
	udb_rel_ptr file_str;
	/** modification time, time when the zone data was changed */
	uint64_t mtime;
	/** modification time, nsecs */
	uint64_t mtime_nsec;
	/** number of RRsets in the zone */
	uint64_t rrset_count;
	/** number of RRs in the zone */
	uint64_t rr_count;
	/** the length of the zone name */
	udb_radstrlen_type namelen;
	/** if the zone is expired */
	uint8_t expired;
	/** if the zone has been changed by AXFR */
	uint8_t is_changed;
	/** the zone (wire uncompressed) name in DNS format */
	uint8_t name[0];
};

/** domain name in the nametree. name allocated after it */
struct domain_d {
	/** radtree node in the nametree for this domain */
	udb_rel_ptr node;
	/** the list of rrsets for this name, single linked */
	udb_rel_ptr rrsets;
	/** length of the domain name */
	udb_radstrlen_type namelen;
	/** the domain (wire uncompressed) name in DNS format */
	uint8_t name[0];
};

/** rrset information. */
struct rrset_d {
	/** next in rrset list */
	udb_rel_ptr next;
	/** the singly linked list of rrs for this rrset */
	udb_rel_ptr rrs;
	/** type of the RRs in this rrset (host order) */
	uint16_t type;
};

/** rr information; wireformat data allocated after it */
struct rr_d {
	/** next in rr list */
	udb_rel_ptr next;
	/** type (host order) */
	uint16_t type;
	/** class (host order) */
	uint16_t klass;
	/** ttl (host order) */
	uint32_t ttl;
	/** length of wireformat */
	uint16_t len;
	/** wireformat of rdata (without rdatalen) */
	uint8_t wire[0];
};

/** init an udb for use as DNS store */
int udb_dns_init_file(udb_base* udb);
/** de-init an udb for use as DNS store */
void udb_dns_deinit_file(udb_base* udb);

/** create a zone */
int udb_zone_create(udb_base* udb, udb_ptr* result, const uint8_t* dname,
	size_t dlen);
/** clear all RRsets from a zone */
void udb_zone_clear(udb_base* udb, udb_ptr* zone);
/** delete a zone */
void udb_zone_delete(udb_base* udb, udb_ptr* zone);
/** find a zone by name (exact match) */
int udb_zone_search(udb_base* udb, udb_ptr* result, const uint8_t* dname,
	size_t dlen);
/** get modification time for zone or 0 */
void udb_zone_get_mtime(udb_base* udb, const uint8_t* dname, size_t dlen,
	struct timespec* mtime);
/** set log str in udb, or remove it */
void udb_zone_set_log_str(udb_base* udb, udb_ptr* zone, const char* str);
/** set file str in udb, or remove it */
void udb_zone_set_file_str(udb_base* udb, udb_ptr* zone, const char* str);
/** get file string for zone or NULL */
const char* udb_zone_get_file_str(udb_base* udb, const uint8_t* dname,
	size_t dlen);
/** find a domain name in the zone domain tree */
int udb_domain_find(udb_base* udb, udb_ptr* zone, const uint8_t* nm,
	size_t nmlen, udb_ptr* result);
/** find rrset in domain */
int udb_rrset_find(udb_base* udb, udb_ptr* domain, uint16_t t, udb_ptr* res);

/** add an RR to a zone */
int udb_zone_add_rr(udb_base* udb, udb_ptr* zone, const uint8_t* nm,
	size_t nmlen, uint16_t t, uint16_t k, uint32_t ttl, uint8_t* rdata,
	size_t rdatalen);
/** del an RR from a zone */
void udb_zone_del_rr(udb_base* udb, udb_ptr* zone, const uint8_t* nm,
	size_t nmlen, uint16_t t, uint16_t k, uint8_t* rdata, size_t rdatalen);

/** get pretty string for nsec3parameters (static buffer returned) */
const char* udb_nsec3param_string(udb_ptr* rr);

/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_zone */
void udb_zone_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);
/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_domain */
void udb_domain_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);
/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_rrset */
void udb_rrset_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);
/** for use in udb-walkfunc, walks relptrs in udb_chunk_type_rr */
void udb_rr_walk_chunk(void* base, void* d, uint64_t s,
	udb_walk_relptr_cb* cb, void* arg);

/** walk through relptrs in registered types */
void namedb_walkfunc(void* base, void* warg, uint8_t t, void* d, uint64_t s,
        udb_walk_relptr_cb* cb, void* arg);

#define ZONE(ptr) ((struct zone_d*)UDB_PTR(ptr))
#define DOMAIN(ptr) ((struct domain_d*)UDB_PTR(ptr))
#define RRSET(ptr) ((struct rrset_d*)UDB_PTR(ptr))
#define RR(ptr) ((struct rr_d*)UDB_PTR(ptr))

#endif /* UDB_ZONE_H */
