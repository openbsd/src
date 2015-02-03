/*
 * axfr.c -- generating AXFR responses.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include "axfr.h"
#include "dns.h"
#include "packet.h"
#include "options.h"

#define AXFR_TSIG_SIGN_EVERY_NTH	96	/* tsig sign every N packets. */

query_state_type
query_axfr(struct nsd *nsd, struct query *query)
{
	domain_type *closest_match;
	domain_type *closest_encloser;
	int exact;
	int added;
	uint16_t total_added = 0;

	if (query->axfr_is_done)
		return QUERY_PROCESSED;

	if (query->maxlen > AXFR_MAX_MESSAGE_LEN)
		query->maxlen = AXFR_MAX_MESSAGE_LEN;

	assert(!query_overflow(query));
	/* only keep running values for most packets */
	query->tsig_prepare_it = 0;
	query->tsig_update_it = 1;
	if(query->tsig_sign_it) {
		/* prepare for next updates */
		query->tsig_prepare_it = 1;
		query->tsig_sign_it = 0;
	}

	if (query->axfr_zone == NULL) {
		domain_type* qdomain;
		/* Start AXFR.  */
		STATUP(nsd, raxfr);
		exact = namedb_lookup(nsd->db,
				      query->qname,
				      &closest_match,
				      &closest_encloser);

		qdomain = closest_encloser;
		query->axfr_zone = domain_find_zone(nsd->db, closest_encloser);

		if (!exact
		    || query->axfr_zone == NULL
		    || query->axfr_zone->apex != qdomain
		    || query->axfr_zone->soa_rrset == NULL)
		{
			/* No SOA no transfer */
			RCODE_SET(query->packet, RCODE_NOTAUTH);
			return QUERY_PROCESSED;
		}
		ZTATUP(nsd, query->axfr_zone, raxfr);

		query->axfr_current_domain = qdomain;
		query->axfr_current_rrset = NULL;
		query->axfr_current_rr = 0;
		if(query->tsig.status == TSIG_OK) {
			query->tsig_sign_it = 1; /* sign first packet in stream */
		}

		query_add_compression_domain(query, qdomain, QHEADERSZ);

		assert(query->axfr_zone->soa_rrset->rr_count == 1);
		added = packet_encode_rr(query,
					 query->axfr_zone->apex,
					 &query->axfr_zone->soa_rrset->rrs[0],
					 query->axfr_zone->soa_rrset->rrs[0].ttl);
		if (!added) {
			/* XXX: This should never happen... generate error code? */
			abort();
		}
		++total_added;
	} else {
		/*
		 * Query name and EDNS need not be repeated after the
		 * first response packet.
		 */
		query->edns.status = EDNS_NOT_PRESENT;
		buffer_set_limit(query->packet, QHEADERSZ);
		QDCOUNT_SET(query->packet, 0);
		query_prepare_response(query);
	}

	/* Add zone RRs until answer is full.  */
	assert(query->axfr_current_domain);

	do {
		if (!query->axfr_current_rrset) {
			query->axfr_current_rrset = domain_find_any_rrset(
				query->axfr_current_domain,
				query->axfr_zone);
			query->axfr_current_rr = 0;
		}
		while (query->axfr_current_rrset) {
			if (query->axfr_current_rrset != query->axfr_zone->soa_rrset
			    && query->axfr_current_rrset->zone == query->axfr_zone)
			{
				while (query->axfr_current_rr < query->axfr_current_rrset->rr_count) {
					added = packet_encode_rr(
						query,
						query->axfr_current_domain,
						&query->axfr_current_rrset->rrs[query->axfr_current_rr],
						query->axfr_current_rrset->rrs[query->axfr_current_rr].ttl);
					if (!added)
						goto return_answer;
					++total_added;
					++query->axfr_current_rr;
				}
			}

			query->axfr_current_rrset = query->axfr_current_rrset->next;
			query->axfr_current_rr = 0;
		}
		assert(query->axfr_current_domain);
		query->axfr_current_domain
			= domain_next(query->axfr_current_domain);
	}
	while (query->axfr_current_domain != NULL &&
			domain_is_subdomain(query->axfr_current_domain,
					    query->axfr_zone->apex));

	/* Add terminating SOA RR.  */
	assert(query->axfr_zone->soa_rrset->rr_count == 1);
	added = packet_encode_rr(query,
				 query->axfr_zone->apex,
				 &query->axfr_zone->soa_rrset->rrs[0],
				 query->axfr_zone->soa_rrset->rrs[0].ttl);
	if (added) {
		++total_added;
		query->tsig_sign_it = 1; /* sign last packet */
		query->axfr_is_done = 1;
	}

return_answer:
	AA_SET(query->packet);
	ANCOUNT_SET(query->packet, total_added);
	NSCOUNT_SET(query->packet, 0);
	ARCOUNT_SET(query->packet, 0);

	/* check if it needs tsig signatures */
	if(query->tsig.status == TSIG_OK) {
		if(query->tsig.updates_since_last_prepare >= AXFR_TSIG_SIGN_EVERY_NTH) {
			query->tsig_sign_it = 1;
		}
	}
	query_clear_compression_tables(query);
	return QUERY_IN_AXFR;
}

/*
 * Answer if this is an AXFR or IXFR query.
 */
query_state_type
answer_axfr_ixfr(struct nsd *nsd, struct query *q)
{
	acl_options_t *acl = NULL;
	/* Is it AXFR? */
	switch (q->qtype) {
	case TYPE_AXFR:
		if (q->tcp) {
			zone_options_t* zone_opt;
			zone_opt = zone_options_find(nsd->options, q->qname);
			if(!zone_opt ||
			   acl_check_incoming(zone_opt->pattern->provide_xfr, q, &acl)==-1)
			{
				if (verbosity > 0) {
					char a[128];
					addr2str(&q->addr, a, sizeof(a));
					VERBOSITY(1, (LOG_INFO, "axfr for zone %s from client %s refused, %s",
						dname_to_string(q->qname, NULL), a, acl?"blocked":"no acl matches"));
				}
				DEBUG(DEBUG_XFRD,1, (LOG_INFO, "axfr refused, %s",
					acl?"blocked":"no acl matches"));
				if (!zone_opt) {
					RCODE_SET(q->packet, RCODE_NOTAUTH);
				} else {
					RCODE_SET(q->packet, RCODE_REFUSE);
				}
				return QUERY_PROCESSED;
			}
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "axfr admitted acl %s %s",
				acl->ip_address_spec, acl->key_name?acl->key_name:"NOKEY"));
			return query_axfr(nsd, q);
		}
		/** Fallthrough: AXFR over UDP queries are discarded. */
	case TYPE_IXFR:
		RCODE_SET(q->packet, RCODE_IMPL);
		return QUERY_PROCESSED;
	default:
		return QUERY_DISCARDED;
	}
}
