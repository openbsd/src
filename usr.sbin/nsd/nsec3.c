/*
 * nsec3.c -- nsec3 handling.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#include <config.h>
#ifdef NSEC3
#include <stdio.h>
#include <stdlib.h>

#include "nsec3.h"
#include "iterated_hash.h"
#include "namedb.h"
#include "nsd.h"
#include "answer.h"

#define NSEC3_SHA1_HASH 1 /* same type code as DS hash */

/* detect is the latter rrset has the same hashalgo, iterations and salt
   as the base. Does not compare optout bit, or other rdata.
   base=NULL uses the zone soa_rr. */
static int nsec3_rrset_params_ok(rr_type* base, rrset_type* rrset);

static void
detect_nsec3_params(rr_type* nsec3_apex,
	const unsigned char** salt, int* salt_len, int* iter)
{
	/* always uses first NSEC3 record with SOA bit set */
	assert(salt && salt_len && iter);
	assert(nsec3_apex);
	*salt_len = rdata_atom_data(nsec3_apex->rdatas[3])[0];
	*salt = (unsigned char*)(rdata_atom_data(nsec3_apex->rdatas[3])+1);
	*iter = read_uint16(rdata_atom_data(nsec3_apex->rdatas[2]));
}

static const dname_type *
nsec3_hash_dname_param(region_type *region, zone_type *zone,
	const dname_type *dname, rr_type* param_rr)
{
	unsigned char hash[SHA_DIGEST_LENGTH];
	char b32[SHA_DIGEST_LENGTH*2+1];
	const unsigned char* nsec3_salt = NULL;
	int nsec3_saltlength = 0;
	int nsec3_iterations = 0;

	detect_nsec3_params(param_rr, &nsec3_salt,
		&nsec3_saltlength, &nsec3_iterations);
	iterated_hash(hash, nsec3_salt, nsec3_saltlength, dname_name(dname),
		dname->name_size, nsec3_iterations);
	b32_ntop(hash, sizeof(hash), b32, sizeof(b32));
	dname=dname_parse(region, b32);
	dname=dname_concatenate(region, dname, domain_dname(zone->apex));
	return dname;
}

const dname_type *
nsec3_hash_dname(region_type *region, zone_type *zone,
	const dname_type *dname)
{
	return nsec3_hash_dname_param(region, zone,
		dname, zone->nsec3_soa_rr);
}

static int
nsec3_has_soa(rr_type* rr)
{
	if(rdata_atom_size(rr->rdatas[5]) >= 3 && /* has types in bitmap */
		rdata_atom_data(rr->rdatas[5])[0] == 0 && /* first window = 0, */
						/* [1]: windowlen must be >= 1 */
		rdata_atom_data(rr->rdatas[5])[2]&0x02)  /* SOA bit set */
		return 1;
	return 0;
}

