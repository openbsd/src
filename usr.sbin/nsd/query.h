/*
 * query.h -- manipulation with the queries
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef _QUERY_H_
#define _QUERY_H_

#include <assert.h>
#include <string.h>

#include "namedb.h"
#include "nsd.h"
#include "packet.h"
#include "tsig.h"

enum query_state {
	QUERY_PROCESSED,
	QUERY_DISCARDED,
	QUERY_IN_AXFR
};
typedef enum query_state query_state_type;

/* Query as we pass it around */
typedef struct query query_type;
struct query {
	/*
	 * Memory region freed whenever the query is reset.
	 */
	region_type *region;

	/*
	 * The address the query was received from.
	 */
#ifdef INET6
	struct sockaddr_storage addr;
#else
	struct sockaddr_in addr;
#endif
	socklen_t addrlen;

	/*
	 * Maximum supported query size.
	 */
	size_t maxlen;

	/*
	 * Space reserved for optional records like EDNS.
	 */
	size_t reserved_space;

	/* EDNS information provided by the client.  */
	edns_record_type edns;

	/* TSIG record information and running hash for query-response */
	tsig_record_type tsig;
	/* tsig actions can be overridden, for axfr transfer. */
	int tsig_prepare_it, tsig_update_it, tsig_sign_it;

	int tcp;
	uint16_t tcplen;

	buffer_type *packet;

	/* Normalized query domain name.  */
	const dname_type *qname;

	/* Query type and class in host byte order.  */
	uint16_t qtype;
	uint16_t qclass;

	/* The zone used to answer the query.  */
	zone_type *zone;

	/* The delegation domain, if any.  */
	domain_type *delegation_domain;

	/* The delegation NS rrset, if any.  */
	rrset_type *delegation_rrset;

	/* Original opcode.  */
	uint8_t opcode;

	/*
	 * The number of CNAMES followed.  After a CNAME is followed
	 * we no longer change the RCODE to NXDOMAIN and no longer add
	 * SOA records to the authority section in case of NXDOMAIN
	 * and NODATA.
	 * Also includes number of DNAMES followed.
	 */
	int cname_count;

	/* Used for dname compression.  */
	uint16_t     compressed_dname_count;
	domain_type *compressed_dnames[MAXRRSPP];

	 /*
	  * Indexed by domain->number, index 0 is reserved for the
	  * query name when generated from a wildcard record.
	  */
	uint16_t    *compressed_dname_offsets;
	size_t compressed_dname_offsets_size;

	/* number of temporary domains used for the query */
	size_t number_temporary_domains;

	/*
	 * Used for AXFR processing.
	 */
	int          axfr_is_done;
	zone_type   *axfr_zone;
	domain_type *axfr_current_domain;
	rrset_type  *axfr_current_rrset;
	uint16_t     axfr_current_rr;

#ifdef RATELIMIT
	/* if we encountered a wildcard, its domain */
	domain_type *wildcard_domain;
#endif
};


/* Check if the last write resulted in an overflow.  */
static inline int query_overflow(struct query *q);

/*
 * Store the offset of the specified domain in the dname compression
 * table.
 */
void query_put_dname_offset(struct query *query,
			    domain_type  *domain,
			    uint16_t      offset);
/*
 * Lookup the offset of the specified domain in the dname compression
 * table.  Offset 0 is used to indicate the domain is not yet in the
 * compression table.
 */
static inline
uint16_t query_get_dname_offset(struct query *query, domain_type *domain)
{
	return query->compressed_dname_offsets[domain->number];
}

/*
 * Remove all compressed dnames that have an offset that points beyond
 * the end of the current answer.  This must be done after some RRs
 * are truncated and before adding new RRs.  Otherwise dnames may be
 * compressed using truncated data!
 */
void query_clear_dname_offsets(struct query *query, size_t max_offset);

/*
 * Clear the compression tables.
 */
void query_clear_compression_tables(struct query *query);

/*
 * Enter the specified domain into the compression table starting at
 * the specified offset.
 */
void query_add_compression_domain(struct query *query,
				  domain_type  *domain,
				  uint16_t      offset);


/*
 * Create a new query structure.
 */
query_type *query_create(region_type *region,
			 uint16_t *compressed_dname_offsets,
			 size_t compressed_dname_size);

/*
 * Reset a query structure so it is ready for receiving and processing
 * a new query.
 */
void query_reset(query_type *query, size_t maxlen, int is_tcp);

/*
 * Process a query and write the response in the query I/O buffer.
 */
query_state_type query_process(query_type *q, nsd_type *nsd);

/*
 * Prepare the query structure for writing the response. The packet
 * data up-to the current packet limit is preserved. This usually
 * includes the packet header and question section. Space is reserved
 * for the optional EDNS record, if required.
 */
void query_prepare_response(query_type *q);

/*
 * Add EDNS0 information to the response if required.
 */
void query_add_optional(query_type *q, nsd_type *nsd);

/*
 * Write an error response into the query structure with the indicated
 * RCODE.
 */
query_state_type query_error(query_type *q, nsd_rc_type rcode);

static inline int
query_overflow(query_type *q)
{
	return buffer_position(q->packet) > (q->maxlen - q->reserved_space);
}
#endif /* _QUERY_H_ */
