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

/* $Id: nsap_22.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

/* Reviewed: Fri Mar 17 10:41:07 PST 2000 by gson */

/* RFC1706 */

#ifndef RDATA_IN_1_NSAP_22_C
#define RDATA_IN_1_NSAP_22_C

static inline isc_result_t
totext_in_nsap(ARGS_TOTEXT) {
	isc_region_t region;
	char buf[sizeof("xx")];

	REQUIRE(rdata->type == dns_rdatatype_nsap);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);
	RETERR(isc_str_tobuffer("0x", target));
	while (region.length != 0) {
		snprintf(buf, sizeof(buf), "%02x", region.base[0]);
		isc_region_consume(&region, 1);
		RETERR(isc_str_tobuffer(buf, target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_in_nsap(ARGS_FROMWIRE) {
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_nsap);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(options);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &region);
	if (region.length < 1)
		return (ISC_R_UNEXPECTEDEND);

	RETERR(isc_mem_tobuffer(target, region.base, region.length));
	isc_buffer_forward(source, region.length);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_in_nsap(ARGS_TOWIRE) {
	REQUIRE(rdata->type == dns_rdatatype_nsap);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_IN_1_NSAP_22_C */
