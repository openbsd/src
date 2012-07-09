/*
 * namedb.c -- common namedb operations.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "namedb.h"


static domain_type *
allocate_domain_info(domain_table_type *table,
		     const dname_type *dname,
		     domain_type *parent)
{
	domain_type *result;

	assert(table);
	assert(dname);
	assert(parent);

	result = (domain_type *) region_alloc(table->region,
					      sizeof(domain_type));
	result->node.key = dname_partial_copy(
		table->region, dname, domain_dname(parent)->label_count + 1);
	result->parent = parent;
	result->nextdiff = NULL;
	result->wildcard_child_closest_match = result;
	result->rrsets = NULL;
	result->number = 0;
#ifdef NSEC3
	result->nsec3_cover = NULL;
#ifdef FULL_PREHASH
	result->nsec3_wcard_child_cover = NULL;
	result->nsec3_ds_parent_cover = NULL;
	result->nsec3_lookup = NULL;
	result->nsec3_is_exact = 0;
	result->nsec3_ds_parent_is_exact = 0;
#endif /* FULL_PREHASH */
#endif /* NSEC3 */
	result->is_existing = 0;
	result->is_apex = 0;
	result->has_SOA = 0;

	return result;
}

domain_table_type *
domain_table_create(region_type *region)
{
	const dname_type *origin;
	domain_table_type *result;
	domain_type *root;

	assert(region);

	origin = dname_make(region, (uint8_t *) "", 0);

	root = (domain_type *) region_alloc(region, sizeof(domain_type));
	root->node.key = origin;
	root->parent = NULL;
	root->nextdiff = NULL;
	root->wildcard_child_closest_match = root;
	root->rrsets = NULL;
	root->number = 1; /* 0 is used for after header */
	root->is_existing = 0;
	root->is_apex = 0;
	root->has_SOA = 0;
#ifdef NSEC3
	root->nsec3_cover = NULL;
#ifdef FULL_PREHASH
	root->nsec3_is_exact = 0;
	root->nsec3_ds_parent_is_exact = 0;
	root->nsec3_wcard_child_cover = NULL;
	root->nsec3_ds_parent_cover = NULL;
	root->nsec3_lookup = NULL;
#endif /* FULL_PREHASH */
#endif /* NSEC3 */

	result = (domain_table_type *) region_alloc(region,
						    sizeof(domain_table_type));
	result->region = region;
	result->names_to_domains = rbtree_create(
		region, (int (*)(const void *, const void *)) dname_compare);
	rbtree_insert(result->names_to_domains, (rbnode_t *) root);

	result->root = root;

	return result;
}

int
domain_table_search(domain_table_type *table,
		   const dname_type   *dname,
		   domain_type       **closest_match,
		   domain_type       **closest_encloser)
{
	int exact;
	uint8_t label_match_count;

	assert(table);
	assert(dname);
	assert(closest_match);
	assert(closest_encloser);

	exact = rbtree_find_less_equal(table->names_to_domains, dname, (rbnode_t **) closest_match);
	assert(*closest_match);

	*closest_encloser = *closest_match;

	if (!exact) {
		label_match_count = dname_label_match_count(
			domain_dname(*closest_encloser),
			dname);
		assert(label_match_count < dname->label_count);
		while (label_match_count < domain_dname(*closest_encloser)->label_count) {
			(*closest_encloser) = (*closest_encloser)->parent;
			assert(*closest_encloser);
		}
	}

	return exact;
}

domain_type *
domain_table_find(domain_table_type *table,
		  const dname_type *dname)
{
	domain_type *closest_match;
	domain_type *closest_encloser;
	int exact;

	exact = domain_table_search(
		table, dname, &closest_match, &closest_encloser);
	return exact ? closest_encloser : NULL;
}


domain_type *
domain_table_insert(domain_table_type *table,
		    const dname_type  *dname)
{
	domain_type *closest_match;
	domain_type *closest_encloser;
	domain_type *result;
	int exact;

	assert(table);
	assert(dname);

	exact = domain_table_search(
		table, dname, &closest_match, &closest_encloser);
	if (exact) {
		result = closest_encloser;
	} else {
		assert(domain_dname(closest_encloser)->label_count < dname->label_count);

		/* Insert new node(s).  */
		do {
			result = allocate_domain_info(table,
						      dname,
						      closest_encloser);
			rbtree_insert(table->names_to_domains, (rbnode_t *) result);
			result->number = table->names_to_domains->count;

			/*
			 * If the newly added domain name is larger
			 * than the parent's current
			 * wildcard_child_closest_match but smaller or
			 * equal to the wildcard domain name, update
			 * the parent's wildcard_child_closest_match
			 * field.
			 */
			if (label_compare(dname_name(domain_dname(result)),
					  (const uint8_t *) "\001*") <= 0
			    && dname_compare(domain_dname(result),
					     domain_dname(closest_encloser->wildcard_child_closest_match)) > 0)
			{
				closest_encloser->wildcard_child_closest_match
					= result;
			}
			closest_encloser = result;
		} while (domain_dname(closest_encloser)->label_count < dname->label_count);
	}

	return result;
}

