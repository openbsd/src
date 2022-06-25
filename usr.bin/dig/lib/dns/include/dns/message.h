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

#ifndef DNS_MESSAGE_H
#define DNS_MESSAGE_H 1

/***
 ***	Imports
 ***/

#include <dns/compress.h>
#include <dns/masterdump.h>
#include <dns/types.h>

#include <dst/dst.h>

/*! \file dns/message.h
 * \brief Message Handling Module
 *
 * How this beast works:
 *
 * When a dns message is received in a buffer, dns_message_parse() is called
 * on the memory region.  Various items are checked including the format
 * of the message (if counts are right, if counts consume the entire sections,
 * and if sections consume the entire message) and known pseudo-RRs in the
 * additional data section are analyzed and removed.
 *
 * TSIG checking is also done at this layer, and any DNSSEC transaction
 * signatures should also be checked here.
 *
 * Notes on using the gettemp*() and puttemp*() functions:
 *
 * These functions return items (names, rdatasets, etc) allocated from some
 * internal state of the dns_message_t.
 *
 * Names and rdatasets must be put back into the dns_message_t in
 * one of two ways.  Assume a name was allocated via
 * dns_message_gettempname():
 *
 *\li	(1) insert it into a section, using dns_message_addname().
 *
 *\li	(2) return it to the message using dns_message_puttempname().
 *
 * The same applies to rdatasets.
 *
 * On the other hand, offsets, rdatalists and rdatas allocated using
 * dns_message_gettemp*() will always be freed automatically
 * when the message is reset or destroyed; calling dns_message_puttemp*()
 * on rdatalists and rdatas is optional and serves only to enable the item
 * to be reused multiple times during the lifetime of the message; offsets
 * cannot be reused.
 *
 * Buffers allocated using isc_buffer_allocate() can be automatically freed
 * as well by giving the buffer to the message using dns_message_takebuffer().
 * Doing this will cause the buffer to be freed using isc_buffer_free()
 * when the section lists are cleared, such as in a reset or in a destroy.
 * Since the buffer itself exists until the message is destroyed, this sort
 * of code can be written:
 *
 * \code
 *	buffer = isc_buffer_allocate(mctx, 512);
 *	name = NULL;
 *	name = dns_message_gettempname(message, &name);
 *	dns_name_init(name, NULL);
 *	result = dns_name_fromtext(name, &source, dns_rootname, 0, buffer);
 *	dns_message_takebuffer(message, &buffer);
 * \endcode
 *
 *
 * TODO:
 *
 * XXX Needed:  ways to set and retrieve EDNS information, add rdata to a
 * section, move rdata from one section to another, remove rdata, etc.
 */

#define DNS_MESSAGEFLAG_QR		0x8000U
#define DNS_MESSAGEFLAG_AA		0x0400U
#define DNS_MESSAGEFLAG_TC		0x0200U
#define DNS_MESSAGEFLAG_RD		0x0100U
#define DNS_MESSAGEFLAG_RA		0x0080U
#define DNS_MESSAGEFLAG_AD		0x0020U
#define DNS_MESSAGEFLAG_CD		0x0010U

/*%< EDNS0 extended message flags */
#define DNS_MESSAGEEXTFLAG_DO		0x8000U

/*%< EDNS0 extended OPT codes */
#define DNS_OPT_NSID		3		/*%< NSID opt code */
#define DNS_OPT_CLIENT_SUBNET	8		/*%< client subnet opt code */
#define DNS_OPT_EXPIRE		9		/*%< EXPIRE opt code */
#define DNS_OPT_COOKIE		10		/*%< COOKIE opt code */
#define DNS_OPT_PAD		12		/*%< PAD opt code */
#define DNS_OPT_KEY_TAG		14		/*%< Key tag opt code */
#define DNS_OPT_EDE		15		/* RFC 8914 */

/*%< The number of EDNS options we know about. */
#define DNS_EDNSOPTIONS	4

#define DNS_MESSAGE_REPLYPRESERVE	(DNS_MESSAGEFLAG_RD|DNS_MESSAGEFLAG_CD)
#define DNS_MESSAGEEXTFLAG_REPLYPRESERVE (DNS_MESSAGEEXTFLAG_DO)

#define DNS_MESSAGE_HEADERLEN		12 /*%< 6 uint16_t's */

/*
 * Ordering here matters.  DNS_SECTION_ANY must be the lowest and negative,
 * and DNS_SECTION_MAX must be one greater than the last used section.
 */
