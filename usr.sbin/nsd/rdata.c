/*
 * rdata.c -- RDATA conversion functions.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "rdata.h"
#include "zonec.h"

/* Taken from RFC 4398, section 2.1.  */
lookup_table_type dns_certificate_types[] = {
/*	0		Reserved */
	{ 1, "PKIX" },	/* X.509 as per PKIX */
	{ 2, "SPKI" },	/* SPKI cert */
	{ 3, "PGP" },	/* OpenPGP packet */
	{ 4, "IPKIX" },	/* The URL of an X.509 data object */
	{ 5, "ISPKI" },	/* The URL of an SPKI certificate */
	{ 6, "IPGP" },	/* The fingerprint and URL of an OpenPGP packet */
	{ 7, "ACPKIX" },	/* Attribute Certificate */
	{ 8, "IACPKIX" },	/* The URL of an Attribute Certificate */
	{ 253, "URI" },	/* URI private */
	{ 254, "OID" },	/* OID private */
/*	255 		Reserved */
/* 	256-65279	Available for IANA assignment */
/*	65280-65534	Experimental */
/*	65535		Reserved */
	{ 0, NULL }
};

/* Taken from RFC 2535, section 7.  */
lookup_table_type dns_algorithms[] = {
	{ 1, "RSAMD5" },	/* RFC 2537 */
	{ 2, "DH" },		/* RFC 2539 */
	{ 3, "DSA" },		/* RFC 2536 */
	{ 4, "ECC" },
	{ 5, "RSASHA1" },	/* RFC 3110 */
	{ 252, "INDIRECT" },
	{ 253, "PRIVATEDNS" },
	{ 254, "PRIVATEOID" },
	{ 0, NULL }
};

typedef int (*rdata_to_string_type)(buffer_type *output,
				    rdata_atom_type rdata,
				    rr_type *rr);

static int
rdata_dname_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	buffer_printf(output,
		      "%s",
		      dname_to_string(domain_dname(rdata_atom_domain(rdata)),
				      NULL));
	return 1;
}

static int
rdata_dns_name_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	const uint8_t *data = rdata_atom_data(rdata);
	size_t offset = 0;
	uint8_t length = data[offset];
	size_t i;

	while (length > 0)
	{
		if (offset) /* concat label */
			buffer_printf(output, ".");

		for (i = 1; i <= length; ++i) {
			char ch = (char) data[i+offset];
			if (isprint((int)ch))
				buffer_printf(output, "%c", ch);
			else
				buffer_printf(output, "\\%03u", (unsigned) ch);
		}
		/* next label */
		offset = offset+length+1;
		length = data[offset];
	}

	/* root label */
	buffer_printf(output, ".");
	return 1;
}

static int
rdata_text_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	const uint8_t *data = rdata_atom_data(rdata);
	uint8_t length = data[0];
	size_t i;

	buffer_printf(output, "\"");
	for (i = 1; i <= length; ++i) {
		char ch = (char) data[i];
		if (isprint((int)ch)) {
			if (ch == '"' || ch == '\\') {
				buffer_printf(output, "\\");
			}
			buffer_printf(output, "%c", ch);
		} else {
			buffer_printf(output, "\\%03u", (unsigned) ch);
		}
	}
	buffer_printf(output, "\"");
	return 1;
}

static int
rdata_byte_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint8_t data = *rdata_atom_data(rdata);
	buffer_printf(output, "%lu", (unsigned long) data);
	return 1;
}

static int
rdata_short_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint16_t data = read_uint16(rdata_atom_data(rdata));
	buffer_printf(output, "%lu", (unsigned long) data);
	return 1;
}

static int
rdata_long_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint32_t data = read_uint32(rdata_atom_data(rdata));
	buffer_printf(output, "%lu", (unsigned long) data);
	return 1;
}

static int
rdata_a_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int result = 0;
	char str[200];
	if (inet_ntop(AF_INET, rdata_atom_data(rdata), str, sizeof(str))) {
		buffer_printf(output, "%s", str);
		result = 1;
	}
	return result;
}

