/*
 * namedb.c -- common namedb operations.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

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
	result->wildcard_child_closest_match = result;
	result->rrsets = NULL;
	result->number = 0;
#ifdef NSEC3
	result->nsec3_cover = NULL;
	result->nsec3_wcard_child_cover = NULL;
	result->nsec3_ds_parent_cover = NULL;
	result->nsec3_lookup = NULL;
	result->nsec3_is_exact = 0;
	result->nsec3_ds_parent_is_exact = 0;
#endif
	result->is_existing = 0;
	result->is_apex = 0;

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
	root->wildcard_child_closest_match = root;
	root->rrsets = NULL;
	root->number = 1; /* 0 is used for after header */
	root->is_existing = 0;
	root->is_apex = 0;
#ifdef NSEC3
	root->nsec3_is_exact = 0;
	root->nsec3_ds_parent_is_exact = 0;
	root->nsec3_cover = NULL;
	root->nsec3_wcard_child_cover = NULL;
	root->nsec3_ds_parent_cover = NULL;
	root->nsec3_lookup = NULL;
#endif

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
