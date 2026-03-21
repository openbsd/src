/*
 * dns.h -- DNS definitions.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef DNS_H
#define DNS_H
struct rr;
struct buffer;
struct domain;
struct domain_table;
struct query;

enum rr_section {
	QUESTION_SECTION,
	ANSWER_SECTION,
	AUTHORITY_SECTION,
	/*
	 * Use a split authority section to ensure that optional
	 * NS RRsets in the response can be omitted.
	 */
	OPTIONAL_AUTHORITY_SECTION,
	ADDITIONAL_SECTION,
	/*
	 * Use a split additional section to ensure A records appear
	 * before any AAAA records (this is recommended practice to
	 * avoid truncating the additional section for IPv4 clients
	 * that do not specify EDNS0), and AAAA records before other
	 * types of additional records (such as X25 and ISDN).
	 * Encode_answer sets the ARCOUNT field of the response packet
	 * correctly.
	 */
	ADDITIONAL_A_SECTION = ADDITIONAL_SECTION,
	ADDITIONAL_AAAA_SECTION,
	ADDITIONAL_OTHER_SECTION,

	RR_SECTION_COUNT
};
typedef enum rr_section rr_section_type;

/* Possible OPCODE values */
#define OPCODE_QUERY		0 	/* a standard query (QUERY) */
#define OPCODE_IQUERY		1 	/* an inverse query (IQUERY) */
#define OPCODE_STATUS		2 	/* a server status request (STATUS) */
#define OPCODE_NOTIFY		4 	/* NOTIFY */
#define OPCODE_UPDATE		5 	/* Dynamic update */

/* Possible RCODE values */
#define RCODE_OK		0 	/* No error condition */
#define RCODE_FORMAT		1 	/* Format error */
#define RCODE_SERVFAIL		2 	/* Server failure */
#define RCODE_NXDOMAIN		3 	/* Name Error */
#define RCODE_IMPL		4 	/* Not implemented */
#define RCODE_REFUSE		5 	/* Refused */
#define RCODE_YXDOMAIN		6	/* name should not exist */
#define RCODE_YXRRSET		7	/* rrset should not exist */
#define RCODE_NXRRSET		8	/* rrset does not exist */
#define RCODE_NOTAUTH		9	/* server not authoritative */
#define RCODE_NOTZONE		10	/* name not inside zone */

/* Standardized NSD return code.  Partially maps to DNS RCODE values.  */
enum nsd_rc
{
	/* Discard the client request.  */
	NSD_RC_DISCARD  = -1,
	/* OK, continue normal processing.  */
	NSD_RC_OK       = RCODE_OK,
	/* Return the appropriate error code to the client.  */
	NSD_RC_FORMAT   = RCODE_FORMAT,
	NSD_RC_SERVFAIL = RCODE_SERVFAIL,
	NSD_RC_NXDOMAIN = RCODE_NXDOMAIN,
	NSD_RC_IMPL     = RCODE_IMPL,
	NSD_RC_REFUSE   = RCODE_REFUSE,
	NSD_RC_NOTAUTH  = RCODE_NOTAUTH
};
typedef enum nsd_rc nsd_rc_type;

/* RFC1035 */
#define CLASS_IN	1	/* Class IN */
#define CLASS_CS	2	/* Class CS */
#define CLASS_CH	3	/* Class CHAOS */
#define CLASS_HS	4	/* Class HS */
#define CLASS_NONE	254	/* Class NONE rfc2136 */
#define CLASS_ANY	255	/* Class ANY */

