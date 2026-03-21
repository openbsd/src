/*
 * zonec.c -- zone compiler.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <netinet/in.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "zonec.h"

#include "dname.h"
#include "dns.h"
#include "namedb.h"
#include "rdata.h"
#include "region-allocator.h"
#include "util.h"
#include "options.h"
#include "nsec3.h"
#include "zone.h"

/*
 * Find rrset type for any zone
 */
static rrset_type*
domain_find_rrset_any(domain_type *domain, uint16_t type)
{
	rrset_type *result = domain->rrsets;
	while (result) {
		if (rrset_rrtype(result) == type) {
			return result;
		}
		result = result->next;
	}
	return NULL;
}

/*
 * Check for DNAME type. Nothing is allowed below it
 */
static int
check_dname(zone_type* zone)
{
	domain_type* domain;
	for(domain = zone->apex; domain && domain_is_subdomain(domain,
		zone->apex); domain=domain_next(domain))
	{
		if(domain->is_existing) {
			/* there may not be DNAMEs above it */
			domain_type* parent = domain->parent;
#ifdef NSEC3
			if(domain_has_only_NSEC3(domain, NULL))
				continue;
#endif
			while(parent) {
				if(domain_find_rrset_any(parent, TYPE_DNAME)) {
					log_msg(LOG_ERR, "While checking node %s,",
						domain_to_string(domain));
					log_msg(LOG_ERR, "DNAME at %s has data below it. "
						"This is not allowed (rfc 2672).",
						domain_to_string(parent));
					return 0;
				}
				parent = parent->parent;
			}
		}
	}

	return 1;
}

static int
has_soa(domain_type* domain)
{
	rrset_type* p = NULL;
	if(!domain) return 0;
	for(p = domain->rrsets; p; p = p->next)
		if(rrset_rrtype(p) == TYPE_SOA)
			return 1;
	return 0;
}

struct zonec_state {
	struct namedb *database;
	struct domain_table *domains;
	struct zone *zone;
	size_t errors;
	size_t records;

	struct collect_rrs c;
};

static void zonec_commit_rrset(zone_parser_t *parser, struct zonec_state *state)
{
	struct rrset *rrset;
	int priority = parser->options.secondary ? ZONE_WARNING : ZONE_ERROR;

	if(!state->c.domain || state->c.rr_count == 0)
		return;
	if (!state->c.rrset) {
		rrset = region_alloc(state->database->region, sizeof(*rrset)
#ifdef PACKED_STRUCTS
			+ sizeof(rr_type*) * state->c.rr_count /* Add space for RRs. */
#endif
			);
		rrset->zone = state->zone;
		rrset->rr_count = state->c.rr_count;
#ifndef PACKED_STRUCTS
		rrset->rrs = region_alloc(state->database->region,
				sizeof(rr_type*) * state->c.rr_count);
#endif
		memcpy(rrset->rrs, state->c.rrs, state->c.rr_count * sizeof(rr_type*));
		switch (state->c.type) {
			case TYPE_CNAME:
				if (!domain_find_non_cname_rrset(state->c.domain, state->zone))
					break;
				zone_log(parser, priority, "CNAME and other data at the same name");
				break;
			case TYPE_RRSIG:
			case TYPE_NXT:
			case TYPE_SIG:
			case TYPE_NSEC:
			case TYPE_NSEC3:
				break;
			default:
				if (!domain_find_rrset(state->c.domain, state->zone, TYPE_CNAME))
					break;
				zone_log(parser, priority, "CNAME and other data at the same name");
				break;
		}
		/* Add it */
		domain_add_rrset(state->c.domain, rrset);
	} else {
#ifndef PACKED_STRUCTS
		struct rr **rrs;
#else
		struct rrset *rrset_orig;
#endif
		/* Add it... */
		rrset = state->c.rrset;
#ifndef PACKED_STRUCTS
		rrs = rrset->rrs;
		rrset->rrs = region_alloc_array(
			state->database->region, rrset->rr_count + state->c.rr_count, sizeof(*rrs));
		memcpy(rrset->rrs, rrs, rrset->rr_count * sizeof(*rrs));
		region_recycle(state->database->region, rrs, rrset->rr_count * sizeof(*rrs));
#else
		rrset_orig = rrset;
		rrset = region_alloc(state->database->region,
			sizeof(rrset_type) +
			(rrset_orig->rr_count+state->c.rr_count)*sizeof(rr_type*));
		memcpy(rrset, rrset_orig,
			sizeof(rrset_type) +
			rrset_orig->rr_count*sizeof(rr_type*));
		if(state->c.rrset_prev)
			state->c.rrset_prev->next = rrset;
		else	state->c.domain->rrsets = rrset;
		region_recycle(state->database->region, rrset_orig,
			sizeof(rrset_type) +
			rrset_orig->rr_count*sizeof(rr_type*));
#endif /* PACKED_STRUCTS */
		memcpy(rrset->rrs + rrset->rr_count, state->c.rrs, state->c.rr_count * sizeof(rr_type*));
		rrset->rr_count += state->c.rr_count;
	}
	state->records += state->c.rr_count;;
	/* Check we have SOA */
	if (state->c.rrs[0]->owner == state->zone->apex)
		apex_rrset_checks(state->database, rrset, state->c.rrs[0]->owner);

	state->c.domain = NULL;
	state->c.type = -1;
	state->c.rrset = NULL;
#ifdef PACKED_STRUCTS
	state->c.rrset_prev = NULL;
#endif
	state->c.rr_count = 0;
}

