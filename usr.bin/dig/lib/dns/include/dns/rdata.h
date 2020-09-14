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

#ifndef DNS_RDATA_H
#define DNS_RDATA_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/rdata.h
 * \brief
 * Provides facilities for manipulating DNS rdata, including conversions to
 * and from wire format and text format.
 *
 * Given the large amount of rdata possible in a nameserver, it was important
 * to come up with a very efficient way of storing rdata, but at the same
 * time allow it to be manipulated.
 *
 * The decision was to store rdata in uncompressed wire format,
 * and not to make it a fully abstracted object; i.e. certain parts of the
 * server know rdata is stored that way.  This saves a lot of memory, and
 * makes adding rdata to messages easy.  Having much of the server know
 * the representation would be perilous, and we certainly don't want each
 * user of rdata to be manipulating such a low-level structure.  This is
 * where the rdata module comes in.  The module allows rdata handles to be
 * created and attached to uncompressed wire format regions.  All rdata
 * operations and conversions are done through these handles.
 *
 * Implementation Notes:
 *
 *\li	The routines in this module are expected to be synthesized by the
 *	build process from a set of source files, one per rdata type.  For
 *	portability, it's probably best that the building be done by a C
 *	program.  Adding a new rdata type will be a simple matter of adding
 *	a file to a directory and rebuilding the server.  *All* knowledge of
 *	the format of a particular rdata type is in this file.
 *
 * MP:
 *\li	Clients of this module must impose any required synchronization.
 *
 * Reliability:
 *\li	This module deals with low-level byte streams.  Errors in any of
 *	the functions are likely to crash the server or corrupt memory.
 *
 *\li	Rdata is typed, and the caller must know what type of rdata it has.
 *	A caller that gets this wrong could crash the server.
 *
 *\li	The fromstruct() and tostruct() routines use a void * pointer to
 *	represent the structure.  The caller must ensure that it passes a
 *	pointer to the appropriate type, or the server could crash or memory
 *	could be corrupted.
 *
 * Resources:
 *\li	None.
 *
 * Security:
 *
 *\li	*** WARNING ***
 *	dns_rdata_fromwire() deals with raw network data.  An error in
 *	this routine could result in the failure or hijacking of the server.
 *
 * Standards:
 *\li	RFC1035
 *\li	Draft EDNS0 (0)
 *\li	Draft EDNS1 (0)
 *\li	Draft Binary Labels (2)
 *\li	Draft Local Compression (1)
 *\li	Various RFCs for particular types; these will be documented in the
 *	 sources files of the types.
 *
 */

/***
 *** Imports
 ***/

#include <dns/types.h>
#include <dns/name.h>
#include <dns/message.h>

/***
 *** Types
 ***/

typedef struct dns_rdatacommon {
	dns_rdataclass_t			rdclass;
	dns_rdatatype_t				rdtype;
	ISC_LINK(struct dns_rdatacommon)	link;
} dns_rdatacommon_t;

typedef struct dns_rdata_cname {
	dns_rdatacommon_t	common;
	dns_name_t		cname;
} dns_rdata_cname_t;

typedef struct dns_rdata_ns {
	dns_rdatacommon_t	common;
	dns_name_t		name;
} dns_rdata_ns_t;

typedef struct dns_rdata_soa {
	dns_rdatacommon_t	common;
	dns_name_t		origin;
	dns_name_t		contact;
	uint32_t		serial;		/*%< host order */
	uint32_t		refresh;	/*%< host order */
	uint32_t		retry;		/*%< host order */
	uint32_t		expire;		/*%< host order */
	uint32_t		minimum;	/*%< host order */
} dns_rdata_soa_t;

typedef struct dns_rdata_any_tsig {
	dns_rdatacommon_t	common;
	dns_name_t		algorithm;
	uint64_t		timesigned;
	uint16_t		fudge;
	uint16_t		siglen;
	unsigned char *		signature;
	uint16_t		originalid;
	uint16_t		error;
	uint16_t		otherlen;
	unsigned char *		other;
} dns_rdata_any_tsig_t;

/*%
 ***** An 'rdata' is a handle to a binary region.  The handle has an RR
 ***** class and type, and the data in the binary region is in the format
 ***** of the given class and type.
 *****/
/*%
 * Clients are strongly discouraged from using this type directly, with
 * the exception of the 'link' field which may be used directly for whatever
 * purpose the client desires.
 */
struct dns_rdata {
	unsigned char *			data;
	unsigned int			length;
	dns_rdataclass_t		rdclass;
	dns_rdatatype_t			type;
	unsigned int			flags;
	ISC_LINK(dns_rdata_t)		link;
};

#define DNS_RDATA_INIT { NULL, 0, 0, 0, 0, {(void*)(-1), (void *)(-1)}}

