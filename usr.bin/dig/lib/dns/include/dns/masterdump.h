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

#ifndef DNS_MASTERDUMP_H
#define DNS_MASTERDUMP_H 1

/*! \file dns/masterdump.h */

/***
 ***	Imports
 ***/

#include <stdio.h>

#include <dns/types.h>

/***
 *** Types
 ***/

typedef struct dns_master_style dns_master_style_t;

/***
 *** Definitions
 ***/

/*
 * Flags affecting master file formatting.  Flags 0x0000FFFF
 * define the formatting of the rdata part and are defined in
 * rdata.h.
 */

/*% Omit the owner name when possible. */
#define	DNS_STYLEFLAG_OMIT_OWNER        0x000010000ULL

/*%
 * Omit the TTL when possible.  If DNS_STYLEFLAG_TTL is
 * also set, this means no TTLs are ever printed
 * because $TTL directives are generated before every
 * change in the TTL.  In this case, no columns need to
 * be reserved for the TTL.  Master files generated with
 * these options will be rejected by BIND 4.x because it
 * does not recognize the $TTL directive.
 *
 * If DNS_STYLEFLAG_TTL is not also set, the TTL will be
 * omitted when it is equal to the previous TTL.
 * This is correct according to RFC1035, but the
 * TTLs may be silently misinterpreted by older
 * versions of BIND which use the SOA MINTTL as a
 * default TTL value.
 */
#define	DNS_STYLEFLAG_OMIT_TTL		0x000020000ULL

/*% Omit the class when possible. */
#define	DNS_STYLEFLAG_OMIT_CLASS	0x000040000ULL

/*% Output $TTL directives. */
#define	DNS_STYLEFLAG_TTL		0x000080000ULL

/*%
 * Output $ORIGIN directives and print owner names relative to
 * the origin when possible.
 */
#define	DNS_STYLEFLAG_REL_OWNER		0x000100000ULL

/*% Print domain names in RR data in relative form when possible.
   For this to take effect, DNS_STYLEFLAG_REL_OWNER must also be set. */
#define	DNS_STYLEFLAG_REL_DATA		0x000200000ULL

/*% Print the trust level of each rdataset. */
#define	DNS_STYLEFLAG_TRUST		0x000400000ULL

/*% Print negative caching entries. */
#define	DNS_STYLEFLAG_NCACHE		0x000800000ULL

/*% Never print the TTL. */
#define	DNS_STYLEFLAG_NO_TTL		0x001000000ULL

/*% Never print the CLASS. */
#define	DNS_STYLEFLAG_NO_CLASS		0x002000000ULL

/*% Report re-signing time. */
#define	DNS_STYLEFLAG_RESIGN		0x004000000ULL

/*% Don't printout the cryptographic parts of DNSSEC records. */
#define	DNS_STYLEFLAG_NOCRYPTO		0x008000000ULL

/*% Comment out data by prepending with ";" */
#define	DNS_STYLEFLAG_COMMENTDATA	0x010000000ULL

/***
 ***	Constants
 ***/

/*%
 * The style used for debugging, "dig" output, etc.
 */
extern const dns_master_style_t dns_master_style_debug;

/***
 ***	Functions
 ***/

isc_result_t
dns_master_rdatasettotext(dns_name_t *owner_name,
			  dns_rdataset_t *rdataset,
			  const dns_master_style_t *style,
			  isc_buffer_t *target);
/*%<
 * Convert 'rdataset' to text format, storing the result in 'target'.
 *
 * Notes:
 *\li	The rdata cursor position will be changed.
 *
 * Requires:
 *\li	'rdataset' is a valid non-question rdataset.
 *
 *\li	'rdataset' is not empty.
 */

isc_result_t
dns_master_questiontotext(dns_name_t *owner_name,
			  dns_rdataset_t *rdataset,
			  const dns_master_style_t *style,
			  isc_buffer_t *target);

isc_result_t
dns_master_stylecreate2(dns_master_style_t **style, unsigned int flags,
		       unsigned int ttl_column, unsigned int class_column,
		       unsigned int type_column, unsigned int rdata_column,
		       unsigned int line_length, unsigned int tab_width,
		       unsigned int split_width);
void
dns_master_styledestroy(dns_master_style_t **style);

#endif /* DNS_MASTERDUMP_H */
