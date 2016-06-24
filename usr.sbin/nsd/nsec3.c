/*
 * nsec3.c -- nsec3 handling.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#include "config.h"
#ifdef NSEC3
#include <stdio.h>
#include <stdlib.h>

#include "nsec3.h"
#include "iterated_hash.h"
#include "namedb.h"
#include "nsd.h"
#include "answer.h"
#include "udbzone.h"
#include "options.h"

#define NSEC3_RDATA_BITMAP 5

/* compare nsec3 hashes in nsec3 tree */
static int
cmp_hash_tree(const void* x, const void* y)
{
	const domain_type* a = (const domain_type*)x;
	const domain_type* b = (const domain_type*)y;
	return memcmp(a->nsec3->nsec3_hash, b->nsec3->nsec3_hash,
		NSEC3_HASH_LEN);
}

/* compare nsec3 hashes in nsec3 wc tree */
static int
cmp_wchash_tree(const void* x, const void* y)
{
	const domain_type* a = (const domain_type*)x;
	const domain_type* b = (const domain_type*)y;
	return memcmp(a->nsec3->nsec3_wc_hash, b->nsec3->nsec3_wc_hash,
		NSEC3_HASH_LEN);
}

/* compare nsec3 hashes in nsec3 ds tree */
static int
cmp_dshash_tree(const void* x, const void* y)
{
	const domain_type* a = (const domain_type*)x;
	const domain_type* b = (const domain_type*)y;
	return memcmp(a->nsec3->nsec3_ds_parent_hash,
		b->nsec3->nsec3_ds_parent_hash, NSEC3_HASH_LEN);
}

/* compare base32-encoded nsec3 hashes in nsec3 rr tree, they are
 * stored in the domain name of the node */
static int
cmp_nsec3_tree(const void* x, const void* y)
{
	const domain_type* a = (const domain_type*)x;
	const domain_type* b = (const domain_type*)y;
	/* labelcount + 32long label */
	assert(dname_name(a->dname)[0] == 32);
	assert(dname_name(b->dname)[0] == 32);
	return memcmp(dname_name(a->dname), dname_name(b->dname), 33);
}

void nsec3_zone_trees_create(struct region* region, zone_type* zone)
{
	if(!zone->nsec3tree)
		zone->nsec3tree = rbtree_create(region, cmp_nsec3_tree);
	if(!zone->hashtree)
		zone->hashtree = rbtree_create(region, cmp_hash_tree);
	if(!zone->wchashtree)
		zone->wchashtree = rbtree_create(region, cmp_wchash_tree);
	if(!zone->dshashtree)
		zone->dshashtree = rbtree_create(region, cmp_dshash_tree);
}

void nsec3_hash_tree_clear(struct zone* zone)
{
	hash_tree_clear(zone->nsec3tree);
	hash_tree_clear(zone->hashtree);
	hash_tree_clear(zone->wchashtree);
	hash_tree_clear(zone->dshashtree);
}

static void
detect_nsec3_params(rr_type* nsec3_apex,
	const unsigned char** salt, int* salt_len, int* iter)
{
	assert(salt && salt_len && iter);
	assert(nsec3_apex);
	*salt_len = rdata_atom_data(nsec3_apex->rdatas[3])[0];
	*salt = (unsigned char*)(rdata_atom_data(nsec3_apex->rdatas[3])+1);
	*iter = read_uint16(rdata_atom_data(nsec3_apex->rdatas[2]));
}

const dname_type *
nsec3_b32_create(region_type* region, zone_type* zone, unsigned char* hash)
{
	const dname_type* dname;
	char b32[SHA_DIGEST_LENGTH*2+1];
	b32_ntop(hash, SHA_DIGEST_LENGTH, b32, sizeof(b32));
	dname=dname_parse(region, b32);
	dname=dname_concatenate(region, dname, domain_dname(zone->apex));
	return dname;
}

void
nsec3_hash_and_store(zone_type* zone, const dname_type* dname, uint8_t* store)
{
	const unsigned char* nsec3_salt = NULL;
	int nsec3_saltlength = 0;
	int nsec3_iterations = 0;

	detect_nsec3_params(zone->nsec3_param, &nsec3_salt,
		&nsec3_saltlength, &nsec3_iterations);
	iterated_hash((unsigned char*)store, nsec3_salt, nsec3_saltlength,
		dname_name(dname), dname->name_size, nsec3_iterations);
}

#define STORE_HASH(x,y) memmove(domain->nsec3->x,y,NSEC3_HASH_LEN); domain->nsec3->have_##x =1;

/** find hash or create it and store it */
static void
nsec3_lookup_hash_and_wc(zone_type* zone, const dname_type* dname,
	domain_type* domain, region_type* tmpregion)
{
	const dname_type* wcard;
	if(domain->nsec3->have_nsec3_hash && domain->nsec3->have_nsec3_wc_hash) {
		return;
	}
	/* lookup failed; disk failure or so */
	nsec3_hash_and_store(zone, dname, domain->nsec3->nsec3_hash);
	domain->nsec3->have_nsec3_hash = 1;
	wcard = dname_parse(tmpregion, "*");
	wcard = dname_concatenate(tmpregion, wcard, dname);
	nsec3_hash_and_store(zone, wcard, domain->nsec3->nsec3_wc_hash);
	domain->nsec3->have_nsec3_wc_hash = 1;
}

