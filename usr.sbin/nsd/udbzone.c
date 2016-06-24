/*
 * udbzone -- store zone and rrset information in udb file.
 *
 * Copyright (c) 2011, NLnet Labs.  See LICENSE for license.
 */
#include "config.h"
#include "udbzone.h"
#include "util.h"
#include "iterated_hash.h"
#include "dns.h"
#include "dname.h"
#include "difffile.h"
#include <string.h>

/** delete the zone plain its own data */
static void
udb_zone_delete_plain(udb_base* udb, udb_ptr* zone)
{
	udb_ptr dtree;
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);
	udb_zone_clear(udb, zone);
	udb_rptr_zero(&ZONE(zone)->node, udb);
	udb_rptr_zero(&ZONE(zone)->nsec3param, udb);
	udb_rptr_zero(&ZONE(zone)->log_str, udb);
	udb_rptr_zero(&ZONE(zone)->file_str, udb);
	udb_ptr_new(&dtree, udb, &ZONE(zone)->domains);
	udb_rptr_zero(&ZONE(zone)->domains, udb);
	udb_radix_tree_delete(udb, &dtree);
	udb_ptr_free_space(zone, udb,
		sizeof(struct zone_d)+ZONE(zone)->namelen);
}

int
udb_dns_init_file(udb_base* udb)
{
	udb_ptr ztree;
	if(!udb_radix_tree_create(udb, &ztree)) {
		return 0;
	}
	udb_base_set_userdata(udb, ztree.data);
	udb_ptr_unlink(&ztree, udb);
	return 1;
}

void
udb_dns_deinit_file(udb_base* udb)
{
	udb_ptr ztree;
	udb_ptr z;
	udb_ptr_new(&ztree, udb, udb_base_get_userdata(udb));
	if(udb_ptr_is_null(&ztree)) {
		return;
	}
	assert(udb_ptr_get_type(&ztree) == udb_chunk_type_radtree);
	/* delete all zones */
	for(udb_radix_first(udb, &ztree, &z); z.data; udb_radix_next(udb, &z)){
		udb_ptr zone;
		udb_ptr_new(&zone, udb, &RADNODE(&z)->elem);
		udb_rptr_zero(&RADNODE(&z)->elem, udb);
		udb_zone_delete_plain(udb, &zone);
	}
	udb_ptr_unlink(&z, udb);

	udb_base_set_userdata(udb, 0);
	udb_radix_tree_delete(udb, &ztree);
}

int
udb_zone_create(udb_base* udb, udb_ptr* result, const uint8_t* dname,
	size_t dlen)
{
	udb_ptr ztree, z, node, dtree;
	udb_ptr_new(&ztree, udb, udb_base_get_userdata(udb));
	assert(udb_ptr_get_type(&ztree) == udb_chunk_type_radtree);
	udb_ptr_init(result, udb);
	if(udb_zone_search(udb, &z, dname, dlen)) {
		udb_ptr_unlink(&ztree, udb);
		udb_ptr_unlink(&z, udb);
		/* duplicate */
		return 0;
	}
	if(!udb_ptr_alloc_space(&z, udb, udb_chunk_type_zone,
		sizeof(struct zone_d)+dlen)) {
		udb_ptr_unlink(&ztree, udb);
		/* failed alloc */
		return 0;
	}
	/* init the zone object */
	udb_rel_ptr_init(&ZONE(&z)->node);
	udb_rel_ptr_init(&ZONE(&z)->domains);
	udb_rel_ptr_init(&ZONE(&z)->nsec3param);
	udb_rel_ptr_init(&ZONE(&z)->log_str);
	udb_rel_ptr_init(&ZONE(&z)->file_str);
	ZONE(&z)->rrset_count = 0;
	ZONE(&z)->rr_count = 0;
	ZONE(&z)->expired = 0;
	ZONE(&z)->mtime = 0;
	ZONE(&z)->mtime_nsec = 0;
	ZONE(&z)->namelen = dlen;
	memmove(ZONE(&z)->name, dname, dlen);
	if(!udb_radix_tree_create(udb, &dtree)) {
		udb_ptr_free_space(&z, udb, sizeof(struct zone_d)+dlen);
		udb_ptr_unlink(&ztree, udb);
		/* failed alloc */
		return 0;
	}
	udb_rptr_set_ptr(&ZONE(&z)->domains, udb, &dtree);

	/* insert it */
	if(!udb_radname_insert(udb, &ztree, dname, dlen, &z, &node)) {
		udb_ptr_free_space(&z, udb, sizeof(struct zone_d)+dlen);
		udb_ptr_unlink(&ztree, udb);
		udb_radix_tree_delete(udb, &dtree);
		udb_ptr_unlink(&dtree, udb);
		/* failed alloc */
		return 0;
	}
	udb_rptr_set_ptr(&ZONE(&z)->node, udb, &node);
	udb_ptr_set_ptr(result, udb, &z);
	udb_ptr_unlink(&z, udb);
	udb_ptr_unlink(&dtree, udb);
	udb_ptr_unlink(&ztree, udb);
	udb_ptr_unlink(&node, udb);
	return 1;
}