int
domain_table_iterate(domain_table_type *table,
		    domain_table_iterator_type iterator,
		    void *user_data)
{
	const void *dname;
	void *node;
	int error = 0;

	assert(table);

	RBTREE_WALK(table->names_to_domains, dname, node) {
		error += iterator((domain_type *) node, user_data);
	}

	return error;
}


void
domain_add_rrset(domain_type *domain, rrset_type *rrset)
{
#if 0 	/* fast */
	rrset->next = domain->rrsets;
	domain->rrsets = rrset;
#else
	/* preserve ordering, add at end */
	rrset_type** p = &domain->rrsets;
	while(*p)
		p = &((*p)->next);
	*p = rrset;
	rrset->next = 0;
#endif

	while (domain && !domain->is_existing) {
		domain->is_existing = 1;
		domain = domain->parent;
	}
}


rrset_type *
domain_find_rrset(domain_type *domain, zone_type *zone, uint16_t type)
{
	rrset_type *result = domain->rrsets;

	while (result) {
		if (result->zone == zone && rrset_rrtype(result) == type) {
			return result;
		}
		result = result->next;
	}
	return NULL;
}

rrset_type *
domain_find_any_rrset(domain_type *domain, zone_type *zone)
{
	rrset_type *result = domain->rrsets;

	while (result) {
		if (result->zone == zone) {
			return result;
		}
		result = result->next;
	}
	return NULL;
}

zone_type *
domain_find_zone(domain_type *domain)
{
	rrset_type *rrset;
	while (domain) {
		for (rrset = domain->rrsets; rrset; rrset = rrset->next) {
			if (rrset_rrtype(rrset) == TYPE_SOA) {
				return rrset->zone;
			}
		}
		domain = domain->parent;
	}
	return NULL;
}

#ifndef FULL_PREHASH
domain_type *
domain_find_zone_apex(domain_type *domain) {
	while (domain != NULL) {
		if (domain->has_SOA != 0)
			return domain;
		domain = domain->parent;
	}
	return NULL;
}
#endif /* !FULL_PREHASH */

zone_type *
domain_find_parent_zone(zone_type *zone)
{
	rrset_type *rrset;

	assert(zone);

	for (rrset = zone->apex->rrsets; rrset; rrset = rrset->next) {
		if (rrset->zone != zone && rrset_rrtype(rrset) == TYPE_NS) {
			return rrset->zone;
		}
	}
	return NULL;
}

domain_type *
domain_find_ns_rrsets(domain_type *domain, zone_type *zone, rrset_type **ns)
{
	while (domain && domain != zone->apex) {
		*ns = domain_find_rrset(domain, zone, TYPE_NS);
		if (*ns)
			return domain;
		domain = domain->parent;
	}

	*ns = NULL;
	return NULL;
}

int
domain_is_glue(domain_type *domain, zone_type *zone)
{
	rrset_type *unused;
	domain_type *ns_domain = domain_find_ns_rrsets(domain, zone, &unused);
	return (ns_domain != NULL &&
		domain_find_rrset(ns_domain, zone, TYPE_SOA) == NULL);
}

domain_type *
domain_wildcard_child(domain_type *domain)
{
	domain_type *wildcard_child;

	assert(domain);
	assert(domain->wildcard_child_closest_match);

	wildcard_child = domain->wildcard_child_closest_match;
	if (wildcard_child != domain
	    && label_is_wildcard(dname_name(domain_dname(wildcard_child))))
	{
		return wildcard_child;
	} else {
		return NULL;
	}
}

int
zone_is_secure(zone_type *zone)
{
	assert(zone);
	return zone->is_secure;
}

uint16_t
rr_rrsig_type_covered(rr_type *rr)
{
	assert(rr->type == TYPE_RRSIG);
	assert(rr->rdata_count > 0);
	assert(rdata_atom_size(rr->rdatas[0]) == sizeof(uint16_t));

	return ntohs(* (uint16_t *) rdata_atom_data(rr->rdatas[0]));
}

zone_type *
namedb_find_zone(namedb_type *db, domain_type *domain)
{
	zone_type *zone;

	for (zone = db->zones; zone; zone = zone->next) {
		if (zone->apex == domain)
			break;
	}

	return zone;
}