static rr_type*
find_zone_nsec3(namedb_type* namedb, zone_type *zone)
{
	size_t i;
	domain_type* domain;
	region_type* tmpregion;
	/* Check settings in NSEC3PARAM.
	   Hash algorithm must be OK. And a NSEC3 with soa bit
	   must map to the zone apex.  */
	rrset_type* paramset = domain_find_rrset(zone->apex, zone, TYPE_NSEC3PARAM);
	if(!paramset || !paramset->rrs || !paramset->rr_count)
		return 0;
	tmpregion = region_create(xalloc, free);
	for(i=0; i<paramset->rr_count; i++)
	{
		rr_type* rr = &paramset->rrs[i];
		const dname_type* hashed_apex;
		rrset_type* nsec3_rrset;
		size_t j;

		if(rdata_atom_data(rr->rdatas[0])[0] != NSEC3_SHA1_HASH) {
			log_msg(LOG_ERR, "%s NSEC3PARAM entry %d has unknown hash algo %d",
				dname_to_string(domain_dname(zone->apex), NULL), (int)i,
				rdata_atom_data(rr->rdatas[0])[0]);
			continue;
		}
		if(rdata_atom_data(rr->rdatas[1])[0] != 0) {
			/* draft-nsec3-09: NSEC3PARAM records with flags
			   field value other than zero MUST be ignored. */
			continue;
		}
		/* check hash of apex -> NSEC3 with soa bit on */
		hashed_apex = nsec3_hash_dname_param(tmpregion,
			zone, domain_dname(zone->apex), &paramset->rrs[i]);
		domain = domain_table_find(namedb->domains, hashed_apex);
		if(!domain) {
			log_msg(LOG_ERR, "%s NSEC3PARAM entry %d has no hash(apex).",
				dname_to_string(domain_dname(zone->apex), NULL), (int)i);
			log_msg(LOG_ERR, "hash(apex)= %s",
				dname_to_string(hashed_apex, NULL));
			continue;
		}
		nsec3_rrset = domain_find_rrset(domain, zone, TYPE_NSEC3);
		if(!nsec3_rrset) {
			log_msg(LOG_ERR, "%s NSEC3PARAM entry %d: hash(apex) has no NSEC3 RRset",
				dname_to_string(domain_dname(zone->apex), NULL), (int)i);
			continue;
		}
		/* find SOA bit enabled nsec3, with the same settings */
		for(j=0; j<nsec3_rrset->rr_count; j++)
		{
			const unsigned char *salt1, *salt2;
			int saltlen1, saltlen2, iter1, iter2;
			if(!nsec3_has_soa(&nsec3_rrset->rrs[j]))
				continue;
			/* check params OK. Ignores the optout bit. */
			detect_nsec3_params(rr, &salt1, &saltlen1, &iter1);
			detect_nsec3_params(&nsec3_rrset->rrs[j],
				&salt2, &saltlen2, &iter2);
			if(saltlen1 == saltlen2 && iter1 == iter2 &&
				rdata_atom_data(rr->rdatas[0])[0] == /* algo */
					rdata_atom_data(nsec3_rrset->rrs[j].rdatas[0])[0]
				&& memcmp(salt1, salt2, saltlen1) == 0) {
				/* found it */
				DEBUG(DEBUG_QUERY, 1, (LOG_INFO,
					"detected NSEC3 for zone %s saltlen=%d iter=%d",
					dname_to_string(domain_dname(
					zone->apex),0), saltlen2, iter2));
				region_destroy(tmpregion);
				return &nsec3_rrset->rrs[j];
			}
		}
		log_msg(LOG_ERR, "%s NSEC3PARAM entry %d: hash(apex) no NSEC3 with SOAbit",
			dname_to_string(domain_dname(zone->apex), NULL), (int)i);
	}
	region_destroy(tmpregion);
	return 0;
}

/* check that the rrset has an NSEC3 that uses the same parameters as the
   zone is using. Pass NSEC3 rrset, and zone must have nsec3_rrset set.
   if you pass NULL then 0 is returned. */
static int
nsec3_rrset_params_ok(rr_type* base, rrset_type* rrset)
{
	rdata_atom_type* prd;
	rdata_atom_type* rd;
	size_t i;
	if(!rrset)
		return 0; /* without rrset, no matching params either */
	assert(rrset && rrset->zone && (base || rrset->zone->nsec3_soa_rr));
	if(!base)
		base = rrset->zone->nsec3_soa_rr;
	prd = base->rdatas;
	for(i=0; i<rrset->rr_count; ++i)
	{
		rd = rrset->rrs[i].rdatas;
		assert(rrset->rrs[i].type == TYPE_NSEC3);
		if(rdata_atom_data(rd[0])[0] ==
			rdata_atom_data(prd[0])[0] && /* hash algo */
		   rdata_atom_data(rd[2])[0] ==
			rdata_atom_data(prd[2])[0] && /* iterations 0 */
		   rdata_atom_data(rd[2])[1] ==
			rdata_atom_data(prd[2])[1] && /* iterations 1 */
		   rdata_atom_data(rd[3])[0] ==
			rdata_atom_data(prd[3])[0] && /* salt length */
		   memcmp(rdata_atom_data(rd[3])+1,
			rdata_atom_data(prd[3])+1, rdata_atom_data(rd[3])[0])
			== 0 )
		{
			/* this NSEC3 matches nsec3 parameters from zone */
			return 1;
		}
	}
	return 0;
}

