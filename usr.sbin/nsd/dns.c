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
#include "zonec.h"
#include "zparser.h"

/* Taken from RFC 1035, section 3.2.4.  */
static lookup_table_type dns_rrclasses[] = {
	{ CLASS_IN, "IN" },	/* the Internet */
	{ CLASS_CS, "CS" },	/* the CSNET class (Obsolete) */
	{ CLASS_CH, "CH" },	/* the CHAOS class */
	{ CLASS_HS, "HS" },	/* Hesiod */
	{ 0, NULL }
};

static rrtype_descriptor_type rrtype_descriptors[(RRTYPE_DESCRIPTORS_LENGTH+1)] = {
	/* 0 */
	{ 0, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 1 */
	{ TYPE_A, "A", T_A, 1, 1,
	  { RDATA_WF_A }, { RDATA_ZF_A } },
	/* 2 */
	{ TYPE_NS, "NS", T_NS, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 3 */
	{ TYPE_MD, "MD", T_MD, 1, 1,
	  { RDATA_WF_UNCOMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 4 */
	{ TYPE_MF, "MF", T_MF, 1, 1,
	  { RDATA_WF_UNCOMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 5 */
	{ TYPE_CNAME, "CNAME", T_CNAME, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 6 */
	{ TYPE_SOA, "SOA", T_SOA, 7, 7,
	  { RDATA_WF_COMPRESSED_DNAME, RDATA_WF_COMPRESSED_DNAME, RDATA_WF_LONG,
	    RDATA_WF_LONG, RDATA_WF_LONG, RDATA_WF_LONG, RDATA_WF_LONG },
	  { RDATA_ZF_DNAME, RDATA_ZF_DNAME, RDATA_ZF_PERIOD, RDATA_ZF_PERIOD,
	    RDATA_ZF_PERIOD, RDATA_ZF_PERIOD, RDATA_ZF_PERIOD } },
	/* 7 */
	{ TYPE_MB, "MB", T_MB, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 8 */
	{ TYPE_MG, "MG", T_MG, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 9 */
	{ TYPE_MR, "MR", T_MR, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 10 */
	{ TYPE_NULL, "NULL", T_UTYPE, 1, 1,
	  { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 11 */
	{ TYPE_WKS, "WKS", T_WKS, 2, 2,
	  { RDATA_WF_A, RDATA_WF_BINARY },
	  { RDATA_ZF_A, RDATA_ZF_SERVICES } },
	/* 12 */
	{ TYPE_PTR, "PTR", T_PTR, 1, 1,
	  { RDATA_WF_COMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 13 */
	{ TYPE_HINFO, "HINFO", T_HINFO, 2, 2,
	  { RDATA_WF_TEXT, RDATA_WF_TEXT }, { RDATA_ZF_TEXT, RDATA_ZF_TEXT } },
	/* 14 */
	{ TYPE_MINFO, "MINFO", T_MINFO, 2, 2,
	  { RDATA_WF_COMPRESSED_DNAME, RDATA_WF_COMPRESSED_DNAME },
	  { RDATA_ZF_DNAME, RDATA_ZF_DNAME } },
	/* 15 */
	{ TYPE_MX, "MX", T_MX, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_COMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 16 */
	{ TYPE_TXT, "TXT", T_TXT, 1, 1,
	  { RDATA_WF_TEXTS },
	  { RDATA_ZF_TEXTS } },
	/* 17 */
	{ TYPE_RP, "RP", T_RP, 2, 2,
	  { RDATA_WF_UNCOMPRESSED_DNAME, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_DNAME, RDATA_ZF_DNAME } },
	/* 18 */
	{ TYPE_AFSDB, "AFSDB", T_AFSDB, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 19 */
	{ TYPE_X25, "X25", T_X25, 1, 1,
	  { RDATA_WF_TEXT },
	  { RDATA_ZF_TEXT } },
	/* 20 */
	{ TYPE_ISDN, "ISDN", T_ISDN, 1, 2,
	  { RDATA_WF_TEXT, RDATA_WF_TEXT },
	  { RDATA_ZF_TEXT, RDATA_ZF_TEXT } },
	/* 21 */
	{ TYPE_RT, "RT", T_RT, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 22 */
	{ TYPE_NSAP, "NSAP", T_NSAP, 1, 1,
	  { RDATA_WF_BINARY },
	  { RDATA_ZF_NSAP } },
	/* 23 */
	{ 23, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 24 */
	{ TYPE_SIG, "SIG", T_SIG, 9, 9,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_LONG,
	    RDATA_WF_LONG, RDATA_WF_LONG, RDATA_WF_SHORT,
	    RDATA_WF_UNCOMPRESSED_DNAME, RDATA_WF_BINARY },
	  { RDATA_ZF_RRTYPE, RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_PERIOD,
	    RDATA_ZF_TIME, RDATA_ZF_TIME, RDATA_ZF_SHORT, RDATA_ZF_DNAME,
	    RDATA_ZF_BASE64 } },
	/* 25 */
	{ TYPE_KEY, "KEY", T_KEY, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_BYTE, RDATA_ZF_ALGORITHM,
	    RDATA_ZF_BASE64 } },
	/* 26 */
	{ TYPE_PX, "PX", T_PX, 3, 3,
	  { RDATA_WF_SHORT, RDATA_WF_UNCOMPRESSED_DNAME,
	    RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME, RDATA_ZF_DNAME } },
	/* 27 */
	{ 27, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 28 */
	{ TYPE_AAAA, "AAAA", T_AAAA, 1, 1,
	  { RDATA_WF_AAAA },
	  { RDATA_ZF_AAAA } },
	/* 29 */
	{ TYPE_LOC, "LOC", T_LOC, 1, 1,
	  { RDATA_WF_BINARY },
	  { RDATA_ZF_LOC } },
	/* 30 */
	{ TYPE_NXT, "NXT", T_NXT, 2, 2,
	  { RDATA_WF_UNCOMPRESSED_DNAME, RDATA_WF_BINARY },
	  { RDATA_ZF_DNAME, RDATA_ZF_NXT } },
	/* 31 */
	{ 31, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 32 */
	{ 32, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 33 */
	{ TYPE_SRV, "SRV", T_SRV, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_SHORT, RDATA_WF_SHORT,
	    RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_SHORT, RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 34 */
	{ 34, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 35 */
	{ TYPE_NAPTR, "NAPTR", T_NAPTR, 6, 6,
	  { RDATA_WF_SHORT, RDATA_WF_SHORT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_SHORT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_DNAME } },
	/* 36 */
	{ TYPE_KX, "KX", T_KX, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 37 */
	{ TYPE_CERT, "CERT", T_CERT, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_CERTIFICATE_TYPE, RDATA_ZF_SHORT, RDATA_ZF_ALGORITHM,
	    RDATA_ZF_BASE64 } },
	/* 38 */
	{ TYPE_A6, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 39 */
	{ TYPE_DNAME, "DNAME", T_DNAME, 1, 1,
	  { RDATA_WF_UNCOMPRESSED_DNAME }, { RDATA_ZF_DNAME } },
	/* 40 */
	{ 40, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 41 */
	{ TYPE_OPT, "OPT", T_UTYPE, 1, 1,
	  { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 42 */
	{ TYPE_APL, "APL", T_APL, 0, MAXRDATALEN,
	  { RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL,
	    RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL, RDATA_WF_APL },
	  { RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL,
	    RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL, RDATA_ZF_APL } },
	/* 43 */
	{ TYPE_DS, "DS", T_DS, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_ALGORITHM, RDATA_ZF_BYTE, RDATA_ZF_HEX } },
	/* 44 */
	{ TYPE_SSHFP, "SSHFP", T_SSHFP, 3, 3,
	  { RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_HEX } },
	/* 45 */
	{ TYPE_IPSECKEY, "IPSECKEY", T_IPSECKEY, 4, 5,
	  { RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_IPSECGATEWAY,
	    RDATA_WF_BINARY },
	  { RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_IPSECGATEWAY,
	    RDATA_ZF_BASE64 } },
	/* 46 */
	{ TYPE_RRSIG, "RRSIG", T_RRSIG, 9, 9,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_LONG,
	    RDATA_WF_LONG, RDATA_WF_LONG, RDATA_WF_SHORT,
	    RDATA_WF_LITERAL_DNAME, RDATA_WF_BINARY },
	  { RDATA_ZF_RRTYPE, RDATA_ZF_ALGORITHM, RDATA_ZF_BYTE, RDATA_ZF_PERIOD,
	    RDATA_ZF_TIME, RDATA_ZF_TIME, RDATA_ZF_SHORT,
		RDATA_ZF_LITERAL_DNAME, RDATA_ZF_BASE64 } },
	/* 47 */
	{ TYPE_NSEC, "NSEC", T_NSEC, 2, 2,
	  { RDATA_WF_LITERAL_DNAME, RDATA_WF_BINARY },
	  { RDATA_ZF_LITERAL_DNAME, RDATA_ZF_NSEC } },
	/* 48 */
	{ TYPE_DNSKEY, "DNSKEY", T_DNSKEY, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_BYTE, RDATA_ZF_ALGORITHM,
	    RDATA_ZF_BASE64 } },
	/* 49 */
	{ TYPE_DHCID, "DHCID", T_DHCID, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_BASE64 } },
	/* 50 */
	{ TYPE_NSEC3, "NSEC3", T_NSEC3, 6, 6,
	  { RDATA_WF_BYTE, /* hash type */
	    RDATA_WF_BYTE, /* flags */
	    RDATA_WF_SHORT, /* iterations */
	    RDATA_WF_BINARYWITHLENGTH, /* salt */
	    RDATA_WF_BINARYWITHLENGTH, /* next hashed name */
	    RDATA_WF_BINARY /* type bitmap */ },
	  { RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_SHORT, RDATA_ZF_HEX_LEN,
	    RDATA_ZF_BASE32, RDATA_ZF_NSEC } },
	/* 51 */
	{ TYPE_NSEC3PARAM, "NSEC3PARAM", T_NSEC3PARAM, 4, 4,
	  { RDATA_WF_BYTE, /* hash type */
	    RDATA_WF_BYTE, /* flags */
	    RDATA_WF_SHORT, /* iterations */
	    RDATA_WF_BINARYWITHLENGTH /* salt */ },
	  { RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_SHORT, RDATA_ZF_HEX_LEN } },
	/* 52 */
	{ TYPE_TLSA, "TLSA", T_TLSA, 4, 4,
	  { RDATA_WF_BYTE, /* usage */
	    RDATA_WF_BYTE, /* selector */
	    RDATA_WF_BYTE, /* matching type */
	    RDATA_WF_BINARY }, /* certificate association data */
	  { RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_BYTE, RDATA_ZF_HEX } },
	/* 53 */
	{ 53, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 54 */
	{ 54, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 55 - HIP [RFC 5205] */
	{ 55, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 56 - NINFO */
	{ 56, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 57 - RKEY */
	{ 57, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 58 - TALINK */
	{ 58, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 59 - CDS */
	{ TYPE_CDS, "CDS", T_CDS, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_ALGORITHM, RDATA_ZF_BYTE, RDATA_ZF_HEX } },
	/* 60 - CDNSKEY */
	{ TYPE_CDNSKEY, "CDNSKEY", T_CDNSKEY, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_BYTE, RDATA_ZF_ALGORITHM,
	    RDATA_ZF_BASE64 } },
	/* 61 */
	{ 61, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 62 */
	{ 62, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 63 */
	{ 63, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 64 */
	{ 64, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 65 */
	{ 65, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 66 */
	{ 66, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 67 */
	{ 67, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 68 */
	{ 68, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 69 */
	{ 69, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 70 */
	{ 70, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 71 */
	{ 71, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 72 */
	{ 72, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 73 */
	{ 73, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 74 */
	{ 74, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 75 */
	{ 75, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 76 */
	{ 76, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 77 */
	{ 77, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 78 */
	{ 78, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 79 */
	{ 79, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 80 */
	{ 80, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 81 */
	{ 81, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 82 */
	{ 82, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 83 */
	{ 83, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 84 */
	{ 84, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 85 */
	{ 85, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 86 */
	{ 86, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 87 */
	{ 87, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 88 */
	{ 88, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 89 */
	{ 89, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 90 */
	{ 90, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 91 */
	{ 91, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 92 */
	{ 92, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 93 */
	{ 93, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 94 */
	{ 94, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 95 */
	{ 95, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 96 */
	{ 96, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 97 */
	{ 97, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 98 */
	{ 98, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 99 */
	{ TYPE_SPF, "SPF", T_SPF, 1, MAXRDATALEN,
	  { RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT,
	    RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT, RDATA_WF_TEXT },
	  { RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT,
	    RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT, RDATA_ZF_TEXT } },
	/* 100 - UINFO */
	{ 100, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 101 - UID */
	{ 101, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 102 - GID */
	{ 102, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 103 - UNSPEC */
	{ 103, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 104 */
	{ TYPE_NID, "NID", T_NID, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_ILNP64 },
	  { RDATA_ZF_SHORT, RDATA_ZF_ILNP64 } },
	/* 105 */
	{ TYPE_L32, "L32", T_L32, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_A },
	  { RDATA_ZF_SHORT, RDATA_ZF_A } },
	/* 106 */
	{ TYPE_L64, "L64", T_L64, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_ILNP64 },
	  { RDATA_ZF_SHORT, RDATA_ZF_ILNP64 } },
	/* 107 */
	{ TYPE_LP, "LP", T_LP, 2, 2,
	  { RDATA_WF_SHORT, RDATA_WF_UNCOMPRESSED_DNAME },
	  { RDATA_ZF_SHORT, RDATA_ZF_DNAME } },
	/* 108 */
	{ TYPE_EUI48, "EUI48", T_EUI48, 1, 1,
	  { RDATA_WF_EUI48 }, { RDATA_ZF_EUI48 } },
	/* 109 */
	{ TYPE_EUI64, "EUI64", T_EUI64, 1, 1,
	  { RDATA_WF_EUI64 }, { RDATA_ZF_EUI64 } },
	/* 110 */
	{ 110, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 111 */
	{ 111, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 112 */
	{ 112, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 113 */
	{ 113, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 114 */
	{ 114, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 115 */
	{ 115, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 116 */
	{ 116, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 117 */
	{ 117, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 118 */
	{ 118, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 119 */
	{ 119, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 120 */
	{ 120, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 121 */
	{ 121, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 122 */
	{ 122, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 123 */
	{ 123, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 124 */
	{ 124, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 125 */
	{ 125, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 126 */
	{ 126, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 127 */
	{ 127, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 128 */
	{ 128, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 129 */
	{ 129, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 130 */
	{ 130, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 131 */
	{ 131, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 132 */
	{ 132, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 133 */
	{ 133, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 134 */
	{ 134, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 135 */
	{ 135, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 136 */
	{ 136, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 137 */
	{ 137, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 138 */
	{ 138, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 139 */
	{ 139, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 140 */
	{ 140, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 141 */
	{ 141, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 142 */
	{ 142, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 143 */
	{ 143, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 144 */
	{ 144, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 145 */
	{ 145, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 146 */
	{ 146, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 147 */
	{ 147, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 148 */
	{ 148, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 149 */
	{ 149, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 150 */
	{ 150, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 151 */
	{ 151, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 152 */
	{ 152, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 153 */
	{ 153, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 154 */
	{ 154, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 155 */
	{ 155, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 156 */
	{ 156, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 157 */
	{ 157, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 158 */
	{ 158, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 159 */
	{ 159, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 160 */
	{ 160, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 161 */
	{ 161, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 162 */
	{ 162, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 163 */
	{ 163, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 164 */
	{ 164, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 165 */
	{ 165, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 166 */
	{ 166, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 167 */
	{ 167, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 168 */
	{ 168, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 169 */
	{ 169, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 170 */
	{ 170, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 171 */
	{ 171, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 172 */
	{ 172, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 173 */
	{ 173, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 174 */
	{ 174, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 175 */
	{ 175, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 176 */
	{ 176, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 177 */
	{ 177, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 178 */
	{ 178, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 179 */
	{ 179, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 180 */
	{ 180, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 181 */
	{ 181, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 182 */
	{ 182, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 183 */
	{ 183, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 184 */
	{ 184, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 185 */
	{ 185, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 186 */
	{ 186, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 187 */
	{ 187, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 188 */
	{ 188, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 189 */
	{ 189, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 190 */
	{ 190, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 191 */
	{ 191, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 192 */
	{ 192, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 193 */
	{ 193, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 194 */
	{ 194, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 195 */
	{ 195, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 196 */
	{ 196, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 197 */
	{ 197, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 198 */
	{ 198, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 199 */
	{ 199, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 200 */
	{ 200, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 201 */
	{ 201, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 202 */
	{ 202, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 203 */
	{ 203, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 204 */
	{ 204, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 205 */
	{ 205, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 206 */
	{ 206, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 207 */
	{ 207, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 208 */
	{ 208, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 209 */
	{ 209, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 210 */
	{ 210, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 211 */
	{ 211, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 212 */
	{ 212, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 213 */
	{ 213, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 214 */
	{ 214, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 215 */
	{ 215, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 216 */
	{ 216, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 217 */
	{ 217, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 218 */
	{ 218, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 219 */
	{ 219, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 220 */
	{ 220, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 221 */
	{ 221, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 222 */
	{ 222, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 223 */
	{ 223, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 224 */
	{ 224, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 225 */
	{ 225, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 226 */
	{ 226, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 227 */
	{ 227, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 228 */
	{ 228, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 229 */
	{ 229, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 230 */
	{ 230, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 231 */
	{ 231, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 232 */
	{ 232, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 233 */
	{ 233, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 234 */
	{ 234, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 235 */
	{ 235, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 236 */
	{ 236, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 237 */
	{ 237, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 238 */
	{ 238, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 239 */
	{ 239, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 240 */
	{ 240, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 241 */
	{ 241, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 242 */
	{ 242, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 243 */
	{ 243, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 244 */
	{ 244, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 245 */
	{ 245, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 246 */
	{ 246, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 247 */
	{ 247, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 248 */
	{ 248, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 249 - TKEY [RFC 2930] */
	{ 249, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 250 - TSIG [RFC 2845] */
	{ 250, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 251 - IXFR [RFC 1995] */
	{ 251, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 252 - AXFR [RFC 1035, RFC 5936] */
	{ 252, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 253 - MAILB [RFC 1035] */
	{ 253, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 254 - MAILA [RFC 1035] */
	{ 254, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 255 - * [RFC 1035, RFC 6895] */
	{ 255, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 256 - URI */
	{ 256, NULL, T_UTYPE, 1, 1, { RDATA_WF_BINARY }, { RDATA_ZF_UNKNOWN } },
	/* 257 - CAA [RFC 6844] */
	{ TYPE_CAA, "CAA", T_CAA, 3, 3,
	  { RDATA_WF_BYTE, RDATA_WF_TEXT, RDATA_WF_LONG_TEXT },
	  { RDATA_ZF_BYTE, RDATA_ZF_TAG, RDATA_ZF_LONG_TEXT } },

	/* 32768 - TA */
	/* 32769 */
	{ TYPE_DLV, "DLV", T_DLV, 4, 4,
	  { RDATA_WF_SHORT, RDATA_WF_BYTE, RDATA_WF_BYTE, RDATA_WF_BINARY },
	  { RDATA_ZF_SHORT, RDATA_ZF_ALGORITHM, RDATA_ZF_BYTE, RDATA_ZF_HEX } },
};

rrtype_descriptor_type *
rrtype_descriptor_by_type(uint16_t type)
{
	if (type < RRTYPE_DESCRIPTORS_LENGTH)
		return &rrtype_descriptors[type];
	else if (type == TYPE_DLV)
		return &rrtype_descriptors[PSEUDO_TYPE_DLV];
	return &rrtype_descriptors[0];
}

rrtype_descriptor_type *
rrtype_descriptor_by_name(const char *name)
{
	int i;

	for (i = 0; i < RRTYPE_DESCRIPTORS_LENGTH; ++i) {
		if (rrtype_descriptors[i].name
		    && strcasecmp(rrtype_descriptors[i].name, name) == 0)
		{
			return &rrtype_descriptors[i];
		}
	}

	if (rrtype_descriptors[PSEUDO_TYPE_DLV].name
	    && strcasecmp(rrtype_descriptors[PSEUDO_TYPE_DLV].name, name) == 0)
	{
		return &rrtype_descriptors[PSEUDO_TYPE_DLV];
	}

	return NULL;
}

const char *
rrtype_to_string(uint16_t rrtype)
{
	static char buf[20];
	rrtype_descriptor_type *descriptor = rrtype_descriptor_by_type(rrtype);
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
	rrtype_descriptor_type *entry;

	/* Because this routine is called during zone parse for every record,
	 * we optimise for frequently occuring records.
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

	entry = rrtype_descriptor_by_name(name);
	if (entry) {
		return entry->type;
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