/** delete an RR */
static void
rr_delete(udb_base* udb, udb_ptr* rr)
{
	assert(udb_ptr_get_type(rr) == udb_chunk_type_rr);
	udb_rptr_zero(&RR(rr)->next, udb);
	udb_ptr_free_space(rr, udb, sizeof(struct rr_d)+RR(rr)->len);
}

/** delete an rrset */
static void
rrset_delete(udb_base* udb, udb_ptr* rrset)
{
	udb_ptr rr, n;
	assert(udb_ptr_get_type(rrset) == udb_chunk_type_rrset);

	/* free RRs */
	udb_ptr_new(&rr, udb, &RRSET(rrset)->rrs);
	udb_ptr_init(&n, udb);
	udb_rptr_zero(&RRSET(rrset)->rrs, udb);
	while(!udb_ptr_is_null(&rr)) {
		udb_ptr_set_rptr(&n, udb, &RR(&rr)->next);
		rr_delete(udb, &rr);
		udb_ptr_set_ptr(&rr, udb, &n);
		udb_ptr_zero(&n, udb);
	}
	udb_ptr_unlink(&n, udb);
	udb_ptr_unlink(&rr, udb);

	udb_rptr_zero(&RRSET(rrset)->next, udb);
	udb_ptr_free_space(rrset, udb, sizeof(struct rrset_d));
}

/** clear a domain of its rrsets, rrs */
static void
domain_clear(udb_base* udb, udb_ptr* d)
{
	udb_ptr rrset, n;
	assert(udb_ptr_get_type(d) == udb_chunk_type_domain);
	udb_ptr_new(&rrset, udb, &DOMAIN(d)->rrsets);
	udb_ptr_init(&n, udb);
	udb_rptr_zero(&DOMAIN(d)->rrsets, udb);
	while(!udb_ptr_is_null(&rrset)) {
		udb_ptr_set_rptr(&n, udb, &RRSET(&rrset)->next);
		rrset_delete(udb, &rrset);
		udb_ptr_set_ptr(&rrset, udb, &n);
		udb_ptr_zero(&n, udb);
	}
	udb_ptr_unlink(&n, udb);
	udb_ptr_unlink(&rrset, udb);
}

/** delete a domain and all its rrsets, rrs */
static void
domain_delete(udb_base* udb, udb_ptr* d)
{
	domain_clear(udb, d);
	udb_rptr_zero(&DOMAIN(d)->node, udb);
	udb_ptr_free_space(d, udb,
		sizeof(struct domain_d)+DOMAIN(d)->namelen);
}

/** delete domain but also unlink from tree at zone */
static void
domain_delete_unlink(udb_base* udb, udb_ptr* z, udb_ptr* d)
{
	udb_ptr dtree, n;
	udb_ptr_new(&dtree, udb, &ZONE(z)->domains);
	udb_ptr_new(&n, udb, &DOMAIN(d)->node);
	udb_rptr_zero(&DOMAIN(d)->node, udb);
	udb_radix_delete(udb, &dtree, &n);
	udb_ptr_unlink(&dtree, udb);
	udb_ptr_unlink(&n, udb);
	domain_delete(udb, d);
}