int
nsec3_find_cover(namedb_type* db, zone_type* zone,
	const dname_type* hashname, domain_type** result)
{
	rrset_type *rrset;
	domain_type *walk;
	domain_type *closest_match;
	domain_type *closest_encloser;
	int exact;

	assert(result);
	assert(zone->nsec3_soa_rr);

	exact = domain_table_search(
		db->domains, hashname, &closest_match, &closest_encloser);
	/* exact match of hashed domain name + it has an NSEC3? */
	if(exact &&
	   nsec3_rrset_params_ok(NULL,
	   	domain_find_rrset(closest_encloser, zone, TYPE_NSEC3))) {
		*result = closest_encloser;
		assert(*result != 0);
		return 1;
	}

	/* find covering NSEC3 record, lexicographically before the closest match */
	/* use nsec3_lookup to jumpstart the search */
	walk = closest_match->nsec3_lookup;
	rrset = 0;
	while(walk && dname_is_subdomain(domain_dname(walk), domain_dname(zone->apex)))
	{
		if(nsec3_rrset_params_ok(NULL,
			domain_find_rrset(walk, zone, TYPE_NSEC3))) {
			/* this rrset is OK NSEC3, exit while */
			rrset = domain_find_rrset(walk, zone, TYPE_NSEC3);
			break;
		}
		walk = domain_previous(walk);
	}
	if(rrset)
		*result = walk;
	else 	{
		/*
		 * There are no NSEC3s before the closest match.
		 * so the hash name is before the first NSEC3 record in the zone.
		 * use last NSEC3, which covers the wraparound in hash space
		 *
		 * Since the zone has an NSEC3 with the SOA bit set for NSEC3 to turn on,
		 * there is also a last nsec3, so find_cover always assigns *result!=0.
		 */
		*result = zone->nsec3_last;
	}
	assert(*result != 0);
	return 0;
}

static void
prehash_domain(namedb_type* db, zone_type* zone,
	domain_type* domain, region_type* region)
{
	/* find it */
	domain_type* result = 0;
	const dname_type *wcard, *wcard_child, *hashname;
	int exact;

	if(!zone->nsec3_soa_rr)
	{
		/* set to 0 (in case NSEC3 removed after an update) */
		domain->nsec3_is_exact = 0;
		domain->nsec3_cover = NULL;
		domain->nsec3_wcard_child_cover = NULL;
		return;
	}

	hashname = nsec3_hash_dname(region, zone, domain_dname(domain));
	exact = nsec3_find_cover(db, zone, hashname, &result);
	domain->nsec3_cover = result;
	if(exact)
		domain->nsec3_is_exact = 1;
	else	domain->nsec3_is_exact = 0;

	/* find cover for *.domain for wildcard denial */
	wcard = dname_parse(region, "*");
	wcard_child = dname_concatenate(region, wcard, domain_dname(domain));
	hashname = nsec3_hash_dname(region, zone, wcard_child);
	exact = nsec3_find_cover(db, zone, hashname, &result);
	domain->nsec3_wcard_child_cover = result;

	if(exact && !domain_wildcard_child(domain))
	{
		/* We found an exact match for the *.domain NSEC3 hash,
		 * but the domain wildcard child (*.domain) does not exist.
		 * Thus there is a hash collision. It will cause servfail
		 * for NXdomain queries below this domain.
		 */
		log_msg(LOG_WARNING, "prehash: collision of wildcard "
			"denial for %s. Sign zone with different salt "
			"to remove collision.",
			dname_to_string(domain_dname(domain),0));
	}
}

static void
prehash_ds(namedb_type* db, zone_type* zone,
	domain_type* domain, region_type* region)
{
	domain_type* result = 0;
	const dname_type* hashname;
	int exact;

	if(!zone->nsec3_soa_rr) {
		domain->nsec3_ds_parent_is_exact = 0;
		domain->nsec3_ds_parent_cover = NULL;
		return;
	}

	/* hash again, other zone could have different hash parameters */
	hashname = nsec3_hash_dname(region, zone, domain_dname(domain));
	exact = nsec3_find_cover(db, zone, hashname, &result);
	if(exact)
		domain->nsec3_ds_parent_is_exact = 1;
	else 	domain->nsec3_ds_parent_is_exact = 0;
	domain->nsec3_ds_parent_cover = result;
}