rrset_type *
domain_find_non_cname_rrset(domain_type *domain, zone_type *zone)
{
	/* find any rrset type that is not allowed next to a CNAME */
	/* nothing is allowed next to a CNAME, except RRSIG, NSEC, NSEC3 */
	rrset_type *result = domain->rrsets;

	while (result) {
		if (result->zone == zone && /* here is the list of exceptions*/
			rrset_rrtype(result) != TYPE_CNAME &&
			rrset_rrtype(result) != TYPE_RRSIG &&
			rrset_rrtype(result) != TYPE_NXT &&
			rrset_rrtype(result) != TYPE_SIG &&
			rrset_rrtype(result) != TYPE_NSEC &&
			rrset_rrtype(result) != TYPE_NSEC3 ) {
			return result;
		}
		result = result->next;
	}
	return NULL;
}

/**
 * Create namedb.
 *
 */
struct namedb *
namedb_create(void)
{
	struct namedb *db = NULL;
	region_type *region = NULL;
#ifdef NSEC3
#ifndef FULL_PREHASH
	region_type *nsec3_region = NULL;
	region_type *nsec3_mod_region = NULL;
#endif /* !FULL_PREHASH */
#endif /* NSEC3 */

#ifdef USE_MMAP_ALLOC
	region = region_create_custom(mmap_alloc, mmap_free,
		MMAP_ALLOC_CHUNK_SIZE, MMAP_ALLOC_LARGE_OBJECT_SIZE,
		MMAP_ALLOC_INITIAL_CLEANUP_SIZE, 1);
#else /* !USE_MMAP_ALLOC */
	region = region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1);
#endif /* !USE_MMAP_ALLOC */
	if (region == NULL)
		return NULL;

#ifdef NSEC3
#ifndef FULL_PREHASH
#ifdef USE_MMAP_ALLOC
	nsec3_region = region_create_custom(mmap_alloc, mmap_free,
		MMAP_ALLOC_CHUNK_SIZE, MMAP_ALLOC_LARGE_OBJECT_SIZE,
		MMAP_ALLOC_INITIAL_CLEANUP_SIZE, 1);
#else /* !USE_MMAP_ALLOC */
	nsec3_region = region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1);
#endif /* !USE_MMAP_ALLOC */
	if (nsec3_region == NULL) {
		region_destroy(region);
		return NULL;
	}
#ifdef USE_MMAP_ALLOC
	nsec3_mod_region = region_create_custom(mmap_alloc, mmap_free,
		MMAP_ALLOC_CHUNK_SIZE, MMAP_ALLOC_LARGE_OBJECT_SIZE,
		MMAP_ALLOC_INITIAL_CLEANUP_SIZE, 1);
#else /* !USE_MMAP_ALLOC */
	nsec3_mod_region = region_create_custom(xalloc, free,
		DEFAULT_CHUNK_SIZE, DEFAULT_LARGE_OBJECT_SIZE,
		DEFAULT_INITIAL_CLEANUP_SIZE, 1);
#endif /* !USE_MMAP_ALLOC */
	if (nsec3_mod_region == NULL) {
		region_destroy(region);
		region_destroy(nsec3_region);
		return NULL;
	}
#endif /* !FULL_PREHASH */
#endif /* NSEC3 */

	/* Make a new structure... */
	db = (namedb_type *) region_alloc(region, sizeof(namedb_type));
	db->region = region;
#ifdef NSEC3
#ifndef FULL_PREHASH
	db->nsec3_region = nsec3_region;
	db->nsec3_mod_region = nsec3_mod_region;
	db->nsec3_mod_domains = NULL;
#endif /* !FULL_PREHASH */
#endif /* NSEC3 */
	db->domains = domain_table_create(region);
	db->zones = NULL;
	db->zone_count = 0;
	db->filename = NULL;
	db->fd = NULL;
	db->crc = ~0;
	db->crc_pos = 0;
	db->diff_skip = 0;
	db->diff_pos = 0;
	return db;
}

/**
 * Destroy namedb.
 *
 */
void
namedb_destroy(struct namedb *db)
{
#ifdef NSEC3
#ifndef FULL_PREHASH
	region_destroy(db->nsec3_mod_region);
	db->nsec3_mod_region = NULL;
	db->nsec3_mod_domains = NULL;
	region_destroy(db->nsec3_region);
	db->nsec3_region = NULL;
#endif /* !FULL_PREHASH */
#endif /* NSEC3 */
	region_destroy(db->region);
}


#ifdef NSEC3
#ifndef FULL_PREHASH
int
zone_nsec3_domains_create(struct namedb *db, struct zone *zone)
{
	if ((db == NULL) || (zone == NULL))
		return EINVAL;
	if (zone->nsec3_domains != NULL)
		return 0;
	zone->nsec3_domains = rbtree_create(db->nsec3_region,
					    dname_compare);
	if (zone->nsec3_domains == NULL)
		return ENOMEM;
	return 0;
}