typedef int dns_section_t;
#define DNS_SECTION_ANY			(-1)
#define DNS_SECTION_QUESTION		0
#define DNS_SECTION_ANSWER		1
#define DNS_SECTION_AUTHORITY		2
#define DNS_SECTION_ADDITIONAL		3
#define DNS_SECTION_MAX			4

typedef int dns_pseudosection_t;
#define DNS_PSEUDOSECTION_ANY		(-1)
#define DNS_PSEUDOSECTION_OPT           0
#define DNS_PSEUDOSECTION_TSIG          1
#define DNS_PSEUDOSECTION_SIG0          2
#define DNS_PSEUDOSECTION_MAX           3

typedef int dns_messagetextflag_t;
#define DNS_MESSAGETEXTFLAG_NOCOMMENTS	0x0001
#define DNS_MESSAGETEXTFLAG_NOHEADERS	0x0002
#define DNS_MESSAGETEXTFLAG_ONESOA	0x0004
#define DNS_MESSAGETEXTFLAG_OMITSOA	0x0008

/*
 * Dynamic update names for these sections.
 */
#define DNS_SECTION_ZONE		DNS_SECTION_QUESTION
#define DNS_SECTION_PREREQUISITE	DNS_SECTION_ANSWER
#define DNS_SECTION_UPDATE		DNS_SECTION_AUTHORITY

/*
 * These tell the message library how the created dns_message_t will be used.
 */
#define DNS_MESSAGE_INTENTUNKNOWN	0 /*%< internal use only */
#define DNS_MESSAGE_INTENTPARSE		1 /*%< parsing messages */
#define DNS_MESSAGE_INTENTRENDER	2 /*%< rendering */

/*
 * Control behavior of parsing
 */
#define DNS_MESSAGEPARSE_BESTEFFORT	0x0002	/*%< return a message if a
						   recoverable parse error
						   occurs */
#define DNS_MESSAGEPARSE_IGNORETRUNCATION 0x0008 /*%< truncation errors are
						  * not fatal. */

typedef struct dns_msgblock dns_msgblock_t;

struct dns_message {
	/* public from here down */
	dns_messageid_t			id;
	unsigned int			flags;
	dns_rcode_t			rcode;
	dns_opcode_t			opcode;
	dns_rdataclass_t		rdclass;

	/* 4 real, 1 pseudo */
	unsigned int			counts[DNS_SECTION_MAX];

	/* private from here down */
	dns_namelist_t			sections[DNS_SECTION_MAX];
	dns_name_t		       *cursors[DNS_SECTION_MAX];
	dns_rdataset_t		       *opt;
	dns_rdataset_t		       *sig0;
	dns_rdataset_t		       *tsig;

	int				state;
	unsigned int			from_to_wire : 2;
	unsigned int			header_ok : 1;
	unsigned int			question_ok : 1;
	unsigned int			tcp_continuation : 1;
	unsigned int			verified_sig : 1;
	unsigned int			verify_attempted : 1;
	unsigned int			free_query : 1;
	unsigned int			free_saved : 1;
	unsigned int			sitok : 1;
	unsigned int			sitbad : 1;
	unsigned int			tkey : 1;
	unsigned int			rdclass_set : 1;

	unsigned int			opt_reserved;
	unsigned int			sig_reserved;
	unsigned int			reserved; /* reserved space (render) */

	isc_buffer_t		       *buffer;
	dns_compress_t		       *cctx;

	isc_bufferlist_t		scratchpad;
	isc_bufferlist_t		cleanup;

	ISC_LIST(dns_msgblock_t)	rdatas;
	ISC_LIST(dns_msgblock_t)	rdatalists;
	ISC_LIST(dns_msgblock_t)	offsets;

	ISC_LIST(dns_rdata_t)		freerdata;
	ISC_LIST(dns_rdatalist_t)	freerdatalist;

	dns_rcode_t			tsigstatus;
	dns_rcode_t			querytsigstatus;
	dns_name_t		       *tsigname; /* Owner name of TSIG, if any */
	dns_rdataset_t		       *querytsig;
	dns_tsigkey_t		       *tsigkey;
	dst_context_t		       *tsigctx;
	int				sigstart;
	int				timeadjust;

	dns_name_t		       *sig0name; /* Owner name of SIG0, if any */
	dns_rcode_t			sig0status;
	isc_region_t			query;
	isc_region_t			saved;
};

