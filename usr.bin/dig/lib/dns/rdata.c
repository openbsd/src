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

/* $Id: rdata.c,v 1.25 2020/02/25 12:37:15 florian Exp $ */

/*! \file */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <isc/base64.h>
#include <isc/hex.h>
#include <isc/util.h>
#include <isc/buffer.h>

#include <dns/cert.h>
#include <dns/compress.h>
#include <dns/keyvalues.h>
#include <dns/rcode.h>
#include <dns/rdata.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/time.h>
#include <dns/ttl.h>

#define RETERR(x) \
	do { \
		isc_result_t _r = (x); \
		if (_r != ISC_R_SUCCESS) \
			return (_r); \
	} while (0)

#define ARGS_TOTEXT	dns_rdata_t *rdata, dns_rdata_textctx_t *tctx, \
			isc_buffer_t *target

#define ARGS_FROMWIRE	int rdclass, dns_rdatatype_t type, \
			isc_buffer_t *source, dns_decompress_t *dctx, \
			unsigned int options, isc_buffer_t *target

#define ARGS_TOWIRE	dns_rdata_t *rdata, dns_compress_t *cctx, \
			isc_buffer_t *target

#define ARGS_FROMSTRUCT	int rdclass, dns_rdatatype_t type, \
			void *source, isc_buffer_t *target

#define ARGS_TOSTRUCT	const dns_rdata_t *rdata, void *target

#define ARGS_FREESTRUCT void *source

#define ARGS_CHECKOWNER dns_name_t *name, dns_rdataclass_t rdclass, \
			dns_rdatatype_t type, isc_boolean_t wildcard

/*%
 * Context structure for the totext_ functions.
 * Contains formatting options for rdata-to-text
 * conversion.
 */
typedef struct dns_rdata_textctx {
	dns_name_t *origin;	/*%< Current origin, or NULL. */
	unsigned int flags;	/*%< DNS_STYLEFLAG_*  */
	unsigned int width;	/*%< Width of rdata column. */
	const char *linebreak;	/*%< Line break string. */
} dns_rdata_textctx_t;

static isc_result_t
txt_totext(isc_region_t *source, isc_boolean_t quote, isc_buffer_t *target);

static isc_result_t
txt_fromwire(isc_buffer_t *source, isc_buffer_t *target);

static isc_result_t
multitxt_totext(isc_region_t *source, isc_buffer_t *target);

static isc_boolean_t
name_prefix(dns_name_t *name, dns_name_t *origin, dns_name_t *target);

static unsigned int
name_length(dns_name_t *name);

static isc_result_t
str_totext(const char *source, isc_buffer_t *target);

static isc_result_t
inet_totext(int af, isc_region_t *src, isc_buffer_t *target);

static isc_boolean_t
buffer_empty(isc_buffer_t *source);

static isc_result_t
uint32_tobuffer(uint32_t, isc_buffer_t *target);

static isc_result_t
uint16_tobuffer(uint32_t, isc_buffer_t *target);

static isc_result_t
name_tobuffer(dns_name_t *name, isc_buffer_t *target);

static uint32_t
uint32_fromregion(isc_region_t *region);

static uint16_t
uint16_fromregion(isc_region_t *region);

static uint8_t
uint8_fromregion(isc_region_t *region);

static uint8_t
uint8_consume_fromregion(isc_region_t *region);

static isc_result_t
mem_tobuffer(isc_buffer_t *target, void *base, unsigned int length);

static isc_result_t
btoa_totext(unsigned char *inbuf, int inbuflen, isc_buffer_t *target);

static isc_result_t
rdata_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	     isc_buffer_t *target);

static isc_result_t
unknown_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	       isc_buffer_t *target);

static inline isc_result_t
generic_totext_key(ARGS_TOTEXT);

static inline isc_result_t
generic_fromwire_key(ARGS_FROMWIRE);

static isc_result_t
generic_totext_txt(ARGS_TOTEXT);

static isc_result_t
generic_fromwire_txt(ARGS_FROMWIRE);

static isc_result_t
generic_totext_ds(ARGS_TOTEXT);

static isc_result_t
generic_fromwire_ds(ARGS_FROMWIRE);

static isc_result_t
generic_totext_tlsa(ARGS_TOTEXT);

static isc_result_t
generic_fromwire_tlsa(ARGS_FROMWIRE);

static inline isc_result_t
name_duporclone(dns_name_t *source, dns_name_t *target) {

	return (dns_name_dup(source, target));
}

static inline void *
mem_maybedup(void *source, size_t length) {
	void *copy;

	copy = malloc(length);
	if (copy != NULL)
		memmove(copy, source, length);

	return (copy);
}