static void
nsec3_lookup_hash_ds(zone_type* zone, const dname_type* dname,
	domain_type* domain)
{
	if(domain->nsec3->have_nsec3_ds_parent_hash) {
		return;
	}
	/* lookup failed; disk failure or so */
	nsec3_hash_and_store(zone, dname, domain->nsec3->nsec3_ds_parent_hash);
	domain->nsec3->have_nsec3_ds_parent_hash = 1;
}

static int
nsec3_has_soa(rr_type* rr)
{
	if(rdata_atom_size(rr->rdatas[NSEC3_RDATA_BITMAP]) >= 3 && /* has types in bitmap */
		rdata_atom_data(rr->rdatas[NSEC3_RDATA_BITMAP])[0] == 0 && /* first window = 0, */
		/* [1]: bitmap length must be >= 1 */
		/* [2]: bit[6] = SOA, thus mask first bitmap octet with 0x02 */
		rdata_atom_data(rr->rdatas[NSEC3_RDATA_BITMAP])[2]&0x02) { /* SOA bit set */
		return 1;
	}
	return 0;
}

static rr_type*
check_apex_soa(namedb_type* namedb, zone_type *zone)
{
	uint8_t h[NSEC3_HASH_LEN];
	domain_type* domain;
	const dname_type* hashed_apex, *dname = domain_dname(zone->apex);
	unsigned j;
	rrset_type* nsec3_rrset;
	region_type* tmpregion;

	nsec3_hash_and_store(zone, dname, h);
	tmpregion = region_create(xalloc, free);
	hashed_apex = nsec3_b32_create(tmpregion, zone, h);
	domain = domain_table_find(namedb->domains, hashed_apex);
	if(!domain) {
		log_msg(LOG_ERR, "%s NSEC3PARAM entry has no hash(apex).",
			domain_to_string(zone->apex));
		log_msg(LOG_ERR, "hash(apex)= %s",
			dname_to_string(hashed_apex, NULL));
		region_destroy(tmpregion);
		return NULL;
	}
	nsec3_rrset = domain_find_rrset(domain, zone, TYPE_NSEC3);
	if(!nsec3_rrset) {
		log_msg(LOG_ERR, "%s NSEC3PARAM entry: hash(apex) has no NSEC3 RRset.",
			domain_to_string(zone->apex));
		log_msg(LOG_ERR, "hash(apex)= %s",
			dname_to_string(hashed_apex, NULL));
		region_destroy(tmpregion);
		return NULL;
	}
	for(j=0; j<nsec3_rrset->rr_count; j++) {
		if(nsec3_has_soa(&nsec3_rrset->rrs[j])) {
			region_destroy(tmpregion);
			return &nsec3_rrset->rrs[j];
		}
	}
	log_msg(LOG_ERR, "%s NSEC3PARAM entry: hash(apex) NSEC3 has no SOA flag.",
		domain_to_string(zone->apex));
	log_msg(LOG_ERR, "hash(apex)= %s",
		dname_to_string(hashed_apex, NULL));
	region_destroy(tmpregion);
	return NULL;
}

static struct rr*
udb_zone_find_nsec3param(udb_base* udb, udb_ptr* uz, struct zone* z)
{
	udb_ptr urr;
	unsigned i;
	rrset_type* rrset = domain_find_rrset(z->apex, z, TYPE_NSEC3PARAM);
	if(!rrset) /* no NSEC3PARAM in mem */
		return NULL;
	udb_ptr_new(&urr, udb, &ZONE(uz)->nsec3param);
	if(!urr.data || RR(&urr)->len < 5) {
		/* no NSEC3PARAM in udb */
		udb_ptr_unlink(&urr, udb);
		return NULL;
	}
	/* find matching NSEC3PARAM RR in memory */
	for(i=0; i<rrset->rr_count; i++) {
		/* if this RR matches the udb RR then we are done */
		rdata_atom_type* rd = rrset->rrs[i].rdatas;
		if(rrset->rrs[i].rdata_count < 4) continue;
		if(RR(&urr)->wire[0] == rdata_atom_data(rd[0])[0] && /*alg*/
		   RR(&urr)->wire[1] == rdata_atom_data(rd[1])[0] && /*flg*/
		   RR(&urr)->wire[2] == rdata_atom_data(rd[2])[0] && /*iter*/
		   RR(&urr)->wire[3] == rdata_atom_data(rd[2])[1] &&
		   RR(&urr)->wire[4] == rdata_atom_data(rd[3])[0] && /*slen*/
		   RR(&urr)->len >= 5 + RR(&urr)->wire[4] &&
		   memcmp(RR(&urr)->wire+5, rdata_atom_data(rd[3])+1,
			rdata_atom_data(rd[3])[0]) == 0) {
			udb_ptr_unlink(&urr, udb);
			return &rrset->rrs[i];
		}
	}
	udb_ptr_unlink(&urr, udb);
	return NULL;
}