struct dns_ednsopt {
	uint16_t			code;
	uint16_t			length;
	unsigned char			*value;
};

/***
 *** Functions
 ***/

isc_result_t
dns_message_create(unsigned int intent, dns_message_t **msgp);

/*%<
 * Create msg structure.
 *
 * This function will allocate some internal blocks of memory that are
 * expected to be needed for parsing or rendering nearly any type of message.
 *
 * Requires:
 *\li	'mctx' be a valid memory context.
 *
 *\li	'msgp' be non-null and '*msg' be NULL.
 *
 *\li	'intent' must be one of DNS_MESSAGE_INTENTPARSE or
 *	#DNS_MESSAGE_INTENTRENDER.
 *
 * Ensures:
 *\li	The data in "*msg" is set to indicate an unused and empty msg
 *	structure.
 *
 * Returns:
 *\li	#ISC_R_NOMEMORY		-- out of memory
 *\li	#ISC_R_SUCCESS		-- success
 */

void
dns_message_destroy(dns_message_t **msgp);
/*%<
 * Destroy all state in the message.
 *
 * Requires:
 *
 *\li	'msgp' be valid.
 *
 * Ensures:
 *\li	'*msgp' == NULL
 */

isc_result_t
dns_message_sectiontotext(dns_message_t *msg, dns_section_t section,
			  const dns_master_style_t *style,
			  dns_messagetextflag_t flags,
			  isc_buffer_t *target);

isc_result_t
dns_message_pseudosectiontotext(dns_message_t *msg,
				dns_pseudosection_t section,
				const dns_master_style_t *style,
				dns_messagetextflag_t flags,
				isc_buffer_t *target);
/*%<
 * Convert section 'section' or 'pseudosection' of message 'msg' to
 * a cleartext representation
 *
 * Notes:
 *     \li See dns_message_totext for meanings of flags.
 *
 * Requires:
 *
 *\li	'msg' is a valid message.
 *
 *\li	'style' is a valid master dump style.
 *
 *\li	'target' is a valid buffer.
 *
 *\li	'section' is a valid section label.
 *
 * Ensures:
 *
 *\li	If the result is success:
 *		The used space in 'target' is updated.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOSPACE
 *\li	#ISC_R_NOMORE
 *
 *\li	Note: On error return, *target may be partially filled with data.
*/

isc_result_t
dns_message_parse(dns_message_t *msg, isc_buffer_t *source,
		  unsigned int options);
/*%<
 * Parse raw wire data in 'source' as a DNS message.
 *
 * OPT records are detected and stored in the pseudo-section "opt".
 * TSIGs are detected and stored in the pseudo-section "tsig".
 *
 * A separate dns_name_t object will be created for each RR in the
 * message.  Each such dns_name_t will have a single rdataset containing the
 * single RR, and the order of the RRs in the message is preserved.
 *
 * If #DNS_MESSAGEPARSE_BESTEFFORT is set, errors in message content will
 * not be considered FORMERRs.  If the entire message can be parsed, it
 * will be returned and DNS_R_RECOVERABLE will be returned.
 *
 * If #DNS_MESSAGEPARSE_IGNORETRUNCATION is set then return as many complete
 * RR's as possible, DNS_R_RECOVERABLE will be returned.
 *
 *
 * Requires:
 *\li	"msg" be valid.
 *
 *\li	"buffer" be a wire format buffer.
 *
 * Ensures:
 *\li	The buffer's data format is correct.
 *
 *\li	The buffer's contents verify as correct regarding header bits, buffer
 * 	and rdata sizes, etc.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well
 *\li	#ISC_R_NOMEMORY		-- no memory
 *\li	#DNS_R_RECOVERABLE	-- the message parsed properly, but contained
 *				   errors.
 *\li	Many other errors possible XXXMLG
 */

isc_result_t
dns_message_renderbegin(dns_message_t *msg, dns_compress_t *cctx,
			isc_buffer_t *buffer);
/*%<
 * Begin rendering on a message.  Only one call can be made to this function
 * per message.
 *
 * The compression context is "owned" by the message library until
 * dns_message_renderend() is called.  It must be invalidated by the caller.
 *
 * The buffer is "owned" by the message library until dns_message_renderend()
 * is called.
 *
 * Requires:
 *
 *\li	'msg' be valid.
 *
 *\li	'cctx' be valid.
 *
 *\li	'buffer' is a valid buffer.
 *
 * Side Effects:
 *
 *\li	The buffer is cleared before it is used.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well
 *\li	#ISC_R_NOSPACE		-- output buffer is too small
 */