static inline isc_result_t
typemap_totext(isc_region_t *sr, dns_rdata_textctx_t *tctx,
	       isc_buffer_t *target)
{
	unsigned int i, j, k;
	unsigned int window, len;
	isc_boolean_t first = ISC_TRUE;

	for (i = 0; i < sr->length; i += len) {
		if (tctx != NULL &&
		    (tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0) {
			RETERR(str_totext(tctx->linebreak, target));
			first = ISC_TRUE;
		}
		INSIST(i + 2 <= sr->length);
		window = sr->base[i];
		len = sr->base[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= sr->length);
		for (j = 0; j < len; j++) {
			dns_rdatatype_t t;
			if (sr->base[i + j] == 0)
				continue;
			for (k = 0; k < 8; k++) {
				if ((sr->base[i + j] & (0x80 >> k)) == 0)
					continue;
				t = window * 256 + j * 8 + k;
				if (!first)
					RETERR(str_totext(" ", target));
				first = ISC_FALSE;
				if (dns_rdatatype_isknown(t)) {
					RETERR(dns_rdatatype_totext(t, target));
				} else {
					char buf[sizeof("TYPE65535")];
					snprintf(buf, sizeof(buf), "TYPE%u", t);
					RETERR(str_totext(buf, target));
				}
			}
		}
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
typemap_test(isc_region_t *sr, isc_boolean_t allow_empty) {
	unsigned int window, lastwindow = 0;
	unsigned int len;
	isc_boolean_t first = ISC_TRUE;
	unsigned int i;

	for (i = 0; i < sr->length; i += len) {
		/*
		 * Check for overflow.
		 */
		if (i + 2 > sr->length)
			RETERR(DNS_R_FORMERR);
		window = sr->base[i];
		len = sr->base[i + 1];
		i += 2;
		/*
		 * Check that bitmap windows are in the correct order.
		 */
		if (!first && window <= lastwindow)
			RETERR(DNS_R_FORMERR);
		/*
		 * Check for legal lengths.
		 */
		if (len < 1 || len > 32)
			RETERR(DNS_R_FORMERR);
		/*
		 * Check for overflow.
		 */
		if (i + len > sr->length)
			RETERR(DNS_R_FORMERR);
		/*
		 * The last octet of the bitmap must be non zero.
		 */
		if (sr->base[i + len - 1] == 0)
			RETERR(DNS_R_FORMERR);
		lastwindow = window;
		first = ISC_FALSE;
	}
	if (i != sr->length)
		return (DNS_R_EXTRADATA);
	if (!allow_empty && first)
		RETERR(DNS_R_FORMERR);
	return (ISC_R_SUCCESS);
}

static const char decdigits[] = "0123456789";

#include "code.h"

/***
 *** Initialization
 ***/

void
dns_rdata_init(dns_rdata_t *rdata) {

	REQUIRE(rdata != NULL);

	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = 0;
	rdata->type = 0;
	rdata->flags = 0;
	ISC_LINK_INIT(rdata, link);
	/* ISC_LIST_INIT(rdata->list); */
}

void
dns_rdata_reset(dns_rdata_t *rdata) {

	REQUIRE(rdata != NULL);

	REQUIRE(!ISC_LINK_LINKED(rdata, link));
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = 0;
	rdata->type = 0;
	rdata->flags = 0;
}

/***
 ***
 ***/

void
dns_rdata_clone(const dns_rdata_t *src, dns_rdata_t *target) {

	REQUIRE(src != NULL);
	REQUIRE(target != NULL);

	REQUIRE(DNS_RDATA_INITIALIZED(target));

	REQUIRE(DNS_RDATA_VALIDFLAGS(src));
	REQUIRE(DNS_RDATA_VALIDFLAGS(target));

	target->data = src->data;
	target->length = src->length;
	target->rdclass = src->rdclass;
	target->type = src->type;
	target->flags = src->flags;
}

/***
 *** Conversions
 ***/

void
dns_rdata_fromregion(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, isc_region_t *r)
{

	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_INITIALIZED(rdata));
	REQUIRE(r != NULL);

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	rdata->data = r->base;
	rdata->length = r->length;
	rdata->rdclass = rdclass;
	rdata->type = type;
	rdata->flags = 0;
}

void
dns_rdata_toregion(const dns_rdata_t *rdata, isc_region_t *r) {

	REQUIRE(rdata != NULL);
	REQUIRE(r != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	r->base = rdata->data;
	r->length = rdata->length;
}

isc_result_t
dns_rdata_fromwire(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_buffer_t *source,
		   dns_decompress_t *dctx, unsigned int options,
		   isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_region_t region;
	isc_buffer_t ss;
	isc_buffer_t st;
	isc_boolean_t use_default = ISC_FALSE;
	uint32_t activelength;
	unsigned int length;

	REQUIRE(dctx != NULL);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}
	REQUIRE(source != NULL);
	REQUIRE(target != NULL);

	if (type == 0)
		return (DNS_R_FORMERR);

	ss = *source;
	st = *target;

	activelength = isc_buffer_activelength(source);
	INSIST(activelength < 65536);

	FROMWIRESWITCH

	if (use_default) {
		if (activelength > isc_buffer_availablelength(target))
			result = ISC_R_NOSPACE;
		else {
			isc_buffer_putmem(target, isc_buffer_current(source),
					  activelength);
			isc_buffer_forward(source, activelength);
			result = ISC_R_SUCCESS;
		}
	}

	/*
	 * Reject any rdata that expands out to more than DNS_RDATA_MAXLENGTH
	 * as we cannot transmit it.
	 */
	length = isc_buffer_usedlength(target) - isc_buffer_usedlength(&st);
	if (result == ISC_R_SUCCESS && length > DNS_RDATA_MAXLENGTH)
		result = DNS_R_FORMERR;

	/*
	 * We should have consumed all of our buffer.
	 */
	if (result == ISC_R_SUCCESS && !buffer_empty(source))
		result = DNS_R_EXTRADATA;

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = length;
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}

	if (result != ISC_R_SUCCESS) {
		*source = ss;
		*target = st;
	}
	return (result);
}

isc_result_t
dns_rdata_towire(dns_rdata_t *rdata, dns_compress_t *cctx,
		 isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;
	isc_region_t tr;
	isc_buffer_t st;

	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Some DynDNS meta-RRs have empty rdata.
	 */
	if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
		INSIST(rdata->length == 0);
		return (ISC_R_SUCCESS);
	}

	st = *target;

	TOWIRESWITCH

	if (use_default) {
		isc_buffer_availableregion(target, &tr);
		if (tr.length < rdata->length)
			return (ISC_R_NOSPACE);
		memmove(tr.base, rdata->data, rdata->length);
		isc_buffer_add(target, rdata->length);
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS) {
		*target = st;
		INSIST(target->used < 65536);
		dns_compress_rollback(cctx, (uint16_t)target->used);
	}
	return (result);
}

