/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DNS_TYPES_H
#define DNS_TYPES_H 1

/*! \file dns/types.h
 * \brief
 * Including this file gives you type declarations suitable for use in
 * .h files, which lets us avoid circular type reference problems.
 * \brief
 * To actually use a type or get declarations of its methods, you must
 * include the appropriate .h file too.
 */

#include <stdio.h>

#include <isc/types.h>

typedef struct dns_acache			dns_acache_t;
typedef uint16_t				dns_cert_t;
typedef struct dns_compress			dns_compress_t;
typedef struct dns_db				dns_db_t;
typedef void					dns_dbnode_t;
typedef void					dns_dbversion_t;
typedef struct dns_decompress			dns_decompress_t;
typedef struct dns_ednsopt			dns_ednsopt_t;
typedef struct dns_fixedname			dns_fixedname_t;
typedef uint64_t				dns_masterstyle_flags_t;
typedef struct dns_message			dns_message_t;
typedef uint16_t				dns_messageid_t;
typedef isc_region_t				dns_label_t;
typedef struct dns_name				dns_name_t;
typedef ISC_LIST(dns_name_t)			dns_namelist_t;
typedef uint16_t				dns_opcode_t;
typedef unsigned char				dns_offsets_t[128];
typedef uint16_t				dns_rcode_t;
typedef struct dns_rdata			dns_rdata_t;
typedef uint16_t				dns_rdataclass_t;
typedef struct dns_rdatalist			dns_rdatalist_t;
typedef struct dns_rdataset			dns_rdataset_t;
typedef uint16_t				dns_rdatatype_t;
typedef uint8_t				dns_secalg_t;
typedef struct dns_tsigkey			dns_tsigkey_t;
typedef uint32_t				dns_ttl_t;
typedef struct dns_view				dns_view_t;
typedef struct dns_zone				dns_zone_t;

typedef enum {
	dns_namereln_none = 0,
	dns_namereln_contains = 1,
	dns_namereln_subdomain = 2,
	dns_namereln_equal = 3,
	dns_namereln_commonancestor = 4
} dns_namereln_t;

enum {
	dns_rdataclass_reserved0 = 0,
	dns_rdataclass_in = 1,
	dns_rdataclass_chaos = 3,
	dns_rdataclass_ch = 3,
	dns_rdataclass_hs = 4,
	dns_rdataclass_none = 254,
	dns_rdataclass_any = 255
};

enum {
	dns_rdatatype_none = 0,
	dns_rdatatype_a = 1,
	dns_rdatatype_ns = 2,
	dns_rdatatype_md = 3,
	dns_rdatatype_mf = 4,
	dns_rdatatype_cname = 5,
	dns_rdatatype_soa = 6,
	dns_rdatatype_mb = 7,
	dns_rdatatype_mg = 8,
	dns_rdatatype_mr = 9,
	dns_rdatatype_null = 10,
	dns_rdatatype_wks = 11,
	dns_rdatatype_ptr = 12,
	dns_rdatatype_hinfo = 13,
	dns_rdatatype_minfo = 14,
	dns_rdatatype_mx = 15,
	dns_rdatatype_txt = 16,
	dns_rdatatype_rp = 17,
	dns_rdatatype_afsdb = 18,
	dns_rdatatype_x25 = 19,
	dns_rdatatype_isdn = 20,
	dns_rdatatype_rt = 21,
	dns_rdatatype_nsap = 22,
	dns_rdatatype_nsap_ptr = 23,
	dns_rdatatype_sig = 24,
	dns_rdatatype_key = 25,
	dns_rdatatype_px = 26,
	dns_rdatatype_gpos = 27,
	dns_rdatatype_aaaa = 28,
	dns_rdatatype_loc = 29,
	dns_rdatatype_nxt = 30,
	dns_rdatatype_srv = 33,
	dns_rdatatype_naptr = 35,
	dns_rdatatype_kx = 36,
	dns_rdatatype_cert = 37,
	dns_rdatatype_a6 = 38,
	dns_rdatatype_dname = 39,
	dns_rdatatype_sink = 40,
	dns_rdatatype_opt = 41,
	dns_rdatatype_apl = 42,
	dns_rdatatype_ds = 43,
	dns_rdatatype_sshfp = 44,
	dns_rdatatype_ipseckey = 45,
	dns_rdatatype_rrsig = 46,
	dns_rdatatype_nsec = 47,
	dns_rdatatype_dnskey = 48,
	dns_rdatatype_dhcid = 49,
	dns_rdatatype_nsec3 = 50,
	dns_rdatatype_nsec3param = 51,
	dns_rdatatype_tlsa = 52,
	dns_rdatatype_smimea = 53,
	dns_rdatatype_hip = 55,
	dns_rdatatype_ninfo = 56,
	dns_rdatatype_rkey = 57,
	dns_rdatatype_talink = 58,
	dns_rdatatype_cds = 59,
	dns_rdatatype_cdnskey = 60,
	dns_rdatatype_openpgpkey = 61,
	dns_rdatatype_csync = 62,
	dns_rdatatype_zonemd = 63,
	dns_rdatatype_svcb = 64,
	dns_rdatatype_https = 65,
	dns_rdatatype_spf = 99,
	dns_rdatatype_unspec = 103,
	dns_rdatatype_nid = 104,
	dns_rdatatype_l32 = 105,
	dns_rdatatype_l64 = 106,
	dns_rdatatype_lp = 107,
	dns_rdatatype_eui48 = 108,
	dns_rdatatype_eui64 = 109,
	dns_rdatatype_tkey = 249,
	dns_rdatatype_tsig = 250,
	dns_rdatatype_uri = 256,
	dns_rdatatype_caa = 257,
	dns_rdatatype_avc = 258,
	dns_rdatatype_doa = 259,
	dns_rdatatype_ta = 32768,
	dns_rdatatype_dlv = 32769,
	dns_rdatatype_keydata = 65533,
	dns_rdatatype_ixfr = 251,
	dns_rdatatype_axfr = 252,
	dns_rdatatype_mailb = 253,
	dns_rdatatype_maila = 254,
	dns_rdatatype_any = 255
};