int
zone_nsec3_domains_destroy(struct namedb *db, struct zone *zone)
{
	rbnode_t *node;
	if ((db == NULL) || (zone == NULL))
		return EINVAL;
	if (zone->nsec3_domains == NULL)
		return 0;

	node = rbtree_postorder_first(zone->nsec3_domains->root);
	while (node != RBTREE_NULL) {
		struct nsec3_domain *nsec3_domain =
			(struct nsec3_domain *) node;
		node = rbtree_postorder_next(node);

		if (nsec3_domain->covers != NULL) {
			nsec3_domain->covers->nsec3_cover = NULL;
		}
		region_recycle(db->nsec3_region, nsec3_domain,
			sizeof(*nsec3_domain));
	}
	region_recycle(db->nsec3_region, zone->nsec3_domains,
		sizeof(*(zone->nsec3_domains)));
	zone->nsec3_domains = NULL;
	return 0;
}


int
namedb_add_nsec3_domain(struct namedb *db, struct domain *domain,
	struct zone *zone)
{
	struct nsec3_domain *nsec3_domain;
	if (zone->nsec3_domains == NULL)
		return 0;
	nsec3_domain = (struct nsec3_domain *) region_alloc(db->nsec3_region,
		sizeof(*nsec3_domain));
	if (nsec3_domain == NULL)
		return ENOMEM;
	nsec3_domain->node.key = domain_dname(domain);
	nsec3_domain->nsec3_domain = domain;
	nsec3_domain->covers = NULL;
	if (rbtree_insert(zone->nsec3_domains, (rbnode_t *) nsec3_domain) == NULL) {
		region_recycle(db->nsec3_region, nsec3_domain, sizeof(*nsec3_domain));
	}
	return 0;
}


int
namedb_del_nsec3_domain(struct namedb *db, struct domain *domain,
	struct zone *zone)
{
	rbnode_t *node;
	struct nsec3_domain *nsec3_domain;
	int error = 0;

	if (zone->nsec3_domains == NULL)
		return 0;

	node = rbtree_delete(zone->nsec3_domains, domain_dname(domain));
	if (node == NULL)
		return 0;

	nsec3_domain = (struct nsec3_domain *) node;
	if (nsec3_domain->covers != NULL) {
		/*
		 * It is possible that this NSEC3 domain was modified
		 * due to the addition/deletion of another NSEC3 domain.
		 * Make sure it gets added to the NSEC3 list later by
		 * making sure it's covered domain is added to the
		 * NSEC3 mod list. S64#3441
		 */
		error = namedb_add_nsec3_mod_domain(db, nsec3_domain->covers);
		nsec3_domain->covers->nsec3_cover = NULL;
		nsec3_domain->covers = NULL;
	}
	region_recycle(db->nsec3_region, nsec3_domain, sizeof(*nsec3_domain));
	return error;
}


int
namedb_nsec3_mod_domains_create(struct namedb *db)
{
	if (db == NULL)
		return EINVAL;
	namedb_nsec3_mod_domains_destroy(db);

	db->nsec3_mod_domains = rbtree_create(db->nsec3_mod_region, dname_compare);
	if (db->nsec3_mod_domains == NULL)
		return ENOMEM;
	return 0;
}


int
namedb_nsec3_mod_domains_destroy(struct namedb *db)
{
	if (db == NULL)
		return EINVAL;
	if (db->nsec3_mod_domains == NULL)
		return 0;
	region_free_all(db->nsec3_mod_region);
	db->nsec3_mod_domains = NULL;
	return 0;
}

int
namedb_add_nsec3_mod_domain(struct namedb *db, struct domain *domain)
{
	struct nsec3_mod_domain *nsec3_mod_domain;
	nsec3_mod_domain = (struct nsec3_mod_domain *)
		region_alloc(db->nsec3_mod_region, sizeof(*nsec3_mod_domain));
	if (nsec3_mod_domain == NULL) {
		log_msg(LOG_ERR,
			"memory allocation failure on modified domain");
		return ENOMEM;
	}
	nsec3_mod_domain->node.key = domain_dname(domain);
	nsec3_mod_domain->domain = domain;

	if (rbtree_insert(db->nsec3_mod_domains, (rbnode_t *) nsec3_mod_domain) == NULL) {
		region_recycle(db->nsec3_mod_region, nsec3_mod_domain,
			sizeof(*nsec3_mod_domain));
	}
	return 0;
}
#endif /* !FULL_PREHASH */
#endif /* NSEC3 */