static isc_result_t
unknown_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	       isc_buffer_t *target)
{
	isc_result_t result;
	char buf[sizeof("65535")];
	isc_region_t sr;

	strlcpy(buf, "\\# ", sizeof(buf));
	result = str_totext(buf, target);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdata_toregion(rdata, &sr);
	INSIST(sr.length < 65536);
	snprintf(buf, sizeof(buf), "%u", sr.length);
	result = str_totext(buf, target);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (sr.length != 0U) {
		if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
			result = str_totext(" ( ", target);
		else
			result = str_totext(" ", target);

		if (result != ISC_R_SUCCESS)
			return (result);

		if (tctx->width == 0) /* No splitting */
			result = isc_hex_totext(&sr, 0, "", target);
		else
			result = isc_hex_totext(&sr, tctx->width - 2,
						tctx->linebreak,
						target);
		if (result == ISC_R_SUCCESS &&
		    (tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
			result = str_totext(" )", target);
	}
	return (result);
}

static isc_result_t
rdata_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	     isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;
	unsigned int cur;

	REQUIRE(rdata != NULL);
	REQUIRE(tctx->origin == NULL ||
		dns_name_isabsolute(tctx->origin) == ISC_TRUE);

	/*
	 * Some DynDNS meta-RRs have empty rdata.
	 */
	if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
		INSIST(rdata->length == 0);
		return (ISC_R_SUCCESS);
	}

	cur = isc_buffer_usedlength(target);

	TOTEXTSWITCH

	if (use_default || (result == ISC_R_NOTIMPLEMENTED)) {
		unsigned int u = isc_buffer_usedlength(target);

		INSIST(u >= cur);
		isc_buffer_subtract(target, u - cur);
		result = unknown_totext(rdata, tctx, target);
	}

	return (result);
}

isc_result_t
dns_rdata_totext(dns_rdata_t *rdata, dns_name_t *origin, isc_buffer_t *target)
{
	dns_rdata_textctx_t tctx;

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Set up formatting options for single-line output.
	 */
	tctx.origin = origin;
	tctx.flags = 0;
	tctx.width = 60;
	tctx.linebreak = " ";
	return (rdata_totext(rdata, &tctx, target));
}

isc_result_t
dns_rdata_tofmttext(dns_rdata_t *rdata, dns_name_t *origin,
		    unsigned int flags, unsigned int width,
		    unsigned int split_width, const char *linebreak,
		    isc_buffer_t *target)
{
	dns_rdata_textctx_t tctx;

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Set up formatting options for formatted output.
	 */
	tctx.origin = origin;
	tctx.flags = flags;
	if (split_width == 0xffffffff)
		tctx.width = width;
	else
		tctx.width = split_width;

	if ((flags & DNS_STYLEFLAG_MULTILINE) != 0)
		tctx.linebreak = linebreak;
	else {
		if (split_width == 0xffffffff)
			tctx.width = 60; /* Used for hex word length only. */
		tctx.linebreak = " ";
	}
	return (rdata_totext(rdata, &tctx, target));
}

isc_result_t
dns_rdata_fromstruct_soa(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, dns_rdata_soa_t *soa,
		     isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_buffer_t st;
	isc_region_t region;
	unsigned int length;

	REQUIRE(soa != NULL);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}

	st = *target;
	result = fromstruct_soa(rdclass, type, soa, target);

	length = isc_buffer_usedlength(target) - isc_buffer_usedlength(&st);
	if (result == ISC_R_SUCCESS && length > DNS_RDATA_MAXLENGTH)
		result = ISC_R_NOSPACE;

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = length;
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}
	if (result != ISC_R_SUCCESS)
		*target = st;
	return (result);
}

isc_result_t
dns_rdata_fromstruct_tsig(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, dns_rdata_any_tsig_t *tsig,
		     isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_buffer_t st;
	isc_region_t region;
	unsigned int length;

	REQUIRE(tsig != NULL);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}

	st = *target;
	result = fromstruct_any_tsig(rdclass, type, tsig, target);

	length = isc_buffer_usedlength(target) - isc_buffer_usedlength(&st);
	if (result == ISC_R_SUCCESS && length > DNS_RDATA_MAXLENGTH)
		result = ISC_R_NOSPACE;

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = length;
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}
	if (result != ISC_R_SUCCESS)
		*target = st;
	return (result);
}

isc_result_t
dns_rdata_tostruct_cname(const dns_rdata_t *rdata, dns_rdata_cname_t *cname) {
	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	return (tostruct_cname(rdata, cname));
}

isc_result_t
dns_rdata_tostruct_ns(const dns_rdata_t *rdata, dns_rdata_ns_t *ns) {
	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	return (tostruct_ns(rdata, ns));
}

isc_result_t
dns_rdata_tostruct_soa(const dns_rdata_t *rdata, dns_rdata_soa_t *soa) {
	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	return (tostruct_soa(rdata, soa));
}

