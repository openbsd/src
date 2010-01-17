/*
 * axfr.c -- generating AXFR responses.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

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
#ifdef TSIG
	/* only keep running values for most packets */
	query->tsig_prepare_it = 0;
	query->tsig_update_it = 1;
	if(query->tsig_sign_it) {
		/* prepare for next updates */
		query->tsig_prepare_it = 1;
		query->tsig_sign_it = 0;
	}
#endif /* TSIG */

	if (query->axfr_zone == NULL) {
		/* Start AXFR.  */
		exact = namedb_lookup(nsd->db,
				      query->qname,
				      &closest_match,
				      &closest_encloser);

		query->domain = closest_encloser;
		query->axfr_zone = domain_find_zone(closest_encloser);

		if (!exact
		    || query->axfr_zone == NULL
		    || query->axfr_zone->apex != query->domain)
		{
			/* No SOA no transfer */
			RCODE_SET(query->packet, RCODE_REFUSE);
			return QUERY_PROCESSED;
		}

		query->axfr_current_domain
			= (domain_type *) rbtree_first(nsd->db->domains->names_to_domains);
		query->axfr_current_rrset = NULL;
		query->axfr_current_rr = 0;
#ifdef TSIG
		if(query->tsig.status == TSIG_OK) {
			query->tsig_sign_it = 1; /* sign first packet in stream */
		}
#endif /* TSIG */

		query_add_compression_domain(query, query->domain, QHEADERSZ);

		assert(query->axfr_zone->soa_rrset->rr_count == 1);
		added = packet_encode_rr(query,
					 query->axfr_zone->apex,
					 &query->axfr_zone->soa_rrset->rrs[0]);
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

	while ((rbnode_t *) query->axfr_current_domain != RBTREE_NULL) {
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
						&query->axfr_current_rrset->rrs[query->axfr_current_rr]);
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
			= (domain_type *) rbtree_next((rbnode_t *) query->axfr_current_domain);
	}

	/* Add terminating SOA RR.  */
	assert(query->axfr_zone->soa_rrset->rr_count == 1);
	added = packet_encode_rr(query,
				 query->axfr_zone->apex,
				 &query->axfr_zone->soa_rrset->rrs[0]);
	if (added) {
		++total_added;
#ifdef TSIG
		query->tsig_sign_it = 1; /* sign last packet */
#endif /* TSIG */
		query->axfr_is_done = 1;
	}

return_answer:
	ANCOUNT_SET(query->packet, total_added);
	NSCOUNT_SET(query->packet, 0);
	ARCOUNT_SET(query->packet, 0);

#ifdef TSIG
	/* check if it needs tsig signatures */
	if(query->tsig.status == TSIG_OK) {
		if(query->tsig.updates_since_last_prepare >= AXFR_TSIG_SIGN_EVERY_NTH) {
			query->tsig_sign_it = 1;
		}
	}
#endif /* TSIG */
	query_clear_compression_tables(query);
	return QUERY_IN_AXFR;
}

/*
 * Answer if this is an AXFR or IXFR query.
 */
query_state_type
answer_axfr_ixfr(struct nsd *nsd, struct query *q)
{
	acl_options_t *acl;
	/* Is it AXFR? */
	switch (q->qtype) {
	case TYPE_AXFR:
		if (q->tcp) {
			zone_options_t* zone_opt;
			zone_opt = zone_options_find(nsd->options, q->qname);
			if(!zone_opt ||
			   acl_check_incoming(zone_opt->provide_xfr, q, &acl)==-1)
			{
				char address[128];

				if (addr2ip(q->addr, address, 128)) {
					DEBUG(DEBUG_XFRD,1, (LOG_INFO,
						"addr2ip failed"));
					strlcpy(address, "[unknown]", sizeof(address));
				}

				VERBOSITY(1, (LOG_INFO, "axfr for zone %s from client %s refused, %s", dname_to_string(q->qname, NULL), address, acl?"blocked":"no acl matches"));
				DEBUG(DEBUG_XFRD,1, (LOG_INFO, "axfr refused, %s",
						acl?"blocked":"no acl matches"));
				RCODE_SET(q->packet, RCODE_REFUSE);
				return QUERY_PROCESSED;
			}
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "axfr admitted acl %s %s",
				acl->ip_address_spec, acl->key_name?acl->key_name:"NOKEY"));
			return query_axfr(nsd, q);
		}
	case TYPE_IXFR:
		RCODE_SET(q->packet, RCODE_IMPL);
		return QUERY_PROCESSED;
	default:
		return QUERY_DISCARDED;
	}
}
