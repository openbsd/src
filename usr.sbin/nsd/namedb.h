/*
 * namedb.h -- nsd(8) internal namespace database definitions
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef _NAMEDB_H_
#define	_NAMEDB_H_

#include <stdio.h>

#include "dname.h"
#include "dns.h"
#include "rbtree.h"
struct zone_options;
struct nsd_options;

#define	NAMEDB_MAGIC		"NSDdbV07"
#define	NAMEDB_MAGIC_SIZE	8

typedef union rdata_atom rdata_atom_type;
typedef struct rrset rrset_type;
typedef struct rr rr_type;

/*
 * A domain name table supporting fast insert and search operations.
 */
typedef struct domain_table domain_table_type;
typedef struct domain domain_type;
typedef struct zone zone_type;

struct domain_table
{
	region_type *region;
	rbtree_t      *names_to_domains;
	domain_type *root;
};

struct domain
{
	rbnode_t     node;
	domain_type *parent;
	domain_type *wildcard_child_closest_match;
	rrset_type  *rrsets;
#ifdef NSEC3
	/* (if nsec3 chain complete) always the covering nsec3 record */
	domain_type *nsec3_cover;
	/* the nsec3 that covers the wildcard child of this domain. */
	domain_type *nsec3_wcard_child_cover;
	/* for the DS case we must answer on the parent side of zone cut */
	domain_type *nsec3_ds_parent_cover;
	/* the NSEC3 domain that has a hash-base32 <= than this dname. */
	/* or NULL (no smaller one within this zone)
	 * this variable is used to look up the NSEC3 record that matches
	 * or covers a given b64-encoded-hash-string domain name.
	 * The result of the lookup is stored in the *_cover variables.
	 * The variable makes it possible to perform a rbtree lookup for
	 * a name, then take this 'jump' to the previous element that contains
	 * an NSEC3 record, with hopefully the correct parameters. */
	domain_type *nsec3_lookup;
#endif
	uint32_t     number; /* Unique domain name number.  */

	/*
	 * This domain name exists (see wildcard clarification draft).
	 */
	unsigned     is_existing : 1;
	unsigned     is_apex : 1;
#ifdef NSEC3
	/* if the domain has an NSEC3 for it, use cover ptr to get it. */
	unsigned     nsec3_is_exact : 1;
	/* same but on parent side */
	unsigned     nsec3_ds_parent_is_exact : 1;
#endif
};

struct zone
{
	zone_type   *next;
	domain_type *apex;
	rrset_type  *soa_rrset;
	rrset_type  *soa_nx_rrset; /* see bug #103 */
	rrset_type  *ns_rrset;
#ifdef NSEC3
	rr_type	    *nsec3_soa_rr; /* rrset with SOA bit set */
	domain_type *nsec3_last; /* last domain with nsec3, wraps */
#endif
	struct zone_options *opts;
	uint32_t     number;
	uint8_t*     dirty; /* array of dirty-flags, per child */
	unsigned     is_secure : 1; /* zone uses DNSSEC */
	unsigned     updated : 1; /* zone SOA was updated */
	unsigned     is_ok : 1; /* zone has not expired. */
};

/* a RR in DNS */
struct rr {
	domain_type     *owner;
	rdata_atom_type *rdatas;
	uint32_t         ttl;
	uint16_t         type;
	uint16_t         klass;
	uint16_t         rdata_count;
};

/*
 * An RRset consists of at least one RR.  All RRs are from the same
 * zone.
 */
struct rrset
{
	rrset_type *next;
	zone_type  *zone;
	rr_type    *rrs;
	uint16_t    rr_count;
};

/*
 * The field used is based on the wireformat the atom is stored in.
 * The allowed wireformats are defined by the rdata_wireformat_type
 * enumeration.
 */
union rdata_atom
{
	/* RDATA_WF_COMPRESSED_DNAME, RDATA_WF_UNCOMPRESSED_DNAME */
	domain_type *domain;

	/* Default. */
	uint16_t    *data;
};

/*
 * Create a new domain_table containing only the root domain.
 */
domain_table_type *domain_table_create(region_type *region);

/*
 * Search the domain table for a match and the closest encloser.
 */
int domain_table_search(domain_table_type *table,
			const dname_type  *dname,
			domain_type      **closest_match,
			domain_type      **closest_encloser);

/*
 * The number of domains stored in the table (minimum is one for the
 * root domain).
 */
static inline uint32_t
domain_table_count(domain_table_type *table)
{
	return table->names_to_domains->count;
}

/*
 * Find the specified dname in the domain_table.  NULL is returned if
 * there is no exact match.
 */
domain_type *domain_table_find(domain_table_type *table,
			       const dname_type  *dname);

/*
 * Insert a domain name in the domain table.  If the domain name is
 * not yet present in the table it is copied and a new dname_info node
 * is created (as well as for the missing parent domain names, if
 * any).  Otherwise the domain_type that is already in the
 * domain_table is returned.
 */
