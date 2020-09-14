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

/* $Id: dns_result.c,v 1.5 2020/09/14 08:40:43 florian Exp $ */

/*! \file */
#include <isc/util.h>

#include <dns/result.h>

static const char *text[DNS_R_NRESULTS] = {
	"label too long",		       /*%< 0 DNS_R_LABELTOOLONG */
	"bad escape",			       /*%< 1 DNS_R_BADESCAPE */
	/*!
	 * Note that DNS_R_BADBITSTRING and DNS_R_BITSTRINGTOOLONG are
	 * deprecated.
	 */
	"bad bitstring",		       /*%< 2 DNS_R_BADBITSTRING */
	"bitstring too long",		       /*%< 3 DNS_R_BITSTRINGTOOLONG */
	"empty label",			       /*%< 4 DNS_R_EMPTYLABEL */

	"bad dotted quad",		       /*%< 5 DNS_R_BADDOTTEDQUAD */
	"invalid NS owner name (wildcard)",    /*%< 6 DNS_R_INVALIDNS */
	"unknown class/type",		       /*%< 7 DNS_R_UNKNOWN */
	"bad label type",		       /*%< 8 DNS_R_BADLABELTYPE */
	"bad compression pointer",	       /*%< 9 DNS_R_BADPOINTER */

	"too many hops",		       /*%< 10 DNS_R_TOOMANYHOPS */
	"disallowed (by application policy)",  /*%< 11 DNS_R_DISALLOWED */
	"extra input text",		       /*%< 12 DNS_R_EXTRATOKEN */
	"extra input data",		       /*%< 13 DNS_R_EXTRADATA */
	"text too long",		       /*%< 14 DNS_R_TEXTTOOLONG */

	"not at top of zone",		       /*%< 15 DNS_R_NOTZONETOP */
	"syntax error",			       /*%< 16 DNS_R_SYNTAX */
	"bad checksum",			       /*%< 17 DNS_R_BADCKSUM */
	"bad IPv6 address",		       /*%< 18 DNS_R_BADAAAA */
	"no owner",			       /*%< 19 DNS_R_NOOWNER */

	"no ttl",			       /*%< 20 DNS_R_NOTTL */
	"bad class",			       /*%< 21 DNS_R_BADCLASS */
	"name too long",		       /*%< 22 DNS_R_NAMETOOLONG */
	"partial match",		       /*%< 23 DNS_R_PARTIALMATCH */
	"new origin",			       /*%< 24 DNS_R_NEWORIGIN */

	"unchanged",			       /*%< 25 DNS_R_UNCHANGED */
	"bad ttl",			       /*%< 26 DNS_R_BADTTL */
	"more data needed/to be rendered",     /*%< 27 DNS_R_NOREDATA */
	"continue",			       /*%< 28 DNS_R_CONTINUE */
	"delegation",			       /*%< 29 DNS_R_DELEGATION */

	"glue",				       /*%< 30 DNS_R_GLUE */
	"dname",			       /*%< 31 DNS_R_DNAME */
	"cname",			       /*%< 32 DNS_R_CNAME */
	"bad database",			       /*%< 33 DNS_R_BADDB */
	"zonecut",			       /*%< 34 DNS_R_ZONECUT */

	"bad zone",			       /*%< 35 DNS_R_BADZONE */
	"more data",			       /*%< 36 DNS_R_MOREDATA */
	"up to date",			       /*%< 37 DNS_R_UPTODATE */
	"tsig verify failure",		       /*%< 38 DNS_R_TSIGVERIFYFAILURE */
	"tsig indicates error",		       /*%< 39 DNS_R_TSIGERRORSET */

	"RRSIG failed to verify",	       /*%< 40 DNS_R_SIGINVALID */
	"RRSIG has expired",		       /*%< 41 DNS_R_SIGEXPIRED */
	"RRSIG validity period has not begun", /*%< 42 DNS_R_SIGFUTURE */
	"key is unauthorized to sign data",    /*%< 43 DNS_R_KEYUNAUTHORIZED */
	"invalid time",			       /*%< 44 DNS_R_INVALIDTIME */

	"expected a TSIG or SIG(0)",	       /*%< 45 DNS_R_EXPECTEDTSIG */
	"did not expect a TSIG or SIG(0)",     /*%< 46 DNS_R_UNEXPECTEDTSIG */
	"TKEY is unacceptable",		       /*%< 47 DNS_R_INVALIDTKEY */
	"hint",				       /*%< 48 DNS_R_HINT */
	"drop",				       /*%< 49 DNS_R_DROP */

	"zone not loaded",		       /*%< 50 DNS_R_NOTLOADED */
	"ncache nxdomain",		       /*%< 51 DNS_R_NCACHENXDOMAIN */
	"ncache nxrrset",		       /*%< 52 DNS_R_NCACHENXRRSET */
	"wait",				       /*%< 53 DNS_R_WAIT */
	"not verified yet",		       /*%< 54 DNS_R_NOTVERIFIEDYET */

	"no identity",			       /*%< 55 DNS_R_NOIDENTITY */
	"no journal",			       /*%< 56 DNS_R_NOJOURNAL */
	"alias",			       /*%< 57 DNS_R_ALIAS */
	"use TCP",			       /*%< 58 DNS_R_USETCP */
	"no valid RRSIG",		       /*%< 59 DNS_R_NOVALIDSIG */

	"no valid NSEC",		       /*%< 60 DNS_R_NOVALIDNSEC */
	"insecurity proof failed",	       /*%< 61 DNS_R_NOTINSECURE */
	"unknown service",		       /*%< 62 DNS_R_UNKNOWNSERVICE */
	"recoverable error occurred",	       /*%< 63 DNS_R_RECOVERABLE */
	"unknown opt attribute record",	       /*%< 64 DNS_R_UNKNOWNOPT */

	"unexpected message id",	       /*%< 65 DNS_R_UNEXPECTEDID */
	"seen include file",		       /*%< 66 DNS_R_SEENINCLUDE */
	"not exact",		       	       /*%< 67 DNS_R_NOTEXACT */
	"address blackholed",	       	       /*%< 68 DNS_R_BLACKHOLED */
	"bad algorithm",		       /*%< 69 DNS_R_BADALG */

	"invalid use of a meta type",	       /*%< 70 DNS_R_METATYPE */
	"CNAME and other data",		       /*%< 71 DNS_R_CNAMEANDOTHER */
	"multiple RRs of singleton type",      /*%< 72 DNS_R_SINGLETON */
	"hint nxrrset",			       /*%< 73 DNS_R_HINTNXRRSET */
	"no master file configured",	       /*%< 74 DNS_R_NOMASTERFILE */

	"unknown protocol",		       /*%< 75 DNS_R_UNKNOWNPROTO */
	"clocks are unsynchronized",	       /*%< 76 DNS_R_CLOCKSKEW */
	"IXFR failed",			       /*%< 77 DNS_R_BADIXFR */
	"not authoritative",		       /*%< 78 DNS_R_NOTAUTHORITATIVE */
	"no valid KEY",		       	       /*%< 79 DNS_R_NOVALIDKEY */

	"obsolete",			       /*%< 80 DNS_R_OBSOLETE */
	"already frozen",		       /*%< 81 DNS_R_FROZEN */
	"unknown flag",			       /*%< 82 DNS_R_UNKNOWNFLAG */
	"expected a response",		       /*%< 83 DNS_R_EXPECTEDRESPONSE */
	"no valid DS",			       /*%< 84 DNS_R_NOVALIDDS */

	"NS is an address",		       /*%< 85 DNS_R_NSISADDRESS */
	"received FORMERR",		       /*%< 86 DNS_R_REMOTEFORMERR */
	"truncated TCP response",	       /*%< 87 DNS_R_TRUNCATEDTCP */
	"lame server detected",		       /*%< 88 DNS_R_LAME */
	"unexpected RCODE",		       /*%< 89 DNS_R_UNEXPECTEDRCODE */

	"unexpected OPCODE",		       /*%< 90 DNS_R_UNEXPECTEDOPCODE */
	"chase DS servers",		       /*%< 91 DNS_R_CHASEDSSERVERS */
	"empty name",			       /*%< 92 DNS_R_EMPTYNAME */
	"empty wild",			       /*%< 93 DNS_R_EMPTYWILD */
	"bad bitmap",			       /*%< 94 DNS_R_BADBITMAP */

	"from wildcard",		       /*%< 95 DNS_R_FROMWILDCARD */
	"bad owner name (check-names)",	       /*%< 96 DNS_R_BADOWNERNAME */
	"bad name (check-names)",	       /*%< 97 DNS_R_BADNAME */
	"dynamic zone",			       /*%< 98 DNS_R_DYNAMIC */
	"unknown command",		       /*%< 99 DNS_R_UNKNOWNCOMMAND */

	"must-be-secure",		       /*%< 100 DNS_R_MUSTBESECURE */
	"covering NSEC record returned",       /*%< 101 DNS_R_COVERINGNSEC */
	"MX is an address",		       /*%< 102 DNS_R_MXISADDRESS */
	"duplicate query",		       /*%< 103 DNS_R_DUPLICATE */
	"invalid NSEC3 owner name (wildcard)", /*%< 104 DNS_R_INVALIDNSEC3 */

	"not master",			       /*%< 105 DNS_R_NOTMASTER */
	"broken trust chain",		       /*%< 106 DNS_R_BROKENCHAIN */
	"expired",			       /*%< 107 DNS_R_EXPIRED */
	"not dynamic",			       /*%< 108 DNS_R_NOTDYNAMIC */
	"bad EUI",			       /*%< 109 DNS_R_BADEUI */

	"covered by negative trust anchor",    /*%< 110 DNS_R_NTACOVERED */
	"bad CDS",			       /*%< 111 DNS_R_BADCDS */
	"bad CDNSKEY",			       /*%< 112 DNS_R_BADCDNSKEY */
	"malformed OPT option",		       /*%< 113 DNS_R_OPTERR */
	"malformed DNSTAP data",	       /*%< 114 DNS_R_BADDNSTAP */

	"TSIG in wrong location",	       /*%< 115 DNS_R_BADTSIG */
	"SIG(0) in wrong location",	       /*%< 116 DNS_R_BADSIG0 */
	"too many records",	               /*%< 117 DNS_R_TOOMANYRECORDS */
};

