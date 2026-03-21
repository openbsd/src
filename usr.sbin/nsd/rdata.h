/*
 * rdata.h -- RDATA conversion functions.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef RDATA_H
#define RDATA_H

#include "dns.h"
#include "namedb.h"

/* High bit of the APL length field is the negation bit.  */
#define APL_NEGATION_MASK      0x80U
#define APL_LENGTH_MASK	       (~APL_NEGATION_MASK)

/* High bit of the AMTRELAY Type byte in rdata[1] is the Discovery Optional
 * flag, the D-bit. */
#define AMTRELAY_DISCOVERY_OPTIONAL_MASK 0x80U
#define AMTRELAY_TYPE_MASK		 0x7fU

extern lookup_table_type dns_certificate_types[];
extern lookup_table_type dns_algorithms[];

/*
 * Function signature for svcparam print. Input offset is at key uint16_t
 * in rdata.
 * @param output: the string is printed to the buffer.
 * @param svcparamkey: the key that is printed.
 * @param data: the data for the svcparam, from rdata.
 * @param datalen: length of data in bytes.
 * @return false on failure.
 */
typedef int(*nsd_print_svcparam_rdata_type)(
	struct buffer* output,
	uint16_t svcparamkey,
	const uint8_t* data,
	uint16_t datalen);

typedef struct nsd_svcparam_descriptor nsd_svcparam_descriptor_type;

/* Descriptor for svcparam rdata fields. With type, name and print func. */
struct nsd_svcparam_descriptor {
	/* The svc param key */
	uint16_t key;
	/* The name of the key */
	const char *name;
	/* Print function that prints the key, from rdata. */
	nsd_print_svcparam_rdata_type print_rdata;
};

int print_unknown_rdata_field(buffer_type *output,
	const nsd_type_descriptor_type *descriptor, const rr_type *rr);
int print_unknown_rdata(buffer_type *output,
	const nsd_type_descriptor_type *descriptor, const rr_type *rr);

/* print rdata to a text string (as for a zone file) returns 0
  on a failure (bufpos is reset to original position).
  returns 1 on success, bufpos is moved. */
int print_rdata(buffer_type *output, const nsd_type_descriptor_type *descriptor,
	const rr_type *rr);

/* Read rdata for an unknown RR type. */
int32_t read_generic_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for an unknown RR type. */
void write_generic_rdata(struct query *query, const struct rr *rr);

/* Print rdata for an unknown RR type. */
int print_generic_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for an RR type with one compressed dname. */
int32_t read_compressed_name_rdata(struct domain_table *domains,
	uint16_t rdlength, struct buffer *packet, struct rr **rr);

/* Write rdata for an RR type with one compressed dname. */
void write_compressed_name_rdata(struct query *query, const struct rr *rr);

/* Print rdata for an RR type with one compressed or uncompressed dname.
 * But not a dname type literal. */
int print_name_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for an RR type with one uncompressed dname. */
int32_t read_uncompressed_name_rdata(struct domain_table *domains,
	uint16_t rdlength, struct buffer *packet, struct rr **rr);

/* Write rdata for an RR type with one uncompressed dname. */
void write_uncompressed_name_rdata(struct query *query, const struct rr *rr);

/* Read rdata for type A. */
int32_t read_a_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type A. */
int print_a_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type SOA. */
int32_t read_soa_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type SOA. */
void write_soa_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type SOA. */
int print_soa_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type SOA, on two lines, with parentheses. */
int print_soa_rdata_twoline(struct buffer *output, const struct rr *rr);

