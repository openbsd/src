/*
 * dns.c -- DNS definitions.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "dns.h"
#include "rdata.h"

/* Taken from RFC 1035, section 3.2.4.  */
static lookup_table_type dns_rrclasses[] = {
	{ CLASS_IN, "IN" },	/* the Internet */
	{ CLASS_CS, "CS" },	/* the CSNET class (Obsolete) */
	{ CLASS_CH, "CH" },	/* the CHAOS class */
	{ CLASS_HS, "HS" },	/* Hesiod */
	{ 0, NULL }
};

/* For a standard field, it is not optional, has no rdata field functions. */
#define FIELD(name, size) { name, 0 /* is_optional */, size, NULL /* calc_len_func */, NULL /* calc_len_uncompressed_wire_func */ }

/* For a field entry with all values, for optional fields, or with defined
 * rdata field functions. */
#define FIELD_ENTRY(name, is_optional, size, calc_len_func, cal_len_uncompressed_wire_func ) { name, is_optional, size, calc_len_func, cal_len_uncompressed_wire_func }

static const struct nsd_rdata_descriptor generic_rdata_fields[] = {
	FIELD("", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor a_rdata_fields[] = {
	FIELD("address", 4)
};

static const struct nsd_rdata_descriptor ns_rdata_fields[] = {
	FIELD("host", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor md_rdata_fields[] = {
	FIELD("madname", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor mf_rdata_fields[] = {
	FIELD("madname", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor cname_rdata_fields[] = {
	FIELD("host", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor soa_rdata_fields[] = {
	FIELD("primary", RDATA_COMPRESSED_DNAME),
	FIELD("mailbox", RDATA_COMPRESSED_DNAME),
	FIELD("serial", 4),
	FIELD("refresh", 4),
	FIELD("retry", 4),
	FIELD("expire", 4),
	FIELD("minimum", 4)
};

static const struct nsd_rdata_descriptor mb_rdata_fields[] = {
	FIELD("madname", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor mg_rdata_fields[] = {
	FIELD("mgmname", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor mr_rdata_fields[] = {
	FIELD("newname", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor wks_rdata_fields[] = {
	FIELD("address", 4),
	FIELD("protocol", 1),
	FIELD("bitmap", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor ptr_rdata_fields[] = {
	FIELD("ptrdname", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor hinfo_rdata_fields[] = {
	FIELD("cpu", RDATA_STRING),
	FIELD("os", RDATA_STRING)
};

static const struct nsd_rdata_descriptor minfo_rdata_fields[] = {
	FIELD("rmailbx", RDATA_COMPRESSED_DNAME),
	FIELD("emailbx", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor mx_rdata_fields[] = {
	FIELD("priority", 2),
	FIELD("hostname", RDATA_COMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor txt_rdata_fields[] = {
	FIELD("text", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor rp_rdata_fields[] = {
	FIELD("mailbox", RDATA_UNCOMPRESSED_DNAME),
	FIELD("text", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor afsdb_rdata_fields[] = {
	FIELD("subtype", 2),
	FIELD("hostname", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor x25_rdata_fields[] = {
	FIELD("address", RDATA_STRING)
};

static const struct nsd_rdata_descriptor isdn_rdata_fields[] = {
	FIELD("address", RDATA_STRING),
	FIELD_ENTRY("subaddress", 1, RDATA_STRING, NULL, NULL)
};

static const struct nsd_rdata_descriptor rt_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("hostname", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor nsap_rdata_fields[] = {
	FIELD("address", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nsap_ptr_rdata_fields[] = {
	FIELD("hostname", RDATA_STRING)
};

static const struct nsd_rdata_descriptor sig_rdata_fields[] = {
	FIELD("sigtype", 2),
	FIELD("algorithm", 1),
	FIELD("labels", 1),
	FIELD("origttl", 4),
	FIELD("expire", 4),
	FIELD("inception", 4),
	FIELD("keytag", 2),
	FIELD("signer", RDATA_LITERAL_DNAME),
	FIELD("signature", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor key_rdata_fields[] = {
	FIELD("flags", 2),
	FIELD("protocol", 1),
	FIELD("algorithm", 1),
	FIELD("publickey", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor px_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("map822", RDATA_UNCOMPRESSED_DNAME),
	FIELD("mapx400", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor gpos_rdata_fields[] = {
	FIELD("latitude", RDATA_STRING),
	FIELD("longitude", RDATA_STRING),
	FIELD("altitude", RDATA_STRING)
};

static const struct nsd_rdata_descriptor aaaa_rdata_fields[] = {
	FIELD("address", 16)
};

static const struct nsd_rdata_descriptor loc_rdata_fields[] = {
	FIELD("version", 1),
	FIELD("size", 1),
	FIELD("horizontal precision", 1),
	FIELD("vertical precision", 1),
	FIELD("latitude", 4),
	FIELD("longitude", 4),
	FIELD("altitude", 4),
};

static const struct nsd_rdata_descriptor nxt_rdata_fields[] = {
	FIELD("next domain name", RDATA_UNCOMPRESSED_DNAME),
	FIELD("type bit map", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor eid_rdata_fields[] = {
	FIELD("end point identifier", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nimloc_rdata_fields[] = {
	FIELD("nimrod locator", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor srv_rdata_fields[] = {
	FIELD("priority", 2),
	FIELD("weight", 2),
	FIELD("port", 2),
	FIELD("target", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor atma_rdata_fields[] = {
	FIELD("address", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor naptr_rdata_fields[] = {
	FIELD("order", 2),
	FIELD("preference", 2),
	FIELD("flags", RDATA_STRING),
	FIELD("services", RDATA_STRING),
	FIELD("regex", RDATA_STRING),
	FIELD("replacement", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor kx_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("exchanger", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor cert_rdata_fields[] = {
	FIELD("type", 2),
	FIELD("key tag", 2),
	FIELD("algorithm", 1),
	FIELD("certificate", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor a6_rdata_fields[] = {
	FIELD("address", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor dname_rdata_fields[] = {
	FIELD("source", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor sink_rdata_fields[] = {
	FIELD("coding", 1),
	FIELD("subcoding", 1),
	FIELD("data", RDATA_REMAINDER),
};

static const struct nsd_rdata_descriptor apl_rdata_fields[] = {
	FIELD_ENTRY("prefix", 1, RDATA_REMAINDER, NULL, NULL)
};

static const struct nsd_rdata_descriptor ds_rdata_fields[] = {
	FIELD("keytag", 2),
	FIELD("algorithm", 1),
	FIELD("digtype", 1),
	FIELD("digest", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor sshfp_rdata_fields[] = {
	FIELD("algorithm", 1),
	FIELD("ftype", 1),
	FIELD("fingerprint", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor ipseckey_rdata_fields[] = {
	FIELD("precedence", 1),
	FIELD("gateway type", 1),
	FIELD("algorithm", 1),
	FIELD_ENTRY("gateway", 0, RDATA_IPSECGATEWAY,
		ipseckey_gateway_length, ipseckey_gateway_length),
	FIELD_ENTRY("public key", 1, RDATA_REMAINDER, NULL, NULL)
};

static const struct nsd_rdata_descriptor rrsig_rdata_fields[] = {
	FIELD("rrtype", 2),
	FIELD("algorithm", 1),
	FIELD("labels", 1),
	FIELD("origttl", 4),
	FIELD("expire", 4),
	FIELD("inception", 4),
	FIELD("keytag", 2),
	FIELD("signer", RDATA_LITERAL_DNAME),
	FIELD("signature", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nsec_rdata_fields[] = {
	FIELD("next", RDATA_LITERAL_DNAME),
	FIELD("types", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor dnskey_rdata_fields[] = {
	FIELD("flags", 2),
	FIELD("protocol", 1),
	FIELD("algorithm", 1),
	FIELD("publickey", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor dhcid_rdata_fields[] = {
	FIELD("dhcpinfo", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nsec3_rdata_fields[] = {
	FIELD("algorithm", 1),
	FIELD("flags", 1),
	FIELD("iterations", 2),
	FIELD("salt", RDATA_BINARY),
	FIELD("next", RDATA_BINARY),
	FIELD("types", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nsec3param_rdata_fields[] = {
	FIELD("algorithm", 1),
	FIELD("flags", 1),
	FIELD("iterations", 2),
	FIELD("salt", RDATA_BINARY)
};

static const struct nsd_rdata_descriptor tlsa_rdata_fields[] = {
	FIELD("usage", 1),
	FIELD("selector", 1),
	FIELD("matching type", 1),
	FIELD("certificate association data", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor smimea_rdata_fields[] = {
	FIELD("usage", 1),
	FIELD("selector", 1),
	FIELD("matching type", 1),
	FIELD("certificate association data", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor hip_rdata_fields[] = {
	FIELD("hip", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor ninfo_rdata_fields[] = {
	FIELD("text", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor rkey_rdata_fields[] = {
	FIELD("flags", 2),
	FIELD("protocol", 1),
	FIELD("algorithm", 1),
	FIELD("publickey", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor talink_rdata_fields[] = {
	FIELD("start or previous", RDATA_LITERAL_DNAME),
	FIELD("end or next", RDATA_LITERAL_DNAME)
};

static const struct nsd_rdata_descriptor cds_rdata_fields[] = {
	FIELD("keytag", 2),
	FIELD("algorithm", 1),
	FIELD("digtype", 1),
	FIELD("digest", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor cdnskey_rdata_fields[] = {
	FIELD("flags", 2),
	FIELD("protocol", 1),
	FIELD("algorithm", 1),
	FIELD("publickey", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor openpgpkey_rdata_fields[] = {
	FIELD("key", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor csync_rdata_fields[] = {
	FIELD("serial", 4),
	FIELD("flags", 2),
	FIELD("types", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor zonemd_rdata_fields[] = {
	FIELD("serial", 4),
	FIELD("scheme", 1),
	FIELD("algorithm", 1),
	FIELD("digest", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor svcb_rdata_fields[] = {
	FIELD("priority", 2),
	FIELD("target", RDATA_UNCOMPRESSED_DNAME),
	FIELD("params", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor https_rdata_fields[] = {
	FIELD("priority", 2),
	FIELD("target", RDATA_UNCOMPRESSED_DNAME),
	FIELD("params", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor dsync_rdata_fields[] = {
	FIELD("rrtype", 2),
	FIELD("scheme", 1),
	FIELD("port", 2),
	FIELD("target", RDATA_LITERAL_DNAME)
};

static const struct nsd_rdata_descriptor spf_rdata_fields[] = {
	FIELD("text", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor nid_rdata_fields[] = {
	FIELD("nid", 2),
	FIELD("locator", 8)
};

static const struct nsd_rdata_descriptor l32_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("locator", 4)
};

static const struct nsd_rdata_descriptor l64_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("locator", 8)
};

static const struct nsd_rdata_descriptor lp_rdata_fields[] = {
	FIELD("preference", 2),
	FIELD("pointer", RDATA_UNCOMPRESSED_DNAME)
};

static const struct nsd_rdata_descriptor eui48_rdata_fields[] = {
	FIELD("address", 6)
};

static const struct nsd_rdata_descriptor eui64_rdata_fields[] = {
	FIELD("address", 8)
};

static const struct nsd_rdata_descriptor uri_rdata_fields[] = {
	FIELD("priority", 2),
	FIELD("weight", 2),
	FIELD("target", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor caa_rdata_fields[] = {
	FIELD("flags", 1),
	FIELD("tag", RDATA_STRING),
	FIELD("value", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor avc_rdata_fields[] = {
	FIELD("text", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor doa_rdata_fields[] = {
	FIELD("enterprise", 4),
	FIELD("type", 4),
	FIELD("location", 1),
	FIELD("media type", RDATA_STRING),
	FIELD("data", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor amtrelay_rdata_fields[] = {
	FIELD("precedence", 1),
	FIELD("discovery_type", 1),
	FIELD_ENTRY("relay", 0, RDATA_AMTRELAY_RELAY,
		amtrelay_relay_length, amtrelay_relay_length)
};

static const struct nsd_rdata_descriptor resinfo_rdata_fields[] = {
	FIELD("text", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor wallet_rdata_fields[] = {
	FIELD("wallet", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor cla_rdata_fields[] = {
	FIELD("CLA", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor ipn_rdata_fields[] = {
	FIELD("CBHE Node Number", 8)
};

static const struct nsd_rdata_descriptor ta_rdata_fields[] = {
	FIELD("key", 2),
	FIELD("algorithm", 1),
	FIELD("type", 1),
	FIELD("digest", RDATA_REMAINDER)
};

static const struct nsd_rdata_descriptor dlv_rdata_fields[] = {
	FIELD("key", 2),
	FIELD("algorithm", 1),
	FIELD("type", 1),
	FIELD("digest", RDATA_REMAINDER)
};

#define TYPE(name, code, bools, read, write, print, fields) \
  { code, name, bools, read, write, print, { sizeof(fields)/sizeof(fields[0]), fields } }

#define UNKNOWN_TYPE(code) \
  { code, NULL /* mnemonic */, 0 /* has_references */, 0 /* is_compressible */, 0 /* has_dnames */, read_generic_rdata, write_generic_rdata, print_generic_rdata, { sizeof(generic_rdata_fields)/sizeof(generic_rdata_fields[0]), generic_rdata_fields } }

/* The RR type has no references, it is a binary wireformat.
 * has_references, is_compressible, has_dnames. */
#define TYPE_HAS_NO_REFS 0, 0, 0
/* The RR type has references, it has compressed dnames. */
#define TYPE_HAS_COMPRESSED_DNAME 1, 1, 1
/* The RR type has references, it has uncompressed dnames. */
#define TYPE_HAS_UNCOMPRESSED_DNAME 1, 0, 1
/* The RR type has no references, it has literal dnames. */
#define TYPE_HAS_LITERAL_DNAME 0, 0, 1
/* Set the bools, has_references, is_compressible, has_dnames. */
#define TYPE_HAS_FLAGS(has_references, is_compressible, has_dnames) has_references, is_compressible, has_dnames

const nsd_type_descriptor_type type_descriptors[] = {
	/* 0 */
	UNKNOWN_TYPE(0), /* Type 0 - Reserved [RFC 6895] */
	/* 1 */
	TYPE("A", TYPE_A, TYPE_HAS_NO_REFS,
		read_a_rdata, write_generic_rdata,
		print_a_rdata, a_rdata_fields),
	/* 2 */
	TYPE("NS", TYPE_NS, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, ns_rdata_fields),
	/* 3 */
	TYPE("MD", TYPE_MD, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_uncompressed_name_rdata, write_uncompressed_name_rdata,
		print_name_rdata, md_rdata_fields),
	/* 4 */
	TYPE("MF", TYPE_MF, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_uncompressed_name_rdata, write_uncompressed_name_rdata,
		print_name_rdata, mf_rdata_fields),
	/* 5 */
	TYPE("CNAME", TYPE_CNAME, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, cname_rdata_fields),
	/* 6 */
	TYPE("SOA", TYPE_SOA, TYPE_HAS_COMPRESSED_DNAME,
		read_soa_rdata, write_soa_rdata,
		print_soa_rdata, soa_rdata_fields),
	/* 7 */
	TYPE("MB", TYPE_MB, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, mb_rdata_fields),
	/* 8 */
	TYPE("MG", TYPE_MG, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, mg_rdata_fields),
	/* 9 */
	TYPE("MR", TYPE_MR, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, mr_rdata_fields),
	/* 10 */
	TYPE("NULL", TYPE_NULL, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_generic_rdata, generic_rdata_fields),
	/* 11 */
	TYPE("WKS", TYPE_WKS, TYPE_HAS_NO_REFS,
		read_wks_rdata, write_generic_rdata,
		print_wks_rdata, wks_rdata_fields),
	/* 12 */
	TYPE("PTR", TYPE_PTR, TYPE_HAS_COMPRESSED_DNAME,
		read_compressed_name_rdata, write_compressed_name_rdata,
		print_name_rdata, ptr_rdata_fields),
	/* 13 */
	TYPE("HINFO", TYPE_HINFO, TYPE_HAS_NO_REFS,
		read_hinfo_rdata, write_generic_rdata,
		print_hinfo_rdata, hinfo_rdata_fields),
	/* 14 */
	TYPE("MINFO", TYPE_MINFO, TYPE_HAS_COMPRESSED_DNAME,
		read_minfo_rdata, write_minfo_rdata,
		print_minfo_rdata, minfo_rdata_fields),
	/* 15 */
	TYPE("MX", TYPE_MX, TYPE_HAS_COMPRESSED_DNAME,
		read_mx_rdata, write_mx_rdata,
		print_mx_rdata, mx_rdata_fields),
	/* 16 */
	TYPE("TXT", TYPE_TXT, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, txt_rdata_fields),
	/* 17 */
	TYPE("RP", TYPE_RP, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_rp_rdata, write_rp_rdata, print_rp_rdata,
		rp_rdata_fields),
	/* 18 */
	TYPE("AFSDB", TYPE_AFSDB, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_afsdb_rdata, write_afsdb_rdata, print_afsdb_rdata,
		afsdb_rdata_fields),
	/* 19 */
	TYPE("X25", TYPE_X25, TYPE_HAS_NO_REFS,
		read_x25_rdata, write_generic_rdata, print_x25_rdata,
		x25_rdata_fields),
	/* 20 */
	TYPE("ISDN", TYPE_ISDN, TYPE_HAS_NO_REFS,
		read_isdn_rdata, write_generic_rdata, print_isdn_rdata,
		isdn_rdata_fields),
	/* 21 */
	TYPE("RT", TYPE_RT, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_rt_rdata, write_rt_rdata, print_mx_rdata,
		rt_rdata_fields),
	/* 22 */
	TYPE("NSAP", TYPE_NSAP, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_nsap_rdata,
		nsap_rdata_fields),
	/* 23 */
	TYPE("NSAP-PTR", TYPE_NSAP_PTR, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_nsap_ptr_rdata, nsap_ptr_rdata_fields),
	/* 24 */
	TYPE("SIG", TYPE_SIG, TYPE_HAS_LITERAL_DNAME,
		read_rrsig_rdata, write_generic_rdata, print_rrsig_rdata,
		sig_rdata_fields),
	/* 25 */
	TYPE("KEY", TYPE_KEY, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_key_rdata,
		key_rdata_fields),
	/* 26 */
	TYPE("PX", TYPE_PX, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_px_rdata, write_px_rdata, print_px_rdata,
		px_rdata_fields),
	/* 27 */
	TYPE("GPOS", TYPE_GPOS, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_gpos_rdata,
		gpos_rdata_fields),
	/* 28 */
	TYPE("AAAA", TYPE_AAAA, TYPE_HAS_NO_REFS,
		read_aaaa_rdata, write_generic_rdata, print_aaaa_rdata,
		aaaa_rdata_fields),
	/* 29 */
	TYPE("LOC", TYPE_LOC, TYPE_HAS_NO_REFS,
		read_loc_rdata, write_generic_rdata, print_loc_rdata,
		loc_rdata_fields),
	/* 30 */
	TYPE("NXT", TYPE_NXT, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_nxt_rdata, write_nxt_rdata, print_nxt_rdata,
		nxt_rdata_fields),
	/* 31 */
	TYPE("EID", TYPE_EID, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_eid_rdata,
		eid_rdata_fields),
	/* 32 */
	TYPE("NIMLOC", TYPE_NIMLOC, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_nimloc_rdata,
		nimloc_rdata_fields),
	/* 33 */
	TYPE("SRV", TYPE_SRV, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_srv_rdata, write_srv_rdata,
		print_srv_rdata, srv_rdata_fields),
	/* 34 */
	TYPE("ATMA", TYPE_ATMA, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_atma_rdata,
		atma_rdata_fields),
	/* 35 */
	TYPE("NAPTR", TYPE_NAPTR, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_naptr_rdata, write_naptr_rdata,
		print_naptr_rdata, naptr_rdata_fields),
	/* 36 */
	TYPE("KX", TYPE_KX, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_kx_rdata, write_kx_rdata,
		print_mx_rdata, kx_rdata_fields),
	/* 37 */
	TYPE("CERT", TYPE_CERT, TYPE_HAS_NO_REFS,
		read_cert_rdata, write_generic_rdata,
		print_cert_rdata, cert_rdata_fields),
	/* 38 */
	TYPE("A6", TYPE_A6, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_generic_rdata, a6_rdata_fields),
	/* 39 */
	TYPE("DNAME", TYPE_DNAME, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_uncompressed_name_rdata, write_uncompressed_name_rdata,
		print_name_rdata, dname_rdata_fields),
	/* 40 */
	TYPE("SINK", TYPE_SINK, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata, print_sink_rdata,
		sink_rdata_fields),

	UNKNOWN_TYPE(41), /* Type 41 - OPT */

	/* 42 */
	TYPE("APL", TYPE_APL, TYPE_HAS_NO_REFS,
		read_apl_rdata, write_generic_rdata,
		print_apl_rdata, apl_rdata_fields),
	/* 43 */
	TYPE("DS", TYPE_DS, TYPE_HAS_NO_REFS,
		read_ds_rdata, write_generic_rdata,
		print_ds_rdata, ds_rdata_fields),
	/* 44 */
	TYPE("SSHFP", TYPE_SSHFP, TYPE_HAS_NO_REFS,
		read_sshfp_rdata, write_generic_rdata,
		print_sshfp_rdata, sshfp_rdata_fields),
	/* 45 */
	TYPE("IPSECKEY", TYPE_IPSECKEY, TYPE_HAS_LITERAL_DNAME,
		read_ipseckey_rdata, write_generic_rdata,
		print_ipseckey_rdata, ipseckey_rdata_fields),
	/* 46 */
	TYPE("RRSIG", TYPE_RRSIG, TYPE_HAS_LITERAL_DNAME,
		read_rrsig_rdata, write_generic_rdata,
		print_rrsig_rdata, rrsig_rdata_fields),
	/* 47 */
	TYPE("NSEC", TYPE_NSEC, TYPE_HAS_LITERAL_DNAME,
		read_nsec_rdata, write_generic_rdata,
		print_nsec_rdata, nsec_rdata_fields),
	/* 48 */
	TYPE("DNSKEY", TYPE_DNSKEY, TYPE_HAS_NO_REFS,
		read_dnskey_rdata, write_generic_rdata,
		print_dnskey_rdata, dnskey_rdata_fields),
	/* 49 */
	TYPE("DHCID", TYPE_DHCID, TYPE_HAS_NO_REFS,
		read_dhcid_rdata, write_generic_rdata,
		print_dhcid_rdata, dhcid_rdata_fields),
	/* 50 */
	TYPE("NSEC3", TYPE_NSEC3, TYPE_HAS_NO_REFS,
		read_nsec3_rdata, write_generic_rdata,
		print_nsec3_rdata, nsec3_rdata_fields),
	/* 51 */
	TYPE("NSEC3PARAM", TYPE_NSEC3PARAM, TYPE_HAS_NO_REFS,
		read_nsec3param_rdata, write_generic_rdata,
		print_nsec3param_rdata, nsec3param_rdata_fields),
	/* 52 */
	TYPE("TLSA", TYPE_TLSA, TYPE_HAS_NO_REFS,
		read_tlsa_rdata, write_generic_rdata,
		print_tlsa_rdata, tlsa_rdata_fields),
	/* 53 */
	TYPE("SMIMEA", TYPE_SMIMEA, TYPE_HAS_NO_REFS,
		read_tlsa_rdata, write_generic_rdata,
		print_tlsa_rdata, smimea_rdata_fields),

	UNKNOWN_TYPE(54),

	/* 55 */
	TYPE("HIP", TYPE_HIP, TYPE_HAS_LITERAL_DNAME,
		read_hip_rdata, write_generic_rdata,
		print_hip_rdata, hip_rdata_fields),
	/* 56 */
	TYPE("NINFO", TYPE_NINFO, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, ninfo_rdata_fields),
	/* 57 */
	TYPE("RKEY", TYPE_RKEY, TYPE_HAS_NO_REFS,
		read_rkey_rdata, write_generic_rdata,
		print_rkey_rdata, rkey_rdata_fields),
	/* 58 */
	TYPE("TALINK", TYPE_TALINK, TYPE_HAS_LITERAL_DNAME,
		read_talink_rdata, write_generic_rdata,
		print_talink_rdata, talink_rdata_fields),
	/* 59 */
	TYPE("CDS", TYPE_CDS, TYPE_HAS_NO_REFS,
		read_ds_rdata, write_generic_rdata,
		print_ds_rdata, cds_rdata_fields),
	/* 60 */
	TYPE("CDNSKEY", TYPE_CDNSKEY, TYPE_HAS_NO_REFS,
		read_dnskey_rdata, write_generic_rdata,
		print_dnskey_rdata, cdnskey_rdata_fields),
	/* 61 */
	TYPE("OPENPGPKEY", TYPE_OPENPGPKEY, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_openpgpkey_rdata, openpgpkey_rdata_fields),
	/* 62 */
	TYPE("CSYNC", TYPE_CSYNC, TYPE_HAS_NO_REFS,
		read_csync_rdata, write_generic_rdata,
		print_csync_rdata, csync_rdata_fields),
	/* 63 */
	TYPE("ZONEMD", TYPE_ZONEMD, TYPE_HAS_NO_REFS,
		read_zonemd_rdata, write_generic_rdata,
		print_zonemd_rdata, zonemd_rdata_fields),
	/* 64 */
	TYPE("SVCB", TYPE_SVCB, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_svcb_rdata, write_svcb_rdata,
		print_svcb_rdata, svcb_rdata_fields),
	/* 65 */
	TYPE("HTTPS", TYPE_HTTPS, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_svcb_rdata, write_svcb_rdata,
		print_svcb_rdata, https_rdata_fields),
	/* 66 */
	TYPE("DSYNC", TYPE_DSYNC, TYPE_HAS_LITERAL_DNAME,
		read_dsync_rdata, write_generic_rdata,
		print_dsync_rdata, dsync_rdata_fields),

	UNKNOWN_TYPE(67),
	UNKNOWN_TYPE(68),
	UNKNOWN_TYPE(69),
	UNKNOWN_TYPE(70),
	UNKNOWN_TYPE(71),
	UNKNOWN_TYPE(72),
	UNKNOWN_TYPE(73),
	UNKNOWN_TYPE(74),
	UNKNOWN_TYPE(75),
	UNKNOWN_TYPE(76),
	UNKNOWN_TYPE(77),
	UNKNOWN_TYPE(78),
	UNKNOWN_TYPE(79),
	UNKNOWN_TYPE(80),
	UNKNOWN_TYPE(81),
	UNKNOWN_TYPE(82),
	UNKNOWN_TYPE(83),
	UNKNOWN_TYPE(84),
	UNKNOWN_TYPE(85),
	UNKNOWN_TYPE(86),
	UNKNOWN_TYPE(87),
	UNKNOWN_TYPE(88),
	UNKNOWN_TYPE(89),
	UNKNOWN_TYPE(90),
	UNKNOWN_TYPE(91),
	UNKNOWN_TYPE(92),
	UNKNOWN_TYPE(93),
	UNKNOWN_TYPE(94),
	UNKNOWN_TYPE(95),
	UNKNOWN_TYPE(96),
	UNKNOWN_TYPE(97),
	UNKNOWN_TYPE(98),

	/* 99 */
	TYPE("SPF", TYPE_SPF, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, spf_rdata_fields),

	UNKNOWN_TYPE(100), /* Type 100 - UINFO [The RR type code is reserved, no reference] */
	UNKNOWN_TYPE(101), /* Type 101 - UID [The RR type code is reserved, no reference] */
	UNKNOWN_TYPE(102), /* Type 102 - GID [The RR type code is reserved, no reference] */
	UNKNOWN_TYPE(103), /* Type 103 - UNSPEC [The RR type code is reserved, no reference] */

	/* 104 */
	TYPE("NID", TYPE_NID, TYPE_HAS_NO_REFS,
		read_nid_rdata, write_generic_rdata,
		print_nid_rdata, nid_rdata_fields),
	/* 105 */
	TYPE("L32", TYPE_L32, TYPE_HAS_NO_REFS,
		read_l32_rdata, write_generic_rdata,
		print_l32_rdata, l32_rdata_fields),
	/* 106 */
	TYPE("L64", TYPE_L64, TYPE_HAS_NO_REFS,
		read_l64_rdata, write_generic_rdata,
		print_l64_rdata, l64_rdata_fields),
	/* 107 */
	TYPE("LP", TYPE_LP, TYPE_HAS_UNCOMPRESSED_DNAME,
		read_lp_rdata, write_lp_rdata,
		print_lp_rdata, lp_rdata_fields),
	/* 108 */
	TYPE("EUI48", TYPE_EUI48, TYPE_HAS_NO_REFS,
		read_eui48_rdata, write_generic_rdata,
		print_eui48_rdata, eui48_rdata_fields),
	/* 109 */
	TYPE("EUI64", TYPE_EUI64, TYPE_HAS_NO_REFS,
		read_eui64_rdata, write_generic_rdata,
		print_eui64_rdata, eui64_rdata_fields),

	UNKNOWN_TYPE(110),
	UNKNOWN_TYPE(111),
	UNKNOWN_TYPE(112),
	UNKNOWN_TYPE(113),
	UNKNOWN_TYPE(114),
	UNKNOWN_TYPE(115),
	UNKNOWN_TYPE(116),
	UNKNOWN_TYPE(117),
	UNKNOWN_TYPE(118),
	UNKNOWN_TYPE(119),
	UNKNOWN_TYPE(120),
	UNKNOWN_TYPE(121),
	UNKNOWN_TYPE(122),
	UNKNOWN_TYPE(123),
	UNKNOWN_TYPE(124),
	UNKNOWN_TYPE(125),
	UNKNOWN_TYPE(126),
	UNKNOWN_TYPE(127),

	/* 128 */
	/* The mnemonic is included so it can be printed in type bitmaps.*/
	TYPE("NXNAME", TYPE_NXNAME, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_generic_rdata, generic_rdata_fields),

	UNKNOWN_TYPE(129),
	UNKNOWN_TYPE(130),
	UNKNOWN_TYPE(131),
	UNKNOWN_TYPE(132),
	UNKNOWN_TYPE(133),
	UNKNOWN_TYPE(134),
	UNKNOWN_TYPE(135),
	UNKNOWN_TYPE(136),
	UNKNOWN_TYPE(137),
	UNKNOWN_TYPE(138),
	UNKNOWN_TYPE(139),
	UNKNOWN_TYPE(140),
	UNKNOWN_TYPE(141),
	UNKNOWN_TYPE(142),
	UNKNOWN_TYPE(143),
	UNKNOWN_TYPE(144),
	UNKNOWN_TYPE(145),
	UNKNOWN_TYPE(146),
	UNKNOWN_TYPE(147),
	UNKNOWN_TYPE(148),
	UNKNOWN_TYPE(149),
	UNKNOWN_TYPE(150),
	UNKNOWN_TYPE(151),
	UNKNOWN_TYPE(152),
	UNKNOWN_TYPE(153),
	UNKNOWN_TYPE(154),
	UNKNOWN_TYPE(155),
	UNKNOWN_TYPE(156),
	UNKNOWN_TYPE(157),
	UNKNOWN_TYPE(158),
	UNKNOWN_TYPE(159),
	UNKNOWN_TYPE(160),
	UNKNOWN_TYPE(161),
	UNKNOWN_TYPE(162),
	UNKNOWN_TYPE(163),
	UNKNOWN_TYPE(164),
	UNKNOWN_TYPE(165),
	UNKNOWN_TYPE(166),
	UNKNOWN_TYPE(167),
	UNKNOWN_TYPE(168),
	UNKNOWN_TYPE(169),
	UNKNOWN_TYPE(170),
	UNKNOWN_TYPE(171),
	UNKNOWN_TYPE(172),
	UNKNOWN_TYPE(173),
	UNKNOWN_TYPE(174),
	UNKNOWN_TYPE(175),
	UNKNOWN_TYPE(176),
	UNKNOWN_TYPE(177),
	UNKNOWN_TYPE(178),
	UNKNOWN_TYPE(179),
	UNKNOWN_TYPE(180),
	UNKNOWN_TYPE(181),
	UNKNOWN_TYPE(182),
	UNKNOWN_TYPE(183),
	UNKNOWN_TYPE(184),
	UNKNOWN_TYPE(185),
	UNKNOWN_TYPE(186),
	UNKNOWN_TYPE(187),
	UNKNOWN_TYPE(188),
	UNKNOWN_TYPE(189),
	UNKNOWN_TYPE(190),
	UNKNOWN_TYPE(191),
	UNKNOWN_TYPE(192),
	UNKNOWN_TYPE(193),
	UNKNOWN_TYPE(194),
	UNKNOWN_TYPE(195),
	UNKNOWN_TYPE(196),
	UNKNOWN_TYPE(197),
	UNKNOWN_TYPE(198),
	UNKNOWN_TYPE(199),
	UNKNOWN_TYPE(200),
	UNKNOWN_TYPE(201),
	UNKNOWN_TYPE(202),
	UNKNOWN_TYPE(203),
	UNKNOWN_TYPE(204),
	UNKNOWN_TYPE(205),
	UNKNOWN_TYPE(206),
	UNKNOWN_TYPE(207),
	UNKNOWN_TYPE(208),
	UNKNOWN_TYPE(209),
	UNKNOWN_TYPE(210),
	UNKNOWN_TYPE(211),
	UNKNOWN_TYPE(212),
	UNKNOWN_TYPE(213),
	UNKNOWN_TYPE(214),
	UNKNOWN_TYPE(215),
	UNKNOWN_TYPE(216),
	UNKNOWN_TYPE(217),
	UNKNOWN_TYPE(218),
	UNKNOWN_TYPE(219),
	UNKNOWN_TYPE(220),
	UNKNOWN_TYPE(221),
	UNKNOWN_TYPE(222),
	UNKNOWN_TYPE(223),
	UNKNOWN_TYPE(224),
	UNKNOWN_TYPE(225),
	UNKNOWN_TYPE(226),
	UNKNOWN_TYPE(227),
	UNKNOWN_TYPE(228),
	UNKNOWN_TYPE(229),
	UNKNOWN_TYPE(230),
	UNKNOWN_TYPE(231),
	UNKNOWN_TYPE(232),
	UNKNOWN_TYPE(233),
	UNKNOWN_TYPE(234),
	UNKNOWN_TYPE(235),
	UNKNOWN_TYPE(236),
	UNKNOWN_TYPE(237),
	UNKNOWN_TYPE(238),
	UNKNOWN_TYPE(239),
	UNKNOWN_TYPE(240),
	UNKNOWN_TYPE(241),
	UNKNOWN_TYPE(242),
	UNKNOWN_TYPE(243),
	UNKNOWN_TYPE(244),
	UNKNOWN_TYPE(245),
	UNKNOWN_TYPE(246),
	UNKNOWN_TYPE(247),
	UNKNOWN_TYPE(248),
	UNKNOWN_TYPE(249), /* Type 249 - TKEY [RFC 2930] */
	UNKNOWN_TYPE(250), /* Type 250 - TSIG */
	UNKNOWN_TYPE(251), /* Type 251 - IXFR */
	UNKNOWN_TYPE(252), /* Type 252 - AXFR */
	UNKNOWN_TYPE(253), /* Type 253 - MAILB */
	UNKNOWN_TYPE(254), /* Type 254 - MAILA */
	UNKNOWN_TYPE(255), /* Type 255 - ANY */

	/* 256 */
	TYPE("URI", TYPE_URI, TYPE_HAS_NO_REFS,
		read_uri_rdata, write_generic_rdata,
		print_uri_rdata, uri_rdata_fields),
	/* 257 */
	TYPE("CAA", TYPE_CAA, TYPE_HAS_NO_REFS,
		read_caa_rdata, write_generic_rdata,
		print_caa_rdata, caa_rdata_fields),
	/* 258 */
	TYPE("AVC", TYPE_AVC, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, avc_rdata_fields),
	/* 259 */
	TYPE("DOA", TYPE_DOA, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_doa_rdata, doa_rdata_fields),
	/* 260 */
	TYPE("AMTRELAY", TYPE_AMTRELAY, TYPE_HAS_LITERAL_DNAME,
		read_amtrelay_rdata, write_generic_rdata,
		print_amtrelay_rdata, amtrelay_rdata_fields),
	/* 261 */
	TYPE("RESINFO", TYPE_RESINFO, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_resinfo_rdata, resinfo_rdata_fields),
	/* 262 */
	TYPE("WALLET", TYPE_WALLET, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, wallet_rdata_fields),
	/* 263 */
	TYPE("CLA", TYPE_CLA, TYPE_HAS_NO_REFS,
		read_txt_rdata, write_generic_rdata,
		print_txt_rdata, cla_rdata_fields),
	/* 264 */
	TYPE("IPN", TYPE_IPN, TYPE_HAS_NO_REFS,
		read_generic_rdata, write_generic_rdata,
		print_ipn_rdata, ipn_rdata_fields),

	/* 32768 */
	TYPE("TA", TYPE_TA, TYPE_HAS_NO_REFS,
		read_dlv_rdata, write_generic_rdata,
		print_dlv_rdata, ta_rdata_fields),
	/* 32769 */
	TYPE("DLV", TYPE_DLV, TYPE_HAS_NO_REFS,
		read_dlv_rdata, write_generic_rdata,
		print_dlv_rdata, dlv_rdata_fields)
};

#undef UNKNOWN_TYPE
#undef TYPE
#undef FIELD
#undef FIELD_ENTRY
#undef TYPE_HAS_NO_REFS
#undef TYPE_HAS_COMPRESSED_DNAME
#undef TYPE_HAS_UNCOMPRESSED_DNAME
#undef TYPE_HAS_LITERAL_DNAME
#undef TYPE_HAS_FLAGS

const char *
rrtype_to_string(uint16_t rrtype)
{
	static char buf[20];
	const nsd_type_descriptor_type *descriptor =
		nsd_type_descriptor(rrtype);
	if (descriptor->name) {
		return descriptor->name;
	} else {
		snprintf(buf, sizeof(buf), "TYPE%d", (int) rrtype);
		return buf;
	}
}

/*
 * Lookup the type in the ztypes lookup table.  If not found, check if
 * the type uses the "TYPExxx" notation for unknown types.
 *
 * Return 0 if no type matches.
 */
uint16_t
rrtype_from_string(const char *name)
{
	char *end;
	long rrtype;

	/* Because this routine is called during zone parse for every record,
	 * we optimise for frequently occurring records.
	 * Also, we optimise for 'IN' and numbers are not rr types, because
	 * during parse this routine is called for every rr class and TTL
	 * to determine that it is not an RR type */
	switch(name[0]) {
	case 'r':
	case 'R':
		if(strcasecmp(name+1, "RSIG") == 0) return TYPE_RRSIG;
		break;
	case 'n':
	case 'N':
		switch(name[1]) {
		case 's':
		case 'S':
			switch(name[2]) {
			case 0: return TYPE_NS;
			case 'e':
			case 'E':
				if(strcasecmp(name+2, "EC") == 0) return TYPE_NSEC;
				if(strcasecmp(name+2, "EC3") == 0) return TYPE_NSEC3;
				if(strcasecmp(name+2, "EC3PARAM") == 0) return TYPE_NSEC3PARAM;
				break;
			}
			break;
		}
		break;
	case 'd':
	case 'D':
		switch(name[1]) {
		case 's':
		case 'S':
			if(name[2]==0) return TYPE_DS;
			break;
		case 'n':
		case 'N':
			if(strcasecmp(name+2, "SKEY") == 0) return TYPE_DNSKEY;
			break;
		}
		break;
	case 'a':
	case 'A':
		switch(name[1]) {
		case 0:	return TYPE_A;
		case 'a':
		case 'A':
			if(strcasecmp(name+2, "AA") == 0) return TYPE_AAAA;
			break;
		}
		break;
	case 's':
	case 'S':
		if(strcasecmp(name+1, "OA") == 0) return TYPE_SOA;
		break;
	case 't':
	case 'T':
		if(strcasecmp(name+1, "XT") == 0) return TYPE_TXT;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return 0; /* no RR types start with 0-9 */
	case 'i':
	case 'I':
		switch(name[1]) {
		case 'n':
		case 'N':
			return 0; /* 'IN' is a class not a type */
		}
		break;
	}

	for (int i=0, n=sizeof(type_descriptors)/sizeof(type_descriptors[0]); i < n; i++) {
		if (type_descriptors[i].name && strcasecmp(type_descriptors[i].name, name) == 0)
			return type_descriptors[i].type;
	}

	if (strlen(name) < 5)
		return 0;

	if (strncasecmp(name, "TYPE", 4) != 0)
		return 0;

	if (!isdigit((unsigned char)name[4]))
		return 0;

	/* The rest from the string must be a number.  */
	rrtype = strtol(name + 4, &end, 10);
	if (*end != '\0')
		return 0;
	if (rrtype < 0 || rrtype > 65535L)
		return 0;

	return (uint16_t) rrtype;
}

const char *
rrclass_to_string(uint16_t rrclass)
{
	static char buf[20];
	lookup_table_type *entry = lookup_by_id(dns_rrclasses, rrclass);
	if (entry) {
		assert(strlen(entry->name) < sizeof(buf));
		strlcpy(buf, entry->name, sizeof(buf));
	} else {
		snprintf(buf, sizeof(buf), "CLASS%d", (int) rrclass);
	}
	return buf;
}

uint16_t
rrclass_from_string(const char *name)
{
        char *end;
        long rrclass;
	lookup_table_type *entry;

	entry = lookup_by_name(dns_rrclasses, name);
	if (entry) {
		return (uint16_t) entry->id;
	}

	if (strlen(name) < 6)
		return 0;

	if (strncasecmp(name, "CLASS", 5) != 0)
		return 0;

	if (!isdigit((unsigned char)name[5]))
		return 0;

	/* The rest from the string must be a number.  */
	rrclass = strtol(name + 5, &end, 10);
	if (*end != '\0')
		return 0;
	if (rrclass < 0 || rrclass > 65535L)
		return 0;

	return (uint16_t) rrclass;
}