static const char *rcode_text[DNS_R_NRCODERESULTS] = {
	"NOERROR",				/*%< 0 DNS_R_NOERROR */
	"FORMERR",				/*%< 1 DNS_R_FORMERR */
	"SERVFAIL",				/*%< 2 DNS_R_SERVFAIL */
	"NXDOMAIN",				/*%< 3 DNS_R_NXDOMAIN */
	"NOTIMP",				/*%< 4 DNS_R_NOTIMP */

	"REFUSED",				/*%< 5 DNS_R_REFUSED */
	"YXDOMAIN",				/*%< 6 DNS_R_YXDOMAIN */
	"YXRRSET",				/*%< 7 DNS_R_YXRRSET */
	"NXRRSET",				/*%< 8 DNS_R_NXRRSET */
	"NOTAUTH",				/*%< 9 DNS_R_NOTAUTH */

	"NOTZONE",				/*%< 10 DNS_R_NOTZONE */
	"<rcode 11>",				/*%< 11 has no macro */
	"<rcode 12>",				/*%< 12 has no macro */
	"<rcode 13>",				/*%< 13 has no macro */
	"<rcode 14>",				/*%< 14 has no macro */

	"<rcode 15>",				/*%< 15 has no macro */
	"BADVERS",				/*%< 16 DNS_R_BADVERS */
};

#define DNS_RESULT_RESULTSET			2
#define DNS_RESULT_RCODERESULTSET		3

static int		once = 0;

static void
initialize_action(void) {
	isc_result_t result;

	result = isc_result_register(ISC_RESULTCLASS_DNS, DNS_R_NRESULTS,
				     text, DNS_RESULT_RESULTSET);
	if (result == ISC_R_SUCCESS)
		result = isc_result_register(ISC_RESULTCLASS_DNSRCODE,
					     DNS_R_NRCODERESULTS,
					     rcode_text,
					     DNS_RESULT_RCODERESULTSET);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_result_register() failed: %u", result);
}

static void
initialize(void) {
	if (!once) {
		once = 1;
		initialize_action();
	}
}

void
dns_result_register(void) {
	initialize();
}