/* Read rdata for type WKS. */
int32_t read_wks_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type WKS. */
int print_wks_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type HINFO. */
int32_t read_hinfo_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type HINFO. */
int print_hinfo_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type MINFO. */
int32_t read_minfo_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type MINFO. */
void write_minfo_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type MINFO. */
int print_minfo_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type MX. */
int32_t read_mx_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type MX. */
void write_mx_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type MX. */
int print_mx_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type TXT. */
int32_t read_txt_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type TXT. */
int print_txt_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type RP. */
int32_t read_rp_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type RP. */
void write_rp_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type RP. */
int print_rp_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type AFSDB. */
int32_t read_afsdb_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type AFSDB. */
void write_afsdb_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type AFSDB. */
int print_afsdb_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type X25. */
int32_t read_x25_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type X25. */
int print_x25_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type ISDN. */
int32_t read_isdn_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type ISDN. */
int print_isdn_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type RT. */
int32_t read_rt_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type RT. */
void write_rt_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type NSAP. */
int print_nsap_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type NSAP-PTR. */
int print_nsap_ptr_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type KEY. */
int print_key_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type PX. */
int32_t read_px_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type PX. */
void write_px_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type PX. */
int print_px_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type GPOS. */
int print_gpos_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type AAAA. */
int32_t read_aaaa_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type AAAA. */
int print_aaaa_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type LOC. */
int32_t read_loc_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type LOC. */
int print_loc_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NXT. */
int32_t read_nxt_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type NXT. */
void write_nxt_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type NXT. */
int print_nxt_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type EID. */
int print_eid_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type NIMLOC. */
int print_nimloc_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type SRV. */
int32_t read_srv_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type SRV. */
void write_srv_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type SRV. */
int print_srv_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type ATMA. */
int print_atma_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NAPTR. */
int32_t read_naptr_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type NAPTR. */
void write_naptr_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type NAPTR. */
int print_naptr_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type KX. */
int32_t read_kx_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type KX. */
void write_kx_rdata(struct query *query, const struct rr *rr);

/* Read rdata for type CERT. */
int32_t read_cert_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type CERT. */
int print_cert_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type SINK. */
int print_sink_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type APL. */
int32_t read_apl_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type APL. */
int print_apl_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type DS. */
int32_t read_ds_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type DS. */
int print_ds_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type SSHFP. */
int32_t read_sshfp_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type SSHFP. */
int print_sshfp_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type IPSECKEY. */
int32_t read_ipseckey_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type IPSECKEY. */
int print_ipseckey_rdata(struct buffer *output, const struct rr *rr);

/* Determine length of IPSECKEY gateway field. */
int32_t ipseckey_gateway_length(uint16_t rdlength, const uint8_t *rdata,
	uint16_t offset, struct domain** domain);

/* Read rdata for type RRSIG. */
int32_t read_rrsig_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type RRSIG. */
int print_rrsig_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NSEC. */
int32_t read_nsec_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type NSEC. */
int print_nsec_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type DNSKEY. */
int32_t read_dnskey_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type DNSKEY. */
int print_dnskey_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type DHCID. */
int32_t read_dhcid_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type DHCID. */
int print_dhcid_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NSEC3. */
int32_t read_nsec3_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type NSEC3. */
int print_nsec3_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NSEC3PARAM. */
int32_t read_nsec3param_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type NSEC3PARAM. */
int print_nsec3param_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type TLSA. */
int32_t read_tlsa_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type TLSA. */
int print_tlsa_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type HIP. */
int32_t read_hip_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type HIP. */
int print_hip_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type RKEY. */
int32_t read_rkey_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type RKEY. */
int print_rkey_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type TALINK. */
int32_t read_talink_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type TALINK. */
int print_talink_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type OPENPGPKEY. */
int print_openpgpkey_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type CSYNC. */
int32_t read_csync_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type CSYNC. */
int print_csync_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type ZONEMD. */
int32_t read_zonemd_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type ZONEMD. */
int print_zonemd_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type SVCB. */
int32_t read_svcb_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type SVCB. */
void write_svcb_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type SVCB. */
int print_svcb_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type DSYNC. */
int32_t read_dsync_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type DSYNC. */
int print_dsync_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type NID. */
int32_t read_nid_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type NID. */
int print_nid_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type L32. */
int32_t read_l32_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type L32. */
int print_l32_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type L64. */
int32_t read_l64_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type L64. */
int print_l64_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type LP. */
int32_t read_lp_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Write rdata for type LP. */
void write_lp_rdata(struct query *query, const struct rr *rr);