void
udb_zone_clear(udb_base* udb, udb_ptr* zone)
{
	udb_ptr dtree, d;
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);
	udb_ptr_new(&dtree, udb, &ZONE(zone)->domains);
	udb_rptr_zero(&ZONE(zone)->nsec3param, udb);
	udb_zone_set_log_str(udb, zone, NULL);
	udb_zone_set_file_str(udb, zone, NULL);

	/* walk and delete all domains, rrsets, rrs, but keep tree */
	for(udb_radix_first(udb, &dtree, &d); d.data; udb_radix_next(udb, &d)){
		udb_ptr domain;
		udb_ptr_new(&domain, udb, &RADNODE(&d)->elem);
		udb_rptr_zero(&RADNODE(&d)->elem, udb);
		domain_delete(udb, &domain);
	}
	udb_ptr_unlink(&d, udb);
	udb_radix_tree_clear(udb, &dtree);
	ZONE(zone)->rrset_count = 0;
	ZONE(zone)->rr_count = 0;
	ZONE(zone)->expired = 0;
	ZONE(zone)->mtime = 0;
	ZONE(zone)->mtime_nsec = 0;
	udb_ptr_unlink(&dtree, udb);
}

void
udb_zone_delete(udb_base* udb, udb_ptr* zone)
{
	udb_ptr ztree, n;
	udb_ptr_new(&ztree, udb, udb_base_get_userdata(udb));
	udb_ptr_new(&n, udb, &ZONE(zone)->node);
	udb_rptr_zero(&ZONE(zone)->node, udb);
	udb_radix_delete(udb, &ztree, &n);
	udb_ptr_unlink(&ztree, udb);
	udb_ptr_unlink(&n, udb);
	udb_zone_delete_plain(udb, zone);
}

int
udb_zone_search(udb_base* udb, udb_ptr* result, const uint8_t* dname,
	size_t dname_len)
{
	udb_ptr ztree;
	udb_ptr_new(&ztree, udb, udb_base_get_userdata(udb));
	assert(udb_ptr_get_type(&ztree) == udb_chunk_type_radtree);
	if(udb_radname_search(udb, &ztree, dname, dname_len, result)) {
		if(result->data)
			udb_ptr_set_rptr(result, udb, &RADNODE(result)->elem);
		udb_ptr_unlink(&ztree, udb);
		return (result->data != 0);
	}
	udb_ptr_unlink(&ztree, udb);
	return 0;
}

void udb_zone_get_mtime(udb_base* udb, const uint8_t* dname, size_t dlen,
	struct timespec* mtime)
{
	udb_ptr z;
	if(udb_zone_search(udb, &z, dname, dlen)) {
		mtime->tv_sec = ZONE(&z)->mtime;
		mtime->tv_nsec = ZONE(&z)->mtime_nsec;
		udb_ptr_unlink(&z, udb);
		return;
	}
	mtime->tv_sec = 0;
	mtime->tv_nsec = 0;
}

void udb_zone_set_log_str(udb_base* udb, udb_ptr* zone, const char* str)
{
	/* delete original log str (if any) */
	if(ZONE(zone)->log_str.data) {
		udb_ptr s;
		size_t sz;
		udb_ptr_new(&s, udb, &ZONE(zone)->log_str);
		udb_rptr_zero(&ZONE(zone)->log_str, udb);
		sz = strlen((char*)udb_ptr_data(&s))+1;
		udb_ptr_free_space(&s, udb, sz);
	}

	/* set new log str */
	if(str) {
		udb_ptr s;
		size_t sz = strlen(str)+1;
		if(!udb_ptr_alloc_space(&s, udb, udb_chunk_type_data, sz)) {
			return; /* failed to allocate log string */
		}
		memmove(udb_ptr_data(&s), str, sz);
		udb_rptr_set_ptr(&ZONE(zone)->log_str, udb, &s);
		udb_ptr_unlink(&s, udb);
	}
}