isc_result_t
dns_message_renderreserve(dns_message_t *msg, unsigned int space);
/*%<
 * XXXMLG should use size_t rather than unsigned int once the buffer
 * API is cleaned up
 *
 * Reserve "space" bytes in the given buffer.
 *
 * Requires:
 *
 *\li	'msg' be valid.
 *
 *\li	dns_message_renderbegin() was called.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well.
 *\li	#ISC_R_NOSPACE		-- not enough free space in the buffer.
 */

void
dns_message_renderrelease(dns_message_t *msg, unsigned int space);
/*%<
 * XXXMLG should use size_t rather than unsigned int once the buffer
 * API is cleaned up
 *
 * Release "space" bytes in the given buffer that was previously reserved.
 *
 * Requires:
 *
 *\li	'msg' be valid.
 *
 *\li	'space' is less than or equal to the total amount of space reserved
 *	via prior calls to dns_message_renderreserve().
 *
 *\li	dns_message_renderbegin() was called.
 */

isc_result_t
dns_message_rendersection(dns_message_t *msg, dns_section_t section);
/*%<
 * Render all names, rdatalists, etc from the given section at the
 * specified priority or higher.
 *
 * Requires:
 *\li	'msg' be valid.
 *
 *\li	'section' be a valid section.
 *
 *\li	dns_message_renderbegin() was called.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all records were written, and there are
 *				   no more records for this section.
 *\li	#ISC_R_NOSPACE		-- Not enough room in the buffer to write
 *				   all records requested.
 *\li	#DNS_R_MOREDATA		-- All requested records written, and there
 *				   are records remaining for this section.
 */

void
dns_message_renderheader(dns_message_t *msg, isc_buffer_t *target);
/*%<
 * Render the message header.  This is implicitly called by
 * dns_message_renderend().
 *
 * Requires:
 *
 *\li	'msg' be a valid message.
 *
 *\li	dns_message_renderbegin() was called.
 *
 *\li	'target' is a valid buffer with enough space to hold a message header
 */

isc_result_t
dns_message_renderend(dns_message_t *msg);
/*%<
 * Finish rendering to the buffer.  Note that more data can be in the
 * 'msg' structure.  Destroying the structure will free this, or in a multi-
 * part EDNS1 message this data can be rendered to another buffer later.
 *
 * Requires:
 *
 *\li	'msg' be a valid message.
 *
 *\li	dns_message_renderbegin() was called.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well.
 */

void
dns_message_renderreset(dns_message_t *msg);
/*%<
 * Reset the message so that it may be rendered again.
 *
 * Notes:
 *
 *\li	If dns_message_renderbegin() has been called, dns_message_renderend()
 *	must be called before calling this function.
 *
 * Requires:
 *
 *\li	'msg' be a valid message with rendering intent.
 */

isc_result_t
dns_message_firstname(dns_message_t *msg, dns_section_t section);
/*%<
 * Set internal per-section name pointer to the beginning of the section.
 *
 * The functions dns_message_firstname() and dns_message_nextname() may
 * be used for iterating over the owner names in a section.
 *
 * Requires:
 *
 *\li   	'msg' be valid.
 *
 *\li	'section' be a valid section.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMORE		-- No names on given section.
 */

isc_result_t
dns_message_nextname(dns_message_t *msg, dns_section_t section);
/*%<
 * Sets the internal per-section name pointer to point to the next name
 * in that section.
 *
 * Requires:
 *
 * \li  	'msg' be valid.
 *
 *\li	'section' be a valid section.
 *
 *\li	dns_message_firstname() must have been called on this section,
 *	and the result was ISC_R_SUCCESS.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMORE		-- No more names in given section.
 */

void
dns_message_currentname(dns_message_t *msg, dns_section_t section,
			dns_name_t **name);
/*%<
 * Sets 'name' to point to the name where the per-section internal name
 * pointer is currently set.
 *
 * This function returns the name in the database, so any data associated
 * with it (via the name's "list" member) contains the actual rdatasets.
 *
 * Requires:
 *
 *\li	'msg' be valid.
 *
 *\li	'name' be non-NULL, and *name be NULL.
 *
 *\li	'section' be a valid section.
 *
 *\li	dns_message_firstname() must have been called on this section,
 *	and the result of it and any dns_message_nextname() calls was
 *	#ISC_R_SUCCESS.
 */

