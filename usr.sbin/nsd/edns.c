/*
 * edns.c -- EDNS definitions (RFC 2671).
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */


#include "config.h"

#include <string.h>

#include "dns.h"
#include "edns.h"
#include "nsd.h"
#include "query.h"

void
edns_init_data(edns_data_type *data, uint16_t max_length)
{
	memset(data, 0, sizeof(edns_data_type));
	/* record type: OPT */
	data->ok[1] = (TYPE_OPT & 0xff00) >> 8;	/* type_hi */
	data->ok[2] = TYPE_OPT & 0x00ff;	/* type_lo */
	/* udp payload size */
	data->ok[3] = (max_length & 0xff00) >> 8; /* size_hi */
	data->ok[4] = max_length & 0x00ff;	  /* size_lo */

	data->error[1] = (TYPE_OPT & 0xff00) >> 8;	/* type_hi */
	data->error[2] = TYPE_OPT & 0x00ff;		/* type_lo */
	data->error[3] = (max_length & 0xff00) >> 8;	/* size_hi */
	data->error[4] = max_length & 0x00ff;		/* size_lo */
	data->error[5] = 1;	/* XXX Extended RCODE=BAD VERS */
}

void
edns_init_nsid(edns_data_type *data, uint16_t nsid_len)
{
       /* add nsid length bytes */
       data->rdata_nsid[0] = ((OPT_HDR + nsid_len) & 0xff00) >> 8; /* length_hi */
       data->rdata_nsid[1] = ((OPT_HDR + nsid_len) & 0x00ff);      /* length_lo */

       /* NSID OPT HDR */
       data->nsid[0] = (NSID_CODE & 0xff00) >> 8;
       data->nsid[1] = (NSID_CODE & 0x00ff);
       data->nsid[2] = (nsid_len & 0xff00) >> 8;
       data->nsid[3] = (nsid_len & 0x00ff);
}

void
edns_init_record(edns_record_type *edns)
{
	edns->status = EDNS_NOT_PRESENT;
	edns->position = 0;
	edns->maxlen = 0;
	edns->opt_reserved_space = 0;
	edns->dnssec_ok = 0;
	edns->nsid = 0;
}

/** handle a single edns option in the query */
static int
edns_handle_option(uint16_t optcode, uint16_t optlen, buffer_type* packet,
	edns_record_type* edns, struct query* query, nsd_type* nsd)
{
	(void) query; /* in case edns options need the query structure */
	/* handle opt code and read the optlen bytes from the packet */
	switch(optcode) {
	case NSID_CODE:
		/* is NSID enabled? */
		if(nsd->nsid_len > 0) {
			edns->nsid = 1;
			/* we have to check optlen, and move the buffer along */
			buffer_skip(packet, optlen);
			/* in the reply we need space for optcode+optlen+nsid_bytes */
			edns->opt_reserved_space += OPT_HDR + nsd->nsid_len;
		} else {
			/* ignore option */
			buffer_skip(packet, optlen);
		}
		break;
	default:
		buffer_skip(packet, optlen);
		break;
	}
	return 1;
}

int
edns_parse_record(edns_record_type *edns, buffer_type *packet,
	query_type* query, nsd_type* nsd)
{
	/* OPT record type... */
	uint8_t  opt_owner;
	uint16_t opt_type;
	uint16_t opt_class;
	uint8_t  opt_version;
	uint16_t opt_flags;
	uint16_t opt_rdlen;

	edns->position = buffer_position(packet);

	if (!buffer_available(packet, (OPT_LEN + OPT_RDATA)))
		return 0;

	opt_owner = buffer_read_u8(packet);
	opt_type = buffer_read_u16(packet);
	if (opt_owner != 0 || opt_type != TYPE_OPT) {
		/* Not EDNS.  */
		buffer_set_position(packet, edns->position);
		return 0;
	}

	opt_class = buffer_read_u16(packet);
	(void)buffer_read_u8(packet); /* opt_extended_rcode */
	opt_version = buffer_read_u8(packet);
	opt_flags = buffer_read_u16(packet);
	opt_rdlen = buffer_read_u16(packet);

	if (opt_version != 0) {
		/* The only error is VERSION not implemented */
		edns->status = EDNS_ERROR;
		return 1;
	}

	if (opt_rdlen > 0) {
		if(!buffer_available(packet, opt_rdlen))
			return 0;
		/* there is more to come, read opt code */
		while(opt_rdlen >= 4) {
			uint16_t optcode = buffer_read_u16(packet);
			uint16_t optlen = buffer_read_u16(packet);
			if(opt_rdlen < 4+optlen)
				return 0; /* opt too long, formerr */
			opt_rdlen -= (4+optlen);
			if(!edns_handle_option(optcode, optlen, packet,
				edns, query, nsd))
				return 0;
		}
		if(opt_rdlen != 0)
			return 0;
	}

	edns->status = EDNS_OK;
	edns->maxlen = opt_class;
	edns->dnssec_ok = opt_flags & DNSSEC_OK_MASK;
	return 1;
}

size_t
edns_reserved_space(edns_record_type *edns)
{
	/* MIEK; when a pkt is too large?? */
	return edns->status == EDNS_NOT_PRESENT ? 0 : (OPT_LEN + OPT_RDATA + edns->opt_reserved_space);
}