#define TYPE_A		1	/* a host address */
#define TYPE_NS		2	/* an authoritative name server */
#define TYPE_MD		3	/* a mail destination (Obsolete - use MX) */
#define TYPE_MF		4	/* a mail forwarder (Obsolete - use MX) */
#define TYPE_CNAME	5	/* the canonical name for an alias */
#define TYPE_SOA	6	/* marks the start of a zone of authority */
#define TYPE_MB		7	/* a mailbox domain name (EXPERIMENTAL) */
#define TYPE_MG		8	/* a mail group member (EXPERIMENTAL) */
#define TYPE_MR		9	/* a mail rename domain name (EXPERIMENTAL) */
#define TYPE_NULL	10	/* a null RR (EXPERIMENTAL) */
#define TYPE_WKS	11	/* a well known service description */
#define TYPE_PTR	12	/* a domain name pointer */
#define TYPE_HINFO	13	/* host information */
#define TYPE_MINFO	14	/* mailbox or mail list information */
#define TYPE_MX		15	/* mail exchange */
#define TYPE_TXT	16	/* text strings */
#define TYPE_RP		17	/* RFC1183 */
#define TYPE_AFSDB	18	/* RFC1183 */
#define TYPE_X25	19	/* RFC1183 */
#define TYPE_ISDN	20	/* RFC1183 */
#define TYPE_RT		21	/* RFC1183 */
#define TYPE_NSAP	22	/* RFC1706 (deprecated by RFC9121) */
#define TYPE_NSAP_PTR	23	/* RFC1348 (deprecated by RFC9121) */
#define TYPE_SIG	24	/* 2535typecode */
#define TYPE_KEY	25	/* 2535typecode */
#define TYPE_PX		26	/* RFC2163 */
#define TYPE_GPOS	27	/* RFC1712 */
#define TYPE_AAAA	28	/* ipv6 address */
#define TYPE_LOC	29	/* LOC record  RFC1876 */
#define TYPE_NXT	30	/* 2535typecode */
#define TYPE_EID	31	/* draft-ietf-nimrod-dns-01 */
#define TYPE_NIMLOC	32	/* draft-ietf-nimrod-dns-01 */
#define TYPE_SRV	33	/* SRV record RFC2782 */
#define TYPE_ATMA	34	/* ATM Address */
#define TYPE_NAPTR	35	/* RFC2915 */
#define TYPE_KX		36	/* RFC2230 Key Exchange Delegation Record */
#define TYPE_CERT	37	/* RFC2538 */
#define TYPE_A6		38	/* RFC2874 */
#define TYPE_DNAME	39	/* RFC2672 */
#define TYPE_SINK	40	/* draft-eastlake-kitchen-sink */
#define TYPE_OPT	41	/* Pseudo OPT record... */
#define TYPE_APL	42	/* RFC3123 */
#define TYPE_DS		43	/* RFC 4033, 4034, and 4035 */
#define TYPE_SSHFP	44	/* SSH Key Fingerprint */
#define TYPE_IPSECKEY	45	/* public key for ipsec use. RFC 4025 */
#define TYPE_RRSIG	46	/* RFC 4033, 4034, and 4035 */
#define TYPE_NSEC	47	/* RFC 4033, 4034, and 4035 */
#define TYPE_DNSKEY	48	/* RFC 4033, 4034, and 4035 */
#define TYPE_DHCID	49	/* RFC4701 DHCP information */
#define TYPE_NSEC3	50	/* NSEC3, secure denial, prevents zonewalking */
#define TYPE_NSEC3PARAM 51	/* NSEC3PARAM at zone apex nsec3 parameters */
#define TYPE_TLSA	52	/* RFC 6698 */
#define TYPE_SMIMEA	53	/* RFC 8162 */
#define TYPE_HIP	55	/* RFC 8005 */
#define TYPE_NINFO	56	/* NINFO/ninfo-completed-template */
#define TYPE_RKEY	57	/* RKEY/rkey-completed-template */
#define TYPE_TALINK	58	/* draft-ietf-dnsop-dnssec-trust-history */
#define TYPE_CDS	59	/* RFC 7344 */
#define TYPE_CDNSKEY	60	/* RFC 7344 */
#define TYPE_OPENPGPKEY 61	/* RFC 7929 */
#define TYPE_CSYNC	62	/* RFC 7477 */
#define TYPE_ZONEMD	63	/* RFC 8976 */
#define TYPE_SVCB	64	/* RFC 9460 */
#define TYPE_HTTPS	65	/* RFC 9460 */
#define TYPE_DSYNC	66	/* RFC 9859 */