static int
rdata_aaaa_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int result = 0;
	char str[200];
	if (inet_ntop(AF_INET6, rdata_atom_data(rdata), str, sizeof(str))) {
		buffer_printf(output, "%s", str);
		result = 1;
	}
	return result;
}

static int
rdata_rrtype_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint16_t type = read_uint16(rdata_atom_data(rdata));
	buffer_printf(output, "%s", rrtype_to_string(type));
	return 1;
}

static int
rdata_algorithm_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint8_t id = *rdata_atom_data(rdata);
	lookup_table_type *alg
		= lookup_by_id(dns_algorithms, id);
	if (alg) {
		buffer_printf(output, "%s", alg->name);
	} else {
		buffer_printf(output, "%u", (unsigned) id);
	}
	return 1;
}

static int
rdata_certificate_type_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint16_t id = read_uint16(rdata_atom_data(rdata));
	lookup_table_type *type
		= lookup_by_id(dns_certificate_types, id);
	if (type) {
		buffer_printf(output, "%s", type->name);
	} else {
		buffer_printf(output, "%u", (unsigned) id);
	}
	return 1;
}

static int
rdata_period_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	uint32_t period = read_uint32(rdata_atom_data(rdata));
	buffer_printf(output, "%lu", (unsigned long) period);
	return 1;
}

static int
rdata_time_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int result = 0;
	time_t time = (time_t) read_uint32(rdata_atom_data(rdata));
	struct tm *tm = gmtime(&time);
	char buf[15];
	if (strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", tm)) {
		buffer_printf(output, "%s", buf);
		result = 1;
	}
	return result;
}

static int
rdata_base32_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int length;
	size_t size = rdata_atom_size(rdata);
	if(size == 0) {
		buffer_write(output, "-", 1);
		return 1;
	}
	size -= 1; /* remove length byte from count */
	buffer_reserve(output, size * 2 + 1);
	length = b32_ntop(rdata_atom_data(rdata)+1, size,
			  (char *) buffer_current(output), size * 2);
	if (length > 0) {
		buffer_skip(output, length);
	}
	return length != -1;
}

static int
rdata_base64_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int length;
	size_t size = rdata_atom_size(rdata);
	buffer_reserve(output, size * 2 + 1);
	length = b64_ntop(rdata_atom_data(rdata), size,
			  (char *) buffer_current(output), size * 2);
	if (length > 0) {
		buffer_skip(output, length);
	}
	return length != -1;
}

static void
hex_to_string(buffer_type *output, const uint8_t *data, size_t size)
{
	static const char hexdigits[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};
	size_t i;

	buffer_reserve(output, size * 2);
	for (i = 0; i < size; ++i) {
		uint8_t octet = *data++;
		buffer_write_u8(output, hexdigits[octet >> 4]);
		buffer_write_u8(output, hexdigits[octet & 0x0f]);
	}
}

static int
rdata_hex_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	hex_to_string(output, rdata_atom_data(rdata), rdata_atom_size(rdata));
	return 1;
}

static int
rdata_hexlen_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	if(rdata_atom_size(rdata) <= 1) {
		/* NSEC3 salt hex can be empty */
		buffer_printf(output, "-");
		return 1;
	}
	hex_to_string(output, rdata_atom_data(rdata)+1, rdata_atom_size(rdata)-1);
	return 1;
}

static int
rdata_nsap_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	buffer_printf(output, "0x");
	hex_to_string(output, rdata_atom_data(rdata), rdata_atom_size(rdata));
	return 1;
}

static int
rdata_apl_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int result = 0;
	buffer_type packet;

	buffer_create_from(
		&packet, rdata_atom_data(rdata), rdata_atom_size(rdata));

	if (buffer_available(&packet, 4)) {
		uint16_t address_family = buffer_read_u16(&packet);
		uint8_t prefix = buffer_read_u8(&packet);
		uint8_t length = buffer_read_u8(&packet);
		int negated = length & APL_NEGATION_MASK;
		int af = -1;

		length &= APL_LENGTH_MASK;
		switch (address_family) {
		case 1: af = AF_INET; break;
		case 2: af = AF_INET6; break;
		}
		if (af != -1 && buffer_available(&packet, length)) {
			char text_address[1000];
			uint8_t address[128];
			memset(address, 0, sizeof(address));
			buffer_read(&packet, address, length);
			if (inet_ntop(af, address, text_address, sizeof(text_address))) {
				buffer_printf(output, "%s%d:%s/%d",
					      negated ? "!" : "",
					      (int) address_family,
					      text_address,
					      (int) prefix);
				result = 1;
			}
		}
	}
	return result;
}