domain_type *domain_table_insert(domain_table_type *table,
				 const dname_type  *dname);


/*
 * Iterate over all the domain names in the domain tree.
 */
typedef int (*domain_table_iterator_type)(domain_type *node,
					   void *user_data);

int domain_table_iterate(domain_table_type *table,
			  domain_table_iterator_type iterator,
			  void *user_data);

/*
 * Add an RRset to the specified domain.  Updates the is_existing flag
 * as required.
 */
void domain_add_rrset(domain_type *domain, rrset_type *rrset);

rrset_type *domain_find_rrset(domain_type *domain, zone_type *zone, uint16_t type);
rrset_type *domain_find_any_rrset(domain_type *domain, zone_type *zone);

zone_type *domain_find_zone(domain_type *domain);
zone_type *domain_find_parent_zone(zone_type *zone);

domain_type *domain_find_ns_rrsets(domain_type *domain, zone_type *zone, rrset_type **ns);

int domain_is_glue(domain_type *domain, zone_type *zone);

rrset_type *domain_find_non_cname_rrset(domain_type *domain, zone_type *zone);

domain_type *domain_wildcard_child(domain_type *domain);

int zone_is_secure(zone_type *zone);

static inline const dname_type *
domain_dname(domain_type *domain)
{
	return (const dname_type *) domain->node.key;
}

static inline domain_type *
domain_previous(domain_type *domain)
{
	rbnode_t *prev = rbtree_previous((rbnode_t *) domain);
	return prev == RBTREE_NULL ? NULL : (domain_type *) prev;
}

static inline domain_type *
domain_next(domain_type *domain)
{
	rbnode_t *next = rbtree_next((rbnode_t *) domain);
	return next == RBTREE_NULL ? NULL : (domain_type *) next;
}

/*
 * The type covered by the signature in the specified RRSIG RR.
 */
uint16_t rr_rrsig_type_covered(rr_type *rr);

typedef struct namedb namedb_type;
struct namedb
{
	region_type       *region;
	domain_table_type *domains;
	zone_type         *zones;
	size_t	  	  zone_count;
	char              *filename;
	FILE              *fd;
	/* the timestamp on the ixfr.db file */
	struct timeval	  diff_timestamp;
	/* the CRC on the nsd.db file and position of CRC in the db file */
	uint32_t	  crc;
	off_t		  crc_pos;
	/* if diff_skip=1, diff_pos contains the nsd.diff place to continue */
	uint8_t		  diff_skip;
	off_t		  diff_pos;
};

static inline int rdata_atom_is_domain(uint16_t type, size_t index);

static inline domain_type *
rdata_atom_domain(rdata_atom_type atom)
{
	return atom.domain;
}

static inline uint16_t
rdata_atom_size(rdata_atom_type atom)
{
	return *atom.data;
}

static inline uint8_t *
rdata_atom_data(rdata_atom_type atom)
{
	return (uint8_t *) (atom.data + 1);
}


/*
 * Find the zone for the specified DOMAIN in DB.
 */
zone_type *namedb_find_zone(namedb_type *db, domain_type *domain);

/* dbcreate.c */
struct namedb *namedb_new(const char *filename);
int namedb_save(struct namedb *db);
void namedb_discard(struct namedb *db);


/* dbaccess.c */
int namedb_lookup (struct namedb    *db,
		   const dname_type *dname,
		   domain_type     **closest_match,
		   domain_type     **closest_encloser);
/* pass number of children (to alloc in dirty array */
struct namedb *namedb_open(const char *filename, struct nsd_options* opt,
	size_t num_children);
void namedb_fd_close(struct namedb *db);
void namedb_close(struct namedb *db);

static inline int
rdata_atom_is_domain(uint16_t type, size_t index)
{
	const rrtype_descriptor_type *descriptor
		= rrtype_descriptor_by_type(type);
	return (index < descriptor->maximum
		&& (descriptor->wireformat[index] == RDATA_WF_COMPRESSED_DNAME
		    || descriptor->wireformat[index] == RDATA_WF_UNCOMPRESSED_DNAME));
}

static inline rdata_wireformat_type
rdata_atom_wireformat_type(uint16_t type, size_t index)
{
	const rrtype_descriptor_type *descriptor
		= rrtype_descriptor_by_type(type);
	assert(index < descriptor->maximum);
	return (rdata_wireformat_type) descriptor->wireformat[index];
}

static inline uint16_t
rrset_rrtype(rrset_type *rrset)
{
	assert(rrset);
	assert(rrset->rr_count > 0);
	return rrset->rrs[0].type;
}

static inline uint16_t
rrset_rrclass(rrset_type *rrset)
{
	assert(rrset);
	assert(rrset->rr_count > 0);
	return rrset->rrs[0].klass;
}


#endif