#define TYPE_SPF        99      /* RFC 4408 */

#define TYPE_NID        104     /* RFC 6742 */
#define TYPE_L32        105     /* RFC 6742 */
#define TYPE_L64        106     /* RFC 6742 */
#define TYPE_LP         107     /* RFC 6742 */
#define TYPE_EUI48      108     /* RFC 7043 */
#define TYPE_EUI64      109     /* RFC 7043 */

#define TYPE_NXNAME	128	/* RFC 9824 */

#define TYPE_TSIG	250	/* RFC 2845 */
#define TYPE_IXFR	251	/* RFC 1995 */
#define TYPE_AXFR	252	/* RFC 1035, RFC 5936 */
#define TYPE_MAILB	253	/* A request for mailbox-related records (MB, MG or MR) [RFC 1035] */
#define TYPE_MAILA	254	/* A request for mail agent RRs (Obsolete - see MX) [RFC 1035] */
#define TYPE_ANY	255	/* any type (wildcard) [RFC 1035, RFC 6895] */
#define TYPE_URI	256	/* RFC 7553 */
#define TYPE_CAA	257	/* RFC 6844 */
#define TYPE_AVC	258	/* AVC/avc-completed-template */
#define TYPE_DOA	259	/* draft-durand-doa-over-dns */
#define TYPE_AMTRELAY	260	/* RFC 8777 */
#define TYPE_RESINFO	261	/* RFC 9606 */
#define TYPE_WALLET	262	/* WALLET/wallet-completed-template */
#define TYPE_CLA	263	/* CLA/cla-completed-template */
#define TYPE_IPN	264	/* IPN/ipn-completed-template draft-johnson-dns-ipn-cla-07 */

#define TYPE_TA		32768	/* http://www.watson.org/~weiler/INI1999-19.pdf */
#define TYPE_DLV	32769	/* RFC 4431 */

#define SVCB_KEY_MANDATORY		0
#define SVCB_KEY_ALPN			1
#define SVCB_KEY_NO_DEFAULT_ALPN	2
#define SVCB_KEY_PORT			3
#define SVCB_KEY_IPV4HINT		4
#define SVCB_KEY_ECH			5
#define SVCB_KEY_IPV6HINT		6
#define SVCB_KEY_DOHPATH		7
#define SVCB_KEY_OHTTP			8
#define SVCB_KEY_TLS_SUPPORTED_GROUPS	9

#define MAXLABELLEN	63
#define MAXDOMAINLEN	255

#define MAX_RDLENGTH	65535

/* Maximum size of a single RR.  */
#define MAX_RR_SIZE \
	(MAXDOMAINLEN + sizeof(uint32_t) + 4*sizeof(uint16_t) + MAX_RDLENGTH)

#define IP4ADDRLEN	(32/8)
#define IP6ADDRLEN	(128/8)
#define EUI48ADDRLEN	(48/8)
#define EUI64ADDRLEN	(64/8)

#define NSEC3_HASH_LEN 20

/*
 * The following RDATA values are used in nsd_rdata_descriptor.length to
 * indicate a specialized value. They are negative, the normal lengths
 * are 0..65535 and length is int32_t.
 */
/* The rdata is a compressed domain name. In namedb it is a reference
 * with a pointer to struct domain. On the wire, the name can be a
 * compressed name. The pointer is stored in line and is likely unaligned. */
#define RDATA_COMPRESSED_DNAME -1
/* The rdata is an uncompressed domain name. In namedb it is a reference
 * with a pointer to struct domain. The pointer is stored in line and is
 * likely unaligned. */
#define RDATA_UNCOMPRESSED_DNAME -2
/* The rdata is a literal domain name. It is not a reference to struct
 * domain, and stored as uncompressed wireformat octets. */
#define RDATA_LITERAL_DNAME -3
/* The rdata is a string. It starts with a uint8_t length byte. */
#define RDATA_STRING -4
/* The rdata is binary. It starts with a uint8_t length byte. */
#define RDATA_BINARY -5
/* The rdata is of type IPSECGATEWAY because of its encoding elsewhere in
 * the RR. */