static int
rdata_services_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	int result = 0;
	buffer_type packet;

	buffer_create_from(
		&packet, rdata_atom_data(rdata), rdata_atom_size(rdata));

	if (buffer_available(&packet, 1)) {
		uint8_t protocol_number = buffer_read_u8(&packet);
		ssize_t bitmap_size = buffer_remaining(&packet);
		uint8_t *bitmap = buffer_current(&packet);
		struct protoent *proto = getprotobynumber(protocol_number);

		if (proto) {
			int i;

			buffer_printf(output, "%s", proto->p_name);

			for (i = 0; i < bitmap_size * 8; ++i) {
				if (get_bit(bitmap, i)) {
					struct servent *service = getservbyport((int)htons(i), proto->p_name);
					if (service) {
						buffer_printf(output, " %s", service->s_name);
					} else {
						buffer_printf(output, " %d", i);
					}
				}
			}
			buffer_skip(&packet, bitmap_size);
			result = 1;
		}
	}
	return result;
}

static int
rdata_ipsecgateway_to_string(buffer_type *output, rdata_atom_type rdata, rr_type* rr)
{
	int gateway_type = rdata_atom_data(rr->rdatas[1])[0];
	switch(gateway_type) {
	case IPSECKEY_NOGATEWAY:
		buffer_printf(output, ".");
		break;
	case IPSECKEY_IP4:
		rdata_a_to_string(output, rdata, rr);
		break;
	case IPSECKEY_IP6:
		rdata_aaaa_to_string(output, rdata, rr);
		break;
	case IPSECKEY_DNAME:
		rdata_dname_to_string(output, rdata, rr);
		break;
	default:
		return 0;
	}
	return 1;
}

static int
rdata_nxt_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	size_t i;
	uint8_t *bitmap = rdata_atom_data(rdata);
	size_t bitmap_size = rdata_atom_size(rdata);

	for (i = 0; i < bitmap_size * 8; ++i) {
		if (get_bit(bitmap, i)) {
			buffer_printf(output, "%s ", rrtype_to_string(i));
		}
	}

	buffer_skip(output, -1);

	return 1;
}

static int
rdata_nsec_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
	size_t saved_position = buffer_position(output);
	buffer_type packet;
	int insert_space = 0;

	buffer_create_from(
		&packet, rdata_atom_data(rdata), rdata_atom_size(rdata));

	while (buffer_available(&packet, 2)) {
		uint8_t window = buffer_read_u8(&packet);
		uint8_t bitmap_size = buffer_read_u8(&packet);
		uint8_t *bitmap = buffer_current(&packet);
		int i;

		if (!buffer_available(&packet, bitmap_size)) {
			buffer_set_position(output, saved_position);
			return 0;
		}

		for (i = 0; i < bitmap_size * 8; ++i) {
			if (get_bit(bitmap, i)) {
				buffer_printf(output,
					      "%s%s",
					      insert_space ? " " : "",
					      rrtype_to_string(
						      window * 256 + i));
				insert_space = 1;
			}
		}
		buffer_skip(&packet, bitmap_size);
	}

	return 1;
}

static int
rdata_loc_to_string(buffer_type *ATTR_UNUSED(output),
		    rdata_atom_type ATTR_UNUSED(rdata),
		    rr_type* ATTR_UNUSED(rr))
{
	/*
	 * Returning 0 forces the record to be printed in unknown
	 * format.
	 */
	return 0;
}