/* Print rdata for type LP. */
int print_lp_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type EUI48. */
int32_t read_eui48_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type EUI48. */
int print_eui48_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type EUI64. */
int32_t read_eui64_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type EUI64. */
int print_eui64_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type URI. */
int32_t read_uri_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type URI. */
int print_uri_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type resinfo. */
int print_resinfo_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type CAA. */
int32_t read_caa_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type CAA. */
int print_caa_rdata(struct buffer *output, const struct rr *rr);

/* Print rdata for type DOA. */
int print_doa_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type AMTRELAY. */
int32_t read_amtrelay_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type AMTRELAY. */
int print_amtrelay_rdata(struct buffer *output, const struct rr *rr);

/* Determine length of AMTRELAY relay field. */
int32_t amtrelay_relay_length(uint16_t rdlength, const uint8_t *rdata,
	uint16_t offset, struct domain** domain);

/* Print rdata for type IPN. */
int print_ipn_rdata(struct buffer *output, const struct rr *rr);

/* Read rdata for type DLV. */
int32_t read_dlv_rdata(struct domain_table *domains, uint16_t rdlength,
	struct buffer *packet, struct rr **rr);

/* Print rdata for type DLV. */
int print_dlv_rdata(struct buffer *output, const struct rr *rr);

/*
 * Look up the uncompressed wireformat length of the rdata.
 * The pointer references in it are taking up the length of their uncompressed
 * domain names. The length is without the RR's rdatalength uint16 preceding.
 * @param rr: the rr, the RR type and rdata are used.
 * @result -1 on failure, otherwise length in bytes.
 */
int32_t rr_calculate_uncompressed_rdata_length(const rr_type* rr);

/*
 * Write uncompressed wireformat rdata to buffer. The pointer references
 * and domains are uncompressed wireformat domain names. The uint16 rdlength
 * is not written before it.
 * @param rr: the rr, with RR type and rdata.
 * @param buf: destination.
 * @param len: length of buffer.
 */
void rr_write_uncompressed_rdata(const rr_type* rr, uint8_t* buf, size_t len);

/*
 * Look up the field length. The field length is returned as a length
 * in the rdata that is stored. For a reference, the pointer is returned too.
 * Before calling it check if the field is_optional, and rdlength is
 * reached by offset, then there are no more rdata fields.
 * Also if the index has reached the rdata field length count, fields end.
 * It checks if the field fits in the rdata buffer, failure if not.
 * Then check for domain ptr or not, and handle the field at rr->rdata+offset.
 * Continue the loop by incrementing offset with field_len, and index++.
 *
 * @param descriptor: type descriptor.
 * @param index: field index.
 * @param rr: the rr with the rdata.
 * @param offset: current position in the rdata.
 *	It is not updated, because the caller has to do that.
 * @param field_len: the field length is returned.
 * @param domain: the pointer is returned when the field is a reference.
 * @return false on failure, when the rdata stored is badly formatted, like
 *	the rdata buffer is too short.
 */
int lookup_rdata_field_entry(const nsd_type_descriptor_type* descriptor,
	size_t index, const rr_type* rr, uint16_t offset, uint16_t* field_len,
	struct domain** domain);

/* Look up the field length. Same as lookup_rdata_field_entry, but the rdata
 * is uncompressed wireformat. The length returned skips the field in the
 * uncompressed wireformat. */
int lookup_rdata_field_entry_uncompressed_wire(
	const nsd_type_descriptor_type* descriptor, size_t index,
	const uint8_t* rdata, uint16_t rdlength, uint16_t offset,
	uint16_t* field_len, struct domain** domain);

/*
 * Compare rdata for equality. This is easier than the sorted compare,
 * it treats field types as a difference too, so a reference instead of
 * a wireformat field makes for a different RR.
 * The RRs have to be the same type already.
 * It iterates over the RR type fields. The RRs and the rdatas are the
 * namedb format, that is with references stored as pointers.
 * @param rr1: RR to compare rdata 1. The rdata can contain pointers.
 * @param rr2: RR to compare rdata 2. The rdata can contain pointers.
 * @return true if rdata is equal.
 */
int equal_rr_rdata(const nsd_type_descriptor_type *descriptor,
	const struct rr *rr1, const struct rr *rr2);

/*
 * Compare rdata for equality. Same as equal_rr_rdata, but the second
 * rdata is passed as uncompressed wireformat, the first has the in-memory
 * rdata format.
 */
