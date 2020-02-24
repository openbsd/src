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

/* $Id: mb_7.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Wed Mar 15 17:31:26 PST 2000 by bwelling */

#ifndef RDATA_GENERIC_MB_7_C
#define RDATA_GENERIC_MB_7_C

#define RRTYPE_MB_ATTRIBUTES (0)

static inline isc_result_t
totext_mb(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_mb);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_mb(ARGS_FROMWIRE) {
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_mb);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_mb(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_mb);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_mb(ARGS_FROMSTRUCT) {
	dns_rdata_mb_t *mb = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_mb);
	REQUIRE(source != NULL);
	REQUIRE(mb->common.rdtype == type);
	REQUIRE(mb->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&mb->mb, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_mb(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_mb_t *mb = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_mb);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	mb->common.rdclass = rdata->rdclass;
	mb->common.rdtype = rdata->type;
	ISC_LINK_INIT(&mb->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	dns_name_init(&mb->mb, NULL);
	RETERR(name_duporclone(&name, &mb->mb));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_mb(ARGS_FREESTRUCT) {
	dns_rdata_mb_t *mb = source;

	REQUIRE(source != NULL);

	dns_name_free(&mb->mb);
}



#endif	/* RDATA_GENERIC_MB_7_C */
