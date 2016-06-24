/*
 * namedb.h -- nsd(8) internal namespace database definitions
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef _NAMEDB_H_
#define	_NAMEDB_H_

#include <stdio.h>

#include "dname.h"
#include "dns.h"
#include "radtree.h"
#include "rbtree.h"
struct zone_options;
struct nsd_options;
struct udb_base;
struct udb_ptr;
struct nsd;

typedef union rdata_atom rdata_atom_type;
typedef struct rrset rrset_type;
typedef struct rr rr_type;

/*
 * A domain name table supporting fast insert and search operations.
 */
typedef struct domain_table domain_table_type;
typedef struct domain domain_type;
typedef struct zone zone_type;
typedef struct namedb namedb_type;

struct domain_table
{
	region_type* region;
	struct radtree *nametree;
	domain_type* root;
	/* ptr to biggest domain.number and last in list.
	 * the root is the lowest and first in the list. */
	domain_type *numlist_last;
#ifdef NSEC3
	/* the prehash list, start of the list */
	domain_type* prehash_list;
#endif /* NSEC3 */
};

#ifdef NSEC3
struct nsec3_domain_data {
	/* (if nsec3 chain complete) always the covering nsec3 record */
	domain_type* nsec3_cover;
	/* the nsec3 that covers the wildcard child of this domain. */
	domain_type* nsec3_wcard_child_cover;
	/* for the DS case we must answer on the parent side of zone cut */
	domain_type* nsec3_ds_parent_cover;
	/* NSEC3 domains to prehash, prev and next on the list or cleared */
	domain_type* prehash_prev, *prehash_next;
	/* entry in the nsec3tree (for NSEC3s in the chain in use) */
	rbnode_t nsec3_node;
	/* entry in the hashtree (for precompiled domains) */
	rbnode_t hash_node;
	/* entry in the wchashtree (the wildcard precompile) */
	rbnode_t wchash_node;
	/* entry in the dshashtree (the parent ds precompile) */
	rbnode_t dshash_node;

	/* nsec3 hash */
	uint8_t nsec3_hash[NSEC3_HASH_LEN];
	/* nsec3 hash of wildcard before this name */
	uint8_t nsec3_wc_hash[NSEC3_HASH_LEN];
	/* parent-side DS hash */
	uint8_t nsec3_ds_parent_hash[NSEC3_HASH_LEN];
	/* if the nsec3 has is available */
	unsigned  have_nsec3_hash : 1;
	unsigned  have_nsec3_wc_hash : 1;
	unsigned  have_nsec3_ds_parent_hash : 1;
	/* if the domain has an NSEC3 for it, use cover ptr to get it. */
	unsigned     nsec3_is_exact : 1;
	/* same but on parent side */
	unsigned     nsec3_ds_parent_is_exact : 1;
};
#endif /* NSEC3 */

struct domain
{
	struct radnode* rnode;
	const dname_type* dname;
	domain_type* parent;
	domain_type* wildcard_child_closest_match;
	rrset_type* rrsets;
#ifdef NSEC3
	struct nsec3_domain_data* nsec3;
#endif
	/* double-linked list sorted by domain.number */
	domain_type* numlist_prev, *numlist_next;
	size_t     number; /* Unique domain name number.  */
	size_t     usage; /* number of ptrs to this from RRs(in rdata) and
			     from zone-apex pointers, also the root has one
			     more to make sure it cannot be deleted. */

	/*
	 * This domain name exists (see wildcard clarification draft).
	 */
	unsigned     is_existing : 1;
	unsigned     is_apex : 1;
};

struct zone
{
	struct radnode *node; /* this entry in zonetree */
	domain_type* apex;
	rrset_type*  soa_rrset;
	rrset_type*  soa_nx_rrset; /* see bug #103 */
	rrset_type*  ns_rrset;
#ifdef NSEC3
	rr_type* nsec3_param; /* NSEC3PARAM RR of chain in use or NULL */
	domain_type* nsec3_last; /* last domain with nsec3, wraps */
	/* in these trees, the root contains an elem ptr to the radtree* */
	rbtree_t* nsec3tree; /* tree with relevant NSEC3 domains */
	rbtree_t* hashtree; /* tree, hashed NSEC3precompiled domains */
	rbtree_t* wchashtree; /* tree, wildcard hashed domains */
	rbtree_t* dshashtree; /* tree, ds-parent-hash domains */
#endif
	struct zone_options* opts;
	char*        filename; /* set if read from file, which file */
	char*        logstr; /* set for zone xfer, the log string */
	struct timespec mtime; /* time of last modification */
	unsigned     zonestatid; /* array index for zone stats */
	unsigned     is_secure : 1; /* zone uses DNSSEC */
	unsigned     is_ok : 1; /* zone has not expired. */
	unsigned     is_changed : 1; /* zone was changed by AXFR */
};