#define RDATA_IPSECGATEWAY -6
/* The rdata is the remainder of the record, to the end of the bytes, possibly
 * zero bytes. The length of the field is determined by the rdata length. */
#define RDATA_REMAINDER -7
/* The rdata is of type AMTRELAYRELAY because of its encoding elsewhere in
 * the RR. */
#define RDATA_AMTRELAY_RELAY -8

/*
 * Function signature to determine length of the rdata field.
 * @param rdlength: length of the input rdata.
 * @param rdata: input bytes with rdata of the RR.
 * @param offset: current byte position in rdata.
 * 	offset is required for the ipsecgateway where we need to read
 * 	a couple bytes back
 * @param domain: this value can be returned as NULL, in which case the
 *	function return value is a length in bytes in wireformat.
 *	If this value is returned nonNULL, it is the special reference
 *	object that needs different treatment. The function return value
 *	is the length that needs to be skipped in rdata to get past the
 *	field, that is a reference when that is a pointer.
 *	For other types of objects an additional function argument could be
 *	added, and then handling in the caller.
 * @return length in bytes. Or -1 on failure, like rdata length too short.
 */
typedef int32_t(*nsd_rdata_field_length_type)(
	uint16_t rdlength,
	const uint8_t *rdata,
	uint16_t offset,
	struct domain** domain);

typedef struct nsd_rdata_descriptor nsd_rdata_descriptor_type;

/*
 * Descriptor table. For DNS RRTypes has information on the resource record
 * type. the descriptor table shouldn't be used for validation.
 * the read function will take care of that. it's used for
 * implementing copy functions that are very specific, but where
 * the data has already been checked for validity.
 */
struct nsd_rdata_descriptor {
	/* Field name of the rdata, like 'primary server'. */
	const char *name;

	/* If the field is optional. When the field is not present, eg. no
	 * bytes of rdata, and it is optional, this is fine and the rdata
	 * is shorter. Also no following rdatas after it. Non optional
	 * rdatas must be present. */
	int is_optional;

	/* The length, in bytes, of the rdata field. Can be set to
	 * a specialized value, like RDATA_COMPRESSED_DNAME,
	 * RDATA_STRING, ..., if there is a length function, that is used.
	 * That is for any type where the length depends on a value in
	 * the rdata itself. */
	int32_t length;

	/* Determine size of rdata field. Returns the size of uncompressed
	 * rdata on the wire, or -1 on failure, like when it is malformed.
	 * So for references this is a different number. Used for ipseckey
	 * gateway, because the type depends on earlier data. Also amtrelay
	 * relay. This function takes the in-memory rdata representation.
	 * If the field has a special object, return -1 on failure or the
	 * length of the object in the rdata, with domain ptr returned to
	 * the special object. */
	nsd_rdata_field_length_type calculate_length;

	/* Determine size of rdata field. Like calculate_length, but this
	 * function takes uncompressed wireformat in the rdata that is passed.
	 */
	nsd_rdata_field_length_type calculate_length_uncompressed_wire;
};

typedef struct nsd_type_descriptor nsd_type_descriptor_type;
struct nsd_type_descriptor;

/* The rdata is malformed. The wireformat is not correct.
 * NSD cannot store a wireformat with a malformed domain name, when that
 * name needs to become a reference to struct domain. */
#define MALFORMED -1
/* The rdata is not read, it is truncated, some was copied, but then
 * an error occurred, like memory allocation failure. */
#define TRUNCATED -2

/*
 * Function signature to read rdata. From a packet into memory.
 * @param domains: the domain table.
 * @param rdlength: the length of the rdata in the packet.
 * @param packet: the packet.
 * @param rr: an RR is returned.
 * @return the number of bytes that are read from the packet. Or negative
 *	for an error, like MALFORMED, TRUNCATED.
 */
typedef int32_t(*nsd_read_rdata_type)(
	struct domain_table *domains,
	uint16_t rdlength,
	struct buffer *packet,
	struct rr **rr);