static struct rr*
db_find_nsec3param(struct zone* z, struct rr* avoid_rr)
{
	unsigned i;
	rrset_type* rrset = domain_find_rrset(z->apex, z, TYPE_NSEC3PARAM);
	if(!rrset) /* no NSEC3PARAM in mem */
		return NULL;
	/* find first nsec3param we can support (SHA1, no flags) */
	for(i=0; i<rrset->rr_count; i++) {
		rdata_atom_type* rd = rrset->rrs[i].rdatas;
		/* do not use the RR that is going to be deleted (in IXFR) */
		if(&rrset->rrs[i] == avoid_rr) continue;
		if(rrset->rrs[i].rdata_count < 4) continue;
		if(rdata_atom_data(rd[0])[0] == NSEC3_SHA1_HASH &&
			rdata_atom_data(rd[1])[0] == 0) {
			if(2 <= verbosity) {
				char str[MAX_RDLENGTH*2+16];
				char* p;
				p = str+snprintf(str, sizeof(str), "%u %u %u ",
					(unsigned)rdata_atom_data(rd[0])[0],
					(unsigned)rdata_atom_data(rd[1])[0],
					(unsigned)read_uint16(rdata_atom_data(rd[2])));
				if(rdata_atom_data(rd[3])[0] == 0)
					*p++ = '-';
				else {
					p += hex_ntop(rdata_atom_data(rd[3])+1,
						rdata_atom_data(rd[3])[0], p,
						sizeof(str)-strlen(str)-1);
				}
				*p = 0;
				VERBOSITY(2, (LOG_INFO, "rehash of zone %s with parameters %s",
					domain_to_string(z->apex), str));
			}
			return &rrset->rrs[i];
		}
	}
	return NULL;
}

void
nsec3_find_zone_param(struct namedb* db, struct zone* zone, udb_ptr* z,
	struct rr* avoid_rr)
{
	/* get nsec3param RR from udb */
	if(db->udb)
		zone->nsec3_param = udb_zone_find_nsec3param(db->udb, z, zone);
	/* no db, get from memory, avoid using the rr that is going to be
	 * deleted, avoid_rr */
	else	zone->nsec3_param = db_find_nsec3param(zone, avoid_rr);
}

/* check params ok for one RR */
static int
nsec3_rdata_params_ok(rdata_atom_type* prd, rdata_atom_type* rd)
{
	return (rdata_atom_data(rd[0])[0] ==
		rdata_atom_data(prd[0])[0] && /* hash algo */
	   rdata_atom_data(rd[2])[0] ==
		rdata_atom_data(prd[2])[0] && /* iterations 0 */
	   rdata_atom_data(rd[2])[1] ==
		rdata_atom_data(prd[2])[1] && /* iterations 1 */
	   rdata_atom_data(rd[3])[0] ==
		rdata_atom_data(prd[3])[0] && /* salt length */
	   memcmp(rdata_atom_data(rd[3])+1,
		rdata_atom_data(prd[3])+1, rdata_atom_data(rd[3])[0])
		== 0 );
}

int
nsec3_rr_uses_params(rr_type* rr, zone_type* zone)
{
	if(!rr || rr->rdata_count < 4)
		return 0;
	return nsec3_rdata_params_ok(zone->nsec3_param->rdatas, rr->rdatas);
}

int
nsec3_in_chain_count(domain_type* domain, zone_type* zone)
{
	rrset_type* rrset = domain_find_rrset(domain, zone, TYPE_NSEC3);
	unsigned i;
	int count = 0;
	if(!rrset || !zone->nsec3_param)
		return 0; /* no NSEC3s, none in the chain */
	for(i=0; i<rrset->rr_count; i++) {
		if(nsec3_rr_uses_params(&rrset->rrs[i], zone))
			count++;
	}
	return count;
}

struct domain*
nsec3_chain_find_prev(struct zone* zone, struct domain* domain)
{
	if(domain->nsec3 && domain->nsec3->nsec3_node.key) {
		/* see if there is a prev */
		rbnode_t* r = rbtree_previous(&domain->nsec3->nsec3_node);
		if(r != RBTREE_NULL) {
			/* found a previous, which is not the root-node in
			 * the prehash tree (and thus points to the tree) */
			return (domain_type*)r->key;
		}
	}
	if(zone->nsec3_last)
		return zone->nsec3_last;
	return NULL;
}