void udb_zone_set_file_str(udb_base* udb, udb_ptr* zone, const char* str)
{
	/* delete original file str (if any) */
	if(ZONE(zone)->file_str.data) {
		udb_ptr s;
		size_t sz;
		udb_ptr_new(&s, udb, &ZONE(zone)->file_str);
		udb_rptr_zero(&ZONE(zone)->file_str, udb);
		sz = strlen((char*)udb_ptr_data(&s))+1;
		udb_ptr_free_space(&s, udb, sz);
	}

	/* set new file str */
	if(str) {
		udb_ptr s;
		size_t sz = strlen(str)+1;
		if(!udb_ptr_alloc_space(&s, udb, udb_chunk_type_data, sz)) {
			return; /* failed to allocate file string */
		}
		memmove(udb_ptr_data(&s), str, sz);
		udb_rptr_set_ptr(&ZONE(zone)->file_str, udb, &s);
		udb_ptr_unlink(&s, udb);
	}
}

const char* udb_zone_get_file_str(udb_base* udb, const uint8_t* dname,
	size_t dlen)
{
	udb_ptr z;
	if(udb_zone_search(udb, &z, dname, dlen)) {
		const char* str;
		if(ZONE(&z)->file_str.data) {
			udb_ptr s;
			udb_ptr_new(&s, udb, &ZONE(&z)->file_str);
			str = (const char*)udb_ptr_data(&s);
			udb_ptr_unlink(&s, udb);
		} else str = NULL;
		udb_ptr_unlink(&z, udb);
		return str;
	}
	return NULL;
}

#ifdef NSEC3
/** select the nsec3param for nsec3 usage */
static void
select_nsec3_param(udb_base* udb, udb_ptr* zone, udb_ptr* rrset)
{
	udb_ptr rr;
	udb_ptr_new(&rr, udb, &RRSET(rrset)->rrs);
	while(rr.data) {
		if(RR(&rr)->len >= 5 && RR(&rr)->wire[0] == NSEC3_SHA1_HASH &&
			RR(&rr)->wire[1] == 0) {
			udb_rptr_set_ptr(&ZONE(zone)->nsec3param, udb, &rr);
			udb_ptr_unlink(&rr, udb);
			return;
		}
		udb_ptr_set_rptr(&rr, udb, &RR(&rr)->next);
	}
	udb_ptr_unlink(&rr, udb);
}

const char*
udb_nsec3param_string(udb_ptr* rr)
{
	/* max saltlenth plus first couple of numbers (3+1+5+1+3+1) */
	static char params[MAX_RDLENGTH*2+16];
	char* p;
	assert(RR(rr)->len >= 5);
	p = params + snprintf(params, sizeof(params), "%u %u %u ",
		(unsigned)RR(rr)->wire[0], (unsigned)RR(rr)->wire[1],
		(unsigned)read_uint16(&RR(rr)->wire[2]));
	if(RR(rr)->wire[4] == 0) {
		*p++ = '-';
	} else {
		assert(RR(rr)->len >= 5+RR(rr)->wire[4]);
		p += hex_ntop(&RR(rr)->wire[5], RR(rr)->wire[4], p,
			sizeof(params)-strlen(params)-1);
	}
	*p = 0;
	return params;
}

/** look in zone for new selected nsec3param record from rrset */
static void
zone_hash_nsec3param(udb_base* udb, udb_ptr* zone, udb_ptr* rrset)
{
	select_nsec3_param(udb, zone, rrset);
	if(ZONE(zone)->nsec3param.data == 0)
		return;
	/* prettyprint the nsec3 parameters we are using */
	if(2 <= verbosity) {
		udb_ptr par;
		udb_ptr_new(&par, udb, &ZONE(zone)->nsec3param);
		VERBOSITY(1, (LOG_INFO, "rehash of zone %s with parameters %s",
			wiredname2str(ZONE(zone)->name),
			udb_nsec3param_string(&par)));
		udb_ptr_unlink(&par, udb);
	}
}
#endif /* NSEC3 */