static int
rdata_unknown_to_string(buffer_type *output, rdata_atom_type rdata,
	rr_type* ATTR_UNUSED(rr))
{
 	uint16_t size = rdata_atom_size(rdata);
 	buffer_printf(output, "\\# %lu ", (unsigned long) size);
	hex_to_string(output, rdata_atom_data(rdata), size);
	return 1;
}

static rdata_to_string_type rdata_to_string_table[RDATA_ZF_UNKNOWN + 1] = {
	rdata_dname_to_string,
	rdata_dns_name_to_string,
	rdata_text_to_string,
	rdata_byte_to_string,
	rdata_short_to_string,
	rdata_long_to_string,
	rdata_a_to_string,
	rdata_aaaa_to_string,
	rdata_rrtype_to_string,
	rdata_algorithm_to_string,
	rdata_certificate_type_to_string,
	rdata_period_to_string,
	rdata_time_to_string,
	rdata_base64_to_string,
	rdata_base32_to_string,
	rdata_hex_to_string,
	rdata_hexlen_to_string,
	rdata_nsap_to_string,
	rdata_apl_to_string,
	rdata_ipsecgateway_to_string,
	rdata_services_to_string,
	rdata_nxt_to_string,
	rdata_nsec_to_string,
	rdata_loc_to_string,
	rdata_unknown_to_string
};

int
rdata_atom_to_string(buffer_type *output, rdata_zoneformat_type type,
		     rdata_atom_type rdata, rr_type* record)
{
	return rdata_to_string_table[type](output, rdata, record);
}

ssize_t
rdata_wireformat_to_rdata_atoms(region_type *region,
				domain_table_type *owners,
				uint16_t rrtype,
				uint16_t data_size,
				buffer_type *packet,
				rdata_atom_type **rdatas)
{
	size_t end = buffer_position(packet) + data_size;
	ssize_t i;
	rdata_atom_type temp_rdatas[MAXRDATALEN];
	rrtype_descriptor_type *descriptor = rrtype_descriptor_by_type(rrtype);
	region_type *temp_region;

	assert(descriptor->maximum <= MAXRDATALEN);

	if (!buffer_available(packet, data_size)) {
		return -1;
	}

	temp_region = region_create(xalloc, free);

	for (i = 0; i < descriptor->maximum; ++i) {
		int is_domain = 0;
		int is_normalized = 0;
		int is_wirestore = 0;
		size_t length = 0;
		int required = i < descriptor->minimum;

		switch (rdata_atom_wireformat_type(rrtype, i)) {
		case RDATA_WF_COMPRESSED_DNAME:
		case RDATA_WF_UNCOMPRESSED_DNAME:
			is_domain = 1;
			is_normalized = 1;
			break;
		case RDATA_WF_LITERAL_DNAME:
			is_domain = 1;
			is_wirestore = 1;
			break;
		case RDATA_WF_BYTE:
			length = sizeof(uint8_t);
			break;
		case RDATA_WF_SHORT:
			length = sizeof(uint16_t);
			break;
		case RDATA_WF_LONG:
			length = sizeof(uint32_t);
			break;
		case RDATA_WF_TEXT:
		case RDATA_WF_BINARYWITHLENGTH:
			/* Length is stored in the first byte.  */
			length = 1;
			if (buffer_position(packet) + length <= end) {
				length += buffer_current(packet)[length - 1];
			}
			break;
		case RDATA_WF_A:
			length = sizeof(in_addr_t);
			break;
		case RDATA_WF_AAAA:
			length = IP6ADDRLEN;
			break;
		case RDATA_WF_BINARY:
			/* Remaining RDATA is binary.  */
			length = end - buffer_position(packet);
			break;
		case RDATA_WF_APL:
			length = (sizeof(uint16_t)    /* address family */
				  + sizeof(uint8_t)   /* prefix */
				  + sizeof(uint8_t)); /* length */
			if (buffer_position(packet) + length <= end) {
				/* Mask out negation bit.  */
				length += (buffer_current(packet)[length - 1]
					   & APL_LENGTH_MASK);
			}
			break;
		case RDATA_WF_IPSECGATEWAY:
			switch(rdata_atom_data(temp_rdatas[1])[0]) /* gateway type */ {
			default:
			case IPSECKEY_NOGATEWAY:
				length = 0;
				break;
			case IPSECKEY_IP4:
				length = IP4ADDRLEN;
				break;
			case IPSECKEY_IP6:
				length = IP6ADDRLEN;
				break;
			case IPSECKEY_DNAME:
				is_domain = 1;
				is_normalized = 1;
				is_wirestore = 1;
				break;
			}
			break;
		}

		if (is_domain) {
			const dname_type *dname;

			if (!required && buffer_position(packet) == end) {
				break;
			}

			dname = dname_make_from_packet(
				temp_region, packet, 1, is_normalized);
			if (!dname || buffer_position(packet) > end) {
				/* Error in domain name.  */
				region_destroy(temp_region);
				return -1;
			}
			if(is_wirestore) {
				temp_rdatas[i].data = (uint16_t *) region_alloc(
                                	region, sizeof(uint16_t) + dname->name_size);
				temp_rdatas[i].data[0] = dname->name_size;
				memcpy(temp_rdatas[i].data+1, dname_name(dname),
					dname->name_size);
			} else
				temp_rdatas[i].domain
					= domain_table_insert(owners, dname);
		} else {
			if (buffer_position(packet) + length > end) {
				if (required) {
					/* Truncated RDATA.  */
					region_destroy(temp_region);
					return -1;
				} else {
					break;
				}
			}

			temp_rdatas[i].data = (uint16_t *) region_alloc(
				region, sizeof(uint16_t) + length);
			temp_rdatas[i].data[0] = length;
			buffer_read(packet, temp_rdatas[i].data + 1, length);
		}
	}

	if (buffer_position(packet) < end) {
		/* Trailing garbage.  */
		region_destroy(temp_region);
		return -1;
	}

	*rdatas = (rdata_atom_type *) region_alloc_init(
		region, temp_rdatas, i * sizeof(rdata_atom_type));
	region_destroy(temp_region);
	return i;
}