isc_result_t
dns_rdata_tostruct_tsig(const dns_rdata_t *rdata, dns_rdata_any_tsig_t *tsig) {
	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	return (tostruct_any_tsig(rdata, tsig));
}

void
dns_rdata_freestruct_cname(dns_rdata_cname_t *cname) {
	REQUIRE(cname != NULL);

	freestruct_cname(cname);
}

void
dns_rdata_freestruct_ns(dns_rdata_ns_t *ns) {
	REQUIRE(ns != NULL);

	freestruct_ns(ns);
}

void
dns_rdata_freestruct_soa(dns_rdata_soa_t *soa) {
	REQUIRE(soa != NULL);

	freestruct_soa(soa);
}

void
dns_rdata_freestruct_tsig(dns_rdata_any_tsig_t *tsig) {
	REQUIRE(tsig != NULL);

	freestruct_any_tsig(tsig);
}

isc_boolean_t
dns_rdata_checkowner_nsec3(dns_name_t *name, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, isc_boolean_t wildcard)
{
	return checkowner_nsec3(name, rdclass, type, wildcard);
}

unsigned int
dns_rdatatype_attributes(dns_rdatatype_t type)
{
	switch (type) {
	case 0:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 1:
		return (RRTYPE_A_ATTRIBUTES);
	case 2:
		return (RRTYPE_NS_ATTRIBUTES);
	case 3:
		return (RRTYPE_MD_ATTRIBUTES);
	case 4:
		return (RRTYPE_MF_ATTRIBUTES);
	case 5:
		return (RRTYPE_CNAME_ATTRIBUTES);
	case 6:
		return (RRTYPE_SOA_ATTRIBUTES);
	case 7:
		return (RRTYPE_MB_ATTRIBUTES);
	case 8:
		return (RRTYPE_MG_ATTRIBUTES);
	case 9:
		return (RRTYPE_MR_ATTRIBUTES);
	case 10:
		return (RRTYPE_NULL_ATTRIBUTES);
	case 11:
		return (RRTYPE_WKS_ATTRIBUTES);
	case 12:
		return (RRTYPE_PTR_ATTRIBUTES);
	case 13:
		return (RRTYPE_HINFO_ATTRIBUTES);
	case 14:
		return (RRTYPE_MINFO_ATTRIBUTES);
	case 15:
		return (RRTYPE_MX_ATTRIBUTES);
	case 16:
		return (RRTYPE_TXT_ATTRIBUTES);
	case 17:
		return (RRTYPE_RP_ATTRIBUTES);
	case 18:
		return (RRTYPE_AFSDB_ATTRIBUTES);
	case 19:
		return (RRTYPE_X25_ATTRIBUTES);
	case 20:
		return (RRTYPE_ISDN_ATTRIBUTES);
	case 21:
		return (RRTYPE_RT_ATTRIBUTES);
	case 22:
		return (RRTYPE_NSAP_ATTRIBUTES);
	case 23:
		return (RRTYPE_NSAP_PTR_ATTRIBUTES);
	case 24:
		return (RRTYPE_SIG_ATTRIBUTES);
	case 25:
		return (RRTYPE_KEY_ATTRIBUTES);
	case 26:
		return (RRTYPE_PX_ATTRIBUTES);
	case 27:
		return (RRTYPE_GPOS_ATTRIBUTES);
	case 28:
		return (RRTYPE_AAAA_ATTRIBUTES);
	case 29:
		return (RRTYPE_LOC_ATTRIBUTES);
	case 30:
		return (RRTYPE_NXT_ATTRIBUTES);
	case 31:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 32:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 33:
		return (RRTYPE_SRV_ATTRIBUTES);
	case 34:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 35:
		return (RRTYPE_NAPTR_ATTRIBUTES);
	case 36:
		return (RRTYPE_KX_ATTRIBUTES);
	case 37:
		return (RRTYPE_CERT_ATTRIBUTES);
	case 38:
		return (RRTYPE_A6_ATTRIBUTES);
	case 39:
		return (RRTYPE_DNAME_ATTRIBUTES);
	case 40:
		return (RRTYPE_SINK_ATTRIBUTES);
	case 41:
		return (RRTYPE_OPT_ATTRIBUTES);
	case 42:
		return (RRTYPE_APL_ATTRIBUTES);
	case 43:
		return (RRTYPE_DS_ATTRIBUTES);
	case 44:
		return (RRTYPE_SSHFP_ATTRIBUTES);
	case 45:
		return (RRTYPE_IPSECKEY_ATTRIBUTES);
	case 46:
		return (RRTYPE_RRSIG_ATTRIBUTES);
	case 47:
		return (RRTYPE_NSEC_ATTRIBUTES);
	case 48:
		return (RRTYPE_DNSKEY_ATTRIBUTES);
	case 49:
		return (RRTYPE_DHCID_ATTRIBUTES);
	case 50:
		return (RRTYPE_NSEC3_ATTRIBUTES);
	case 51:
		return (RRTYPE_NSEC3PARAM_ATTRIBUTES);
	case 52:
		return (RRTYPE_TLSA_ATTRIBUTES);
	case 53:
		return (RRTYPE_SMIMEA_ATTRIBUTES);
	case 55:
		return (RRTYPE_HIP_ATTRIBUTES);
	case 56:
		return (RRTYPE_NINFO_ATTRIBUTES);
	case 57:
		return (RRTYPE_RKEY_ATTRIBUTES);
	case 58:
		return (RRTYPE_TALINK_ATTRIBUTES);
	case 59:
		return (RRTYPE_CDS_ATTRIBUTES);
	case 60:
		return (RRTYPE_CDNSKEY_ATTRIBUTES);
	case 61:
		return (RRTYPE_OPENPGPKEY_ATTRIBUTES);
	case 62:
		return (RRTYPE_CSYNC_ATTRIBUTES);
	case 99:
		return (RRTYPE_SPF_ATTRIBUTES);
	case 100:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 101:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 102:
		return (DNS_RDATATYPEATTR_RESERVED);
	case 103:
		return (RRTYPE_UNSPEC_ATTRIBUTES);
	case 104:
		return (RRTYPE_NID_ATTRIBUTES);
	case 105:
		return (RRTYPE_L32_ATTRIBUTES);
	case 106:
		return (RRTYPE_L64_ATTRIBUTES);
	case 107:
		return (RRTYPE_LP_ATTRIBUTES);
	case 108:
		return (RRTYPE_EUI48_ATTRIBUTES);
	case 109:
		return (RRTYPE_EUI64_ATTRIBUTES);
	case 249:
		return (RRTYPE_TKEY_ATTRIBUTES);
	case 250:
		return (RRTYPE_TSIG_ATTRIBUTES);
	case 251:
		return (DNS_RDATATYPEATTR_META |
		    DNS_RDATATYPEATTR_QUESTIONONLY);
	case 252:
		return (DNS_RDATATYPEATTR_META |
		    DNS_RDATATYPEATTR_QUESTIONONLY);
	case 253:
		return (DNS_RDATATYPEATTR_META |
		    DNS_RDATATYPEATTR_QUESTIONONLY);
	case 254:
		return (DNS_RDATATYPEATTR_META |
		    DNS_RDATATYPEATTR_QUESTIONONLY);
	case 255:
		return (DNS_RDATATYPEATTR_META |
		    DNS_RDATATYPEATTR_QUESTIONONLY);
	case 256:
		return (RRTYPE_URI_ATTRIBUTES);
	case 257:
		return (RRTYPE_CAA_ATTRIBUTES);
	case 258:
		return (RRTYPE_AVC_ATTRIBUTES);
	case 259:
		return (RRTYPE_DOA_ATTRIBUTES);
	case 32768:
		return (RRTYPE_TA_ATTRIBUTES);
	case 32769:
		return (RRTYPE_DLV_ATTRIBUTES);
	case 65533:
		return (RRTYPE_KEYDATA_ATTRIBUTES);
	}

	if (type >= (dns_rdatatype_t)128 && type < (dns_rdatatype_t)255)
		return (DNS_RDATATYPEATTR_UNKNOWN | DNS_RDATATYPEATTR_META);
	return (DNS_RDATATYPEATTR_UNKNOWN);
}