/** create a new domain name */
static int
domain_create(udb_base* udb, udb_ptr* zone, const uint8_t* nm, size_t nmlen,
	udb_ptr* result)
{
	udb_ptr dtree, node;
	/* create domain chunk */
	if(!udb_ptr_alloc_space(result, udb, udb_chunk_type_domain,
		sizeof(struct domain_d)+nmlen))
		return 0;
	udb_rel_ptr_init(&DOMAIN(result)->node);
	udb_rel_ptr_init(&DOMAIN(result)->rrsets);
	DOMAIN(result)->namelen = nmlen;
	memmove(DOMAIN(result)->name, nm, nmlen);

	/* insert into domain tree */
	udb_ptr_new(&dtree, udb, &ZONE(zone)->domains);
	if(!udb_radname_insert(udb, &dtree, nm, nmlen, result, &node)) {
		udb_ptr_free_space(result, udb, sizeof(struct domain_d)+nmlen);
		udb_ptr_unlink(&dtree, udb);
		return 0;
	}
	udb_rptr_set_ptr(&DOMAIN(result)->node, udb, &node);
	udb_ptr_unlink(&dtree, udb);
	udb_ptr_unlink(&node, udb);
	return 1;
}

int
udb_domain_find(udb_base* udb, udb_ptr* zone, const uint8_t* nm, size_t nmlen,
	udb_ptr* result)
{
	int r;
	udb_ptr dtree;
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);
	udb_ptr_new(&dtree, udb, &ZONE(zone)->domains);
	r = udb_radname_search(udb, &dtree, nm, nmlen, result);
	if(result->data)
		udb_ptr_set_rptr(result, udb, &RADNODE(result)->elem);
	udb_ptr_unlink(&dtree, udb);
	return r && result->data;
}

/** find or create a domain name in the zone domain tree */
static int
domain_find_or_create(udb_base* udb, udb_ptr* zone, const uint8_t* nm,
	size_t nmlen, udb_ptr* result)
{
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);
	if(udb_domain_find(udb, zone, nm, nmlen, result))
		return 1;
	return domain_create(udb, zone, nm, nmlen, result);
}

/** remove rrset from the domain name rrset-list */
static void
domain_remove_rrset(udb_base* udb, udb_ptr* domain, uint16_t t)
{
	udb_ptr p, prev;
	assert(udb_ptr_get_type(domain) == udb_chunk_type_domain);
	udb_ptr_new(&p, udb, &DOMAIN(domain)->rrsets);
	udb_ptr_init(&prev, udb);
	while(p.data) {
		if(RRSET(&p)->type == t) {
			/* remove it */
			if(prev.data == 0) {
				/* first rrset */
				udb_rptr_set_rptr(&DOMAIN(domain)->rrsets,
					udb, &RRSET(&p)->next);
			} else {
				udb_rptr_set_rptr(&RRSET(&prev)->next,
					udb, &RRSET(&p)->next);
			}
			udb_ptr_unlink(&prev, udb);
			rrset_delete(udb, &p);
			return;
		}
		udb_ptr_set_ptr(&prev, udb, &p);
		udb_ptr_set_rptr(&p, udb, &RRSET(&p)->next);
	}
	/* rrset does not exist */
	udb_ptr_unlink(&prev, udb);
	udb_ptr_unlink(&p, udb);
}

/** create rrset in the domain rrset list */
static int
rrset_create(udb_base* udb, udb_ptr* domain, uint16_t t, udb_ptr* res)
{
	/* create it */
	if(!udb_ptr_alloc_space(res, udb, udb_chunk_type_rrset,
		sizeof(struct rrset_d)))
		return 0;
	udb_rel_ptr_init(&RRSET(res)->next);
	udb_rel_ptr_init(&RRSET(res)->rrs);
	RRSET(res)->type = t;
	
#if 0
	/* link it in, at the front */
	udb_rptr_set_rptr(&RRSET(res)->next, udb, &DOMAIN(domain)->rrsets);
	udb_rptr_set_ptr(&DOMAIN(domain)->rrsets, udb, res);
#else 
	/* preserve RRset order, link at end */
	if(DOMAIN(domain)->rrsets.data == 0) {
		udb_rptr_set_ptr(&DOMAIN(domain)->rrsets, udb, res);
	} else {
		udb_ptr p;
		udb_ptr_new(&p, udb, &DOMAIN(domain)->rrsets);
		while(RRSET(&p)->next.data)
			udb_ptr_set_rptr(&p, udb, &RRSET(&p)->next);
		udb_rptr_set_ptr(&RRSET(&p)->next, udb, res);
		udb_ptr_unlink(&p, udb);
	}
#endif
	return 1;
}