void
nsec3_clear_precompile(struct namedb* db, zone_type* zone)
{
	domain_type* walk;
	/* clear prehash items (there must not be items for other zones) */
	prehash_clear(db->domains);
	/* clear trees */
	hash_tree_clear(zone->nsec3tree);
	hash_tree_clear(zone->hashtree);
	hash_tree_clear(zone->wchashtree);
	hash_tree_clear(zone->dshashtree);
	/* wipe hashes */

	/* wipe precompile */
	walk = zone->apex;
	while(walk && domain_is_subdomain(walk, zone->apex)) {
		if(walk->nsec3) {
			if(nsec3_domain_part_of_zone(walk, zone)) {
				walk->nsec3->nsec3_node.key = NULL;
				walk->nsec3->nsec3_cover = NULL;
				walk->nsec3->nsec3_wcard_child_cover = NULL;
				walk->nsec3->nsec3_is_exact = 0;
				walk->nsec3->have_nsec3_hash = 0;
				walk->nsec3->have_nsec3_wc_hash = 0;
				walk->nsec3->hash_node.key = NULL;
				walk->nsec3->wchash_node.key = NULL;
			}
			if(!walk->parent ||
				nsec3_domain_part_of_zone(walk->parent, zone)) {
				walk->nsec3->nsec3_ds_parent_cover = NULL;
				walk->nsec3->nsec3_ds_parent_is_exact = 0;
				walk->nsec3->have_nsec3_ds_parent_hash = 0;
				walk->nsec3->dshash_node.key = NULL;
			}
		}
		walk = domain_next(walk);
	}
	zone->nsec3_last = NULL;
}

/* see if domain name is part of (existing names in) the nsec3 zone */
int
nsec3_domain_part_of_zone(domain_type* d, zone_type* z)
{
	while(d) {
		if(d->is_apex)
			return (z->apex == d); /* zonecut, if right zone*/
		d = d->parent;
	}
	return 0;
}

/* condition when a domain is precompiled */
int
nsec3_condition_hash(domain_type* d, zone_type* z)
{
	return d->is_existing && !domain_has_only_NSEC3(d, z) &&
		nsec3_domain_part_of_zone(d, z) && !domain_is_glue(d, z);
}

/* condition when a domain is ds precompiled */
int
nsec3_condition_dshash(domain_type* d, zone_type* z)
{
	return d->is_existing && !domain_has_only_NSEC3(d, z) &&
		(domain_find_rrset(d, z, TYPE_DS) ||
		domain_find_rrset(d, z, TYPE_NS)) && d != z->apex;
}

zone_type*
nsec3_tree_zone(namedb_type* db, domain_type* d)
{
	/* see nsec3_domain_part_of_zone; domains part of zone that has
	 * apex above them */
	/* this does not use the rrset->zone pointer because there may be
	 * no rrsets left at apex (no SOA), e.g. during IXFR */
	while(d) {
		if(d->is_apex) {
			/* we can try a SOA if its present (faster than tree)*/
			/* DNSKEY and NSEC3PARAM are also good indicators */
			rrset_type *rrset;
			for (rrset = d->rrsets; rrset; rrset = rrset->next)
				if (rrset_rrtype(rrset) == TYPE_SOA ||
					rrset_rrtype(rrset) == TYPE_DNSKEY ||
					rrset_rrtype(rrset) == TYPE_NSEC3PARAM)
					return rrset->zone;
			return namedb_find_zone(db, d->dname);
		}
		d = d->parent;
	}
	return NULL;
}

zone_type*
nsec3_tree_dszone(namedb_type* db, domain_type* d)
{
	/* the DStree does not contain nodes with d==z->apex */
	if(d->is_apex)
		d = d->parent;
	return nsec3_tree_zone(db, d);
}

int
nsec3_find_cover(zone_type* zone, uint8_t* hash, size_t hashlen,
	domain_type** result)
{
	rbnode_t* r = NULL;
	int exact;
	domain_type d;
	uint8_t n[48];

	/* nsec3tree is sorted by b32 encoded domain name of the NSEC3 */
	b32_ntop(hash, hashlen, (char*)(n+5), sizeof(n)-5);
	d.dname = (dname_type*)n;
	n[0] = 34; /* name_size */
	n[1] = 2; /* label_count */
	n[2] = 0; /* label_offset[0] */
	n[3] = 0; /* label_offset[1] */
	n[4] = 32; /* label-size[0] */

	assert(result);
	assert(zone->nsec3_param && zone->nsec3tree);

	exact = rbtree_find_less_equal(zone->nsec3tree, &d, &r);
	if(r) {
		*result = (domain_type*)r->key;
	} else {
		*result = zone->nsec3_last;
	}
	return exact;
}

void
nsec3_precompile_domain(struct namedb* db, struct domain* domain,
	struct zone* zone, region_type* tmpregion)
{
	domain_type* result = 0;
	int exact;
	allocate_domain_nsec3(db->domains, domain);

	/* hash it */
	nsec3_lookup_hash_and_wc(zone, domain_dname(domain), domain, tmpregion);

	/* add into tree */
	zone_add_domain_in_hash_tree(db->region, &zone->hashtree,
		cmp_hash_tree, domain, &domain->nsec3->hash_node);
	zone_add_domain_in_hash_tree(db->region, &zone->wchashtree,
		cmp_wchash_tree, domain, &domain->nsec3->wchash_node);

