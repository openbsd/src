/*
 * packet.c -- low-level DNS packet encoding and decoding functions.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <string.h>

#include "packet.h"
#include "query.h"
#include "rdata.h"

static void
encode_dname(query_type *q, domain_type *domain)
{
	while (domain->parent && query_get_dname_offset(q, domain) == 0) {
		query_put_dname_offset(q, domain, buffer_position(q->packet));
		DEBUG(DEBUG_NAME_COMPRESSION, 2,
		      (LOG_INFO, "dname: %s, number: %lu, offset: %u\n",
		       dname_to_string(domain_dname(domain), NULL),
		       (unsigned long) domain->number,
		       query_get_dname_offset(q, domain)));
		buffer_write(q->packet, dname_name(domain_dname(domain)),
			     label_length(dname_name(domain_dname(domain))) + 1U);
		domain = domain->parent;
	}
	if (domain->parent) {
		DEBUG(DEBUG_NAME_COMPRESSION, 2,
		      (LOG_INFO, "dname: %s, number: %lu, pointer: %u\n",
		       dname_to_string(domain_dname(domain), NULL),
		       (unsigned long) domain->number,
		       query_get_dname_offset(q, domain)));
		assert(query_get_dname_offset(q, domain) <= MAX_COMPRESSION_OFFSET);
		buffer_write_u16(q->packet,
				 0xc000 | query_get_dname_offset(q, domain));
	} else {
		buffer_write_u8(q->packet, 0);
	}
}

int
packet_encode_rr(query_type *q, domain_type *owner, rr_type *rr)
{
	size_t truncation_mark;
	uint16_t rdlength = 0;
	size_t rdlength_pos;
	uint16_t j;

	assert(q);
	assert(owner);
	assert(rr);

	/*
	 * If the record does not in fit in the packet the packet size
	 * will be restored to the mark.
	 */
	truncation_mark = buffer_position(q->packet);

	encode_dname(q, owner);
	buffer_write_u16(q->packet, rr->type);
	buffer_write_u16(q->packet, rr->klass);
	buffer_write_u32(q->packet, rr->ttl);

	/* Reserve space for rdlength. */
	rdlength_pos = buffer_position(q->packet);
	buffer_skip(q->packet, sizeof(rdlength));

	for (j = 0; j < rr->rdata_count; ++j) {
		switch (rdata_atom_wireformat_type(rr->type, j)) {
		case RDATA_WF_COMPRESSED_DNAME:
			encode_dname(q, rdata_atom_domain(rr->rdatas[j]));
			break;
		case RDATA_WF_UNCOMPRESSED_DNAME:
		{
			const dname_type *dname = domain_dname(
				rdata_atom_domain(rr->rdatas[j]));
			buffer_write(q->packet,
				     dname_name(dname), dname->name_size);
			break;
		}
		default:
			buffer_write(q->packet,
				     rdata_atom_data(rr->rdatas[j]),
				     rdata_atom_size(rr->rdatas[j]));
			break;
		}
	}

	if (!query_overflow(q)) {
		rdlength = (buffer_position(q->packet) - rdlength_pos
			    - sizeof(rdlength));
		buffer_write_u16_at(q->packet, rdlength_pos, rdlength);
		return 1;
	} else {
		buffer_set_position(q->packet, truncation_mark);
		query_clear_dname_offsets(q, truncation_mark);
		assert(!query_overflow(q));
		return 0;
	}
}

int
packet_encode_rrset(query_type *query,
		    domain_type *owner,
		    rrset_type *rrset,
		    int section)
{
	uint16_t i;
	size_t truncation_mark;
	uint16_t added = 0;
	int all_added = 1;
	int truncate_rrset = (section == ANSWER_SECTION ||
							section == AUTHORITY_SECTION);
	rrset_type *rrsig;

	assert(rrset->rr_count > 0);

	truncation_mark = buffer_position(query->packet);

	for (i = 0; i < rrset->rr_count; ++i) {
		if (packet_encode_rr(query, owner, &rrset->rrs[i])) {
			++added;
		} else {
			all_added = 0;
			break;
		}
	}

	if (all_added &&
	    query->edns.dnssec_ok &&
	    zone_is_secure(rrset->zone) &&
	    rrset_rrtype(rrset) != TYPE_RRSIG &&
	    (rrsig = domain_find_rrset(owner, rrset->zone, TYPE_RRSIG)))
	{
		for (i = 0; i < rrsig->rr_count; ++i) {
			if (rr_rrsig_type_covered(&rrsig->rrs[i])
			    == rrset_rrtype(rrset))
			{
				if (packet_encode_rr(query, owner,
						     &rrsig->rrs[i]))
				{
					++added;
				} else {
					all_added = 0;
					break;
				}
			}
		}
	}

	if (!all_added && truncate_rrset) {
		/* Truncate entire RRset and set truncate flag. */
		buffer_set_position(query->packet, truncation_mark);
		query_clear_dname_offsets(query, truncation_mark);
		TC_SET(query->packet);
		added = 0;
	}

	return added;
}

