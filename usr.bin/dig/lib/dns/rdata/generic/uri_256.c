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

/* $Id: uri_256.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

#ifndef GENERIC_URI_256_C
#define GENERIC_URI_256_C 1

static inline isc_result_t
totext_uri(ARGS_TOTEXT) {
	isc_region_t region;
	unsigned short priority, weight;
	char buf[sizeof("65000 ")];

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_uri);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);

	/*
	 * Priority
	 */
	priority = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u ", priority);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Weight
	 */
	weight = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u ", weight);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Target URI
	 */
	RETERR(multitxt_totext(&region, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_uri(ARGS_FROMWIRE) {
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_uri);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	/*
	 * Priority, weight
	 */
	isc_buffer_activeregion(source, &region);
	if (region.length < 4)
		return (ISC_R_UNEXPECTEDEND);

	/*
	 * Priority, weight and target URI
	 */
	isc_buffer_forward(source, region.length);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

static inline isc_result_t
towire_uri(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_uri);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

#endif /* GENERIC_URI_256_C */