int32_t zonec_accept(
	zone_parser_t *parser,
	const zone_name_t *owner,
	uint16_t type,
	uint16_t class,
	uint32_t ttl,
	uint16_t rdlength,
	const uint8_t *rdata,
	void *user_data)
{
	struct rr *rr;
	struct dname_buffer dname;
	struct domain *domain;
	struct buffer buffer;
	int priority;
	int32_t code;
	const struct nsd_type_descriptor *descriptor;
	struct zonec_state *state = (struct zonec_state *)user_data;
	assert(state);

	buffer_create_from(&buffer, rdata, rdlength);

	priority = parser->options.secondary ? ZONE_WARNING : ZONE_ERROR;
	/* limit to IN class */
	if (class != CLASS_IN)
		zone_log(parser, priority, "only class IN is supported");

	if(!dname_make_buffered(&dname, (uint8_t*)owner->octets, 1)) {
		zone_log(parser, ZONE_ERROR, "the owner cannot be converted");
		return ZONE_BAD_PARAMETER;
	}
	domain = domain_table_insert(state->domains, (void*)&dname);
	assert(domain);
	if (domain != state->c.domain || type != state->c.type
	||  state->c.rr_count >= (int)(sizeof(state->c.rrs) / sizeof(*state->c.rrs))){
		zonec_commit_rrset(parser, state);
		state->c.domain = domain;
		state->c.type = type;
		state->c.rrset = NULL;
#ifdef PACKED_STRUCTS
		state->c.rrset_prev = NULL;
#endif
	}
	descriptor = nsd_type_descriptor(type);
	code = descriptor->read_rdata(state->domains, rdlength, &buffer, &rr);
	if(code < 0) {
		zone_log(parser, ZONE_ERROR, "the RR rdata fields are wrong for the type, %s %s %s",
			dname_to_string((void*)&dname,0),
			rrtype_to_string(type),
			read_rdata_fail_str(code));
		if(code == TRUNCATED)
			return ZONE_OUT_OF_MEMORY;
		return ZONE_BAD_PARAMETER;
	}
	rr->owner = domain;
	rr->type = type;
	rr->klass = class;
	rr->ttl = ttl;

	/* we have the zone already */
	if (type == TYPE_SOA) {
		if (domain != state->zone->apex) {
			char s[MAXDOMAINLEN*5];
			snprintf(s, sizeof(s), "%s", domain_to_string(domain));
			zone_log(parser, priority, "SOA record with invalid domain name, '%s' is not '%s'",
				domain_to_string(state->zone->apex), s);
		} else if (has_soa(domain)) {
			zone_log(parser, priority, "this SOA record was already encountered");
		}
		domain->is_apex = 1;
	}

	if (!domain_is_subdomain(domain, state->zone->apex)) {
		char s[MAXDOMAINLEN*5];
		snprintf(s, sizeof(s), "%s", domain_to_string(state->zone->apex));
		zone_log(parser, priority, "out of zone data: %s is outside the zone for fqdn %s",
		         s, domain_to_string(domain));
		if (!parser->options.secondary) {
			return ZONE_SEMANTIC_ERROR;
		}
	}
	/* With the first RR for a RRset in this position in the zone file,
	 * find the RRset */
	if (state->c.rr_count == 0)  {
#ifndef PACKED_STRUCTS
		state->c.rrset = domain_find_rrset(state->c.domain, state->zone, state->c.type);
#else
		state->c.rrset = domain_find_rrset_and_prev(state->c.domain, state->zone, state->c.type, &state->c.rrset_prev);
#endif
	}
	if (type == TYPE_RRSIG)
		; /* pass */
	else if (state->c.rrset && ttl != state->c.rrset->rrs[0]->ttl) {
		zone_log(parser, ZONE_WARNING,
			"%s TTL %"PRIu32" does not match TTL %u of %s RRset",
			domain_to_string(domain), ttl,
			state->c.rrset->rrs[0]->ttl, rrtype_to_string(type));

	} else if (state->c.rr_count && ttl != state->c.rrs[0]->ttl) {
		zone_log(parser, ZONE_WARNING,
			"%s TTL %"PRIu32" does not match TTL %u of %s RRset",
			domain_to_string(domain), ttl,
			state->c.rrs[0]->ttl, rrtype_to_string(type));
	}
	if (state->c.rrset || state->c.rr_count) {
		switch (type) {
			case TYPE_CNAME:
				zone_log(parser, priority, "multiple CNAMEs at the same name");
				break;
			case TYPE_DNAME:
				zone_log(parser, priority, "multiple DNAMEs at the same name");
				break;
			default:
				break;
		}
	}
	if (state->c.rrset) {
		/* Search for possible duplicates in existing RRset */
		for (int i = 0; i < state->c.rrset->rr_count; i++) {
			if (!equal_rr_rdata(descriptor, rr, state->c.rrset->rrs[i]))
				continue;
			/* Discard the duplicates... */
			/* Lower the usage counter for domains in the rdata. */
			rr_lower_usage(state->database, rr);
			region_recycle(state->database->region, rr, sizeof(*rr) + rr->rdlength);
			return 0;
		}
	}
	/* Search for possible duplicates in already batched RRs */
	for (int i = 0; i < state->c.rr_count; i++) {
		if (!equal_rr_rdata(descriptor, rr, state->c.rrs[i]))
			continue;
		/* Discard the duplicates... */
		/* Lower the usage counter for domains in the rdata. */
		rr_lower_usage(state->database, rr);
		region_recycle(state->database->region, rr, sizeof(*rr) + rr->rdlength);
		return 0;
	}
	state->c.rrs[state->c.rr_count++] = rr;
	return 0;
}