/*%
 * rcodes.
 */
enum {
	/*
	 * Standard rcodes.
	 */
	dns_rcode_noerror = 0,
#define dns_rcode_noerror		((dns_rcode_t)dns_rcode_noerror)
	dns_rcode_formerr = 1,
#define dns_rcode_formerr		((dns_rcode_t)dns_rcode_formerr)
	dns_rcode_servfail = 2,
#define dns_rcode_servfail		((dns_rcode_t)dns_rcode_servfail)
	dns_rcode_nxdomain = 3,
#define dns_rcode_nxdomain		((dns_rcode_t)dns_rcode_nxdomain)
	dns_rcode_notimp = 4,
#define dns_rcode_notimp		((dns_rcode_t)dns_rcode_notimp)
	dns_rcode_refused = 5,
#define dns_rcode_refused		((dns_rcode_t)dns_rcode_refused)
	dns_rcode_yxdomain = 6,
#define dns_rcode_yxdomain		((dns_rcode_t)dns_rcode_yxdomain)
	dns_rcode_yxrrset = 7,
#define dns_rcode_yxrrset		((dns_rcode_t)dns_rcode_yxrrset)
	dns_rcode_nxrrset = 8,
#define dns_rcode_nxrrset		((dns_rcode_t)dns_rcode_nxrrset)
	dns_rcode_notauth = 9,
#define dns_rcode_notauth		((dns_rcode_t)dns_rcode_notauth)
	dns_rcode_notzone = 10,
#define dns_rcode_notzone		((dns_rcode_t)dns_rcode_notzone)
	/*
	 * Extended rcodes.
	 */
	dns_rcode_badvers = 16,
#define dns_rcode_badvers		((dns_rcode_t)dns_rcode_badvers)
	dns_rcode_badcookie = 23
#define dns_rcode_badcookie		((dns_rcode_t)dns_rcode_badcookie)
	/* Private space [3841..4095] */
};

/*%
 * TSIG errors.
 */
enum {
	dns_tsigerror_badsig = 16,
	dns_tsigerror_badkey = 17,
	dns_tsigerror_badtime = 18,
	dns_tsigerror_badmode = 19,
	dns_tsigerror_badname = 20,
	dns_tsigerror_badalg = 21,
	dns_tsigerror_badtrunc = 22
};

/*%
 * Opcodes.
 */
enum {
	dns_opcode_query = 0,
#define dns_opcode_query		((dns_opcode_t)dns_opcode_query)
	dns_opcode_iquery = 1,
#define dns_opcode_iquery		((dns_opcode_t)dns_opcode_iquery)
	dns_opcode_status = 2,
#define dns_opcode_status		((dns_opcode_t)dns_opcode_status)
	dns_opcode_notify = 4,
#define dns_opcode_notify		((dns_opcode_t)dns_opcode_notify)
	dns_opcode_update = 5		/* dynamic update */
#define dns_opcode_update		((dns_opcode_t)dns_opcode_update)
};

/*
 * Functions.
 */
typedef int
(*dns_rdatasetorderfunc_t)(const dns_rdata_t *, const void *);

#endif /* DNS_TYPES_H */