isc_result_t
dns_message_findname(dns_message_t *msg, dns_section_t section,
		     dns_name_t *target, dns_rdatatype_t type,
		     dns_rdatatype_t covers, dns_name_t **foundname,
		     dns_rdataset_t **rdataset);
/*%<
 * Search for a name in the specified section.  If it is found, *name is
 * set to point to the name, and *rdataset is set to point to the found
 * rdataset (if type is specified as other than dns_rdatatype_any).
 *
 * Requires:
 *\li	'msg' be valid.
 *
 *\li	'section' be a valid section.
 *
 *\li	If a pointer to the name is desired, 'foundname' should be non-NULL.
 *	If it is non-NULL, '*foundname' MUST be NULL.
 *
 *\li	If a type other than dns_datatype_any is searched for, 'rdataset'
 *	may be non-NULL, '*rdataset' be NULL, and will point at the found
 *	rdataset.  If the type is dns_datatype_any, 'rdataset' must be NULL.
 *
 *\li	'target' be a valid name.
 *
 *\li	'type' be a valid type.
 *
 *\li	If 'type' is dns_rdatatype_rrsig, 'covers' must be a valid type.
 *	Otherwise it should be 0.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well.
 *\li	#DNS_R_NXDOMAIN		-- name does not exist in that section.
 *\li	#DNS_R_NXRRSET		-- The name does exist, but the desired
 *				   type does not.
 */

isc_result_t
dns_message_findtype(dns_name_t *name, dns_rdatatype_t type,
		     dns_rdatatype_t covers, dns_rdataset_t **rdataset);
/*%<
 * Search the name for the specified type.  If it is found, *rdataset is
 * filled in with a pointer to that rdataset.
 *
 * Requires:
 *\li	if '**rdataset' is non-NULL, *rdataset needs to be NULL.
 *
 *\li	'type' be a valid type, and NOT dns_rdatatype_any.
 *
 *\li	If 'type' is dns_rdatatype_rrsig, 'covers' must be a valid type.
 *	Otherwise it should be 0.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well.
 *\li	#ISC_R_NOTFOUND		-- the desired type does not exist.
 */

isc_result_t
dns_message_find(dns_name_t *name, dns_rdataclass_t rdclass,
		 dns_rdatatype_t type, dns_rdatatype_t covers,
		 dns_rdataset_t **rdataset);
/*%<
 * Search the name for the specified rdclass and type.  If it is found,
 * *rdataset is filled in with a pointer to that rdataset.
 *
 * Requires:
 *\li	if '**rdataset' is non-NULL, *rdataset needs to be NULL.
 *
 *\li	'type' be a valid type, and NOT dns_rdatatype_any.
 *
 *\li	If 'type' is dns_rdatatype_rrsig, 'covers' must be a valid type.
 *	Otherwise it should be 0.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- all is well.
 *\li	#ISC_R_NOTFOUND		-- the desired type does not exist.
 */

void
dns_message_addname(dns_message_t *msg, dns_name_t *name,
		    dns_section_t section);
/*%<
 * Adds the name to the given section.
 *
 * It is the caller's responsibility to enforce any unique name requirements
 * in a section.
 *
 * Requires:
 *
 *\li	'msg' be valid, and be a renderable message.
 *
 *\li	'name' be a valid absolute name.
 *
 *\li	'section' be a named section.
 */

/*
 * LOANOUT FUNCTIONS
 *
 * Each of these functions loan a particular type of data to the caller.
 * The storage for these will vanish when the message is destroyed or
 * reset, and must NOT be used after these operations.
 */

isc_result_t
dns_message_gettempname(dns_message_t *msg, dns_name_t **item);
/*%<
 * Return a name that can be used for any temporary purpose, including
 * inserting into the message's linked lists.  The name must be returned
 * to the message code using dns_message_puttempname() or inserted into
 * one of the message's sections before the message is destroyed.
 *
 * It is the caller's responsibility to initialize this name.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item == NULL
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMEMORY		-- No item can be allocated.
 */

isc_result_t
dns_message_gettemprdata(dns_message_t *msg, dns_rdata_t **item);
/*%<
 * Return a rdata that can be used for any temporary purpose, including
 * inserting into the message's linked lists.  The rdata will be freed
 * when the message is destroyed or reset.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item == NULL
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMEMORY		-- No item can be allocated.
 */