static int32_t zonec_include(
  zone_parser_t *parser,
  const char *file,
  const char *path,
  void *user_data)
{
	char **paths;
	struct zonec_state *state;
	struct namedb *database;
	struct zone *zone;

	(void)parser;
	(void)file;

	state = (struct zonec_state *)user_data;
	database = state->database;
	zone = state->zone;

	assert((zone->includes.count == 0) == (zone->includes.paths == NULL));

	for (size_t i=0; i < zone->includes.count; i++)
		if (strcmp(path, zone->includes.paths[i]) == 0)
			return 0;

	paths = region_alloc_array(
		database->region, zone->includes.count + 1, sizeof(*paths));
	if (zone->includes.count) {
		const size_t size = zone->includes.count * sizeof(*paths);
		memcpy(paths, zone->includes.paths, size);
		region_recycle(database->region, zone->includes.paths, size);
	}
	paths[zone->includes.count] = region_strdup(database->region, path);
	zone->includes.count++;
	zone->includes.paths = paths;
	return 0;
}

static void zonec_log(
	zone_parser_t *parser,
	uint32_t category,
	const char *file,
	size_t line,
	const char *message,
	void *user_data)
{
	int priority;
	struct zonec_state *state = (struct zonec_state *)user_data;

	assert(state);
	(void)parser;

	switch (category) {
	case ZONE_INFO:
		priority = LOG_INFO;
		break;
	case ZONE_WARNING:
		priority = LOG_WARNING;
		break;
	default:
		priority = LOG_ERR;
		state->errors++;
		break;
	}

	if (file)
		log_msg(priority, "%s:%zu: %s", file, line, message);
	else
		log_msg(priority, "%s", message);
}

/*
 * Reads the specified zone into the memory
 * nsd_options can be NULL if no config file is passed.
 */
unsigned int
zonec_read(
	struct namedb *database,
	struct domain_table *domains,
	const char *name,
	const char *zonefile,
	struct zone *zone)
{
	const struct dname *origin;
	zone_parser_t parser;
	zone_options_t options;
	zone_name_buffer_t name_buffer;
	zone_rdata_buffer_t rdata_buffer;
	struct zonec_state state;
	zone_buffers_t buffers = { 1, &name_buffer, &rdata_buffer };