int
udb_rrset_find(udb_base* udb, udb_ptr* domain, uint16_t t, udb_ptr* res)
{
	assert(udb_ptr_get_type(domain) == udb_chunk_type_domain);
	udb_ptr_init(res, udb);
	udb_ptr_set_rptr(res, udb, &DOMAIN(domain)->rrsets);
	while(res->data) {
		if(RRSET(res)->type == t)
			return 1;
		udb_ptr_set_rptr(res, udb, &RRSET(res)->next);
	}
	/* rrset does not exist and res->data is conveniently zero */
	return 0;
}

/** find or create rrset in the domain rrset list */
static int
rrset_find_or_create(udb_base* udb, udb_ptr* domain, uint16_t t, udb_ptr* res)
{
	if(udb_rrset_find(udb, domain, t, res))
		return 1;
	return rrset_create(udb, domain, t, res);
}

/** see if RR matches type, class and rdata */
static int
rr_match(udb_ptr* rr, uint16_t t, uint16_t k, uint8_t* rdata, size_t rdatalen)
{
	return RR(rr)->type == t && RR(rr)->klass == k &&
		RR(rr)->len == rdatalen &&
		memcmp(RR(rr)->wire, rdata, rdatalen) == 0;
}

/** see if RR exists in the RR list that matches the rdata, and return it */
static int
rr_search(udb_base* udb, udb_ptr* rrset, uint16_t t, uint16_t k,
	uint8_t* rdata, size_t rdatalen, udb_ptr* result)
{
	assert(udb_ptr_get_type(rrset) == udb_chunk_type_rrset);
	udb_ptr_init(result, udb);
	udb_ptr_set_rptr(result, udb, &RRSET(rrset)->rrs);
	while(result->data) {
		if(rr_match(result, t, k, rdata, rdatalen))
			return 1; /* found */
		udb_ptr_set_rptr(result, udb, &RR(result)->next);
	}
	/* not found and result->data is conveniently zero */
	return 0;
}

/** create RR chunk */
static int
rr_create(udb_base* udb, uint16_t t, uint16_t k, uint32_t ttl,
	uint8_t* rdata, size_t rdatalen, udb_ptr* rr)
{
	if(!udb_ptr_alloc_space(rr, udb, udb_chunk_type_rr,
		sizeof(struct rr_d)+rdatalen))
		return 0;
	udb_rel_ptr_init(&RR(rr)->next);
	RR(rr)->type = t;
	RR(rr)->klass = k;
	RR(rr)->ttl = ttl;
	RR(rr)->len = rdatalen;
	memmove(RR(rr)->wire, rdata, rdatalen);
	return 1;
}

/** add an RR to an RRset. */
static int
rrset_add_rr(udb_base* udb, udb_ptr* rrset, uint16_t t, uint16_t k,
	uint32_t ttl, uint8_t* rdata, size_t rdatalen)
{
	udb_ptr rr;
	assert(udb_ptr_get_type(rrset) == udb_chunk_type_rrset);
	/* create it */
	if(!rr_create(udb, t, k, ttl, rdata, rdatalen, &rr))
		return 0;

	/* add at end, to preserve order of RRs */
	if(RRSET(rrset)->rrs.data == 0) {
		udb_rptr_set_ptr(&RRSET(rrset)->rrs, udb, &rr);
	} else {
		udb_ptr lastrr;
		udb_ptr_new(&lastrr, udb, &RRSET(rrset)->rrs);
		while(RR(&lastrr)->next.data)
			udb_ptr_set_rptr(&lastrr, udb, &RR(&lastrr)->next);
		udb_rptr_set_ptr(&RR(&lastrr)->next, udb, &rr);
		udb_ptr_unlink(&lastrr, udb);
	}
	udb_ptr_unlink(&rr, udb);
	return 1;
}