isc_result_t
dns_message_gettemprdataset(dns_message_t *msg, dns_rdataset_t **item);
/*%<
 * Return a rdataset that can be used for any temporary purpose, including
 * inserting into the message's linked lists. The name must be returned
 * to the message code using dns_message_puttempname() or inserted into
 * one of the message's sections before the message is destroyed.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item == NULL
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMEMORY		-- No item can be allocated.
 */

isc_result_t
dns_message_gettemprdatalist(dns_message_t *msg, dns_rdatalist_t **item);
/*%<
 * Return a rdatalist that can be used for any temporary purpose, including
 * inserting into the message's linked lists.  The rdatalist will be
 * destroyed when the message is destroyed or reset.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item == NULL
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		-- All is well.
 *\li	#ISC_R_NOMEMORY		-- No item can be allocated.
 */

void
dns_message_puttempname(dns_message_t *msg, dns_name_t **item);
/*%<
 * Return a borrowed name to the message's name free list.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item point to a name returned by
 *	dns_message_gettempname()
 *
 * Ensures:
 *\li	*item == NULL
 */

void
dns_message_puttemprdata(dns_message_t *msg, dns_rdata_t **item);
/*%<
 * Return a borrowed rdata to the message's rdata free list.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item point to a rdata returned by
 *	dns_message_gettemprdata()
 *
 * Ensures:
 *\li	*item == NULL
 */

void
dns_message_puttemprdataset(dns_message_t *msg, dns_rdataset_t **item);
/*%<
 * Return a borrowed rdataset to the message's rdataset free list.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item point to a rdataset returned by
 *	dns_message_gettemprdataset()
 *
 * Ensures:
 *\li	*item == NULL
 */

void
dns_message_puttemprdatalist(dns_message_t *msg, dns_rdatalist_t **item);
/*%<
 * Return a borrowed rdatalist to the message's rdatalist free list.
 *
 * Requires:
 *\li	msg be a valid message
 *
 *\li	item != NULL && *item point to a rdatalist returned by
 *	dns_message_gettemprdatalist()
 *
 * Ensures:
 *\li	*item == NULL
 */

isc_result_t
dns_message_peekheader(isc_buffer_t *source, dns_messageid_t *idp,
		       unsigned int *flagsp);
/*%<
 * Assume the remaining region of "source" is a DNS message.  Peek into
 * it and fill in "*idp" with the message id, and "*flagsp" with the flags.
 *
 * Requires:
 *
 *\li	source != NULL
 *
 * Ensures:
 *
 *\li	if (idp != NULL) *idp == message id.
 *
 *\li	if (flagsp != NULL) *flagsp == message flags.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		-- all is well.
 *
 *\li	#ISC_R_UNEXPECTEDEND	-- buffer doesn't contain enough for a header.
 */

dns_rdataset_t *
dns_message_getopt(dns_message_t *msg);
/*%<
 * Get the OPT record for 'msg'.
 *
 * Requires:
 *
 *\li	'msg' is a valid message.
 *
 * Returns:
 *
 *\li	The OPT rdataset of 'msg', or NULL if there isn't one.
 */

isc_result_t
dns_message_setopt(dns_message_t *msg, dns_rdataset_t *opt);
/*%<
 * Set the OPT record for 'msg'.
 *
 * Requires:
 *
 *\li	'msg' is a valid message with rendering intent
 *	and no sections have been rendered.
 *
 *\li	'opt' is a valid OPT record.
 *
 * Ensures:
 *
 *\li	The OPT record has either been freed or ownership of it has
 *	been transferred to the message.
 *
 *\li	If ISC_R_SUCCESS was returned, the OPT record will be rendered
 *	when dns_message_renderend() is called.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		-- all is well.
 *
 *\li	#ISC_R_NOSPACE		-- there is no space for the OPT record.
 */

dns_rdataset_t *
dns_message_gettsig(dns_message_t *msg, dns_name_t **owner);
/*%<
 * Get the TSIG record and owner for 'msg'.
 *
 * Requires:
 *
 *\li	'msg' is a valid message.
 *\li	'owner' is NULL or *owner is NULL.
 *
 * Returns:
 *
 *\li	The TSIG rdataset of 'msg', or NULL if there isn't one.
 *
 * Ensures:
 *
 * \li	If 'owner' is not NULL, it will point to the owner name.
 */