/* a RR in DNS */
struct rr {
	domain_type*     owner;
	rdata_atom_type* rdatas;
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
	rrset_type* next;
	zone_type*  zone;
	rr_type*    rrs;
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
	domain_type* domain;

	/* Default. */
	uint16_t*    data;
};

/*
 * Create a new domain_table containing only the root domain.
 */
domain_table_type *domain_table_create(region_type *region);

/*
 * Search the domain table for a match and the closest encloser.
 */
int domain_table_search(domain_table_type* table,
			const dname_type* dname,
			domain_type      **closest_match,
			domain_type      **closest_encloser);

/*
 * The number of domains stored in the table (minimum is one for the
 * root domain).
 */
static inline uint32_t
domain_table_count(domain_table_type* table)
{
	return table->nametree->count;
}

/*
 * Find the specified dname in the domain_table.  NULL is returned if
 * there is no exact match.
 */
domain_type* domain_table_find(domain_table_type* table,
			       const dname_type* dname);

/*
 * Insert a domain name in the domain table.  If the domain name is
 * not yet present in the table it is copied and a new dname_info node
 * is created (as well as for the missing parent domain names, if
 * any).  Otherwise the domain_type that is already in the
 * domain_table is returned.
 */
domain_type *domain_table_insert(domain_table_type *table,
				 const dname_type  *dname);

/* put domain into nsec3 hash space tree */
void zone_add_domain_in_hash_tree(region_type* region, rbtree_t** tree,
	int (*cmpf)(const void*, const void*), domain_type* domain,
	rbnode_t* node);
void zone_del_domain_in_hash_tree(rbtree_t* tree, rbnode_t* node);
void hash_tree_clear(rbtree_t* tree);
void hash_tree_delete(region_type* region, rbtree_t* tree);
void prehash_clear(domain_table_type* table);
void prehash_add(domain_table_type* table, domain_type* domain);
void prehash_del(domain_table_type* table, domain_type* domain);
int domain_is_prehash(domain_table_type* table, domain_type* domain);

/*
 * Add an RRset to the specified domain.  Updates the is_existing flag
 * as required.
 */
void domain_add_rrset(domain_type* domain, rrset_type* rrset);

rrset_type* domain_find_rrset(domain_type* domain, zone_type* zone, uint16_t type);
rrset_type* domain_find_any_rrset(domain_type* domain, zone_type* zone);

zone_type* domain_find_zone(namedb_type* db, domain_type* domain);
zone_type* domain_find_parent_zone(zone_type* zone);

domain_type* domain_find_ns_rrsets(domain_type* domain, zone_type* zone, rrset_type **ns);
/* find DNAME rrset in domain->parent or higher and return that domain */
domain_type * find_dname_above(domain_type* domain, zone_type* zone);

int domain_is_glue(domain_type* domain, zone_type* zone);

rrset_type* domain_find_non_cname_rrset(domain_type* domain, zone_type* zone);

domain_type* domain_wildcard_child(domain_type* domain);
domain_type *domain_previous_existing_child(domain_type* domain);

int zone_is_secure(zone_type* zone);

static inline const dname_type *
domain_dname(domain_type* domain)
{
	return domain->dname;
}

static inline domain_type *
domain_previous(domain_type* domain)
{
	struct radnode* prev = radix_prev(domain->rnode);
	return prev == NULL ? NULL : (domain_type*)prev->elem;
}

static inline domain_type *
domain_next(domain_type* domain)
{
	struct radnode* next = radix_next(domain->rnode);
	return next == NULL ? NULL : (domain_type*)next->elem;
}

/* easy comparison for subdomain, true if d1 is subdomain of d2. */
static inline int domain_is_subdomain(domain_type* d1, domain_type* d2)
{ return dname_is_subdomain(domain_dname(d1), domain_dname(d2)); }
/* easy printout, to static buffer of dname_to_string, fqdn. */
static inline const char* domain_to_string(domain_type* domain)
{ return dname_to_string(domain_dname(domain), NULL); }