/** remove an RR from an RRset. return 0 if RR did not exist. */
static int
rrset_del_rr(udb_base* udb, udb_ptr* rrset, uint16_t t, uint16_t k,
	uint8_t* rdata, size_t rdatalen)
{
	udb_ptr p, prev;
	assert(udb_ptr_get_type(rrset) == udb_chunk_type_rrset);
	udb_ptr_new(&p, udb, &RRSET(rrset)->rrs);
	udb_ptr_init(&prev, udb);
	while(p.data) {
		if(rr_match(&p, t, k, rdata, rdatalen)) {
			/* remove it */
			if(prev.data == 0) {
				/* first in list */
				udb_rptr_set_rptr(&RRSET(rrset)->rrs, udb,
					&RR(&p)->next);
			} else {
				udb_rptr_set_rptr(&RR(&prev)->next, udb,
					&RR(&p)->next);
			}
			udb_ptr_unlink(&prev, udb);
			rr_delete(udb, &p);
			return 1;
		}
		udb_ptr_set_ptr(&prev, udb, &p);
		udb_ptr_set_rptr(&p, udb, &RR(&p)->next);
	}
	/* not found */
	udb_ptr_unlink(&prev, udb);
	udb_ptr_unlink(&p, udb);
	return 0;
}

int
udb_zone_add_rr(udb_base* udb, udb_ptr* zone, const uint8_t* nm, size_t nmlen,
	uint16_t t, uint16_t k, uint32_t ttl, uint8_t* rdata, size_t rdatalen)
{
	udb_ptr domain, rrset, rr;
	int created_rrset = 0;
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);

	/* find or create domain */
	if(!domain_find_or_create(udb, zone, nm, nmlen, &domain)) {
		return 0;
	}
	/* find or create rrset(type) */
	if(!rrset_find_or_create(udb, &domain, t, &rrset)) {
		goto exit_clean_domain;
	}
	if(RRSET(&rrset)->rrs.data == 0)
		created_rrset = 1;
	/* test for duplicate RRs */
	if(rr_search(udb, &rrset, t, k, rdata, rdatalen, &rr)) {
		udb_ptr_unlink(&rr, udb);
		goto exit_clean_domain_rrset;
	}
	/* add RR to rrset */
	if(!rrset_add_rr(udb, &rrset, t, k, ttl, rdata, rdatalen)) {
	exit_clean_domain_rrset:
		/* if rrset was created, remove it */
		if(RRSET(&rrset)->rrs.data == 0) {
			udb_ptr_zero(&rrset, udb);
			domain_remove_rrset(udb, &domain, t);
		}
		udb_ptr_unlink(&rrset, udb);
	exit_clean_domain:
		/* if domain created, delete it */
		if(DOMAIN(&domain)->rrsets.data == 0)
			domain_delete_unlink(udb, zone, &domain);
		udb_ptr_unlink(&domain, udb);
		return 0;
	}
	/* success, account changes */
	if(created_rrset)
		ZONE(zone)->rrset_count ++;
	ZONE(zone)->rr_count ++;
#ifdef NSEC3
	if(t == TYPE_NSEC3PARAM && ZONE(zone)->nsec3param.data == 0)
		zone_hash_nsec3param(udb, zone, &rrset);
#endif /* NSEC3 */
	udb_ptr_unlink(&domain, udb);
	udb_ptr_unlink(&rrset, udb);
	return 1;
}