int
packet_skip_dname(buffer_type *packet)
{
	while (1) {
		uint8_t label_size;
		if (!buffer_available(packet, 1))
			return 0;

		label_size = buffer_read_u8(packet);
		if (label_size == 0) {
			return 1;
		} else if ((label_size & 0xc0) != 0) {
			if (!buffer_available(packet, 1))
				return 0;
			buffer_skip(packet, 1);
			return 1;
		} else if (!buffer_available(packet, label_size)) {
			return 0;
		} else {
			buffer_skip(packet, label_size);
		}
	}
}

int
packet_skip_rr(buffer_type *packet, int question_section)
{
	if (!packet_skip_dname(packet))
		return 0;

	if (question_section) {
		if (!buffer_available(packet, 4))
			return 0;
		buffer_skip(packet, 4);
	} else {
		uint16_t rdata_size;
		if (!buffer_available(packet, 10))
			return 0;
		buffer_skip(packet, 8);
		rdata_size = buffer_read_u16(packet);
		if (!buffer_available(packet, rdata_size))
			return 0;
		buffer_skip(packet, rdata_size);
	}

	return 1;
}

rr_type *
packet_read_rr(region_type *region, domain_table_type *owners,
	       buffer_type *packet, int question_section)
{
	const dname_type *owner;
	uint16_t rdlength;
	ssize_t rdata_count;
	rdata_atom_type *rdatas;
	rr_type *result = (rr_type *) region_alloc(region, sizeof(rr_type));

	owner = dname_make_from_packet(region, packet, 1, 1);
	if (!owner || !buffer_available(packet, 2*sizeof(uint16_t))) {
		return NULL;
	}

	result->owner = domain_table_insert(owners, owner);
	result->type = buffer_read_u16(packet);
	result->klass = buffer_read_u16(packet);

	if (question_section) {
		result->ttl = 0;
		result->rdata_count = 0;
		result->rdatas = NULL;
		return result;
	} else if (!buffer_available(packet, sizeof(uint32_t) + sizeof(uint16_t))) {
		return NULL;
	}

	result->ttl = buffer_read_u32(packet);
	rdlength = buffer_read_u16(packet);

	if (!buffer_available(packet, rdlength)) {
		return NULL;
	}

	rdata_count = rdata_wireformat_to_rdata_atoms(
		region, owners, result->type, rdlength, packet, &rdatas);
	if (rdata_count == -1) {
		return NULL;
	}
	result->rdata_count = rdata_count;
	result->rdatas = rdatas;

	return result;
}

int packet_read_query_section(buffer_type *packet,
	uint8_t* dst, uint16_t* qtype, uint16_t* qclass)
{
	uint8_t *query_name = buffer_current(packet);
	uint8_t *src = query_name;
	size_t len;

	while (*src) {
		/*
		 * If we are out of buffer limits or we have a pointer
		 * in question dname or the domain name is longer than
		 * MAXDOMAINLEN ...
		 */
		if ((*src & 0xc0) ||
		    (src + *src + 2 > buffer_end(packet)) ||
		    (src + *src + 2 > query_name + MAXDOMAINLEN))
		{
			return 0;
		}
		memcpy(dst, src, *src + 1);
		dst += *src + 1;
		src += *src + 1;
	}
	*dst++ = *src++;

	/* Make sure name is not too long or we have stripped packet... */
	len = src - query_name;
	if (len > MAXDOMAINLEN ||
	    (src + 2*sizeof(uint16_t) > buffer_end(packet)))
	{
		return 0;
	}
	buffer_set_position(packet, src - buffer_begin(packet));

	*qtype = buffer_read_u16(packet);
	*qclass = buffer_read_u16(packet);
	return 1;
}