isc_result_t
dns_rdatatype_fromtext(dns_rdatatype_t *typep, isc_textregion_t *source) {
	unsigned int hash;
	unsigned int n;
	unsigned char a, b;

	n = source->length;

	if (n == 0)
		return (DNS_R_UNKNOWN);

	a = tolower((unsigned char)source->base[0]);
	b = tolower((unsigned char)source->base[n - 1]);

	hash = ((a + n) * b) % 256;

	/*
	 * This switch block is inlined via \#define, and will use "return"
	 * to return a result to the caller if it is a valid (known)
	 * rdatatype name.
	 */
	RDATATYPE_FROMTEXT_SW(hash, source->base, n, typep);

	if (source->length > 4 && source->length < (4 + sizeof("65000")) &&
	    strncasecmp("type", source->base, 4) == 0) {
		char buf[sizeof("65000")];
		char *endp;
		unsigned int val;

		/*
		 * source->base is not required to be NUL terminated.
		 * Copy up to remaining bytes and NUL terminate.
		 */
		snprintf(buf, sizeof(buf), "%.*s",
			 (int)(source->length - 4), source->base + 4);
		val = strtoul(buf, &endp, 10);
		if (*endp == '\0' && val <= 0xffff) {
			*typep = (dns_rdatatype_t)val;
			return (ISC_R_SUCCESS);
		}
	}

	return (DNS_R_UNKNOWN);
}