void
udb_zone_del_rr(udb_base* udb, udb_ptr* zone, const uint8_t* nm, size_t nmlen,
	uint16_t t, uint16_t k, uint8_t* rdata, size_t rdatalen)
{
	udb_ptr domain, rrset;
	assert(udb_ptr_get_type(zone) == udb_chunk_type_zone);
	/* find the domain */
	if(!udb_domain_find(udb, zone, nm, nmlen, &domain))
		return;
	/* find the rrset */
	if(!udb_rrset_find(udb, &domain, t, &rrset)) {
		udb_ptr_unlink(&domain, udb);
		return;
	}
	/* remove the RR */
#ifdef NSEC3
	if(t == TYPE_NSEC3PARAM) {
		udb_ptr rr;
		if(rr_search(udb, &rrset, t, k, rdata, rdatalen, &rr)) {
			if(rr.data == ZONE(zone)->nsec3param.data) {
				udb_rptr_zero(&ZONE(zone)->nsec3param, udb);
			}
			udb_ptr_unlink(&rr, udb);
		}
	}
#endif /* NSEC3 */
	if(!rrset_del_rr(udb, &rrset, t, k, rdata, rdatalen)) {
		/* rr did not exist */
		udb_ptr_unlink(&domain, udb);
		udb_ptr_unlink(&rrset, udb);
		return;
	}
	ZONE(zone)->rr_count --;
#ifdef NSEC3
	if(t == TYPE_NSEC3PARAM && ZONE(zone)->nsec3param.data == 0 &&
		RRSET(&rrset)->rrs.data != 0) {
		zone_hash_nsec3param(udb, zone, &rrset);
	}
#endif /* NSEC3 */
	/* see we we can remove the rrset too */
	if(RRSET(&rrset)->rrs.data == 0) {
		udb_ptr_zero(&rrset, udb);
		domain_remove_rrset(udb, &domain, t);
		ZONE(zone)->rrset_count --;
	}
	/* see if we can remove the domain name too */
	if(DOMAIN(&domain)->rrsets.data == 0) {
		domain_delete_unlink(udb, zone, &domain);
	}
	udb_ptr_unlink(&rrset, udb);
	udb_ptr_unlink(&domain, udb);
}

void
udb_zone_walk_chunk(void* base, void* d, uint64_t s, udb_walk_relptr_cb* cb,
	void* arg)
{
	struct zone_d* p = (struct zone_d*)d;
	assert(s >= sizeof(struct zone_d)+p->namelen);
	(void)s;
	(*cb)(base, &p->node, arg);
	(*cb)(base, &p->domains, arg);
	(*cb)(base, &p->nsec3param, arg);
	(*cb)(base, &p->log_str, arg);
	(*cb)(base, &p->file_str, arg);
}

void
udb_domain_walk_chunk(void* base, void* d, uint64_t s, udb_walk_relptr_cb* cb,
	void* arg)
{
	struct domain_d* p = (struct domain_d*)d;
	assert(s >= sizeof(struct domain_d)+p->namelen);
	(void)s;
	(*cb)(base, &p->node, arg);
	(*cb)(base, &p->rrsets, arg);
}

void
udb_rrset_walk_chunk(void* base, void* d, uint64_t s, udb_walk_relptr_cb* cb,
	void* arg)
{
	struct rrset_d* p = (struct rrset_d*)d;
	assert(s >= sizeof(struct rrset_d));
	(void)s;
	(*cb)(base, &p->next, arg);
	(*cb)(base, &p->rrs, arg);
}

void
udb_rr_walk_chunk(void* base, void* d, uint64_t s, udb_walk_relptr_cb* cb,
	void* arg)
{
	struct rr_d* p = (struct rr_d*)d;
	assert(s >= sizeof(struct rr_d)+p->len);
	(void)s;
	(*cb)(base, &p->next, arg);
}

void
udb_task_walk_chunk(void* base, void* d, uint64_t s, udb_walk_relptr_cb* cb,
	void* arg)
{
	struct task_list_d* p = (struct task_list_d*)d;
	assert(s >= p->size);
	(void)s;
	(*cb)(base, &p->next, arg);
}

void namedb_walkfunc(void* base, void* warg, uint8_t t, void* d, uint64_t s,
        udb_walk_relptr_cb* cb, void* arg)
{
        (void)warg;
        switch(t) {
	case udb_chunk_type_radtree:
		udb_radix_tree_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_radnode:
		udb_radix_node_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_radarray:
		udb_radix_array_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_zone:
		udb_zone_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_domain:
		udb_domain_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_rrset:
		udb_rrset_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_rr:
		udb_rr_walk_chunk(base, d, s, cb, arg);
		break;
	case udb_chunk_type_task:
		udb_task_walk_chunk(base, d, s, cb, arg);
		break;
	default:
		/* no rel ptrs */
		break;
	}
}
