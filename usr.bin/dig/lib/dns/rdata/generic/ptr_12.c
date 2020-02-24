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

/* $Id: ptr_12.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Thu Mar 16 14:05:12 PST 2000 by explorer */

#ifndef RDATA_GENERIC_PTR_12_C
#define RDATA_GENERIC_PTR_12_C

#define RRTYPE_PTR_ATTRIBUTES (0)

static inline isc_result_t
totext_ptr(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_ptr);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_ptr(ARGS_FROMWIRE) {
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_ptr);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_ptr(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_ptr);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_ptr(ARGS_FROMSTRUCT) {
	dns_rdata_ptr_t *ptr = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_ptr);
	REQUIRE(source != NULL);
	REQUIRE(ptr->common.rdtype == type);
	REQUIRE(ptr->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&ptr->ptr, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_ptr(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_ptr_t *ptr = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_ptr);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	ptr->common.rdclass = rdata->rdclass;
	ptr->common.rdtype = rdata->type;
	ISC_LINK_INIT(&ptr->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	dns_name_init(&ptr->ptr, NULL);
	RETERR(name_duporclone(&name, &ptr->ptr));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_ptr(ARGS_FREESTRUCT) {
	dns_rdata_ptr_t *ptr = source;

	REQUIRE(source != NULL);
	REQUIRE(ptr->common.rdtype == dns_rdatatype_ptr);

	dns_name_free(&ptr->ptr);
}


static unsigned char ip6_arpa_data[]  = "\003IP6\004ARPA";
static unsigned char ip6_arpa_offsets[] = { 0, 4, 9 };
static const dns_name_t ip6_arpa =
	DNS_NAME_INITABSOLUTE(ip6_arpa_data, ip6_arpa_offsets);

static unsigned char ip6_int_data[]  = "\003IP6\003INT";
static unsigned char ip6_int_offsets[] = { 0, 4, 8 };
static const dns_name_t ip6_int =
	DNS_NAME_INITABSOLUTE(ip6_int_data, ip6_int_offsets);

static unsigned char in_addr_arpa_data[]  = "\007IN-ADDR\004ARPA";
static unsigned char in_addr_arpa_offsets[] = { 0, 8, 13 };
static const dns_name_t in_addr_arpa =
	DNS_NAME_INITABSOLUTE(in_addr_arpa_data, in_addr_arpa_offsets);

#endif	/* RDATA_GENERIC_PTR_12_C */