	/* lookup in tree cover ptr (or exact) */
	exact = nsec3_find_cover(zone, domain->nsec3->nsec3_hash,
		sizeof(domain->nsec3->nsec3_hash), &result);
	domain->nsec3->nsec3_cover = result;
	if(exact)
		domain->nsec3->nsec3_is_exact = 1;
	else	domain->nsec3->nsec3_is_exact = 0;

	/* find cover for *.domain for wildcard denial */
	exact = nsec3_find_cover(zone, domain->nsec3->nsec3_wc_hash,
		sizeof(domain->nsec3->nsec3_wc_hash), &result);
	domain->nsec3->nsec3_wcard_child_cover = result;
}

void
nsec3_precompile_domain_ds(struct namedb* db, struct domain* domain,
	struct zone* zone)
{
	domain_type* result = 0;
	int exact;
	allocate_domain_nsec3(db->domains, domain);

	/* hash it : it could have different hash parameters then the
	   other hash for this domain name */
	nsec3_lookup_hash_ds(zone, domain_dname(domain), domain);
	/* lookup in tree cover ptr (or exact) */
	exact = nsec3_find_cover(zone, domain->nsec3->nsec3_ds_parent_hash,
		sizeof(domain->nsec3->nsec3_ds_parent_hash), &result);
	if(exact)
		domain->nsec3->nsec3_ds_parent_is_exact = 1;
	else 	domain->nsec3->nsec3_ds_parent_is_exact = 0;
	domain->nsec3->nsec3_ds_parent_cover = result;
	/* add into tree */
	zone_add_domain_in_hash_tree(db->region, &zone->dshashtree,
		cmp_dshash_tree, domain, &domain->nsec3->dshash_node);
}

static void
parse_nsec3_name(const dname_type* dname, uint8_t* hash, size_t buflen)
{
	/* first label must be the match, */
	size_t lablen = (buflen-1) * 8 / 5;
	const uint8_t* wire = dname_name(dname);
	assert(lablen == 32 && buflen == NSEC3_HASH_LEN+1);
	/* labels of length 32 for SHA1, and must have space+1 for convert */
	if(wire[0] != lablen) {
		/* not NSEC3 */
		memset(hash, 0, buflen);
		return;
	}
	(void)b32_pton((char*)wire+1, hash, buflen);
}

void
nsec3_precompile_nsec3rr(namedb_type* db, struct domain* domain,
	struct zone* zone)
{
	allocate_domain_nsec3(db->domains, domain);
	/* add into nsec3tree */
	zone_add_domain_in_hash_tree(db->region, &zone->nsec3tree,
		cmp_nsec3_tree, domain, &domain->nsec3->nsec3_node);
	/* fixup the last in the zone */
	if(rbtree_last(zone->nsec3tree)->key == domain) {
		zone->nsec3_last = domain;
	}
}

void
nsec3_precompile_newparam(namedb_type* db, zone_type* zone)
{
	region_type* tmpregion = region_create(xalloc, free);
	domain_type* walk;
	time_t s = time(NULL);
	unsigned long n = 0, c = 0;

	/* add nsec3s of chain to nsec3tree */
	for(walk=zone->apex; walk && domain_is_subdomain(walk, zone->apex);
		walk = domain_next(walk)) {
		n++;
		if(nsec3_in_chain_count(walk, zone) != 0) {
			nsec3_precompile_nsec3rr(db, walk, zone);
		}
	}
	/* hash and precompile zone */
	for(walk=zone->apex; walk && domain_is_subdomain(walk, zone->apex);
		walk = domain_next(walk)) {
		if(nsec3_condition_hash(walk, zone)) {
			nsec3_precompile_domain(db, walk, zone, tmpregion);
			region_free_all(tmpregion);
		}
		if(nsec3_condition_dshash(walk, zone))
			nsec3_precompile_domain_ds(db, walk, zone);
		if(++c % ZONEC_PCT_COUNT == 0 && time(NULL) > s + ZONEC_PCT_TIME) {
			s = time(NULL);
			VERBOSITY(1, (LOG_INFO, "nsec3 %s %d %%",
				zone->opts->name,
				(int)(c*((unsigned long)100)/n)));
		}
	}
	region_destroy(tmpregion);
}

void
prehash_zone_complete(struct namedb* db, struct zone* zone)
{
	udb_ptr udbz;

	/* robust clear it */
	nsec3_clear_precompile(db, zone);
	/* find zone settings */

	assert(db && zone);
	if(db->udb) {
		if(!udb_zone_search(db->udb, &udbz, dname_name(domain_dname(
			zone->apex)), domain_dname(zone->apex)->name_size)) {
			udb_ptr_init(&udbz, db->udb); /* zero the ptr */
		}
	}
	nsec3_find_zone_param(db, zone, &udbz, NULL);
	if(!zone->nsec3_param || !check_apex_soa(db, zone)) {
		zone->nsec3_param = NULL;
		zone->nsec3_last = NULL;
		if(db->udb)
			udb_ptr_unlink(&udbz, db->udb);
		return;
	}
	if(db->udb)
		udb_ptr_unlink(&udbz, db->udb);
	nsec3_precompile_newparam(db, zone);
}