/*
 * The type covered by the signature in the specified RRSIG RR.
 */
uint16_t rr_rrsig_type_covered(rr_type* rr);

struct namedb
{
	region_type*       region;
	domain_table_type* domains;
	struct radtree*    zonetree;
	struct udb_base*   udb;
	/* the timestamp on the ixfr.db file */
	struct timeval	  diff_timestamp;
	/* if diff_skip=1, diff_pos contains the nsd.diff place to continue */
	uint8_t		  diff_skip;
	off_t		  diff_pos;
};

static inline int rdata_atom_is_domain(uint16_t type, size_t index);
static inline int rdata_atom_is_literal_domain(uint16_t type, size_t index);

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


/* Find the zone for the specified dname in DB. */
zone_type *namedb_find_zone(namedb_type *db, const dname_type *dname);
/*
 * Delete a domain name from the domain table.  Removes dname_info node.
 * Only deletes if usage is 0, has no rrsets and no children.  Checks parents
 * for deletion as well.  Adjusts numberlist(domain.number), and 
 * wcard_child closest match.
 */
void domain_table_deldomain(namedb_type* db, domain_type* domain);


/** dbcreate.c */
int udb_write_rr(struct udb_base* udb, struct udb_ptr* z, rr_type* rr);
void udb_del_rr(struct udb_base* udb, struct udb_ptr* z, rr_type* rr);
int write_zone_to_udb(struct udb_base* udb, zone_type* zone,
	struct timespec* mtime, const char* file_str);
/** marshal rdata into buffer, must be MAX_RDLENGTH in size */
size_t rr_marshal_rdata(rr_type* rr, uint8_t* rdata, size_t sz);
/* dbaccess.c */
int namedb_lookup (struct namedb* db,
		   const dname_type* dname,
		   domain_type     **closest_match,
		   domain_type     **closest_encloser);
/* pass number of children (to alloc in dirty array */
struct namedb *namedb_open(const char *filename, struct nsd_options* opt);
void namedb_close_udb(struct namedb* db);
void namedb_close(struct namedb* db);
void namedb_check_zonefiles(struct nsd* nsd, struct nsd_options* opt,
	struct udb_base* taskudb, struct udb_ptr* last_task);
void namedb_check_zonefile(struct nsd* nsd, struct udb_base* taskudb,
	struct udb_ptr* last_task, struct zone_options* zo);
/** zone one zonefile into memory and revert on parse error, write to udb */
void namedb_read_zonefile(struct nsd* nsd, struct zone* zone,
	struct udb_base* taskudb, struct udb_ptr* last_task);
void apex_rrset_checks(struct namedb* db, rrset_type* rrset,
	domain_type* domain);
zone_type* namedb_zone_create(namedb_type* db, const dname_type* dname,
        struct zone_options* zopt);
void namedb_zone_delete(namedb_type* db, zone_type* zone);
void namedb_write_zonefile(struct nsd* nsd, struct zone_options* zopt);
void namedb_write_zonefiles(struct nsd* nsd, struct nsd_options* options);
int create_dirs(const char* path);
int file_get_mtime(const char* file, struct timespec* mtime, int* nonexist);
void allocate_domain_nsec3(domain_table_type *table, domain_type *result);

static inline int
rdata_atom_is_domain(uint16_t type, size_t index)
{
	const rrtype_descriptor_type *descriptor
		= rrtype_descriptor_by_type(type);
	return (index < descriptor->maximum
		&& (descriptor->wireformat[index] == RDATA_WF_COMPRESSED_DNAME
		    || descriptor->wireformat[index] == RDATA_WF_UNCOMPRESSED_DNAME));
}

static inline int
rdata_atom_is_literal_domain(uint16_t type, size_t index)
{
	const rrtype_descriptor_type *descriptor
		= rrtype_descriptor_by_type(type);
	return (index < descriptor->maximum
		&& (descriptor->wireformat[index] == RDATA_WF_LITERAL_DNAME));
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
rrset_rrtype(rrset_type* rrset)
{
	assert(rrset);
	assert(rrset->rr_count > 0);
	return rrset->rrs[0].type;
}

static inline uint16_t
rrset_rrclass(rrset_type* rrset)
{
	assert(rrset);
	assert(rrset->rr_count > 0);
	return rrset->rrs[0].klass;
}

#endif