	state.database = database;
	state.domains = domains;
	state.zone = zone;
	state.errors = 0;
	state.records = 0;

	state.c.domain = NULL;
	state.c.type = -1;
	state.c.rrset = NULL;
#ifdef PACKED_STRUCTS
	state.c.rrset_prev = NULL;
#endif
	state.c.rr_count = 0;
	origin = domain_dname(zone->apex);
	memset(&options, 0, sizeof(options));
	options.origin.octets = dname_name(origin);
	options.origin.length = origin->name_size;
	options.default_ttl = DEFAULT_TTL;
	options.default_class = CLASS_IN;
	options.secondary = zone_is_slave(zone->opts) != 0;
	options.pretty_ttls = true; /* non-standard, for backwards compatibility */
	options.log.callback = &zonec_log;
	options.accept.callback = &zonec_accept;
	options.include.callback = &zonec_include;

	/* Parse and process all RRs.  */
	if (zone_parse(&parser, &options, &buffers, zonefile, &state) != 0) {
		/* With all socked up RRs,
		 * lower the usage counter for domains in the rdata.
		 */
		for (int i = 0; i < state.c.rr_count; i++) {
			/* Lower the usage counter for domains in the rdata. */
			rr_lower_usage(database, state.c.rrs[i]);
			region_recycle( database->region, state.c.rrs[i]
			              , sizeof(*state.c.rrs[i])
			              + state.c.rrs[i]->rdlength);
		}
		return state.errors;
	}
	zonec_commit_rrset(&parser, &state);

	/* Check if zone file contained a correct SOA record */
	if (!zone) {
		log_msg(LOG_ERR, "zone configured as '%s' has no content.", name);
		state.errors++;
	} else if (!zone->soa_rrset || zone->soa_rrset->rr_count == 0) {
		log_msg(LOG_ERR, "zone configured as '%s' has no SOA record", name);
		state.errors++;
	} else if (dname_compare(domain_dname(zone->soa_rrset->rrs[0]->owner), origin) != 0) {
		log_msg(LOG_ERR, "zone configured as '%s', but SOA has owner '%s'",
		        name, domain_to_string(zone->soa_rrset->rrs[0]->owner));
		state.errors++;
	}

	if(!zone_is_slave(zone->opts) && !check_dname(zone))
		state.errors++;

	return state.errors;
}

void
apex_rrset_checks(namedb_type* db, rrset_type* rrset, domain_type* domain)
{
	uint32_t soa_minimum = 0;
	unsigned i;
	zone_type* zone = rrset->zone;
	assert(domain == zone->apex);
	(void)domain;
	if (rrset_rrtype(rrset) == TYPE_SOA) {
		zone->soa_rrset = rrset;

		/* BUG #103 add another soa with a tweaked ttl */
		if(zone->soa_nx_rrset == 0) {
			zone->soa_nx_rrset = region_alloc(db->region,
				sizeof(rrset_type)
#ifdef PACKED_STRUCTS
				+ sizeof(rr_type*)
#endif
				);
			zone->soa_nx_rrset->rr_count = 1;
			zone->soa_nx_rrset->next = 0;
			zone->soa_nx_rrset->zone = zone;
#ifndef PACKED_STRUCTS
			zone->soa_nx_rrset->rrs = region_alloc(db->region,
				sizeof(rr_type*));
#endif
			zone->soa_nx_rrset->rrs[0] = region_alloc(db->region,
				sizeof(rr_type)+rrset->rrs[0]->rdlength);
		}
		memcpy(zone->soa_nx_rrset->rrs[0], rrset->rrs[0],
			sizeof(rr_type)+rrset->rrs[0]->rdlength);

		/* check the ttl and MINIMUM value and set accordingly */
		retrieve_soa_rdata_minttl(rrset->rrs[0], &soa_minimum);
		if (rrset->rrs[0]->ttl > soa_minimum) {
			zone->soa_nx_rrset->rrs[0]->ttl = soa_minimum;
		}
	} else if (rrset_rrtype(rrset) == TYPE_NS) {
		zone->ns_rrset = rrset;
	} else if (rrset_rrtype(rrset) == TYPE_RRSIG) {
		for (i = 0; i < rrset->rr_count; ++i) {
			if(rr_rrsig_type_covered(rrset->rrs[i])==TYPE_DNSKEY){
				zone->is_secure = 1;
				break;
			}
		}
	}
}