size_t
rdata_maximum_wireformat_size(rrtype_descriptor_type *descriptor,
			      size_t rdata_count,
			      rdata_atom_type *rdatas)
{
	size_t result = 0;
	size_t i;
	for (i = 0; i < rdata_count; ++i) {
		if (rdata_atom_is_domain(descriptor->type, i)) {
			result += domain_dname(rdata_atom_domain(rdatas[i]))->name_size;
		} else {
			result += rdata_atom_size(rdatas[i]);
		}
	}
	return result;
}

int
rdata_atoms_to_unknown_string(buffer_type *output,
			      rrtype_descriptor_type *descriptor,
			      size_t rdata_count,
			      rdata_atom_type *rdatas)
{
	size_t i;
	size_t size =
		rdata_maximum_wireformat_size(descriptor, rdata_count, rdatas);
	buffer_printf(output, " \\# %lu ", (unsigned long) size);
	for (i = 0; i < rdata_count; ++i) {
		if (rdata_atom_is_domain(descriptor->type, i)) {
			const dname_type *dname =
				domain_dname(rdata_atom_domain(rdatas[i]));
			hex_to_string(
				output, dname_name(dname), dname->name_size);
		} else {
			hex_to_string(output, rdata_atom_data(rdatas[i]),
				rdata_atom_size(rdatas[i]));
		}
	}
	return 1;
}

int
print_rdata(buffer_type *output, rrtype_descriptor_type *descriptor,
	    rr_type *record)
{
	size_t i;
	size_t saved_position = buffer_position(output);

	for (i = 0; i < record->rdata_count; ++i) {
		if (i == 0) {
			buffer_printf(output, "\t");
		} else if (descriptor->type == TYPE_SOA && i == 2) {
			buffer_printf(output, " (\n\t\t");
		} else {
			buffer_printf(output, " ");
		}
		if (!rdata_atom_to_string(
			    output,
			    (rdata_zoneformat_type) descriptor->zoneformat[i],
			    record->rdatas[i], record))
		{
			buffer_set_position(output, saved_position);
			return 0;
		}
	}
	if (descriptor->type == TYPE_SOA) {
		buffer_printf(output, " )");
	}

	return 1;
}