isc_result_t
dns_rdatatype_totext(dns_rdatatype_t type, isc_buffer_t *target) {
	char buf[sizeof("TYPE65535")];

	switch (type) {
	case 0:
		return (str_totext("RESERVED0", target));
	case 1:
		return (str_totext("A", target));
	case 2:
		return (str_totext("NS", target));
	case 3:
		return (str_totext("MD", target));
	case 4:
		return (str_totext("MF", target));
	case 5:
		return (str_totext("CNAME", target));
	case 6:
		return (str_totext("SOA", target));
	case 7:
		return (str_totext("MB", target));
	case 8:
		return (str_totext("MG", target));
	case 9:
		return (str_totext("MR", target));
	case 10:
		return (str_totext("NULL", target));
	case 11:
		return (str_totext("WKS", target));
	case 12:
		return (str_totext("PTR", target));
	case 13:
		return (str_totext("HINFO", target));
	case 14:
		return (str_totext("MINFO", target));
	case 15:
		return (str_totext("MX", target));
	case 16:
		return (str_totext("TXT", target));
	case 17:
		return (str_totext("RP", target));
	case 18:
		return (str_totext("AFSDB", target));
	case 19:
		return (str_totext("X25", target));
	case 20:
		return (str_totext("ISDN", target));
	case 21:
		return (str_totext("RT", target));
	case 22:
		return (str_totext("NSAP", target));
	case 23:
		return (str_totext("NSAP-PTR", target));
	case 24:
		return (str_totext("SIG", target));
	case 25:
		return (str_totext("KEY", target));
	case 26:
		return (str_totext("PX", target));
	case 27:
		return (str_totext("GPOS", target));
	case 28:
		return (str_totext("AAAA", target));
	case 29:
		return (str_totext("LOC", target));
	case 30:
		return (str_totext("NXT", target));
	case 31:
		return (str_totext("EID", target));
	case 32:
		return (str_totext("NIMLOC", target));
	case 33:
		return (str_totext("SRV", target));
	case 34:
		return (str_totext("ATMA", target));
	case 35:
		return (str_totext("NAPTR", target));
	case 36:
		return (str_totext("KX", target));
	case 37:
		return (str_totext("CERT", target));
	case 38:
		return (str_totext("A6", target));
	case 39:
		return (str_totext("DNAME", target));
	case 40:
		return (str_totext("SINK", target));
	case 41:
		return (str_totext("OPT", target));
	case 42:
		return (str_totext("APL", target));
	case 43:
		return (str_totext("DS", target));
	case 44:
		return (str_totext("SSHFP", target));
	case 45:
		return (str_totext("IPSECKEY", target));
	case 46:
		return (str_totext("RRSIG", target));
	case 47:
		return (str_totext("NSEC", target));
	case 48:
		return (str_totext("DNSKEY", target));
	case 49:
		return (str_totext("DHCID", target));
	case 50:
		return (str_totext("NSEC3", target));
	case 51:
		return (str_totext("NSEC3PARAM", target));
	case 52:
		return (str_totext("TLSA", target));
	case 53:
		return (str_totext("SMIMEA", target));
	case 55:
		return (str_totext("HIP", target));
	case 56:
		return (str_totext("NINFO", target));
	case 57:
		return (str_totext("RKEY", target));
	case 58:
		return (str_totext("TALINK", target));
	case 59:
		return (str_totext("CDS", target));
	case 60:
		return (str_totext("CDNSKEY", target));
	case 61:
		return (str_totext("OPENPGPKEY", target));
	case 62:
		return (str_totext("CSYNC", target));
	case 99:
		return (str_totext("SPF", target));
	case 100:
		return (str_totext("UINFO", target));
	case 101:
		return (str_totext("UID", target));
	case 102:
		return (str_totext("GID", target));
	case 103:
		return (str_totext("UNSPEC", target));
	case 104:
		return (str_totext("NID", target));
	case 105:
		return (str_totext("L32", target));
	case 106:
		return (str_totext("L64", target));
	case 107:
		return (str_totext("LP", target));
	case 108:
		return (str_totext("EUI48", target));
	case 109:
		return (str_totext("EUI64", target));
	case 249:
		return (str_totext("TKEY", target));
	case 250:
		return (str_totext("TSIG", target));
	case 251:
		return (str_totext("IXFR", target));
	case 252:
		return (str_totext("AXFR", target));
	case 253:
		return (str_totext("MAILB", target));
	case 254:
		return (str_totext("MAILA", target));
	case 255:
		return (str_totext("ANY", target));
	case 256:
		return (str_totext("URI", target));
	case 257:
		return (str_totext("CAA", target));
	case 258:
		return (str_totext("AVC", target));
	case 259:
		return (str_totext("DOA", target));
	case 32768:
		return (str_totext("TA", target));
	case 32769:
		return (str_totext("DLV", target));
	default:
		snprintf(buf, sizeof(buf), "TYPE%u", type);
		return (str_totext(buf, target));
	}
}

void
dns_rdatatype_format(dns_rdatatype_t rdtype,
		     char *array, unsigned int size)
{
	isc_result_t result;
	isc_buffer_t buf;

	if (size == 0U)
		return;

	isc_buffer_init(&buf, array, size);
	result = dns_rdatatype_totext(rdtype, &buf);
	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}
	if (result != ISC_R_SUCCESS)
		strlcpy(array, "<unknown>", size);
}

/*
 * Private function.
 */

static unsigned int
name_length(dns_name_t *name) {
	return (name->length);
}

static isc_result_t
txt_totext(isc_region_t *source, isc_boolean_t quote, isc_buffer_t *target) {
	unsigned int tl;
	unsigned int n;
	unsigned char *sp;
	char *tp;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	sp = source->base;
	tp = (char *)region.base;
	tl = region.length;

	n = *sp++;

	REQUIRE(n + 1 <= source->length);
	if (n == 0U)
		REQUIRE(quote == ISC_TRUE);

	if (quote) {
		if (tl < 1)
			return (ISC_R_NOSPACE);
		*tp++ = '"';
		tl--;
	}
	while (n--) {
		/*
		 * \DDD space (0x20) if not quoting.
		 */
		if (*sp < (quote ? 0x20 : 0x21) || *sp >= 0x7f) {
			if (tl < 4)
				return (ISC_R_NOSPACE);
			*tp++ = 0x5c;
			*tp++ = 0x30 + ((*sp / 100) % 10);
			*tp++ = 0x30 + ((*sp / 10) % 10);
			*tp++ = 0x30 + (*sp % 10);
			sp++;
			tl -= 4;
			continue;
		}
		/*
		 * Escape double quote and backslash.  If we are not
		 * enclosing the string in double quotes also escape
		 * at sign and semicolon.
		 */
		if (*sp == 0x22 || *sp == 0x5c ||
		    (!quote && (*sp == 0x40 || *sp == 0x3b))) {
			if (tl < 2)
				return (ISC_R_NOSPACE);
			*tp++ = '\\';
			tl--;
		}
		if (tl < 1)
			return (ISC_R_NOSPACE);
		*tp++ = *sp++;
		tl--;
	}
	if (quote) {
		if (tl < 1)
			return (ISC_R_NOSPACE);
		*tp++ = '"';
		tl--;
		POST(tl);
	}
	isc_buffer_add(target, (unsigned int)(tp - (char *)region.base));
	isc_region_consume(source, *source->base + 1);
	return (ISC_R_SUCCESS);
}