static void
prehash_zone(struct namedb* db, struct zone* zone)
{
	domain_type *walk;
	domain_type *last_nsec3_node;
	region_type *temp_region;
	assert(db && zone);

	/* find zone settings */
	zone->nsec3_soa_rr = find_zone_nsec3(db, zone);
	if(!zone->nsec3_soa_rr) {
		zone->nsec3_last = 0;
		return;
	}

	temp_region = region_create(xalloc, free);

	/* go through entire zone and setup nsec3_lookup speedup */
	walk = zone->apex;
	last_nsec3_node = NULL;
	/* since we walk in sorted order, we pass all NSEC3s in sorted
	   order and we can set the lookup ptrs */
	while(walk && dname_is_subdomain(
		domain_dname(walk), domain_dname(zone->apex)))
	{
		zone_type* z = domain_find_zone(walk);
		if(z && z==zone)
		{
			if(domain_find_rrset(walk, zone, TYPE_NSEC3))
				last_nsec3_node = walk;
			walk->nsec3_lookup = last_nsec3_node;
		}
		walk = domain_next(walk);
	}
	zone->nsec3_last = last_nsec3_node;

	/* go through entire zone */
	walk = zone->apex;
	while(walk && dname_is_subdomain(
		domain_dname(walk), domain_dname(zone->apex)))
	{
		zone_type* z;
		if(!walk->is_existing || domain_has_only_NSEC3(walk, zone)) {
			walk->nsec3_cover = NULL;
			walk->nsec3_wcard_child_cover = NULL;
			walk = domain_next(walk);
			continue;
		}
		z = domain_find_zone(walk);
		if(z && z==zone && !domain_is_glue(walk, zone))
		{
			prehash_domain(db, zone, walk, temp_region);
			region_free_all(temp_region);
		}
		/* prehash the DS (parent zone) */
		if(domain_find_rrset(walk, zone, TYPE_DS) ||
			(domain_find_rrset(walk, zone, TYPE_NS) &&
			 walk != zone->apex))
		{
			assert(walk != zone->apex /* DS must be above zone cut */);
			prehash_ds(db, zone, walk, temp_region);
			region_free_all(temp_region);
		}
		walk = domain_next(walk);
	}
	region_destroy(temp_region);
}

void
prehash(struct namedb* db, int updated_only)
{
	zone_type *z;
	time_t end, start = time(NULL);
	int count = 0;
	for(z = db->zones; z; z = z->next)
	{
		if(!updated_only || z->updated) {
			prehash_zone(db, z);
			if(z->nsec3_soa_rr)
				count++;
		}
	}
	end = time(NULL);
	if(count > 0)
		VERBOSITY(1, (LOG_INFO, "nsec3-prepare took %d "
		"seconds for %d zones.", (int)(end-start), count));
}

/* add the NSEC3 rrset to the query answer at the given domain */
static void
nsec3_add_rrset(struct query *query, struct answer *answer,
	rr_section_type section, struct domain* domain)
{
	if(domain) {
		rrset_type* rrset = domain_find_rrset(domain, query->zone, TYPE_NSEC3);
		if(rrset)
			answer_add_rrset(answer, section, domain, rrset);
	}
}

/* this routine does hashing at query-time. slow. */
static void
nsec3_add_nonexist_proof(struct query *query, struct answer *answer,
        struct domain *encloser, struct namedb* db, const dname_type* qname)
{
	const dname_type *to_prove, *hashed;
	domain_type *cover=0;
	assert(encloser);
	/* if query=a.b.c.d encloser=c.d. then proof needed for b.c.d. */
	/* if query=a.b.c.d encloser=*.c.d. then proof needed for b.c.d. */
	to_prove = dname_partial_copy(query->region, qname,
		dname_label_match_count(qname, domain_dname(encloser))+1);
	/* generate proof that one label below closest encloser does not exist */
	hashed = nsec3_hash_dname(query->region, query->zone, to_prove);
	if(nsec3_find_cover(db, query->zone, hashed, &cover))
	{
		/* exact match, hash collision */
		/* the hashed name of the query corresponds to an existing name. */
		log_msg(LOG_ERR, "nsec3 hash collision for name=%s",
			dname_to_string(to_prove, NULL));
		RCODE_SET(query->packet, RCODE_SERVFAIL);
		return;
	}
	else
	{
		/* cover proves the qname does not exist */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION, cover);
	}
}

static void
nsec3_add_closest_encloser_proof(
	struct query *query, struct answer *answer,
	struct domain *closest_encloser, struct namedb* db,
	const dname_type* qname)
{
	if(!closest_encloser)
		return;
	/* prove that below closest encloser nothing exists */
	nsec3_add_nonexist_proof(query, answer, closest_encloser, db, qname);
	/* proof that closest encloser exists */
	if(closest_encloser->nsec3_is_exact)
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			closest_encloser->nsec3_cover);
}

void
nsec3_answer_wildcard(struct query *query, struct answer *answer,
        struct domain *wildcard, struct namedb* db, const dname_type* qname)
{
	if(!wildcard)
		return;
	if(!query->zone->nsec3_soa_rr)
		return;
	nsec3_add_nonexist_proof(query, answer, wildcard, db, qname);
}