#define DNS_RDATA_INITIALIZED(rdata) \
	((rdata)->data == NULL && (rdata)->length == 0 && \
	 (rdata)->rdclass == 0 && (rdata)->type == 0 && (rdata)->flags == 0 && \
	 !ISC_LINK_LINKED((rdata), link))

#define DNS_RDATA_UPDATE	0x0001		/*%< update pseudo record. */
#define DNS_RDATA_OFFLINE	0x0002		/*%< RRSIG has a offline key. */

#define DNS_RDATA_VALIDFLAGS(rdata) \
	(((rdata)->flags & ~(DNS_RDATA_UPDATE|DNS_RDATA_OFFLINE)) == 0)

/*
 * The maximum length of a RDATA that can be sent on the wire.
 * Max packet size (65535) less header (12), less name (1), type (2),
 * class (2), ttl(4), length (2).
 *
 * None of the defined types that support name compression can exceed
 * this and all new types are to be sent uncompressed.
 */

#define DNS_RDATA_MAXLENGTH	65512U

/*
 * Flags affecting rdata formatting style.  Flags 0xFFFF0000
 * are used by masterfile-level formatting and defined elsewhere.
 * See additional comments at dns_rdata_tofmttext().
 */

/*% Split the rdata into multiple lines to try to keep it
 within the "width". */
#define DNS_STYLEFLAG_MULTILINE		0x00000001ULL

/*% Output explanatory comments. */
#define DNS_STYLEFLAG_COMMENT		0x00000002ULL
#define DNS_STYLEFLAG_RRCOMMENT		0x00000004ULL

/*% Output KEYDATA in human readable format. */
#define DNS_STYLEFLAG_KEYDATA		0x00000008ULL

/*% Output textual RR type and RDATA in RFC 3597 unknown format */
#define DNS_STYLEFLAG_UNKNOWNFORMAT	0x00000010ULL

#define DNS_RDATA_DOWNCASE		DNS_NAME_DOWNCASE
#define DNS_RDATA_CHECKNAMES		DNS_NAME_CHECKNAMES
#define DNS_RDATA_CHECKNAMESFAIL	DNS_NAME_CHECKNAMESFAIL
#define DNS_RDATA_CHECKREVERSE		DNS_NAME_CHECKREVERSE
#define DNS_RDATA_CHECKMX		DNS_NAME_CHECKMX
#define DNS_RDATA_CHECKMXFAIL		DNS_NAME_CHECKMXFAIL
#define DNS_RDATA_UNKNOWNESCAPE		0x80000000

/***
 *** Initialization
 ***/

void
dns_rdata_init(dns_rdata_t *rdata);
/*%<
 * Make 'rdata' empty.
 *
 * Requires:
 *	'rdata' is a valid rdata (i.e. not NULL, points to a struct dns_rdata)
 */

void
dns_rdata_reset(dns_rdata_t *rdata);
/*%<
 * Make 'rdata' empty.
 *
 * Requires:
 *\li	'rdata' is a previously initialized rdata and is not linked.
 */

void
dns_rdata_clone(const dns_rdata_t *src, dns_rdata_t *target);
/*%<
 * Clone 'target' from 'src'.
 *
 * Requires:
 *\li	'src' to be initialized.
 *\li	'target' to be initialized.
 */

/***
 *** Conversions
 ***/

void
dns_rdata_fromregion(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, isc_region_t *r);
/*%<
 * Make 'rdata' refer to region 'r'.
 *
 * Requires:
 *
 *\li	The data in 'r' is properly formatted for whatever type it is.
 */

void
dns_rdata_toregion(const dns_rdata_t *rdata, isc_region_t *r);
/*%<
 * Make 'r' refer to 'rdata'.
 */

isc_result_t
dns_rdata_fromwire(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_buffer_t *source,
		   dns_decompress_t *dctx, unsigned int options,
		   isc_buffer_t *target);