static isc_result_t
txt_fromwire(isc_buffer_t *source, isc_buffer_t *target) {
	unsigned int n;
	isc_region_t sregion;
	isc_region_t tregion;

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length == 0)
		return (ISC_R_UNEXPECTEDEND);
	n = *sregion.base + 1;
	if (n > sregion.length)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_availableregion(target, &tregion);
	if (n > tregion.length)
		return (ISC_R_NOSPACE);

	if (tregion.base != sregion.base)
		memmove(tregion.base, sregion.base, n);
	isc_buffer_forward(source, n);
	isc_buffer_add(target, n);
	return (ISC_R_SUCCESS);
}

/*
 * Conversion of TXT-like rdata fields without length limits.
 */
static isc_result_t
multitxt_totext(isc_region_t *source, isc_buffer_t *target) {
	unsigned int tl;
	unsigned int n0, n;
	unsigned char *sp;
	char *tp;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	sp = source->base;
	tp = (char *)region.base;
	tl = region.length;

	if (tl < 1)
		return (ISC_R_NOSPACE);
	*tp++ = '"';
	tl--;
	do {
		n = source->length;
		n0 = source->length - 1;

		while (n--) {
			if (*sp < 0x20 || *sp >= 0x7f) {
				if (tl < 4)
					return (ISC_R_NOSPACE);
				*tp++ = 0x5c;
				*tp++ = 0x30 + ((*sp / 100) % 10);
				*tp++ = 0x30 + ((*sp / 10) % 10);
				*tp++ = 0x30 + (*sp % 10);
				sp++;
				tl -= 4;
				continue;
			}
			/* double quote, backslash */
			if (*sp == 0x22 || *sp == 0x5c) {
				if (tl < 2)
					return (ISC_R_NOSPACE);
				*tp++ = '\\';
				tl--;
			}
			if (tl < 1)
				return (ISC_R_NOSPACE);
			*tp++ = *sp++;
			tl--;
		}
		isc_region_consume(source, n0 + 1);
	} while (source->length != 0);
	if (tl < 1)
		return (ISC_R_NOSPACE);
	*tp++ = '"';
	tl--;
	POST(tl);
	isc_buffer_add(target, (unsigned int)(tp - (char *)region.base));
	return (ISC_R_SUCCESS);
}

static isc_boolean_t
name_prefix(dns_name_t *name, dns_name_t *origin, dns_name_t *target) {
	int l1, l2;

	if (origin == NULL)
		goto return_false;

	if (dns_name_compare(origin, dns_rootname) == 0)
		goto return_false;

	if (!dns_name_issubdomain(name, origin))
		goto return_false;

	l1 = dns_name_countlabels(name);
	l2 = dns_name_countlabels(origin);

	if (l1 == l2)
		goto return_false;

	/* Master files should be case preserving. */
	dns_name_getlabelsequence(name, l1 - l2, l2, target);
	if (!dns_name_caseequal(origin, target))
		goto return_false;

	dns_name_getlabelsequence(name, 0, l1 - l2, target);
	return (ISC_TRUE);

return_false:
	*target = *name;
	return (ISC_FALSE);
}

static isc_result_t
str_totext(const char *source, isc_buffer_t *target) {
	unsigned int l;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	l = strlen(source);

	if (l > region.length)
		return (ISC_R_NOSPACE);

	memmove(region.base, source, l);
	isc_buffer_add(target, l);
	return (ISC_R_SUCCESS);
}

static isc_result_t
inet_totext(int af, isc_region_t *src, isc_buffer_t *target) {
	char tmpbuf[64];

	/* Note - inet_ntop doesn't do size checking on its input. */
	if (inet_ntop(af, src->base, tmpbuf, sizeof(tmpbuf)) == NULL)
		return (ISC_R_NOSPACE);
	if (strlen(tmpbuf) > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);
	isc_buffer_putstr(target, tmpbuf);
	return (ISC_R_SUCCESS);
}

static isc_boolean_t
buffer_empty(isc_buffer_t *source) {
	return((source->current == source->active) ? ISC_TRUE : ISC_FALSE);
}

static isc_result_t
uint32_tobuffer(uint32_t value, isc_buffer_t *target) {
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	if (region.length < 4)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint32(target, value);
	return (ISC_R_SUCCESS);
}

static isc_result_t
uint16_tobuffer(uint32_t value, isc_buffer_t *target) {
	isc_region_t region;

	if (value > 0xffff)
		return (ISC_R_RANGE);
	isc_buffer_availableregion(target, &region);
	if (region.length < 2)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint16(target, (uint16_t)value);
	return (ISC_R_SUCCESS);
}

static isc_result_t
name_tobuffer(dns_name_t *name, isc_buffer_t *target) {
	isc_region_t r;
	dns_name_toregion(name, &r);
	return (isc_buffer_copyregion(target, &r));
}

static uint32_t
uint32_fromregion(isc_region_t *region) {
	uint32_t value;

	REQUIRE(region->length >= 4);
	value = region->base[0] << 24;
	value |= region->base[1] << 16;
	value |= region->base[2] << 8;
	value |= region->base[3];
	return(value);
}