isc_result_t
dns_message_settsigkey(dns_message_t *msg, dns_tsigkey_t *key);
/*%<
 * Set the tsig key for 'msg'.  This is only necessary for when rendering a
 * query or parsing a response.  The key (if non-NULL) is attached to, and
 * will be detached when the message is destroyed.
 *
 * Requires:
 *
 *\li	'msg' is a valid message with rendering intent,
 *	dns_message_renderbegin() has been called, and no sections have been
 *	rendered.
 *\li	'key' is a valid tsig key or NULL.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		-- all is well.
 *
 *\li	#ISC_R_NOSPACE		-- there is no space for the TSIG record.
 */

dns_tsigkey_t *
dns_message_gettsigkey(dns_message_t *msg);
/*%<
 * Gets the tsig key for 'msg'.
 *
 * Requires:
 *
 *\li	'msg' is a valid message
 */

isc_result_t
dns_message_setquerytsig(dns_message_t *msg, isc_buffer_t *querytsig);
/*%<
 * Indicates that 'querytsig' is the TSIG from the signed query for which
 * 'msg' is the response.  This is also used for chained TSIGs in TCP
 * responses.
 *
 * Requires:
 *
 *\li	'querytsig' is a valid buffer as returned by dns_message_getquerytsig()
 *	or NULL
 *
 *\li	'msg' is a valid message
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 */

isc_result_t
dns_message_getquerytsig(dns_message_t *msg, isc_buffer_t **querytsig);
/*%<
 * Gets the tsig from the TSIG from the signed query 'msg'.  This is also used
 * for chained TSIGs in TCP responses.  Unlike dns_message_gettsig, this makes
 * a copy of the data, so can be used if the message is destroyed.
 *
 * Requires:
 *
 *\li	'msg' is a valid signed message
 *\li	'mctx' is a valid memory context
 *\li	querytsig != NULL && *querytsig == NULL
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 *
 * Ensures:
 *\li 	'tsig' points to NULL or an allocated buffer which must be freed
 * 	by the caller.
 */

dns_rdataset_t *
dns_message_getsig0(dns_message_t *msg, dns_name_t **owner);
/*%<
 * Get the SIG(0) record and owner for 'msg'.
 *
 * Requires:
 *
 *\li	'msg' is a valid message.
 *\li	'owner' is NULL or *owner is NULL.
 *
 * Returns:
 *
 *\li	The SIG(0) rdataset of 'msg', or NULL if there isn't one.
 *
 * Ensures:
 *
 * \li	If 'owner' is not NULL, it will point to the owner name.
 */

void
dns_message_takebuffer(dns_message_t *msg, isc_buffer_t **buffer);
/*%<
 * Give the *buffer to the message code to clean up when it is no
 * longer needed.  This is usually when the message is reset or
 * destroyed.
 *
 * Requires:
 *
 *\li	msg be a valid message.
 *
 *\li	buffer != NULL && *buffer is a valid isc_buffer_t, which was
 *	dynamically allocated via isc_buffer_allocate().
 */

isc_result_t
dns_message_rechecksig(dns_message_t *msg, dns_view_t *view);
/*%<
 * Reset the signature state and then if the message was signed,
 * verify the message.
 *
 * Requires:
 *
 *\li	msg is a valid parsed message.
 *\li	view is a valid view or NULL
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		- the message was unsigned, or the message
 *				  was signed correctly.
 *
 *\li	#DNS_R_EXPECTEDTSIG	- A TSIG was expected, but not seen
 *\li	#DNS_R_UNEXPECTEDTSIG	- A TSIG was seen but not expected
 *\li	#DNS_R_TSIGVERIFYFAILURE - The TSIG failed to verify
 */

isc_result_t
dns_message_buildopt(dns_message_t *msg, dns_rdataset_t **opt,
		     unsigned int version, uint16_t udpsize,
		     unsigned int flags, dns_ednsopt_t *ednsopts, size_t count);
/*%<
 * Built a opt record.
 *
 * Requires:
 * \li   msg be a valid message.
 * \li   opt to be a non NULL and *opt to be NULL.
 *
 * Returns:
 * \li	 ISC_R_SUCCESS on success.
 * \li	 ISC_R_NOMEMORY
 * \li	 ISC_R_NOSPACE
 * \li	 other.
 */

#endif /* DNS_MESSAGE_H */