/*%<
 * Copy the possibly-compressed rdata at source into the target region.
 *
 * Notes:
 *\li	Name decompression policy is controlled by 'dctx'.
 *
 *	'options'
 *\li	DNS_RDATA_DOWNCASE	downcase domain names when they are copied
 *				into target.
 *
 * Requires:
 *
 *\li	'rdclass' and 'type' are valid.
 *
 *\li	'source' is a valid buffer, and the active region of 'source'
 *	references the rdata to be processed.
 *
 *\li	'target' is a valid buffer.
 *
 *\li	'dctx' is a valid decompression context.
 *
 * Ensures,
 *	if result is success:
 *	\li 	If 'rdata' is not NULL, it is attached to the target.
 *	\li	The conditions dns_name_fromwire() ensures for names hold
 *		for all names in the rdata.
 *	\li	The current location in source is advanced, and the used space
 *		in target is updated.
 *
 * Result:
 *\li	Success
 *\li	Any non-success status from dns_name_fromwire()
 *\li	Various 'Bad Form' class failures depending on class and type
 *\li	Bad Form: Input too short
 *\li	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_towire(dns_rdata_t *rdata, dns_compress_t *cctx,
		 isc_buffer_t *target);
/*%<
 * Convert 'rdata' into wire format, compressing it as specified by the
 * compression context 'cctx', and storing the result in 'target'.
 *
 * Notes:
 *\li	If the compression context allows global compression, then the
 *	global compression table may be updated.
 *
 * Requires:
 *\li	'rdata' is a valid, non-empty rdata
 *
 *\li	target is a valid buffer
 *
 *\li	Any offsets specified in a global compression table are valid
 *	for target.
 *
 * Ensures,
 *	if the result is success:
 *	\li	The used space in target is updated.
 *
 * Returns:
 *\li	Success
 *\li	Any non-success status from dns_name_towire()
 *\li	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_totext(dns_rdata_t *rdata, dns_name_t *origin, isc_buffer_t *target);
/*%<
 * Convert 'rdata' into text format, storing the result in 'target'.
 * The text will consist of a single line, with fields separated by
 * single spaces.
 *
 * Notes:
 *\li	If 'origin' is not NULL, then any names in the rdata that are
 *	subdomains of 'origin' will be made relative it.
 *
 *\li	XXX Do we *really* want to support 'origin'?  I'm inclined towards "no"
 *	at the moment.
 *
 * Requires:
 *
 *\li	'rdata' is a valid, non-empty rdata
 *
 *\li	'origin' is NULL, or is a valid name
 *
 *\li	'target' is a valid text buffer
 *
 * Ensures,
 *	if the result is success:
 *
 *	\li	The used space in target is updated.
 *
 * Returns:
 *\li	Success
 *\li	Any non-success status from dns_name_totext()
 *\li	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_tofmttext(dns_rdata_t *rdata, dns_name_t *origin, unsigned int flags,
		    unsigned int width, unsigned int split_width,
		    const char *linebreak, isc_buffer_t *target);
/*%<
 * Like dns_rdata_totext, but do formatted output suitable for
 * database dumps.  This is intended for use by dns_db_dump();
 * library users are discouraged from calling it directly.
 *
 * If (flags & #DNS_STYLEFLAG_MULTILINE) != 0, attempt to stay
 * within 'width' by breaking the text into multiple lines.
 * The string 'linebreak' is inserted between lines, and parentheses
 * are added when necessary.  Because RRs contain unbreakable elements
 * such as domain names whose length is variable, unpredictable, and
 * potentially large, there is no guarantee that the lines will
 * not exceed 'width' anyway.
 *
 * If (flags & #DNS_STYLEFLAG_MULTILINE) == 0, the rdata is always
 * printed as a single line, and no parentheses are used.
 * The 'width' and 'linebreak' arguments are ignored.
 *
 * If (flags & #DNS_STYLEFLAG_COMMENT) != 0, output explanatory
 * comments next to things like the SOA timer fields.  Some
 * comments (e.g., the SOA ones) are only printed when multiline
 * output is selected.
 *
 * base64 rdata text (e.g., DNSKEY records) will be split into chunks
 * of 'split_width' characters.  If split_width == 0, the text will
 * not be split at all.  If split_width == UINT_MAX (0xffffffff), then
 * it is undefined and falls back to the default value of 'width'
 */

isc_result_t
dns_rdata_fromstruct_soa(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		          dns_rdatatype_t type, dns_rdata_soa_t *soa,
		          isc_buffer_t *target);
/*%<
 * Convert the C structure representation of an rdata into uncompressed wire
 * format in 'target'.
 *
 * XXX  Should we have a 'size' parameter as a sanity check on target?
 *
 * Requires:
 *
 *\li	'rdclass' and 'type' are valid.
 *
 *\li	'source' points to a valid C struct for the class and type.
 *
 *\li	'target' is a valid buffer.
 *
 *\li	All structure pointers to memory blocks should be NULL if their
 *	corresponding length values are zero.
 *
 * Ensures,
 *	if result is success:
 *	\li 	If 'rdata' is not NULL, it is attached to the target.
 *
 *	\li	The used space in 'target' is updated.
 *
 * Result:
 *\li	Success
 *\li	Various 'Bad Form' class failures depending on class and type
 *\li	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_fromstruct_tsig(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		          dns_rdatatype_t type, dns_rdata_any_tsig_t *tsig,
		          isc_buffer_t *target);
/*%<
 * Convert the C structure representation of an rdata into uncompressed wire
 * format in 'target'.
 *
 * XXX  Should we have a 'size' parameter as a sanity check on target?
 *
 * Requires:
 *
 *\li	'rdclass' and 'type' are valid.
 *
 *\li	'source' points to a valid C struct for the class and type.
 *
 *\li	'target' is a valid buffer.
 *
 *\li	All structure pointers to memory blocks should be NULL if their
 *	corresponding length values are zero.
 *
 * Ensures,
 *	if result is success:
 *	\li 	If 'rdata' is not NULL, it is attached to the target.
 *
 *	\li	The used space in 'target' is updated.
 *
 * Result:
 *\li	Success
 *\li	Various 'Bad Form' class failures depending on class and type
 *\li	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_tostruct_cname(const dns_rdata_t *rdata, dns_rdata_cname_t *cname);
/*%<
 * Convert an rdata into its C structure representation.
 *
 *
 * Requires:
 *
 *\li	'rdata' is a valid, non-empty rdata.
 *
 *\li	'cname' to point to a valid pointer for the type and class.
 *
 * Result:
 *\li	Success
 *\li	Resource Limit: Not enough memory
 */