static void
nsec3_add_ds_proof(struct query *query, struct answer *answer,
	struct domain *domain, int delegpt)
{
	/* assert we are above the zone cut */
	assert(domain != query->zone->apex);
	if(domain->nsec3_ds_parent_is_exact) {
		/* use NSEC3 record from above the zone cut. */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			domain->nsec3_ds_parent_cover);
	} else if (!delegpt && domain->nsec3_is_exact) {
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			domain->nsec3_cover);
	} else {
		/* prove closest provable encloser */
		domain_type* par = domain->parent;
		domain_type* prev_par = 0;

		while(par && !par->nsec3_is_exact)
		{
			prev_par = par;
			par = par->parent;
		}
		assert(par); /* parent zone apex must be provable, thus this ends */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			par->nsec3_cover);
		/* we took several steps to go to the provable parent, so
		   the one below it has no exact nsec3, disprove it.
		   disprove is easy, it has a prehashed cover ptr. */
		if(prev_par) {
			assert(prev_par != domain && !prev_par->nsec3_is_exact);
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				prev_par->nsec3_cover);
		}
		/* add optout range from parent zone */
		/* note: no check of optout bit, resolver checks it */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			domain->nsec3_ds_parent_cover);
	}
}

void
nsec3_answer_nodata(struct query *query, struct answer *answer,
	struct domain *original)
{
	if(!query->zone->nsec3_soa_rr)
		return;
	/* nodata when asking for secure delegation */
	if(query->qtype == TYPE_DS)
	{
		if(original == query->zone->apex) {
			/* DS at zone apex, but server not authoritative for parent zone */
			/* so answer at the child zone level */
			if(original->nsec3_is_exact)
				nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
					original->nsec3_cover);
			return;
		}
		/* query->zone must be the parent zone */
		nsec3_add_ds_proof(query, answer, original, 0);
	}
	/* the nodata is result from a wildcard match */
	else if (original==original->wildcard_child_closest_match
		&& label_is_wildcard(dname_name(domain_dname(original)))) {
		/* denial for wildcard is already there */
		/* add parent proof to have a closest encloser proof for wildcard parent */
		if(original->parent && original->parent->nsec3_is_exact)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				original->parent->nsec3_cover);
		/* proof for wildcard itself */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			original->nsec3_cover);
	}
	else {	/* add nsec3 to prove rrset does not exist */
		if(original->nsec3_is_exact)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				original->nsec3_cover);
	}
}

void
nsec3_answer_delegation(struct query *query, struct answer *answer)
{
	if(!query->zone->nsec3_soa_rr)
		return;
	nsec3_add_ds_proof(query, answer, query->delegation_domain, 1);
}

int
domain_has_only_NSEC3(struct domain* domain, struct zone* zone)
{
	/* check for only NSEC3/RRSIG */
	rrset_type* rrset = domain->rrsets;
	int nsec3_seen = 0, rrsig_seen = 0;
	while(rrset)
	{
		if(!zone || rrset->zone == zone)
		{
			if(rrset->rrs[0].type == TYPE_NSEC3)
				nsec3_seen = 1;
			else if(rrset->rrs[0].type == TYPE_RRSIG)
				rrsig_seen = 1;
			else
				return 0;
		}
		rrset = rrset->next;
	}
	return nsec3_seen;
}

void
nsec3_answer_authoritative(struct domain** match, struct query *query,
	struct answer *answer, struct domain* closest_encloser,
	struct namedb* db, const dname_type* qname)
{
	if(!query->zone->nsec3_soa_rr)
		return;
	assert(match);
	/* there is a match, this has 1 RRset, which is NSEC3, but qtype is not. */
	if(*match &&
#if 0
		query->qtype != TYPE_NSEC3 &&
#endif
		domain_has_only_NSEC3(*match, query->zone))
	{
		/* act as if the NSEC3 domain did not exist, name error */
		*match = 0;
		/* all nsec3s are directly below the apex, that is closest encloser */
		if(query->zone->apex->nsec3_is_exact)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				query->zone->apex->nsec3_cover);
		/* disprove the nsec3 record. */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION, closest_encloser->nsec3_cover);
		/* disprove a wildcard */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION, query->zone->apex->
			nsec3_wcard_child_cover);
		if (domain_wildcard_child(query->zone->apex)) {
			/* wildcard exists below the domain */
			/* wildcard and nsec3 domain clash. server failure. */
			RCODE_SET(query->packet, RCODE_SERVFAIL);
		}
		return;
	}
	if(!*match) {
		/* name error, domain does not exist */
		nsec3_add_closest_encloser_proof(query, answer, closest_encloser,
			db, qname);
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			closest_encloser->nsec3_wcard_child_cover);
	}
}

#endif /* NSEC3 */
