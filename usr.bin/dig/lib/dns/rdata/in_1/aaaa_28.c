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

/* $Id: aaaa_28.c,v 1.10 2020/02/26 18:38:15 florian Exp $ */

/* Reviewed: Thu Mar 16 16:52:50 PST 2000 by bwelling */

/* RFC1886 */

#ifndef RDATA_IN_1_AAAA_28_C
#define RDATA_IN_1_AAAA_28_C

#include <isc/net.h>

static inline isc_result_t
totext_in_aaaa(ARGS_TOTEXT) {
	isc_region_t region;

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_aaaa);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length == 16);

	dns_rdata_toregion(rdata, &region);
	return (inet_totext(AF_INET6, &region, target));
}

static inline isc_result_t
fromwire_in_aaaa(ARGS_FROMWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;

	REQUIRE(type == dns_rdatatype_aaaa);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(options);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);
	if (sregion.length < 16)
		return (ISC_R_UNEXPECTEDEND);
	if (tregion.length < 16)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 16);
	isc_buffer_forward(source, 16);
	isc_buffer_add(target, 16);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_in_aaaa(ARGS_TOWIRE) {
	isc_region_t region;

	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_aaaa);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length == 16);

	isc_buffer_availableregion(target, &region);
	if (region.length < rdata->length)
		return (ISC_R_NOSPACE);
	memmove(region.base, rdata->data, rdata->length);
	isc_buffer_add(target, 16);
	return (ISC_R_SUCCESS);
}

#endif	/* RDATA_IN_1_AAAA_28_C */