static uint16_t
uint16_fromregion(isc_region_t *region) {

	REQUIRE(region->length >= 2);

	return ((region->base[0] << 8) | region->base[1]);
}

static uint8_t
uint8_fromregion(isc_region_t *region) {

	REQUIRE(region->length >= 1);

	return (region->base[0]);
}

static uint8_t
uint8_consume_fromregion(isc_region_t *region) {
	uint8_t r = uint8_fromregion(region);

	isc_region_consume(region, 1);
	return r;
}

static isc_result_t
mem_tobuffer(isc_buffer_t *target, void *base, unsigned int length) {
	isc_region_t tr;

	if (length == 0U)
		return (ISC_R_SUCCESS);

	isc_buffer_availableregion(target, &tr);
	if (length > tr.length)
		return (ISC_R_NOSPACE);
	if (tr.base != base)
		memmove(tr.base, base, length);
	isc_buffer_add(target, length);
	return (ISC_R_SUCCESS);
}

static const char atob_digits[86] =
	"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`" \
	"abcdefghijklmnopqrstu";
/*
 * Subroutines to convert between 8 bit binary bytes and printable ASCII.
 * Computes the number of bytes, and three kinds of simple checksums.
 * Incoming bytes are collected into 32-bit words, then printed in base 85:
 *	exp(85,5) > exp(2,32)
 * The ASCII characters used are between '!' and 'u';
 * 'z' encodes 32-bit zero; 'x' is used to mark the end of encoded data.
 *
 * Originally by Paul Rutter (philabs!per) and Joe Orost (petsd!joe) for
 * the atob/btoa programs, released with the compress program, in mod.sources.
 * Modified by Mike Schwartz 8/19/86 for use in BIND.
 * Modified to be re-entrant 3/2/99.
 */

struct state {
	int32_t Ceor;
	int32_t Csum;
	int32_t Crot;
	int32_t word;
	int32_t bcount;
};

#define Ceor state->Ceor
#define Csum state->Csum
#define Crot state->Crot
#define word state->word
#define bcount state->bcount

static isc_result_t	byte_btoa(int c, isc_buffer_t *, struct state *state);

/*
 * Encode binary byte c into ASCII representation and place into *bufp,
 * advancing bufp.
 */
static isc_result_t
byte_btoa(int c, isc_buffer_t *target, struct state *state) {
	isc_region_t tr;

	isc_buffer_availableregion(target, &tr);
	Ceor ^= c;
	Csum += c;
	Csum += 1;
	if ((Crot & 0x80000000)) {
		Crot <<= 1;
		Crot += 1;
	} else {
		Crot <<= 1;
	}
	Crot += c;

	word <<= 8;
	word |= c;
	if (bcount == 3) {
		if (word == 0) {
			if (tr.length < 1)
				return (ISC_R_NOSPACE);
			tr.base[0] = 'z';
			isc_buffer_add(target, 1);
		} else {
		    register int tmp = 0;
		    register int32_t tmpword = word;

		    if (tmpword < 0) {
			   /*
			    * Because some don't support u_long.
			    */
			tmp = 32;
			tmpword -= (int32_t)(85 * 85 * 85 * 85 * 32);
		    }
		    if (tmpword < 0) {
			tmp = 64;
			tmpword -= (int32_t)(85 * 85 * 85 * 85 * 32);
		    }
			if (tr.length < 5)
				return (ISC_R_NOSPACE);
			tr.base[0] = atob_digits[(tmpword /
					      (int32_t)(85 * 85 * 85 * 85))
						+ tmp];
			tmpword %= (int32_t)(85 * 85 * 85 * 85);
			tr.base[1] = atob_digits[tmpword / (85 * 85 * 85)];
			tmpword %= (85 * 85 * 85);
			tr.base[2] = atob_digits[tmpword / (85 * 85)];
			tmpword %= (85 * 85);
			tr.base[3] = atob_digits[tmpword / 85];
			tmpword %= 85;
			tr.base[4] = atob_digits[tmpword];
			isc_buffer_add(target, 5);
		}
		bcount = 0;
	} else {
		bcount += 1;
	}
	return (ISC_R_SUCCESS);
}

/*
 * Encode the binary data from inbuf, of length inbuflen, into a
 * target.  Return success/failure status
 */
static isc_result_t
btoa_totext(unsigned char *inbuf, int inbuflen, isc_buffer_t *target) {
	int inc;
	struct state statebuf, *state = &statebuf;
	char buf[sizeof("x 2000000000 ffffffff ffffffff ffffffff")];

	Ceor = Csum = Crot = word = bcount = 0;
	for (inc = 0; inc < inbuflen; inbuf++, inc++)
		RETERR(byte_btoa(*inbuf, target, state));

	while (bcount != 0)
		RETERR(byte_btoa(0, target, state));

	/*
	 * Put byte count and checksum information at end of buffer,
	 * delimited by 'x'
	 */
	snprintf(buf, sizeof(buf), "x %d %x %x %x", inbuflen, Ceor, Csum, Crot);
	return (str_totext(buf, target));
}

dns_rdatatype_t
dns_rdata_covers(dns_rdata_t *rdata) {
	if (rdata->type == dns_rdatatype_rrsig)
		return (covers_rrsig(rdata));
	return (covers_sig(rdata));
}

isc_boolean_t
dns_rdatatype_isknown(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_UNKNOWN)
	    == 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}