isc_result_t
dns_rdata_tostruct_ns(const dns_rdata_t *rdata, dns_rdata_ns_t *ns);
/*%<
 * Convert an rdata into its C structure representation.
 *
 *
 * Requires:
 *
 *\li	'rdata' is a valid, non-empty rdata.
 *
 *\li	'ns' to point to a valid pointer for the type and class.
 *
 * Result:
 *\li	Success
 *\li	Resource Limit: Not enough memory
 */

isc_result_t
dns_rdata_tostruct_soa(const dns_rdata_t *rdata, dns_rdata_soa_t *soa);
/*%<
 * Convert an rdata into its C structure representation.
 *
 *
 * Requires:
 *
 *\li	'rdata' is a valid, non-empty rdata.
 *
 *\li	'soa' to point to a valid pointer for the type and class.
 *
 * Result:
 *\li	Success
 *\li	Resource Limit: Not enough memory
 */

isc_result_t
dns_rdata_tostruct_tsig(const dns_rdata_t *rdata, dns_rdata_any_tsig_t *tsig);
/*%<
 * Convert an rdata into its C structure representation.
 *
 *
 * Requires:
 *
 *\li	'rdata' is a valid, non-empty rdata.
 *
 *\li	'tsig' to point to a valid pointer for the type and class.
 *
 * Result:
 *\li	Success
 *\li	Resource Limit: Not enough memory
 */

void
dns_rdata_freestruct_cname(dns_rdata_cname_t *cname);
/*%<
 * Free dynamic memory attached to 'cname' (if any).
 *
 * Requires:
 *
 *\li	'cname' to point to the structure previously filled in by
 *	dns_rdata_tostruct_cname().
 */

void
dns_rdata_freestruct_ns(dns_rdata_ns_t *ns);
/*%<
 * Free dynamic memory attached to 'ns' (if any).
 *
 * Requires:
 *
 *\li	'ns' to point to the structure previously filled in by
 *	dns_rdata_tostruct_ns().
 */

void
dns_rdata_freestruct_soa(dns_rdata_soa_t *soa);
/*%<
 * Free dynamic memory attached to 'soa' (if any).
 *
 * Requires:
 *
 *\li	'soa' to point to the structure previously filled in by
 *	dns_rdata_tostruct_soa().
 */

void
dns_rdata_freestruct_tsig(dns_rdata_any_tsig_t *tsig);
/*%<
 * Free dynamic memory attached to 'tsig' (if any).
 *
 * Requires:
 *
 *\li	'tsig' to point to the structure previously filled in by
 *	dns_rdata_tostruct_tsig().
 */

unsigned int
dns_rdatatype_attributes(dns_rdatatype_t rdtype);
/*%<
 * Return attributes for the given type.
 *
 * Requires:
 *\li	'rdtype' are known.
 *
 * Returns:
 *\li	a bitmask consisting of the following flags.
 */

/*% Is reserved (unusable) */
#define DNS_RDATATYPEATTR_RESERVED		0x00000020U
/*% Is an unknown type */
#define DNS_RDATATYPEATTR_UNKNOWN		0x00000040U

dns_rdatatype_t
dns_rdata_covers(dns_rdata_t *rdata);
/*%<
 * Return the rdatatype that this type covers.
 *
 * Requires:
 *\li	'rdata' is a valid, non-empty rdata.
 *
 *\li	'rdata' is a type that covers other rdata types.
 *
 * Returns:
 *\li	The type covered.
 */

int
dns_rdata_checkowner_nsec3(dns_name_t* name, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, int wildcard);
/*
 * Returns whether this is a valid ownername for this <type,class>.
 * If wildcard is true allow the first label to be a wildcard if
 * appropriate.
 *
 * Requires:
 *	'name' is a valid name.
 */

#endif /* DNS_RDATA_H */