int equal_rr_rdata_uncompressed_wire(const nsd_type_descriptor_type *descriptor,
	const struct rr *rr1, const uint8_t* rr2_rdata, uint16_t rr2_rdlen);

/*
 * Retrieve domain ref at an offset in the rdata.
 * @param rr: the RR to retrieve it for.
 * @param offset: where in the rdata the reference pointer is.
 * @return domain ptr.
 */
struct domain* retrieve_rdata_ref_domain_offset(const struct rr* rr,
	uint16_t offset);

/*
 * Retrieve domain ref from rdata. No offset, rdata starts with ref.
 * @param rr: the RR to retrieve it for.
 * @return domain ptr.
 */
struct domain* retrieve_rdata_ref_domain(const struct rr* rr);

/*
 * Accessor function to the domain in the rdata of the type.
 * The RR must be of the type. The type must have references.
 * @param rr: the rr with rdata
 * @return domain pointer.
 */
typedef struct domain*(*nsd_rdata_ref_domain_type)(
	const struct rr* rr);

/* Access the domain reference for type NS */
struct domain* retrieve_ns_ref_domain(const struct rr* rr);

/* Access the domain reference for type CNAME */
struct domain* retrieve_cname_ref_domain(const struct rr* rr);

/* Access the domain reference for type DNAME */
struct domain* retrieve_dname_ref_domain(const struct rr* rr);

/* Access the domain reference for type MB */
struct domain* retrieve_mb_ref_domain(const struct rr* rr);

/* Access the domain reference for type MX */
struct domain* retrieve_mx_ref_domain(const struct rr* rr);

/* Access the domain reference for type KX */
struct domain* retrieve_kx_ref_domain(const struct rr* rr);

/* Access the domain reference for type RT */
struct domain* retrieve_rt_ref_domain(const struct rr* rr);

/* Access the domain reference for type SRV */
struct domain* retrieve_srv_ref_domain(const struct rr* rr);

/* Access the domain reference for type PTR */
struct domain* retrieve_ptr_ref_domain(const struct rr* rr);

/* Access the serial number for type SOA, false if malformed. */
int retrieve_soa_rdata_serial(const struct rr* rr, uint32_t* serial);

/* Access the minimum ttl for type SOA, false if malformed. */
int retrieve_soa_rdata_minttl(const struct rr* rr, uint32_t* minttl);

/* Access the dname reference for type CNAME */
struct dname* retrieve_cname_ref_dname(const struct rr* rr);

/*
 * Access the domain name reference, that is stored as COMPRESSED_DNAME,
 * or UNCOMPRESSED DNAME, at the start of the rdata, or only part of the rdata.
 * Not for literal DNAMEs. This is similar to a pointer reference,
 * but it may be stored unaligned.
 * @param rr: the resource record.
 * @return domain pointer.
 */
static inline struct domain* rdata_domain_ref(const struct rr* rr) {
	struct domain* domain;
	assert(rr->rdlength >= (uint16_t)sizeof(void*));
	memcpy(&domain, rr->rdata, sizeof(void*));
	return domain;
}

/*
 * Access the domain name reference, that is stored as COMPRESSED_DNAME,
 * or UNCOMPRESSED DNAME, the reference is at an offset in the rdata.
 * Not for literal DNAMEs. This is similar to a pointer reference,
 * but it may be stored unaligned.
 * @param rr: the resource record
 * @param offset: where the reference is found in the rdata. Pass like 2,
 *	for type MX, or sizeof(void*) to access the second domain pointer
 *	of SOA.
 * @return domain pointer.
 */
static inline struct domain* rdata_domain_ref_offset(const struct rr* rr,
	uint16_t offset) {
	struct domain* domain;
	assert(rr->rdlength >= offset+(uint16_t)sizeof(void*));
	memcpy(&domain, rr->rdata+offset, sizeof(void*));
	return domain;
}

/* fixup usage lower for domain names in the rdata */
void rr_lower_usage(namedb_type* db, rr_type* rr);

/* return error string for read_rdata return code that is < 0 */
const char* read_rdata_fail_str(int32_t code);

#endif /* RDATA_H */