static void
init_lookup_key_hash_tree(domain_type* d, uint8_t* hash)
{ memcpy(d->nsec3->nsec3_hash, hash, NSEC3_HASH_LEN); }

static void
init_lookup_key_wc_tree(domain_type* d, uint8_t* hash)
{ memcpy(d->nsec3->nsec3_wc_hash, hash, NSEC3_HASH_LEN); }

static void
init_lookup_key_ds_tree(domain_type* d, uint8_t* hash)
{ memcpy(d->nsec3->nsec3_ds_parent_hash, hash, NSEC3_HASH_LEN); }

/* find first in the tree and true if the first to process it */
static int
process_first(rbtree_t* tree, uint8_t* hash, rbnode_t** p,
	void (*init)(domain_type*, uint8_t*))
{
	domain_type d;
	struct nsec3_domain_data n;
	if(!tree) {
		*p = RBTREE_NULL;
		return 0;
	}
	d.nsec3 = &n;
	init(&d, hash);
	if(rbtree_find_less_equal(tree, &d, p)) {
		/* found an exact match */
		return 1;
	}
	if(!*p) /* before first, go from first */
		*p = rbtree_first(tree);
	/* the inexact, smaller, match we found, does not itself need to
	 * be edited */
	else
		*p = rbtree_next(*p); /* if this becomes NULL, nothing to do */
	return 0;
}

/* set end pointer if possible */
static void
process_end(rbtree_t* tree, uint8_t* hash, rbnode_t** p,
	void (*init)(domain_type*, uint8_t*))
{
	domain_type d;
	struct nsec3_domain_data n;
	if(!tree) {
		*p = RBTREE_NULL;
		return;
	}
	d.nsec3 = &n;
	init(&d, hash);
	if(rbtree_find_less_equal(tree, &d, p)) {
		/* an exact match, fine, because this one does not get
		 * processed */
		return;
	}
	/* inexact element, but if NULL, until first element in tree */
	if(!*p) {
		*p = rbtree_first(tree);
		return;
	}
	/* inexact match, use next element, if possible, the smaller
	 * element is part of the range */
	*p = rbtree_next(*p);
	/* if next returns null, we go until the end of the tree */
}

/* prehash domains in hash range start to end */
static void
process_range(zone_type* zone, domain_type* start,
	domain_type* end, domain_type* nsec3)
{
	/* start NULL means from first in tree */
	/* end NULL means to last in tree */
	rbnode_t *p = RBTREE_NULL, *pwc = RBTREE_NULL, *pds = RBTREE_NULL;
	rbnode_t *p_end = RBTREE_NULL, *pwc_end = RBTREE_NULL, *pds_end = RBTREE_NULL;
	/* because the nodes are on the prehashlist, the domain->nsec3 is
	 * already allocated, and we need not allocate it here */
	/* set start */
	if(start) {
		uint8_t hash[NSEC3_HASH_LEN+1];
		parse_nsec3_name(domain_dname(start), hash, sizeof(hash));
		/* if exact match on first, set is_exact */
		if(process_first(zone->hashtree, hash, &p, init_lookup_key_hash_tree)) {
			((domain_type*)(p->key))->nsec3->nsec3_cover = nsec3;
			((domain_type*)(p->key))->nsec3->nsec3_is_exact = 1;
			p = rbtree_next(p);
		}
		(void)process_first(zone->wchashtree, hash, &pwc, init_lookup_key_wc_tree);
		if(process_first(zone->dshashtree, hash, &pds, init_lookup_key_ds_tree)){
			((domain_type*)(pds->key))->nsec3->
				nsec3_ds_parent_cover = nsec3;
			((domain_type*)(pds->key))->nsec3->
				nsec3_ds_parent_is_exact = 1;
			pds = rbtree_next(pds);
		}
	} else {
		if(zone->hashtree)
			p = rbtree_first(zone->hashtree);
		if(zone->wchashtree)
			pwc = rbtree_first(zone->wchashtree);
		if(zone->dshashtree)
			pds = rbtree_first(zone->dshashtree);
	}
	/* set end */
	if(end) {
		uint8_t hash[NSEC3_HASH_LEN+1];
		parse_nsec3_name(domain_dname(end), hash, sizeof(hash));
		process_end(zone->hashtree, hash, &p_end, init_lookup_key_hash_tree);
		process_end(zone->wchashtree, hash, &pwc_end, init_lookup_key_wc_tree);
		process_end(zone->dshashtree, hash, &pds_end, init_lookup_key_ds_tree);
	}

	/* precompile */
	while(p != RBTREE_NULL && p != p_end) {
		((domain_type*)(p->key))->nsec3->nsec3_cover = nsec3;
		((domain_type*)(p->key))->nsec3->nsec3_is_exact = 0;
		p = rbtree_next(p);
	}
	while(pwc != RBTREE_NULL && pwc != pwc_end) {
		((domain_type*)(pwc->key))->nsec3->
			nsec3_wcard_child_cover = nsec3;
		pwc = rbtree_next(pwc);
	}
	while(pds != RBTREE_NULL && pds != pds_end) {
		((domain_type*)(pds->key))->nsec3->
			nsec3_ds_parent_cover = nsec3;
		((domain_type*)(pds->key))->nsec3->
			nsec3_ds_parent_is_exact = 0;
		pds = rbtree_next(pds);
	}
}