/*
 * Function signature to write rdata. From memory to an answer.
 * @param query: the query that is answered.
 * @param rr: rr to add to the output, in query.packet. It prints the rdata
 *	wireformat to the packet, compressed if needed, not including the
 *	rdlength before the rdata.
 */
typedef void(*nsd_write_rdata_type)(
	struct query *query,
	const struct rr *rr);

/*
 * Function signature to print rdata. The string is in the buffer.
 * The printed string starts with the rdata text. It does not have a newline
 * at end. No space is put before it.
 * @param output: buffer with string on return. The position is moved.
 * @param rr: the record to print the rdata for.
 * @return false on failure. The wireformat can not be printed in the
 *	nice output format.
 */
typedef int(*nsd_print_rdata_type)(
	struct buffer *output,
	const struct rr *rr);

/*
 * Descriptor for a DNS resource record type.
 * There are conversion routines per type, for wire-to-internal,
 * internal-to-wire, and internal-to-text. To stop an explosion in code,
 * there is an rdata field descriptor array to convert between more formats.
 * For internal to compressed wire format we implement a specialized routine so
 * that answering of queries is as optimal as can be.
 * Other formats are conversion from uncompressed wire format to compressed
 * wire format. For conversion from uncompressed or compressed wire format
 * to internal the same import routines can be used, by using a packet buffer
 * and dname_make_from_packet.
 */
struct nsd_type_descriptor {
	/* The RRType number */
	uint16_t type;
	/* Mnemonic. */
	const char *name;
	/* Whether internal RDATA contains direct pointers.
	 * This means the namedb memory contains an in line pointer to
	 * struct domain for domain names. */
	int has_references;
	/* Whether RDATA contains compressible names. The output can be
	 * compressed on the packet. If true, also has_references is true,
	 * for the packet encode routine, that uses the references. */
	int is_compressible;
	/* The type has domain names, like literal dnames in the format. */
	int has_dnames;
	/* Read function that copies rdata for this type to struct rr. */
	nsd_read_rdata_type read_rdata;
	/* Write function, that copies rdata from struct rr to packet. */
	nsd_write_rdata_type write_rdata;
	/* Print function, that writes the struct rr to string. */
	nsd_print_rdata_type print_rdata;
	/* Description of the rdata fields. Used by functions that need
	 * to iterate over the fields. There are binary fields and
	 * references, and wireformat domain names. */
	struct {
		/* Length of the fields array. */
		size_t length;
		/* The rdata field descriptors. */
		const nsd_rdata_descriptor_type *fields;
	} rdata;
};

/* The length of the RRTYPE descriptors arrary */
#define RRTYPE_DESCRIPTORS_LENGTH  (TYPE_IPN + 2)

/*
 * Indexed by type.  The special type "0" can be used to get a
 * descriptor for unknown types (with one binary rdata).
 */
static inline const nsd_type_descriptor_type *nsd_type_descriptor(
	uint16_t rrtype);

const char *rrtype_to_string(uint16_t rrtype);

/*
 * Lookup the type in the ztypes lookup table.  If not found, check if
 * the type uses the "TYPExxx" notation for unknown types.
 *
 * Return 0 if no type matches.
 */
uint16_t rrtype_from_string(const char *name);

const char *rrclass_to_string(uint16_t rrclass);
uint16_t rrclass_from_string(const char *name);

/* The type descriptors array of length RRTYPE_DESCRIPTORS_LENGTH. */
extern const nsd_type_descriptor_type type_descriptors[];

static inline const nsd_type_descriptor_type *
nsd_type_descriptor(uint16_t rrtype)
{
	if (rrtype <= TYPE_IPN)
		return &type_descriptors[rrtype];
	if (rrtype == TYPE_TA)
		return &type_descriptors[TYPE_IPN + 1];
	if (rrtype == TYPE_DLV)
		return &type_descriptors[TYPE_IPN + 2];
	return &type_descriptors[0];
}

#endif /* DNS_H */