/* prehash a domain from the prehash list */
static void
process_prehash_domain(domain_type* domain, zone_type* zone)
{
	/* in the hashtree, wchashtree, dshashtree walk through to next NSEC3
	 * and set precompile pointers to point to this domain (or is_exact),
	 * the first domain can be is_exact. If it is the last NSEC3, also
	 * process the initial part (before the first) */
	rbnode_t* nx;

	/* this domain is part of the prehash list and therefore the
	 * domain->nsec3 is allocated and need not be allocated here */
	assert(domain->nsec3 && domain->nsec3->nsec3_node.key);
	nx = rbtree_next(&domain->nsec3->nsec3_node);
	if(nx != RBTREE_NULL) {
		/* process until next nsec3 */
		domain_type* end = (domain_type*)nx->key;
		process_range(zone, domain, end, domain);
	} else {
		/* first is root, but then comes the first nsec3 */
		domain_type* first = (domain_type*)(rbtree_first(
			zone->nsec3tree)->key);
		/* last in zone */
		process_range(zone, domain, NULL, domain);
		/* also process before first in zone */
		process_range(zone, NULL, first, domain);
	}
}

void prehash_zone(struct namedb* db, struct zone* zone)
{
	domain_type* d;
	if(!zone->nsec3_param) {
		prehash_clear(db->domains);
		return;
	}
	/* process prehash list */
	for(d = db->domains->prehash_list; d; d = d->nsec3->prehash_next) {
		process_prehash_domain(d, zone);
	}
	/* clear prehash list */
	prehash_clear(db->domains);

	if(!check_apex_soa(db, zone)) {
		zone->nsec3_param = NULL;
		zone->nsec3_last = NULL;
	}
}

/* add the NSEC3 rrset to the query answer at the given domain */
static void
nsec3_add_rrset(struct query* query, struct answer* answer,
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
nsec3_add_nonexist_proof(struct query* query, struct answer* answer,
        struct domain* encloser, const dname_type* qname)
{
	uint8_t hash[NSEC3_HASH_LEN];
	const dname_type* to_prove;
	domain_type* cover=0;
	assert(encloser);
	/* if query=a.b.c.d encloser=c.d. then proof needed for b.c.d. */
	/* if query=a.b.c.d encloser=*.c.d. then proof needed for b.c.d. */
	to_prove = dname_partial_copy(query->region, qname,
		dname_label_match_count(qname, domain_dname(encloser))+1);
	/* generate proof that one label below closest encloser does not exist */
	nsec3_hash_and_store(query->zone, to_prove, hash);
	if(nsec3_find_cover(query->zone, hash, sizeof(hash), &cover))
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
	struct query* query, struct answer* answer,
	struct domain* closest_encloser, const dname_type* qname)
{
	if(!closest_encloser)
		return;
	/* prove that below closest encloser nothing exists */
	nsec3_add_nonexist_proof(query, answer, closest_encloser, qname);
	/* proof that closest encloser exists */
	if(closest_encloser->nsec3 && closest_encloser->nsec3->nsec3_is_exact)
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			closest_encloser->nsec3->nsec3_cover);
}

void
nsec3_answer_wildcard(struct query *query, struct answer *answer,
        struct domain *wildcard, const dname_type* qname)
{
	if(!wildcard)
		return;
	if(!query->zone->nsec3_param)
		return;
	nsec3_add_nonexist_proof(query, answer, wildcard, qname);
}

static void
nsec3_add_ds_proof(struct query *query, struct answer *answer,
	struct domain *domain, int delegpt)
{
	/* assert we are above the zone cut */
	assert(domain != query->zone->apex);
	if(domain->nsec3 && domain->nsec3->nsec3_ds_parent_is_exact) {
		/* use NSEC3 record from above the zone cut. */
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			domain->nsec3->nsec3_ds_parent_cover);
	} else if (!delegpt && domain->nsec3 && domain->nsec3->nsec3_is_exact) {
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			domain->nsec3->nsec3_cover);
	} else {
		/* prove closest provable encloser */
		domain_type* par = domain->parent;
		domain_type* prev_par = 0;

		while(par && (!par->nsec3 || !par->nsec3->nsec3_is_exact))
		{
			prev_par = par;
			par = par->parent;
		}
		assert(par); /* parent zone apex must be provable, thus this ends */
		if(!par->nsec3) return;
		nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
			par->nsec3->nsec3_cover);
		/* we took several steps to go to the provable parent, so
		   the one below it has no exact nsec3, disprove it.
		   disprove is easy, it has a prehashed cover ptr. */
		if(prev_par && prev_par->nsec3) {
			assert(prev_par != domain &&
				!prev_par->nsec3->nsec3_is_exact);
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				prev_par->nsec3->nsec3_cover);
		}
		/* add optout range from parent zone */
		/* note: no check of optout bit, resolver checks it */
		if(domain->nsec3)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				domain->nsec3->nsec3_ds_parent_cover);
	}
}

void
nsec3_answer_nodata(struct query* query, struct answer* answer,
	struct domain* original)
{
	if(!query->zone->nsec3_param)
		return;
	/* nodata when asking for secure delegation */
	if(query->qtype == TYPE_DS)
	{
		if(original == query->zone->apex) {
			/* DS at zone apex, but server not authoritative for parent zone */
			/* so answer at the child zone level */
			if(original->nsec3 && original->nsec3->nsec3_is_exact)
				nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
					original->nsec3->nsec3_cover);
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
		/* in other words: nsec3 matching closest encloser */
		if(original->parent && original->parent->nsec3 &&
			original->parent->nsec3->nsec3_is_exact)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				original->parent->nsec3->nsec3_cover);
		/* proof for wildcard itself */
		/* in other words: nsec3 matching source of synthesis */
		if(original->nsec3)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				original->nsec3->nsec3_cover);
	}
	else {	/* add nsec3 to prove rrset does not exist */
		if(original->nsec3) {
			if(!original->nsec3->nsec3_is_exact) {
				/* go up to an existing parent */
				while(original->parent && original->parent->nsec3 && !original->parent->nsec3->nsec3_is_exact)
					original = original->parent;
			}
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				original->nsec3->nsec3_cover);
			if(!original->nsec3->nsec3_is_exact) {
				if(original->parent && original->parent->nsec3 && original->parent->nsec3->nsec3_is_exact)
				    nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
					original->parent->nsec3->nsec3_cover);

			}
		}
	}
}

void
nsec3_answer_delegation(struct query *query, struct answer *answer)
{
	if(!query->zone->nsec3_param)
		return;
	nsec3_add_ds_proof(query, answer, query->delegation_domain, 1);
}

int
domain_has_only_NSEC3(struct domain* domain, struct zone* zone)
{
	/* check for only NSEC3/RRSIG */
	rrset_type* rrset = domain->rrsets;
	int nsec3_seen = 0;
	while(rrset)
	{
		if(!zone || rrset->zone == zone)
		{
			if(rrset->rrs[0].type == TYPE_NSEC3)
				nsec3_seen = 1;
			else if(rrset->rrs[0].type != TYPE_RRSIG)
				return 0;
		}
		rrset = rrset->next;
	}
	return nsec3_seen;
}

void
nsec3_answer_authoritative(struct domain** match, struct query *query,
	struct answer *answer, struct domain* closest_encloser,
	const dname_type* qname)
{
	if(!query->zone->nsec3_param)
		return;
	assert(match);
	/* there is a match, this has 1 RRset, which is NSEC3, but qtype is not. */
        /* !is_existing: no RR types exist at the QNAME, nor at any descendant of QNAME */
	if(*match && !(*match)->is_existing &&
#if 0
		query->qtype != TYPE_NSEC3 &&
#endif
		domain_has_only_NSEC3(*match, query->zone))
	{
		/* act as if the NSEC3 domain did not exist, name error */
		*match = 0;
		/* all nsec3s are directly below the apex, that is closest encloser */
		if(query->zone->apex->nsec3 &&
			query->zone->apex->nsec3->nsec3_is_exact)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				query->zone->apex->nsec3->nsec3_cover);
		/* disprove the nsec3 record. */
		if(closest_encloser->nsec3)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION, closest_encloser->nsec3->nsec3_cover);
		/* disprove a wildcard */
		if(query->zone->apex->nsec3)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				query->zone->apex->nsec3->nsec3_wcard_child_cover);
		if (domain_wildcard_child(query->zone->apex)) {
			/* wildcard exists below the domain */
			/* wildcard and nsec3 domain clash. server failure. */
			RCODE_SET(query->packet, RCODE_SERVFAIL);
		}
		return;
	}
	else if(*match && (*match)->is_existing &&
#if 0
		query->qtype != TYPE_NSEC3 &&
#endif
		(domain_has_only_NSEC3(*match, query->zone) ||
		!domain_find_any_rrset(*match, query->zone)))
	{
		/* this looks like a NSEC3 domain, but is actually an empty non-terminal. */
		nsec3_answer_nodata(query, answer, *match);
		return;
	}
	if(!*match) {
		/* name error, domain does not exist */
		nsec3_add_closest_encloser_proof(query, answer, closest_encloser,
			qname);
		if(closest_encloser->nsec3)
			nsec3_add_rrset(query, answer, AUTHORITY_SECTION,
				closest_encloser->nsec3->nsec3_wcard_child_cover);
	}
}

#endif /* NSEC3 */
